/**
 * @file test_ids.cpp
 * @brief The control tables of ui_screen.h, walked through the real UI.
 *
 * Every screen the host build opens is opened by id, and its table is
 * checked on each view: the ids, the rectangles, and the enabled flags that
 * follow the state the screen gates on.
 */

#include "check.h"
#include "fakes.h"
#include "settings.h"
#include "ui.h"
#include "ui_am32.h"
#include "ui_servo.h"
#include "ui_kst.h"
#include "ui_settings.h"
#include "ui_menu.h"
#include "ui_esc_cmd.h"
#include "ui_sdlog.h"
#include "ui_bridge.h"
#include "usb_bridge.h"
#include "fake_cdc.h"
#include "fake_bridge_line.h"
#include "fake_escwire_port.h"
#include "tusb.h"
#include "ui_screen.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "core1_job.h"
#include "plat.h"

#include <string.h>

/**
 * @brief @p n UI frames of 25 ms, with core1's passes before each, and the
 *        USB bridge's pass as core0's loop runs it.
 */
static void idFrames(int n) {
	for (int i = 0; i < n; i++) {
		fakeAdvance(25);
		fakePumpPoll();
		core1JobPoll();
		tud_task();
		usbBridgeTask();
		uiTick();
	}
}
#define UI_IDS_FRAMES(n) idFrames(n)
#include "ui_ids.h"

/** @brief Failures the helpers reported while @ref g_idFailHook pointed here. */
static int s_idFails = 0;
static void countFail(const char *) { s_idFails++; }

/**
 * @brief The id helpers report a control that is absent or gated the other
 *        way, through @ref g_idFailHook, which counts them here instead of
 *        failing the suite.
 */
static void testHelpersFail() {
	section("Control ids: a helper fails on an absent or misgated control");
	fakeFlashClear(); settingsLoad(); uiInitTester(); idFrames(4);
	g_idFailHook = countFail;

	s_idFails = 0;
	tapId(UI_FIX(UI_BACK));
	int absent = s_idFails;
	s_idFails = 0;
	tapId(UI_FIX(TesterCtl::Throttle));
	int misgated = s_idFails;
	s_idFails = 0;
	tapId(UI_FIX(TesterCtl::Throttle), IdExpect::Disabled);
	int right = s_idFails;
	s_idFails = 0;
	tapId(UI_FIX(TesterCtl::Menu), IdExpect::Disabled);
	int wrongWay = s_idFails;
	bool menuOpen = uiCurrentScreen() == UiScreenId::Menu;

	g_idFailHook = nullptr;
	checkInt("a tap on a control that is absent reports once", absent, 1);
	checkInt("a tap expected to act on a control that does not reports once", misgated, 1);
	checkInt("a tap expected on a disabled control that acts reports once", wrongWay, 1);
	checkTrue("and still taps: MENU opened", menuOpen);
	checkInt("a tap expected on a disabled control that is disabled reports nothing", right, 0);
}

/** @brief The tester's table and the id helpers on it. */
static void testHelpersReport() {
	section("Control ids: the helpers and the tester's table");
	fakeFlashClear(); settingsLoad(); uiInitTester(); idFrames(4);
	checkTrue("the tester is open after uiInit()", uiCurrentScreen() == UiScreenId::Tester);
	checkControlTable("tester, disarmed");
	checkTrue("ARM acts with the card free", ctlEnabled(UI_FIX(TesterCtl::Arm)));
	checkTrue("the throttle does not act while disarmed", !ctlEnabled(UI_FIX(TesterCtl::Throttle)));
	checkTrue("nor the pad", !ctlEnabled(UI_FIX(TesterCtl::Pad)));
	checkTrue("the tester has no BACK", !ctlPresent(UI_FIX(UI_BACK)));
	fakeMscSet(MscState::Serving, 0); idFrames(1);
	checkTrue("ARM does not act while a host holds the card", !ctlEnabled(UI_FIX(TesterCtl::Arm)));
	fakeMscSet(MscState::Idle, 0); idFrames(1);

	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); idFrames(2);
	checkTrue("a hold on ARM by id arms", uiArmed());
	checkTrue("the throttle acts while armed", ctlEnabled(UI_FIX(TesterCtl::Throttle)));
	checkTrue("and the pad", ctlEnabled(UI_FIX(TesterCtl::Pad)));
	checkControlTable("tester, armed");
	tapId(UI_FIX(TesterCtl::Arm));
	checkTrue("a tap on DISARM by id disarms", !uiArmed());
}

