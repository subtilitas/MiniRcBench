/**
 * @file servo_log_fatfs.cpp
 * @brief The servo log, `src/servo_log.cpp`, over FatFs R0.15 itself and the
 *        write cache.
 *
 * Built by `make -C test lognum` beside sd_log_fatfs.cpp, against the same
 * FatFs and the RAM disk of ram_disk.h. servo_log.cpp, the blackbox logger,
 * `card_cache.cpp` and the cache, `src/sdio/sd_cache.c`, run as on the board.
 * Below the cache a card model takes the place of the SDIO host: each block
 * read costs a set time, each block write a transfer and then a busy time
 * the cache polls, and the card can be removed or fail a write. The glue's
 * calls of `sd_card_cache.h` are written here over the cache, as
 * `src/sdio/sd_card_glue.c` writes them over the SDIO host. What lies beside
 * the log is faked: the timer, the servo job's status, the current sensor's
 * sample, the settings.
 *
 * Core0's loop is modelled as main.cpp runs it: a pass of 50 us, a UI frame
 * every 25 ms that takes a reading, then cardCachePoll() and servoLogPoll().
 * Core1's supply guard (`src/supply_guard.cpp`) judges each reading as it is
 * published, as core1 does within a pass.
 *
 * Covers the file's name and its numbering apart from `LOGnnnnn.BFL`; the
 * header, a row per reading, the end line; the start and the end of a log
 * by every cause; a full card, a card removed during the run and a write
 * error; the cache on for the log only, never more than 3 blocks queued by
 * one call of the run and never a flush on the spot, at most one card read a
 * pass; the core0 time a pass spends on the card and the hard cut on time
 * with the log on a card at the specification's times.
 */

#include "check.h"
#include "ram_disk.h"

#include "card_cache.h"
#include "clock.h"
#include "config.h"
#include "esc_task.h"
#include "hw_config.h"
#include "run_summary.h"
#include "sd_log.h"
#include "sd_log_next.h"
#include "sdio/sd_cache.h"
#include "sdio/sd_card_cache.h"
#include "sense.h"
#include "servo_job.h"
#include "servo_log.h"
#include "settings.h"
#include "supply_guard.h"
#include "usb_msc.h"

#include "ff.h"
#include "pico/time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

int g_failures = 0;

// ---------------------------------------------------------------------------
// what lies beside the log
// ---------------------------------------------------------------------------

/** @brief The virtual timer: past 2^32 us, so micros() and time_us_64() differ. */
static uint64_t g_us = 5000000000ull;
uint64_t time_us_64() { return g_us; }
uint32_t time_us_32() { return (uint32_t)g_us; }
void sleep_ms(uint32_t ms) { g_us += (uint64_t)ms * 1000u; }
void sleep_us(uint64_t us) { g_us += us; }

static EscTelemetry g_tel;
void escSnapshot(EscTelemetry *out) { *out = g_tel; }

static Settings g_settings;
Settings *settings() { return &g_settings; }

static sd_card_t g_sdcard;
sd_card_t *sd_get_by_num(size_t) { return &g_sdcard; }
const char *sd_get_drive_prefix(sd_card_t *) { return ""; }
int sd_sdio_init_result(void) { return 0; }

void clockNow(ClockReading *out) { *out = ClockReading{}; }
void diagUptimeText(char *buf, size_t n, uint32_t seconds) {
	snprintf(buf, n, "%lu", (unsigned long)seconds);
}

MscState mscGetState() { return MscState::Idle; }

static SenseSample g_smp;
static bool g_declared = true;
void senseSample(SenseSample *out) { *out = g_smp; }
bool senseDeclared() { return g_declared; }
/** @brief sense.cpp's rule, for the guard: a reading younger than 250 ms. */
bool senseFresh(const SenseSample *s, uint32_t nowMs) {
	return s->seq != 0 && (uint32_t)(nowMs - s->takenMs) < SENSE_STALE_MS;
}

static ServoStatus g_st;
void servoJobStatus(ServoStatus *out) { *out = g_st; }

// ---------------------------------------------------------------------------
// the card under the cache
// ---------------------------------------------------------------------------

/** @brief The card's times and what the run asked of it. */
struct CardModel {
	uint32_t readUs;    /**< One block read. */
	uint32_t xferUs;    /**< One block's transfer to the card. */
	uint32_t busyUs;    /**< The card programming one block; each look at it costs 10 us. */
	uint64_t busyUntil; /**< When the block in flight is programmed. */
	bool     pendingErr; /**< The block in flight was not taken. */
	unsigned reads;     /**< Blocks read from the card. */
	unsigned writes;    /**< Blocks sent to the card. */
	bool     failRead;  /**< A read of @ref failLba fails: no response. */
	uint32_t failLba;
};
static CardModel g_card;

static void cardTimes(uint32_t readUs, uint32_t busyUs) {
	g_card = CardModel{};
	g_card.readUs = readUs;
	g_card.xferUs = 100;   // 512 bytes and CRC at 10 MHz on 4 lines: about 0.1 ms
	g_card.busyUs = busyUs;
}

static sd_err_t devRead(void *, uint32_t lba, uint32_t n, uint8_t *buf) {
	g_card.reads += n;
	g_us += (uint64_t)g_card.readUs * n;
	if (g_card.failRead && lba <= g_card.failLba && g_card.failLba < lba + n) return SDE_NO_RESPONSE;
	return ramDiskRead(buf, lba, n) == RES_OK ? SDE_OK : SDE_NO_RESPONSE;
}

static sd_err_t devWriteStart(void *, uint32_t lba, const uint8_t *buf) {
	g_us += g_card.xferUs;
	if (g_disk.removed) return SDE_NO_RESPONSE;
	g_card.writes++;
	g_card.pendingErr = ramDiskWrite(buf, lba, 1) != RES_OK;
	g_card.busyUntil = g_us + g_card.busyUs;
	return SDE_OK;
}

static sd_err_t devWritePoll(void *, bool *done) {
	g_us += 10;   // one look at DAT0, and CMD13 once it is high
	*done = g_us >= g_card.busyUntil;
	return *done && g_card.pendingErr ? SDE_WRITE : SDE_OK;
}

static const sdc_dev_t kDev = { nullptr, devRead, devWriteStart, devWritePoll };

/** @brief Blocks queued by the writes of the call being measured. */
static unsigned g_queued = 0;
/** @brief millis() of each queued block holding SRV00001.CSV's 8.3 entry: its syncs on FAT32. */
static std::vector<uint32_t> g_entryWrites;
/** @brief The LBA of the last of them. */
static uint32_t g_entryLba = 0;
/** @brief Writes that found the queue without room: a flush on the spot. */
static unsigned g_spotFlushes = 0;

/** @brief The glue's read: the cache while it is on, else the card. */
static DRESULT viaRead(BYTE *buf, LBA_t sector, UINT count) {
	if (g_disk.noInit) return RES_NOTRDY;
	if (sdc_on()) return sdc_read((uint32_t)sector, count, buf) == SDE_OK ? RES_OK : RES_ERROR;
	return devRead(nullptr, (uint32_t)sector, count, buf) == SDE_OK ? RES_OK : RES_ERROR;
}

