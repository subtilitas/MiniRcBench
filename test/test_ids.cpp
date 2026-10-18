/**
 * @file test_ids.cpp
 * @brief The control tables of ui_screen.h, walked through the real UI.
 *
 * Every screen reachable without an ESC is opened by id, and its table is
 * checked on each view: the ids, the rectangles, and the enabled flags that
 * follow the state the screen gates on.
 */

#include "check.h"
#include "fakes.h"
#include "settings.h"
#include "ui.h"
#include "ui_am32.h"
#include "ui_servo.h"
#include "ui_setup.h"
#include "ui_screen.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "core1_job.h"
#include "plat.h"

#include <string.h>

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void idFrames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiTick(); }
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
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	g_idFailHook = countFail;

	s_idFails = 0;
	tapId(UI_FIX(UI_BACK));
	int absent = s_idFails;
	s_idFails = 0;
	tapId(UI_FIX(TesterCtl::Throttle));
	int misgated = s_idFails;
	s_idFails = 0;
	tapId(UI_FIX(TesterCtl::Cfg), IdExpect::Disabled);
	int wrongWay = s_idFails;
	bool cfgOpen = uiCurrentScreen() == UiScreenId::Cfg;
	s_idFails = 0;
	tapId(UI_FIX(CfgCtl::Kst), IdExpect::Disabled);
	int right = s_idFails;

	g_idFailHook = nullptr;
	checkInt("a tap on a control that is absent reports once", absent, 1);
	checkInt("a tap expected to act on a control that does not reports once", misgated, 1);
	checkInt("a tap expected on a disabled control that acts reports once", wrongWay, 1);
	checkTrue("and still taps: CFG opened SETTINGS", cfgOpen);
	checkInt("a tap expected on a disabled control that is disabled reports nothing", right, 0);
}

/** @brief The tester's table and the id helpers on it. */
static void testHelpersReport() {
	section("Control ids: the helpers and the tester's table");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
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

/** @brief SETTINGS, SETUP and SD LOG by id. */
static void testCfgSetupLog() {
	section("Control ids: SETTINGS, SETUP and SD LOG");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Cfg));
	checkTrue("CFG opens SETTINGS", uiCurrentScreen() == UiScreenId::Cfg);
	checkControlTable("SETTINGS");
	checkTrue("SERVO acts", ctlEnabled(UI_FIX(CfgCtl::Servo)));
	checkTrue("KST is drawn disabled", ctlPresent(UI_FIX(CfgCtl::Kst)) && !ctlEnabled(UI_FIX(CfgCtl::Kst)));
	tapId(UI_FIX(CfgCtl::Kst), IdExpect::Disabled);
	checkTrue("a tap on KST stays on SETTINGS", uiCurrentScreen() == UiScreenId::Cfg);

	tapId(UI_FIX(CfgCtl::Setup));
	checkTrue("SETUP opens", uiCurrentScreen() == UiScreenId::Setup);
	checkControlTable("SETUP, KISS off");
	bool kissOn = settings()->kissEnable != 0;
	checkTrue("KISS PIN follows KISS TELEM",
	          ctlEnabled(UI_ROW(SetupRow::KissPin, Plus)) == kissOn);
	tapId(UI_ROW(SetupRow::Kiss, Toggle));
	checkTrue("KISS TELEM by id toggles it", (settings()->kissEnable != 0) != kissOn);
	checkTrue("and KISS PIN follows", ctlEnabled(UI_ROW(SetupRow::KissPin, Plus)) != kissOn);
	checkControlTable("SETUP, KISS toggled");
	checkTrue("BOARD has no control", !ctlPresent(UI_ROW(SetupRow::Board, Minus)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SETUP reaches SETTINGS", uiCurrentScreen() == UiScreenId::Cfg);

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	tapId(UI_FIX(CfgCtl::SdLog));
	checkTrue("SD LOG opens", uiCurrentScreen() == UiScreenId::SdLog);
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
	checkTrue("BACK from SD LOG reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/** @brief SERVO, its SET view and its unplug page by id. */
static void testServo() {
	section("Control ids: SERVO, SET and the unplug page");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Cfg));
	tapId(UI_FIX(CfgCtl::Servo));
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
	checkTrue("BACK from SERVO opens the unplug page", uiCurrentScreen() == UiScreenId::Servo &&
	          !ctlPresent(UI_FIX(ServoCtl::Min)));
	checkTrue("the unplug page shows RESTART or a BACK that waits",
	          ctlPresent(UI_FIX(ServoCtl::Restart)) || !ctlEnabled(UI_FIX(UI_BACK)));
	for (int i = 0; i < 200 && !ctlPresent(UI_FIX(ServoCtl::Restart)); i++) idFrames(1);
	checkControlTable("the unplug page");
	tapId(UI_FIX(ServoCtl::Restart));
	checkTrue("SERVO leaves to SETTINGS", uiCurrentScreen() == UiScreenId::Cfg);
}

/**
 * @brief AM32 CONFIG: BACK alone while the claim drains the pump, then the
 *        list once the fake bootloader has answered.
 */
static void testAm32() {
	section("Control ids: AM32 CONFIG, the handover and the list");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Cfg));
	tapId(UI_FIX(CfgCtl::Am32));
	checkTrue("AM32 opens", uiCurrentScreen() == UiScreenId::Am32);
	UiControl t[UI_CONTROLS_MAX];
	checkInt("during the handover, BACK is the only control", uiControls(t, UI_CONTROLS_MAX), 1);
	checkControlTable("AM32, the handover");
	idFrames(20);
	checkControlTable("AM32, the list");
	checkTrue("the list shows its rows", ctlPresent(UI_ROW(0, Row)));
	checkTrue("nothing to write yet", !ctlEnabled(UI_FIX(Am32Ctl::Write)) &&
	          !ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	checkTrue("no editor bar before a row is selected", !ctlPresent(UI_FIX(Am32Ctl::EditPlus)));
	tapId(UI_ROW(1, Row));
	checkTrue("a tap on a row by id shows the editor bar", ctlEnabled(UI_FIX(Am32Ctl::EditPlus)));
	// The fake ESC's field may sit at its top rail, where + changes nothing.
	tapId(UI_FIX(Am32Ctl::EditPlus));
	if (!ctlEnabled(UI_FIX(Am32Ctl::Write))) tapId(UI_FIX(Am32Ctl::EditMinus));
	checkTrue("a step makes WRITE and REVERT act", ctlEnabled(UI_FIX(Am32Ctl::Write)) &&
	          ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	checkControlTable("AM32, a field changed");
	tapId(UI_FIX(Am32Ctl::Revert));
	tapId(UI_FIX(UI_BACK));
	idFrames(20);
	checkTrue("BACK from AM32 reaches SETTINGS", uiCurrentScreen() == UiScreenId::Cfg);
}

void runIdTests() {
	testHelpersFail();
	testHelpersReport();
	testAm32();
	testCfgSetupLog();
	testServo();
}
