/**
 * @file ui_menu.cpp
 * @brief MENU. @see ui_menu.h
 */

#include "ui_menu.h"
#include "ui.h"
#include "ui_registry.h"
#include "ui_list.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "ui_am32.h"
#include "esc_task.h"
#include "esc_merge.h"
#include "settings.h"
#include "pin_claim.h"
#include "config.h"
#include "gfx.h"
#include "touch.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

/**
 * @defgroup menu_layout MENU layout
 * @brief Six 40 px rows fit under the strip: 252 px of the 258 px, no scroll.
 * @{
 */
#define MENU_VIEW_Y0   UI_BODY_Y              /**< First row of the list, y 62. */
#define MENU_VIEW_H    (GFX_H - MENU_VIEW_Y0) /**< 258 px. */
#define MENU_ROW_H     UI_LIST_TAP_MIN        /**< A row, 40 px. */
#define MENU_ROW_PITCH 42                     /**< Row top to row top. */
#define MENU_ROW_X     8                      /**< Left edge of a row. */
#define MENU_ROW_W     224                    /**< To x 231. */
#define MENU_LABEL_X   UI_MENU_LABEL_X        /**< Label, scale 2. */
#define MENU_STATUS_R  UI_MENU_STATUS_R       /**< Status, scale 1, ends here. */
#define MENU_BAR_X     235                    /**< The scroll bar. */
/** @} */

static_assert(MENU_VIEW_Y0 >= UI_STRIP_Y + UI_STRIP_H, "MENU's list overlaps the strip");
static_assert(6 * MENU_ROW_PITCH <= MENU_VIEW_H, "six MENU rows do not fit the viewport");
static_assert(UI_MENU_MARK_X + 2 * 5 <= MENU_ROW_X + MENU_ROW_W, "the hub mark runs off its row");
static_assert(MENU_ROW_H >= UI_LIST_TAP_MIN && MENU_ROW_PITCH > MENU_ROW_H,
              "a MENU row is lower than the list floor, or has no gap");
static_assert(MENU_ROW_X + MENU_ROW_W < MENU_BAR_X, "MENU's rows run into the scroll bar");

static const UiMenuEntry *s_entries = UI_MENU;
static uint8_t            s_count = UI_MENU_COUNT;
static const UiMenuEntry *s_wantEntries = UI_MENU;
static uint8_t            s_wantCount = UI_MENU_COUNT;

static UiList             s_list;
static const TouchState  *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t           s_drawnKey = 0;

// ---------------------------------------------------------------------------
// the list
// ---------------------------------------------------------------------------

/**
 * @brief Whether the claim is not Free: the strip says what the pins are
 *        doing (uiBusyCaption()), and every row that claims or changes the
 *        wiring is drawn disabled.
 */
static bool claimBusy() { return pinClaimState() != ClaimState::Free; }

/**
 * @brief A tool that claims pins, or changes the wiring a claim latched, is
 *        refused while the claim is not Free: Parking outlasts the leave
 *        page's return to the hub that opened the tool.
 */
static bool pinsBusy(const UiScreen *s) { return uiPinsRefuse(s); }

/**
 * @brief A row's status and its colours: the screen's own, whether or not
 *        the row is refused (rowActs()).
 */
static void rowStatus(int i, char *buf, size_t n, uint16_t *col, uint16_t *fill) {
	const UiScreen *s = s_entries[i].screen;
	buf[0] = '\0';
	*col = C_DIM;
	*fill = C_PANEL;
	if (s->status) s->status(buf, n, col, fill);
}

/** @brief The row's rectangle on the panel with its top at @p y. */
static UiRect rowRect(int y) {
	return UiRect{ MENU_ROW_X, (int16_t)y, MENU_ROW_W, MENU_ROW_H };
}

/**
 * @brief Whether a tap on row @p i acts: an action, or a tool whose pins are
 *        free. A row with neither an action nor a screen never acts.
 */
