/**
 * @file test_sbus_ui.cpp
 * @brief S.BUS MONITOR and RX SIGNALS, its chooser: the formatter of
 *        sbus_view.h at its boundaries, and the screens through the real UI,
 *        the pin claim, the S.BUS job and the UART0 fake.
 *
 * A UI frame here is 25 ms of core1 passes every 500 us, then one pass of
 * the fake DShot pump and one uiTick(), so the job drains the receive FIFO
 * as it does on the chip, far more often than the 2000 us gap that frames
 * S.BUS. The line is the model of sbus_line.h. What the PL011 and the pad do
 * on the chip is not tested here; the inversion on the pin is not measured.
 */

#include "check.h"
#include "fakes.h"
#include "sbus_line.h"
#include "sbus_view.h"
#include "ui_sbus.h"
#include "ui_widgets.h"
#include "ui.h"
#include "gfx.h"
#include "sbus_job.h"
#include "settings.h"
#include "esc_task.h"
#include "esc_merge.h"
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

/** @brief A status with @p frames frames, the last at @p frameMs, bytes with it. */
static SbusStatus status(uint32_t frames, uint32_t frameMs) {
	SbusStatus st;
	memset(&st, 0, sizeof(st));
	st.frames = frames;
	st.lastFrameMs = frameMs;
	st.bytes = frames * 25u;
	st.lastByteMs = frameMs;
	return st;
}

/** @brief The link state of @p st at @p now with no frame-lost frame held. */
static SbusLink linkAt(const SbusStatus &st, uint32_t now) {
	SbusLostHold h;
	memset(&h, 0, sizeof(h));
	return sbusLink(&st, &h, now);
}

static void testLinkState() {
	section("S.BUS view: the link state at its boundaries");

	SbusStatus st = status(0, 0);
	checkTrue("nothing since the bind: NO SIGNAL", linkAt(st, 5) == SbusLink::NoSignal);
	st = status(10, 1000);
	checkTrue("a frame 0 ms ago: OK", linkAt(st, 1000) == SbusLink::Ok);
	checkTrue("99 ms after the last frame and byte: OK", linkAt(st, 1099) == SbusLink::Ok);
	checkTrue("100 ms: NO SIGNAL", linkAt(st, 1100) == SbusLink::NoSignal);
	st.lastByteMs = 1050;
	checkTrue("a clean byte 50 ms ago, the frame 100 ms: NO FRAMES", linkAt(st, 1100) == SbusLink::NoFrames);
	checkTrue("the byte 99 ms ago: still NO FRAMES", linkAt(st, 1149) == SbusLink::NoFrames);
	checkTrue("the byte 100 ms ago: NO SIGNAL", linkAt(st, 1150) == SbusLink::NoSignal);
	st = status(0, 0);
	st.bytes = 40;
	st.lastByteMs = 500;
	checkTrue("bytes and never a frame: NO FRAMES", linkAt(st, 520) == SbusLink::NoFrames);
	checkTrue("a stamp of 0 with a count of 0 is no byte", [] {
		SbusStatus s = status(0, 0);
		s.lastByteMs = 0;
		return linkAt(s, 50) == SbusLink::NoSignal;
	}());

	st = status(10, 1000);
	st.failsafe = true;
	checkTrue("the last frame with the failsafe bit: FAILSAFE", linkAt(st, 1099) == SbusLink::Failsafe);
	checkTrue("100 ms on: NO SIGNAL outranks it", linkAt(st, 1100) == SbusLink::NoSignal);

	// FRAME LOST: held 250 ms from the step that saw the count rise.
	SbusLostHold h;
	memset(&h, 0, sizeof(h));
	st = status(10, 2000);
	sbusLostHoldStep(&h, &st, 2000);
	checkTrue("no frame-lost frame: OK", sbusLink(&st, &h, 2000) == SbusLink::Ok);
	st.lostFrames = 1;
	sbusLostHoldStep(&h, &st, 2000);
	checkTrue("the count rises: FRAME LOST", sbusLink(&st, &h, 2000) == SbusLink::FrameLost);
	for (uint32_t t = 2000; t < 2250; t += 50) {
		st.lastFrameMs = st.lastByteMs = t;
		sbusLostHoldStep(&h, &st, t);
	}
	st.lastFrameMs = st.lastByteMs = 2249;
	sbusLostHoldStep(&h, &st, 2249);
	checkTrue("249 ms on, frames arriving: FRAME LOST", sbusLink(&st, &h, 2249) == SbusLink::FrameLost);
	st.lastFrameMs = st.lastByteMs = 2250;
	checkTrue("250 ms on: OK, before and after the step",
	          sbusLink(&st, &h, 2250) == SbusLink::Ok &&
	          (sbusLostHoldStep(&h, &st, 2250), sbusLink(&st, &h, 2250) == SbusLink::Ok && !h.held));
	st.failsafe = true;
	st.lostFrames = 2;
	sbusLostHoldStep(&h, &st, 2260);
	st.lastFrameMs = st.lastByteMs = 2260;
	checkTrue("FAILSAFE outranks FRAME LOST", sbusLink(&st, &h, 2260) == SbusLink::Failsafe);
	st.failsafe = false;
	st.lostFrames = 0;
	sbusLostHoldStep(&h, &st, 2600);
	st.lastFrameMs = st.lastByteMs = 2600;
	checkTrue("a cleared count, a new bind's, starts no hold",
	          !h.held && h.lostFrames == 0 && sbusLink(&st, &h, 2600) == SbusLink::Ok);
	st.lostFrames = 3;
	sbusLostHoldStep(&h, &st, 2700);
	st.lostFrames = 3;
	sbusLostHoldStep(&h, &st, 2900);
	checkTrue("the same count again restarts nothing: the hold runs from the rise",
	          h.held && h.atMs == 2700);

	// Across a wrap of millis().
	st = status(10, 0xFFFFFFF0u);
	checkTrue("a frame 16 ms before the wrap, read 10 ms after it: OK", linkAt(st, 10) == SbusLink::Ok);
	checkTrue("read 84 ms after the wrap, 100 ms on: NO SIGNAL", linkAt(st, 84) == SbusLink::NoSignal);
	checkTrue("83 ms after it: OK", linkAt(st, 83) == SbusLink::Ok);
	memset(&h, 0, sizeof(h));
	st.lostFrames = 1;
	sbusLostHoldStep(&h, &st, 0xFFFFFFF0u);
	st.lastFrameMs = st.lastByteMs = 230;
	checkTrue("FRAME LOST 249 ms after a rise before the wrap", sbusLink(&st, &h, 233) == SbusLink::FrameLost);
	checkTrue("and not 250 ms after it", sbusLink(&st, &h, 234) == SbusLink::Ok);

	checkTrue("the chip's texts", strcmp(sbusLinkText(SbusLink::NoSignal), "NO SIGNAL") == 0 &&
	          strcmp(sbusLinkText(SbusLink::NoFrames), "NO FRAMES") == 0 &&
	          strcmp(sbusLinkText(SbusLink::Failsafe), "FAILSAFE") == 0 &&
	          strcmp(sbusLinkText(SbusLink::FrameLost), "FRAME LOST") == 0 &&
	          strcmp(sbusLinkText(SbusLink::Ok), "OK") == 0);
	checkTrue("the chip's fills: amber, amber, red, amber, green",
	          sbusLinkFill(SbusLink::NoSignal) == C_AMBER && sbusLinkFill(SbusLink::NoFrames) == C_AMBER &&
	          sbusLinkFill(SbusLink::Failsafe) == C_RED && sbusLinkFill(SbusLink::FrameLost) == C_AMBER &&
	          sbusLinkFill(SbusLink::Ok) == C_GREEN);
	checkInt("FRAME LOST, the widest, 10 characters: 59 px", gfxTextW(SBUS_VIEW_FRAME_LOST, 1), 59);
}

