/**
 * @file ui_am32.cpp
 * @brief ESC SETTINGS: connect, browse AM32 settings, edit, write back; a
 *        Bluejay or BLHeli_S page read, edited and written through the
 *        escwire job, LEAVE, and the leave page after an EFM8 answer.
 *
 * The screen is a state machine. Nothing touches the ESC until the pin claim
 * holds the signal pin (pin_claim.h), and nothing writes until the user has
 * tapped `WRITE...`, read the page that names every change, and held its
 * HOLD TO WRITE down for a full second: a stray tap must never reprogram a
 * motor controller (docs/design/menu-b.md, decision 6).
 */

#include "ui_am32.h"
#include "settings.h"
#include "am32_backup.h"
#include "am32_bl.h"
#include "card_cache.h"
#include "card_dir.h"
#include "am32_eeprom.h"
#include "efm8_backup.h"
#include "efm8_view.h"
#include "esc_3d.h"
#include "esc_task.h"
#include "escwire_job.h"
#include "greet_cycle.h"
#include "pin_claim.h"
#include "touch.h"
#include "usb_msc.h"
#include "ui.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_leave.h"
#include "ui_list.h"
#include "ui_widgets.h"
#include "ui_screen.h"
#include "gfx.h"
#include "config.h"

#include "plat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ROW_H        UI_LIST_TAP_MIN  /**< Height of one settings row, 40 px. */
#define ROW_PITCH    42      /**< Row top to row top: a 2 px gap under each row. */
#define HEX_ROW_H    11      /**< Height of one hex-dump line. */
#define LIST_Y0      UI_BODY_Y  /**< First pixel row of the list viewport. */
#define LIST_Y1      215     /**< Last pixel row of the list viewport. */
#define LIST_H       (LIST_Y1 - LIST_Y0 + 1)
/** @brief Lines of the hex dump: 8 bytes each. */
#define HEX_LINES    ((AM32_SETTINGS_SIZE + 7) / 8)
/** @brief Coarse steps of a swipe that cover any field's range. */
#define SWIPE_SPAN_STEPS 11

/**
 * @defgroup am32_edit Editor bar
 * @brief Big -/+ targets for the selected row.
 *
 * The controls live at the bottom rather than inline in the row: a 26 px row is
 * far smaller than a fingertip, and putting the buttons in a fixed place means
 * they do not move as the list scrolls.
 * @{
 */
#define EDIT_Y       218
#define EDIT_H       44
#define EDIT_BTN_W   UI_EDIT_BTN_W  /**< - and +, 44 px. */
#define BTN_MINUS_X  UI_EDGE
#define BTN_PLUS_X   (GFX_W - EDIT_BTN_W - UI_EDGE)
/** @} */

#define FOOT_Y       UI_FOOT_Y  /**< Top of the write/revert/hex row. */
#define FOOT_H       UI_FOOT_H

/** @brief The three footer targets. */
static const UiRect BTN_WRITE  = { 6, FOOT_Y, 110, FOOT_H };
static const UiRect BTN_REVERT = { 122, FOOT_Y, 52, FOOT_H };
static const UiRect BTN_HEX    = { 180, FOOT_Y, 54, FOOT_H };
/** @brief The editor bar's two steppers. */
static const UiRect BTN_MINUS  = { BTN_MINUS_X, EDIT_Y, EDIT_BTN_W, EDIT_H };
static const UiRect BTN_PLUS   = { BTN_PLUS_X,  EDIT_Y, EDIT_BTN_W, EDIT_H };
/**
 * @brief The write page's wide button, as KST's and Hitec's: HOLD TO WRITE
 *        before the write, OK under the result.
 */
static const UiRect BTN_OK     = { UI_EDGE, FOOT_Y, GFX_W - 2 * UI_EDGE, FOOT_H };
/**
 * @defgroup am32_page The write page's lines
 * @{
 */
#define WP_LINE_Y    UI_BODY_Y  /**< The first change's line, y 62. */
#define WP_PITCH     20         /**< Change line to change line. */
#define WP_LINES     4          /**< Changes listed before `AND n MORE`. */
#define WP_STATE_Y   196        /**< The first state or result line. */
#define WP_STATE_PITCH 12       /**< State line to state line. */
#define WP_BACKUP_Y  236        /**< The backup line. */
/** @} */
/** @brief Characters of a scale-1 line from x 8 to x 232. */
#define RESULT_CHARS 37
// The change lines, a value moved under its label and AND n MORE end above
// the state lines; the two state lines above the backup line; the backup
// line above the hold's bar.
static_assert(WP_LINE_Y + WP_LINES * WP_PITCH + 7 < WP_STATE_Y &&
              WP_LINE_Y + (WP_LINES - 1) * WP_PITCH + 10 + 7 < WP_LINE_Y + WP_LINES * WP_PITCH,
              "the change lines run into the state lines");
static_assert(WP_STATE_Y + WP_STATE_PITCH + 7 < WP_BACKUP_Y, "the result runs into the backup line");
static_assert(WP_BACKUP_Y + 7 < FOOT_Y - UI_HOLD_BAR_H - 1, "the backup line runs into the hold's bar");
static_assert(sizeof(AM32_UI_POWER_CYCLE) - 1 <= RESULT_CHARS, "the power-cycle line runs off the panel");
static_assert(sizeof(AM32_UI_WRITING) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_NO_CARD_LOST) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_USB_LOST) - 1 <= RESULT_CHARS &&
              sizeof("BACKUP FIRST: BACKUP/AM001.TXT") - 1 <= RESULT_CHARS,
              "a write page line runs off the panel");
// The write's texts. Each status doWrite() sets fits one scale-1 line, as
// the asserts below check, so drawWritePage() never wraps one.
#define ST_NO_BACKUP    "NO BACKUP ON THE CARD: NOT WRITTEN" /**< A card refused the backup. */
#define ST_WRITE_FAILED "WRITE FAILED: "  /**< Before am32ResultText(). */
#define ST_VERIFY_READ  "VERIFY READ: "   /**< Before am32ResultText(). */
#define ST_DIFF_WIDEST  "DIFF @2F: GOT FF WANT FF" /**< The widest DIFF: the settings are 48 bytes. */
#define ST_VERIFIED     "WRITE VERIFIED"  /**< A write read back equal. */
#define ST_BACKUP       ". BACKUP "       /**< Before the backup's five-character name. */
static_assert(AM32_SETTINGS_SIZE <= 0x100, "a DIFF offset takes more than two hex digits");
static_assert(sizeof(ST_NO_BACKUP) - 1 <= RESULT_CHARS, "NO BACKUP runs off the panel");
static_assert((int)sizeof(ST_WRITE_FAILED) - 1 + am32ResultTextMax() <= RESULT_CHARS,
              "WRITE FAILED runs off the panel");
static_assert((int)sizeof(ST_VERIFY_READ) - 1 + am32ResultTextMax() <= RESULT_CHARS,
              "VERIFY READ runs off the panel");
static_assert(sizeof(ST_DIFF_WIDEST) - 1 <= RESULT_CHARS, "DIFF runs off the panel");
static_assert(sizeof(ST_VERIFIED ST_BACKUP) - 1 + 5 <= RESULT_CHARS,
              "WRITE VERIFIED. BACKUP AMnnn runs off the panel");
static_assert(sizeof(AM32_UI_RETRY) - 1 <= RESULT_CHARS, "the retry line runs off the panel");
static_assert(sizeof(AM32_UI_CHECK_CARD) - 1 <= RESULT_CHARS, "the card line runs off the panel");
static_assert(AM32_FW_READS_2A == 180, "AM32_UI_OLDFW_1 names 1.80");
// The strip notes and the connect page's lines: scale 1 from x 8.
static_assert(sizeof(AM32_UI_3D_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_3D_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_PROT_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_PROT_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_PROTO_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_PROTO_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_OLDFW_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_OLDFW_2) - 1 <= RESULT_CHARS,
              "a strip note runs off the panel");
static_assert(sizeof(AM32_UI_NO_SETTINGS_1) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_NO_SETTINGS_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_NOT_AM32) - 1 <= RESULT_CHARS,
              "a connect line runs off the panel");
// The EFM8 status: %04X is four hex digits, the signature.
static_assert(sizeof(AM32_UI_EFM8_FMT) - 1 - 4 + 4 <= RESULT_CHARS, "the EFM8 status runs off the panel");
// The EFM8 pages' lines, scale 1, and their headings, scale 2, centred.
static_assert(sizeof(AM32_UI_QUIET) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EW_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_EW_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EW_3) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EW_STOPPED_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_EW_STOPPED_2) - 1 <= RESULT_CHARS &&
              sizeof(EFM8_UI_BAR_EDIT) - 1 <= RESULT_CHARS && sizeof(EFM8_UI_BAR_SHOWN) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EW_BAR_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EXIT_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_EXIT_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EXIT_3) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_EXIT_SENDING) - 1 <= RESULT_CHARS && sizeof(AM32_UI_EXIT_SENT) - 1 <= RESULT_CHARS,
              "an EFM8 page's line runs off the panel");
static_assert(12 * (int)(sizeof(AM32_UI_EW_HEAD) - 1) - 2 <= GFX_W - 2 * UI_MARGIN &&
              12 * (int)(sizeof(AM32_UI_EXIT_HEAD) - 1) - 2 <= GFX_W - 2 * UI_MARGIN,
              "an EFM8 page's heading runs off the panel");
static_assert(EFM8_LINE_CHARS == RESULT_CHARS, "the EFM8 texts are measured against another line");
// The titles, at scale 2 from x 8, end 4 px or more before BACK.
static_assert(UI_MARGIN + 12 * (int)(sizeof(AM32_UI_TITLE) - 1) - 2 + 4 <= UI_BACK_X &&
              UI_MARGIN + 12 * (int)(sizeof(AM32_UI_TITLE_HEX) - 1) - 2 + 4 <= UI_BACK_X &&
              UI_MARGIN + 12 * (int)(sizeof(AM32_UI_TITLE_WRITE) - 1) - 2 + 4 <= UI_BACK_X,
              "an ESC SETTINGS title runs into BACK");
// The leave page's steps, from x 20.
static_assert(sizeof(AM32_UI_LEAVE_1) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(AM32_UI_LEAVE_1A) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(AM32_UI_LEAVE_1B) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(AM32_UI_LEAVE_2) - 1 <= UI_LEAVE_STEP_CHARS,
              "a leave step runs past x 232");
// The read-only view's lines: the layout is one digit (am32Plausible() takes
// 1 to 8), "%u" two characters; the backup's name five characters.
static_assert(sizeof(AM32_UI_RO_FMT) - 1 - 1 <= RESULT_CHARS, "the read-only line runs off the panel");
static_assert(sizeof(AM32_UI_RO_UNVERIFIED AM32_UI_RO_NO_CARD) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_RO_UNVERIFIED AM32_UI_RO_FAILED) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_RO_UNVERIFIED AM32_UI_RO_SAVED) - 1 + 5 <= RESULT_CHARS &&
              sizeof(AM32_UI_RO_BYTES) - 1 <= RESULT_CHARS && sizeof(AM32_UI_RO_TABLE) - 1 <= RESULT_CHARS,
              "a read-only line runs off the panel");
// The refusals of a write, on the result page: one scale-1 line each.
static_assert(sizeof(UI_BOARD_LOW_1) - 1 <= RESULT_CHARS && sizeof(AM32_UI_BOARD_LOW_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_PAGE_STALE) - 1 <= RESULT_CHARS && sizeof(AM32_UI_PAGE_STALE_2) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_RO_REFUSED) - 1 <= RESULT_CHARS,
              "a refusal runs off the panel");
/** @brief The list's viewport. */
static const UiRect LIST_VIEW  = { 0, LIST_Y0, GFX_W, LIST_H };

// Layout invariants, asserted rather than eyeballed. A caption that runs under
// a button is invisible in code review and obvious only in a screenshot.
static_assert(LIST_Y0 >= UI_BODY_Y, "the list overlaps the header strip");
static_assert(ROW_H >= UI_LIST_TAP_MIN && ROW_PITCH > ROW_H,
              "an AM32 row is lower than the list floor, or has no gap");
static_assert(LIST_Y1 < EDIT_Y, "list overlaps the editor bar");
static_assert(EDIT_Y + EDIT_H <= FOOT_Y, "editor bar overlaps the footer");
static_assert(FOOT_Y + FOOT_H <= GFX_H, "footer runs off the panel");
static_assert(BTN_MINUS_X + EDIT_BTN_W < BTN_PLUS_X, "editor buttons overlap");
static_assert(EDIT_H >= UI_TAP_MIN && FOOT_H >= UI_TAP_MIN,
              "an AM32 button is smaller than a fingertip");

/** @brief Screen states. */
enum Am32Screen : uint8_t {
	S_HANDOVER,  /**< Waiting for the pin claim to hold the signal pin. */
	S_CONNECT,   /**< Prompting for ESC power, retrying the handshake. */
	S_LIST,      /**< Browsing and editing settings; with s_hexView, ESC HEX. */
	S_PAGE,      /**< ESC WRITE before the hold: the changes and HOLD TO WRITE. */
	S_WRITING,   /**< ESC WRITE: the write in progress, then its result. */
	/**
	 * A layout the table is not checked against: the bytes, BACK alone.
	 * No state leads from here to S_WRITING.
	 */
	S_VIEW,
	/**
	 * The leave page, after an EFM8 bootloader answered: BACK from any
	 * other state but the EFM8 list opens it. STAY returns to s_leaveFrom;
	 * LEAVE lets go of SIG; RESTART DSHOT gives it back to DShot.
	 */
	S_LEAVE,
	/**
	 * An EFM8 bootloader answered: the escwire job holds SIG, enters and
	 * reads. The connect page shows it, and a page that is not taken stays
	 * named there.
	 */
	S_EW,
	/**
	 * The Bluejay or BLHeli_S list: editable for a written layout, every
	 * row read-only otherwise; with s_hexView, ESC HEX. READ AGAIN runs here.
	 */
	S_EW_LIST,
	/** ESC WRITE of the EFM8 list before the hold: the changes and HOLD TO WRITE. */
	S_EW_PAGE,
	/** ESC WRITE of the EFM8 list: the backup, the job's write, then its result. */
	S_EW_WRITING,
	/** BACK from the EFM8 list: LEAVE goes out, then DShot returns. BACK is drawn disabled. */
	S_EW_EXIT,
	/**
	 * The session ended without the page: the job lets SIG go, and the
	 * claim returns to Held for the AM32 transport to greet again.
	 */
	S_REGREET,
};

