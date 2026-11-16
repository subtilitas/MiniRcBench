/**
 * @file test_esc_cmd_ui.cpp
 * @brief ESC COMMANDS through the real UI: its entry of ESC CONFIG, its layout and
 *        texts, every control and refusal, the holds at 999 and 1000 ms,
 *        BACK while a request waits, and the tester's 3D note.
 *
 * Core1 is the fake of fakes.cpp: the test sets the readiness core1 would
 * publish (fakeSetCmdReady()) and the state of the last sequence
 * (fakeSetCmdState()); a request escRequestCommand() takes reads Waiting
 * until then. The slot's own rules are tested in test_esc_cmd.cpp. What an
 * ESC does with the commands is not tested here and not verified on
 * hardware.
 */

#include "check.h"
#include "fakes.h"
#include "settings.h"
#include "ui.h"
#include "ui_esc_cmd.h"
#include "ui_esc_config.h"
#include "ui_menu.h"
#include "ui_widgets.h"
#include "ui_hold.h"
#include "esc_3d.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "core1_job.h"
#include "dshot/dshot.h"
#include "gfx.h"
#include "theme.h"
#include "plat.h"

#include <stdio.h>
#include <string.h>

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void ecFrames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiTick(); }
}
#define UI_IDS_FRAMES(n) ecFrames(n)
#include "ui_ids.h"

/** @brief One UI frame @p ms after the last, the finger held where it is. */
static void ecFrameAfter(uint32_t ms) {
	fakeAdvance(ms);
	fakePumpPoll();
	core1JobPoll();
	uiTick();
}

/** @brief An ESC answering eRPM 0 with EDT, its frames stamped now. */
static void escAnswering() {
	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.lastRpmMs = millis();
	tel.edtVoltsMs = millis();
	tel.volts = 16.0f;
	tel.packetRate = 998;
	fakeSetTelemetry(&tel);
}

/** @brief A fresh tester: no request, Ready, no 3D note, nothing on the pins. */
static void freshEc() {
	fakeFlashClear();
	settingsLoad();
	uiInitTester();
	fakeSetCmdReady(EscCmdReady::Ready);
	fakeSetCmdState(EscCmdState::Idle, 0);
	esc3dSetNote(false);
	EscTelemetry none;
	memset(&none, 0, sizeof(none));
	fakeSetTelemetry(&none);
	ecFrames(2);
}

/** @brief MENU, ESC CONFIG, the hub, then its entry ESC COMMANDS. */
static void openEc() {
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(toolEntry(&UI_ESC_CMD));
	ecFrames(1);
}

/** @brief Line @p i of the message lines reads @p text in @p col. */
static bool lineIs(int i, const char *text, uint16_t col) {
	return fakeTextIs(UI_MARGIN, UI_ESC_CMD_LINE_Y + i * UI_ESC_CMD_LINE_H, text, col, C_BG, 1);
}

/** @brief Line @p i is blank: nothing but the background from x 0 to 239. */
static bool lineBlank(int i) {
	int x0 = 0, x1 = 0;
	return !fakeInkExtent(UI_ESC_CMD_LINE_Y + i * UI_ESC_CMD_LINE_H, 8, C_BG, &x0, &x1);
}

/** @brief The idle lines, dim, the fourth blank. */
static bool idleShown() {
	return lineIs(0, UI_ESC_CMD_IDLE_1, C_DIM) && lineIs(1, UI_ESC_CMD_IDLE_2, C_DIM) &&
	       lineIs(2, UI_ESC_CMD_CHECK, C_DIM) && lineBlank(3) &&
	       lineIs(4, UI_ESC_CMD_IDLE_5, C_DIM) && lineIs(5, UI_ESC_CMD_IDLE_6, C_DIM);
}

/** @brief Lines @p from to 5 are blank. */
static bool blankFrom(int from) {
	for (int i = from; i < UI_ESC_CMD_LINES; i++)
		if (!lineBlank(i)) return false;
	return true;
}

static const EscCmdCtl ALL[] = {
	EscCmdCtl::Beep1, EscCmdCtl::Beep2, EscCmdCtl::Beep3, EscCmdCtl::Beep4, EscCmdCtl::Beep5,
	EscCmdCtl::Normal, EscCmdCtl::Reversed, EscCmdCtl::Off3d, EscCmdCtl::On3d,
};

/** @brief Every control acts, or none does, as @p live says. */
static bool allControls(bool live) {
	for (EscCmdCtl c : ALL)
		if (!ctlPresent(UI_FIX(c)) || ctlEnabled(UI_FIX(c)) != live) return false;
	return true;
}

/** @brief `NO ESC ANSWERS ON SIG GPn.` for the pin the pump drives. */
static const char *noAnswerLine() {
	static char b[40];
	snprintf(b, sizeof(b), UI_ESC_CMD_NO_ANSWER_FMT, (unsigned)escTaskDshotPin());
	return b;
}

