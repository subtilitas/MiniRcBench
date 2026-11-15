/**
 * @file clock_line.h
 * @brief The clock's line command on the USB serial port: `CLOCK` and the
 *        local time sets the board's clock, `CLOCK?` reads it.
 *
 * usbDevTask() hands the bytes the host sent to clockLinePoll() on core0, in
 * its own pass of the loop, without waiting for input: at most one line a
 * pass, so one set at most. A line ends at a carriage return or a line
 * feed; an empty line is skipped, so CR LF ends one line. Lines that do not
 * start with `CLOCK` are ignored and get no answer. The rest are parsed
 * strictly, byte for byte, and answered with one line:
 *
 * | Sent | Answer |
 * |---|---|
 * | `CLOCK 2026-11-14 15:42:07` | `CLOCK SET 2026-11-14 15:42:07` |
 * | `CLOCK?` | `CLOCK 2026-11-14 15:42:09` or `CLOCK NOT SET` |
 * | a date or time out of range, or a day the month does not have | `CLOCK REFUSED: BAD DATE` |
 * | anything else that starts with `CLOCK` | `CLOCK REFUSED: BAD LINE` |
 * | no answer at 0x51, a failed transfer, the bus stuck | `CLOCK REFUSED: RTC NOT ANSWERING` |
 * | the read-back is not the time sent | `CLOCK REFUSED: RTC READ-BACK DIFFERS` |
 *
 * The time is local time, no time zone and no daylight saving
 * (docs/design/run-summary.md, decision 7); `tools/set_clock.py` sends the
 * computer's. Each answer ends with CR LF, as the telemetry lines do.
 *
 * @see docs/design/run-summary.md, "Setting it"
 */

#pragma once

#include "clock.h"

#include <stddef.h>
#include <stdint.h>

/** @brief Characters of a line kept; a longer line starting with `CLOCK` is a bad line. */
#define CLOCK_LINE_MAX 32
/** @brief Bytes of the longest answer, CR LF and the NUL included. */
#define CLOCK_ANSWER_BYTES 48

/** @brief What a line asks. */
enum class ClockCmd : uint8_t {
	Ignore = 0,  /**< Not a clock line: no answer. */
	Query,       /**< `CLOCK?`. */
	Set,         /**< `CLOCK YYYY-MM-DD HH:MM:SS`, a valid time. */
	BadLine,     /**< Starts with `CLOCK` and is neither form. */
	BadDate,     /**< The set's form, with a time clockValid() refuses. */
};

/** @brief A line as it arrives. Zero-initialised: empty. */
struct ClockLineIn {
	char    buf[CLOCK_LINE_MAX];  /**< The line's first characters. */
	uint8_t len;                  /**< Characters in buf. */
	bool    over;                 /**< More arrived than buf holds. */
};

/**
 * @brief What @p line asks. Pure.
 * @param line The line, without its end; not NUL-terminated.
 * @param len  Its characters, at most @ref CLOCK_LINE_MAX.
 * @param over The line was longer than @p len characters.
 * @param t    The time, filled in for @ref ClockCmd::Set only.
 * @return The command.
 */
ClockCmd clockLineParse(const char *line, size_t len, bool over, ClockTime *t);

/**
 * @brief Carry out @p cmd and write its answer: clockSet() for a set,
 *        clockNow() for a query.
 * @param cmd The command.
 * @param t   The time of a set.
 * @param out At least @ref CLOCK_ANSWER_BYTES bytes.
 * @return The answer's length, CR LF included; 0 for @ref ClockCmd::Ignore.
 */
size_t clockLineAnswer(ClockCmd cmd, const ClockTime *t, char *out);

/**
 * @brief Read bytes from @p get until a line ends or @p get has none, and
 *        answer a line that ended. Returns after the first line that ends,
 *        so a pass carries out one command at most; the bytes after it wait
 *        for the next pass.
 * @param in  The line so far, kept between calls.
 * @param get The next byte, 0 to 255, or a negative number for none now.
 * @param out At least @ref CLOCK_ANSWER_BYTES bytes.
 * @return The answer's length; 0 for no answer.
 */
size_t clockLinePoll(ClockLineIn *in, int (*get)(), char *out);
