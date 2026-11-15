/**
 * @file ui_sbus.cpp
 * @brief S.BUS MONITOR. @see ui_sbus.h
 */

#include "ui_sbus.h"
#include "sbus_view.h"
#include "sbus_job.h"
#include "ui_screen.h"
#include "ui_input.h"
#include "ui_hold.h"
#include "ui_leave.h"
#include "ui_widgets.h"
#include "ui.h"
#include "esc_task.h"
#include "esc_merge.h"
#include "pin_claim.h"
#include "config.h"
#include "gfx.h"
#include "touch.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// layout checks
// ---------------------------------------------------------------------------

/** @brief gfxTextW()'s rule at scale 1: 6 px a character less one. */
#define SB_W1(chars) (6 * (chars) - 1)
/** @brief A string literal's width at scale 1. */
#define SB_TEXT_W(s) SB_W1((int)sizeof(s) - 1)
/** @brief Where the widest chip, FRAME LOST, starts: x 161. */
#define SB_CHIP_X (GFX_W - UI_MARGIN - (SB_TEXT_W(SBUS_VIEW_FRAME_LOST) + 12))

static_assert(UI_MARGIN + SB_W1((int)sizeof(SBUS_UI_TITLE) - 1) * 2 + 4 <= UI_BACK_X,
              "S.BUS MONITOR's title runs into BACK");
// The caption with a three-letter role in place of %s.
static_assert(UI_MARGIN + SB_W1((int)sizeof(SBUS_UI_CAPTION_FMT) - 1 - 2 + 3) + 4 <= SB_CHIP_X,
              "S.BUS MONITOR's caption runs under the widest chip");
static_assert(SBUS_UI_ROW_Y == UI_BODY_Y, "the first channel row is not at the body's top");
static_assert(SBUS_UI_ROW_PITCH > SBUS_UI_ROW_H, "the channel rows touch");
static_assert(SBUS_UI_BAR_H + 1 < SBUS_UI_ROW_H + 1 && 2 + 7 <= SBUS_UI_ROW_H,
              "a bar or a value does not fit its row");
static_assert(UI_MARGIN + SB_W1(2) == SBUS_UI_NUM_END && SBUS_UI_NUM_END + 4 <= SBUS_UI_BAR_X,
              "a channel number runs into its bar");
// The widest value, 2144 us, 4 characters, right-aligned.
static_assert(SBUS_UI_BAR_X + SBUS_UI_BAR_W + 4 <= SBUS_UI_VALUE_END - SB_W1(4),
              "a bar runs into its value");
static_assert(SBUS_UI_VALUE_END == GFX_W - UI_MARGIN, "the values do not end at the right margin");
static_assert(SBUS_UI_ROW_Y + 15 * SBUS_UI_ROW_PITCH + SBUS_UI_ROW_H + 4 <= SBUS_UI_LINE1_Y,
              "the last channel row runs into line 1");
static_assert(SBUS_UI_LINE2_Y - SBUS_UI_LINE1_Y >= 12 && SBUS_UI_LINE3_Y - SBUS_UI_LINE2_Y >= 12 &&
              SBUS_UI_LINE4_Y - SBUS_UI_LINE3_Y >= 12 && SBUS_UI_LINE4_Y + 8 <= GFX_H,
              "the count lines overlap or leave the panel");
// Each line at its widest: line 1's halves, 18 characters each; line 2's,
// 13 and 18; line 3, 35; line 4, 23.
static_assert(UI_MARGIN + SB_W1(18) + 4 <= SBUS_UI_VALUE_END - SB_W1(18) &&
              UI_MARGIN + SB_W1(13) + 4 <= SBUS_UI_VALUE_END - SB_W1(18) &&
              UI_MARGIN + SB_W1(35) <= SBUS_UI_VALUE_END && UI_MARGIN + SB_W1(23) <= SBUS_UI_VALUE_END,
              "a count line runs past the margin, or its halves meet");