static void testInkAndBars() {
	section("S.BUS view: how the channels are drawn, the bar in raw units");

	SbusStatus st = status(0, 0);
	checkTrue("no frame: Empty", sbusInk(&st, SbusLink::NoSignal) == SbusInk::Empty &&
	          sbusInk(&st, SbusLink::NoFrames) == SbusInk::Empty);
	st = status(3, 100);
	checkTrue("NO SIGNAL after frames: Stale", sbusInk(&st, SbusLink::NoSignal) == SbusInk::Stale);
	checkTrue("NO FRAMES after frames: Stale", sbusInk(&st, SbusLink::NoFrames) == SbusInk::Stale);
	checkTrue("FAILSAFE: Failsafe", sbusInk(&st, SbusLink::Failsafe) == SbusInk::Failsafe);
	checkTrue("FRAME LOST and OK: Live", sbusInk(&st, SbusLink::FrameLost) == SbusInk::Live &&
	          sbusInk(&st, SbusLink::Ok) == SbusInk::Live);

	checkInt("raw 0: no fill", sbusBarFill(0, 174), 0);
	checkInt("raw 172, 1000 us: 14 px", sbusBarFill(172, 174), 14);
	checkInt("raw 992, 1500 us: 84 px", sbusBarFill(992, 174), 84);
	checkInt("raw 1811, 2000 us: 153 px", sbusBarFill(1811, 174), 153);
	checkInt("raw 2046: 173 px", sbusBarFill(2046, 174), 173);
	checkInt("raw 2047: the whole 174 px", sbusBarFill(2047, 174), 174);
	checkInt("past 11 bits counts as 2047", sbusBarFill(4000, 174), 174);
}

static void testTexts() {
	section("S.BUS view: the value and the four lines");

	char b[48];
	SbusStatus st = status(0, 0);
	st.channel[0] = 992;
	sbusValueText(b, sizeof(b), &st, 0);
	checkTrue("before the first frame: ----", strcmp(b, "----") == 0);
	sbusDigitalText(b, sizeof(b), &st);
	checkTrue("CH17 --  CH18 --", strcmp(b, "CH17 --  CH18 --") == 0);
	sbusRateText(b, sizeof(b), &st);
	checkTrue("-- MS  -- HZ", strcmp(b, "-- MS  -- HZ") == 0);
	sbusFramesText(b, sizeof(b), &st);
	checkTrue("FRAMES 0", strcmp(b, "FRAMES 0") == 0);
	sbusLostText(b, sizeof(b), &st);
	checkTrue("LOST 0  FS 0", strcmp(b, "LOST 0  FS 0") == 0);
	sbusErrorText(b, sizeof(b), &st);
	checkTrue("PAR 0 FRM 0 BRK 0 OVR 0", strcmp(b, "PAR 0 FRM 0 BRK 0 OVR 0") == 0);
	sbusDecoderText(b, sizeof(b), &st);
	checkTrue("RESYNC 0 FOOTER 0", strcmp(b, "RESYNC 0 FOOTER 0") == 0);

	st = status(1, 0);
	const struct { uint16_t raw; const char *us; } V[] = {
		{ 992, "1500" }, { 172, "1000" }, { 1811, "2000" }, { 0, "895" }, { 2047, "2144" },
	};
	for (const auto &v : V) {
		st.channel[5] = v.raw;
		sbusValueText(b, sizeof(b), &st, 5);
		char what[64];
		snprintf(what, sizeof(what), "raw %u reads %s us (sbus_to_us())", v.raw, v.us);
		checkTrue(what, strcmp(b, v.us) == 0);
	}
	sbusValueText(b, sizeof(b), &st, 16);
	checkTrue("a channel past 16: ----", strcmp(b, "----") == 0);
	st.ch17 = true;
	sbusDigitalText(b, sizeof(b), &st);
	checkTrue("CH17 ON  CH18 OFF", strcmp(b, "CH17 ON  CH18 OFF") == 0);
	st.ch17 = false;
	st.ch18 = true;
	sbusDigitalText(b, sizeof(b), &st);
	checkTrue("CH17 OFF  CH18 ON", strcmp(b, "CH17 OFF  CH18 ON") == 0);

	const struct { uint32_t iv; const char *text; } R[] = {
		{ 14000, "14.0 MS  71.4 HZ" }, { 7000, "7.0 MS  142.9 HZ" }, { 13563, "13.6 MS  73.7 HZ" },
		{ 99999, "100.0 MS  10.0 HZ" }, { 1000, "1.0 MS  999.9 HZ" }, { 9049, "9.0 MS  110.5 HZ" },
		{ 9050, "9.1 MS  110.5 HZ" },
	};
	for (const auto &r : R) {
		st.intervalUs = r.iv;
		sbusRateText(b, sizeof(b), &st);
		char what[64];
		snprintf(what, sizeof(what), "%lu us: %s", (unsigned long)r.iv, r.text);
		checkTrue(what, strcmp(b, r.text) == 0);
	}

	st.frames = 999999;
	sbusFramesText(b, sizeof(b), &st);
	checkTrue("FRAMES 999999", strcmp(b, "FRAMES 999999") == 0);
	st.frames = 1000000;
	sbusFramesText(b, sizeof(b), &st);
	checkTrue("1,000,000 frames show 999999: 13 characters", strcmp(b, "FRAMES 999999") == 0);
	st.lostFrames = 9999;
	st.failsafeFrames = 10000;
	sbusLostText(b, sizeof(b), &st);
	checkTrue("LOST 9999  FS 9999: a count above 9999 shows 9999", strcmp(b, "LOST 9999  FS 9999") == 0);
	st.parityErr = 1; st.framingErr = 2; st.breakErr = 3; st.overrun = 4;
	sbusErrorText(b, sizeof(b), &st);
	checkTrue("each error count in its place", strcmp(b, "PAR 1 FRM 2 BRK 3 OVR 4") == 0);
	st.parityErr = st.framingErr = st.breakErr = st.overrun = 123456;
	sbusErrorText(b, sizeof(b), &st);
	checkTrue("PAR 9999 FRM 9999 BRK 9999 OVR 9999, 35 characters, 209 px",
	          strcmp(b, "PAR 9999 FRM 9999 BRK 9999 OVR 9999") == 0 && gfxTextW(b, 1) == 209);
	st.resyncs = 5; st.badFooter = 6;
	sbusDecoderText(b, sizeof(b), &st);
	checkTrue("RESYNC 5 FOOTER 6", strcmp(b, "RESYNC 5 FOOTER 6") == 0);
	st.resyncs = st.badFooter = 70000;
	sbusDecoderText(b, sizeof(b), &st);
	checkTrue("RESYNC 9999 FOOTER 9999, 23 characters, 137 px",
	          strcmp(b, "RESYNC 9999 FOOTER 9999") == 0 && gfxTextW(b, 1) == 137);
	checkTrue("line 1's widest halves, 18 characters each, 107 px, 10 px apart",
	          gfxTextW("CH17 OFF  CH18 OFF", 1) == 107 && gfxTextW("100.0 MS  999.9 HZ", 1) == 107 &&
	          UI_MARGIN + 107 + 10 == SBUS_UI_VALUE_END - 107);
}

