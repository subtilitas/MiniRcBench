/**
 * @file test_angle_ui.cpp
 * @brief TOOLS and ANGLE METER through the real UI: MENU's TOOLS row, the
 *        chooser, the meter's main view, SETUP and FLIP CAL, on the QMI8658
 *        driver and a register model of the part on the I2C fake.
 *
 * A UI frame is 25 ms, then the pump's and core1's passes and uiTick(),
 * which ticks the driver before the meter. The part latches a sample every
 * 32 ms; the model averages four into a frame. Gravity is set in the part's
 * counts, 16,384 a g, with the board's axes as `BOARD_IMU_AXES` places them:
 * chip X along the long edge, Y along the short edge, Z along the normal.
 * Nothing here is the board's behaviour: the part is the design note's
 * reading of the datasheets (docs/design/angle-meter.md, slice 2 as built).
 */

#include "check.h"
#include "fakes.h"
#include "fake_i2c.h"
#include "ui.h"
#include "ui_angle.h"
#include "ui_tools.h"
#include "ui_settings.h"
#include "settings.h"
#include "sense.h"
#include "qmi8658.h"
#include "board_pins.h"
#include "pin_claim.h"
#include "core1_job.h"
#include "esc_task.h"
#include "gfx.h"
#include "theme.h"
#include "plat.h"

#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void frames(int n) {
	for (int i = 0; i < n; i++) {
		fakeAdvance(25);
		fakePumpPoll();
		core1JobPoll();
		uiTick();
	}
}
#define UI_IDS_FRAMES(n) frames(n)
#include "ui_ids.h"

#define IMU QMI8658_I2C_ADDR
#define DEG_TO_RAD 0.017453292519943295

static FakeQmi8658 *s_q = nullptr;

/** @brief A pixel of the frame. */
static uint16_t px(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/** @brief Nothing but @p c in the rectangle. */
static bool allOf(int x, int y, int w, int h, uint16_t c) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (px(i, j) != c) return false;
	return true;
}

/**
 * @brief @p text at scale @p scale, ending at x 231, top at @p y, on the
 *        background, and the character cell before it blank: no more text
 *        to its left.
 */
static bool rightIs(int y, const char *text, uint16_t col, int scale) {
	const int x = 231 - gfxTextW(text, scale);
	const int cell = 6 * scale;
	return fakeTextIs(x, y, text, col, C_BG, scale) &&
	       (x - cell < 0 || allOf(x - cell, y, cell, 7 * scale, C_BG));
}

/** @brief @p text at scale 1 from x 8, top at @p y, on the background. */
static bool lineIs(int y, const char *text, uint16_t col) {
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, 1);
}

/** @brief Rows @p y to @p y + @p h - 1 hold no ink. */
static bool blank(int y, int h) {
	int x0, x1;
	return !fakeInkExtent(y, h, C_BG, &x0, &x1);
}

/** @brief The strip reads @p cap in @p capCol beside the chip @p chip on @p fill. */
static bool stripIs(const char *cap, uint16_t capCol, const char *chip, uint16_t fill) {
	uint16_t fg = (fill == C_AMBER || fill == C_INK) ? C_PAPER : C_ONACCENT;
	return fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, cap, capCol, C_BG, 1) &&
	       fakeTextIs(uiChipX(chip) + 6, UI_STRIP_Y + 4, chip, fg, fill, 1);
}

/**
 * @brief The chip reads @p chip on @p fill, whatever the caption; with
 *        `C_GRID`, as the neutral chip.
 */
static bool chipIs(const char *chip, uint16_t fill) {
	if (fill == C_GRID) return neutralChipIs(chip);
	uint16_t fg = (fill == C_AMBER || fill == C_INK) ? C_PAPER : C_ONACCENT;
	return fakeTextIs(uiChipX(chip) + 6, UI_STRIP_Y + 4, chip, fg, fill, 1);
}

/**
 * @brief Gravity as the part reads it with the board turned @p theta about
 *        the long edge, that edge tilted @p tilt, both in degrees: the long
 *        edge `r`, the short edge `u`, the normal `z` (ALONG AXIS `LONG`).
 */
static void pose(double theta, double tilt = 0.0) {
	double ar = sin(tilt * DEG_TO_RAD);
	double au = cos(tilt * DEG_TO_RAD) * sin(theta * DEG_TO_RAD);
	double az = cos(tilt * DEG_TO_RAD) * cos(theta * DEG_TO_RAD);
	s_q->acc[0] = (int16_t)lround(ar * 16384.0);
	s_q->acc[1] = (int16_t)lround(au * 16384.0);
	s_q->acc[2] = (int16_t)lround(az * 16384.0);
}

/** @brief The part's raw counts, set directly, in g. */
static void raw(double x, double y, double z) {
	s_q->acc[0] = (int16_t)lround(x * 16384.0);
	s_q->acc[1] = (int16_t)lround(y * 16384.0);
	s_q->acc[2] = (int16_t)lround(z * 16384.0);
}

/**
 * @brief The driver closed, an empty bus with a QMI8658 at 0x6B, the sensor
 *        at NONE, fresh settings, the tester, and 2000 ms on so no earlier
 *        reset holds the next back.
 */
static void freshBench(bool part = true) {
	qmi8658Stop();
	fakeFlashClear();
	settingsLoad();
	senseConfigure(0);
	fakeI2cReset();
	senseNote(1, 1);
	fakeAdvance(QMI8658_RESET_GAP_MS);
	s_q = part ? fakeI2cAddQmi8658(IMU) : nullptr;
	if (s_q) s_q->temp = (int16_t)(31.4 * 256.0);
	uiInit();
	frames(2);
}

/** @brief From the tester to MENU, then TOOLS. */
static void openTools() {
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_TOOLS));
}

/** @brief From the tester through MENU and TOOLS to the meter. */
static void openMeter() {
	openTools();
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
}

/** @brief The angle's text at scale 5, ending at x 231, at y 66. */
static bool angleIs(const char *text, uint16_t col) { return rightIs(66, text, col, 5); }

/**
 * @brief Frames until the meter reads settled, at most @p n, then 1 s more
 *        for the filter to come within a hundredth of a degree. The first
 *        200 ms are not looked at: a frame of four samples, 128 ms, has to
 *        carry a new pose into the model before its still time restarts.
 */
static bool settle(int n = 240) {
	frames(8);
	for (int i = 0; i < n; i++) {
		frames(1);
		AngleReading r;
		angleRead(uiAngleModel(), &r);
		if (r.settled) {
			frames(40);
			return true;
		}
	}
	return false;
}

/** @brief The reading now. */
static AngleReading reading() {
	AngleReading r;
	angleRead(uiAngleModel(), &r);
	return r;
}

// ---------------------------------------------------------------------------
// TOOLS
// ---------------------------------------------------------------------------

/**
 * @brief TOOLS (angle-meter.md decision 1, menu-b.md "MENU"): MENU's sixth
 *        row opens the chooser, which claims nothing, with ANGLE METER over
 *        its line; BACK returns to MENU.
 */