static_assert(UI_MARGIN + SB_TEXT_W(SBUS_UI_UNPLUG_2) <= GFX_W - UI_MARGIN &&
              UI_MARGIN + SB_TEXT_W(SBUS_UI_KISS_1) <= GFX_W - UI_MARGIN &&
              UI_MARGIN + SB_TEXT_W(SBUS_UI_NO_GP1_2) <= GFX_W - UI_MARGIN,
              "a message line runs past the margin");
static_assert(sizeof(SBUS_UI_UNPLUG) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(SBUS_UI_RESTART_FMT) - 1 - 2 + 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(SBUS_UI_RESTART_GP1_1) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(SBUS_UI_RESTART_GP1_2) - 1 <= UI_LEAVE_STEP_CHARS,
              "a leave step runs past x 232");
static_assert(SB_TEXT_W(SBUS_UI_BACK_TO) + 8 <= UI_LEAVE_LEFT.w, "BACK TO MONITOR does not fit its button");

/** @brief RETRY, the wide button, under a failed bind. */
static constexpr UiRect BTN_RETRY = { UI_EDGE, UI_FOOT_Y, GFX_W - 2 * UI_EDGE, UI_FOOT_H };

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

/** @brief Which view owns the panel. */
enum class View : uint8_t { Main, Leave };

static View         s_view = View::Main;
static bool         s_claimed = false;   /**< pinClaimBegin() took this screen's claim. */
/** @brief Why the last claim was refused; EscAnswering also for the screen's own check. */
static ClaimRefusal s_refusal = ClaimRefusal::None;
static uint8_t      s_roles = 0;         /**< sbusClaimRoles() of the wiring; 0: no role on GP1. */
static SbusStatus   s_st;                /**< This frame's status from core1. */
static SbusLostHold s_lost;              /**< Frame-lost frames, for the hold. */
static UiLeave      s_leave;             /**< The leave page: its phase and its presses. */
static const TouchState *s_t = nullptr;  /**< This frame's touch. */
static bool         s_full = true;       /**< Repaint every region this frame. */
static bool         s_rows = false;      /**< The body shows the channels, not a message. */
/** @brief Keys of the regions drawn last: top, message, 16 rows, 4 lines. */
static uint32_t     s_keys[2 + SBUS_CHANNELS + 4];

/** @brief Region indices into s_keys. */
enum : int { K_TOP = 0, K_MSG = 1, K_ROW0 = 2, K_LINE0 = 2 + (int)SBUS_CHANNELS };

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/** @brief FNV-1a of a key's bytes. */
static uint32_t hashOf(const void *p, size_t n) {
	const uint8_t *b = (const uint8_t *)p;
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
	return h;
}

/** @brief A hash of a line of text for a key; 0 for none. */
static uint32_t textKey(const char *s) { return s ? hashOf(s, strlen(s)) | 1u : 0; }

/** @brief True when region @p i must repaint for key @p key. */
static bool changed(int i, const void *key, size_t n) {
	uint32_t h = hashOf(key, n);
	if (!s_full && s_keys[i] == h) return false;
	s_keys[i] = h;
	return true;
}

/** @brief GP1's role in the wiring: `SIG` when SIG is GP1, else `AUX`. */
static const char *gp1Role() { return s_roles == PIN_SIG ? "SIG" : "AUX"; }

/** @brief The claim holds GP1 and the job is bound: a failed bind leaves it Held. */
static bool live() { return s_claimed && pinClaimState() == ClaimState::Running; }

/**
 * @brief Whether BACK acts on this frame: everywhere but on the leave page,
 *        where BACK TO MONITOR and RESTART DSHOT are the ways on
 *        (docs/design/menu-b.md, decision 5).
 */
static bool backActs() { return s_view != View::Leave; }

/**
 * @brief Ask for GP1: the roles of the wiring, after the screen's own checks.
 *
 * No role on GP1 asks for nothing. With GP1 as SIG the receiver goes onto
 * the ESC's net, and the claim does not check for an ESC for this user, so
 * the screen refuses while an eRPM frame is fresh, by escFieldFresh(), the
 * rule pinClaimCheck() uses, and after its Armed and Busy rules, which come
 * first there.
 */
