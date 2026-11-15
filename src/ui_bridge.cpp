/**
 * @file ui_bridge.cpp
 * @brief USB BRIDGE. @see ui_bridge.h
 */

#include "ui_bridge.h"
#include "usb_bridge.h"
#include "bridge_msp.h"
#include "bridge_4way.h"
#include "usb_msc.h"
#include "ui_screen.h"
#include "ui_input.h"
#include "ui_hold.h"
#include "ui_leave.h"
#include "ui_widgets.h"
#include "ui.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "core1_job.h"
#include "gfx.h"
#include "touch.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// layout checks
// ---------------------------------------------------------------------------

/** @brief gfxTextW()'s rule at scale 1: 6 px a character less one. */
#define BR_W1(chars) (6 * (chars) - 1)
/** @brief ...and at scale 2. */
#define BR_W2(chars) (12 * (chars) - 2)
/** @brief A string literal's characters. */
#define BR_LEN(s) ((int)sizeof(s) - 1)
/** @brief Where the widest chip, PORT CLOSED, starts: x 155. */
#define BR_CHIP_X (GFX_W - UI_MARGIN - (BR_W1(BR_LEN(BRIDGE_UI_PORT_CLOSED)) + 12))
/** @brief The widest counter, `9999999 B`. */
#define BR_COUNT_CHARS 9
/** @brief The widest frames line, `FRAMES 9999999  LOST 9999999`. */
#define BR_FRAMES_CHARS 28
/** @brief The widest last line, `LAST SET ADDRESS 7E00`. */
#define BR_LAST_CHARS 21

static_assert(UI_MARGIN + BR_W2(BR_LEN(BRIDGE_UI_TITLE)) + 44 <= UI_BACK_X,
              "USB BRIDGE's title is not 44 px clear of BACK");
// A one-digit GPIO in place of %u.
static_assert(UI_MARGIN + BR_W1(BR_LEN(BRIDGE_UI_CAPTION_FMT) - 1) + 40 <= BR_CHIP_X,
              "the strip's caption is not 40 px clear of the widest chip");
static_assert(UI_MARGIN + BR_W1(BR_LEN(BRIDGE_UI_RELEASING_FMT) - 1) + 40 <= BR_CHIP_X,
              "RELEASING runs under the chip's place");
static_assert(BRIDGE_UI_MODE_CAP_Y == UI_BODY_Y && BRIDGE_UI_MODE_CAP_Y + 8 <= BRIDGE_UI_MODE_Y &&
              BRIDGE_UI_MODE_Y + 14 + 8 <= BRIDGE_UI_COUNT_CAP_Y &&
              BRIDGE_UI_COUNT_CAP_Y + 8 <= BRIDGE_UI_COUNT_Y &&
              BRIDGE_UI_COUNT_Y + 14 + 8 <= BRIDGE_UI_FRAMES_Y &&
              BRIDGE_UI_FRAMES_Y + 8 + 4 <= BRIDGE_UI_LAST_Y &&
              BRIDGE_UI_LAST_Y + 8 + 8 <= BRIDGE_UI_MSG_Y,
              "two of USB BRIDGE's lines overlap");
static_assert(UI_MARGIN + BR_W2(BR_LEN(BRIDGE_UI_PASSTHROUGH)) <= GFX_W - UI_MARGIN,
              "the mode runs past the margin");
static_assert(UI_MARGIN + BR_W2(BR_COUNT_CHARS) + 4 <= BRIDGE_UI_RIGHT_X &&
              BRIDGE_UI_RIGHT_X + BR_W2(BR_COUNT_CHARS) <= GFX_W - UI_MARGIN,
              "a counter runs into the other, or past the margin");
static_assert(UI_MARGIN + BR_W1(BR_FRAMES_CHARS) <= GFX_W - UI_MARGIN &&
              UI_MARGIN + BR_W1(BR_LAST_CHARS) <= GFX_W - UI_MARGIN,
              "the frames line or the last line runs past the margin");