// ---------------------------------------------------------------------------
// the screens, through the real UI
// ---------------------------------------------------------------------------

/**
 * @brief @p n UI frames of 25 ms: core1's job passes every 500 us, the
 *        pump's pass, then the UI.
 */
static void sbFrames(int n) {
	for (int i = 0; i < n; i++) {
		for (int k = 0; k < 50; k++) {
			fakeAdvanceUs(500);
			core1JobPoll();
		}
		fakePumpPoll();
		uiTick();
	}
}

/** @brief Frames with no pump pass: a claim that is Stopping stays Stopping. */
static void framesNoPump(int n) {
	for (int i = 0; i < n; i++) {
		fakeAdvance(25);
		core1JobPoll();
		uiTick();
	}
}

#define UI_IDS_FRAMES(n) sbFrames(n)
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

/** @brief No ESC on either wire. */
static void noEsc() {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
}

/** @brief SIG and AUX on @p sig and @p aux, in the settings and the pump. */
static void wiring(uint8_t sig, uint8_t aux) {
	settings()->dshotPin = sig;
	settings()->kissPin = aux;
	escTaskConfigure(sig, settings()->dshotKbaud, settings()->kissEnable != 0, aux);
	sbFrames(2);
}

/** @brief The tester, default settings, no ESC, nothing on the line. */
static void freshUi() {
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) sbFrames(1);
	fakeFlashClear();
	settingsLoad();
	fakeClockSet(clk_peri, 150000000u);
	fakeUartReset();
	noEsc();
	uiInitTester();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
	wiring(0, 1);
	sbFrames(2);
}

/** @brief From the tester through MENU to RX SIGNALS, the chooser. */
static void openRx() {
	tapId(UI_FIX(TesterCtl::Menu));
	sbFrames(1);
	tapId(menuRow(&UI_RX_SIGNALS));
	sbFrames(1);
}

/** @brief The line at @p y reads @p text from x 8 in @p col. */
static bool leftIs(int y, const char *text, uint16_t col) {
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, 1);
}

/** @brief The line at @p y reads @p text ending at x 231, in @p col. */
static bool rightIs(int y, const char *text, uint16_t col) {
	return fakeTextIs(SBUS_UI_VALUE_END - gfxTextW(text, 1), y, text, col, C_BG, 1);
}

/** @brief The chip reads @p text on @p fill, the label in @p fg. */
static bool chipIs(const char *text, uint16_t fill, uint16_t fg) {
	return fakeTextIs(uiChipX(text) + 6, UI_STRIP_Y + 4, text, fg, fill, 1) &&
	       pixel(uiChipX(text) + 1, UI_STRIP_Y + 3) == fill;
}

/** @brief No chip on the strip: its right part is the background. */
static bool noChip() { return allOf(150, UI_STRIP_Y, GFX_W - 150, UI_STRIP_H, C_BG); }

/** @brief The top of channel row @p i, 0 to 15. */
static int rowY(int i) { return SBUS_UI_ROW_Y + i * SBUS_UI_ROW_PITCH; }

/**
 * @brief Row @p i shows @p raw: its fill in @p bar to raw x 174 / 2047 px
 *        and the background after it, and the value ending at x 231 in
 *        @p val. The column under a tick is skipped.
 */
static bool rowShows(int i, uint16_t raw, const char *value, uint16_t bar, uint16_t val) {
	const int y = rowY(i) + 5;
	const int fill = sbusBarFill(raw, SBUS_UI_FILL_W);
	for (int x = 0; x < SBUS_UI_FILL_W; x++) {
		const int px = SBUS_UI_BAR_X + 1 + x;
		if (px == 39 || px == 109 || px == 178) continue;
		if (pixel(px, y) != (x < fill ? bar : C_BG)) return false;
	}
	return rightIs(rowY(i) + 2, value, val);
}

/** @brief The S.BUS claim is Running and the job bound. */
static bool sbusRunning() {
	return pinClaimHolder() == PinUser::Sbus && pinClaimState() == ClaimState::Running &&
	       core1JobRunning() == Core1Job::Sbus;
}

/** @brief The channels of the preview: the three ticks, the ends and the middle. */
static SbusLineFrame previewFrame() {
	static const uint16_t RAW[SBUS_CHANNELS] = {
		992, 992, 172, 992, 1811, 172, 992, 1400, 600, 1811, 992, 992, 172, 2047, 0, 1200,
	};
	SbusLineFrame f = sbusLineFrame(0);
	memcpy(f.ch, RAW, sizeof(f.ch));
	return f;
}

/** @brief Put @p n frames of @p f on the line, from 3 ms from now, every @p periodUs. */
static uint64_t send(const SbusLineFrame &f, int n, uint32_t periodUs = SBUS_LINE_14MS) {
	return sbusLineStream(time_us_64() + 3000u, periodUs, n, f);
}

/** @brief The S.BUS status as core1 last published it. */
static SbusStatus jobStatus() {
	SbusStatus st;
	sbusJobStatus(&st);
	return st;
}

