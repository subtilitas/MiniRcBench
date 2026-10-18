/**
 * @file ui_servo.h
 * @brief The servo tester screens: SERVO, its SET screen, and the unplug
 *        page before DShot restarts.
 *
 * Servo PWM (pulse-width modulation) on the ESC pin, and on the KISS pin for
 * two servos. Entry: MENU → SERVO TESTER. The screen claims the pins
 * (pin_claim.h) and hands the servo job (servo_job.h) its profile and its
 * commands; core1 makes the pulses.
 *
 * The screen has the motor screen's two states under other names. SAFE: the
 * pins bound at level 0, no pulses. HOLD TO START, 1000 ms, puts CENTRE on
 * every bound pin; STOP, on press, ends the pulses from the next PWM frame. A
 * screen that stalls for 250 ms stops them too, and a new HOLD TO START is
 * needed.
 *
 * @see docs/design/servo-tester.md
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Whether the screen is built.
 * @return true: the SERVO TESTER row is live.
 */
bool uiServoAvailable();

/**
 * @brief Open the screen: the default profile, SPEED and sweep, and a claim
 *        of the pins, asked again every frame until it is accepted.
 */
void uiServoEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left, after RESTART DSHOT on the unplug
 *         page, or after BACK when no job had run.
 */
bool uiServoTick(const TouchState *t);

/**
 * @brief The SERVO controls, of all three views; the SET rows are
 *        `UI_ROW(n, Row)`. @see ui_screen.h
 */
enum class ServoCtl : uint8_t {
	Min = 1, Centre, Max, Sweep,  /**< The button row. */
	Start,                        /**< HOLD TO START, or STOP. */
	Set,                          /**< SET. */
	SelA, SelB, SelAB,            /**< The selector, with two servos. */
	Retry,                        /**< RETRY under a job or bind error. */
	Dial,                         /**< The dial's band; a press drives only on the dial itself. */
	Speed,                        /**< SPEED's track. */
	EditMinus, EditPlus,          /**< SET's editor bar, while a row is selected. */
	Apply,                        /**< HOLD TO APPLY. */
	Defaults,                     /**< DEFAULTS, or STOP. */
	Restart,                      /**< RESTART DSHOT on the unplug page. */
};

struct UiControl;

/**
 * @brief The SERVO view that is up controls on the current frame. @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiServoControls(UiControl *out, uint8_t max);

struct UiScreen;
/** @brief SERVO TESTER. */
extern const UiScreen UI_SERVO;
