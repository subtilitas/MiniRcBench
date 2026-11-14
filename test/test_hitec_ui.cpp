/**
 * @file test_hitec_ui.cpp
 * @brief HITEC SERVO through the real UI: MENU, the family, the claim, the
 *        entry and the read, the field list, the write, FILES, the leave and
 *        the unplug page.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * Every check runs the shipped code: ui_hitec.cpp, the Hitec job from the
 * job table, the pin claim and hitec_files.cpp. The servo is the simulator
 * behind the fake line (fake_hitec_line.h), the card the in-memory volume of
 * fake_fatfs.h. A frame runs core1 once a millisecond for 25 ms, the fake
 * DShot pump once, then the UI. Controls are pressed by id.
 */

#include "check.h"
#include "fakes.h"
#include "fake_fatfs.h"
#include "fake_hitec_line.h"
#include "gfx.h"
#include "card_cache.h"
#include "core1_job.h"
#include "esc_task.h"
#include "hitec_files.h"
#include "hitec_job.h"
#include "hitec_view.h"
#include "kst_view.h"
#include "pin_claim.h"
#include "sd_log.h"
#include "settings.h"
#include "theme.h"
#include "touch.h"
#include "ui.h"
#include "ui_hitec.h"
#include "ui_menu.h"
#include "ui_screen.h"
#include "usb_msc.h"
#include "servo_supply.h"
#include "sense.h"
#include "fake_i2c.h"
#include "plat.h"

// rcbench's fixture brings its test harness, whose reporting is not used here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "rcbench/hitec_reg_fix.h"
#pragma GCC diagnostic pop

#include <initializer_list>
#include <stdio.h>
#include <string.h>

/** @brief The level the bench drives on GP28. */
static bool enHigh() {
	FakePin p = fakePin(servoSupplyPinOf(1));
	return p.output && p.high;
}

/** @brief One pass of core1, the servo following GP28. */
static void core1Pass() {
	fakeAdvance(1);
	fakeHitecSimUpdate(enHigh(), time_us_32());
	core1JobPoll();
	fakeHitecSimUpdate(enHigh(), time_us_32());
}

/** @brief One UI frame: 25 passes of core1, one of the pump, then the UI. */
static void hitecFrame() {
	for (int i = 0; i < 25; i++) core1Pass();
	fakePumpPoll();
	uiTick();
}

static void hitecFrames(int n) {
	for (int i = 0; i < n; i++) hitecFrame();
}

#define UI_IDS_FRAMES(n) hitecFrames(n)
#include "ui_ids.h"

static HitecStatus job() {
	HitecStatus st;
	hitecJobStatus(&st);
	return st;
}

/** @brief The image the job holds. */
static hitec_image_t jobImage() { return job().image; }

/** @brief SPEED of the image the job holds. */
static int32_t jobSpeed();

/** @brief Frames until the view is @p v, at most @p max. */
static bool untilView(HitecView v, int max) {
	for (int i = 0; i < max && uiHitecView() != v; i++) hitecFrame();
	return uiHitecView() == v;
}

/** @brief Frames until @p id is on the screen, at most @p max. */
static bool untilCtl(UiControlId id, int max) {
	for (int i = 0; i < max && !ctlPresent(id); i++) hitecFrame();
	return ctlPresent(id);
}

/** @brief Frames until @p id acts, at most @p max. */
static bool untilEnabled(UiControlId id, int max) {
	for (int i = 0; i < max && !ctlEnabled(id); i++) hitecFrame();
	return ctlEnabled(id);
}

/**
 * @brief Whether the 8 px line at @p y reads @p text from x 8 in @p col: the
 *        line is redrawn with it and compared, and the frame restored.
 */
static bool lineIs(int y, const char *text, uint16_t col) {
	uint16_t *fb = gfxBuffer();
	static uint16_t keep[8][GFX_W];
	memcpy(keep, fb + y * GFX_W, sizeof(keep));
	uint32_t shown = fakeRegionHash(0, y, GFX_W, 8);
	gfxRect(0, y, GFX_W, 8, C_BG);
	gfxText(8, y, text, col, 1);
	bool same = fakeRegionHash(0, y, GFX_W, 8) == shown;
	memcpy(fb + y * GFX_W, keep, sizeof(keep));
	return same;
}

static void card() {
	fakeFatReset();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
}

static void noCard() {
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
}

/** @brief SERVO SUPPLY at GP28 (@p switched) or NONE. */
static void supply(bool switched) {
	settings()->servoSupply = switched ? 1 : 0;
	servoSupplyConfigure(switched ? 1 : 0);
}

/**
 * @brief The board fresh, a card, a servo of @p family on the line, the
 *        tester up. With @p switched the servo runs on GP28's supply, else on
 *        its own.
 */
static void freshHitec(uint8_t family, bool switched) {
	fakeFlashClear();
	settingsLoad();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	uiInit();
	escTaskConfigure(0, 600, false, 1);
	supply(switched);
	fakeHitecLineReset(family);
	fakeHitecSim()->userSupply = !switched;
	card();
	hitecFrames(2);
}

/** @brief The tester to HITEC SERVO, through MENU. */
static void openHitec() {
	tapId(UI_FIX(TesterCtl::Menu));
	hitecFrames(1);
	tapId(menuRow(&UI_HITEC));
	hitecFrames(2);
}

/** @brief Open the screen, choose @p family and wait for the field list. */
static bool toList(uint8_t family) {
	openHitec();
	tapId(UI_FIX(family == HITEC_CLASSIC ? HitecCtl::Classic : HitecCtl::DSeries));
	return untilView(HitecView::List, 800);
}

/** @brief Hold @p id until it leaves the screen, at most 80 frames; true when it left. */
static bool holdUntilGone(UiControlId id) {
	int x, y;
	ctlCentre(id, &x, &y);
	if (x < 0) return false;
	fakePress(x, y);
	hitecFrame();
	for (int i = 0; i < 80 && ctlPresent(id); i++) { fakeHold(x, y); hitecFrame(); }
	bool gone = !ctlPresent(id);
	fakeRelease();
	hitecFrame();
	return gone;
}

/** @brief Tap @p id; true when it left the screen. */
static bool tapUntilGone(UiControlId id) {
	tapId(id);
	return !ctlPresent(id);
}

/** @brief The D series SPEED the job's image holds. */
static int32_t speedOf(const hitec_image_t *img) {
	hitec_value_t v;
	if (hitec_param_get(img, HITEC_LAYOUT_D, HITEC_P_D_SPEED, &v) != HITEC_GET_OK) return -1;
	return v.value;
}

static int32_t jobSpeed() {
	hitec_image_t img = jobImage();
	return speedOf(&img);
}

/** @brief Whether the model the job read is one rcbench measured. */
static bool hitecModelMeasuredNow() {
	hitec_image_t img = jobImage();
	return hitecModelMeasured(&img);
}

/** @brief SPEED one step down in the draft: row 0 selected, - tapped. */
static void speedDown() {
	revealRow(0);
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(HitecCtl::EditMinus));
}

/** @brief From any view: leave, restart DShot, back to the tester. */
static void leaveAll() {
	for (int i = 0; i < 10 && uiCurrentScreen() == UiScreenId::Hitec; i++) {
		HitecView v = uiHitecView();
		if (v == HitecView::Unplug) {
			if (untilCtl(UI_FIX(HitecCtl::Restart), 200)) tapId(UI_FIX(HitecCtl::Restart));
		} else if (v == HitecView::Leave) {
			if (untilEnabled(UI_FIX(HitecCtl::Leave), 400)) tapId(UI_FIX(HitecCtl::Leave));
		} else if (v == HitecView::Write && !ctlPresent(UI_FIX(HitecCtl::WriteHold)) &&
		           !ctlPresent(UI_FIX(HitecCtl::WriteOk))) {
			hitecFrames(40);
		} else {
			tapId(UI_FIX(UI_BACK));
		}
		hitecFrames(2);
	}
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) hitecFrame();
	if (uiCurrentScreen() == UiScreenId::Menu) tapId(UI_FIX(UI_BACK));
	hitecFrames(2);
	supply(false);
}

/** @brief Writes the servo had taken when the record first stood on the card. */
static int s_writesAtRecord = -1;

/** @brief fakeFatOnCall(): the servo's write count once the record is on the card. */
static void watchRecord() {
	if (s_writesAtRecord < 0 && fakeFatExists("HITEC/HITEC.TXT"))
		s_writesAtRecord = (int)fakeHitecSim()->writes;
}