static void testChooser() {
	section("RX SIGNALS: the chooser, MONITOR and OUTPUT");
	freshUi();
	openRx();
	checkTrue("the RX SIGNALS row opens RX SIGNALS", uiCurrentScreen() == UiScreenId::RxSignals);
	checkTrue("it claims nothing: DShot runs on SIG", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended() && fakePinReturned());
	sbFrames(20);
	checkTrue("half a second on, still nothing claimed", pinClaimState() == ClaimState::Free);
	checkTrue("the title reads RX SIGNALS, 118 px",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_RX_TITLE, C_TEXT, C_PANEL, 2) &&
	          gfxTextW(UI_RX_TITLE, 2) == 118);
	checkTrue("the strip: SIG GP0  AUX GP1, dim", leftIs(UI_STRIP_Y + 4, "SIG GP0  AUX GP1", C_DIM));
	checkInt("two entries", UI_RX_ENTRIES_COUNT, 2);
	const UiRect mon = ctlRect(UI_FIX(RxCtl::Monitor)), out = ctlRect(UI_FIX(RxCtl::Output));
	checkTrue("MONITOR: 228 x 46 px at (6, 62)", mon.x == 6 && mon.y == 62 && mon.w == 228 && mon.h == 46);
	checkTrue("OUTPUT: 228 x 46 px at (6, 142)", out.x == 6 && out.y == 142 && out.w == 228 && out.h == 46);
	checkTrue("MONITOR acts, its label C_TEXT on the panel, centred at scale 2",
	          ctlEnabled(UI_FIX(RxCtl::Monitor)) &&
	          fakeTextIs(mon.x + (mon.w - gfxTextW("MONITOR", 2)) / 2, mon.y + 16, "MONITOR", C_TEXT,
	                     C_PANEL, 2));
	checkTrue("its line at y 112, dim", leftIs(112, "S.BUS FROM A RECEIVER, ON GP1.", C_DIM));
	checkTrue("OUTPUT acts, its label C_TEXT on the panel, centred at scale 2",
	          ctlEnabled(UI_FIX(RxCtl::Output)) &&
	          fakeTextIs(out.x + (out.w - gfxTextW("OUTPUT", 2)) / 2, out.y + 16, "OUTPUT", C_TEXT, C_PANEL, 2));
	checkTrue("its lines at y 192 and 204, dim: the output's warning",
	          leftIs(192, "S.BUS TO A FLIGHT CONTROLLER.", C_DIM) && leftIs(204, "ON GP0, NEVER TO AN ESC.", C_DIM));
	{
		int x0 = 0, x1 = 0;
		checkTrue("nothing under the strip reaches past the buttons' 6 px edges",
		          fakeInkExtent(UI_BODY_Y, GFX_H - UI_BODY_Y, C_BG, &x0, &x1) && x0 >= 6 && x1 < GFX_W - 6);
		checkTrue("the last line ends at y 210", !fakeInkExtent(212, GFX_H - 212, C_BG, &x0, &x1));
	}
	checkControlTable("RX SIGNALS");
	fakeDumpFrame("shot_rx_chooser.ppm");
	fakePress(mon.x + mon.w / 2, mon.y + mon.h / 2);
	sbFrames(1);
	checkTrue("a press on MONITOR draws it pressed", pixel(mon.x + 2, mon.y + mon.h / 2) == C_INK);
	fakeRelease();
	sbFrames(1);
	checkTrue("its lift opens S.BUS MONITOR", uiCurrentScreen() == UiScreenId::Sbus);
	checkTrue("whose entry point asked for the claim: SIG and AUX",
	          pinClaimHolder() == PinUser::Sbus);
	sbFrames(10);
	tapId(UI_FIX(UI_BACK));
	sbFrames(60);
	tapId(UI_FIX(SbusCtl::Release));
	checkTrue("RESTART DSHOT returns to the chooser, its opener", uiCurrentScreen() == UiScreenId::RxSignals);
	sbFrames(4);
	checkTrue("DShot runs again", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	checkTrue("the chooser's BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
}

/** @brief The default wiring: the claim, the waiting monitor, frames, BACK. */
static void testMonitor() {
	section("S.BUS MONITOR: GP1 is AUX, through the claim to frames");
	freshUi();
	openRx();
	tapId(UI_FIX(RxCtl::Monitor), IdExpect::Enabled);
	checkTrue("MONITOR opens S.BUS MONITOR", uiCurrentScreen() == UiScreenId::Sbus);
	checkTrue("its claim: S.BUS, Stopping", pinClaimHolder() == PinUser::Sbus);
	framesNoPump(2);
	checkTrue("Stopping: RELEASING AUX GP1, dim, at y 80",
	          pinClaimState() == ClaimState::Stopping && leftIs(SBUS_UI_MSG_Y, "RELEASING AUX GP1", C_DIM));
	checkTrue("the title S.BUS MONITOR, 154 px, 4 px or more before BACK",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, SBUS_UI_TITLE, C_TEXT, C_PANEL, 2) &&
	          gfxTextW(SBUS_UI_TITLE, 2) == 154 && UI_MARGIN + 154 + 4 <= UI_BACK_X);
	checkTrue("the caption names GP1 by its role: AUX GP1  100 KBAUD  8E2, dim",
	          leftIs(UI_STRIP_Y + 4, "AUX GP1  100 KBAUD  8E2", C_DIM));
	checkTrue("and no chip before the job runs", noChip());
	sbFrames(4);
	checkTrue("the claim runs the S.BUS job", sbusRunning());
	checkTrue("it asked for SIG and AUX: AUX is GP1",
	          pinClaimGpio(PIN_SIG) == 0 && pinClaimGpio(PIN_AUX) == SBUS_GPIO);
	checkTrue("GP1 listens: UART function, input inverted",
	          fakePin(SBUS_GPIO).function == GPIO_FUNC_UART &&
	          fakePin(SBUS_GPIO).inover == GPIO_OVERRIDE_INVERT);

	// Before the first frame.
	checkTrue("the chip: NO SIGNAL, amber, its label C_PAPER",
	          chipIs(SBUS_VIEW_NO_SIGNAL, C_AMBER, C_PAPER));
	checkTrue("the caption ends 16 px before the widest chip",
	          uiChipX(SBUS_VIEW_FRAME_LOST) - (UI_MARGIN + gfxTextW("AUX GP1  100 KBAUD  8E2", 1)) == 16 &&
	          gfxTextW("AUX GP1  100 KBAUD  8E2", 1) == 137);
	bool empty = true;
	for (int i = 0; i < (int)SBUS_CHANNELS; i++) empty = empty && rowShows(i, 0, "----", C_BG, C_DIM);
	checkTrue("16 empty bars and ---- values, dim", empty);
	checkTrue("the numbers 1 and 16, dim, ending at x 18",
	          fakeTextIs(SBUS_UI_NUM_END - gfxTextW("1", 1), rowY(0) + 2, "1", C_DIM, C_BG, 1) &&
	          fakeTextIs(SBUS_UI_NUM_END - gfxTextW("16", 1), rowY(15) + 2, "16", C_DIM, C_BG, 1));
	checkTrue("a bar's outline: dim, x 24 to 199",
	          pixel(24, rowY(0) + 5) == C_DIM && pixel(199, rowY(0) + 5) == C_DIM &&
	          pixel(100, rowY(0) + 1) == C_DIM && pixel(100, rowY(0) + 9) == C_DIM &&
	          pixel(200, rowY(0) + 5) == C_BG);
	checkTrue("its ticks at raw 172, 992 and 1811: x 39, 109, 178",
	          pixel(39, rowY(0) + 5) == C_GRID && pixel(109, rowY(0) + 5) == C_GRID &&
	          pixel(178, rowY(0) + 5) == C_GRID && pixel(40, rowY(0) + 5) == C_BG);
	checkTrue("line 1: CH17 --  CH18 -- and -- MS  -- HZ, dim",
	          leftIs(SBUS_UI_LINE1_Y, "CH17 --  CH18 --", C_DIM) && rightIs(SBUS_UI_LINE1_Y, "-- MS  -- HZ", C_DIM));
	checkTrue("line 2: FRAMES 0 and LOST 0  FS 0",
	          leftIs(SBUS_UI_LINE2_Y, "FRAMES 0", C_TEXT) && rightIs(SBUS_UI_LINE2_Y, "LOST 0  FS 0", C_TEXT));
	checkTrue("line 3: PAR 0 FRM 0 BRK 0 OVR 0", leftIs(SBUS_UI_LINE3_Y, "PAR 0 FRM 0 BRK 0 OVR 0", C_TEXT));
	checkTrue("line 4: RESYNC 0 FOOTER 0", leftIs(SBUS_UI_LINE4_Y, "RESYNC 0 FOOTER 0", C_TEXT));
	checkControlTable("S.BUS MONITOR, no signal");
	checkTrue("no control but BACK", ctlEnabled(UI_FIX(UI_BACK)) && !ctlPresent(UI_FIX(SbusCtl::Retry)));
	fakeDumpFrame("shot_sbus_waiting.ppm");

	// A receiver at 14 ms.
	SbusLineFrame f = previewFrame();
	send(f, 40);
	sbFrames(12);
	SbusStatus st = jobStatus();
	checkTrue("frames decode", st.frames >= 18);
	checkTrue("the chip: OK, green, its label C_ONACCENT", chipIs(SBUS_VIEW_OK, C_GREEN, C_ONACCENT));
	bool rows = true;
	for (int i = 0; i < (int)SBUS_CHANNELS; i++) {
		char v[8];
		snprintf(v, sizeof(v), "%u", (unsigned)sbus_to_us(f.ch[i]));
		rows = rows && rowShows(i, f.ch[i], v, C_LIME, C_TEXT);
	}
	checkTrue("16 bars in raw units, live in lime, values in us, C_TEXT", rows);
	checkTrue("channel 1 at raw 992 reads 1500", rightIs(rowY(0) + 2, "1500", C_TEXT));
	checkTrue("channel 14 at raw 2047 fills the bar: 2144", rowShows(13, 2047, "2144", C_LIME, C_TEXT) &&
	          pixel(198, rowY(13) + 5) == C_LIME);
	checkTrue("channel 15 at raw 0: no fill, 895", rowShows(14, 0, "895", C_LIME, C_TEXT));
	checkTrue("line 1: CH17 OFF  CH18 OFF and 14.0 MS  71.4 HZ",
	          leftIs(SBUS_UI_LINE1_Y, "CH17 OFF  CH18 OFF", C_TEXT) &&
	          rightIs(SBUS_UI_LINE1_Y, "14.0 MS  71.4 HZ", C_TEXT));
	{
		char fr[24];
		snprintf(fr, sizeof(fr), "FRAMES %lu", (unsigned long)st.frames);
		checkTrue("line 2: the frame count", leftIs(SBUS_UI_LINE2_Y, fr, C_TEXT));
	}
	{
		int x0 = 0, x1 = 0;
		checkTrue("the body from x 8 to x 231",
		          fakeInkExtent(UI_BODY_Y, GFX_H - UI_BODY_Y, C_BG, &x0, &x1) && x0 >= UI_MARGIN &&
		          x1 <= SBUS_UI_VALUE_END - 1);
		checkTrue("the last line ends by y 307", !fakeInkExtent(SBUS_UI_LINE4_Y + 8, GFX_H - SBUS_UI_LINE4_Y - 8,
		                                                       C_BG, &x0, &x1));
	}
	checkControlTable("S.BUS MONITOR, frames");
	sbFrames(12);
	fakeDumpFrame("shot_sbus_live.ppm");

	// CH17 on, and a frame with the frame-lost bit.
	SbusLineFrame g = f;
	g.flags = SBUS_FLAG_CH17;
	send(g, 1);
	sbFrames(1);
	checkTrue("CH17 ON  CH18 OFF", leftIs(SBUS_UI_LINE1_Y, "CH17 ON  CH18 OFF", C_TEXT));
	g.flags = SBUS_FLAG_FRAME_LOST;
	uint64_t t = send(g, 1);
	t = sbusLineStream(t, SBUS_LINE_14MS, 40, f);
	sbFrames(1);
	checkTrue("a frame-lost frame: FRAME LOST, amber", chipIs(SBUS_VIEW_FRAME_LOST, C_AMBER, C_PAPER));
	checkTrue("its count: LOST 1", rightIs(SBUS_UI_LINE2_Y, "LOST 1  FS 0", C_TEXT));
	checkTrue("the bars stay live", rowShows(0, 992, "1500", C_LIME, C_TEXT));
	sbFrames(9);
	checkTrue("225 ms on, frames clean since: still FRAME LOST",
	          chipIs(SBUS_VIEW_FRAME_LOST, C_AMBER, C_PAPER));
	sbFrames(2);
	checkTrue("275 ms on: OK", chipIs(SBUS_VIEW_OK, C_GREEN, C_ONACCENT));

	// Failsafe: the receiver's failsafe values with the bit.
	SbusLineFrame fs = sbusLineFrame(0);
	for (unsigned i = 0; i < SBUS_CHANNELS; i++) fs.ch[i] = i == 2 ? 172 : 992;
	fs.flags = SBUS_FLAG_FAILSAFE;
	t = sbusLineStream(t, SBUS_LINE_14MS, 30, fs);
	sbFrames(30);
	checkTrue("FAILSAFE, red", chipIs(SBUS_VIEW_FAILSAFE, C_RED, C_ONACCENT));
	checkTrue("the failsafe values in red", rowShows(0, 992, "1500", C_RED, C_RED) &&
	          rowShows(2, 172, "1000", C_RED, C_RED));
	checkTrue("line 1 stays C_TEXT: the failsafe frame is current",
	          leftIs(SBUS_UI_LINE1_Y, "CH17 OFF  CH18 OFF", C_TEXT) &&
	          rightIs(SBUS_UI_LINE1_Y, "14.0 MS  71.4 HZ", C_TEXT));
	st = jobStatus();
	{
		char lost[32];
		snprintf(lost, sizeof(lost), "LOST 1  FS %lu", (unsigned long)st.failsafeFrames);
		checkTrue("their count: FS", st.failsafeFrames > 0 && rightIs(SBUS_UI_LINE2_Y, lost, C_TEXT));
	}
	fakeDumpFrame("shot_sbus_failsafe.ppm");

	// The receiver stops: the last frame stays readable, dim.
	sbFrames(40);
	checkTrue("no frame for 100 ms: NO SIGNAL", chipIs(SBUS_VIEW_NO_SIGNAL, C_AMBER, C_PAPER));
	checkTrue("the last frame's bars and values, dim", rowShows(0, 992, "1500", C_DIM, C_DIM) &&
	          rowShows(2, 172, "1000", C_DIM, C_DIM));
	checkTrue("line 1 dim: the last frame's", leftIs(SBUS_UI_LINE1_Y, "CH17 OFF  CH18 OFF", C_DIM));
	fakeDumpFrame("shot_sbus_lost.ppm");

	// Bytes at 8E2 that make no frame: a footer the decoder refuses.
	SbusLineFrame bad = f;
	bad.footer = 0xFF;
	send(bad, 20);
	sbFrames(8);
	st = jobStatus();
	checkTrue("bytes and no frame: NO FRAMES", chipIs(SBUS_VIEW_NO_FRAMES, C_AMBER, C_PAPER));
	{
		char dec[48];
		snprintf(dec, sizeof(dec), "RESYNC %lu FOOTER %lu", (unsigned long)st.resyncs,
		         (unsigned long)st.badFooter);
		checkTrue("the footer count says why", st.badFooter > 0 && leftIs(SBUS_UI_LINE4_Y, dec, C_TEXT));
	}
	checkTrue("the bars stay dim", rowShows(0, 992, "1500", C_DIM, C_DIM));
	sbFrames(12);

	// Errors on the line.
	SbusLineFrame e1 = f, e2 = f, e3 = f;
	e1.faultAt = 4; e1.fault = SbusFault::Parity;
	e2.faultAt = 9; e2.fault = SbusFault::Framing;
	e3.faultAt = 12; e3.fault = SbusFault::Break;
	t = send(e1, 1);
	t = sbusLineStream(t, SBUS_LINE_14MS, 1, e2);
	t = sbusLineStream(t, SBUS_LINE_14MS, 1, e3);
	sbusLineStream(t, SBUS_LINE_14MS, 8, f);
	sbFrames(6);
	checkTrue("one parity, framing and break error each", leftIs(SBUS_UI_LINE3_Y, "PAR 1 FRM 1 BRK 1 OVR 0", C_TEXT));
	checkTrue("frames again: OK", chipIs(SBUS_VIEW_OK, C_GREEN, C_ONACCENT));
	sbFrames(8);

	// BACK: the leave page, opened releasing.
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK opens the leave page: the claim Leaving or the pin let go",
	          uiCurrentScreen() == UiScreenId::Sbus && pinClaimState() == ClaimState::Leaving);
	checkTrue("BACK on the page is drawn disabled", !ctlEnabled(UI_FIX(UI_BACK)) &&
	          backLook() == BackLook::Disabled);
	checkTrue("releasing: BACK TO MONITOR and RESTART DSHOT do not act yet",
	          ctlPresent(UI_FIX(SbusCtl::BackToMonitor)) && !ctlEnabled(UI_FIX(SbusCtl::BackToMonitor)) &&
	          !ctlEnabled(UI_FIX(SbusCtl::Release)) && leftIs(UI_LEAVE_NOTE_Y, UI_LEAVE_RELEASING, C_DIM));
	sbFrames(1);
	checkTrue("UART0 is off, GP1 a plain input with no inversion",
	          !fakeUart().enabled && fakePin(SBUS_GPIO).function == GPIO_FUNC_SIO &&
	          fakePin(SBUS_GPIO).inover == GPIO_OVERRIDE_NORMAL);
	checkTrue("the pin is let go at once", pinClaimUnplugDue());
	checkTrue("step 1, current: UNPLUG THE RECEIVER., ESC TESTER SENDS DSHOT ON SIG GP0.",
	          leaveStepIs(66, '1', SBUS_UI_UNPLUG, true) && leaveStepIs(78, 0, "ESC TESTER SENDS DSHOT ON SIG GP0.", true));
	checkTrue("step 2: BACK TO RX SIGNALS.", leaveStepIs(92, '2', "BACK TO RX SIGNALS.", false));
	checkTrue("the strip: SIG GP0  AUX GP1, dim", leftIs(UI_STRIP_Y + 4, "SIG GP0  AUX GP1", C_DIM));
	checkTrue("BACK TO MONITOR acts once the pin is let go", ctlEnabled(UI_FIX(SbusCtl::BackToMonitor)));
	checkTrue("its label fits the 110 px button: 89 px", gfxTextW(SBUS_UI_BACK_TO, 1) == 89 &&
	          fakeTextIs(UI_LEAVE_LEFT.x + (UI_LEAVE_LEFT.w - 89) / 2, UI_LEAVE_LEFT.y + 19, SBUS_UI_BACK_TO,
	                     C_TEXT, C_PANEL, 1));
	checkTrue("RESTART DSHOT does not act yet", !ctlEnabled(UI_FIX(SbusCtl::Release)));
	checkControlTable("S.BUS MONITOR, leave page");
	fakeDumpFrame("shot_sbus_leave.ppm");
	tapId(UI_FIX(SbusCtl::BackToMonitor));
	sbFrames(2);
	checkTrue("BACK TO MONITOR: the claim kept, the job bound again", sbusRunning() &&
	          uiCurrentScreen() == UiScreenId::Sbus);
	checkTrue("every count starts at 0 again", jobStatus().frames == 0 &&
	          leftIs(SBUS_UI_LINE2_Y, "FRAMES 0", C_TEXT) && leftIs(SBUS_UI_LINE3_Y, "PAR 0 FRM 0 BRK 0 OVR 0", C_TEXT));
	checkTrue("NO SIGNAL, empty bars", chipIs(SBUS_VIEW_NO_SIGNAL, C_AMBER, C_PAPER) &&
	          rowShows(0, 0, "----", C_BG, C_DIM));
	tapId(UI_FIX(UI_BACK));
	sbFrames(1);
	const uint32_t released = millis();
	checkTrue("the pin let go on the frame after BACK", pinClaimUnplugDue());
	while (millis() - released < 975) sbFrames(1);
	checkTrue("975 ms after the pin went: RESTART DSHOT still waits", !ctlEnabled(UI_FIX(SbusCtl::Release)));
	sbFrames(1);
	checkTrue("1000 ms: it acts", ctlEnabled(UI_FIX(SbusCtl::Release)));
	int resumes = fakeResumeCount();
	tapId(UI_FIX(SbusCtl::Release));
	checkTrue("RESTART DSHOT: RX SIGNALS, DShot resumed with the job idle",
	          uiCurrentScreen() == UiScreenId::RxSignals && fakeResumeCount() == resumes + 1 && fakeResumeJobIdle());
	tapId(UI_FIX(UI_BACK));  // to MENU
	sbFrames(4);
	checkTrue("the claim Free, GP1 left with no inversion", pinClaimState() == ClaimState::Free &&
	          fakePin(SBUS_GPIO).inover == GPIO_OVERRIDE_NORMAL);
	openTester();
}

