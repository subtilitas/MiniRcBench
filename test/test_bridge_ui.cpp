/**
 * @file test_bridge_ui.cpp
 * @brief USB BRIDGE through the real UI (slice 2 of
 *        docs/design/usb-bridge.md): the entry on ESC CONFIG, the claim, the
 *        screen's lines, BACK in each state, the leave page, and the console.
 *
 * A UI frame here is 25 ms: core1's job, `tud_task()` and usbBridgeTask()
 * every 250 µs, as core0's loop runs the last two every pass, then a pass of
 * the fake DShot pump and one uiTick(). The PC is test/fake_cdc.h, the ESC
 * the line fake's model ESC. Nothing here has run on the chip.
 */

#include "check.h"
#include "fakes.h"
#include "fake_cdc.h"
#include "fake_bridge_line.h"
#include "ui_bridge.h"
#include "usb_bridge.h"
#include "bridge_line_port.h"
#include "ui_esc_config.h"
#include "ui_am32.h"
#include "ui_widgets.h"
#include "ui_leave.h"
#include "ui.h"
#include "gfx.h"
#include "settings.h"
#include "esc_task.h"
#include "core1_job.h"
#include "pin_claim.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "plat.h"
#include "tusb.h"

#include <stdio.h>
#include <string.h>

/** @brief Times DShot was up while the bridge's line was open. */
static int g_together = 0;

/** @brief @p n UI frames of 25 ms. */
static void brFrames(int n) {
	for (int i = 0; i < n; i++) {
		for (int k = 0; k < 100; k++) {
			fakeAdvanceUs(250);
			core1JobPoll();
			tud_task();
			usbBridgeTask();
			if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
		}
		fakePumpPoll();
		uiTick();
		if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
	}
}

/** @brief Frames with no pump pass: a claim that is Stopping stays Stopping. */
static void framesNoPump(int n) {
	for (int i = 0; i < n; i++) {
		fakeAdvance(25);
		core1JobPoll();
		tud_task();
		usbBridgeTask();
		uiTick();
	}
}

#define UI_IDS_FRAMES(n) brFrames(n)
#include "ui_ids.h"

static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

static bool allOf(int x, int y, int w, int h, uint16_t c) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (pixel(i, j) != c) return false;
	return true;
}

/** @brief The line at @p y reads @p text from x 8 in @p col, scale @p scale. */
static bool leftIs(int y, const char *text, uint16_t col, int scale = 1) {
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, scale);
}

/** @brief Message line @p i reads @p text in @p col. */
static bool msgIs(int i, const char *text, uint16_t col) {
	return leftIs(BRIDGE_UI_MSG_Y + i * BRIDGE_UI_MSG_H, text, col);
}

/** @brief Message line @p i is blank. */
static bool msgBlank(int i) {
	return allOf(0, BRIDGE_UI_MSG_Y + i * BRIDGE_UI_MSG_H, GFX_W, 8, C_BG);
}

/** @brief The waiting and running lines, all dim, but for line @p skip0 and @p skip1. */
static bool defaultLines(int skip0 = -1, int skip1 = -1) {
	const char *L[] = { BRIDGE_UI_LINE_0, BRIDGE_UI_LINE_1, BRIDGE_UI_LINE_2, nullptr,
	                    BRIDGE_UI_LINE_4, BRIDGE_UI_LINE_5 };
	for (int i = 0; i < 6; i++) {
		if (i == skip0 || i == skip1) continue;
		if (L[i] ? !msgIs(i, L[i], C_DIM) : !msgBlank(i)) return false;
	}
	return true;
}

/** @brief The chip reads @p text on @p fill. */
static bool chipIs(const char *text, uint16_t fill, uint16_t fg) {
	return fakeTextIs(uiChipX(text) + 6, UI_STRIP_Y + 4, text, fg, fill, 1) &&
	       pixel(uiChipX(text) + 1, UI_STRIP_Y + 3) == fill;
}

/** @brief The counters read @p toEsc and @p toPc. */
static bool countersAre(const char *toEsc, const char *toPc) {
	return leftIs(BRIDGE_UI_COUNT_Y, toEsc, C_TEXT, 2) &&
	       fakeTextIs(BRIDGE_UI_RIGHT_X, BRIDGE_UI_COUNT_Y, toPc, C_TEXT, C_BG, 2);
}

/** @brief Nothing under the strip reaches x 233 or beyond, nor x 7 or below. */
static bool inkWithin() {
	int x0 = 0, x1 = 0;
	return !fakeInkExtent(UI_BODY_Y, GFX_H - UI_BODY_Y, C_BG, &x0, &x1) || (x0 >= 6 && x1 < GFX_W - 6);
}

