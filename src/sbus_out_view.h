/**
 * @file sbus_out_view.h
 * @brief What the S.BUS OUTPUT screen shows, as pure functions of the job's
 *        status and the screen's own choices: the chip, the two notes, each
 *        row's label and value, the step a slider moves in, the throttle
 *        ceiling on the track, and the idle rule.
 *
 * S.BUS is Futaba's serial bus for receiver channels (sbus_out_job.h). The
 * screen (ui_sbus_out.h) reads SbusOutStatus once per UI frame and draws
 * what these functions return; nothing here touches the panel or the clock,
 * so the host suite checks every rule at its boundaries.
 *
 * The notes, first matching row wins (docs/design/sbus-output.md, "Main
 * view"):
 *
 * | State | Note 1 | Note 2 |
 * |---|---|---|
 * | RUNNING | `THROTTLE CH3: 1000 TO 1200 US (20%).`, `C_TEXT` | `CH17 OFF  CH18 OFF  FS OFF  LOST OFF`, `C_TEXT`; @ref UI_COL_WARN while a flag is on |
 * | Stopped by the heartbeat | `STOPPED: SCREEN STALLED`, @ref UI_COL_ALERT | `NEVER TO AN ESC. PROPS OFF.`, @ref UI_COL_WARN |
 * | Encoder refused | `STOPPED: FRAME REFUSED.`, @ref UI_COL_WARN | as above |
 * | Stopped by the idle rule | `STOPPED: NO TOUCH FOR 30 S.`, @ref UI_COL_WARN | as above |
 * | Stopped by a touch that does not answer | `STOPPED: TOUCH DID NOT ANSWER`, @ref UI_COL_WARN | as above |
 * | SAFE | `GP0 GOES TO A FLIGHT CONTROLLER.`, @ref UI_COL_WARN | `NEVER TO AN ESC. PROPS OFF.`, @ref UI_COL_WARN |
 *
 * @see docs/design/sbus-output.md, "Screen"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sbus_out_job.h"

/**
 * @defgroup sbus_out_view_text S.BUS OUTPUT's chip and note texts
 * @{
 */
#define SBUS_OUT_VIEW_SAFE     "SAFE"     /**< The chip while SAFE: 4 characters. */
#define SBUS_OUT_VIEW_RUNNING  "RUNNING"  /**< While frames go out: 7 characters, the widest. */
#define SBUS_OUT_VIEW_STALLED  "STALLED"  /**< After a heartbeat lapse: 7 characters. */
/** @brief SAFE, note 1: 32 characters, 191 px. */
#define SBUS_OUT_VIEW_WARN_1   "GP0 GOES TO A FLIGHT CONTROLLER."
/** @brief Note 2 whenever the frames are off, SET's footer then: 27 characters, 161 px. */
#define SBUS_OUT_VIEW_WARN_2   "NEVER TO AN ESC. PROPS OFF."
/** @brief RUNNING, note 1: throttle channel, top in us, MAX THROTTLE in %; at most 37 characters. */
#define SBUS_OUT_VIEW_CEIL_FMT "THROTTLE CH%u: 1000 TO %u US (%u%%)."
/** @brief After a heartbeat lapse: 23 characters, 137 px, as SERVO TESTER's. */
#define SBUS_OUT_VIEW_STALL    "STOPPED: SCREEN STALLED"
/** @brief After the idle rule: 27 characters, 161 px. */
#define SBUS_OUT_VIEW_IDLE     "STOPPED: NO TOUCH FOR 30 S."
/**
 * @brief After a touch that does not answer (uiTouchLost()): 29 characters,
 *        173 px, as SERVO TESTER's touch cut.
 */
#define SBUS_OUT_VIEW_TOUCH    "STOPPED: TOUCH DID NOT ANSWER"
/** @brief After a frame sbus_encode() refused: 23 characters, 137 px. */
#define SBUS_OUT_VIEW_REFUSED  "STOPPED: FRAME REFUSED."
/** @} */

/** @brief The step a slider moves in, us. */
#define SBUS_OUT_VIEW_STEP_US 5u
/** @brief The slider's left end: raw 172. */
#define SBUS_OUT_VIEW_US_LO 1000u
/** @brief The slider's right end: raw 1811. */
#define SBUS_OUT_VIEW_US_HI 2000u

