/**
 * @file test_dshot_life.cpp
 * @brief Slice D2 of docs/design/dshot-lifetime.md: boot parked, the rule,
 *        the claim's Busy and Parking, the arm gate, the hubs' line and the
 *        texts that state DShot's restart, through the real UI on the fake
 *        pump of fakes.cpp.
 *
 * Core1 is not a thread here: lifeFrame() runs the pump's pass and the job's
 * pass, then the UI frame, as the other UI suites do; framesNoPump() holds
 * core1's pump back for the frames between a tap and the bind.
 */

#include "check.h"
#include "fakes.h"
#include "esc_task.h"
#include "core1_job.h"
#include "pin_claim.h"
#include "settings.h"
#include "config.h"
#include "ui.h"
#include "ui_screen.h"
#include "ui_menu.h"
#include "ui_hub.h"
#include "ui_leave.h"
#include "ui_servo.h"
#include "ui_esc_cmd.h"
#include "ui_sbus.h"
#include "ui_sbus_out.h"
#include "ui_am32.h"
#include "ui_hitec.h"
#include "ui_settings.h"
#include "ui_sdlog.h"
#include "ui_widgets.h"
#include "gfx.h"
#include "theme.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "plat.h"

#include <stdio.h>
#include <string.h>

/** @brief One frame @p ms after the last: core1's pump and job passes, then the UI. */
static void lifeFrame(uint32_t ms) {
	fakeAdvance(ms);
	fakePumpPoll();
	core1JobPoll();
	uiTick();
}

/** @brief @p n frames of 25 ms. */
static void lifeFrames(int n) {
	for (int i = 0; i < n; i++) lifeFrame(25);
}

/** @brief @p n frames of 25 ms with core1's pump held back. */
static void framesNoPump(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); core1JobPoll(); uiTick(); }
}

#define UI_IDS_FRAMES(n) lifeFrames(n)
#include "ui_ids.h"

/** @brief The board's default SIG and AUX, as the stored defaults put them. */
#define SIG_GPIO 0
#define AUX_GPIO 1

/** @brief SIG parked: an SIO output driving low. */
static bool sigParked(unsigned gpio) {
	FakePin p = fakePin(gpio);
	return p.function == GPIO_FUNC_SIO && p.output && !p.high;
}

/** @brief AUX parked: an input with the pull-down on and the input buffer off. */
static bool auxParked(unsigned gpio) {
	FakePin p = fakePin(gpio);
	return !p.output && p.pull == FakePull::Down && p.inputOff;
}

/** @brief Whether every pixel of the rectangle is @p c. */
static bool allOf(int x, int y, int w, int h, uint16_t c) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (gfxBuffer()[j * GFX_W + i] != c) return false;
	return true;
}

/** @brief Columns of the arm badge's progress bar drawn, at y 21. */
static int badgeProgress() {
	int n = 0;
	for (int x = 4; x < 96; x++) n += gfxBuffer()[21 * GFX_W + x] == C_LIME;
	return n;
}

/** @brief The tester's track reads the arm gate's text, amber. */
static bool gateTextShown() {
	const UiRect t = ctlRect(UI_FIX(TesterCtl::Throttle));
	return fakeTextIs(t.x + 8, t.y + (t.h - 7) / 2, UI_TESTER_GATE_TEXT, C_AMBER, C_PANEL, 1);
}

/** @brief HOLD TO ARM drawn disabled: an outline with the background inside. */
static bool armDrawnDisabled() {
	const UiRect r = ctlRect(UI_FIX(TesterCtl::Arm));
	return gfxBuffer()[(r.y + r.h / 2) * GFX_W + r.x + 4] == C_BG;
}

/** @brief A fresh board on MENU, the pump parked, no card held, no ESC. */
static void freshLife() {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakeMscSet(MscState::Idle, 0);
	fakeTouchAnswers(true);
	fakeFlashClear();
	settingsLoad();
	uiInitMenu();
	escTaskConfigure(settings()->dshotPin, settings()->dshotKbaud, settings()->kissEnable,
	                 settings()->kissPin);
	lifeFrames(2);
}