/** @brief MENU, SETTINGS and SD LOG by id. */
static void testMenuSettingsLog() {
	section("Control ids: MENU, SETTINGS and SD LOG");
	fakeFlashClear(); settingsLoad(); uiInitTester(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	// One frame for each screen's first paint: the table check compares the
	// header's BACK with the table.
	idFrames(1);
	checkControlTable("MENU");
	checkTrue("ESC TESTER acts", ctlEnabled(menuRow(&UI_TESTER)));
	checkTrue("ESC CONFIG acts", ctlEnabled(menuRow(&UI_ESC_CONFIG)));
	checkTrue("SERVO acts", ctlEnabled(menuRow(&UI_SERVO_HUB_SCREEN)));
	checkTrue("RX SIGNALS acts", ctlEnabled(menuRow(&UI_RX_SIGNALS)));
	checkTrue("the six rows by id, no seventh", ctlPresent(UI_ROW(5, Row)) && !ctlPresent(UI_ROW(6, Row)));

	tapId(toolEntry(&UI_ESC_CMD));
	checkTrue("ESC COMMANDS opens", uiCurrentScreen() == UiScreenId::EscCmd);
	idFrames(1);
	checkControlTable("ESC COMMANDS");
	{
		bool all = true;
		for (uint8_t c = (uint8_t)EscCmdCtl::Beep1; c <= (uint8_t)EscCmdCtl::On3d; c++)
			all = all && ctlEnabled(UI_FIX(c));
		checkTrue("its controls by id: BEEP 1 to 5 and the four holds act at Ready, no tenth",
		          all && ctlEnabled(UI_FIX(UI_BACK)) && !ctlPresent(UI_FIX(10)));
		fakeSetCmdReady(EscCmdReady::NoAnswer);
		idFrames(1);
		bool none = true;
		for (uint8_t c = (uint8_t)EscCmdCtl::Beep1; c <= (uint8_t)EscCmdCtl::On3d; c++)
			none = none && ctlPresent(UI_FIX(c)) && !ctlEnabled(UI_FIX(c));
		checkTrue("at NoAnswer none acts, BACK does", none && ctlEnabled(UI_FIX(UI_BACK)));
		checkControlTable("ESC COMMANDS, no ESC");
		fakeSetCmdReady(EscCmdReady::Ready);
		idFrames(1);
	}
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: ESC CONFIG, the opener", uiCurrentScreen() == UiScreenId::EscConfig);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
	idFrames(1);

	tapId(menuRow(&UI_RX_SIGNALS));
	checkTrue("RX SIGNALS opens", uiCurrentScreen() == UiScreenId::RxSignals);
	idFrames(1);
	checkControlTable("RX SIGNALS");
	checkTrue("its entries by id: MONITOR and OUTPUT act",
	          ctlEnabled(UI_FIX(RxCtl::Monitor)) && ctlEnabled(UI_FIX(RxCtl::Output)) &&
	          !ctlPresent(UI_FIX(3)));
	// No ESC on GP0: the bootloader probe hears nothing.
	fakeEwReset(0xB2, true);
	fakeEwSim()->dead = true;
	tapId(UI_FIX(RxCtl::Output));
	checkTrue("S.BUS OUTPUT opens by id and claims GP0's role",
	          uiCurrentScreen() == UiScreenId::SbusOut && pinClaimHolder() == PinUser::SbusOut);
	idFrames(20);
	checkControlTable("S.BUS OUTPUT, before SAFE");
	checkTrue("its table: BACK, HOLD TO START not acting, CH 9-16 and SET, no row",
	          ctlEnabled(UI_FIX(UI_BACK)) && ctlPresent(UI_FIX(SbusOutCtl::Start)) &&
	          !ctlEnabled(UI_FIX(SbusOutCtl::Start)) && ctlEnabled(UI_FIX(SbusOutCtl::Page)) &&
	          ctlEnabled(UI_FIX(SbusOutCtl::Set)) && !ctlPresent(UI_FIX(SbusOutCtl::Row0)) &&
	          !ctlPresent(UI_FIX(SbusOutCtl::Retry)));
	tapId(UI_FIX(SbusOutCtl::Set));
	checkControlTable("OUTPUT SET");
	checkTrue("SET: five rows, no STOP while SAFE", ctlPresent(UI_ROW(0, Row)) &&
	          !ctlPresent(UI_FIX(SbusOutCtl::Stop)));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	idFrames(1);
	checkControlTable("S.BUS OUTPUT, leave page");
	checkTrue("BACK: the leave page, BACK TO OUTPUT acting, RESTART DSHOT not yet",
	          !ctlEnabled(UI_FIX(UI_BACK)) && ctlEnabled(UI_FIX(SbusOutCtl::BackToOutput)) &&
	          ctlPresent(UI_FIX(SbusOutCtl::Release)) && !ctlEnabled(UI_FIX(SbusOutCtl::Release)));
	idFrames(44);
	tapId(UI_FIX(SbusOutCtl::Release));
	checkTrue("RESTART DSHOT: RX SIGNALS, the opener", uiCurrentScreen() == UiScreenId::RxSignals);
	tapId(UI_FIX(UI_BACK));  // to MENU
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) idFrames(1);
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	idFrames(1);

	tapId(menuRow(&UI_ESC_CONFIG));
	checkTrue("ESC CONFIG opens", uiCurrentScreen() == UiScreenId::EscConfig);
	idFrames(1);
	checkControlTable("ESC CONFIG");
	checkTrue("its entries by id: ESC COMMANDS, ESC SETTINGS and USB BRIDGE act, no fourth",
	          ctlEnabled(UI_FIX(EscCfgCtl::Commands)) && ctlEnabled(UI_FIX(EscCfgCtl::Settings)) &&
	          ctlEnabled(UI_FIX(EscCfgCtl::Bridge)) && !ctlPresent(UI_FIX(4)));
	fakeBridgeLineReset();
	fakeCdcReset();
	tapId(UI_FIX(EscCfgCtl::Bridge));
	checkTrue("USB BRIDGE opens by id and claims SIG", uiCurrentScreen() == UiScreenId::Bridge &&
	          pinClaimHolder() == PinUser::Bridge);
	for (int k = 0; k < 20 && usbBridgeMode() == BridgeMode::Closed; k++) idFrames(1);
	idFrames(1);
	checkControlTable("USB BRIDGE");
	checkTrue("its table: BACK acts, nothing else", ctlEnabled(UI_FIX(UI_BACK)) &&
	          !ctlPresent(UI_FIX(BridgeCtl::Retry)) && !ctlPresent(UI_FIX(BridgeCtl::Bridge)) &&
	          !ctlPresent(UI_FIX(BridgeCtl::Release)));
	tapId(UI_FIX(UI_BACK));
	idFrames(1);
	checkControlTable("USB BRIDGE, leave page");
	checkTrue("BACK: the leave page, BRIDGE acting, RESTART DSHOT not yet",
	          !ctlEnabled(UI_FIX(UI_BACK)) && ctlEnabled(UI_FIX(BridgeCtl::Bridge)) &&
	          ctlPresent(UI_FIX(BridgeCtl::Release)) && !ctlEnabled(UI_FIX(BridgeCtl::Release)));
	idFrames(44);
	tapId(UI_FIX(BridgeCtl::Release));
	checkTrue("RESTART DSHOT: ESC CONFIG, the opener", uiCurrentScreen() == UiScreenId::EscConfig);
	tapId(UI_FIX(UI_BACK));  // to MENU
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) idFrames(1);
	idFrames(1);
	checkTrue("Free, the console back", pinClaimState() == ClaimState::Free && fakeConsoleOn());

	tapId(menuRow(&UI_SERVO_HUB_SCREEN));
	checkTrue("SERVO opens", uiCurrentScreen() == UiScreenId::ServoHub);
	idFrames(1);
	checkControlTable("SERVO");
	checkTrue("its entries by id at scroll 0: three act, HITEC D SERIES, cut, is listed and does not, no fifth",
	          ctlEnabled(HUB_ENTRY(0)) && ctlEnabled(HUB_ENTRY(1)) && ctlEnabled(HUB_ENTRY(2)) &&
	          ctlPresent(HUB_ENTRY(3)) && !ctlEnabled(HUB_ENTRY(3)) && !ctlPresent(HUB_ENTRY(4)));
	tapId(toolEntry(&UI_ANGLE));
	idFrames(1);
	checkTrue("ANGLE METER opens on its main view",
	          uiCurrentScreen() == UiScreenId::Angle && uiAngleView() == AngleView::Main);
	idFrames(4);
	checkControlTable("ANGLE METER");
	checkTrue("its controls by id: ZERO, drawn disabled with no part answering, RESET MAX and SETUP act",
	          ctlPresent(UI_FIX(AngleCtl::Zero)) && !ctlEnabled(UI_FIX(AngleCtl::Zero)) &&
	          ctlEnabled(UI_FIX(AngleCtl::ResetMax)) && ctlEnabled(UI_FIX(AngleCtl::Setup)) &&
	          !ctlPresent(UI_FIX(AngleCtl::Along)) && !ctlPresent(UI_FIX(AngleCtl::Keep)));
	tapId(UI_FIX(AngleCtl::Setup));
	checkControlTable("ANGLE SETUP");
	checkTrue("SETUP's controls: ALONG AXIS, LENGTH's - and +, SIGN, FLIP, all acting",
	          ctlEnabled(UI_FIX(AngleCtl::Along)) && ctlEnabled(UI_FIX(AngleCtl::LenMinus)) &&
	          ctlEnabled(UI_FIX(AngleCtl::LenPlus)) && ctlEnabled(UI_FIX(AngleCtl::Sign)) &&
	          ctlEnabled(UI_FIX(AngleCtl::Flip)) && !ctlPresent(UI_FIX(AngleCtl::Zero)));
	tapId(UI_FIX(AngleCtl::Flip));
	checkControlTable("FLIP CAL");
	checkTrue("FLIP CAL's controls: HOLD TO KEEP without a result does not act, AGAIN does",
	          ctlPresent(UI_FIX(AngleCtl::Keep)) && !ctlEnabled(UI_FIX(AngleCtl::Keep)) &&
	          ctlEnabled(UI_FIX(AngleCtl::Again)) && !ctlPresent(UI_FIX(AngleCtl::Flip)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("FLIP CAL's BACK: SETUP", uiAngleView() == AngleView::Setup);
	tapId(UI_FIX(UI_BACK));
	checkTrue("SETUP's BACK: the main view", uiAngleView() == AngleView::Main);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: SERVO, the opener", uiCurrentScreen() == UiScreenId::ServoHub);
	idFrames(1);
	revealEntry(4);
	checkControlTable("SERVO, scrolled to its end");
	checkTrue("scrolled to its end: KST / CHASERVO and HITEC D SERIES act, HITEC CLASSIC without the switch does not",
	          !ctlPresent(HUB_ENTRY(0)) && ctlEnabled(HUB_ENTRY(2)) && ctlEnabled(HUB_ENTRY(3)) &&
	          ctlPresent(HUB_ENTRY(4)) && !ctlEnabled(HUB_ENTRY(4)) && !ctlPresent(HUB_ENTRY(5)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
	idFrames(1);

	tapId(menuRow(&UI_SETTINGS));
	checkTrue("SETTINGS opens", uiCurrentScreen() == UiScreenId::Settings);
	idFrames(1);
	checkControlTable("SETTINGS at 0 px");
	revealRow((int)SettingsRowId::Aux);
	checkControlTable("SETTINGS, AUX PIN in view");
	bool kissOn = settings()->kissEnable != 0;
	checkTrue("AUX PIN acts", ctlEnabled(UI_ROW(SettingsRowId::Aux, Plus)));
	tapId(UI_ROW(SettingsRowId::Kiss, Toggle));
	checkTrue("KISS TELEM by id toggles it", (settings()->kissEnable != 0) != kissOn);
	checkTrue("and AUX PIN acts either way", ctlEnabled(UI_ROW(SettingsRowId::Aux, Plus)));
	checkControlTable("SETTINGS, KISS toggled");
	revealRow((int)SettingsRowId::BuiltFor);
	checkTrue("BUILT FOR shows and has no control", ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Row)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Toggle)));
	revealRow((int)SettingsRowId::Version);
	checkTrue("VERSION shows and has no control", ctlPresent(UI_ROW(SettingsRowId::Version, Row)) &&
	          !ctlEnabled(UI_ROW(SettingsRowId::Version, Row)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::Version, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::Version, Toggle)));
	checkControlTable("SETTINGS at the end");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SETTINGS reaches MENU", uiCurrentScreen() == UiScreenId::Menu);

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	tapId(menuRow(&UI_SDLOG));
	checkTrue("SD LOG opens", uiCurrentScreen() == UiScreenId::SdLog);
	idFrames(1);
	checkControlTable("SD LOG, no card");
	checkTrue("no card: START does not act", !ctlEnabled(UI_FIX(SdLogCtl::Toggle)));
	checkTrue("nor USB DRIVE", !ctlEnabled(UI_FIX(SdLogCtl::Usb)));
	checkTrue("RETRY acts", ctlEnabled(UI_FIX(SdLogCtl::Retry)));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st); idFrames(1);
	checkTrue("a card: START acts", ctlEnabled(UI_FIX(SdLogCtl::Toggle)));
	fakeMscSet(MscState::Serving, 0); idFrames(1);
	checkControlTable("SD LOG, card held");
	checkTrue("card held: EJECT acts", ctlEnabled(UI_FIX(SdLogCtl::Toggle)));
	checkTrue("RETRY is gone", !ctlPresent(UI_FIX(SdLogCtl::Retry)));
	checkTrue("and USB DRIVE", !ctlPresent(UI_FIX(SdLogCtl::Usb)));
	fakeMscSet(MscState::Idle, 0); idFrames(1);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SD LOG reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
	checkTrue("MENU's row ESC TESTER reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/** @brief SERVO, its SET view and its leave page by id. */
static void testServo() {
	section("Control ids: SERVO, SET and the leave page");
	fakeFlashClear(); settingsLoad(); uiInitTester(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(toolEntry(&UI_SERVO));
	checkTrue("SERVO opens", uiCurrentScreen() == UiScreenId::Servo);
	idFrames(20);
	checkControlTable("SERVO, SAFE");
	checkTrue("SAFE: MIN does not act", !ctlEnabled(UI_FIX(ServoCtl::Min)));
	checkTrue("nor the dial", !ctlEnabled(UI_FIX(ServoCtl::Dial)));
	checkTrue("SET acts", ctlEnabled(UI_FIX(ServoCtl::Set)));

	tapId(UI_FIX(ServoCtl::Set));
	checkTrue("SET is SERVO's view", uiCurrentScreen() == UiScreenId::Servo);
	checkTrue("SET shows HOLD TO APPLY", ctlPresent(UI_FIX(ServoCtl::Apply)));
	checkTrue("nothing to apply yet", !ctlEnabled(UI_FIX(ServoCtl::Apply)));
	checkTrue("no row selected: no editor bar", !ctlPresent(UI_FIX(ServoCtl::EditPlus)));
	checkTrue("its two hints in C_DIM: TAP A ROW TO EDIT IT, SWIPE IT SIDEWAYS FOR COARSE",
	          fakeTextCentredIs(218 + 12, "TAP A ROW TO EDIT IT", C_DIM, C_BG, 1) &&
	          fakeTextCentredIs(218 + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_DIM, C_BG, 1));
	checkControlTable("SET");
	revealRow(11);
	checkTrue("the last row scrolls whole", ctlRect(UI_ROW(11, Row)).h == listRowH());
	revealRow(0);
	tapId(UI_ROW(2, Row));
	checkTrue("a tap on a row by id selects it: the editor bar shows",
	          ctlPresent(UI_FIX(ServoCtl::EditPlus)));
	checkControlTable("SET, a row selected");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SET shows SERVO's buttons", ctlPresent(UI_FIX(ServoCtl::Min)));

	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SERVO opens the leave page", uiCurrentScreen() == UiScreenId::Servo &&
	          !ctlPresent(UI_FIX(ServoCtl::Min)));
	checkTrue("the leave page: BACK TO SERVO, RESTART DSHOT, BACK disabled",
	          ctlPresent(UI_FIX(ServoCtl::NextServo)) && ctlPresent(UI_FIX(ServoCtl::Release)) &&
	          !ctlEnabled(UI_FIX(UI_BACK)));
	checkControlTable("the leave page, releasing");
	for (int i = 0; i < 200 && !ctlEnabled(UI_FIX(ServoCtl::Release)); i++) idFrames(1);
	checkControlTable("the leave page");
	tapId(UI_FIX(ServoCtl::Release));
	checkTrue("SERVO TESTER leaves to SERVO, the hub that opened it", uiCurrentScreen() == UiScreenId::ServoHub);
	tapId(UI_FIX(UI_BACK));
	checkTrue("whose BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
}

/**
 * @brief ESC SETTINGS: BACK alone while the claim drains the pump, then the
 *        list once the fake bootloader has answered.
 */
static void testAm32() {
	section("Control ids: ESC SETTINGS, the handover and the list");
	fakeFlashClear(); settingsLoad(); uiInitTester(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	openEscSettings();
	checkTrue("AM32 opens", uiCurrentScreen() == UiScreenId::Am32);
	idFrames(1);  // its first paint, for the table check's BACK
	UiControl t[UI_CONTROLS_MAX];
	checkInt("during the handover, BACK is the only control", uiControls(t, UI_CONTROLS_MAX), 1);
	checkControlTable("AM32, the handover");
	idFrames(50);  // past the connect cycle's 1000 ms quiet phase
	checkControlTable("AM32, the list");
	checkTrue("the list shows its rows", ctlPresent(UI_ROW(0, Row)));
	checkTrue("nothing to write yet: WRITE and REVERT are there and do not act",
	          ctlPresent(UI_FIX(Am32Ctl::Write)) && !ctlEnabled(UI_FIX(Am32Ctl::Write)) &&
	          ctlPresent(UI_FIX(Am32Ctl::Revert)) && !ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	checkTrue("no editor bar before a row is selected", !ctlPresent(UI_FIX(Am32Ctl::EditPlus)));
	checkTrue("its two hints in C_DIM: TAP A SETTING TO EDIT, SWIPE IT SIDEWAYS FOR COARSE",
	          fakeTextCentredIs(218 + 12, "TAP A SETTING TO EDIT", C_DIM, C_BG, 1) &&
	          fakeTextCentredIs(218 + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_DIM, C_BG, 1));
	tapId(UI_ROW(1, Row));
	checkTrue("a tap on a row by id shows the editor bar", ctlEnabled(UI_FIX(Am32Ctl::EditPlus)));
	// The fake ESC's image puts some fields on their top rail, where + changes
	// nothing. The first visible row that + changes is stepped, and - on the
	// same row takes the change back.
	int stepped = -1;
	for (int r = 0; r < 6 && stepped < 0; r++) {
		tapId(UI_ROW(r, Row));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		if (ctlEnabled(UI_FIX(Am32Ctl::Write))) stepped = r;
	}
	checkTrue("+ changes one of the first six rows", stepped >= 0);
	checkTrue("+ makes WRITE and REVERT act", ctlEnabled(UI_FIX(Am32Ctl::Write)) &&
	          ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	checkControlTable("AM32, a field changed");
	tapId(UI_FIX(Am32Ctl::EditMinus));
	checkTrue("- on the same row takes it back: WRITE does not act",
	          ctlPresent(UI_FIX(Am32Ctl::Write)) && !ctlEnabled(UI_FIX(Am32Ctl::Write)));
	tapId(UI_FIX(Am32Ctl::EditPlus));
	checkTrue("+ again: REVERT acts", ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	tapId(UI_FIX(Am32Ctl::Revert));
	checkTrue("REVERT throws the edit away: WRITE and REVERT are there and do not act",
	          ctlPresent(UI_FIX(Am32Ctl::Write)) && ctlPresent(UI_FIX(Am32Ctl::Revert)) &&
	          !ctlEnabled(UI_FIX(Am32Ctl::Write)) && !ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	tapId(UI_FIX(UI_BACK));
	idFrames(20);
	checkTrue("BACK from AM32 reaches ESC CONFIG, the opener", uiCurrentScreen() == UiScreenId::EscConfig);
}

/** @brief Whether @p id's rectangle on the current frame is x, y, w, h. */
static bool rectIs(UiControlId id, int x, int y, int w, int h) {
	UiRect r = ctlRect(id);
	return r.x == x && r.y == y && r.w == w && r.h == h;
}

/** @brief Whether @p label, scale @p sc in @p col on @p bg, is centred in @p id's rectangle. */
static bool labelCentred(UiControlId id, const char *label, uint16_t col, uint16_t bg, int sc) {
	UiRect r = ctlRect(id);
	int w = gfxTextW(label, sc);
	return w + 2 * 4 <= r.w &&
	       fakeTextIs(r.x + (r.w - w) / 2, r.y + (r.h - 7 * sc) / 2, label, col, bg, sc);
}

/**
 * @brief Decision 14 of docs/design/menu-b.md, the layout table, on SETTINGS,
 *        SD LOG and SERVO SET: the button edge x 6 to x 234, the two-button
 *        footer 6/150 and 162/72 at y 268, 46 px tall, the editor's 44 px
 *        - and +, each label inside its button with 4 px each side.
 */
static void testLayoutTable() {
	section("Control ids: the layout table on SETTINGS, SD LOG and SERVO SET");
	fakeFlashClear(); settingsLoad(); uiInitTester(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_SETTINGS));
	idFrames(1);
	checkTrue("SETTINGS: HOLD TO SAVE at 6/150, y 268, 46 px",
	          rectIs(UI_FIX(SettingsCtl::Save), 6, 268, 150, 46));
	checkTrue("HOLD DEFAULTS at 162/72, ending at the button edge, x 234",
	          rectIs(UI_FIX(SettingsCtl::Defaults), 162, 268, 72, 46) &&
	          162 + 72 == GFX_W - UI_EDGE);
	checkTrue("HOLD TO SAVE's label inside its button",
	          labelCentred(UI_FIX(SettingsCtl::Save), "HOLD TO SAVE", C_TEXT, C_PANEL, 1));
	checkControlTable("SETTINGS, the footer");
	tapId(UI_FIX(UI_BACK));

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	tapId(menuRow(&UI_SDLOG));
	idFrames(1);
	checkTrue("SD LOG: START from x 6 to x 234, y 240, 44 px",
	          rectIs(UI_FIX(SdLogCtl::Toggle), 6, 240, 228, 44));
	checkTrue("RETRY MOUNT 6/112 and USB DRIVE 122/112, y 288, 30 px",
	          rectIs(UI_FIX(SdLogCtl::Retry), 6, 288, 112, 30) &&
	          rectIs(UI_FIX(SdLogCtl::Usb), 122, 288, 112, 30));
	checkTrue("START, RETRY MOUNT and USB DRIVE inside their buttons",
	          labelCentred(UI_FIX(SdLogCtl::Toggle), "START", C_TEXT, C_PANEL, 2) &&
	          labelCentred(UI_FIX(SdLogCtl::Retry), "RETRY MOUNT", C_TEXT, C_PANEL, 1) &&
	          labelCentred(UI_FIX(SdLogCtl::Usb), "USB DRIVE", C_TEXT, C_PANEL, 1));
	checkControlTable("SD LOG, the button edge");
	fakeMscSet(MscState::Serving, 0); idFrames(1);
	checkTrue("card held: EJECT keeps START's rectangle",
	          rectIs(UI_FIX(SdLogCtl::Toggle), 6, 240, 228, 44));
	fakeMscSet(MscState::Idle, 0); idFrames(1);
	tapId(UI_FIX(UI_BACK));

	tapId(toolEntry(&UI_SERVO));
	idFrames(20);
	tapId(UI_FIX(ServoCtl::Set));
	idFrames(1);
	checkTrue("SERVO SET: HOLD TO APPLY at 6/150, y 268, 46 px",
	          rectIs(UI_FIX(ServoCtl::Apply), 6, 268, 150, 46));
	checkTrue("DEFAULTS at 162/72", rectIs(UI_FIX(ServoCtl::Defaults), 162, 268, 72, 46));
	checkTrue("DEFAULTS' label inside its button",
	          labelCentred(UI_FIX(ServoCtl::Defaults), "DEFAULTS", C_AMBER, C_PANEL, 1));
	tapId(UI_ROW(2, Row));
	checkTrue("the editor's - at x 6 and + ending at x 234, 44 x 44 px",
	          rectIs(UI_FIX(ServoCtl::EditMinus), 6, 218, 44, 44) &&
	          rectIs(UI_FIX(ServoCtl::EditPlus), 190, 218, 44, 44));
	checkTrue("the hint centred between them, 140 px, C_DIM",
	          fakeTextIs(50 + (140 - gfxTextW("SWIPE = COARSE", 1)) / 2, 218 + 34, "SWIPE = COARSE",
	                     C_DIM, C_BG, 1));
	checkTrue("RATE at 50 HZ: + is a neutral button, C_PANEL, the frame, C_TEXT at scale 2",
	          ctlEnabled(UI_FIX(ServoCtl::EditPlus)) && neutralButtonIs(UI_FIX(ServoCtl::EditPlus), "+", 2));
	checkTrue("and - at its lowest step is drawn disabled, C_BG with a C_GRID label",
	          !ctlEnabled(UI_FIX(ServoCtl::EditMinus)) && disabledButtonIs(UI_FIX(ServoCtl::EditMinus), "-", 2));
	tapId(UI_FIX(ServoCtl::EditPlus));
	checkTrue("one step up: both are neutral buttons",
	          ctlEnabled(UI_FIX(ServoCtl::EditMinus)) && neutralButtonIs(UI_FIX(ServoCtl::EditMinus), "-", 2) &&
	          neutralButtonIs(UI_FIX(ServoCtl::EditPlus), "+", 2));
	tapId(UI_FIX(ServoCtl::EditMinus));
	checkControlTable("SERVO SET, the layout table");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	for (int i = 0; i < 200 && !ctlEnabled(UI_FIX(ServoCtl::Release)); i++) idFrames(1);
	tapId(UI_FIX(ServoCtl::Release));
	checkTrue("back at SERVO, the hub", uiCurrentScreen() == UiScreenId::ServoHub);
	for (int i = 0; i < 200 && pinClaimState() != ClaimState::Free; i++) idFrames(1);
	checkTrue("then MENU and the tester", backToTester());
}

/**
 * @brief buttonCornersClear(), the walk of checkControlTable(): it takes a
 *        drawn button for one, counts it, and reports a corner pixel that is
 *        not the ground's.
 */
static void testCornerWalk() {
	section("Control ids: the walk over a table's button corners");
	const UiControl t[] = {
		{ UI_FIX(1), { 20, 100, 110, 46 }, true },
		{ UI_FIX(2), { 140, 100, 64, 32 }, false },
		{ UI_FIX(3), { 20, 200, 200, 40 }, true },   // nothing drawn here: a list row
	};
	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		const char *tn = th ? "high contrast" : "dark";
		char what[96];
		gfxFill(C_BG);
		uiButton(t[0].rect, "ONE", C_PANEL, C_TEXT, 2, true, true);
		uiButton(t[1].rect, "TWO", C_PANEL, C_TEXT, 1, false, false);
		snprintf(what, sizeof(what), "%s: a pressed and a disabled button are walked, the bare row is not", tn);
		checkInt(what, buttonCornersClear(t, 3), 2);
		uint16_t *fb = gfxBuffer();
		fb[100 * GFX_W + 21] = C_PANEL;
		snprintf(what, sizeof(what), "%s: a fill left at (1,0) of a corner is reported", tn);
		checkInt(what, buttonCornersClear(t, 3), -1);
		fb[100 * GFX_W + 21] = C_BG;
		fb[(100 + 31) * GFX_W + 140 + 63] = C_GRID;
		snprintf(what, sizeof(what), "%s: a frame's pixel left at the last corner is reported", tn);
		checkInt(what, buttonCornersClear(t, 3), -1);
		// A square frame, as a button was drawn before the radius.
		gfxFill(C_BG);
		gfxRect(20, 100, 110, 46, C_GRID);
		gfxRect(21, 101, 108, 44, C_PANEL);
		snprintf(what, sizeof(what), "%s: a button with square corners is reported", tn);
		checkInt(what, buttonCornersClear(t, 1), -1);
	}
	themeSet(Theme::Dark);
	gfxFill(C_BG);
}

/**
 * @brief The walk over a table's changed values (changedFillsClear()): it
 *        finds the rectangle of a changed row and of none other, and reports
 *        a label that ends less than 4 px before the rectangle.
 */
static void testChangedWalk() {
	section("Control ids: the walk over a table's changed values");
	const UiControl t[] = {
		{ UI_ROW(0, Row), { 0, 100, GFX_W, 40 }, true },
		{ UI_ROW(1, Row), { 0, 142, GFX_W, 40 }, true },
		{ UI_FIX(1), { 6, 268, 110, 46 }, true },
	};
	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		const char *tn = th ? "high contrast" : "dark";
		char what[112];
		gfxFill(C_BG);
		UiField a = { "PULSE", "TYPE", "STANDARD", UiValue::Setting, UiEdge::None, false };
		UiField b = { "PULSE", "RATE", "50 HZ", UiValue::Setting, UiEdge::None, true };
		uiListDrawField(100, 40, a);
		uiListDrawField(142, 40, b);
		// A cyan button is not a row and is not taken for a changed value.
		uiButton(t[2].rect, "BACK", C_CYAN, C_ONACCENT, 2, false, true);
		snprintf(what, sizeof(what), "%s: two settings as they stand: no rectangle", tn);
		checkInt(what, changedFillsClear(t, 3), 0);
		a.state = UiValue::Changed; a.edge = UiEdge::Changed;
		b.state = UiValue::Changed; b.edge = UiEdge::Rule;
		uiListDrawField(100, 40, a);
		uiListDrawField(142, 40, b);
		snprintf(what, sizeof(what), "%s: both changed, one selected: two rectangles, each clear", tn);
		checkInt(what, changedFillsClear(t, 3), 2);
		// 23 characters at scale 1 end at x 145; STANDARD's rectangle starts
		// at x 135.
		a.label = "LABEL OF 23 CHARACTERS.";
		uiListDrawField(100, 40, a);
		snprintf(what, sizeof(what), "%s: a label that runs under the rectangle is reported", tn);
		checkInt(what, changedFillsClear(t, 3), -1);
		// 21 characters end at x 132: 2 px before the rectangle.
		a.label = "LABEL OF 21 CHARACTER";
		uiListDrawField(100, 40, a);
		snprintf(what, sizeof(what), "%s: a label that ends 2 px before the rectangle is reported", tn);
		checkInt(what, changedFillsClear(t, 3), -1);
		snprintf(what, sizeof(what), "%s: that label's end, 2 px before the rectangle's x 135", tn);
		checkInt(what, 8 + gfxTextW(a.label, 1) + 2, GFX_W - UI_MARGIN - gfxTextW("STANDARD", 2) - UI_FIELD_FILL_DX);
		// 20 characters end at x 126: 8 px before it.
		a.label = "LABEL OF 20 CHARACTE";
		uiListDrawField(100, 40, a);
		snprintf(what, sizeof(what), "%s: a label that ends 8 px before the rectangle passes", tn);
		checkInt(what, changedFillsClear(t, 3), 2);
	}
	themeSet(Theme::Dark);
	gfxFill(C_BG);
}

void runIdTests() {
	testHelpersFail();
	testHelpersReport();
	testAm32();
	testMenuSettingsLog();
	testServo();
	testLayoutTable();
	testCornerWalk();
	testChangedWalk();
}