static int g_blPayload = 0;
static uint32_t g_ackUs = 300;
/** @brief The ESC: every frame answered with the ACK after g_ackUs, but SET BUFFER. */
static int escAck(const uint8_t *f, int n, uint8_t *r, uint32_t *d) {
	*d = g_ackUs;
	if (f[0] == 0xFE && n == 6 && !g_blPayload) {
		g_blPayload = 1;
		return 0;
	}
	g_blPayload = 0;
	r[0] = 0x30;
	return 1;
}

/** @brief The ESC answers 1100 bytes to anything. */
static int escFlood(const uint8_t *, int, uint8_t *r, uint32_t *d) {
	for (int i = 0; i < 1100; i++) r[i] = (uint8_t)i;
	*d = 300;
	return 1100;
}

static const uint8_t SET_ADDR[] = {0xFF, 0x00, 0x7E, 0x00, 0x3C, 0x8B};
static const uint8_t PROG[]     = {0x01, 0x01, 0x11, 0x22};
static const uint8_t RUN[]      = {0x00, 0x00, 0x00, 0x00};   // 00 00 and its CRC

/** @brief The tester, default settings, no ESC, no card, the PC on the port with DTR clear. */
static void freshUi(FakeEscReply r = escAck) {
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) {
		pinClaimRestart(pinClaimHolder());
		brFrames(1);
	}
	brFrames(2);
	fakeFlashClear();
	settingsLoad();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	uiInit();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
	escTaskConfigure(0, settings()->dshotKbaud, settings()->kissEnable != 0, 1);
	fakeBridgeLineReset();
	fakeBridgeLineResponder(r);
	fakeCdcReset();
	g_ackUs = 300;
	g_blPayload = 0;
	brFrames(4);
}

/** @brief Tester → MENU → ESC CONFIG → USB BRIDGE, by id. */
static void openBridgeScreen() {
	tapId(UI_FIX(TesterCtl::Menu));
	brFrames(1);
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	tapId(UI_FIX(EscCfgCtl::Bridge));
}

/** @brief Frames until the bridge is open, at most @p n. */
static bool untilOpen(int n = 20) {
	for (int k = 0; k < n && usbBridgeMode() == BridgeMode::Closed; k++) brFrames(1);
	return usbBridgeMode() != BridgeMode::Closed;
}

/** @brief From the leave page: RESTART DSHOT once live, then frames until Free. */
static void restartFromPage() {
	brFrames(44);
	tapId(UI_FIX(BridgeCtl::Restart));
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) brFrames(1);
	brFrames(1);
}

