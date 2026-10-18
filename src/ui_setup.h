/**
 * @file ui_setup.h
 * @brief The SETUP screen: wiring, display, and the one button that persists it.
 *
 * Everything a user might have to change about how the board is *connected*
 * lives here, alongside the two display settings that matter on a bench in the
 * sun. Reached from **CFG -> SETUP**.
 *
 * The screen exists because of one property this firmware could not otherwise
 * fix: getting the ESC pin wrong is silent. Nothing errors, nothing warns, and
 * the ESC simply never hears a frame. So the screen shows the live packet rate
 * next to the pin selector — change the pin, watch LINK come off zero, and the
 * question is answered without navigating anywhere.
 *
 * @see settings.h for what is stored and how it is validated.
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Enter the screen. Call once, from the settings screen.
 *
 * @pre The caller has already disarmed. The screen changes the pin the DShot
 *      pump drives, which cannot be done under a live motor.
 */
void uiSetupEnter();

/**
 * @brief Run one frame of the setup screen.
 *
 * @param t Touch state for this frame.
 * @return false when the user has left; the caller should resume its own
 *         screen and repaint.
 */
bool uiSetupTick(const TouchState *t);

/** @brief The SETUP rows, in screen order. @see ui_screen.h */
enum class SetupRow : uint8_t {
	Board,      /**< BOARD, read-only. */
	Pin,        /**< ESC PIN, - and +. */
	Speed,      /**< DSHOT rate, - and +. */
	Kiss,       /**< KISS TELEM, toggle. */
	KissPin,    /**< KISS PIN, - and +, while KISS is on. */
	Contrast,   /**< HIGH CONTRAST, toggle. */
	Backlight,  /**< BACKLIGHT, - and +. */
};

/** @brief The SETUP footer. @see ui_screen.h */
enum class SetupCtl : uint8_t {
	Save = 1,  /**< HOLD TO SAVE. */
	Reset,     /**< RESET. */
};

struct UiControl;

/**
 * @brief The SETUP controls on the current frame. @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiSetupControls(UiControl *out, uint8_t max);
