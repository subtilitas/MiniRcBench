/**
 * @file ui_menu.cpp
 * @brief MENU and the BEEP action. @see ui_menu.h
 */

#include "ui_menu.h"
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
 * @brief Six 40 px rows fit under the strip; a seventh scrolls 36 px.
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

/** @brief A flash is up: the deadline alone reads true again 2^31 ms on. */
static bool     s_beepShown = false;

static bool beepFlashing() {
	if (s_beepShown && (int32_t)(millis() - s_beepUntil) >= 0) s_beepShown = false;
	return s_beepShown;
}

static void beepAction() {
	s_beepOk = escRequestBeep(1);
	s_beepShown = true;
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
	nullptr, nullptr, beepAction, nullptr, nullptr, false,
};

// ---------------------------------------------------------------------------
// the list
// ---------------------------------------------------------------------------

/**
 * @brief Whether the claim is not Free: the strip says DShot restarts, and
 *        every row that claims or changes the wiring is drawn disabled.
 */
static bool claimBusy() { return pinClaimState() != ClaimState::Free; }

/**
 * @brief A tool that claims pins, or changes the wiring a claim latched, is
 *        refused while the claim is not Free: Restoring outlasts RESTART
 *        DSHOT's return to MENU.
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

/** @brief What the strip's chip says. */
enum class MenuEdt : uint8_t {
	NoEsc,  /**< No eRPM frame within ESC_LINK_STALE_MS: NO ESC, dim. */
	Off,    /**< eRPM arrives and EDT does not: EDT OFF, red. */
	On,     /**< EDT arrives: EDT ON, green. */
};

/** @brief Everything a frame draws, hashed: a change repaints. */
static uint32_t frameKey(MenuEdt edt, bool runCue) {
	uint32_t h = 2166136261u;
	h = mix(h, (uint32_t)s_list.scrollPx);
	h = mix(h, (uint32_t)edt);
	h = mix(h, runCue);
	h = mix(h, settings()->dshotPin);
	h = mix(h, settings()->kissPin);
	h = mix(h, settings()->dshotKbaud);
	h = mix(h, claimBusy());
	for (int i = 0; i < s_count; i++) {
		char st[24];
		uint16_t col, fill;
		rowStatus(i, st, sizeof(st), &col, &fill);
		for (const char *c = st; *c; c++) h = mix(h, (uint8_t)*c);
		h = mix(h, ((uint32_t)col << 16) | fill);
		h = mix(h, rowPressed(i));
		h = mix(h, rowActs(i));
	}
	h = mix(h, uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

/**
 * @brief The run cue's line under the caption, x 8 to 180, ends left of the
 *        NO ESC chip, which starts at x 185 (6 px a character at scale 1).
 *        The chip beside the cue reads NO ESC: the cue goes with the first
 *        eRPM frame after the AM32 handshake, and the handshake needs the ESC
 *        powered up again, which takes longer than the 500 ms an older frame
 *        keeps EDT ON or EDT OFF up.
 */
static_assert(UI_MARGIN + 6 * (int)(sizeof(AM32_UI_RUN_CUE) - 1) - 1 <
              GFX_W - UI_MARGIN - (6 * (int)(sizeof(UI_MENU_NO_ESC) - 1) - 1 + 12),
              "MENU's run cue runs under the NO ESC chip");

/**
 * @brief The caption at its widest on this board, whose signal pins are GP0
 *        and GP1, and the restarting caption, each end 4 px before the
 *        widest chip, EDT OFF, which starts at x 179.
 */
static_assert(UI_MARGIN + 6 * (int)(sizeof("SIG GP1  AUX GP0  DSHOT1200") - 1) - 1 + 4 <=
                  GFX_W - UI_MARGIN - (6 * (int)(sizeof(UI_MENU_EDT_OFF) - 1) - 1 + 12) &&
              UI_MARGIN + 6 * (int)(sizeof("DSHOT RESTARTING ON SIG GP1") - 1) - 1 + 4 <=
                  GFX_W - UI_MARGIN - (6 * (int)(sizeof(UI_MENU_EDT_OFF) - 1) - 1 + 12),
              "MENU's caption runs under the EDT chip");

static void drawMenu(MenuEdt edt, bool runCue) {
	gfxFill(C_BG);
	uiHeader("MENU", s_t);
	uiStripClear();
	char cap[40];
	uint16_t capCol = C_DIM;
	if (claimBusy()) {
		// Why the rows that claim or change the wiring are drawn disabled.
		snprintf(cap, sizeof(cap), UI_MENU_RESTARTING_FMT, (unsigned)settings()->dshotPin);
		capCol = UI_COL_WARN;
	} else {
		snprintf(cap, sizeof(cap), UI_MENU_CAPTION_FMT, (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin, (unsigned)settings()->dshotKbaud);
	}
	// After an AM32 session that reached the bootloader, a second line says
	// what starts the ESC again: leaving sends it no command. @see uiAm32RunCue()
	if (runCue) uiStripTwoLines(cap, capCol, AM32_UI_RUN_CUE, UI_COL_WARN);
	else uiStripText(cap, capCol);
	// Read-only: the firmware enables EDT for each ESC as it appears, and
	// arriving frames are the only evidence that it is delivering. Red is for
	// an ESC that answers without EDT; with no ESC on the pin there is nothing
	// wrong to flag, so the chip says that, dim.
	switch (edt) {
		case MenuEdt::On:    uiChip(UI_MENU_EDT_ON, C_GREEN);  break;
		case MenuEdt::Off:   uiChip(UI_MENU_EDT_OFF, C_RED);   break;
		case MenuEdt::NoEsc: uiChip(UI_MENU_NO_ESC, C_GRID);   break;
	}
	uiListDraw(&s_list, drawRow, nullptr);
	uiListDrawBar(&s_list, MENU_BAR_X);
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void uiMenuEnter() {
	// A flash cut short by leaving MENU does not come back.
	s_beepShown = false;
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
		} else if (!pinsBusy(s) && s->enter && s->tick) {
			*open = s;
			r = UiMenuResult::Open;
		}
	}
	if (r == UiMenuResult::Stay) {
		EscTelemetry tel;
		escSnapshot(&tel);
		EscReading rd;
		escMerge(&tel, millis(), KISS_STALE_MS, EDT_STALE_MS, &rd);
		MenuEdt edt = rd.edtFresh ? MenuEdt::On
		            : escFieldFresh(tel.lastRpmMs, millis(), ESC_LINK_STALE_MS) ? MenuEdt::Off
		                                                                        : MenuEdt::NoEsc;
		bool runCue = uiAm32RunCue(tel.lastRpmMs);
		uint32_t key = frameKey(edt, runCue);
		if (key != s_drawnKey) {
			s_drawnKey = key;
			drawMenu(edt, runCue);
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
	uiMenuEnter, nullptr, nullptr, menuControls, nullptr, false,
};