static void claimPin() {
	s_roles = sbusClaimRoles(escTaskDshotPin(), escTaskKissPin());
	s_claimed = false;
	if (!s_roles) {
		s_refusal = ClaimRefusal::None;
		return;
	}
	if (s_roles == PIN_SIG && !uiArmed() && pinClaimState() == ClaimState::Free) {
		EscTelemetry tel;
		escSnapshot(&tel);
		if (escFieldFresh(tel.lastRpmMs, millis(), ESC_LINK_STALE_MS)) {
			s_refusal = ClaimRefusal::EscAnswering;
			return;
		}
	}
	s_refusal = pinClaimBegin(PinUser::Sbus, s_roles);
	s_claimed = s_refusal == ClaimRefusal::None;
}

/**
 * @brief What the body shows in place of the channels, or null for the
 *        channels: the claim's refusal, its progress or a failed bind.
 * @param[out] l1    First line.
 * @param[out] l2    Second line, or null.
 * @param[out] col   Their colour.
 * @param[out] retry RETRY is offered.
 */
static void message(const char **l1, const char **l2, uint16_t *col, bool *retry) {
	static char a[40];
	*l1 = *l2 = nullptr;
	*col = UI_COL_WARN;
	*retry = false;
	if (!s_roles) {
		*l1 = SBUS_UI_NO_GP1_1; *l2 = SBUS_UI_NO_GP1_2;
		return;
	}
	if (!s_claimed) {
		switch (s_refusal) {
			case ClaimRefusal::EscAnswering: *l1 = SBUS_UI_ESC_1;  *l2 = SBUS_UI_UNPLUG_2; return;
			case ClaimRefusal::KissLive:     *l1 = SBUS_UI_KISS_1; *l2 = SBUS_UI_UNPLUG_2; return;
			case ClaimRefusal::Armed:        *l1 = "DISARM FIRST"; return;
			default:                         *l1 = UI_WAIT_RESTART_TEXT; return;
		}
	}
	ClaimState cs = pinClaimState();
	if (cs == ClaimState::Stopping) {
		snprintf(a, sizeof(a), "RELEASING %s GP1", gp1Role());
		*l1 = a; *col = C_DIM;
		return;
	}
	uint8_t err = pinClaimJobError();
	if (err == CORE1_JOB_NO_GP1) {
		*l1 = SBUS_UI_NO_GP1_1; *l2 = SBUS_UI_NO_GP1_2;
		return;
	}
	if (err) {
		static const char *const WHY[] = {"", "REFUSED", "PWM", "PIO", "DMA", "CLOCK"};
		snprintf(a, sizeof(a), "%s GP1 COULD NOT BE TAKEN (%s)", gp1Role(), WHY[err < 6 ? err : 1]);
		*l1 = a;
		*retry = true;
		return;
	}
	if (cs != ClaimState::Running) {
		snprintf(a, sizeof(a), "TAKING %s GP1", gp1Role());
		*l1 = a; *col = C_DIM;
	}
}

// ---------------------------------------------------------------------------
// the monitor
// ---------------------------------------------------------------------------

/** @brief The header, the caption and, while the job runs, the link chip. */
static void drawTop(bool running, SbusLink link) {
	struct { bool back, backOk, running; uint8_t link, roles; } k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.backOk = backActs();
	k.running = running;
	k.link = (uint8_t)link;
	k.roles = s_roles;
	if (!changed(K_TOP, &k, sizeof(k))) return;
	uiHeader(SBUS_UI_TITLE, s_t, k.backOk);
	uiStripClear();
	char cap[40];
	if (s_roles) snprintf(cap, sizeof(cap), SBUS_UI_CAPTION_FMT, gp1Role());
	else snprintf(cap, sizeof(cap), "%s", SBUS_UI_CAPTION_TAIL);
	uiStripText(cap, C_DIM);
	if (running) uiChip(sbusLinkText(link), sbusLinkFill(link));
}