/** @brief BACK, then through the leave page if it opened, until Free. */
static void leaveBridge() {
	tapId(UI_FIX(UI_BACK));
	brFrames(1);
	if (uiCurrentScreen() == UiScreenId::Bridge && ctlPresent(UI_FIX(BridgeCtl::Restart))) restartFromPage();
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) brFrames(1);
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void testLayout() {
	section("USB BRIDGE: the entry, the claim, the screen at rest");
	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	brFrames(1);
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	const UiRect br = ctlRect(UI_FIX(EscCfgCtl::Bridge));
	fakePress(br.x + br.w / 2, br.y + br.h / 2);
	brFrames(1);
	fakeRelease();
	framesNoPump(1);
	checkTrue("ESC CONFIG's USB BRIDGE opens the screen", uiCurrentScreen() == UiScreenId::Bridge);
	checkTrue("which claims SIG for the bridge on the tap's frame",
	          pinClaimHolder() == PinUser::Bridge && pinClaimState() == ClaimState::Stopping);
	framesNoPump(1);
	checkTrue("Stopping: the strip reads RELEASING SIG GP0, 101 px",
	          leftIs(UI_STRIP_Y + 4, "RELEASING SIG GP0", C_DIM) && gfxTextW("RELEASING SIG GP0", 1) == 101);
	checkTrue("no chip, no mode, the lines", allOf(150, UI_STRIP_Y, 90, UI_STRIP_H, C_BG) &&
	          allOf(0, BRIDGE_UI_MODE_CAP_Y, GFX_W, 80, C_BG) && defaultLines());
	checkTrue("the console has the port still", fakeConsoleOn());
	fakeDumpFrame("shot_bridge_stopping.ppm");
	checkTrue("the bridge opens on Running", untilOpen());
	brFrames(1);
	checkTrue("the console off", !fakeConsoleOn());
	checkTrue("the title USB BRIDGE, 118 px, 44 px clear of BACK",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, BRIDGE_UI_TITLE, C_TEXT, C_PANEL, 2) &&
	          gfxTextW(BRIDGE_UI_TITLE, 2) == 118 && UI_MARGIN + 118 + 44 == UI_BACK_X);
	checkTrue("the strip: SIG GP0  19200 8N1, 107 px", leftIs(UI_STRIP_Y + 4, "SIG GP0  19200 8N1", C_DIM) &&
	          gfxTextW("SIG GP0  19200 8N1", 1) == 107);
	checkTrue("DTR clear: PORT CLOSED on grey, from x 155", chipIs(BRIDGE_UI_PORT_CLOSED, C_GRID, C_ONACCENT) &&
	          uiChipX(BRIDGE_UI_PORT_CLOSED) == 155);
	checkTrue("MODE, dim, at y 62; WAITING at scale 2, y 72",
	          leftIs(62, "MODE", C_DIM) && leftIs(72, "WAITING", C_TEXT, 2));
	checkTrue("the captions at y 98, x 8 and x 122",
	          leftIs(98, "PC > ESC", C_DIM) && fakeTextIs(122, 98, "ESC > PC", C_DIM, C_BG, 1));
	checkTrue("the counters at 0 B, scale 2, y 108", countersAre("0 B", "0 B"));
	checkTrue("FRAMES 0  LOST 0 at y 134", leftIs(134, "FRAMES 0  LOST 0", C_TEXT));
	checkTrue("no last command", allOf(0, 148, GFX_W, 8, C_BG));
	checkTrue("the six lines, dim, line 3 blank", defaultLines());
	checkTrue("line 0 is 221 px, to x 228", gfxTextW(BRIDGE_UI_LINE_0, 1) == 221);
	checkTrue("nothing under the strip past the 6 px edges", inkWithin());
	checkTrue("the last line ends at y 235: nothing below", allOf(0, 236, GFX_W, GFX_H - 236, C_BG));
	checkTrue("BACK live", backLook() == BackLook::Live && ctlEnabled(UI_FIX(UI_BACK)));
	checkTrue("no RETRY, nothing else in the table", !ctlPresent(UI_FIX(BridgeCtl::Retry)) &&
	          !ctlPresent(UI_FIX(BridgeCtl::Bridge)) && !ctlPresent(UI_FIX(BridgeCtl::Restart)));
	checkControlTable("USB BRIDGE, waiting");
	fakeDumpFrame("shot_bridge_waiting.ppm");
	fakeCdcDtr(true);
	brFrames(1);
	checkTrue("DTR set: PORT OPEN on lime", chipIs(BRIDGE_UI_PORT_OPEN, UI_COL_LIVE, C_ONACCENT));
	tapId(UI_FIX(UI_BACK));
	brFrames(1);
	checkTrue("BACK with nothing sent: the leave page, the console still off",
	          uiCurrentScreen() == UiScreenId::Bridge && ctlPresent(UI_FIX(BridgeCtl::Restart)) &&
	          !fakeConsoleOn());
	restartFromPage();
	checkTrue("RESTART DSHOT: MENU, Free, the console back", uiCurrentScreen() == UiScreenId::Menu &&
	          pinClaimState() == ClaimState::Free && fakeConsoleOn() && !escTaskSuspended());
	tapId(UI_FIX(UI_BACK));
}

