/**
 * @file test_menu.cpp
 * @brief MENU, SETTINGS and the navigation between them and the tools,
 *        through the real UI.
 *
 * The cases slice C of docs/design/menu.md lists: SD LOG's BACK to MENU,
 * every registry entry opened from MENU and back, MENU's disarm, BEEP's
 * flash on its row, the rows drawn disabled while the claim restores, MENU's
 * scroll over registries of six and seven rows, SETTINGS whole and tappable at 0 px as the
 * sixth row, SD LOG cut until dragged, and SETTINGS' list: its scroll range,
 * each row by id, no scroll from a control, the 40 px rule for a cut
 * control, HOLD TO SAVE at every scroll, and the pole and ceiling rows.
 */

#include "check.h"
#include "fakes.h"
#include "settings.h"
#include "ui.h"
#include "ui_am32.h"
#include "ui_menu.h"
#include "ui_registry.h"
#include "ui_sdlog.h"
#include "ui_servo.h"
#include "ui_hitec.h"
#include "ui_kst.h"
#include "ui_settings.h"
#include "ui_screen.h"
#include "esc_task.h"
#include "gfx.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "pin_claim.h"
#include "core1_job.h"
#include "config.h"
#include "plat.h"
#include "touch.h"
#include "diag.h"
#include "version.h"

#include <stddef.h>
#include <stdio.h>

#include <string.h>

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void menuFrames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiTick(); }
}
#define UI_IDS_FRAMES(n) menuFrames(n)
#include "ui_ids.h"

/** @brief A pixel of the frame. */
static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/**
 * @brief Whether SETTINGS' note line, y 254, reads @p text in @p col: the line
 *        is redrawn with it into a copy and compared, and the frame restored.
 */
static bool noteIs(const char *text, uint16_t col) {
	uint16_t *fb = gfxBuffer();
	uint16_t keep[8][GFX_W];
	memcpy(keep, fb + 254 * GFX_W, sizeof(keep));
	uint32_t shown = fakeRegionHash(0, 254, GFX_W, 8);
	gfxRect(0, 254, GFX_W, 8, C_BG);
	gfxText(UI_MARGIN, 254, text, col, 1);
	bool same = fakeRegionHash(0, 254, GFX_W, 8) == shown;
	memcpy(fb + 254 * GFX_W, keep, sizeof(keep));
	return same;
}

/** @brief The tester, from a fresh boot with the compiled defaults. */
static void freshTester() {
	// A claim outlives uiInit(); let the last test's claim end first.
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	fakeFlashClear(); settingsLoad(); uiInit();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
	menuFrames(4);
}

/** @brief Tester → MENU, and the frame that draws it. */
static void openMenu() {
	tapId(UI_FIX(TesterCtl::Menu));
	menuFrames(1);
}

/**
 * @brief Frames without the pump's pass: DShot does not resume, so a claim
 *        that is Restoring stays Restoring.
 */
static void framesNoPump(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); core1JobPoll(); uiTick(); }
}

/** @brief SD LOG's BACK returns to MENU in both card states, and MENU's BACK to the tester. */
static void testSdLogBack() {
	section("MENU: SD LOG's BACK returns to MENU");
	freshTester();
	openMenu();
	tapId(menuRow(&UI_SDLOG));
	checkTrue("SD LOG opens from MENU", uiCurrentScreen() == UiScreenId::SdLog);
	tapId(UI_FIX(UI_BACK));
	checkTrue("card idle: BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);

	tapId(menuRow(&UI_SDLOG));
	fakeMscSet(MscState::Serving, 0); menuFrames(1);
	tapId(UI_FIX(UI_BACK));
	checkTrue("card held by a host: BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	checkTrue("then MENU's BACK reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	pressId(UI_FIX(TesterCtl::Arm), IdExpect::Disabled);
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); menuFrames(2);
	checkTrue("arming is refused while the host holds the card", !uiArmed());
	fakeMscSet(MscState::Idle, 0); menuFrames(1);
}

/** @brief Every registry entry: a tool opens from MENU and its BACK lands on MENU; an action stays. */
static void testEveryEntry() {
	section("MENU: every registry entry opens and returns to MENU");
	freshTester();
	openMenu();
	uint8_t n;
	const UiMenuEntry *e = uiMenuEntries(&n);
	checkInt("the registry has seven rows", n, 7);
	for (uint8_t i = 0; i < n; i++) {
		const UiScreen *s = e[i].screen;
		char what[64];
		tapId(menuRow(s));
		if (s->action) {
			snprintf(what, sizeof(what), "%s acts and MENU stays", e[i].label);
			checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
			continue;
		}
		snprintf(what, sizeof(what), "%s opens its screen", e[i].label);
		checkTrue(what, uiCurrentScreen() == s->id);
		menuFrames(10);
		checkControlTable(e[i].label);
		tapId(UI_FIX(UI_BACK));
		// SERVO and KST SERVO pass the leave page once their job has held the
		// pins: RESTART DSHOT acts 1000 ms after the release.
		for (int k = 0; k < 200 && ctlEnabled(UI_FIX(ServoCtl::Restart)) == false &&
		                uiCurrentScreen() == UiScreenId::Servo; k++)
			menuFrames(1);
		if (uiCurrentScreen() == UiScreenId::Servo) tapId(UI_FIX(ServoCtl::Restart));
		// KST SERVO asks for LEAVE first when it found the servo.
		for (int k = 0; k < 200 && ctlEnabled(UI_FIX(KstCtl::Restart)) == false &&
		                uiCurrentScreen() == UiScreenId::Kst; k++) {
			if (ctlEnabled(UI_FIX(KstCtl::Leave))) tapId(UI_FIX(KstCtl::Leave));
			else if (uiKstView() == KstView::List) tapId(UI_FIX(UI_BACK));
			menuFrames(1);
		}
		if (uiCurrentScreen() == UiScreenId::Kst) tapId(UI_FIX(KstCtl::Restart));
		// HITEC SERVO holds SIG from its first frame: the leave page too.
		for (int k = 0; k < 200 && ctlEnabled(UI_FIX(HitecCtl::Restart)) == false &&
		                uiCurrentScreen() == UiScreenId::Hitec; k++)
			menuFrames(1);
		if (uiCurrentScreen() == UiScreenId::Hitec) tapId(UI_FIX(HitecCtl::Restart));
		for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
		snprintf(what, sizeof(what), "BACK from %s lands on MENU", e[i].label);
		checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
	}
	tapId(UI_FIX(UI_BACK));
}

/** @brief MENU disarms on open, BEEP flashes on its row, and KST SERVO has its row. */
static void testMenuDisarmAndBeep() {
	section("MENU: opening it disarms; BEEP's flash is on its row");
	freshTester();
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); menuFrames(2);
	checkTrue("armed on the tester", uiArmed() && fakeArmed());
	tapId(UI_FIX(TesterCtl::Latch));
	openMenu();
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("and the tester is disarmed", !uiArmed() && !fakeArmed());
	checkInt("with the throttle at 0", uiThrottle(), 0);
	checkInt("KST SERVO is the fourth row", uiMenuRowOf(&UI_KST), 3);
	checkControlTable("MENU");

	UiRect beep = ctlRect(menuRow(&UI_BEEP));
	int beeps = fakeBeepRequests();
	tapId(menuRow(&UI_BEEP));
	checkInt("BEEP sends the beacon", fakeBeepRequests(), beeps + 1);
	checkTrue("its row shows SENT on an ink fill", pixel(beep.x + 4, beep.y + beep.h - 5) == C_INK);
	menuFrames(16);
	checkTrue("400 ms later the fill is back to the panel",
	          pixel(beep.x + 4, beep.y + beep.h - 5) == C_PANEL);

	// The refusal is unreachable through MENU, which disarms; forced here.
	escSetArmed(true);
	tapId(menuRow(&UI_BEEP));
	checkTrue("refused while armed: the row shows REFUSED on an amber fill",
	          pixel(beep.x + 4, beep.y + beep.h - 5) == C_AMBER);
	escSetArmed(false);
	menuFrames(16);
	checkTrue("the flash is over", pixel(beep.x + 4, beep.y + beep.h - 5) == C_PANEL);
	fakeAdvance(0x80000000u);
	menuFrames(2);
	checkTrue("2^31 ms after the last BEEP the row does not flash",
	          pixel(beep.x + 4, beep.y + beep.h - 5) == C_PANEL);

	// A flash cut short by leaving MENU, 100 ms into its 350 ms.
	tapId(menuRow(&UI_BEEP));
	menuFrames(4);
	tapId(UI_FIX(UI_BACK));
	// Past 2^31 ms after the flash's end, where the bare deadline reads
	// "not yet" again.
	fakeAdvance(0x80000000u + 1000u);
	uiTick();
	tapId(UI_FIX(TesterCtl::Menu));
	menuFrames(2);
	checkTrue("a flash cut short by BACK is not back past 2^31 ms",
	          uiCurrentScreen() == UiScreenId::Menu &&
	          pixel(beep.x + 4, beep.y + beep.h - 5) == C_PANEL);
	tapId(UI_FIX(UI_BACK));
}