/** @brief The bar's and the value's colours for an ink. */
static void inkColours(SbusInk ink, uint16_t *bar, uint16_t *value) {
	switch (ink) {
		case SbusInk::Live:     *bar = UI_COL_LIVE; *value = C_TEXT; return;
		case SbusInk::Failsafe: *bar = UI_COL_ON;   *value = UI_COL_ON; return;
		case SbusInk::Stale:    *bar = C_DIM;       *value = C_DIM; return;
		case SbusInk::Empty:    break;
	}
	*bar = C_BG;
	*value = C_DIM;
}

/** @brief The x of raw @p raw on a bar: its fill's end, for the ticks. */
static int rawX(uint16_t raw) { return SBUS_UI_BAR_X + 1 + sbusBarFill(raw, SBUS_UI_FILL_W); }

/** @brief Channel row @p i: number, bar in raw units with its three ticks, value. */
static void drawRow(int i, SbusInk ink) {
	struct { uint8_t ink; uint16_t raw; } k;
	memset(&k, 0, sizeof(k));
	k.ink = (uint8_t)ink;
	k.raw = ink == SbusInk::Empty ? 0 : s_st.channel[i];
	if (!changed(K_ROW0 + i, &k, sizeof(k))) return;
	const int y = SBUS_UI_ROW_Y + i * SBUS_UI_ROW_PITCH;
	gfxRect(0, y, GFX_W, SBUS_UI_ROW_H, C_BG);
	char num[4];
	snprintf(num, sizeof(num), "%d", i + 1);
	gfxText(SBUS_UI_NUM_END - gfxTextW(num, 1), y + 2, num, C_DIM, 1);
	// The outline, 1 px, and the fill inside it from x 25.
	const int bx = SBUS_UI_BAR_X, by = y + 1;
	gfxHLine(bx, by, SBUS_UI_BAR_W, C_DIM);
	gfxHLine(bx, by + SBUS_UI_BAR_H - 1, SBUS_UI_BAR_W, C_DIM);
	gfxVLine(bx, by, SBUS_UI_BAR_H, C_DIM);
	gfxVLine(bx + SBUS_UI_BAR_W - 1, by, SBUS_UI_BAR_H, C_DIM);
	uint16_t bar, val;
	inkColours(ink, &bar, &val);
	// Empty has raw 0: no fill.
	gfxRect(bx + 1, by + 1, sbusBarFill(k.raw, SBUS_UI_FILL_W), SBUS_UI_BAR_H - 2, bar);
	// 1000, 1500 and 2000 us by sbus_to_us(): raw 172, 992 and 1811.
	static const uint16_t TICKS[] = { SBUS_RAW_1000US, 992, SBUS_RAW_2000US };
	for (uint16_t t : TICKS) gfxVLine(rawX(t), by + 1, SBUS_UI_BAR_H - 2, C_GRID);
	char v[8];
	sbusValueText(v, sizeof(v), &s_st, (unsigned)i);
	gfxText(SBUS_UI_VALUE_END - gfxTextW(v, 1), y + 2, v, val, 1);
}

/** @brief One count line: @p left from x 8 and @p right ending at x 231. */
static void drawLine(int idx, int y, const char *left, const char *right, uint16_t col) {
	struct { uint32_t l, r; uint16_t col; } k;
	memset(&k, 0, sizeof(k));
	k.l = textKey(left);
	k.r = textKey(right);
	k.col = col;
	if (!changed(K_LINE0 + idx, &k, sizeof(k))) return;
	gfxRect(0, y, GFX_W, 8, C_BG);
	if (left) gfxText(UI_MARGIN, y, left, col, 1);
	if (right) gfxText(SBUS_UI_VALUE_END - gfxTextW(right, 1), y, right, col, 1);
}

