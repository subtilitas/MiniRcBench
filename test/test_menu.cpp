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
#include "touch.h"

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
		checkTrue("nor SETTINGS, which changes the wiring the claim latched",
		          !ctlEnabled(menuRow(&UI_SETTINGS)));
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
		{ "SETTINGS", 9, &UI_SETTINGS }, { "SD LOG", 10, &UI_SDLOG },
	};
	static const UiMenuEntry seven[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 9, &UI_SETTINGS }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 9, &UI_SETTINGS },
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
	menuFrames(1);
	UiRect save0 = ctlRect(UI_FIX(SettingsCtl::Save));
	checkTrue("a change: the settings are unsaved", settingsDirty());
	// The note line, y 254, 8 px: its pixels at 0 px are the UNSAVED note.
	uint32_t note0 = fakeRegionHash(0, 254, GFX_W, 8);
	for (int scroll = 0; scroll < 3; scroll++) {
		char what[64];
		snprintf(what, sizeof(what), "scroll step %d: HOLD TO SAVE stays at y 268", scroll);
		checkInt(what, ctlRect(UI_FIX(SettingsCtl::Save)).y, save0.y);
		snprintf(what, sizeof(what), "scroll step %d: the UNSAVED note stays", scroll);
		checkTrue(what, fakeRegionHash(0, 254, GFX_W, 8) == note0);
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
	for (uint8_t i = 0; i < n; i++) {
		check(e[i], "at rest");
		if (e[i].screen->pinUser != PinUser::None || e[i].screen->freePins) {
			char what[80];
			snprintf(what, sizeof(what), "%s: PINS BUSY within its widest", e[i].label);
			checkTrue(what, strlen("PINS BUSY") <= e[i].statusWidest);
		}
	}
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
	// SETTINGS, unsaved.
	settings()->poles = (uint8_t)(settings()->poles + 2);
	check(e[uiMenuRowOf(&UI_SETTINGS)], "unsaved");
	settingsLoad();
	// The two pin readouts at the widest pins the board offers.
	settings()->dshotPin = 29;
	settings()->kissPin = 28;
	check(e[uiMenuRowOf(&UI_AM32)], "GP29");
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

/** @brief MENU opens at 0 px again when a tool returns to it. */
static void testMenuScrollResets() {
	section("MENU: a tool's return opens MENU at 0 px");
	static const UiMenuEntry seven[] = {
		{ "BEEP THE ESC", 8, &UI_BEEP }, { "AM32 CONFIG", 9, &UI_AM32 },
		{ "SERVO TESTER", 9, &UI_SERVO }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 9, &UI_SETTINGS }, { "SD LOG", 10, &UI_SDLOG },
		{ "SETTINGS", 9, &UI_SETTINGS },
	};
	freshTester();
	uiMenuUseRegistry(seven, 7);
	openMenu();
	fakePress(120, 250); menuFrames(1);
	for (int k = 1; k <= 12; k++) { fakeHold(120, 250 - 6 * k); menuFrames(1); }
	fakeRelease(); menuFrames(1);
	checkInt("scrolled 36 px: the seventh row at y 278", ctlRect(UI_ROW(6, Row)).y, 278);
	tapId(UI_ROW(6, Row));
	checkTrue("its SETTINGS opens", uiCurrentScreen() == UiScreenId::Settings);
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
 *        the other one ends. The backstop under MENU's PINS BUSY.
 */
static void testAm32WaitsBesideAClaim() {
	section("AM32 CONFIG: opened beside another claim, it waits");
	freshTester();
	checkTrue("SERVO takes the pins", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	static const TouchState none = {};
	uiAm32Enter();
	for (int i = 0; i < 10; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiAm32Tick(&none); }
	checkTrue("AM32 does not take the pins", pinClaimHolder() == PinUser::Servo);
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
	testMenuUnsaved();
	testMenuStatusWidths();
	testSettingsValueWidths();
	testMenuScrollResets();
	testAm32WaitsBesideAClaim();
}
