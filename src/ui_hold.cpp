/**
 * @file ui_hold.cpp
 * @brief The hold-to-act control. @see ui_hold.h
 */

#include "ui_hold.h"
#include "gfx.h"
#include "touch.h"

void uiHoldReset(UiHold *h) {
	h->holding = false;
	h->latched = false;
	h->startMs = 0;
}

bool uiHoldUpdate(UiHold *h, const UiRect &r, const TouchState *t, bool enabled,
                  uint32_t nowMs) {
	// A new press, or no finger at all, ends the latch of the last firing.
	if (!t || !t->down || t->pressed) h->latched = false;
	if (!t || !enabled || h->latched || !uiPressing(r, t)) {
		h->holding = false;
		return false;
	}
	if (!h->holding) {
		h->holding = true;
		h->startMs = nowMs;
	}
	if ((uint32_t)(nowMs - h->startMs) < UI_HOLD_MS) return false;
	h->holding = false;
	h->latched = true;
	return true;
}

int uiHoldPercent(const UiHold *h, uint32_t nowMs) {
	if (!h->holding) return 0;
	uint32_t pct = (uint32_t)(nowMs - h->startMs) * 100u / UI_HOLD_MS;
	return pct > 100 ? 100 : (int)pct;
}

void uiHoldDrawBar(const UiHold *h, const UiRect &r, int y, uint32_t nowMs) {
	gfxRect(r.x, y, r.w, UI_HOLD_BAR_H, C_BG);
	if (h->holding)
		gfxRect(r.x, y, r.w * uiHoldPercent(h, nowMs) / 100, UI_HOLD_BAR_H, C_LIME);
}