/** @brief The glue's write: queued while the cache is on, else each block waited for. */
static DRESULT viaWrite(const BYTE *buf, LBA_t sector, UINT count) {
	if (g_disk.noInit) return RES_NOTRDY;
	if (sdc_on()) {
		uint16_t left = 0;
		sdc_state(&left);
		if (left + count > SDC_QUEUE_BLOCKS) g_spotFlushes++;
		g_queued += count;
		for (UINT i = 0; i < count; i++)
			if (memmem(buf + i * 512u, 512, "SRV00001CSV", 11)) {
				g_entryWrites.push_back((uint32_t)(g_us / 1000u));
				g_entryLba = (uint32_t)(sector + i);
			}
		return sdc_write((uint32_t)sector, count, buf) == SDE_OK ? RES_OK : RES_ERROR;
	}
	for (UINT i = 0; i < count; i++) {
		g_us += g_card.xferUs + g_card.busyUs;
		if (ramDiskWrite(buf + i * 512u, sector + i, 1) != RES_OK) return RES_ERROR;
	}
	return RES_OK;
}

// sd_card_cache.h, as sd_card_glue.c gives it over the SDIO host.
bool sd_sdio_cache_begin(sd_card_t *, uint32_t lba, uint32_t off) {
	return !g_disk.noInit && sdc_begin(&kDev, lba, off) == SDE_OK;
}
void sd_sdio_cache_poll(sd_card_t *) { if (sdc_on()) sdc_poll(); }
int sd_sdio_cache_state(uint16_t *left) {
	if (!sdc_on()) {
		if (left) *left = 0;
		return SD_SDIO_CACHE_OFF;
	}
	return (int)sdc_state(left);
}
int sd_sdio_cache_flush(sd_card_t *) { return sdc_on() ? (int)sdc_flush() : SD_SDIO_CACHE_OFF; }
void sd_sdio_cache_drop(sd_card_t *) {
	sdc_drop();
	g_disk.noInit = true;   // the next mount initialises the card
}
bool sd_sdio_read_past(sd_card_t *, uint32_t lba, uint8_t *buf) {
	if (g_disk.noInit) return false;
	if (sdc_on()) sdc_settle();
	return devRead(nullptr, lba, 1, buf) == SDE_OK;
}
void sd_sdio_cache_end(sd_card_t *) {
	if (!sdc_on()) return;
	sdc_flush();
	sdc_end();
}

// ---------------------------------------------------------------------------
// core0's loop and core1's guard
// ---------------------------------------------------------------------------

static FATFS s_fs;
static uint32_t nowMs() { return (uint32_t)(g_us / 1000u); }

/** @brief A reading as the row should give it, and the pulses beside it. */
struct Reading {
	uint32_t takenMs;
	int32_t  ua;
	uint16_t mv;
	uint8_t  flags;
	uint16_t a, b;
};

/** @brief The model's state: the readings, the times, the guard. */
struct Loop {
	uint32_t nextUiMs;
	int32_t  ua;            /**< The current the next reading takes. */
	uint16_t mv;
	uint8_t  flags;
	bool     read;          /**< UI frames take readings. */
	std::vector<Reading> readings;   /**< Every reading since the log opened. */
	uint64_t worstPassUs;   /**< The longest core0 time of cardCachePoll() and servoLogPoll(). */
	uint64_t worstGapUs;    /**< The longest time between two readings. */
	uint64_t lastReadingUs;
	unsigned worstReads;    /**< The most card reads in one pass. */
	unsigned worstQueued;   /**< The most blocks one servoLogPoll() queued while the run went on. */
	bool     running;       /**< The run is on: the queue counts. */
	SupplyGuard guard;
	GuardRules  rules;
	GuardCut    cut;
	uint64_t    cutUs;      /**< When the guard cut, core1's time. */
	bool        logging;    /**< The core0 loop calls the log: off for the baseline. */
};
static Loop g_loop;

static void loopReset() {
	g_loop = Loop{};
	g_loop.nextUiMs = nowMs();
	g_loop.ua = 120000;
	g_loop.mv = 5020;
	g_loop.read = true;
	g_loop.rules = { true, true, true };
	g_loop.logging = true;
	g_loop.lastReadingUs = g_us;
}

/** @brief Core1 judging the published sample, within its next pass. */
static void guardStep() {
	GuardCut c = supplyGuardStep(&g_loop.guard, &g_smp, &g_loop.rules, nowMs());
	if (c != GuardCut::None && g_loop.cut == GuardCut::None) {
		g_loop.cut = c;
		g_loop.cutUs = g_us;
	}
}

/** @brief Core0's read of the sensor in the UI frame. */
static void takeReading() {
	g_smp.seq++;
	g_smp.takenMs = nowMs();
	g_smp.currentUa = g_loop.ua;
	g_smp.busMv = g_loop.mv;
	g_smp.flags = g_loop.flags;
	g_smp.state = (uint8_t)SenseState::Online;
	uint64_t gap = g_us - g_loop.lastReadingUs;
	if (gap > g_loop.worstGapUs) g_loop.worstGapUs = gap;
	g_loop.lastReadingUs = g_us;
	g_loop.readings.push_back({ g_smp.takenMs, g_smp.currentUa, g_smp.busMv, g_smp.flags,
	                            g_st.outUs[0], g_st.outUs[1] });
}

/** @brief One pass of main.cpp's loop. */
static void pass() {
	g_us += 50;   // USB, the bridge, the UI between its frames
	if ((int32_t)(nowMs() - g_loop.nextUiMs) >= 0) {
		g_loop.nextUiMs = nowMs() + 25;
		if (g_loop.read) {
			takeReading();
			guardStep();
		}
	}
	uint64_t t0 = g_us;
	unsigned r0 = g_card.reads;
	sdLogTick(time_us_32(), 0);
	sdLogFlush();
	cardCachePoll();
	g_queued = 0;
	if (g_loop.logging) servoLogPoll(nowMs());
	if (g_loop.running && g_queued > g_loop.worstQueued) g_loop.worstQueued = g_queued;
	uint64_t took = g_us - t0;
	if (took > g_loop.worstPassUs) g_loop.worstPassUs = took;
	unsigned reads = g_card.reads - r0;
	if (reads > g_loop.worstReads) g_loop.worstReads = reads;
	guardStep();
}

/** @brief Passes for @p ms of the virtual clock. */
static void runFor(uint32_t ms) {
	uint64_t end = g_us + (uint64_t)ms * 1000u;
	while (g_us < end) pass();
}

// ---------------------------------------------------------------------------
// the servo job, as its status shows it
// ---------------------------------------------------------------------------

static uint16_t g_cmdSeq = 100;
static uint32_t g_startMs = 0;