/** @brief Tap MENU's row ESC TESTER, and the frames until core1 binds. */
static void openTesterBound() {
	tapId(menuRow(&UI_TESTER));
	lifeFrames(2);
}

/** @brief Frames until the arm gate releases. */
static void waitGate() {
	for (int k = 0; k < 100 && uiArmGated(escTaskBoundMs(), millis()); k++) lifeFrames(1);
}

/** @brief Frame by 1 ms until the clock reads @p t. */
static void frameAt(uint32_t t) {
	uint32_t now = millis();
	if ((int32_t)(t - now) > 0) lifeFrame(t - now);
}

// ---------------------------------------------------------------------------

/**
 * @brief Power-up: escTaskInit() parks SIG low and AUX on its pull-down on
 *        core0, and uiInit() opens MENU with no bind and no frame.
 */
static void testBoot() {
	section("DShot lifetime: power-up parks, MENU binds nothing");

	freshLife();
	fakeFlashClear();
	settingsLoad();
	fakePumpPowerOn();
	fakePinsReset();
	int binds = fakeRebuildCount(), frames = fakeFrameCount(), starts = fakeResumeCount();
	escTaskInit();
	checkTrue("escTaskInit() parks: SIG driven low, AUX on its pull-down, the buffer off",
	          escTaskParked() && sigParked(settings()->dshotPin) && auxParked(settings()->kissPin) &&
	          fakePin(settings()->dshotPin).highEnables == 0);
	uiInit();
	checkTrue("after uiInit(): MENU, the pump parked, no bind",
	          uiCurrentScreen() == UiScreenId::Menu && escTaskParked() && fakeRebuildCount() == binds);
	checkTrue("Run never asked", fakeResumeCount() == starts && !uiDshotWanted());
	lifeFrames(40);
	checkTrue("a second of MENU: parked, no bind, no frame, the bind stamp 0",
	          escTaskParked() && fakeRebuildCount() == binds && fakeFrameCount() == frames &&
	          escTaskBoundMs() == 0);
	checkTrue("MENU's strip holds no chip: its right end is the background",
	          allOf(170, UI_STRIP_Y, GFX_W - 170, UI_STRIP_H, C_BG) &&
	          !fakeTextIs(GFX_W - UI_MARGIN - gfxTextW(UI_MENU_NO_ESC, 1) - 6, UI_STRIP_Y + 4,
	                      UI_MENU_NO_ESC, C_DIM, C_BG, 1));
	uiDrawSplash();
	char pin[48];
	snprintf(pin, sizeof(pin), UI_SPLASH_PIN_FMT, (unsigned)settings()->dshotPin);
	int x0 = 0, x1 = 0;
	checkTrue("the splash draws no pin line", !fakeTextCentredIs(288, pin, C_AMBER, C_BG, 1) &&
	          !fakeInkExtent(268, GFX_H - 268, C_BG, &x0, &x1));
	freshLife();
}

