/**
 * @file ui_servo.h
 * @brief The servo tester screens: SERVO TESTER, its SET screen, and the
 *        leave page before the pins park.
 *
 * Servo PWM (pulse-width modulation) on the ESC pin, and on the KISS pin for
 * two servos. Entry: MENU → SERVO → SERVO TESTER, the first entry of the
 * hub SERVO (ui_servo_hub.h). The screen claims the pins (pin_claim.h) and
 * hands the servo job (servo_job.h) its profile and its commands; core1
 * makes the pulses.
 *
 * The screen has the motor screen's two states under other names. SAFE: the
 * pins bound at level 0, no pulses. HOLD TO START, 1000 ms, puts CENTRE on
 * every bound pin; STOP, on press, ends the pulses from the next PWM frame. A
 * screen that stalls for 250 ms stops them too, and a new HOLD TO START is
 * needed.
 *
 * With SERVO SUPPLY at GP28 or GP29 the job switches the servo's supply on
 * at START and off whenever the pulses end (servo_job.h); the pulses start
 * once the supply is up. The strip's second line says where the supply is.
 *
 * @see docs/design/servo-tester.md
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/**
 * @brief The header's title on the main view and the leave page: the hub
 *        entry's label, 12 characters, 142 px at scale 2, from x 8 to x 149,
 *        20 px before BACK at x 170.
 */
#define SERVO_UI_TITLE "SERVO TESTER"

/**
 * @defgroup ui_servo_hints SERVO TESTER's hint lines
 * @{
 */
/**
 * @brief The strip's second line while SERVO SUPPLY is GP28 or GP29, with
 *        the pin's number for `%u`, drawn in amber by uiStripTwoLines()
 *        under the caption: the supply off, as it is while SAFE. At most 28
 *        characters, 167 px from x 8, so each ends 4 px before the widest
 *        chip, STALLED.
 */
#define SERVO_UI_SUPPLY_FMT "GP%u SUPPLY ON AT START"
/** @brief ...after START, while the pulses wait for the supply. */
#define SERVO_UI_SUPPLY_UP_FMT "GP%u SUPPLY COMING UP"
/** @brief ...while the supply is up and the pulses run. */
#define SERVO_UI_SUPPLY_ON_FMT "GP%u SUPPLY ON"
/** @brief ...after a switch-off, while the sensor reads above 2.5 V 2000 ms or more later. */
#define SERVO_UI_SUPPLY_NOT_OFF_FMT "GP%u SUPPLY NOT OFF"
/**
 * @brief SAFE with the switch: the dial band's two notes, amber, for `%u`
 *        SIG's GPIO: pulses are throttle to an ESC, and its BEC (battery
 *        eliminator circuit) wire gets the converter's output.
 */
#define SERVO_UI_ESC_1_FMT "PULSES ON GP%u ARE THROTTLE TO AN ESC,"
/** @brief ...and its second line. */
#define SERVO_UI_ESC_2 "AND ITS BEC WIRE GETS THE SUPPLY."
/** @brief The supply did not reach 4.5 V within 500 ms: first line, amber. */
#define SERVO_UI_NO_START_1 "SUPPLY NOT UP: NO 4.5 V IN 500 MS."
/** @brief ...and its second line, dim, with the enable pin for `%u`. */
#define SERVO_UI_NO_START_2_FMT "CHECK THE CONVERTER ON GP%u."
/** @brief START refused, CURRENT SENSE at INA219 and the part not online: first line, amber. */
#define SERVO_UI_NO_SENSOR_1 "START WAITS FOR THE CURRENT SENSOR."
/** @brief ...and its second line, dim. */
#define SERVO_UI_NO_SENSOR_2 "NONE IN CURRENT SENSE SKIPS IT."
/** @brief START refused while the shared bus is stuck, no trip: first line, amber. */
#define SERVO_UI_BUS_WAIT_1 "START WAITS: THE I2C BUS IS STUCK."
/** @brief ...and its second line, dim. */
#define SERVO_UI_BUS_WAIT_2 "HOLD TO START WORKS ONCE IT IS FREE."
/** @brief START refused while the touch controller has not answered for 250 ms, no trip: first line, amber. */
#define SERVO_UI_TOUCH_WAIT_1 "START WAITS: TOUCH DOES NOT ANSWER."
/** @brief ...and its second line, dim. */
#define SERVO_UI_TOUCH_WAIT_2 "HOLD TO START WORKS ONCE IT ANSWERS."
/** @brief The touch-loss note's second line after the touch-loss stop or core1's 250 ms cut ended a run, dim. */
#define SERVO_UI_TOUCH_LOST_2 "THE SERVO IS STOPPED."
/** @brief The supply cut because the touch controller did not answer for 250 ms. */
#define SERVO_UI_TOUCH_SILENT "STOPPED: TOUCH DID NOT ANSWER"
/** @brief The supply cut because the shared bus was stuck. */
#define SERVO_UI_BUS_STUCK "STOPPED: TOUCH BUS STUCK"
/**
 * @brief The chips of the cuts of supply_guard.h, `C_MAGENTA`, latched as
 *        STALLED is: the hard cut. Wider than RUNNING: while one shows, the
 *        caption ends 4 px before it.
 */