/** @brief A pixel of the frame. */
static uint16_t px(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/** @brief Control @p c is drawn live, its fill the panel, or disabled, an outline. */
static bool drawnLive(EscCmdCtl c, bool live) {
	UiRect r = ctlRect(UI_FIX(c));
	return px(r.x + 6, r.y + r.h - 5) == (live ? C_PANEL : C_BG);
}

static void testRowAndOpen() {
	section("ESC COMMANDS: ESC CONFIG's first entry, open only while the claim is Free");
	freshEc();
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); ecFrames(2);
	checkTrue("armed on the tester", uiArmed() && fakeArmed());
	openEc();
	checkTrue("ESC CONFIG's first entry opens ESC COMMANDS, over the hub",
	          navIsTool(UiScreenId::EscConfig, UiScreenId::EscCmd) &&
	          UI_ESC_CFG_ENTRIES[0].screen == &UI_ESC_CMD && uiMenuRowOf(&UI_ESC_CMD) == -1);
	checkTrue("disarmed on the way in, the throttle 0",
	          !uiArmed() && !fakeArmed() && uiThrottle() == 0);
	checkTrue("it claims nothing: DShot runs on SIG", pinClaimState() == ClaimState::Free);
	checkTrue("its screen: no pin user, opens only while the pins are free",
	          UI_ESC_CMD.pinUser == PinUser::None && UI_ESC_CMD.freePins && !UI_ESC_CMD.action &&
	          !UI_ESC_CMD.status);
	checkControlTable("ESC COMMANDS");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK returns to ESC CONFIG, the hub that opened it", navIsHub(UiScreenId::EscConfig));

	checkTrue("another claim holds the pins",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	ecFrames(2);
	tapId(UI_FIX(EscCfgCtl::Commands), IdExpect::Disabled);
	checkTrue("then its entry is gated and opens nothing", navIsHub(UiScreenId::EscConfig));
	pinClaimEnd(PinUser::Am32);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) ecFrames(1);
	ecFrames(1);
	tapId(UI_FIX(EscCfgCtl::Commands));
	checkTrue("once Free it opens", uiCurrentScreen() == UiScreenId::EscCmd);
	tapId(UI_FIX(UI_BACK));
	backToTester();
}