/** @brief The rule, pure, for every screen, and live on each screen MENU reaches. */
static void testRule() {
	section("DShot lifetime: the rule, screen by screen");

	const UiScreenId M = UiScreenId::Menu;
	int wrong = 0;
	for (int t = 0; t < (int)UiScreenId::Count; t++) {
		UiScreenId top = (UiScreenId)t;
		UiScreenId two[2] = { M, top };
		bool want = top == UiScreenId::Tester || top == UiScreenId::EscCmd;
		wrong += uiDshotRule(two, 2, false) != want;
		wrong += uiDshotRule(two, 2, true);
		for (int u = 0; u < (int)UiScreenId::Count; u++) {
			UiScreenId three[3] = { M, (UiScreenId)u, top };
			bool w3 = want || (top == UiScreenId::Settings && (UiScreenId)u == UiScreenId::Tester);
			wrong += uiDshotRule(three, 3, false) != w3;
		}
	}
	checkInt("ESC TESTER and ESC COMMANDS on top, and SETUP over the tester: true; else false",
	         wrong, 0);
	UiScreenId menu[1] = { M };
	UiScreenId setupTester[3] = { M, UiScreenId::Tester, UiScreenId::Settings };
	checkTrue("MENU alone: false; SETUP over the tester: true; with the cue held (D3): false",
	          !uiDshotRule(menu, 1, false) && uiDshotRule(setupTester, 3, false) &&
	          !uiDshotRule(setupTester, 3, true) && !uiDshotRule(menu, 0, false));

	// Live: each screen but the two and SETUP over the tester parks.
	freshLife();
	const struct { const UiScreen *row; const char *name; } ROWS[] = {
		{ &UI_ESC_CONFIG, "ESC CONFIG" }, { &UI_SERVO_HUB_SCREEN, "SERVO" },
		{ &UI_RX_SIGNALS, "RX SIGNALS" }, { &UI_SDLOG, "SD LOG" }, { &UI_SETTINGS, "SETTINGS" },
	};
	for (const auto &r : ROWS) {
		int binds = fakeRebuildCount();
		tapId(menuRow(r.row));
		lifeFrames(1);
		char what[96];
		snprintf(what, sizeof(what), "%s from MENU: not wanted, parked, no bind", r.name);
		checkTrue(what, uiCurrentScreen() == r.row->id && !uiDshotWanted() && escTaskParked() &&
		          fakeRebuildCount() == binds);
		tapId(UI_FIX(UI_BACK));
		lifeFrames(1);
	}
	int binds = fakeRebuildCount();
	tapId(toolEntry(&UI_ANGLE));
	lifeFrames(1);
	checkTrue("ANGLE METER from SERVO: not wanted, parked, no bind",
	          uiCurrentScreen() == UiScreenId::Angle && !uiDshotWanted() && escTaskParked() &&
	          fakeRebuildCount() == binds);
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
	checkTrue("back on MENU", uiCurrentScreen() == UiScreenId::Menu);
}

/**
 * @brief ESC TESTER binds once; its MENU button parks after the drain. ESC
 *        COMMANDS binds; its BACK parks with no drain.
 */
static void testOpenClose() {
	section("DShot lifetime: ESC TESTER and ESC COMMANDS open and close");

	freshLife();
	int binds = fakeRebuildCount();
	tapId(menuRow(&UI_TESTER));
	checkTrue("the tap pushes the tester; Run is asked on the next frame",
	          uiCurrentScreen() == UiScreenId::Tester && uiDshotWanted());
	lifeFrames(2);
	checkTrue("the tester binds once", fakeRebuildCount() == binds + 1 && escTaskRunning() &&
	          pinClaimDshotRunning());
	checkTrue("its first frames are the EDT enable or zero throttle",
	          fakeLastFrame().throttle == 0);
	waitGate();
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 44);
	fakeRelease();
	lifeFrames(1);
	checkTrue("armed", uiArmed() && fakeArmed());
	tapId(UI_FIX(TesterCtl::Latch));
	{
		const UiRect t = ctlRect(UI_FIX(TesterCtl::Throttle));
		const int y = t.y + t.h / 2;
		fakePress(t.x + 10, y); lifeFrames(1);
		for (int k = 1; k <= 6; k++) { fakeHold(t.x + 10 + 8 * k, y); lifeFrames(1); }
		fakeRelease(); lifeFrames(1);
	}
	checkTrue("a throttle stands", fakeThrottle() > 0);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("MENU: disarmed", uiCurrentScreen() == UiScreenId::Menu && !uiArmed() && !fakeArmed());
	// LATCH kept the throttle going out until the MENU tap's frame.
	uint32_t last = fakeLastThrottleMs();
	checkTrue("a throttle frame went out within the last frame", millis() - last <= 25);
	bool zeros = true;
	uint32_t parkedAt = 0;
	for (int k = 0; k < 100 && !parkedAt; k++) {
		lifeFrame(5);
		if (escTaskParked()) parkedAt = millis();
		else zeros = zeros && fakeLastFrame().throttle == 0 && !fakeLastFrame().sendCommand;
	}
	checkTrue("zero frames until the park", zeros);
	checkInt("parked 251 to 255 ms after the last throttle frame",
	         parkedAt && parkedAt - last > 250 && parkedAt - last <= 255 ? 1 : (int)(parkedAt - last), 1);
	checkTrue("no bind on MENU", fakeRebuildCount() == binds + 1);
	checkTrue("SIG parked low", sigParked(SIG_GPIO));

	// ESC COMMANDS from ESC CONFIG.
	tapId(toolEntry(&UI_ESC_CMD));
	lifeFrames(2);
	checkTrue("ESC COMMANDS binds", uiCurrentScreen() == UiScreenId::EscCmd &&
	          fakeRebuildCount() == binds + 2 && escTaskRunning());
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: ESC CONFIG", uiCurrentScreen() == UiScreenId::EscConfig);
	lifeFrame(1);
	checkTrue("parked on the first pump pass after the frame that asked: no drain",
	          !escTaskParked());
	lifeFrame(1);
	checkTrue("parked", escTaskParked() && fakeRebuildCount() == binds + 2);
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
}

