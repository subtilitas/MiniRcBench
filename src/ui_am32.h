/**
 * @file ui_am32.h
 * @brief AM32 ESC configuration screen.
 *
 * A separate mode from the throttle tester: entering it claims the signal pin
 * (pin_claim.h), which disarms, drains and tears down the DShot driver, and
 * hands the pin to the bootloader transport. Leaving it gives the pin back.
 *
 * @see am32_bl.h for the transport, am32_eeprom.h for the settings layout.
 */

#pragma once

#include <stdint.h>

struct TouchState;

/** @brief Enter config mode. Claims the signal pin, which disarms. */
void uiAm32Enter();

/** @brief Leave config mode and end the claim, which resumes DShot. */
void uiAm32Exit();

/** @brief True while config mode owns the screen. */
bool uiAm32Active();

/**
 * @brief Run one frame of the config screen.
 * @param t Touch state for this frame.
 * @return false once the user has asked to leave.
 */
bool uiAm32Tick(const TouchState *t);

/** @brief The AM32 CONFIG controls; the list rows are `UI_ROW(n, Row)`. @see ui_screen.h */
enum class Am32Ctl : uint8_t {
	EditMinus = 1, EditPlus,  /**< The editor bar, while a field is selected. */
	Write,                    /**< HOLD TO WRITE. */
	Revert,                   /**< REVERT. */
	Hex,                      /**< HEX or FIELDS. */
	Ok,                       /**< OK on the write result. */
};

struct UiControl;

/**
 * @brief The AM32 CONFIG controls on the current frame. @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiAm32Controls(UiControl *out, uint8_t max);