static void testLayout() {
	section("ESC COMMANDS: layout, texts and widths");
	freshEc();
	escAnswering();
	openEc();
	checkTrue("the title, 142 px at scale 2 from x 8, 21 px clear of BACK at x 170",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_ESC_CMD_TITLE, C_TEXT, C_PANEL, 2) &&
	          gfxTextW(UI_ESC_CMD_TITLE, 2) == 142 && UI_MARGIN + 142 + 20 <= UI_BACK_X);
	char cap[32];
	snprintf(cap, sizeof(cap), UI_ESC_CMD_STRIP_FMT, (unsigned)escTaskDshotPin(),
	         (unsigned)escTaskDshotKbaud());
	{
		char want[32];
		snprintf(want, sizeof(want), "SIG GP%u  DSHOT600", (unsigned)settings()->dshotPin);
		checkStr("the strip's caption: SIG's pin and the rate", cap, want);
	}
	checkTrue("dim at x 8, y 44, 101 px", fakeTextIs(UI_MARGIN, 44, cap, C_DIM, C_BG, 1) &&
	          gfxTextW(cap, 1) == 101);
	checkTrue("its widest, SIG GP1  DSHOT1200, 107 px, ends 64 px before EDT OFF at x 179",
	          gfxTextW("SIG GP1  DSHOT1200", 1) == 107 &&
	          uiChipX(UI_MENU_EDT_OFF) - (UI_MARGIN + 107) == 64);
	{
		const int w = gfxTextW(UI_MENU_EDT_ON, 1) + 12, x = GFX_W - UI_MARGIN - w;
		checkTrue("the chip: EDT ON, green, as MENU's",
		          fakeTextIs(x + 6, UI_STRIP_Y + (UI_STRIP_H - 7) / 2, UI_MENU_EDT_ON, C_ONACCENT,
		                     C_GREEN, 1));
	}
	checkTrue("the captions BEEP, DIRECTION, 3D MODE, dim at y 62, 120, 182",
	          fakeTextIs(UI_MARGIN, 62, "BEEP", C_DIM, C_BG, 1) &&
	          fakeTextIs(UI_MARGIN, 120, "DIRECTION", C_DIM, C_BG, 1) &&
	          fakeTextIs(UI_MARGIN, 182, "3D MODE", C_DIM, C_BG, 1));
	bool beeps = true;
	for (int i = 0; i < 5; i++) {
		UiRect r = ctlRect(UI_FIX((uint8_t)EscCmdCtl::Beep1 + i));
		char n[2] = { (char)('1' + i), '\0' };
		beeps = beeps && r.x == 6 + 47 * i && r.y == 72 && r.w == 40 && r.h == 40 &&
		        fakeTextIs(r.x + (40 - 10) / 2, r.y + 13, n, C_TEXT, C_PANEL, 2);
	}
	checkTrue("BEEP 1 to 5: 40 x 40 px at x 6 + 47i, y 72, the digit centred at scale 2", beeps);
	struct H { EscCmdCtl id; int x, y; const char *label; int w; };
	const H holds[] = {
		{ EscCmdCtl::Normal, 6, 130, "NORMAL", 70 },  { EscCmdCtl::Reversed, 123, 130, "REVERSED", 94 },
		{ EscCmdCtl::Off3d, 6, 192, "3D OFF", 70 },   { EscCmdCtl::On3d, 123, 192, "3D ON", 58 },
	};
	for (const H &h : holds) {
		UiRect r = ctlRect(UI_FIX(h.id));
		char what[96];
		snprintf(what, sizeof(what), "%s: 111 x 44 px at (%d, %d), %d px at scale 2, centred",
		         h.label, h.x, h.y, h.w);
		checkTrue(what, r.x == h.x && r.y == h.y && r.w == 111 && r.h == 44 &&
		          gfxTextW(h.label, 2) == h.w &&
		          fakeTextIs(r.x + (111 - h.w) / 2, r.y + 15, h.label, C_TEXT, C_PANEL, 2));
	}
	checkTrue("REVERSED leaves 8 px each side", (111 - gfxTextW("REVERSED", 2)) / 2 == 8);
	checkTrue("idle: the six lines, dim, from y 244 at a 12 px pitch", idleShown());
	int lx0 = 0, lx1 = 0;
	checkTrue("the last line ends at y 310", !fakeInkExtent(311, GFX_H - 311, C_BG, &lx0, &lx1));
	{
		int x0 = 0, x1 = 0;
		checkTrue("nothing under the strip reaches past x 6 or x 233",
		          fakeInkExtent(UI_BODY_Y, GFX_H - UI_BODY_Y, C_BG, &x0, &x1) && x0 >= 6 && x1 <= 233);
	}
	// Every text against the widths the note gives.
	struct W { const char *t; int w; };
	const W widths[] = {
		{ UI_ESC_CMD_IDLE_1, 197 }, { UI_ESC_CMD_IDLE_2, 221 }, { UI_ESC_CMD_CHECK, 209 },
		{ UI_ESC_CMD_IDLE_5, 221 }, { UI_ESC_CMD_IDLE_6, 143 },
		{ "SENDING 8 (REVERSED), THEN 12 (SAVE).", 221 }, { "SENT 8 (REVERSED), THEN 12 (SAVE).", 203 },
		{ "SENT 7 (NORMAL), THEN 12 (SAVE).", 191 }, { UI_ESC_CMD_UNCONFIRMED, 209 },
		{ "SENDING 10 (3D ON), THEN 12 (SAVE).", 209 }, { "SENDING 9 (3D OFF), THEN 12 (SAVE).", 209 },
		{ "SENT 10 (3D ON), THEN 12 (SAVE).", 191 }, { UI_ESC_CMD_3D_NOW, 197 },
		{ UI_ESC_CMD_3D_LOW, 215 }, { UI_ESC_CMD_3D_HIGH, 191 }, { UI_ESC_CMD_3D_ENDS, 209 },
		{ "SENT 9 (3D OFF), THEN 12 (SAVE).", 191 }, { UI_ESC_CMD_3D_STAYS, 191 },
		{ UI_ESC_CMD_3D_OFF_LOST, 221 },
		{ "SENDING 3 (BEEP 3).", 113 }, { "SENT 3 (BEEP 3).", 95 },
		{ "NO ESC ANSWERS ON SIG GP0.", 155 }, { UI_ESC_CMD_TURNING, 209 },
		{ UI_ESC_CMD_SETTLING, 149 }, { UI_ESC_CMD_NOT_SENT, 215 }, { UI_ESC_CMD_SAVE_LOST, 209 },
		{ UI_ESC_CMD_EXPIRED, 191 },
	};
	bool fit = true;
	for (const W &w : widths) {
		bool ok = gfxTextW(w.t, 1) == w.w && UI_MARGIN + w.w <= 229;
		if (!ok) printf("    %s: %d px, want %d\n", w.t, gfxTextW(w.t, 1), w.w);
		fit = fit && ok;
	}
	checkTrue("every message line at its width, within x 8 to 228", fit);
	escAnswering();
	ecFrames(1);
	fakeDumpFrame("shot_esccmd.ppm");
	tapId(UI_FIX(UI_BACK));
	backToTester();
}

