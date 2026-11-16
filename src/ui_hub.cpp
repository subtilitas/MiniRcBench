/**
 * @file ui_hub.cpp
 * @brief The hub, the one chooser widget. @see ui_hub.h
 */

#include "ui_hub.h"
#include "ui_input.h"
#include "ui_list.h"
#include "ui_widgets.h"
#include "ui_menu.h"
#include "pin_claim.h"
#include "settings.h"
#include "servo_supply.h"
#include "gfx.h"
#include "touch.h"

#include <stdio.h>

static_assert(UI_HUB_ENTRY_Y == UI_BODY_Y, "a hub's first entry is not at the body's top");
static_assert(UI_EDGE + UI_HUB_ENTRY_W == GFX_W - UI_EDGE, "a hub's entries are not the wide button");
static_assert(UI_HUB_ENTRY_H >= UI_TAP_MIN, "a hub entry is smaller than a fingertip");
// The busy captions with a one-digit GPIO: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_BUSY_RELEASING_FMT) - 1 - 2 + 1 <= UI_HUB_LINE_CHARS &&
              sizeof(UI_BUSY_PARKING_FMT) - 1 - 2 + 1 <= UI_HUB_LINE_CHARS,
              "the hub's busy caption runs off the panel");
static_assert(sizeof(UI_HUB_NO_ESC_CHECK) - 1 <= UI_HUB_LINE_CHARS,
              "the hubs' NO ESC CHECK line runs off the panel");
static_assert(UI_MARGIN + 6 * UI_HUB_LINE_CHARS - 1 <= GFX_W, "a hub line runs off the panel");
static_assert(UI_HUB_ENTRY_Y + (UI_HUB_PAGE_ENTRIES - 1) * UI_HUB_ENTRY_PITCH + UI_HUB_ENTRY_H <= GFX_H,
              "a hub's last button runs off the panel");

/** @brief The viewport: the 258 px under the strip. */
#define HUB_VIEW_H (GFX_H - UI_HUB_ENTRY_Y)

static_assert(uiHubContentPx(UI_HUB_PAGE_ENTRIES) <= HUB_VIEW_H &&
              uiHubContentPx(UI_HUB_PAGE_ENTRIES + 1) > HUB_VIEW_H,
              "UI_HUB_PAGE_ENTRIES is not the number of entries that fill the page");
static_assert(UI_HUB_ENTRY_H >= UI_LIST_TAP_MIN, "a hub entry is lower than the list's press floor");
static_assert(UI_EDGE + UI_HUB_ENTRY_W < UI_HUB_BAR_X && UI_HUB_BAR_X + UI_LIST_BAR_W <= GFX_W,
              "a hub's entries run into the scroll bar, or the bar off the panel");
static_assert(1 + UI_HUB_MAX_ENTRIES <= UI_CONTROLS_MAX, "a hub's controls do not fit the table");

/** @brief Room for a strip or a line: @ref UI_HUB_LINE_CHARS characters and the terminator, with slack. */
#define HUB_TEXT_MAX 48

static const TouchState *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t s_drawnKey = 0;
/**
 * @brief The open hub's entries as a list: one row an entry. One hub is
 *        open at a time, under at most one tool, so one list serves the
 *        three hubs; uiHubEnter() sets it up and uiHubResume() keeps it.
 */
static UiList s_list;

UiRect uiHubEntryRect(int i) {
	return UiRect{ UI_EDGE, (int16_t)(UI_HUB_ENTRY_Y + i * UI_HUB_ENTRY_PITCH - s_list.scrollPx),
	               UI_HUB_ENTRY_W, UI_HUB_ENTRY_H };
}

/** @brief Whether the open hub scrolls: it has more entries than fill the page. */
static bool scrolls() { return uiListMaxScroll(&s_list) > 0; }

/**
 * @brief Whether a finger is on entry @p i as a press of it. On a hub that
 *        fills the page, the button's own press. On a scrolling hub, the
 *        list's: the press began on the entry, shows
 *        @ref UI_LIST_TAP_MIN px of it or more, and has not become a drag.
 */