/** @brief A pin-claiming tool is refused on MENU while the claim restores. */
static void testPinsBusy() {
	section("MENU: tools drawn disabled while the claim restores");
	freshTester();
	// An earlier AM32 test can leave MENU's run cue standing; the cue stays on
	// the device until an eRPM frame arrives after the handshake. The PINS
	// BUSY picture is about the rows, so feed one such frame, let MENU clear
	// the cue, and take the telemetry away again.
	{
		EscTelemetry t;
		memset(&t, 0, sizeof(t));
		t.lastRpmMs = millis();
		fakeSetTelemetry(&t);
		fakeAdvance(1);
		openMenu();
		menuFrames(2);
		memset(&t, 0, sizeof(t));
		fakeSetTelemetry(&t);
		tapId(UI_FIX(UI_BACK));
		menuFrames(2);
	}
	openMenu();
	tapId(menuRow(&UI_SERVO));
	checkTrue("SERVO opens", uiCurrentScreen() == UiScreenId::Servo);
	menuFrames(4);
	checkTrue("its claim runs", pinClaimState() == ClaimState::Running);
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 200 && !ctlEnabled(UI_FIX(ServoCtl::Restart)); k++) menuFrames(1);
	pressId(UI_FIX(ServoCtl::Restart));
	fakeRelease();
	// The release frame leaves for MENU. Without the pump's pass the claim
	// stays Restoring, the state RESTART DSHOT leaves it in.
	framesNoPump(2);
	bool busySeen = uiCurrentScreen() == UiScreenId::Menu && pinClaimState() != ClaimState::Free;
	checkTrue("RESTART DSHOT returns to MENU while the claim restores", busySeen);
	if (busySeen) {
		// SETTINGS is MENU's sixth row, whole at scroll 0. SD LOG is the
		// seventh: a drag brings it whole, with no pump pass, so the claim
		// stays Restoring.
		const UiControlId settingsRow = UI_ROW(uiMenuRowOf(&UI_SETTINGS), Row);
		checkInt("SETTINGS' row is whole without a drag", ctlRect(settingsRow).h, 40);
		checkTrue("SERVO TESTER does not act", !ctlEnabled(menuRow(&UI_SERVO)));
		char want[40];
		snprintf(want, sizeof(want), "DSHOT RESTARTING ON SIG GP%u", (unsigned)settings()->dshotPin);
		checkTrue("the strip says why: DSHOT RESTARTING ON SIG GP0, amber",
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_AMBER, C_BG, 1));
		char pins[16];
		snprintf(pins, sizeof(pins), "GP%u+%u", (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin);
		const UiRect sr = ctlRect(menuRow(&UI_SERVO));
		checkTrue("SERVO TESTER keeps its own status under the outline",
		          fakeTextIs(UI_MENU_STATUS_R - gfxTextW(pins, 1), sr.y + 16, pins, C_DIM, C_BG, 1));
		checkTrue("nor AM32 CONFIG", !ctlEnabled(menuRow(&UI_AM32)));
		checkTrue("nor SETTINGS, which changes the wiring the claim latched",
		          !ctlEnabled(settingsRow));
		fakePress(120, 250); framesNoPump(1);
		for (int k = 1; k <= 12; k++) { fakeHold(120, 250 - 6 * k); framesNoPump(1); }
		fakeRelease(); framesNoPump(1);
		const UiControlId logRow = UI_ROW(uiMenuRowOf(&UI_SDLOG), Row);
		checkInt("SD LOG's row is whole after the drag", ctlRect(logRow).h, 40);
		checkTrue("SD LOG, which claims nothing, acts", ctlEnabled(logRow));
		fakeDumpFrame("shot_menu_restarting.ppm");
		UiControl c;
		ctlFind(menuRow(&UI_SERVO), &c);
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(1);
		checkTrue("the claim still restores", pinClaimState() == ClaimState::Restoring);
		checkTrue("a tap while busy opens nothing", uiCurrentScreen() == UiScreenId::Menu);
		ctlFind(settingsRow, &c);
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(1);
		checkTrue("nor does a tap on SETTINGS while the claim restores",
		          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Restoring);
	}
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	checkTrue("once Free, SERVO TESTER acts again", ctlEnabled(menuRow(&UI_SERVO)));
	tapId(UI_FIX(UI_BACK));
}

