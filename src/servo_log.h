/**
 * @file servo_log.h
 * @brief The servo tester's log, `LOGS/SRVnnnnn.CSV`: one row per reading
 *        of the current sensor from HOLD TO START to the run's end.
 *
 * Core0 only. HOLD TO START opens the file before it sends the Start
 * command (servoLogStart(), servoLogFollow()); servoLogPoll(), on every pass
 * of core0's loop after cardCachePoll(), turns each new reading into a row
 * in a RAM ring (log_ring.h), sends the ring to the card and closes the file
 * once the job reports the run over: STOP, a trip, a cut, a supply that did
 * not come up, or BACK.
 *
 * @section servolog_card The card while a log runs
 *
 * The write cache (card_cache.h) is on from the open to the close, so card
 * writes return once queued. During the run each pass makes at most one
 * FatFs call, an f_write() of one 512-byte block or an f_sync(), and only
 * while the cache reads Idle: nothing queued, nothing in flight. The call
 * then queues one block of rows, or the sync's directory entry; past the
 * preallocation, where the file grows a cluster at a time, the sync adds
 * FSINFO and the FAT's block in both copies: at most 4 of the cache's 32. No
 * write of the run waits for the card's busy line; a pass reads the card at
 * most once, for the cache's swap check at a batch's first block, or for a
 * block FatFs asks for that neither its window nor the read cache holds. Everything else that touches the card, the open with its
 * directory scan and preallocation, the last rows, the end line, the
 * truncation, the close and the cache's last flush, runs with the pulses at
 * level 0 and the switched supply off: before the Start command and after the
 * job has reported the run over.
 *
 * Core1 runs the pulses, the cuts and the move timing and calls nothing here.
 *
 * @see docs/design/current-sense.md, "Slice 6 as built"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sd_log.h"
#include "servo_job.h"

/** @brief The file's first line: its columns. */
#define SERVO_LOG_HEADER "ms,pulse_a_us,pulse_b_us,current_ma,bus_mv,flags\n"
/** @brief The longest row, its line feed included: every field at its widest. */
#define SERVO_LOG_ROW_MAX 48u
/** @brief The longest end line, its line feed included. */
#define SERVO_LOG_END_MAX 80u
/**
 * @brief The RAM ring between the rows and the card, bytes. A multiple of
 *        the 512-byte block, which servo_log.cpp asserts: about 500 rows,
 *        12 s of rows when the card takes none.
 */
#define SERVO_LOG_RING_BYTES 16384u
/**
 * @brief The file's preallocation at the open, bytes (f_expand()): about
 *        50 minutes of rows at 40 rows a second. The close hands back what
 *        the rows did not use (f_truncate()).
 */
#define SERVO_LOG_PREALLOC_BYTES (4u * 1024u * 1024u)

/** @brief How a servo log ended: the end line's `end=`. */
enum class ServoLogHow : uint8_t {
	None,         /**< No log ended yet, or one runs. */
	Stop,         /**< STOP, on SERVO or on SET. */
	Back,         /**< BACK: the claim's end. */
	Stalled,      /**< The heartbeat trip: the screen stalled. */
	TouchSilent,  /**< The touch controller did not answer for 250 ms. */
	BusStuck,     /**< The sensor found the shared bus stuck. */
	Overcurrent,  /**< The hard cut. */
	Stall,        /**< The stall cut. */
	LowSupply,    /**< The collapse cut. */
	NoSensor,     /**< The sensor-lost cut. */
	NoStart,      /**< The switched supply did not come up. */
};

/** @brief What the screen shows of the log. Copied out by servoLogStatus(). */
struct ServoLogStatus {
	bool        active;      /**< A log is open: from HOLD TO START to the run's end. */
	uint32_t    fileNumber;  /**< N of `SRVnnnnn.CSV`: the open log's, or the last one's; 0 for none. */
	uint32_t    rows;        /**< Readings turned into rows while the log wrote, dropped ones included. */
	uint32_t    dropped;     /**< Rows the ring had no room for. */
	/**
	 * @brief Why the log stopped writing or did not start: SdLogEnd::None,
	 *        CardFull, WriteFailed (a write, the close or the cache's flush
	 *        failed: a card removed, swapped or faulty), OpenFailed or
	 *        NoFreeNumber (`SRV99999.CSV` in `LOGS/`). Kept until the next
	 *        HOLD TO START.
	 */
	SdLogEnd    fault;
	ServoLogHow how;         /**< How the last log ended; None while one runs. */
};