static Am32Screen s_state = S_HANDOVER;
static uint8_t    s_eeprom[AM32_EEPROM_SIZE];
static uint8_t    s_original[AM32_EEPROM_SIZE];
static bool       s_hexView = false;
/**
 * @brief The field list, or the hex dump while `s_hexView` is set: one
 *        list, scrolled in pixels. @see ui_list.h
 */
static UiList     s_list;
static int        s_selected = -1;
static char       s_status[40] = "";
/** @brief The last write verified: the result names the power cycle. */
static bool       s_verified = false;
/** @brief The next step under a result that is not verified, or null. */
static const char *s_nextStep = nullptr;
/** @brief The session's backup is on the card; BACKUP/ holds it as s_backupName. */
static bool       s_backupDone = false;
static uint32_t   s_backupVolume = 0;     /**< The serial number of the volume the backup went to. */
static bool       s_backupOnCard = false; /**< The backup went to a card: the swap check is armed. */
static bool       s_backupVolumeRead = false; /**< s_backupVolume was read; false fails the check. */
/** @brief The settings as read at entry: the session's backup. */
static uint8_t    s_entry[AM32_SETTINGS_SIZE];
static char       s_backupName[BACKUP_NAME_MAX] = "";
/** @brief The write page's HOLD TO WRITE. */
static UiHold     s_writeHold;
/**
 * @brief A press landed on the result page after the write ended. OK takes
 *        only such a press: OK lies over HOLD TO WRITE, and the finger that
 *        ended the hold lifts there.
 */
static bool       s_okPress = false;
/** @brief The settings as read when the write page opened: the page's old values. */
static uint8_t    s_pageFrom[AM32_SETTINGS_SIZE];
/** @brief What the write page writes: the edits when it opened. */
static uint8_t    s_pageTo[AM32_SETTINGS_SIZE];
/** @brief The name the backup takes on the card, for the page's backup line; empty if not known. */
static char       s_backupNext[BACKUP_NAME_MAX] = "";
/**
 * @brief The card as the page last saw it, @ref cardSeen(): a change
 *        repaints the backup line, and a card mounted names its backup.
 */
static uint8_t    s_pageCard = 0;
/**
 * @brief BACK on the list with edits not written: the first arms, the second
 *        drops them (docs/design/menu-b.md, decision 7).
 */
static UiBackGuard s_guard;
static uint8_t    s_layoutRev = 0;
static bool       s_leaving = false;
static bool       s_redraw = true;
/** @brief pinClaimBegin() accepted this screen's claim. */
static bool       s_claimed = false;
/** @brief What became of the read-only view's backup, saved as it opened. */
enum class RoBackup : uint8_t { NoCard, Saved, Failed };
static RoBackup   s_roBackup = RoBackup::NoCard;

/** @brief Indices into AM32_FIELDS that apply to this EEPROM's layout. */
static uint16_t s_visible[64];
static uint16_t s_visibleCount = 0;

/** @brief The AM32 bootloader link is fixed at 19200 8N1. */
#define AM32_LINK_BAUD 19200

#if AM32_FORCE_LOW_JUMP
static bool     s_jumped = false;  /**< Optional low-hold jump already tried. */
#endif
static int      s_attempt = 0;     /**< Init strings sent since entering. */

/** @brief What the connect page says under its status. */
enum ConnectNote : uint8_t {
	CN_NONE,         /**< Silence or a read that failed: the status alone. */
	CN_GARBLED,      /**< Bytes came back that are not a bootloader's reply. */
	CN_NO_SETTINGS,  /**< The bootloader answered; its settings are not AM32's. */
	CN_EFM8,         /**< An EFM8 bootloader answered: the session enters and reads. */
	CN_EW_REFUSED,   /**< The page is not taken: s_ewNote1 and s_ewNote2 say why. */
	CN_EW_STOPPED,   /**< The session ended without the page; the cycle greets again. */
};
static ConnectNote s_connectNote = CN_NONE;

/** @brief The leave page: its phase and its presses. */
static UiLeave    s_leave;
/** @brief Where STAY on the leave page returns to. */
static Am32Screen s_leaveFrom = S_CONNECT;
/** @brief The signature of the EFM8 bootloader that answered in this session; 0 for none. */
static uint16_t   s_efm8Sig = 0;
/**
 * @brief An EFM8 bootloader answered in this session, or a reply held a piece
 *        of an EFM8 answer (blAnswerEfm8Fragment()): BACK opens the leave page.
 */
static bool       s_efm8Seen = false;

/** @brief The connect cycle: quiet on the pull-up, then a burst of greetings. */
static GreetCycle s_cycle;
/** @brief No greeting of this cycle has had an answer yet, or the last one had none. */
static bool       s_silent = true;

/**
 * @defgroup am32_ew The escwire session's state on core0
 * @{
 */
static EwJobStatus s_ew;               /**< The job's status, read each frame of S_EW and S_EW_EXIT. */
static bool        s_ewEntered = false; /**< Enter went to the job in this session. */
static uint16_t    s_ewSeq = 0;         /**< The seq of the last command sent to the job. */
static ew_ident_t  s_ewId;              /**< The page's identification, from the read. */
static uint8_t     s_ewPage[EW_PAGE_MAX]; /**< The page as read. */
static Efm8Row     s_ewRows[EFM8_ROWS_MAX]; /**< The list's rows. */
static int         s_ewRowCount = 0;    /**< Rows in s_ewRows. */
static int         s_ewSel = -1;        /**< The selected row; -1 for none. */
static const char *s_ewNote1 = nullptr; /**< CN_EW_REFUSED's first line. */
static const char *s_ewNote2 = nullptr; /**< CN_EW_REFUSED's second line. */
/** @brief S_EW_EXIT: LEAVE went out, and the claim is ending. */
static bool        s_exitLeft = false;
/** @brief The edits: the page as read with the list's fields stepped. */
static uint8_t     s_ewDraft[EW_PAGE_MAX];
/** @brief The session's first read: its backup, `BJnnn.TXT` or `BHnnn.TXT`. */
static uint8_t     s_ewEntry[EW_PAGE_MAX];
static ew_contact_t s_ewEntryContact;  /**< The contact the first read came under. */
static ew_ident_t  s_ewEntryId;        /**< Its identification. */
/** @brief ESC WRITE's old values: the page as read when it opened. */
static uint8_t     s_ewFrom[EW_PAGE_MAX];
/** @brief What ESC WRITE writes: the edits when it opened. */
static uint8_t     s_ewTo[EW_PAGE_MAX];
/** @brief The draft the last write sent: a half-written page takes it again, or the page as read. */
static uint8_t     s_ewSent[EW_PAGE_MAX];
/** @brief A write of ESC WRITE or the list's READ AGAIN runs on the job; BACK is drawn disabled. */
static bool        s_ewOpRun = false;
/** @brief The hold ended: the backup and the job's Write go on the next frame, after WRITING is drawn. */
static bool        s_ewStartDue = false;
static bool        s_ewDirty = false;    /**< The page is half written: only its draft or the page as read goes. */
static bool        s_ewReread = false;   /**< Half written, and read since: a write may follow. */
static bool        s_ewMustRead = false; /**< READ AGAIN in WRITE...'s place until a read succeeds. */
/** @brief The last write ended the session: OK greets the ESC again. */
static bool        s_ewEnded = false;
/** @brief Where the read-only list's backup went as it opened. */
static Efm8Backup  s_ewRoBackup = Efm8Backup::NoCard;
/** @} */

/** @brief A handshake succeeded, or an EFM8 bootloader answered, since the restart: MENU's cue stands. */
static bool     s_blReached = false;
/** @brief When that handshake succeeded, ms. */
static uint32_t s_blReachedMs = 0;

/**
 * @brief Held-button repeat state for the editor bar.
 *
 * The rule itself lives in ui_input.h now, shared with SETTINGS,
 * which had no repeat at all until it was factored out -- getting the throttle
 * ceiling from 20 % to 100 % was eight separate taps.
 */
static Repeat s_editRepeat;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/** @brief Rebuild the visible-field list for the layout revision and the
 *         firmware version just read. */
static void buildVisible() {
	s_layoutRev = s_eeprom[AM32_OFF_LAYOUT_REVISION];
	s_visibleCount = 0;
	for (uint16_t i = 0; i < AM32_FIELD_COUNT && s_visibleCount < 64; i++) {
		if (am32FieldShown(&AM32_FIELDS[i], s_eeprom)) {
			s_visible[s_visibleCount++] = i;
		}
	}
}

/** @brief True if any settings byte differs from what we read. */
static bool anyDirty() {
	return memcmp(s_eeprom, s_original, AM32_SETTINGS_SIZE) != 0;
}

/**
 * @brief The listed fields that differ between @p from and @p to: the count
 *        the BACK guard and the write page name. Each field is one byte, and
 *        the editor steps listed fields only.
 */
static int changeCount(const uint8_t *from, const uint8_t *to) {
	int n = 0;
	for (uint16_t i = 0; i < s_visibleCount; i++) {
		uint8_t off = AM32_FIELDS[s_visible[i]].offset;
		if (from[off] != to[off]) n++;
	}
	return n;
}

/** @brief The write runs: ESC WRITE shows WRITING and BACK is drawn disabled. */
static bool writing() { return s_state == S_WRITING && strcmp(s_status, "WRITING...") == 0; }

/**
 * @brief An EFM8 write, or the read after one, runs on the job: ESC WRITE
 *        shows WRITING and BACK is drawn disabled.
 */
static bool ewRunning() { return s_ewOpRun && (s_state == S_EW_WRITING || s_state == S_EW_LIST); }

/**
 * @brief Header BACK acts: in every state but while a write or the read
 *        after one runs, on the leave page and while LEAVE goes out, where
 *        it is drawn disabled (menu-b.md, decision 25).
 */
static bool backLive() { return !writing() && !ewRunning() && s_state != S_LEAVE && s_state != S_EW_EXIT; }

/**
 * @brief The write page's HOLD TO WRITE acts: there are edits, and the page
 *        names at least one of them. A byte no listed field covers is never
 *        written from a page that lists nothing.
 */
static bool pageWritable() { return anyDirty() && changeCount(s_pageFrom, s_pageTo) > 0; }

/**
 * @brief The changes BACK on the list would drop, for the guard: the listed
 *        fields that differ, and at least 1 while any byte differs.
 */
static int pendingChanges() {
	int n = changeCount(s_original, s_eeprom);
	return n ? n : (anyDirty() ? 1 : 0);
}

/** @brief The EFM8 list edits: the page is of a written layout, Bluejay 208 or BLHeli_S 33. */
static bool ewWritable() { return efm8ViewOf(&s_ewId) == Efm8View::List; }

/** @brief The EFM8 rows whose draft differs from the page as read: the count the guard names. */
static int ewPending() { return efm8Changes(s_ewRows, s_ewRowCount, s_ewPage, s_ewDraft); }

/** @brief The draft differs from the page as read. */
static bool ewEdited() { return memcmp(s_ewDraft, s_ewPage, sizeof(s_ewDraft)) != 0; }

/**
 * @brief The field the editor bar steps: the selected row, editable, on a
 *        page that is not half written, with nothing running.
 */
static const ew_field_t *ewEditField() {
	if (s_hexView || s_ewSel < 0 || s_ewSel >= s_ewRowCount || s_ewDirty || s_ewOpRun) return nullptr;
	const ew_field_t *f = s_ewRows[s_ewSel].f;
	return efm8RowEditable(&s_ewId, f) ? f : nullptr;
}

/**
 * @brief `WRITE...` acts on the EFM8 list: edits, or a half-written page
 *        read since, which takes its draft or the page as read.
 */
static bool ewWriteLive() {
	return ewWritable() && !s_ewMustRead && !s_ewOpRun && (s_ewDirty ? s_ewReread : ewEdited());
}

/** @brief REVERT acts on the EFM8 list. */
static bool ewRevertLive() { return ewWritable() && !s_ewOpRun && ewEdited(); }

/**
 * @brief ESC WRITE's hold acts: the page names a change, or the page is
 *        half written and read since, and the page writes its draft again
 *        or the page as first read (docs/design/bluejay-blheli-s.md).
 */
static bool ewPageWritable() {
	if (s_ewDirty)
		return s_ewReread && (memcmp(s_ewTo, s_ewSent, sizeof(s_ewTo)) == 0 ||
		                      memcmp(s_ewTo, s_ewFrom, sizeof(s_ewTo)) == 0);
	return efm8Changes(s_ewRows, s_ewRowCount, s_ewFrom, s_ewTo) > 0;
}

/** @brief One step of an EFM8 field for a swipe. @see efm8Step() */
static bool ewSwipeStep(void *ctx, int dir) {
	const ew_field_t *f = (const ew_field_t *)ctx;
	return efm8Step(f, &s_ewDraft[f->off], dir);
}

// hit(), button() and pressingBtn() used to live here. They were a third copy
// of the same three functions and are uiPressing() and uiButton() now.
// @see ui_widgets.h