static void testTools() {
	section("TOOLS: MENU's sixth row, the chooser, ANGLE METER");
	freshBench();
	tapId(UI_FIX(TesterCtl::Menu));
	frames(1);
	const UiControlId row = menuRow(&UI_TOOLS);
	const UiRect r = ctlRect(row);
	checkTrue("MENU's sixth row reads TOOLS, 58 px at scale 2 from x 16, no status",
	          uiMenuRowOf(&UI_TOOLS) == 5 && gfxTextW(UI_TOOLS_TITLE, 2) == 58 &&
	          r.y == 272 && r.h == 40 &&
	          fakeTextIs(UI_MARGIN + 8, r.y + 13, UI_TOOLS_TITLE, C_TEXT, C_PANEL, 2) &&
	          allOf(r.x + 100, r.y + 4, r.w - 104, r.h - 8, C_PANEL));
	checkTrue("the chooser opens whatever the claim's state and claims nothing",
	          !UI_TOOLS.freePins && UI_TOOLS.pinUser == PinUser::None && !UI_TOOLS.status &&
	          UI_TOOLS.id == UiScreenId::Tools);
	checkTrue("the meter claims nothing either, and opens only while the claim is Free",
	          UI_ANGLE.pinUser == PinUser::None && UI_ANGLE.freePins && !UI_ANGLE.status);
	tapId(row);
	checkTrue("TOOLS opens the chooser", uiCurrentScreen() == UiScreenId::Tools);
	frames(4);
	checkTrue("it claims nothing: DShot runs on SIG", pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended());
	checkTrue("nothing is sent to 0x6B", fakeI2cTransactionsTo(IMU) == 0);
	checkTrue("the title reads TOOLS",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_TOOLS_TITLE, C_TEXT, C_PANEL, 2));
	{
		char strip[32];
		snprintf(strip, sizeof(strip), "DSHOT RUNS ON SIG GP%u", (unsigned)settings()->dshotPin);
		checkTrue("the strip: DSHOT RUNS ON SIG GP0, dim, 125 px",
		          lineIs(UI_STRIP_Y + 4, strip, C_DIM) && gfxTextW(strip, 1) == 125);
	}
	checkInt("one entry", UI_TOOLS_ENTRIES_COUNT, 1);
	const UiRect a = ctlRect(UI_FIX(ToolsCtl::Angle));
	checkTrue("ANGLE METER: 228 x 46 px at (6, 62)", a.x == 6 && a.y == 62 && a.w == 228 && a.h == 46);
	checkTrue("it acts, its label C_TEXT on the panel, centred at scale 2, 130 px",
	          ctlEnabled(UI_FIX(ToolsCtl::Angle)) && gfxTextW(UI_ANGLE_TITLE, 2) == 130 &&
	          fakeTextIs(a.x + (a.w - 130) / 2, a.y + 16, UI_ANGLE_TITLE, C_TEXT, C_PANEL, 2));
	checkTrue("its line at y 112, dim, 215 px",
	          lineIs(112, "INCIDENCE, DECALAGE, DIHEDRAL, THROW", C_DIM) &&
	          gfxTextW("INCIDENCE, DECALAGE, DIHEDRAL, THROW", 1) == 215);
	checkTrue("under it nothing", blank(124, 320 - 124));
	checkControlTable("TOOLS");
	fakeDumpFrame("shot_tools_chooser.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("still nothing sent to 0x6B", fakeI2cTransactionsTo(IMU) == 0);
	tapId(UI_FIX(UI_BACK));
}

// ---------------------------------------------------------------------------
// opening and BACK
// ---------------------------------------------------------------------------

/**
 * @brief The meter opens the driver and BACK closes it: CTRL7 00h, nothing
 *        to 0x6B after it, MENU up; the next visit starts unzeroed.
 */
static void testOpenAndBack() {
	section("ANGLE METER: opened from TOOLS, BACK writes CTRL7 00h");
	freshBench();
	frames(40);
	checkInt("closed: nothing to 0x6B in a second", fakeI2cTransactionsTo(IMU), 0);
	openMeter();
	checkTrue("ANGLE METER opens", uiCurrentScreen() == UiScreenId::Angle &&
	          uiAngleView() == AngleView::Main);
	checkTrue("from TOOLS: no claim, DShot runs", pinClaimState() == ClaimState::Free);
	checkTrue("the title reads ANGLE METER, 4 px or more before BACK",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_ANGLE_TITLE, C_TEXT, C_PANEL, 2) &&
	          UI_MARGIN + gfxTextW(UI_ANGLE_TITLE, 2) + 4 <= UI_BACK_X);
	frames(1);
	checkTrue("the driver opened: a reset written", s_q->resets == 1);
	checkTrue("STARTING while the part starts", chipIs("STARTING", C_AMBER));
	checkTrue("the angle --- in grey", angleIs("---", C_DIM));
	frames(20);
	checkTrue("online, CTRL7 01h", qmi8658State() == ImuState::Online && s_q->reg[0x08] == 0x01);
	checkTrue("samples reach the model", qmi8658Counts()->samples > 4 && reading().valid);
	checkControlTable("ANGLE METER");
	tapId(UI_FIX(AngleCtl::Zero));
	settle();
	frames(60);
	checkTrue("zeroed on this visit", reading().zeroed);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK returns to TOOLS", uiCurrentScreen() == UiScreenId::Tools);
	tapId(UI_FIX(UI_BACK));  // to MENU
	checkTrue("CTRL7 00h: the accelerometer off", s_q->reg[0x08] == 0x00 &&
	          qmi8658State() == ImuState::Closed);
	uint32_t t = fakeI2cTransactionsTo(IMU);
	frames(80);
	checkInt("nothing to 0x6B after BACK, in two seconds", fakeI2cTransactionsTo(IMU) - t, 0);
	tapId(menuRow(&UI_TOOLS));
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("a new visit: no zero, no MAX", !reading().zeroed && !reading().hasMax[0] &&
	          !reading().hasMax[1] && reading().zero == AngleZero::None);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

// ---------------------------------------------------------------------------
// the reading
// ---------------------------------------------------------------------------

