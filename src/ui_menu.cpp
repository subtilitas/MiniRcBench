/**
 * @file ui_menu.cpp
 * @brief MENU and the BEEP action. @see ui_menu.h
 */

#include "ui_menu.h"
#include "ui_registry.h"
#include "ui_list.h"
#include "ui_input.h"
#include "ui_widgets.h"
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
 * @brief Six 40 px rows fit under the strip; a seventh scrolls 36 px.
 * @{
 */
#define MENU_VIEW_Y0   UI_BODY_Y              /**< First row of the list, y 62. */
#define MENU_VIEW_H    (GFX_H - MENU_VIEW_Y0) /**< 258 px. */
#define MENU_ROW_H     UI_LIST_TAP_MIN        /**< A row, 40 px. */
#define MENU_ROW_PITCH 42                     /**< Row top to row top. */
#define MENU_ROW_X     8                      /**< Left edge of a row. */
#define MENU_ROW_W     224                    /**< To x 231. */
#define MENU_LABEL_X   16                     /**< Label, scale 2. */
#define MENU_STATUS_R  222                    /**< Status, scale 1, ends here. */
#define MENU_BAR_X     235                    /**< The scroll bar. */
/** @} */

static_assert(MENU_VIEW_Y0 >= UI_STRIP_Y + UI_STRIP_H, "MENU's list overlaps the strip");
static_assert(6 * MENU_ROW_PITCH <= MENU_VIEW_H, "six MENU rows do not fit the viewport");
static_assert(MENU_ROW_H >= UI_LIST_TAP_MIN && MENU_ROW_PITCH > MENU_ROW_H,
              "a MENU row is lower than the list floor, or has no gap");
static_assert(MENU_ROW_X + MENU_ROW_W < MENU_BAR_X, "MENU's rows run into the scroll bar");

/** @brief How long BEEP's row shows SENT or REFUSED, ms. */
#define BEEP_FLASH_MS 350

static const UiMenuEntry *s_entries = UI_MENU;
static uint8_t            s_count = UI_MENU_COUNT;
static const UiMenuEntry *s_wantEntries = UI_MENU;
static uint8_t            s_wantCount = UI_MENU_COUNT;

static UiList             s_list;
static const TouchState  *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t           s_drawnKey = 0;

static bool     s_beepOk = false;
static uint32_t s_beepUntil = 0;

// ---------------------------------------------------------------------------
// BEEP
// ---------------------------------------------------------------------------

static bool beepFlashing() { return (int32_t)(millis() - s_beepUntil) < 0; }

static void beepAction() {
	s_beepOk = escRequestBeep(1);
	s_beepUntil = millis() + BEEP_FLASH_MS;
}

static void beepStatus(char *buf, size_t n, uint16_t *col, uint16_t *fill) {
	// The refusal is unreachable through MENU, which disarms on open; the
	// flash stays as the backstop. @see escRequestBeep()
	if (beepFlashing()) {
		snprintf(buf, n, "%s", s_beepOk ? "SENT" : "REFUSED");
		*col = C_PAPER;
		*fill = s_beepOk ? C_INK : C_AMBER;
		return;
	}
	snprintf(buf, n, "DISARMED");
	*col = C_DIM;
}

const UiScreen UI_BEEP = {
	UiScreenId::Beep, "BEEP THE ESC", PinUser::None, beepStatus,
	nullptr, nullptr, beepAction, nullptr, nullptr,
};

// ---------------------------------------------------------------------------
// the list
// ---------------------------------------------------------------------------

/** @brief The tool's claim is refused while the pins are not Free. */
static bool pinsBusy(const UiScreen *s) {
	return s->pinUser != PinUser::None && pinClaimState() != ClaimState::Free;
}

/** @brief A row's status, its colours and whether a tap opens nothing. */
static void rowStatus(int i, char *buf, size_t n, uint16_t *col, uint16_t *fill) {
	const UiScreen *s = s_entries[i].screen;
	buf[0] = '\0';
	*col = C_DIM;
	*fill = C_PANEL;
	if (pinsBusy(s)) {
		snprintf(buf, n, "PINS BUSY");
		*col = C_AMBER;
		return;
	}
	if (s->status) s->status(buf, n, col, fill);
}

/** @brief The row's rectangle on the panel with its top at @p y. */
static UiRect rowRect(int y) {
	return UiRect{ MENU_ROW_X, (int16_t)y, MENU_ROW_W, MENU_ROW_H };
}

/** @brief Whether a tap on row @p i acts: an action, or a tool whose pins are free. */
static bool rowActs(int i) {
	const UiScreen *s = s_entries[i].screen;
	return s->action || !pinsBusy(s);
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
	gfxText(MENU_STATUS_R - gfxTextW(st, 1), y + 16, st, col, 1);
}

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/** @brief Everything a frame draws, hashed: a change repaints. */
static uint32_t frameKey(bool edt) {
	uint32_t h = 2166136261u;
	h = mix(h, (uint32_t)s_list.scrollPx);
	h = mix(h, edt);
	h = mix(h, settings()->dshotPin);
	h = mix(h, settings()->dshotKbaud);
	for (int i = 0; i < s_count; i++) {
		char st[24];
		uint16_t col, fill;
		rowStatus(i, st, sizeof(st), &col, &fill);
		for (const char *c = st; *c; c++) h = mix(h, (uint8_t)*c);
		h = mix(h, ((uint32_t)col << 16) | fill);
		h = mix(h, rowPressed(i));
	}
	h = mix(h, uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

static void drawMenu(bool edt) {
	gfxFill(C_BG);
	uiHeader("MENU", s_t);
	uiStripClear();
	char cap[32];
	snprintf(cap, sizeof(cap), "GP%u  %u KBIT/S", (unsigned)settings()->dshotPin,
	         (unsigned)settings()->dshotKbaud);
	uiStripText(cap, C_DIM);
	// Read-only: the firmware enables EDT for each ESC as it appears, and
	// arriving frames are the only evidence that it is delivering.
	uiChip(edt ? "EDT ON" : "EDT OFF", edt ? C_GREEN : C_RED);
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
	if (uiBackTapped(s_t)) return UiMenuResult::Back;

	UiListEvent e = uiListTouch(&s_list, s_t);
	UiMenuResult r = UiMenuResult::Stay;
	if (e.kind == UiListEv::Tap && e.row >= 0 && e.row < s_count) {
		const UiScreen *s = s_entries[e.row].screen;
		if (s->action) {
			s->action();
		} else if (!pinsBusy(s) && s->enter) {
			*open = s;
			r = UiMenuResult::Open;
		}
	}
	if (r == UiMenuResult::Stay) {
		EscTelemetry tel;
		escSnapshot(&tel);
		EscReading rd;
		escMerge(&tel, millis(), KISS_STALE_MS, EDT_STALE_MS, &rd);
		uint32_t key = frameKey(rd.edtFresh);
		if (key != s_drawnKey) {
			s_drawnKey = key;
			drawMenu(rd.edtFresh);
		}
	}
	return r;
}

static uint8_t menuControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	uint8_t first = n;
	uiListAddRows(&s_list, out, &n, max, true);
	for (uint8_t i = first; i < n; i++)
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
	uiMenuEnter, nullptr, nullptr, menuControls, nullptr,
};
