/**
 * @file servo_log.cpp
 * @brief The servo tester's log. Core0 only. @see servo_log.h
 */

#include "servo_log.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

size_t servoLogRowText(char *buf, size_t n, uint32_t ms, uint16_t aUs, uint16_t bUs,
                       int32_t ua, uint16_t mv, uint8_t flags) {
	// Half away from zero, in 64 bits: the sum would overflow at INT32_MIN.
	int64_t u = ua;
	int64_t ma = (u < 0 ? u - 500 : u + 500) / 1000;
	int len = snprintf(buf, n, "%lu,%u,%u,%lld,%u,%u\n", (unsigned long)ms, (unsigned)aUs,
	                   (unsigned)bUs, (long long)ma, (unsigned)mv, (unsigned)flags);
	return len < 0 ? n : (size_t)len;
}

const char *servoLogHowName(ServoLogHow how) {
	switch (how) {
		case ServoLogHow::Stop:        return "STOP";
		case ServoLogHow::Back:        return "BACK";
		case ServoLogHow::Stalled:     return "STALLED";
		case ServoLogHow::TouchSilent: return "TOUCH_SILENT";
		case ServoLogHow::BusStuck:    return "BUS_STUCK";
		case ServoLogHow::Overcurrent: return "OVERCURRENT";
		case ServoLogHow::Stall:       return "STALL";
		case ServoLogHow::LowSupply:   return "LOW_SUPPLY";
		case ServoLogHow::NoSensor:    return "NO_SENSOR";
		case ServoLogHow::NoStart:     return "NO_START";
		case ServoLogHow::None:        break;
	}
	return "NONE";
}

size_t servoLogEndText(char *buf, size_t n, ServoLogHow how, uint32_t rows,
                       uint32_t dropped, uint16_t upMs) {
	char up[8] = "-";
	if (upMs != SUPPLY_UP_NONE) snprintf(up, sizeof(up), "%u", (unsigned)upMs);
	int len = snprintf(buf, n, "# end=%s rows=%lu dropped=%lu up_ms=%s\n", servoLogHowName(how),
	                   (unsigned long)rows, (unsigned long)dropped, up);
	return len < 0 ? n : (size_t)len;
}

ServoLogHow servoLogHowOf(const ServoStatus *st, bool back) {
	if (st->tripped) {
		switch (st->powerFault) {
			case ServoPowerFault::TouchSilent: return ServoLogHow::TouchSilent;
			case ServoPowerFault::BusStuck:    return ServoLogHow::BusStuck;
			case ServoPowerFault::Overcurrent: return ServoLogHow::Overcurrent;
			case ServoPowerFault::Stall:       return ServoLogHow::Stall;
			case ServoPowerFault::LowSupply:   return ServoLogHow::LowSupply;
			case ServoPowerFault::NoSensor:    return ServoLogHow::NoSensor;
			default:                           return ServoLogHow::Stalled;
		}
	}
	if (st->powerFault == ServoPowerFault::NoStart) return ServoLogHow::NoStart;
	return back ? ServoLogHow::Back : ServoLogHow::Stop;
}

bool servoLogRunOver(const ServoStatus *st, uint16_t startSeq) {
	return (int16_t)(uint16_t)(st->ackSeq - startSeq) >= 0 && !st->running;
}

#if SD_LOG_ENABLE

#include "card_cache.h"
#include "log_ring.h"
#include "sd_log_next.h"
#include "sense.h"

#include "ff.h"

static_assert(SERVO_LOG_RING_BYTES % 512u == 0,
              "SERVO_LOG_RING_BYTES must be a multiple of the 512-byte block");

/** @brief One block: what each f_write() of the run sends. */
static const uint32_t kBlock = 512u;

static FIL      s_file;
/**
 * @brief The file's fast-seek table (FatFs's CLMT, cluster link map table):
 *        the reserve is one contiguous run, so its size, one fragment and
 *        the end take 4 of these 8 words. While it is set, f_write() finds
 *        each cluster in it and never reads the FAT.
 */
static DWORD    s_clmt[8];
/** @brief The bytes reserved at the open: SERVO_LOG_PREALLOC_BYTES or a half of it, down to SERVO_LOG_RESERVE_MIN_BYTES. */
static uint32_t s_reserve = 0;
/** @brief The rows used the whole SERVO_LOG_PREALLOC_BYTES: the writing stopped there. */
static bool     s_atLimit = false;
static LogRing  s_ring;
static uint8_t  s_ringBuf[SERVO_LOG_RING_BYTES];
/** @brief The next servo log's number, read from `LOGS/` once per mount. */
static SdLogNextCache s_next = {};