/** @brief Six rows fit; seven scroll 36 px; the scroll bar shows only then. */
static void testMenuScroll() {
	section("MENU: six rows fit, seven scroll 36 px");
	static const UiMenuEntry six[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 9, &UI_SETTINGS }, { "SD LOG", 10, &UI_SDLOG },
	};
	static const UiMenuEntry seven[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "KST SERVO", 9, &UI_KST },
		{ "HITEC SERVO", 9, &UI_HITEC }, { "SETTINGS", 9, &UI_SETTINGS },
		{ "SD LOG", 10, &UI_SDLOG },
	};
	freshTester();

	uiMenuUseRegistry(six, 6);
	openMenu();
	checkTrue("six rows: the last is whole", ctlEnabled(UI_ROW(5, Row)));
	checkInt("and ends at y 311", ctlRect(UI_ROW(5, Row)).y + ctlRect(UI_ROW(5, Row)).h, 312);
	UiRect r0 = ctlRect(UI_ROW(0, Row));
	fakePress(120, 200); menuFrames(1);
	for (int k = 1; k <= 8; k++) { fakeHold(120, 200 - 6 * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
	checkInt("a drag up does not scroll six rows", ctlRect(UI_ROW(0, Row)).y, r0.y);
	checkTrue("and no scroll bar is drawn", pixel(236, 100) == C_BG);
	tapId(UI_FIX(UI_BACK));

	uiMenuUseRegistry(seven, 7);
	openMenu();
	checkTrue("seven rows: the seventh is cut", !ctlEnabled(UI_ROW(6, Row)));
	checkTrue("and the scroll bar is drawn", pixel(236, 70) == C_GRID);
	fakePress(120, 250); menuFrames(1);
	for (int k = 1; k <= 12; k++) { fakeHold(120, 250 - 6 * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
	checkInt("a drag up scrolls by 36 px and no further: the seventh row at y 278",
	         ctlRect(UI_ROW(6, Row)).y, 62 + 6 * 42 - 36);
	checkTrue("the seventh row is whole and acts", ctlEnabled(UI_ROW(6, Row)));
	checkTrue("the drag opened nothing", uiCurrentScreen() == UiScreenId::Menu);
	checkControlTable("MENU of seven, scrolled");
	tapId(UI_FIX(UI_BACK));
	uiMenuUseRegistry(nullptr, 0);
}

/** @brief The built registry: SETTINGS is the sixth row, whole and tappable at 0 px. */
static void testSettingsRowVisible() {
	section("MENU: SETTINGS is the sixth row, whole without a drag");
	freshTester();
	uiMenuUseRegistry(nullptr, 0);
	openMenu();
	checkInt("SETTINGS is the sixth row", uiMenuRowOf(&UI_SETTINGS), 5);
	checkInt("SD LOG is the seventh row", uiMenuRowOf(&UI_SDLOG), 6);
	const UiControlId settingsRow = UI_ROW(5, Row);
	checkInt("SETTINGS is whole at 0 px", ctlRect(settingsRow).h, 40);
	checkInt("and ends at y 311", ctlRect(settingsRow).y + ctlRect(settingsRow).h, 312);
	checkTrue("SD LOG, the seventh, is cut at 0 px", !ctlEnabled(UI_ROW(6, Row)));
	checkTrue("SETTINGS acts at 0 px", ctlEnabled(settingsRow));
	tapId(settingsRow);
	checkTrue("a tap with no drag opens SETTINGS", uiCurrentScreen() == UiScreenId::Settings);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK lands on MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("SD LOG is cut again at 0 px", !ctlEnabled(UI_ROW(6, Row)));
	checkTrue("a drag brings SD LOG whole and acts",
	          ctlEnabled(menuRow(&UI_SDLOG)) && ctlRect(UI_ROW(6, Row)).h == 40);
	tapId(menuRow(&UI_SDLOG));
	checkTrue("and a tap opens SD LOG", uiCurrentScreen() == UiScreenId::SdLog);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief Opens SETTINGS from a fresh tester. */
static void openSettings() {
	freshTester();
	openMenu();
	tapId(menuRow(&UI_SETTINGS));
}

/** @brief Drag the SETTINGS list from (120, @p y0) by @p dy, in 4 px frames. */
static void dragList(int y0, int dy) {
	fakePress(120, y0); menuFrames(1);
	int steps = (dy < 0 ? -dy : dy) / 4;
	for (int k = 1; k <= steps; k++) { fakeHold(120, y0 + (dy < 0 ? -4 : 4) * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
}

/** @brief SETTINGS' scroll: 0 px on open, 620 px at most, from a drag on a label only. */
static void testSettingsScroll() {
	section("SETTINGS: the scroll range, and no scroll from a control");
	openSettings();
	checkTrue("SETTINGS opens", uiCurrentScreen() == UiScreenId::Settings);
	UiRect poles = ctlRect(UI_ROW(SettingsRowId::Poles, Row));
	checkInt("opened at 0 px: MOTOR POLES at y 60", poles.y, 60);

	// A press on +, dragged up 40 px: the control keeps the press.
	UiRect plus = ctlRect(UI_ROW(SettingsRowId::Poles, Plus));
	uint8_t polesBefore = settings()->poles;
	fakePress(plus.x + plus.w / 2, plus.y + plus.h / 2); menuFrames(1);
	for (int k = 1; k <= 10; k++) { fakeHold(plus.x + plus.w / 2, plus.y + plus.h / 2 - 4 * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
	checkInt("a drag that starts on + does not scroll", ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);
	checkInt("and + stepped once, on touch-down", settings()->poles, polesBefore + 2);

	dragList(230, -180);
	dragList(230, -180);
	dragList(230, -180);
	dragList(230, -180);
	UiRect last = ctlRect(UI_ROW(SettingsRowId::Uptime, Row));
	checkInt("dragged to the end: UPTIME, the last row, at y 190, so the scroll is 620 px",
	         last.y, 190);
	checkTrue("and is whole", last.h == 44);
	checkInt("LAST RESTART above it at y 140", ctlRect(UI_ROW(SettingsRowId::LastRestart, Row)).y, 140);
	checkInt("VERSION above the DIAGNOSTICS caption at y 70", ctlRect(UI_ROW(SettingsRowId::Version, Row)).y, 70);
	dragList(60, 180);
	dragList(60, 180);
	dragList(60, 180);
	dragList(60, 180);
	checkInt("dragged back: MOTOR POLES at y 60 again", ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
}

/**
 * @brief DIAGNOSTICS, the last section: LAST RESTART as captured at boot and
 *        UPTIME, read-only; each reason's colour; UPTIME repaints once a
 *        second while it shows, not within the second, and not while it is
 *        scrolled out.
 */
static void testSettingsDiagnostics() {
	section("SETTINGS: DIAGNOSTICS, LAST RESTART and UPTIME");
	fakeSetChipReset(DIAG_CR_HAD_BOR);
	fakeSetWatchdog(0, false);
	diagCaptureRestart();
	openSettings();

	// At 0 px UPTIME is out of view: a pixel put in the list stays, so
	// nothing repaints when the second turns.
	menuFrames(2);
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	menuFrames(41);
	checkTrue("UPTIME out of view: no repaint as the seconds pass",
	          pixel(120, 100) == 0x1234);

	for (int i = 0; i < 4; i++) dragList(230, -180);
	UiRect last = ctlRect(UI_ROW(SettingsRowId::LastRestart, Row));
	checkInt("LAST RESTART at y 140 at the end of the list", last.y, 140);
	checkTrue("DIAGNOSTICS' caption at y 128, 8 px into it",
	          fakeTextIs(UI_MARGIN, 128, "DIAGNOSTICS", C_TEXT, C_BG, 1));
	checkTrue("LAST RESTART's label",
	          fakeTextIs(UI_MARGIN, 140 + 18, "LAST RESTART", C_DIM, C_BG, 1));
	checkTrue("a brown-out reads BROWN-OUT in amber, right-aligned to x 232",
	          fakeTextIs(232 - gfxTextW("BROWN-OUT", 1), 140 + 18, "BROWN-OUT", C_AMBER, C_BG, 1));
	checkTrue("LAST RESTART has no control",
	          !ctlPresent(UI_ROW(SettingsRowId::LastRestart, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::LastRestart, Toggle)) &&
	          !ctlEnabled(UI_ROW(SettingsRowId::LastRestart, Row)));
	checkTrue("UPTIME has no control",
	          !ctlPresent(UI_ROW(SettingsRowId::Uptime, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::Uptime, Toggle)) &&
	          !ctlEnabled(UI_ROW(SettingsRowId::Uptime, Row)));

	char up[24];
	diagUptimeText(up, sizeof(up), diagUptimeSeconds());
	checkTrue("UPTIME's label", fakeTextIs(UI_MARGIN, 190 + 18, "UPTIME", C_DIM, C_BG, 1));
	checkTrue("UPTIME reads the time since boot, H:MM:SS",
	          fakeTextIs(232 - gfxTextW(up, 1), 190 + 18, up, C_GRID, C_BG, 1));
	// Frames to the first one of the next whole second.
	uint32_t s0 = diagUptimeSeconds();
	while (diagUptimeSeconds() == s0) menuFrames(1);
	diagUptimeText(up, sizeof(up), diagUptimeSeconds());
	checkTrue("a second later it reads one second more, with no touch",
	          fakeTextIs(232 - gfxTextW(up, 1), 190 + 18, up, C_GRID, C_BG, 1));

	// Within that second nothing repaints: a pixel put in the list, away from
	// UPTIME's value, stays for 30 frames, 750 ms. The next second repaints.
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	uint32_t s1 = diagUptimeSeconds();
	menuFrames(30);
	checkTrue("still the same second", diagUptimeSeconds() == s1);
	checkTrue("UPTIME in view: no repaint within a second", pixel(120, 100) == 0x1234);
	while (diagUptimeSeconds() == s1) menuFrames(1);
	checkTrue("and a repaint once the next second turns", pixel(120, 100) != 0x1234);

	// Each reason's colour: amber for a fault, the grid colour otherwise.
	struct { uint32_t cr, reason; int boot; const char *text; uint16_t col; } shown[] = {
		{ DIAG_CR_HAD_POR,            0, 0, "POWER-ON", C_GRID },
		{ DIAG_CR_HAD_GLITCH_DETECT,  0, 0, "GLITCH",   C_AMBER },
		{ DIAG_CR_HAD_WATCHDOG_RESET_RSM, 0, 0, "WATCHDOG", C_AMBER },
		{ DIAG_CR_HAD_POR,            1, DIAG_BOOT_TYPE_FLASH_UPDATE, "UPDATE", C_GRID },
		{ DIAG_CR_HAD_POR,            1, 0, "REBOOT",   C_GRID },
	};
	for (const auto &c : shown) {
		fakeSetChipReset(c.cr);
		fakeSetWatchdog(c.reason, false);
		fakeSetBootType(c.boot);
		diagCaptureRestart();
		tapId(UI_FIX(UI_BACK));
		tapId(menuRow(&UI_SETTINGS));
		for (int i = 0; i < 4; i++) dragList(230, -180);
		char what[64];
		snprintf(what, sizeof(what), "%s in %s", c.text, c.col == C_AMBER ? "amber" : "the grid colour");
		checkTrue(what, fakeTextIs(232 - gfxTextW(c.text, 1), 140 + 18, c.text, c.col, C_BG, 1));
	}
	checkControlTable("SETTINGS, DIAGNOSTICS");
	fakeSetWatchdog(0, false);
	fakeSetBootType(0);
	fakeSetChipReset(DIAG_CR_HAD_POR);
	diagCaptureRestart();
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief Hold HOLD DEFAULTS until it fires, then lift. */
static void holdReset() {
	pressId(UI_FIX(SettingsCtl::Defaults));
	holdId(UI_FIX(SettingsCtl::Defaults), 41);
	fakeRelease(); menuFrames(1);
}

/** @brief A control cut by the viewport takes a press only while 40 px show. */
static void testSettingsCutControl() {
	section("SETTINGS: a cut control takes a press only while 40 px show");
	openSettings();
	// DSHOT's row starts at content y 190, y 230 on the panel: 10 px show.
	UiControl c;
	checkTrue("DSHOT's + is in the table", ctlFind(UI_ROW(SettingsRowId::Speed, Plus), &c));
	checkInt("cut to 10 px", c.rect.h, 10);
	checkTrue("and does not act", !c.enabled);
	uint16_t kbaud = settings()->dshotKbaud;
	fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2); menuFrames(2);
	fakeRelease(); menuFrames(1);
	checkInt("a press on the 10 px that show changes nothing", settings()->dshotKbaud, kbaud);
	// A drag of 40 px from the label of MAX THROTTLE: the row shows whole.
	dragList(150, -40);
	checkTrue("scrolled 40 px: DSHOT's + shows 44 px and acts",
	          ctlFind(UI_ROW(SettingsRowId::Speed, Plus), &c) && c.rect.h == 44 && c.enabled);
	tapId(UI_ROW(SettingsRowId::Speed, Plus));
	checkTrue("and a tap steps it", settings()->dshotKbaud != kbaud);
	holdReset();
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief Every row by id: each stepper both ways, each toggle, and the read-only row. */
static void testSettingsRows() {
	section("SETTINGS: each row by id");
	openSettings();
	Settings before = *settings();
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	checkInt("MOTOR POLES + steps by 2", settings()->poles, before.poles + 2);
	checkInt("and reaches the eRPM divisor", fakePoles(), before.poles + 2);
	tapId(UI_ROW(SettingsRowId::Poles, Minus));
	checkInt("MOTOR POLES - steps back", settings()->poles, before.poles);
	tapId(UI_ROW(SettingsRowId::MaxThrottle, Plus));
	checkInt("MAX THROTTLE + steps by 5 %", settings()->maxThrottle, before.maxThrottle + MAX_THROTTLE_STEP);
	revealRow((int)SettingsRowId::Sig);
	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	checkTrue("SIG PIN + moves the pin", settings()->dshotPin != before.dshotPin);
	checkTrue("and AUX takes SIG's old pin", settings()->kissPin == before.dshotPin);
	revealRow((int)SettingsRowId::Speed);
	tapId(UI_ROW(SettingsRowId::Speed, Minus));
	checkTrue("DSHOT - changes the rate", settings()->dshotKbaud != before.dshotKbaud);
	revealRow((int)SettingsRowId::Aux);
	checkTrue("AUX PIN acts whatever KISS is", ctlEnabled(UI_ROW(SettingsRowId::Aux, Plus)));
	tapId(UI_ROW(SettingsRowId::Aux, Plus));
	checkTrue("AUX PIN + swaps the two back",
	          settings()->dshotPin == before.dshotPin && settings()->kissPin == before.kissPin);
	revealRow((int)SettingsRowId::ServoSupply);
	tapId(UI_ROW(SettingsRowId::ServoSupply, Plus));
	checkInt("SERVO SUPPLY + steps to GP28", settings()->servoSupply, 1);
	tapId(UI_ROW(SettingsRowId::ServoSupply, Minus));
	checkInt("SERVO SUPPLY - steps back to OWN", settings()->servoSupply, 0);
	revealRow((int)SettingsRowId::Contrast);
	tapId(UI_ROW(SettingsRowId::Contrast, Toggle));
	checkTrue("CONTRAST toggles", settings()->highContrast != before.highContrast);
	tapId(UI_ROW(SettingsRowId::Contrast, Toggle));
	revealRow((int)SettingsRowId::Backlight);
	tapId(UI_ROW(SettingsRowId::Backlight, Minus));
	checkTrue("BACKLIGHT - dims", settings()->backlight < before.backlight);
	revealRow((int)SettingsRowId::BuiltFor);
	checkTrue("BUILT FOR has no control", !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Toggle)));

	holdReset();
	checkInt("DEFAULTS restores the pole count", settings()->poles, DEFAULT_MOTOR_POLES);
	checkInt("and the ceiling", settings()->maxThrottle, DEFAULT_MAX_THROTTLE);
	checkInt("and the eRPM divisor follows", fakePoles(), DEFAULT_MOTOR_POLES);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief BACKLIGHT stops at 16, and a step that changes nothing applies nothing. */
static void testBacklightFloorAndNoOpSteps() {
	section("SETTINGS: BACKLIGHT's floor of 16, and steps that change nothing");
	openSettings();
	settings()->backlight = 40;
	revealRow((int)SettingsRowId::Backlight);
	tapId(UI_ROW(SettingsRowId::Backlight, Minus));
	checkInt("40, then - : 16, the floor", settings()->backlight, 16);
	checkTrue("no ADJUSTED note: the note reads UNSAVED", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	tapId(UI_ROW(SettingsRowId::Backlight, Minus));
	checkInt("- at 16 stays at 16", settings()->backlight, 16);

	// Save at 255, then + changes nothing: the note keeps SAVED.
	settings()->backlight = 255;
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 41);
	fakeRelease(); menuFrames(1);
	checkTrue("saved: the note reads SAVED", noteIs("SAVED", C_GREEN));
	tapId(UI_ROW(SettingsRowId::Backlight, Plus));
	checkInt("+ at 255 stays at 255", settings()->backlight, 255);
	checkTrue("and applies nothing: the note still reads SAVED", noteIs("SAVED", C_GREEN));

	// KISS on: a step of SIG PIN or AUX PIN swaps the two free pins, and a
	// second step swaps them back to what is saved.
	revealRow((int)SettingsRowId::Kiss);
	if (!settings()->kissEnable) tapId(UI_ROW(SettingsRowId::Kiss, Toggle));
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 41);
	fakeRelease(); menuFrames(1);
	checkTrue("KISS on and saved: the note reads SAVED", settings()->kissEnable && noteIs("SAVED", C_GREEN));
	uint8_t esc = settings()->dshotPin, kiss = settings()->kissPin;
	revealRow((int)SettingsRowId::Sig);
	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	checkTrue("SIG PIN + with KISS on swaps SIG and AUX",
	          settings()->dshotPin == kiss && settings()->kissPin == esc);
	checkTrue("and the note reads UNSAVED", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	revealRow((int)SettingsRowId::Aux);
	tapId(UI_ROW(SettingsRowId::Aux, Plus));
	checkTrue("AUX PIN + swaps them back",
	          settings()->dshotPin == esc && settings()->kissPin == kiss);
	checkTrue("what is saved: nothing unsaved",
	          !settingsDirty() && noteIs("SAVED SETTINGS IN USE", C_DIM));
	holdReset();
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief Save the live settings with a 1025 ms hold on HOLD TO SAVE. */
static void holdSave() {
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 41);
	fakeRelease(); menuFrames(1);
}

/**
 * @brief DEFAULTS' note: shown while the defaults DEFAULTS loaded are live and a restart
 *        would bring the saved values back, and at no other time.
 */
static void testResetNote() {
	section("SETTINGS: DEFAULTS LOADED - RESTART TO UNDO after DEFAULTS");
	static const char *const NOTE = "DEFAULTS LOADED - RESTART TO UNDO";
	checkTrue("the note fits the line", UI_MARGIN + gfxTextW(NOTE, 1) <= GFX_W - UI_MARGIN);
	openSettings();
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	holdSave();
	checkTrue("saved poles other than the default", !settingsDirty() &&
	          settings()->poles != DEFAULT_MOTOR_POLES);

	holdReset();
	checkTrue("DEFAULTS: the defaults live, unsaved", settingsDirty() &&
	          settings()->poles == DEFAULT_MOTOR_POLES);
	checkTrue("the note reads DEFAULTS LOADED - RESTART TO UNDO", noteIs(NOTE, C_AMBER));
	menuFrames(3);
	checkTrue("and keeps it", noteIs(NOTE, C_AMBER));

	// Leaving and coming back changes nothing: still true.
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(2);
	checkTrue("SETTINGS again", uiCurrentScreen() == UiScreenId::Settings);
	checkTrue("SETTINGS again: the note still reads DEFAULTS LOADED", noteIs(NOTE, C_AMBER));

	// Another change ends it, also one that steps back to the default.
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Minus));
	checkTrue("a change after DEFAULTS: UNSAVED", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	checkTrue("back at the default: still UNSAVED, not the DEFAULTS note",
	          settings()->poles == DEFAULT_MOTOR_POLES && settingsDirty() &&
	          noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));

	// A save ends it: a restart loads these values now.
	holdReset();
	checkTrue("DEFAULTS again: the note", noteIs(NOTE, C_AMBER));
	holdSave();
	checkTrue("HOLD TO SAVE: SAVED, not the DEFAULTS note", !settingsDirty() && noteIs("SAVED", C_GREEN));
	menuFrames(2);
	checkTrue("and it does not come back", noteIs("SAVED", C_GREEN));

	// DEFAULTS while the defaults are what a restart loads: nothing to undo.
	holdReset();
	checkTrue("DEFAULTS over saved defaults: no DEFAULTS note", !settingsDirty() && !noteIs(NOTE, C_AMBER));

	// A change in KST's SERVO SUPPLY row, which writes the settings itself.
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	holdSave();
	holdReset();
	checkTrue("saved other poles, DEFAULTS: the note", noteIs(NOTE, C_AMBER));
	revealRow((int)SettingsRowId::ServoSupply);
	tapId(UI_ROW(SettingsRowId::ServoSupply, Plus));
	checkTrue("SERVO SUPPLY stepped: UNSAVED", settings()->servoSupply == 1 &&
	          noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	tapId(UI_ROW(SettingsRowId::ServoSupply, Minus));
	checkTrue("and back: still not the DEFAULTS note", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));

	// A change made while SETTINGS is not open ends it too.
	holdReset();
	checkTrue("DEFAULTS again: the note", noteIs(NOTE, C_AMBER));
	tapId(UI_FIX(UI_BACK));
	settings()->backlight = (uint8_t)(settings()->backlight - 8);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(2);
	checkTrue("changed elsewhere: UNSAVED on return", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));

	// A failed save ends it: the sector may already be erased and written.
	holdReset();
	checkTrue("DEFAULTS: the note", noteIs(NOTE, C_AMBER));
	fakeFlashSetWritable(false);
	holdSave();
	checkTrue("HOLD TO SAVE refused by the flash: SAVE FAILED",
	          noteIs("SAVE FAILED - FLASH REFUSED THE WRITE", C_RED));
	tapId(UI_FIX(UI_BACK));
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(2);
	checkTrue("SETTINGS again: UNSAVED, not the DEFAULTS note",
	          settingsDirty() && noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	fakeFlashSetWritable(true);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/**
 * @brief HOLD DEFAULTS: a tap loads nothing, 1000 ms on the button load the
 *        defaults, a slide off cancels, and its bar and fill show the hold.
 */
static void testResetHold() {
	section("SETTINGS: HOLD DEFAULTS loads the defaults after a 1 s hold");
	openSettings();
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	holdSave();
	const uint8_t saved = settings()->poles;
	checkTrue("saved poles other than the default", !settingsDirty() && saved != DEFAULT_MOTOR_POLES);

	const UiRect r = ctlRect(UI_FIX(SettingsCtl::Defaults));
	// Two lines, 12 px apart, centred in the 72 x 46 button: y 281 and 293.
	checkTrue("the label reads HOLD over DEFAULTS, amber, centred",
	          fakeTextIs(r.x + (r.w - gfxTextW("HOLD", 1)) / 2, r.y + 13, "HOLD", C_AMBER, C_PANEL, 1) &&
	          fakeTextIs(r.x + (r.w - gfxTextW("DEFAULTS", 1)) / 2, r.y + 25, "DEFAULTS", C_AMBER,
	                     C_PANEL, 1));
	checkTrue("DEFAULTS, 47 px, inside the 72 px with 4 px each side",
	          gfxTextW("DEFAULTS", 1) == 47 && r.w == 72 && 47 + 2 * 4 <= r.w);
	checkTrue("no RESET left on the button", !fakeTextIs(r.x + (r.w - gfxTextW("HOLD RESET", 1)) / 2,
	                                                     r.y + (r.h - 7) / 2, "HOLD RESET", C_AMBER,
	                                                     C_PANEL, 1));
	tapId(UI_FIX(SettingsCtl::Defaults));
	checkTrue("a tap: nothing loaded", settings()->poles == saved && !settingsDirty());

	// The press frame starts the timer; frame k is k x 25 ms after it.
	pressId(UI_FIX(SettingsCtl::Defaults));
	holdId(UI_FIX(SettingsCtl::Defaults), 20);
	{
		const uint16_t *fb = gfxBuffer() + (r.y - 4) * GFX_W;
		checkTrue("500 ms: the bar over DEFAULTS, 36 px of the 72",
		          fb[r.x] == C_LIME && fb[r.x + 35] == C_LIME && fb[r.x + 36] == C_BG);
		checkTrue("and the button red while held, as every ui_hold hold",
		          pixel(r.x + 4, r.y + r.h - 5) == C_RED);
		checkTrue("its two lines in C_ONACCENT on the red, pressed 1 px in",
		          fakeTextIs(r.x + 1 + (r.w - gfxTextW("HOLD", 1)) / 2, r.y + 14, "HOLD", C_ONACCENT,
		                     C_RED, 1) &&
		          fakeTextIs(r.x + 1 + (r.w - gfxTextW("DEFAULTS", 1)) / 2, r.y + 26, "DEFAULTS",
		                     C_ONACCENT, C_RED, 1));
	}
	holdId(UI_FIX(SettingsCtl::Defaults), 19);
	checkTrue("975 ms: nothing loaded", settings()->poles == saved && !settingsDirty());
	holdId(UI_FIX(SettingsCtl::Defaults), 1);
	checkTrue("1000 ms: the defaults live, unsaved",
	          settings()->poles == DEFAULT_MOTOR_POLES && settingsDirty() &&
	          noteIs("DEFAULTS LOADED - RESTART TO UNDO", C_AMBER));
	settings()->poles = saved;
	holdId(UI_FIX(SettingsCtl::Defaults), 60);
	checkTrue("held 1.5 s more: one firing per press, the change after it kept",
	          settings()->poles == saved);
	settings()->poles = DEFAULT_MOTOR_POLES;
	fakeRelease(); menuFrames(1);
	checkTrue("lifted: the button back to its panel fill", pixel(r.x + 4, r.y + r.h - 5) == C_PANEL);

	// Back to the saved poles: a slide off the button mid-hold loads nothing.
	tapId(UI_FIX(UI_BACK));
	settingsLoad();
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(2);
	checkTrue("saved poles again", settings()->poles == saved && !settingsDirty());
	pressId(UI_FIX(SettingsCtl::Defaults));
	holdId(UI_FIX(SettingsCtl::Defaults), 20);
	holdId(UI_FIX(SettingsCtl::Save), 40);
	fakeRelease(); menuFrames(1);
	checkTrue("slid onto SAVE after 500 ms, held 1 s more: nothing loaded, nothing saved",
	          settings()->poles == saved && !settingsDirty());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief The note and HOLD TO SAVE do not move with the list, and the hold saves at any scroll. */
static void testSettingsSaveAnywhere() {
	section("SETTINGS: UNSAVED and HOLD TO SAVE at every scroll");
	openSettings();
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	menuFrames(1);
	UiRect save0 = ctlRect(UI_FIX(SettingsCtl::Save));
	checkTrue("a change: the settings are unsaved", settingsDirty());
	for (int scroll = 0; scroll < 3; scroll++) {
		char what[64];
		snprintf(what, sizeof(what), "scroll step %d: HOLD TO SAVE stays at y 268", scroll);
		checkInt(what, ctlRect(UI_FIX(SettingsCtl::Save)).y, save0.y);
		snprintf(what, sizeof(what), "scroll step %d: the note reads UNSAVED in amber", scroll);
		checkTrue(what, noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
		dragList(200, -120);
	}
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 41);
	fakeRelease(); menuFrames(1);
	checkTrue("a 1025 ms hold saves at the end of the list", !settingsDirty());
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	checkTrue("back on MENU", uiCurrentScreen() == UiScreenId::Menu);
	UiRect row = ctlRect(menuRow(&UI_SETTINGS));
	int amber = 0;
	for (int y = row.y; y < row.y + row.h; y++)
		for (int x = row.x; x < row.x + row.w; x++) amber += pixel(x, y) == C_AMBER;
	checkInt("MENU: the SETTINGS row reads nothing once saved, no amber UNSAVED", amber, 0);
	tapId(UI_FIX(UI_BACK));
}

/** @brief Unsaved changes: MENU's SETTINGS row reads UNSAVED in amber. */
static void testMenuUnsaved() {
	section("MENU: SETTINGS reads UNSAVED while a change is not saved");
	openSettings();
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	UiRect row = ctlRect(menuRow(&UI_SETTINGS));
	int amber = 0;
	for (int y = row.y; y < row.y + row.h; y++)
		for (int x = row.x; x < row.x + row.w; x++) amber += pixel(x, y) == C_AMBER;
	checkTrue("the SETTINGS row shows amber text", amber > 0);
	tapId(UI_FIX(UI_BACK));
}

/** @brief A whole block of @p version in the fake flash, with @p s as its payload. */
static void putBlock(uint16_t version, const Settings *s) {
	SettingsBlock b;
	memset(&b, 0, sizeof(b));
	b.magic = SETTINGS_MAGIC;
	b.version = version;
	b.size = sizeof(Settings);
	b.s = *s;
	b.crc32 = settingsCrc32(&b, offsetof(SettingsBlock, crc32));
	fakeFlashClear();
	memcpy(fakeFlashBytes(), &b, sizeof(b));
}

/** @brief A block of this version holding @p s, as a save writes it. */
static void storeValid(const Settings *s) { putBlock(SETTINGS_VERSION, s); }

/** @brief The amber pixels on MENU's SETTINGS row. */
static int settingsRowAmber() {
	UiRect row = ctlRect(menuRow(&UI_SETTINGS));
	int amber = 0;
	for (int y = row.y; y < row.y + row.h; y++)
		for (int x = row.x; x < row.x + row.w; x++) amber += pixel(x, y) == C_AMBER;
	return amber;
}

/**
 * @brief Settings an update discarded: MENU's SETTINGS row reads RESET and
 *        SETTINGS' note says so until a save lands; never on a board that
 *        never had settings.
 */
static void testResetByUpdate() {
	section("SETTINGS: a reset by an update shows until a save lands");
	static const char *const NOTE = "RESET BY UPDATE - CHECK AND SAVE";
	checkTrue("the note fits the line", UI_MARGIN + gfxTextW(NOTE, 1) <= GFX_W - UI_MARGIN);
	char b[32];
	uint16_t col = 0, fill = 0;

	// A board that never had settings.
	freshTester();
	openMenu();
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("blank flash: MENU's SETTINGS row reads nothing", b[0] == '\0' && settingsRowAmber() == 0);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("and SETTINGS reads NOTHING SAVED: DEFAULTS IN USE", noteIs("NOTHING SAVED: DEFAULTS IN USE", C_DIM));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));

	// A version 6 block, of a layout this firmware does not know.
	Settings old;
	settingsDefaults(&old);
	old.poles = 22;
	old.maxThrottle = 1000;
	putBlock(SETTINGS_VERSION + 1, &old);
	settingsLoad();
	uiInit();
	menuFrames(2);
	checkTrue("a version 6 block: discarded, the defaults live",
	          !settingsStored() && settingsResetByUpdate() && settings()->poles == DEFAULT_MOTOR_POLES);
	openMenu();
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("MENU's SETTINGS row reads UPDATED, amber", strcmp(b, "UPDATED") == 0 && col == C_AMBER);
	checkTrue("drawn on the row", settingsRowAmber() > 0);
	fakeDumpFrame("shot_menu_updated.ppm");
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("SETTINGS: the note reads RESET BY UPDATE, amber", noteIs(NOTE, C_AMBER));
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	checkTrue("a change: UNSAVED outranks it", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("and MENU reads UNSAVED", strcmp(b, "UNSAVED") == 0);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	tapId(UI_ROW(SettingsRowId::Poles, Minus));
	checkTrue("stepped back: RESET BY UPDATE again", !settingsDirty() && noteIs(NOTE, C_AMBER));
	fakeFlashSetWritable(false);
	holdSave();
	checkTrue("a save the flash refuses: SAVE FAILED, the reset kept",
	          noteIs("SAVE FAILED - FLASH REFUSED THE WRITE", C_RED) && settingsResetByUpdate());
	fakeFlashSetWritable(true);
	holdSave();
	checkTrue("HOLD TO SAVE lands: SAVED", noteIs("SAVED", C_GREEN) && !settingsResetByUpdate());
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("MENU: the row reads nothing", b[0] == '\0' && settingsRowAmber() == 0);
	tapId(UI_FIX(UI_BACK));

	// The splash, on the next boot after the update and after the save.
	putBlock(SETTINGS_VERSION + 1, &old);
	settingsLoad();
	uiDrawSplash();
	checkTrue("splash: SETTINGS RESET BY UPDATE at y 40, amber",
	          fakeTextCentredIs(40, "SETTINGS RESET BY UPDATE", C_AMBER, C_BG, 1));
	checkTrue("and CHECK MENU > SETTINGS at y 52, amber",
	          fakeTextCentredIs(52, "CHECK MENU > SETTINGS", C_AMBER, C_BG, 1));
	int x0 = 0, x1 = 0;
	checkTrue("nothing else above MINIRC but the two lines",
	          fakeInkExtent(40, 19, C_BG, &x0, &x1) && x0 >= UI_MARGIN && x1 < GFX_W - UI_MARGIN &&
	          !fakeInkExtent(0, 40, C_BG, &x0, &x1) && !fakeInkExtent(59, 45, C_BG, &x0, &x1));
	fakeDumpFrame("shot_splash_reset.ppm");

	// With TOUCH CONTROLLER NOT FOUND too: three lines, none on another, the
	// name, the version and the warning where they were.
	fakeTouchAnswers(false);
	touchInit();
	uiDrawSplash();
	checkTrue("both: SETTINGS RESET BY UPDATE at y 40",
	          fakeTextCentredIs(40, "SETTINGS RESET BY UPDATE", C_AMBER, C_BG, 1));
	checkTrue("CHECK MENU > SETTINGS at y 52",
	          fakeTextCentredIs(52, "CHECK MENU > SETTINGS", C_AMBER, C_BG, 1));
	checkTrue("TOUCH CONTROLLER NOT FOUND at y 64",
	          fakeTextCentredIs(64, UI_SPLASH_TOUCH_TEXT, C_AMBER, C_BG, 1));
	checkTrue("a clear row under each line, 8 rows for high contrast",
	          !fakeInkExtent(48, 4, C_BG, &x0, &x1) && !fakeInkExtent(60, 4, C_BG, &x0, &x1));
	checkTrue("clear from the touch line to the name at y 104",
	          !fakeInkExtent(72, 32, C_BG, &x0, &x1));
	char pinLine[48];
	snprintf(pinLine, sizeof(pinLine), UI_SPLASH_PIN_FMT, (unsigned)settings()->dshotPin);
	checkTrue("the name, the version and the warning in place",
	          fakeTextCentredIs(104, "MINIRC", C_LIME, C_BG, 4) &&
	          fakeTextCentredIs(260, "VERSION " FW_VERSION_STRING, C_GRID, C_BG, 1) &&
	          fakeTextCentredIs(288, pinLine, C_AMBER, C_BG, 1));
	fakeDumpFrame("shot_splash_reset_no_touch.ppm");
	fakeTouchAnswers(true);
	touchInit();
	fakeFlashClear();
	settingsLoad();
	uiDrawSplash();
	checkTrue("blank flash: no reset lines", !fakeInkExtent(0, 104, C_BG, &x0, &x1));
	storeValid(&old);
	settingsLoad();
	uiDrawSplash();
	checkTrue("a block of this version: no reset lines",
	          settingsStored() && !fakeInkExtent(0, 104, C_BG, &x0, &x1));
	freshTester();
}

/**
 * @brief Settings an update carried over from a version 4 block: the splash's
 *        two lines, MENU's UPDATED and SETTINGS' note until a save lands.
 */
static void testCarriedOver() {
	section("SETTINGS: a carry-over by an update shows until a save lands");
	static const char *const NOTE = "CARRIED OVER BY UPDATE - CHECK, SAVE";
	checkTrue("the note fits the line", UI_MARGIN + gfxTextW(NOTE, 1) <= GFX_W - UI_MARGIN);
	checkTrue("the splash's lines fit",
	          gfxTextW(UI_SPLASH_CARRIED_TEXT, 1) <= GFX_W - 2 * UI_MARGIN &&
	          gfxTextW(UI_SPLASH_CARRIED_TEXT2, 1) <= GFX_W - 2 * UI_MARGIN);
	char b[32];
	uint16_t col = 0, fill = 0;

	// A version 4 block, as the firmware before CURRENT SENSE wrote it: the
	// layout of today's, the sensor's byte its padding.
	Settings old;
	settingsDefaults(&old);
	old.poles = 22;
	old.maxThrottle = 1000;
	old.currentSense = 0;
	putBlock(4, &old);
	settingsLoad();
	uiInit();
	menuFrames(2);
	checkTrue("a version 4 block: carried over, its settings live",
	          settingsStored() && settingsCarriedOver() && !settingsResetByUpdate() &&
	          settings()->poles == 22 && settings()->maxThrottle == 1000);
	openMenu();
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("MENU's SETTINGS row reads UPDATED, amber", strcmp(b, "UPDATED") == 0 && col == C_AMBER);
	checkTrue("drawn on the row", settingsRowAmber() > 0);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("SETTINGS: the note reads CARRIED OVER, amber", noteIs(NOTE, C_AMBER));
	fakeDumpFrame("shot_settings_carried.ppm");
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Minus));
	checkTrue("a change: UNSAVED outranks it", noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	checkTrue("stepped back: CARRIED OVER again", !settingsDirty() && noteIs(NOTE, C_AMBER));
	fakeFlashSetWritable(false);
	holdSave();
	checkTrue("a save the flash refuses: SAVE FAILED, the carry-over kept",
	          noteIs("SAVE FAILED - FLASH REFUSED THE WRITE", C_RED) && settingsCarriedOver());
	fakeFlashSetWritable(true);
	holdSave();
	checkTrue("HOLD TO SAVE with nothing changed lands: SAVED",
	          noteIs("SAVED", C_GREEN) && !settingsCarriedOver());
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("MENU: the row reads nothing", b[0] == '\0' && settingsRowAmber() == 0);
	tapId(UI_FIX(UI_BACK));
	settingsLoad();
	checkTrue("the next boot: a version 5 block, the settings kept, no notice",
	          settingsStored() && !settingsCarriedOver() && settings()->poles == 22);

	// The splash, on the boot after the update.
	putBlock(4, &old);
	settingsLoad();
	uiDrawSplash();
	checkTrue("splash: the first line at y 40, amber",
	          fakeTextCentredIs(40, UI_SPLASH_CARRIED_TEXT, C_AMBER, C_BG, 1));
	checkTrue("and the second at y 52, amber",
	          fakeTextCentredIs(52, UI_SPLASH_CARRIED_TEXT2, C_AMBER, C_BG, 1));
	checkTrue("not the reset's", !fakeTextCentredIs(40, "SETTINGS RESET BY UPDATE", C_AMBER, C_BG, 1));
	int x0 = 0, x1 = 0;
	checkTrue("nothing else above MINIRC but the two lines",
	          fakeInkExtent(40, 19, C_BG, &x0, &x1) && x0 >= UI_MARGIN && x1 < GFX_W - UI_MARGIN &&
	          !fakeInkExtent(0, 40, C_BG, &x0, &x1) && !fakeInkExtent(59, 45, C_BG, &x0, &x1));
	fakeDumpFrame("shot_splash_carried.ppm");
	fakeFlashClear();
	settingsLoad();
	freshTester();
}

/**
 * @brief SIG PIN moves DShot at once: SETTINGS repeats the splash's warning
 *        for the live pin while it is not the pin the splash named.
 */
static void testEscPinNote() {
	section("SETTINGS: SIG PIN off the boot pin repeats the splash's warning");
	openSettings();
	menuFrames(1);
	const uint8_t boot = settings()->dshotPin;
	checkTrue("at the boot pin: no warning", noteIs("NOTHING SAVED: DEFAULTS IN USE", C_DIM));
	revealRow((int)SettingsRowId::Sig);
	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	const uint8_t moved = settings()->dshotPin;
	char want[40];
	snprintf(want, sizeof(want), UI_SPLASH_PIN_FMT, (unsigned)moved);
	checkTrue("SIG PIN stepped: DShot on the other pin", moved != boot && fakeWiring().dshotPin == moved);
	checkTrue("the splash's warning for it at y 230, amber",
	          fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1));
	checkTrue("and the note line still reads UNSAVED",
	          settingsDirty() && noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	checkTrue("it fits the line", UI_MARGIN + gfxTextW(want, 1) <= GFX_W - UI_MARGIN);
	fakeDumpFrame("shot_settings_pin.ppm");
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	checkTrue("another change: the warning and UNSAVED",
	          fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1) &&
	          noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	holdSave();
	checkTrue("saved: the warning and SAVED",
	          !settingsDirty() && fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1) &&
	          noteIs("SAVED", C_GREEN));
	// The list ends at y 227 while the warning shows: it scrolls 632 px.
	for (int k = 0; k < 5; k++) dragList(200, -180);
	checkInt("dragged to the end: UPTIME at y 178, 12 px higher",
	         ctlRect(UI_ROW(SettingsRowId::Uptime, Row)).y, 178);
	checkTrue("and whole", ctlRect(UI_ROW(SettingsRowId::Uptime, Row)).h == 44);
	checkTrue("the warning keeps its line",
	          fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1));
	for (int k = 0; k < 5; k++) dragList(60, 180);
	tapId(UI_FIX(UI_BACK));
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("SETTINGS again: the warning stays, the saved set named",
	          fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1) &&
	          noteIs("SAVED SETTINGS IN USE", C_DIM));
	fakeFlashSetWritable(false);
	revealRow((int)SettingsRowId::Poles);
	tapId(UI_ROW(SettingsRowId::Poles, Minus));
	holdSave();
	checkTrue("a failed save: SAVE FAILED, and the warning",
	          noteIs("SAVE FAILED - FLASH REFUSED THE WRITE", C_RED) &&
	          fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1));
	fakeFlashSetWritable(true);
	revealRow((int)SettingsRowId::Sig);
	tapId(UI_ROW(SettingsRowId::Sig, Minus));
	checkTrue("back on the boot pin: no warning",
	          settings()->dshotPin == boot && !fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1));
	checkInt("and the list to y 239 again: MOTOR POLES at y 60",
	         ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));

	// The warning gone while scrolled to the end: the list takes its 200 px
	// back and the scroll is clamped to 620 px, with nothing blank under it.
	// Once by DEFAULTS, once by the pin set back with SETTINGS open, as a tool's
	// row writes the settings.
	for (int how = 0; how < 2; how++) {
		openSettings();
		menuFrames(1);
		revealRow((int)SettingsRowId::Sig);
		tapId(UI_ROW(SettingsRowId::Sig, Plus));
		for (int k = 0; k < 5; k++) dragList(200, -180);
		checkInt(how ? "pin moved, at the end: UPTIME at y 178"
		             : "pin moved, at the end, before DEFAULTS: UPTIME at y 178",
		         ctlRect(UI_ROW(SettingsRowId::Uptime, Row)).y, 178);
		if (how == 0) holdReset();
		else          settings()->dshotPin = boot;
		menuFrames(1);
		UiRect up = ctlRect(UI_ROW(SettingsRowId::Uptime, Row));
		checkTrue(how ? "pin set back: no warning" : "DEFAULTS: no warning",
		          settings()->dshotPin == boot && !fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1));
		checkInt("the scroll clamped to 620 px: UPTIME at y 190", up.y, 190);
		checkTrue("and whole, its bottom at y 234", up.h == 44);
		checkInt("LAST RESTART at y 140", ctlRect(UI_ROW(SettingsRowId::LastRestart, Row)).y, 140);
		for (int k = 0; k < 5; k++) dragList(60, 180);
		checkInt("dragged back: MOTOR POLES at y 60", ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);
		checkInt("the list ends at y 239 again: DSHOT's row, y 230 to 239, shows 10 px",
		         ctlRect(UI_ROW(SettingsRowId::Speed, Row)).h, 10);
		tapId(UI_FIX(UI_BACK));
		tapId(UI_FIX(UI_BACK));
	}

	// Booted on the other pin: DEFAULTS puts DShot back on the default pin,
	// which the splash did not name: the warning, and DEFAULTS's note under it.
	Settings other;
	settingsDefaults(&other);
	other.dshotPin = moved;
	storeValid(&other);
	settingsLoad();
	uiInit();
	menuFrames(2);
	openMenu();
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("booted on the other pin: no warning",
	          noteIs("SAVED SETTINGS IN USE", C_DIM) &&
	          !fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1));
	holdReset();
	snprintf(want, sizeof(want), UI_SPLASH_PIN_FMT, (unsigned)boot);
	checkTrue("DEFAULTS: the warning for the default pin, and DEFAULTS's note",
	          settings()->dshotPin == boot && fakeTextIs(UI_MARGIN, 230, want, C_AMBER, C_BG, 1) &&
	          noteIs("DEFAULTS LOADED - RESTART TO UNDO", C_AMBER));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	freshTester();
}

