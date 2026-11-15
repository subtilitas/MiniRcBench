/**
 * @file sbus_view.h
 * @brief What the S.BUS MONITOR screen shows, as pure functions of the job's
 *        status and the time: the link state, how the channels are drawn,
 *        and the text of each line.
 *
 * S.BUS is Futaba's serial bus for receiver channels (sbus_job.h). The
 * screen (ui_sbus.h) reads SbusStatus once per UI frame and draws what these
 * functions return; nothing here touches the panel or the clock, so the host
 * suite checks every rule at its boundaries.
 *
 * The link state, first matching row wins (docs/design/rx-signals.md,
 * "The job"):
 *
 * | State | Rule | Chip |
 * |---|---|---|
 * | `NO SIGNAL` | no frame and no clean byte for @ref SBUS_LOST_MS, or none since the bind | `C_AMBER` |
 * | `NO FRAMES` | a clean byte within it, no frame within it | `C_AMBER` |
 * | `FAILSAFE` | the last frame, within it, carries the failsafe bit | `C_RED` |
 * | `FRAME LOST` | a frame with the frame-lost bit seen within @ref SBUS_VIEW_LOST_HOLD_MS | `C_AMBER` |
 * | `OK` | otherwise | `C_GREEN` |
 *
 * Ages are differences of `millis()` values, so a wrap of the clock changes
 * nothing. A count of 0 means "none since the bind", whatever the stamp says.
 *
 * @see docs/design/rx-signals.md, "Screen"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sbus_job.h"

/**
 * @brief How long a frame with the frame-lost bit keeps `FRAME LOST` up, ms:
 *        one such frame shows on a 40 Hz screen.
 */
#define SBUS_VIEW_LOST_HOLD_MS 250u
/** @brief The highest count a count line shows; more shows this. */
#define SBUS_VIEW_COUNT_MAX 9999u
/** @brief The highest frame count `FRAMES` shows; more shows this. */
#define SBUS_VIEW_FRAMES_MAX 999999u
/** @brief The highest rate the rate line shows, in tenths of a hertz: 999.9 Hz. */
#define SBUS_VIEW_RATE_MAX 9999u

/**
 * @defgroup sbus_view_text The link chip's texts
 * @{
 */
#define SBUS_VIEW_NO_SIGNAL  "NO SIGNAL"   /**< 9 characters. */
#define SBUS_VIEW_NO_FRAMES  "NO FRAMES"   /**< 9 characters. */
#define SBUS_VIEW_FAILSAFE   "FAILSAFE"    /**< 8 characters. */
#define SBUS_VIEW_FRAME_LOST "FRAME LOST"  /**< 10 characters, the widest. */
#define SBUS_VIEW_OK         "OK"          /**< 2 characters. */
/** @} */

/** @brief The link state. See the table in this file's description. */
enum class SbusLink : uint8_t {
	NoSignal,   /**< Nothing on the line. */
	NoFrames,   /**< Bytes at 100,000 baud 8E2, no frame the decoder takes. */
	Failsafe,   /**< The receiver reports failsafe. */
	FrameLost,  /**< A frame-lost frame within the hold. */
	Ok,         /**< Frames arrive. */
};

/** @brief How the channel bars and values are drawn. */
enum class SbusInk : uint8_t {
	Empty,     /**< No frame since the bind: outlines and `----`. */
	Live,      /**< The last frame is current: `UI_COL_LIVE` bars, `C_TEXT` values. */
	Failsafe,  /**< The receiver's failsafe values: `C_RED`. */
	Stale,     /**< No frame for @ref SBUS_LOST_MS, 100 ms; the last frame in `C_DIM`. */
};

/**
 * @brief Core0's memory of frame-lost frames, for the hold of `FRAME LOST`.
 *        Zero it at each visit and after BACK TO MONITOR.
 */
struct SbusLostHold {
	uint32_t lostFrames;  /**< SbusStatus::lostFrames as last seen. */
	uint32_t atMs;        /**< millis() of the step that saw it rise. */
	bool     held;        /**< A rise within @ref SBUS_VIEW_LOST_HOLD_MS. */
};