/**
 * @brief SETUP from the readouts keeps DShot up; a SIG PIN change there
 *        binds on the new pin, and the bind stamp moves. From MENU the
 *        change re-parks with no bind.
 */
static void testSetupOverTester() {
	section("DShot lifetime: SETUP over the tester, and from MENU");

	freshLife();
	openTesterBound();
	waitGate();
	tapId(UI_FIX(TesterCtl::Max));
	lifeFrames(2);
	checkTrue("SETUP over the tester: DShot runs", uiCurrentScreen() == UiScreenId::Settings &&
	          uiNavAt(1) == UiScreenId::Tester && uiDshotWanted() && escTaskRunning());
	uint32_t stamp = escTaskBoundMs();
	int binds = fakeRebuildCount();
	revealRow((int)SettingsRowId::Sig);
	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	const uint8_t moved = settings()->dshotPin;
	lifeFrames(2);
	checkTrue("a SIG PIN change binds on the new pin while SETUP shows",
	          uiCurrentScreen() == UiScreenId::Settings && moved != SIG_GPIO &&
	          fakeRebuildCount() == binds + 1 && fakeResumeWiring().dshotPin == moved && escTaskRunning());
	checkTrue("the bind stamp moves", escTaskBoundMs() != 0 && escTaskBoundMs() != stamp);
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
	checkTrue("back on the tester: the gate holds from the new bind",
	          uiCurrentScreen() == UiScreenId::Tester && uiArmGated(escTaskBoundMs(), millis()) &&
	          !ctlEnabled(UI_FIX(TesterCtl::Arm)));
	tapId(UI_FIX(TesterCtl::Menu));
	lifeFrames(2);
	checkTrue("MENU: parked on the new pin", escTaskParked() && sigParked(moved));

	// SETUP from MENU: the change re-parks, no bind.
	binds = fakeRebuildCount();
	tapId(menuRow(&UI_SETTINGS));
	lifeFrames(1);
	revealRow((int)SettingsRowId::Sig);
	tapId(UI_ROW(SettingsRowId::Sig, Minus));
	lifeFrames(2);
	checkTrue("from MENU: SIG back on GP0, parked there, the old pin let go, no bind",
	          settings()->dshotPin == SIG_GPIO && escTaskParked() && sigParked(SIG_GPIO) &&
	          !fakePin(moved).output && fakeRebuildCount() == binds);
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
	freshLife();
}

/**
 * @brief The arm gate: 1500 ms from core1's bind, HOLD TO ARM drawn
 *        disabled with the track's text; a press that lands while it holds
 *        is not timed.
 */
