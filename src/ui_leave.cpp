/**
 * @file ui_leave.cpp
 * @brief The leave page. @see ui_leave.h
 */

#include "ui_leave.h"
#include "board_pins.h"
#include "gfx.h"
#include "touch.h"

#include <stdio.h>

void uiLeaveOpen(UiLeave *l, UiLeavePhase phase, uint32_t waitMs) {
	l->before = phase == UiLeavePhase::Before;
	l->phase = l->before ? UiLeavePhase::Before : UiLeavePhase::Releasing;
	l->leaveEnabled = true;
	l->leavePress = false;
	l->restartPress = false;
	l->releasedMs = 0;
	l->waitMs = waitMs;
}

void uiLeavePoll(UiLeave *l, bool due, uint32_t nowMs) {
	if (l->phase != UiLeavePhase::Releasing || !due) return;
	l->phase = UiLeavePhase::Released;
	l->releasedMs = nowMs;
}

int uiLeaveCurrent(const UiLeave *l) {
	return l->phase != UiLeavePhase::Before && l->before ? 1 : 0;
}

bool uiLeaveRestartLive(const UiLeave *l, uint32_t nowMs) {
	return l->phase == UiLeavePhase::Released && (uint32_t)(nowMs - l->releasedMs) >= l->waitMs;
}

UiLeaveEv uiLeaveTouch(UiLeave *l, const TouchState *t, uint32_t nowMs) {
	if (!t) return UiLeaveEv::None;
	// LEAVE and RESTART DSHOT count a press by when it landed. A press that
	// landed while LEAVE was disabled never leaves; one that landed while
	// RESTART DSHOT waited, or on LEAVE, never restarts DShot. uiTapped()
	// below holds the press to the button it landed on.
	if (t->pressed) {
		l->leavePress = l->phase == UiLeavePhase::Before && l->leaveEnabled;
		l->restartPress = uiLeaveRestartLive(l, nowMs);
	}
	if (l->phase == UiLeavePhase::Before) {
		if (uiTapped(UI_LEAVE_LEFT, t)) return UiLeaveEv::Stay;
		bool counts = l->leavePress && l->leaveEnabled;
		if (!counts || !uiTapped(UI_LEAVE_WIDE, t)) return UiLeaveEv::None;
		l->phase = UiLeavePhase::Releasing;
		return UiLeaveEv::Leave;
	}
	if (l->phase != UiLeavePhase::Released) return UiLeaveEv::None;
	if (uiTapped(UI_LEAVE_LEFT, t)) return UiLeaveEv::Next;
	if (l->restartPress && uiTapped(UI_LEAVE_WIDE, t)) return UiLeaveEv::Restart;
	return UiLeaveEv::None;
}

// UI_LEAVE_DSHOT_FMT fits with a one-digit GPIO: SIG is one of GP0 to GP9.
static_assert(BOARD_FREE_GPIO_MASK <= 0x3FFu, "SIG's GPIO has two digits: the DShot step runs past x 232");

void uiLeaveUnplugSteps(UiLeaveText *text, int first, unsigned sig, char *buf, size_t n) {
	snprintf(buf, n, UI_LEAVE_DSHOT_FMT, sig);
	text->step[first][0] = UI_LEAVE_UNPLUG;
	text->step[first][1] = buf;
	text->step[first][2] = UI_LEAVE_PULSE;
	text->step[first + 1][0] = UI_LEAVE_RESTART;
	text->step[first + 1][1] = nullptr;
	text->step[first + 1][2] = nullptr;
}

/** @brief Draw step @p i from @p y. @return The next step's top. */
static int drawStep(const UiLeaveText &text, int i, int y, bool current) {
	uint16_t col = current ? UI_COL_WARN : C_DIM;
	int top = y;
	for (int k = 0; k < UI_LEAVE_STEP_LINES && text.step[i][k]; k++) {
		if (y + 7 >= UI_LEAVE_NOTE_Y) break;
		if (k == 0) {
			char n[2] = { (char)('1' + i), '\0' };
			gfxText(UI_MARGIN, y, n, col, 1);
		}
		gfxText(UI_LEAVE_TEXT_X, y, text.step[i][k], col, 1);
		y += UI_LEAVE_LINE_H;
	}
	// The bar runs from the first line's top to the last drawn line's foot.
	if (current) gfxRect(0, top, UI_LEAVE_BAR_W, y - UI_LEAVE_LINE_H + 7 - top, UI_COL_WARN);
	return y + UI_LEAVE_STEP_GAP;
}

void uiLeaveDraw(const UiLeave *l, const UiLeaveText &text, const TouchState *t,
                 uint32_t nowMs) {
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	int cur = uiLeaveCurrent(l);
	int y = UI_LEAVE_STEP_Y;
	for (int i = 0; i < UI_LEAVE_STEPS && text.step[i][0]; i++)
		y = drawStep(text, i, y, i == cur);

	bool before = l->phase == UiLeavePhase::Before;
	uint16_t noteCol = before ? UI_COL_WARN : C_DIM;
	for (int k = 0; k < UI_LEAVE_NOTES && text.note[k]; k++)
		gfxText(UI_MARGIN, UI_LEAVE_NOTE_Y + k * UI_LEAVE_LINE_H, text.note[k], noteCol, 1);

	const char *left = before ? "STAY" : text.next;
	bool leftLive = before || l->phase == UiLeavePhase::Released;
	if (left)
		uiButton(UI_LEAVE_LEFT, left, C_PANEL, C_TEXT, 1,
		         leftLive && uiPressing(UI_LEAVE_LEFT, t), leftLive);
	bool wideLive = before ? l->leaveEnabled : uiLeaveRestartLive(l, nowMs);
	// The wide button looks pressed only under a press that counts: one that
	// landed while LEAVE was disabled, or during RESTART DSHOT's wait, does
	// nothing on its lift.
	bool counts = before ? l->leavePress : l->restartPress;
	uiButton(UI_LEAVE_WIDE, before ? "LEAVE" : "RESTART DSHOT", C_PANEL, C_TEXT, 2,
	         wideLive && counts && uiPressing(UI_LEAVE_WIDE, t), wideLive);
}
