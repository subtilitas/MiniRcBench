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
#include "pin_claim.h"
#include "sd_log.h"
#include "settings.h"
#include "theme.h"
#include "ui.h"
#include "ui_hitec.h"
#include "ui_menu.h"
#include "ui_screen.h"
#include "usb_msc.h"
#include "servo_supply.h"
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
	section("Hitec screen: MENU opens it, the family");
	freshHitec(HITEC_D, false);
	openHitec();
	checkTrue("HITEC SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Hitec);
	checkTrue("on the family view", uiHitecView() == HitecView::Family);
	checkTrue("no pin claimed yet", pinClaimState() == ClaimState::Free);
	checkTrue("CLASSIC disabled with NONE", !ctlEnabled(UI_FIX(HitecCtl::Classic)));
	checkTrue("D SERIES acts", ctlEnabled(UI_FIX(HitecCtl::DSeries)));
	checkControlTable("Hitec family");
	fakeDumpFrame("shot_hitec_family.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	hitecFrames(2);

	freshHitec(HITEC_CLASSIC, true);
	openHitec();
	checkTrue("with GP28: CLASSIC acts", ctlEnabled(UI_FIX(HitecCtl::Classic)));
	fakeDumpFrame("shot_hitec_family_classic.ppm");
	leaveAll();
}

static void testDSeriesRead() {
	section("Hitec screen: a D series servo on its own supply, the claim, the read");
	freshHitec(HITEC_D, false);
	openHitec();
	tapId(UI_FIX(HitecCtl::DSeries));
	checkTrue("D SERIES: the main view", uiHitecView() == HitecView::Main);
	checkTrue("it claims SIG", pinClaimHolder() == PinUser::Hitec);
	hitecFrames(4);
	checkControlTable("Hitec main");
	checkTrue("the read ends on the field list", untilView(HitecView::List, 800));
	HitecStatus st = job();
	checkTrue("in the mode with the image and the backup",
	          st.job == HitecJobState::InMode && st.hasImage && st.hasBackup);
	checkTrue("GP28 untouched", !enHigh());
	checkControlTable("Hitec list");
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
	section("Hitec screen: HOLD TO WRITE, the backup and record first, the result");
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	int32_t before = jobSpeed();
	speedDown();

	// Not at 750 ms, not on a slide off.
	pressId(UI_FIX(HitecCtl::Write));
	int x, y;
	ctlCentre(UI_FIX(HitecCtl::Write), &x, &y);
	for (int i = 0; i < 29; i++) { fakeHold(x, y); hitecFrame(); }
	fakeRelease(); hitecFrame();
	checkTrue("a 750 ms hold opens nothing", uiHitecView() == HitecView::List);
	pressId(UI_FIX(HitecCtl::Write));
	for (int i = 0; i < 20; i++) { fakeHold(x, y); hitecFrame(); }
	for (int i = 0; i < 30; i++) { fakeHold(x, 120); hitecFrame(); }
	fakeRelease(); hitecFrame();
	checkTrue("a hold slid off opens nothing", uiHitecView() == HitecView::List);

	checkTrue("a full hold opens the write page", holdUntilGone(UI_FIX(HitecCtl::Write)));
	checkTrue("the write page", uiHitecView() == HitecView::Write);
	checkControlTable("Hitec write page");
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("a second write ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	checkTrue("no second backup", !fakeFatExists("BACKUP/HT002.TXT"));
	tapId(UI_FIX(HitecCtl::WriteOk));

	// A later write while the record removal still flushes: the flush ends,
	// then this write's record, then its frames.
	speedDown();
	holdUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 4);
	s_writesAtRecord = -1;
	fakeFatOnCall(watchRecord);
	unsigned writes1 = fakeHitecSim()->writes;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(4);
	checkTrue("an earlier batch flushing: no write frame yet", fakeHitecSim()->writes == writes1);
	fakeCacheSet(CardCache::Idle, 0);
	checkTrue("then the write ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 800));
	fakeFatOnCall(nullptr);
	checkTrue("its record stood on the card before its first write frame",
	          s_writesAtRecord == (int)writes1);
	checkTrue("verified", job().result == HITEC_SES_OK);
	tapId(UI_FIX(HitecCtl::WriteOk));

	// BACK during a write: the write ends, then the leave page.
	speedDown();
	holdUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the write runs", job().job == HitecJobState::Busy);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK keeps the write page while it runs", uiHitecView() == HitecView::Write);
	checkControlTable("Hitec writing");
	checkTrue("the leave page after the write", untilView(HitecView::Leave, 800));
	checkTrue("the write ended verified", job().result == HITEC_SES_OK);
	leaveAll();
}

static void testFlushWait() {
	section("Hitec screen: the write waits for the backup's flush to the card");
	fakeCacheReset();
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	checkTrue("the claim turned the cache on", fakeCacheCalls().begins >= 1 &&
	                                          cardCacheState() == CardCache::Idle);
	speedDown();
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = fakeHitecSim()->writes;
	unsigned recovers = fakeCacheCalls().recovers;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	hitecFrames(2);
	fakeCacheSet(CardCache::Failed, 0);
	hitecFrames(2);
	checkTrue("a failed flush: no write frame, the card mounted again",
	          fakeHitecSim()->writes == writes && fakeCacheCalls().recovers == recovers + 1);
	checkTrue("and the page says why", ctlPresent(UI_FIX(HitecCtl::WriteOk)) &&
	          lineIs(196, "THE CARD DID NOT TAKE THE BACKUP.", C_AMBER));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	untilCtl(UI_FIX(HitecCtl::WriteOk), 800);
	tapId(UI_FIX(HitecCtl::WriteOk));
	checkTrue("written", jobSpeed() < before);
	tapId(UI_FIX(HitecCtl::Files));
	checkTrue("RESTORE acts once the servo differs from the backup", ctlEnabled(UI_FIX(HitecCtl::Restore)));
	checkTrue("HOLD TO RESTORE opens the write page", holdUntilGone(UI_FIX(HitecCtl::Restore)));
	checkTrue("the restore page", uiHitecView() == HitecView::Write);
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	leaveAll();
	fakeMscSet(MscState::Idle, 0);

	// A card that takes no file stops the write.
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	speedDown();
	holdUntilGone(UI_FIX(HitecCtl::Write));
	fakeFatWriteProtect(true);
	unsigned writes0 = fakeHitecSim()->writes;
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("a card that takes no file: a note and OK", untilCtl(UI_FIX(HitecCtl::WriteOk), 200));
	checkTrue("nothing written to the servo", fakeHitecSim()->writes == writes0);
	fakeFatWriteProtect(false);
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the write ends", untilCtl(UI_FIX(HitecCtl::WriteOk), 1200));
	hitec_value_t v;
	HitecStatus st = job();
	checkTrue("the checksum reads OK",
	          hitec_param_get(&st.image, HITEC_LAYOUT_HS5, HITEC_P_C_CHECKSUM, &v) == HITEC_GET_OK &&
	          v.value == HITEC_SUM_OK);
	tapId(UI_FIX(HitecCtl::WriteOk));

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
	holdUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("its write page", uiHitecView() == HitecView::Write);
	fakeDumpFrame("shot_hitec_not_measured.ppm");
	tapId(UI_FIX(UI_BACK));
	leaveAll();
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	checkTrue("RESTART DSHOT after the 2 s off", untilCtl(UI_FIX(HitecCtl::Restart), 200));
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

static void testIdAndBack() {
	section("Hitec screen: the ID note, STOP, BACK in the entry and the read, a kept edit");
	freshHitec(HITEC_D, false);
	checkTrue("the field list", toList(HITEC_D));
	revealRow(12);
	tapId(UI_ROW(12, Row));
	tapId(UI_FIX(HitecCtl::EditPlus));
	holdUntilGone(UI_FIX(HitecCtl::Write));
	checkTrue("an ID write: its page", uiHitecView() == HitecView::Write);
	checkTrue("names the ID the bench addresses",
	          fakeCountColour(C_AMBER) > 0);
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
	holdUntilGone(UI_FIX(HitecCtl::WriteHold));
	checkTrue("the first write", untilCtl(UI_FIX(HitecCtl::WriteOk), 800) && fakeFatExists("BACKUP/HT001.TXT"));
	tapId(UI_FIX(HitecCtl::WriteOk));

	// Another serial: the card is mounted again and the backup saved to it.
	fakeVolumeSet(2);
	unsigned recovers = fakeCacheCalls().recovers;
	unsigned writes = fakeHitecSim()->writes;
	speedDown();
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	holdUntilGone(UI_FIX(HitecCtl::Write));
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
	testStallAndBack();
	testIdAndBack();
	testCardSwap();
	testStallInWrite();
}
