/**
 * @file ui_hub.cpp
 * @brief The hub, the one chooser widget. @see ui_hub.h
 */

#include "ui_hub.h"
#include "ui_input.h"
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
// The busy strip with a one-digit GPIO: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_MENU_RESTARTING_FMT) - 1 - 2 + 1 <= UI_HUB_LINE_CHARS,
              "the hub's busy strip runs off the panel");
static_assert(UI_MARGIN + 6 * UI_HUB_LINE_CHARS - 1 <= GFX_W, "a hub line runs off the panel");
static_assert(UI_HUB_ENTRY_Y + (UI_HUB_PAGE_ENTRIES - 1) * UI_HUB_ENTRY_PITCH + UI_HUB_ENTRY_H <= GFX_H,
              "a hub's last button runs off the panel");

/** @brief Room for a strip or a line: @ref UI_HUB_LINE_CHARS characters and the terminator, with slack. */
#define HUB_TEXT_MAX 48

static const TouchState *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t s_drawnKey = 0;

UiRect uiHubEntryRect(int i) {
	return UiRect{ UI_EDGE, (int16_t)(UI_HUB_ENTRY_Y + i * UI_HUB_ENTRY_PITCH), UI_HUB_ENTRY_W,
	               UI_HUB_ENTRY_H };
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
static uint32_t frameKey(const UiHub &hub, const char *strip) {
	uint32_t h = 2166136261u;
	for (const char *p = strip; *p; p++) h = mix(h, (uint8_t)*p);
	h = mix(h, servoSupplyPin());
	h = mix(h, claimBusy());
	for (int i = 0; i < hub.count; i++) {
		bool acts = uiHubEntryActs(hub, i);
		h = mix(h, acts);
		h = mix(h, acts && uiPressing(uiHubEntryRect(i), s_t));
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

static void draw(const UiHub &hub, const char *strip) {
	gfxFill(C_BG);
	uiHeader(hub.title, s_t);
	uiStripClear();
	uiStripText(strip, claimBusy() ? UI_COL_WARN : C_DIM);
	for (int i = 0; i < hub.count; i++) {
		UiRect r = uiHubEntryRect(i);
		bool acts = uiHubEntryActs(hub, i);
		uiButton(r, hub.entries[i].label, C_PANEL, C_TEXT, 2, acts && uiPressing(r, s_t), acts);
		drawLines(hub.entries[i], r, entryOwnGate(hub, i));
	}
}

void uiHubEnter() { s_drawnKey = 0; }

void uiHubResume() { s_drawnKey = 0; }

bool uiHubTick(const UiHub &h, const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	if (uiBackTapped(s_t)) return false;
	for (int i = 0; i < h.count; i++) {
		// A disabled entry opens nothing; the tool's own entry point asks
		// for the claim on this frame, after this tick.
		if (uiHubEntryActs(h, i) && uiTapped(uiHubEntryRect(i), s_t)) {
			uiChooseTool(h.entries[i].screen, h.entries[i].enter);
			return true;
		}
	}
	char strip[HUB_TEXT_MAX];
	strip[0] = '\0';
	// While the claim is not Free, why the refused entries are drawn
	// disabled, in MENU's words: Restoring outlasts the leave page's last
	// button, which returns here.
	if (claimBusy()) snprintf(strip, sizeof(strip), UI_MENU_RESTARTING_FMT, (unsigned)settings()->dshotPin);
	else h.strip(strip, sizeof(strip));
	uint32_t key = frameKey(h, strip);
	if (key != s_drawnKey) {
		s_drawnKey = key;
		draw(h, strip);
	}
	return true;
}

uint8_t uiHubControls(const UiHub &h, UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	for (int i = 0; i < h.count; i++)
		uiControlAdd(out, &n, max, HUB_ENTRY(i), uiHubEntryRect(i), uiHubEntryActs(h, i));
	return n;
}
