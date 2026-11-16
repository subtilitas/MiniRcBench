/**
 * @file test_menu.cpp
 * @brief MENU, SETUP and the navigation between them and the tools,
 *        through the real UI.
 *
 * The cases slice C of docs/design/menu.md lists: LOGS' BACK to MENU,
 * every registry entry opened from MENU and back, the tester's disarm on
 * its way to MENU, the rows drawn disabled while the claim restores, MENU's
 * scroll over registries of six and seven rows, and SETUP's list: its
 * scroll range, each row by id, no scroll from a control, the 40 px rule
 * for a cut control, HOLD TO SAVE at every scroll, and the pole and ceiling
 * rows. Slice 3 of docs/design/menu-c.md: MENU the root with no BACK, ESC
 * TESTER its first row. Slice 4: six rows and no scroll, `>` on the three
 * rows that open a hub, SERVO's entries and their lines, and ESC PROGRAM's
 * three entries.
 */

#include "check.h"
#include "fakes.h"
#include "settings.h"
#include "ui.h"
#include "ui_am32.h"
#include "ui_bridge.h"
#include "ui_menu.h"
#include "ui_esc_cmd.h"
#include "ui_registry.h"
#include "ui_sdlog.h"
#include "ui_servo.h"
#include "ui_servo_hub.h"
#include "ui_esc_config.h"
#include "ui_rx_signals.h"
#include "ui_angle.h"
#include "ui_hub.h"
#include "servo_supply.h"
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
#include "clock.h"
#include "sense.h"
#include "board_pins.h"
#include "fake_i2c.h"

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

/** @brief Pixels of @p c from row @p y0 to the panel's foot: the hub's body, under the strip. */
static int colourBelow(uint16_t c, int y0) {
	int n = 0;
	for (int i = y0 * GFX_W; i < GFX_W * GFX_H; i++) n += gfxBuffer()[i] == c;
	return n;
}

/** @brief Whether every pixel of the rectangle is @p c. */
static bool allOf(int x, int y, int w, int h, uint16_t c) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (pixel(i, j) != c) return false;
	return true;
}

/**
 * @brief Whether SETUP's note line, y 254, reads @p text in @p col: the line
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
	// A claim outlives uiInitTester(); let the last test's claim end first.
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	fakeFlashClear(); settingsLoad(); uiInitTester();
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

/** @brief LOGS' BACK returns to MENU in both card states, and MENU's row ESC TESTER to the tester. */
static void testSdLogBack() {
	section("MENU: LOGS' BACK returns to MENU");
	freshTester();
	openMenu();
	tapId(menuRow(&UI_SDLOG));
	checkTrue("LOGS opens from MENU", uiCurrentScreen() == UiScreenId::SdLog);
	tapId(UI_FIX(UI_BACK));
	checkTrue("card idle: BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);

	tapId(menuRow(&UI_SDLOG));
	fakeMscSet(MscState::Serving, 0); menuFrames(1);
	tapId(UI_FIX(UI_BACK));
	checkTrue("card held by a host: BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
	checkTrue("then MENU's row ESC TESTER reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	pressId(UI_FIX(TesterCtl::Arm), IdExpect::Disabled);
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); menuFrames(2);
	checkTrue("arming is refused while the host holds the card", !uiArmed());
	fakeMscSet(MscState::Idle, 0); menuFrames(1);
}

/**
 * @brief The headers of LOGS and SETUP read the names of their MENU rows
 *        (menu-c.md owner decision 6), and the old names are drawn nowhere.
 */
static void testRenamedHeaders() {
	section("MENU: the headers read LOGS and SETUP");
	freshTester();
	openMenu();
	tapId(menuRow(&UI_SDLOG));
	menuFrames(2);
	checkTrue("LOGS opens with its header reading LOGS, 46 px at scale 2",
	          uiCurrentScreen() == UiScreenId::SdLog && gfxTextW("LOGS", 2) == 46 &&
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "LOGS", C_TEXT, C_PANEL, 2) &&
	          !fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SD LOG", C_TEXT, C_PANEL, 2));
	tapId(UI_FIX(UI_BACK));
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(2);
	checkTrue("SETUP opens with its header reading SETUP, 58 px at scale 2",
	          uiCurrentScreen() == UiScreenId::Settings && gfxTextW("SETUP", 2) == 58 &&
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SETUP", C_TEXT, C_PANEL, 2) &&
	          !fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SETTINGS", C_TEXT, C_PANEL, 2));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
}

/** @brief Every registry entry: a tool opens from MENU and its BACK lands on MENU; an action stays. */
static void testEveryEntry() {
	section("MENU: every registry entry opens and returns to MENU");
	freshTester();
	openMenu();
	uint8_t n;
	const UiMenuEntry *e = uiMenuEntries(&n);
	checkInt("the registry has six rows", n, 6);
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
		// The tester has no header: its MENU button is its way back.
		tapId(s == &UI_TESTER ? UI_FIX(TesterCtl::Menu) : UI_FIX(UI_BACK));
		// SERVO TESTER passes the leave page once its job has held the
		// pins: RESTART DSHOT acts 1000 ms after the release.
		for (int k = 0; k < 200 && ctlEnabled(UI_FIX(ServoCtl::Release)) == false &&
		                uiCurrentScreen() == UiScreenId::Servo; k++)
			menuFrames(1);
		if (uiCurrentScreen() == UiScreenId::Servo) tapId(UI_FIX(ServoCtl::Release));
		for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
		snprintf(what, sizeof(what), "BACK from %s lands on MENU", e[i].label);
		checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
	}
	openTester();
}

/**
 * @brief The tester's MENU button disarms on the way back to MENU; ESC
 *        TESTER is MENU's first row, ESC PROGRAM the second and SERVO the
 *        third.
 */
static void testMenuDisarm() {
	section("MENU: the tester's MENU button disarms; ESC TESTER is the first row");
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
	checkInt("ESC TESTER is the first row", uiMenuRowOf(&UI_TESTER), 0);
	checkInt("ESC PROGRAM is the second row", uiMenuRowOf(&UI_ESC_CONFIG), 1);
	checkInt("SERVO is the third row", uiMenuRowOf(&UI_SERVO_HUB_SCREEN), 2);
	checkInt("ESC COMMANDS has no row: ESC PROGRAM opens it", uiMenuRowOf(&UI_ESC_CMD), -1);
	checkControlTable("MENU");
	int sent = fakeBeepRequests();
	tapId(toolEntry(&UI_ESC_CMD));
	checkTrue("ESC COMMANDS' entry opens its page and sends nothing",
	          uiCurrentScreen() == UiScreenId::EscCmd && fakeBeepRequests() == sent);
	tapId(UI_FIX(UI_BACK));
	checkTrue("whose BACK returns to ESC PROGRAM", uiCurrentScreen() == UiScreenId::EscConfig);
	tapId(UI_FIX(UI_BACK));
	checkTrue("whose BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
}

/** @brief Whether the tester's LATCH button is drawn off, and its caption reads THROTTLE. */
static bool latchOffShown() {
	const UiRect r = ctlRect(UI_FIX(TesterCtl::Latch));
	return pixel(r.x + 4, r.y + 4) == C_PANEL && fakeTextIs(8, 236, "THROTTLE", C_DIM, C_BG, 1);
}

/**
 * @brief Slice 3 of docs/design/menu-c.md: MENU is the root. After
 *        uiInit() the screen is MENU, alone on the stack, with no BACK in
 *        its table or on the panel; a tap on row 0 opens the tester over
 *        it; the tester's MENU button disarms and returns; the tester opens
 *        disarmed, at throttle 0, with LATCH off, every time, and the log
 *        hears the disarm while MENU shows.
 */
static void testMenuRoot() {
	section("MENU: the root, no BACK; ESC TESTER opens the tester, whose MENU returns");
	fakeFlashClear(); settingsLoad();
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	uiInit();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	menuFrames(2);
	checkTrue("after uiInit(): MENU, alone on the stack",
	          uiCurrentScreen() == UiScreenId::Menu && uiNavDepth() == 1 &&
	          uiNavAt(0) == UiScreenId::Menu);
	checkTrue("MENU's control table has no BACK", !ctlPresent(UI_FIX(UI_BACK)));
	checkTrue("the header reads MENU and draws no BACK: the band is the panel fill",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "MENU", C_TEXT, C_PANEL, 2) &&
	          allOf(uiBackRect().x, uiBackRect().y, uiBackRect().w, uiBackRect().h, C_PANEL));
	checkControlTable("MENU, the root");
	{
		const UiRect b = uiBackRect();
		fakePress(b.x + b.w / 2, b.y + b.h / 2); menuFrames(2);
		fakeRelease(); menuFrames(1);
		checkTrue("a tap where BACK stands elsewhere leaves nothing: MENU stays",
		          uiCurrentScreen() == UiScreenId::Menu && uiNavDepth() == 1);
	}
	checkTrue("ESC TESTER, row 0, acts while the claim is Free", ctlEnabled(UI_ROW(0, Row)));
	tapId(UI_ROW(0, Row));
	checkTrue("a tap on row 0 opens the tester, over MENU",
	          uiCurrentScreen() == UiScreenId::Tester && uiNavDepth() == 2 &&
	          uiNavAt(0) == UiScreenId::Menu && uiNavAt(1) == UiScreenId::Tester);
	menuFrames(1);
	checkTrue("it opens disarmed, at throttle 0, LATCH off",
	          !uiArmed() && !fakeArmed() && uiThrottle() == 0 && latchOffShown());
	// The arm gate: 1500 ms after core1's bind (docs/design/dshot-lifetime.md).
	for (int k = 0; k < 100 && uiArmGated(escTaskBoundMs(), millis()); k++) menuFrames(1);

	// Armed, LATCH on, a throttle standing: the MENU button clears all three.
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); menuFrames(2);
	checkTrue("armed", uiArmed() && fakeArmed());
	checkTrue("an armed tester drives an auto log", sdLogActive());
	tapId(UI_FIX(TesterCtl::Latch));
	{
		const UiRect t = ctlRect(UI_FIX(TesterCtl::Throttle));
		const int y = t.y + t.h / 2;
		fakePress(t.x + 10, y); menuFrames(1);
		for (int k = 1; k <= 6; k++) { fakeHold(t.x + 10 + 8 * k, y); menuFrames(1); }
		fakeRelease(); menuFrames(1);
	}
	checkTrue("LATCH holds a throttle after the lift", uiThrottle() > 0 && fakeThrottle() > 0);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("the MENU button returns to MENU, alone on the stack",
	          uiCurrentScreen() == UiScreenId::Menu && uiNavDepth() == 1);
	checkTrue("disarmed, the throttle 0 and sent as 0",
	          !uiArmed() && !fakeArmed() && uiThrottle() == 0 && fakeThrottle() == 0);
	menuFrames(1);
	checkTrue("the log hears the disarm on MENU's first frame", !sdLogActive());
	menuFrames(40);
	checkTrue("MENU keeps the arm off and sends no throttle",
	          !uiArmed() && !fakeArmed() && fakeThrottle() == 0);
	tapId(menuRow(&UI_TESTER));
	menuFrames(1);
	checkTrue("opened again: disarmed, at throttle 0, LATCH off",
	          uiCurrentScreen() == UiScreenId::Tester && !uiArmed() && !fakeArmed() &&
	          uiThrottle() == 0 && latchOffShown());
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
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
		openTester();
		menuFrames(2);
	}
	openMenu();
	tapId(toolEntry(&UI_SERVO));
	checkTrue("SERVO TESTER opens", uiCurrentScreen() == UiScreenId::Servo);
	menuFrames(4);
	checkTrue("its claim runs", pinClaimState() == ClaimState::Running);
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 200 && !ctlEnabled(UI_FIX(ServoCtl::Release)); k++) menuFrames(1);
	pressId(UI_FIX(ServoCtl::Release));
	fakeRelease();
	// The release frame leaves for SERVO, the hub. Without the pump's pass
	// the claim stays Parking, the state the leave page's SERVO leaves it in.
	framesNoPump(2);
	bool busySeen = uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() != ClaimState::Free;
	checkTrue("SERVO, the last button, returns to SERVO, the hub, while the claim parks", busySeen);
	if (busySeen) {
		char want[40];
		snprintf(want, sizeof(want), "SETTING SIG GP%u LOW", (unsigned)settings()->dshotPin);
		UiControl c;
		checkTrue("the hub's strip says why on its second line: SETTING SIG GP0 LOW, amber, "
		          "in the NO ESC CHECK line's place",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1) &&
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, UI_HUB_NO_ESC_CHECK, C_AMBER, C_BG, 1) &&
		          gfxTextW(want, 1) == 113);
		checkTrue("SERVO TESTER, which claims the pins, is drawn disabled and does not act",
		          ctlFind(HUB_ENTRY(0), &c) && !c.enabled && pixel(c.rect.x + 4, c.rect.y + c.rect.h / 2) == C_BG);
		checkTrue("nor ANGLE METER, which opens only while the pins are free", !ctlEnabled(HUB_ENTRY(1)));
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(1);
		checkTrue("a tap on SERVO TESTER while busy opens nothing",
		          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() == ClaimState::Parking);
		// BACK to MENU, with no pump pass, so the claim stays Parking.
		const UiRect b = uiBackRect();
		fakePress(b.x + b.w / 2, b.y + b.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(2);
		checkTrue("BACK: MENU, the claim still parking",
		          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Parking);
		// The six rows are whole at scroll 0: no drag reaches a row.
		const UiControlId testerRow = UI_ROW(uiMenuRowOf(&UI_TESTER), Row);
		checkTrue("ESC TESTER, whose DShot needs the pin, does not act",
		          ctlRect(testerRow).h == 40 && !ctlEnabled(testerRow));
		{
			const UiRect tr = ctlRect(testerRow);
			checkTrue("and is drawn disabled: an outline, its label dim, no status",
			          pixel(tr.x + 4, tr.y + tr.h / 2) == C_BG &&
			          fakeTextIs(UI_MENU_LABEL_X, tr.y + 13, UI_TESTER_LABEL, C_DIM, C_BG, 2) &&
			          allOf(tr.x + 160, tr.y + 4, tr.w - 164, tr.h - 8, C_BG));
		}
		checkTrue("MENU's strip says why: SETTING SIG GP0 LOW, amber",
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_AMBER, C_BG, 1));
		checkTrue("ESC PROGRAM acts", ctlEnabled(menuRow(&UI_ESC_CONFIG)));
		checkTrue("SERVO acts, though its entries claim", ctlEnabled(menuRow(&UI_SERVO_HUB_SCREEN)));
		const UiRect rx = ctlRect(menuRow(&UI_RX_SIGNALS));
		checkTrue("RECEIVER acts, filled, with its mark in C_CYAN and no status",
		          ctlEnabled(menuRow(&UI_RX_SIGNALS)) && pixel(rx.x + 4, rx.y + rx.h / 2) == C_PANEL &&
		          allOf(rx.x + 160, rx.y + 4, UI_MENU_MARK_X - (rx.x + 160), rx.h - 8, C_PANEL) &&
		          fakeTextIs(UI_MENU_MARK_X, rx.y + 13, UI_MENU_MARK, C_CYAN, C_PANEL, 2));
		const UiControlId logRow = UI_ROW(uiMenuRowOf(&UI_SDLOG), Row);
		checkInt("LOGS' row is whole", ctlRect(logRow).h, 40);
		checkTrue("LOGS, which claims nothing, acts", ctlEnabled(logRow));
		const UiControlId settingsRow = UI_ROW(uiMenuRowOf(&UI_SETTINGS), Row);
		checkInt("SETUP's row is whole", ctlRect(settingsRow).h, 40);
		checkTrue("nor SETUP, which changes the wiring the claim latched",
		          !ctlEnabled(settingsRow));
		fakeDumpFrame("shot_menu_parking.ppm");
		ctlFind(settingsRow, &c);
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(1);
		checkTrue("a tap on SETUP while the claim restores opens nothing",
		          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Parking);
		ctlFind(testerRow, &c);
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(1);
		checkTrue("nor does a tap on ESC TESTER while the claim restores",
		          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Parking);
	}
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	if (uiCurrentScreen() != UiScreenId::Menu) tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	checkTrue("once Free, ESC TESTER acts again", ctlEnabled(menuRow(&UI_TESTER)));
	checkTrue("and SETUP", ctlEnabled(menuRow(&UI_SETTINGS)));
	openTester();
}