/** @brief The angle, the strip, the chip and the caption from the part's gravity. */
static void testReading() {
	section("ANGLE METER: the angle, the axis's tilt, SETTLED and MOVING");
	freshBench();
	pose(0.0);
	openMeter();
	checkTrue("settled flat", settle());
	checkTrue("+0.0` in lime, scale 5, ending at x 231", angleIs("+0.0`", C_LIME));
	checkTrue("the strip: LONG EDGE  AXIS TILT 0`, SETTLED on green",
	          stripIs("LONG EDGE  AXIS TILT 0`", C_DIM, "SETTLED", C_GREEN));
	checkTrue("NOT ZEROED: FROM LEVEL, dim, at y 108", lineIs(108, UI_ANGLE_NOT_ZEROED, C_DIM));
	checkTrue("LENGTH OFF: SET LENGTH IN SETUP FOR MM, no travel",
	          lineIs(148, UI_ANGLE_LEN_OFF, C_DIM) && blank(122, 22));
	checkTrue("MAX: --  --, the caption at y 170",
	          lineIs(170, "MAX", C_DIM) && rightIs(166, "--  --", C_TEXT, 2));
	checkTrue("the IMU's temperature, 31.4`C, dim", lineIs(204, "IMU 31.4`C", C_DIM));
	checkTrue("no hint", blank(252, 8));
	checkTrue("ZERO, RESET MAX and SETUP at 6, 84 and 162, 72 x 46",
	          ctlRect(UI_FIX(AngleCtl::Zero)).x == 6 && ctlRect(UI_FIX(AngleCtl::ResetMax)).x == 84 &&
	          ctlRect(UI_FIX(AngleCtl::Setup)).x == 162 && ctlRect(UI_FIX(AngleCtl::Setup)).w == 72 &&
	          ctlRect(UI_FIX(AngleCtl::Zero)).y == 268 && ctlRect(UI_FIX(AngleCtl::Zero)).h == 46);
	{
		const UiRect z = ctlRect(UI_FIX(AngleCtl::Zero)), m = ctlRect(UI_FIX(AngleCtl::ResetMax));
		checkTrue("ZERO at scale 2 and RESET MAX at scale 1, inside their buttons",
		          fakeTextIs(z.x + (z.w - gfxTextW("ZERO", 2)) / 2, z.y + 16, "ZERO", C_TEXT, C_PANEL, 2) &&
		          fakeTextIs(m.x + (m.w - gfxTextW("RESET MAX", 1)) / 2, m.y + 19, "RESET MAX", C_TEXT,
		                     C_PANEL, 1) &&
		          gfxTextW("RESET MAX", 1) + 8 <= m.w);
	}
	checkControlTable("ANGLE METER, settled");

	pose(12.4);
	frames(8);
	checkTrue("turned: MOVING, the neutral chip: frame, C_BG inside, C_DIM label", chipIs("MOVING", C_GRID));
	checkTrue("and no label in the ink of a fill",
	          !fakeTextIs(uiChipX("MOVING") + 6, UI_STRIP_Y + 4, "MOVING", C_ONACCENT, C_GRID, 1));
	checkTrue("settles again", settle());
	checkTrue("+12.4`", angleIs("+12.4`", C_LIME));
	pose(-7.25);
	settle();
	frames(40);
	checkTrue("-7.3`: tenths rounded half away from zero", angleIs("-7.3`", C_LIME));

	// The axis's tilt: amber from 45°, TOO STEEP from 70°.
	pose(3.0, 44.9);
	settle();
	frames(40);
	checkTrue("tilt 44.9`: 44`, dim", stripIs("LONG EDGE  AXIS TILT 44`", C_DIM, "SETTLED", C_GREEN));
	pose(3.0, 45.1);
	settle();
	frames(40);
	checkTrue("tilt 45.1`: 45`, amber", stripIs("LONG EDGE  AXIS TILT 45`", C_AMBER, "SETTLED", C_GREEN));
	pose(3.0, -45.1);
	settle();
	frames(160);   // the tilt alone moved: the filter's tilt comes after the still time
	checkTrue("tilt -45.1`, the other end up: 45`, amber",
	          stripIs("LONG EDGE  AXIS TILT 45`", C_AMBER, "SETTLED", C_GREEN));
	checkTrue("the angle still reads, its change the rotation", angleIs("+3.0`", C_LIME));
	pose(3.0, 69.9);
	settle();
	frames(40);
	checkTrue("tilt 69.9`: the angle shows", angleIs("+3.0`", C_LIME) && chipIs("SETTLED", C_GREEN));
	pose(3.0, 70.1);
	frames(60);
	checkTrue("tilt 70.1`: ---, TOO STEEP on amber",
	          angleIs("---", C_DIM) && stripIs("LONG EDGE  AXIS TILT 70`", C_AMBER, "TOO STEEP", C_AMBER));
	checkTrue("and TURN THE AXIS NEARER LEVEL. under it, amber", lineIs(108, UI_ANGLE_STEEP_LINE, C_AMBER));
	checkTrue("ZERO still acts: it takes no zero while too steep", ctlEnabled(UI_FIX(AngleCtl::Zero)));
	{
		// The widest strip: SHORT EDGE, a tilt of two digits and TOO STEEP,
		// 10 px or more apart.
		int x0, x1;
		fakeInkExtent(UI_STRIP_Y + 4, 7, C_BG, &x0, &x1);
		checkTrue("the strip's caption ends 10 px or more before the chip",
		          gfxTextW("SHORT EDGE  AXIS TILT 90`", 1) + UI_MARGIN + 10 <= uiChipX("TOO STEEP"));
	}
	fakeDumpFrame("shot_angle_steep.ppm");
	pose(0.0);
	settle();

	// No new sample for 300 ms: the reading is stale.
	s_q->reg[0x08] = 0x00;
	frames(21);
	checkTrue("stale: ---, STARTING on amber", angleIs("---", C_DIM) && chipIs("STARTING", C_AMBER) &&
	          stripIs("LONG EDGE  AXIS TILT --", C_DIM, "STARTING", C_AMBER));
	checkTrue("the tilt reads -- without a reading", !reading().valid);
	s_q->reg[0x08] = 0x01;
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief No part, another part, a part that stops answering. */
static void testNoImu() {
	section("ANGLE METER: NO IMU, another part, a part lost");
	freshBench(false);
	openMeter();
	frames(10);
	checkTrue("no part: NO IMU on amber", chipIs("NO IMU", C_AMBER));
	checkTrue("NO ANSWER FROM THE IMU AT 0X6B., amber, at y 80, in place of the angle",
	          lineIs(80, UI_ANGLE_NO_ANSWER, C_AMBER) && blank(66, 14) && blank(88, 14));
	checkTrue("ZERO is drawn disabled and does not act", !ctlEnabled(UI_FIX(AngleCtl::Zero)) &&
	          px(6 + 4, 268 + 23) == C_BG);
	tapId(UI_FIX(AngleCtl::Zero), IdExpect::Disabled);
	checkTrue("a tap arms nothing", reading().zero == AngleZero::None && blank(252, 8));
	checkTrue("RESET MAX and SETUP act", ctlEnabled(UI_FIX(AngleCtl::ResetMax)) &&
	          ctlEnabled(UI_FIX(AngleCtl::Setup)));
	checkControlTable("ANGLE METER, no IMU");
	fakeDumpFrame("shot_angle_no_imu.ppm");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));

	freshBench();
	s_q->whoAmI = 0x3C;
	s_q->reg[0x00] = 0x3C;
	openMeter();
	frames(4);
	checkTrue("another part: NOT A QMI8658: ID 0X3C., amber, NO IMU",
	          lineIs(80, "NOT A QMI8658: ID 0X3C.", C_AMBER) && chipIs("NO IMU", C_AMBER));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));

	freshBench();
	pose(5.0);
	openMeter();
	settle();
	checkTrue("reading", angleIs("+5.0`", C_LIME));
	fakeI2cRemove(IMU);
	frames(4);
	checkTrue("the part gone: offline after three failures, NO IMU and the line",
	          qmi8658State() == ImuState::Offline && chipIs("NO IMU", C_AMBER) &&
	          lineIs(80, UI_ANGLE_NO_ANSWER, C_AMBER));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK leaves", uiCurrentScreen() == UiScreenId::Tools);
	tapId(UI_FIX(UI_BACK));  // to MENU
	tapId(UI_FIX(UI_BACK));
}

// ---------------------------------------------------------------------------
// ZERO, MAX, the travel, the temperature
// ---------------------------------------------------------------------------