static bool rowActs(int i) {
	const UiScreen *s = s_entries[i].screen;
	if (s->action) return true;
	return s->enter && s->tick && !pinsBusy(s);
}

/** @brief Whether the press on row @p i still counts as a tap of it. */
static bool rowPressed(int i) {
	if (!s_t || !s_t->down || s_list.candidate != i || s_list.mode != UiListMode::None)
		return false;
	if (!rowActs(i)) return false;
	return uiPressing(uiListRowRect(&s_list, i), s_t);
}

static void drawRow(int i, int y, void *) {
	char st[24];
	uint16_t col, fill;
	rowStatus(i, st, sizeof(st), &col, &fill);
	bool lit = fill != C_PANEL;
	bool acts = rowActs(i);
	// A refused tool is drawn disabled, an outline, and gated on the same
	// value in uiMenuTick() and the control table. @see ui_widgets.h
	uiButton(rowRect(y), "", fill, C_TEXT, 2, rowPressed(i), acts);
	gfxText(MENU_LABEL_X, y + 13, s_entries[i].label,
	        lit ? C_PAPER : (acts ? C_TEXT : C_DIM), 2);
	if (col == C_RED && !lit && st[0]) {
		// A red status, RECORDING: a red badge with dark ink, 5.45, where red
		// text on the row's C_PANEL read 4.03. Square, as every chip.
		gfxRect(MENU_STATUS_R - gfxTextW(st, 1) - 3, y + 14, gfxTextW(st, 1) + 6, 11, C_RED);
		gfxText(MENU_STATUS_R - gfxTextW(st, 1), y + 16, st, C_ONACCENT, 1);
	} else {
		gfxText(MENU_STATUS_R - gfxTextW(st, 1), y + 16, st, col, 1);
	}
	// A row that opens a hub: one level down (menu-c.md decision 13).
	if (uiHubOf(s_entries[i].screen)) gfxText(UI_MENU_MARK_X, y + 13, UI_MENU_MARK, acts ? C_CYAN : C_GRID, 2);
}

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/** @brief Everything a frame draws, hashed: a change repaints. */
static uint32_t frameKey(bool runCue) {
	uint32_t h = 2166136261u;
	h = mix(h, (uint32_t)s_list.scrollPx);
	h = mix(h, runCue);
	h = mix(h, settings()->dshotPin);
	h = mix(h, settings()->kissPin);
	h = mix(h, settings()->dshotKbaud);
	h = mix(h, claimBusy());
	h = mix(h, (uint32_t)pinClaimState());
	for (int i = 0; i < s_count; i++) {
		char st[24];
		uint16_t col, fill;
		rowStatus(i, st, sizeof(st), &col, &fill);
		for (const char *c = st; *c; c++) h = mix(h, (uint8_t)*c);
		h = mix(h, ((uint32_t)col << 16) | fill);
		h = mix(h, rowPressed(i));
		h = mix(h, rowActs(i));
	}
	return h | 1u;
}

/**
 * @brief The run cue, the caption at its widest on this board, whose signal
 *        pins are GP0 and GP1, and the busy captions each fit the strip's
 *        line, x 8 to 228: 37 characters. MENU draws no chip
 *        (docs/design/menu-c.md, decision 12): with DShot parked on MENU an
 *        EDT chip would read nothing.
 */
static_assert(sizeof(AM32_UI_RUN_CUE) - 1 <= 37 &&
              sizeof("SIG GP1  AUX GP0  DSHOT1200") - 1 <= 37 &&
              sizeof(UI_BUSY_RELEASING_FMT) - 1 - 2 + 1 <= 37 &&
              sizeof(UI_BUSY_PARKING_FMT) - 1 - 2 + 1 <= 37,
              "MENU's strip runs off the panel");

void uiBusyCaption(char *buf, size_t n) {
	unsigned sig = (unsigned)settings()->dshotPin;
	if (pinClaimState() == ClaimState::Parking) snprintf(buf, n, UI_BUSY_PARKING_FMT, sig);
	else snprintf(buf, n, UI_BUSY_RELEASING_FMT, sig);
}