static void testFamily() {
	section("Hitec screen: MENU opens it, the claim, the family");
	freshHitec(HITEC_D, false);
	tapId(UI_FIX(TesterCtl::Menu));
	hitecFrames(1);
	int resumes = fakeResumeCount();
	tapId(menuRow(&UI_HITEC));
	checkTrue("HITEC SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Hitec);
	checkTrue("on the family view", uiHitecView() == HitecView::Family);
	// The claim begins on the screen's first frame, before a family is known.
	hitecFrames(1);
	checkTrue("the claim on SIG begins on the first frame", pinClaimHolder() == PinUser::Hitec &&
	          pinClaimState() != ClaimState::Free && !fakePinReturned());
	{
		char rel[40];
		snprintf(rel, sizeof(rel), "RELEASING SIG GP%u FROM DSHOT", (unsigned)escTaskDshotPin());
		checkTrue("while DShot drains: RELEASING SIG GP0 FROM DSHOT, dim",
		          pinClaimState() == ClaimState::Stopping &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, rel, C_DIM, C_BG, 1));
	}
	hitecFrames(1);
	char held[40];
	snprintf(held, sizeof(held), HITEC_UI_HELD_FMT, (unsigned)escTaskDshotPin());
	checkTrue("DShot drained and stopped: Held", pinClaimState() == ClaimState::Held &&
	          escTaskSuspended());
	checkTrue("no job and no line before the tap", core1JobPhase() == Core1JobPhase::Idle &&
	          fakeHitecLineOpens() == 0);
	checkTrue("the strip: " HITEC_UI_HELD_FMT ", green",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, held, UI_COL_SAFE, C_BG, 1));
	checkTrue("then " HITEC_UI_CHOOSE,
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, HITEC_UI_CHOOSE, C_DIM, C_BG, 1));
	checkTrue("CLASSIC disabled with NONE", !ctlEnabled(UI_FIX(HitecCtl::Classic)));
	checkTrue("D SERIES acts", ctlEnabled(UI_FIX(HitecCtl::DSeries)));
	checkControlTable("Hitec family");
	// CLASSIC's reason names the hardware first, then the setting; D SERIES
	// says when to power the servo. Each line is redrawn over the full width,
	// so nothing else may share its rows.
	const UiRect cl = ctlRect(UI_FIX(HitecCtl::Classic));
	const UiRect ds = ctlRect(UI_FIX(HitecCtl::DSeries));
	checkTrue("NONE: CLASSIC needs the converter of wiring part C",
	          lineIs(cl.y + cl.h + 16, HITEC_UI_CLASSIC_WHY_1, C_AMBER));
	checkTrue("and SETTINGS > SERVO SUPPLY GP28/29",
	          lineIs(cl.y + cl.h + 28, HITEC_UI_CLASSIC_WHY_2, C_AMBER));
	checkTrue("the second line ends above D SERIES", cl.y + cl.h + 28 + 7 < ds.y);
	checkTrue("D SERIES: power the servo up, then tap",
	          lineIs(ds.y + ds.h + 18, HITEC_UI_D_POWER, C_DIM));
	checkTrue("36, 36 and 29 characters: 215, 215 and 173 px, to x 223 at most",
	          gfxTextW(HITEC_UI_CLASSIC_WHY_1, 1) == 215 && gfxTextW(HITEC_UI_CLASSIC_WHY_2, 1) == 215 &&
	          gfxTextW(HITEC_UI_D_POWER, 1) == 173);
	int x0 = 0, x1 = 0;
	checkTrue("no note reaches the panel's 8 px margin",
	          fakeInkExtent(cl.y + cl.h + 4, 34, C_BG, &x0, &x1) && x0 >= 8 && x1 < GFX_W - 8 &&
	          fakeInkExtent(ds.y + ds.h + 4, 24, C_BG, &x0, &x1) && x0 >= 8 && x1 < GFX_W - 8);
	checkTrue("nor does the strip", fakeInkExtent(UI_STRIP_LINE1_Y, 18, C_BG, &x0, &x1) &&
	          x0 >= 8 && x1 < GFX_W - 8);
	fakeDumpFrame("shot_hitec_family.ppm");
	// BACK: the pin went to a servo, so the unplug page and RESTART DSHOT,
	// as LEAVE; BACK there returns to the family, still held, with no job.
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the unplug page", uiHitecView() == HitecView::Unplug &&
	          pinClaimState() == ClaimState::Leaving);
	checkTrue("RESTART DSHOT shows", untilCtl(UI_FIX(HitecCtl::Restart), 10));
	checkTrue("DShot is still down", !fakePinReturned() && fakeResumeCount() == resumes);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK on the unplug page: the family view again, held, no job",
	          uiHitecView() == HitecView::Family && pinClaimState() == ClaimState::Held &&
	          core1JobPhase() == Core1JobPhase::Idle && fakeHitecLineOpens() == 0);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK again: the unplug page", untilCtl(UI_FIX(HitecCtl::Restart), 10));
	tapId(UI_FIX(HitecCtl::Restart));
	checkTrue("RESTART DSHOT: MENU, DShot resumed", uiCurrentScreen() == UiScreenId::Menu &&
	          fakePinReturned() && fakeResumeCount() == resumes + 1);
	checkTrue("MENU draws HITEC SERVO disabled while the pump rebuilds", pinClaimState() == ClaimState::Restoring &&
	          !ctlEnabled(menuRow(&UI_HITEC)));
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) hitecFrame();
	checkTrue("then the claim is free", pinClaimState() == ClaimState::Free &&
	          ctlEnabled(menuRow(&UI_HITEC)));
	tapId(UI_FIX(UI_BACK));
	hitecFrames(2);

	section("Hitec family: an ESC answering on SIG keeps DShot");
	{
		EscTelemetry t;
		memset(&t, 0, sizeof(t));
		t.lastRpmMs = millis();
		fakeSetTelemetry(&t);
		tapId(UI_FIX(TesterCtl::Menu));
		hitecFrames(1);
		tapId(menuRow(&UI_HITEC));
		t.lastRpmMs = millis();
		fakeSetTelemetry(&t);
		hitecFrames(2);
		char esc[40];
		snprintf(esc, sizeof(esc), HITEC_UI_ESC_FMT, (unsigned)escTaskDshotPin());
		char want[40];
		snprintf(want, sizeof(want), "AN ESC ANSWERS ON SIG GP%u: UNPLUG IT.",
		         (unsigned)escTaskDshotPin());
		checkStr("the ESC line names SIG", esc, want);
		checkInt("37 characters, 221 px, to x 228", gfxTextW(esc, 1), 221);
		checkTrue("refused: no claim, DShot runs", pinClaimState() == ClaimState::Free &&
		          fakePinReturned());
		checkTrue("the strip: " HITEC_UI_ESC_FMT,
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, esc, C_AMBER, C_BG, 1) &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, HITEC_UI_ESC_2, C_AMBER, C_BG, 1));
		checkTrue("no family acts", !ctlEnabled(UI_FIX(HitecCtl::DSeries)) &&
		          !ctlEnabled(UI_FIX(HitecCtl::Classic)));
		tapId(UI_FIX(HitecCtl::DSeries), IdExpect::Disabled);
		checkTrue("a tap on D SERIES opens nothing", uiHitecView() == HitecView::Family &&
		          core1JobPhase() == Core1JobPhase::Idle);
		fakeDumpFrame("shot_hitec_family_esc.ppm");
		// Quiet for 500 ms: the claim follows by itself.
		memset(&t, 0, sizeof(t));
		fakeSetTelemetry(&t);
		hitecFrames(3);
		checkTrue("the ESC quiet: the claim holds, D SERIES acts",
		          pinClaimState() == ClaimState::Held && ctlEnabled(UI_FIX(HitecCtl::DSeries)));
		leaveAll();
	}

	freshHitec(HITEC_CLASSIC, true);
	openHitec();
	checkTrue("with GP28: CLASSIC acts", ctlEnabled(UI_FIX(HitecCtl::Classic)));
	checkTrue("and its reason is gone", lineIs(cl.y + cl.h + 16, "", C_AMBER) &&
	          lineIs(cl.y + cl.h + 28, "", C_AMBER));
	checkTrue("D SERIES names the switch, not the power-up",
	          lineIs(ds.y + ds.h + 18, "SUPPLY SWITCHED ON GP28", C_DIM));
	checkTrue("the supply stays off until the tap", !enHigh() && fakeHitecLineOpens() == 0);
	fakeDumpFrame("shot_hitec_family_classic.ppm");
	leaveAll();
}

static void testDSeriesRead() {
	section("Hitec screen: a D series servo on its own supply, the claim, the read");
	freshHitec(HITEC_D, false);
	openHitec();
	tapId(UI_FIX(HitecCtl::DSeries));
	checkTrue("D SERIES: the main view", uiHitecView() == HitecView::Main);
	{
		int x0 = 0, x1 = 0;
		checkTrue("the main view clears the strip band: y 56 to 61 blank",
		          !fakeInkExtent(56, 6, C_BG, &x0, &x1));
	}
	checkTrue("it claims SIG", pinClaimHolder() == PinUser::Hitec);
	hitecFrames(4);
	checkControlTable("Hitec main");
	checkTrue("the read ends on the field list", untilView(HitecView::List, 800));
	HitecStatus st = job();
	checkTrue("in the mode with the image and the backup",
	          st.job == HitecJobState::InMode && st.hasImage && st.hasBackup);
	checkTrue("GP28 untouched", !enHigh());
	checkControlTable("Hitec list");
	{
		char model[24], want[48];
		hitecModelText(&st.image, model, sizeof(model));
		snprintf(want, sizeof(want), "%s  BACKUP IN RAM  GP0", model);
		checkTrue("the strip: model, backup and pin, two spaces apart, 155 px beside PROG MODE",
		          gfxTextW(want, 1) == 155 && fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_DIM, C_BG, 1));
		int x0 = 0, x1 = 0;
		checkTrue("the family view's second strip line is gone: y 56 to 61 blank",
		          !fakeInkExtent(56, 6, C_BG, &x0, &x1));
		// Ink put into y 56 to 61 by hand; the list's next drawing clears
		// it, as its strip clear covers y 40 to 61.
		gfxRect(UI_MARGIN, 56, 100, 6, C_TEXT);
		revealRow(1);
		tapId(UI_ROW(1, Row));
		hitecFrames(1);
		checkTrue("the list redrawn: y 56 to 61 blank again", uiHitecView() == HitecView::List &&
		          !fakeInkExtent(56, 6, C_BG, &x0, &x1));
	}
	checkTrue("HOLD TO WRITE waits for an edit", !ctlEnabled(UI_FIX(HitecCtl::Write)));
	checkTrue("REVERT too", !ctlEnabled(UI_FIX(HitecCtl::Revert)));
	checkTrue("MODEL's row cannot be selected", !ctlEnabled(UI_ROW(11, Row)));
	fakeDumpFrame("shot_hitec_list.ppm");

	speedDown();
	checkTrue("an edit: HOLD TO WRITE acts", ctlEnabled(UI_FIX(HitecCtl::Write)));
	checkTrue("and REVERT", ctlEnabled(UI_FIX(HitecCtl::Revert)));
	checkControlTable("Hitec list, editing");
	fakeDumpFrame("shot_hitec_preview.ppm");
	tapId(UI_FIX(HitecCtl::Revert));
	checkTrue("REVERT drops it", !ctlEnabled(UI_FIX(HitecCtl::Write)) && !ctlEnabled(UI_FIX(HitecCtl::Revert)));

	// A sideways swipe on NEUTRAL steps it by a coarse step, 1490 counts.
	revealRow(3);
	int x, y;
	ctlCentre(UI_ROW(3, Row), &x, &y);
	fakePress(x, y); hitecFrame();
	for (int k = 1; k <= 10; k++) { fakeHold(x - 4 * k, y); hitecFrame(); }
	fakeRelease(); hitecFrame();
	checkTrue("a sideways swipe on NEUTRAL is an edit", ctlEnabled(UI_FIX(HitecCtl::Write)));
	tapId(UI_FIX(HitecCtl::Revert));
	leaveAll();
}

