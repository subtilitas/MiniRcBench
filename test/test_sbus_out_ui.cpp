/**
 * @file test_sbus_out_ui.cpp
 * @brief S.BUS OUTPUT: the formatter of sbus_out_view.h at its boundaries,
 *        and the screen through the real UI, the pin claim, the S.BUS output
 *        job with its bootloader probe, the escwire line's fake and the UART0
 *        fake's transmitter.
 *
 * A UI frame here is 25 ms of core1 passes every 500 us, then one pass of
 * the fake DShot pump and one uiTick(), which stamps the heartbeat. A test
 * that needs a frame at an exact millisecond runs tick() with its length.
 * The ESC on GP0 is rcbench's simulated ESC behind the escwire line's fake,
 * unplugged unless a test plugs it in. What the PL011 and the pad do on the
 * chip is not tested here; the levels on GP0 are not measured.
 */

#include "check.h"
#include "fakes.h"
#include "fake_escwire_port.h"
#include "sbus_out_view.h"
#include "sbus_out_job.h"
#include "ui_sbus_out.h"
#include "ui_am32.h"
#include "ui_widgets.h"
#include "ui.h"
#include "gfx.h"
#include "settings.h"
#include "esc_task.h"
#include "core1_job.h"
#include "pin_claim.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "plat.h"

#include <hardware/gpio.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// the formatter
// ---------------------------------------------------------------------------

/** @brief A status: SAFE, no frame since the bind. */
static SbusOutStatus status() {
	SbusOutStatus st;
	memset(&st, 0, sizeof(st));
	st.phase = SbusOutPhase::Uart;
	return st;
}