static void testTraffic() {
	section("USB BRIDGE: passthrough, the counters, the last command, the port");
	freshUi();
	openBridgeScreen();
	untilOpen();
	fakeCdcDtr(true);
	brFrames(12);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	brFrames(1);
	checkTrue("the frame out: PASSTHROUGH", leftIs(72, "PASSTHROUGH", C_TEXT, 2));
	brFrames(10);
	checkTrue("6 B to the ESC, 7 B to the PC", countersAre("6 B", "7 B"));
	checkTrue("FRAMES 1  LOST 0", leftIs(134, "FRAMES 1  LOST 0", C_TEXT));
	checkTrue("LAST SET ADDRESS 7E00, dim, 125 px", leftIs(148, "LAST SET ADDRESS 7E00", C_DIM) &&
	          gfxTextW("LAST SET ADDRESS 7E00", 1) == 125);
	checkControlTable("USB BRIDGE, passthrough");
	fakeDumpFrame("shot_bridge_passthrough.ppm");

	// The counters repaint at most every 250 ms.
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	brFrames(1);
	bool first = countersAre("12 B", "14 B");
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	brFrames(1);
	UsbBridgeStats st;
	usbBridgeStats(&st);
	bool held = st.toEsc == 18 && countersAre("12 B", "14 B");
	brFrames(8);
	bool still = countersAre("12 B", "14 B");
	brFrames(1);
	checkTrue("a change after 250 ms quiet: drawn at once", first);
	checkTrue("the next 25 ms later: held while 250 ms have not passed", held && still);
	checkTrue("drawn 250 ms after the last paint", countersAre("18 B", "21 B"));

	// The PC closes the port after traffic.
	fakeCdcDtr(false);
	brFrames(2);
	checkTrue("DTR falls: WAITING, THE PC CLOSED THE PORT., dim, PORT CLOSED",
	          leftIs(72, "WAITING", C_TEXT, 2) && msgIs(0, BRIDGE_UI_CLOSED, C_DIM) &&
	          defaultLines(0) && chipIs(BRIDGE_UI_PORT_CLOSED, C_GRID, C_ONACCENT));
	checkInt("THE PC CLOSED THE PORT. is 137 px", gfxTextW(BRIDGE_UI_CLOSED, 1), 137);
	fakeCdcDtr(true);
	fakeCdcHostWrite(RUN, sizeof(RUN));
	brFrames(12);
	checkTrue("the PC's next frame: PASSTHROUGH, the line gone, LAST RUN",
	          leftIs(72, "PASSTHROUGH", C_TEXT, 2) && defaultLines() && leftIs(148, "LAST RUN", C_DIM));
	const uint8_t other[] = {0x42, 0x01};
	fakeCdcHostWrite(other, sizeof(other));
	brFrames(12);
	checkTrue("a first byte with no name: LAST FRAME 42", leftIs(148, "LAST FRAME 42", C_DIM));
	tapId(UI_FIX(UI_BACK));
	brFrames(1);
	checkTrue("BACK after a RUN: the leave page all the same, no FLASH note",
	          ctlPresent(UI_FIX(BridgeCtl::Restart)) && allOf(0, UI_LEAVE_NOTE_Y, GFX_W, 8, C_BG));
	restartFromPage();
	tapId(UI_FIX(UI_BACK));
}