/** @brief ZERO armed until settled, its 10 s limit; MAX and RESET MAX. */
static void testZeroAndMax() {
	section("ANGLE METER: ZERO when settled, its 10 s limit, MAX, RESET MAX");
	freshBench();
	pose(4.0);
	openMeter();
	settle();
	tapId(UI_FIX(AngleCtl::Zero));
	checkTrue("ZERO armed: the hint, amber, at y 252",
	          reading().zero == AngleZero::Armed && lineIs(252, UI_ANGLE_HINT_ARMED, C_AMBER));
	frames(36);
	checkTrue("900 ms after the lift: not taken yet", reading().zero == AngleZero::Armed);
	frames(20);
	checkTrue("taken from a settled reading a second after the lift",
	          reading().zero == AngleZero::Taken && reading().zeroed);
	checkTrue("+0.0`, FROM ZERO, no hint", angleIs("+0.0`", C_LIME) &&
	          lineIs(108, UI_ANGLE_FROM_ZERO, C_DIM) && blank(252, 8));
	checkTrue("the temperature since the zero: +0.0`C",
	          lineIs(204, "IMU 31.4`C, +0.0`C SINCE ZERO", C_DIM));

	pose(24.0);
	settle();
	frames(40);
	checkTrue("+20.0` from the zero", angleIs("+20.0`", C_LIME));
	checkTrue("MAX +20.0` this side, -- the other", rightIs(166, "--  +20.0`", C_TEXT, 2));
	pose(-11.0);
	settle();
	frames(40);
	checkTrue("-15.0` and its MAX", angleIs("-15.0`", C_LIME) && rightIs(166, "-15.0`  +20.0`", C_TEXT, 2));
	// A knock passes through without settling: MAX does not take it.
	pose(40.0);
	frames(3);
	pose(-11.0);
	settle();
	frames(40);
	checkTrue("a knock to +36` that never settled: MAX unchanged", rightIs(166, "-15.0`  +20.0`", C_TEXT, 2));
	tapId(UI_FIX(AngleCtl::ResetMax));
	checkTrue("RESET MAX: --  -- on the tap's frame", rightIs(166, "--  --", C_TEXT, 2));
	frames(8);
	checkTrue("then the settled side taken again, the other side gone",
	          !reading().hasMax[1] && rightIs(166, "-15.0`  --", C_TEXT, 2));

	// The temperature since the zero: amber from 2.0 °C.
	s_q->temp = (int16_t)lround((31.4 + 1.9) * 256.0);
	frames(20);
	checkTrue("+1.9`C since the zero: dim", lineIs(204, "IMU 33.3`C, +1.9`C SINCE ZERO", C_DIM));
	s_q->temp = (int16_t)lround((31.4 + 2.0) * 256.0) + 1;
	frames(20);
	checkTrue("+2.0`C: amber", lineIs(204, "IMU 33.4`C, +2.0`C SINCE ZERO", C_AMBER));
	s_q->temp = (int16_t)(31.4 * 256.0);
	frames(20);

	// ZERO with the board never still: none within 10 s, the old zero kept.
	tapId(UI_FIX(AngleCtl::Zero));
	double th = 0.0;
	for (int i = 0; i < 396; i++) {
		th += 1.0;
		pose(th);
		frames(1);
	}
	checkTrue("9.9 s moving: still armed", reading().zero == AngleZero::Armed);
	for (int i = 0; i < 8; i++) {
		th += 1.0;
		pose(th);
		frames(1);
	}
	checkTrue("10 s: NOT SETTLED IN 10 S: NO ZERO, amber",
	          reading().zero == AngleZero::TimedOut && lineIs(252, UI_ANGLE_HINT_TIMEOUT, C_AMBER));
	pose(4.0);
	settle();
	frames(40);
	checkTrue("the old zero kept: +0.0` back at 4`", angleIs("+0.0`", C_LIME));
	fakeDumpFrame("shot_angle_zero_timeout.ppm");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------

/** @brief SETUP's row @p i, label and caption, from x 8. */
static bool setupRowIs(int i, const char *label, const char *cap, const char *value) {
	int y = 40 + 50 * i;
	const UiRect t = { 138, (int16_t)y, 94, 44 };
	bool l = cap ? lineIs(y + 12, label, C_DIM) && lineIs(y + 25, cap, C_DIM)
	             : lineIs(y + 18, label, C_DIM);
	return l && fakeTextIs(t.x + (t.w - gfxTextW(value, 2)) / 2, y + 15, value, C_TEXT, C_PANEL, 2);
}

/** @brief SETUP's rows, LENGTH's steps, the save on BACK; the IMU runs on. */
static void testSetup() {
	section("ANGLE SETUP: ALONG AXIS, LENGTH, SIGN, CALIBRATION, one save on BACK");
	freshBench();
	pose(10.0);
	openMeter();
	settle();
	tapId(UI_FIX(AngleCtl::Zero));
	frames(60);
	pose(30.0);
	settle();
	frames(40);
	checkTrue("zeroed, +20.0`, MAX +20.0`", reading().zeroed && angleIs("+20.0`", C_LIME));
	uint32_t samples = qmi8658Counts()->samples;
	int writes = fakeFlashWrites();
	tapId(UI_FIX(AngleCtl::Setup));
	checkTrue("SETUP opens", uiAngleView() == AngleView::Setup && uiCurrentScreen() == UiScreenId::Angle);
	checkTrue("its title, ANGLE SETUP",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_ANGLE_SETUP_TITLE, C_TEXT, C_PANEL, 2));
	checkTrue("ALONG AXIS: LONG, LONG EDGE ON AXIS", setupRowIs(0, "ALONG AXIS", "LONG EDGE ON AXIS", "LONG"));
	checkTrue("LENGTH, MM: OFF at scale 2 ending at x 132, HINGE TO EDGE under it",
	          lineIs(90 + 18, "LENGTH, MM", C_DIM) &&
	          fakeTextIs(132 - gfxTextW("OFF", 2), 98, "OFF", C_TEXT, C_BG, 2) &&
	          fakeTextIs(132 - gfxTextW("HINGE TO EDGE", 1), 117, "HINGE TO EDGE", C_DIM, C_BG, 1));
	checkTrue("SIGN: NORMAL", setupRowIs(2, "SIGN", nullptr, "NORMAL"));
	checkTrue("CALIBRATION: FLIP, NOT CALIBRATED", setupRowIs(3, "CALIBRATION", UI_ANGLE_CAL_NO, "FLIP"));
	checkTrue("the rects: toggles 94 x 44 from x 138, - and + 44 x 44 at 138 and 188",
	          ctlRect(UI_FIX(AngleCtl::Along)).x == 138 && ctlRect(UI_FIX(AngleCtl::Along)).w == 94 &&
	          ctlRect(UI_FIX(AngleCtl::LenMinus)).x == 138 && ctlRect(UI_FIX(AngleCtl::LenPlus)).x == 188 &&
	          ctlRect(UI_FIX(AngleCtl::LenPlus)).w == 44 && ctlRect(UI_FIX(AngleCtl::Flip)).y == 190);
	checkControlTable("ANGLE SETUP");
	frames(40);
	checkTrue("the IMU runs on: samples taken, CTRL7 still 01h",
	          qmi8658Counts()->samples > samples && s_q->reg[0x08] == 0x01);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK with nothing changed: the main view, nothing written",
	          uiAngleView() == AngleView::Main && fakeFlashWrites() == writes);
	checkTrue("ZERO and MAX kept", reading().zeroed && reading().hasMax[1] && angleIs("+20.0`", C_LIME));

	// LENGTH: + from OFF is 10 mm; - from 10 is OFF; held, it repeats to 500.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::LenPlus));
	checkTrue("+ from OFF: 10", settings()->angleLengthMm == 10 &&
	          fakeTextIs(132 - gfxTextW("10", 2), 98, "10", C_TEXT, C_BG, 2));
	tapId(UI_FIX(AngleCtl::LenMinus));
	checkTrue("- from 10: OFF", settings()->angleLengthMm == 0);
	tapId(UI_FIX(AngleCtl::LenMinus));
	checkTrue("- at OFF: OFF", settings()->angleLengthMm == 0);
	pressId(UI_FIX(AngleCtl::LenPlus));
	holdId(UI_FIX(AngleCtl::LenPlus), 800);
	fakeRelease();
	frames(1);
	checkTrue("held: up to 500 and no further", settings()->angleLengthMm == 500 &&
	          fakeTextIs(132 - gfxTextW("500", 2), 98, "500", C_TEXT, C_BG, 2));
	tapId(UI_FIX(AngleCtl::LenPlus));
	checkInt("+ at 500: 500", settings()->angleLengthMm, 500);
	checkTrue("nothing written while stepping", fakeFlashWrites() == writes);
	for (int i = 0; i < 460; i++) {
		if (settings()->angleLengthMm == 40) break;
		tapId(UI_FIX(AngleCtl::LenMinus));
	}
	checkInt("stepped down to 40", settings()->angleLengthMm, 40);

	// SIGN: the reading turns, ZERO and MAX stay.
	tapId(UI_FIX(AngleCtl::Sign));
	checkTrue("SIGN: INVERT", setupRowIs(2, "SIGN", nullptr, "INVERT") && settings()->angleInvert == 1);
	fakeDumpFrame("shot_angle_setup.ppm");
	tapId(UI_FIX(UI_BACK));
	checkInt("BACK after changes: one write", fakeFlashWrites() - writes, 1);
	checkTrue("flash holds LENGTH 40 and INVERT",
	          [] { SettingsBlock b; memcpy(&b, fakeFlashBytes(), sizeof(b));
	               return b.s.angleLengthMm == 40 && b.s.angleInvert == 1; }());
	checkTrue("the main view: -20.0`, the zero kept, MAX swapped",
	          angleIs("-20.0`", C_LIME) && rightIs(166, "-20.0`  --", C_TEXT, 2));
	checkTrue("L 40 MM, HINGE TO TRAILING EDGE", lineIs(148, "L 40 MM, HINGE TO TRAILING EDGE", C_DIM));
	checkTrue("the travel: 40 x sin 20` = -13.7 MM, scale 3", rightIs(122, "-13.7 MM", C_LIME, 3));
	checkTrue("and MAX's: -13.7 MM  --", rightIs(186, "-13.7 MM  --", C_DIM, 1));
	fakeDumpFrame("shot_angle_main.ppm");
	pose(30.0, 70.1);
	frames(160);
	checkTrue("too steep with LENGTH set: --- MM, dim", rightIs(122, "--- MM", C_DIM, 3) &&
	          angleIs("---", C_DIM));
	pose(30.0);
	settle();
	checkTrue("level again: -20.0`", angleIs("-20.0`", C_LIME));

	// LENGTH alone: a change of it is a change, saved on BACK.
	writes = fakeFlashWrites();
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::LenPlus));
	tapId(UI_FIX(UI_BACK));
	checkTrue("LENGTH 41 alone: one write", fakeFlashWrites() - writes == 1 &&
	          [] { SettingsBlock b; memcpy(&b, fakeFlashBytes(), sizeof(b));
	               return b.s.angleLengthMm == 41; }());
	writes = fakeFlashWrites();
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::LenMinus));
	tapId(UI_FIX(UI_BACK));
	checkTrue("and back to 40: one write", fakeFlashWrites() - writes == 1);

	// ALONG AXIS: a new axis drops the zero and MAX.
	writes = fakeFlashWrites();
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Along));
	checkTrue("ALONG AXIS: SHORT, SHORT EDGE ON AXIS",
	          setupRowIs(0, "ALONG AXIS", "SHORT EDGE ON AXIS", "SHORT") && settings()->angleAlong == 1);
	pose(30.5);
	tapId(UI_FIX(UI_BACK));
	checkTrue("one write, and the zero and MAX gone",
	          fakeFlashWrites() - writes == 1 && !reading().zeroed && !reading().hasMax[0] &&
	          !reading().hasMax[1]);
	settle();
	frames(160);
	checkTrue("the strip names the short edge, tilted 30` by the 30.5` turn about the long one",
	          stripIs("SHORT EDGE  AXIS TILT 30`", C_DIM, "SETTLED", C_GREEN));
	// A failed save: the values stay live, the hint says so.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Along));
	fakeFlashSetWritable(false);
	tapId(UI_FIX(UI_BACK));
	fakeFlashSetWritable(true);
	checkTrue("a save that fails: SETUP NOT SAVED TO FLASH., amber, the value live",
	          lineIs(252, UI_ANGLE_HINT_UNSAVED, C_AMBER) && settings()->angleAlong == 0 &&
	          uiAngleModel()->set.along == AngleAlong::Long);
	tapId(UI_FIX(UI_BACK));
	checkTrue("CTRL7 00h on BACK from the main view, which returns to TOOLS",
	          s_q->reg[0x08] == 0x00 && uiCurrentScreen() == UiScreenId::Tools);
	// The next visit takes the setup from the live settings.
	tapId(UI_FIX(ToolsCtl::Angle));
	checkTrue("the next visit: LENGTH 40, INVERT, LONG", uiAngleModel()->set.lengthMm == 40 &&
	          uiAngleModel()->set.invert && uiAngleModel()->set.along == AngleAlong::Long);
	tapId(UI_FIX(UI_BACK));
	backToTester();
	settingsLoad();
	checkTrue("a restart: what flash holds, SHORT, the failed save's LONG lost",
	          settings()->angleAlong == 1 && settings()->angleLengthMm == 40);
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_TOOLS));
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("the visit after it opens on SHORT, LENGTH 40, INVERT",
	          uiAngleModel()->set.along == AngleAlong::Short && uiAngleModel()->set.lengthMm == 40 &&
	          uiAngleModel()->set.invert);
	settle();
	checkTrue("the strip names the short edge", fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "SHORT EDGE  AXIS TILT ",
	                                                      C_DIM, C_BG, 1));
	checkTrue("flash holds the fields: no hint", !settingsAngleDirty() && blank(252, 8));

	// A failed save is tried again: the hint stays while the fields differ
	// from flash, across a new visit, and SETUP's BACK with no edit saves.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Along));
	fakeFlashSetWritable(false);
	tapId(UI_FIX(UI_BACK));
	fakeFlashSetWritable(true);
	checkTrue("LONG not saved: the hint", lineIs(252, UI_ANGLE_HINT_UNSAVED, C_AMBER) && settingsAngleDirty());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("a new visit: still LONG, the hint still there",
	          uiAngleModel()->set.along == AngleAlong::Long && lineIs(252, UI_ANGLE_HINT_UNSAVED, C_AMBER));
	int writes2 = fakeFlashWrites();
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(UI_BACK));
	checkTrue("SETUP's BACK with no edit tries again: written, the hint gone",
	          fakeFlashWrites() - writes2 == 1 && !settingsAngleDirty() && blank(252, 8) &&
	          [] { SettingsBlock b; memcpy(&b, fakeFlashBytes(), sizeof(b)); return b.s.angleAlong == 0; }());
	writes2 = fakeFlashWrites();
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(UI_BACK));
	checkTrue("and once saved, BACK writes nothing", fakeFlashWrites() == writes2);

	// SETTINGS' HOLD TO SAVE stores them too.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Along));
	fakeFlashSetWritable(false);
	tapId(UI_FIX(UI_BACK));
	fakeFlashSetWritable(true);
	checkTrue("SHORT not saved", settingsAngleDirty());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));                  // TOOLS to MENU
	tapId(menuRow(&UI_SETTINGS));
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 41);
	fakeRelease();
	frames(1);
	checkTrue("SETTINGS' HOLD TO SAVE stores the meter's fields",
	          !settingsAngleDirty() &&
	          [] { SettingsBlock b; memcpy(&b, fakeFlashBytes(), sizeof(b)); return b.s.angleAlong == 1; }());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/**
 * @brief SETTINGS' HOLD DEFAULTS keeps the meter's fields, which SETTINGS
 *        does not show: DEFAULTS left unsaved, then a save of the meter,
 *        leaves the stored calibration as it was.
 */