static void drawMenu(bool runCue) {
	gfxFill(C_BG);
	// The root: no BACK (docs/design/menu-c.md decision 10).
	uiHeaderRoot("MENU");
	uiStripClear();
	char cap[40];
	uint16_t capCol = C_DIM;
	if (claimBusy()) {
		// Why the rows that claim or change the wiring are drawn disabled.
		uiBusyCaption(cap, sizeof(cap));
		capCol = UI_COL_WARN;
	} else {
		snprintf(cap, sizeof(cap), UI_MENU_CAPTION_FMT, (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin, (unsigned)settings()->dshotKbaud);
	}
	// After a session that reached a bootloader, a second line says what
	// starts the ESC again: leaving sends it no command. @see uiAm32RunCue(),
	// usbBridgeRunCue(), sbusOutRunCue()
	if (runCue) uiStripTwoLines(cap, capCol, AM32_UI_RUN_CUE, UI_COL_WARN);
	else uiStripText(cap, capCol);
	uiListDraw(&s_list, drawRow, nullptr);
	uiListDrawBar(&s_list, MENU_BAR_X);
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void uiMenuEnter() {
	s_entries = s_wantEntries;
	s_count = s_wantCount;
	uiListInit(&s_list, UiRect{ MENU_ROW_X, MENU_VIEW_Y0, MENU_ROW_W, MENU_VIEW_H },
	           MENU_ROW_PITCH, s_count);
	uiListSetBody(&s_list, MENU_ROW_H, UI_LIST_TAP_MIN);
	s_drawnKey = 0;
}

UiMenuResult uiMenuTick(const TouchState *t, const UiScreen **open) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	*open = nullptr;

	UiListEvent e = uiListTouch(&s_list, s_t);
	UiMenuResult r = UiMenuResult::Stay;
	if (e.kind == UiListEv::Tap && e.row >= 0 && e.row < s_count) {
		const UiScreen *s = s_entries[e.row].screen;
		if (s->action) {
			s->action();
		} else if (rowActs(e.row)) {
			*open = s;
			r = UiMenuResult::Open;
		}
	}
	if (r == UiMenuResult::Stay) {
		EscTelemetry tel;
		escSnapshot(&tel);
		// ESC SETTINGS' cue, the USB bridge's (usb_bridge.h) or the S.BUS
		// output's probe's (sbus_out_job.h): each is a bootloader that
		// may wait and was not told to run. uiRunCue() ORs them, as the
		// start step of ESC TESTER and ESC COMMANDS reads them.
		bool runCue = uiRunCue(tel.lastRpmMs);
		uint32_t key = frameKey(runCue);
		if (key != s_drawnKey) {
			s_drawnKey = key;
			drawMenu(runCue);
		}
	}
	return r;
}

static uint8_t menuControls(UiControl *out, uint8_t max) {
	// The root has no BACK: the table holds the rows only.
	uint8_t n = 0;
	uiListAddRows(&s_list, out, &n, max, true);
	for (uint8_t i = 0; i < n; i++)
		if (!rowActs(out[i].id.row)) out[i].enabled = false;
	return n;
}

int uiMenuRowOf(const UiScreen *s) {
	for (int i = 0; i < s_count; i++)
		if (s_entries[i].screen == s) return i;
	return -1;
}

const UiMenuEntry *uiMenuEntries(uint8_t *n) {
	*n = s_count;
	return s_entries;
}

void uiMenuUseRegistry(const UiMenuEntry *e, uint8_t n) {
	s_wantEntries = e ? e : UI_MENU;
	s_wantCount = e ? n : UI_MENU_COUNT;
}

const UiScreen UI_MENU_SCREEN = {
	UiScreenId::Menu, "MENU", PinUser::None, nullptr,
	uiMenuEnter, nullptr, nullptr, menuControls, nullptr, false,
};