static void testWriteAndLeave() {
	section("USB BRIDGE: a write, BACK disabled, the leave page");
	freshUi();
	openBridgeScreen();
	untilOpen();
	fakeCdcDtr(true);
	g_ackUs = 1000000;
	fakeCdcHostWrite(PROG, sizeof(PROG));
	brFrames(12);
	checkTrue("PROGRAM out, its answer a second away: WRITING..., amber",
	          msgIs(0, BRIDGE_UI_WRITING, C_AMBER) && defaultLines(0) &&
	          gfxTextW(BRIDGE_UI_WRITING, 1) == 203);
	checkTrue("BACK drawn disabled and reported so", backLook() == BackLook::Disabled &&
	          !ctlEnabled(UI_FIX(UI_BACK)));
	checkControlTable("USB BRIDGE, a write waits");
	checkTrue("LAST PROGRAM, its echo counted", leftIs(148, "LAST PROGRAM", C_DIM) && countersAre("4 B", "4 B"));
	fakeDumpFrame("shot_bridge_writing.ppm");
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("a tap on BACK does nothing", uiCurrentScreen() == UiScreenId::Bridge &&
	          pinClaimState() == ClaimState::Running);
	brFrames(40);
	checkTrue("the ACK: BACK live, the line back", backLook() == BackLook::Live &&
	          ctlEnabled(UI_FIX(UI_BACK)) && defaultLines());

	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK after a write with no RUN: the leave page, not MENU",
	          uiCurrentScreen() == UiScreenId::Bridge && pinClaimLeavePage() &&
	          pinClaimState() == ClaimState::Leaving);
	checkTrue("its first step, current: POWER THE ESC OFF., 107 px",
	          leaveStepIs(UI_LEAVE_STEP_Y, '1', BRIDGE_UI_LEAVE_1, true) &&
	          gfxTextW(BRIDGE_UI_LEAVE_1, 1) == 107);
	checkTrue("the second: RESTART DSHOT.", leaveStepIs(UI_LEAVE_STEP_Y + 14, '2', UI_LEAVE_RESTART, false));
	checkTrue("the note at y 158, 185 px", fakeTextIs(UI_MARGIN, UI_LEAVE_NOTE_Y, BRIDGE_UI_LEAVE_NOTE, C_DIM,
	                                                   C_BG, 1) &&
	          gfxTextW(BRIDGE_UI_LEAVE_NOTE, 1) == 185);
	checkTrue("BACK drawn disabled on the page", backLook() == BackLook::Disabled &&
	          !ctlEnabled(UI_FIX(UI_BACK)));
	checkTrue("BRIDGE drawn disabled while the job lets the line go",
	          !ctlEnabled(UI_FIX(BridgeCtl::Bridge)) && fakeTextIs(UI_MARGIN, UI_LEAVE_NOTE_Y + 12,
	                                                                UI_LEAVE_RELEASING, C_DIM, C_BG, 1));
	brFrames(1);
	checkTrue("BRIDGE live once the line is let go; RESTART DSHOT not yet",
	          ctlEnabled(UI_FIX(BridgeCtl::Bridge)) && !ctlEnabled(UI_FIX(BridgeCtl::Restart)));
	checkTrue("its label in the left button", fakeTextIs(UI_LEAVE_LEFT.x + (UI_LEAVE_LEFT.w - gfxTextW("BRIDGE", 1)) / 2,
	                                                      UI_LEAVE_LEFT.y + 19, "BRIDGE", C_TEXT, C_PANEL, 1));
	checkTrue("DShot down, the console off", escTaskSuspended() && !fakeConsoleOn());
	checkControlTable("USB BRIDGE, leave page");
	fakeDumpFrame("shot_bridge_leave.ppm");
	tapId(UI_FIX(BridgeCtl::Restart), IdExpect::Disabled);
	checkTrue("RESTART DSHOT early: nothing", uiCurrentScreen() == UiScreenId::Bridge && escTaskSuspended());

	tapId(UI_FIX(BridgeCtl::Bridge));
	checkTrue("BRIDGE: back on the bridge", uiCurrentScreen() == UiScreenId::Bridge &&
	          !ctlPresent(UI_FIX(BridgeCtl::Bridge)));
	checkTrue("the bridge open again, the counts at 0", untilOpen() && [] {
		brFrames(12);
		return countersAre("0 B", "0 B") && leftIs(72, "WAITING", C_TEXT, 2);
	}());
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK with no write since: the page all the same, the claim keeps it",
	          uiCurrentScreen() == UiScreenId::Bridge && ctlPresent(UI_FIX(BridgeCtl::Restart)));
	brFrames(44);
	checkTrue("1000 ms after the line was let go: RESTART DSHOT live", ctlEnabled(UI_FIX(BridgeCtl::Restart)));
	tapId(UI_FIX(BridgeCtl::Restart));
	checkTrue("RESTART DSHOT: MENU", uiCurrentScreen() == UiScreenId::Menu);
	for (int k = 0; k < 4 && pinClaimState() != ClaimState::Free; k++) brFrames(1);
	brFrames(1);
	checkTrue("Free, DShot up, the console back", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended() && fakeConsoleOn());

	// A write finished by RUN: the page without the FLASH note.
	freshUi();
	openBridgeScreen();
	untilOpen();
	fakeCdcHostWrite(PROG, sizeof(PROG));
	brFrames(4);
	fakeCdcHostWrite(RUN, sizeof(RUN));
	brFrames(4);
	tapId(UI_FIX(UI_BACK));
	brFrames(1);
	checkTrue("PROGRAM, then RUN: the leave page without the FLASH note",
	          ctlPresent(UI_FIX(BridgeCtl::Restart)) && pinClaimLeavePage() &&
	          allOf(0, UI_LEAVE_NOTE_Y, GFX_W, 8, C_BG) && usbBridgeRunCue(0));
	restartFromPage();
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
}