static void testWrite() {
	section("Hitec screen: WRITE..., HOLD TO WRITE, the backup and record first, the result");
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	int32_t before = jobSpeed();
	speedDown();

	// WRITE...: nothing while pressed, nothing on a slide off; the tap's
	// release opens the write page.
	checkTrue("the list's button reads WRITE...",
	          fakeTextIs(6 + (110 - gfxTextW("WRITE...", 1)) / 2, 268 + (46 - 7) / 2, "WRITE...", C_TEXT,
	                     C_PANEL, 1));
	pressId(UI_FIX(HitecCtl::Write));
	int x, y;
	ctlCentre(UI_FIX(HitecCtl::Write), &x, &y);
	for (int i = 0; i < 60; i++) { fakeHold(x, y); hitecFrame(); }
	checkTrue("a 1500 ms press opens nothing while down", uiHitecView() == HitecView::List);
	for (int i = 0; i < 30; i++) { fakeHold(x, 120); hitecFrame(); }
	fakeRelease(); hitecFrame();
	checkTrue("a press slid off opens nothing", uiHitecView() == HitecView::List);

	checkTrue("a tap opens the write page", tapUntilGone(UI_FIX(HitecCtl::Write)));
	checkTrue("the write page", uiHitecView() == HitecView::Write);
	hitecFrames(60);
	checkTrue("the tap's press is not timed: 1500 ms on, HOLD TO WRITE still waits",
	          uiHitecView() == HitecView::Write && ctlEnabled(UI_FIX(HitecCtl::WriteHold)) &&
	          jobSpeed() == before);
	checkControlTable("Hitec write page");
	checkTrue("the strip counts 1 CHANGE", fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "1 CHANGE", C_DIM, C_BG, 1) &&
	          !fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "1 VALUE", C_DIM, C_BG, 1));
	fakeDumpFrame("shot_hitec_write.ppm");
	s_writesAtRecord = -1;
	fakeFatOnCall(watchRecord);
	unsigned writes0 = fakeHitecSim()->writes;
	checkTrue("HOLD TO WRITE writes", holdUntilGone(UI_FIX(HitecCtl::WriteHold)));
	checkTrue("the result", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	fakeFatOnCall(nullptr);
	checkTrue("the record stood on the card before the first write frame",
	          s_writesAtRecord == (int)writes0);
	checkTrue("the backup is in the ring", fakeFatExists("BACKUP/HT001.TXT"));
	checkTrue("the verified write removed the record", !fakeFatExists("HITEC/HITEC.TXT"));
	HitecStatus st = job();
	checkTrue("verified", st.result == HITEC_SES_OK && speedOf(&st.image) < before);
	checkTrue("the servo holds it", fakeHitecSim()->writes > writes0);
	checkControlTable("Hitec write result");
	tapId(UI_FIX(HitecCtl::WriteOk));
	checkTrue("OK: the field list", uiHitecView() == HitecView::List);
	checkTrue("nothing pending", !ctlEnabled(UI_FIX(HitecCtl::Write)));

	// A second write of the session: no second backup.
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("a second write ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	checkTrue("no second backup", !fakeFatExists("BACKUP/HT002.TXT"));
	tapId(UI_FIX(HitecCtl::WriteOk));

	// A later write while the record removal still flushes: the flush ends,
	// then this write's record, then its frames.
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 4);
	s_writesAtRecord = -1;
	fakeFatOnCall(watchRecord);
	unsigned writes1 = fakeHitecSim()->writes;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(4);
	checkTrue("an earlier batch flushing: no write frame yet", fakeHitecSim()->writes == writes1);
	checkTrue("waiting for the flush: BACK disabled, and drawn so", uiHitecView() == HitecView::Write &&
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("a tap on it: the page waits, no write frame", uiHitecView() == HitecView::Write &&
	          fakeHitecSim()->writes == writes1);
	fakeCacheSet(CardCache::Idle, 0);
	checkTrue("then the write ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	fakeFatOnCall(nullptr);
	checkTrue("its record stood on the card before its first write frame",
	          s_writesAtRecord == (int)writes1);
	checkTrue("verified", job().result == HITEC_SES_OK);
	tapId(UI_FIX(HitecCtl::WriteOk));

	// BACK during a write is drawn disabled and does nothing: no leave is
	// queued, and the result follows on the page.
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the write runs", job().job == HitecJobState::Busy);
	checkTrue("BACK disabled while it runs, and drawn so",
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("BACK keeps the write page while it runs", uiHitecView() == HitecView::Write);
	checkControlTable("Hitec writing");
	checkTrue("the result on the page, no leave page",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && uiHitecView() == HitecView::Write);
	checkTrue("the write ended verified", job().result == HITEC_SES_OK);
	checkTrue("BACK live again under the result", ctlEnabled(UI_FIX(UI_BACK)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK with the result: the list", uiHitecView() == HitecView::List);
	leaveAll();
}

static void testFlushWait() {
	section("Hitec screen: the write waits for the backup's flush to the card");
	fakeCacheReset();
	freshHitec(HITEC_D, false);
	SdLogStatus logging;
	memset(&logging, 0, sizeof(logging));
	logging.state = SdLogState::Logging;
	fakeSdLogSet(&logging);
	checkTrue("the field list", toList(HITEC_D));
	checkTrue("the claim turned the cache on", fakeCacheCalls().begins >= 1 &&
	                                          cardCacheState() == CardCache::Idle);
	{
		SdLogStatus ended;
		sdLogStatus(&ended);
		checkTrue("the claim stopped the running log, recorded as HITEC SERVO's",
		          ended.state == SdLogState::Idle && ended.end == SdLogEnd::ByHitec);
	}
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	unsigned writes = fakeHitecSim()->writes;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(5);
	checkTrue("the backup queued: no write frame yet",
	          fakeHitecSim()->writes == writes && fakeFatExists("BACKUP/HT001.TXT"));
	checkTrue("the page says so", lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER) &&
	                              lineIs(210, "BLOCKS LEFT: 12", C_TEXT));
	checkTrue("no hold to press meanwhile", !ctlPresent(UI_FIX(HitecCtl::WriteHold)));
	fakeCacheSet(CardCache::Idle, 0);
	checkTrue("flushed: the write runs and verifies",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && job().result == HITEC_SES_OK &&
	          fakeHitecSim()->writes > writes);
	tapId(UI_FIX(HitecCtl::WriteOk));
	unsigned ends = fakeCacheCalls().ends;
	leaveAll();
	checkTrue("the claim's end turns the cache off", fakeCacheCalls().ends > ends &&
	                                                cardCacheState() == CardCache::Off);

	// A flush that fails: nothing reaches the servo.
	freshHitec(HITEC_D, false);
	checkTrue("again the field list", toList(HITEC_D));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = fakeHitecSim()->writes;
	unsigned recovers = fakeCacheCalls().recovers;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(2);
	fakeCacheSet(CardCache::Failed, 0);
	hitecFrames(2);
	checkTrue("a failed flush: no write frame, the card mounted again",
	          fakeHitecSim()->writes == writes && fakeCacheCalls().recovers == recovers + 1);
	checkTrue("and the page says why, and what next, in three lines",
	          ctlPresent(UI_FIX(HitecCtl::WriteOk)) &&
	          lineIs(196, "THE CARD DID NOT TAKE THE BACKUP", C_AMBER) &&
	          lineIs(208, "(REMOVED OR SWAPPED?). NOTHING WENT TO", C_AMBER) &&
	          lineIs(220, "THE SERVO. CHECK THE CARD, HOLD AGAIN.", C_AMBER));
	tapId(UI_FIX(HitecCtl::WriteOk));
	// Holding again retries: the backup is saved again, then the write runs.
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("held again: the write sent and verified",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && fakeHitecSim()->writes > writes &&
	          job().result == HITEC_SES_OK);
	tapId(UI_FIX(HitecCtl::WriteOk));
	// A later write whose batch fails: the page names a file, not the backup.
	writes = fakeHitecSim()->writes;
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(2);
	fakeCacheSet(CardCache::Failed, 0);
	hitecFrames(2);
	checkTrue("a later write's batch failed: A FILE, nothing to the servo, in three lines",
	          ctlPresent(UI_FIX(HitecCtl::WriteOk)) && fakeHitecSim()->writes == writes &&
	          lineIs(196, "THE CARD DID NOT TAKE A FILE (REMOVED", C_AMBER) &&
	          lineIs(208, "OR SWAPPED?). NOTHING WENT TO THE", C_AMBER) &&
	          lineIs(220, "SERVO. CHECK THE CARD, HOLD AGAIN.", C_AMBER));
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();
	fakeCacheReset();
}

static void testWriteError() {
	section("Hitec screen: a write in error, READ AGAIN");
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	speedDown();
	fakeHitecSim()->ignoreAddr = 0x54;
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the result", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	HitecStatus st = job();
	checkTrue("the write failed", st.result != HITEC_SES_OK);
	checkTrue("the record stays", fakeFatExists("HITEC/HITEC.TXT"));
	fakeDumpFrame("shot_hitec_write_error.ppm");
	fakeHitecSim()->ignoreAddr = -1;
	tapId(UI_FIX(HitecCtl::WriteOk));
	checkTrue("back on the field list", uiHitecView() == HitecView::List);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the leave page", uiHitecView() == HitecView::Leave);
	checkControlTable("Hitec leave");
	tapId(UI_FIX(HitecCtl::LeaveRead));
	checkTrue("READ AGAIN reads", untilView(HitecView::List, 400));
	for (int i = 0; i < 800 && job().job != HitecJobState::InMode; i++) hitecFrame();
	checkTrue("in the mode again", job().job == HitecJobState::InMode);
	leaveAll();
	checkTrue("the record of the failed write stays after LEAVE", fakeFatExists("HITEC/HITEC.TXT"));
}

static void testFiles() {
	section("Hitec screen: FILES, SAVE AS, LOAD, a refused file, HOLD TO RESTORE");
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("FILES", uiHitecView() == HitecView::Files);
	checkControlTable("Hitec files");
	checkTrue("SAVE AS acts", ctlEnabled(UI_FIX(HitecCtl::SaveAs)));
	tapId(UI_FIX(HitecCtl::SaveAs));
	checkTrue("SAVE AS wrote HT001", fakeFatExists("HITEC/HT001.TXT"));
	checkTrue("LOAD waits for a row", !ctlEnabled(UI_FIX(HitecCtl::Load)));
	checkTrue("RESTORE waits for a write", !ctlEnabled(UI_FIX(HitecCtl::Restore)));
	fakeDumpFrame("shot_hitec_files.ppm");

	// A classic file is refused on a D series claim.
	hitec_image_t c;
	hs5645(&c);
	char text[HITEC_CARD_FILE_MAX];
	size_t n = hitec_card_file_text(&c, text, sizeof(text));
	fakeFatPut("HITEC/HT002.TXT", text, n);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Files));
	tapId(UI_ROW(1, Row));
	tapId(UI_FIX(HitecCtl::Load));
	checkTrue("a classic file: refused, FILES stays", uiHitecView() == HitecView::Files);
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(HitecCtl::Load));
	checkTrue("its own file loads: the field list", uiHitecView() == HitecView::List);
	checkTrue("an equal file leaves nothing pending", !ctlEnabled(UI_FIX(HitecCtl::Write)));

	// A write, then HOLD TO RESTORE puts the backup back.
	int32_t before = jobSpeed();
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	untilCtl(UI_FIX(HitecCtl::WriteOk), 800);
	tapId(UI_FIX(HitecCtl::WriteOk));
	checkTrue("written", jobSpeed() < before);
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("RESTORE acts once the servo differs from the backup", ctlEnabled(UI_FIX(HitecCtl::Restore)));
	checkTrue("the restore button reads RESTORE...",
	          fakeTextIs(122 + (112 - gfxTextW("RESTORE...", 1)) / 2, ctlRect(UI_FIX(HitecCtl::Restore)).y +
	                     (ctlRect(UI_FIX(HitecCtl::Restore)).h - 7) / 2, "RESTORE...", C_TEXT, C_PANEL, 1));
	checkTrue("a tap on RESTORE... opens the write page", tapUntilGone(UI_FIX(HitecCtl::Restore)));
	checkTrue("the restore page", uiHitecView() == HitecView::Write);
	hitecFrames(60);
	checkTrue("it restores nothing until its hold", jobSpeed() != before &&
	          ctlEnabled(UI_FIX(HitecCtl::WriteHold)));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the restore ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	checkInt("the servo is back to the backup", jobSpeed(), before);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();
}

static void testRcbenchName() {
	section("Hitec screen: a file of rcbench's name in FILES");
	freshHitec(HITEC_D, false);
	fakeFatMkdir("HITEC");
	hitec_image_t d;
	d485(&d);
	char text[HITEC_CARD_FILE_MAX];
	size_t n = hitec_card_file_text(&d, text, sizeof(text));
	fakeFatPut("HITEC/HITEC001.TXT", text, n);
	checkTrue("the field list", toList(HITEC_D));
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("the file's row reads its name whole, without .TXT",
	          lineIs(UI_BODY_Y + 16, "HITEC001", C_DIM));
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(HitecCtl::Load));
	checkTrue("it loads", uiHitecView() == HitecView::List);
	leaveAll();
}

