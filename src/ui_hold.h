/**
 * @file ui_hold.h
 * @brief A hold-to-act control: one timer, its progress bar, one firing per
 *        press.
 *
 * HOLD TO WRITE on AM32, HOLD TO START on SERVO, HOLD TO APPLY on SET and
 * HOLD TO SAVE on SETUP run on this. HOLD TO ARM keeps its own timer in
 * ui.cpp: it draws its progress in the status badge and adds a zero-throttle
 * condition.
 *
 * The rules:
 *
 * - **Only a press that landed on the live control counts.** The hold has
 *   to see the press's first frame (`pressed`) with the finger on the
 *   control and the control live. A press that landed elsewhere, landed
 *   while the control was drawn disabled, or whose first frame the hold did
 *   not see, is never timed, however long it stays.
 * - **The timer runs while the finger is on the control.** A finger that
 *   slides off resets it; sliding back on, the same press starts it again
 *   from zero.
 * - **It fires once, @ref UI_HOLD_MS after the timer started.** The frame on
 *   which it has run that long fires; one millisecond less does not.
 * - **A hold that fired needs a new press**, so a finger left on the control
 *   does not fire it again.
 * - **The screen decides when the control is live.** A frame on which it is
 *   not resets the timer and ends the press's claim: the control has to be
 *   live on the frame a press lands, and on every frame after.
 *
 * Pure but for uiHoldDrawBar(), which draws through gfx.h.
 */

#pragma once

#include <stdint.h>

#include "config.h"
#include "ui_widgets.h"

struct TouchState;

/** @brief How long a hold lasts before it fires, ms: the arm hold's length. */
#define UI_HOLD_MS     ARM_HOLD_MS
/** @brief Height of a hold's progress bar, px. */
#define UI_HOLD_BAR_H  3

/** @brief One hold. Zero-initialised, or after uiHoldReset(), it is idle. */
struct UiHold {
	bool     holding;  /**< A press on the control is being timed. */
	bool     live;     /**< The press landed on the live control, has stayed
	                        live and has not fired: it may be timed. */
	uint32_t startMs;  /**< When the timer started. */
};

/**
 * @brief Make the hold idle: no timer, and no press it may time. A finger
 *        already down is not timed; the next press that lands is.
 * @param[out] h The hold.
 */
void uiHoldReset(UiHold *h);

/**
 * @brief Feed one frame of touch to the hold.
 * @param[in,out] h  The hold.
 * @param r          The control.
 * @param t          This frame's touch; null is treated as no touch.
 * @param enabled    The control is live. False resets the timer, and the
 *                   press under way is not timed again.
 * @param nowMs      Current millis().
 * @return true on the one frame the hold fires.
 */
bool uiHoldUpdate(UiHold *h, const UiRect &r, const TouchState *t, bool enabled,
                  uint32_t nowMs);

/**
 * @brief How far the hold has run, for its progress bar.
 * @param h     The hold.
 * @param nowMs Current millis().
 * @return 0 to 100 while the hold runs, capped at 100; 0 when it does not.
 */
int uiHoldPercent(const UiHold *h, uint32_t nowMs);

/**
 * @brief Draw the progress bar: a track of @ref UI_HOLD_BAR_H px across the
 *        control's width, cleared to @ref C_BG, and while the hold runs a
 *        @ref C_LIME fill from the left, its width the control's times
 *        uiHoldPercent() over 100.
 * @param h     The hold.
 * @param r     The control; the bar takes its x and its width.
 * @param y     Top row of the bar.
 * @param nowMs Current millis().
 */
void uiHoldDrawBar(const UiHold *h, const UiRect &r, int y, uint32_t nowMs);
