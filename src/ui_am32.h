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

/**
 * @brief Run one frame of the config screen.
 * @param t Touch state for this frame.
 * @return false once the user has asked to leave.
 */
bool uiAm32Tick(const TouchState *t);

/**
 * @brief The line under a verified write's result: the ESC reads its
 *        settings at its own boot. 29 characters, 173 px at scale 1.
 */
#define AM32_UI_POWER_CYCLE "POWER-CYCLE THE ESC TO APPLY."

/**
 * @brief The line under a write that is not verified: the edits stay, and a
 *        hold of WRITE runs the write again. 34 characters, 203 px.
 */
#define AM32_UI_RETRY "NOT VERIFIED. TAP OK, WRITE AGAIN."
/**
 * @brief The line under NO BACKUP ON THE CARD: the next write saves the
 *        backup again first. 33 characters, 197 px.
 */
#define AM32_UI_CHECK_CARD "CHECK THE CARD, THEN WRITE AGAIN."

/**
 * @defgroup am32_notes Strip notes
 * @brief Two amber lines on the strip of the settings list, in place of the
 *        device caption, through uiStripTwoLines(). Each line is at most 37
 *        characters, 221 px.
 *
 * The selected row's note comes first, then 3D MODE, then PROTOCOL.
 * @{
 */
/** @brief 3D MODE on: AM32 splits DShot 48..2047 into two directions, and the
 *         tester sends 47 + throttle, 1 to 2000, so 50 % is 1047, the top of
 *         the lower half. */
#define AM32_UI_3D_1 "3D MODE: 50% THROTTLE IS FULL POWER,"
/** @brief The 3D note's second line. */
#define AM32_UI_3D_2 "ABOVE 50% THE OTHER WAY. PROP OFF."
/** @brief TEMP LIMIT, CURRENT LIMIT or LOW VOLT CUTOFF selected and off. */
#define AM32_UI_PROT_1 "PROTECTION OFF: THE ESC DOES NOT"
/** @brief The protection note's second line. */
#define AM32_UI_PROT_2 "CUT POWER ON THIS LIMIT."
/** @brief PROTOCOL at a value AM32 does not run on DShot with. @see am32InputNotDshot() */
#define AM32_UI_PROTO_1 "THE TESTER SENDS DSHOT ONLY:"
/** @brief The PROTOCOL note's second line. */
#define AM32_UI_PROTO_2 "THE ESC WILL NOT RUN HERE."
/** @} */

/** @brief The connect page's last line under a reply it could not parse. */
#define AM32_UI_NOT_AM32 "IS THE ESC RUNNING AM32?"
/** @brief The connect page's first line under NO VALID SETTINGS. */
#define AM32_UI_NO_SETTINGS_1 "NOT AN AM32 ESC, OR BLANK SETTINGS."
/**
 * @brief The connect page's second line under NO VALID SETTINGS and under
 *        `LAYOUT n NOT KNOWN`.
 */
#define AM32_UI_NO_SETTINGS_2 "NOTHING WILL BE WRITTEN."
/**
 * @brief The connect page's first line under `LAYOUT n NOT KNOWN`, a layout
 *        revision above AM32_LAYOUT_MAX_KNOWN in am32_eeprom.h; the second
 *        is `AM32_UI_NO_SETTINGS_2`.
 */
#define AM32_UI_LAYOUT_1 "THE TESTER KNOWS LAYOUTS 1 TO 4."

/**
 * @brief MENU's strip line after a session that reached the bootloader:
 *        leaving sends no command to start the ESC's firmware. 29
 *        characters, 173 px. @see uiAm32RunCue()
 */
#define AM32_UI_RUN_CUE "POWER-CYCLE THE ESC TO RUN IT"

/**
 * @brief Whether MENU shows @ref AM32_UI_RUN_CUE.
 *
 * Set when a handshake with the bootloader succeeds. Cleared by an eRPM frame
 * that arrived after that handshake, which only the ESC's firmware sends; a
 * restart clears it as well, since it is not stored.
 *
 * @param lastRpmMs EscTelemetry::lastRpmMs.
 * @return true while the cue stands.
 */
bool uiAm32RunCue(uint32_t lastRpmMs);

/** @brief The AM32 CONFIG controls; the list rows are `UI_ROW(n, Row)`. @see ui_screen.h */
enum class Am32Ctl : uint8_t {
	EditMinus = 1, EditPlus,  /**< The editor bar, while a field is selected. */
	Write,                    /**< HOLD TO WRITE. */
	Revert,                   /**< REVERT. */
	Hex,                      /**< HEX or FIELDS. */
	Ok,                       /**< OK under the write; acts once the write has finished. */
};

struct UiControl;

/**
 * @brief The AM32 CONFIG controls on the current frame. @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiAm32Controls(UiControl *out, uint8_t max);

struct UiScreen;
/** @brief AM32 CONFIG. */
extern const UiScreen UI_AM32;
