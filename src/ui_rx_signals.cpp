/**
 * @file ui_rx_signals.cpp
 * @brief RX SIGNALS, the chooser. @see ui_rx_signals.h
 */

#include "ui_rx_signals.h"
#include "ui_servo_prog.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "esc_task.h"
#include "gfx.h"
#include "touch.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_RX_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "RX SIGNALS' title runs into BACK");
// One-digit GPIOs: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_RX_STRIP_FMT) - 1 - 4 + 2 <= UI_PROG_LINE_CHARS,
              "RX SIGNALS' strip runs off the panel");

static const TouchState *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t s_drawnKey = 0;

/** @brief Entry @p i's button: PROGRAMMER's rectangles. */
static UiRect entryRect(int i) {
	return UiRect{ UI_EDGE, (int16_t)(UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH), UI_PROG_ENTRY_W,
	               UI_PROG_ENTRY_H };
}

/** @brief Whether entry @p i acts: a tool behind it. Drawn, reported and tapped on this. */
static bool entryActs(int i) { return UI_RX_ENTRIES[i].screen != nullptr; }

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/** @brief Everything a frame draws, hashed: a change repaints. */
static uint32_t frameKey() {
	uint32_t h = 2166136261u;
	h = mix(h, escTaskDshotPin());
	h = mix(h, escTaskKissPin());
	for (int i = 0; i < UI_RX_ENTRIES_COUNT; i++) h = mix(h, entryActs(i) && uiPressing(entryRect(i), s_t));
	h = mix(h, uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

static void draw() {
	gfxFill(C_BG);
	uiHeader(UI_RX_TITLE, s_t);
	uiStripClear();
	char strip[40];
	snprintf(strip, sizeof(strip), UI_RX_STRIP_FMT, (unsigned)escTaskDshotPin(),
	         (unsigned)escTaskKissPin());
	uiStripText(strip, C_DIM);
	for (int i = 0; i < UI_RX_ENTRIES_COUNT; i++) {
		const UiRxEntry &e = UI_RX_ENTRIES[i];
		UiRect r = entryRect(i);
		bool acts = entryActs(i);
		// uiButton() draws a disabled button unpressed.
		uiButton(r, e.label, C_PANEL, C_TEXT, 2, uiPressing(r, s_t), acts);
		int y = r.y + UI_PROG_LINE_DY;
		for (const char *l : e.lines) {
			if (!l) break;
			gfxText(UI_MARGIN, y, l, C_DIM, 1);
			y += UI_PROG_LINE_H;
		}
	}
}

void uiRxSignalsEnter() { s_drawnKey = 0; }

bool uiRxSignalsTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	if (uiBackTapped(s_t)) return false;
	for (int i = 0; i < UI_RX_ENTRIES_COUNT; i++) {
		// An entry with no tool opens nothing; the tool's own entry point
		// asks for the claim on this frame, after this tick.
		if (entryActs(i) && uiTapped(entryRect(i), s_t)) {
			uiChooseTool(UI_RX_ENTRIES[i].screen, nullptr);
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

uint8_t uiRxSignalsControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	for (int i = 0; i < UI_RX_ENTRIES_COUNT; i++)
		uiControlAdd(out, &n, max, UI_FIX((uint8_t)RxCtl::Monitor + i), entryRect(i), entryActs(i));
	return n;
}

const UiScreen UI_RX_SIGNALS = {
	UiScreenId::RxSignals, UI_RX_TITLE, PinUser::None, nullptr,
	uiRxSignalsEnter, uiRxSignalsTick, nullptr, uiRxSignalsControls, nullptr, true,
};
