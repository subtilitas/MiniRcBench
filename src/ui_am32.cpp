/**
 * @file ui_am32.cpp
 * @brief AM32 config screen: connect, browse settings, edit, write back.
 *
 * The screen is a state machine. Nothing touches the ESC until the pin claim
 * holds the signal pin (pin_claim.h), and nothing writes until the user has
 * held the WRITE button down for a full second — a stray tap must never
 * reprogram a motor controller.
 */

#include "ui_am32.h"
#include "settings.h"
#include "am32_backup.h"
#include "am32_bl.h"
#include "card_cache.h"
#include "card_dir.h"
#include "am32_eeprom.h"
#include "pin_claim.h"
#include "touch.h"
#include "ui_hold.h"
#include "ui_input.h"
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
#define EDIT_BTN_W   58
#define BTN_MINUS_X  6
#define BTN_PLUS_X   (GFX_W - EDIT_BTN_W - 6)
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
/** @brief The OK button on the write-result screen. */
static const UiRect BTN_OK     = { 70, 200, 100, 36 };
/** @brief Top of the write's state or result. @see drawResultText() */
#define RESULT_Y     140
/** @brief Characters of a scale-1 line from x 8 to x 232. */
#define RESULT_CHARS 37
// The result's lines, the power-cycle line 30 px under its first, end above OK.
static_assert(RESULT_Y + 30 + 7 < 200, "the result runs into OK");
static_assert(sizeof(AM32_UI_POWER_CYCLE) - 1 <= RESULT_CHARS, "the power-cycle line runs off the panel");
// The write's texts. Each status doWrite() sets fits one scale-1 line, as
// the asserts below check, so drawResultText() never wraps.
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
              sizeof(AM32_UI_NOT_AM32) - 1 <= RESULT_CHARS &&
              sizeof(AM32_UI_LAYOUT_1) - 1 <= RESULT_CHARS,
              "a connect line runs off the panel");
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
	S_LIST,      /**< Browsing and editing settings. */
	S_WRITING,   /**< Write in progress or just finished. */
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
/** @brief HOLD TO WRITE. */
static UiHold     s_writeHold;
static uint8_t    s_layoutRev = 0;
static bool       s_leaving = false;
static bool       s_redraw = true;
/** @brief pinClaimBegin() accepted this screen's claim. */
static bool       s_claimed = false;

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
	CN_LAYOUT,       /**< AM32's settings, in a layout the table is not checked against. */
};
static ConnectNote s_connectNote = CN_NONE;

/** @brief A handshake succeeded since the restart: MENU's cue stands. */
static bool     s_blReached = false;
/** @brief When that handshake succeeded, ms. */
static uint32_t s_blReachedMs = 0;
/** @brief The ESC may run in 3D mode: see uiAm32Esc3d() for what sets and clears it. */
static bool     s_esc3d = false;

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

static void drawHeader() {
	uiHeader("AM32 CONFIG", s_frameTouch);

	// The device name qualifies the title rather than being part of it, so it
	// goes on the shared strip -- the same row MENU puts its EDT chip on.
	// It used to be a second line inside a 50 px header this screen had to
	// itself, which is why BACK ended up 8 px lower here than on SETTINGS.
	// The clear covers the band of a note's second line too, y 40 to 61,
	// which the caption alone leaves standing. @see listNote()
	uiStripTwoLines(nullptr, C_BG, nullptr, C_BG);
	char name[AM32_NAME_LEN + 1];
	if (s_state == S_LIST || s_state == S_WRITING) {
		am32DeviceName(s_eeprom, name);
		char buf[48];
		if (name[0]) {
			snprintf(buf, sizeof(buf), "%s  %s  FW %u.%u", name, am32BlEscTypeName(),
			         s_eeprom[AM32_OFF_MAIN_REVISION], s_eeprom[AM32_OFF_SUB_REVISION]);
		} else {
			snprintf(buf, sizeof(buf), "%s  FW %u.%u  LAYOUT %u", am32BlEscTypeName(),
			         s_eeprom[AM32_OFF_MAIN_REVISION], s_eeprom[AM32_OFF_SUB_REVISION],
			         s_layoutRev);
		}
		const char *l1, *l2;
		if (s_state == S_LIST && listNote(&l1, &l2)) uiStripTwoLines(l1, UI_COL_WARN, l2, UI_COL_WARN);
		else uiStripText(buf, C_DIM);
	} else if (s_status[0]) {
		uiStripText(s_status, C_DIM);
	}
}

