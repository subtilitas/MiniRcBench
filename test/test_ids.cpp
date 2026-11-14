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
#include "ui_sdlog.h"
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

/** @brief MENU, SETTINGS and SD LOG by id. */
static void testMenuSettingsLog() {
	section("Control ids: MENU, SETTINGS and SD LOG");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	// One frame for each screen's first paint: the table check compares the
	// header's BACK with the table.
	idFrames(1);
	checkControlTable("MENU");
	checkTrue("SERVO TESTER acts", ctlEnabled(menuRow(&UI_SERVO)));
	checkTrue("KST SERVO acts", ctlEnabled(menuRow(&UI_KST)));

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
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/** @brief SERVO, its SET view and its unplug page by id. */
static void testServo() {
	section("Control ids: SERVO, SET and the unplug page");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_SERVO));
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
	checkTrue("SERVO leaves to MENU", uiCurrentScreen() == UiScreenId::Menu);
}

/**
 * @brief AM32 CONFIG: BACK alone while the claim drains the pump, then the
 *        list once the fake bootloader has answered.
 */
static void testAm32() {
	section("Control ids: AM32 CONFIG, the handover and the list");
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 opens", uiCurrentScreen() == UiScreenId::Am32);
	idFrames(1);  // its first paint, for the table check's BACK
	UiControl t[UI_CONTROLS_MAX];
	checkInt("during the handover, BACK is the only control", uiControls(t, UI_CONTROLS_MAX), 1);
	checkControlTable("AM32, the handover");
	idFrames(20);
	checkControlTable("AM32, the list");
	checkTrue("the list shows its rows", ctlPresent(UI_ROW(0, Row)));
	checkTrue("nothing to write yet: WRITE and REVERT are there and do not act",
	          ctlPresent(UI_FIX(Am32Ctl::Write)) && !ctlEnabled(UI_FIX(Am32Ctl::Write)) &&
	          ctlPresent(UI_FIX(Am32Ctl::Revert)) && !ctlEnabled(UI_FIX(Am32Ctl::Revert)));
	checkTrue("no editor bar before a row is selected", !ctlPresent(UI_FIX(Am32Ctl::EditPlus)));
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
	checkTrue("BACK from AM32 reaches SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
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
	fakeFlashClear(); settingsLoad(); uiInit(); idFrames(4);
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
	          labelCentred(UI_FIX(SdLogCtl::Toggle), "START", C_LIME, C_PANEL, 2) &&
	          labelCentred(UI_FIX(SdLogCtl::Retry), "RETRY MOUNT", C_CYAN, C_PANEL, 1) &&
	          labelCentred(UI_FIX(SdLogCtl::Usb), "USB DRIVE", C_CYAN, C_PANEL, 1));
	checkControlTable("SD LOG, the button edge");
	fakeMscSet(MscState::Serving, 0); idFrames(1);
	checkTrue("card held: EJECT keeps START's rectangle",
	          rectIs(UI_FIX(SdLogCtl::Toggle), 6, 240, 228, 44));
	fakeMscSet(MscState::Idle, 0); idFrames(1);
	tapId(UI_FIX(UI_BACK));

	tapId(menuRow(&UI_SERVO));
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
	checkTrue("the hint centred between them, 140 px",
	          fakeTextIs(50 + (140 - gfxTextW("SWIPE = COARSE", 1)) / 2, 218 + 34, "SWIPE = COARSE",
	                     C_GRID, C_BG, 1));
	checkControlTable("SERVO SET, the layout table");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	for (int i = 0; i < 200 && !ctlPresent(UI_FIX(ServoCtl::Restart)); i++) idFrames(1);
	tapId(UI_FIX(ServoCtl::Restart));
	checkTrue("back at MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
}

void runIdTests() {
	testHelpersFail();
	testHelpersReport();
	testAm32();
	testMenuSettingsLog();
	testServo();
	testLayoutTable();
}