/** @brief Every MENU status, in each state it has, fits the registry's widest. */
static void testMenuStatusWidths() {
	section("MENU: every status fits its row's widest");
	freshTester();
	openMenu();
	uint8_t n;
	const UiMenuEntry *e = uiMenuEntries(&n);
	char b[32];
	uint16_t col, fill;
	auto check = [&](const UiMenuEntry &en, const char *state) {
		b[0] = '\0';
		if (en.screen->status) en.screen->status(b, sizeof(b), &col, &fill);
		char what[128];
		snprintf(what, sizeof(what), "%s, %s: \"%s\" within %u characters", en.label, state, b,
		         (unsigned)en.statusWidest);
		checkTrue(what, strlen(b) <= en.statusWidest);
	};
	// A row refused while the claim is not Free keeps its own status: no
	// state of the claim adds a word to a row.
	for (uint8_t i = 0; i < n; i++) check(e[i], "at rest");
	// BEEP's flash, sent and refused.
	int beep = uiMenuRowOf(&UI_BEEP);
	tapId(UI_ROW(beep, Row));
	check(e[beep], "sent");
	escSetArmed(true);
	tapId(UI_ROW(beep, Row));
	check(e[beep], "refused");
	escSetArmed(false);
	menuFrames(16);
	// SD LOG's states.
	int log = uiMenuRowOf(&UI_SDLOG);
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	const SdLogState states[] = { SdLogState::NoCard, SdLogState::Idle, SdLogState::Logging, SdLogState::Error };
	for (SdLogState s : states) { st.state = s; fakeSdLogSet(&st); check(e[log], "a card state"); }
	fakeMscSet(MscState::Serving, 0); check(e[log], "a host holds the card");
	fakeMscSet(MscState::Idle, 0);
	// SETTINGS, unsaved, and reset by an update.
	settings()->poles = (uint8_t)(settings()->poles + 2);
	check(e[uiMenuRowOf(&UI_SETTINGS)], "unsaved");
	{
		Settings d;
		settingsDefaults(&d);
		putBlock(SETTINGS_VERSION + 1, &d);
		settingsLoad();
		check(e[uiMenuRowOf(&UI_SETTINGS)], "reset by an update");
		fakeFlashClear();
	}
	settingsLoad();
	// The two pin readouts at the widest pins the board offers, and at two
	// digits, GP28 and GP29, which no board offers yet.
	settings()->dshotPin = 0;
	settings()->kissPin = 1;
	check(e[uiMenuRowOf(&UI_AM32)], "GP0");
	check(e[uiMenuRowOf(&UI_SERVO)], "GP0+1");
	settings()->dshotPin = 29;
	settings()->kissPin = 28;
	check(e[uiMenuRowOf(&UI_AM32)], "GP29");
	check(e[uiMenuRowOf(&UI_KST)], "GP29");
	check(e[uiMenuRowOf(&UI_HITEC)], "GP29");
	check(e[uiMenuRowOf(&UI_SERVO)], "GP29+28");
	settingsLoad();
	tapId(UI_FIX(UI_BACK));
}