static void testArmGate() {
	section("DShot lifetime: the arm gate, 1500 ms after the bind");

	freshLife();
	openTesterBound();
	uint32_t bound = escTaskBoundMs();
	checkTrue("bound", bound != 0 && escTaskRunning());
	frameAt(bound + 1499);
	checkTrue("1499 ms after the bind: HOLD TO ARM disabled, drawn so, the track's text",
	          !ctlEnabled(UI_FIX(TesterCtl::Arm)) && armDrawnDisabled() && gateTextShown());
	checkTrue("the text is 26 characters, 155 px", strlen(UI_TESTER_GATE_TEXT) == 26 &&
	          gfxTextW(UI_TESTER_GATE_TEXT, 1) == 155);
	int ax, ay;
	ctlCentre(UI_FIX(TesterCtl::Arm), &ax, &ay);
	fakePress(ax, ay);
	lifeFrame(0);
	bool armed = false;
	int progress = 0;
	for (int k = 0; k < 120; k++) {
		fakeHold(ax, ay);
		lifeFrame(25);
		armed = armed || uiArmed();
		progress += badgeProgress();
	}
	checkTrue("a press that lands at 1499 ms, held 3000 ms: no arm, no badge progress",
	          !armed && progress == 0 && !fakeArmed());
	fakeRelease();
	lifeFrame(25);
	checkTrue("1500 ms on: HOLD TO ARM live, ARM TO ENABLE again",
	          ctlEnabled(UI_FIX(TesterCtl::Arm)) && !gateTextShown());

	// A new bind: MENU and the row again.
	tapId(UI_FIX(TesterCtl::Menu));
	lifeFrames(20);
	openTesterBound();
	bound = escTaskBoundMs();
	frameAt(bound + 1500);
	checkTrue("1500 ms after the bind: live", ctlEnabled(UI_FIX(TesterCtl::Arm)) && !gateTextShown());
	fakePress(ax, ay);
	lifeFrame(0);
	int p1 = 0, p2 = 0;
	while (millis() < bound + 2475) {
		fakeHold(ax, ay);
		lifeFrame(25);
		if (millis() == bound + 1900) p1 = badgeProgress();
		if (millis() == bound + 2400) p2 = badgeProgress();
	}
	checkTrue("its progress grows: 92 px in 1000 ms", p1 > 0 && p2 > p1);
	checkTrue("2475 ms after the bind: not yet armed", !uiArmed());
	fakeHold(ax, ay);
	lifeFrame(25);
	checkTrue("a press that lands at 1500 ms arms at 2500 ms", uiArmed() && millis() == bound + 2500);
	fakeRelease();
	lifeFrame(25);
	tapId(UI_FIX(TesterCtl::Arm));   // DISARM: acts on the press, gate or not
	lifeFrames(1);
	checkTrue("DISARM", !uiArmed());

	// #49's touch loss refuses arming too: drawn disabled.
	fakeTouchAnswers(false);
	lifeFrames(6);
	checkTrue("touch lost: HOLD TO ARM disabled and drawn so",
	          uiTouchLost() && !ctlEnabled(UI_FIX(TesterCtl::Arm)) && armDrawnDisabled());
	fakeTouchAnswers(true);
	lifeFrames(2);
	checkTrue("answering again: live", ctlEnabled(UI_FIX(TesterCtl::Arm)));
	tapId(UI_FIX(TesterCtl::Menu));
	lifeFrames(2);
}

/** @brief The frames between the tap and core1's bind: the stamp is 0 and gates. */
static void testTapToBind() {
	section("DShot lifetime: the frames between the tap and the bind");

	freshLife();
	const UiRect row = ctlRect(menuRow(&UI_TESTER));
	fakePress(row.x + row.w / 2, row.y + row.h / 2);
	framesNoPump(1);
	fakeRelease();
	framesNoPump(1);
	framesNoPump(2);
	checkTrue("the tester shows, Run asked, nothing bound: the stamp 0",
	          uiCurrentScreen() == UiScreenId::Tester && uiDshotWanted() && escTaskBoundMs() == 0);
	checkTrue("HOLD TO ARM disabled and drawn so, the track's text",
	          !ctlEnabled(UI_FIX(TesterCtl::Arm)) && armDrawnDisabled() && gateTextShown());
	int ax, ay;
	ctlCentre(UI_FIX(TesterCtl::Arm), &ax, &ay);
	fakePress(ax, ay);
	framesNoPump(1);
	bool armed = false;
	int progress = 0;
	for (int k = 0; k < 44; k++) {
		fakeHold(ax, ay);
		framesNoPump(1);
		armed = armed || uiArmed();
		progress += badgeProgress();
	}
	checkTrue("a press held through those frames is not timed", !armed && progress == 0);
	fakeHold(ax, ay);
	lifeFrame(25);
	uint32_t bound = escTaskBoundMs();
	checkTrue("core1 binds: the stamp", bound != 0);
	for (int k = 0; k < 120; k++) {
		fakeHold(ax, ay);
		lifeFrame(25);
		armed = armed || uiArmed();
	}
	checkTrue("the same press, held 3000 ms past the bind: no arm", !armed);
	fakeRelease();
	lifeFrame(25);
	checkTrue("the gate counts from the stamp: gated at 1499 ms, not at 1500 ms",
	          uiArmGated(bound, bound + 1499) && !uiArmGated(bound, bound + 1500) &&
	          uiArmGated(0, bound + 100000));
	tapId(UI_FIX(TesterCtl::Menu));
	lifeFrames(2);
}