static void testViewChipNotes() {
	section("S.BUS OUTPUT view: the chip and the two notes in each state");

	SbusOutStatus st = status();
	checkTrue("SAFE: SAFE on green", sbusOutChip(&st) == SbusOutChip::Safe &&
	          strcmp(sbusOutChipText(SbusOutChip::Safe), "SAFE") == 0 &&
	          sbusOutChipFill(SbusOutChip::Safe) == UI_COL_SAFE);
	st.running = true;
	checkTrue("RUNNING on red", sbusOutChip(&st) == SbusOutChip::Running &&
	          strcmp(sbusOutChipText(SbusOutChip::Running), "RUNNING") == 0 &&
	          sbusOutChipFill(SbusOutChip::Running) == UI_COL_ON);
	st.running = false;
	st.tripped = true;
	checkTrue("tripped: STALLED on magenta", sbusOutChip(&st) == SbusOutChip::Stalled &&
	          strcmp(sbusOutChipText(SbusOutChip::Stalled), "STALLED") == 0 &&
	          sbusOutChipFill(SbusOutChip::Stalled) == UI_COL_ALERT);
	st.running = true;
	checkTrue("RUNNING outranks a trip", sbusOutChip(&st) == SbusOutChip::Running);
	checkTrue("RUNNING and STALLED, 7 characters, 41 px, the widest",
	          gfxTextW(SBUS_OUT_VIEW_RUNNING, 1) == 41 && gfxTextW(SBUS_OUT_VIEW_STALLED, 1) == 41);

	char a[48], b[48];
	uint16_t ca = 0, cb = 0;
	st = status();
	sbusOutNotes(&st, SbusOutStop::None, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
	checkTrue("SAFE: the two warnings, amber",
	          strcmp(a, "GP0 GOES TO A FLIGHT CONTROLLER.") == 0 && strcmp(b, "NEVER TO AN ESC. PROPS OFF.") == 0 &&
	          ca == UI_COL_WARN && cb == UI_COL_WARN);
	checkTrue("191 px and 161 px", gfxTextW(a, 1) == 191 && gfxTextW(b, 1) == 161);

	st.running = true;
	sbusOutNotes(&st, SbusOutStop::None, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
	checkTrue("RUNNING at the default: THROTTLE CH3: 1000 TO 1200 US (20%).",
	          strcmp(a, "THROTTLE CH3: 1000 TO 1200 US (20%).") == 0 && ca == C_TEXT);
	checkTrue("the flags all off, C_TEXT", strcmp(b, "CH17 OFF  CH18 OFF  FS OFF  LOST OFF") == 0 &&
	          cb == C_TEXT && gfxTextW(b, 1) == 215);
	sbusOutNotes(&st, SbusOutStop::None, 0, 1811, 2000, a, b, sizeof(a), &ca, &cb);
	checkTrue("at 100 %, channel 1: 37 characters, 221 px",
	          strcmp(a, "THROTTLE CH1: 1000 TO 2000 US (100%).") == 0 && gfxTextW(a, 1) == 221 &&
	          UI_MARGIN + 221 <= GFX_W - UI_MARGIN);
	sbusOutNotes(&st, SbusOutStop::None, 3, 253, 100, a, b, sizeof(a), &ca, &cb);
	checkTrue("at 100 of 2000, channel 4: 1049 us, 5 %",
	          strcmp(a, "THROTTLE CH4: 1000 TO 1049 US (5%).") == 0);
	sbusOutNotes(&st, SbusOutStop::None, 3, 1811, 2500, a, b, sizeof(a), &ca, &cb);
	checkTrue("MAX THROTTLE past 2000 reads 100 %", strcmp(a, "THROTTLE CH4: 1000 TO 2000 US (100%).") == 0);
	const struct { bool c17, c18, fs, lost; const char *text; } F[] = {
		{ true, false, false, false, "CH17 ON  CH18 OFF  FS OFF  LOST OFF" },
		{ false, true, false, false, "CH17 OFF  CH18 ON  FS OFF  LOST OFF" },
		{ false, false, true, false, "CH17 OFF  CH18 OFF  FS ON  LOST OFF" },
		{ false, false, false, true, "CH17 OFF  CH18 OFF  FS OFF  LOST ON" },
	};
	for (const auto &f : F) {
		st.sent.ch17 = f.c17; st.sent.ch18 = f.c18; st.sent.failsafe = f.fs; st.sent.frame_lost = f.lost;
		sbusOutNotes(&st, SbusOutStop::None, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
		char what[64];
		snprintf(what, sizeof(what), "%s, amber", f.text);
		checkTrue(what, strcmp(b, f.text) == 0 && cb == UI_COL_WARN);
	}
	st.sent = sbus_frame_t{};
	sbusOutNotes(&st, SbusOutStop::Idle, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
	checkTrue("RUNNING outranks an idle stop", strcmp(a, "THROTTLE CH3: 1000 TO 1200 US (20%).") == 0);

	st = status();
	st.tripped = true;
	st.refused = true;
	sbusOutNotes(&st, SbusOutStop::Idle, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
	checkTrue("the heartbeat first: STOPPED: SCREEN STALLED, magenta; HOLD TO START AGAIN., dim",
	          strcmp(a, "STOPPED: SCREEN STALLED") == 0 && ca == UI_COL_ALERT &&
	          strcmp(b, "HOLD TO START AGAIN.") == 0 && cb == C_DIM && gfxTextW(a, 1) == 137 &&
	          gfxTextW(b, 1) == 119);
	st.tripped = false;
	sbusOutNotes(&st, SbusOutStop::Idle, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
	checkTrue("then a refused frame: STOPPED: FRAME REFUSED., amber",
	          strcmp(a, "STOPPED: FRAME REFUSED.") == 0 && ca == UI_COL_WARN &&
	          strcmp(b, "HOLD TO START AGAIN.") == 0 && cb == C_DIM && gfxTextW(a, 1) == 137);
	st.refused = false;
	sbusOutNotes(&st, SbusOutStop::Idle, 2, 499, 400, a, b, sizeof(a), &ca, &cb);
	checkTrue("then the idle rule: STOPPED: NO TOUCH FOR 30 S., amber",
	          strcmp(a, "STOPPED: NO TOUCH FOR 30 S.") == 0 && ca == UI_COL_WARN &&
	          strcmp(b, "HOLD TO START AGAIN.") == 0 && cb == C_DIM && gfxTextW(a, 1) == 161);
}

static void testViewRows() {
	section("S.BUS OUTPUT view: labels, values, steps, the ceiling's x and the idle rule");

	char b[16];
	checkTrue("channel 3 with the throttle on it: CH3 THR, 41 px",
	          sbusOutLabel(b, sizeof(b), 2, 2) && strcmp(b, "CH3 THR") == 0 && gfxTextW(b, 1) == 41);
	checkTrue("channel 16: CH16", !sbusOutLabel(b, sizeof(b), 15, 2) && strcmp(b, "CH16") == 0);
	checkTrue("channel 1 with the throttle on 1: CH1 THR",
	          sbusOutLabel(b, sizeof(b), 0, 0) && strcmp(b, "CH1 THR") == 0);

	SbusOutStatus st = status();
	const char *const START3[] = { "1500", "1500", "1000", "1500", "1000" };
	bool ok = true;
	for (unsigned i = 0; i < 5; i++) {
		sbusOutValueText(b, sizeof(b), &st, i, 2);
		ok = ok && strcmp(b, START3[i]) == 0;
	}
	sbusOutValueText(b, sizeof(b), &st, 15, 2);
	checkTrue("before a frame: the START frame's, 1500 1500 1000 1500, then 1000", ok && strcmp(b, "1000") == 0);
	sbusOutValueText(b, sizeof(b), &st, 0, 0);
	checkTrue("with the throttle on channel 1: channel 1 at 1000", strcmp(b, "1000") == 0);
	checkInt("a channel past 16 reads raw 172", sbusOutRawOf(&st, 16, 2), 172);
	st.frames = 1;
	for (unsigned i = 0; i < SBUS_CHANNELS; i++) st.sent.channel[i] = 1811;
	st.sent.channel[2] = 499;
	sbusOutValueText(b, sizeof(b), &st, 2, 2);
	checkTrue("after a frame: the frame's, raw 499 reads 1200", strcmp(b, "1200") == 0);
	sbusOutValueText(b, sizeof(b), &st, 0, 2);
	checkTrue("raw 1811 reads 2000, 23 px", strcmp(b, "2000") == 0 && gfxTextW(b, 1) == 23);

	checkInt("1000.0: 1000", sbusOutStepUs(1000.0f, 2000), 1000);
	checkInt("1002.4: 1000", sbusOutStepUs(1002.4f, 2000), 1000);
	checkInt("1002.5: 1005", sbusOutStepUs(1002.5f, 2000), 1005);
	checkInt("1568.9: 1570", sbusOutStepUs(1568.9f, 2000), 1570);
	checkInt("below 1000: 1000", sbusOutStepUs(940.0f, 2000), 1000);
	checkInt("above 2000: 2000", sbusOutStepUs(2100.0f, 2000), 2000);
	checkInt("1999.0: 2000", sbusOutStepUs(1999.0f, 2000), 2000);
	checkInt("held to the top, 1200", sbusOutStepUs(1500.0f, 1200), 1200);
	checkInt("held to a top off the step, 1049", sbusOutStepUs(1500.0f, 1049), 1049);
	checkInt("under the top, untouched", sbusOutStepUs(1195.0f, 1200), 1195);

	checkInt("1500 us: raw 992", sbusOutRawFor(1500, 1811), 992);
	checkInt("1200 us under a ceiling of 499: 499, not sbus_from_us()'s 500", sbusOutRawFor(1200, 499), 499);
	checkInt("sbus_from_us(1200) is 500", sbus_from_us(1200), 500);
	checkInt("1000 us: 172", sbusOutRawFor(1000, 1811), 172);
	checkInt("900 us: held to 172", sbusOutRawFor(900, 1811), 172);
	checkInt("2100 us: held to 1811", sbusOutRawFor(2100, 1811), 1811);
	checkInt("a top above 1811 reads as 1811", sbusOutRawFor(2100, 2047), 1811);

	checkInt("1000 us at x 54", sbusOutUsX(1000, 54, 146), 54);
	checkInt("1200 us, the default ceiling, at x 83", sbusOutUsX(1200, 54, 146), 83);
	checkInt("1500 us at x 126", sbusOutUsX(1500, 54, 146), 126);
	checkInt("2000 us at x 199", sbusOutUsX(2000, 54, 146), 199);
	checkInt("below 1000 at x 54", sbusOutUsX(900, 54, 146), 54);
	checkInt("above 2000 at x 199", sbusOutUsX(2100, 54, 146), 199);

	sbus_frame_t f;
	sbusOutStartFrame(&f, 2);
	checkTrue("the START frame is on START", !sbusOutOffStart(&f, 2));
	f.channel[0] = 1811; f.channel[1] = 172; f.channel[3] = 1811;
	checkTrue("sticks other than the throttle anywhere: still on START", !sbusOutOffStart(&f, 2));
	f.failsafe = f.frame_lost = true;
	checkTrue("the failsafe and frame-lost bits: still on START", !sbusOutOffStart(&f, 2));
	sbusOutStartFrame(&f, 2);
	f.channel[2] = 173;
	checkTrue("the throttle at raw 173: off", sbusOutOffStart(&f, 2));
	sbusOutStartFrame(&f, 2);
	bool each = true;
	for (unsigned i = 4; i < SBUS_CHANNELS; i++) {
		sbusOutStartFrame(&f, 2);
		f.channel[i] = 173;
		each = each && sbusOutOffStart(&f, 2);
	}
	checkTrue("each of channels 5 to 16 off its START value: off", each);
	sbusOutStartFrame(&f, 2);
	f.ch17 = true;
	checkTrue("CH17 on: off", sbusOutOffStart(&f, 2));
	f.ch17 = false;
	f.ch18 = true;
	checkTrue("CH18 on: off", sbusOutOffStart(&f, 2));
	sbusOutStartFrame(&f, 0);
	f.channel[0] = 173;
	checkTrue("the throttle on channel 1, at 173: off", sbusOutOffStart(&f, 0));

	sbusOutStartFrame(&f, 2);
	f.channel[2] = 300;
	checkTrue("idle: not at 30,000 ms", !sbusOutIdleDue(true, &f, 2, 31000, 1000));
	checkTrue("at 30,001 ms", sbusOutIdleDue(true, &f, 2, 31001, 1000));
	checkTrue("not while SAFE", !sbusOutIdleDue(false, &f, 2, 99000, 1000));
	sbusOutStartFrame(&f, 2);
	checkTrue("never on the START frame", !sbusOutIdleDue(true, &f, 2, 999000, 1000));
	f.ch18 = true;
	checkTrue("across a wrap of millis()", sbusOutIdleDue(true, &f, 2, 30001u - 16u, 0xFFFFFFF0u) &&
	          !sbusOutIdleDue(true, &f, 2, 30000u - 16u, 0xFFFFFFF0u));
}

// ---------------------------------------------------------------------------
// the screen, through the real UI
// ---------------------------------------------------------------------------

/** @brief One UI frame of @p ms: core1's passes every 500 us, the pump's pass, then the UI. */
static void tick(uint32_t ms) {
	for (uint32_t k = 0; k < 2u * ms; k++) {
		fakeAdvanceUs(500);
		core1JobPoll();
	}
	fakePumpPoll();
	uiTick();
}

/** @brief @p n UI frames of 25 ms. */
static void soFrames(int n) {
	for (int i = 0; i < n; i++) tick(25);
}

/** @brief Core1's passes for @p ms with no UI frame: the heartbeat lapses after 250 ms. */
static void stall(uint32_t ms) {
	for (uint32_t k = 0; k < 2u * ms; k++) {
		fakeAdvanceUs(500);
		core1JobPoll();
	}
}

#define UI_IDS_FRAMES(n) soFrames(n)
#include "ui_ids.h"

/** @brief A pixel of the frame. */
static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/** @brief Whether every pixel of the rectangle is @p c. */
static bool allOf(int x, int y, int w, int h, uint16_t c) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (pixel(i, j) != c) return false;
	return true;
}

/** @brief The line at @p y reads @p text from x 8 in @p col. */
static bool leftIs(int y, const char *text, uint16_t col) {
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, 1);
}

/** @brief The chip reads @p text on @p fill. */
static bool chipIs(const char *text, uint16_t fill) {
	uint16_t fg = fill == C_AMBER || fill == C_INK ? C_PAPER : C_ONACCENT;
	return fakeTextIs(uiChipX(text) + 6, UI_STRIP_Y + 4, text, fg, fill, 1) &&
	       pixel(uiChipX(text) + 1, UI_STRIP_Y + 3) == fill;
}

/** @brief No chip on the strip: its right part is the background. */
static bool noChip() { return allOf(150, UI_STRIP_Y, GFX_W - 150, UI_STRIP_H, C_BG); }

/** @brief The rows' area reads @p l1, @p l2 and @p l3 (null: blank) in @p col. */
static bool messageIs(const char *l1, const char *l2, const char *l3, uint16_t col) {
	bool ok = leftIs(SBOUT_UI_MSG_Y, l1, col);
	ok = ok && (l2 ? leftIs(SBOUT_UI_MSG_Y + SBOUT_UI_MSG_H, l2, col)
	               : allOf(0, SBOUT_UI_MSG_Y + SBOUT_UI_MSG_H, GFX_W, 7, C_BG));
	ok = ok && (l3 ? leftIs(SBOUT_UI_MSG_Y + 2 * SBOUT_UI_MSG_H, l3, col)
	               : allOf(0, SBOUT_UI_MSG_Y + 2 * SBOUT_UI_MSG_H, GFX_W, 7, C_BG));
	return ok;
}

/** @brief The top of the page's row @p i. */
static int rowTop(int i) { return SBOUT_UI_ROW_Y + i * SBOUT_UI_ROW_PITCH; }

/** @brief Row @p i reads @p label in @p lcol and @p value ending at x 231 in @p vcol. */
static bool rowIs(int i, const char *label, uint16_t lcol, const char *value, uint16_t vcol) {
	int y = rowTop(i) + SBOUT_UI_TEXT_DY;
	return fakeTextIs(UI_MARGIN, y, label, lcol, C_BG, 1) &&
	       fakeTextIs(SBOUT_UI_VALUE_END - gfxTextW(value, 1), y, value, vcol, C_BG, 1);
}

/** @brief The S.BUS output's status as core1 last published it. */
static SbusOutStatus jobSt() {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	return st;
}

/** @brief The command the screen last sent, taken or not. */
static SbusOutCmd held() {
	SbusOutCmd c;
	sbusOutJobHeld(&c);
	return c;
}

/** @brief No ESC on SIG's telemetry. */
static void noTelemetry() {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
}

/** @brief No ESC on GP0: rcbench's model, unplugged. */
static void noEscOnGp0() {
	fakeEwReset(0xB2, true);
	fakeEwSim()->dead = true;
	fakeEwAnswerEach(false);
}

/** @brief SIG and AUX on @p sig and @p aux, in the settings and the pump. */
static void wiring(uint8_t sig, uint8_t aux) {
	settings()->dshotPin = sig;
	settings()->kissPin = aux;
	escTaskConfigure(sig, settings()->dshotKbaud, settings()->kissEnable != 0, aux);
	soFrames(2);
}

/** @brief The tester, default settings, no ESC anywhere. */
static void freshUi() {
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) soFrames(1);
	fakeFlashClear();
	settingsLoad();
	fakeClockSet(clk_peri, 150000000u);
	fakeUartReset();
	noTelemetry();
	noEscOnGp0();
	uiInit();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
	wiring(0, 1);
	soFrames(2);
}

/** @brief From the tester through MENU and RX SIGNALS to S.BUS OUTPUT. */
static void openOutput() {
	tapId(UI_FIX(TesterCtl::Menu));
	soFrames(1);
	tapId(menuRow(&UI_RX_SIGNALS));
	soFrames(1);
	tapId(UI_FIX(RxCtl::Output));
}

/** @brief Frames until the probe passed; false after 3 s. */
static bool toSafe() {
	for (int k = 0; k < 120 && !sbusOutProbePassed(); k++) soFrames(1);
	soFrames(1);
	return sbusOutProbePassed();
}

/** @brief HOLD TO START for 1000 ms and the release: frames run after it. */
static bool holdStart() {
	int x, y;
	ctlCentre(UI_FIX(SbusOutCtl::Start), &x, &y);
	fakePress(x, y);
	tick(25);
	for (int k = 0; k < 40; k++) { fakeHold(x, y); tick(25); }
	fakeRelease();
	soFrames(2);
	return jobSt().running;
}

/** @brief Drag the page's row @p i by @p dx px and lift: down, one move, up. */
static void drag(int i, int dx) {
	int x, y;
	ctlCentre(UI_FIX((uint8_t)SbusOutCtl::Row0 + i), &x, &y);
	fakePress(x, y);
	tick(25);
	fakeHold(x + dx, y);
	tick(25);
	fakeRelease();
	tick(25);
}

/** @brief The frame last loaded on the line. */
static sbus_frame_t sent() { return jobSt().sent; }

/** @brief BACK, the leave page released, RESTART DSHOT after its wait: MENU, Free. */
static bool leaveToMenu() {
	if (uiCurrentScreen() != UiScreenId::SbusOut) return false;
	if (ctlEnabled(UI_FIX(UI_BACK))) tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 80 && !ctlEnabled(UI_FIX(SbusOutCtl::Restart)); k++) soFrames(1);
	tapId(UI_FIX(SbusOutCtl::Restart));
	soFrames(4);
	return uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Free;
}

/** @brief The entry, the claim, the probe and the gate, SAFE, START at 1000 ms. */
static void testEntryProbeStart() {
	section("S.BUS OUTPUT: OUTPUT opens it; the probe; START only through the gate");
	freshUi();
	openOutput();
	checkTrue("OUTPUT opens S.BUS OUTPUT, whose entry point claimed GP0's role",
	          uiCurrentScreen() == UiScreenId::SbusOut && pinClaimHolder() == PinUser::SbusOut);
	// A frame with no pump pass: the claim stays Stopping.
	fakeAdvance(25);
	core1JobPoll();
	uiTick();
	checkTrue("the title S.BUS OUTPUT, 142 px, 20 px before BACK",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, SBOUT_UI_TITLE, C_TEXT, C_PANEL, 2) &&
	          gfxTextW(SBOUT_UI_TITLE, 2) == 142 && UI_MARGIN + 142 + 20 == UI_BACK_X);
	checkTrue("the caption SIG GP0  100 KBAUD  8E2, dim, 137 px",
	          leftIs(UI_STRIP_Y + 4, "SIG GP0  100 KBAUD  8E2", C_DIM) &&
	          gfxTextW("SIG GP0  100 KBAUD  8E2", 1) == 137);
	checkTrue("the drain: RELEASING SIG GP0, dim, no chip",
	          pinClaimState() == ClaimState::Stopping && messageIs("RELEASING SIG GP0", nullptr, nullptr, C_DIM) &&
	          noChip());
	soFrames(12);
	checkTrue("the claim asked for SIG alone: SIG is GP0",
	          pinClaimGpio(PIN_SIG) == 0 && pinClaimGpio(PIN_AUX) == PIN_CLAIM_NO_GPIO);
	checkTrue("the probe: CHECKING GP0 FOR AN ESC., dim, 143 px, no chip",
	          sbusOutProbePoll() == SbusOutProbeState::Checking &&
	          messageIs(SBOUT_UI_CHECKING, nullptr, nullptr, C_DIM) && gfxTextW(SBOUT_UI_CHECKING, 1) == 143 &&
	          noChip());
	checkTrue("no rows and no notes: the notes' lines blank",
	          allOf(0, SBOUT_UI_NOTE1_Y, GFX_W, SBOUT_UI_ROW_Y - SBOUT_UI_NOTE1_Y, C_BG) &&
	          !ctlPresent(UI_FIX(SbusOutCtl::Row0)));
	checkTrue("HOLD TO START drawn, disabled: the gate is shut",
	          ctlPresent(UI_FIX(SbusOutCtl::Start)) && !ctlEnabled(UI_FIX(SbusOutCtl::Start)) &&
	          fakeTextIs(6 + (110 - 77) / 2, SBOUT_UI_BOT_Y + 12, "HOLD TO START", C_GRID, C_BG, 1));
	checkControlTable("S.BUS OUTPUT, the probe");
	fakeDumpFrame("shot_sbus_out_checking.ppm");

	// A finger on START through the rest of the probe and 1100 ms after.
	const uint16_t seq0 = held().seq;
	int x, y;
	ctlCentre(UI_FIX(SbusOutCtl::Start), &x, &y);
	fakePress(x, y);
	tick(25);
	bool bar = false, sentAny = false;
	int k = 0;
	for (; k < 120 && !sbusOutProbePassed(); k++) {
		fakeHold(x, y);
		tick(25);
		bar |= pixel(7, SBOUT_UI_BAR_Y + 1) != C_BG;
		sentAny |= held().seq != seq0;
	}
	checkTrue("a press on START during the probe: no progress bar, no command",
	          k > 30 && !bar && !sentAny && !jobSt().running);
	for (int j = 0; j < 44; j++) { fakeHold(x, y); tick(25); }
	checkTrue("the probe passed under it: the same press starts nothing, 1100 ms on",
	          sbusOutProbePassed() && held().seq == seq0 && !jobSt().running);
	fakeRelease();
	soFrames(1);

	checkTrue("SAFE: the chip on green", chipIs("SAFE", UI_COL_SAFE));
	checkTrue("the caption ends 34 px before RUNNING's chip",
	          uiChipX("RUNNING") - (UI_MARGIN + 137) == 34 && uiChipX("RUNNING") == 179);
	checkTrue("note 1 at y 62, amber: GP0 GOES TO A FLIGHT CONTROLLER.",
	          leftIs(SBOUT_UI_NOTE1_Y, "GP0 GOES TO A FLIGHT CONTROLLER.", UI_COL_WARN));
	checkTrue("note 2 at y 70, amber: NEVER TO AN ESC. PROPS OFF.",
	          leftIs(SBOUT_UI_NOTE2_Y, "NEVER TO AN ESC. PROPS OFF.", UI_COL_WARN));
	checkTrue("the rows: the START frame, dim; CH3 THR amber",
	          rowIs(0, "CH1", C_DIM, "1500", C_DIM) && rowIs(1, "CH2", C_DIM, "1500", C_DIM) &&
	          rowIs(2, "CH3 THR", UI_COL_WARN, "1000", C_DIM) && rowIs(3, "CH4", C_DIM, "1500", C_DIM) &&
	          rowIs(4, "CH5", C_DIM, "1000", C_DIM) && rowIs(7, "CH8", C_DIM, "1000", C_DIM));
	bool rowsOff = true;
	for (int i = 0; i < SBOUT_UI_ROWS; i++) {
		UiRect r = ctlRect(UI_FIX((uint8_t)SbusOutCtl::Row0 + i));
		rowsOff = rowsOff && !ctlEnabled(UI_FIX((uint8_t)SbusOutCtl::Row0 + i)) && r.x == 54 &&
		          r.y == 80 + 25 * i && r.w == 146 && r.h == 22;
	}
	checkTrue("eight rows of 22 px at a 25 px pitch, disabled while SAFE", rowsOff);
	checkTrue("the ceiling's mark at x 83 on the throttle row, amber",
	          pixel(83, rowTop(2)) == UI_COL_WARN && pixel(83, rowTop(2) + 21) == UI_COL_WARN &&
	          pixel(82, rowTop(2)) == C_BG && pixel(83, rowTop(1)) != UI_COL_WARN);
	checkTrue("the track past it dim", pixel(150, rowTop(2) + SBOUT_UI_TRACK_DY + 4) == C_DIM &&
	          pixel(150, rowTop(1) + SBOUT_UI_TRACK_DY + 4) != C_DIM);
	checkTrue("HOLD TO START acts; CH 9-16 and SET",
	          ctlEnabled(UI_FIX(SbusOutCtl::Start)) && ctlEnabled(UI_FIX(SbusOutCtl::Page)) &&
	          ctlEnabled(UI_FIX(SbusOutCtl::Set)));
	{
		const UiRect s = ctlRect(UI_FIX(SbusOutCtl::Start)), p = ctlRect(UI_FIX(SbusOutCtl::Page)),
		             t = ctlRect(UI_FIX(SbusOutCtl::Set));
		checkTrue("the bottom row: 6/110, 120/55, 179/55 at y 285, 32 px",
		          s.x == 6 && s.w == 110 && p.x == 120 && p.w == 55 && t.x == 179 && t.w == 55 &&
		          s.y == 285 && s.h == 32 && t.y == 285);
		checkTrue("their labels: HOLD TO START, CH 9-16, SET",
		          fakeTextIs(s.x + (110 - 77) / 2, s.y + 12, "HOLD TO START", C_TEXT, C_PANEL, 1) &&
		          fakeTextIs(p.x + (55 - 41) / 2, p.y + 12, "CH 9-16", C_TEXT, C_PANEL, 1) &&
		          fakeTextIs(t.x + (55 - 17) / 2, t.y + 12, "SET", C_TEXT, C_PANEL, 1));
	}
	{
		int x0 = 0, x1 = 0;
		checkTrue("everything under the strip from x 0 to x 233",
		          fakeInkExtent(UI_BODY_Y, GFX_H - UI_BODY_Y, C_BG, &x0, &x1) && x1 <= 233);
		checkTrue("the note band from x 8 to x 231",
		          fakeInkExtent(SBOUT_UI_NOTE1_Y, 16, C_BG, &x0, &x1) && x0 == UI_MARGIN && x1 <= 231);
	}
	checkControlTable("S.BUS OUTPUT, SAFE");
	fakeDumpFrame("shot_sbus_out_safe.ppm");

	// A drag while SAFE moves nothing and sends nothing.
	drag(0, 60);
	checkTrue("a drag on CH1 while SAFE: nothing moves, nothing is sent",
	          held().seq == seq0 && rowIs(0, "CH1", C_DIM, "1500", C_DIM));

	// HOLD TO START: 999 ms do not start it, 1000 ms do.
	ctlCentre(UI_FIX(SbusOutCtl::Start), &x, &y);
	fakePress(x, y);
	tick(25);
	checkTrue("the press: the hold runs, nothing sent yet", held().seq == seq0);
	fakeHold(x, y);
	tick(999);
	checkTrue("999 ms: no START", held().seq == seq0 && !jobSt().running);
	fakeHold(x, y);
	tick(1);
	SbusOutCmd c = held();
	checkTrue("1000 ms: START, the throttle on channel 3, MAX THROTTLE's ceiling, raw 499",
	          c.seq != seq0 && c.kind == SbusOutCmdKind::Start && c.thrCh == 2 && c.ceilRaw == 499);
	fakeRelease();
	soFrames(2);
	SbusOutStatus st = jobSt();
	sbus_frame_t start;
	sbusOutStartFrame(&start, 2);
	checkTrue("frames: the START frame on the line", st.running && st.frames > 0 &&
	          memcmp(&st.sent, &start, sizeof(start)) == 0);
	checkTrue("RUNNING on red", chipIs("RUNNING", UI_COL_ON));
	checkTrue("note 1: THROTTLE CH3: 1000 TO 1200 US (20%).",
	          leftIs(SBOUT_UI_NOTE1_Y, "THROTTLE CH3: 1000 TO 1200 US (20%).", C_TEXT));
	checkTrue("note 2: the flags, all off", leftIs(SBOUT_UI_NOTE2_Y, "CH17 OFF  CH18 OFF  FS OFF  LOST OFF", C_TEXT));
	checkTrue("the values live, lime", rowIs(0, "CH1", C_DIM, "1500", UI_COL_LIVE) &&
	          rowIs(2, "CH3 THR", UI_COL_WARN, "1000", UI_COL_LIVE));
	checkTrue("STOP at scale 2 on red; the rows act",
	          fakeTextIs(6 + (110 - 46) / 2, SBOUT_UI_BOT_Y + 9, "STOP", C_ONACCENT, UI_COL_ON, 2) &&
	          ctlEnabled(UI_FIX(SbusOutCtl::Row0)) && ctlEnabled(UI_FIX(SbusOutCtl::Start)));
	checkControlTable("S.BUS OUTPUT, RUNNING");
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

/** @brief The sliders, MAX THROTTLE on the screen, the page, STOP. */
static void testSlidersCeiling() {
	section("S.BUS OUTPUT: the sliders, the throttle under MAX THROTTLE, the page, STOP");
	freshUi();
	openOutput();
	checkTrue("SAFE", toSafe());
	checkTrue("START", holdStart());

	// A press alone moves nothing: delta drag, tap-to-set off.
	int x, y;
	ctlCentre(UI_FIX((uint8_t)SbusOutCtl::Row0 + 0), &x, &y);
	const uint16_t seq0 = held().seq;
	fakePress(x + 60, y);
	tick(25);
	fakeRelease();
	soFrames(2);
	checkTrue("a press on CH1's track far right of its knob: nothing sent", held().seq == seq0 &&
	          sent().channel[0] == 992);
	drag(0, 10);
	soFrames(2);
	checkTrue("a drag of 10 px on CH1: 1570 us, raw sbus_from_us(1570)",
	          sent().channel[0] == sbus_from_us(1570) && rowIs(0, "CH1", C_DIM, "1570", UI_COL_LIVE) &&
	          held().kind == SbusOutCmdKind::Frame);
	drag(0, -300);
	soFrames(2);
	checkTrue("past the left end: 1000 us, raw 172", sent().channel[0] == 172);

	// The throttle row stops at the ceiling's mark: the screen offers no more.
	drag(2, 200);
	soFrames(2);
	SbusOutCmd c = held();
	checkTrue("the throttle dragged 200 px: the screen sends raw 499, MAX THROTTLE's ceiling",
	          c.kind == SbusOutCmdKind::Frame && c.channel[2] == 499 && sent().channel[2] == 499);
	checkTrue("it reads 1200, the knob at the mark",
	          rowIs(2, "CH3 THR", UI_COL_WARN, "1200", UI_COL_LIVE) && pixel(83, rowTop(2)) == UI_COL_WARN);
	checkTrue("the knob from x 76, over the mark's middle, which shows above and below it",
	          pixel(75, rowTop(2) + 11) != C_TEXT && pixel(77, rowTop(2) + 11) == C_TEXT &&
	          pixel(83, rowTop(2) + 11) == C_TEXT && pixel(83, rowTop(2) + 1) == UI_COL_WARN &&
	          pixel(83, rowTop(2) + 20) == UI_COL_WARN);
	ctlCentre(UI_FIX((uint8_t)SbusOutCtl::Row0 + 2), &x, &y);
	fakePress(x, y);
	tick(25);
	bool over = false;
	for (int dx = 10; dx <= 150; dx += 10) {
		fakeHold(x + dx, y);
		tick(25);
		over |= held().channel[2] > 499;
	}
	checkTrue("a long drag past it, frame by frame: never above raw 499", !over);
	fakeHold(x + 60, y);
	tick(25);
	fakeRelease();
	soFrames(2);
	checkTrue("back from 150 px to 60 px past the press: still at the ceiling",
	          sent().channel[2] == 499);
	drag(2, -20);
	soFrames(2);
	checkTrue("released at the ceiling, a drag of -20 px from there: below it",
	          sent().channel[2] < 499 && sent().channel[2] > 172);
	fakeDumpFrame("shot_sbus_out_running.ppm");

	// The page: CH 9-16.
	tapId(UI_FIX(SbusOutCtl::Page));
	checkTrue("CH 9-16: the second page, CH9 to CH16, the button CH 1-8",
	          rowIs(0, "CH9", C_DIM, "1000", UI_COL_LIVE) && rowIs(7, "CH16", C_DIM, "1000", UI_COL_LIVE) &&
	          fakeTextIs(120 + (55 - 35) / 2, SBOUT_UI_BOT_Y + 12, "CH 1-8", C_TEXT, C_PANEL, 1));
	drag(0, 20);
	soFrames(2);
	checkTrue("a drag on CH9 moves channel 9", sent().channel[8] > 172 && sent().channel[0] == 172);
	checkControlTable("S.BUS OUTPUT, page 2");
	tapId(UI_FIX(SbusOutCtl::Page));
	checkTrue("CH 1-8 again", rowIs(0, "CH1", C_DIM, "1000", UI_COL_LIVE));

	// STOP acts on the press.
	ctlCentre(UI_FIX(SbusOutCtl::Start), &x, &y);
	fakePress(x, y);
	tick(25);
	checkTrue("STOP on the press: a Safe", held().kind == SbusOutCmdKind::Safe);
	soFrames(1);
	const int tx = fakeUartTxCount();
	soFrames(2);
	checkTrue("frames stop at once", !jobSt().running && fakeUartTxCount() == tx);
	for (int k = 0; k < 44; k++) { fakeHold(x, y); tick(25); }
	checkTrue("the press that stopped them starts nothing", !jobSt().running);
	fakeRelease();
	soFrames(1);
	checkTrue("SAFE again: the warnings, the last values dim",
	          chipIs("SAFE", UI_COL_SAFE) &&
	          leftIs(SBOUT_UI_NOTE1_Y, "GP0 GOES TO A FLIGHT CONTROLLER.", UI_COL_WARN) &&
	          leftIs(SBOUT_UI_NOTE2_Y, "NEVER TO AN ESC. PROPS OFF.", UI_COL_WARN) &&
	          rowIs(0, "CH1", C_DIM, "1000", C_DIM) && !ctlEnabled(UI_FIX(SbusOutCtl::Row0)));
	SbusOutStatus st = jobSt();
	checkTrue("the throttle's last value kept, dim", st.sent.channel[2] < 499 && st.sent.channel[2] > 172);
	checkTrue("START again: the START frame, whatever the sliders showed",
	          holdStart() && sent().channel[0] == 992 && sent().channel[2] == 172 &&
	          sent().channel[8] == 172 && rowIs(2, "CH3 THR", UI_COL_WARN, "1000", UI_COL_LIVE));
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));

	// MAX THROTTLE at 1000 of 2000: raw 991, 1500 us, the mark at x 126.
	freshUi();
	settings()->maxThrottle = 1000;
	openOutput();
	checkTrue("SAFE at MAX THROTTLE 1000", toSafe());
	checkTrue("the mark at x 126", pixel(126, rowTop(2)) == UI_COL_WARN);
	checkTrue("START", holdStart());
	checkTrue("its ceiling: raw 991, 50 %", held().ceilRaw == 991 &&
	          leftIs(SBOUT_UI_NOTE1_Y, "THROTTLE CH3: 1000 TO 1500 US (50%).", C_TEXT));
	drag(2, 200);
	soFrames(2);
	checkTrue("dragged far: raw 991, 1500 us", sent().channel[2] == 991 && held().channel[2] == 991 &&
	          rowIs(2, "CH3 THR", UI_COL_WARN, "1500", UI_COL_LIVE));
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

/** @brief The heartbeat: frames stop when the UI stops; STALLED; a new START. */
static void testHeartbeat() {
	section("S.BUS OUTPUT: the heartbeat stops the frames; STALLED and a new START");
	freshUi();
	openOutput();
	checkTrue("SAFE", toSafe());
	checkTrue("START", holdStart());
	drag(2, 20);
	soFrames(2);
	stall(249);
	checkTrue("249 ms with no UI frame: frames still go out", jobSt().running);
	stall(30);
	const int tx = fakeUartTxCount();
	stall(100);
	SbusOutStatus st = jobSt();
	checkTrue("past 250 ms: the trip, no frame more", !st.running && st.tripped && fakeUartTxCount() == tx);
	soFrames(1);
	checkTrue("the chip: STALLED on magenta", chipIs("STALLED", UI_COL_ALERT));
	checkTrue("STOPPED: SCREEN STALLED, magenta; HOLD TO START AGAIN., dim",
	          leftIs(SBOUT_UI_NOTE1_Y, "STOPPED: SCREEN STALLED", UI_COL_ALERT) &&
	          leftIs(SBOUT_UI_NOTE2_Y, "HOLD TO START AGAIN.", C_DIM));
	checkTrue("the rows disabled, the last values dim",
	          !ctlEnabled(UI_FIX(SbusOutCtl::Row0)) && rowIs(0, "CH1", C_DIM, "1500", C_DIM) &&
	          sent().channel[2] > 172);
	checkTrue("HOLD TO START acts", ctlEnabled(UI_FIX(SbusOutCtl::Start)));
	soFrames(20);
	checkTrue("frames do not come back by themselves", !jobSt().running && fakeUartTxCount() == tx);
	fakeDumpFrame("shot_sbus_out_stalled.ppm");
	checkTrue("a new HOLD TO START: frames, RUNNING, the trip gone",
	          holdStart() && !jobSt().tripped && chipIs("RUNNING", UI_COL_ON) && sent().channel[2] == 172);
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

/**
 * @brief The idle rule: more than 30 s with no touch while the frame is off
 *        its START values, and never on them.
 */
static void testIdle() {
	section("S.BUS OUTPUT: the idle rule at 30,000 and 30,001 ms");

	/** @brief Run frames of 200 ms up to @p untilMs, then one to the exact millisecond. */
	auto runTo = [](uint32_t untilMs) {
		while (millis() + 200 < untilMs) tick(200);
		if (millis() < untilMs) tick(untilMs - millis());
	};
	/** @brief Lift the finger after a touch at the frame of @p at; the last touch is then. */
	freshUi();
	openOutput();
	checkTrue("SAFE", toSafe());
	checkTrue("START", holdStart());
	uint32_t t0 = millis();
	runTo(t0 + 40000);
	checkTrue("the START frame, 40 s without touch: frames go on", jobSt().running);

	// The throttle above raw 172.
	int x, y;
	ctlCentre(UI_FIX((uint8_t)SbusOutCtl::Row0 + 2), &x, &y);
	fakePress(x, y);
	tick(25);
	fakeHold(x + 10, y);
	tick(25);
	const uint32_t touch = millis();
	fakeRelease();
	tick(25);
	soFrames(1);
	checkTrue("the throttle above its stop", sent().channel[2] > 172);
	runTo(touch + 30000);
	checkTrue("30,000 ms after the last touch: frames go on", jobSt().running &&
	          held().kind == SbusOutCmdKind::Frame);
	tick(1);
	checkTrue("30,001 ms: STOP", held().kind == SbusOutCmdKind::Safe);
	soFrames(1);
	checkTrue("frames stopped; the chip SAFE", !jobSt().running && chipIs("SAFE", UI_COL_SAFE));
	checkTrue("STOPPED: NO TOUCH FOR 30 S., amber; HOLD TO START AGAIN., dim",
	          leftIs(SBOUT_UI_NOTE1_Y, "STOPPED: NO TOUCH FOR 30 S.", UI_COL_WARN) &&
	          leftIs(SBOUT_UI_NOTE2_Y, "HOLD TO START AGAIN.", C_DIM));
	checkTrue("a new START clears it", holdStart() &&
	          leftIs(SBOUT_UI_NOTE1_Y, "THROTTLE CH3: 1000 TO 1200 US (20%).", C_TEXT));

	// Channel 9 off its START value.
	tapId(UI_FIX(SbusOutCtl::Page));
	ctlCentre(UI_FIX((uint8_t)SbusOutCtl::Row0 + 0), &x, &y);
	fakePress(x, y);
	tick(25);
	fakeHold(x + 10, y);
	tick(25);
	const uint32_t touch9 = millis();
	fakeRelease();
	tick(25);
	soFrames(1);
	checkTrue("channel 9 above raw 172, the throttle at its stop",
	          sent().channel[8] > 172 && sent().channel[2] == 172);
	runTo(touch9 + 30000);
	checkTrue("channel 9: frames at 30,000 ms", jobSt().running);
	tick(1);
	soFrames(1);
	checkTrue("and stopped at 30,001 ms", !jobSt().running &&
	          leftIs(SBOUT_UI_NOTE1_Y, "STOPPED: NO TOUCH FOR 30 S.", UI_COL_WARN));
	tapId(UI_FIX(SbusOutCtl::Page));

	// CH17 on, from SET.
	checkTrue("START", holdStart());
	tapId(UI_FIX(SbusOutCtl::Set));
	tapId(UI_ROW(1, Row));
	tapId(UI_FIX(SbusOutCtl::EditPlus));
	const uint32_t touch17 = millis() - 25;
	soFrames(1);
	checkTrue("CH17 on", sent().ch17);
	runTo(touch17 + 30000);
	checkTrue("CH17: frames at 30,000 ms", jobSt().running);
	tick(1);
	soFrames(1);
	checkTrue("stopped at 30,001 ms, on SET too", !jobSt().running);
	tapId(UI_FIX(UI_BACK));
	checkTrue("the main view: the idle stop's note",
	          leftIs(SBOUT_UI_NOTE1_Y, "STOPPED: NO TOUCH FOR 30 S.", UI_COL_WARN));
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

/** @brief SET: the rows, their locks, the flags in the frame, STOP, the footer's warning. */
static void testSet() {
	section("S.BUS OUTPUT: SET, its locks, the flags, STOP and the footer's warning");
	freshUi();
	openOutput();
	checkTrue("SAFE", toSafe());
	tapId(UI_FIX(SbusOutCtl::Set));
	checkTrue("SET: OUTPUT SET, 118 px",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, SBOUT_UI_SET_TITLE, C_TEXT, C_PANEL, 2) &&
	          gfxTextW(SBOUT_UI_SET_TITLE, 2) == 118);
	checkTrue("its note while SAFE: START TO SET THE FLAGS., amber",
	          leftIs(UI_STRIP_Y + 2, SBOUT_UI_SET_START, UI_COL_WARN));
	checkTrue("the footer: NEVER TO AN ESC. PROPS OFF. at (8, 287), amber, no STOP",
	          leftIs(SBOUT_UI_FOOT_TEXT_Y, "NEVER TO AN ESC. PROPS OFF.", UI_COL_WARN) &&
	          !ctlPresent(UI_FIX(SbusOutCtl::Stop)) && allOf(170, UI_FOOT_Y, 64, UI_FOOT_H, C_BG));
	checkTrue("the rows from y 62: CHANNEL first, CH3 at scale 2 right-aligned to x 231",
	          ctlPresent(UI_ROW(0, Row)) && ctlRect(UI_ROW(0, Row)).y == UI_BODY_Y &&
	          fakeTextIs(232 - gfxTextW("CH3", 2), UI_BODY_Y + 13, "CH3", C_TEXT, C_BG, 2));
	checkControlTable("OUTPUT SET, SAFE");
	fakeDumpFrame("shot_sbus_out_set.ppm");
	tapId(UI_ROW(1, Row));
	const uint16_t seqSet = held().seq;
	checkTrue("CH17 while SAFE: - and + do not act",
	          ctlPresent(UI_FIX(SbusOutCtl::EditPlus)) && !ctlEnabled(UI_FIX(SbusOutCtl::EditPlus)) &&
	          !ctlEnabled(UI_FIX(SbusOutCtl::EditMinus)));
	tapId(UI_FIX(SbusOutCtl::EditPlus), IdExpect::Disabled);
	checkTrue("a tap on + while SAFE: nothing sent, CH17 reads OFF", held().seq == seqSet &&
	          fakeTextIs(232 - gfxTextW("OFF", 2), UI_BODY_Y + 42 + 13, "OFF", C_GRID, C_PANEL, 2));
	tapId(UI_ROW(0, Row));
	checkTrue("CHANNEL while SAFE: - and + act from CH3",
	          ctlEnabled(UI_FIX(SbusOutCtl::EditPlus)) && ctlEnabled(UI_FIX(SbusOutCtl::EditMinus)));
	tapId(UI_FIX(SbusOutCtl::EditMinus));
	tapId(UI_FIX(SbusOutCtl::EditMinus));
	checkTrue("CH1: - stops there", !ctlEnabled(UI_FIX(SbusOutCtl::EditMinus)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the main view, CH1 THR on row 1 with the ceiling's mark",
	          rowIs(0, "CH1 THR", UI_COL_WARN, "1000", C_DIM) && rowIs(2, "CH3", C_DIM, "1500", C_DIM) &&
	          pixel(83, rowTop(0)) == UI_COL_WARN && pixel(83, rowTop(2)) != UI_COL_WARN);
	checkTrue("START", holdStart());
	SbusOutStatus st = jobSt();
	checkTrue("START took channel 1: raw 172 there, 992 on channel 3",
	          st.sent.channel[0] == 172 && st.sent.channel[2] == 992);
	drag(0, 200);
	soFrames(2);
	checkTrue("channel 1 held at the ceiling", sent().channel[0] == 499 && sent().channel[2] == 992);
	tapId(UI_FIX(SbusOutCtl::Set));
	checkTrue("its note while RUNNING: STOP TO CHANGE THE CHANNEL.",
	          leftIs(UI_STRIP_Y + 2, SBOUT_UI_SET_STOP, UI_COL_WARN));
	tapId(UI_ROW(0, Row));
	checkTrue("CHANNEL while RUNNING: locked",
	          !ctlEnabled(UI_FIX(SbusOutCtl::EditPlus)) && !ctlEnabled(UI_FIX(SbusOutCtl::EditMinus)));
	const struct { int row; const char *what; } FL[] = {
		{ 1, "CH17" }, { 2, "CH18" }, { 3, "FAILSAFE BIT" }, { 4, "FRAME LOST BIT" },
	};
	for (const auto &f : FL) {
		revealRow(f.row);
		tapId(UI_ROW(f.row, Row));
		tapId(UI_FIX(SbusOutCtl::EditPlus));
		soFrames(2);
		sbus_frame_t s = sent();
		bool on = f.row == 1 ? s.ch17 : f.row == 2 ? s.ch18 : f.row == 3 ? s.failsafe : s.frame_lost;
		char what[64];
		snprintf(what, sizeof(what), "%s ON while RUNNING: in the frame; + stops", f.what);
		checkTrue(what, on && !ctlEnabled(UI_FIX(SbusOutCtl::EditPlus)));
	}
	checkTrue("STOP in the footer's right button, 162/72", ctlEnabled(UI_FIX(SbusOutCtl::Stop)) &&
	          ctlRect(UI_FIX(SbusOutCtl::Stop)).x == 162 && ctlRect(UI_FIX(SbusOutCtl::Stop)).w == 72);
	checkControlTable("OUTPUT SET, RUNNING");
	tapId(UI_ROW(4, Row));
	tapId(UI_FIX(SbusOutCtl::EditMinus));
	soFrames(2);
	checkTrue("FRAME LOST BIT back off", !sent().frame_lost && sent().failsafe);
	tapId(UI_FIX(UI_BACK));
	checkTrue("the main view: note 2 with the flags on, amber",
	          leftIs(SBOUT_UI_NOTE2_Y, "CH17 ON  CH18 ON  FS ON  LOST OFF", UI_COL_WARN));
	tapId(UI_FIX(SbusOutCtl::Set));
	int x, y;
	ctlCentre(UI_FIX(SbusOutCtl::Stop), &x, &y);
	fakePress(x, y);
	tick(25);
	checkTrue("STOP on SET acts on the press", held().kind == SbusOutCmdKind::Safe);
	soFrames(2);
	fakeRelease();
	soFrames(1);
	checkTrue("frames stopped; the footer's warning back",
	          !jobSt().running && leftIs(SBOUT_UI_FOOT_TEXT_Y, "NEVER TO AN ESC. PROPS OFF.", UI_COL_WARN));
	tapId(UI_FIX(UI_BACK));
	checkTrue("START", holdStart());
	checkTrue("the START frame clears the flags", !sent().ch17 && !sent().ch18 && !sent().failsafe);
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

/** @brief BACK: the leave page; BACK TO OUTPUT a new visit and probe; RESTART DSHOT. */
static void testLeave() {
	section("S.BUS OUTPUT: the leave page, BACK TO OUTPUT, RESTART DSHOT");
	freshUi();
	openOutput();
	checkTrue("SAFE", toSafe());
	checkTrue("START", holdStart());
	stall(300);
	soFrames(1);
	checkTrue("stalled", jobSt().tripped);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the leave page, opened releasing", uiCurrentScreen() == UiScreenId::SbusOut &&
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled &&
	          ctlPresent(UI_FIX(SbusOutCtl::BackToOutput)));
	soFrames(1);
	checkTrue("GP0 let go: no frames, the UART off", pinClaimUnplugDue() && !jobSt().running &&
	          !fakeUart().enabled);
	checkTrue("step 1, current: UNPLUG THE FLIGHT CONTROLLER., DSHOT RESTARTS ON SIG GP0.",
	          leaveStepIs(66, '1', SBOUT_UI_UNPLUG, true) && leaveStepIs(78, 0, "DSHOT RESTARTS ON SIG GP0.", true));
	checkTrue("step 2: RESTART DSHOT.", leaveStepIs(92, '2', UI_LEAVE_RESTART, false));
	checkTrue("the strip: SIG GP0  AUX GP1", leftIs(UI_STRIP_Y + 4, "SIG GP0  AUX GP1", C_DIM));
	checkTrue("BACK TO OUTPUT acts, 83 px in its button; RESTART DSHOT waits",
	          ctlEnabled(UI_FIX(SbusOutCtl::BackToOutput)) && gfxTextW(SBOUT_UI_BACK_TO, 1) == 83 &&
	          leaveLeftIs(SBOUT_UI_BACK_TO, true) && !ctlEnabled(UI_FIX(SbusOutCtl::Restart)));
	checkControlTable("S.BUS OUTPUT, leave page");
	fakeDumpFrame("shot_sbus_out_leave.ppm");
	const int opens = fakeEwOpens();
	tapId(UI_FIX(SbusOutCtl::BackToOutput));
	soFrames(2);
	checkTrue("BACK TO OUTPUT: the claim kept, a new visit: a new probe, the gate shut",
	          pinClaimHolder() == PinUser::SbusOut && sbusOutProbePoll() == SbusOutProbeState::Checking &&
	          fakeEwOpens() == opens + 1 && !sbusOutProbePassed() &&
	          messageIs(SBOUT_UI_CHECKING, nullptr, nullptr, C_DIM) && !ctlEnabled(UI_FIX(SbusOutCtl::Start)));
	checkTrue("SAFE after it, the trip forgotten", toSafe() && chipIs("SAFE", UI_COL_SAFE) &&
	          leftIs(SBOUT_UI_NOTE1_Y, "GP0 GOES TO A FLIGHT CONTROLLER.", UI_COL_WARN));
	checkTrue("START", holdStart());
	tapId(UI_FIX(UI_BACK));
	soFrames(1);
	const uint32_t released = millis();
	checkTrue("BACK while RUNNING: frames stop, GP0 let go", pinClaimUnplugDue() && !jobSt().running);
	while (millis() - released < 975) soFrames(1);
	checkTrue("975 ms after: RESTART DSHOT still waits", !ctlEnabled(UI_FIX(SbusOutCtl::Restart)));
	soFrames(1);
	checkTrue("1000 ms: it acts", ctlEnabled(UI_FIX(SbusOutCtl::Restart)));
	int resumes = fakeResumeCount();
	tapId(UI_FIX(SbusOutCtl::Restart));
	checkTrue("RESTART DSHOT: MENU, DShot resumed with the job idle",
	          uiCurrentScreen() == UiScreenId::Menu && fakeResumeCount() == resumes + 1 && fakeResumeJobIdle());
	soFrames(4);
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));

	// BACK during the probe: the leave page too, GP0 let go on the pull-up.
	freshUi();
	openOutput();
	soFrames(14);
	checkTrue("the probe", sbusOutProbePoll() == SbusOutProbeState::Checking);
	tapId(UI_FIX(UI_BACK));
	soFrames(1);
	checkTrue("BACK in the probe: the leave page, GP0 on the pull-up, never the UART's",
	          ctlPresent(UI_FIX(SbusOutCtl::BackToOutput)) && pinClaimUnplugDue() &&
	          fakePin(0).pull == FakePull::Up && fakePin(0).function == GPIO_FUNC_SIO);
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

/** @brief A bootloader answers: START refused; after EFM8, decision 8's page. */
static void testBootloader() {
	section("S.BUS OUTPUT: a bootloader answers the probe; decision 8's leave page");
	freshUi();
	fakeEwReset(0xB2, true);
	fakeEwAnswerEach(true);
	openOutput();
	for (int k = 0; k < 80 && sbusOutProbePoll() != SbusOutProbeState::Answered; k++) soFrames(1);
	soFrames(1);
	checkTrue("an EFM8 bootloader answers", sbusOutProbePoll() == SbusOutProbeState::Answered &&
	          sbusOutProbeEfm8());
	checkTrue("the rows' area asks for the ESC's battery and lead to come off, amber",
	          messageIs(SBOUT_UI_BL_1, SBOUT_UI_BL_2, SBOUT_UI_BL_3, UI_COL_WARN) && noChip());
	checkTrue("197, 191 and 185 px", gfxTextW(SBOUT_UI_BL_1, 1) == 197 && gfxTextW(SBOUT_UI_BL_2, 1) == 191 &&
	          gfxTextW(SBOUT_UI_BL_3, 1) == 185);
	checkControlTable("S.BUS OUTPUT, a bootloader answers");
	fakeDumpFrame("shot_sbus_out_bootloader.ppm");
	const uint16_t seq0 = held().seq;
	int x, y;
	ctlCentre(UI_FIX(SbusOutCtl::Start), &x, &y);
	fakePress(x, y);
	tick(25);
	for (int k = 0; k < 100; k++) { fakeHold(x, y); tick(25); }
	fakeRelease();
	soFrames(1);
	checkTrue("2.5 s on START while it answers: no command, the gate shut, START disabled",
	          held().seq == seq0 && !sbusOutProbePassed() && !ctlEnabled(UI_FIX(SbusOutCtl::Start)) &&
	          messageIs(SBOUT_UI_BL_1, SBOUT_UI_BL_2, SBOUT_UI_BL_3, UI_COL_WARN));

	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: decision 8's page before LEAVE, the claim kept",
	          pinClaimState() == ClaimState::Running && ctlEnabled(UI_FIX(SbusOutCtl::Stay)) &&
	          ctlEnabled(UI_FIX(SbusOutCtl::Leave)) && !ctlEnabled(UI_FIX(UI_BACK)));
	checkTrue("step 1, current: ESC SETTINGS' three lines",
	          leaveStepIs(66, '1', AM32_UI_LEAVE_1, true) && leaveStepIs(78, 0, AM32_UI_LEAVE_1A, true) &&
	          leaveStepIs(90, 0, AM32_UI_LEAVE_1B, true));
	checkTrue("step 2: UNPLUG THE FLIGHT CONTROLLER., DSHOT RESTARTS ON SIG GP0.",
	          leaveStepIs(104, '2', SBOUT_UI_UNPLUG, false) &&
	          leaveStepIs(116, 0, "DSHOT RESTARTS ON SIG GP0.", false));
	checkTrue("step 3: RESTART DSHOT., THEN PLUG THE ESC BACK IN., ending at y 148",
	          leaveStepIs(130, '3', UI_LEAVE_RESTART, false) && leaveStepIs(142, 0, SBOUT_UI_PLUG_ESC, false) &&
	          allOf(0, 149, GFX_W, UI_LEAVE_NOTE_Y - 149, C_BG));
	checkTrue("STAY, and LEAVE at scale 2", leaveLeftIs("STAY", true) && leaveWideIs("LEAVE", true));
	checkControlTable("S.BUS OUTPUT, decision 8's page");
	fakeDumpFrame("shot_sbus_out_efm8_leave.ppm");
	tapId(UI_FIX(SbusOutCtl::Stay));
	checkTrue("STAY: the screen and the probe", ctlPresent(UI_FIX(SbusOutCtl::Start)) &&
	          sbusOutProbePoll() == SbusOutProbeState::Answered && pinClaimState() == ClaimState::Running);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(SbusOutCtl::Leave));
	soFrames(1);
	checkTrue("LEAVE: GP0 let go; step 2 current; BACK TO OUTPUT and RESTART DSHOT",
	          pinClaimUnplugDue() && leaveStepIs(104, '2', SBOUT_UI_UNPLUG, true) &&
	          leaveStepIs(66, '1', AM32_UI_LEAVE_1, false) && ctlEnabled(UI_FIX(SbusOutCtl::BackToOutput)) &&
	          ctlPresent(UI_FIX(SbusOutCtl::Restart)));
	checkTrue("DShot not back yet", pinClaimState() != ClaimState::Free && sbusOutRunCue(0));

	// Unplugged; BACK TO OUTPUT probes again and passes.
	fakeEwSim()->dead = true;
	tapId(UI_FIX(SbusOutCtl::BackToOutput));
	checkTrue("BACK TO OUTPUT: a new probe", toSafe() && chipIs("SAFE", UI_COL_SAFE));
	checkTrue("START", holdStart());
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK with the EFM8 mark kept: decision 8's page again, the frames stopped",
	          ctlPresent(UI_FIX(SbusOutCtl::Stay)) && held().kind == SbusOutCmdKind::Safe);
	soFrames(1);
	checkTrue("no frames while the page waits for LEAVE", !jobSt().running);
	tapId(UI_FIX(SbusOutCtl::Leave));
	for (int k = 0; k < 60 && !ctlEnabled(UI_FIX(SbusOutCtl::Restart)); k++) soFrames(1);
	tapId(UI_FIX(SbusOutCtl::Restart));
	soFrames(4);
	checkTrue("RESTART DSHOT: MENU, the mark and the cue gone",
	          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Free &&
	          !sbusOutProbeEfm8() && !sbusOutRunCue(0));
	tapId(UI_FIX(UI_BACK));

	// An AM32 bootloader: the same lines; BACK opens the plain page.
	freshUi();
	fakeEwResetAm32();
	fakeEwAnswerEach(true);
	openOutput();
	for (int k = 0; k < 80 && sbusOutProbePoll() != SbusOutProbeState::Answered; k++) soFrames(1);
	soFrames(1);
	checkTrue("an AM32 bootloader answers: the same lines, no EFM8 mark",
	          messageIs(SBOUT_UI_BL_1, SBOUT_UI_BL_2, SBOUT_UI_BL_3, UI_COL_WARN) && !sbusOutProbeEfm8() &&
	          !ctlEnabled(UI_FIX(SbusOutCtl::Start)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the plain leave page, opened releasing",
	          ctlPresent(UI_FIX(SbusOutCtl::BackToOutput)) && !ctlPresent(UI_FIX(SbusOutCtl::Stay)));
	soFrames(1);
	checkTrue("step 1: UNPLUG THE FLIGHT CONTROLLER.", leaveStepIs(66, '1', SBOUT_UI_UNPLUG, true));
	checkTrue("back to MENU", leaveToMenu());
	checkTrue("the AM32 cue kept by RESTART DSHOT", sbusOutRunCue(0));
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	checkTrue("gone after an eRPM frame", !sbusOutRunCue(t.lastRpmMs));
	noTelemetry();
	tapId(UI_FIX(UI_BACK));
	noEscOnGp0();
}