/** @brief Touch state for the frame being drawn. Set at the top of uiAm32Tick(). */
static const TouchState *s_frameTouch = nullptr;

/**
 * @brief Set the list up for the current view, scrolled to 0 px: the fields
 *        in 26 px rows, or the hex dump in 11 px lines.
 */
static void openList() {
	if (s_hexView) uiListInit(&s_list, LIST_VIEW, HEX_ROW_H, HEX_LINES);
	else {
		uiListInit(&s_list, LIST_VIEW, ROW_PITCH, s_visibleCount);
		uiListSetBody(&s_list, ROW_H, UI_LIST_TAP_MIN);
	}
}

/**
 * @brief A field's own steps per coarse step of a swipe: the range over
 *        @ref SWIPE_SPAN_STEPS times the step, rounded up, so that 11 coarse
 *        steps, 176 px, cover any field's range.
 * @param f The field.
 * @return Steps of @p f per coarse step.
 */
static int coarseFactor(const Am32Field *f) {
	int step = f->step ? f->step : 1;
	int range = (int)f->rawMax - (int)f->rawMin;
	int span = SWIPE_SPAN_STEPS * step;
	return (range + span - 1) / span;
}

/**
 * @brief One step of a field for a swipe, the step the editor bar's - and +
 *        take: false at the rail, where the step changes nothing.
 * @param ctx The field, an Am32Field.
 * @param dir -1 or +1.
 * @return true when the value changed.
 */
static bool swipeStep(void *ctx, int dir) {
	const Am32Field *f = (const Am32Field *)ctx;
	uint8_t before = s_eeprom[f->offset];
	am32Adjust(f, s_eeprom, dir);
	return s_eeprom[f->offset] != before;
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
/** @brief The ESC's firmware reads PROTOCOL, byte 0x2E: the list shows it. */
static bool protocolShown() {
	for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++)
		if (AM32_FIELDS[i].offset == AM32_OFF_INPUT_TYPE)
			return am32FieldShown(&AM32_FIELDS[i], s_eeprom);
	return false;
}

/**
 * @brief The read-only view's strip: the layout, then NOT VERIFIED and
 *        whether the backup is on the card. @see am32_view
 * @param[out] l1 First line.
 * @param[out] l2 Second line.
 */
static void readOnlyNote(const char **l1, const char **l2) {
	static char a[40], b[40];
	snprintf(a, sizeof(a), AM32_UI_RO_FMT, (unsigned)s_eeprom[AM32_OFF_LAYOUT_REVISION]);
	if (s_roBackup == RoBackup::Saved)
		snprintf(b, sizeof(b), AM32_UI_RO_UNVERIFIED AM32_UI_RO_SAVED "%.5s", s_backupName);
	else
		snprintf(b, sizeof(b), "%s", s_roBackup == RoBackup::Failed
		                                 ? AM32_UI_RO_UNVERIFIED AM32_UI_RO_FAILED
		                                 : AM32_UI_RO_UNVERIFIED AM32_UI_RO_NO_CARD);
	*l1 = a;
	*l2 = b;
}

/**
 * @brief The strip's note for the settings list, if one applies: the
 *        selected row's first (a protection turned off), then 3D MODE on,
 *        then a PROTOCOL on which AM32 takes no DShot, shown only where the
 *        PROTOCOL row is listed, then firmware that does not read every
 *        byte. @see am32_notes
 * @param[out] l1 First line.
 * @param[out] l2 Second line.
 * @return false when none applies: the strip keeps the device caption.
 */
static bool listNote(const char **l1, const char **l2) {
	const Am32Field *sel = (!s_hexView && s_selected >= 0 && s_selected < (int)AM32_FIELD_COUNT)
	                           ? &AM32_FIELDS[s_selected] : nullptr;
	if (sel && am32ProtectionOff(sel, s_eeprom)) {
		*l1 = AM32_UI_PROT_1;
		*l2 = AM32_UI_PROT_2;
	} else if (am32Is3d(s_eeprom)) {
		*l1 = AM32_UI_3D_1;
		*l2 = AM32_UI_3D_2;
	} else if (am32InputNotDshot(s_eeprom) && protocolShown()) {
		*l1 = AM32_UI_PROTO_1;
		*l2 = AM32_UI_PROTO_2;
	} else if (am32Firmware(s_eeprom) < AM32_FW_READS_2A) {
		*l1 = AM32_UI_OLDFW_1;
		*l2 = AM32_UI_OLDFW_2;
	} else {
		return false;
	}
	return true;
}

/**
 * @brief The title: ESC HEX over the hex dump of the list, ESC WRITE on the
 *        write page, else ESC SETTINGS (docs/design/menu-b.md, decision 13,
 *        as revised by bluejay-blheli-s.md's call A). The read-only view
 *        and the leave page are the main view's: ESC SETTINGS. @see am32_titles
 */
static const char *title() {
	if ((s_state == S_LIST || s_state == S_EW_LIST) && s_hexView) return AM32_UI_TITLE_HEX;
	if (s_state == S_PAGE || s_state == S_WRITING || s_state == S_EW_PAGE || s_state == S_EW_WRITING)
		return AM32_UI_TITLE_WRITE;
	return AM32_UI_TITLE;
}

static void drawHeader() {
	uiHeader(title(), s_frameTouch, backLive());

	// The device name qualifies the title rather than being part of it, so it
	// goes on the shared strip -- the same row MENU puts its EDT chip on.
	// It used to be a second line inside a 50 px header this screen had to
	// itself, which is why BACK ended up 8 px lower here than on SETTINGS.
	// The clear covers the band of a note's second line too, y 40 to 61,
	// which the caption alone leaves standing. @see listNote()
	uiStripTwoLines(nullptr, C_BG, nullptr, C_BG);
	char name[AM32_NAME_LEN + 1];
	if (s_state == S_LIST || s_state == S_PAGE || s_state == S_WRITING || s_state == S_VIEW) {
		am32DeviceName(s_eeprom, name);
		char buf[48];
		// The firmware is named here, not in the title (am32_titles).
		if (name[0]) {
			snprintf(buf, sizeof(buf), "%s  %s  AM32 %u.%u", name, am32BlEscTypeName(),
			         s_eeprom[AM32_OFF_MAIN_REVISION], s_eeprom[AM32_OFF_SUB_REVISION]);
		} else {
			snprintf(buf, sizeof(buf), "%s  AM32 %u.%u  LAYOUT %u", am32BlEscTypeName(),
			         s_eeprom[AM32_OFF_MAIN_REVISION], s_eeprom[AM32_OFF_SUB_REVISION],
			         s_layoutRev);
		}
		const char *l1, *l2;
		if (s_state == S_PAGE || s_state == S_WRITING) {
			// The write page's count, from the settings it opened on.
			int n = changeCount(s_pageFrom, s_pageTo);
			snprintf(buf, sizeof(buf), "%d CHANGE%s", n, n == 1 ? "" : "S");
			uiStripText(buf, C_DIM);
		} else if (s_state == S_VIEW) {
			readOnlyNote(&l1, &l2);
			uiStripTwoLines(l1, UI_COL_WARN, l2, UI_COL_WARN);
		} else if (s_state == S_LIST && s_guard.armed) {
			// The armed guard names what a second BACK drops.
			uiBackGuardDraw(pendingChanges(), "WRITTEN");
		} else if (s_state == S_LIST && listNote(&l1, &l2)) {
			uiStripTwoLines(l1, UI_COL_WARN, l2, UI_COL_WARN);
		} else {
			uiStripText(buf, C_DIM);
		}
	} else if (s_state == S_EW_PAGE || s_state == S_EW_WRITING) {
		// The write page's count, from the page it opened on.
		int n = efm8Changes(s_ewRows, s_ewRowCount, s_ewFrom, s_ewTo);
		char buf[24];
		snprintf(buf, sizeof(buf), "%d CHANGE%s", n, n == 1 ? "" : "S");
		uiStripText(buf, C_DIM);
	} else if (s_state == S_EW_LIST) {
		// The armed guard first, then the selected row's note, then why the
		// list is read-only, then a half-written page, then 3D, then the
		// caption (docs/design/bluejay-blheli-s.md).
		static char ro[EFM8_LINE_CHARS + 1], ro2[EFM8_LINE_CHARS + 1];
		const char *l1 = nullptr, *l2 = nullptr;
		bool sel = !s_hexView && s_ewSel >= 0 && s_ewSel < s_ewRowCount;
		if (s_guard.armed && s_ewDirty) {
			// The second BACK leaves an erased or partly written page behind.
			uiStripTwoLines(EFM8_UI_GUARD_1, UI_COL_WARN, EFM8_UI_GUARD_2, UI_COL_WARN);
		} else if (s_guard.armed) {
			uiBackGuardDraw(ewPending(), "WRITTEN");
		} else if (sel && efm8RowNote(s_ewRows[s_ewSel].f, s_ewDraft[s_ewRows[s_ewSel].f->off], &l1, &l2)) {
			uiStripTwoLines(l1, UI_COL_WARN, l2, UI_COL_WARN);
		} else if (efm8ReadOnlyLines(&s_ewId, s_ewPage, s_ewRoBackup, s_backupName, ro, ro2)) {
			uiStripTwoLines(ro, UI_COL_WARN, ro2, UI_COL_WARN);
		} else if (s_ewDirty) {
			uiStripTwoLines(EFM8_UI_TORN_1, UI_COL_WARN, s_ewReread ? EFM8_UI_DIRTY_WRITE : EFM8_UI_DIRTY_READ,
			                UI_COL_WARN);
		} else if (efm8Is3d(s_ewDraft)) {
			uiStripTwoLines(AM32_UI_3D_1, UI_COL_WARN, AM32_UI_3D_2, UI_COL_WARN);
		} else {
			char buf[EFM8_LINE_CHARS + 1];
			efm8Caption(&s_ewId, s_ewPage, s_efm8Sig, buf, sizeof(buf));
			uiStripText(buf, C_DIM);
		}
	} else if (s_state == S_LEAVE) {
		// The bootloader that answered, and the pin it waits on.
		uint8_t sig = pinClaimGpio(PIN_SIG);
		if (sig == PIN_CLAIM_NO_GPIO) sig = settings()->dshotPin;
		char buf[32];
		// A piece of an answer names no part.
		if (s_efm8Sig)
			snprintf(buf, sizeof(buf), "EFM8 %04X  SIG GP%u", (unsigned)s_efm8Sig, (unsigned)sig);
		else
			snprintf(buf, sizeof(buf), "EFM8  SIG GP%u", (unsigned)sig);
		uiStripText(buf, C_DIM);
	} else if (s_status[0]) {
		uiStripText(s_status, C_DIM);
	}
}

static void drawConnect() {
	gfxRect(0, LIST_Y0, GFX_W, GFX_H - LIST_Y0, C_BG);

	if (s_state == S_EW) {
		// An EFM8 bootloader answered and waits: no power cycle to ask for.
		gfxTextCenter(84, AM32_UI_EW_HEAD, C_TEXT, 2);
		gfxTextCenter(116, AM32_UI_EW_1, C_DIM, 1);
		gfxTextCenter(130, AM32_UI_EW_2, C_DIM, 1);
		gfxTextCenter(144, AM32_UI_EW_3, C_DIM, 1);
	} else if (s_state == S_EW_EXIT) {
		gfxTextCenter(84, AM32_UI_EXIT_HEAD, C_TEXT, 2);
		gfxTextCenter(116, AM32_UI_EXIT_1, C_DIM, 1);
		gfxTextCenter(130, AM32_UI_EXIT_2, C_DIM, 1);
		gfxTextCenter(144, AM32_UI_EXIT_3, UI_COL_WARN, 1);
	} else {
		// The current step, in the warning colour: the one thing to do now.
		gfxTextCenter(84, "POWER-CYCLE THE ESC", UI_COL_WARN, 2);
		gfxTextCenter(116, "THE BOOTLOADER ONLY LISTENS", C_DIM, 1);
		gfxTextCenter(130, "BRIEFLY AT POWER-UP. UNPLUG", C_DIM, 1);
		gfxTextCenter(144, "AND REPLUG THE ESC BATTERY.", C_DIM, 1);
	}

	gfxTextCenter(176, s_status[0] ? s_status : "SEARCHING...", C_AMBER, 1);

	// Only if the ESC answered with something we could not parse, or with
	// settings that are not AM32's. Silence is the normal state here -- it
	// just means the power-up window has not come round yet -- so saying
	// anything about it would be noise.
	// The status above, at y 176, names the bytes or the layout; these
	// lines say what it means.
	// An EFM8 bootloader's answer: the status names it while the session
	// reads; a page not taken, or a session that stopped, has two lines.
	const char *n1 = nullptr, *n2 = nullptr;
	if (s_connectNote == CN_GARBLED) {
		n1 = "REPLY NOT UNDERSTOOD:";
		n2 = AM32_UI_NOT_AM32;
	} else if (s_connectNote == CN_NO_SETTINGS) {
		n1 = AM32_UI_NO_SETTINGS_1;
		n2 = AM32_UI_NO_SETTINGS_2;
	} else if (s_connectNote == CN_EW_REFUSED) {
		n1 = s_ewNote1;
		n2 = s_ewNote2;
	} else if (s_connectNote == CN_EW_STOPPED) {
		n1 = AM32_UI_EW_STOPPED_1;
		n2 = AM32_UI_EW_STOPPED_2;
	}
	if (n1) gfxTextCenter(196, n1, C_AMBER, 1);
	if (n2) gfxTextCenter(210, n2, C_AMBER, 1);

	// A spinner beats a static screen: it shows we are still retrying. Under
	// a note it shrinks and moves down, clear of the note's lines.
	bool lines = n1 != nullptr;
	int r = lines ? 16 : 40;
	int cy = lines ? 266 : 240;
	int phase = (int)((millis() / 150) % 12);
	for (int i = 0; i < 12; i++) {
		int ang = i * 30;
		int dx = (int)(r * cosf(ang * 3.14159f / 180.0f));
		int dy = (int)(r * sinf(ang * 3.14159f / 180.0f));
		uint16_t c = (i == phase) ? C_LIME : C_PANEL;
		gfxRect(GFX_W / 2 + dx - 2, cy + dy - 2, 5, 5, c);
	}
}