/**
 * @brief A rebind during a timed press, as SETUP's applyLive() would cause:
 *        the stamp moves and the press is cancelled for good.
 */
static void testRebindCancels() {
	section("DShot lifetime: a rebind under a timed press cancels it");

	freshLife();
	openTesterBound();
	waitGate();
	int ax, ay;
	ctlCentre(UI_FIX(TesterCtl::Arm), &ax, &ay);
	fakePress(ax, ay);
	lifeFrame(0);
	for (int k = 0; k < 20; k++) { fakeHold(ax, ay); lifeFrame(25); }
	checkTrue("timed for 500 ms: progress drawn, not armed", badgeProgress() > 0 && !uiArmed());
	uint32_t old = escTaskBoundMs();
	// What SETUP's applyLive() does on a SIG PIN change; no UI path reaches
	// SETUP with a finger on HOLD TO ARM, so the case checks the rule.
	escTaskConfigure(AUX_GPIO, settings()->dshotKbaud, false, SIG_GPIO);
	fakeHold(ax, ay);
	lifeFrame(25);
	uint32_t now = escTaskBoundMs();
	checkTrue("core1 rebinds: the stamp moves", now != 0 && now != old);
	checkTrue("the press is cancelled, its progress gone", badgeProgress() == 0 && !uiArmed());
	bool armed = false;
	for (int k = 0; k < 120; k++) {
		fakeHold(ax, ay);
		lifeFrame(25);
		armed = armed || uiArmed();
	}
	checkTrue("held on 3000 ms: no arm, at 1000 ms or later", !armed);
	fakeRelease();
	lifeFrame(1);
	checkTrue("the gate counts 1500 ms from the new stamp",
	          uiArmGated(now, now + 1499) && !uiArmGated(now, now + 1500));
	escTaskConfigure(SIG_GPIO, settings()->dshotKbaud, settings()->kissEnable, AUX_GPIO);
	tapId(UI_FIX(TesterCtl::Menu));
	lifeFrames(20);
}