static void testDefaultsKeepMeter() {
	section("ANGLE METER: SETTINGS' HOLD DEFAULTS keeps the meter's setup and calibration");
	freshBench();
	Settings c;
	settingsDefaults(&c);
	c.poles = 22;
	c.angleAlong = 1;
	c.angleInvert = 1;
	c.angleLengthMm = 40;
	c.angleCalValid = 1;
	c.angleCalOffG[0] = 0.03f;
	c.angleCalOffG[2] = 0.04f;
	c.angleCalGain[2] = 1.02f;
	*settings() = c;
	checkTrue("a calibrated setup saved", settingsSave());
	settingsLoad();
	uiInit();
	frames(2);
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_SETTINGS));
	pressId(UI_FIX(SettingsCtl::Defaults));
	holdId(UI_FIX(SettingsCtl::Defaults), 41);
	fakeRelease();
	frames(1);
	Settings d;
	settingsDefaults(&d);
	checkTrue("HOLD DEFAULTS: the pole count back to its default",
	          settings()->poles == d.poles && settingsDirty());
	checkTrue("the meter's fields kept", settings()->angleAlong == 1 && settings()->angleInvert == 1 &&
	          settings()->angleLengthMm == 40 && settings()->angleCalValid == 1 &&
	          settings()->angleCalOffG[0] == 0.03f && settings()->angleCalGain[2] == 1.02f &&
	          !settingsAngleDirty());
	tapId(UI_FIX(UI_BACK));
	tapId(menuRow(&UI_TOOLS));
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("the meter opens calibrated", angleCalValid(&uiAngleModel()->set.cal));
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::LenPlus));
	tapId(UI_FIX(UI_BACK));
	SettingsBlock b;
	memcpy(&b, fakeFlashBytes(), sizeof(b));
	checkTrue("the meter's save: LENGTH 41, the calibration stored as it was",
	          b.s.angleLengthMm == 41 && b.s.angleCalValid == 1 && b.s.angleCalOffG[0] == 0.03f &&
	          b.s.angleCalOffG[2] == 0.04f && b.s.angleCalGain[2] == 1.02f);
	checkTrue("and SETTINGS' defaults not saved by it: the pole count stored 22",
	          b.s.poles == 22 && settingsDirty());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	settingsLoad();
	checkTrue("a restart: the calibration and the pole count as stored",
	          settings()->angleCalValid == 1 && settings()->angleCalOffG[0] == 0.03f && settings()->poles == 22);
}