/** @brief GP1 is SIG: the screen's own ESC check, its texts, the leave page's lines. */
static void testGp1Sig() {
	section("S.BUS MONITOR: GP1 is SIG, the screen's own ESC check");
	freshUi();
	wiring(1, 0);
	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.lastRpmMs = millis();
	tel.erpm = 1000;
	fakeSetTelemetry(&tel);
	const uint32_t rpmAt = millis();
	openRx();
	checkTrue("the chooser's strip: SIG GP1  AUX GP0", leftIs(UI_STRIP_Y + 4, "SIG GP1  AUX GP0", C_DIM));
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(1);
	checkTrue("an ESC answers on GP1, which is SIG: no claim", uiCurrentScreen() == UiScreenId::Sbus &&
	          pinClaimState() == ClaimState::Free && !escTaskSuspended());
	checkTrue("AN ESC ANSWERS ON SIG GP1., amber, at y 80", leftIs(SBUS_UI_MSG_Y, SBUS_UI_ESC_1, C_AMBER));
	checkTrue("UNPLUG IT. PLUG THE RECEIVER IN, amber, at y 94, 185 px",
	          leftIs(SBUS_UI_MSG_Y + SBUS_UI_MSG_H, SBUS_UI_UNPLUG_2, C_AMBER) &&
	          gfxTextW(SBUS_UI_UNPLUG_2, 1) == 185);
	checkTrue("ONCE THE LINK CHIP SHOWS., amber, at y 108, 149 px",
	          leftIs(SBUS_UI_MSG_Y + 2 * SBUS_UI_MSG_H, SBUS_UI_UNPLUG_3, C_AMBER) &&
	          gfxTextW(SBUS_UI_UNPLUG_3, 1) == 149);
	checkStr("the three lines' exact text", SBUS_UI_ESC_1 " " SBUS_UI_UNPLUG_2 " " SBUS_UI_UNPLUG_3,
	         "AN ESC ANSWERS ON SIG GP1. UNPLUG IT. PLUG THE RECEIVER IN ONCE THE LINK CHIP SHOWS.");
	checkTrue("the caption: SIG GP1  100 KBAUD  8E2", leftIs(UI_STRIP_Y + 4, "SIG GP1  100 KBAUD  8E2", C_DIM));
	checkControlTable("S.BUS MONITOR, an ESC on GP1");
	fakeDumpFrame("shot_sbus_esc.ppm");
	// The check asks again every frame; 500 ms after the last eRPM frame it
	// claims by itself.
	while (millis() - rpmAt < 475) sbFrames(1);
	checkTrue("475 ms after the last eRPM frame: still no claim", pinClaimState() == ClaimState::Free);
	while (millis() - rpmAt < 525) sbFrames(1);
	checkTrue("525 ms: the claim, SIG alone", pinClaimHolder() == PinUser::Sbus &&
	          pinClaimState() != ClaimState::Free);
	sbFrames(4);
	checkTrue("the job binds GP1 as SIG", sbusRunning() && pinClaimGpio(PIN_SIG) == SBUS_GPIO &&
	          pinClaimGpio(PIN_AUX) == PIN_CLAIM_NO_GPIO);
	send(previewFrame(), 10, SBUS_LINE_7MS);
	sbFrames(4);
	checkTrue("frames at 7 ms: 7.0 MS  142.9 HZ", rightIs(SBUS_UI_LINE1_Y, "7.0 MS  142.9 HZ", C_TEXT));
	// An ESC that answers once the claim holds changes nothing: DShot is down.
	tel.lastRpmMs = millis();
	fakeSetTelemetry(&tel);
	sbFrames(1);
	checkTrue("the claim stays", sbusRunning());
	tapId(UI_FIX(UI_BACK));
	checkTrue("the leave page's step 1 names DShot on GP1, where the receiver drives",
	          leaveStepIs(66, '1', SBUS_UI_UNPLUG, true) && leaveStepIs(78, 0, SBUS_UI_RESTART_GP1_1, true) &&
	          leaveStepIs(90, 0, SBUS_UI_RESTART_GP1_2, true) && leaveStepIs(104, '2', "BACK TO RX SIGNALS.", false));
	checkTrue("the strip: SIG GP1", leftIs(UI_STRIP_Y + 4, "SIG GP1", C_DIM) &&
	          allOf(60, UI_STRIP_Y, 120, UI_STRIP_H, C_BG));
	fakeDumpFrame("shot_sbus_leave_sig.ppm");
	sbFrames(42);
	tapId(UI_FIX(SbusCtl::Release));
	sbFrames(4);
	checkTrue("RESTART DSHOT: RX SIGNALS, Free", uiCurrentScreen() == UiScreenId::RxSignals &&
	          pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));  // to MENU
	openTester();
	noEsc();

	// In the default wiring the receiver does not go onto SIG: no check.
	freshUi();
	tel.lastRpmMs = millis();
	fakeSetTelemetry(&tel);
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	checkTrue("GP1 is AUX: an ESC on SIG does not hold the claim back",
	          pinClaimHolder() == PinUser::Sbus);
	sbFrames(16);
	checkTrue("the job runs", sbusRunning());
	tapId(UI_FIX(UI_BACK));
	sbFrames(42);
	tapId(UI_FIX(SbusCtl::Release));
	sbFrames(4);
	checkTrue("RESTART DSHOT: RX SIGNALS", uiCurrentScreen() == UiScreenId::RxSignals);
	tapId(UI_FIX(UI_BACK));
	openTester();
	noEsc();
}