/** @brief The four count lines under the channels. */
static void drawLines(SbusInk ink) {
	char a[40], b[40];
	// Line 1 describes the last frame: dim when it is not current.
	uint16_t c1 = (ink == SbusInk::Live || ink == SbusInk::Failsafe) ? C_TEXT : C_DIM;
	sbusDigitalText(a, sizeof(a), &s_st);
	sbusRateText(b, sizeof(b), &s_st);
	drawLine(0, SBUS_UI_LINE1_Y, a, b, c1);
	sbusFramesText(a, sizeof(a), &s_st);
	sbusLostText(b, sizeof(b), &s_st);
	drawLine(1, SBUS_UI_LINE2_Y, a, b, C_TEXT);
	sbusErrorText(a, sizeof(a), &s_st);
	drawLine(2, SBUS_UI_LINE3_Y, a, nullptr, C_TEXT);
	sbusDecoderText(a, sizeof(a), &s_st);
	drawLine(3, SBUS_UI_LINE4_Y, a, nullptr, C_TEXT);
}

/** @brief The body in place of the channels: the message and RETRY. */
static void drawMessage(const char *l1, const char *l2, uint16_t col, bool retry) {
	struct { uint32_t l1, l2; uint16_t col; bool retry, rp; } k;
	memset(&k, 0, sizeof(k));
	k.l1 = textKey(l1);
	k.l2 = textKey(l2);
	k.col = col;
	k.retry = retry;
	k.rp = retry && uiPressing(BTN_RETRY, s_t);
	if (!changed(K_MSG, &k, sizeof(k))) return;
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	if (l1) gfxText(UI_MARGIN, SBUS_UI_MSG_Y, l1, col, 1);
	if (l2) gfxText(UI_MARGIN, SBUS_UI_MSG_Y + SBUS_UI_MSG_H, l2, col, 1);
	if (retry) uiButton(BTN_RETRY, "RETRY", C_PANEL, C_TEXT, 2, k.rp);
}

// ---------------------------------------------------------------------------
// the leave page
// ---------------------------------------------------------------------------

/**
 * @brief Draw the leave page: no step before LEAVE, since BACK on the
 *        monitor is the leave, so it opens releasing.
 */
static void drawLeave() {
	uint32_t now = millis();
	struct {
		bool    back, left, wide, live, restartPress, released;
		uint8_t sig, roles;
	} k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.left = uiPressing(UI_LEAVE_LEFT, s_t);
	k.wide = uiPressing(UI_LEAVE_WIDE, s_t);
	k.live = uiLeaveRestartLive(&s_leave, now);
	k.restartPress = s_leave.restartPress;
	k.released = s_leave.phase == UiLeavePhase::Released;
	k.sig = pinClaimGpio(PIN_SIG) != PIN_CLAIM_NO_GPIO ? pinClaimGpio(PIN_SIG) : escTaskDshotPin();
	k.roles = s_roles;
	if (!changed(K_TOP, &k, sizeof(k))) return;
	uiHeader(SBUS_UI_TITLE, s_t, backActs());
	uiStripClear();
	char strip[32];
	if (s_roles == PIN_SIG) snprintf(strip, sizeof(strip), "SIG GP%u", (unsigned)k.sig);
	else snprintf(strip, sizeof(strip), "SIG GP%u  AUX GP1", (unsigned)k.sig);
	uiStripText(strip, C_DIM);
	static char dshot[40];
	UiLeaveText tx;
	memset(&tx, 0, sizeof(tx));
	tx.step[0][0] = SBUS_UI_UNPLUG;
	if (s_roles == PIN_SIG) {
		tx.step[0][1] = SBUS_UI_RESTART_GP1_1;
		tx.step[0][2] = SBUS_UI_RESTART_GP1_2;
	} else {
		snprintf(dshot, sizeof(dshot), SBUS_UI_RESTART_FMT, (unsigned)k.sig);
		tx.step[0][1] = dshot;
	}
	tx.step[1][0] = UI_LEAVE_RESTART;
	tx.next = SBUS_UI_BACK_TO;
	if (!k.released) tx.note[0] = UI_LEAVE_RELEASING;
	uiLeaveDraw(&s_leave, tx, s_t, now);
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

uint8_t uiSbusControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), backActs());
	if (s_view == View::Leave) {
		uiControlAdd(out, &n, max, UI_FIX(SbusCtl::BackToMonitor), UI_LEAVE_LEFT,
		             s_leave.phase == UiLeavePhase::Released);
		uiControlAdd(out, &n, max, UI_FIX(SbusCtl::Restart), UI_LEAVE_WIDE,
		             uiLeaveRestartLive(&s_leave, millis()));
		return n;
	}
	const char *l1, *l2;
	uint16_t col;
	bool retry;
	message(&l1, &l2, &col, &retry);
	if (retry) uiControlAdd(out, &n, max, UI_FIX(SbusCtl::Retry), BTN_RETRY, true);
	return n;
}