static bool entryPressing(int i) {
	if (!scrolls()) return uiPressing(uiHubEntryRect(i), s_t);
	return s_t->down && s_list.candidate == i && s_list.mode == UiListMode::None &&
	       uiPressing(uiListRowRect(&s_list, i), s_t);
}

/** @brief Whether entry @p i's own gate holds: the `why` lines are drawn while it fails. */
static bool entryOwnGate(const UiHub &h, int i) {
	const UiHubEntry &e = h.entries[i];
	return !e.enabled || e.enabled();
}

bool uiHubEntryActs(const UiHub &h, int i) {
	return entryOwnGate(h, i) && !uiPinsRefuse(h.entries[i].screen);
}

/**
 * @brief Whether the pin claim is not Free: the strip says why the entries
 *        whose tools claim, or need the pins free, are drawn disabled.
 */
static bool claimBusy() { return pinClaimState() != ClaimState::Free; }

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/**
 * @brief Everything a frame of @p hub draws, hashed: a change repaints. The
 *        strip's text carries the pins, and the supply's pin the power lines.
 */
static uint32_t frameKey(const UiHub &hub, const char *strip, const char *second) {
	uint32_t h = 2166136261u;
	for (const char *p = strip; *p; p++) h = mix(h, (uint8_t)*p);
	h = mix(h, 0x1Fu);
	for (const char *p = second; p && *p; p++) h = mix(h, (uint8_t)*p);
	h = mix(h, servoSupplyPin());
	h = mix(h, claimBusy());
	h = mix(h, (uint32_t)s_list.scrollPx);
	for (int i = 0; i < hub.count; i++) {
		bool acts = uiHubEntryActs(hub, i);
		h = mix(h, acts);
		h = mix(h, hub.entries[i].noteWhen && hub.entries[i].noteWhen());
		h = mix(h, acts && entryPressing(i));
	}
	h = mix(h, uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

/**
 * @brief The lines under entry @p e's button @p r, from @ref UI_HUB_LINE_DY
 *        at a @ref UI_HUB_LINE_H pitch: the plain lines, then the power
 *        line, or, while the entry's own gate fails, the lines that say why.
 *        An entry the claim refuses keeps its lines: the strip says why.
 */
static void drawLines(const UiHubEntry &e, const UiRect &r, bool own) {
	int y = r.y + UI_HUB_LINE_DY;
	for (const char *l : e.lines) {
		if (!l) break;
		gfxText(UI_MARGIN, y, l, C_DIM, 1);
		y += UI_HUB_LINE_H;
	}
	if (e.note && e.noteWhen && e.noteWhen()) {
		gfxText(UI_MARGIN, y, e.note, C_AMBER, 1);
		y += UI_HUB_LINE_H;
	}
	if (!own) {
		for (const char *w : e.why) {
			if (!w) continue;
			gfxText(UI_MARGIN, y, w, UI_COL_WARN, 1);
			y += UI_HUB_LINE_H;
		}
		return;
	}
	if (!e.powerSwitchedFmt) return;
	char p[HUB_TEXT_MAX];
	if (servoSupplyFitted()) snprintf(p, sizeof(p), e.powerSwitchedFmt, (unsigned)servoSupplyPin());
	else snprintf(p, sizeof(p), "%s", e.powerOwn ? e.powerOwn : "");
	if (p[0]) gfxText(UI_MARGIN, y, p, C_DIM, 1);
}

/** @brief Draw entry @p i of the hub @p ctx with its top at @p y: the button and its lines. */
static void drawEntry(int i, int y, void *ctx) {
	const UiHub &hub = *(const UiHub *)ctx;
	UiRect r = UiRect{ UI_EDGE, (int16_t)y, UI_HUB_ENTRY_W, UI_HUB_ENTRY_H };
	bool acts = uiHubEntryActs(hub, i);
	uiButton(r, hub.entries[i].label, C_PANEL, C_TEXT, 2, acts && entryPressing(i), acts);
	drawLines(hub.entries[i], r, entryOwnGate(hub, i));
}

/**
 * @brief The strip: the hub's own line, and under it the busy caption in
 *        @ref UI_COL_WARN while the claim is not Free, else the hub's
 *        warning in `C_AMBER` (@ref UiHub::warn), if it has one.
 */
static void draw(const UiHub &hub, const char *strip, const char *second) {
	gfxFill(C_BG);
	uiHeader(hub.title, s_t);
	uiStripClear();
	if (!second) uiStripText(strip, C_DIM);
	else uiStripTwoLines(strip, C_DIM, second, claimBusy() ? UI_COL_WARN : C_AMBER);
	// The list clips to the 258 px under the strip: an entry cut by the
	// panel's edge shows that the list goes on.
	uiListDraw(&s_list, drawEntry, (void *)&hub);
	uiListDrawBar(&s_list, UI_HUB_BAR_X);
}

void uiHubEnter(const UiHub &h) {
	uiListInit(&s_list, UiRect{ UI_EDGE, UI_HUB_ENTRY_Y, UI_HUB_ENTRY_W, HUB_VIEW_H }, UI_HUB_ENTRY_PITCH,
	           h.count);
	uiListSetBody(&s_list, UI_HUB_ENTRY_H, UI_LIST_TAP_MIN);
	// The last entry's third line ends 1 px past its 80 px.
	s_list.contentPx = uiHubContentPx(h.count);
	s_drawnKey = 0;
}

void uiHubResume() { s_drawnKey = 0; }

int uiHubScroll() { return s_list.scrollPx; }

int uiHubMaxScroll() { return uiListMaxScroll(&s_list); }

/**
 * @brief The entry this frame's touch tapped, or -1. A hub that fills the
 *        page takes a tap as a button does. A scrolling hub takes the
 *        list's gesture: a vertical drag scrolls and taps nothing, and an
 *        entry cut below @ref UI_LIST_TAP_MIN px takes no press.
 */
static int entryTapped(const UiHub &h) {
	if (!scrolls()) {
		for (int i = 0; i < h.count; i++)
			if (uiTapped(uiHubEntryRect(i), s_t)) return i;
		return -1;
	}
	UiListEvent e = uiListTouch(&s_list, s_t);
	return e.kind == UiListEv::Tap && e.row < h.count ? e.row : -1;
}

bool uiHubTick(const UiHub &h, const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	if (uiBackTapped(s_t)) return false;
	// A disabled entry opens nothing; the tool's own entry point asks for
	// the claim on this frame, after this tick.
	int i = entryTapped(h);
	if (i >= 0 && uiHubEntryActs(h, i)) {
		uiChooseTool(h.entries[i].screen, h.entries[i].enter);
		return true;
	}
	char strip[HUB_TEXT_MAX];
	strip[0] = '\0';
	h.strip(strip, sizeof(strip));
	// While the claim is not Free, why the refused entries are drawn
	// disabled, in MENU's words, in the warning's place: Parking outlasts
	// the leave page's last button, which returns here.
	char busy[HUB_TEXT_MAX];
	const char *second = h.warn;
	if (claimBusy()) {
		uiBusyCaption(busy, sizeof(busy));
		second = busy;
	}
	uint32_t key = frameKey(h, strip, second);
	if (key != s_drawnKey) {
		s_drawnKey = key;
		draw(h, strip, second);
	}
	return true;
}

uint8_t uiHubControls(const UiHub &h, UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	for (int i = 0; i < h.count; i++) {
		// The visible part, as the list takes its press.
		UiRect r = uiListRowRect(&s_list, i);
		if (r.h > 0)
			uiControlAdd(out, &n, max, HUB_ENTRY(i), r, uiHubEntryActs(h, i) && r.h >= s_list.pressMin);
	}
	return n;
}