static void drawConnect() {
	gfxRect(0, LIST_Y0, GFX_W, GFX_H - LIST_Y0, C_BG);

	gfxTextCenter(84, "POWER-CYCLE THE ESC", C_LIME, 2);
	gfxTextCenter(116, "THE BOOTLOADER ONLY LISTENS", C_DIM, 1);
	gfxTextCenter(130, "BRIEFLY AT POWER-UP. UNPLUG", C_DIM, 1);
	gfxTextCenter(144, "AND REPLUG THE ESC BATTERY.", C_DIM, 1);

	gfxTextCenter(176, s_status[0] ? s_status : "SEARCHING...", C_AMBER, 1);

	// Only if the ESC answered with something we could not parse, or with
	// settings that are not AM32's. Silence is the normal state here -- it
	// just means the power-up window has not come round yet -- so saying
	// anything about it would be noise.
	// The status above, at y 176, names the bytes or the layout; these
	// lines say what it means.
	if (s_connectNote == CN_GARBLED) {
		gfxTextCenter(196, "REPLY NOT UNDERSTOOD:", C_AMBER, 1);
		gfxTextCenter(210, AM32_UI_NOT_AM32, C_AMBER, 1);
	} else if (s_connectNote == CN_NO_SETTINGS) {
		gfxTextCenter(196, AM32_UI_NO_SETTINGS_1, C_AMBER, 1);
		gfxTextCenter(210, AM32_UI_NO_SETTINGS_2, C_AMBER, 1);
	} else if (s_connectNote == CN_LAYOUT) {
		gfxTextCenter(196, AM32_UI_LAYOUT_1, C_AMBER, 1);
		gfxTextCenter(210, AM32_UI_NO_SETTINGS_2, C_AMBER, 1);
	}

	// A spinner beats a static screen: it shows we are still retrying. Under
	// a note it shrinks and moves down, clear of the note's lines.
	int r = s_connectNote == CN_NONE ? 40 : 16;
	int cy = s_connectNote == CN_NONE ? 240 : 266;
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
	bool sel = (s_selected == (int)fieldIdx);
	bool changed = s_eeprom[f->offset] != s_original[f->offset];

	gfxRect(0, y, GFX_W, ROW_H - 1, sel ? C_PANEL : C_BG);
	if (changed) gfxRect(0, y, 3, ROW_H - 1, C_AMBER);

	// The two scale-1 lines and the value, each centred in the 39 px the
	// row paints above its rule.
	gfxText(8, y + 11, f->group, C_GRID, 1);
	gfxText(8, y + 21, f->name, sel ? C_TEXT : C_DIM, 1);

	char val[24];
	am32FormatValue(f, s_eeprom, val, sizeof(val));

	gfxText(GFX_W - 8 - gfxTextW(val, 2), y + 13, val,
	        changed ? C_AMBER : (sel ? C_LIME : C_TEXT), 2);
	gfxHLine(0, y + ROW_H - 1, GFX_W, C_PANEL);
}

/**
 * @brief Draw line @p index of the hex dump with its top edge at @p y.
 * @param index Line of the dump: bytes 8 x @p index onwards.
 * @param y     Top of the line.
 */