/** @brief The strip's chip. */
enum class SbusOutChip : uint8_t {
	Safe,     /**< `SAFE` on @ref UI_COL_SAFE; no frames, no trip. */
	Running,  /**< `RUNNING` on @ref UI_COL_ON. */
	Stalled,  /**< `STALLED` on @ref UI_COL_ALERT; a heartbeat lapse stopped the frames. */
};

/** @brief A stop the screen made itself, which the job's status does not carry. */
enum class SbusOutStop : uint8_t {
	None,  /**< No stop of the screen's own since START. */
	Idle,  /**< The idle rule (sbusOutIdleDue()) sent STOP. */
	Touch, /**< A touch that does not answer (uiTouchLost()) sent STOP. */
};

/**
 * @brief The chip for @p st: RUNNING while frames go out, STALLED after a
 *        heartbeat lapse, else SAFE.
 */
SbusOutChip sbusOutChip(const SbusOutStatus *st);

/** @brief The chip's text. */
const char *sbusOutChipText(SbusOutChip c);

/** @brief The chip's fill. */
uint16_t sbusOutChipFill(SbusOutChip c);

/**
 * @brief The two notes under the strip, by the table in this file's
 *        description.
 * @param st          The job's status.
 * @param stop        The screen's own stop since START.
 * @param thrCh       The throttle channel's index, 0 to 3.
 * @param ceilRaw     The throttle ceiling START took, raw.
 * @param maxThrottle `settings()->maxThrottle`, out of 2000.
 * @param[out] n1     Note 1, at least 40 bytes.
 * @param[out] n2     Note 2, at least 40 bytes.
 * @param n           Room in each.
 * @param[out] c1     Note 1's colour.
 * @param[out] c2     Note 2's colour.
 */
void sbusOutNotes(const SbusOutStatus *st, SbusOutStop stop, uint8_t thrCh, uint16_t ceilRaw,
                  uint16_t maxThrottle, char *n1, char *n2, size_t n, uint16_t *c1, uint16_t *c2);

/**
 * @brief Channel @p ch's label: `CH1` to `CH16`, `CH3 THR` on the throttle
 *        channel. At most 7 characters.
 * @return True for the throttle channel, drawn in @ref UI_COL_WARN.
 */
bool sbusOutLabel(char *buf, size_t n, unsigned ch, uint8_t thrCh);

/**
 * @brief Channel @p ch's raw value as the line carries it: the frame last
 *        loaded, or the START frame's before the first frame since the bind
 *        or since START.
 */
uint16_t sbusOutRawOf(const SbusOutStatus *st, unsigned ch, uint8_t thrCh);

/**
 * @brief Channel @p ch's value in us, sbus_to_us() of sbusOutRawOf(): 4
 *        characters, `1000` to `2000`.
 */
void sbusOutValueText(char *buf, size_t n, const SbusOutStatus *st, unsigned ch, uint8_t thrCh);

/**
 * @brief A slider's value in us, as a step of @ref SBUS_OUT_VIEW_STEP_US
 *        from 1000 to 2000, rounded to the nearest, then held to @p topUs.
 * @param value The slider's value.
 * @param topUs The highest the step may be: 2000, or the throttle ceiling's.
 */
uint16_t sbusOutStepUs(float value, uint16_t topUs);

/**
 * @brief The raw value a slider at @p us commands: sbus_from_us(), held to
 *        raw 172 to @p topRaw, which is 1811 or the throttle ceiling.
 */
uint16_t sbusOutRawFor(uint16_t us, uint16_t topRaw);

/**
 * @brief The x of @p us on a track @p w px wide from @p x0: 1000 us at x0,
 *        2000 us at x0 + w - 1, rounded down. The throttle ceiling's mark.
 */
int sbusOutUsX(uint16_t us, int x0, int w);

/**
 * @brief Whether frame @p f is off the START frame where the idle rule
 *        looks: the throttle channel above raw 172, or any of channels 5 to
 *        16, CH17 or CH18 off its START value. An FC armed through a switch
 *        channel idles its motors with the throttle at its stop.
 */
bool sbusOutOffStart(const sbus_frame_t *f, uint8_t thrCh);

/**
 * @brief The idle rule: while @p running and @p f is off the START frame,
 *        more than @ref IDLE_DISARM_MS since the last touch, @p touchMs.
 *        Ages are differences, so a wrap of millis() changes nothing.
 */
bool sbusOutIdleDue(bool running, const sbus_frame_t *f, uint8_t thrCh, uint32_t nowMs,
                    uint32_t touchMs);