/**
 * @brief Draw list row @p index, a field, with its top edge at @p y.
 *
 * Takes the row's index and its top edge in pixels: with a pixel scroll the
 * first and last rows drawn are usually partly outside the viewport, and the
 * clip uiListDraw() sets keeps the overhang there.
 *
 * @param index Row of the list, an index into @ref s_visible.
 * @param y     Top of the row, which may be above LIST_Y0 or below LIST_Y1.
 */
static void drawRow(int index, int y, void *) {
	uint16_t fieldIdx = s_visible[index];
	const Am32Field *f = &AM32_FIELDS[fieldIdx];
	bool changed = s_eeprom[f->offset] != s_original[f->offset];
	char val[24];
	am32FormatValue(f, s_eeprom, val, sizeof(val));
	// A change is cyan with a cyan bar; selection fills the row and lights
	// the label, never the value (docs/design/menu-b.md, decision 8).
	UiField row = { f->group, f->name, val, changed ? UiValue::Changed : UiValue::Plain,
	                changed ? UiEdge::Changed : UiEdge::None, s_selected == (int)fieldIdx };
	uiListDrawField(y, ROW_H, row);
}

/**
 * @brief Draw line @p index of the hex dump with its top edge at @p y.
 * @param index Line of the dump: bytes 8 x @p index onwards.
 * @param y     Top of the line.
 */
static void drawHexLine(int index, int y, void *) {
	// 8 bytes per line keeps each row inside 240 px at scale 1: 27
	// characters, 161 px from x 8.
	int base = index * 8;
	char line[64];
	int n = snprintf(line, sizeof(line), "%02X ", base);
	for (int i = 0; i < 8 && base + i < AM32_SETTINGS_SIZE; i++) {
		n += snprintf(line + n, sizeof(line) - n, "%02X ", s_eeprom[base + i]);
	}
	gfxText(UI_MARGIN, y, line, C_DIM, 1);
}

/**
 * @brief The editor bar: - and +, the field's name over its value, and the
 *        swipe's hint. A changed field names its value as read, as KST's bar
 *        does: `PWM FREQ  24KHZ ->` over the new value; a line wider than the
 *        gap keeps `24KHZ ->` (docs/design/menu-b.md, decision 6). A value
 *        wider than the gap at scale 2 is drawn at scale 1.
 * @param name The field's name.
 * @param val  Its value now.
 * @param old  Its value as read when it changed, else null.
 */
static void drawEditBar(const char *name, const char *val, const char *old) {
	uiButton(BTN_MINUS, "-", C_GRID, C_ONACCENT, 1,
	         uiPressing(BTN_MINUS, s_frameTouch));
	uiButton(BTN_PLUS,  "+", C_GRID, C_ONACCENT, 1,
	         uiPressing(BTN_PLUS, s_frameTouch));
	// The label is worth the space: the buttons are far from the row they
	// act on, so the bar has to say what it is editing.
	int cx = BTN_MINUS_X + EDIT_BTN_W;
	int cw = BTN_PLUS_X - cx;
	char top[64];
	if (old) {
		snprintf(top, sizeof(top), "%s  %s ->", name, old);
		if (gfxTextW(top, 1) > cw) snprintf(top, sizeof(top), "%s ->", old);
	} else {
		snprintf(top, sizeof(top), "%s", name);
	}
	gfxText(cx + (cw - gfxTextW(top, 1)) / 2, EDIT_Y + 2, top, C_DIM, 1);
	int scale = gfxTextW(val, 2) <= cw ? 2 : 1;
	gfxText(cx + (cw - gfxTextW(val, scale)) / 2, EDIT_Y + (scale == 2 ? 14 : 18), val,
	        old ? UI_COL_CHANGED : C_TEXT, scale);
	// Short enough to clear both buttons: the gap between them is only
	// (BTN_PLUS_X - cx) px, and a longer caption ran under them.
	const char *hint = "SWIPE = COARSE";
	gfxText(cx + (cw - gfxTextW(hint, 1)) / 2, EDIT_Y + 33, hint, C_GRID, 1);
}

static void drawList() {
	// The clip is what makes a pixel scroll possible at all: the top and bottom
	// rows are usually partial, and without it the overhang lands on the header
	// strip above and the editor bar below. @see gfx_clip
	uiListDraw(&s_list, s_hexView ? drawHexLine : drawRow, nullptr);

	gfxRect(0, LIST_Y1 + 1, GFX_W, GFX_H - LIST_Y1 - 1, C_BG);

	// ---- editor bar: big -/+ acting on the selected row ----
	const Am32Field *sel = (s_selected >= 0 && s_selected < (int)AM32_FIELD_COUNT)
	                           ? &AM32_FIELDS[s_selected] : nullptr;

	if (sel && !s_hexView) {
		char val[24], old[24];
		am32FormatValue(sel, s_eeprom, val, sizeof(val));
		bool ch = s_eeprom[sel->offset] != s_original[sel->offset];
		if (ch) am32FormatValue(sel, s_original, old, sizeof(old));
		drawEditBar(sel->name, val, ch ? old : nullptr);
	} else if (!s_hexView) {
		gfxTextCenter(EDIT_Y + 12, "TAP A SETTING TO EDIT", C_GRID, 1);
		gfxTextCenter(EDIT_Y + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_GRID, 1);
	}

	// ---- footer ----
	bool dirty = anyDirty();
	// Both are gated on the same `dirty` their hit tests are. WRITE... opens
	// the write page on a tap; only that page holds (docs/design/menu-b.md,
	// decision 6). REVERT acts only while it is drawn live. @see #23
	uiButton(BTN_WRITE, "WRITE...", C_PANEL, C_TEXT, 1,
	         dirty && uiPressing(BTN_WRITE, s_frameTouch), dirty);
	uiButton(BTN_REVERT, "REVERT", C_PANEL, C_AMBER, 1,
	         uiPressing(BTN_REVERT, s_frameTouch), dirty);
	uiButton(BTN_HEX, s_hexView ? "FIELDS" : "HEX", C_PANEL, C_TEXT, 1,
	         uiPressing(BTN_HEX, s_frameTouch));
}

/**
 * @brief The read-only view: the bytes in hex, and under them why nothing
 *        else is shown. No button: BACK in the header is its one control.
 */
static void drawView() {
	uiListDraw(&s_list, drawHexLine, nullptr);
	gfxRect(0, LIST_Y1 + 1, GFX_W, GFX_H - LIST_Y1 - 1, C_BG);
	gfxTextCenter(EDIT_Y + 12, AM32_UI_RO_BYTES, C_GRID, 1);
	gfxTextCenter(EDIT_Y + 26, AM32_UI_RO_TABLE, C_GRID, 1);
}

/**
 * @brief Draw row @p index of the EFM8 list, a field, with its top edge at
 *        @p y: the draft's value; an editable row as AM32's, a change cyan
 *        with its bar; a row shown only grey; a value none of the field's
 *        amber.
 */
static void drawEwRow(int index, int y, void *) {
	const Efm8Row &r = s_ewRows[index];
	const uint8_t off = r.f->off;
	char val[32];
	bool ok = efm8ValueText(r.f, s_ewDraft[off], val, sizeof(val));
	bool changed = s_ewDraft[off] != s_ewPage[off];
	UiValue v = !ok ? UiValue::Odd
	            : !efm8RowEditable(&s_ewId, r.f) ? UiValue::ReadOnly
	            : changed ? UiValue::Changed : UiValue::Plain;
	UiField row = { r.group, r.f->name, val, v, changed ? UiEdge::Changed : UiEdge::None, s_ewSel == index };
	uiListDrawField(y, ROW_H, row);
}

/** @brief Lines of the EFM8 hex dump: 8 bytes each, 14 or 32. */
static int ewHexLines() { return (s_ewId.len + 7) / 8; }

/**
 * @brief Line @p index of the EFM8 hex dump: the offset and 8 bytes, or
 *        fewer on the last line of a 255-byte page.
 */
static void drawEwHexLine(int index, int y, void *) {
	int base = index * 8;
	char line[64];
	int n = snprintf(line, sizeof(line), "%02X ", base);
	for (int i = 0; i < 8 && base + i < s_ewId.len; i++)
		n += snprintf(line + n, sizeof(line) - n, "%02X ", s_ewPage[base + i]);
	gfxText(UI_MARGIN, y, line, C_DIM, 1);
}

/** @brief Set the EFM8 list up for the current view, scrolled to 0 px. */
static void openEwList() {
	if (s_hexView) uiListInit(&s_list, LIST_VIEW, HEX_ROW_H, ewHexLines());
	else {
		uiListInit(&s_list, LIST_VIEW, ROW_PITCH, s_ewRowCount);
		uiListSetBody(&s_list, ROW_H, UI_LIST_TAP_MIN);
	}
}

/**
 * @brief The EFM8 list: the rows or the hex dump; the editor bar on an
 *        editable row, else the bar's two lines; WRITE..., or READ AGAIN in
 *        its place after a write that needs a read, REVERT, HEX or FIELDS.
 */
static void drawEwList() {
	uiListDraw(&s_list, s_hexView ? drawEwHexLine : drawEwRow, nullptr);
	gfxRect(0, LIST_Y1 + 1, GFX_W, GFX_H - LIST_Y1 - 1, C_BG);
	const ew_field_t *f = ewEditField();
	if (f) {
		char val[32], old[32];
		bool ch = s_ewDraft[f->off] != s_ewPage[f->off];
		efm8ValueText(f, s_ewDraft[f->off], val, sizeof(val));
		if (ch) efm8ValueText(f, s_ewPage[f->off], old, sizeof(old));
		drawEditBar(f->name, val, ch ? old : nullptr);
	} else {
		gfxTextCenter(EDIT_Y + 12, ewWritable() ? EFM8_UI_BAR_EDIT : EFM8_UI_BAR_SHOWN, C_GRID, 1);
		gfxTextCenter(EDIT_Y + 26, AM32_UI_EW_BAR_2, C_GRID, 1);
	}
	if (s_ewMustRead) {
		bool live = !s_ewOpRun;
		uiButton(BTN_WRITE, "READ AGAIN", C_PANEL, C_TEXT, 1, live && uiPressing(BTN_WRITE, s_frameTouch), live);
	} else {
		bool live = ewWriteLive();
		uiButton(BTN_WRITE, "WRITE...", C_PANEL, C_TEXT, 1, live && uiPressing(BTN_WRITE, s_frameTouch), live);
	}
	bool rev = ewRevertLive();
	uiButton(BTN_REVERT, "REVERT", C_PANEL, C_AMBER, 1, rev && uiPressing(BTN_REVERT, s_frameTouch), rev);
	uiButton(BTN_HEX, s_hexView ? "FIELDS" : "HEX", C_PANEL, C_TEXT, 1,
	         !s_ewOpRun && uiPressing(BTN_HEX, s_frameTouch), !s_ewOpRun);
}

/**
 * @brief The backup line of the write page: where the settings as read go
 *        before the first write, or went. Amber where they stay in RAM only.
 * @param[out] col The line's colour.
 * @return The line.
 */
static const char *backupLine(uint16_t *col) {
	static char b[48];
	*col = C_DIM;
	if (s_backupDone) {
		snprintf(b, sizeof(b), "BACKUP: BACKUP/%s", s_backupName);
	} else if (!cardMounted()) {
		*col = UI_COL_WARN;
		snprintf(b, sizeof(b), "%s", mscHoldsCard(mscGetState()) ? AM32_UI_USB_LOST : AM32_UI_NO_CARD_LOST);
	} else if (s_backupNext[0]) {
		snprintf(b, sizeof(b), "BACKUP FIRST: BACKUP/%s", s_backupNext);
	} else {
		snprintf(b, sizeof(b), "BACKUP FIRST: TO THE CARD");
	}
	return b;
}

/**
 * @brief ESC WRITE under its change lines: the state, the backup line, then
 *        HOLD TO WRITE before the write or OK after it.
 * @param page     Before the hold: HOLD TO WRITE.
 * @param running  The write runs: WRITING: DO NOT UNPLUG., OK disabled.
 * @param verified The second line under a verified write, `C_TEXT`.
 * @param holdLive HOLD TO WRITE acts.
 */
static void drawWriteTail(bool page, bool running, const char *verified, bool holdLive) {
	if (running) {
		gfxText(UI_MARGIN, WP_STATE_Y, AM32_UI_WRITING, UI_COL_WARN, 1);
	} else if (!page) {
		// The result. Every status of the write fits one scale-1 line: the
		// asserts at RESULT_CHARS. Verified: green; AM32 reads its settings
		// at boot, so whether it runs the new ones without a power cycle is
		// not known, and the line is right either way; an EFM8 firmware
		// reads them when LEAVE starts it. Not verified or refused: amber,
		// and the next step under it.
		gfxText(UI_MARGIN, WP_STATE_Y, s_status, s_verified ? UI_COL_SAFE : UI_COL_WARN, 1);
		if (s_verified && verified)
			gfxText(UI_MARGIN, WP_STATE_Y + WP_STATE_PITCH, verified, C_TEXT, 1);
		else if (!s_verified && s_nextStep)
			gfxText(UI_MARGIN, WP_STATE_Y + WP_STATE_PITCH, s_nextStep, UI_COL_WARN, 1);
	}
	uint16_t bc;
	const char *bl = backupLine(&bc);
	gfxText(UI_MARGIN, WP_BACKUP_Y, bl, bc, 1);

	if (page) {
		// C_RED while holding rather than C_REDDARK: that colour's other job
		// is the dead-telemetry digit, which has to read as "off" and
		// therefore inverts direction between the two palettes. A fill that
		// carries white text needs to be dark in both, and C_RED is.
		bool holding = s_writeHold.holding;
		uiHoldDrawBar(&s_writeHold, BTN_OK, FOOT_Y - 4, millis());
		uiButton(BTN_OK, "HOLD TO WRITE", holding ? C_RED : C_PANEL, holding ? C_ONACCENT : C_TEXT, 2,
		         holding, holdLive);
	} else {
		// Pressed only under a press that can act: not under the hold's
		// finger. Drawn disabled while the write runs.
		bool done = !running;
		uiButton(BTN_OK, "OK", C_PANEL, C_TEXT, 2, done && s_okPress && uiPressing(BTN_OK, s_frameTouch),
		         done);
	}
}