static void testNoCardWrite() {
	section("Hitec screen: no card, a card held by USB");
	freshHitec(HITEC_D, false);
	noCard();
	checkTrue("the field list", toList(HITEC_D));
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("RETRY MOUNT in place of SAVE AS", ctlPresent(UI_FIX(HitecCtl::RetryMount)) &&
	          !ctlPresent(UI_FIX(HitecCtl::SaveAs)));
	checkControlTable("Hitec files, no card");
	tapId(UI_FIX(UI_BACK));
	int32_t before = jobSpeed();
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	hitecFrames(1);
	checkTrue("no card: the old values lost at LEAVE, amber",
	          lineIs(236, "NO CARD: OLD VALUES LOST AT LEAVE", C_AMBER));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the write runs without a card", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	checkTrue("verified", job().result == HITEC_SES_OK && jobSpeed() < before);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();

	freshHitec(HITEC_D, false);
	fakeMscSet(MscState::Serving, 0);
	checkTrue("the field list", toList(HITEC_D));
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("a card held by USB: RETRY MOUNT disabled", !ctlEnabled(UI_FIX(HitecCtl::RetryMount)));
	tapId(UI_FIX(UI_BACK));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	hitecFrames(1);
	checkTrue("a card held by USB: the same cost, amber",
	          lineIs(236, "CARD ON USB: OLD VALUES LOST AT LEAVE", C_AMBER));
	tapId(UI_FIX(UI_BACK));
	leaveAll();
	fakeMscSet(MscState::Idle, 0);

	// A card that takes no file stops the write.
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeFatWriteProtect(true);
	unsigned writes0 = fakeHitecSim()->writes;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("a card that takes no file: a note and OK", untilCtl(UI_FIX(HitecCtl::WriteOk), 200));
	checkTrue("nothing written to the servo", fakeHitecSim()->writes == writes0);
	checkTrue("the note says why, that the servo is unchanged, and what next",
	          lineIs(196, "THE CARD TOOK NO FILE (FULL OR", C_AMBER) &&
	          lineIs(208, "REMOVED?). NOTHING WENT TO THE SERVO.", C_AMBER) &&
	          lineIs(220, "CHECK THE CARD, HOLD AGAIN.", C_AMBER));
	fakeFatWriteProtect(false);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();

	// The card refuses the backup's write and takes the record.
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the backup refused, the record taken: the write runs",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && job().result == HITEC_SES_OK &&
	          !fakeFatExists("BACKUP/HT001.TXT"));
	hitecFrames(1);
	checkTrue("the page names no backup file: lost at LEAVE, amber",
	          lineIs(236, "NO BACKUP ON THE CARD: LOST AT LEAVE", C_AMBER));
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();
}

static void testRecordAtEntry() {
	section("Hitec screen: a record found at entry");
	freshHitec(HITEC_D, false);
	fakeFatMkdir("HITEC");
	hitec_card_record_t r;
	memset(&r, 0, sizeof(r));
	r.series = HITEC_D;
	r.pin = 0;
	d485(&r.before);
	r.target = r.before;
	hitec_param_set(&r.target, HITEC_LAYOUT_D, HITEC_P_D_SPEED, 40);
	char text[HITEC_CARD_TEXT_MAX];
	size_t n = hitec_card_text(&r, text, sizeof(text));
	fakeFatPut("HITEC/HITEC.TXT", text, n);
	checkTrue("the field list", toList(HITEC_D));
	tapId(UI_FIX(HitecCtl::Files));
	tapId(UI_ROW(0, Row));
	checkTrue("RECORD is the first row and loads", ctlEnabled(UI_FIX(HitecCtl::Load)));
	tapId(UI_FIX(HitecCtl::Load));
	checkTrue("its image before the write: the field list", uiHitecView() == HitecView::List);
	speedDown();
	tapId(UI_FIX(HitecCtl::Revert));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	untilCtl(UI_FIX(HitecCtl::WriteOk), 800);
	checkTrue("the old record moved into the ring first, the backup after it",
	          fakeFatExists("BACKUP/HT001.TXT") && fakeFatExists("BACKUP/HT002.TXT"));
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();
}

static void testClassic() {
	section("Hitec screen: a classic servo on GP28, REPAIR, FACTORY VALUES, a model not measured");
	freshHitec(HITEC_CLASSIC, true);
	// A checksum that does not sum: the classic cells 0x00 to 0x1C.
	fakeHitecSim()->mem[0x1C] ^= 0x01;
	checkTrue("the field list", toList(HITEC_CLASSIC));
	checkTrue("GP28 on", enHigh());
	{
		// MODEL  BACKUP IN RAM  GP0  Vnn runs into the PROG MODE chip: the
		// version and the pin, the items furthest right, go; the backup's
		// state and the spaces stay.
		char model[24], full[64], pin[64], want[48];
		HitecStatus cs = job();
		hitecModelText(&cs.image, model, sizeof(model));
		snprintf(want, sizeof(want), "%s  BACKUP IN RAM", model);
		snprintf(pin, sizeof(pin), "%s  GP0", want);
		snprintf(full, sizeof(full), "%s  V%02X", pin, cs.image.version);
		const int room = uiChipX(HITEC_UI_CHIP) - 4 - UI_MARGIN;
		checkTrue("the room ends 4 px before the chip, at x 163", room == 155);
		checkTrue("with the pin, or the pin and the version, it would not fit",
		          gfxTextW(full, 1) > room && gfxTextW(pin, 1) > room);
		checkTrue("the strip keeps the backup: model and backup, two spaces apart",
		          gfxTextW(want, 1) <= room && fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_DIM, C_BG, 1));
	}
	int sumRow = -1;
	HitecRow rows[HITEC_ROWS_MAX];
	int nr = hitecRows(HITEC_LAYOUT_HS5, rows);
	for (int i = 0; i < nr; i++)
		if (rows[i].param == HITEC_P_C_CHECKSUM) sumRow = i;
	revealRow(sumRow);
	checkTrue("a bad CHECKSUM row can be selected", ctlEnabled(UI_ROW(sumRow, Row)));
	tapId(UI_ROW(sumRow, Row));
	checkTrue("REPAIR in place of - and +", ctlPresent(UI_FIX(HitecCtl::Repair)) &&
	          !ctlPresent(UI_FIX(HitecCtl::EditMinus)));
	checkControlTable("Hitec repair");
	fakeDumpFrame("shot_hitec_repair.ppm");
	tapId(UI_FIX(HitecCtl::Repair));
	checkTrue("REPAIR is an edit to write", ctlEnabled(UI_FIX(HitecCtl::Write)));
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the write ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 1200));
	hitec_value_t v;
	HitecStatus st = job();
	checkTrue("the checksum reads OK",
	          hitec_param_get(&st.image, HITEC_LAYOUT_HS5, HITEC_P_C_CHECKSUM, &v) == HITEC_GET_OK &&
	          v.value == HITEC_SUM_OK);
	tapId(UI_FIX(HitecCtl::WriteOk));
	{
		// After the first write the backup has a name: beside PROG MODE the
		// version and the pin still go.
		char model[24], want[48], pin[64];
		hitecModelText(&st.image, model, sizeof(model));
		snprintf(want, sizeof(want), "%s  BACKUP HT001", model);
		snprintf(pin, sizeof(pin), "%s  GP0", want);
		checkTrue("the written backup on the strip: model and backup HT001",
		          uiHitecView() == HitecView::List && gfxTextW(pin, 1) > uiChipX(HITEC_UI_CHIP) - 4 - UI_MARGIN &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_DIM, C_BG, 1));
	}

	tapId(UI_FIX(HitecCtl::Files));
	tapId(UI_ROW(1, Row));
	checkTrue("FACTORY VALUES loads", ctlEnabled(UI_FIX(HitecCtl::Load)));
	tapId(UI_FIX(HitecCtl::Load));
	checkTrue("into the draft: the field list", uiHitecView() == HitecView::List);
	leaveAll();
	checkTrue("LEAVE switched GP28 off", !enHigh());

	// An HS-5955TG: not measured, written in the HS-5xxx layout.
	freshHitec(HITEC_CLASSIC, true);
	hitec_image_t img;
	hs5955(&img);
	memcpy(fakeHitecSim()->mem, img.mem, sizeof(fakeHitecSim()->mem));
	fakeHitecSim()->version = img.version;
	checkTrue("an HS-5955TG reads", toList(HITEC_CLASSIC));
	checkTrue("not measured", !hitecModelMeasuredNow());
	revealRow(0);
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(HitecCtl::EditMinus));
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("its write page", uiHitecView() == HitecView::Write);
	fakeDumpFrame("shot_hitec_not_measured.ppm");
	tapId(UI_FIX(UI_BACK));
	leaveAll();
}

