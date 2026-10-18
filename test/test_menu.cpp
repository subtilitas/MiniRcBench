/**
 * @file test_menu.cpp
 * @brief MENU, SETTINGS and the navigation between them and the tools,
 *        through the real UI.
 *
 * The cases slice C of docs/design/menu.md lists: SD LOG's BACK to MENU,
 * every registry entry opened from MENU and back, MENU's disarm, BEEP's
 * flash on its row, PINS BUSY while the claim restores, MENU's scroll over
 * registries of six and seven rows, and SETTINGS' list: its scroll range,
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

#include <string.h>

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void menuFrames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiTick(); }
}
#define UI_IDS_FRAMES(n) menuFrames(n)
#include "ui_ids.h"

/** @brief A pixel of the frame. */
static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

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
	checkInt("slice C's registry has five rows", n, 5);
	for (uint8_t i = 0; i < n; i++) {
		const UiScreen *s = e[i].screen;
		char what[64];
		tapId(UI_ROW(i, Row));
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
		// SERVO passes the unplug page once its job has held the pins.
		for (int k = 0; k < 200 && ctlPresent(UI_FIX(ServoCtl::Restart)) == false &&
		                uiCurrentScreen() == UiScreenId::Servo; k++)
			menuFrames(1);
		if (uiCurrentScreen() == UiScreenId::Servo) tapId(UI_FIX(ServoCtl::Restart));
		for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) menuFrames(1);
		snprintf(what, sizeof(what), "BACK from %s lands on MENU", e[i].label);
		checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
	}
	tapId(UI_FIX(UI_BACK));
}

/** @brief MENU disarms on open, BEEP flashes on its row, and there is no KST row. */
static void testMenuDisarmAndBeep() {
	section("MENU: opening it disarms; BEEP's flash is on its row");
	freshTester();
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); menuFrames(2);
	checkTrue("armed on the tester", uiArmed() && fakeArmed());
	tapId(UI_FIX(TesterCtl::Hold));
	openMenu();
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("and the tester is disarmed", !uiArmed() && !fakeArmed());
	checkInt("with the throttle at 0", uiThrottle(), 0);
	checkTrue("KST has no row before slice 3", uiMenuRowOf(&UI_KST) < 0);
	checkControlTable("MENU");

	UiRect beep = ctlRect(menuRow(&UI_BEEP));
	int beeps = fakeBeepRequests();
	tapId(menuRow(&UI_BEEP));
	checkInt("BEEP sends the beacon", fakeBeepRequests(), beeps + 1);
	checkTrue("its row shows SENT on an ink fill", pixel(beep.x + 4, beep.y + beep.h - 5) == C_INK);
	fakeDumpFrame("shot_menu_beep_flash.ppm");
	menuFrames(16);
	checkTrue("400 ms later the fill is back to the panel",
	          pixel(beep.x + 4, beep.y + beep.h - 5) == C_PANEL);

	// The refusal is unreachable through MENU, which disarms; forced here.
	escSetArmed(true);
	tapId(menuRow(&UI_BEEP));
	checkTrue("refused while armed: the row shows REFUSED on an amber fill",
	          pixel(beep.x + 4, beep.y + beep.h - 5) == C_AMBER);
	fakeDumpFrame("shot_menu_beep_refused.ppm");
	escSetArmed(false);
	menuFrames(16);
	tapId(UI_FIX(UI_BACK));
}

/** @brief A pin-claiming tool is refused on MENU while the claim restores. */
static void testPinsBusy() {
	section("MENU: PINS BUSY while the claim restores");
	freshTester();
	openMenu();
	tapId(menuRow(&UI_SERVO));
	checkTrue("SERVO opens", uiCurrentScreen() == UiScreenId::Servo);
	menuFrames(4);
	checkTrue("its claim runs", pinClaimState() == ClaimState::Running);
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 200 && !ctlPresent(UI_FIX(ServoCtl::Restart)); k++) menuFrames(1);
	pressId(UI_FIX(ServoCtl::Restart));
	fakeRelease();
	// The release frame leaves for MENU. Without the pump's pass the claim
	// stays Restoring, the state RESTART DSHOT leaves it in.
	framesNoPump(2);
	bool busySeen = uiCurrentScreen() == UiScreenId::Menu && pinClaimState() != ClaimState::Free;
	checkTrue("RESTART DSHOT returns to MENU while the claim restores", busySeen);
	if (busySeen) {
		checkTrue("SERVO TESTER does not act", !ctlEnabled(menuRow(&UI_SERVO)));
		checkTrue("nor AM32 CONFIG", !ctlEnabled(menuRow(&UI_AM32)));
		checkTrue("SD LOG, which claims nothing, acts", ctlEnabled(menuRow(&UI_SDLOG)));
		fakeDumpFrame("shot_menu_pins_busy.ppm");
		UiControl c;
		ctlFind(menuRow(&UI_SERVO), &c);
		fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
		framesNoPump(2);
		fakeRelease();
		framesNoPump(1);
		checkTrue("the claim still restores", pinClaimState() == ClaimState::Restoring);
		checkTrue("a tap while busy opens nothing", uiCurrentScreen() == UiScreenId::Menu);
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
		{ "SETTINGS", 7, &UI_SETTINGS }, { "SD LOG", 10, &UI_SDLOG },
	};
	static const UiMenuEntry seven[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 7, &UI_SETTINGS }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 7, &UI_SETTINGS },
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
	fakeDumpFrame("shot_menu_seven.ppm");
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