/**
 * @brief ESC WRITE: the changes from the ESC as read when the page opened to
 *        the edits, field by field, label in `C_TEXT` and `old -> new`
 *        (uiChangeText()); at most @ref WP_LINES, then `AND n MORE`. Under
 *        them the state: nothing before the hold, @ref AM32_UI_WRITING while
 *        the write runs, then the result. The backup line, then HOLD TO
 *        WRITE or OK in the wide button.
 */
static void drawWritePage() {
	gfxRect(0, LIST_Y0, GFX_W, GFX_H - LIST_Y0, C_BG);
	char buf[64];  // two values of 23 characters and ` -> `
	int y = WP_LINE_Y, shown = 0;
	int total = changeCount(s_pageFrom, s_pageTo);
	for (uint16_t i = 0; i < s_visibleCount; i++) {
		const Am32Field *f = &AM32_FIELDS[s_visible[i]];
		if (s_pageFrom[f->offset] == s_pageTo[f->offset]) continue;
		if (shown == WP_LINES) {
			snprintf(buf, sizeof(buf), "AND %d MORE", total - WP_LINES);
			gfxText(UI_MARGIN, y, buf, C_DIM, 1);
			break;
		}
		char a[24], b[24];
		am32FormatValue(f, s_pageFrom, a, sizeof(a));
		am32FormatValue(f, s_pageTo, b, sizeof(b));
		gfxText(UI_MARGIN, y, f->name, C_TEXT, 1);
		snprintf(buf, sizeof(buf), "%s -> %s", a, b);
		// Values that would meet the label go to the line below it.
		int x = GFX_W - UI_MARGIN - gfxTextW(buf, 1);
		int vy = UI_MARGIN + gfxTextW(f->name, 1) + 6 > x ? y + 10 : y;
		uiChangeText(x, vy, a, b);
		y += WP_PITCH;
		shown++;
	}
	drawWriteTail(s_state == S_PAGE, writing(), AM32_UI_POWER_CYCLE, pageWritable());
}

/**
 * @brief ESC WRITE of a Bluejay or BLHeli_S page: the changes from the page
 *        as read when it opened to the edits, field by field, as AM32's
 *        page lists them; a page with no change, a half-written page's
 *        page as read, says so (@ref EFM8_UI_AS_READ). The state lines, the
 *        backup line and HOLD TO WRITE or OK as on AM32's.
 */
static void drawEwWritePage() {
	gfxRect(0, LIST_Y0, GFX_W, GFX_H - LIST_Y0, C_BG);
	char buf[80];
	int y = WP_LINE_Y, shown = 0;
	int total = efm8Changes(s_ewRows, s_ewRowCount, s_ewFrom, s_ewTo);
	if (total == 0) gfxText(UI_MARGIN, y, EFM8_UI_AS_READ, C_TEXT, 1);
	for (int i = 0; i < s_ewRowCount; i++) {
		const ew_field_t *f = s_ewRows[i].f;
		if (s_ewFrom[f->off] == s_ewTo[f->off]) continue;
		if (shown == WP_LINES) {
			snprintf(buf, sizeof(buf), "AND %d MORE", total - WP_LINES);
			gfxText(UI_MARGIN, y, buf, C_DIM, 1);
			break;
		}
		char a[32], b[32];
		efm8ValueText(f, s_ewFrom[f->off], a, sizeof(a));
		efm8ValueText(f, s_ewTo[f->off], b, sizeof(b));
		gfxText(UI_MARGIN, y, f->name, C_TEXT, 1);
		snprintf(buf, sizeof(buf), "%s -> %s", a, b);
		// Values that would meet the label go to the line below it.
		int x = GFX_W - UI_MARGIN - gfxTextW(buf, 1);
		int vy = UI_MARGIN + gfxTextW(f->name, 1) + 6 > x ? y + 10 : y;
		uiChangeText(x, vy, a, b);
		y += WP_PITCH;
		shown++;
	}
	drawWriteTail(s_state == S_EW_PAGE, ewRunning(), s_nextStep, ewPageWritable());
}

static void endClaim();
static void resetBackup();
static BackupResult ewSave(char *name);
static bool backupFirst(BackupResult (*save)(char *name));
static void lookUpBackup();
static uint8_t cardSeen();

/**
 * @brief The leave page of decision 8 of docs/design/bluejay-blheli-s.md:
 *        unplug the ESC's battery, then LEAVE; plug it in after RESTART
 *        DSHOT. No left button after LEAVE: there is no next device.
 */
static void drawLeave() {
	UiLeaveText tx;
	memset(&tx, 0, sizeof(tx));
	tx.step[0][0] = AM32_UI_LEAVE_1;
	tx.step[0][1] = AM32_UI_LEAVE_1A;
	tx.step[0][2] = AM32_UI_LEAVE_1B;
	tx.step[1][0] = AM32_UI_LEAVE_2;
	tx.next = nullptr;
	if (s_leave.phase == UiLeavePhase::Releasing) tx.note[0] = UI_LEAVE_RELEASING;
	uiLeaveDraw(&s_leave, tx, s_frameTouch, millis());
}

/**
 * @brief BACK after an EFM8 answer: the leave page, before LEAVE. The
 *        transport keeps SIG until LEAVE; STAY returns to @p from.
 * @param from The state BACK left.
 */
static void openLeave(Am32Screen from) {
	s_leaveFrom = from;
	s_guard = UiBackGuard{};
	s_state = S_LEAVE;
	uiLeaveOpen(&s_leave, UiLeavePhase::Before, UI_HOLD_MS);
	s_redraw = true;
}

/**
 * @brief One frame of the leave page.
 * @param t This frame's touch.
 * @return false once RESTART DSHOT gave SIG back to DShot: the screen leaves.
 */
static bool leaveTick(const TouchState *t) {
	uint32_t now = millis();
	uiLeavePoll(&s_leave, pinClaimUnplugDue(), now);
	switch (uiLeaveTouch(&s_leave, t, now)) {
		case UiLeaveEv::Stay:
			s_state = s_leaveFrom;
			break;
		case UiLeaveEv::Leave:
			// The claim has the leave page: DShot waits for RESTART DSHOT.
			endClaim();
			break;
		case UiLeaveEv::Restart:
			if (!pinClaimUnplugDue()) break;
			pinClaimRestart(PinUser::Am32);
			// The steps power-cycled the ESC: MENU's cue goes.
			s_blReached = false;
			s_state = S_HANDOVER;
			return false;
		case UiLeaveEv::Next:
		case UiLeaveEv::None:
			break;
	}
	// RESTART DSHOT turns live by the clock: the page repaints every frame,
	// as the connect page does.
	s_redraw = true;
	return true;
}

// ---------------------------------------------------------------------------
// state machine
// ---------------------------------------------------------------------------

/**
 * @brief Ask for the signal pin. On a refusal the reason goes on the screen
 *        and S_HANDOVER asks again next frame.
 *
 * Neither refusal is reachable through the UI: MENU disarms on entry, and
 * no other screen that claims is open while this one is. AM32 is exempt from
 * the ESC-answering rule, since it needs the ESC on the pin.
 */
static void claimPins() {
	ClaimRefusal r = pinClaimBegin(PinUser::Am32, PIN_SIG);
	s_claimed = (r == ClaimRefusal::None);
	// The card's writes queue in SRAM for the claim; a running log stops.
	if (s_claimed) cardCacheBegin(SdLogEnd::ByAm32);
	if (s_claimed)
		snprintf(s_status, sizeof(s_status), "RELEASING SIG GP%u", (unsigned)settings()->dshotPin);
	else
		snprintf(s_status, sizeof(s_status), "%s",
		         r == ClaimRefusal::Armed ? "DISARM FIRST" : UI_WAIT_RESTART_TEXT);
}

void uiAm32Enter() {
	s_state = S_HANDOVER;
	s_hexView = false;
	s_selected = -1;
	s_leaving = false;
	s_redraw = true;
	uiHoldReset(&s_writeHold);
	s_okPress = false;
	s_guard = UiBackGuard{};
	s_backupNext[0] = '\0';
	s_visibleCount = 0;
	openList();
#if AM32_FORCE_LOW_JUMP
	s_jumped = false;
#endif
	s_attempt = 0;
	s_connectNote = CN_NONE;
	s_efm8Sig = 0;
	s_efm8Seen = false;
	s_ewEntered = false;
	s_ewRowCount = 0;
	s_ewSel = -1;
	s_exitLeft = false;
	s_ewOpRun = s_ewStartDue = false;
	s_ewDirty = s_ewReread = s_ewMustRead = s_ewEnded = false;
	s_ewRoBackup = Efm8Backup::NoCard;
	memset(&s_ewId, 0, sizeof(s_ewId));
	s_leaveFrom = S_CONNECT;
	s_editRepeat = Repeat{};
	am32BlSetBaud(AM32_LINK_BAUD);
	memset(s_eeprom, 0, sizeof(s_eeprom));
	memset(s_original, 0, sizeof(s_original));
	s_backupDone = false;
	s_backupOnCard = false;
	s_backupName[0] = '\0';
	s_roBackup = RoBackup::NoCard;
	claimPins();
}

/**
 * @brief The transport lets go of the pin, the card's queued writes end,
 *        and the claim ends: DShot resumes within the call, or, once an EFM8
 *        bootloader answered, the claim waits for RESTART DSHOT.
 */
static void endClaim() {
	am32BlEnd();
	cardCacheEnd();
	pinClaimEnd(PinUser::Am32);
}

void uiAm32Exit() {
	// The transport lets go of the pin before DShot takes it back. An AM32
	// claim no EFM8 bootloader answered has started no core1 job and has no
	// leave page, so pinClaimEnd() resumes the pump within the call.
	endClaim();
	s_state = S_HANDOVER;
}

/**
 * @brief The read-only view's backup, as a session's before its first
 *        write: to the card as the view opens, with a card in, since no
 *        write follows. The AM32 transport blocks core0 anyway, so the flush
 *        runs on the spot.
 */
static void saveReadOnlyBackup() {
	s_roBackup = RoBackup::NoCard;
	if (!cardMounted()) return;
	BackupResult b = am32BackupSave(s_entry, am32BlEscTypeName(), am32BlEepromAddr(), s_backupName);
	if (b == BackupResult::Ok && cardCacheFlush() == CardCache::Failed) b = BackupResult::WriteError;
	if (cardCacheState() == CardCache::Failed) cardCacheRecover();
	s_roBackup = b == BackupResult::Ok ? RoBackup::Saved : RoBackup::Failed;
	s_backupDone = b == BackupResult::Ok;
}