static void testReadiness() {
	section("ESC COMMANDS: live only at Ready, the amber line says why");
	freshEc();
	openEc();
	checkTrue("Ready: every control acts, drawn live", allControls(true) &&
	          drawnLive(EscCmdCtl::Beep1, true) && drawnLive(EscCmdCtl::On3d, true));
	struct R { EscCmdReady r; const char *line; const char *what; };
	const R refused[] = {
		{ EscCmdReady::NoAnswer, noAnswerLine(), "no ESC answering" },
		{ EscCmdReady::Turning, UI_ESC_CMD_TURNING, "the motor turning" },
		{ EscCmdReady::Settling, UI_ESC_CMD_SETTLING, "the ESC settling" },
	};
	for (const R &c : refused) {
		fakeSetCmdReady(c.r);
		ecFrames(1);
		char what[96];
		snprintf(what, sizeof(what), "%s: every control drawn disabled and gated", c.what);
		checkTrue(what, allControls(false) && drawnLive(EscCmdCtl::Beep3, false) &&
		          drawnLive(EscCmdCtl::Reversed, false));
		snprintf(what, sizeof(what), "%s: the one amber line, %s", c.what, c.line);
		checkTrue(what, lineIs(0, c.line, C_AMBER) && blankFrom(1));
		int before = fakeBeepRequests();
		tapId(UI_FIX(EscCmdCtl::Beep1), IdExpect::Disabled);
		pressId(UI_FIX(EscCmdCtl::Normal), IdExpect::Disabled);
		holdId(UI_FIX(EscCmdCtl::Normal), 50);
		fakeRelease(); ecFrames(1);
		snprintf(what, sizeof(what), "%s: a tap and a 1250 ms hold send nothing", c.what);
		checkTrue(what, fakeBeepRequests() == before);
		checkTrue("BACK stays live", ctlEnabled(UI_FIX(UI_BACK)));
		if (c.r == EscCmdReady::NoAnswer) {
			EscTelemetry none;
			memset(&none, 0, sizeof(none));
			fakeSetTelemetry(&none);
			ecFrames(1);
			checkTrue("no eRPM: NO ESC, the neutral chip: frame, C_BG inside, C_DIM label, as MENU's",
			          neutralChipIs(UI_MENU_NO_ESC));
			fakeDumpFrame("shot_esccmd_no_esc.ppm");
		}
	}
	// The pin in the line is SIG's: swap SIG and AUX.
	const FakeWiring w = fakeWiring();
	const uint8_t other = w.dshotPin == 1 ? 0 : 1;
	escTaskConfigure(other, w.dshotKbaud, w.kissEnable, w.dshotPin);
	fakeSetCmdReady(EscCmdReady::NoAnswer);
	ecFrames(1);
	{
		char line[40], strip[32];
		snprintf(line, sizeof(line), "NO ESC ANSWERS ON SIG GP%u.", (unsigned)other);
		snprintf(strip, sizeof(strip), "SIG GP%u  DSHOT600", (unsigned)other);
		checkTrue("SIG swapped: the line and the strip name the other pin",
		          lineIs(0, line, C_AMBER) && fakeTextIs(UI_MARGIN, 44, strip, C_DIM, C_BG, 1));
	}
	escTaskConfigure(w.dshotPin, w.dshotKbaud, w.kissEnable, w.kissPin);
	ecFrames(2);
	fakeSetCmdReady(EscCmdReady::Busy);
	ecFrames(1);
	checkTrue("Busy, an EDT enable going out: live, the idle lines",
	          allControls(true) && drawnLive(EscCmdCtl::Beep1, true) && idleShown());
	{
		int before = fakeBeepRequests();
		bool save = true;
		tapId(UI_FIX(EscCmdCtl::Beep1));
		checkTrue("a tap on BEEP 1 at Busy is taken, beacon 1 without a save",
		          fakeBeepRequests() == before + 1 && fakeLastCommand(&save) == 1 && !save);
		checkTrue("and waits: SENDING 1 (BEEP 1)., every control and BACK disabled",
		          escCommandState(nullptr) == EscCmdState::Waiting &&
		          lineIs(0, "SENDING 1 (BEEP 1).", C_TEXT) && allControls(false) &&
		          !ctlEnabled(UI_FIX(UI_BACK)));
		fakeSetCmdState(EscCmdState::Sent, 1);
		ecFrames(1);
	}
	fakeSetCmdReady(EscCmdReady::Ready);
	ecFrames(1);
	checkTrue("Ready again: live", allControls(true) && lineIs(0, "SENT 1 (BEEP 1).", C_TEXT));
	tapId(UI_FIX(UI_BACK));
	backToTester();
}