/**
 * @brief Note a rise of SbusStatus::lostFrames, once per UI frame, before
 *        sbusLink(). Pure.
 *
 * A rise starts the hold at @p nowMs; a fall, a new bind's cleared count,
 * only takes the new count. The hold ends @ref SBUS_VIEW_LOST_HOLD_MS after
 * it started.
 *
 * @param[in,out] h The memory.
 * @param st        The job's status.
 * @param nowMs     millis().
 */
void sbusLostHoldStep(SbusLostHold *h, const SbusStatus *st, uint32_t nowMs);

/**
 * @brief The link state. Pure.
 * @param st    The job's status.
 * @param h     The frame-lost memory, stepped this frame.
 * @param nowMs millis().
 * @return The first state whose rule holds.
 */
SbusLink sbusLink(const SbusStatus *st, const SbusLostHold *h, uint32_t nowMs);

/**
 * @brief The chip's text for a link state.
 * @param l The state.
 * @return `NO SIGNAL`, `NO FRAMES`, `FAILSAFE`, `FRAME LOST` or `OK`.
 */
const char *sbusLinkText(SbusLink l);

/**
 * @brief The chip's fill for a link state, from the colour roles of
 *        ui_widgets.h: `UI_COL_WARN`, `UI_COL_ON` for FAILSAFE,
 *        `UI_COL_SAFE` for OK.
 * @param l The state.
 * @return The colour.
 */
uint16_t sbusLinkFill(SbusLink l);

/**
 * @brief How the channels are drawn. Pure.
 * @param st The job's status.
 * @param l  The link state.
 * @return Empty before the first frame; Stale while no frame arrived for
 *         @ref SBUS_LOST_MS (NO SIGNAL and NO FRAMES); Failsafe; else Live.
 */
SbusInk sbusInk(const SbusStatus *st, SbusLink l);

/**
 * @brief The fill of a bar for a raw value, in raw units. Pure.
 * @param raw    0 to 2047; more counts as 2047.
 * @param widthPx The fill at raw 2047, px.
 * @return raw x @p widthPx / 2047, rounded down.
 */
int sbusBarFill(uint16_t raw, int widthPx);

/**
 * @brief A channel's value: µs by rcbench's sbus_to_us(), or `----`
 *        before the first frame. 3 or 4 characters.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 8 bytes or more.
 * @param st       The job's status.
 * @param ch       Channel 0 to 15.
 */
void sbusValueText(char *buf, size_t n, const SbusStatus *st, unsigned ch);

/**
 * @brief Line 1's left part: `CH17 OFF  CH18 ON`; `CH17 --  CH18 --` before
 *        the first frame. At most 18 characters.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 24 bytes or more.
 * @param st       The job's status.
 */
void sbusDigitalText(char *buf, size_t n, const SbusStatus *st);

/**
 * @brief Line 1's right part: the mean interval and the frame rate,
 *        `14.0 MS  71.4 HZ`; `-- MS  -- HZ` while there is no interval. At
 *        most 18 characters: `100.0 MS  999.9 HZ`.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 24 bytes or more.
 * @param st       The job's status.
 */
void sbusRateText(char *buf, size_t n, const SbusStatus *st);

/**
 * @brief Line 2's left part: `FRAMES 123456`, at most 13 characters.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 16 bytes or more.
 * @param st       The job's status.
 */
void sbusFramesText(char *buf, size_t n, const SbusStatus *st);

/**
 * @brief Line 2's right part: frames with the frame-lost and the failsafe
 *        bit, `LOST 12  FS 3`, at most 18 characters.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 24 bytes or more.
 * @param st       The job's status.
 */
void sbusLostText(char *buf, size_t n, const SbusStatus *st);

/**
 * @brief Line 3: the UART's error counts, `PAR 0 FRM 0 BRK 0 OVR 0`, at most
 *        35 characters.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 40 bytes or more.
 * @param st       The job's status.
 */
void sbusErrorText(char *buf, size_t n, const SbusStatus *st);

/**
 * @brief Line 4: the decoder's counts, `RESYNC 2 FOOTER 0`, at most 23
 *        characters.
 * @param[out] buf Room for the text.
 * @param n        Size of @p buf, 32 bytes or more.
 * @param st       The job's status.
 */
void sbusDecoderText(char *buf, size_t n, const SbusStatus *st);