/**
 * @brief With CURRENT SENSE at INA219, the strip reads the current after
 *        SUPPLY ON while the switched supply powers the servo up.
 */
static void testSupplyCurrent() {
	section("Hitec screen: the current after SUPPLY ON");
	freshHitec(HITEC_CLASSIC, true);
	settings()->currentSense = 1;
	senseConfigure(1);
	fakeI2cReset();
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	ina->currentA = 0.08;
	openHitec();
	tapId(UI_FIX(HitecCtl::Classic));
	for (int i = 0; i < 400 && !(job().job == HitecJobState::Entering && job().power == HitecPower::Listen);
	     i++)
		hitecFrame();
	checkTrue("listening, the supply on: SIG GP0  SUPPLY ON 0.08 A",
	          job().power == HitecPower::Listen && lineIs(UI_STRIP_Y + 4, "SIG GP0  SUPPLY ON 0.08 A", C_DIM));
	leaveAll();

	// STOP during a D series entry: RETRY, the supply off, no current on the
	// strip, so a new reading repaints nothing.
	freshHitec(HITEC_D, true);
	settings()->currentSense = 1;
	senseConfigure(1);
	openHitec();
	tapId(UI_FIX(HitecCtl::DSeries));
	untilCtl(UI_FIX(HitecCtl::Stop), 200);
	tapId(UI_FIX(HitecCtl::Stop));
	checkTrue("STOP: RETRY", untilCtl(UI_FIX(HitecCtl::Retry), 400));
	hitecFrames(3);
	ina->currentA = 0.31;
	hitecFrame();
	checkInt("a new reading while the strip shows none: nothing repainted", fakeLastFlushBands(), 0);
	leaveAll();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
}

static void testStallAndBack() {
	section("Hitec screen: a stall and RETRY, BACK in every view, RESTART DSHOT");
	freshHitec(HITEC_D, true);
	checkTrue("the field list on GP28", toList(HITEC_D));
	// The UI stops for 400 ms: the heartbeat lapses and the job switches off.
	for (int i = 0; i < 400; i++) core1Pass();
	hitecFrames(4);
	checkTrue("the stall: the main view with RETRY", untilCtl(UI_FIX(HitecCtl::Retry), 200));
	checkTrue("GP28 off", !enHigh());
	checkControlTable("Hitec stall");
	fakeDumpFrame("shot_hitec_stall.ppm");
	tapId(UI_FIX(HitecCtl::Retry));
	checkTrue("RETRY reads again", untilView(HitecView::List, 1200));

	tapId(UI_FIX(HitecCtl::Files));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from FILES: the field list", uiHitecView() == HitecView::List);
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the write page: the field list", uiHitecView() == HitecView::List);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the list: the leave page", uiHitecView() == HitecView::Leave);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the leave page: the field list", uiHitecView() == HitecView::List);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Leave));
	checkTrue("LEAVE: the unplug page", uiHitecView() == HitecView::Unplug);
	hitecFrame();
	checkTrue("GP28 off on the next frame", !enHigh());
	checkTrue("BACK waits for the 2 s off: disabled, and drawn so",
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	checkControlTable("Hitec unplug, the supply off");
	checkTrue("RESTART DSHOT after the 2 s off", untilCtl(UI_FIX(HitecCtl::Restart), 200));
	checkTrue("BACK acts from that frame on, and is drawn live",
	          ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Live);
	checkControlTable("Hitec unplug");
	fakeDumpFrame("shot_hitec_unplug.ppm");
	tapId(UI_FIX(HitecCtl::Restart));
	hitecFrames(4);
	checkTrue("RESTART DSHOT: MENU", uiCurrentScreen() == UiScreenId::Menu);
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) hitecFrame();
	checkTrue("the claim is free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	hitecFrames(2);
	supply(false);
}

/**
 * @brief Whether the ID note's lines, wrapped at @p width, are drawn in amber:
 *        centred in the editor bar's 136 px from x 52 at y 232, 242, 252
 *        when @p bar, else from x 18 at the first y from 40 to 184 that
 *        holds the first line, 12 px apart.
 */
static bool idNoteDrawn(const char *text, int width, bool bar) {
	char l[3][48];
	int n = kstWrap(text, width, l, 3);
	if (n < 1 || strstr(l[n - 1], "...")) return false;
	if (bar) {
		for (int i = 0; i < n; i++)
			if (!fakeTextIs(52 + (136 - gfxTextW(l[i], 1)) / 2, 218 + 14 + 10 * i, l[i], C_AMBER, C_BG, 1))
				return false;
		return true;
	}
	for (int y = 40; y <= 184; y++) {
		if (!fakeTextIs(UI_MARGIN + 12, y, l[0], C_AMBER, C_BG, 1)) continue;
		for (int i = 1; i < n; i++)
			if (!fakeTextIs(UI_MARGIN + 12, y + 12 * i, l[i], C_AMBER, C_BG, 1)) return false;
		return true;
	}
	return false;
}

/** @brief The y of an amber note line @p text at x 18 on the write page, or -1. */
static int noteLineY(const char *text) {
	for (int y = UI_BODY_Y; y <= 190; y++)
		if (fakeTextIs(UI_MARGIN + 12, y, text, C_AMBER, C_BG, 1)) return y;
	return -1;
}

/** @brief The y the horn line is drawn at on the write page, or -1. */
static int hornLineY() {
	for (int y = UI_BODY_Y; y <= 190; y++)
		if (fakeTextIs(UI_MARGIN, y, "KEEP THE SERVO HORN CLEAR.", C_TEXT, C_BG, 1)) return y;
	return -1;
}

/** @brief The y of value row @p label at x 8 on the write page, or -1. */
static int rowLabelY(const char *label) {
	for (int y = UI_BODY_Y; y <= 190; y++)
		if (fakeTextIs(UI_MARGIN, y, label, C_TEXT, C_BG, 1)) return y;
	return -1;
}

/** @brief The n of `AND n MORE` on the write page, or -1. */
static int andMore() {
	char b[24];
	for (int n = 1; n <= 30; n++) {
		snprintf(b, sizeof(b), "AND %d MORE", n);
		for (int y = UI_BODY_Y; y <= 190; y++)
			if (fakeTextIs(UI_MARGIN, y, b, C_DIM, C_BG, 1)) return n;
	}
	return -1;
}

/** @brief Whether the write page draws nothing from y 192 to the state lines at y 196. */
static bool clearBelow192() {
	const uint16_t *fb = gfxBuffer();
	for (int y = 192; y < 196; y++)
		for (int x = 0; x < GFX_W; x++)
			if (fb[y * GFX_W + x] != C_BG) return false;
	return true;
}

/** @brief The D series value rows the write page lists, by label, from y 62 to 190. */
static int rowsListed() {
	HitecRow rows[HITEC_ROWS_MAX];
	int nr = hitecRows(HITEC_LAYOUT_D, rows), n = 0;
	for (int i = 0; i < nr; i++)
		if (hitecWritten(HITEC_LAYOUT_D, rows[i].param) && rowLabelY(rows[i].label) >= 0) n++;
	return n;
}

/** @brief @p p of @p img to its smallest edit, or its largest when it holds that already. */
static void moveParam(hitec_image_t *img, uint8_t p) {
	const hitec_param_info_t *pi = hitec_param(HITEC_LAYOUT_D, p);
	hitec_value_t v;
	hitec_param_get(img, HITEC_LAYOUT_D, p, &v);
	hitec_param_set(img, HITEC_LAYOUT_D, p, v.value == pi->min ? pi->max : pi->min);
}

/** @brief Put @p img on the card as HITEC/HT001.TXT. */
static void putFile(const hitec_image_t *img) {
	char text[HITEC_CARD_FILE_MAX];
	size_t n = hitec_card_file_text(img, text, sizeof(text));
	fakeFatMkdir("HITEC");
	fakeFatPut("HITEC/HT001.TXT", text, n);
}

/** @brief FILES, the first file, LOAD; true on the field list. */
static bool loadFirstFile() {
	tapId(UI_FIX(HitecCtl::Files));
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(HitecCtl::Load));
	return uiHitecView() == HitecView::List;
}

/**
 * @brief The write page keeps, in this order of priority, the horn line, the
 *        ID note and FILE OF ANOTHER MODEL, MODEL NOT MEASURED, then the
 *        value rows with the ID first, then the soft rules.
 */