/** @brief Six rows fit; seven scroll 36 px; the scroll bar shows only then. */
static void testMenuScroll() {
	section("MENU: six rows fit, seven scroll 36 px");
	static const UiMenuEntry six[] = {
		{ "ESC COMMANDS", 0, &UI_ESC_CMD }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "LOGS", 10, &UI_SDLOG },
		{ "SETUP", 9, &UI_SETTINGS }, { "LOGS", 10, &UI_SDLOG },
	};
	static const UiMenuEntry seven[] = {
		{ "ESC COMMANDS", 0, &UI_ESC_CMD }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "SERVO PROGRAMMER", 0, &UI_SERVO_HUB_SCREEN },
		{ "RECEIVER", 9, &UI_RX_SIGNALS }, { "SETUP", 9, &UI_SETTINGS },
		{ "LOGS", 10, &UI_SDLOG },
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

	// MENU has no BACK: a tool's return opens it again, with the registry
	// the test asks for.
	uiMenuUseRegistry(seven, 7);
	tapId(UI_ROW(3, Row));
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
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
	uiMenuUseRegistry(nullptr, 0);
	tapId(UI_ROW(6, Row));
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	openTester();
}

/**
 * @brief The built registry (docs/design/menu-c.md owner decision 4, slice
 *        4): six rows, ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS,
 *        SETUP. They take 252 px of the 258 px viewport: the scroll
 *        range is 0, every row is whole and acts at scroll 0, the last
 *        ends at y 311, and the three rows that open a hub carry `>`.
 */
