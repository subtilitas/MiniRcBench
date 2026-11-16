/**
 * @file ui_settings.h
 * @brief SETUP: every persisted setting in one scrolling list in sections,
 *        and the hold that saves them.
 *
 * Reached from MENU. The sections, in order: ESC TESTER (motor poles and
 * the throttle ceiling), WIRING (SIG and AUX, the DShot rate, KISS), SERVO
 * SUPPLY (the switched supply and the current sensor), each tool's own
 * section in registry order, DISPLAY, BOARD and DIAGNOSTICS (the last
 * restart's reason and the uptime, read-only, @ref diag.h). Every change
 * applies at once; only writing flash needs HOLD TO SAVE, 1000 ms. HOLD
 * DEFAULTS, 1000 ms, loads the compiled defaults into the live settings and
 * writes nothing; the angle meter's fields, which SETUP does not show,
 * keep their values.
 *
 * The list scrolls under a drag that starts off a control. A press on a
 * control never scrolls; `-` and `+` step on touch-down and repeat while
 * held, a toggle fires on release. A control cut by the viewport's edge
 * takes a press only while 40 px of it shows (@ref UI_LIST_TAP_MIN).
 *
 * LINK, the live packet rate and error share, sits under the list, so a
 * change of SIG is answered on this screen.
 *
 * @see settings.h for what is stored and how it is validated;
 *      docs/design/menu.md, "SETUP".
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/**
 * @brief The SETUP rows, as `UI_ROW(n, part)` numbers them: rows count
 *        from the top of the list, captions not counted. SERVO SUPPLY is a
 *        core section after WIRING; a tool's own section would follow it in
 *        registry order, and no tool has one.
 */
enum class SettingsRowId : uint8_t {
	Poles,        /**< MOTOR POLES, - and +. */
	MaxThrottle,  /**< MAX THROTTLE, - and +. */
	Sig,          /**< SIG PIN, - and +, its J5 pin under the value. */
	Speed,        /**< DSHOT, the bitrate in kbit/s, - and +. */
	Kiss,         /**< KISS TELEM, toggle: KISS listens on AUX. */
	Aux,          /**< AUX PIN, - and +, J5 pin under it; acts with KISS off. */
	ServoSupply,  /**< SERVO SUPPLY, - and +: OWN, GP28, GP29; J4 pin under. */
	CurrentSense, /**< CURRENT SENSE, toggle, below SERVO SUPPLY. */
	Contrast,     /**< CONTRAST, toggle. */
	Backlight,    /**< BACKLIGHT, - and +. */
	BuiltFor,     /**< BUILT FOR, read-only. */
	Version,      /**< VERSION, the firmware's, read-only. */
	Clock,        /**< CLOCK, the board's clock to the minute or NOT SET, read-only. */
	LastRestart,  /**< LAST RESTART, why the chip restarted, read-only. */
	Uptime,       /**< UPTIME, time since boot, read-only. */
};

/** @brief The SETUP controls outside the list. @see ui_screen.h */
enum class SettingsCtl : uint8_t {
	Save = 1,  /**< HOLD TO SAVE. */
	Defaults,  /**< HOLD DEFAULTS. */
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
 * @brief The sections SETUP shows, in order: ESC TESTER, WIRING, SERVO
 *        SUPPLY, each tool's section, DISPLAY, BOARD and DIAGNOSTICS.
 *        Valid after uiSettingsEnter(); for the host tests.
 * @param[out] out At most @p max sections.
 * @param max      Room in @p out.
 * @return The number written.
 */
uint8_t uiSettingsSections(const SettingsSection **out, uint8_t max);

struct UiHub;

/** @brief The hub a screen shows, or null: uiHubOf()'s shape. */
typedef const UiHub *(*UiHubOfFn)(const UiScreen *s);

/**
 * @brief The tools' SETUP sections of a registry, each once, in the
 *        order of its rows: a row's own section (@ref UiScreen::settings),
 *        then, for a row that opens a hub, the sections of the hub's
 *        entries, top to bottom. SETUP shows them between SERVO SUPPLY
 *        and DISPLAY.
 * @param e        The rows.
 * @param n        How many.
 * @param hubOf    The hub a row's screen shows, as uiHubOf(); null: no row
 *                 opens a hub.
 * @param[out] out At most @p max sections.
 * @param max      Room in @p out.
 * @return The number written.
 */
uint8_t uiSettingsToolSections(const UiMenuEntry *e, uint8_t n, UiHubOfFn hubOf,
                               const SettingsSection **out, uint8_t max);

/** @brief SETUP. */
extern const UiScreen UI_SETTINGS;