/** @brief KISS live on AUX, no role on GP1, a refused clock and RETRY, BACK before the claim. */
static void testRefusals() {
	section("S.BUS MONITOR: KISS on GP1, no GP1, the clock, BACK before the claim");
	freshUi();
	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.kissLastMs = millis();
	fakeSetTelemetry(&tel);
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(1);
	checkTrue("KISS frames on AUX GP1: refused", pinClaimState() == ClaimState::Free);
	checkTrue("KISS WIRE ON AUX GP1 IS LIVE. and the same second and third lines, amber",
	          leftIs(SBUS_UI_MSG_Y, SBUS_UI_KISS_1, C_AMBER) &&
	          leftIs(SBUS_UI_MSG_Y + SBUS_UI_MSG_H, SBUS_UI_UNPLUG_2, C_AMBER) &&
	          leftIs(SBUS_UI_MSG_Y + 2 * SBUS_UI_MSG_H, SBUS_UI_UNPLUG_3, C_AMBER));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK before a claim: RX SIGNALS, nothing claimed", uiCurrentScreen() == UiScreenId::RxSignals &&
	          pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));  // to MENU
	openTester();
	noEsc();

	// No role on GP1: unreachable through SETTINGS, whose steppers swap.
	freshUi();
	wiring(0, 0);
	openRx();
	checkTrue("the chooser's strip: SIG GP0  AUX GP0", leftIs(UI_STRIP_Y + 4, "SIG GP0  AUX GP0", C_DIM));
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(4);
	checkTrue("no role on GP1: no claim", pinClaimState() == ClaimState::Free &&
	          uiCurrentScreen() == UiScreenId::Sbus);
	checkTrue("the caption with no role on GP1: GP1  100 KBAUD  8E2",
	          leftIs(UI_STRIP_Y + 4, "GP1  100 KBAUD  8E2", C_DIM));
	checkTrue("S.BUS NEEDS GP1. and SET A PIN TO GP1 IN SETTINGS., amber",
	          leftIs(SBUS_UI_MSG_Y, SBUS_UI_NO_GP1_1, C_AMBER) &&
	          leftIs(SBUS_UI_MSG_Y + SBUS_UI_MSG_H, SBUS_UI_NO_GP1_2, C_AMBER));
	checkTrue("no RETRY", !ctlPresent(UI_FIX(SbusCtl::Retry)));
	fakeDumpFrame("shot_sbus_no_gp1.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: RX SIGNALS", uiCurrentScreen() == UiScreenId::RxSignals && pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));  // to MENU
	openTester();

	// clk_peri at 1 MHz: uart_init() reaches 62,500 baud, and the bind
	// refuses; RETRY binds once the clock is right.
	freshUi();
	fakeClockSet(clk_peri, 1000000u);
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(6);
	checkTrue("the bind refused: CORE1_JOB_CLOCK, the claim Held",
	          pinClaimJobError() == CORE1_JOB_CLOCK && pinClaimState() == ClaimState::Held);
	checkTrue("AUX GP1 COULD NOT BE TAKEN (CLOCK), amber",
	          leftIs(SBUS_UI_MSG_Y, "AUX GP1 COULD NOT BE TAKEN (CLOCK)", C_AMBER));
	checkTrue("RETRY, the wide button, acts", ctlEnabled(UI_FIX(SbusCtl::Retry)) &&
	          ctlRect(UI_FIX(SbusCtl::Retry)).y == UI_FOOT_Y && ctlRect(UI_FIX(SbusCtl::Retry)).w == 228);
	checkTrue("and no chip", noChip());
	checkControlTable("S.BUS MONITOR, the clock refused");
	tapId(UI_FIX(SbusCtl::Retry));
	sbFrames(2);
	checkTrue("RETRY with the clock still off: refused again", pinClaimJobError() == CORE1_JOB_CLOCK);
	fakeClockSet(clk_peri, 150000000u);
	tapId(UI_FIX(SbusCtl::Retry));
	sbFrames(4);
	checkTrue("RETRY at 150 MHz: the job runs, RETRY gone", sbusRunning() &&
	          !ctlPresent(UI_FIX(SbusCtl::Retry)) && chipIs(SBUS_VIEW_NO_SIGNAL, C_AMBER, C_PAPER));
	checkTrue("the message and RETRY gone, the channels drawn: the gaps between rows clear",
	          rowShows(0, 0, "----", C_BG, C_DIM) && rowShows(2, 0, "----", C_BG, C_DIM) &&
	          allOf(0, rowY(2) - 1, GFX_W, 1, C_BG) && allOf(0, SBUS_UI_LINE1_Y + 8, GFX_W, 6, C_BG) &&
	          allOf(0, SBUS_UI_LINE4_Y + 8, GFX_W, GFX_H - SBUS_UI_LINE4_Y - 8, C_BG));
	tapId(UI_FIX(UI_BACK));
	sbFrames(42);
	tapId(UI_FIX(SbusCtl::Release));
	sbFrames(4);
	checkTrue("RESTART DSHOT: RX SIGNALS", uiCurrentScreen() == UiScreenId::RxSignals);
	tapId(UI_FIX(UI_BACK));
	openTester();

	// Backstops the UI cannot reach. Another claim holds the pins when
	// MONITOR is tapped: the claim's Busy rule comes before the screen's
	// ESC check, as in pinClaimCheck().
	freshUi();
	wiring(1, 0);
	tel.lastRpmMs = millis();
	tel.kissLastMs = 0;
	fakeSetTelemetry(&tel);
	openRx();
	checkTrue("another claim, taken by the test", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	sbFrames(1);
	checkTrue("RX SIGNALS gates MONITOR while the claim is not Free", !ctlEnabled(UI_FIX(RxCtl::Monitor)));
	// Opened by hand past the hub's gate: the screen's own refusal.
	uiChooseTool(&UI_SBUS, nullptr);
	sbFrames(2);
	checkTrue("busy, with an ESC on GP1: WAIT: DSHOT RESTARTING, not the ESC lines",
	          leftIs(SBUS_UI_MSG_Y, UI_WAIT_RESTART_TEXT, C_AMBER) && pinClaimHolder() == PinUser::Am32);
	tapId(UI_FIX(UI_BACK));
	pinClaimEnd(PinUser::Am32);
	sbFrames(4);
	tapId(UI_FIX(UI_BACK));
	noEsc();

	// The wiring changes under a claim that is Stopping, as only a test can
	// make it: the job finds no role on GP1 and refuses with
	// CORE1_JOB_NO_GP1, and the screen shows its own lines, without RETRY.
	freshUi();
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	escTaskConfigure(0, settings()->dshotKbaud, settings()->kissEnable != 0, 0);
	sbFrames(4);
	checkTrue("the job's CORE1_JOB_NO_GP1", pinClaimJobError() == CORE1_JOB_NO_GP1);
	checkTrue("S.BUS NEEDS GP1., no RETRY", leftIs(SBUS_UI_MSG_Y, SBUS_UI_NO_GP1_1, C_AMBER) &&
	          !ctlPresent(UI_FIX(SbusCtl::Retry)));
	tapId(UI_FIX(UI_BACK));
	sbFrames(42);
	tapId(UI_FIX(SbusCtl::Release));
	sbFrames(4);
	checkTrue("RESTART DSHOT: RX SIGNALS", uiCurrentScreen() == UiScreenId::RxSignals);
	tapId(UI_FIX(UI_BACK));
	openTester();

	// BACK during Stopping: DShot resumes at once, no leave page.
	freshUi();
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	framesNoPump(1);
	checkTrue("Stopping", pinClaimState() == ClaimState::Stopping);
	UiControl b = {};
	ctlFind(UI_FIX(UI_BACK), &b);
	fakePress(b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2);
	framesNoPump(2);
	fakeRelease();
	framesNoPump(1);
	checkTrue("BACK while Stopping: RX SIGNALS, DShot resumed", uiCurrentScreen() == UiScreenId::RxSignals &&
	          fakePinReturned());
	tapId(UI_FIX(UI_BACK));  // to MENU
	sbFrames(4);
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	openTester();
}