static void testWritePageHornLine() {
	section("Hitec write page: what is never left out, and the ID first");
	static const char *const FILE_LINE = "FILE OF ANOTHER MODEL: D485HW.";
	// ID 0 -> 1 and DIRECTION on a D485HW.
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	revealRow(12);
	tapId(UI_ROW(12, Row));
	tapId(UI_FIX(HitecCtl::EditPlus));
	revealRow(6);
	tapId(UI_ROW(6, Row));
	tapId(UI_FIX(HitecCtl::EditPlus));
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("ID and DIRECTION: the write page", uiHitecView() == HitecView::Write);
	int hy = hornLineY();
	checkTrue("the ID row first, at y 62", rowLabelY("ID") == UI_BODY_Y);
	checkTrue("the horn line under the rows", hy > rowLabelY("DIRECTION"));
	checkTrue("then the ID note, whole", idNoteDrawn(HITEC_UI_ID_NOTE_PAGE, 36, false) &&
	          noteLineY("THE BENCH ADDRESSES ID 0 AND MAY NOT") == hy + 12 &&
	          noteLineY("READ OR RESTORE THIS SERVO AGAIN.") == hy + 24);
	checkTrue("nothing below y 192", clearBelow192());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Revert));
	leaveAll();

	// A D645MW, not measured, with a D485HW file loaded, ID 0 -> 1 and
	// DIRECTION: FILE OF ANOTHER MODEL shows nowhere else.
	freshHitec(HITEC_D, false);
	hitec_image_t f;
	d485(&f);
	putFile(&f);
	fakeHitecSim()->mem[0] = (uint8_t)(34645 & 0xFF);
	fakeHitecSim()->mem[1] = (uint8_t)(34645 >> 8);
	checkTrue("a D645MW reads", toList(HITEC_D));
	checkTrue("not measured", !hitecModelMeasuredNow());
	checkTrue("the D485HW file loads", loadFirstFile());
	revealRow(12);
	tapId(UI_ROW(12, Row));
	tapId(UI_FIX(HitecCtl::EditPlus));
	revealRow(6);
	tapId(UI_ROW(6, Row));
	tapId(UI_FIX(HitecCtl::EditPlus));
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("its write page", uiHitecView() == HitecView::Write);
	hy = hornLineY();
	checkTrue("the horn line", hy > 0);
	checkTrue("the ID note, whole", idNoteDrawn(HITEC_UI_ID_NOTE_PAGE, 36, false));
	checkTrue("FILE OF ANOTHER MODEL", noteLineY(FILE_LINE) > hy);
	checkTrue("MODEL NOT MEASURED, whole", noteLineY("MODEL NOT MEASURED: WRITTEN IN THE D") > hy &&
	          noteLineY("LAYOUT.") > hy);
	checkTrue("the ID row first", rowLabelY("ID") == UI_BODY_Y);
	checkTrue("nothing below y 192", clearBelow192());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Revert));
	leaveAll();

	// A LOAD that changes ten values, the ID among them, on a D485HW.
	static const uint8_t TEN[] = {
		HITEC_P_D_SPEED, HITEC_P_D_DEAD_BAND, HITEC_P_D_SOFT_START, HITEC_P_D_NEUTRAL,
		HITEC_P_D_DIRECTION, HITEC_P_D_FAILSAFE, HITEC_P_D_OVERLOAD, HITEC_P_D_SMART_SENSOR,
		HITEC_P_D_SENSITIVITY, HITEC_P_D_ID,
	};
	freshHitec(HITEC_D, false);
	hitec_image_t servo;
	d485(&servo);
	d485(&f);
	for (uint8_t p : TEN) moveParam(&f, p);
	int changed = hitecChangedCount(&servo, &f, HITEC_LAYOUT_D);
	putFile(&f);
	checkTrue("the field list", toList(HITEC_D));
	checkTrue("ten or more values change", changed >= 10);
	checkTrue("the file loads", loadFirstFile());
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("the write page", uiHitecView() == HitecView::Write);
	checkTrue("the ID row first", rowLabelY("ID") == UI_BODY_Y);
	checkTrue("AND n MORE counts the rows not listed", andMore() == changed - rowsListed());
	checkTrue("4 rows at most, as KST's page", rowsListed() == HITEC_PAGE_ROWS_MAX && rowsListed() == 4);
	{
		char want[24];
		snprintf(want, sizeof(want), "%d CHANGES", changed);
		checkTrue("the strip counts CHANGES, the leave page's word",
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_DIM, C_BG, 1));
	}
	checkTrue("the horn line", hornLineY() > 0);
	checkTrue("the ID note, whole", idNoteDrawn(HITEC_UI_ID_NOTE_PAGE, 36, false));
	checkTrue("nothing below y 192", clearBelow192());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Revert));
	leaveAll();

	// Everything at once: a D645MW whose FAILSAFE is a position, a D485HW
	// file moving ten values, the ID and the travel among them, and a
	// FAILSAFE row that wraps.
	freshHitec(HITEC_D, false);
	d485(&servo);
	hitec_image_put_reg(&servo, 0x00, 34645);
	hitec_param_set(&servo, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 900);
	memcpy(fakeHitecSim()->mem, servo.mem, sizeof(fakeHitecSim()->mem));
	d485(&f);
	for (uint8_t p : TEN) moveParam(&f, p);
	hitec_param_set(&f, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 2100);
	putFile(&f);
	checkTrue("a D645MW reads", toList(HITEC_D) && !hitecModelMeasuredNow());
	checkTrue("the file loads", loadFirstFile());
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("the write page", uiHitecView() == HitecView::Write);
	hy = hornLineY();
	changed = hitecChangedCount(&servo, &f, HITEC_LAYOUT_D);
	checkTrue("the ID row first", rowLabelY("ID") == UI_BODY_Y);
	checkTrue("AND n MORE counts the rows not listed", andMore() == changed - rowsListed());
	checkTrue("the horn line", hy > 0);
	checkTrue("the ID note, whole", idNoteDrawn(HITEC_UI_ID_NOTE_PAGE, 36, false));
	checkTrue("FILE OF ANOTHER MODEL", noteLineY(FILE_LINE) > hy);
	checkTrue("MODEL NOT MEASURED, whole", noteLineY("MODEL NOT MEASURED: WRITTEN IN THE D") > hy &&
	          noteLineY("LAYOUT.") > hy);
	checkTrue("nothing below y 192", clearBelow192());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Revert));
	leaveAll();

	// The same notes over a row that wraps: ID, then FAILSAFE 900 US -> 2100
	// US under its label, then two more.
	freshHitec(HITEC_D, false);
	memcpy(fakeHitecSim()->mem, servo.mem, sizeof(fakeHitecSim()->mem));
	d485(&f);
	moveParam(&f, HITEC_P_D_ID);
	moveParam(&f, HITEC_P_D_OVERLOAD);
	moveParam(&f, HITEC_P_D_SMART_SENSOR);
	hitec_param_set(&f, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 2100);
	putFile(&f);
	checkTrue("a D645MW reads", toList(HITEC_D) && !hitecModelMeasuredNow());
	checkTrue("the file loads", loadFirstFile());
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("the write page", uiHitecView() == HitecView::Write);
	fakeDumpFrame("shot_hitec_write_full.ppm");
	hy = hornLineY();
	changed = hitecChangedCount(&servo, &f, HITEC_LAYOUT_D);
	checkTrue("the ID row first, FAILSAFE second", rowLabelY("ID") == UI_BODY_Y &&
	          rowLabelY("FAILSAFE") == UI_BODY_Y + 12);
	checkTrue("FAILSAFE's value under its label: the old value dim, the new cyan",
	          fakeChangeIs(UI_BODY_Y + 22, "900 US (900)", "2100 US (2100)", C_BG));
	checkTrue("AND n MORE counts the rest", andMore() == changed - rowsListed() && andMore() > 0);
	checkTrue("the horn line, the ID note, FILE, MODEL",
	          hy > 0 && idNoteDrawn(HITEC_UI_ID_NOTE_PAGE, 36, false) && noteLineY(FILE_LINE) > hy &&
	          noteLineY("LAYOUT.") > hy);
	checkTrue("nothing below y 192", clearBelow192());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Revert));
	leaveAll();
}

static void testIdAndBack() {
	section("Hitec screen: the ID note, STOP, BACK in the entry and the read, a kept edit");
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	revealRow(12);
	tapId(UI_ROW(12, Row));
	checkTrue("ID 0: no ID note in the editor bar", !idNoteDrawn(HITEC_UI_ID_NOTE_BAR, 22, true));
	tapId(UI_FIX(HitecCtl::EditPlus));
	hitecFrame();
	checkTrue("ID 1: the editor bar says a write may lock out read and restore",
	          idNoteDrawn(HITEC_UI_ID_NOTE_BAR, 22, true));
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("an ID write: its page", uiHitecView() == HitecView::Write);
	checkTrue("the page says the bench may not read or restore the servo after it",
	          idNoteDrawn(HITEC_UI_ID_NOTE_PAGE, 36, false));
	fakeDumpFrame("shot_hitec_id.ppm");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::Revert));

	// An edit survives READ AGAIN, and FILES during the read does not fail it.
	speedDown();
	tapId(UI_FIX(UI_BACK));
	checkTrue("the leave page", uiHitecView() == HitecView::Leave);
	tapId(UI_FIX(HitecCtl::LeaveRead));
	hitecFrame();
	checkTrue("the read runs", job().job == HitecJobState::Busy);
	tapId(UI_FIX(HitecCtl::Files));
	tapId(UI_FIX(UI_BACK));
	for (int i = 0; i < 800 && job().job != HitecJobState::InMode; i++) hitecFrame();
	hitecFrames(2);
	checkTrue("the read ended in the mode", job().job == HitecJobState::InMode && job().result == HITEC_SES_OK);
	checkTrue("the edit is still pending", ctlEnabled(UI_FIX(HitecCtl::Write)));
	tapId(UI_FIX(HitecCtl::Revert));

	// A DIRECTION reversal kept through READ AGAIN keeps its mirrored ends.
	revealRow(6);
	tapId(UI_ROW(6, Row));
	tapId(UI_FIX(HitecCtl::EditPlus));
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(HitecCtl::LeaveRead));
	hitecFrame();
	for (int i = 0; i < 800 && job().job != HitecJobState::InMode; i++) hitecFrame();
	hitecFrames(2);
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the reversal writes", untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && job().result == HITEC_SES_OK);
	hitec_image_t want;
	d485(&want);
	hitec_param_set(&want, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION, HITEC_CCW);
	hitec_image_t got = jobImage();
	bool same = true;
	for (uint8_t p : { (uint8_t)HITEC_P_D_DIRECTION, (uint8_t)HITEC_P_D_NEUTRAL, (uint8_t)HITEC_P_D_LEFT_END,
	                   (uint8_t)HITEC_P_D_RIGHT_END })
		if (hitecDiffers(&want, &got, HITEC_LAYOUT_D, p)) same = false;
	checkTrue("the servo holds the reversal with its ends mirrored", same);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();

	// STOP during the entry, BACK during the entry.
	freshHitec(HITEC_D, true);
	openHitec();
	tapId(UI_FIX(HitecCtl::DSeries));
	checkTrue("the entry offers STOP", untilCtl(UI_FIX(HitecCtl::Stop), 200));
	tapId(UI_FIX(HitecCtl::Stop));
	checkTrue("STOP: RETRY", untilCtl(UI_FIX(HitecCtl::Retry), 400));
	checkTrue("the supply is off", !enHigh());
	tapId(UI_FIX(HitecCtl::Retry));
	checkTrue("RETRY enters again", untilCtl(UI_FIX(HitecCtl::Stop), 200));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK during the entry: the unplug page", uiHitecView() == HitecView::Unplug);
	checkTrue("RESTART DSHOT after the off time", untilCtl(UI_FIX(HitecCtl::Restart), 200));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK on the unplug page: a new session on the main view", uiHitecView() == HitecView::Main);
	checkTrue("it reads the servo again", untilView(HitecView::List, 1200));
	leaveAll();

	// BACK during the entry's own read: it stops, the leave page follows.
	freshHitec(HITEC_D, false);
	openHitec();
	tapId(UI_FIX(HitecCtl::DSeries));
	for (int i = 0; i < 400 && job().job != HitecJobState::Busy; i++) hitecFrame();
	hitecFrames(2);
	checkTrue("the entry's read runs", job().job == HitecJobState::Busy && uiHitecView() == HitecView::Main);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the leave page", uiHitecView() == HitecView::Leave);
	for (int i = 0; i < 200 && job().job == HitecJobState::Busy; i++) hitecFrame();
	checkTrue("the read stopped", job().job == HitecJobState::InMode && !job().hasImage);
	checkTrue("READ AGAIN acts", ctlEnabled(UI_FIX(HitecCtl::LeaveRead)));
	tapId(UI_FIX(HitecCtl::LeaveRead));
	checkTrue("READ AGAIN: the field list", untilView(HitecView::List, 1200));
	leaveAll();
}