/** @brief Busy while Run is wanted; pinClaimDshot() acts only while Free. */
static void testClaimRules() {
	section("DShot lifetime: the claim's Busy and pinClaimDshot() under a claim");

	freshLife();
	openTesterBound();
	checkTrue("on the tester: a claim is busy",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::Busy &&
	          pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(TesterCtl::Menu));
	lifeFrames(1);
	checkTrue("MENU: a claim is not busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	int starts = fakeResumeCount(), parks = fakeParkCount();
	pinClaimDshot(true);
	checkTrue("pinClaimDshot(true) while a claim holds does nothing",
	          fakeResumeCount() == starts && pinClaimHolder() == PinUser::Am32 && !uiDshotWanted());
	pinClaimDshot(false);
	checkTrue("nor pinClaimDshot(false)", fakeParkCount() == parks);
	fakePumpPoll();
	checkTrue("held, released, no bind", pinClaimPoll() == ClaimState::Held && escTaskSuspended());
	pinClaimEnd(PinUser::Am32);
	lifeFrames(2);
	checkTrue("BACK: parked, Free", pinClaimState() == ClaimState::Free && escTaskParked());
}

/**
 * @brief SERVO TESTER's leave: the wide button reads SERVO, the last step
 *        BACK TO SERVO.; the tap parks, the hub reads SETTING SIG GP0 LOW
 *        while Parking, then Free with no bind.
 */
static void testLeaveParks() {
	section("DShot lifetime: the leave page's last button parks");

	freshLife();
	int binds = fakeRebuildCount();
	tapId(toolEntry(&UI_SERVO));
	lifeFrames(4);
	checkTrue("SERVO TESTER runs on the claim, no DShot",
	          pinClaimState() == ClaimState::Running && fakeRebuildCount() == binds);
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 80 && !ctlEnabled(UI_FIX(ServoCtl::Release)); k++) lifeFrames(1);
	lifeFrames(1);
	checkTrue("the wide button reads SERVO, live; the left NEXT SERVO",
	          leaveWideIs("SERVO", true) && leaveLeftIs("NEXT SERVO", true));
	checkTrue("step 1: ESC TESTER'S DSHOT ON SIG GP0 READS, 35 characters",
	          leaveStepIs(UI_LEAVE_STEP_Y + 12, 0, "ESC TESTER'S DSHOT ON SIG GP0 READS", true) &&
	          strlen("ESC TESTER'S DSHOT ON SIG GP0 READS") == UI_LEAVE_STEP_CHARS);
	checkTrue("the last step: BACK TO SERVO.",
	          leaveStepIs(UI_LEAVE_STEP_Y + 38, '2', "BACK TO SERVO.", false));
	pressId(UI_FIX(ServoCtl::Release));
	fakeRelease();
	framesNoPump(2);
	char want[32];
	snprintf(want, sizeof(want), "SETTING SIG GP%u LOW", (unsigned)SIG_GPIO);
	checkTrue("SERVO, the hub, while the claim parks: SETTING SIG GP0 LOW, amber, line 2",
	          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() == ClaimState::Parking &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));
	checkTrue("no DShot asked", fakeResumeCount() >= 0 && !uiDshotWanted());
	lifeFrames(1);
	checkTrue("then Free, parked, no bind", pinClaimState() == ClaimState::Free && escTaskParked() &&
	          sigParked(SIG_GPIO) && fakeRebuildCount() == binds);
	checkTrue("and the NO ESC CHECK line again",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, UI_HUB_NO_ESC_CHECK, C_AMBER, C_BG, 1));
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
}

/** @brief The hubs' second strip line, and MONITOR's line with SIG on GP1. */
static void testHubLines() {
	section("DShot lifetime: NO ESC CHECK on SERVO and RX SIGNALS, MONITOR on GP1");

	freshLife();
	checkTrue("the line: 35 characters, 209 px",
	          strlen(UI_HUB_NO_ESC_CHECK) == 35 && gfxTextW(UI_HUB_NO_ESC_CHECK, 1) == 209);
	const struct { const UiScreen *s; bool line; const char *name; } HUBS[] = {
		{ &UI_SERVO_HUB_SCREEN, true, "SERVO" }, { &UI_RX_SIGNALS, true, "RX SIGNALS" },
		{ &UI_ESC_CONFIG, false, "ESC CONFIG" },
	};
	for (const auto &h : HUBS) {
		tapId(menuRow(h.s));
		lifeFrames(1);
		char what[96];
		snprintf(what, sizeof(what), "%s, the claim Free: %s", h.name,
		         h.line ? "NO ESC CHECK: UNPLUG ANY ESC FIRST. in amber, line 2" : "no second line");
		bool shown = fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, UI_HUB_NO_ESC_CHECK, C_AMBER, C_BG, 1);
		checkTrue(what, h.line ? shown : (!shown && allOf(0, UI_STRIP_LINE2_Y, GFX_W, 7, C_BG)));
		tapId(UI_FIX(UI_BACK));
		lifeFrames(1);
	}
	checkTrue("a claim held", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	char busy[32];
	snprintf(busy, sizeof(busy), "RELEASING SIG GP%u", (unsigned)SIG_GPIO);
	for (const auto &h : HUBS) {
		tapId(menuRow(h.s));
		lifeFrames(1);
		char what[96];
		snprintf(what, sizeof(what), "%s, the claim not Free: RELEASING SIG GP0 in its place", h.name);
		checkTrue(what, fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, busy, C_AMBER, C_BG, 1) &&
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, UI_HUB_NO_ESC_CHECK, C_AMBER, C_BG, 1));
		tapId(UI_FIX(UI_BACK));
		lifeFrames(1);
	}
	pinClaimEnd(PinUser::Am32);
	lifeFrames(2);

	// MONITOR's line follows SIG: none on GP0.
	tapId(menuRow(&UI_RX_SIGNALS));
	lifeFrames(1);
	const int noteY = UI_HUB_ENTRY_Y + UI_HUB_LINE_DY + UI_HUB_LINE_H;
	checkTrue("SIG GP0: no MONITOR line", !fakeTextIs(UI_MARGIN, noteY, UI_RX_MONITOR_GP1, C_AMBER, C_BG, 1) &&
	          allOf(0, noteY, GFX_W, 7, C_BG));
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
	escTaskConfigure(AUX_GPIO, settings()->dshotKbaud, false, SIG_GPIO);
	lifeFrames(1);
	tapId(menuRow(&UI_RX_SIGNALS));
	lifeFrames(1);
	checkTrue("SIG GP1: GP1 IS SIG: PLUG IN AFTER THE TAP. under MONITOR, amber, 203 px",
	          fakeTextIs(UI_MARGIN, noteY, UI_RX_MONITOR_GP1, C_AMBER, C_BG, 1) &&
	          gfxTextW(UI_RX_MONITOR_GP1, 1) == 203);
	tapId(UI_FIX(UI_BACK));
	lifeFrames(1);
	escTaskConfigure(SIG_GPIO, settings()->dshotKbaud, settings()->kissEnable, AUX_GPIO);
	lifeFrames(1);
}