/** @brief Only BACK on the main view leaves the meter; SETUP's and FLIP CAL's stay in it. */
static void testOnlyExit() {
	section("ANGLE METER: only BACK on the main view leaves it");
	freshBench();
	openMeter();
	frames(20);
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(UI_BACK));
	checkTrue("SETUP's BACK: still the meter, the driver online",
	          uiCurrentScreen() == UiScreenId::Angle && uiAngleView() == AngleView::Main &&
	          qmi8658State() == ImuState::Online && s_q->reg[0x08] == 0x01);
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Flip));
	tapId(UI_FIX(UI_BACK));
	checkTrue("FLIP CAL's BACK: still the meter, on SETUP", uiCurrentScreen() == UiScreenId::Angle &&
	          uiAngleView() == AngleView::Setup && qmi8658State() == ImuState::Online);
	tapId(UI_FIX(UI_BACK));
	for (AngleCtl c : { AngleCtl::Zero, AngleCtl::ResetMax }) {
		tapId(UI_FIX(c));
		checkTrue("ZERO and RESET MAX stay on the main view",
		          uiCurrentScreen() == UiScreenId::Angle && uiAngleView() == AngleView::Main);
	}
	frames(480);
	checkTrue("12 s on: still the meter, still reading", uiCurrentScreen() == UiScreenId::Angle &&
	          qmi8658State() == ImuState::Online);
	tapId(UI_FIX(UI_BACK));
	checkTrue("the main view's BACK leaves, CTRL7 00h", uiCurrentScreen() == UiScreenId::Tools &&
	          qmi8658State() == ImuState::Closed && s_q->reg[0x08] == 0x00);
	tapId(UI_FIX(UI_BACK));  // to MENU
	tapId(UI_FIX(UI_BACK));
}

// ---------------------------------------------------------------------------
// FLIP CAL
// ---------------------------------------------------------------------------

/** @brief The flip's lines at y 62 and 74. */
static bool flipLines(const char *a, const char *b) {
	return lineIs(62, a, C_TEXT) && (b ? lineIs(74, b, C_TEXT) : blank(74, 8));
}

/** @brief The part's offsets, g, and the normal's gain, for the flip. */
static const double OX = 0.03, OY = -0.02, OZ = 0.04, KZ = 1.02;

/** @brief Run the flip to its result on a level surface, X's offset @p ox. */
static void flipToResult(double ox = OX) {
	raw(ox, OY, KZ + OZ);                  // P1, screen up
	frames(140);
	raw(0.4, 0.1, 0.9);                    // lifted
	frames(10);
	raw(ox, OY, KZ + OZ);                  // P2, turned: on a level surface the same
	frames(140);
	raw(ox, OY, -KZ + OZ);                 // P3, screen down
	frames(140);
	raw(ox, OY, KZ + OZ);                  // up again
	frames(10);
}