static void testCardSwap() {
	section("Hitec screen: another card in the slot since the backup");
	fakeCacheReset();
	fakeVolumeSet(1);
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the first write", untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && fakeFatExists("BACKUP/HT001.TXT"));
	tapId(UI_FIX(HitecCtl::WriteOk));

	// Another serial: the card is mounted again and the backup saved to it.
	fakeVolumeSet(2);
	unsigned recovers = fakeCacheCalls().recovers;
	unsigned writes = fakeHitecSim()->writes;
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("another card: mounted again, the backup saved to it, then the write",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && fakeCacheCalls().recovers == recovers + 1 &&
	          fakeFatExists("BACKUP/HT002.TXT") && job().result == HITEC_SES_OK &&
	          fakeHitecSim()->writes > writes);
	tapId(UI_FIX(HitecCtl::WriteOk));

	// Swapped while a batch was queued: the check at the flush's end.
	fakeCacheSet(CardCache::Flushing, 2);
	recovers = fakeCacheCalls().recovers;
	writes = fakeHitecSim()->writes;
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(2);
	checkTrue("a batch queued: no write frame", fakeHitecSim()->writes == writes);
	fakeVolumeSet(3);
	fakeCacheSet(CardCache::Idle, 0);
	checkTrue("swapped during the wait: mounted again, the backup saved, then the write",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && fakeCacheCalls().recovers == recovers + 1 &&
	          fakeFatExists("BACKUP/HT003.TXT") && job().result == HITEC_SES_OK);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();
	fakeVolumeSet(1);
	fakeCacheReset();
}

static void testStallInWrite() {
	section("Hitec screen: a stall during a write");
	freshHitec(HITEC_D, true);
	checkTrue("the field list on GP28", toList(HITEC_D));
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the write runs", job().job == HitecJobState::Busy);
	// The UI stops for 2 s: the job finishes the write, then switches off.
	for (int i = 0; i < 2000; i++) core1Pass();
	hitecFrames(4);
	checkTrue("the stall: RETRY", untilCtl(UI_FIX(HitecCtl::Retry), 200));
	checkTrue("GP28 off", !enHigh());
	checkTrue("the job says it wrote", job().stallWrote);
	checkTrue("the record stays on the card", fakeFatExists("HITEC/HITEC.TXT"));
	leaveAll();
}

/** @brief The board's voltage at @p volts, held for enough frames to settle. */
static void boardVolts(float volts) {
	fakeAdcSet((uint16_t)(volts / 3.0f / 3.3f * 4095.0f + 0.5f));
	hitecFrames(60);
}

/** @brief Hold @p id for 50 frames, 1250 ms, and let go. */
static void holdFor(UiControlId id) {
	int x, y;
	ctlCentre(id, &x, &y);
	fakePress(x, y);
	hitecFrame();
	for (int i = 0; i < 50; i++) { fakeHold(x, y); hitecFrame(); }
	fakeRelease();
	hitecFrames(2);
}

/**
 * @brief The BOARD voltage red refuses the write page's hold, a write and a
 *        restore: no file and no write, two amber lines in the page's state
 *        lines. Amber, no cell and the text colour write. A write that waits
 *        for its backup's flush is refused when the voltage turns red
 *        meanwhile; a write already running is not stopped.
 */
static void testBoardLow() {
	section("Hitec screen: no write or restore while the BOARD voltage is red");
	fakeCacheReset();
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	int32_t before = jobSpeed();
	speedDown();
	checkTrue("the write page", tapUntilGone(UI_FIX(HitecCtl::Write)));
	boardVolts(3.20f);
	checkTrue("3.20 V: red", uiBoardCellNow() == BoardCell::Critical);
	unsigned writes0 = fakeHitecSim()->writes;
	holdFor(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the hold writes nothing: no backup, no record, no write",
	          !fakeFatExists("BACKUP/HT001.TXT") && !fakeFatExists("HITEC/HITEC.TXT") &&
	          fakeHitecSim()->writes == writes0 && jobSpeed() == before);
	checkTrue(UI_BOARD_LOW_1 ", amber, in the state lines", lineIs(196, UI_BOARD_LOW_1, C_AMBER));
	checkTrue(UI_BOARD_LOW_2 ", amber", lineIs(208, UI_BOARD_LOW_2, C_AMBER));
	int x0 = 0, x1 = 0;
	checkTrue("both inside the panel's 8 px margins",
	          fakeInkExtent(196, 20, C_BG, &x0, &x1) && x0 >= 8 && x1 < GFX_W - 8);
	checkTrue("HOLD TO WRITE stays", ctlEnabled(UI_FIX(HitecCtl::WriteHold)));
	checkTrue("the horn line stays: nothing was written", hornLineY() >= 0);
	checkControlTable("Hitec write page, board low");
	fakeDumpFrame("shot_hitec_board_low.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the field list", uiHitecView() == HitecView::List);
	checkTrue("the write page again: no refusal lines before a hold",
	          tapUntilGone(UI_FIX(HitecCtl::Write)) && uiHitecView() == HitecView::Write &&
	          !lineIs(196, UI_BOARD_LOW_1, C_AMBER) && !lineIs(208, UI_BOARD_LOW_2, C_AMBER));

	// Amber and no cell to judge: each writes.
	const float volts[] = { 3.40f, 1.00f };
	const BoardCell band[] = { BoardCell::Low, BoardCell::NoCell };
	const char *what[] = { "3.40 V, amber: the hold writes", "1.00 V, no cell: the hold writes" };
	for (int k = 0; k < 2; k++) {
		boardVolts(volts[k]);
		int32_t was = jobSpeed();
		if (k > 0) {
			speedDown();
			tapUntilGone(UI_FIX(HitecCtl::Write));
		}
		checkTrue(what[k], uiBoardCellNow() == band[k] && holdUntilGone(UI_FIX(HitecCtl::WriteHold)) &&
		          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && job().result == HITEC_SES_OK &&
		          jobSpeed() < was);
		tapId(UI_FIX(HitecCtl::WriteOk));
	}
	checkTrue("the backup went first", fakeFatExists("BACKUP/HT001.TXT"));

	// 3.92 V writes; a write already running is not stopped by red.
	boardVolts(3.92f);
	int32_t was = jobSpeed();
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("3.92 V: the hold starts the write", uiBoardCellNow() == BoardCell::Ok &&
	          holdUntilGone(UI_FIX(HitecCtl::WriteHold)));
	fakeAdcSet((uint16_t)(3.20f / 3.0f / 3.3f * 4095.0f + 0.5f));
	for (int i = 0; i < 40 && uiBoardCellNow() != BoardCell::Critical; i++) hitecFrame();
	checkTrue("the band turns red while the write runs", uiBoardCellNow() == BoardCell::Critical &&
	          job().job == HitecJobState::Busy && !ctlPresent(UI_FIX(HitecCtl::WriteOk)));
	checkTrue("red while it runs: the write ends verified",
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && job().result == HITEC_SES_OK &&
	          jobSpeed() < was);
	tapId(UI_FIX(HitecCtl::WriteOk));

	// RESTORE goes through the write page: refused the same way.
	hitecFrames(60);
	checkTrue("red again", uiBoardCellNow() == BoardCell::Critical);
	int32_t written = jobSpeed();
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("a tap on RESTORE... opens the restore page", tapUntilGone(UI_FIX(HitecCtl::Restore)) &&
	          uiHitecView() == HitecView::Write);
	holdFor(UI_FIX(HitecCtl::WriteHold));
	checkTrue("and its hold restores nothing", jobSpeed() == written &&
	          lineIs(196, UI_BOARD_LOW_1, C_AMBER));
	boardVolts(3.92f);
	checkTrue("3.92 V: the hold restores", holdUntilGone(UI_FIX(HitecCtl::WriteHold)) &&
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && jobSpeed() == before);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();

	section("Hitec screen: the BOARD voltage turns red while the backup flushes");
	fakeCacheReset();
	freshHitec(HITEC_D, false);
	SdLogStatus logging;
	memset(&logging, 0, sizeof(logging));
	logging.state = SdLogState::Logging;
	fakeSdLogSet(&logging);
	checkTrue("the field list", toList(HITEC_D));
	before = jobSpeed();
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes0 = fakeHitecSim()->writes;
	checkTrue("the hold at 3.92 V queues the backup", holdUntilGone(UI_FIX(HitecCtl::WriteHold)) &&
	          lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER));
	boardVolts(3.20f);
	fakeCacheSet(CardCache::Idle, 0);
	hitecFrames(5);
	checkTrue("red when the flush ends: no write, the two lines",
	          fakeHitecSim()->writes == writes0 && jobSpeed() == before &&
	          lineIs(196, UI_BOARD_LOW_1, C_AMBER) && ctlEnabled(UI_FIX(HitecCtl::WriteHold)));
	boardVolts(3.92f);
	checkTrue("3.92 V: the next hold writes", holdUntilGone(UI_FIX(HitecCtl::WriteHold)) &&
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && jobSpeed() < before);
	tapId(UI_FIX(HitecCtl::WriteOk));

	// A hold at red while an earlier batch flushes: refused at once, not
	// after the flush.
	before = jobSpeed();
	speedDown();
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 5);
	boardVolts(3.20f);
	writes0 = fakeHitecSim()->writes;
	holdFor(UI_FIX(HitecCtl::WriteHold));
	checkTrue("a hold at red during a flush: the refusal at once, no wait",
	          lineIs(196, UI_BOARD_LOW_1, C_AMBER) && lineIs(208, UI_BOARD_LOW_2, C_AMBER) &&
	          !lineIs(196, "SAVING THE FILES TO THE CARD.", C_AMBER) &&
	          ctlEnabled(UI_FIX(HitecCtl::WriteHold)) && fakeHitecSim()->writes == writes0);
	fakeCacheSet(CardCache::Idle, 0);
	hitecFrames(3);
	checkTrue("the flush ends: still nothing written, no record",
	          fakeHitecSim()->writes == writes0 && !fakeFatExists("HITEC/HITEC.TXT") &&
	          jobSpeed() == before);
	boardVolts(3.92f);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the field list, the change kept", uiHitecView() == HitecView::List);

	// A later write waits for an earlier batch before its record goes in:
	// red when that flush ends, the record stays off the card too.
	tapUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 5);
	writes0 = fakeHitecSim()->writes;
	checkTrue("no record on the card after the verified write", !fakeFatExists("HITEC/HITEC.TXT"));
	checkTrue("the hold at 3.92 V waits for the earlier batch",
	          holdUntilGone(UI_FIX(HitecCtl::WriteHold)) &&
	          lineIs(196, "SAVING THE FILES TO THE CARD.", C_AMBER));
	boardVolts(3.20f);
	fakeCacheSet(CardCache::Idle, 0);
	hitecFrames(5);
	checkTrue("red when it ends: no record, no write, the two lines",
	          !fakeFatExists("HITEC/HITEC.TXT") && fakeHitecSim()->writes == writes0 &&
	          jobSpeed() == before && lineIs(196, UI_BOARD_LOW_1, C_AMBER));
	boardVolts(3.92f);
	checkTrue("3.92 V: the hold writes", holdUntilGone(UI_FIX(HitecCtl::WriteHold)) &&
	          untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && jobSpeed() < before);
	tapId(UI_FIX(HitecCtl::WriteOk));
	leaveAll();
	fakeCacheReset();
	fakeAdcSet(fakeAdcDefault());
}