/**
 * @brief BACK TO MONITOR starts the frame-lost hold again with the counts: a
 *        frame-lost frame seen just before BACK does not hold FRAME LOST
 *        over the new bind's first frames.
 */
static void testKeepClearsHold() {
	section("S.BUS MONITOR: BACK TO MONITOR forgets a frame-lost frame");
	freshUi();
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(6);
	checkTrue("running", sbusRunning());
	SbusLineFrame f = previewFrame(), g = f;
	g.flags = SBUS_FLAG_FRAME_LOST;
	sbusLineStream(send(f, 4), SBUS_LINE_14MS, 1, g);
	sbFrames(3);
	const uint32_t seen = millis();
	checkTrue("FRAME LOST", chipIs(SBUS_VIEW_FRAME_LOST, C_AMBER, C_PAPER));
	tapId(UI_FIX(UI_BACK));
	sbFrames(1);
	tapId(UI_FIX(SbusCtl::BackToMonitor));
	for (int k = 0; k < 4 && !sbusRunning(); k++) sbFrames(1);
	send(f, 4);
	sbFrames(1);
	checkTrue("the new bind's frames within 250 ms of it", jobStatus().frames > 0 && millis() - seen < 250);
	checkTrue("read OK, not FRAME LOST", chipIs(SBUS_VIEW_OK, C_GREEN, C_ONACCENT));
	tapId(UI_FIX(UI_BACK));
	sbFrames(42);
	tapId(UI_FIX(SbusCtl::Release));
	sbFrames(4);
	checkTrue("RESTART DSHOT: RX SIGNALS", uiCurrentScreen() == UiScreenId::RxSignals);
	tapId(UI_FIX(UI_BACK));
	openTester();
}

