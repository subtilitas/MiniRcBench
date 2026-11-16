/**
 * @file ui_am32.h
 * @brief ESC SETTINGS, the ESC settings screen: AM32, Bluejay and
 *        BLHeli_S read and written.
 *
 * A separate mode from the throttle tester, opened from the ESC CONFIG
 * hub (ui_esc_config.h): entering it claims the signal pin
 * (pin_claim.h), which disarms, drains and tears down the DShot driver, and
 * hands the pin to the bootloader transport. Leaving it gives the pin back.
 *
 * The connect page runs the connect cycle (greet_cycle.h): 1000 ms with the
 * line on the pull-up and nothing sent, then 3 greetings, in turn. The
 * greeting is answered by an AM32 bootloader or by the EFM8 bootloader of
 * Bluejay and BLHeli_S (bl_answer.h). An AM32 answer opens the settings.
 * An EFM8 answer gives the claim the leave page (pinClaimWantLeavePage()),
 * sets MENU's run cue, ends the AM32 transport and starts the claim's core1
 * job, rcbench's escwire session (escwire_job.h), which makes its own first
 * contact and reads the page (docs/design/bluejay-blheli-s.md, slices 3
 * and 4). A page of a written layout, Bluejay 208 or BLHeli_S 33, opens as
 * an editable list; an older layout, lost tags or a wrong marker as a
 * read-only list whose backup is saved as it opens; a page not taken stays
 * on the connect page, named. `WRITE...` opens ESC WRITE, whose HOLD TO
 * WRITE saves the backup of the page as first read (`BJnnn.TXT`,
 * `BHnnn.TXT`) and then has the job write the draft, verified by reading
 * back. BACK from the list sends LEAVE, `00 01`, which starts the ESC's
 * firmware, and gives SIG back to DShot; the cue stays until an eRPM frame.
 * BACK on a half-written page is guarded: the second aborts the session
 * with no LEAVE. Every other BACK after an EFM8 answer, or after a reply
 * that holds a piece of one, opens the leave page of decision 8 of that
 * note, whose RESTART DSHOT gives SIG back to DShot and clears the cue.
 *
 * @see am32_bl.h for the transport, am32_eeprom.h for the settings layout.
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

#include "am32_eeprom.h"

struct TouchState;

/** @brief Enter config mode. Claims the signal pin, which disarms. */
void uiAm32Enter();

/**
 * @brief Leave config mode and end the claim, which resumes DShot within
 *        the call unless an EFM8 bootloader answered: then the claim waits
 *        in Leaving for RESTART DSHOT, which only the screen's leave page
 *        offers, so the screen leaves through that page instead.
 */
void uiAm32Exit();

/**
 * @defgroup am32_titles Titles
 * @brief One set for every firmware (docs/design/bluejay-blheli-s.md, the
 *        developer's call A of 2026-11-15); the strip names the firmware.
 *        At scale 2 from x 8, each ends 4 px or more before BACK at x 170.
 * @{
 */
/** @brief The screen: 12 characters, 142 px. ESC CONFIG, the hub, names its entry so too. */
#define AM32_UI_TITLE "ESC SETTINGS"
/** @brief The hex dump of the list: 7 characters, 82 px. */
#define AM32_UI_TITLE_HEX "ESC HEX"
/** @brief The write page: 9 characters, 106 px. */
#define AM32_UI_TITLE_WRITE "ESC WRITE"
/** @} */

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
 * @defgroup am32_write ESC WRITE
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
 *        settings as read stay in RAM, and leaving ESC SETTINGS loses them.
 *        Amber. 35 characters, 209 px.
 */
#define AM32_UI_NO_CARD_LOST "NO CARD: OLD VALUES LOST ON LEAVING"
/** @brief As @ref AM32_UI_NO_CARD_LOST while a USB host holds the card. 37 characters, 221 px. */
#define AM32_UI_USB_LOST "USB DRIVE: OLD VALUES LOST ON LEAVING"
/**
 * @brief The line under UI_BOARD_LOW_1 on ESC WRITE. The result stands
 *        under OK, then WRITE... and the page's hold write again. 37
 *        characters, 221 px. KST and Hitec keep their hold live under the
 *        same refusal and say UI_BOARD_LOW_2.
 */
#define AM32_UI_BOARD_LOW_2 "CHARGE THE CELL. TAP OK, WRITE AGAIN."
/**
 * @brief The write's refusal of settings that differ from what the page
 *        showed when its hold began: the page writes what it shows or
 *        nothing. No path through the screen reaches it. 35 characters.
 */
#define AM32_UI_PAGE_STALE "CHANGED SINCE THE PAGE: NOT WRITTEN"
/** @brief The line under @ref AM32_UI_PAGE_STALE. 20 characters. */
#define AM32_UI_PAGE_STALE_2 "TAP OK, WRITE AGAIN."
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
 *        bar's place two `C_DIM` ones; each at most 37 characters, 221 px.
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

/**
 * @brief The connect page's status after an EFM8 bootloader answered,
 *        with its signature, while the escwire session enters and reads:
 *        28 characters, 167 px, for example EFM8 E8B2 ANSWERED: READING.
 */
#define AM32_UI_EFM8_FMT "EFM8 %04X ANSWERED: READING."

/** @brief The connect cycle's status in its quiet phase: 23 characters, 137 px. */
#define AM32_UI_QUIET "LINE HIGH, NOTHING SENT"

/**
 * @defgroup am32_efm8 The EFM8 pages
 * @brief The connect page while the escwire session holds SIG, the list's
 *        bar and the page while LEAVE goes out (docs/design/bluejay-blheli-s.md,
 *        slice 3). Each line at most 37 characters, centred at scale 1; the
 *        headings at scale 2 at most 18 characters.
 * @{
 */