/** @brief Every SETTINGS value, across each row's range, fits the row's widest. */
static void testSettingsValueWidths() {
	section("SETTINGS: every value fits its row's widest");
	openSettings();
	const SettingsSection *secs[16];
	uint8_t ns = uiSettingsSections(secs, 16);
	checkTrue("SETTINGS shows its sections", ns >= 4);
	Settings *s = settings();
	Settings keep = *s;
	bool fits = true;
	char worst[64] = "";
	auto sweep = [&]() {
		for (uint8_t i = 0; i < ns; i++)
			for (uint8_t r = 0; r < secs[i]->count; r++) {
				char b[40];
				uint16_t col;
				secs[i]->rows[r].value(b, sizeof(b), &col);
				if (strlen(b) > secs[i]->rows[r].widest) {
					fits = false;
					snprintf(worst, sizeof(worst), "%s: \"%s\"", secs[i]->rows[r].label, b);
				}
			}
	};
	for (int p = MIN_MOTOR_POLES; p <= MAX_MOTOR_POLES; p += 2) { s->poles = (uint8_t)p; sweep(); }
	for (int t = MAX_THROTTLE_STEP; t <= MAX_THROTTLE_CEILING; t += MAX_THROTTLE_STEP) { s->maxThrottle = (uint16_t)t; sweep(); }
	const uint16_t rates[] = { 150, 300, 600, 1200 };
	for (uint16_t k : rates) { s->dshotKbaud = k; sweep(); }
	for (int pin = 0; pin < 30; pin++) {
		s->dshotPin = (uint8_t)pin; s->kissPin = (uint8_t)pin;
		s->kissEnable = 1; sweep(); s->kissEnable = 0; sweep();
	}
	for (int bl = 16; bl <= 255; bl += 1) { s->backlight = (uint8_t)bl; sweep(); }
	s->highContrast = 1; sweep(); s->highContrast = 0; sweep();
	*s = keep;
	char what[128];
	snprintf(what, sizeof(what), "every value fits its widest%s%s", worst[0] ? "; widest broken by " : "", worst);
	checkTrue(what, fits);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/**
 * @brief SETTINGS values are neutral (menu-b.md, decision 11): the MAX
 *        THROTTLE, SIG PIN, AUX PIN and SERVO SUPPLY values in the text
 *        colour, and no value in cyan, the "changed" colour, or in lime, a
 *        live reading.
 */
static void testSettingsValueColours() {
	section("SETTINGS: values in the text colour, no cyan or lime");
	openSettings();
	const SettingsSection *secs[16];
	uint8_t ns = uiSettingsSections(secs, 16);
	Settings keep = *settings();
	settings()->kissEnable = 1;
	settings()->kissPin = (uint8_t)(settings()->dshotPin == 0 ? 1 : 0);
	int named = 0;
	bool neutral = true;
	char what[64];
	for (uint8_t i = 0; i < ns; i++)
		for (uint8_t r = 0; r < secs[i]->count; r++) {
			const SettingsRow &row = secs[i]->rows[r];
			char b[40];
			uint16_t col;
			row.value(b, sizeof(b), &col);
			if (col == C_CYAN || col == C_LIME) neutral = false;
			if (!strcmp(row.label, "MAX THROTTLE") || !strcmp(row.label, "SIG PIN") ||
			    !strcmp(row.label, "AUX PIN") || !strcmp(row.label, "SERVO SUPPLY")) {
				snprintf(what, sizeof(what), "%s: %s in the text colour", row.label, b);
				checkTrue(what, col == C_TEXT);
				named++;
			}
		}
	checkInt("the four rows found", named, 4);
	checkTrue("no value cyan or lime", neutral);
	*settings() = keep;
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief MENU opens at 0 px again when a tool returns to it. */
static void testMenuScrollResets() {
	section("MENU: a tool's return opens MENU at 0 px");
	static const UiMenuEntry seven[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "KST SERVO", 9, &UI_KST },
		{ "HITEC SERVO", 9, &UI_HITEC }, { "SETTINGS", 9, &UI_SETTINGS },
		{ "SD LOG", 10, &UI_SDLOG },
	};
	freshTester();
	uiMenuUseRegistry(seven, 7);
	openMenu();
	fakePress(120, 250); menuFrames(1);
	for (int k = 1; k <= 12; k++) { fakeHold(120, 250 - 6 * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
	checkInt("scrolled 36 px: the seventh row at y 278", ctlRect(UI_ROW(6, Row)).y, 278);
	tapId(UI_ROW(6, Row));
	checkTrue("its SD LOG opens", uiCurrentScreen() == UiScreenId::SdLog);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK lands on MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("at 0 px: the first row whole at y 62",
	          ctlRect(UI_ROW(0, Row)).y == 62 && ctlRect(UI_ROW(0, Row)).h == 40);
	checkInt("and the seventh cut to 6 px at y 314", ctlRect(UI_ROW(6, Row)).y, 314);
	tapId(UI_FIX(UI_BACK));
	uiMenuUseRegistry(nullptr, 0);
}

/**
 * @brief AM32 CONFIG's own refusal: opened beside another claim, as MENU no
 *        longer allows, it waits, leaves that claim alone, and claims once
 *        the other one ends. The backstop under MENU's disabled rows.
 */
static void testAm32WaitsBesideAClaim() {
	section("AM32 CONFIG: opened beside another claim, it waits");
	freshTester();
	checkTrue("SERVO takes the pins", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	static const TouchState none = {};
	fakeAm32Reset();
	uiAm32Enter();
	for (int i = 0; i < 10; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiAm32Tick(&none); }
	checkTrue("AM32 does not take the pins", pinClaimHolder() == PinUser::Servo);
	UiControl tbl[UI_CONTROLS_MAX];
	checkInt("AM32 stays in its handover: BACK is its only control",
	         UI_AM32.controls(tbl, UI_CONTROLS_MAX), 1);
	checkInt("and the bootloader transport never starts on SERVO's pin",
	         fakeAm32Pin(), 255);
	uiAm32Exit();
	checkTrue("leaving AM32 leaves SERVO's claim alone", pinClaimHolder() == PinUser::Servo);
	uiAm32Enter();
	pinClaimEnd(PinUser::Servo);
	bool claimed = false;
	for (int i = 0; i < 200 && !claimed; i++) {
		fakeAdvance(25); fakePumpPoll(); core1JobPoll();
		if (pinClaimState() == ClaimState::Leaving) pinClaimRestart(PinUser::Servo);
		uiAm32Tick(&none);
		claimed = pinClaimHolder() == PinUser::Am32;
	}
	checkTrue("AM32 claims once SERVO's claim has ended", claimed);
	uiAm32Exit();
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	checkTrue("and its exit frees the pins", pinClaimState() == ClaimState::Free);
	uiInit();
}

/**
 * @brief SERVO SUPPLY is a core section after WIRING, with or without a
 *        MENU row for KST SERVO: no tool carries it.
 */
static void testSupplySection() {
	section("SETTINGS: SERVO SUPPLY is a core section after WIRING");
	checkTrue("KST SERVO carries no SETTINGS section", UI_KST.settings == nullptr);
	static const char *const WANT[] = {
		"ESC TESTER", "WIRING", "SERVO SUPPLY", "DISPLAY", "BOARD", "DIAGNOSTICS",
	};
	static const UiMenuEntry noKst[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 4, &UI_AM32 },
		{ "SERVO TESTER", 7, &UI_SERVO }, { "SETTINGS", 7, &UI_SETTINGS },
		{ "SD LOG", 10, &UI_SDLOG },
	};
	for (int pass = 0; pass < 2; pass++) {
		uiMenuUseRegistry(pass ? noKst : nullptr, pass ? 5 : 0);
		openSettings();
		const SettingsSection *secs[16];
		uint8_t ns = uiSettingsSections(secs, 16);
		bool same = ns == 6;
		for (uint8_t i = 0; same && i < ns; i++) same = strcmp(secs[i]->caption, WANT[i]) == 0;
		checkTrue(pass ? "without KST SERVO on MENU: the same six sections"
		               : "the registry's MENU: ESC TESTER, WIRING, SERVO SUPPLY, DISPLAY, BOARD, DIAGNOSTICS",
		          same);
		checkTrue("SERVO SUPPLY holds SERVO SUPPLY, then CURRENT SENSE",
		          ns > 2 && secs[2]->count == 2 && strcmp(secs[2]->rows[0].label, "SERVO SUPPLY") == 0 &&
		          strcmp(secs[2]->rows[1].label, "CURRENT SENSE") == 0);
		revealRow((int)SettingsRowId::ServoSupply);
		tapId(UI_ROW(SettingsRowId::ServoSupply, Plus));
		checkInt("its + acts: GP28", settings()->servoSupply, 1);
		tapId(UI_ROW(SettingsRowId::ServoSupply, Minus));
		tapId(UI_FIX(UI_BACK));
		tapId(UI_FIX(UI_BACK));
	}
	uiMenuUseRegistry(nullptr, 0);
	freshTester();
}

/**
 * @brief MENU repaints its strip when AUX moves or the claim changes, also
 *        with a registry whose rows claim nothing, and the tools' refusal
 *        reads WAIT: DSHOT RESTARTING.
 */
static void testMenuStripRepaints() {
	section("MENU: the strip follows AUX and the claim");
	checkStr("the tools' refusal", UI_WAIT_RESTART_TEXT, "WAIT: DSHOT RESTARTING");
	static const UiMenuEntry quiet[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "SD LOG", 10, &UI_SDLOG },
	};
	uiMenuUseRegistry(quiet, 2);
	freshTester();
	openMenu();
	menuFrames(2);
	char want[40];
	auto captionIs = [&](uint16_t col) {
		return fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, col, C_BG, 1) ||
		       fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, want, col, C_BG, 1);
	};
	const uint8_t sig = settings()->dshotPin, aux = settings()->kissPin;
	settings()->kissPin = (uint8_t)(aux == 1 ? 28 : 1);   // drawn only, never driven
	menuFrames(1);
	snprintf(want, sizeof(want), "SIG GP%u  AUX GP%u  DSHOT%u", (unsigned)sig,
	         (unsigned)settings()->kissPin, (unsigned)settings()->dshotKbaud);
	checkTrue("AUX changes while MENU shows: the caption follows", captionIs(C_DIM));
	settings()->kissPin = aux;
	menuFrames(1);
	checkTrue("a claim taken", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	framesNoPump(1);
	snprintf(want, sizeof(want), "DSHOT RESTARTING ON SIG GP%u", (unsigned)sig);
	checkTrue("no row claims: the strip still says DShot restarts", captionIs(C_AMBER));
	pinClaimEnd(PinUser::Am32);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	tapId(UI_FIX(UI_BACK));
	uiMenuUseRegistry(nullptr, 0);
	freshTester();
}

void runMenuTests() {
	testSdLogBack();
	testEveryEntry();
	testMenuDisarmAndBeep();
	testPinsBusy();
	testMenuScroll();
	testSettingsRowVisible();
	testSettingsScroll();
	testSettingsDiagnostics();
	testSettingsCutControl();
	testSettingsRows();
	testSettingsSaveAnywhere();
	testBacklightFloorAndNoOpSteps();
	testResetNote();
	testResetHold();
	testMenuUnsaved();
	testResetByUpdate();
	testCarriedOver();
	testEscPinNote();
	testMenuStatusWidths();
	testSettingsValueWidths();
	testSettingsValueColours();
	testMenuScrollResets();
	testAm32WaitsBesideAClaim();
	testSupplySection();
	testMenuStripRepaints();
}