static void testBackStates() {
	section("USB BRIDGE: BACK while Stopping, after a failed bind, refusals");
	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	brFrames(1);
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	const UiRect br = ctlRect(UI_FIX(EscCfgCtl::Bridge));
	fakePress(br.x + br.w / 2, br.y + br.h / 2);
	brFrames(1);
	fakeRelease();
	framesNoPump(1);
	const int resumes = fakeResumeCount();
	const UiRect back = uiBackRect();
	fakePress(back.x + 10, back.y + 10);
	framesNoPump(1);
	fakeRelease();
	framesNoPump(1);
	checkTrue("BACK while Stopping: MENU, DShot asked back at once",
	          uiCurrentScreen() == UiScreenId::Menu && fakeResumeCount() == resumes + 1 &&
	          fakeBridgeLineOpens() == 0);
	brFrames(4);
	checkTrue("Free, the console never switched", pinClaimState() == ClaimState::Free &&
	          fakeConsoleSwitches() == 0);
	tapId(UI_FIX(UI_BACK));

	// A failed bind.
	freshUi();
	fakeBridgeLineRefuse(CORE1_JOB_PIO);
	openBridgeScreen();
	brFrames(4);
	checkTrue("a refused bind: SIG GP0 COULD NOT BE TAKEN (PIO), amber",
	          msgIs(0, "SIG GP0 COULD NOT BE TAKEN (PIO)", C_AMBER) && msgBlank(1) &&
	          gfxTextW("SIG GP0 COULD NOT BE TAKEN (PIO)", 1) == 191);
	checkTrue("(CLOCK), the widest, is 203 px", gfxTextW("SIG GP0 COULD NOT BE TAKEN (CLOCK)", 1) == 203);
	checkTrue("RETRY in the footer: 228 x 46 at (6, 268)", ctlEnabled(UI_FIX(BridgeCtl::Retry)) && [] {
		UiRect r = ctlRect(UI_FIX(BridgeCtl::Retry));
		return r.x == 6 && r.y == 268 && r.w == 228 && r.h == 46;
	}());
	checkTrue("no mode, no chip, the console on", allOf(0, BRIDGE_UI_MODE_CAP_Y, GFX_W, 80, C_BG) &&
	          allOf(150, UI_STRIP_Y, 90, UI_STRIP_H, C_BG) && fakeConsoleOn());
	checkControlTable("USB BRIDGE, a failed bind");
	fakeDumpFrame("shot_bridge_failed.ppm");
	fakeBridgeLineRefuse(0);
	tapId(UI_FIX(BridgeCtl::Retry));
	checkTrue("RETRY: the bridge opens", untilOpen() && !ctlPresent(UI_FIX(BridgeCtl::Retry)));
	brFrames(1);
	checkTrue("the lines and the mode", defaultLines() && leftIs(72, "WAITING", C_TEXT, 2) &&
	          allOf(0, UI_FOOT_Y, GFX_W, GFX_H - UI_FOOT_Y, C_BG));
	leaveBridge();
	fakeBridgeLineRefuse(CORE1_JOB_CLOCK);
	usbBridgeRunCueClear();
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	tapId(UI_FIX(EscCfgCtl::Bridge));
	brFrames(4);
	checkTrue("a refused clock: (CLOCK)", msgIs(0, "SIG GP0 COULD NOT BE TAKEN (CLOCK)", C_AMBER));
	const int r2 = fakeResumeCount();
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from a failed bind: MENU, DShot asked back", uiCurrentScreen() == UiScreenId::Menu &&
	          fakeResumeCount() == r2 + 1);
	brFrames(4);
	checkTrue("  the bind never ran, the cue stands: MENU warns", usbBridgeRunCue(0) &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_RUN_CUE, UI_COL_WARN, C_BG, 1));
	usbBridgeRunCueClear();
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	fakeBridgeLineRefuse(0);
	tapId(UI_FIX(UI_BACK));

	// USB DRIVE holds the card.
	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	brFrames(1);
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	fakeMscSet(MscState::Serving, 0);
	tapId(UI_FIX(EscCfgCtl::Bridge));
	brFrames(4);
	checkTrue("USB DRIVE holds the card: no claim, EJECT THE USB DRIVE FIRST., amber, 155 px",
	          uiCurrentScreen() == UiScreenId::Bridge && pinClaimState() == ClaimState::Free &&
	          msgIs(0, BRIDGE_UI_EJECT, C_AMBER) && msgBlank(1) && gfxTextW(BRIDGE_UI_EJECT, 1) == 155);
	checkTrue("DShot runs, the console on", !escTaskSuspended() && fakeConsoleOn());
	checkControlTable("USB BRIDGE, USB DRIVE");
	fakeDumpFrame("shot_bridge_eject.ppm");
	fakeMscSet(MscState::Handover, 0);
	brFrames(2);
	checkTrue("handing over counts as holding it", pinClaimState() == ClaimState::Free);
	fakeMscSet(MscState::Idle, 0);
	brFrames(1);
	checkTrue("ejected: the claim begins", pinClaimHolder() == PinUser::Bridge);
	checkTrue("and the bridge opens", untilOpen());
	leaveBridge();
	fakeMscSet(MscState::Serving, 0);
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	tapId(UI_FIX(EscCfgCtl::Bridge));
	brFrames(2);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK with no claim: MENU, Free", uiCurrentScreen() == UiScreenId::Menu &&
	          pinClaimState() == ClaimState::Free);
	fakeMscSet(MscState::Idle, 0);
	tapId(UI_FIX(UI_BACK));

	// A claim another holds: the refusal, as ESC SETTINGS shows it.
	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	brFrames(1);
	tapId(menuRow(&UI_ESC_CONFIG));
	brFrames(1);
	pinClaimBegin(PinUser::Am32, PIN_SIG);
	tapId(UI_FIX(EscCfgCtl::Bridge));
	brFrames(1);
	checkTrue("a claim held: WAIT: DSHOT RESTARTING, amber", msgIs(0, UI_WAIT_RESTART_TEXT, C_AMBER) &&
	          pinClaimHolder() == PinUser::Am32);
	pinClaimEnd(PinUser::Am32);
	brFrames(3);
	checkTrue("asked again each frame: the bridge's claim once Free", pinClaimHolder() == PinUser::Bridge);
	leaveBridge();
	tapId(UI_FIX(UI_BACK));
}