static void drawHexLine(int index, int y, void *) {
	// 8 bytes per line keeps each row inside 240 px at scale 1.
	int base = index * 8;
	char line[64];
	int n = snprintf(line, sizeof(line), "%02X ", base);
	for (int i = 0; i < 8 && base + i < AM32_SETTINGS_SIZE; i++) {
		n += snprintf(line + n, sizeof(line) - n, "%02X ", s_eeprom[base + i]);
	}
	gfxText(6, y, line, C_DIM, 1);
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
		uiButton(BTN_MINUS, "-", C_GRID, C_ONACCENT, 1,
		         uiPressing(BTN_MINUS, s_frameTouch));
		uiButton(BTN_PLUS,  "+", C_GRID, C_ONACCENT, 1,
		         uiPressing(BTN_PLUS, s_frameTouch));
		// The label is worth the space: the buttons are far from the row they
		// act on, so the bar has to say what it is editing.
		int cx = BTN_MINUS_X + EDIT_BTN_W;
		int cw = BTN_PLUS_X - cx;
		gfxText(cx + (cw - gfxTextW(sel->name, 1)) / 2, EDIT_Y + 2, sel->name, C_DIM, 1);
		char val[24];
		am32FormatValue(sel, s_eeprom, val, sizeof(val));
		bool ch = s_eeprom[sel->offset] != s_original[sel->offset];
		gfxText(cx + (cw - gfxTextW(val, 2)) / 2, EDIT_Y + 14, val,
		        ch ? C_AMBER : C_LIME, 2);
		// Short enough to clear both buttons: the gap between them is only
		// (BTN_PLUS_X - cx) px, and a longer caption ran under them.
		const char *hint = "SWIPE = COARSE";
		gfxText(cx + (cw - gfxTextW(hint, 1)) / 2, EDIT_Y + 33, hint, C_GRID, 1);
	} else if (!s_hexView) {
		gfxTextCenter(EDIT_Y + 12, "TAP A SETTING TO EDIT", C_GRID, 1);
		gfxTextCenter(EDIT_Y + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_GRID, 1);
	}

	// ---- footer ----
	bool dirty = anyDirty();
	// C_RED while holding rather than C_REDDARK: that colour's other job is the
	// dead-telemetry digit, which has to read as "off" and therefore inverts
	// direction between the two palettes. A fill that carries white text needs
	// to be dark in both, and C_RED is.
	bool holding = s_writeHold.holding;
	uint16_t writeFill = holding ? C_RED : C_PANEL;
	uiHoldDrawBar(&s_writeHold, BTN_WRITE, FOOT_Y - 4, millis());
	// Both are gated on the same `dirty` their hit tests are. WRITE already
	// was; REVERT was drawn grey and ran regardless, which was harmless only
	// because reverting nothing to nothing is a no-op. @see #23
	uiButton(BTN_WRITE, dirty ? "HOLD TO WRITE" : "NO CHANGES",
	         writeFill, holding ? C_ONACCENT : C_TEXT, 1, holding, dirty);
	uiButton(BTN_REVERT, "REVERT", C_PANEL, C_AMBER, 1,
	         uiPressing(BTN_REVERT, s_frameTouch), dirty);
	uiButton(BTN_HEX, s_hexView ? "FIELDS" : "HEX", C_PANEL, C_CYAN, 1,
	         uiPressing(BTN_HEX, s_frameTouch));
}

/**
 * @brief The write's state or result, centred at y @ref RESULT_Y, at scale 2
 *        when it fits the panel, else at scale 1. `WRITE VERIFIED. BACKUP
 *        AM001` is 334 px at scale 2 and ran off both edges. Every status of
 *        the write fits one scale-1 line: the asserts at @ref RESULT_CHARS.
 * @param s The text.
 */
static void drawResultText(const char *s) {
	gfxTextCenter(RESULT_Y, s, C_TEXT, gfxTextW(s, 2) <= GFX_W - 2 * UI_MARGIN ? 2 : 1);
}

static void drawWriting() {
	gfxRect(0, LIST_Y0, GFX_W, GFX_H - LIST_Y0, C_BG);
	drawResultText(s_status);
	// The ESC reads its settings at boot: whether it runs the new ones
	// without a power cycle is not known, and the line is right either way.
	if (s_verified) gfxTextCenter(RESULT_Y + 30, AM32_UI_POWER_CYCLE, C_TEXT, 1);
	// Not verified: the edits stay in the list, and the next hold of WRITE
	// runs the write, and the backup if it is missing, again.
	else if (s_nextStep) gfxTextCenter(RESULT_Y + 30, s_nextStep, UI_COL_WARN, 1);
	uiButton(BTN_OK, "OK", C_PANEL, C_TEXT, 1, uiPressing(BTN_OK, s_frameTouch));
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
	snprintf(s_status, sizeof(s_status), "%s",
	         s_claimed ? "RELEASING DSHOT PIN"
	                   : (r == ClaimRefusal::Armed ? "DISARM FIRST" : "PINS BUSY"));
}