/** @brief Attempt one handshake plus settings read. */
static void tryConnect() {
	uint8_t info[9];

	// The bootloader opens its listening window at power-up, so the way in is
	// to repeat the init string as fast as possible and be transmitting when
	// that window appears. Holding the line low, in contrast, makes us deaf for
	// its duration and loses more windows than it forces.
#if AM32_FORCE_LOW_JUMP
	if (!s_jumped) {
		am32BlJumpToBootloader();
		s_jumped = true;
	}
#endif

	s_attempt++;
	Am32Result r = am32BlHandshake(info);
	// Only silence lets the quiet phase replace the status.
	s_silent = am32BlAnswer().kind == BlAnswerKind::Silent;
	if (r != AM32_OK) {
		const uint8_t *rx;
		uint8_t n = am32BlLastRx(&rx);
		BlAnswer a = am32BlAnswer();

		if (a.kind == BlAnswerKind::Efm8) {
			// Bluejay's or BLHeli_S's bootloader: it waits for commands with
			// no timeout, and DShot frames could pass as one. The claim gets
			// the leave page first, MENU the run cue; then the transport lets
			// go of SIG, last, and the claim's job takes it on core1's next
			// pass (docs/design/bluejay-blheli-s.md, "The escwire session").
			s_efm8Sig = a.signature;
			s_efm8Seen = true;
			pinClaimWantLeavePage(PinUser::Am32);
			s_blReached = true;
			s_blReachedMs = millis();
			snprintf(s_status, sizeof(s_status), AM32_UI_EFM8_FMT, (unsigned)a.signature);
			s_connectNote = CN_EFM8;
			s_ewEntered = false;
			am32BlEnd();
			pinClaimStartJob(PinUser::Am32);
			s_state = S_EW;
			return;
		}
		if (n) {
			// Bytes arrived: the link is alive and this is a framing or rate
			// problem, which is a different fix from silence entirely.
			snprintf(s_status, sizeof(s_status), "%luB: %02X %02X %02X @%lu",
			         (unsigned long)n, rx[0], n > 1 ? rx[1] : 0, n > 2 ? rx[2] : 0,
			         (unsigned long)am32BlBaud());
			s_connectNote = CN_GARBLED;
			if (a.fragment) {
				// A piece of an EFM8 answer, a byte of it lost: its
				// bootloader may wait on the line, so DShot follows only
				// from the leave page, and MENU gets the run cue.
				s_efm8Seen = true;
				pinClaimWantLeavePage(PinUser::Am32);
				s_blReached = true;
				s_blReachedMs = millis();
			}
			return;
		}

		// Silence after a session that stopped keeps its two lines.
		snprintf(s_status, sizeof(s_status), "WAITING... (TRY %d)", s_attempt);
		if (s_connectNote != CN_EW_STOPPED) s_connectNote = CN_NONE;
		return;
	}
	// The ESC sits in its bootloader now, and leaving sends no command that
	// starts its firmware: MENU says so until an eRPM frame arrives.
	s_blReached = true;
	s_blReachedMs = millis();
	s_connectNote = CN_NONE;

	r = am32BlRead(am32BlEepromAddr(), s_eeprom, AM32_SETTINGS_SIZE);
	if (r != AM32_OK) {
		snprintf(s_status, sizeof(s_status), "READ FAILED (%s)", am32ResultText(r));
		return;
	}

	if (!am32Plausible(s_eeprom)) {
		// The screen stays on the connect page, which has no WRITE: the
		// list is never opened on these bytes.
		snprintf(s_status, sizeof(s_status), "NO VALID SETTINGS");
		s_connectNote = CN_NO_SETTINGS;
		return;
	}
	memcpy(s_original, s_eeprom, sizeof(s_eeprom));
	memcpy(s_entry, s_eeprom, sizeof(s_entry));
	// An EFM8 session before this answer kept its own backup.
	resetBackup();
	s_selected = -1;
	// A layout the table is not checked against may have moved any field:
	// the read-only view, the bytes alone, and the backup. Its state has no
	// way to S_WRITING (uiAm32Tick()).
	bool known = am32LayoutKnown(s_eeprom);
	s_hexView = !known;
	buildVisible();
	openList();
	if (!known) saveReadOnlyBackup();
	s_state = known ? S_LIST : S_VIEW;
	s_redraw = true;
	snprintf(s_status, sizeof(s_status), "READ OK");
	// Only the list reads byte 0x12 as 3D MODE: in a layout not known it
	// may hold another field, so the read-only view leaves the note.
	if (known) esc3dSetNote(am32Is3d(s_eeprom));
}

/**
 * @brief The quiet phase's status: the line on the pull-up, nothing sent.
 *        An answer's status stays until the next answer replaces it.
 */
static void quietStatus() {
	if (s_silent) snprintf(s_status, sizeof(s_status), AM32_UI_QUIET);
}

/** @brief The transport takes SIG again and the connect cycle begins with its quiet phase. */
static void beginConnect() {
	am32BlBegin(pinClaimGpio(PIN_SIG));
	greetCycleStart(&s_cycle, millis());
	s_silent = true;
	quietStatus();
	s_state = S_CONNECT;
}

/**
 * @brief The session ended without the page: the job lets SIG go, and the
 *        claim, which keeps its leave page, returns to Held for the
 *        transport (S_REGREET). The ESC may still wait in its bootloader.
 */
static void regreet() {
	s_connectNote = CN_EW_STOPPED;
	s_status[0] = '\0';
	pinClaimEnd(PinUser::Am32);
	s_state = S_REGREET;
}

/** @brief S_REGREET: once the job is idle the claim is Held again, and the cycle greets. */
static void regreetTick() {
	pinClaimPoll();
	if (!pinClaimUnplugDue()) return;
	pinClaimKeep(PinUser::Am32);
	beginConnect();
}

/** @brief Send @p op to the job and keep its seq. */
static void ewSend(EwJobOp op) { s_ewSeq = escwireJobCommand(op); }

/**
 * @brief The read ended: the list opens on the page, every row read-only.
 *        A page whose marker is wrong leaves the tester's 3D note: its
 *        firmware loads its defaults at its next start.
 */
static void openEw() {
	s_ewId = s_ew.id;
	memcpy(s_ewPage, s_ew.page, sizeof(s_ewPage));
	memcpy(s_ewDraft, s_ewPage, sizeof(s_ewDraft));
	// The session's first read: the backup before its first write.
	memcpy(s_ewEntry, s_ewPage, sizeof(s_ewEntry));
	s_ewEntryContact = s_ew.contact;
	s_ewEntryId = s_ew.id;
	resetBackup();
	s_ewDirty = s_ewReread = s_ewMustRead = s_ewEnded = false;
	s_ewOpRun = s_ewStartDue = false;
	s_guard = UiBackGuard{};
	s_ewRowCount = efm8Rows(s_ewId.fw, s_ewId.layout, s_ewRows, EFM8_ROWS_MAX);
	s_ewSel = -1;
	s_hexView = false;
	openEwList();
	if (s_ewId.why != EW_WHY_MARKER) esc3dSetNote(efm8Is3d(s_ewPage));
	s_state = S_EW_LIST;
	// A read-only list saves its backup as it opens, as AM32's read-only
	// view does: no write follows to save it.
	if (efm8ViewOf(&s_ewId) == Efm8View::ReadOnly) {
		s_ewRoBackup = Efm8Backup::NoCard;
		if (cardMounted()) {
			BackupResult b = ewSave(s_backupName);
			if (b == BackupResult::Ok && cardCacheFlush() == CardCache::Failed) b = BackupResult::WriteError;
			if (cardCacheState() == CardCache::Failed) cardCacheRecover();
			s_ewRoBackup = b == BackupResult::Ok ? Efm8Backup::Saved : Efm8Backup::Failed;
			s_backupDone = b == BackupResult::Ok;
		}
	}
	s_redraw = true;
}

/**
 * @brief S_EW: the job binds, enters and reads. A page not taken stays
 *        named on the connect page; anything else that ends the session
 *        without the page greets again.
 */
static void ewTick() {
	pinClaimPoll();
	uint8_t err = pinClaimJobError();
	if (err) {
		static const char *const WHY[] = {"", "REFUSED", "PWM", "PIO", "DMA", "CLOCK", "GP1", "PIN"};
		snprintf(s_status, sizeof(s_status), "SIG GP%u COULD NOT BE TAKEN (%s)",
		         (unsigned)pinClaimGpio(PIN_SIG), WHY[err < 8 ? err : 1]);
		return;
	}
	if (pinClaimState() != ClaimState::Running) return;
	escwireJobStatus(&s_ew);
	if (!s_ewEntered) {
		ewSend(EwJobOp::Enter);
		s_ewEntered = true;
		return;
	}
	if (s_ew.ackSeq != s_ewSeq || s_ew.job == EwJobState::Busy) return;
	// The page not taken: nothing more to do until BACK.
	if (s_connectNote == CN_EW_REFUSED) return;
	if (s_ew.refused != EwJobRefusal::None) {
		regreet();
		return;
	}
	const bool efm8 = (s_ew.contact.signature >> 8) == BL_ANSWER_EFM8_SIG_HI;
	if (s_ew.lastOp == EwJobOp::Enter && s_ew.result == EW_SES_OK &&
	    s_ew.contact.cpu == EW_CPU_EFM8) {
		// The session's own contact names the part from here on.
		s_efm8Sig = s_ew.contact.signature;
		ewSend(EwJobOp::Read);
		return;
	}
	if (s_ew.lastOp == EwJobOp::Enter && s_ew.result == EW_SES_ERR_NOT_KNOWN && efm8) {
		s_efm8Sig = s_ew.contact.signature;
		static char st[EFM8_LINE_CHARS + 1];
		efm8RefusalLines(&s_ew.id, &s_ew.contact, s_ew.page, st, &s_ewNote1, &s_ewNote2);
		snprintf(s_status, sizeof(s_status), "%s", st);
		s_connectNote = CN_EW_REFUSED;
		return;
	}
	if (s_ew.lastOp == EwJobOp::Read && s_ew.result == EW_SES_OK &&
	    efm8ViewOf(&s_ew.id) != Efm8View::Refused) {
		openEw();
		return;
	}
	// Silence, another answer, another processor, a read that failed or a
	// stop: the transport greets again.
	regreet();
}

/** @brief BACK from the EFM8 list: LEAVE goes out (S_EW_EXIT). */
static void startExit() {
	s_exitLeft = false;
	s_hexView = false;
	snprintf(s_status, sizeof(s_status), AM32_UI_EXIT_SENDING);
	ewSend(EwJobOp::Leave);
	s_state = S_EW_EXIT;
	s_redraw = true;
}

/**
 * @brief S_EW_EXIT: LEAVE's `00 01` starts the ESC's firmware, so once it
 *        went out the claim gives SIG back to DShot without the leave page's
 *        steps (decision 3 of bluejay-blheli-s.md). LEAVE has no answer, so
 *        MENU's run cue stays until an eRPM frame after it shows the firmware
 *        runs (bench item B11). A LEAVE that did not go out opens the leave
 *        page.
 * @return false once DShot is on its way back: the screen leaves.
 */
static bool exitTick() {
	pinClaimPoll();
	if (!s_exitLeft) {
		escwireJobStatus(&s_ew);
		if (s_ew.ackSeq != s_ewSeq || s_ew.job == EwJobState::Busy) return true;
		if (s_ew.refused != EwJobRefusal::None && s_ew.inMode && !s_ew.dirty) {
			// Not taken, the session as it was: ask again. A half-written
			// page refuses LEAVE for good: the leave page then.
			ewSend(EwJobOp::Leave);
			return true;
		}
		if (!s_ew.left) {
			openLeave(S_EW);
			return true;
		}
		s_exitLeft = true;
		snprintf(s_status, sizeof(s_status), AM32_UI_EXIT_SENT);
		endClaim();
	}
	// The job lets SIG go first; DShot restarts once it is idle.
	if (!pinClaimUnplugDue()) return true;
	pinClaimRestart(PinUser::Am32);
	s_state = S_HANDOVER;
	return false;
}

/** @brief `WRITE...` on the EFM8 list: ESC WRITE, on the edits as they are now. */
static void openEwPage() {
	memcpy(s_ewFrom, s_ewPage, sizeof(s_ewFrom));
	memcpy(s_ewTo, s_ewDraft, sizeof(s_ewTo));
	uiHoldReset(&s_writeHold);
	s_okPress = false;
	s_verified = false;
	s_nextStep = nullptr;
	s_status[0] = '\0';
	s_state = S_EW_PAGE;
	lookUpBackup();
	s_redraw = true;
}

/** @brief READ AGAIN: the job reads the page; the list waits for it. */
static void ewReadAgain() {
	ewSend(EwJobOp::Read);
	s_ewOpRun = true;
	s_redraw = true;
}

/**
 * @brief READ AGAIN ended. A page not half written is the page as read
 *        from now on, and the edits go onto it field by field; a half-written
 *        page keeps the page as first read, and the write may follow. A read
 *        that failed keeps READ AGAIN; a session that ended greets again.
 */
static void ewReadPoll() {
	escwireJobStatus(&s_ew);
	if (s_ew.ackSeq != s_ewSeq || s_ew.job == EwJobState::Busy) return;
	if (s_ew.refused == EwJobRefusal::NoUi) {
		// Core0 stalled past the heartbeat: nothing went out; ask again.
		ewSend(EwJobOp::Read);
		return;
	}
	s_ewOpRun = false;
	s_redraw = true;
	if (!s_ew.inMode) {
		regreet();
		return;
	}
	if (s_ew.refused != EwJobRefusal::None || s_ew.lastOp != EwJobOp::Read || s_ew.result != EW_SES_OK) return;
	if (!s_ew.dirty) {
		uint8_t draft[EW_PAGE_MAX];
		memcpy(draft, s_ew.page, sizeof(draft));
		for (int i = 0; i < s_ewRowCount; i++) {
			const uint8_t off = s_ewRows[i].f->off;
			if (s_ewDraft[off] != s_ewPage[off]) draft[off] = s_ewDraft[off];
		}
		memcpy(s_ewPage, s_ew.page, sizeof(s_ewPage));
		memcpy(s_ewDraft, draft, sizeof(s_ewDraft));
	}
	s_ewDirty = s_ew.dirty;
	s_ewReread = s_ew.reread;
	s_ewMustRead = false;
}

/**
 * @brief The EFM8 list's touch: WRITE... or READ AGAIN, the editor bar's -
 *        and +, REVERT, HEX or FIELDS, and the list's gesture, which
 *        scrolls, selects, and steps an editable row by a swipe. A read-only
 *        list and a half-written page edit nothing.
 */
static void ewListTouch(const TouchState *t) {
	if (s_ewMustRead) {
		if (uiTapped(BTN_WRITE, t)) {
			ewReadAgain();
			return;
		}
	} else if (ewWriteLive() && uiTapped(BTN_WRITE, t)) {
		openEwPage();
		return;
	}
	const ew_field_t *f = ewEditField();
	bool onMinus = uiPressing(BTN_MINUS, t);
	bool onPlus  = uiPressing(BTN_PLUS, t);
	if (f && (onMinus || onPlus)) {
		int dir = onMinus ? -1 : +1;
		if (repeatFires(&s_editRepeat, dir, millis())) {
			efm8Step(f, &s_ewDraft[f->off], dir);
			s_redraw = true;
		}
		return;
	}
	if (!t->down) repeatFires(&s_editRepeat, 0, millis());
	if (ewRevertLive() && uiTapped(BTN_REVERT, t)) {
		memcpy(s_ewDraft, s_ewPage, sizeof(s_ewDraft));
		s_redraw = true;
		return;
	}
	if (uiTapped(BTN_HEX, t)) {
		s_hexView = !s_hexView;
		openEwList();
		s_redraw = true;
		return;
	}
	UiListEvent e = uiListTouch(&s_list, t);
	if (e.kind == UiListEv::Scrolled) {
		s_redraw = true;
		return;
	}
	if (s_hexView || e.kind == UiListEv::None) return;
	if (e.row != s_ewSel) {
		s_ewSel = e.row;
		s_redraw = true;
	}
	if (e.kind != UiListEv::Swipe || e.steps == 0) return;
	const ew_field_t *g = ewEditField();
	if (g && uiSwipeApply(e.steps, efm8CoarseFactor(g), ewSwipeStep, (void *)g)) s_redraw = true;
}