static void testBeeps() {
	section("ESC COMMANDS: BEEP 1 to 5, a tap each");
	freshEc();
	openEc();
	for (int n = 1; n <= 5; n++) {
		int before = fakeBeepRequests();
		bool save = true;
		tapId(UI_FIX((uint8_t)EscCmdCtl::Beep1 + n - 1));
		char what[96], line[40];
		snprintf(what, sizeof(what), "BEEP %d asks for beacon %d without a save", n, n);
		checkTrue(what, fakeBeepRequests() == before + 1 && fakeLastCommand(&save) == n && !save);
		snprintf(line, sizeof(line), "SENDING %d (BEEP %d).", n, n);
		snprintf(what, sizeof(what), "waiting: %s, every control and BACK disabled", line);
		checkTrue(what, lineIs(0, line, C_TEXT) && blankFrom(1) && allControls(false) &&
		          !ctlEnabled(UI_FIX(UI_BACK)));
		if (n == 3) fakeDumpFrame("shot_esccmd_sending.ppm");
		tapId(UI_FIX(EscCmdCtl::Beep5), IdExpect::Disabled);
		checkInt("a tap on BEEP 5 meanwhile sends nothing", fakeBeepRequests(), before + 1);
		fakeSetCmdState(EscCmdState::Sent, (uint8_t)n);
		fakeSetCmdReady(EscCmdReady::Busy);
		ecFrames(1);
		snprintf(line, sizeof(line), "SENT %d (BEEP %d).", n, n);
		snprintf(what, sizeof(what), "in the beep's gap, Busy: %s, the controls and BACK live", line);
		checkTrue(what, lineIs(0, line, C_TEXT) && blankFrom(1) && allControls(true) &&
		          ctlEnabled(UI_FIX(UI_BACK)));
		fakeSetCmdReady(EscCmdReady::Ready);
		ecFrames(1);
		checkTrue("the gap over: live, the line stays", allControls(true) && lineIs(0, line, C_TEXT));
	}
	// A press that slides off a BEEP sends nothing.
	int before = fakeBeepRequests();
	int x, y;
	ctlCentre(UI_FIX(EscCmdCtl::Beep2), &x, &y);
	fakePress(x, y); ecFrames(1);
	fakeHold(x, y + 50); ecFrames(1);
	fakeRelease(); ecFrames(1);
	checkInt("a press slid off BEEP 2 sends nothing", fakeBeepRequests(), before);
	// Refused although drawn live: armed, unreachable here, forced.
	escSetArmed(true);
	tapId(UI_FIX(EscCmdCtl::Beep4));
	checkTrue("a refused tap sends nothing and shows the idle lines, no outcome",
	          fakeBeepRequests() == before && idleShown());
	escSetArmed(false);
	tapId(UI_FIX(UI_BACK));
	backToTester();
}

/** @brief Press hold @p c and keep it @p ms, in 25 ms frames and one of the remainder. */
static void holdFor(EscCmdCtl c, uint32_t ms) {
	int x, y;
	ctlCentre(UI_FIX(c), &x, &y);
	fakePress(x, y);
	ecFrameAfter(0);
	uint32_t t = 0;
	while (t + 25 <= ms) { fakeHold(x, y); ecFrameAfter(25); t += 25; }
	if (t < ms) { fakeHold(x, y); ecFrameAfter(ms - t); }
}

