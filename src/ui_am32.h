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

#include "am32_eeprom.h"

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
 * A layout the table does not know comes first: the list is read-only.
 * Then the selected row's note, then 3D MODE, then PROTOCOL where the
 * firmware reads byte 0x2E, then firmware below 1.80 or unknown.
 * @{
 */
/**
 * @brief Read-only, first line: the layout revision, one digit (a plausible
 *        block has 1 to 8). @see am32ViewLayout()
 */
#define AM32_UI_RO_FMT "LAYOUT %u NOT KNOWN: READ-ONLY."
/** @brief Read-only, second line, above layout 4: the fields as layout 4 has them. */
#define AM32_UI_RO_FIELDS "VALUES UNVERIFIED."
/** @brief Read-only, second line, below layout 2: the hex view alone. */
#define AM32_UI_RO_HEX "SHOWN AS BYTES."
/** @brief After either: the backup went to the card, with its five-character name. */
#define AM32_UI_RO_SAVED " BACKUP "
/** @brief After either: no card, or the card held by USB. */
#define AM32_UI_RO_NO_CARD " NO CARD: NO BACKUP"
/** @brief After either: the card did not take the backup. */
#define AM32_UI_RO_FAILED " BACKUP FAILED"
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
/**
 * @brief Firmware below @ref AM32_FW_READS_2A, or a version byte AM32 did
 *        not write: bytes 0x2A to 0x2F are not shown.
 */
#define AM32_UI_OLDFW_1 "FIRMWARE BELOW 1.80 OR UNKNOWN:"
/** @brief The old-firmware note's second line. */
#define AM32_UI_OLDFW_2 "FIELDS IT MAY NOT READ ARE HIDDEN."
/** @} */

/** @brief The connect page's last line under a reply it could not parse. */
#define AM32_UI_NOT_AM32 "IS THE ESC RUNNING AM32?"
/** @brief The connect page's first line under NO VALID SETTINGS. */
#define AM32_UI_NO_SETTINGS_1 "NOT AN AM32 ESC, OR BLANK SETTINGS."
/** @brief The connect page's second line under NO VALID SETTINGS. */
#define AM32_UI_NO_SETTINGS_2 "NOTHING WILL BE WRITTEN."
/**
 * @brief The write's refusal on a read-only list. No control of the list
 *        reaches it; doWrite() checks the flag before anything else.
 */
#define AM32_UI_RO_REFUSED "READ-ONLY: NOT WRITTEN"

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