/**
 * @brief docs/design/menu-c.md slice 2: MONITOR returns to RX SIGNALS, the
 *        chooser that opened it, by BACK before a claim and by RESTART
 *        DSHOT, the leave page's last button.
 */
static void testNavOpener() {
	section("Navigation: MONITOR returns to RX SIGNALS, its opener");
	freshUi();
	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.kissLastMs = millis();
	fakeSetTelemetry(&tel);
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(1);
	checkTrue("KISS on AUX: MENU, RX SIGNALS, S.BUS MONITOR, no claim",
	          navIsTool(UiScreenId::RxSignals, UiScreenId::Sbus) && pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK before a claim: RX SIGNALS", navIsHub(UiScreenId::RxSignals));
	checkTrue("then MENU and the tester", backToTester());
	noEsc();

	freshUi();
	openRx();
	tapId(UI_FIX(RxCtl::Monitor));
	sbFrames(10);
	checkTrue("the claim held: MENU, RX SIGNALS, S.BUS MONITOR",
	          navIsTool(UiScreenId::RxSignals, UiScreenId::Sbus) && pinClaimHolder() == PinUser::Sbus);
	tapId(UI_FIX(UI_BACK));
	sbFrames(42);
	tapId(UI_FIX(SbusCtl::Release));
	sbFrames(4);
	checkTrue("RESTART DSHOT: RX SIGNALS, the claim Free",
	          navIsHub(UiScreenId::RxSignals) && pinClaimState() == ClaimState::Free);
	checkTrue("then MENU and the tester", backToTester());
}

void runSbusUiTests() {
	testLinkState();
	testInkAndBars();
	testTexts();
	testChooser();
	testMonitor();
	testGp1Sig();
	testRefusals();
	testKeepClearsHold();
	testNavOpener();
}