static void testSettingsRowVisible() {
	section("MENU: six rows, no scroll; > on the three hub rows");
	freshTester();
	uiMenuUseRegistry(nullptr, 0);
	openMenu();
	uint8_t n;
	const UiMenuEntry *e = uiMenuEntries(&n);
	static const char *const ORDER[] = {
		"ESC TESTER", "ESC PROGRAM", "SERVO", "RECEIVER", "LOGS", "SETUP",
	};
	checkInt("UI_MENU_COUNT is 6", UI_MENU_COUNT, 6);
	bool order = n == 6;
	for (uint8_t i = 0; order && i < n; i++) order = strcmp(e[i].label, ORDER[i]) == 0;
	checkTrue("ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS, SETUP, in that order", order);
	checkTrue("the rows' screens", uiMenuRowOf(&UI_TESTER) == 0 && uiMenuRowOf(&UI_ESC_CONFIG) == 1 &&
	          uiMenuRowOf(&UI_SERVO_HUB_SCREEN) == 2 && uiMenuRowOf(&UI_RX_SIGNALS) == 3 &&
	          uiMenuRowOf(&UI_SDLOG) == 4 && uiMenuRowOf(&UI_SETTINGS) == 5);
	checkTrue("no row for ESC COMMANDS, ESC SETTINGS or USB BRIDGE: ESC PROGRAM opens them",
	          uiMenuRowOf(&UI_ESC_CMD) < 0 && uiMenuRowOf(&UI_AM32) < 0 && uiMenuRowOf(&UI_BRIDGE) < 0);
	checkTrue("no row for SERVO TESTER, ANGLE METER, KST SERVO or HITEC SERVO: SERVO opens them",
	          uiMenuRowOf(&UI_SERVO) < 0 && uiMenuRowOf(&UI_ANGLE) < 0 && uiMenuRowOf(&UI_KST) < 0 &&
	          uiMenuRowOf(&UI_HITEC) < 0);
	checkTrue("the rows of the tester and the hubs have no status (decision 18)",
	          !UI_TESTER.status && !UI_ESC_CONFIG.status && !UI_SERVO_HUB_SCREEN.status &&
	          !UI_RX_SIGNALS.status && e[0].statusWidest == 0 && e[1].statusWidest == 0 &&
	          e[2].statusWidest == 0 && e[3].statusWidest == 0);
	checkTrue("three rows open a hub: ESC PROGRAM, SERVO, RECEIVER",
	          !uiHubOf(e[0].screen) && uiHubOf(e[1].screen) == &UI_ESC_CFG_HUB &&
	          uiHubOf(e[2].screen) == &UI_SERVO_HUB && uiHubOf(e[3].screen) == &UI_RX_HUB &&
	          !uiHubOf(e[4].screen) && !uiHubOf(e[5].screen));

	// No scroll: six rows at a 42 px pitch from y 62, each whole and acting.
	checkTrue("no scroll bar is drawn", allOf(235, 62, 5, 258, C_BG));
	for (int i = 0; i < 6; i++) {
		char what[96];
		UiControl c = {};
		snprintf(what, sizeof(what), "%s: row %d, whole at y %d, 224 x 40 px from x 8, acting", ORDER[i], i,
		         62 + 42 * i);
		checkTrue(what, ctlFind(UI_ROW(i, Row), &c) && c.rect.x == 8 && c.rect.y == 62 + 42 * i &&
		                c.rect.w == 224 && c.rect.h == 40 && c.enabled);
		const int w = gfxTextW(ORDER[i], 2);
		snprintf(what, sizeof(what), "%s: its label at scale 2 from x 16, %d px, ends at x %d", ORDER[i], w,
		         16 + w);
		checkTrue(what, fakeTextIs(UI_MENU_LABEL_X, c.rect.y + 13, ORDER[i], C_TEXT, C_PANEL, 2));
		const bool hub = uiHubOf(e[i].screen) != nullptr;
		if (hub) {
			// The mark: > at scale 2, 10 px, from x 213, its last column
			// x 222, in C_CYAN; the label ends 8 px or more before it.
			snprintf(what, sizeof(what), "%s: > at scale 2 from x 213 in C_CYAN, the row blank up to it",
			         ORDER[i]);
			checkTrue(what, fakeTextIs(UI_MENU_MARK_X, c.rect.y + 13, ">", C_CYAN, C_PANEL, 2) &&
			                allOf(16 + w + 1, c.rect.y + 2, UI_MENU_MARK_X - (16 + w + 1), 36, C_PANEL) &&
			                allOf(UI_MENU_MARK_X + 10, c.rect.y + 2, 228 - (UI_MENU_MARK_X + 10), 36, C_PANEL) &&
			                16 + w + UI_MENU_LABEL_GAP <= UI_MENU_MARK_X);
		} else if (i == 0) {
			snprintf(what, sizeof(what), "%s: no status and no mark", ORDER[i]);
			checkTrue(what, allOf(16 + w + 1, c.rect.y + 2, 228 - (16 + w + 1), 36, C_PANEL));
		} else {
			snprintf(what, sizeof(what), "%s: a status row, no mark at x 213", ORDER[i]);
			checkTrue(what, !fakeTextIs(UI_MENU_MARK_X, c.rect.y + 13, ">", C_CYAN, C_PANEL, 2));
		}
	}
	checkTrue("the mark is 10 px wide and its last column is x 222",
	          gfxTextW(UI_MENU_MARK, 2) == 10 && UI_MENU_MARK_X == 213 && UI_MENU_MARK_X + 10 - 1 == 222);
	checkTrue("the widths of the note: ESC TESTER 118, ESC PROGRAM 130, SERVO 58, RECEIVER 94, "
	          "LOGS 46, SETUP 58 px",
	          gfxTextW("ESC TESTER", 2) == 118 && gfxTextW("ESC PROGRAM", 2) == 130 &&
	          gfxTextW("SERVO", 2) == 58 && gfxTextW("RECEIVER", 2) == 94 && gfxTextW("LOGS", 2) == 46 &&
	          gfxTextW("SETUP", 2) == 58);
	checkTrue("the widest statuses clear their labels by 8 px or more: CARD ERROR from x 163, UNSAVED from x 181",
	          UI_MENU_STATUS_R - gfxTextW("CARD ERROR", 1) == 163 && 16 + 46 + 8 <= 163 &&
	          UI_MENU_STATUS_R - gfxTextW("UNSAVED", 1) == 181 && 16 + 58 + 8 <= 181);
	checkInt("the last row ends at y 311", ctlRect(UI_ROW(5, Row)).y + ctlRect(UI_ROW(5, Row)).h - 1, 311);
	checkTrue("8 px stay free under it", allOf(0, 312, GFX_W, 8, C_BG));
	checkTrue("no seventh row", !ctlPresent(UI_ROW(6, Row)));
	checkControlTable("MENU, six rows");
	{
		// A drag moves nothing: uiListMaxScroll() of MENU is 0.
		const uint32_t rest = fakeRegionHash(0, 0, GFX_W, GFX_H);
		fakePress(120, 250); menuFrames(1);
		for (int k = 1; k <= 21; k++) { fakeHold(120, 250 - 6 * k); menuFrames(1); }
		fakeRelease(); menuFrames(1);
		checkTrue("a drag of 126 px up scrolls nothing and opens nothing",
		          uiCurrentScreen() == UiScreenId::Menu && ctlRect(UI_ROW(0, Row)).y == 62 &&
		          ctlRect(UI_ROW(5, Row)).y == 272 && fakeRegionHash(0, 0, GFX_W, GFX_H) == rest);
		fakePress(120, 100); menuFrames(1);
		for (int k = 1; k <= 21; k++) { fakeHold(120, 100 + 6 * k); menuFrames(1); }
		fakeRelease(); menuFrames(1);
		checkTrue("nor a drag down", ctlRect(UI_ROW(0, Row)).y == 62 &&
		          fakeRegionHash(0, 0, GFX_W, GFX_H) == rest);
	}
	// Every row acts at scroll 0: one tap opens its screen, with no drag.
	static const UiScreenId OPENS[] = {
		UiScreenId::Tester, UiScreenId::EscConfig, UiScreenId::ServoHub, UiScreenId::RxSignals,
		UiScreenId::SdLog, UiScreenId::Settings,
	};
	for (int i = 0; i < 6; i++) {
		char what[96];
		UiControl c = {};
		ctlFind(UI_ROW(i, Row), &c);
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2); menuFrames(2);
		fakeRelease(); menuFrames(1);
		snprintf(what, sizeof(what), "%s: one tap at scroll 0 opens its screen, over MENU", ORDER[i]);
		checkTrue(what, uiCurrentScreen() == OPENS[i] && uiNavDepth() == 2);
		menuFrames(1);
		tapId(i == 0 ? UI_FIX(TesterCtl::Menu) : UI_FIX(UI_BACK));
		menuFrames(1);
		snprintf(what, sizeof(what), "%s: and returns to MENU", ORDER[i]);
		checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
	}
	openTester();
}

/**
 * @brief RECEIVER (rx-signals.md, decision 4): the row acts whatever the
 *        claim's state and opens its chooser, which claims nothing, with no
 *        status (menu-b.md, decision 18). While the claim is not Free the
 *        chooser draws its two entries disabled (menu-c.md, slice 2).
 */
