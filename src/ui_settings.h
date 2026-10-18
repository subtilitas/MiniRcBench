/**
 * @file ui_settings.h
 * @brief SETTINGS: every persisted setting in one scrolling list in sections,
 *        and the hold that saves them.
 *
 * Reached from MENU. The sections, in order: ESC TESTER (motor poles and
 * the throttle ceiling), WIRING, each tool's own section in registry order,
 * DISPLAY and BOARD. Every change applies at once; only writing flash needs
 * HOLD TO SAVE, 1000 ms. RESET loads the compiled defaults into the live
 * settings and writes nothing.
 *
 * The list scrolls under a drag that starts off a control. A press on a
 * control never scrolls; `-` and `+` step on touch-down and repeat while
 * held, a toggle fires on release. A control cut by the viewport's edge
 * takes a press only while 40 px of it shows (@ref UI_LIST_TAP_MIN).
 *
 * LINK, the live packet rate and error share, sits under the list, so a
 * change of the ESC pin is answered on this screen.
 *
 * @see settings.h for what is stored and how it is validated;
 *      docs/design/menu.md, "SETTINGS".
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/**
 * @brief The SETTINGS rows of the core sections, as `UI_ROW(n, part)`
 *        numbers them: rows count from the top of the list, captions not
 *        counted. A tool section shifts the rows after WIRING.
 */
enum class SettingsRowId : uint8_t {
	Poles,        /**< MOTOR POLES, - and +. */
	MaxThrottle,  /**< MAX THROTTLE, - and +. */
	Pin,          /**< ESC PIN, - and +. */
	Speed,        /**< DSHOT KBAUD, - and +. */
	Kiss,         /**< KISS TELEM, toggle. */
	KissPin,      /**< KISS PIN, - and +, while KISS is on. */
	Contrast,     /**< CONTRAST, toggle. */
	Backlight,    /**< BACKLIGHT, - and +. */
	BuiltFor,     /**< BUILT FOR, read-only. */
};

/** @brief The SETTINGS controls outside the list. @see ui_screen.h */
enum class SettingsCtl : uint8_t {
	Save = 1,  /**< HOLD TO SAVE. */
	Reset,     /**< RESET. */
};

/**
 * @brief Open the screen, scrolled to 0 px.
 * @pre The caller has disarmed: the screen changes the pin the DShot pump
 *      drives.
 */
void uiSettingsEnter();

/**
 * @brief Run one frame of the screen.
 * @param t This frame's touch.
 * @return false once BACK has left.
 */
bool uiSettingsTick(const TouchState *t);

/**
 * @brief The sections SETTINGS shows, in order: the core ones and each
 *        tool's. Valid after uiSettingsEnter(); for the host tests.
 * @param[out] out At most @p max sections.
 * @param max      Room in @p out.
 * @return The number written.
 */
uint8_t uiSettingsSections(const SettingsSection **out, uint8_t max);

/** @brief SETTINGS. */
extern const UiScreen UI_SETTINGS;
