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
 * @brief The line under a write that is not verified: the edits stay, and
 *        OK, `WRITE...` and the page's HOLD TO WRITE run the write again.
 *        34 characters, 203 px.
 */
#define AM32_UI_RETRY "NOT VERIFIED. TAP OK, WRITE AGAIN."
/**
 * @brief The line under NO BACKUP ON THE CARD: the next write saves the
 *        backup again first. 33 characters, 197 px.
 */
#define AM32_UI_CHECK_CARD "CHECK THE CARD, THEN WRITE AGAIN."

/**
 * @defgroup am32_write AM32 WRITE
 * @brief The write page: `WRITE...` on the list opens it, its HOLD TO WRITE
 *        writes. It lists each field that changes, label and
 *        `old -> new`, at most 4 and then `AND n MORE`; the state lines from
 *        y 196; the backup line at y 236. Each line is at most 37
 *        characters, 221 px, at scale 1 from x 8.
 * @{
 */
/** @brief The state line while the write runs, amber. 23 characters, 137 px. */
#define AM32_UI_WRITING "WRITING: DO NOT UNPLUG."
/**
 * @brief The backup line with no card and no backup on a card yet: the
 *        settings as read stay in RAM, and leaving AM32 CONFIG loses them.
 *        Amber. 35 characters, 209 px.
 */
#define AM32_UI_NO_CARD_LOST "NO CARD: OLD VALUES LOST ON LEAVING"
/** @brief As @ref AM32_UI_NO_CARD_LOST while a USB host holds the card. 37 characters, 221 px. */
#define AM32_UI_USB_LOST "USB DRIVE: OLD VALUES LOST ON LEAVING"
/** @} */

/**
 * @defgroup am32_notes Strip notes
 * @brief Two amber lines on the strip of the settings list, in place of the
 *        device caption, through uiStripTwoLines(). Each line is at most 37
 *        characters, 221 px.
 *
 * The selected row's note comes first, then 3D MODE, then PROTOCOL where the
 * firmware reads byte 0x2E, then firmware below 1.80 or unknown.
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
/**
 * @brief Firmware below @ref AM32_FW_READS_2A, or a version byte AM32 did
 *        not write: bytes 0x2A to 0x2F are not shown.
 */
#define AM32_UI_OLDFW_1 "FIRMWARE BELOW 1.80 OR UNKNOWN:"
/** @brief The old-firmware note's second line. */
#define AM32_UI_OLDFW_2 "FIELDS IT MAY NOT READ ARE HIDDEN."
/** @} */

/**
 * @defgroup am32_view The read-only view
 * @brief A plausible block in a layout outside @ref AM32_LAYOUT_MIN_KNOWN to
 *        @ref AM32_LAYOUT_MAX_KNOWN, 2 to 4: the bytes in hex, no field, no
 *        control but BACK. The strip carries two amber lines, the editor
 *        bar's place two grey ones; each at most 37 characters, 221 px.
 * @{
 */
/** @brief The strip's first line: the layout revision, 1 or 5 to 8, one digit. */
#define AM32_UI_RO_FMT "LAYOUT %u NOT KNOWN: READ-ONLY."
/** @brief The strip's second line begins with this. */
#define AM32_UI_RO_UNVERIFIED "NOT VERIFIED."
/** @brief ...then this and the backup's five-character name: it is on the card. */
#define AM32_UI_RO_SAVED " BACKUP "
/** @brief ...or this: no card, or the card held by USB. */
#define AM32_UI_RO_NO_CARD " NO CARD: NO BACKUP."
/** @brief ...or this: the card did not take the backup. */
#define AM32_UI_RO_FAILED " BACKUP FAILED."
/** @brief Under the bytes, first line. */
#define AM32_UI_RO_BYTES "THE BYTES AS READ. THE FIELD TABLE"
/** @brief Under the bytes, second line. */
#define AM32_UI_RO_TABLE "IS NOT CHECKED FOR THIS LAYOUT."
/**
 * @brief The write's own refusal of a block in a layout outside 2 to 4. The
 *        read-only view has no control that leads to the write; doWrite()
 *        refuses on the bytes as read before anything else.
 */
#define AM32_UI_RO_REFUSED "LAYOUT NOT KNOWN: NOT WRITTEN"
/** @} */

/** @brief The connect page's last line under a reply it could not parse. */
#define AM32_UI_NOT_AM32 "IS THE ESC RUNNING AM32?"
/** @brief The connect page's first line under NO VALID SETTINGS. */
#define AM32_UI_NO_SETTINGS_1 "NOT AN AM32 ESC, OR BLANK SETTINGS."
/** @brief The connect page's second line under NO VALID SETTINGS. */
#define AM32_UI_NO_SETTINGS_2 "NOTHING WILL BE WRITTEN."
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

/**
 * @brief The tester's note while @ref uiAm32Esc3d() is true: 35 characters,
 *        209 px at scale 1, amber, on the RPM area's bottom row (y 114) in
 *        place of the pole count and the swipe hint, except while a drag on
 *        the pad is engaged: SWIPE ACTIVE takes the row then.
 */
#define AM32_UI_TESTER_3D "3D MODE ESC: THROTTLE SCALE DIFFERS"

/**
 * @brief Whether the ESC on the pin may run in 3D mode, for the
 *        tester's @ref AM32_UI_TESTER_3D.
 *
 * Set by a read that opens the list on settings with 3D MODE on, and by a
 * write of settings with 3D MODE on that the ESC accepted, whether its
 * read-back then verifies, fails or differs. Cleared only by such a read or
 * a verified write with 3D MODE off, and by a restart, since it is not
 * stored. A failed read, a read that opens no list (the read-only view of a
 * layout not known among them: byte 0x12 may be another field there), a
 * write the ESC refused and a write of 3D MODE off that is not verified leave
 * it as it was. Nothing tells it that the ESC on
 * the pin was swapped for another one.
 *
 * @return true while the note stands.
 */
bool uiAm32Esc3d();

/** @brief The AM32 CONFIG controls; the list rows are `UI_ROW(n, Row)`. @see ui_screen.h */
enum class Am32Ctl : uint8_t {
	EditMinus = 1, EditPlus,  /**< The editor bar, while a field is selected. */
	Write,                    /**< `WRITE...`: a tap opens AM32 WRITE. */
	Revert,                   /**< REVERT. */
	Hex,                      /**< HEX or FIELDS. */
	Ok,                       /**< OK under the write; acts once the write has finished. */
	WriteHold,                /**< AM32 WRITE's HOLD TO WRITE, 1000 ms: the write. */
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