static void testRxSignalsRow() {
	section("MENU: RECEIVER opens its chooser, which gates its entries on the claim");
	freshTester();
	openMenu();
	const UiControlId row = menuRow(&UI_RX_SIGNALS);
	const UiRect r = ctlRect(row);
	auto rxLive = [&]() {
		return ctlEnabled(row) && pixel(r.x + 4, r.y + r.h / 2) == C_PANEL &&
		       fakeTextIs(UI_MENU_LABEL_X, r.y + 13, "RECEIVER", C_TEXT, C_PANEL, 2) &&
		       allOf(r.x + 160, r.y + 4, UI_MENU_MARK_X - (r.x + 160), r.h - 8, C_PANEL) &&
		       fakeTextIs(UI_MENU_MARK_X, r.y + 13, UI_MENU_MARK, C_CYAN, C_PANEL, 2);
	};
	auto rxDisabled = [&]() {
		return !ctlEnabled(row) && pixel(r.x + 4, r.y + r.h / 2) == C_BG &&
		       fakeTextIs(UI_MENU_LABEL_X, r.y + 13, "RECEIVER", C_DIM, C_BG, 2) &&
		       allOf(r.x + 160, r.y + 4, r.w - 164, r.h - 8, C_BG);
	};
	checkTrue("claim Free: filled, the label in C_TEXT, its mark, no status", pinClaimState() == ClaimState::Free &&
	          rxLive());
	checkTrue("the row is RECEIVER's chooser, which opens whatever the claim's state",
	          !UI_RX_SIGNALS.freePins && UI_RX_SIGNALS.pinUser == PinUser::None &&
	          UI_RX_SIGNALS.id == UiScreenId::RxSignals);
	// A claim taken under MENU, as only a test can: Stopping, then Held.
	checkTrue("a claim taken", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	for (int k = 0; k < 3; k++) {
		menuFrames(1);
		pinClaimPoll();
		char what[64];
		snprintf(what, sizeof(what), "claim state %d: RECEIVER filled and acting, no status",
		         (int)pinClaimState());
		checkTrue(what, pinClaimState() != ClaimState::Free && rxLive() && !rxDisabled());
	}
	tapId(row);
	checkTrue("a tap opens RECEIVER", uiCurrentScreen() == UiScreenId::RxSignals);
	menuFrames(1);
	checkTrue("whose MONITOR and OUTPUT are drawn disabled and gated",
	          !ctlEnabled(UI_FIX(RxCtl::Monitor)) && !ctlEnabled(UI_FIX(RxCtl::Output)));
	tapId(UI_FIX(RxCtl::Monitor), IdExpect::Disabled);
	checkTrue("a tap on MONITOR opens nothing and claims nothing",
	          uiCurrentScreen() == UiScreenId::RxSignals && pinClaimHolder() == PinUser::Am32);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	pinClaimEnd(PinUser::Am32);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	menuFrames(1);
	checkTrue("claim Free again: it acts", rxLive());
	tapId(row);
	checkTrue("a tap opens RECEIVER", uiCurrentScreen() == UiScreenId::RxSignals);
	checkTrue("which claims nothing: DShot parked", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended() && !fakePinReturned());
	tapId(UI_FIX(UI_BACK));
	checkTrue("its BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
}

/** @brief Whether the 8 px line at @p y reads @p text from x 8 in @p col, alone on its rows. */
static bool lineIs(int y, const char *text, uint16_t col) {
	uint16_t *fb = gfxBuffer();
	static uint16_t keep[8][GFX_W];
	memcpy(keep, fb + y * GFX_W, sizeof(keep));
	uint32_t shown = fakeRegionHash(0, y, GFX_W, 8);
	gfxRect(0, y, GFX_W, 8, C_BG);
	gfxText(UI_MARGIN, y, text, col, 1);
	bool same = fakeRegionHash(0, y, GFX_W, 8) == shown;
	memcpy(fb + y * GFX_W, keep, sizeof(keep));
	return same;
}

/** @brief Whether the hub's line at @p y reads @p text from x 8 in @p col; the scroll bar may share its rows. */
static bool hubLineIs(int y, const char *text, uint16_t col) {
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, 1);
}

/**
 * @brief SERVO, the hub, and its three programmer entries
 *        (docs/design/menu-b.md, decisions 2 and 34; docs/design/menu-c.md,
 *        "The hub"): it claims nothing; KST / CHASERVO, HITEC D SERIES and
 *        HITEC CLASSIC are its entries 3 to 5, with their lines with SERVO
 *        SUPPLY at OWN and at GP28 and GP29, CLASSIC disabled without the
 *        switch; BACK to MENU.
 */
static void testServoProg() {
	section("SERVO: the hub, and its programmer entries");
	freshTester();
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
	openMenu();
	tapId(menuRow(&UI_SERVO_HUB_SCREEN));
	menuFrames(1);
	checkTrue("SERVO opens SERVO, the hub", uiCurrentScreen() == UiScreenId::ServoHub);
	checkTrue("it claims nothing: DShot parked", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended() && !fakePinReturned());
	menuFrames(20);
	checkTrue("half a second on, still nothing claimed", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended());
	checkTrue("the title reads SERVO",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_SERVO_HUB_TITLE, C_TEXT, C_PANEL, 2));
	checkTrue("5 characters, 58 px, ending 4 px or more before BACK",
	          gfxTextW(UI_SERVO_HUB_TITLE, 2) == 58 && UI_MARGIN + 58 + 4 <= UI_BACK_X);
	{
		char own[48];
		snprintf(own, sizeof(own), "SIG GP%u  AUX GP%u  SERVO SUPPLY OWN", (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin);
		checkTrue("the strip: SIG GP0  AUX GP1  SERVO SUPPLY OWN, dim",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, own, C_DIM, C_BG, 1));
		checkTrue("its second line: NO ESC CHECK: UNPLUG ANY ESC FIRST., amber, 35 characters, 209 px",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, "NO ESC CHECK: UNPLUG ANY ESC FIRST.", C_AMBER,
		                     C_BG, 1) && gfxTextW(UI_HUB_NO_ESC_CHECK, 1) == 209);
	}
	checkInt("five entries", UI_SERVO_ENTRIES_COUNT, 5);
	const int kst = progEntryOf(uiKstEnter), d = progEntryOf(uiHitecEnterD),
	          cl = progEntryOf(uiHitecEnterClassic);
	checkTrue("SERVO TESTER, ANGLE METER, then KST / CHASERVO, HITEC D SERIES, HITEC CLASSIC",
	          UI_SERVO_ENTRIES[0].screen == &UI_SERVO && UI_SERVO_ENTRIES[1].screen == &UI_ANGLE && kst == 2 &&
	          d == 3 && cl == 4);
	// The programmers at the hub's full scroll, 146 px: entry i at y
	// 62 + 80 x i - 146.
	hubScrollEnd();
	checkInt("dragged to its end: 146 px", uiHubScroll(), 146);
	static const char *const LABEL[] = { "KST / CHASERVO", "HITEC D SERIES", "HITEC CLASSIC" };
	for (int i = 0; i < 3; i++) {
		const UiRect r = ctlRect(HUB_ENTRY(kst + i));
		const int top = 62 + 80 * (kst + i) - 146;
		char what[80];
		snprintf(what, sizeof(what), "%s: 228 x 46 px at (6, %d)", LABEL[i], top);
		checkTrue(what, r.x == 6 && r.y == top && r.w == 228 && r.h == 46);
		snprintf(what, sizeof(what), "%s: its label centred at scale 2", LABEL[i]);
		const bool live = kst + i != cl;
		checkTrue(what, strcmp(UI_SERVO_ENTRIES[kst + i].label, LABEL[i]) == 0 &&
		                fakeTextIs(r.x + (r.w - gfxTextW(LABEL[i], 2)) / 2, r.y + (r.h - 14) / 2, LABEL[i],
		                           live ? C_TEXT : C_GRID, live ? C_PANEL : C_BG, 2));
		snprintf(what, sizeof(what), "%s %s", LABEL[i], live ? "acts" : "is drawn disabled with OWN");
		checkTrue(what, ctlEnabled(HUB_ENTRY(kst + i)) == live);
	}
	checkTrue("KST: POWER THE SERVO UP AFTER THE TAP. at y 126, dim",
	          hubLineIs(126, "POWER THE SERVO UP AFTER THE TAP.", C_DIM));
	checkTrue("D SERIES: its models at y 206, dim", hubLineIs(206, "D485HW, D645MW AND THE OTHERS", C_DIM));
	checkTrue("then POWER THE SERVO UP AFTER THE TAP. at y 218 (decision 34)",
	          hubLineIs(218, "POWER THE SERVO UP AFTER THE TAP.", C_DIM));
	checkTrue("CLASSIC: its models at y 286, dim", hubLineIs(286, "HS-5XXX, HS-7XXX", C_DIM));
	checkTrue("then NEEDS THE CONVERTER OF WIRING PART C at y 298, amber",
	          hubLineIs(298, "NEEDS THE CONVERTER OF WIRING PART C", C_AMBER));
	checkTrue("and AND SETUP > SERVO SUPPLY GP28/29. at y 310, amber",
	          hubLineIs(310, "AND SETUP > SERVO SUPPLY GP28/29.", C_AMBER));
	checkTrue("33, 29, 16, 36 and 33 characters: the widest 215 px, to x 223",
	          gfxTextW("POWER THE SERVO UP AFTER THE TAP.", 1) == 197 &&
	          gfxTextW("NEEDS THE CONVERTER OF WIRING PART C", 1) == 215 &&
	          gfxTextW("AND SETUP > SERVO SUPPLY GP28/29.", 1) == 197);
	{
		int x0 = 0, x1 = 0;
		checkTrue("nothing under the strip reaches past the buttons' left edge or the scroll bar at x 235 to 237",
		          fakeInkExtent(UI_BODY_Y, GFX_H - UI_BODY_Y, C_BG, &x0, &x1) && x0 >= 6 && x1 == 237);
		checkTrue("the last line ends at y 316, above the panel's end",
		          !allOf(0, 316, 235, 1, C_BG) && allOf(0, 317, 235, 3, C_BG));
	}
	checkControlTable("SERVO, OWN, at its end");
	tapId(HUB_ENTRY(cl), IdExpect::Disabled);
	checkTrue("a tap on CLASSIC opens nothing and claims nothing",
	          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() == ClaimState::Free);
	{
		const UiRect r = ctlRect(HUB_ENTRY(cl));
		fakePress(r.x + r.w / 2, r.y + r.h / 2);
		menuFrames(1);
		checkTrue("a press on CLASSIC draws no pressed look",
		          fakeTextIs(r.x + (r.w - gfxTextW("HITEC CLASSIC", 2)) / 2, r.y + 16, "HITEC CLASSIC",
		                     C_GRID, C_BG, 2));
		fakeRelease();
		menuFrames(1);
	}
	{
		const UiRect r = ctlRect(HUB_ENTRY(kst));
		const uint32_t rest = fakeRegionHash(r.x, r.y, r.w, r.h);
		fakePress(r.x + r.w / 2, r.y + r.h / 2);
		menuFrames(1);
		checkTrue("a press on KST / CHASERVO draws it pressed, and opens nothing yet",
		          fakeRegionHash(r.x, r.y, r.w, r.h) != rest && uiCurrentScreen() == UiScreenId::ServoHub &&
		          pinClaimState() == ClaimState::Free);
		fakeHold(r.x + r.w / 2, 30);
		menuFrames(1);
		fakeRelease();
		menuFrames(1);
		checkTrue("slid off before the lift: nothing opens, nothing claimed",
		          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() == ClaimState::Free);
	}
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Free);

	for (uint8_t mode = 1; mode <= 2; mode++) {
		settings()->servoSupply = mode;
		servoSupplyConfigure(mode);
		const unsigned gp = servoSupplyPinOf(mode);
		tapId(menuRow(&UI_SERVO_HUB_SCREEN));
		menuFrames(1);
		char want[48], what[96];
		snprintf(want, sizeof(want), "SIG GP%u  AUX GP%u  SERVO SUPPLY GP%u", (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin, gp);
		snprintf(what, sizeof(what), "GP%u: the strip %s, 209 px", gp, want);
		checkTrue(what, fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, want, C_DIM, C_BG, 1) &&
		                gfxTextW(want, 1) == 209);
		hubScrollEnd();
		snprintf(want, sizeof(want), "THE BENCH SWITCHES IT ON GP%u.", gp);
		snprintf(what, sizeof(what), "GP%u: %s at y 126, 218 and 298, dim", gp, want);
		checkTrue(what, hubLineIs(126, want, C_DIM) && hubLineIs(218, want, C_DIM) && hubLineIs(298, want, C_DIM));
		snprintf(what, sizeof(what), "GP%u: CLASSIC acts, its amber lines gone", gp);
		checkTrue(what, ctlEnabled(HUB_ENTRY(cl)) && allOf(0, 306, 235, 14, C_BG) &&
		                colourBelow(C_AMBER, UI_BODY_Y) == 0);
		snprintf(what, sizeof(what), "SERVO, GP%u", gp);
		checkControlTable(what);
		if (mode == 1) fakeDumpFrame("shot_servo_hub_switched.ppm");
		tapId(UI_FIX(UI_BACK));
	}
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
	openTester();
}

/** @brief Opens SETUP from a fresh tester. */
static void openSettings() {
	freshTester();
	openMenu();
	tapId(menuRow(&UI_SETTINGS));
}