/** @brief The connect page's heading while the session holds SIG, in place of POWER-CYCLE THE ESC. */
#define AM32_UI_EW_HEAD "EFM8 BOOTLOADER"
/** @brief Under it, first line: 31 characters. */
#define AM32_UI_EW_1 "THE ESC WAITS IN ITS BOOTLOADER"
/** @brief Second line: 31 characters. */
#define AM32_UI_EW_2 "FOR COMMANDS. ITS FIRMWARE DOES"
/** @brief Third line: 24 characters. */
#define AM32_UI_EW_3 "NOT RUN UNTIL IT LEAVES."
/** @brief An entry or read that ended without the page, first line: 31 characters, amber. */
#define AM32_UI_EW_STOPPED_1 "THE ESC ANSWERED, THEN STOPPED."
/** @brief Its second line: the connect cycle greets again. */
#define AM32_UI_EW_STOPPED_2 "SEARCHING AGAIN."
/** @brief The list's bar, second line, under EFM8_UI_BAR_EDIT or EFM8_UI_BAR_SHOWN: 33 characters, 197 px, `C_DIM`. */
#define AM32_UI_EW_BAR_2 "BACK SENDS LEAVE: THE ESC STARTS."
/** @brief The page while LEAVE goes out: its heading. */
#define AM32_UI_EXIT_HEAD "LEAVING"
/** @brief Under it, first line: 30 characters. */
#define AM32_UI_EXIT_1 "THE ESC STARTS ITS FIRMWARE ON"
/** @brief Second line: 31 characters. ESC TESTER's DShot runs it, not the leave. */
#define AM32_UI_EXIT_2 "LEAVE. ESC TESTER THEN RUNS IT."
/** @brief Third line: 23 characters. */
#define AM32_UI_EXIT_3 "KEEP THE PROPELLER OFF."
/** @brief The status while LEAVE runs: 20 characters. */
#define AM32_UI_EXIT_SENDING "SENDING LEAVE: 00 01"
/** @brief The status once it went: 11 characters. */
#define AM32_UI_EXIT_SENT "LEAVE SENT."
/** @} */

/**
 * @defgroup am32_leave The leave page after an EFM8 answer
 * @brief Decision 8 of docs/design/bluejay-blheli-s.md: the leave page of
 *        SERVO TESTER, KST SERVO and HITEC SERVO (ui_leave.h), opened
 *        before LEAVE. A step line starts at x 20: at most 35 characters.
 * @{
 */
/** @brief Step 1: 35 characters, 209 px, to x 228. */
#define AM32_UI_LEAVE_1 "UNPLUG THE ESC BATTERY, THEN LEAVE."
/** @brief Step 1's first detail line: 33 characters, 197 px. */
#define AM32_UI_LEAVE_1A "ITS BOOTLOADER WAITS FOR COMMANDS"
/** @brief Step 1's second detail line: 26 characters, 155 px. */
#define AM32_UI_LEAVE_1B "AND MAY READ DSHOT AS ONE."
/**
 * @brief Step 2, with the title of the hub ESC SETTINGS returns to in
 *        place of the format's string: PLUG IT IN ONCE ESC CONFIG SHOWS.,
 *        33 characters. SIG is parked
 *        low by then, where an EFM8 does not enter its bootloader (from
 *        source, not measured).
 */
#define AM32_UI_LEAVE_2 "PLUG IT IN ONCE %s SHOWS."
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
 * Set when a handshake with the bootloader succeeds, and when an EFM8
 * bootloader answers the greeting. Cleared by an eRPM frame that arrived
 * after that answer, which only the ESC's firmware sends, and by RESTART
 * DSHOT on the leave page, whose steps power-cycle the ESC; a restart of the
 * board clears it as well, since it is not stored. LEAVE's `00 01` leaves it:
 * LEAVE has no answer, so only an eRPM frame after it shows that the firmware
 * started. A reply that holds a piece of an EFM8 answer sets it too.
 *
 * @param lastRpmMs EscTelemetry::lastRpmMs.
 * @return true while the cue stands.
 */
bool uiAm32RunCue(uint32_t lastRpmMs);

/** @brief The ESC SETTINGS controls; the list rows are `UI_ROW(n, Row)`. @see ui_screen.h */
enum class Am32Ctl : uint8_t {
	EditMinus = 1, EditPlus,  /**< The editor bar, while a field is selected. */
	Write,                    /**< `WRITE...`: a tap opens ESC WRITE. */
	Revert,                   /**< REVERT. */
	Hex,                      /**< HEX or FIELDS. */
	Ok,                       /**< OK under the write; acts once the write has finished. */
	WriteHold,                /**< ESC WRITE's HOLD TO WRITE, 1000 ms: the write. */
	Stay,                     /**< STAY on the leave page, before LEAVE: back to the screen. */
	Leave,                    /**< LEAVE on the leave page: the transport lets go of SIG. */
	Release,                  /**< The hub's title in LEAVE's place: parks the pins and returns to the hub, live 1000 ms after SIG is let go. */
	ReadAgain,                /**< READ AGAIN in WRITE...'s place on the EFM8 list, after a write that needs a read. */
	OldPage,                  /**< OLD PAGE on the connect page: the page kept from a session that ended half written. */
};

struct UiControl;

/**
 * @brief The ESC SETTINGS controls on the current frame. @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiAm32Controls(UiControl *out, uint8_t max);

struct UiScreen;
/** @brief ESC SETTINGS. */
inline constexpr const UiScreen UI_AM32 = {
	UiScreenId::Am32, AM32_UI_TITLE, PinUser::Am32, nullptr,
	uiAm32Enter, uiAm32Tick, nullptr, uiAm32Controls, nullptr, false,
};
