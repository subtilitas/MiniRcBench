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

/** @brief The id helpers fail a test on a control that is absent or gated the other way. */
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
	if (uiCurrentScreen() == UiScreenId::Servo) {
		checkTrue("the unplug page shows RESTART or a BACK that waits",
		          ctlPresent(UI_FIX(ServoCtl::Restart)) || !ctlEnabled(UI_FIX(UI_BACK)));
		for (int i = 0; i < 200 && !ctlPresent(UI_FIX(ServoCtl::Restart)); i++) idFrames(1);
		checkControlTable("the unplug page");
		tapId(UI_FIX(ServoCtl::Restart));
	}
	checkTrue("SERVO leaves to SETTINGS", uiCurrentScreen() == UiScreenId::Cfg);
}

void runIdTests() {
	testHelpersReport();
	testCfgSetupLog();
	testServo();
}