/** @brief The framebuffer's pixel at (@p x, @p y). */
static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/** @brief Whether any pixel of the rectangle is @p col. */
static bool regionHas(int x, int y, int w, int h, uint16_t col) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (pixel(i, j) == col) return true;
	return false;
}

/** @brief The D series list row labelled @p label, or -1. */
static int dRowOf(const char *label) {
	HitecRow rows[HITEC_ROWS_MAX];
	int n = hitecRows(HITEC_LAYOUT_D, rows);
	for (int i = 0; i < n; i++)
		if (!strcmp(rows[i].label, label)) return i;
	return -1;
}

/**
 * @brief The colour roles on HITEC SERVO (docs/design/menu-b.md, decisions 8
 *        and 11): the headline amber, PROG MODE on blue, a row's value by its
 *        state with its bar and no `*`, the editor bar's value likewise,
 *        REVERT amber.
 */
static void testColourRoles() {
	section("Hitec screen: the colour roles");
	freshHitec(HITEC_D, true);
	openHitec();
	tapId(UI_FIX(HitecCtl::DSeries));
	hitecFrames(2);
	checkTrue("SUPPLY OFF in amber, the current step",
	          fakeTextCentredIs(70, "SUPPLY OFF", C_AMBER, C_BG, 2));
	checkTrue("the field list", untilView(HitecView::List, 800));
	hitecFrames(1);
	const int cw = gfxTextW("PROG MODE", 1) + 12, cx = GFX_W - UI_MARGIN - cw;
	checkTrue("PROG MODE on blue, a square 65 px chip from x 167",
	          cx == 167 && fakeTextIs(cx + 6, 44, "PROG MODE", C_ONACCENT, C_BLUE, 1) &&
	          pixel(cx, 42) == C_BLUE && pixel(cx + cw - 1, 53) == C_BLUE && pixel(cx, 41) == C_BG);
	checkTrue("the strip's caption ends 4 px or more before the chip",
	          !regionHas(cx - 4, UI_STRIP_Y, 4, UI_STRIP_H, C_DIM));

	const int sp = dRowOf("SPEED"), db = dRowOf("DIRECTION");
	checkTrue("SPEED and DIRECTION rows", sp >= 0 && db >= 0);
	revealRow(sp);
	tapId(UI_ROW(sp, Row));
	hitecFrames(1);
	UiRect r = ctlRect(UI_ROW(sp, Row));
	auto valueHas = [&](const UiRect &q, uint16_t col) { return regionHas(120, q.y + 13, 112, 14, col); };
	checkTrue("selected, as read: the value in the text colour, never lime; no bar",
	          valueHas(r, C_TEXT) && !valueHas(r, C_LIME) && pixel(1, r.y + 5) == C_PANEL);
	checkTrue("the editor bar's value in the text colour",
	          regionHas(52, 234, 136, 14, C_TEXT) && !regionHas(52, 234, 136, 14, C_LIME));
	tapId(UI_FIX(HitecCtl::EditMinus));
	hitecFrames(1);
	r = ctlRect(UI_ROW(sp, Row));
	checkTrue("-: changed, the value cyan, a 3 px cyan bar",
	          valueHas(r, C_CYAN) && !valueHas(r, C_TEXT) && pixel(0, r.y + 5) == C_CYAN &&
	          pixel(2, r.y + 5) == C_CYAN && pixel(3, r.y + 5) == C_PANEL);
	checkTrue("the label carries no *: SPEED from x 8 in the text colour",
	          fakeTextIs(UI_MARGIN, r.y + 21, "SPEED", C_TEXT, C_PANEL, 1));
	checkTrue("the editor bar's value cyan",
	          regionHas(52, 234, 136, 14, C_CYAN) && !regionHas(52, 234, 136, 14, C_LIME));
	const UiRect rv = ctlRect(UI_FIX(HitecCtl::Revert));
	checkTrue("REVERT's label amber",
	          fakeTextIs(rv.x + (rv.w - gfxTextW("REVERT", 1)) / 2, rv.y + (rv.h - 7) / 2,
	                     "REVERT", C_AMBER, C_PANEL, 1));
	tapId(UI_FIX(HitecCtl::Revert));
	revealRow(db);
	tapId(UI_ROW(db, Row));
	tapId(UI_FIX(ctlEnabled(UI_FIX(HitecCtl::EditMinus)) ? HitecCtl::EditMinus : HitecCtl::EditPlus));
	hitecFrames(1);
	r = ctlRect(UI_ROW(db, Row));
	checkTrue("DIRECTION reversed, a soft rule: the bar amber, the value cyan",
	          valueHas(r, C_CYAN) && pixel(1, r.y + 5) == C_AMBER);
	checkTrue("the rule's top line in cyan, the new value", regionHas(52, 220, 136, 7, C_CYAN));
	tapId(UI_FIX(HitecCtl::Revert));
	HitecRow rows[HITEC_ROWS_MAX];
	int n = hitecRows(HITEC_LAYOUT_D, rows), ro = -1;
	for (int i = 0; i < n && ro < 0; i++)
		if (!hitecEditable(HITEC_LAYOUT_D, rows[i].param)) ro = i;
	checkTrue("a row the bench does not write", ro >= 0);
	revealRow(ro);
	hitecFrames(1);
	r = ctlRect(UI_ROW(ro, Row));
	checkTrue("read-only: its label and value grey",
	          fakeTextIs(UI_MARGIN, r.y + 21, rows[ro].label, C_GRID, C_BG, 1) &&
	          !valueHas(r, C_TEXT) && !valueHas(r, C_LIME));
	leaveAll();

	// A SPEED the encoding does not take, 1234 at 0x54: RAW 1234, amber in
	// the row and in the editor bar alike.
	freshHitec(HITEC_D, false);
	fakeHitecSim()->mem[0x54] = 0xD2;
	fakeHitecSim()->mem[0x55] = 0x04;
	checkTrue("a raw SPEED: the field list", toList(HITEC_D));
	revealRow(sp);
	tapId(UI_ROW(sp, Row));
	hitecFrames(1);
	r = ctlRect(UI_ROW(sp, Row));
	checkTrue("RAW 1234 amber in the row",
	          fakeTextIs(GFX_W - UI_MARGIN - gfxTextW("RAW 1234", 2), r.y + 13, "RAW 1234", C_AMBER,
	                     C_PANEL, 2));
	checkTrue("and amber in the editor bar, as the row",
	          regionHas(52, 234, 136, 14, C_AMBER) && !regionHas(52, 234, 136, 14, C_TEXT));
	leaveAll();
}

/**
 * @brief The strip's room at its edge beside PROG MODE: a D series model
 *        rcbench does not name reads `D SERIES` and its number, so the
 *        number's digits set the strip's width in 6 px steps. With 2 digits
 *        `D SERIES 42  BACKUP IN RAM` is 155 px and keeps the backup, ending
 *        4 px before the chip; with 3, 161 px, it does not fit, and the model
 *        stays alone. Widths at scale 1 are 6 n - 1 px, so no strip ends
 *        1 to 3 px before the chip: the room's 4 px term and a room of 159 px
 *        draw the same strips.
 */
static void testStripRoom() {
	section("Hitec screen: the strip's room beside PROG MODE, 155 px");
	const struct { uint16_t number; const char *want; } cases[] = {
		{ 42, "D SERIES 42  BACKUP IN RAM" },
		{ 420, "D SERIES 420" },
	};
	for (const auto &c : cases) {
		freshHitec(HITEC_D, false);
		fakeHitecSim()->mem[0x00] = (uint8_t)(c.number & 0xFF);
		fakeHitecSim()->mem[0x01] = (uint8_t)(c.number >> 8);
		char what[96];
		snprintf(what, sizeof(what), "model number %u: the field list", c.number);
		checkTrue(what, toList(HITEC_D));
		hitecFrames(1);
		snprintf(what, sizeof(what), "the strip reads %s, %d px, and nothing after it", c.want, gfxTextW(c.want, 1));
		const int end = UI_MARGIN + gfxTextW(c.want, 1);
		checkTrue(what, fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, c.want, C_DIM, C_BG, 1) &&
		          !regionHas(end, UI_STRIP_Y, uiChipX(HITEC_UI_CHIP) - end, UI_STRIP_H, C_DIM));
		leaveAll();
	}
	checkInt("D SERIES 42  BACKUP IN RAM: 155 px, to x 162", gfxTextW("D SERIES 42  BACKUP IN RAM", 1), 155);
	checkInt("D SERIES 420  BACKUP IN RAM: 161 px", gfxTextW("D SERIES 420  BACKUP IN RAM", 1), 161);
}

/**
 * @brief Another claim holds the pins: HITEC SERVO, opened by hand since
 *        MENU refuses its row, says to wait for DShot's restart.
 */
static void testBusyRefusal() {
	section("HITEC screen: another claim holds the pins");
	freshHitec(HITEC_D, false);
	checkTrue("another claim holds the pins", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	TouchState none;
	memset(&none, 0, sizeof(none));
	UI_HITEC.enter();
	UI_HITEC.tick(&none);
	checkTrue("HITEC refused: WAIT: DSHOT RESTARTING, amber",
	          pinClaimHolder() == PinUser::Am32 && lineIs(UI_STRIP_Y + 4, UI_WAIT_RESTART_TEXT, C_AMBER));
	pinClaimEnd(PinUser::Am32);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) hitecFrames(1);
	freshHitec(HITEC_D, false);
}

void runHitecUiTests() {
	testFamily();
	testDSeriesRead();
	testWrite();
	testFlushWait();
	testWriteError();
	testFiles();
	testRcbenchName();
	testNoCardWrite();
	testRecordAtEntry();
	testClassic();
	testSupplyCurrent();
	testStallAndBack();
	testIdAndBack();
	testWritePageHornLine();
	testCardSwap();
	testStallInWrite();
	testBoardLow();
	testColourRoles();
	testStripRoom();
	testBusyRefusal();
}