/**
 * @brief One row: the reading's time since the open, both pulses, the
 *        current, the bus voltage and the reading's flags. Pure.
 *
 * `12345,1500,0,420,5020,0` and a line feed. The current is rounded to the
 * nearest mA, half away from zero; a pulse of 0 is a pin at level 0 or a
 * servo not bound.
 *
 * @param buf    Where the row goes; at least @ref SERVO_LOG_ROW_MAX bytes.
 * @param n      Its size.
 * @param ms     The reading's takenMs less the open's millis().
 * @param aUs    SIG's pulse, ServoStatus::outUs[0], us.
 * @param bUs    AUX's pulse, us.
 * @param ua     SenseSample::currentUa.
 * @param mv     SenseSample::busMv.
 * @param flags  SenseSample::flags: 1 clipped, 2 the first reading after a reset.
 * @return The row's length; @p n or more for a cut row.
 */
size_t servoLogRowText(char *buf, size_t n, uint32_t ms, uint16_t aUs, uint16_t bUs,
                       int32_t ua, uint16_t mv, uint8_t flags);

/**
 * @brief The end line, the file's last: `# end=STOP rows=2400 dropped=0
 *        up_ms=40` and a line feed. `up_ms=-` without a start-up time. Pure.
 * @param buf     Where the line goes; at least @ref SERVO_LOG_END_MAX bytes.
 * @param n       Its size.
 * @param how     How the run ended.
 * @param rows    ServoLogStatus::rows.
 * @param dropped ServoLogStatus::dropped.
 * @param upMs    ServoStatus::upMs: @ref SUPPLY_UP_NONE for none.
 * @return The line's length; @p n or more for a cut line.
 */
size_t servoLogEndText(char *buf, size_t n, ServoLogHow how, uint32_t rows,
                       uint32_t dropped, uint16_t upMs);

/** @brief The end line's name of @p how: `STOP`, `LOW_SUPPLY`, ... Pure. */
const char *servoLogHowName(ServoLogHow how);

/**
 * @brief How the run ended, from the job's status once it is over: a trip by
 *        its cause, then a supply that did not come up, then BACK if
 *        @p back, else STOP. Pure.
 * @param st   The job's status.
 * @param back BACK was tapped during the log (servoLogBack()).
 * @return How it ended.
 */
ServoLogHow servoLogHowOf(const ServoStatus *st, bool back);

/**
 * @brief Whether the run the log follows is over: the job has taken the
 *        Start command of @p startSeq, or a later one, and is not RUNNING.
 *        A refused Start is over at once. Pure.
 * @param st       The job's status.
 * @param startSeq The seq servoJobCommand() returned for the Start.
 * @return True once the run is over.
 */
bool servoLogRunOver(const ServoStatus *st, uint16_t startSeq);

/**
 * @brief Open `LOGS/SRVnnnnn.CSV` for a run: at HOLD TO START, before the
 *        Start command.
 *
 * Nothing without CURRENT SENSE at INA219, which gives the readings, or
 * without a mounted card (cardMounted()). Otherwise the write cache goes on
 * (cardCacheBegin()), which stops a running blackbox log as SERVO TESTER's
 * (SdLogEnd::ByServo), and the file is created: N one above the highest
 * `SRVnnnnn.CSV` in `LOGS/`, numbered apart from `LOGnnnnn.BFL`, read once
 * per mount (sd_log_next.h). Its preallocation, @ref SERVO_LOG_PREALLOC_BYTES,
 * is reserved and the header row goes into the ring. A file that does not
 * open turns the cache off again and records the fault.
 *
 * @param nowMs millis(): the rows' time 0.
 * @return True with a log open.
 */
bool servoLogStart(uint32_t nowMs);

/**
 * @brief The Start command's seq, right after servoLogStart(): the log ends
 *        once the job has taken it and is not RUNNING (servoLogRunOver()).
 * @param seq servoJobCommand()'s return for the Start.
 */
void servoLogFollow(uint16_t seq);

/** @brief BACK during the log: its end line reads `end=BACK` unless a trip ended the run. */
void servoLogBack();

/**
 * @brief One pass of the log, after cardCachePoll(). Nothing while no log is
 *        open.
 *
 * A new reading becomes a row in the ring, dropped and counted when the ring
 * has no room. While the cache reads Idle, the ring's oldest 512 bytes go to
 * the file, or the file is synced every @ref SD_LOG_SYNC_MS. A cache that
 * reads Failed, or a write that fails or finds the card full, stops the
 * writing for the rest of the run and records the fault. Once the run is
 * over: the ring's rest and the end line, the truncation and the close, the
 * cache flushed and turned off; after a failed write the card mounted again
 * (cardCacheRecover()).
 *
 * @param nowMs millis().
 */
void servoLogPoll(uint32_t nowMs);

/**
 * @brief Read the log's state.
 * @param[out] out Filled in.
 */
void servoLogStatus(ServoLogStatus *out);