/** @brief Format the card, mount it for the logger, route its blocks through the glue. */
static void format(BYTE fmt, LBA_t sectors, DWORD au) {
	ramDiskFormat(&s_fs, fmt, sectors, au);
	g_disk.viaRead = viaRead;
	g_disk.viaWrite = viaWrite;
	g_sdcard.state.sectors = (uint32_t)sectors;
	if (!sdLogBegin()) { fprintf(stderr, "sdLogBegin failed\n"); exit(2); }
}

/** @brief The job SAFE, nothing tripped, no start-up time. */
static void jobSafe() {
	g_st = ServoStatus{};
	g_st.ackSeq = g_cmdSeq;
	g_st.upMs = SUPPLY_UP_NONE;
}

/**
 * @brief HOLD TO START as ui_servo.cpp makes it: the log opened, the Start
 *        sent and followed; core1 takes it at once unless @p taken is false.
 * @return servoLogStart()'s answer.
 */
static bool holdStart(bool taken = true) {
	g_startMs = nowMs();
	g_loop.readings.clear();
	bool open = servoLogStart(g_startMs);
	servoLogFollow(++g_cmdSeq);
	if (taken) {
		g_st.ackSeq = g_cmdSeq;
		g_st.running = true;
		g_st.tripped = false;
		g_st.powerFault = ServoPowerFault::None;
		g_st.outUs[0] = 1500;
		g_st.outUs[1] = 1480;
		g_st.upMs = 40;
		supplyGuardArm(&g_loop.guard, &g_smp, nowMs());
		g_loop.running = true;
	}
	return open;
}

/** @brief The run ends: STOP taken, or a cut or trip with @p fault. */
static void runEnds(bool tripped = false, ServoPowerFault fault = ServoPowerFault::None) {
	g_st.ackSeq = ++g_cmdSeq;
	g_st.running = false;
	g_st.tripped = tripped;
	g_st.powerFault = fault;
	g_st.outUs[0] = g_st.outUs[1] = 0;
	supplyGuardDisarm(&g_loop.guard);
	g_loop.running = false;
}

static ServoLogStatus logStatus() {
	ServoLogStatus s;
	servoLogStatus(&s);
	return s;
}

static SdLogStatus escStatus() {
	SdLogStatus s;
	sdLogStatus(&s);
	return s;
}

/** @brief A file's bytes; false when it does not open. */
static bool readFile(const char *path, std::string *out) {
	FIL f;
	out->clear();
	if (f_open(&f, path, FA_READ) != FR_OK) return false;
	char buf[512];
	UINT got = 0;
	while (f_read(&f, buf, sizeof(buf), &got) == FR_OK && got > 0) out->append(buf, got);
	f_close(&f);
	return true;
}

static bool exists(const char *path) {
	FILINFO fno;
	return f_stat(path, &fno) == FR_OK;
}

/** @brief Put a file at @p path holding @p text, as a computer would. */
static void put(const char *path, const char *text) {
	FIL f;
	UINT w = 0;
	if (f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK ||
	    f_write(&f, text, (UINT)strlen(text), &w) != FR_OK || f_close(&f) != FR_OK) {
		fprintf(stderr, "put %s\n", path);
		exit(2);
	}
}

static DWORD freeClusters() {
	DWORD n = 0;
	FATFS *fs = nullptr;
	return f_getfree("", &n, &fs) == FR_OK ? n : 0;
}

/** @brief Leave @p keep clusters free: a file over the rest, as a full card. */
static void fill(DWORD keep) {
	DWORD freeClst = 0;
	FATFS *fs = nullptr;
	FIL f;
	if (f_getfree("", &freeClst, &fs) != FR_OK || freeClst < keep ||
	    f_open(&f, "/FILL.BIN", FA_WRITE | FA_CREATE_NEW) != FR_OK ||
	    f_expand(&f, (FSIZE_t)(freeClst - keep) * fs->csize * 512u, 1) != FR_OK ||
	    f_close(&f) != FR_OK) {
		fprintf(stderr, "fill failed\n");
		exit(2);
	}
}

/** @brief The header and a row for each reading of @p rs, as the file should hold them. */
static std::string expected(const std::vector<Reading> &rs, size_t from = 0) {
	std::string s = from == 0 ? SERVO_LOG_HEADER : "";
	for (size_t i = from; i < rs.size(); i++) {
		const Reading &r = rs[i];
		char row[64];
		size_t len = servoLogRowText(row, sizeof(row), r.takenMs - g_startMs, r.a, r.b, r.ua, r.mv,
		                             r.flags);
		s.append(row, len);
	}
	return s;
}

static std::string endLine(ServoLogHow how, uint32_t rows, uint32_t dropped, uint16_t up) {
	char e[96];
	size_t len = servoLogEndText(e, sizeof(e), how, rows, dropped, up);
	return std::string(e, len);
}

/**
 * @brief Whether @p got is whole blocks of @p want, at least @p least bytes
 *        of them, then zeros to its end: a file the close never truncated.
 */
static bool writtenThenZeros(const std::string &got, const std::string &want, size_t least) {
	size_t n = got.find_last_not_of('\0');
	n = n == std::string::npos ? 0 : n + 1;
	size_t blocks = (n + 511) / 512 * 512;
	if (blocks < least || blocks > want.size() + 511) return false;
	if (want.compare(0, n, got, 0, n) != 0) return false;
	return got.find_first_not_of('\0', blocks) == std::string::npos;
}