static_assert(BRIDGE_UI_MSG_Y + (BRIDGE_UI_MSG_LINES - 1) * BRIDGE_UI_MSG_H + 7 == 235 &&
              BRIDGE_UI_MSG_Y + (BRIDGE_UI_MSG_LINES - 1) * BRIDGE_UI_MSG_H + 7 + 4 <= UI_FOOT_Y,
              "the message lines do not end at y 235, or run into RETRY");
// A message line from x 8 holds 37 characters, to x 228.
static_assert(BR_LEN(BRIDGE_UI_LINE_0) <= 37 && BR_LEN(BRIDGE_UI_LINE_1) <= 37 &&
              BR_LEN(BRIDGE_UI_LINE_2) <= 37 && BR_LEN(BRIDGE_UI_LINE_4) <= 37 &&
              BR_LEN(BRIDGE_UI_LINE_5) <= 37 && BR_LEN(BRIDGE_UI_WRITING) <= 37 &&
              BR_LEN(BRIDGE_UI_LOST) <= 37 && BR_LEN(BRIDGE_UI_CLOSED) <= 37 &&
              BR_LEN(BRIDGE_UI_EJECT) <= 37 &&
              BR_LEN(BRIDGE_UI_NOT_TAKEN_FMT) - 4 + 1 + 5 <= 37 &&
              BR_LEN(UI_WAIT_RESTART_TEXT) <= 37,
              "a message line runs past x 228");
static_assert(BR_LEN(BRIDGE_UI_LEAVE_1) <= UI_LEAVE_STEP_CHARS &&
              BR_LEN(UI_LEAVE_RESTART) <= UI_LEAVE_STEP_CHARS &&
              BR_LEN(BRIDGE_UI_LEAVE_NOTE) <= UI_LEAVE_NOTE_CHARS,
              "a leave step or the note runs past x 232");
static_assert(BR_W1(BR_LEN(BRIDGE_UI_BACK_TO)) + 8 <= UI_LEAVE_LEFT.w, "BRIDGE does not fit its button");

/** @brief RETRY, the wide button, after a failed bind. */
static constexpr UiRect BTN_RETRY = { UI_EDGE, UI_FOOT_Y, GFX_W - 2 * UI_EDGE, UI_FOOT_H };

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

/** @brief Which view owns the panel. */
enum class View : uint8_t { Main, Leave };

static View         s_view = View::Main;
static bool         s_claimed = false;   /**< pinClaimBegin() took this screen's claim. */
static bool         s_eject = false;     /**< USB DRIVE holds the card: no claim. */
static ClaimRefusal s_refusal = ClaimRefusal::None;
static UiLeave      s_leave;
static const TouchState *s_t = nullptr;
static bool         s_full = true;       /**< Repaint every region this frame. */
static bool         s_body = false;      /**< The mode and the counts are drawn. */
static uint32_t     s_countsMs = 0;      /**< millis() of the counts' last repaint. */
/** @brief Keys of the regions drawn last: top, mode, counts, messages, RETRY. */
static uint32_t     s_keys[5];
enum : int { K_TOP = 0, K_MODE, K_COUNTS, K_MSG, K_FOOT };

static uint32_t hashOf(const void *p, size_t n) {
	const uint8_t *b = (const uint8_t *)p;
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
	return h;
}

static uint32_t textKey(const char *s) { return s ? hashOf(s, strlen(s)) | 1u : 0; }

/** @brief True when region @p i must repaint for key @p key. */
static bool changed(int i, const void *key, size_t n) {
	uint32_t h = hashOf(key, n);
	if (!s_full && s_keys[i] == h) return false;
	s_keys[i] = h;
	return true;
}

/** @brief SIG's GPIO: the claim's latch once Held, the pump's before. */
static unsigned sigGpio() {
	uint8_t g = pinClaimGpio(PIN_SIG);
	return g != PIN_CLAIM_NO_GPIO ? g : escTaskDshotPin();
}

/** @brief The bridge is open: the claim Running for it (usb_bridge.h). */
static bool bridgeOpen() { return usbBridgeMode() != BridgeMode::Closed; }

/**
 * @brief Whether BACK acts: not on the leave page, and not while a
 *        write-class frame waits for its answer (decision 7).
 */