/** @brief Drag the SETUP list from (120, @p y0) by @p dy, in 4 px frames. */
static void dragList(int y0, int dy) {
	fakePress(120, y0); menuFrames(1);
	int steps = (dy < 0 ? -dy : dy) / 4;
	for (int k = 1; k <= steps; k++) { fakeHold(120, y0 + (dy < 0 ? -4 : 4) * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
}

/**
 * @brief ESC PROGRAM (menu-c.md "The hub"; bluejay-blheli-s.md decision 1 and
 *        call B, usb-bridge.md decision 1): MENU's second row opens the hub,
 *        which claims nothing; ESC COMMANDS, its first entry, opens the
 *        DShot commands, which claim nothing; ESC SETTINGS opens the
 *        settings screen, which claims SIG on the tap; USB BRIDGE opens the
 *        bridge, which claims SIG on the tap; the entries are drawn
 *        disabled while the claim is not Free.
 */
static void testEscConfig() {
	section("ESC PROGRAM: the hub, ESC COMMANDS, ESC SETTINGS and USB BRIDGE");
	freshTester();
	openMenu();
	const UiControlId row = menuRow(&UI_ESC_CONFIG);
	{
		const UiRect r = ctlRect(row);
		checkTrue("MENU's second row reads ESC PROGRAM, 130 px at scale 2, its mark and no status",
		          uiMenuRowOf(&UI_ESC_CONFIG) == 1 && gfxTextW(UI_ESC_CFG_TITLE, 2) == 130 &&
		          fakeTextIs(UI_MARGIN + 8, r.y + 13, UI_ESC_CFG_TITLE, C_TEXT, C_PANEL, 2) &&
		          allOf(r.x + 160, r.y + 4, UI_MENU_MARK_X - (r.x + 160), r.h - 8, C_PANEL) &&
		          fakeTextIs(UI_MENU_MARK_X, r.y + 13, UI_MENU_MARK, C_CYAN, C_PANEL, 2));
		checkTrue("the chooser opens whatever the claim's state and claims nothing",
		          !UI_ESC_CONFIG.freePins && UI_ESC_CONFIG.pinUser == PinUser::None &&
		          UI_ESC_CONFIG.id == UiScreenId::EscConfig && !UI_ESC_CONFIG.status);
		checkTrue("a claim taken", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
		menuFrames(1);
		checkTrue("while the claim is not Free: drawn live, acting",
		          ctlEnabled(row) && pixel(r.x + 4, r.y + r.h / 2) == C_PANEL &&
		          fakeTextIs(UI_MARGIN + 8, r.y + 13, UI_ESC_CFG_TITLE, C_TEXT, C_PANEL, 2));
		tapId(row);
		checkTrue("a tap opens ESC PROGRAM", uiCurrentScreen() == UiScreenId::EscConfig);
		menuFrames(1);
		checkTrue("whose ESC COMMANDS, ESC SETTINGS and USB BRIDGE are drawn disabled and gated",
		          ctlPresent(UI_FIX(EscCfgCtl::Commands)) && !ctlEnabled(UI_FIX(EscCfgCtl::Commands)) &&
		          !ctlEnabled(UI_FIX(EscCfgCtl::Settings)) && !ctlEnabled(UI_FIX(EscCfgCtl::Bridge)));
		tapId(UI_FIX(UI_BACK));
		checkTrue("BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
		pinClaimEnd(PinUser::Servo);
		for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) {
			menuFrames(1);
			pinClaimRelease(PinUser::Servo);
		}
		menuFrames(1);
		checkTrue("Free again: it acts", ctlEnabled(row) && pixel(r.x + 4, r.y + r.h / 2) == C_PANEL);
	}
	tapId(menuRow(&UI_ESC_CONFIG));
	menuFrames(1);
	checkTrue("ESC PROGRAM opens the chooser", uiCurrentScreen() == UiScreenId::EscConfig);
	checkTrue("it claims nothing: DShot parked", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended() && !fakePinReturned());
	menuFrames(20);
	checkTrue("half a second on, still nothing claimed", pinClaimState() == ClaimState::Free);
	checkTrue("the title reads ESC PROGRAM, ending 4 px or more before BACK",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_ESC_CFG_TITLE, C_TEXT, C_PANEL, 2) &&
	          UI_MARGIN + gfxTextW(UI_ESC_CFG_TITLE, 2) + 4 <= UI_BACK_X);
	{
		char strip[16];
		snprintf(strip, sizeof(strip), "SIG GP%u", (unsigned)settings()->dshotPin);
		checkTrue("the strip: SIG GP0, dim", lineIs(UI_STRIP_Y + 4, strip, C_DIM));
	}
	checkInt("three entries", UI_ESC_CFG_ENTRIES_COUNT, 3);
	checkInt("which fill the page: no scroll", uiHubMaxScroll(), 0);
	const UiRect cmd = ctlRect(UI_FIX(EscCfgCtl::Commands));
	const UiRect set = ctlRect(UI_FIX(EscCfgCtl::Settings)), br = ctlRect(UI_FIX(EscCfgCtl::Bridge));
	checkTrue("ESC COMMANDS: 228 x 46 px at (6, 62)", cmd.x == 6 && cmd.y == 62 && cmd.w == 228 && cmd.h == 46);
	checkTrue("ESC SETTINGS: 228 x 46 px at (6, 142)", set.x == 6 && set.y == 142 && set.w == 228 && set.h == 46);
	checkTrue("USB BRIDGE: 228 x 46 px at (6, 222)", br.x == 6 && br.y == 222 && br.w == 228 && br.h == 46);
	checkTrue("ESC COMMANDS acts, its label C_TEXT on the panel, centred at scale 2, 142 px",
	          ctlEnabled(UI_FIX(EscCfgCtl::Commands)) && gfxTextW("ESC COMMANDS", 2) == 142 &&
	          fakeTextIs(cmd.x + (cmd.w - 142) / 2, cmd.y + 16, "ESC COMMANDS", C_TEXT, C_PANEL, 2));
	checkTrue("its lines at y 112 and 124, dim: 32 characters, 191 px, and 23, 137 px",
	          lineIs(112, "BEEP 1 TO 5, DIRECTION, 3D MODE.", C_DIM) &&
	          lineIs(124, "SENT AS DSHOT COMMANDS.", C_DIM) &&
	          gfxTextW("BEEP 1 TO 5, DIRECTION, 3D MODE.", 1) == 191 &&
	          gfxTextW("SENT AS DSHOT COMMANDS.", 1) == 137);
	checkTrue("ESC SETTINGS acts, its label C_TEXT on the panel, centred at scale 2, 142 px",
	          ctlEnabled(UI_FIX(EscCfgCtl::Settings)) && gfxTextW("ESC SETTINGS", 2) == 142 &&
	          fakeTextIs(set.x + (set.w - 142) / 2, set.y + 16, "ESC SETTINGS", C_TEXT, C_PANEL, 2));
	checkTrue("its lines at y 192 and 204, dim",
	          lineIs(192, "AM32 SETTINGS, READ AND WRITTEN.", C_DIM) &&
	          lineIs(204, "BLUEJAY, BLHELI_S: READ AND WRITTEN.", C_DIM));
	checkTrue("USB BRIDGE acts, its label C_TEXT on the panel, centred at scale 2, 118 px",
	          ctlEnabled(UI_FIX(EscCfgCtl::Bridge)) && gfxTextW("USB BRIDGE", 2) == 118 &&
	          pixel(br.x + 4, br.y + br.h - 5) == C_PANEL &&
	          fakeTextIs(br.x + (br.w - 118) / 2, br.y + 16, "USB BRIDGE", C_TEXT, C_PANEL, 2));
	checkTrue("its lines at y 272 and 284, dim, the second ending at y 290",
	          lineIs(272, "A DESKTOP ESC TOOL OVER USB.", C_DIM) &&
	          lineIs(284, "MSP, 4-WAY AND PASSTHROUGH.", C_DIM) && allOf(0, 292, GFX_W, 28, C_BG));
	checkControlTable("ESC PROGRAM");
	fakeDumpFrame("shot_esc_chooser.ppm");
	fakePress(br.x + br.w / 2, br.y + br.h / 2);
	menuFrames(1);
	checkTrue("a press on USB BRIDGE draws it pressed", pixel(br.x + 2, br.y + br.h / 2) == C_INK);
	checkTrue("and claims nothing yet", pinClaimState() == ClaimState::Free);
	fakeRelease();
	menuFrames(1);
	checkTrue("its lift opens USB BRIDGE", uiCurrentScreen() == UiScreenId::Bridge);
	checkTrue("which claims SIG on the tap's frame", pinClaimHolder() == PinUser::Bridge);
	tapId(UI_FIX(UI_BACK));
	if (uiCurrentScreen() == UiScreenId::Bridge) {
		// The bridge ran: its leave page; RESTART DSHOT once live.
		menuFrames(44);
		tapId(UI_FIX(BridgeCtl::Release));
	}
	checkTrue("USB BRIDGE's BACK, or its RESTART DSHOT, returns to the hub, its opener",
	          uiCurrentScreen() == UiScreenId::EscConfig);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	checkTrue("DShot runs again", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	checkTrue("the chooser's BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(menuRow(&UI_ESC_CONFIG));
	menuFrames(1);
	fakePress(set.x + set.w / 2, set.y + set.h / 2);
	menuFrames(1);
	checkTrue("a press on ESC SETTINGS draws it pressed", pixel(set.x + 2, set.y + set.h / 2) == C_INK);
	checkTrue("and claims nothing yet", pinClaimState() == ClaimState::Free);
	fakeRelease();
	menuFrames(1);
	checkTrue("its lift opens ESC SETTINGS", uiCurrentScreen() == UiScreenId::Am32);
	checkTrue("which claims SIG on the tap's frame", pinClaimHolder() == PinUser::Am32);
	tapId(UI_FIX(UI_BACK));
	checkTrue("ESC SETTINGS' BACK returns to the hub, its opener",
	          uiCurrentScreen() == UiScreenId::EscConfig);
	for (int k = 0; k < 4 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	checkTrue("DShot runs again", pinClaimState() == ClaimState::Free);
	menuFrames(1);
	tapId(UI_FIX(EscCfgCtl::Commands));
	menuFrames(1);
	checkTrue("ESC COMMANDS opens over the hub and claims nothing",
	          navIsTool(UiScreenId::EscConfig, UiScreenId::EscCmd) && pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	checkTrue("ESC COMMANDS' BACK returns to the hub, its opener", navIsHub(UiScreenId::EscConfig));
	tapId(UI_FIX(UI_BACK));
	openTester();
}

/** @brief SETUP's scroll: 0 px on open, 620 px at most, from a drag on a label only. */
static void testSettingsScroll() {
	section("SETUP: the scroll range, and no scroll from a control");
	openSettings();
	checkTrue("SETUP opens", uiCurrentScreen() == UiScreenId::Settings);
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
	checkInt("dragged to the end: UPTIME, the last row, at y 190, so the scroll is 670 px",
	         last.y, 190);
	checkTrue("and is whole", last.h == 44);
	checkInt("LAST RESTART above it at y 140", ctlRect(UI_ROW(SettingsRowId::LastRestart, Row)).y, 140);
	checkInt("CLOCK above the DIAGNOSTICS caption at y 70", ctlRect(UI_ROW(SettingsRowId::Clock, Row)).y, 70);
	dragList(60, 180);
	dragList(60, 180);
	dragList(60, 180);
	dragList(60, 180);
	checkInt("dragged back: MOTOR POLES at y 60 again", ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
}

/**
 * @brief DIAGNOSTICS, the last section: LAST RESTART as captured at boot and
 *        UPTIME, read-only; each reason's colour; UPTIME repaints once a
 *        second while it shows, not within the second, and not while it is
 *        scrolled out.
 */
static void testSettingsDiagnostics() {
	section("SETUP: DIAGNOSTICS, LAST RESTART and UPTIME");
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
	          fakeTextIs(UI_MARGIN, 128, "DIAGNOSTICS", C_CYAN, C_BG, 1));
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
	checkControlTable("SETUP, DIAGNOSTICS");
	fakeSetWatchdog(0, false);
	fakeSetBootType(0);
	fakeSetChipReset(DIAG_CR_HAD_POR);
	diagCaptureRestart();
	tapId(UI_FIX(UI_BACK));
	openTester();
}

/**
 * @brief BOARD's CLOCK: NOT SET in the dim colour until a set, then the kept
 *        time to the minute in the grid colour; a repaint at the set and
 *        when the minute turns, none within the minute, none while the row
 *        is scrolled out.
 */
static void testSettingsClock() {
	section("SETUP: BOARD's CLOCK");
	fakeI2cReset();
	fakeI2cAddPcf85063(PCF85063_I2C_ADDR);   // after its power-up: OS set
	senseInit();
	clockInit();
	openSettings();

	// To the end, then 60 px back: CLOCK's row, content y 700, at y 130;
	// UPTIME's, content y 820, at y 250, out of view, so its seconds do not
	// repaint the list.
	for (int i = 0; i < 4; i++) dragList(230, -180);
	dragList(60, 60);
	const int y = ctlRect(UI_ROW(SettingsRowId::Clock, Row)).y;
	checkInt("CLOCK's row at y 130", y, 130);
	checkTrue("UPTIME's row out of view", !ctlPresent(UI_ROW(SettingsRowId::Uptime, Row)));
	checkTrue("CLOCK's label", fakeTextIs(UI_MARGIN, y + 12, "CLOCK", C_DIM, C_BG, 1));
	checkTrue("its caption", fakeTextIs(UI_MARGIN, y + 25, "SET AT EACH POWER-UP", C_DIM, C_BG, 1));
	checkTrue("not set: NOT SET, dim, right-aligned to x 232",
	          fakeTextIs(232 - gfxTextW("NOT SET", 1), y + 18, "NOT SET", C_DIM, C_BG, 1));
	menuFrames(2);
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	menuFrames(81);
	checkTrue("not set: no repaint as 2 s pass", pixel(120, 100) == 0x1234);

	// A set repaints at the next frame.
	const ClockTime t = { 2026, 11, 14, 15, 42, 50 };
	checkInt("the set", (long)clockSet(&t), (long)ClockSetResult::Set);
	menuFrames(1);
	checkTrue("the set repaints", pixel(120, 100) != 0x1234);
	checkTrue("CLOCK reads 2026-11-14 15:42 in the grid colour",
	          fakeTextIs(232 - gfxTextW("2026-11-14 15:42", 1), y + 18, "2026-11-14 15:42", C_GRID,
	                     C_BG, 1));
	checkTrue("the caption stays", fakeTextIs(UI_MARGIN, y + 25, "SET AT EACH POWER-UP", C_DIM, C_BG, 1));

	// Within the minute nothing repaints; the next minute does.
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	menuFrames(200);   // 5 s: 15:42:55
	ClockReading r;
	clockNow(&r);
	checkInt("still minute 42", r.t.minute, 42);
	checkTrue("no repaint within the minute", pixel(120, 100) == 0x1234);
	for (clockNow(&r); r.t.minute == 42; clockNow(&r)) menuFrames(1);
	checkTrue("a repaint once the minute turns", pixel(120, 100) != 0x1234);
	checkTrue("CLOCK reads 15:43",
	          fakeTextIs(232 - gfxTextW("2026-11-14 15:43", 1), y + 18, "2026-11-14 15:43", C_GRID,
	                     C_BG, 1));

	// The first minute the part can hold, 2000-01-01 00:00, is not NOT SET:
	// a set to it from a clock not set repaints the row.
	fakeI2cReset();
	fakeI2cAddPcf85063(PCF85063_I2C_ADDR);
	senseInit();
	clockInit();
	menuFrames(1);
	checkTrue("not set again", fakeTextIs(232 - gfxTextW("NOT SET", 1), y + 18, "NOT SET", C_DIM, C_BG, 1));
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	const ClockTime first = { 2000, 1, 1, 0, 0, 30 };
	checkInt("a set to 2000-01-01 00:00:30", (long)clockSet(&first), (long)ClockSetResult::Set);
	menuFrames(1);
	checkTrue("repaints", pixel(120, 100) != 0x1234);
	checkTrue("CLOCK reads 2000-01-01 00:00",
	          fakeTextIs(232 - gfxTextW("2000-01-01 00:00", 1), y + 18, "2000-01-01 00:00", C_GRID,
	                     C_BG, 1));

	// Scrolled out: the minute turns and nothing repaints.
	for (int i = 0; i < 4; i++) dragList(60, 180);
	checkTrue("at the top, CLOCK's row out of view", !ctlPresent(UI_ROW(SettingsRowId::Clock, Row)));
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	menuFrames(2400);  // 60 s
	checkTrue("CLOCK out of view: no repaint as the minute turns", pixel(120, 100) == 0x1234);
	// Scrolled 480 px: VERSION's row, content y 650, at y 210, in view;
	// CLOCK's, content y 700, at y 260, out of it. The minute does not repaint.
	dragList(230, -180);
	dragList(230, -180);
	dragList(230, -120);
	checkInt("480 px: VERSION's row at y 210", ctlRect(UI_ROW(SettingsRowId::Version, Row)).y, 210);
	checkTrue("CLOCK's row out of view", !ctlPresent(UI_ROW(SettingsRowId::Clock, Row)));
	gfxBuffer()[100 * GFX_W + 120] = 0x1234;
	menuFrames(2400);  // 60 s
	checkTrue("VERSION in view, CLOCK not: no repaint as the minute turns",
	          pixel(120, 100) == 0x1234);

	tapId(UI_FIX(UI_BACK));
	openTester();
	// A later test finds the clock not set.
	fakeI2cReset();
	senseInit();
	clockInit();
}

/** @brief Hold HOLD DEFAULTS until it fires, then lift. */
static void holdReset() {
	pressId(UI_FIX(SettingsCtl::Defaults));
	holdId(UI_FIX(SettingsCtl::Defaults), 41);
	fakeRelease(); menuFrames(1);
}

/** @brief A control cut by the viewport takes a press only while 40 px show. */
static void testSettingsCutControl() {
	section("SETUP: a cut control takes a press only while 40 px show");
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
	openTester();
}

/** @brief Every row by id: each stepper both ways, each toggle, and the read-only row. */
static void testSettingsRows() {
	section("SETUP: each row by id");
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
	openTester();
}

/** @brief BACKLIGHT stops at 16, and a step that changes nothing applies nothing. */
static void testBacklightFloorAndNoOpSteps() {
	section("SETUP: BACKLIGHT's floor of 16, and steps that change nothing");
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
	openTester();
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
	section("SETUP: DEFAULTS LOADED - RESTART TO UNDO after DEFAULTS");
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
	checkTrue("SETUP again", uiCurrentScreen() == UiScreenId::Settings);
	checkTrue("SETUP again: the note still reads DEFAULTS LOADED", noteIs(NOTE, C_AMBER));

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

	// A change made while SETUP is not open ends it too.
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
	checkTrue("SETUP again: UNSAVED, not the DEFAULTS note",
	          settingsDirty() && noteIs("UNSAVED - HOLD SAVE TO KEEP", C_AMBER));
	fakeFlashSetWritable(true);
	tapId(UI_FIX(UI_BACK));
	openTester();
}

/**
 * @brief HOLD DEFAULTS: a tap loads nothing, 1000 ms on the button load the
 *        defaults, a slide off cancels, and its bar and fill show the hold.
 */
static void testResetHold() {
	section("SETUP: HOLD DEFAULTS loads the defaults after a 1 s hold");
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
	openTester();
}

/** @brief The note and HOLD TO SAVE do not move with the list, and the hold saves at any scroll. */
static void testSettingsSaveAnywhere() {
	section("SETUP: UNSAVED and HOLD TO SAVE at every scroll");
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
	checkInt("MENU: the SETUP row reads nothing once saved, no amber UNSAVED", amber, 0);
	openTester();
}

/** @brief Unsaved changes: MENU's SETUP row reads UNSAVED in amber. */
static void testMenuUnsaved() {
	section("MENU: SETUP reads UNSAVED while a change is not saved");
	openSettings();
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	tapId(UI_FIX(UI_BACK));
	menuFrames(1);
	UiRect row = ctlRect(menuRow(&UI_SETTINGS));
	int amber = 0;
	for (int y = row.y; y < row.y + row.h; y++)
		for (int x = row.x; x < row.x + row.w; x++) amber += pixel(x, y) == C_AMBER;
	checkTrue("the SETUP row shows amber text", amber > 0);
	openTester();
}

/**
 * @brief A whole block of @p version in the fake flash, with @p s as its
 *        payload: all of it for this version and any later one, its first
 *        14 bytes, the layout of versions 3 to 5, with the CRC at byte 24
 *        for an earlier one.
 */
static void putBlock(uint16_t version, const Settings *s) {
	SettingsBlock b;
	memset(&b, 0, sizeof(b));
	b.magic = SETTINGS_MAGIC;
	b.version = version;
	b.size = sizeof(Settings);
	b.s = *s;
	b.crc32 = settingsCrc32(&b, offsetof(SettingsBlock, crc32));
	fakeFlashClear();
	if (version >= SETTINGS_VERSION) {
		memcpy(fakeFlashBytes(), &b, sizeof(b));
		return;
	}
	uint8_t raw[28];
	memcpy(raw, &b, 22);
	raw[6] = 14;
	raw[7] = 0;
	raw[22] = raw[23] = 0;
	const uint32_t crc = settingsCrc32(raw, 24);
	memcpy(raw + 24, &crc, 4);
	memcpy(fakeFlashBytes(), raw, sizeof(raw));
}

/** @brief A block of this version holding @p s, as a save writes it. */
static void storeValid(const Settings *s) { putBlock(SETTINGS_VERSION, s); }

/** @brief The amber pixels on MENU's SETUP row. */
static int settingsRowAmber() {
	UiRect row = ctlRect(menuRow(&UI_SETTINGS));
	int amber = 0;
	for (int y = row.y; y < row.y + row.h; y++)
		for (int x = row.x; x < row.x + row.w; x++) amber += pixel(x, y) == C_AMBER;
	return amber;
}

/**
 * @brief Settings an update discarded: MENU's SETUP row reads RESET and
 *        SETUP's note says so until a save lands; never on a board that
 *        never had settings.
 */
static void testResetByUpdate() {
	section("SETUP: a reset by an update shows until a save lands");
	static const char *const NOTE = "RESET BY UPDATE - CHECK AND SAVE";
	checkTrue("the note fits the line", UI_MARGIN + gfxTextW(NOTE, 1) <= GFX_W - UI_MARGIN);
	char b[32];
	uint16_t col = 0, fill = 0;

	// A board that never had settings.
	freshTester();
	openMenu();
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("blank flash: MENU's SETUP row reads nothing", b[0] == '\0' && settingsRowAmber() == 0);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("and SETUP reads NOTHING SAVED: DEFAULTS IN USE", noteIs("NOTHING SAVED: DEFAULTS IN USE", C_DIM));
	tapId(UI_FIX(UI_BACK));
	openTester();

	// A version 6 block, of a layout this firmware does not know.
	Settings old;
	settingsDefaults(&old);
	old.poles = 22;
	old.maxThrottle = 1000;
	putBlock(SETTINGS_VERSION + 1, &old);
	settingsLoad();
	uiInitTester();
	menuFrames(2);
	checkTrue("a version 6 block: discarded, the defaults live",
	          !settingsStored() && settingsResetByUpdate() && settings()->poles == DEFAULT_MOTOR_POLES);
	openMenu();
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("MENU's SETUP row reads UPDATED, amber", strcmp(b, "UPDATED") == 0 && col == C_AMBER);
	checkTrue("drawn on the row", settingsRowAmber() > 0);
	fakeDumpFrame("shot_menu_updated.ppm");
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("SETUP: the note reads RESET BY UPDATE, amber", noteIs(NOTE, C_AMBER));
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
	openTester();

	// The splash, on the next boot after the update and after the save.
	putBlock(SETTINGS_VERSION + 1, &old);
	settingsLoad();
	uiDrawSplash();
	checkTrue("splash: SETTINGS RESET BY UPDATE at y 40, amber",
	          fakeTextCentredIs(40, "SETTINGS RESET BY UPDATE", C_AMBER, C_BG, 1));
	checkTrue("and CHECK MENU > SETUP at y 52, amber",
	          fakeTextCentredIs(52, "CHECK MENU > SETUP", C_AMBER, C_BG, 1));
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
	checkTrue("CHECK MENU > SETUP at y 52",
	          fakeTextCentredIs(52, "CHECK MENU > SETUP", C_AMBER, C_BG, 1));
	checkTrue("TOUCH CONTROLLER NOT FOUND at y 64",
	          fakeTextCentredIs(64, UI_SPLASH_TOUCH_TEXT, C_AMBER, C_BG, 1));
	checkTrue("a clear row under each line, 8 rows for high contrast",
	          !fakeInkExtent(48, 4, C_BG, &x0, &x1) && !fakeInkExtent(60, 4, C_BG, &x0, &x1));
	checkTrue("clear from the touch line to the name at y 104",
	          !fakeInkExtent(72, 32, C_BG, &x0, &x1));
	char pinLine[48];
	snprintf(pinLine, sizeof(pinLine), UI_SPLASH_PIN_FMT, (unsigned)settings()->dshotPin);
	checkTrue("the name and the version in place, no pin line",
	          fakeTextCentredIs(104, "MINIRC", C_LIME, C_BG, 4) &&
	          fakeTextCentredIs(260, "VERSION " FW_VERSION_STRING, C_GRID, C_BG, 1) &&
	          !fakeTextCentredIs(288, pinLine, C_AMBER, C_BG, 1) && !fakeInkExtent(268, 52, C_BG, &x0, &x1));
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
 *        two lines, MENU's UPDATED and SETUP's note until a save lands.
 */
static void testCarriedOver() {
	section("SETUP: a carry-over by an update shows until a save lands");
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
	uiInitTester();
	menuFrames(2);
	checkTrue("a version 4 block: carried over, its settings live",
	          settingsStored() && settingsCarriedOver() && !settingsResetByUpdate() &&
	          settings()->poles == 22 && settings()->maxThrottle == 1000);
	openMenu();
	UI_SETTINGS.status(b, sizeof(b), &col, &fill);
	checkTrue("MENU's SETUP row reads UPDATED, amber", strcmp(b, "UPDATED") == 0 && col == C_AMBER);
	checkTrue("drawn on the row", settingsRowAmber() > 0);
	tapId(menuRow(&UI_SETTINGS));
	menuFrames(1);
	checkTrue("SETUP: the note reads CARRIED OVER, amber", noteIs(NOTE, C_AMBER));
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
	openTester();
	settingsLoad();
	checkTrue("the next boot: a version 6 block, the settings kept, no notice",
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
 * @brief SIG PIN moves DShot at once: SETUP repeats the splash's warning
 *        for the live pin while it is not the pin the splash named.
 */
static void testEscPinNote() {
	section("SETUP: SIG PIN off the boot pin repeats the splash's warning");
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
	checkTrue("SETUP again: the warning stays, the saved set named",
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
	openTester();

	// The warning gone while scrolled to the end: the list takes its 200 px
	// back and the scroll is clamped to 620 px, with nothing blank under it.
	// Once by DEFAULTS, once by the pin set back with SETUP open, as a tool's
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
		openTester();
	}

	// Booted on the other pin: DEFAULTS puts DShot back on the default pin,
	// which the splash did not name: the warning, and DEFAULTS's note under it.
	Settings other;
	settingsDefaults(&other);
	other.dshotPin = moved;
	storeValid(&other);
	settingsLoad();
	uiInitTester();
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
	openTester();
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
	// LOGS' states.
	int log = uiMenuRowOf(&UI_SDLOG);
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	const SdLogState states[] = { SdLogState::NoCard, SdLogState::Idle, SdLogState::Logging, SdLogState::Error };
	for (SdLogState s : states) { st.state = s; fakeSdLogSet(&st); check(e[log], "a card state"); }
	fakeMscSet(MscState::Serving, 0); check(e[log], "a host holds the card");
	fakeMscSet(MscState::Idle, 0);
	// SETUP, unsaved, and reset by an update.
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
	openTester();
}

/** @brief Every SETUP value, across each row's range, fits the row's widest. */
static void testSettingsValueWidths() {
	section("SETUP: every value fits its row's widest");
	openSettings();
	const SettingsSection *secs[16];
	uint8_t ns = uiSettingsSections(secs, 16);
	checkTrue("SETUP shows its sections", ns >= 4);
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
	openTester();
}

/**
 * @brief SETUP values are neutral (menu-b.md, decision 11): the MAX
 *        THROTTLE, SIG PIN, AUX PIN and SERVO SUPPLY values in the text
 *        colour, and no value in cyan, the "changed" colour, or in lime, a
 *        live reading.
 */
static void testSettingsValueColours() {
	section("SETUP: a setting's value and a section caption cyan, a warning and a read-only value not");
	openSettings();
	const SettingsSection *secs[16];
	uint8_t ns = uiSettingsSections(secs, 16);
	Settings keep = *settings();
	settings()->kissEnable = 1;
	settings()->kissPin = (uint8_t)(settings()->dshotPin == 0 ? 1 : 0);
	int named = 0, steppers = 0, readOnly = 0;
	bool settingsCyan = true, readOnlyPlain = true, noText = true, noLime = true;
	char what[64];
	for (uint8_t i = 0; i < ns; i++)
		for (uint8_t r = 0; r < secs[i]->count; r++) {
			const SettingsRow &row = secs[i]->rows[r];
			char b[40];
			uint16_t col = 0;
			row.value(b, sizeof(b), &col);
			if (col == C_TEXT) noText = false;
			if (col == C_LIME) noLime = false;
			if (row.kind == SettingsKind::Stepper) {
				steppers++;
				if (col != C_CYAN) settingsCyan = false;
			} else if (row.kind == SettingsKind::ReadOnly) {
				readOnly++;
				if (col == C_CYAN) readOnlyPlain = false;
			}
			if (!strcmp(row.label, "MAX THROTTLE") || !strcmp(row.label, "SIG PIN") ||
			    !strcmp(row.label, "AUX PIN") || !strcmp(row.label, "SERVO SUPPLY")) {
				snprintf(what, sizeof(what), "%s: %s in C_CYAN", row.label, b);
				checkTrue(what, col == C_CYAN);
				named++;
			}
		}
	checkInt("the four rows found", named, 4);
	checkInt("every value drawn as text beside - and + is C_CYAN: the rows", steppers, 7);
	checkTrue("and each of them", settingsCyan);
	checkTrue("a read-only value is never C_CYAN: BUILT FOR, VERSION, CLOCK, LAST RESTART, UPTIME",
	          readOnly == 5 && readOnlyPlain);
	checkTrue("no value in the text colour, none lime", noText && noLime);

	// On the panel: the first section's caption and its first value, and
	// the toggle beside them, which keeps a button's label.
	menuFrames(2);
	char poles[8];
	snprintf(poles, sizeof(poles), "%u", (unsigned)settings()->poles);
	checkTrue("the caption ESC TESTER in C_CYAN at scale 1",
	          fakeTextIs(UI_MARGIN, ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y - 20 + 8, secs[0]->caption,
	                     C_CYAN, C_BG, 1) && !strcmp(secs[0]->caption, "ESC TESTER"));
	UiRect pr = ctlRect(UI_ROW(SettingsRowId::Poles, Row));
	checkTrue("MOTOR POLES: its value in C_CYAN at scale 2, its label C_DIM",
	          fakeTextIs(132 - gfxTextW(poles, 2), pr.y + 15, poles, C_CYAN, C_BG, 2) &&
	          fakeTextIs(UI_MARGIN, pr.y + 18, "MOTOR POLES", C_DIM, C_BG, 1));
	UiRect m = ctlRect(UI_ROW(SettingsRowId::Poles, Minus));
	checkTrue("its - stays a neutral button: C_TEXT on C_PANEL",
	          fakeTextIs(m.x + (m.w - gfxTextW("-", 2)) / 2, m.y + (m.h - 14) / 2, "-", C_TEXT, C_PANEL, 2));
	*settings() = keep;
	tapId(UI_FIX(UI_BACK));
	openTester();
}

/** @brief MENU opens at 0 px again when a tool returns to it. */
static void testMenuScrollResets() {
	section("MENU: a tool's return opens MENU at 0 px");
	static const UiMenuEntry seven[] = {
		{ "ESC COMMANDS", 0, &UI_ESC_CMD }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "SERVO PROGRAMMER", 0, &UI_SERVO_HUB_SCREEN },
		{ "RECEIVER", 9, &UI_RX_SIGNALS }, { "SETUP", 9, &UI_SETTINGS },
		{ "LOGS", 10, &UI_SDLOG },
	};
	freshTester();
	uiMenuUseRegistry(seven, 7);
	openMenu();
	fakePress(120, 250); menuFrames(1);
	for (int k = 1; k <= 12; k++) { fakeHold(120, 250 - 6 * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
	checkInt("scrolled 36 px: the seventh row at y 278", ctlRect(UI_ROW(6, Row)).y, 278);
	tapId(UI_ROW(6, Row));
	checkTrue("its LOGS opens", uiCurrentScreen() == UiScreenId::SdLog);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK lands on MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("at 0 px: the first row whole at y 62",
	          ctlRect(UI_ROW(0, Row)).y == 62 && ctlRect(UI_ROW(0, Row)).h == 40);
	checkInt("and the seventh cut to 6 px at y 314", ctlRect(UI_ROW(6, Row)).y, 314);
	uiMenuUseRegistry(nullptr, 0);
	tapId(UI_ROW(5, Row));                     // SETUP, and back: the registry's MENU
	tapId(UI_FIX(UI_BACK));
	openTester();
}

/**
 * @brief ESC SETTINGS' own refusal: opened beside another claim, as MENU no
 *        longer allows, it waits, leaves that claim alone, and claims once
 *        the other one ends. The backstop under MENU's disabled rows.
 */
static void testAm32WaitsBesideAClaim() {
	section("ESC SETTINGS: opened beside another claim, it waits");
	freshTester();
	testerToMenu();
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
		if (pinClaimState() == ClaimState::Leaving) pinClaimRelease(PinUser::Servo);
		uiAm32Tick(&none);
		claimed = pinClaimHolder() == PinUser::Am32;
	}
	checkTrue("AM32 claims once SERVO's claim has ended", claimed);
	uiAm32Exit();
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	checkTrue("and its exit frees the pins", pinClaimState() == ClaimState::Free);
	uiInitTester();
}

/**
 * @brief SERVO SUPPLY is a core section after WIRING, with the built
 *        registry and with a shorter one: no tool carries it, and KST
 *        SERVO, which no MENU row opens, has none.
 */
static void testSupplySection() {
	section("SETUP: SERVO SUPPLY is a core section after WIRING");
	checkTrue("KST SERVO carries no SETUP section", UI_KST.settings == nullptr);
	static const char *const WANT[] = {
		"ESC TESTER", "WIRING", "SERVO SUPPLY", "DISPLAY", "BOARD", "DIAGNOSTICS",
	};
	static const UiMenuEntry noKst[] = {
		{ "ESC TESTER", 0, &UI_TESTER },
		{ "ESC COMMANDS", 0, &UI_ESC_CMD }, { "AM32 CONFIG", 4, &UI_AM32 },
		{ "SERVO TESTER", 7, &UI_SERVO }, { "SETUP", 7, &UI_SETTINGS },
		{ "LOGS", 10, &UI_SDLOG },
	};
	for (int pass = 0; pass < 2; pass++) {
		uiMenuUseRegistry(pass ? noKst : nullptr, pass ? 6 : 0);
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
		openTester();
	}
	uiMenuUseRegistry(nullptr, 0);
	freshTester();
}

static void noStrip(char *buf, size_t n) { if (n) buf[0] = '\0'; }
static const UiHub *s_testHub = nullptr;
static const UiScreen *s_testHubScreen = nullptr;
static const UiHub *testHubOf(const UiScreen *s) { return s == s_testHubScreen ? s_testHub : nullptr; }

/**
 * @brief SETUP walks the hubs' entries for a tool's section
 *        (docs/design/menu-c.md, slice 4): a row's own section, then the
 *        sections of the entries of the hub the row opens, each once, in
 *        the order of the rows.
 */
static void testToolSectionsWalkHubs() {
	section("SETUP: a tool's section is found behind its hub");
	static const SettingsSection secA = { "TOOL A", nullptr, 0 };
	static const SettingsSection secB = { "TOOL B", nullptr, 0 };
	static const SettingsSection secC = { "TOOL C", nullptr, 0 };
	static const UiScreen a = { UiScreenId::Kst, "A", PinUser::None, nullptr, nullptr, nullptr, nullptr, nullptr,
	                            &secA, true };
	static const UiScreen b = { UiScreenId::Hitec, "B", PinUser::None, nullptr, nullptr, nullptr, nullptr,
	                            nullptr, &secB, true };
	static const UiScreen c = { UiScreenId::Angle, "C", PinUser::None, nullptr, nullptr, nullptr, nullptr,
	                            nullptr, &secC, true };
	static const UiScreen plain = { UiScreenId::SdLog, "P", PinUser::None, nullptr, nullptr, nullptr, nullptr,
	                                nullptr, nullptr, true };
	static const UiScreen hubScreen = { UiScreenId::ServoHub, "HUB", PinUser::None, nullptr, nullptr, nullptr,
	                                    nullptr, nullptr, nullptr, false };
	// B stands behind two entries, as HITEC SERVO does behind two of SERVO's.
	static const UiHubEntry entries[] = {
		{ "PLAIN", { nullptr, nullptr }, &plain, nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr, nullptr, nullptr },
		{ "B ONE", { nullptr, nullptr }, &b, nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr, nullptr, nullptr },
		{ "A", { nullptr, nullptr }, &a, nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr, nullptr, nullptr },
		{ "B TWO", { nullptr, nullptr }, &b, nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr, nullptr, nullptr },
	};
	static const UiHub hub = { "HUB", noStrip, entries, 4, nullptr };
	static const UiMenuEntry rows[] = {
		{ "P", 0, &plain }, { "HUB", 0, &hubScreen }, { "C", 0, &c }, { "A", 0, &a },
	};
	s_testHub = &hub;
	s_testHubScreen = &hubScreen;
	const SettingsSection *out[8];
	uint8_t n = uiSettingsToolSections(rows, 4, testHubOf, out, 8);
	checkInt("three sections: B and A behind the hub, then C", n, 3);
	checkTrue("in the order of the rows, the hub's entries in the hub's place: B, A, C",
	          n == 3 && out[0] == &secB && out[1] == &secA && out[2] == &secC);
	checkTrue("B, behind two entries, and A, behind an entry and a row, are there once each",
	          n == 3 && out[0] != out[1] && out[1] != out[2]);
	n = uiSettingsToolSections(rows, 4, nullptr, out, 8);
	checkTrue("with no hub walked: the rows' own sections only, C and A",
	          n == 2 && out[0] == &secC && out[1] == &secA);
	n = uiSettingsToolSections(rows, 4, testHubOf, out, 2);
	checkTrue("room for two: B and A, and no write past it", n == 2 && out[0] == &secB && out[1] == &secA);
	// The built registry: no tool carries a section today, behind a hub or not.
	uint8_t rc;
	const UiMenuEntry *e = uiMenuEntries(&rc);
	checkInt("the built registry and its three hubs: no tool section", uiSettingsToolSections(e, rc, uiHubOf, out, 8), 0);
	int behind = 0;
	for (uint8_t i = 0; i < rc; i++) {
		const UiHub *h = uiHubOf(e[i].screen);
		behind += h ? h->count : 0;
	}
	checkInt("which hold ten entries that the walk visits", behind, 10);
}

/**
 * @brief MENU repaints its strip when AUX moves or the claim changes, also
 *        with a registry whose rows claim nothing, and the tools' refusal
 *        reads WAIT: THE PINS ARE BUSY.
 */
static void testMenuStripRepaints() {
	section("MENU: the strip follows AUX and the claim");
	checkStr("the tools' refusal", UI_WAIT_RESTART_TEXT, "WAIT: THE PINS ARE BUSY");
	static const UiMenuEntry quiet[] = {
		{ "ESC COMMANDS", 0, &UI_ESC_CMD }, { "LOGS", 10, &UI_SDLOG },
	};
	freshTester();
	uiMenuUseRegistry(quiet, 2);
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
	snprintf(want, sizeof(want), "RELEASING SIG GP%u", (unsigned)sig);
	checkTrue("no row claims: the strip still reads RELEASING SIG GP0", captionIs(C_AMBER));
	pinClaimEnd(PinUser::Am32);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
	uiMenuUseRegistry(nullptr, 0);
	freshTester();
}

void runMenuTests() {
	testMenuRoot();
	testSdLogBack();
	testRenamedHeaders();
	testEveryEntry();
	testMenuDisarm();
	testPinsBusy();
	testMenuScroll();
	testSettingsRowVisible();
	testRxSignalsRow();
	testServoProg();
	testEscConfig();
	testSettingsScroll();
	testSettingsDiagnostics();
	testSettingsClock();
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
	testToolSectionsWalkHubs();
	testMenuStripRepaints();
}