static void testFlip() {
	section("FLIP CAL: the steps, touches ignored screen down, HOLD TO KEEP at 1000 ms");
	freshBench();
	raw(OX, OY, KZ + OZ);
	openMeter();
	settle();
	tapId(UI_FIX(AngleCtl::Zero));
	frames(60);
	checkTrue("zeroed before the flip", reading().zeroed);
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Flip));
	checkTrue("FLIP CAL opens", uiAngleView() == AngleView::Flip &&
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, UI_ANGLE_FLIP_TITLE, C_TEXT, C_PANEL, 2));
	checkTrue("STEP 1 OF 3 on the strip, its lines",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "STEP 1 OF 3", C_DIM, C_BG, 1) &&
	          flipLines("LAY THE BOARD ON A FLAT SURFACE,", "SCREEN UP. HOLD STILL."));
	checkTrue("HOLD TO KEEP drawn disabled, AGAIN acts",
	          !ctlEnabled(UI_FIX(AngleCtl::Keep)) && ctlEnabled(UI_FIX(AngleCtl::Again)));
	checkTrue("HOLD TO KEEP 150 px from x 6, AGAIN 72 px from x 162",
	          ctlRect(UI_FIX(AngleCtl::Keep)).w == 150 && ctlRect(UI_FIX(AngleCtl::Again)).x == 162 &&
	          gfxTextW("HOLD TO KEEP", 2) == 142);
	checkControlTable("FLIP CAL");
	frames(140);
	checkTrue("P1 taken: STEP 2 OF 3", uiAngleModel()->flip.step == FlipStep::Turn &&
	          flipLines("TURN IT ROUND, SCREEN UP, SAME SPOT.", nullptr));
	raw(0.4, 0.1, 0.9);
	frames(10);
	raw(OX, OY, KZ + OZ);
	frames(140);
	checkTrue("P2 taken: STEP 3 OF 3", uiAngleModel()->flip.step == FlipStep::Down &&
	          flipLines("TURN IT SCREEN DOWN, WAIT 3 S,", "THEN SCREEN UP AGAIN."));
	raw(OX, OY, -KZ + OZ);
	frames(10);
	checkTrue("screen down: BACK and AGAIN drawn disabled, reported disabled",
	          reading().faceDown && !ctlEnabled(UI_FIX(UI_BACK)) && !ctlEnabled(UI_FIX(AngleCtl::Again)) &&
	          backLook() == BackLook::Disabled);
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	tapId(UI_FIX(AngleCtl::Again), IdExpect::Disabled);
	checkTrue("a tap on either does nothing", uiAngleView() == AngleView::Flip &&
	          uiAngleModel()->flip.step != FlipStep::Up);
	frames(140);
	checkTrue("P3 taken", uiAngleModel()->flip.step == FlipStep::Back);
	raw(OX, OY, KZ + OZ);
	frames(10);
	checkTrue("up again: the result", uiAngleModel()->flip.step == FlipStep::Done);
	checkTrue("RESULT, the offsets in mg and the normal's gain",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "RESULT", C_DIM, C_BG, 1) &&
	          flipLines("OFFSET X 30  Y -20  Z 40 MG", "GAIN Z 1.020"));
	checkTrue("HOLD TO KEEP acts", ctlEnabled(UI_FIX(AngleCtl::Keep)) && ctlEnabled(UI_FIX(UI_BACK)));
	settle();
	checkTrue("the board still up: the result stays", uiAngleModel()->flip.step == FlipStep::Done &&
	          ctlEnabled(UI_FIX(AngleCtl::Keep)) && chipIs("SETTLED", C_GREEN));
	fakeDumpFrame("shot_angle_flip.ppm");

	// HOLD TO KEEP: not at 975 ms, at 1000 ms.
	int writes = fakeFlashWrites();
	pressId(UI_FIX(AngleCtl::Keep));
	holdId(UI_FIX(AngleCtl::Keep), 39);
	checkTrue("held 975 ms: not kept", !angleCalValid(&uiAngleModel()->set.cal) &&
	          fakeFlashWrites() == writes);
	holdId(UI_FIX(AngleCtl::Keep), 1);
	checkTrue("1000 ms: kept, saved once", angleCalValid(&uiAngleModel()->set.cal) &&
	          fakeFlashWrites() - writes == 1);
	fakeRelease();
	frames(1);
	checkTrue("KEPT AND SAVED., green; HOLD TO KEEP disabled",
	          lineIs(252, UI_ANGLE_KEPT, C_GREEN) && !ctlEnabled(UI_FIX(AngleCtl::Keep)));
	{
		SettingsBlock b;
		memcpy(&b, fakeFlashBytes(), sizeof(b));
		checkTrue("flash holds the calibration",
		          b.s.angleCalValid == 1 && fabs(b.s.angleCalOffG[0] - OX) < 0.001 &&
		          fabs(b.s.angleCalOffG[1] - OY) < 0.001 && fabs(b.s.angleCalOffG[2] - OZ) < 0.001 &&
		          fabs(b.s.angleCalGain[2] - KZ) < 0.001 && b.s.angleCalGain[0] == 1.0f);
	}
	checkTrue("the new calibration drops the zero", !reading().zeroed);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: SETUP, CALIBRATED", uiAngleView() == AngleView::Setup &&
	          setupRowIs(3, "CALIBRATION", UI_ANGLE_CAL_YES, "FLIP"));
	tapId(UI_FIX(UI_BACK));
	writes = fakeFlashWrites();
	checkTrue("SETUP's BACK writes nothing more: the flip saved", fakeFlashWrites() == writes);
	settle();
	frames(40);
	checkTrue("calibrated: level reads +0.0`", angleIs("+0.0`", C_LIME));

	// AGAIN, then BACK without keeping: the calibration as it was.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Flip));
	flipToResult();
	checkTrue("a second result", uiAngleModel()->flip.step == FlipStep::Done);
	tapId(UI_FIX(AngleCtl::Again));
	checkTrue("AGAIN: step 1", uiAngleModel()->flip.step == FlipStep::Up &&
	          !ctlEnabled(UI_FIX(AngleCtl::Keep)));
	AngleCal before = uiAngleModel()->set.cal;
	writes = fakeFlashWrites();
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK without keeping: SETUP, nothing written, the calibration as it was",
	          uiAngleView() == AngleView::Setup && fakeFlashWrites() == writes &&
	          memcmp(&before, &uiAngleModel()->set.cal, sizeof(before)) == 0 &&
	          !uiAngleModel()->flipOn);
	tapId(UI_FIX(UI_BACK));

	// The same calibration kept again: nothing to write, saved all the same.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Flip));
	flipToResult();
	writes = fakeFlashWrites();
	pressId(UI_FIX(AngleCtl::Keep));
	holdId(UI_FIX(AngleCtl::Keep), 41);
	fakeRelease();
	frames(1);
	checkTrue("the same result kept: KEPT AND SAVED., no write",
	          lineIs(252, UI_ANGLE_KEPT, C_GREEN) && fakeFlashWrites() == writes);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));

	// A save that fails: applied, not saved.
	tapId(UI_FIX(AngleCtl::Setup));
	tapId(UI_FIX(AngleCtl::Flip));
	flipToResult(0.06);
	fakeFlashSetWritable(false);
	pressId(UI_FIX(AngleCtl::Keep));
	holdId(UI_FIX(AngleCtl::Keep), 41);
	fakeRelease();
	frames(1);
	fakeFlashSetWritable(true);
	checkTrue("a save the flash refuses: APPLIED, NOT SAVED TO FLASH., amber, the calibration live",
	          lineIs(252, UI_ANGLE_KEPT_UNSAVED, C_AMBER) &&
	          fabs(uiAngleModel()->set.cal.offG[0] - 0.06) < 0.001 &&
	          fabs(settings()->angleCalOffG[0] - 0.06) < 0.001);
	tapId(UI_FIX(UI_BACK));
	writes = fakeFlashWrites();
	fakeFlashSetWritable(false);
	tapId(UI_FIX(UI_BACK));
	checkTrue("SETUP's BACK tries the changed calibration again; refused: the hint, flash as it was",
	          lineIs(252, UI_ANGLE_HINT_UNSAVED, C_AMBER) &&
	          [] { SettingsBlock b; memcpy(&b, fakeFlashBytes(), sizeof(b));
	               return fabs(b.s.angleCalOffG[0] - OX) < 0.001; }());
	fakeFlashSetWritable(true);
	tapId(UI_FIX(UI_BACK));
	checkTrue("CTRL7 00h on BACK from the main view", s_q->reg[0x08] == 0x00);

	// The next visit takes the calibration from the live settings: the one
	// applied, not saved; after a restart the one flash holds.
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("the next visit: the calibration applied, X's offset 60 mg",
	          angleCalValid(&uiAngleModel()->set.cal) &&
	          fabs(uiAngleModel()->set.cal.offG[0] - 0.06) < 0.001 &&
	          fabs(uiAngleModel()->set.cal.offG[2] - OZ) < 0.001 &&
	          fabs(uiAngleModel()->set.cal.gain[2] - KZ) < 0.001);
	tapId(UI_FIX(AngleCtl::Setup));
	writes = fakeFlashWrites();
	tapId(UI_FIX(UI_BACK));
	checkTrue("SETUP opened and left with no edit: the calibration not yet saved is saved now",
	          fakeFlashWrites() - writes == 1 && !settingsAngleDirty() &&
	          [] { SettingsBlock b; memcpy(&b, fakeFlashBytes(), sizeof(b));
	               return fabs(b.s.angleCalOffG[0] - 0.06) < 0.001; }());
	tapId(UI_FIX(UI_BACK));
	backToTester();
	settingsLoad();
	tapId(UI_FIX(TesterCtl::Menu));
	tapId(menuRow(&UI_TOOLS));
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("after a restart: the calibration saved, X's offset 60 mg",
	          angleCalValid(&uiAngleModel()->set.cal) &&
	          fabs(uiAngleModel()->set.cal.offG[0] - 0.06) < 0.001);
	tapId(UI_FIX(AngleCtl::Setup));
	checkTrue("SETUP: CALIBRATED", setupRowIs(3, "CALIBRATION", UI_ANGLE_CAL_YES, "FLIP"));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
}