static void testHolds() {
	section("ESC COMMANDS: the four holds at 999 and 1000 ms, each then 12");
	freshEc();
	openEc();
	struct H { EscCmdCtl id; uint8_t cmd; const char *name; };
	const H holds[] = {
		{ EscCmdCtl::Normal, DSHOT_CMD_SPIN_DIRECTION_1, "NORMAL" },
		{ EscCmdCtl::Reversed, DSHOT_CMD_SPIN_DIRECTION_2, "REVERSED" },
		{ EscCmdCtl::Off3d, DSHOT_CMD_3D_MODE_OFF, "3D OFF" },
		{ EscCmdCtl::On3d, DSHOT_CMD_3D_MODE_ON, "3D ON" },
	};
	for (const H &h : holds) {
		int before = fakeBeepRequests();
		holdFor(h.id, UI_HOLD_MS - 1);
		char what[96];
		snprintf(what, sizeof(what), "%s held 999 ms: nothing sent, its fill red", h.name);
		UiRect r = ctlRect(UI_FIX(h.id));
		checkTrue(what, fakeBeepRequests() == before && px(r.x + 6, r.y + 6) == C_RED);
		snprintf(what, sizeof(what), "%s: its bar under it, lime, nearly full", h.name);
		checkTrue(what, px(r.x + 1, r.y + r.h + 1) == C_LIME && px(r.x + 105, r.y + r.h + 1) == C_LIME);
		fakeHold(r.x + r.w / 2, r.y + r.h / 2);
		ecFrameAfter(1);
		bool save = false;
		snprintf(what, sizeof(what), "%s at 1000 ms: command %u with the save", h.name, h.cmd);
		checkTrue(what, fakeBeepRequests() == before + 1 && fakeLastCommand(&save) == h.cmd && save);
		char line[40];
		snprintf(line, sizeof(line), UI_ESC_CMD_SENDING_FMT, (unsigned)h.cmd, h.name);
		snprintf(what, sizeof(what), "waiting: %s; controls and BACK disabled", line);
		checkTrue(what, lineIs(0, line, C_TEXT) && blankFrom(1) && allControls(false) &&
		          !ctlEnabled(UI_FIX(UI_BACK)));
		holdFor(h.id, 1500);
		checkInt("the finger left on it sends no more", fakeBeepRequests(), before + 1);
		fakeRelease();
		ecFrames(1);
		fakeSetCmdState(EscCmdState::Sending, h.cmd);
		ecFrames(1);
		checkTrue("sending: the same line, BACK disabled",
		          lineIs(0, line, C_TEXT) && !ctlEnabled(UI_FIX(UI_BACK)));
		tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
		checkTrue("a tap on BACK stays", uiCurrentScreen() == UiScreenId::EscCmd);
		fakeSetCmdState(EscCmdState::Sent, h.cmd);
		ecFrames(1);
		checkTrue("sent: BACK live again", ctlEnabled(UI_FIX(UI_BACK)));
		fakeSetCmdState(EscCmdState::Idle, 0);
		tapId(UI_FIX(UI_BACK));
		tapId(toolEntry(&UI_ESC_CMD));
	}
	// An EDT enable's Busy, 11 ms within a hold, leaves the hold running.
	{
		int before = fakeBeepRequests();
		UiRect n = ctlRect(UI_FIX(EscCmdCtl::Reversed));
		holdFor(EscCmdCtl::Reversed, 500);
		fakeSetCmdReady(EscCmdReady::Busy);
		fakeHold(n.x + n.w / 2, n.y + n.h / 2); ecFrameAfter(25);
		fakeSetCmdReady(EscCmdReady::Ready);
		for (int i = 0; i < 18; i++) { fakeHold(n.x + n.w / 2, n.y + n.h / 2); ecFrameAfter(25); }
		fakeHold(n.x + n.w / 2, n.y + n.h / 2); ecFrameAfter(24);
		checkInt("a hold across a Busy frame: nothing at 999 ms", fakeBeepRequests(), before);
		fakeHold(n.x + n.w / 2, n.y + n.h / 2); ecFrameAfter(1);
		bool save = false;
		checkTrue("and 8 with the save at 1000 ms",
		          fakeBeepRequests() == before + 1 && fakeLastCommand(&save) == 8 && save);
		fakeRelease(); ecFrames(1);
		fakeSetCmdState(EscCmdState::Sent, 8);
		ecFrames(1);
	}
	// A hold slid off resets; the same press back on starts again from 0.
	int before = fakeBeepRequests();
	UiRect r = ctlRect(UI_FIX(EscCmdCtl::Reversed));
	fakePress(r.x + 50, r.y + 20); ecFrameAfter(0);
	for (int i = 0; i < 30; i++) { fakeHold(r.x + 50, r.y + 20); ecFrameAfter(25); }
	fakeHold(r.x + 50, r.y + 80); ecFrameAfter(25);
	for (int i = 0; i < 30; i++) { fakeHold(r.x + 50, r.y + 20); ecFrameAfter(25); }
	checkInt("750 ms, off, 750 ms back on: nothing sent", fakeBeepRequests(), before);
	fakeRelease(); ecFrames(1);
	// The readiness falls mid-hold: the hold ends.
	holdFor(EscCmdCtl::Normal, 500);
	fakeSetCmdReady(EscCmdReady::Turning);
	fakeHold(r.x - 60, r.y + 20); ecFrameAfter(25);
	fakeSetCmdReady(EscCmdReady::Ready);
	{
		UiRect n = ctlRect(UI_FIX(EscCmdCtl::Normal));
		for (int i = 0; i < 40; i++) { fakeHold(n.x + n.w / 2, n.y + n.h / 2); ecFrameAfter(25); }
	}
	checkInt("a hold the readiness broke never fires on the same press", fakeBeepRequests(), before);
	fakeRelease(); ecFrames(1);
	tapId(UI_FIX(UI_BACK));
	backToTester();
}

/** @brief Run a hold of @p c to its request and set the state core1 reports. */
static void sequence(EscCmdCtl c, EscCmdState st, uint8_t cmd) {
	holdFor(c, UI_HOLD_MS);
	fakeRelease();
	ecFrames(1);
	fakeSetCmdState(st, cmd);
	ecFrames(1);
}