/** @brief The texts of slice D2, as they read and as they fit. */
static void testTexts() {
	section("DShot lifetime: the texts that state DShot's restart");

	char buf[48];
	snprintf(buf, sizeof(buf), UI_LEAVE_DSHOT_FMT, 0u);
	checkStr("the leave page's unplug step", buf, "ESC TESTER'S DSHOT ON SIG GP0 READS");
	snprintf(buf, sizeof(buf), UI_BUSY_RELEASING_FMT, 0u);
	checkStr("the busy caption while Leaving", buf, "RELEASING SIG GP0");
	snprintf(buf, sizeof(buf), UI_BUSY_PARKING_FMT, 0u);
	checkStr("the busy caption while Parking", buf, "SETTING SIG GP0 LOW");
	checkStr("a tool's refusal", UI_WAIT_RESTART_TEXT, "WAIT: THE PINS ARE BUSY");
	snprintf(buf, sizeof(buf), SBUS_UI_RESTART_FMT, 0u);
	checkStr("S.BUS MONITOR's step", buf, "ESC TESTER SENDS DSHOT ON SIG GP0.");
	snprintf(buf, sizeof(buf), SBOUT_UI_RESTART_FMT, 0u);
	checkStr("S.BUS OUTPUT's step", buf, "ESC TESTER SENDS DSHOT ON SIG GP0.");
	checkStr("S.BUS MONITOR's step with SIG on GP1", SBUS_UI_RESTART_GP1_1, "SIG GP1 IS HELD LOW, WHERE");
	checkStr("ESC SETTINGS' exit line", AM32_UI_EXIT_2, "LEAVE. ESC TESTER THEN RUNS IT.");
	snprintf(buf, sizeof(buf), AM32_UI_LEAVE_2, "ESC CONFIG");
	checkStr("ESC SETTINGS' last step", buf, "PLUG IT IN ONCE ESC CONFIG SHOWS.");
	checkStr("HITEC SERVO's line under the ESC", HITEC_UI_ESC_2, "NOTHING IS SENT UNTIL THEN.");
	checkStr("the arm gate's track", UI_TESTER_GATE_TEXT, "DSHOT STARTING: WAIT 1.5 S");
	checkTrue("each fits its line: 35, 34, 31, 33 characters",
	          strlen("ESC TESTER SENDS DSHOT ON SIG GP0.") <= UI_LEAVE_STEP_CHARS &&
	          strlen(AM32_UI_EXIT_2) <= 37 && strlen(buf) <= UI_LEAVE_STEP_CHARS &&
	          strlen("BACK TO RX SIGNALS.") <= UI_LEAVE_STEP_CHARS);
}

void runDshotLifeTests() {
	testBoot();
	testRule();
	testOpenClose();
	testSetupOverTester();
	testArmGate();
	testTapToBind();
	testRebindCancels();
	testClaimRules();
	testLeaveParks();
	testHubLines();
	testTexts();
	freshLife();
	openTester();
	lifeFrames(2);
}