static bool     s_active = false;   /**< A log is open. */
static bool     s_writing = false;  /**< Rows go to the card: no fault yet. */
static uint16_t s_startSeq = 0;     /**< The Start command's seq (servoLogFollow()). */
static bool     s_back = false;     /**< BACK was tapped during the log. */
static uint32_t s_startMs = 0;      /**< millis() at the open: the rows' time 0. */
static uint32_t s_seq = 0;          /**< Seq of the last reading seen. */
static uint32_t s_fileNo = 0;
static uint32_t s_rows = 0;
static uint32_t s_dropped = 0;
static SdLogEnd s_fault = SdLogEnd::None;
static ServoLogHow s_how = ServoLogHow::None;

/** @brief Stop the writing for the rest of the run, recording @p why. */
static void fail(SdLogEnd why) {
	s_fault = why;
	s_writing = false;
}

/** @brief Close the file just opened and remove it: no log after all, for @p why. */
static void dropOpen(uint32_t n, SdLogEnd why) {
	f_close(&s_file);
	char path[40];
	snprintf(path, sizeof(path), "%s/%s%05lu%s", SD_LOG_DIR_PATH, SD_LOG_KIND_SRV.head,
	         (unsigned long)n, SD_LOG_KIND_SRV.tail);
	f_unlink(path);
	s_fault = why;
	cardCacheEnd();
}

bool servoLogStart(uint32_t nowMs) {
	if (s_active) return true;
	s_fault = SdLogEnd::None;
	s_how = ServoLogHow::None;
	s_rows = s_dropped = 0;
	s_fileNo = 0;
	s_atLimit = false;
	// No readings, no rows.
	if (!senseDeclared()) return false;
	// The cache from the open on, so the open's reads fill its read cache:
	// the directory's block and the file's FAT blocks are those the run reads
	// again. This also stops a running blackbox log, whose 14 blocks a second
	// would fill the queue. Without a mounted card the cache stays off: no
	// card, nothing to write to.
	if (!cardCacheBegin(SdLogEnd::ByServo)) return false;
	uint32_t n = 0;
	SdLogEnd why = sdLogOpenNext(&s_next, sdLogFs(), SD_LOG_DIR_PATH, &s_file, &n,
	                             &SD_LOG_KIND_SRV);
	if (why != SdLogEnd::None) {
		s_fault = why;
		cardCacheEnd();
		return false;
	}
	// A contiguous run reserved now, while nothing is powered: the run
	// writes inside it and never searches the FAT. A card without 16 MiB
	// free in one run gets the largest half that fits, down to 64 KiB.
	uint32_t reserve = SERVO_LOG_PREALLOC_BYTES;
	while (reserve >= SERVO_LOG_RESERVE_MIN_BYTES && f_expand(&s_file, reserve, 1) != FR_OK)
		reserve /= 2;
	if (reserve < SERVO_LOG_RESERVE_MIN_BYTES) {
		dropOpen(n, SdLogEnd::CardFull);
		return false;
	}
	// The run's clusters from the table, not from the FAT: no read.
	s_clmt[0] = sizeof(s_clmt) / sizeof(s_clmt[0]);
	s_file.cltbl = s_clmt;
	if (f_lseek(&s_file, CREATE_LINKMAP) != FR_OK) {
		dropOpen(n, SdLogEnd::OpenFailed);
		return false;
	}
	// The reserve's FAT blocks, FSINFO and the entry out now: the entry holds
	// the reserved size until the close truncates it, as the blackbox log's.
	// The run makes no f_sync(), which reads the directory's block.
	if (f_sync(&s_file) != FR_OK) {
		dropOpen(n, SdLogEnd::OpenFailed);
		return false;
	}
	s_reserve = reserve;
	logRingInit(&s_ring, s_ringBuf, sizeof(s_ringBuf));
	logRingWrite(&s_ring, (const uint8_t *)SERVO_LOG_HEADER, sizeof(SERVO_LOG_HEADER) - 1);
	SenseSample smp;
	senseSample(&smp);
	s_seq = smp.seq;
	s_fileNo = n;
	s_startMs = nowMs;
	s_back = false;
	s_writing = true;
	s_active = true;
	return true;
}

void servoLogFollow(uint16_t seq) { s_startSeq = seq; }

void servoLogBack() { s_back = true; }