/**
 * @brief ESC WRITE before the hold: HOLD TO WRITE, 1000 ms on ui_hold,
 *        starts the write on the next frame, once WRITING is drawn.
 */
static void ewPageTouch(const TouchState *t) {
	if (cardSeen() != s_pageCard) {
		lookUpBackup();
		s_redraw = true;
	}
	bool wasHolding = s_writeHold.holding;
	if (uiHoldUpdate(&s_writeHold, BTN_OK, t, ewPageWritable(), millis())) {
		s_state = S_EW_WRITING;
		s_verified = false;
		s_nextStep = nullptr;
		snprintf(s_status, sizeof(s_status), "WRITING...");
		s_ewOpRun = true;
		s_ewStartDue = true;
		s_redraw = true;
		return;
	}
	if (wasHolding || s_writeHold.holding) s_redraw = true;
}

/**
 * @brief Whether the EFM8 write may not start, and why: a page that is not
 *        of a written layout or a draft rcbench's map does not allow, a page
 *        other than ESC WRITE showed, or the BOARD voltage in the red band.
 *        Nothing goes to the card or the ESC.
 */
static bool ewWriteRefused() {
	const size_t n = s_ewId.len;
	if (!ewWritable() || ew_image_check(s_ewId.fw, s_ewId.layout, s_ewPage, s_ewTo, n, nullptr) != EW_IMG_OK) {
		snprintf(s_status, sizeof(s_status), EFM8_UI_REFUSED_W);
		s_nextStep = nullptr;
		return true;
	}
	if (memcmp(s_ewFrom, s_ewPage, sizeof(s_ewFrom)) != 0 || memcmp(s_ewTo, s_ewDraft, sizeof(s_ewTo)) != 0) {
		snprintf(s_status, sizeof(s_status), AM32_UI_PAGE_STALE);
		s_nextStep = AM32_UI_PAGE_STALE_2;
		return true;
	}
	if (uiBoardWriteRefused()) {
		snprintf(s_status, sizeof(s_status), UI_BOARD_LOW_1);
		s_nextStep = AM32_UI_BOARD_LOW_2;
		return true;
	}
	return false;
}

/** @brief The hold ended: the refusals, the backup, then the job's Write. */
static void ewStartWrite() {
	s_ewStartDue = false;
	s_verified = false;
	if (ewWriteRefused() || !backupFirst(ewSave)) {
		s_ewOpRun = false;
		return;
	}
	memcpy(s_ewSent, s_ewTo, sizeof(s_ewSent));
	// The backup's flush may have held core0 past the heartbeat.
	escHeartbeat();
	s_ewSeq = escwireJobWrite(s_ewTo, s_ewId.len);
}

/**
 * @brief The job's write ended: its result on ESC WRITE. A verified write
 *        makes the draft the page as read and sets the tester's 3D note from
 *        it; a half-written page with 3D in its draft sets the note, since
 *        the ESC may run on it.
 */
static void ewWritePoll() {
	escwireJobStatus(&s_ew);
	if (s_ew.ackSeq != s_ewSeq || s_ew.job == EwJobState::Busy) return;
	if (s_ew.refused == EwJobRefusal::NoUi) {
		// Nothing went out: ask again, the heartbeat fed.
		escHeartbeat();
		s_ewSeq = escwireJobWrite(s_ewTo, s_ewId.len);
		return;
	}
	s_ewOpRun = false;
	if (s_ew.refused != EwJobRefusal::None) {
		snprintf(s_status, sizeof(s_status), EFM8_UI_REFUSED_W);
		s_nextStep = nullptr;
		return;
	}
	Efm8Result r = efm8WriteResult(s_ew.result, s_ew.dirty, s_ew.inMode);
	s_ewDirty = s_ew.dirty;
	s_ewReread = s_ew.reread;
	s_ewMustRead = r.mustRead;
	s_ewEnded = !s_ew.inMode;
	s_verified = r.verified;
	s_nextStep = r.l2;
	if (r.verified) {
		memcpy(s_ewPage, s_ew.page, sizeof(s_ewPage));
		memcpy(s_ewDraft, s_ew.page, sizeof(s_ewDraft));
		esc3dSetNote(efm8Is3d(s_ewPage));
		if (s_backupDone) snprintf(s_status, sizeof(s_status), ST_VERIFIED ST_BACKUP "%.5s", s_backupName);
		else snprintf(s_status, sizeof(s_status), ST_VERIFIED);
	} else {
		snprintf(s_status, sizeof(s_status), "%s", r.l1);
		if (s_ewDirty && efm8Is3d(s_ewSent)) esc3dSetNote(true);
	}
}

/**
 * @brief OK or BACK under ESC WRITE's result: the list, or, after a write
 *        that ended the session, OK greets the ESC again and BACK opens the
 *        leave page.
 * @param ok OK, else BACK.
 */
static void ewLeavePage(bool ok) {
	s_okPress = false;
	uiHoldReset(&s_writeHold);
	if (s_ewEnded && ok) regreet();
	else if (s_ewEnded) openLeave(S_EW);
	else s_state = S_EW_LIST;
	s_redraw = true;
}

/**
 * @brief Whether the write may not start, and why, on the result page: a
 *        block read in a layout the table is not checked against, settings
 *        other than those the write page showed, or the board's voltage in
 *        the red band. Nothing goes to the card or the
 *        ESC. The layout is taken from the bytes as read, not from the
 *        screen's state, so a way into S_WRITING from the read-only view,
 *        were there one, still writes nothing.
 */
static bool writeRefused() {
	if (!am32LayoutKnown(s_original) || !am32LayoutKnown(s_eeprom)) {
		snprintf(s_status, sizeof(s_status), AM32_UI_RO_REFUSED);
		s_nextStep = nullptr;
		return true;
	}
	// The page writes what it shows: the settings it listed, from the ESC as
	// read when it opened. Nothing edits under the page, so this holds on
	// every path through the screen; any other way into S_WRITING writes
	// nothing.
	if (memcmp(s_pageFrom, s_original, AM32_SETTINGS_SIZE) != 0 ||
	    memcmp(s_pageTo, s_eeprom, AM32_SETTINGS_SIZE) != 0) {
		snprintf(s_status, sizeof(s_status), AM32_UI_PAGE_STALE);
		s_nextStep = AM32_UI_PAGE_STALE_2;
		return true;
	}
	if (uiBoardWriteRefused()) {
		snprintf(s_status, sizeof(s_status), UI_BOARD_LOW_1);
		s_nextStep = AM32_UI_BOARD_LOW_2;
		return true;
	}
	return false;
}

/**
 * @brief The session's backup before its first write. The page as first
 *        read goes to the card before the first write of the session.
 *        Without a card the write runs and the backup stays in memory until
 *        a write with a card saves it; a card that refuses it stops the
 *        write: the status says so and false comes back. A card swapped
 *        since the backup reached the card holds none: the card in the slot
 *        is mounted again and the backup saved to it. Core0 waits for the
 *        flush on the spot: the AM32 transport blocks it anyway, and an EFM8
 *        bootloader waits for its next command with no timeout.
 * @param save Saves the session's backup into the ring and names it.
 * @return true when the write may go.
 */
static bool backupFirst(BackupResult (*save)(char *name)) {
	uint32_t v = 0;
	if (s_backupOnCard && cardMounted() &&
	    (!s_backupVolumeRead || !cardVolumeSerial(&v) || v != s_backupVolume)) {
		cardCacheRecover();
		s_backupDone = s_backupOnCard = false;
	}
	if (!s_backupDone && cardMounted()) {
		BackupResult b = save(s_backupName);
		// The ESC is written only once the card holds the backup.
		if (b == BackupResult::Ok && cardCacheFlush() == CardCache::Failed) b = BackupResult::WriteError;
		// A batch that failed during the save or its flush: the card is
		// mounted again, and the next WRITE saves the backup again.
		if (cardCacheState() == CardCache::Failed) cardCacheRecover();
		if (b != BackupResult::Ok) {
			snprintf(s_status, sizeof(s_status), ST_NO_BACKUP);
			s_nextStep = AM32_UI_CHECK_CARD;
			return false;
		}
		s_backupDone = true;
		// A serial number that does not read leaves the check armed and
		// failing: the next WRITE mounts the card again and saves again.
		s_backupOnCard = true;
		s_backupVolumeRead = cardVolumeSerial(&s_backupVolume);
	}
	return true;
}

/** @brief The AM32 session's backup: the settings as read at entry, `AMnnn.TXT`. */
static BackupResult am32Save(char *name) {
	return am32BackupSave(s_entry, am32BlEscTypeName(), am32BlEepromAddr(), name);
}

/** @brief The EFM8 session's backup: the page as first read, `BJnnn.TXT` or `BHnnn.TXT`. */
static BackupResult ewSave(char *name) {
	return efm8BackupSave(&s_ewEntryContact, &s_ewEntryId, s_ewEntry, name);
}

/** @brief A new device's settings were read: no backup of them yet. */
static void resetBackup() {
	s_backupDone = false;
	s_backupOnCard = false;
	s_backupName[0] = '\0';
	s_backupNext[0] = '\0';
}

/** @brief Commit the settings block. */
static void doWrite() {
	// Set again only by a write that reads back equal: the result's colour
	// and the power-cycle line follow it. The hold that starts the write
	// clears it already (pageTouch()); this is a backstop for any
	// other way into S_WRITING.
	s_verified = false;
	if (writeRefused()) return;
	if (!backupFirst(am32Save)) return;
	Am32Result r = am32BlWrite(am32BlEepromAddr(), s_eeprom, AM32_SETTINGS_SIZE);
	if (r != AM32_OK) {
		snprintf(s_status, sizeof(s_status), ST_WRITE_FAILED "%s", am32ResultText(r));
		s_nextStep = AM32_UI_RETRY;
		return;
	}
	// The ESC accepted the block. With 3D MODE on in it the tester's note is
	// set now, before the read-back: a read-back that fails or differs may
	// still leave the ESC in 3D mode. Only a verified write or a list read
	// clears it. @see esc3dNote()
	if (am32Is3d(s_eeprom)) esc3dSetNote(true);
	// Read back rather than trusting the ACK: a write that reports success but
	// lands wrong is worse than one that fails loudly.
	//
	// Give the ESC time first. It is erasing and reprogramming a flash page and
	// will not answer sensibly until that finishes -- reading too soon is what
	// makes a perfectly good write look like a failure.
	uint8_t check[AM32_SETTINGS_SIZE];
	r = AM32_ERR_TIMEOUT;
	for (int attempt = 0; attempt < 3 && r != AM32_OK; attempt++) {
		delay(AM32_WRITE_SETTLE_MS);
		r = am32BlRead(am32BlEepromAddr(), check, AM32_SETTINGS_SIZE);
	}
	if (r != AM32_OK) {
		snprintf(s_status, sizeof(s_status), ST_VERIFY_READ "%s", am32ResultText(r));
		s_nextStep = AM32_UI_RETRY;
		return;
	}
	if (memcmp(check, s_eeprom, AM32_SETTINGS_SIZE) != 0) {
		// Name the first offending byte. "Mismatch" alone cannot distinguish a
		// write that did not take from a read that came back skewed.
		int i = 0;
		while (i < AM32_SETTINGS_SIZE && check[i] == s_eeprom[i]) i++;
		snprintf(s_status, sizeof(s_status), "DIFF @%02X: GOT %02X WANT %02X",
		         i, check[i], s_eeprom[i]);
		s_nextStep = AM32_UI_RETRY;
		return;
	}
	memcpy(s_original, s_eeprom, AM32_SETTINGS_SIZE);
	s_verified = true;
	esc3dSetNote(am32Is3d(s_eeprom));
	if (s_backupDone) snprintf(s_status, sizeof(s_status), ST_VERIFIED ST_BACKUP "%.5s", s_backupName);
	else snprintf(s_status, sizeof(s_status), ST_VERIFIED);
}


/** @brief The card for the backup line: 2 mounted, 1 held by a USB host, 0 none. */
static uint8_t cardSeen() {
	if (cardMounted()) return 2;
	return mscHoldsCard(mscGetState()) ? 1 : 0;
}

/** @brief The name the backup will take on the card, for the page's backup line. */
static void lookUpBackup() {
	s_pageCard = cardSeen();
	s_backupNext[0] = '\0';
	// The card is read on the spot: the AM32 transport blocks core0 anyway,
	// and an EFM8 bootloader waits for its next command.
	bool ew = s_state == S_EW_PAGE || s_state == S_EW_WRITING;
	if (!s_backupDone && s_pageCard == 2 &&
	    (ew ? efm8BackupNext(s_ewEntryId.fw, s_backupNext) : am32BackupNext(s_backupNext)) != BackupResult::Ok)
		s_backupNext[0] = '\0';
}

/**
 * @brief `WRITE...` tapped: ESC WRITE, on the settings as they are now. The
 *        hold starts fresh, so the tap's own press is never timed.
 */
static void openPage() {
	memcpy(s_pageFrom, s_original, sizeof(s_pageFrom));
	memcpy(s_pageTo, s_eeprom, sizeof(s_pageTo));
	uiHoldReset(&s_writeHold);
	s_okPress = false;
	s_verified = false;
	s_nextStep = nullptr;
	s_status[0] = '\0';
	lookUpBackup();
	s_state = S_PAGE;
	s_redraw = true;
}