static void testOutcomes() {
	section("ESC COMMANDS: each outcome's lines, and the 3D note");
	freshEc();
	openEc();
	sequence(EscCmdCtl::Reversed, EscCmdState::Sent, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("REVERSED sent: three lines, C_TEXT",
	          lineIs(0, "SENT 8 (REVERSED), THEN 12 (SAVE).", C_TEXT) &&
	          lineIs(1, UI_ESC_CMD_UNCONFIRMED, C_TEXT) && lineIs(2, UI_ESC_CMD_CHECK, C_TEXT) &&
	          blankFrom(3));
	checkTrue("no 3D note, no direction note (decision 10)", !esc3dNote());
	sequence(EscCmdCtl::Normal, EscCmdState::Sent, DSHOT_CMD_SPIN_DIRECTION_1);
	checkTrue("NORMAL sent", lineIs(0, "SENT 7 (NORMAL), THEN 12 (SAVE).", C_TEXT) &&
	          lineIs(1, UI_ESC_CMD_UNCONFIRMED, C_TEXT));

	sequence(EscCmdCtl::On3d, EscCmdState::NotSent, DSHOT_CMD_3D_MODE_ON);
	checkTrue("3D ON not sent: the amber line, and no 3D note",
	          lineIs(0, UI_ESC_CMD_NOT_SENT, C_AMBER) && blankFrom(1) && !esc3dNote());
	sequence(EscCmdCtl::On3d, EscCmdState::Expired, DSHOT_CMD_3D_MODE_ON);
	checkTrue("3D ON expired: NOT SENT: THE ESC WAS NOT READY., and no 3D note",
	          lineIs(0, UI_ESC_CMD_EXPIRED, C_AMBER) && blankFrom(1) && !esc3dNote());
	sequence(EscCmdCtl::On3d, EscCmdState::SaveLost, DSHOT_CMD_3D_MODE_ON);
	checkTrue("3D ON with the save lost: two amber lines, the second that the note is set",
	          lineIs(0, UI_ESC_CMD_SAVE_LOST, C_AMBER) && lineIs(1, UI_ESC_CMD_3D_NOW, C_AMBER) &&
	          blankFrom(2));
	checkTrue("and the 3D note stands: the ESC may run in 3D", esc3dNote());
	sequence(EscCmdCtl::Normal, EscCmdState::Sent, DSHOT_CMD_SPIN_DIRECTION_1);
	esc3dSetNote(false);
	sequence(EscCmdCtl::On3d, EscCmdState::Sent, DSHOT_CMD_3D_MODE_ON);
	checkTrue("3D ON sent: three lines C_TEXT, two C_AMBER",
	          lineIs(0, "SENT 10 (3D ON), THEN 12 (SAVE).", C_TEXT) &&
	          lineIs(1, UI_ESC_CMD_3D_NOW, C_TEXT) && lineIs(2, UI_ESC_CMD_3D_ENDS, C_TEXT) &&
	          lineIs(3, UI_ESC_CMD_3D_LOW, C_AMBER) && lineIs(4, UI_ESC_CMD_3D_HIGH, C_AMBER) &&
	          lineBlank(5));
	checkTrue("the 3D note stands", esc3dNote());
	escAnswering();
	ecFrames(1);
	fakeDumpFrame("shot_esccmd_3d_on.ppm");
	fakeSetCmdReady(EscCmdReady::NoAnswer);
	ecFrames(1);
	checkTrue("the ESC gone after it: the outcome stays, the reason on the sixth line",
	          lineIs(0, "SENT 10 (3D ON), THEN 12 (SAVE).", C_TEXT) &&
	          lineIs(4, UI_ESC_CMD_3D_HIGH, C_AMBER) &&
	          lineIs(5, noAnswerLine(), C_AMBER) && allControls(false));
	fakeSetCmdReady(EscCmdReady::Ready);
	ecFrames(1);
	sequence(EscCmdCtl::Off3d, EscCmdState::Sent, DSHOT_CMD_3D_MODE_OFF);
	checkTrue("3D OFF sent with the note standing: two lines",
	          lineIs(0, "SENT 9 (3D OFF), THEN 12 (SAVE).", C_TEXT) &&
	          lineIs(1, UI_ESC_CMD_3D_STAYS, C_TEXT) && blankFrom(2));
	checkTrue("and the 3D note stays (decision 4)", esc3dNote());
	tapId(UI_FIX(UI_BACK));
	backToTester();
	ecFrames(2);
	checkTrue("the tester shows the note: " ESC_3D_TESTER_NOTE,
	          fakeTextIs(10, 114, ESC_3D_TESTER_NOTE, C_AMBER, C_BG, 1));
	checkTrue("33 characters, 197 px, from x 10 to x 206",
	          gfxTextW(ESC_3D_TESTER_NOTE, 1) == 197);

	// Opened again: idle, whatever the last sequence was.
	openEc();
	checkTrue("opened again: the idle lines", idleShown());
	esc3dSetNote(false);
	sequence(EscCmdCtl::Off3d, EscCmdState::Sent, DSHOT_CMD_3D_MODE_OFF);
	checkTrue("3D OFF sent with no note standing: none set, and no line about it",
	          !esc3dNote() && lineIs(0, "SENT 9 (3D OFF), THEN 12 (SAVE).", C_TEXT) &&
	          blankFrom(1));
	sequence(EscCmdCtl::Off3d, EscCmdState::SaveLost, DSHOT_CMD_3D_MODE_OFF);
	checkTrue("3D OFF with the save lost: the 3D line, not the direction check",
	          lineIs(0, UI_ESC_CMD_SAVE_LOST, C_AMBER) &&
	          lineIs(1, UI_ESC_CMD_3D_OFF_LOST, C_AMBER) && blankFrom(2));
	sequence(EscCmdCtl::Reversed, EscCmdState::SaveLost, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("REVERSED with the save lost: the direction check",
	          lineIs(0, UI_ESC_CMD_SAVE_LOST, C_AMBER) && lineIs(1, UI_ESC_CMD_CHECK, C_AMBER) &&
	          blankFrom(2));
	tapId(UI_FIX(UI_BACK));
	backToTester();
}

static void testBackCancels() {
	section("ESC COMMANDS: BACK cancels a request, and is disabled while one waits");
	freshEc();
	openEc();
	int cancels = fakeCommandCancels();
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK leaves to ESC CONFIG and cancels", uiCurrentScreen() == UiScreenId::EscConfig &&
	          fakeCommandCancels() == cancels + 1);
	tapId(toolEntry(&UI_ESC_CMD));
	tapId(UI_FIX(EscCmdCtl::Beep2));
	const UiRect back = ctlRect(UI_FIX(UI_BACK));
	checkTrue("a beacon waits: BACK disabled, drawn as an outline", !ctlEnabled(UI_FIX(UI_BACK)) &&
	          escCommandState(nullptr) == EscCmdState::Waiting &&
	          px(back.x + 6, back.y + back.h - 5) == C_BG);
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("and a tap on it leaves nothing, cancels nothing",
	          uiCurrentScreen() == UiScreenId::EscCmd && fakeCommandCancels() == cancels + 1);
	fakeSetCmdState(EscCmdState::Expired, DSHOT_CMD_BEEP2);
	ecFrames(1);
	checkTrue("expired: the amber line, BACK live, drawn filled",
	          lineIs(0, UI_ESC_CMD_EXPIRED, C_AMBER) && ctlEnabled(UI_FIX(UI_BACK)) &&
	          px(back.x + 6, back.y + back.h - 5) == C_PANEL);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK cancels again", fakeCommandCancels() == cancels + 2);
	backToTester();
	fakeSetCmdState(EscCmdState::Idle, 0);

	// A press on BACK begun while 3D ON goes out, lifted on the first frame
	// that reads Sent: BACK does not act on it, and the note is set.
	esc3dSetNote(false);
	openEc();
	holdFor(EscCmdCtl::On3d, UI_HOLD_MS);
	fakeRelease();
	ecFrames(1);
	fakeSetCmdState(EscCmdState::Sending, DSHOT_CMD_3D_MODE_ON);
	ecFrames(1);
	cancels = fakeCommandCancels();
	fakePress(back.x + back.w / 2, back.y + back.h / 2);
	ecFrames(2);
	fakeSetCmdState(EscCmdState::Sent, DSHOT_CMD_3D_MODE_ON);
	fakeRelease();
	ecFrames(1);
	checkTrue("BACK pressed while 3D ON goes out, lifted at Sent: the page stays, the note is set",
	          uiCurrentScreen() == UiScreenId::EscCmd && esc3dNote() &&
	          fakeCommandCancels() == cancels);
	// The same with the press lifted on the frame of SaveLost.
	esc3dSetNote(false);
	tapId(UI_FIX(UI_BACK));
	tapId(toolEntry(&UI_ESC_CMD));
	holdFor(EscCmdCtl::On3d, UI_HOLD_MS);
	fakeRelease();
	ecFrames(1);
	fakePress(back.x + back.w / 2, back.y + back.h / 2);
	ecFrames(2);
	fakeSetCmdState(EscCmdState::SaveLost, DSHOT_CMD_3D_MODE_ON);
	fakeRelease();
	ecFrames(1);
	checkTrue("lifted at SaveLost: the page stays, the note is set",
	          uiCurrentScreen() == UiScreenId::EscCmd && esc3dNote());
	tapId(UI_FIX(UI_BACK));
	checkTrue("a fresh tap on BACK then leaves", uiCurrentScreen() == UiScreenId::EscConfig);

	// A press on BEEP 2 begun while no ESC answered, lifted once Ready.
	tapId(toolEntry(&UI_ESC_CMD));
	fakeSetCmdReady(EscCmdReady::NoAnswer);
	ecFrames(1);
	int before = fakeBeepRequests();
	const UiRect b2 = ctlRect(UI_FIX(EscCmdCtl::Beep2));
	fakePress(b2.x + b2.w / 2, b2.y + b2.h / 2);
	ecFrames(1);
	fakeSetCmdReady(EscCmdReady::Ready);
	ecFrames(2);
	fakeRelease();
	ecFrames(1);
	checkInt("a press on BEEP 2 that landed while it was disabled sends nothing when lifted live",
	         fakeBeepRequests(), before);
	tapId(UI_FIX(EscCmdCtl::Beep2));
	checkInt("a fresh tap sends beacon 2", fakeBeepRequests(), before + 1);
	fakeSetCmdState(EscCmdState::Sent, 2);
	ecFrames(1);
	tapId(UI_FIX(UI_BACK));
	backToTester();
	fakeSetCmdState(EscCmdState::Idle, 0);
	esc3dSetNote(false);
}

void runEscCmdUiTests() {
	testRowAndOpen();
	testLayout();
	testReadiness();
	testBeeps();
	testHolds();
	testOutcomes();
	testBackCancels();
	freshEc();
}