/** @brief Every view's texts inside the panel and their rectangles, at their widest. */
static void testFits() {
	section("ANGLE METER: the widest texts fit");
	checkTrue("the strip's widest, 149 px, 10 px before TOO STEEP's chip at x 167",
	          gfxTextW("SHORT EDGE  AXIS TILT 90`", 1) == 149 && uiChipX("TOO STEEP") == 167);
	checkTrue("-180.0` at scale 5: 205 px, from x 26", gfxTextW("-180.0`", 5) == 205);
	checkTrue("-500.0 MM at scale 3: 159 px, from x 72", gfxTextW("-500.0 MM", 3) == 159);
	checkTrue("MAX's widest, 190 px at scale 2, from x 41, after MAX at x 8 to 24",
	          gfxTextW("-180.0`  +180.0`", 2) == 190 && UI_MARGIN + gfxTextW("MAX", 1) + 8 <= 231 - 190);
	checkTrue("the temperature's widest, 197 px", UI_MARGIN + gfxTextW("IMU -128.0`C, -256.0`C SINCE ZERO", 1) <= 231);
	checkTrue("L 500 MM, HINGE TO TRAILING EDGE: 191 px",
	          gfxTextW("L 500 MM, HINGE TO TRAILING EDGE", 1) == 191);
	checkTrue("the hints and the IMU lines inside x 8 to 231",
	          UI_MARGIN + gfxTextW(UI_ANGLE_HINT_ARMED, 1) <= 231 &&
	          UI_MARGIN + gfxTextW(UI_ANGLE_HINT_TIMEOUT, 1) <= 231 &&
	          UI_MARGIN + gfxTextW(UI_ANGLE_HINT_UNSAVED, 1) <= 231 &&
	          UI_MARGIN + gfxTextW(UI_ANGLE_NO_ANSWER, 1) <= 231 &&
	          UI_MARGIN + gfxTextW(UI_ANGLE_KEPT_UNSAVED, 1) <= 231);
	checkTrue("FLIP CAL's widest line, 215 px, ends at x 223",
	          UI_MARGIN + gfxTextW("TURN IT ROUND, SCREEN UP, SAME SPOT.", 1) == 223);
	checkTrue("SETUP: SHORT EDGE ON AXIS, 107 px, before the toggle at x 138",
	          UI_MARGIN + gfxTextW("SHORT EDGE ON AXIS", 1) + 4 <= 138);
	checkTrue("toggle texts within 86 px", gfxTextW("NORMAL", 2) <= 86 && gfxTextW("INVERT", 2) <= 86);
}

/**
 * @brief docs/design/menu-c.md slice 2: ANGLE METER returns to TOOLS, the
 *        chooser that opened it, by BACK; TOOLS is drawn again as it was
 *        left. The meter has no leave page.
 */
static void testNavOpener() {
	section("Navigation: ANGLE METER returns to TOOLS, its opener");
	freshBench();
	openTools();
	frames(1);
	checkTrue("TOOLS: MENU, TOOLS", navIsHub(UiScreenId::Tools));
	const uint32_t tools = fakeRegionHash(0, 0, GFX_W, GFX_H);
	tapId(UI_FIX(ToolsCtl::Angle));
	frames(1);
	checkTrue("ANGLE METER: MENU, TOOLS, ANGLE METER", navIsTool(UiScreenId::Tools, UiScreenId::Angle));
	tapId(UI_FIX(UI_BACK));
	frames(1);
	checkTrue("BACK: TOOLS, the stack MENU, TOOLS", navIsHub(UiScreenId::Tools));
	checkTrue("TOOLS drawn again as it was left", fakeRegionHash(0, 0, GFX_W, GFX_H) == tools);
	// The meter opened with no press drawn on TOOLS: the frame TOOLS last
	// drew is the frame it draws on the return, which repaints all the same.
	uiChooseTool(&UI_ANGLE, nullptr);
	frames(2);
	checkTrue("opened with no press: ANGLE METER over TOOLS", navIsTool(UiScreenId::Tools, UiScreenId::Angle));
	tapId(UI_FIX(UI_BACK));
	frames(1);
	checkTrue("BACK: TOOLS drawn again, the same frame",
	          navIsHub(UiScreenId::Tools) && fakeRegionHash(0, 0, GFX_W, GFX_H) == tools);
	checkTrue("then MENU and the tester", backToTester());
}

void runAngleUiTests() {
	testFits();
	testTools();
	testOpenAndBack();
	testReading();
	testNoImu();
	testZeroAndMax();
	testSetup();
	testFlip();
	testDefaultsKeepMeter();
	testOnlyExit();
	testNavOpener();
	qmi8658Stop();
	fakeI2cReset();
	fakeFlashClear();
	settingsLoad();
	uiInit();
}