static void testLostAndGp1() {
	section("USB BRIDGE: bytes lost; SIG on GP1");
	freshUi(escFlood);
	openBridgeScreen();
	untilOpen();
	fakeCdcHostStall(true);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	brFrames(30);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	brFrames(40);
	UsbBridgeStats s;
	usbBridgeStats(&s);
	char line[40];
	snprintf(line, sizeof(line), "LOST %lu", (unsigned long)s.lost);
	checkTrue("LOST above 0", s.lost > 0);
	checkTrue("line 1: BYTES LOST: THE BRIDGE FELL BEHIND., amber, 209 px",
	          msgIs(1, BRIDGE_UI_LOST, C_AMBER) && defaultLines(1) && gfxTextW(BRIDGE_UI_LOST, 1) == 209);
	checkTrue("LOST amber in the frames line", leftIs(134, "FRAMES 2  ", C_TEXT) &&
	          fakeTextIs(UI_MARGIN + 60, 134, line, C_AMBER, C_BG, 1));
	fakeDumpFrame("shot_bridge_lost.ppm");
	fakeCdcHostStall(false);
	leaveBridge();
	tapId(UI_FIX(UI_BACK));

	freshUi();
	settings()->dshotPin = 1;
	settings()->kissPin = 0;
	escTaskConfigure(1, settings()->dshotKbaud, settings()->kissEnable != 0, 0);
	brFrames(2);
	openBridgeScreen();
	untilOpen();
	brFrames(1);
	checkTrue("SIG on GP1: the strip, and the line on GP1",
	          leftIs(UI_STRIP_Y + 4, "SIG GP1  19200 8N1", C_DIM) && fakeBridgeLinePin() == 1);
	leaveBridge();
	tapId(UI_FIX(UI_BACK));
	settings()->dshotPin = 0;
	settings()->kissPin = 1;
	escTaskConfigure(0, settings()->dshotKbaud, settings()->kissEnable != 0, 1);
	brFrames(2);
}

/** @brief The model bootloader answers the greeting: AM32's answer, or an EFM8's. */
static bool g_efm8 = false;
static int escGreets(const uint8_t *f, int n, uint8_t *r, uint32_t *d) {
	*d = 300;
	if (usbBridgeCmdOf(f, (uint16_t)n, false) == BridgeCmd::Init) {
		static const uint8_t AM32[] = {0x34, 0x37, 0x31, 0x00, 0x1F, 0x06, 0x06, 0x01, 0x30};
		static const uint8_t EFM8[] = {0x34, 0x37, 0x31, 0x64, 0xE8, 0xB2, 0x06, 0x01, 0x30};
		memcpy(r, g_efm8 ? EFM8 : AM32, 9);
		return 9;
	}
	r[0] = 0x30;
	return 1;
}