/** @brief SETTINGS' scroll: 0 px on open, 330 px at most, from a drag on a label only. */
static void testSettingsScroll() {
	section("SETTINGS: the scroll range, and no scroll from a control");
	openSettings();
	checkTrue("SETTINGS opens", uiCurrentScreen() == UiScreenId::Settings);
	UiRect poles = ctlRect(UI_ROW(SettingsRowId::Poles, Row));
	checkInt("opened at 0 px: MOTOR POLES at y 60", poles.y, 60);
	fakeDumpFrame("shot_settings.ppm");

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
	UiRect board = ctlRect(UI_ROW(SettingsRowId::BuiltFor, Row));
	checkInt("dragged to the end: BUILT FOR at y 190, so the scroll is 330 px", board.y, 190);
	checkTrue("and is whole", board.h == 44);
	fakeDumpFrame("shot_settings_scrolled.ppm");
	dragList(60, 180);
	dragList(60, 180);
	checkInt("dragged back: MOTOR POLES at y 60 again", ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
}

/** @brief A control cut by the viewport takes a press only while 40 px show. */
static void testSettingsCutControl() {
	section("SETTINGS: a cut control takes a press only while 40 px show");
	openSettings();
	// DSHOT KBAUD's row starts at content y 190, y 230 on the panel: 10 px show.
	UiControl c;
	checkTrue("DSHOT KBAUD's + is in the table", ctlFind(UI_ROW(SettingsRowId::Speed, Plus), &c));
	checkInt("cut to 10 px", c.rect.h, 10);
	checkTrue("and does not act", !c.enabled);
	uint16_t kbaud = settings()->dshotKbaud;
	fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2); menuFrames(2);
	fakeRelease(); menuFrames(1);
	checkInt("a press on the 10 px that show changes nothing", settings()->dshotKbaud, kbaud);
	// A drag of 40 px from the label of MAX THROTTLE: the row shows whole.
	dragList(150, -40);
	checkTrue("scrolled 40 px: DSHOT KBAUD's + shows 44 px and acts",
	          ctlFind(UI_ROW(SettingsRowId::Speed, Plus), &c) && c.rect.h == 44 && c.enabled);
	tapId(UI_ROW(SettingsRowId::Speed, Plus));
	checkTrue("and a tap steps it", settings()->dshotKbaud != kbaud);
	tapId(UI_FIX(SettingsCtl::Reset));
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
	revealRow((int)SettingsRowId::Pin);
	tapId(UI_ROW(SettingsRowId::Pin, Plus));
	checkTrue("ESC PIN + moves the pin", settings()->dshotPin != before.dshotPin);
	revealRow((int)SettingsRowId::Speed);
	tapId(UI_ROW(SettingsRowId::Speed, Minus));
	checkTrue("DSHOT KBAUD - changes the rate", settings()->dshotKbaud != before.dshotKbaud);
	revealRow((int)SettingsRowId::KissPin);
	checkTrue("KISS off: KISS PIN does not act",
	          before.kissEnable || !ctlEnabled(UI_ROW(SettingsRowId::KissPin, Plus)));
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

	tapId(UI_FIX(SettingsCtl::Reset));
	checkInt("RESET restores the pole count", settings()->poles, DEFAULT_MOTOR_POLES);
	checkInt("and the ceiling", settings()->maxThrottle, DEFAULT_MAX_THROTTLE);
	checkInt("and the eRPM divisor follows", fakePoles(), DEFAULT_MOTOR_POLES);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief The note and HOLD TO SAVE do not move with the list, and the hold saves at any scroll. */
static void testSettingsSaveAnywhere() {
	section("SETTINGS: UNSAVED and HOLD TO SAVE at every scroll");
	openSettings();
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	UiRect save0 = ctlRect(UI_FIX(SettingsCtl::Save));
	checkTrue("a change: MENU's SETTINGS row will read UNSAVED", settingsDirty());
	for (int scroll = 0; scroll < 3; scroll++) {
		char what[64];
		snprintf(what, sizeof(what), "scroll step %d: HOLD TO SAVE stays at y 268", scroll);
		checkInt(what, ctlRect(UI_FIX(SettingsCtl::Save)).y, save0.y);
		dragList(200, -120);
	}
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 41);
	fakeRelease(); menuFrames(1);
	checkTrue("a 1025 ms hold saves at the end of the list", !settingsDirty());
	tapId(UI_FIX(UI_BACK));
	checkTrue("MENU: SETTINGS reads nothing once saved", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
}

void runMenuTests() {
	testSdLogBack();
	testEveryEntry();
	testMenuDisarmAndBeep();
	testPinsBusy();
	testMenuScroll();
	testSettingsScroll();
	testSettingsCutControl();
	testSettingsRows();
	testSettingsSaveAnywhere();
}
