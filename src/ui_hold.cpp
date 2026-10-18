/**
 * @file ui_hold.cpp
 * @brief The hold-to-act control. @see ui_hold.h
 */

#include "ui_hold.h"
#include "gfx.h"
#include "touch.h"

void uiHoldReset(UiHold *h) {
	h->holding = false;
	h->live = false;
	h->startMs = 0;
	h->downX = h->downY = -1;
}

bool uiHoldUpdate(UiHold *h, const UiRect &r, const TouchState *t, bool enabled,
                  uint32_t nowMs) {
	if (!t || !t->down) {
		h->holding = false;
		h->live = false;
		return false;
	}
	// A press qualifies on the frame it lands, or never, and only while the
	// control stays live: a press that lands while the control is drawn
	// disabled does not start the timer when the control goes live later.
	// The press is known by where it landed, so a later press whose first
	// frame the hold did not see is not taken for it. uiPressing() below
	// holds the press to the control it landed on.
	if (t->pressed) {
		h->live = true;
		h->downX = t->downX;
		h->downY = t->downY;
	} else if (t->downX != h->downX || t->downY != h->downY) {
		h->live = false;
	}
	if (!enabled) h->live = false;
	if (!h->live || !uiPressing(r, t)) {
		h->holding = false;
		return false;
	}
	if (!h->holding) {
		h->holding = true;
		h->startMs = nowMs;
	}
	if ((uint32_t)(nowMs - h->startMs) < UI_HOLD_MS) return false;
	h->holding = false;
	h->live = false;
	return true;
}

int uiHoldPercent(const UiHold *h, uint32_t nowMs) {
	if (!h->holding) return 0;
	uint32_t pct = (uint32_t)(nowMs - h->startMs) * 100u / UI_HOLD_MS;
	return pct > 100 ? 100 : (int)pct;
}

void uiHoldDrawBar(const UiHold *h, const UiRect &r, int y, uint32_t nowMs) {
	gfxRect(r.x, y, r.w, UI_HOLD_BAR_H, C_BG);
	// uiHoldPercent() is 0 while the hold does not run: no fill.
	gfxRect(r.x, y, r.w * uiHoldPercent(h, nowMs) / 100, UI_HOLD_BAR_H, C_LIME);
}
