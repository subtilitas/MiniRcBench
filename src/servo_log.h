/**
 * @file servo_log.h
 * @brief The servo tester's log, `LOGS/SRVnnnnn.CSV`: one row per reading
 *        of the current sensor from HOLD TO START to the run's end.
 *
 * Core0 only. HOLD TO START draws `PREPARING THE LOG...`, opens the file in
 * the next UI frame (servoLogStart()) and sends the Start command in the
 * frame after the open has returned, with a fresh heartbeat and a fresh touch
 * read (servoLogFollow()); servoLogPoll(), on every pass
 * of core0's loop after cardCachePoll(), turns each new reading into a row
 * in a RAM ring (log_ring.h), sends the ring to the card and closes the file
 * once the job reports the run over: STOP, a trip, a cut, a supply that did
 * not come up, or BACK.
 *
 * @section servolog_card The card while a log runs
 *
 * The write cache (card_cache.h) is on from the open to the close, so card
 * writes return once queued. During the run each pass makes at most one
 * FatFs call, an f_write() of one whole 512-byte block inside the reserve,
 * and only while the cache reads Idle: nothing queued, nothing in flight.
 * FatFs takes the block's cluster from the fast-seek table made at the open,
 * so the call reads nothing, neither the FAT nor the directory, and queues
 * one block of the cache's 32. The run makes no f_sync(): it would read the
 * directory's block. The cache's swap check before each block is a read
 * started in one pass and collected in a later one (sd_cache.h). So during
 * the run core0 never waits for the card. The open, with its directory scan,
 * reserve, map, sync and the flush of all its blocks to the card, runs before
 * the Start command, the pulses at level 0 and the switched supply off; the
 * last rows, the end line, the truncation, the close and the cache's last
 * flush run after the job has reported the run over. An open given up, too
 * slow or failed, makes no further card call: its blocks go out one flush
 * step a pass during the run, without a wait, and at the run's end the queue
 * is flushed first, then its file is closed and removed, from the card it
 * was opened on only, by the volume's serial number, and the cache goes off.
 * A serial number that does not read leaves the file closed and in place,
 * with no remount.
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
#define SERVO_LOG_ROW_MAX 42u
/** @brief The longest end line, its line feed included. */
#define SERVO_LOG_END_MAX 80u
/**
 * @brief The RAM ring between the rows and the card, bytes. A multiple of
 *        the 512-byte block, which servo_log.cpp asserts: about 600 rows of
 *        about 27 bytes, 15 s of rows when the card takes none.
 */
#define SERVO_LOG_RING_BYTES 16384u
/**
 * @brief The file's reserve at the open, bytes (f_expand()), one contiguous
 *        run: about an hour of rows at 1.1 kB/s, 16 blocks of FAT32 FAT
 *        written in each copy at the open with clusters of 4 KiB. A card
 *        without that run free gets @ref SERVO_LOG_RESERVE_MIN_BYTES. The
 *        close hands back what the rows did not use (f_truncate()).
 */
#define SERVO_LOG_PREALLOC_BYTES (4u * 1024u * 1024u)
/** @brief The smaller reserve: without this much free in one run no log starts. */
#define SERVO_LOG_RESERVE_MIN_BYTES (64u * 1024u)
/**
 * @brief The open's time limit, ms: past it, checked before the search of
 *        the FAT for the reserve and at each step of the open's flush, the
 *        file is given up and the run starts without a log. A single FatFs
 *        call is not cut short.
 */