void uiSbusEnter() {
	s_view = View::Main;
	s_claimed = false;
	s_refusal = ClaimRefusal::None;
	memset(&s_st, 0, sizeof(s_st));
	memset(&s_lost, 0, sizeof(s_lost));
	s_full = true;
	s_rows = false;
	claimPin();
}

bool uiSbusTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	sbusJobStatus(&s_st);
	if (s_claimed) pinClaimPoll();
	else if (s_view != View::Leave) claimPin();

	// BACK: before a claim held, or while Stopping, back to the chooser's
	// origin with DShot resumed; once a job has held GP1, the leave page.
	if (backActs() && uiBackTapped(s_t)) {
		if (!s_claimed) return false;
		pinClaimEnd(PinUser::Sbus);
		ClaimState cs = pinClaimState();
		// Restoring: core1 suspended the pump between pinClaimPoll() and
		// pinClaimEnd().
		if (cs == ClaimState::Free || cs == ClaimState::Restoring) return false;
		s_view = View::Leave;
		s_full = true;
		// RESTART DSHOT waits 1000 ms after the pin is let go, which the job
		// does at once, so a double tap restarts nothing.
		uiLeaveOpen(&s_leave, UiLeavePhase::Releasing, UI_HOLD_MS);
	}

	if (s_view == View::Leave) {
		uiLeavePoll(&s_leave, pinClaimUnplugDue(), millis());
		UiLeaveEv ev = uiLeaveTouch(&s_leave, s_t, millis());
		// pinClaimKeep() and pinClaimRestart() act only while the pin is let go.
		if (ev == UiLeaveEv::Restart && pinClaimUnplugDue()) {
			pinClaimRestart(PinUser::Sbus);
			return false;
		}
		if (ev == UiLeaveEv::Next && pinClaimUnplugDue()) {
			pinClaimKeep(PinUser::Sbus);
			s_view = View::Main;
			memset(&s_lost, 0, sizeof(s_lost));
			s_full = true;
			s_rows = false;
		} else {
			if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
			drawLeave();
			s_full = false;
			return true;
		}
	}

	const char *l1, *l2;
	uint16_t col;
	bool retry;
	message(&l1, &l2, &col, &retry);
	if (retry && uiTapped(BTN_RETRY, s_t)) pinClaimRetry(PinUser::Sbus);

	uint32_t now = millis();
	sbusLostHoldStep(&s_lost, &s_st, now);
	SbusLink link = sbusLink(&s_st, &s_lost, now);
	bool running = live();
	// A view's regions do not cover the rows between them, so a new view
	// starts from a clear panel.
	if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
	drawTop(running, link);
	bool rows = running && !l1;
	if (rows != s_rows) {
		// The message and the channels cover different rows of the body.
		gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
		for (int i = K_MSG; i < K_LINE0 + 4; i++) s_keys[i] = 0;
		s_rows = rows;
	}
	if (rows) {
		SbusInk ink = sbusInk(&s_st, link);
		for (int i = 0; i < (int)SBUS_CHANNELS; i++) drawRow(i, ink);
		drawLines(ink);
	} else {
		message(&l1, &l2, &col, &retry);
		drawMessage(l1, l2, col, retry);
	}
	s_full = false;
	return true;
}

const UiScreen UI_SBUS = {
	UiScreenId::Sbus, SBUS_UI_TITLE, PinUser::Sbus, nullptr,
	uiSbusEnter, uiSbusTick, nullptr, uiSbusControls, nullptr, false,
};