/** @brief The claim's messages: an ESC, KISS, no GP0, the clock; GP0 as AUX. */
static void testUnavailable() {
	section("S.BUS OUTPUT: the unavailable pin's messages, GP0 as AUX");

	// An ESC answers on SIG GP0.
	freshUi();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	openOutput();
	soFrames(1);
	checkTrue("an ESC answers: no claim", pinClaimState() == ClaimState::Free);
	checkTrue("AN ESC ANSWERS ON SIG GP0. UNPLUG IT: NO ESC STAYS ON THE BENCH WHILE S.BUS RUNS., amber",
	          messageIs("AN ESC ANSWERS ON SIG GP0.", SBOUT_UI_ESC_2, SBOUT_UI_ESC_3, UI_COL_WARN) && noChip());
	checkTrue("155, 155 and 161 px", gfxTextW("AN ESC ANSWERS ON SIG GP0.", 1) == 155 &&
	          gfxTextW(SBOUT_UI_ESC_2, 1) == 155 && gfxTextW(SBOUT_UI_ESC_3, 1) == 161);
	checkTrue("START disabled", !ctlEnabled(UI_FIX(SbusOutCtl::Start)));
	checkControlTable("S.BUS OUTPUT, an ESC answers");
	fakeDumpFrame("shot_sbus_out_esc.ppm");
	// The next frame's tick is 25 ms on.
	t.lastRpmMs = millis() + 25 - 499;
	fakeSetTelemetry(&t);
	soFrames(1);
	checkTrue("asked again every frame: 499 ms after the last eRPM frame, still refused",
	          pinClaimState() == ClaimState::Free);
	t.lastRpmMs = millis() + 25 - 500;
	fakeSetTelemetry(&t);
	soFrames(1);
	checkTrue("500 ms: claimed", pinClaimHolder() == PinUser::SbusOut);
	noTelemetry();
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 60 && uiCurrentScreen() == UiScreenId::SbusOut; k++) {
		soFrames(1);
		if (ctlEnabled(UI_FIX(SbusOutCtl::Restart))) tapId(UI_FIX(SbusOutCtl::Restart));
	}
	soFrames(4);
	checkTrue("back at MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));

	// GP0 is AUX, KISS live on it.
	freshUi();
	wiring(1, 0);
	memset(&t, 0, sizeof(t));
	t.kissLastMs = millis();
	fakeSetTelemetry(&t);
	openOutput();
	soFrames(1);
	checkTrue("KISS on AUX GP0: refused", pinClaimState() == ClaimState::Free);
	checkTrue("the caption: AUX GP0  100 KBAUD  8E2", leftIs(UI_STRIP_Y + 4, "AUX GP0  100 KBAUD  8E2", C_DIM));
	checkTrue("KISS WIRE ON AUX GP0 IS LIVE. UNPLUG IT. PLUG THE FLIGHT CONTROLLER IN ONCE SAFE SHOWS.",
	          messageIs(SBOUT_UI_KISS_1, SBOUT_UI_KISS_2, SBOUT_UI_KISS_3, UI_COL_WARN));
	checkTrue("173, 155 and 179 px", gfxTextW(SBOUT_UI_KISS_1, 1) == 173 && gfxTextW(SBOUT_UI_KISS_2, 1) == 155 &&
	          gfxTextW(SBOUT_UI_KISS_3, 1) == 179);
	fakeDumpFrame("shot_sbus_out_kiss.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK before a claim: MENU, nothing claimed",
	          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));

	// GP0 is AUX, an ESC answers on SIG GP1.
	freshUi();
	wiring(1, 0);
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	openOutput();
	soFrames(1);
	checkTrue("an ESC on SIG GP1: AN ESC ANSWERS ON SIG GP1.",
	          messageIs("AN ESC ANSWERS ON SIG GP1.", SBOUT_UI_ESC_2, SBOUT_UI_ESC_3, UI_COL_WARN));
	noTelemetry();
	soFrames(22);
	checkTrue("quiet: claimed as SIG and AUX, AUX GP0", pinClaimGpio(PIN_AUX) == 0 &&
	          pinClaimGpio(PIN_SIG) == 1);
	checkTrue("then SAFE, the caption AUX GP0", toSafe() && leftIs(UI_STRIP_Y + 4, "AUX GP0  100 KBAUD  8E2", C_DIM));
	tapId(UI_FIX(UI_BACK));
	soFrames(1);
	checkTrue("its leave page: SIG GP1  AUX GP0, DSHOT RESTARTS ON SIG GP1.",
	          leftIs(UI_STRIP_Y + 4, "SIG GP1  AUX GP0", C_DIM) &&
	          leaveStepIs(78, 0, "DSHOT RESTARTS ON SIG GP1.", true));
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));

	// No role on GP0.
	freshUi();
	wiring(1, 1);
	openOutput();
	soFrames(4);
	checkTrue("no role on GP0: no claim", pinClaimState() == ClaimState::Free &&
	          uiCurrentScreen() == UiScreenId::SbusOut);
	checkTrue("the caption: GP0  100 KBAUD  8E2", leftIs(UI_STRIP_Y + 4, "GP0  100 KBAUD  8E2", C_DIM));
	checkTrue("S.BUS NEEDS GP0. SET A PIN TO GP0 IN SETTINGS., amber",
	          messageIs(SBOUT_UI_NO_GP0_1, SBOUT_UI_NO_GP0_2, nullptr, UI_COL_WARN) &&
	          gfxTextW(SBOUT_UI_NO_GP0_2, 1) == 173);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));

	// The clock refused, RETRY.
	freshUi();
	fakeClockSet(clk_peri, 1000000u);
	openOutput();
	soFrames(14);
	checkTrue("the bind refused: CLOCK", pinClaimJobError() == CORE1_JOB_CLOCK);
	checkTrue("SIG GP0 COULD NOT BE TAKEN (CLOCK), amber, 203 px",
	          messageIs("SIG GP0 COULD NOT BE TAKEN (CLOCK)", nullptr, nullptr, UI_COL_WARN) &&
	          gfxTextW("SIG GP0 COULD NOT BE TAKEN (CLOCK)", 1) == 203);
	checkTrue("RETRY, the wide button; no bottom row",
	          ctlEnabled(UI_FIX(SbusOutCtl::Retry)) && ctlRect(UI_FIX(SbusOutCtl::Retry)).y == UI_FOOT_Y &&
	          ctlRect(UI_FIX(SbusOutCtl::Retry)).w == 228 && !ctlPresent(UI_FIX(SbusOutCtl::Start)) &&
	          !ctlPresent(UI_FIX(SbusOutCtl::Page)));
	checkControlTable("S.BUS OUTPUT, the clock refused");
	fakeClockSet(clk_peri, 150000000u);
	tapId(UI_FIX(SbusOutCtl::Retry));
	soFrames(2);
	checkTrue("RETRY at 150 MHz: the probe, RETRY gone, the bottom row back, the body clear under it",
	          sbusOutProbePoll() == SbusOutProbeState::Checking && !ctlPresent(UI_FIX(SbusOutCtl::Retry)) &&
	          ctlPresent(UI_FIX(SbusOutCtl::Start)) && allOf(0, UI_FOOT_Y, GFX_W, SBOUT_UI_BAR_Y - UI_FOOT_Y, C_BG));
	checkTrue("SAFE", toSafe());
	checkTrue("back to MENU", leaveToMenu());
	tapId(UI_FIX(UI_BACK));
}

void runSbusOutUiTests() {
	testViewChipNotes();
	testViewRows();
	testEntryProbeStart();
	testSlidersCeiling();
	testHeartbeat();
	testIdle();
	testSet();
	testLeave();
	testBootloader();
	testUnavailable();
	noEscOnGp0();
	noTelemetry();
}