#define SERVO_UI_CHIP_HARD "OVERCURRENT"
/** @brief ...the stall cut. */
#define SERVO_UI_CHIP_STALL "STALL"
/** @brief ...the collapse cut. */
#define SERVO_UI_CHIP_LOW "LOW SUPPLY"
/** @brief ...the sensor-lost cut. */
#define SERVO_UI_CHIP_LOST "NO SENSOR"
/** @brief The hard cut's note in the dial band, magenta, over the trip's second line. */
#define SERVO_UI_CUT_HARD "STOPPED: CURRENT 3.0 A OR MORE"
/** @brief The stall cut's note. */
#define SERVO_UI_CUT_STALL "STOPPED: OVER 1.0 A FOR 400 MS"
/** @brief The collapse cut's note. */
#define SERVO_UI_CUT_LOW "STOPPED: UNDER 4.0 V FOR 1000 MS"
/** @brief The sensor-lost cut's note, the design note's text. */
#define SERVO_UI_CUT_LOST "CURRENT SENSOR LOST: SUPPLY OFF."
/**
 * @brief RUNNING with SERVO SUPPLY at OWN and the declared sensor not
 *        online: the dial band's second line, dim. The hard and the stall
 *        cut need its readings, and OWN has no sensor-lost cut.
 */
#define SERVO_UI_OWN_NO_CUTS "NO SENSOR READING: NO CURRENT CUTS."
/**
 * @brief RUNNING with a servo log open: the dial band's second line, dim;
 *        `%05lu` the file's number.
 */
#define SERVO_UI_LOG_FMT "LOGGING TO SRV%05lu.CSV"
/** @brief ...amber once rows were dropped: the number, then the rows dropped. */
#define SERVO_UI_LOG_DROPPED_FMT "SRV%05lu.CSV: %lu ROWS DROPPED"
/**
 * @brief ...amber once the smaller reserve, all the card had free in one
 *        run, is used up: true of a full card and of a fragmented one.
 */
#define SERVO_UI_LOG_FULL "LOG STOPPED: NO ROOM IN ONE PIECE"
/** @brief ...amber when the card had no 64 KiB free in one run for the file. */
#define SERVO_UI_LOG_NO_ROOM "NO LOG: NO 64 KIB IN ONE PIECE"
/** @brief ...amber once the rows filled the file's whole 4 MiB. */
#define SERVO_UI_LOG_LIMIT "LOG STOPPED AT 4 MIB"
/** @brief ...amber after a write, the cache's flush or the swap check failed. */
#define SERVO_UI_LOG_FAILED "LOG STOPPED: CARD WRITE FAILED"
/** @brief ...amber when the file did not open. */
#define SERVO_UI_LOG_NO_OPEN "NO LOG: THE FILE DID NOT OPEN"
/** @brief ...amber when `LOGS/` holds SRV99999.CSV. */
#define SERVO_UI_LOG_NO_NUMBER "NO LOG: SRV99999.CSV EXISTS"
/** @brief ...amber when the open took 2000 ms or more and was given up. */
#define SERVO_UI_LOG_SLOW "NO LOG: CARD TOO SLOW"
/** @brief ...amber when a card was mounted and the write cache did not start on it. */
#define SERVO_UI_LOG_NO_CACHE "NO LOG: THE CARD DID NOT ANSWER"
/**
 * @brief The dial band's two lines while HOLD TO START waits for the log's
 *        open, dim: the first...
 */
#define SERVO_UI_LOG_PREP_1 "PREPARING THE LOG..."
/** @brief ...and the second. */
#define SERVO_UI_LOG_PREP_2 "THE SERVO STARTS AFTER IT."
/** @brief SET's note with the last start-up time measured, `%u` in ms. */
#define SERVO_UI_UP_FMT "SUPPLY UP IN %u MS"
/** @brief The leave page's note while the job lets SIG and AUX go: 18 characters. */
#define SERVO_UI_RELEASING_TWO "RELEASING THE PINS"
/** @brief SET's note while SIG and AUX share a GPIO, refusing TWO: line 1. */
#define SERVO_UI_SAME_PIN_1 "SIG AND AUX ARE THE SAME GPIO."
/** @brief ...and its second line: the rows that set them. */
#define SERVO_UI_SAME_PIN_2 "SET SIG PIN OR AUX PIN IN SETUP."
/** @brief The readout's hint while SAFE with one servo, first line, right-aligned. */
#define SERVO_UI_TWO_1 "TWO SERVOS:"
/** @brief ...and its second line. */
#define SERVO_UI_TWO_2 "SET > SERVOS"
/** @} */

/**
 * @brief Open the screen: the default profile, SPEED and sweep, and a claim
 *        of the pins, asked again every frame until it is accepted.
 */
void uiServoEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left, after the leave page's last
 *         button, or after BACK when no job had run.
 */
bool uiServoTick(const TouchState *t);

/**
 * @brief The SERVO TESTER controls, of all three views; the SET rows are
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
	Release,                      /**< The hub's title on the leave page: parks the pins and returns to the hub, live 1000 ms after the pins are let go. */
	NextServo,                    /**< NEXT SERVO on the leave page: the claim kept, SERVO TESTER SAFE. */
};


/**
 * @brief The SERVO TESTER view that is up controls on the current frame.
 *        @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiServoControls(UiControl *out, uint8_t max);

/** @brief SERVO TESTER, a constant: SERVO's table checks it at build time. */
inline constexpr const UiScreen UI_SERVO = {
	UiScreenId::Servo, SERVO_UI_TITLE, PinUser::Servo, nullptr,
	uiServoEnter, uiServoTick, nullptr, uiServoControls, nullptr, false,
};