#define SERVO_LOG_OPEN_MAX_MS 2000u

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
	 *        CardFull (no 64 KiB free in one run at the open, the smaller
	 *        reserve used up, or a write past the reserve at the end that
	 *        found no room), WriteFailed (a write, the close or the cache's flush
	 *        failed: a card removed, swapped or faulty), OpenFailed or
	 *        NoFreeNumber (`SRV99999.CSV` in `LOGS/`). Kept until the next
	 *        HOLD TO START.
	 */
	SdLogEnd    fault;
	/** @brief The rows filled the whole @ref SERVO_LOG_PREALLOC_BYTES, and the writing stopped there. */
	bool        atLimit;
	/** @brief The open took @ref SERVO_LOG_OPEN_MAX_MS or more, and was given up: no log. */
	bool        slow;
	/** @brief A card was mounted and the write cache did not start on it: no log. */
	bool        noCache;
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
 * @brief Whether the open, begun at @p startMs, has reached its limit,
 *        @ref SERVO_LOG_OPEN_MAX_MS, at @p nowMs, across the clock's wrap.
 *        Pure.
 * @param startMs millis() after the cache went on.
 * @param nowMs   millis() now.
 * @return True at the limit or past it.
 */
bool servoLogOpenLate(uint32_t startMs, uint32_t nowMs);

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
 * @brief Whether HOLD TO START opens a log: CURRENT SENSE at INA219 and a
 *        card mounted (cardMounted()).
 * @return True when servoLogStart() would try.
 */
bool servoLogWanted();

/**
 * @brief Open `LOGS/SRVnnnnn.CSV` for a run: in the UI frame after HOLD TO
 *        START drew its note, a frame before the Start command.
 *
 * Nothing without CURRENT SENSE at INA219, which gives the readings, or
 * without a mounted card, where cardCacheBegin() leaves the cache off. A
 * card mounted on which the cache does not start records
 * ServoLogStatus::noCache. Otherwise the write cache goes on
 * (cardCacheBegin()), which stops a running blackbox log as SERVO TESTER's
 * (SdLogEnd::ByServo), the volume's serial number is read
 * (cardVolumeSerial(), OpenFailed when it does not read), and the file is
 * created: N one above the highest `SRVnnnnn.CSV` in `LOGS/`, numbered
 * apart from `LOGnnnnn.BFL`, read once per mount (sd_log_next.h). Its
 * reserve, @ref SERVO_LOG_PREALLOC_BYTES, or
 * @ref SERVO_LOG_RESERVE_MIN_BYTES on a card without that run free, is
 * allocated, mapped for fast seek and synced, every block of that is flushed
 * to the card, and the header row goes into the ring. No 64 KiB in one run
 * (CardFull), a map, sync or flush that fails (OpenFailed), or
 * @ref SERVO_LOG_OPEN_MAX_MS passed (ServoLogStatus::slow), checked before
 * the FAT's search, between the two tries and at each flush step, gives the
 * file up: at the run's end it is removed if the card has the same serial
 * number, else left where it is (servo_log.h, "The card while a log runs").
 *
 * @param nowMs millis(): the rows' time 0.
 * @return True with a log open.
 */
bool servoLogStart(uint32_t nowMs);

/**
 * @brief The Start command's seq, in the frame after servoLogStart(): the
 *        log ends once the job has taken it and is not RUNNING
 *        (servoLogRunOver()); before it the log does not end. A Start not
 *        sent after all gives the job's last acknowledged seq: the log then
 *        ends at the next pass.
 * @param seq servoJobCommand()'s return for the Start.
 */
void servoLogFollow(uint16_t seq);

/**
 * @brief BACK during the log: its end line reads `end=BACK` unless a trip
 *        ended the run. The next servoLogStart() forgets it.
 */
void servoLogBack();

/**
 * @brief One pass of the log, after cardCachePoll(). Nothing while no log is
 *        open.
 *
 * A new reading becomes a row in the ring, dropped and counted when the ring
 * has no room. While the cache reads Idle, the ring's oldest 512 bytes go to
 * the file, inside the reserve. A cache that reads Failed, a write that
 * fails or finds the card full, or the reserve used up stops the writing
 * for the rest of the run and records why. Once the run is over: the
 * ring's rest and the end line, the truncation and the close, the cache
 * flushed and turned off; after a failed write the card mounted again
 * (cardCacheRecover()).
 */
void servoLogPoll();

/**
 * @brief Read the log's state.
 * @param[out] out Filled in.
 */
void servoLogStatus(ServoLogStatus *out);
