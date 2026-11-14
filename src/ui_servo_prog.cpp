/**
 * @file ui_servo_prog.cpp
 * @brief PROGRAMMER, the chooser. @see ui_servo_prog.h
 */

#include "ui_servo_prog.h"
#include "ui_registry.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "esc_task.h"
#include "servo_supply.h"
#include "gfx.h"
#include "touch.h"

#include <stdio.h>
#include <string.h>

static_assert(UI_PROG_ENTRY_Y == UI_BODY_Y, "PROGRAMMER's first entry is not at the body's top");
static_assert(UI_EDGE + UI_PROG_ENTRY_W == GFX_W - UI_EDGE, "PROGRAMMER's entries are not the wide button");
static_assert(UI_PROG_ENTRY_H >= UI_TAP_MIN, "a PROGRAMMER entry is smaller than a fingertip");
static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_PROG_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "PROGRAMMER's title runs into BACK");
// The strip at its widest, with a one-digit SIG and a two-digit supply pin.
static_assert(sizeof(UI_PROG_STRIP_FMT) - 1 - 4 + 1 + 4 <= UI_PROG_LINE_CHARS,
              "PROGRAMMER's strip runs off the panel");

static const TouchState *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t s_drawnKey = 0;

/** @brief Entry @p i's button. */
static UiRect entryRect(int i) {
	return UiRect{ UI_EDGE, (int16_t)(UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH), UI_PROG_ENTRY_W,
	               UI_PROG_ENTRY_H };
}

/** @brief Whether entry @p i acts: drawn live, reported enabled and tapped on the same value. */
static bool entryActs(int i) {
	const UiProgEntry &e = UI_PROGRAMMERS[i];
	return !e.enabled || e.enabled();
}

/** @brief SIG's GPIO: the claim is Free while the chooser shows, so DShot's pin. */
static unsigned sigGpio() { return escTaskDshotPin(); }

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/** @brief Everything a frame draws, hashed: a change repaints. */
static uint32_t frameKey() {
	uint32_t h = 2166136261u;
	h = mix(h, sigGpio());
	h = mix(h, servoSupplyPin());
	for (int i = 0; i < UI_PROGRAMMERS_COUNT; i++) {
		h = mix(h, entryActs(i));
		h = mix(h, uiPressing(entryRect(i), s_t));
	}
	h = mix(h, uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

/**
 * @brief Entry @p i's lines under its button, from @ref UI_PROG_LINE_DY at
 *        a @ref UI_PROG_LINE_H pitch: the models, then the power line, or,
 *        while the entry is disabled, the lines that say why.
 */
static void drawLines(int i, const UiRect &r) {
	const UiProgEntry &e = UI_PROGRAMMERS[i];
	int y = r.y + UI_PROG_LINE_DY;
	if (e.models) {
		gfxText(UI_MARGIN, y, e.models, C_DIM, 1);
		y += UI_PROG_LINE_H;
	}
	if (!entryActs(i)) {
		for (const char *w : e.why) {
			if (!w) continue;
			gfxText(UI_MARGIN, y, w, UI_COL_WARN, 1);
			y += UI_PROG_LINE_H;
		}
		return;
	}
	char p[48];
	if (servoSupplyFitted()) snprintf(p, sizeof(p), e.powerSwitchedFmt, (unsigned)servoSupplyPin());
	else snprintf(p, sizeof(p), "%s", e.powerOwn ? e.powerOwn : "");
	if (p[0]) gfxText(UI_MARGIN, y, p, C_DIM, 1);
}

static void draw() {
	gfxFill(C_BG);
	uiHeader(UI_PROG_TITLE, s_t);
	uiStripClear();
	char strip[40], supply[8];
	if (servoSupplyFitted()) snprintf(supply, sizeof(supply), "GP%u", (unsigned)servoSupplyPin());
	else snprintf(supply, sizeof(supply), "OWN");
	snprintf(strip, sizeof(strip), UI_PROG_STRIP_FMT, sigGpio(), supply);
	uiStripText(strip, C_DIM);
	for (int i = 0; i < UI_PROGRAMMERS_COUNT; i++) {
		UiRect r = entryRect(i);
		bool acts = entryActs(i);
		uiButton(r, UI_PROGRAMMERS[i].label, C_PANEL, C_TEXT, 2, acts && uiPressing(r, s_t), acts);
		drawLines(i, r);
	}
}

void uiServoProgEnter() { s_drawnKey = 0; }

bool uiServoProgTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	if (uiBackTapped(s_t)) return false;
	for (int i = 0; i < UI_PROGRAMMERS_COUNT; i++) {
		// A disabled entry opens nothing; the tool's own entry point claims
		// SIG on this frame, after this tick.
		if (entryActs(i) && uiTapped(entryRect(i), s_t)) {
			uiChooseTool(UI_PROGRAMMERS[i].screen, UI_PROGRAMMERS[i].enter);
			return true;
		}
	}
	uint32_t key = frameKey();
	if (key != s_drawnKey) {
		s_drawnKey = key;
		draw();
	}
	return true;
}

uint8_t uiServoProgControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	for (int i = 0; i < UI_PROGRAMMERS_COUNT; i++)
		uiControlAdd(out, &n, max, PROG_ENTRY(i), entryRect(i), entryActs(i));
	return n;
}

const UiScreen UI_SERVO_PROG = {
	UiScreenId::ServoProg, UI_PROG_TITLE, PinUser::None, nullptr,
	uiServoProgEnter, uiServoProgTick, nullptr, uiServoProgControls, nullptr, true,
};