/** @brief The reading @p smp as a row, with the pulses of @p st. */
static void addRow(const ServoStatus *st, const SenseSample *smp) {
	char row[SERVO_LOG_ROW_MAX + 1];
	size_t len = servoLogRowText(row, sizeof(row), smp->takenMs - s_startMs, st->outUs[0],
	                             st->outUs[1], smp->currentUa, smp->busMv, smp->flags);
	s_rows++;
	if (len >= sizeof(row) || !logRingWrite(&s_ring, (const uint8_t *)row, (uint32_t)len))
		s_dropped++;
}

/**
 * @brief Write @p len bytes; a short or failed write stops the writing.
 * @return True when all of them went.
 */
static bool put(const void *data, uint32_t len) {
	UINT wrote = 0;
	FRESULT fr = f_write(&s_file, data, len, &wrote);
	if (fr == FR_OK && wrote == len) return true;
	fail(sdLogWriteEnd((int)fr, (uint32_t)wrote, len));
	return false;
}

/**
 * @brief The run's one card call this pass: a block of rows, inside the
 *        reserve. At its end the writing stops: CARD FULL when the card gave
 *        less than SERVO_LOG_PREALLOC_BYTES, else the log is at its limit.
 */
static void writeStep() {
	const uint8_t *p = nullptr;
	if (logRingPeek(&s_ring, &p) < kBlock) return;
	if (f_tell(&s_file) + kBlock > s_reserve) {
		if (s_reserve < SERVO_LOG_PREALLOC_BYTES) {
			fail(SdLogEnd::CardFull);
		} else {
			s_atLimit = true;
			s_writing = false;
		}
		return;
	}
	if (put(p, kBlock)) logRingConsume(&s_ring, kBlock);
}

/**
 * @brief The run is over, the supply off and the pulses at level 0: the
 *        rest of the ring and the end line, the truncation and the close,
 *        then the cache flushed, the card mounted again after a failed
 *        write, and the cache off.
 */
static void finish(ServoLogHow how, uint16_t upMs) {
	s_how = how;
	if (cardCacheState() == CardCache::Failed) fail(SdLogEnd::WriteFailed);
	if (s_fault != SdLogEnd::WriteFailed) {
		// Out of fast-seek mode: past the reserve the file may grow now, a
		// cluster at a time, the supply off.
		s_file.cltbl = nullptr;
		bool ok = true;
		while (ok && logRingUsed(&s_ring) > 0) {
			const uint8_t *p = nullptr;
			uint32_t run = logRingPeek(&s_ring, &p);
			ok = put(p, run);
			if (ok) logRingConsume(&s_ring, run);
		}
		char end[SERVO_LOG_END_MAX + 1];
		size_t len = servoLogEndText(end, sizeof(end), how, s_rows, s_dropped, upMs);
		if (ok && len < sizeof(end)) put(end, (uint32_t)len);
		// A card that filled up keeps the rows written before: the close
		// still commits the entry, as it needs no new cluster.
		// f_close() syncs first: the entry with the size, then the file's
		// release.
		f_truncate(&s_file);
		if (f_close(&s_file) != FR_OK || cardCacheFlush() == CardCache::Failed)
			s_fault = SdLogEnd::WriteFailed;
	}
	// A failed write leaves FatFs's view of the card behind the card's: a
	// mount drops it, as after a failed batch of a backup claim.
	if (s_fault == SdLogEnd::WriteFailed) cardCacheRecover();
	cardCacheEnd();
	s_writing = false;
	s_active = false;
}

void servoLogPoll() {
	if (!s_active) return;
	ServoStatus st;
	servoJobStatus(&st);
	SenseSample smp;
	senseSample(&smp);
	if (s_writing && smp.seq != s_seq) addRow(&st, &smp);
	s_seq = smp.seq;
	if (servoLogRunOver(&st, s_startSeq)) {
		finish(servoLogHowOf(&st, s_back), st.upMs);
		return;
	}
	if (!s_writing) return;
	CardCache c = cardCacheState();
	if (c == CardCache::Failed) {
		fail(SdLogEnd::WriteFailed);
		return;
	}
	if (c == CardCache::Idle) writeStep();
}

void servoLogStatus(ServoLogStatus *out) {
	out->active     = s_active;
	out->fileNumber = s_fileNo;
	out->rows       = s_rows;
	out->dropped    = s_dropped;
	out->fault      = s_fault;
	out->atLimit    = s_atLimit;
	out->how        = s_how;
}

#else  // !SD_LOG_ENABLE

bool servoLogStart(uint32_t) { return false; }
void servoLogFollow(uint16_t) {}
void servoLogBack() {}
void servoLogPoll() {}
void servoLogStatus(ServoLogStatus *out) {
	memset(out, 0, sizeof(*out));
}

#endif