static bool backActs() { return s_view == View::Main && !usbBridgeWriteWaiting(); }

/**
 * @brief Ask for SIG, unless USB DRIVE holds the card (decision 11). The
 *        claim checks for no ESC: the bridge needs it on the pin.
 */
static void claimPin() {
	s_claimed = false;
	s_eject = mscHoldsCard(mscGetState());
	if (s_eject) return;
	s_refusal = pinClaimBegin(PinUser::Bridge, PIN_SIG);
	s_claimed = s_refusal == ClaimRefusal::None;
}

/**
 * @brief The message lines and their colours, and whether RETRY shows.
 * @param[out] l     Six lines; null for a blank one.
 * @param[out] c     Their colours.
 * @param[out] retry RETRY is offered.
 */
static void messages(const char **l, uint16_t *c, bool *retry) {
	static char a[48];
	for (int i = 0; i < BRIDGE_UI_MSG_LINES; i++) {
		l[i] = nullptr;
		c[i] = C_DIM;
	}
	*retry = false;
	if (s_eject) {
		l[0] = BRIDGE_UI_EJECT;
		c[0] = UI_COL_WARN;
		return;
	}
	if (!s_claimed) {
		l[0] = s_refusal == ClaimRefusal::Armed ? "DISARM FIRST" : UI_WAIT_RESTART_TEXT;
		c[0] = UI_COL_WARN;
		return;
	}
	uint8_t err = pinClaimJobError();
	if (pinClaimState() == ClaimState::Held && err) {
		static const char *const WHY[] = {"", "REFUSED", "PWM", "PIO", "DMA", "CLOCK"};
		snprintf(a, sizeof(a), BRIDGE_UI_NOT_TAKEN_FMT, sigGpio(), WHY[err < 6 ? err : 1]);
		l[0] = a;
		c[0] = UI_COL_WARN;
		*retry = true;
		return;
	}
	l[0] = BRIDGE_UI_LINE_0;
	l[1] = BRIDGE_UI_LINE_1;
	l[2] = BRIDGE_UI_LINE_2;
	l[4] = BRIDGE_UI_LINE_4;
	l[5] = BRIDGE_UI_LINE_5;
	if (!bridgeOpen()) return;
	if (usbBridgeWriteWaiting()) {
		l[0] = BRIDGE_UI_WRITING;
		c[0] = UI_COL_WARN;
	} else if (usbBridgePortClosed()) {
		l[0] = BRIDGE_UI_CLOSED;
	}
	UsbBridgeStats st;
	usbBridgeStats(&st);
	if (st.lost) {
		l[1] = BRIDGE_UI_LOST;
		c[1] = UI_COL_WARN;
	}
	if (usbBridgeMotorRefused()) {
		l[2] = BRIDGE_UI_MOTOR;
		c[2] = UI_COL_WARN;
	}
}

/** @brief The mode's text. */
static const char *modeText(BridgeMode m) {
	if (m == BridgeMode::Passthrough) return BRIDGE_UI_PASSTHROUGH;
	if (m == BridgeMode::Msp || m == BridgeMode::FourWay) return BRIDGE_UI_4WAY;
	return BRIDGE_UI_WAITING;
}