/**
 * @brief The write page before the hold: HOLD TO WRITE, 1000 ms on ui_hold,
 *        starts the write. It needs every frame, finger-off included, to time
 *        the hold and to cancel it the moment the finger moves away; only a
 *        press that began on the button counts (ui_hold.h).
 */
static void pageTouch(const TouchState *t) {
	// A card put in while the page is up: the backup line names its file.
	if (cardSeen() != s_pageCard) {
		lookUpBackup();
		s_redraw = true;
	}
	bool wasHolding = s_writeHold.holding;
	if (uiHoldUpdate(&s_writeHold, BTN_OK, t, pageWritable(), millis())) {
		s_state = S_WRITING;
		s_verified = false;
		s_nextStep = nullptr;
		snprintf(s_status, sizeof(s_status), "WRITING...");
		s_redraw = true;
		return;
	}
	if (wasHolding || s_writeHold.holding) s_redraw = true;
}

static void handleListTouch(const TouchState *t) {
	// --- WRITE...: a tap, on release, opens the write page; only that page
	//     holds (docs/design/menu-b.md, decision 6) ---
	bool dirty = anyDirty();
	if (dirty && uiTapped(BTN_WRITE, t)) {
		openPage();
		return;
	}

	// --- editor bar: also needs every frame, to drive hold-to-repeat.
	//     Same press-origin rule: a swipe on the bottom list row that slid
	//     less than one row height used to keep the selection *and* step it
	//     the moment the finger crossed into the bar ---
	const Am32Field *sel = (!s_hexView && s_selected >= 0 &&
	                        s_selected < (int)AM32_FIELD_COUNT)
	                           ? &AM32_FIELDS[s_selected] : nullptr;
	bool onMinus = uiPressing(BTN_MINUS, t);
	bool onPlus  = uiPressing(BTN_PLUS, t);

	if (sel && (onMinus || onPlus)) {
		int dir = onMinus ? -1 : +1;
		if (repeatFires(&s_editRepeat, dir, millis())) {
			am32Adjust(sel, s_eeprom, dir);
			s_redraw = true;
		}
		return;
	}
	if (!t->down) repeatFires(&s_editRepeat, 0, millis());

	// Row selection is not here: a press on the list commits nothing, and
	// handleListGesture() decides whether the gesture is a tap or a drag.

	// --- destructive taps fire on release, inside, having started inside, so
	//     a mis-tap can slide off. REVERT throws away every unsaved edit; it
	//     used to do so on touch-down ---
	if (dirty && uiTapped(BTN_REVERT, t)) {
		memcpy(s_eeprom, s_original, sizeof(s_eeprom));
		s_redraw = true;
	} else if (uiTapped(BTN_HEX, t)) {
		s_hexView = !s_hexView;
		openList();
		s_redraw = true;
	}
}

/**
 * @brief List gestures, on ui_list: a vertical drag scrolls, a sideways swipe
 *        steps the row it began on, a press that goes nowhere selects.
 *
 * The axes lock exclusively, so a swipe is either a scroll or an edit and
 * never both. Sideways gives coarse control, coarseFactor() steps of the field
 * per 16 px, while the editor-bar buttons stay one step per press for fine
 * work. A press selects nothing on its own: a tap selects on release, a swipe
 * selects its row when it locks, and a vertical drag never selects. The hex
 * dump fits the viewport, does not scroll, and takes neither taps nor
 * swipes.
 */
static void handleListGesture(const TouchState *t) {
	UiListEvent e = uiListTouch(&s_list, t);
	if (e.kind == UiListEv::Scrolled) {
		s_redraw = true;
		return;
	}
	if (s_hexView || e.kind == UiListEv::None) return;
	int field = (int)s_visible[e.row];
	// A tap comes on a release frame, which repaints anyway (uiAm32Tick()).
	if (field != s_selected) {
		s_selected = field;
		s_redraw = true;
	}
	if (e.kind != UiListEv::Swipe || e.steps == 0) return;
	const Am32Field *f = &AM32_FIELDS[field];
	if (uiSwipeApply(e.steps, coarseFactor(f), swipeStep, (void *)f)) s_redraw = true;
}

uint8_t uiAm32Controls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	// BACK is drawn disabled while the write runs and on the leave page (backLive()).
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), backLive());
	if (s_state == S_LEAVE) {
		if (s_leave.phase == UiLeavePhase::Before) {
			uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Stay), UI_LEAVE_LEFT, true);
			uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Leave), UI_LEAVE_WIDE, s_leave.leaveEnabled);
		} else {
			uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Restart), UI_LEAVE_WIDE,
			             uiLeaveRestartLive(&s_leave, millis()));
		}
		return n;
	}
	if (s_state == S_PAGE) {
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::WriteHold), BTN_OK, pageWritable());
		return n;
	}
	if (s_state == S_WRITING) {
		// drawWritePage() draws OK from the first frame; it acts once the
		// write has finished.
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Ok), BTN_OK, !writing());
		return n;
	}
	if (s_state == S_EW_PAGE) {
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::WriteHold), BTN_OK, ewPageWritable());
		return n;
	}
	if (s_state == S_EW_WRITING) {
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Ok), BTN_OK, !ewRunning());
		return n;
	}
	if (s_state == S_EW_LIST) {
		// Rows select, for their strip notes and the editor; nothing acts
		// while READ AGAIN runs.
		if (!s_hexView) uiListAddRows(&s_list, out, &n, max, !s_ewOpRun);
		if (ewEditField()) {
			uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::EditMinus), BTN_MINUS, true);
			uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::EditPlus), BTN_PLUS, true);
		}
		if (s_ewMustRead) uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::ReadAgain), BTN_WRITE, !s_ewOpRun);
		else uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Write), BTN_WRITE, ewWriteLive());
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Revert), BTN_REVERT, ewRevertLive());
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Hex), BTN_HEX, !s_ewOpRun);
		return n;
	}
	// The read-only view's, the connect page's and the EFM8 pages' one
	// control is BACK.
	if (s_state != S_LIST) return n;
	// The hex dump takes no tap, so its lines are not controls.
	if (!s_hexView) uiListAddRows(&s_list, out, &n, max, true);
	bool sel = !s_hexView && s_selected >= 0 && s_selected < (int)AM32_FIELD_COUNT;
	if (sel) {
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::EditMinus), BTN_MINUS, true);
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::EditPlus), BTN_PLUS, true);
	}
	bool dirty = anyDirty();
	uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Write), BTN_WRITE, dirty);
	uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Revert), BTN_REVERT, dirty);
	uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Hex), BTN_HEX, true);
	return n;
}

bool uiAm32Tick(const TouchState *t) {
	s_frameTouch = t;
	// A pressed button has to repaint while it is held -- and once more on the
	// release frame, or the pressed look outlives the finger -- and this
	// screen only repaints when something asks it to.
	if (t && (t->down || t->released)) s_redraw = true;

	// BACK is drawn by drawHeader() in every state, so it has to be handled in
	// every state. Doing it here rather than per-state stops it being a button
	// that is visible but inert on whichever screen forgot to check for it.
	// On release, like every navigation tap. It goes one level up: ESC HEX
	// and ESC WRITE to the list, the list through its guard to MENU. While
	// the write runs it is drawn disabled and does nothing (backLive()).
	if (backLive()) {
		bool back = false;
		if (s_state == S_LIST && !s_hexView) {
			// Edits not written: the first BACK arms the guard, the second
			// leaves and drops them; a press elsewhere disarms it
			// (docs/design/menu-b.md, decision 7).
			UiGuardEv e = uiBackGuard(&s_guard, t, pendingChanges());
			if (e != UiGuardEv::None) s_redraw = true;
			back = e == UiGuardEv::Back;
		} else if (s_state == S_EW_LIST && !s_hexView) {
			// The same guard on the EFM8 list's edits; on a half-written
			// page it names what the second BACK leaves behind
			// (docs/design/bluejay-blheli-s.md, "Safety").
			UiGuardEv e = uiBackGuard(&s_guard, t, s_ewDirty ? 1 : ewPending());
			if (e != UiGuardEv::None) s_redraw = true;
			back = e == UiGuardEv::Back;
		} else if (uiBackTapped(t)) {
			if (s_state == S_LIST) {
				s_hexView = false;
				openList();
				s_redraw = true;
			} else if (s_state == S_EW_LIST && s_hexView) {
				s_hexView = false;
				openEwList();
				s_redraw = true;
			} else if (s_state == S_EW_PAGE || s_state == S_EW_WRITING) {
				ewLeavePage(false);
			} else if (s_state == S_PAGE || s_state == S_WRITING) {
				s_okPress = false;
				uiHoldReset(&s_writeHold);
				s_state = am32LayoutKnown(s_original) ? S_LIST : S_VIEW;
				s_redraw = true;
			} else {
				back = true;
			}
		}
		if (back && s_state == S_EW_LIST && s_ewDirty) {
			// The guard's second BACK on a half-written page: the session is
			// aborted with no LEAVE, which the page refuses, and SIG goes
			// back only from the leave page (decision 8 of that note).
			ewSend(EwJobOp::Stop);
			openLeave(S_EW);
		} else if (back && s_state == S_EW_LIST) {
			// The page was read: LEAVE starts the ESC's firmware, then DShot
			// returns (docs/design/bluejay-blheli-s.md, decision 3).
			startExit();
		} else if (back && s_efm8Seen) {
			// An EFM8 bootloader answered and no LEAVE went: SIG goes back to
			// DShot only from the leave page (decision 8 of that note).
			openLeave(s_state);
		} else if (back) {
			s_leaving = true;
			uiAm32Exit();
			return false;
		}
	}

	switch (s_state) {
		case S_HANDOVER:
			if (!s_claimed) claimPins();
			if (s_claimed && pinClaimPoll() == ClaimState::Held) {
				// The pin the claim latched from the pump, not the compiled
				// default: the two part company the moment the user changes it
				// on SETTINGS.
				beginConnect();
			}
			s_redraw = true;
			break;

		case S_CONNECT:
			// The connect cycle: a burst of greetings, one a frame, about
			// 40 ms each, then a quiet phase with nothing sent (greet_cycle.h).
			if (greetCyclePhase(&s_cycle, millis()) == GreetPhase::Greet) {
				tryConnect();
				if (s_state == S_CONNECT) greetCycleSent(&s_cycle, millis());
			} else {
				quietStatus();
			}
			s_redraw = true;
			break;

		case S_EW:
			ewTick();
			s_redraw = true;
			break;

		case S_REGREET:
			regreetTick();
			s_redraw = true;
			break;

		case S_EW_LIST:
			if (s_ewOpRun) {
				// READ AGAIN runs: nothing edits until it ends.
				ewReadPoll();
				s_redraw = true;
			} else {
				ewListTouch(t);
			}
			break;

		case S_EW_PAGE:
			ewPageTouch(t);
			break;

		case S_EW_WRITING:
			if (s_ewOpRun) {
				// The page as it stands while the backup blocks core0, then
				// the job's write on core1: WRITING: DO NOT UNPLUG. and BACK
				// drawn disabled.
				s_okPress = false;
				if (s_ewStartDue) {
					drawHeader();
					drawEwWritePage();
					ewStartWrite();
				} else {
					ewWritePoll();
				}
				s_redraw = true;
				break;
			}
			if (t && t->pressed) s_okPress = true;
			if (s_okPress && uiTapped(BTN_OK, t)) ewLeavePage(true);
			break;

		case S_EW_EXIT:
			if (!exitTick()) return false;
			s_redraw = true;
			break;

		case S_LIST:
			// The buttons first, then the list's gesture, which selects and
			// steps the row a gesture began on (handleListGesture()).
			handleListTouch(t);
			handleListGesture(t);
			break;

		case S_PAGE:
			pageTouch(t);
			break;

		case S_WRITING:
			if (writing()) {
				// The page as it stands while the write blocks core0:
				// WRITING: DO NOT UNPLUG. and BACK drawn disabled.
				s_okPress = false;
				drawHeader();
				drawWritePage();
				doWrite();
				s_redraw = true;
				break;
			}
			if (t && t->pressed) s_okPress = true;
			if (s_okPress && uiTapped(BTN_OK, t)) {
				s_okPress = false;
				s_state = am32LayoutKnown(s_original) ? S_LIST : S_VIEW;
				s_redraw = true;
			}
			break;

		case S_VIEW:
			// BACK, above, is the one control: nothing here selects, steps
			// or writes.
			break;

		case S_LEAVE:
			if (!leaveTick(t)) return false;
			break;
	}

	if (s_redraw) {
		s_redraw = false;
		drawHeader();
		switch (s_state) {
			case S_HANDOVER:
			case S_CONNECT:
			case S_EW:
			case S_REGREET:
			case S_EW_EXIT:  drawConnect(); break;
			case S_EW_LIST:  drawEwList();  break;
			case S_EW_PAGE:
			case S_EW_WRITING: drawEwWritePage(); break;
			case S_LIST:     drawList();    break;
			case S_PAGE:
			case S_WRITING:  drawWritePage(); break;
			case S_VIEW:     drawView();    break;
			case S_LEAVE:    drawLeave();   break;
		}
	}

	if (s_leaving) {
		uiAm32Exit();
		return false;
	}
	return true;
}

bool uiAm32RunCue(uint32_t lastRpmMs) {
	// An eRPM frame stamped after the handshake: the firmware runs again.
	if (s_blReached && lastRpmMs != 0 && (int32_t)(lastRpmMs - s_blReachedMs) > 0)
		s_blReached = false;
	return s_blReached;
}

const UiScreen UI_AM32 = {
	UiScreenId::Am32, AM32_UI_TITLE, PinUser::Am32, nullptr,
	uiAm32Enter, uiAm32Tick, nullptr, uiAm32Controls, nullptr, false,
};