/** @brief Passes until the log has closed, at most @p ms. */
static void untilClosed(uint32_t ms = 2000) {
	uint64_t end = g_us + (uint64_t)ms * 1000u;
	while (logStatus().active && g_us < end) pass();
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void testPure() {
	section("Servo log: the row, the end line, how a run ended");
	char b[96];
	size_t len = servoLogRowText(b, sizeof(b), 12345, 1500, 0, 420000, 5020, 0);
	checkTrue("12345,1500,0,420,5020,0", std::string(b, len) == "12345,1500,0,420,5020,0\n");
	struct { int32_t ua; const char *ma; } r[] = {
		{ 499, "0" }, { 500, "1" }, { 1499, "1" }, { 1500, "2" }, { -499, "0" }, { -500, "-1" },
		{ -1500, "-2" }, { 3200000, "3200" }, { INT32_MIN, "-2147484" }, { INT32_MAX, "2147484" },
	};
	for (auto &c : r) {
		len = servoLogRowText(b, sizeof(b), 0, 0, 0, c.ua, 0, 0);
		std::string want = std::string("0,0,0,") + c.ma + ",0,0\n";
		char what[64];
		snprintf(what, sizeof(what), "%ld uA: %s mA, half away from zero", (long)c.ua, c.ma);
		checkTrue(what, std::string(b, len) == want);
	}
	len = servoLogRowText(b, sizeof(b), 4294967295u, 65535, 65535, INT32_MIN, 65535, 255);
	checkTrue("the widest row", std::string(b, len) == "4294967295,65535,65535,-2147484,65535,255\n");
	checkTrue("within SERVO_LOG_ROW_MAX", len <= SERVO_LOG_ROW_MAX);
	len = servoLogRowText(b, 10, 12345, 1500, 1500, 420000, 5020, 3);
	checkTrue("a cut row: its length is the buffer's or more", len >= 10);
	len = servoLogRowText(b, sizeof(b), 7, 1000, 2000, -3200000, 4100, 3);
	checkTrue("both pulses, a negative current, both flags",
	          std::string(b, len) == "7,1000,2000,-3200,4100,3\n");

	len = servoLogEndText(b, sizeof(b), ServoLogHow::Stop, 2400, 0, 40);
	checkTrue("# end=STOP rows=2400 dropped=0 up_ms=40",
	          std::string(b, len) == "# end=STOP rows=2400 dropped=0 up_ms=40\n");
	len = servoLogEndText(b, sizeof(b), ServoLogHow::Back, 3, 2, SUPPLY_UP_NONE);
	checkTrue("no start-up time: up_ms=-", std::string(b, len) == "# end=BACK rows=3 dropped=2 up_ms=-\n");
	len = servoLogEndText(b, sizeof(b), ServoLogHow::Stop, 1, 0, 0);
	checkTrue("a start-up time of 0 ms", std::string(b, len) == "# end=STOP rows=1 dropped=0 up_ms=0\n");
	len = servoLogEndText(b, sizeof(b), ServoLogHow::TouchSilent, 4294967295u, 4294967295u, 65534);
	checkTrue("the widest end line within SERVO_LOG_END_MAX", len <= SERVO_LOG_END_MAX &&
	          std::string(b, len) ==
	          "# end=TOUCH_SILENT rows=4294967295 dropped=4294967295 up_ms=65534\n");
	len = servoLogEndText(b, 8, ServoLogHow::Stop, 1, 0, 0);
	checkTrue("a cut end line: its length is the buffer's or more", len >= 8);

	struct { ServoLogHow h; const char *n; } names[] = {
		{ ServoLogHow::None, "NONE" }, { ServoLogHow::Stop, "STOP" }, { ServoLogHow::Back, "BACK" },
		{ ServoLogHow::Stalled, "STALLED" }, { ServoLogHow::TouchSilent, "TOUCH_SILENT" },
		{ ServoLogHow::BusStuck, "BUS_STUCK" }, { ServoLogHow::Overcurrent, "OVERCURRENT" },
		{ ServoLogHow::Stall, "STALL" }, { ServoLogHow::LowSupply, "LOW_SUPPLY" },
		{ ServoLogHow::NoSensor, "NO_SENSOR" }, { ServoLogHow::NoStart, "NO_START" },
	};
	bool ok = true;
	for (auto &c : names) ok &= strcmp(servoLogHowName(c.h), c.n) == 0;
	checkTrue("every end's name", ok);

	ServoStatus st = {};
	struct { bool tripped; ServoPowerFault f; bool back; ServoLogHow want; const char *what; } hows[] = {
		{ false, ServoPowerFault::None,        false, ServoLogHow::Stop,        "STOP" },
		{ false, ServoPowerFault::None,        true,  ServoLogHow::Back,        "BACK" },
		{ true,  ServoPowerFault::None,        false, ServoLogHow::Stalled,     "the heartbeat trip" },
		{ true,  ServoPowerFault::TouchSilent, false, ServoLogHow::TouchSilent, "touch silent" },
		{ true,  ServoPowerFault::BusStuck,    false, ServoLogHow::BusStuck,    "bus stuck" },
		{ true,  ServoPowerFault::Overcurrent, true,  ServoLogHow::Overcurrent, "the hard cut, BACK too" },
		{ true,  ServoPowerFault::Stall,       false, ServoLogHow::Stall,       "the stall cut" },
		{ true,  ServoPowerFault::LowSupply,   false, ServoLogHow::LowSupply,   "the collapse cut" },
		{ true,  ServoPowerFault::NoSensor,    false, ServoLogHow::NoSensor,    "the sensor-lost cut" },
		{ false, ServoPowerFault::NoStart,     true,  ServoLogHow::NoStart,     "no start, BACK too" },
		{ false, ServoPowerFault::Overcurrent, false, ServoLogHow::Stop,        "a fault not tripped: STOP" },
	};
	for (auto &c : hows) {
		st.tripped = c.tripped;
		st.powerFault = c.f;
		checkTrue(c.what, servoLogHowOf(&st, c.back) == c.want);
	}

	st = ServoStatus{};
	st.ackSeq = 7;
	st.running = true;
	checkTrue("the Start taken, RUNNING: not over", !servoLogRunOver(&st, 7));
	st.running = false;
	checkTrue("the Start taken, not RUNNING: over (refused, or ended)", servoLogRunOver(&st, 7));
	st.ackSeq = 6;
	checkTrue("the Start not taken yet: not over", !servoLogRunOver(&st, 7));
	st.ackSeq = 8;
	checkTrue("a later command taken, not RUNNING: over", servoLogRunOver(&st, 7));
	st.ackSeq = 0;
	checkTrue("across the wrap: 65535 then 0, over", servoLogRunOver(&st, 65535));
	st.ackSeq = 65535;
	checkTrue("across the wrap: 0 not taken yet at 65535", !servoLogRunOver(&st, 0));
	st.ackSeq = (uint16_t)(7 + 32767);
	checkTrue("32767 commands later: over", servoLogRunOver(&st, 7));
	st.ackSeq = (uint16_t)(7 + 32768);
	checkTrue("32768 later reads as before it", !servoLogRunOver(&st, 7));
}

/** @brief The file's name and its numbering, apart from the blackbox log's. */
static void testNames(const char *vol, BYTE fmt, DWORD au) {
	char t[96];
	snprintf(t, sizeof(t), "Servo log over FatFs, %s: SRVnnnnn.CSV, numbered on its own", vol);
	section(t);
	format(fmt, 2u * 1024u * 1024u, au);
	cardTimes(1000, 5000);
	loopReset();
	jobSafe();
	f_mkdir("/LOGS");
	put("/LOGS/LOG00005.BFL", "H");
	put("/LOGS/srv00007.csv", "a log from a computer, lower case");
	put("/LOGS/SRV00003.CSV", "x");
	put("/LOGS/SRV0009.CSV", "four digits");
	put("/LOGS/SRV00009.TXT", "another extension");
	put("/LOGS/SRV000010.CSV", "six digits");
	put("/LOGS/XSRV00011.CSV", "a longer name");
	checkTrue("HOLD TO START opens a log", holdStart());
	checkInt("after srv00007.csv: SRV00008.CSV", (long)logStatus().fileNumber, 8);
	checkTrue("its file is there", exists("/LOGS/SRV00008.CSV"));
	runFor(100);
	runEnds();
	untilClosed();
	checkTrue("the blackbox log's number is its own: LOG00006.BFL",
	          sdLogStart() && escStatus().fileNumber == 6);
	sdLogStop();
	jobSafe();
	holdStart();
	checkInt("the next: SRV00009.CSV, the number kept", (long)logStatus().fileNumber, 9);
	runEnds();
	untilClosed();
	put("/LOGS/SRV00020.CSV", "a computer put it while the number was kept");
	holdStart();
	checkInt("kept on this mount: SRV00010.CSV", (long)logStatus().fileNumber, 10);
	runEnds();
	untilClosed();
	sdLogRemount();
	holdStart();
	checkInt("a mount reads LOGS/ again: SRV00021.CSV", (long)logStatus().fileNumber, 21);
	runEnds();
	untilClosed();
	checkTrue("and the blackbox log's next: LOG00007.BFL",
	          sdLogStart() && escStatus().fileNumber == 7);
	sdLogStop();

	put("/LOGS/SRV99999.CSV", "the last");
	sdLogRemount();
	jobSafe();
	checkTrue("SRV99999.CSV: no log", !holdStart());
	ServoLogStatus s = logStatus();
	checkTrue("NoFreeNumber, nothing open, the cache off",
	          s.fault == SdLogEnd::NoFreeNumber && !s.active && !sdc_on() && s.fileNumber == 0);
	runFor(100);
	checkTrue("the blackbox log still numbers: LOG00008.BFL",
	          sdLogStart() && escStatus().fileNumber == 8);
	sdLogStop();
	runEnds();
	runFor(50);
	f_mount(nullptr, "", 0);
}

/** @brief A run from HOLD TO START to STOP: the header, the rows, the end line. */
static void testRun(const char *vol, BYTE fmt, DWORD au) {
	char t[96];
	snprintf(t, sizeof(t), "Servo log over FatFs, %s: the rows of a run, STOP", vol);
	section(t);
	format(fmt, 2u * 1024u * 1024u, au);
	cardTimes(1000, 5000);
	loopReset();
	jobSafe();
	runFor(200);
	f_mkdir("/LOGS");
	DWORD freeBefore = freeClusters();

	// A blackbox log started by hand on SD LOG, still running.
	checkTrue("a blackbox log runs", sdLogStart() && sdLogActive());
	runFor(100);
	unsigned seqBefore = g_smp.seq;
	checkTrue("HOLD TO START: a log", holdStart());
	SdLogStatus esc = escStatus();
	checkTrue("the blackbox log stopped by SERVO TESTER, its summary saved",
	          !sdLogActive() && esc.end == SdLogEnd::ByServo && esc.summary &&
	          esc.summary->end == RunSumEnd::ByServo && esc.summary->file == RunSumFile::Saved);
	std::string txt;
	checkTrue("LOG00001.TXT: Ended by: SERVO TESTER",
	          readFile("/LOGS/LOG00001.TXT", &txt) && txt.find("\nEnded by: SERVO TESTER\n") != std::string::npos);
	checkTrue("the write cache on", sdc_on());
	ServoLogStatus s = logStatus();
	checkTrue("a second START call while the log runs: the same log",
	          servoLogStart(nowMs()) && logStatus().fileNumber == s.fileNumber && sdc_on());
	g_entryWrites.clear();
	checkTrue("SRV00001.CSV open, nothing wrong",
	          s.active && s.fileNumber == 1 && s.fault == SdLogEnd::None && s.how == ServoLogHow::None);

	// Ten seconds: the current steps, a clipped reading, a reset flag, B's
	// pulse changes.
	for (int i = 0; i < 40; i++) {
		g_loop.ua = 100000 + i * 25000;
		g_loop.flags = i == 20 ? SENSE_FLAG_CLIPPED : i == 30 ? SENSE_FLAG_RESET : 0;
		g_st.outUs[1] = (uint16_t)(1000 + i * 25);
		runFor(250);
	}
	g_loop.flags = 0;
	if (fmt == FM_FAT32) {
		// The entry is written by the syncs alone: every 2000 ms from the
		// open, each at the first pass the cache is idle.
		bool every = g_entryWrites.size() >= 4;
		uint32_t last = g_startMs, least = 0xFFFFFFFFu, most = 0;
		for (uint32_t ms : g_entryWrites) {
			uint32_t gap = ms - last;
			if (gap < least) least = gap;
			if (gap > most) most = gap;
			last = ms;
		}
		printf("  (%u syncs, %lu to %lu ms apart)\n", (unsigned)g_entryWrites.size(),
		       (unsigned long)least, (unsigned long)most);
		checkTrue("the entry synced every 2000 ms, 2000 ms the shortest",
		          every && least == 2000 && most <= 2030);
	}
	checkTrue("no reading before the open is a row",
	          g_loop.readings.size() > 0 && g_smp.seq > seqBefore);
	// A pass whose sample is not new adds no row.
	g_loop.read = false;
	runFor(20);
	g_loop.read = true;
	s = logStatus();
	checkInt("one row per reading", (long)s.rows, (long)g_loop.readings.size());
	checkInt("none dropped", (long)s.dropped, 0);

	// STOP sent, not taken yet: the log runs on.
	++g_cmdSeq;
	pass();
	checkTrue("STOP not taken yet: the log runs on", logStatus().active);
	g_st.ackSeq = g_cmdSeq;
	g_st.running = false;
	g_st.outUs[0] = g_st.outUs[1] = 0;
	g_loop.running = false;
	pass();
	s = logStatus();
	checkTrue("STOP taken: closed in the pass that sees it", !s.active && s.how == ServoLogHow::Stop &&
	          s.fault == SdLogEnd::None);
	checkTrue("the cache off", !sdc_on());
	std::string got;
	std::string want = expected(g_loop.readings) + endLine(ServoLogHow::Stop, s.rows, 0, 40);
	checkTrue("SRV00001.CSV: the header, a row per reading, the end line",
	          readFile("/LOGS/SRV00001.CSV", &got) && got == want);
	checkTrue("its first line is the header", got.compare(0, strlen(SERVO_LOG_HEADER), SERVO_LOG_HEADER) == 0);
	checkTrue("the clipped and the reset reading's flags",
	          got.find(",1\n") != std::string::npos && got.find(",2\n") != std::string::npos);
	FILINFO fno;
	DWORD csize = s_fs.csize;
	checkTrue("its size is its text: the preallocation handed back",
	          f_stat("/LOGS/SRV00001.CSV", &fno) == FR_OK && fno.fsize == got.size());
	DWORD used = (DWORD)((fno.fsize + csize * 512u - 1) / (csize * 512u));
	DWORD txtUsed = (DWORD)((txt.size() + csize * 512u - 1) / (csize * 512u));
	DWORD bflUsed = 0;
	if (f_stat("/LOGS/LOG00001.BFL", &fno) == FR_OK) bflUsed = (DWORD)((fno.fsize + csize * 512u - 1) / (csize * 512u));
	checkInt("free clusters: only the three files' own", (long)freeClusters(),
	         (long)(freeBefore - used - txtUsed - bflUsed));

	runFor(100);
	s = logStatus();
	checkTrue("later passes leave the closed log alone", !s.active && s.how == ServoLogHow::Stop &&
	          s.fault == SdLogEnd::None && !sdc_on());
	checkTrue("and the file as it was", readFile("/LOGS/SRV00001.CSV", &got) && got == want);

	// A refused Start: taken, never RUNNING. Closed at once, no rows.
	jobSafe();
	holdStart(false);
	g_st.ackSeq = g_cmdSeq;
	pass();
	s = logStatus();
	checkTrue("a refused START: SRV00002.CSV closed at once", !s.active && s.fileNumber == 2 &&
	          s.how == ServoLogHow::Stop && !sdc_on());
	checkTrue("its header, the pass's reading if one came, the end line",
	          s.rows <= 1 && readFile("/LOGS/SRV00002.CSV", &got) &&
	          got == expected(g_loop.readings) + endLine(ServoLogHow::Stop, s.rows, 0, SUPPLY_UP_NONE));

	// Every other end: BACK, the cuts, the trips, a supply that did not come up.
	struct { bool back, tripped; ServoPowerFault f; ServoLogHow how; const char *what; } ends[] = {
		{ true,  false, ServoPowerFault::None,        ServoLogHow::Back,        "BACK: end=BACK" },
		{ false, true,  ServoPowerFault::Overcurrent, ServoLogHow::Overcurrent, "the hard cut: end=OVERCURRENT" },
		{ false, true,  ServoPowerFault::Stall,       ServoLogHow::Stall,       "the stall cut: end=STALL" },
		{ false, true,  ServoPowerFault::LowSupply,   ServoLogHow::LowSupply,   "the collapse: end=LOW_SUPPLY" },
		{ false, true,  ServoPowerFault::NoSensor,    ServoLogHow::NoSensor,    "the sensor lost: end=NO_SENSOR" },
		{ false, true,  ServoPowerFault::None,        ServoLogHow::Stalled,     "the heartbeat: end=STALLED" },
		{ false, true,  ServoPowerFault::TouchSilent, ServoLogHow::TouchSilent, "touch: end=TOUCH_SILENT" },
		{ false, true,  ServoPowerFault::BusStuck,    ServoLogHow::BusStuck,    "the bus: end=BUS_STUCK" },
		{ false, false, ServoPowerFault::NoStart,     ServoLogHow::NoStart,     "no start: end=NO_START" },
	};
	uint32_t n = 3;
	for (auto &e : ends) {
		jobSafe();
		holdStart();
		runFor(600);
		if (e.back) servoLogBack();
		runFor(100);
		runEnds(e.tripped, e.f);
		pass();
		s = logStatus();
		uint32_t rows = s.rows;
		char path[32];
		snprintf(path, sizeof(path), "/LOGS/SRV%05lu.CSV", (unsigned long)n++);
		checkTrue(e.what, !s.active && s.how == e.how && readFile(path, &got) &&
		          got == expected(g_loop.readings) + endLine(e.how, rows, 0, 40) && !sdc_on());
	}
	// BACK in an earlier log does not carry over.
	jobSafe();
	holdStart();
	runFor(100);
	runEnds();
	pass();
	checkTrue("the next log after BACK: end=STOP", logStatus().how == ServoLogHow::Stop);

	// No sensor declared, or no card: no log, the cache stays off, the
	// blackbox log left alone.
	g_declared = false;
	jobSafe();
	sdLogStart();
	checkTrue("CURRENT SENSE at NONE: no log", !holdStart());
	s = logStatus();
	checkTrue("nothing recorded, the cache off, the blackbox log running",
	          !s.active && s.fault == SdLogEnd::None && s.fileNumber == 0 && !sdc_on() && sdLogActive());
	runFor(100);
	runEnds();
	runFor(50);
	g_declared = true;
	sdLogStop();
	checkTrue("USB DRIVE's release", sdLogRelease());
	jobSafe();
	checkTrue("no card mounted: no log", !holdStart());
	s = logStatus();
	checkTrue("nothing recorded, the cache off", !s.active && s.fault == SdLogEnd::None && !sdc_on());
	runFor(100);
	runEnds();
	runFor(50);
	sdLogReacquire();
	f_mount(nullptr, "", 0);
}

/** @brief The card fills during the run: the rows written are kept, the file closed. */
static void testCardFull() {
	section("Servo log over FatFs, FAT32: the card full during the run");
	format(FM_FAT32, 2u * 1024u * 1024u, 4096);
	cardTimes(1000, 5000);
	loopReset();
	jobSafe();
	f_mkdir("/LOGS");
	runFor(100);
	fill(1);   // one cluster, 4096 bytes: no room for the preallocation
	WORD idFull = sdLogFs()->id;
	checkTrue("HOLD TO START: a log", holdStart());
	g_spotFlushes = 0;
	g_loop.worstQueued = 0;
	ServoLogStatus s = logStatus();
	for (int i = 0; i < 400 && s.fault == SdLogEnd::None; i++) {
		runFor(250);
		s = logStatus();
	}
	checkTrue("CARD FULL, the log still open, the run on", s.active && s.fault == SdLogEnd::CardFull);
	checkTrue("growing a cluster at a time: no call queued more than 4 blocks, none found the queue full",
	          g_loop.worstQueued <= 4 && g_spotFlushes == 0);
	uint32_t rows = s.rows;
	runFor(2000);
	checkInt("no rows counted after it", (long)logStatus().rows, (long)rows);
	checkInt("no cluster free", (long)freeClusters(), 0);
	runEnds();
	pass();
	s = logStatus();
	checkTrue("STOP: closed, CARD FULL kept, the cache off",
	          !s.active && s.fault == SdLogEnd::CardFull && s.how == ServoLogHow::Stop && !sdc_on());
	std::string got;
	std::string want = expected(g_loop.readings);
	checkTrue("SRV00001.CSV: the cluster's 4096 bytes, as the rows gave them, no end line",
	          readFile("/LOGS/SRV00001.CSV", &got) && got.size() == 4096 && want.compare(0, 4096, got) == 0);
	checkTrue("the card left mounted: the blackbox logger ready", escStatus().state == SdLogState::Idle);
	checkTrue("and not mounted again", sdLogFs()->id == idFull);

	// The next START: the fault cleared, the card still full.
	jobSafe();
	checkTrue("the next START opens SRV00002.CSV", holdStart() && logStatus().fault == SdLogEnd::None &&
	          logStatus().fileNumber == 2);
	runFor(2000);
	checkTrue("its first block finds the card full", logStatus().fault == SdLogEnd::CardFull);
	runEnds();
	pass();
	checkTrue("closed, empty", !logStatus().active && readFile("/LOGS/SRV00002.CSV", &got) && got.empty());
	f_mount(nullptr, "", 0);
}

/** @brief The card is removed during the run: the writing stops, the card is mounted again at the end. */
static void testRemoved() {
	section("Servo log over FatFs, FAT32: the card removed during the run");
	format(FM_FAT32, 2u * 1024u * 1024u, 4096);
	cardTimes(1000, 5000);
	loopReset();
	jobSafe();
	runFor(100);
	holdStart();
	runFor(5000);   // past the first sync at 2000 ms
	uint32_t before = logStatus().rows;
	g_disk.removed = true;
	runFor(1000);
	ServoLogStatus s = logStatus();
	checkTrue("the swap check fails: WRITE FAILED, the run on", s.active && s.fault == SdLogEnd::WriteFailed);
	checkTrue("the cache Failed until the end", cardCacheState() == CardCache::Failed);
	checkTrue("no row counted after it", s.rows <= before + 41);
	uint32_t rows = s.rows;
	runFor(1000);
	checkInt("none counted later", (long)logStatus().rows, (long)rows);
	runEnds();
	pass();
	s = logStatus();
	checkTrue("STOP: closed, WRITE FAILED kept, the cache off",
	          !s.active && s.fault == SdLogEnd::WriteFailed && !sdc_on());
	checkTrue("mounted again: no card, the blackbox logger says so",
	          escStatus().state == SdLogState::NoCard);
	g_disk.removed = false;
	checkTrue("the card back, RETRY MOUNT mounts it", sdLogRemount());
	std::string got;
	FILINFO fno;
	std::string want = expected(g_loop.readings);
	checkTrue("SRV00001.CSV at its preallocated size, as after a power loss",
	          f_stat("/LOGS/SRV00001.CSV", &fno) == FR_OK && fno.fsize == SERVO_LOG_PREALLOC_BYTES &&
	          readFile("/LOGS/SRV00001.CSV", &got));
	checkTrue("the blocks written before the removal, the rows as given, then zeros",
	          writtenThenZeros(got, want, 4096));
	jobSafe();
	checkTrue("the next START: SRV00002.CSV", holdStart() && logStatus().fileNumber == 2);
	runFor(500);
	runEnds();
	pass();
	checkTrue("whole", readFile("/LOGS/SRV00002.CSV", &got) &&
	          got == expected(g_loop.readings) + endLine(ServoLogHow::Stop, logStatus().rows, 0, 40));
	f_mount(nullptr, "", 0);
}

/** @brief A block the card does not take: the writing stops, the card is mounted again. */
static void testWriteError() {
	section("Servo log over FatFs, FAT32: a write error during the run");
	format(FM_FAT32, 2u * 1024u * 1024u, 4096);
	cardTimes(1000, 5000);
	loopReset();
	jobSafe();
	runFor(100);
	holdStart();
	runFor(3000);
	g_disk.failWrites = true;
	runFor(1000);
	g_disk.failWrites = false;
	ServoLogStatus s = logStatus();
	checkTrue("the block's status: WRITE FAILED, the run on", s.active && s.fault == SdLogEnd::WriteFailed);
	runEnds();
	pass();
	s = logStatus();
	checkTrue("STOP: closed, WRITE FAILED kept, the cache off",
	          !s.active && s.fault == SdLogEnd::WriteFailed && !sdc_on());
	checkTrue("the card mounted again: the blackbox logger ready", escStatus().state == SdLogState::Idle);
	std::string got;
	checkTrue("SRV00001.CSV: the blocks before the failed one, the rows as given, then zeros",
	          readFile("/LOGS/SRV00001.CSV", &got) && got.size() == SERVO_LOG_PREALLOC_BYTES &&
	          writtenThenZeros(got, expected(g_loop.readings), 3000));
	jobSafe();
	checkTrue("the next START: SRV00002.CSV, nothing wrong", holdStart() && logStatus().fileNumber == 2 &&
	          logStatus().fault == SdLogEnd::None);
	runFor(500);
	runEnds();
	pass();
	checkTrue("whole", readFile("/LOGS/SRV00002.CSV", &got) &&
	          got == expected(g_loop.readings) + endLine(ServoLogHow::Stop, logStatus().rows, 0, 40));

	// The write error at the close: the flush fails, the card mounted again.
	jobSafe();
	holdStart();
	runFor(500);
	runEnds();
	g_disk.failWrites = true;
	pass();
	g_disk.failWrites = false;
	s = logStatus();
	checkTrue("the close's flush fails: WRITE FAILED, the cache off",
	          !s.active && s.fault == SdLogEnd::WriteFailed && !sdc_on() &&
	          escStatus().state == SdLogState::Idle);
	f_mount(nullptr, "", 0);
}

/** @brief Read @p path whole through FatFs: its blocks fill the cache's read cache. */
static void readThrough(const char *path) {
	std::string junk;
	readFile(path, &junk);
}

/**
 * @brief A block FatFs needs fails to read while the cache is healthy: the
 *        directory's block, pushed out of the read cache and the FAT window
 *        by another file's 24 blocks. During the run the sync fails; at the
 *        end the close does. Either is WRITE FAILED, and the card is mounted
 *        again.
 */
static void testReadFails() {
	section("Servo log over FatFs, FAT32: a block FatFs needs does not read, the cache healthy");
	for (int atClose = 0; atClose < 2; atClose++) {
		format(FM_FAT32, 2u * 1024u * 1024u, 4096);
		cardTimes(1000, 5000);
		loopReset();
		jobSafe();
		std::string big(24 * 512, 'b');
		put("/BIG.BIN", big.c_str());
		runFor(100);
		g_entryLba = 0;
		holdStart();
		runFor(atClose ? 2500 : 500);   // the first sync, at 2000 ms, writes the entry
		uint32_t dir = g_entryLba;
		checkTrue("the entry's block known", dir != 0 || !atClose);
		if (!atClose) {
			// The open's sync wrote it: find the block holding the name.
			for (auto &kv : g_disk.images[g_disk.card])
				if (memmem(kv.second.data(), 512, "SRV00001CSV", 11)) dir = (uint32_t)kv.first;
		}
		WORD id = sdLogFs()->id;
		readThrough("/BIG.BIN");
		g_card.failLba = dir;
		g_card.failRead = true;
		if (!atClose) {
			runFor(2000);
			ServoLogStatus s = logStatus();
			checkTrue("the sync's read fails: WRITE FAILED, the run on, the cache healthy",
			          s.active && s.fault == SdLogEnd::WriteFailed && cardCacheState() != CardCache::Failed);
		}
		runEnds();
		pass();
		g_card.failRead = false;
		ServoLogStatus s = logStatus();
		checkTrue(atClose ? "the close's read fails: WRITE FAILED, the cache off"
		                  : "STOP: WRITE FAILED kept, no close tried, the cache off",
		          !s.active && s.fault == SdLogEnd::WriteFailed && !sdc_on());
		checkTrue("the card mounted again", sdLogFs()->id != id && escStatus().state == SdLogState::Idle);
		std::string got;
		checkTrue("no end line on the card", readFile("/LOGS/SRV00001.CSV", &got) &&
		          got.find("# end=") == std::string::npos);
		f_mount(nullptr, "", 0);
	}
}

/**
 * @brief The run's card work: the cache's queue, the reads a pass makes, the
 *        core0 time, and a full ring, on a card at the specification's
 *        limits.
 * @param slow A card at the limits: a block read in 100 ms, a block
 *        programmed in 511 ms. Else 1 ms and 5 ms.
 */
static void testCache(bool slow) {
	section(slow ? "Servo log over FatFs, FAT32: the cache, a card at 100 ms a read and 511 ms a block"
	             : "Servo log over FatFs, FAT32: the cache, a card at 1 ms a read and 5 ms a block");
	format(FM_FAT32, 2u * 1024u * 1024u, 4096);
	uint32_t readUs = slow ? 100000 : 1000;
	cardTimes(readUs, slow ? 511000 : 5000);
	loopReset();
	jobSafe();
	runFor(100);
	holdStart();
	// The open may wait for the card: it runs before the Start command.
	g_spotFlushes = 0;
	unsigned reads0 = g_card.reads;
	g_loop.worstPassUs = g_loop.worstGapUs = 0;
	g_loop.worstReads = g_loop.worstQueued = 0;
	g_loop.lastReadingUs = g_us;
	runFor(120000);   // two minutes
	ServoLogStatus s = logStatus();
	checkTrue("the cache on throughout", sdc_on() && s.active && s.fault == SdLogEnd::None);
	checkInt("no write of the run found the queue full", (long)g_spotFlushes, 0);
	checkTrue("no call of the run queued more than 2 blocks: a block of rows, or the sync's entry and FSINFO",
	          g_loop.worstQueued <= 2);
	checkTrue("no pass read the card more than once", g_loop.worstReads <= 1);
	printf("  (worst pass %llu us, worst reading gap %llu us, card reads %u, blocks %u, "
	       "most blocks a call queued %u)\n",
	       (unsigned long long)g_loop.worstPassUs, (unsigned long long)g_loop.worstGapUs,
	       g_card.reads - reads0, g_card.writes, g_loop.worstQueued);
	checkTrue("no pass waited for the busy line: at most a read, a transfer and the pass",
	          g_loop.worstPassUs <= readUs + 100 + 10);
	checkTrue("a reading at least every 25 ms and one pass", g_loop.worstGapUs <= 25000 + readUs + 250);
	checkTrue("no cut by the guard during the run", g_loop.cut == GuardCut::None);
	if (slow) {
		checkTrue("the card took fewer than the rows: rows dropped and counted", s.dropped > 0);
	} else {
		checkInt("a card that keeps up: none dropped", (long)s.dropped, 0);
	}
	runEnds();
	pass();
	s = logStatus();
	std::string got;
	readFile("/LOGS/SRV00001.CSV", &got);
	size_t lines = 0;
	for (char c : got) lines += c == '\n';
	checkInt("the file's rows: those not dropped, the header and the end line",
	         (long)lines, (long)(s.rows - s.dropped + 2));
	char tail[64];
	snprintf(tail, sizeof(tail), " dropped=%lu up_ms=40\n", (unsigned long)s.dropped);
	checkTrue("the end line says how many", got.size() > strlen(tail) &&
	          got.compare(got.size() - strlen(tail), strlen(tail), tail) == 0);
	checkTrue("closed, the cache off", !s.active && s.fault == SdLogEnd::None && !sdc_on());
	f_mount(nullptr, "", 0);
}

/**
 * @brief The hard cut with the log on and off: a step to 3.2 A, 200 times at
 *        every phase, cut at the first reading after it. With the log on a
 *        reading waits at most for the one card read a pass makes.
 * @param readUs One block read: 1 ms, or the specification's 100 ms.
 * @param busyUs One block programmed: 5 ms, or 511 ms.
 */
static void testCutOnTime(uint32_t readUs, uint32_t busyUs) {
	char t[120];
	snprintf(t, sizeof(t), "Servo log over FatFs, FAT32: the hard cut with the log on, %lu ms a read, "
	         "%lu ms a block", (unsigned long)(readUs / 1000), (unsigned long)(busyUs / 1000));
	section(t);
	uint64_t delay[2] = {0, 0}, worstPass[2] = {0, 0}, worstGap[2] = {0, 0};
	for (int on = 0; on < 2; on++) {
		format(FM_FAT32, 2u * 1024u * 1024u, 4096);
		cardTimes(readUs, busyUs);
		loopReset();
		g_loop.logging = on == 1;
		jobSafe();
		runFor(100);
		g_declared = on == 1;   // without a sensor declared no log opens
		holdStart();
		g_declared = true;
		checkTrue(on ? "the log open" : "no log", logStatus().active == (on == 1));
		g_loop.worstPassUs = g_loop.worstGapUs = 0;
		g_loop.lastReadingUs = g_us;
		// 200 steps of 3.2 A, 1009.137 ms apart from the first, so they fall
		// at every phase of the UI frame and of the card's read, transfer and
		// busy time, the ring filling and full.
		uint64_t worst = 0;
		uint64_t t0 = g_us;
		for (int k = 1; k <= 200; k++) {
			// The step comes at its time, during a pass too: core0 may be
			// in the card's read then, and takes the reading after it.
			uint64_t at = t0 + (uint64_t)k * 1009137u;
			while (g_us < at) pass();
			g_loop.ua = 3200000;
			g_loop.cut = GuardCut::None;
			while (g_loop.cut == GuardCut::None && g_us < at + 1000000) pass();
			if (g_loop.cut != GuardCut::Hard) worst = 1000000;
			else if (g_loop.cutUs - at > worst) worst = g_loop.cutUs - at;
			g_loop.ua = 120000;
			supplyGuardArm(&g_loop.guard, &g_smp, nowMs());
		}
		delay[on] = worst;
		worstPass[on] = g_loop.worstPassUs;
		worstGap[on] = g_loop.worstGapUs;
		g_loop.logging = true;
		runEnds();
		pass();
		f_mount(nullptr, "", 0);
	}
	printf("  (the cut after the step at most: log off %llu us, log on %llu us; "
	       "worst pass with the log %llu us, worst reading gap %llu us)\n",
	       (unsigned long long)delay[0], (unsigned long long)delay[1],
	       (unsigned long long)worstPass[1], (unsigned long long)worstGap[1]);
	checkTrue("log off: the hard cut within a frame of the step", delay[0] <= 25000 + 100);
	checkTrue("log on: within a frame, a read, a transfer and a look at the card",
	          delay[1] <= 25000 + 100 + readUs + 100 + 10);
	checkTrue("log on: no pass waits for the busy line", worstPass[1] <= readUs + 100 + 10);
	checkTrue("log on: a reading at least every 250 ms, the heartbeat's and the sensor-lost cut's time",
	          worstGap[1] < 250000);
}

int main() {
	testPure();
	testNames("FAT32", FM_FAT32, 4096);
	testNames("exFAT", FM_EXFAT, 32768);
	testRun("FAT32", FM_FAT32, 4096);
	testRun("exFAT", FM_EXFAT, 32768);
	testCardFull();
	testRemoved();
	testWriteError();
	testReadFails();
	testCache(false);
	testCache(true);
	testCutOnTime(1000, 5000);
	testCutOnTime(100000, 511000);
	printf("\n=======================\n");
	if (g_failures) printf("%d CHECK(S) FAILED\n", g_failures);
	else printf("all checks passed\n");
	return g_failures ? 1 : 0;
}