/** @brief The last command's line: `LAST SET ADDRESS 7E00`, `LAST MSP UID`, `LAST READ 7C00`. */
static void lastText(char *b, size_t n, const BridgeLast &last) {
	const char *name = nullptr;
	switch (last.cmd) {
		case BridgeCmd::SetAddress:
			snprintf(b, n, "LAST %s %04X", usbBridgeCmdName(last.cmd), (unsigned)last.addr);
			return;
		case BridgeCmd::Payload:
			snprintf(b, n, "LAST %s %u B", usbBridgeCmdName(last.cmd), (unsigned)last.len);
			return;
		case BridgeCmd::Other:
			snprintf(b, n, "LAST %s %02X", usbBridgeCmdName(last.cmd), (unsigned)last.first);
			return;
		case BridgeCmd::Msp:
			name = bridgeMspName(last.first);
			if (name) snprintf(b, n, "LAST MSP %s", name);
			else snprintf(b, n, "LAST MSP %u", (unsigned)last.first);
			return;
		case BridgeCmd::FourWay:
			name = bridge4wayName(last.first);
			if (!name) snprintf(b, n, "LAST 4-WAY %02X", (unsigned)last.first);
			else if (bridge4wayNameHasAddress(last.first)) snprintf(b, n, "LAST %s %04X", name, (unsigned)last.addr);
			else snprintf(b, n, "LAST %s", name);
			return;
		default:
			snprintf(b, n, "LAST %s", usbBridgeCmdName(last.cmd));
			return;
	}
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------

/** @brief The header, the strip's caption and, while open, the port's chip. */
static void drawTop() {
	struct { bool back, backOk, stopping; uint8_t chip; unsigned sig; } k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.backOk = backActs();
	k.stopping = s_claimed && pinClaimState() == ClaimState::Stopping;
	k.chip = s_view == View::Main && bridgeOpen() ? (usbBridgePortOpen() ? 1 : 2) : 0;
	k.sig = sigGpio();
	if (!changed(K_TOP, &k, sizeof(k))) return;
	uiHeader(BRIDGE_UI_TITLE, s_t, k.backOk);
	uiStripClear();
	char cap[32];
	snprintf(cap, sizeof(cap), k.stopping ? BRIDGE_UI_RELEASING_FMT : BRIDGE_UI_CAPTION_FMT, k.sig);
	uiStripText(cap, C_DIM);
	if (k.chip == 1) uiChip(BRIDGE_UI_PORT_OPEN, UI_COL_LIVE);
	else if (k.chip == 2) uiChip(BRIDGE_UI_PORT_CLOSED, C_GRID);
}

/** @brief A counter, `1234 B`, clamped to @ref BRIDGE_UI_COUNT_MAX. */
static void countText(char *b, size_t n, uint32_t v) {
	snprintf(b, n, "%lu B", (unsigned long)(v > BRIDGE_UI_COUNT_MAX ? BRIDGE_UI_COUNT_MAX : v));
}

/** @brief The mode, the counters, the frames line and the last command. */
static void drawBody() {
	BridgeMode m = usbBridgeMode();
	if (changed(K_MODE, &m, sizeof(m))) {
		gfxRect(0, BRIDGE_UI_MODE_CAP_Y, GFX_W, BRIDGE_UI_MODE_Y + 14 - BRIDGE_UI_MODE_CAP_Y, C_BG);
		gfxText(UI_MARGIN, BRIDGE_UI_MODE_CAP_Y, BRIDGE_UI_MODE, C_DIM, 1);
		gfxText(UI_MARGIN, BRIDGE_UI_MODE_Y, modeText(m), C_TEXT, 2);
	}
	const uint32_t now = millis();
	if (!s_full && now - s_countsMs < BRIDGE_UI_COUNT_MS) return;
	UsbBridgeStats st;
	usbBridgeStats(&st);
	BridgeLast last = usbBridgeLast();
	struct { UsbBridgeStats st; BridgeLast last; } k;
	memset(&k, 0, sizeof(k));
	k.st = st;
	k.st.lastMs = 0;
	k.st.collisions = 0;
	k.last = last;
	if (!changed(K_COUNTS, &k, sizeof(k))) return;
	s_countsMs = now;
	gfxRect(0, BRIDGE_UI_COUNT_CAP_Y, GFX_W, BRIDGE_UI_LAST_Y + 8 - BRIDGE_UI_COUNT_CAP_Y, C_BG);
	gfxText(UI_MARGIN, BRIDGE_UI_COUNT_CAP_Y, BRIDGE_UI_TO_ESC, C_DIM, 1);
	gfxText(BRIDGE_UI_RIGHT_X, BRIDGE_UI_COUNT_CAP_Y, BRIDGE_UI_TO_PC, C_DIM, 1);
	char b[40];
	countText(b, sizeof(b), st.toEsc);
	gfxText(UI_MARGIN, BRIDGE_UI_COUNT_Y, b, C_TEXT, 2);
	countText(b, sizeof(b), st.toHost);
	gfxText(BRIDGE_UI_RIGHT_X, BRIDGE_UI_COUNT_Y, b, C_TEXT, 2);
	snprintf(b, sizeof(b), BRIDGE_UI_FRAMES_FMT,
	         (unsigned long)(st.frames > BRIDGE_UI_COUNT_MAX ? BRIDGE_UI_COUNT_MAX : st.frames));
	gfxText(UI_MARGIN, BRIDGE_UI_FRAMES_Y, b, C_TEXT, 1);
	int x = UI_MARGIN + 6 * (int)strlen(b);
	snprintf(b, sizeof(b), BRIDGE_UI_LOST_FMT,
	         (unsigned long)(st.lost > BRIDGE_UI_COUNT_MAX ? BRIDGE_UI_COUNT_MAX : st.lost));
	gfxText(x, BRIDGE_UI_FRAMES_Y, b, st.lost ? UI_COL_WARN : C_TEXT, 1);
	if (last.cmd != BridgeCmd::None) {
		lastText(b, sizeof(b), last);
		gfxText(UI_MARGIN, BRIDGE_UI_LAST_Y, b, C_DIM, 1);
	}
}

/** @brief The six message lines. */
static void drawMessages(const char *const *l, const uint16_t *c) {
	uint32_t k[2 * BRIDGE_UI_MSG_LINES];
	for (int i = 0; i < BRIDGE_UI_MSG_LINES; i++) {
		k[2 * i] = textKey(l[i]);
		k[2 * i + 1] = c[i];
	}
	if (!changed(K_MSG, k, sizeof(k))) return;
	gfxRect(0, BRIDGE_UI_MSG_Y, GFX_W, BRIDGE_UI_MSG_LINES * BRIDGE_UI_MSG_H, C_BG);
	for (int i = 0; i < BRIDGE_UI_MSG_LINES; i++)
		if (l[i]) gfxText(UI_MARGIN, BRIDGE_UI_MSG_Y + i * BRIDGE_UI_MSG_H, l[i], c[i], 1);
}

/** @brief RETRY, or the footer cleared. */
static void drawFoot(bool retry) {
	struct { bool retry, pressed; } k = { retry, retry && uiPressing(BTN_RETRY, s_t) };
	if (!changed(K_FOOT, &k, sizeof(k))) return;
	gfxRect(0, UI_FOOT_Y, GFX_W, GFX_H - UI_FOOT_Y, C_BG);
	if (retry) uiButton(BTN_RETRY, "RETRY", C_PANEL, C_TEXT, 2, k.pressed);
}

/** @brief The leave page: opened releasing, BACK having asked for the line. */
static void drawLeave() {
	uint32_t now = millis();
	struct { bool back, left, wide, live, restartPress, released, write; unsigned sig; } k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.left = uiPressing(UI_LEAVE_LEFT, s_t);
	k.wide = uiPressing(UI_LEAVE_WIDE, s_t);
	k.live = uiLeaveRestartLive(&s_leave, now);
	k.restartPress = s_leave.restartPress;
	k.released = s_leave.phase == UiLeavePhase::Released;
	k.write = usbBridgeWriteUnfinished();
	k.sig = sigGpio();
	if (!changed(K_TOP, &k, sizeof(k))) return;
	uiHeader(BRIDGE_UI_TITLE, s_t, backActs());
	uiStripClear();
	char cap[32];
	snprintf(cap, sizeof(cap), BRIDGE_UI_CAPTION_FMT, k.sig);
	uiStripText(cap, C_DIM);
	UiLeaveText tx;
	memset(&tx, 0, sizeof(tx));
	tx.step[0][0] = BRIDGE_UI_LEAVE_1;
	tx.step[1][0] = UI_LEAVE_RESTART;
	// The note after a write with no RUN; a session that only read has none.
	int note = 0;
	if (k.write) tx.note[note++] = BRIDGE_UI_LEAVE_NOTE;
	if (!k.released) tx.note[note] = UI_LEAVE_RELEASING;
	tx.next = BRIDGE_UI_BACK_TO;
	uiLeaveDraw(&s_leave, tx, s_t, now);
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

uint8_t uiBridgeControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), backActs());
	if (s_view == View::Leave) {
		uiControlAdd(out, &n, max, UI_FIX(BridgeCtl::Bridge), UI_LEAVE_LEFT,
		             s_leave.phase == UiLeavePhase::Released);
		uiControlAdd(out, &n, max, UI_FIX(BridgeCtl::Restart), UI_LEAVE_WIDE,
		             uiLeaveRestartLive(&s_leave, millis()));
		return n;
	}
	const char *l[BRIDGE_UI_MSG_LINES];
	uint16_t c[BRIDGE_UI_MSG_LINES];
	bool retry;
	messages(l, c, &retry);
	if (retry) uiControlAdd(out, &n, max, UI_FIX(BridgeCtl::Retry), BTN_RETRY, true);
	return n;
}

