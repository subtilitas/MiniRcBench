/**
 * @file ui_esc_config.cpp
 * @brief ESC CONFIG, the chooser. @see ui_esc_config.h
 */

#include "ui_esc_config.h"
#include "ui_servo_prog.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "esc_task.h"
#include "gfx.h"
#include "touch.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_ESC_CFG_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "ESC CONFIG's title runs into BACK");
// A one-digit GPIO: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_ESC_CFG_STRIP_FMT) - 1 - 2 + 1 <= UI_PROG_LINE_CHARS,
              "ESC CONFIG's strip runs off the panel");

static const TouchState *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t s_drawnKey = 0;

/** @brief Entry @p i's button: PROGRAMMER's rectangles. */
static UiRect entryRect(int i) {
	return UiRect{ UI_EDGE, (int16_t)(UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH), UI_PROG_ENTRY_W,
	               UI_PROG_ENTRY_H };
}

/** @brief Whether entry @p i acts: a screen behind it. Drawn, reported and tapped on this. */
static bool entryActs(int i) { return UI_ESC_CFG_ENTRIES[i].screen != nullptr; }

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/** @brief Everything a frame draws, hashed: a change repaints. */
static uint32_t frameKey() {
	uint32_t h = 2166136261u;
	h = mix(h, escTaskDshotPin());
	for (int i = 0; i < UI_ESC_CFG_ENTRIES_COUNT; i++)
		h = mix(h, entryActs(i) && uiPressing(entryRect(i), s_t));
	h = mix(h, uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

static void draw() {
	gfxFill(C_BG);
	uiHeader(UI_ESC_CFG_TITLE, s_t);
	uiStripClear();
	char strip[16];
	snprintf(strip, sizeof(strip), UI_ESC_CFG_STRIP_FMT, (unsigned)escTaskDshotPin());
	uiStripText(strip, C_DIM);
	for (int i = 0; i < UI_ESC_CFG_ENTRIES_COUNT; i++) {
		const UiEscCfgEntry &e = UI_ESC_CFG_ENTRIES[i];
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

void uiEscConfigEnter() { s_drawnKey = 0; }

bool uiEscConfigTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	if (uiBackTapped(s_t)) return false;
	for (int i = 0; i < UI_ESC_CFG_ENTRIES_COUNT; i++) {
		// USB BRIDGE opens nothing; ESC SETTINGS' own entry point asks for
		// the claim on this frame, after this tick.
		if (entryActs(i) && uiTapped(entryRect(i), s_t)) {
			uiChooseTool(UI_ESC_CFG_ENTRIES[i].screen, nullptr);
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

uint8_t uiEscConfigControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	for (int i = 0; i < UI_ESC_CFG_ENTRIES_COUNT; i++)
		uiControlAdd(out, &n, max, UI_FIX((uint8_t)EscCfgCtl::Settings + i), entryRect(i), entryActs(i));
	return n;
}

const UiScreen UI_ESC_CONFIG = {
	UiScreenId::EscConfig, UI_ESC_CFG_TITLE, PinUser::None, nullptr,
	uiEscConfigEnter, uiEscConfigTick, nullptr, uiEscConfigControls, nullptr, true,
};