/** @brief A tool that greets, reads and lets go without RUN: BACK gives the leave page. */
static void testReadOnly() {
	section("USB BRIDGE: a read-only session leaves the bootloader waiting");
	static const uint8_t INIT21[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                                   0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
	const uint8_t rd[] = {0x03, 0x10, 0x00, 0x00};
	for (int e = 0; e < 2; e++) {
		g_efm8 = e == 1;
		freshUi(escGreets);
		openBridgeScreen();
		untilOpen();
		fakeCdcDtr(true);
		fakeCdcHostWrite(INIT21, sizeof(INIT21));
		brFrames(2);
		fakeCdcHostWrite(rd, sizeof(rd));
		brFrames(2);
		fakeCdcDtr(false);
		brFrames(2);
		checkTrue(g_efm8 ? "EFM8: greeted and read, no write, no RUN" : "AM32: greeted and read, no write, no RUN",
		          !usbBridgeWriteUnfinished() && usbBridgeRunCue(0));
		tapId(UI_FIX(UI_BACK));
		brFrames(1);
		checkTrue("BACK: the leave page, DShot down", uiCurrentScreen() == UiScreenId::Bridge &&
		          ctlPresent(UI_FIX(BridgeCtl::Restart)) && escTaskSuspended());
		checkTrue("its steps; no FLASH note after a session with no write",
		          leaveStepIs(UI_LEAVE_STEP_Y, '1', BRIDGE_UI_LEAVE_1, true) &&
		          leaveStepIs(UI_LEAVE_STEP_Y + 14, '2', UI_LEAVE_RESTART, false) &&
		          allOf(0, UI_LEAVE_NOTE_Y, GFX_W, 8, C_BG));
		if (g_efm8) fakeDumpFrame("shot_bridge_leave_read.ppm");
		brFrames(44);
		tapId(UI_FIX(BridgeCtl::Restart));
		for (int k = 0; k < 4 && pinClaimState() != ClaimState::Free; k++) brFrames(1);
		checkTrue("RESTART DSHOT: MENU, Free, the cue gone", uiCurrentScreen() == UiScreenId::Menu &&
		          pinClaimState() == ClaimState::Free && !usbBridgeRunCue(0));
		tapId(UI_FIX(UI_BACK));
	}
	g_efm8 = false;

	// MENU shows the bridge's cue: a claim of the bridge ended with no leave
	// page, as no screen of today ends it, and the cue left set.
	freshUi(escGreets);
	tapId(UI_FIX(TesterCtl::Menu));
	brFrames(1);
	pinClaimBegin(PinUser::Bridge, PIN_SIG);
	for (int k = 0; k < 20 && usbBridgeMode() == BridgeMode::Closed; k++) {
		pinClaimPoll();
		brFrames(1);
	}
	fakeCdcHostWrite(INIT21, sizeof(INIT21));
	brFrames(3);
	pinClaimEnd(PinUser::Bridge);
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) brFrames(1);
	brFrames(2);
	checkTrue("MENU: the strip's second line reads POWER-CYCLE THE ESC TO RUN IT",
	          usbBridgeRunCue(0) && uiCurrentScreen() == UiScreenId::Menu &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_RUN_CUE, UI_COL_WARN, C_BG, 1));
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeAdvance(5);
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	brFrames(2);
	checkTrue("an eRPM frame: the cue gone from MENU", !usbBridgeRunCue(0) &&
	          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_RUN_CUE, UI_COL_WARN, C_BG, 1));
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	tapId(UI_FIX(UI_BACK));

	// Every BACK on a Running bridge gives the page: no traffic at all, an
	// INIT handed over and not yet answered, MSP alone.
	const uint8_t msp[] = {'$', 'M', '<', 0, 1, 1};
	for (int c = 0; c < 3; c++) {
		freshUi(c == 1 ? escGreets : nullptr);
		openBridgeScreen();
		untilOpen();
		if (c == 1) {
			fakeCdcHostWrite(INIT21, sizeof(INIT21));
			for (int k = 0; k < 40 && usbBridgeLast().cmd != BridgeCmd::Init; k++) {
				fakeAdvanceUs(250);
				core1JobPoll();
				tud_task();
				usbBridgeTask();
			}
			UsbBridgeStats st;
			usbBridgeStats(&st);
			checkTrue("an INIT handed over, its answer not in", usbBridgeLast().cmd == BridgeCmd::Init &&
			          st.toHost == 0);
			const UiRect back = uiBackRect();
			fakePress(back.x + 10, back.y + 10);
			uiTick();
			fakeRelease();
			uiTick();
		} else {
			if (c == 2) {
				fakeCdcHostWrite(msp, sizeof(msp));
				brFrames(4);
			}
			tapId(UI_FIX(UI_BACK));
		}
		brFrames(1);
		static const char *const WHAT[] = {
			"no traffic at all: BACK gives the leave page",
			"BACK between the INIT's handover and its answer: the leave page",
			"MSP alone, nothing answered: the leave page",
		};
		checkTrue(WHAT[c], uiCurrentScreen() == UiScreenId::Bridge && ctlPresent(UI_FIX(BridgeCtl::Restart)) &&
		          escTaskSuspended() && allOf(0, UI_LEAVE_NOTE_Y, GFX_W, 8, C_BG));
		brFrames(44);
		tapId(UI_FIX(BridgeCtl::Restart));
		for (int k = 0; k < 4 && pinClaimState() != ClaimState::Free; k++) brFrames(1);
		checkTrue("RESTART DSHOT: MENU, Free", uiCurrentScreen() == UiScreenId::Menu &&
		          pinClaimState() == ClaimState::Free);
		tapId(UI_FIX(UI_BACK));
	}
}

void runBridgeUiTests() {
	g_together = 0;
	testLayout();
	testTraffic();
	testWriteAndLeave();
	testBackStates();
	testLostAndGp1();
	testReadOnly();
	checkInt("DShot never up with the bridge's line", g_together, 0);
	freshUi();
	fakeBridgeLineReset();
	fakeCdcReset();
}