void uiBridgeEnter() {
	s_view = View::Main;
	s_claimed = false;
	s_eject = false;
	s_refusal = ClaimRefusal::None;
	s_full = true;
	s_body = false;
	claimPin();
}

/**
 * @brief BACK on the main view.
 * @return false when the screen leaves; true when the leave page opened.
 */
static bool back() {
	if (!s_claimed) return false;
	// Once the line was bound the ESC may wait in its bootloader, entered at
	// a power-up with SIG high, after AM32's 2 s without a signal, or by the
	// tool, answered or not, and its flash may be cut: DShot waits for
	// RESTART DSHOT on the leave page (decision 10 as revised).
	if (pinClaimState() == ClaimState::Running)
		pinClaimWantLeavePage(PinUser::Bridge);
	pinClaimEnd(PinUser::Bridge);
	if (!pinClaimLeavePage()) return false;
	s_view = View::Leave;
	s_full = true;
	// The job lets the line go at once; RESTART DSHOT waits 1000 ms after.
	uiLeaveOpen(&s_leave, UiLeavePhase::Releasing, UI_HOLD_MS);
	return true;
}

bool uiBridgeTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	if (s_claimed) pinClaimPoll();
	else if (s_view == View::Main) claimPin();

	if (backActs() && uiBackTapped(s_t) && !back()) return false;

	if (s_view == View::Leave) {
		uiLeavePoll(&s_leave, pinClaimUnplugDue(), millis());
		UiLeaveEv ev = uiLeaveTouch(&s_leave, s_t, millis());
		if (ev == UiLeaveEv::Restart && pinClaimUnplugDue()) {
			pinClaimRestart(PinUser::Bridge);
			// The steps powered the ESC off: MENU's cue goes.
			usbBridgeRunCueClear();
			return false;
		}
		if (ev == UiLeaveEv::Next && pinClaimUnplugDue()) {
			pinClaimKeep(PinUser::Bridge);
			s_view = View::Main;
			s_full = true;
			s_body = false;
		} else {
			if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
			drawLeave();
			s_full = false;
			return true;
		}
	}

	const char *l[BRIDGE_UI_MSG_LINES];
	uint16_t c[BRIDGE_UI_MSG_LINES];
	bool retry;
	messages(l, c, &retry);
	if (retry && uiTapped(BTN_RETRY, s_t)) pinClaimRetry(PinUser::Bridge);

	if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
	drawTop();
	bool body = bridgeOpen();
	if (body != s_body) {
		gfxRect(0, UI_BODY_Y, GFX_W, BRIDGE_UI_MSG_Y - UI_BODY_Y, C_BG);
		s_keys[K_MODE] = s_keys[K_COUNTS] = 0;
		s_countsMs = millis() - BRIDGE_UI_COUNT_MS;
		s_body = body;
	}
	if (body) drawBody();
	messages(l, c, &retry);
	drawMessages(l, c);
	drawFoot(retry);
	s_full = false;
	return true;
}

const UiScreen UI_BRIDGE = {
	UiScreenId::Bridge, BRIDGE_UI_TITLE, PinUser::Bridge, nullptr,
	uiBridgeEnter, uiBridgeTick, nullptr, uiBridgeControls, nullptr, false,
};
