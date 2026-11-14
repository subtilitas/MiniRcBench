/**
 * @file ui.h
 * @brief Touchscreen UI for the DShot motor tester.
 *
 * Owns the arm/throttle state machine and all rendering. Runs entirely on
 * core0 and pushes commands to core1 through the @ref esc_core0 API.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Reset UI state, seed the ADC, and clear the screen.
 * @pre st7789Init() and touchInit() must already have run.
 */
void uiInit();

/**
 * @brief Draw the boot splash into the framebuffer.
 *
 * Separate from setup() so the host tests render the real thing rather than a
 * copy — the documentation screenshots and the width checks would otherwise
 * drift away from what the board actually shows.
 */
void uiDrawSplash();

/**
 * @brief One step of the splash, every UI frame while it is up: a touch
 *        read, and `TOUCH CONTROLLER NOT FOUND` drawn while the controller
 *        has not answered once since touchInit(), cleared once it has.
 *
 * The read goes into a state of its own. The driver keeps the contact
 * between reads, so a finger resting on the glass at the splash's end gives
 * the tester no press edge: it does not arm or start a drag. The driver also
 * keeps where that press began, so lifting it completes a tap on a button
 * that acts on release.
 */
void uiSplashTick();

/**
 * @brief The splash's line while the touch controller does not answer,
 *        centred at @ref UI_SPLASH_TOUCH_Y in amber: 26 characters, 155 px.
 */
#define UI_SPLASH_TOUCH_TEXT "TOUCH CONTROLLER NOT FOUND"
/** @brief The splash's touch line's top row, above the name. */
#define UI_SPLASH_TOUCH_Y 64


/**
 * @brief Run one UI frame.
 *
 * Sends the heartbeat, samples touch and battery, updates the arm and throttle
 * state machines, repaints whatever changed, and flushes it to the panel. Call
 * at a steady rate; the firmware uses ~40 Hz.
 */
void uiTick();

/**
 * @brief The tester's board voltage as drawn top right: rounded to 0.01 V
 *        and clamped to 0.00 V to 9.99 V, so at most five characters,
 *        for example `3.92V`.
 * @param volts The board's battery voltage.
 * @param[out] buf The text.
 * @param n Room in @p buf; 6 bytes or more.
 */
void uiBoardVoltText(float volts, char *buf, size_t n);

/** @brief The colour band of the board's battery voltage. */
enum class BoardCell : uint8_t {
	Ok,        /**< @ref BOARD_CELL_LOW_MV or more: the text colour. */
	Low,       /**< Below @ref BOARD_CELL_LOW_MV, amber. */
	Critical,  /**< Below @ref BOARD_CELL_CRIT_MV, red. */
	NoCell,    /**< Below @ref BOARD_CELL_IMPLAUSIBLE_MV, no working cell to
	                judge: the text colour. */
};

/**
 * @brief The band of @p volts, given the band it was in: a reading falls into
 *        a lower band at its threshold, and leaves it only at the threshold
 *        plus @ref BOARD_CELL_HYST_MV. Taken from the voltage rounded to
 *        centivolts as uiBoardVoltText() draws it. Below
 *        @ref BOARD_CELL_IMPLAUSIBLE_MV it is NoCell, which it leaves at that
 *        threshold plus the hysteresis too, into the band a start-up at that
 *        reading gives: below 3.30 V red, below 3.50 V amber, else Ok.
 * @param volts The board's battery voltage, smoothed.
 * @param prev  The band of the last frame.
 * @return The band now.
 */
BoardCell uiBoardCell(float volts, BoardCell prev);

/**
 * @brief The band of the board's voltage now. uiTick() samples the reading
 *        and steps the band on every frame, before the tester, MENU or a
 *        tool runs, so it is current on every screen.
 * @return The band of the last frame; Ok before the first.
 */
BoardCell uiBoardCellNow();

/**
 * @brief A KST, Hitec or AM32 write, or a restore, is refused: the board's
 *        voltage is in the red band, BoardCell::Critical. NoCell, below
 *        2.50 V, refuses nothing. A write already running is not stopped.
 * @return true while a hold of WRITE or RESTORE must not start one.
 */
static inline bool uiBoardWriteRefused() { return uiBoardCellNow() == BoardCell::Critical; }

/**
 * @defgroup ui_board_low The refusal's two lines
 * @brief Amber, scale 1, at most 37 characters each.
 * @{
 */
/** @brief First line: why nothing was written. */
#define UI_BOARD_LOW_1 "BOARD BATTERY LOW: NOT WRITTEN."
/** @brief Second line: what to do. */
#define UI_BOARD_LOW_2 "CHARGE IT OR POWER THE BOARD BY USB."
/** @} */

/** @brief Current commanded throttle, 0..2000. @return Throttle value. */
uint16_t uiThrottle();

/** @brief Current arm state. @return true when armed. */
bool     uiArmed();

/** @brief The tester's controls. @see ui_screen.h */
enum class TesterCtl : uint8_t {
	Arm = 1,   /**< HOLD TO ARM, or DISARM. Acts while no USB host holds the card. */
	Hold,      /**< HOLD. */
	Menu,      /**< MENU: disarms, zeroes the throttle, turns HOLD off and opens MENU. */
	Throttle,  /**< The throttle gauge's track, as drawn. A press lands 8 px beyond its left, right and top edges and 1 px below it (handleMainTouch()). Acts while armed. */
	Pad,       /**< The relative throttle pad over the readouts. Acts while armed. */
};

/**
 * @brief The throttle track's text while a USB host holds the card, in place
 *        of ARM TO ENABLE: HOLD TO ARM is drawn disabled then.
 */
#define UI_TESTER_CARD_TEXT "EJECT THE USB DRIVE TO ARM"

/**
 * @brief The splash's warning, with the ESC pin's GPIO number for `%u`:
 *        DShot runs on that pin from the end of the splash, and a servo
 *        on it moves.
 */
#define UI_SPLASH_PIN_FMT "UNPLUG ANY SERVO FROM GP%u NOW"

/**
 * @brief The first of the splash's two lines after a reset by an update
 *        (settingsResetByUpdate()), y 40; the second is 12 px under it.
 */
#define UI_SPLASH_RESET_Y 40

struct UiScreen;

/** @brief The tester, the root screen. */
extern const UiScreen UI_TESTER;
