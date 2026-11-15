/**
 * @file ui_angle.h
 * @brief ANGLE METER: the main view, SETUP and FLIP CAL, on the angle model
 *        (angle_model.h) and the QMI8658 driver (qmi8658.h).
 *
 * TOOLS opens the meter (ui_tools.h). Opening it takes ALONG AXIS, SIGN,
 * LENGTH and the calibration from the live settings, begins a visit of the
 * model with no zero and no MAX, and opens the driver, qmi8658Start(). Every
 * frame gives the driver's newest sample to the model, which takes each
 * sample once, and ages the reading. BACK on the main view closes the driver,
 * qmi8658Stop(), which writes CTRL7 00h, and returns to MENU; the next visit
 * starts unzeroed (docs/design/angle-meter.md, decision 4).
 *
 * The meter is open while any of its three views shows: SETUP and FLIP CAL
 * are its own pages, and going to them and back neither stops the driver nor
 * drops the zero and MAX, unless ALONG AXIS or the calibration changes.
 *
 * - **The main view.** The strip names the edge along the axis and the axis's
 *   tilt, amber from 45°, beside a chip: `SETTLED`, `MOVING`, `TOO STEEP`,
 *   `STARTING` or `NO IMU`. The angle from the zero, or from level before a
 *   zero, shows at scale 5 in tenths of a degree, and `---` while the reading
 *   is stale, the model has none yet, or the axis is too steep. With no part
 *   answering, or another part at 0x6B, a line says so in its place. Under it
 *   the travel in mm while LENGTH is set, MAX each side with its travel, and
 *   the IMU's temperature with its change since the zero. ZERO arms on its
 *   tap and takes the zero from a settled reading after the lift; RESET MAX
 *   clears MAX; SETUP opens SETUP.
 * - **SETUP.** ALONG AXIS, LENGTH, SIGN and CALIBRATION, drawn as SETTINGS
 *   draws its rows. A row applies at once; BACK returns to the main view and
 *   saves the four fields while they differ from flash (settingsAngleDirty(),
 *   settingsSaveAngle()), so a save that failed is tried again; the main
 *   view's hint shows while they differ. CALIBRATION's FLIP opens FLIP CAL.
 * - **FLIP CAL.** The three poses of the flip, each step's lines and the live
 *   angle. Touches are ignored while the board lies screen down. With a
 *   result, the 1 s HOLD TO KEEP applies the calibration and saves it; AGAIN
 *   starts over; BACK returns to SETUP without keeping it.
 *
 * No save is made while the ESC is armed; the meter opens through MENU, which
 * disarms. Nothing on the screen states an accuracy: none is measured
 * (docs/design/angle-meter.md, "Accuracy").
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"
#include "angle_model.h"

struct TouchState;

/** @brief The main view's title, and TOOLS' entry: 11 characters, 130 px at scale 2. */
#define UI_ANGLE_TITLE       "ANGLE METER"
/** @brief SETUP's title: 11 characters, 130 px. */
#define UI_ANGLE_SETUP_TITLE "ANGLE SETUP"
/** @brief FLIP CAL's title: 8 characters, 94 px. */
#define UI_ANGLE_FLIP_TITLE  "FLIP CAL"

/**
 * @defgroup ui_angle_texts Texts
 * @{
 */
#define UI_ANGLE_NOT_ZEROED   "NOT ZEROED: FROM LEVEL"          /**< Under the angle before a zero. */
#define UI_ANGLE_FROM_ZERO    "FROM ZERO"                       /**< Under the angle once zeroed. */
#define UI_ANGLE_STEEP_LINE   "TURN THE AXIS NEARER LEVEL."     /**< Under the `---` while too steep, `UI_COL_WARN`. */
#define UI_ANGLE_LEN_FMT      "L %u MM, HINGE TO TRAILING EDGE" /**< LENGTH set. */
#define UI_ANGLE_LEN_OFF      "SET LENGTH IN SETUP FOR MM"      /**< LENGTH `OFF`. */
#define UI_ANGLE_NO_ANSWER    "NO ANSWER FROM THE IMU AT 0X6B." /**< No part answering, or offline. */
#define UI_ANGLE_WRONG_FMT    "NOT A QMI8658: ID 0X%02X."       /**< Another part at 0x6B, its WHO_AM_I. */
#define UI_ANGLE_HINT_ARMED   "ZERO WHEN SETTLED: HOLD STILL"   /**< ZERO armed. */
#define UI_ANGLE_HINT_TIMEOUT "NOT SETTLED IN 10 S: NO ZERO"    /**< ZERO timed out; the old zero stays. */
#define UI_ANGLE_HINT_UNSAVED "SETUP NOT SAVED TO FLASH."       /**< The meter's fields differ from flash: a save failed; the values stay live. */
#define UI_ANGLE_CAL_YES      "CALIBRATED"                      /**< CALIBRATION's caption with a calibration. */
#define UI_ANGLE_CAL_NO       "NOT CALIBRATED"                  /**< And without. */
#define UI_ANGLE_KEPT         "KEPT AND SAVED."                 /**< FLIP CAL after HOLD TO KEEP. */
#define UI_ANGLE_KEPT_UNSAVED "APPLIED, NOT SAVED TO FLASH."    /**< The same, its save failed. */
/** @} */

/** @brief The meter's controls besides BACK, on whichever view shows them. */
enum class AngleCtl : uint8_t {
	Zero = 1,  /**< Main view: ZERO, live while the IMU is online. */
	ResetMax,  /**< Main view: RESET MAX. */
	Setup,     /**< Main view: SETUP. */
	Along,     /**< SETUP: ALONG AXIS, `LONG` or `SHORT`. */
	LenMinus,  /**< SETUP: LENGTH's `-`, to `OFF` below 10 mm. */
	LenPlus,   /**< SETUP: LENGTH's `+`, to 500 mm. */
	Sign,      /**< SETUP: SIGN, `NORMAL` or `INVERT`. */
	Flip,      /**< SETUP: CALIBRATION's `FLIP`, opens FLIP CAL. */
	Keep,      /**< FLIP CAL: HOLD TO KEEP, live with a result not yet kept. */
	Again,     /**< FLIP CAL: AGAIN, the flip from step 1. */
};

/** @brief Which view shows. */
enum class AngleView : uint8_t {
	Main,   /**< The reading, ZERO, RESET MAX, SETUP. */
	Setup,  /**< ANGLE SETUP. */
	Flip,   /**< FLIP CAL. */
};

/** @brief Open the meter: the setup from the settings, a new visit, the driver opened. */
void uiAngleEnter();

/**
 * @brief Run one frame of the meter.
 * @param t This frame's touch.
 * @return false once BACK on the main view has left it, the driver closed.
 */
bool uiAngleTick(const TouchState *t);

/**
 * @brief The meter's controls on the current frame: BACK and the view's own.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiAngleControls(UiControl *out, uint8_t max);

/** @brief The view that shows. @return The view. */
AngleView uiAngleView();

/** @brief The model of the visit, for the host tests. @return The model. */
const AngleModel *uiAngleModel();

/** @brief ANGLE METER. */
extern const UiScreen UI_ANGLE;