void uiAm32Enter() {
	s_state = S_HANDOVER;
	s_hexView = false;
	s_selected = -1;
	s_leaving = false;
	s_redraw = true;
	uiHoldReset(&s_writeHold);
	s_visibleCount = 0;
	openList();
#if AM32_FORCE_LOW_JUMP
	s_jumped = false;
#endif
	s_attempt = 0;
	s_connectNote = CN_NONE;
	s_editRepeat = Repeat{};
	am32BlSetBaud(AM32_LINK_BAUD);
	memset(s_eeprom, 0, sizeof(s_eeprom));
	memset(s_original, 0, sizeof(s_original));
	s_backupDone = false;
	s_backupOnCard = false;
	s_backupName[0] = '\0';
	claimPins();
}

void uiAm32Exit() {
	// The transport lets go of the pin before DShot takes it back. An AM32
	// claim has no core1 job and no unplug page, so pinClaimEnd() resumes the
	// pump within the call.
	am32BlEnd();
	cardCacheEnd();
	pinClaimEnd(PinUser::Am32);
	s_state = S_HANDOVER;
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
	if (r != AM32_OK) {
		const uint8_t *rx;
		uint8_t n = am32BlLastRx(&rx);

		if (n) {
			// Bytes arrived: the link is alive and this is a framing or rate
			// problem, which is a different fix from silence entirely.
			snprintf(s_status, sizeof(s_status), "%luB: %02X %02X %02X @%lu",
			         (unsigned long)n, rx[0], n > 1 ? rx[1] : 0, n > 2 ? rx[2] : 0,
			         (unsigned long)am32BlBaud());
			s_connectNote = CN_GARBLED;
			return;
		}

		snprintf(s_status, sizeof(s_status), "WAITING... (TRY %d)", s_attempt);
		s_connectNote = CN_NONE;
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
	if (!am32LayoutKnown(s_eeprom)) {
		// A newer layout may have moved a field: the list does not open.
		snprintf(s_status, sizeof(s_status), "LAYOUT %u NOT KNOWN",
		         s_eeprom[AM32_OFF_LAYOUT_REVISION]);
		s_connectNote = CN_LAYOUT;
		return;
	}

	memcpy(s_original, s_eeprom, sizeof(s_eeprom));
	memcpy(s_entry, s_eeprom, sizeof(s_entry));
	buildVisible();
	openList();
	s_selected = -1;
	s_state = S_LIST;
	s_redraw = true;
	snprintf(s_status, sizeof(s_status), "READ OK");
	s_esc3d = am32Is3d(s_eeprom);
}

/** @brief Commit the settings block. */
static void doWrite() {
	// The settings as read at entry go to the card before the first write of
	// the session. Without a card the write runs and the backup stays in
	// memory, s_entry, until a write with a card saves it; a card that
	// refuses it stops the write.
	// A card swapped since the backup reached the card holds none: the card
	// in the slot is mounted again and the backup saved to it.
	uint32_t v = 0;
	if (s_backupOnCard && cardMounted() &&
	    (!s_backupVolumeRead || !cardVolumeSerial(&v) || v != s_backupVolume)) {
		cardCacheRecover();
		s_backupDone = s_backupOnCard = false;
	}
	if (!s_backupDone && cardMounted()) {
		BackupResult b = am32BackupSave(s_entry, am32BlEscTypeName(), am32BlEepromAddr(),
		                                s_backupName);
		// The ESC is written only once the card holds the backup. The AM32
		// transport blocks core0 anyway, so the flush runs on the spot.
		if (b == BackupResult::Ok && cardCacheFlush() == CardCache::Failed) b = BackupResult::WriteError;
		// A batch that failed during the save or its flush: the card is
		// mounted again, and the next WRITE saves the backup again.
		if (cardCacheState() == CardCache::Failed) cardCacheRecover();
		if (b != BackupResult::Ok) {
			snprintf(s_status, sizeof(s_status), ST_NO_BACKUP);
			s_nextStep = AM32_UI_CHECK_CARD;
			return;
		}
		s_backupDone = true;
		// A serial number that does not read leaves the check armed and
		// failing: the next WRITE mounts the card again and saves again.
		s_backupOnCard = true;
		s_backupVolumeRead = cardVolumeSerial(&s_backupVolume);
	}
	Am32Result r = am32BlWrite(am32BlEepromAddr(), s_eeprom, AM32_SETTINGS_SIZE);
	if (r != AM32_OK) {
		snprintf(s_status, sizeof(s_status), ST_WRITE_FAILED "%s", am32ResultText(r));
		s_nextStep = AM32_UI_RETRY;
		return;
	}
	// The ESC accepted the block. With 3D MODE on in it the tester's note is
	// set now, before the read-back: a read-back that fails or differs may
	// still leave the ESC in 3D mode. Only a verified write or a list read
	// clears it. @see uiAm32Esc3d()
	if (am32Is3d(s_eeprom)) s_esc3d = true;
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
	s_esc3d = am32Is3d(s_eeprom);
	if (s_backupDone) snprintf(s_status, sizeof(s_status), ST_VERIFIED ST_BACKUP "%.5s", s_backupName);
	else snprintf(s_status, sizeof(s_status), ST_VERIFIED);
}


static void handleListTouch(const TouchState *t) {
	// --- write button: needs every frame, including finger-off, to time the
	//     hold and to cancel it the moment the finger moves away. Only a press
	//     that began on the button counts (ui_hold.h), so a drag that strays
	//     in from the list cannot start a hold ---
	bool dirty = anyDirty();
	bool wasHolding = s_writeHold.holding;
	if (uiHoldUpdate(&s_writeHold, BTN_WRITE, t, dirty, millis())) {
		s_state = S_WRITING;
		s_verified = false;
		s_nextStep = nullptr;
		snprintf(s_status, sizeof(s_status), "WRITING...");
		s_redraw = true;
		return;
	}
	if (wasHolding || s_writeHold.holding) s_redraw = true;

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
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	if (s_state == S_WRITING) {
		// drawWriting() draws OK from the first frame; it acts once the
		// write has finished.
		uiControlAdd(out, &n, max, UI_FIX(Am32Ctl::Ok), BTN_OK,
		             strcmp(s_status, "WRITING...") != 0);
		return n;
	}
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
	// On release, like every navigation tap.
	if (uiBackTapped(t)) {
		s_leaving = true;
		uiAm32Exit();
		return false;
	}

	switch (s_state) {
		case S_HANDOVER:
			if (!s_claimed) claimPins();
			if (s_claimed && pinClaimPoll() == ClaimState::Held) {
				// The pin the claim latched from the pump, not the compiled
				// default: the two part company the moment the user changes it
				// on SETTINGS.
				am32BlBegin(pinClaimGpio(PIN_SIG));
				s_state = S_CONNECT;
				snprintf(s_status, sizeof(s_status), "SEARCHING...");
			}
			s_redraw = true;
			break;

		case S_CONNECT:
			// Retry as fast as the frame loop allows. A failed attempt costs
			// about 40 ms, and the power-up window is narrow, so retry rate is
			// what determines whether we catch it.
			tryConnect();
			s_redraw = true;
			break;

		case S_LIST:
			// The buttons first, then the list's gesture, which selects and
			// steps the row a gesture began on (handleListGesture()).
			handleListTouch(t);
			handleListGesture(t);
			break;

		case S_WRITING:
			if (strcmp(s_status, "WRITING...") == 0) {
				drawHeader();
				drawWriting();
				doWrite();
				s_redraw = true;
			} else if (uiTapped(BTN_OK, t)) {
				s_state = S_LIST;
				s_redraw = true;
			}
			break;
	}

	if (s_redraw) {
		s_redraw = false;
		drawHeader();
		switch (s_state) {
			case S_HANDOVER:
			case S_CONNECT:  drawConnect(); break;
			case S_LIST:     drawList();    break;
			case S_WRITING:  drawWriting(); break;
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

bool uiAm32Esc3d() { return s_esc3d; }

/** @brief AM32 CONFIG's MENU status: the signal pin it claims, from the settings. */
static void am32MenuStatus(char *buf, size_t n, uint16_t *col, uint16_t *) {
	snprintf(buf, n, "GP%u", (unsigned)settings()->dshotPin);
	*col = C_DIM;
}

const UiScreen UI_AM32 = {
	UiScreenId::Am32, "AM32 CONFIG", PinUser::Am32, am32MenuStatus,
	uiAm32Enter, uiAm32Tick, nullptr, uiAm32Controls, nullptr, false,
};
