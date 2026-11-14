/**
 * @file test_kst_ui.cpp
 * @brief KST SERVO through the real UI: MENU, the claim, the search, the
 *        field list, the write, FILES and the leave page.
 *
 * Every check runs the shipped code: ui_kst.cpp, the KST job from the job
 * table, the pin claim and kst_files.cpp. The servo is rcbench's simulator
 * behind the fake line (fake_kst_line.h), the card the in-memory volume of
 * fake_fatfs.h. A frame runs core1 once a millisecond for 25 ms, the fake
 * DShot pump once, then the UI. Controls are pressed by id.
 */

#include "check.h"
#include "fakes.h"
#include "fake_fatfs.h"
#include "fake_kst_line.h"
#include "gfx.h"
#include "core1_job.h"
#include "esc_task.h"
#include "kst_job.h"
#include "kst_line_port.h"
#include "kst_line.h"
#include "card_cache.h"
#include "kst_files.h"
#include "kst_view.h"
#include "pin_claim.h"
#include "sd_log.h"
#include "settings.h"
#include "theme.h"
#include "touch.h"
#include "ui.h"
#include "ui_kst.h"
#include "ui_menu.h"
#include "ui_screen.h"
#include "usb_msc.h"
#include "servo_supply.h"
#include "sense.h"
#include "fake_i2c.h"
#include "sdio/sd_host.h"
#include "kst_supply_sim.h"
#include "hardware/gpio.h"
#include "plat.h"

#include <stdio.h>
#include <string.h>

/** @brief The switched supply's servo, while a switched test runs. */
static SupplySim g_sup;
static bool      g_supOn = false;

/** @brief The level the bench drives on GP28. */
static bool enHigh() {
	FakePin p = fakePin(servoSupplyPinOf(1));
	return p.output && p.high;
}

/** @brief One pass of core1, the switched servo following GP28. */
static void core1Pass() {
	fakeAdvance(1);
	if (g_supOn) supplySimUpdate(&g_sup, enHigh(), time_us_32());
	core1JobPoll();
	if (g_supOn) supplySimUpdate(&g_sup, enHigh(), time_us_32());
}

/** @brief One UI frame: 25 passes of core1, one of the pump, then the UI. */
static void kstFrame() {
	for (int i = 0; i < 25; i++) core1Pass();
	fakePumpPoll();
	uiTick();
}

static void kstFrames(int n) {
	for (int i = 0; i < n; i++) kstFrame();
}

#define UI_IDS_FRAMES(n) kstFrames(n)
#include "ui_ids.h"

static KstStatus job() {
	KstStatus st;
	kstJobStatus(&st);
	return st;
}

/** @brief Frames until the view is @p v, at most @p max. */
static bool untilView(KstView v, int max) {
	for (int i = 0; i < max && uiKstView() != v; i++) kstFrame();
	return uiKstView() == v;
}

/** @brief Frames until @p id is on the screen, at most @p max. */
static bool untilCtl(UiControlId id, int max) {
	for (int i = 0; i < max && !ctlPresent(id); i++) kstFrame();
	return ctlPresent(id);
}

/** @brief Frames until @p id is on the screen and acts, at most @p max. */
static bool untilEnabled(UiControlId id, int max) {
	for (int i = 0; i < max && !ctlEnabled(id); i++) kstFrame();
	return ctlEnabled(id);
}

/** @brief The value the simulated servo holds in @p f. */
static uint16_t simField(kst_field_id_t f) {
	kst_image_t img;
	memcpy(img.r, fakeKstSim()->reg, KST_REG_COUNT);
	return kst_field_get(&img, f);
}

/** @brief Frames the servo had seen at the last card call. */
static unsigned s_framesInCard = 0;

static unsigned n_cardCalls = 0;

/** @brief fakeFatOnCall(): count the card calls. */
static void countCardCall() { n_cardCalls++; }

/** @brief fakeFatOnCall(): core1 for 5 ms, then the servo's write count. */
static void cardCallRunsCore1() {
	for (int i = 0; i < 5; i++) {
		fakeAdvance(1);
		core1JobPoll();
	}
	s_framesInCard = fakeKstSim()->n_frames;
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

/**
 * @brief Amber pixels on the panel outside the main view's headline, y 70 to
 *        y 101, which is amber as the current step: zero means no warning
 *        line, such as an interrupted write's, shows.
 */
static int amberBesideHeadline() {
	const uint16_t *fb = gfxBuffer();
	int n = 0;
	for (int y = 0; y < GFX_H; y++) {
		if (y >= 70 && y <= 101) continue;
		for (int x = 0; x < GFX_W; x++) n += fb[y * GFX_W + x] == C_AMBER;
	}
	return n;
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

/** @brief The board fresh, a card, a powered servo on the line, the tester up. */
static void freshKst(bool powered = true) {
	fakeFlashClear();
	settingsLoad();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	uiInit();
	escTaskConfigure(0, 600, false, 1);
	fakeKstLineReset();
	fakeKstSim()->powered = powered;
	card();
	kstFrames(2);
}

/** @brief The tester to KST SERVO, through MENU. */
static void openKst() {
	tapId(UI_FIX(TesterCtl::Menu));
	kstFrames(1);
	tapId(menuRow(&UI_KST));
	kstFrames(2);
}

/** @brief Open the screen and wait for the field list. */
static bool toList() {
	openKst();
	return untilView(KstView::List, 400);
}

/** @brief The list row of @p f, revealed and tapped. */
static void selectField(kst_field_id_t f) {
	int row = kstRowOf(f);
	revealRow(row);
	tapId(UI_ROW(row, Row));
}

/** @brief Hold @p id until it fires, at most 80 frames; true when it left the screen. */
static bool holdUntilGone(UiControlId id) {
	int x, y;
	ctlCentre(id, &x, &y);
	if (x < 0) return false;
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 80 && ctlPresent(id); i++) { fakeHold(x, y); kstFrame(); }
	bool gone = !ctlPresent(id);
	fakeRelease();
	kstFrame();
	return gone;
}

/** @brief Tap @p id; true when it left the screen. */
static bool tapUntilGone(UiControlId id) {
	tapId(id);
	return !ctlPresent(id);
}

/** @brief From any view: leave, restart DShot, back to the tester. */
static void leaveAll() {
	for (int i = 0; i < 8 && uiCurrentScreen() == UiScreenId::Kst; i++) {
		KstView v = uiKstView();
		if (v == KstView::Leave && ctlPresent(UI_FIX(KstCtl::Restart))) {
			if (untilEnabled(UI_FIX(KstCtl::Restart), 200)) tapId(UI_FIX(KstCtl::Restart));
		} else if (v == KstView::Leave) {
			untilCtl(UI_FIX(KstCtl::Leave), 400);
			if (ctlEnabled(UI_FIX(KstCtl::Leave))) tapId(UI_FIX(KstCtl::Leave));
			else kstFrames(40);
		} else if (v == KstView::Write && !ctlPresent(UI_FIX(KstCtl::WriteHold)) &&
		           !ctlPresent(UI_FIX(KstCtl::WriteOk))) {
			kstFrames(40);
		} else {
			tapId(UI_FIX(UI_BACK));
		}
		kstFrames(2);
	}
	kstFrames(8);
	if (uiCurrentScreen() == UiScreenId::Menu) tapId(UI_FIX(UI_BACK));
	kstFrames(2);
}

static void testOpenAndSearch() {
	section("KST screen: MENU opens it, the claim, the search");
	freshKst(false);
	openKst();
	checkTrue("KST SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Kst);
	checkTrue("it claims SIG", pinClaimHolder() == PinUser::Kst);
	checkTrue("the search starts", untilCtl(UI_FIX(KstCtl::StopSearch), 40));
	checkTrue("the job searches", job().job == KstJobState::Searching);
	checkTrue("on SIG", fakeKstLinePin() == 0);
	checkControlTable("KST search");
	kstFrames(20);
	fakeDumpFrame("shot_kst_search.ppm");

	tapId(UI_FIX(KstCtl::StopSearch));
	checkTrue("STOP SEARCH: RETRY is offered", untilCtl(UI_FIX(KstCtl::Retry), 40));
	checkTrue("the search stopped", job().seek == KstSeek::Stopped);
	checkControlTable("KST stopped");
	tapId(UI_FIX(KstCtl::Retry));
	checkTrue("RETRY: the search runs again", untilCtl(UI_FIX(KstCtl::StopSearch), 20) &&
	                                         job().job == KstJobState::Searching);

	fakeKstSim()->powered = true;
	checkTrue("the servo powered up: found, read, the field list", untilView(KstView::List, 400));
	KstStatus st = job();
	checkTrue("read with a backup", st.hasImage && st.hasBackup && st.inMode);
	checkControlTable("KST list");
	checkTrue("nothing to write yet", !ctlEnabled(UI_FIX(KstCtl::Write)) &&
	                                  !ctlEnabled(UI_FIX(KstCtl::Revert)));
	checkTrue("FILES acts", ctlEnabled(UI_FIX(KstCtl::Files)));
	// A tap on a LOCKED row leaves the selection where it was.
	selectField(KST_F_DEAD_BAND);
	int locked = kstRowOf(KST_F_LOCK);
	revealRow(locked);
	UiRect lr = ctlRect(UI_ROW(locked, Row));
	fakePress(lr.x + lr.w / 2, lr.y + lr.h / 2);
	kstFrame();
	fakeRelease();
	kstFrames(2);
	checkTrue("a LOCKED row takes no tap: DEAD BAND stays selected",
	          ctlEnabled(UI_FIX(KstCtl::EditPlus)));
	kstFrames(2);
	fakeDumpFrame("shot_kst_list.ppm");
	leaveAll();
}

static void testEditAndWrite() {
	section("KST screen: an edit, HOLD TO WRITE, the record and the verify");
	freshKst();
	checkTrue("to the field list", toList());
	sim_t *sim = fakeKstSim();
	uint16_t deadBand = simField(KST_F_DEAD_BAND);

	selectField(KST_F_DEAD_BAND);
	checkTrue("a tap on DEAD BAND: the editor bar", ctlPresent(UI_FIX(KstCtl::EditPlus)));
	checkControlTable("KST editor bar");
	tapId(UI_FIX(KstCtl::EditMinus));
	checkTrue("a step: WRITE and REVERT act", ctlEnabled(UI_FIX(KstCtl::Write)) &&
	                                         ctlEnabled(UI_FIX(KstCtl::Revert)));
	tapId(UI_FIX(KstCtl::Revert));
	checkTrue("REVERT: nothing to write", !ctlEnabled(UI_FIX(KstCtl::Write)));
	tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("+ widens the band: raw down, WRITE acts", ctlEnabled(UI_FIX(KstCtl::Write)));
	kstFrames(1);
	fakeDumpFrame("shot_kst_edit.ppm");

	// WRITE...: a tap on release opens the write page; a press held 1500 ms
	// does nothing until it lifts, and a slide off opens nothing.
	int x, y;
	ctlCentre(UI_FIX(KstCtl::Write), &x, &y);
	checkTrue("the list's button reads WRITE...",
	          fakeTextIs(6 + (110 - gfxTextW("WRITE...", 1)) / 2, 268 + (46 - 7) / 2, "WRITE...", C_TEXT,
	                     C_PANEL, 1));
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 60; i++) { fakeHold(x, y); kstFrame(); }
	checkTrue("1500 ms of press: still the list", uiKstView() == KstView::List);
	for (int i = 0; i < 20; i++) { fakeHold(x, y - 60); kstFrame(); }
	fakeRelease();
	kstFrame();
	checkTrue("a slide off: no write page", uiKstView() == KstView::List);
	checkTrue("a tap: the write page", tapUntilGone(UI_FIX(KstCtl::Write)) &&
	                                    uiKstView() == KstView::Write);
	kstFrames(60);
	checkTrue("the tap's press is not timed: 1500 ms on, HOLD TO WRITE still waits",
	          uiKstView() == KstView::Write && ctlEnabled(UI_FIX(KstCtl::WriteHold)) &&
	          simField(KST_F_DEAD_BAND) == deadBand);
	checkControlTable("KST write page");
	kstFrames(1);
	checkTrue("the strip counts 1 CHANGE, the leave page's word",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "1 CHANGE", C_DIM, C_BG, 1) &&
	          !fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "1 FIELD", C_DIM, C_BG, 1));
	fakeDumpFrame("shot_kst_write.ppm");

	// Core1 runs for 5 ms in every card call, as it does beside core0 on
	// the board: a write command sent before the card calls would send
	// its frames while they run.
	unsigned frames = sim->n_frames;
	s_framesInCard = frames;
	fakeFatOnCall(cardCallRunsCore1);
	unsigned calls = fakeFatCalls(FakeFatCall::Open);
	ctlCentre(UI_FIX(KstCtl::WriteHold), &x, &y);
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 80 && ctlPresent(UI_FIX(KstCtl::WriteHold)); i++) { fakeHold(x, y); kstFrame(); }
	fakeFatOnCall(nullptr);
	fakeRelease();
	checkTrue("the hold starts the write", !ctlPresent(UI_FIX(KstCtl::WriteHold)));
	checkTrue("the backup is on the card first", fakeFatExists("BACKUP/KST001.TXT"));
	checkTrue("and the record", fakeFatExists("KST/KST.TXT"));
	checkTrue("the card calls ran", fakeFatCalls(FakeFatCall::Open) > calls);
	checkInt("no frame at all while they ran", (long)s_framesInCard, (long)frames);
	checkTrue("the result comes", untilCtl(UI_FIX(KstCtl::WriteOk), 400));
	checkTrue("verified", job().result == KST_SES_OK);
	checkTrue("the servo holds the new dead band", simField(KST_F_DEAD_BAND) != deadBand);
	checkTrue("the record is removed after the verify", !fakeFatExists("KST/KST.TXT"));
	checkTrue("the backup stays", fakeFatExists("BACKUP/KST001.TXT"));
	checkTrue("the result page still lists the change, its new value cyan", fakeCountColour(C_CYAN) > 0);
	kstFrames(1);
	fakeDumpFrame("shot_kst_result.ppm");
	tapId(UI_FIX(KstCtl::WriteOk));
	checkTrue("OK: the list, nothing to write", uiKstView() == KstView::List &&
	                                           !ctlEnabled(UI_FIX(KstCtl::Write)));

	// The hard limit: + stops at 98 %, and a write of it holds raw 250.
	selectField(KST_F_DUTY);
	for (int i = 0; i < 12 && ctlEnabled(UI_FIX(KstCtl::EditPlus)); i++) tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("MAX. DUTY's + stops", !ctlEnabled(UI_FIX(KstCtl::EditPlus)));
	selectField(KST_F_DEAD_BAND);
	checkTrue("another field, same changes: its + acts", ctlEnabled(UI_FIX(KstCtl::EditPlus)));
	selectField(KST_F_DUTY);
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
	checkInt("the servo holds 98 %, raw 250, and no more", simField(KST_F_DUTY), KST_DUTY_RAW_MAX);
	checkTrue("a second write writes no new record", !fakeFatExists("KST/KST.TXT") &&
	                                                !fakeFatExists("BACKUP/KST002.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

static void testFiles() {
	section("KST screen: FILES, SAVE AS, LOAD and a refused file");
	freshKst();
	checkTrue("to the field list", toList());
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("FILES opens", uiKstView() == KstView::Files);
	checkTrue("an empty card: SAVE AS acts, LOAD does not",
	          ctlEnabled(UI_FIX(KstCtl::SaveAs)) && !ctlEnabled(UI_FIX(KstCtl::Load)));
	checkTrue("nothing to restore", !ctlEnabled(UI_FIX(KstCtl::Restore)));
	tapId(UI_FIX(KstCtl::SaveAs));
	checkTrue("SAVE AS writes KST001", fakeFatExists("KST/KST001.TXT"));
	tapId(UI_FIX(KstCtl::SaveAs));
	checkTrue("and again KST002", fakeFatExists("KST/KST002.TXT"));
	checkTrue("both listed", ctlPresent(UI_ROW(0, Row)) && ctlPresent(UI_ROW(1, Row)));
	checkControlTable("KST files");
	kstFrames(1);
	fakeDumpFrame("shot_kst_files.ppm");

	// A file that differs from the servo: LOAD makes its values the pending changes.
	kst_image_t other = sim_bench_image();
	kst_field_edit(&other, KST_F_DEAD_BAND, (uint16_t)(kst_field_get(&other, KST_F_DEAD_BAND) + 1));
	static char text[KST_FILE_MAX];
	size_t n = kst_file_format(&other, text, sizeof(text));
	fakeFatPut("KST/WING.TXT", text, n);
	fakeFatPut("KST/BAD.TXT", "KST servo configuration, format 1\nType: V6\nDead Band = wide\n", 59);
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Files));
	// By name, the numbered run by falling number: BAD, KST002, KST001, WING.
	const int bad = 0, wing = 3;
	checkTrue("four files listed", ctlPresent(UI_ROW(wing, Row)) && !ctlPresent(UI_ROW(4, Row)));
	tapId(UI_ROW(bad, Row));
	tapId(UI_FIX(KstCtl::Load));
	checkTrue("a refused file: FILES stays", uiKstView() == KstView::Files);
	tapId(UI_ROW(wing, Row));
	tapId(UI_FIX(KstCtl::Load));
	checkTrue("LOAD: the list, with the file's values pending",
	          uiKstView() == KstView::List && ctlEnabled(UI_FIX(KstCtl::Write)));

	// The KST backups of BACKUP/ stand first, an ESC's backup not at all.
	tapId(UI_FIX(KstCtl::Revert));
	fakeFatMkdir("BACKUP");
	fakeFatPut("BACKUP/KST005.TXT", text, n);
	fakeFatPut("BACKUP/AM006.TXT", "AM32 backup, format 1\n", 22);
	tapId(UI_FIX(KstCtl::Files));
	// Row 0 is the backup: BAD.TXT, first of KST/, would refuse the LOAD.
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(KstCtl::Load));
	checkTrue("LOAD of the backup row: its values pending",
	          uiKstView() == KstView::List && ctlEnabled(UI_FIX(KstCtl::Write)));
	leaveAll();

	// KST/ with 50 files: SAVE AS refuses.
	freshKst();
	fakeFatMkdir("KST");
	for (int i = 1; i <= KST_FILES_SAVE_MAX; i++) {
		char name[24];
		snprintf(name, sizeof(name), "KST/KST%03d.TXT", i);
		fakeFatPut(name, text, n);
	}
	checkTrue("50 files: to the field list", toList());
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("50 files in KST/: SAVE AS does not act", !ctlEnabled(UI_FIX(KstCtl::SaveAs)));
	checkTrue("and the strip says why", fakeCountColour(C_AMBER) > 0);
	f_unlink("KST/KST001.TXT");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("49 files: SAVE AS acts again", ctlEnabled(UI_FIX(KstCtl::SaveAs)));
	tapId(UI_FIX(KstCtl::SaveAs));
	checkTrue("SAVE AS takes KST051, one above the highest", fakeFatExists("KST/KST051.TXT"));
	checkTrue("and then refuses", !ctlEnabled(UI_FIX(KstCtl::SaveAs)));
	leaveAll();
}

static void testLockedAndRestore() {
	section("KST screen: a write left in doubt, and HOLD TO RESTORE");
	freshKst();
	checkTrue("to the field list", toList());
	sim_t *sim = fakeKstSim();
	kst_image_t before;
	memcpy(before.r, sim->reg, KST_REG_COUNT);
	// The first write to BOOST, register 03, lands in 13 as well.
	sim_add_fault(sim, SIM_WRITE, 0x03, 0, 1, SIM_F_WRITE_ALSO, 0x13);
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the result comes", untilCtl(UI_FIX(KstCtl::WriteOk), 400));
	checkTrue("in doubt: locked", job().locked && job().result == KST_SES_ERR_VERIFY);
	checkTrue("the page says so in red", fakeCountColour(C_RED) > 0);
	kstFrames(1);
	fakeDumpFrame("shot_kst_doubt.ppm");
	tapId(UI_FIX(KstCtl::WriteOk));
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("a locked session takes an edit and no write", ctlEnabled(UI_FIX(KstCtl::Revert)) &&
	                                                         !ctlEnabled(UI_FIX(KstCtl::Write)));
	tapId(UI_FIX(KstCtl::Revert));
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("RESTORE... acts", ctlEnabled(UI_FIX(KstCtl::Restore)));
	checkTrue("the restore button reads RESTORE...",
	          fakeTextIs(122 + (112 - gfxTextW("RESTORE...", 1)) / 2, ctlRect(UI_FIX(KstCtl::Restore)).y +
	                     (ctlRect(UI_FIX(KstCtl::Restore)).h - 7) / 2, "RESTORE...", C_TEXT, C_PANEL, 1));
	checkTrue("a tap opens the restore page", tapUntilGone(UI_FIX(KstCtl::Restore)) &&
	                                         uiKstView() == KstView::Write);
	kstFrames(60);
	checkTrue("it restores nothing until its hold: HOLD TO RESTORE waits",
	          ctlEnabled(UI_FIX(KstCtl::WriteHold)) && job().locked &&
	          lineIs(UI_STRIP_Y + 4, "BACK TO THE SESSION'S BACKUP", C_DIM));
	kstFrames(1);
	fakeDumpFrame("shot_kst_restore.ppm");
	checkTrue("the hold runs the restore", holdUntilGone(UI_FIX(KstCtl::WriteHold)));
	for (int i = 0; i < 400 && !ctlPresent(UI_FIX(KstCtl::WriteOk)) &&
	                !ctlPresent(UI_FIX(KstCtl::WriteHold)); i++)
		kstFrame();
	checkTrue("a restore the planner checked: one hold", !ctlPresent(UI_FIX(KstCtl::WriteHold)));
	checkTrue("the result comes", untilCtl(UI_FIX(KstCtl::WriteOk), 600));
	checkTrue("restored and unlocked", job().result == KST_SES_OK && !job().locked);
	checkTrue("the servo holds the backup", memcmp(sim->reg, before.r, KST_REG_COUNT) == 0);
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

/**
 * @brief The leave page after LEAVE, or opened releasing: NEXT SERVO and
 *        RESTART DSHOT drawn disabled until the pin is let go, then NEXT
 *        SERVO live, and RESTART DSHOT live 1000 ms after that frame.
 * @return The ms from the released phase to RESTART DSHOT live; 0 when the
 *         page did not get there.
 */
static uint32_t leaveReleases(const char *where) {
	char what[96];
	snprintf(what, sizeof(what), "%s: NEXT SERVO and RESTART DSHOT on the page", where);
	checkTrue(what, uiKstView() == KstView::Leave && ctlPresent(UI_FIX(KstCtl::NextServo)) &&
	                ctlPresent(UI_FIX(KstCtl::Restart)) && !ctlPresent(UI_FIX(KstCtl::Leave)) &&
	                !ctlPresent(UI_FIX(KstCtl::Stay)));
	if (!untilEnabled(UI_FIX(KstCtl::NextServo), 200)) return 0;
	uint32_t t0 = millis();
	snprintf(what, sizeof(what), "%s: released, RESTART DSHOT waits, drawn disabled", where);
	checkTrue(what, !ctlEnabled(UI_FIX(KstCtl::Restart)) && leaveWideIs("RESTART DSHOT", false) &&
	                leaveLeftIs("NEXT SERVO", true));
	if (!untilEnabled(UI_FIX(KstCtl::Restart), 60)) return 0;
	kstFrames(1);
	snprintf(what, sizeof(what), "%s: RESTART DSHOT drawn live", where);
	checkTrue(what, leaveWideIs("RESTART DSHOT", true));
	return millis() - 25 - t0;
}

static void testLeaveAndBack() {
	section("KST screen: the leave page, its phases and buttons, BACK in every view");
	freshKst(false);
	openKst();
	untilCtl(UI_FIX(KstCtl::StopSearch), 40);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK in a search that found nothing: the leave page, releasing",
	          uiKstView() == KstView::Leave && pinClaimState() == ClaimState::Leaving);
	checkTrue("no servo in the mode: no step before LEAVE, UNPLUG THE SERVO. is step 1",
	          leaveStepIs(UI_LEAVE_STEP_Y, '1', "UNPLUG THE SERVO.", true) &&
	          leaveStepIs(UI_LEAVE_STEP_Y + 12, 0, "DSHOT RESTARTS ON SIG GP0 AND READS", true) &&
	          leaveStepIs(UI_LEAVE_STEP_Y + 24, 0, "AS A 970 US PULSE AT 1000 HZ.", true) &&
	          leaveStepIs(UI_LEAVE_STEP_Y + 38, '2', "RESTART DSHOT.", false));
	checkTrue("BACK disabled on the page, and drawn so",
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	uint32_t waited = leaveReleases("KST, no servo");
	checkTrue("RESTART DSHOT live 1000 ms after the pin is let go", waited >= 1000 && waited < 1025);
	checkControlTable("KST leave, released");
	checkTrue("BACK still disabled once released", !ctlEnabled(UI_FIX(UI_BACK)));
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("BACK there does nothing", uiKstView() == KstView::Leave &&
	                                    pinClaimState() == ClaimState::Leaving);
	tapId(UI_FIX(KstCtl::NextServo));
	checkTrue("NEXT SERVO: the claim kept, the search again", uiKstView() == KstView::Main &&
	          pinClaimHolder() == PinUser::Kst && untilCtl(UI_FIX(KstCtl::StopSearch), 200));
	fakeKstSim()->powered = true;
	checkTrue("found and read", untilView(KstView::List, 400));
	tapId(UI_FIX(KstCtl::Files));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK in FILES: the list", uiKstView() == KstView::List);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK in the list: the leave page, before LEAVE", uiKstView() == KstView::Leave &&
	          ctlEnabled(UI_FIX(KstCtl::Stay)) && ctlEnabled(UI_FIX(KstCtl::Leave)) &&
	          !ctlPresent(UI_FIX(KstCtl::Reboot)) && !ctlPresent(UI_FIX(KstCtl::Restart)));
	checkControlTable("KST leave");
	kstFrames(1);
	checkTrue("BACK drawn disabled before LEAVE", backLook() == BackLook::Disabled);
	checkTrue("the strip: the type and SIG", lineIs(UI_STRIP_Y + 4, "V6  SIG GP0", C_DIM));
	checkTrue("step 1, current: POWER THE SERVO OFF, THEN LEAVE. and why",
	          leaveStepIs(66, '1', "POWER THE SERVO OFF, THEN LEAVE.", true) &&
	          leaveStepIs(78, 0, "IT STAYS IN PROGRAMMING MODE UNTIL", true) &&
	          leaveStepIs(90, 0, "ITS SUPPLY IS OFF.", true));
	checkTrue("steps 2 and 3 dim: UNPLUG THE SERVO., RESTART DSHOT.",
	          leaveStepIs(104, '2', "UNPLUG THE SERVO.", false) &&
	          leaveStepIs(116, 0, "DSHOT RESTARTS ON SIG GP0 AND READS", false) &&
	          leaveStepIs(128, 0, "AS A 970 US PULSE AT 1000 HZ.", false) &&
	          leaveStepIs(142, '3', "RESTART DSHOT.", false));
	checkTrue("STAY and LEAVE drawn live; no note", leaveLeftIs("STAY", true) &&
	          leaveWideIs("LEAVE", true) && leaveNoteBlank(0));
	fakeDumpFrame("shot_leave_kst.ppm");
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("BACK on the leave page does nothing", uiKstView() == KstView::Leave);
	tapId(UI_FIX(KstCtl::Stay));
	checkTrue("STAY: the list, the claim held", uiKstView() == KstView::List &&
	          pinClaimState() == ClaimState::Running);

	// BACK during a write is drawn disabled and does nothing: no leave is
	// queued, and the result follows on the page.
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	checkTrue("BACK live on the write page before the hold",
	          ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Live);
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the write runs: BACK disabled, and drawn so", uiKstView() == KstView::Write &&
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	checkControlTable("KST write page, writing");
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("BACK during the write: the page stays", uiKstView() == KstView::Write);
	checkTrue("the result follows on the page, no leave page",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && uiKstView() == KstView::Write);
	checkTrue("the write ended verified", job().result == KST_SES_OK);
	checkTrue("BACK live again under the result", ctlEnabled(UI_FIX(UI_BACK)));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK with the result: the list", uiKstView() == KstView::List);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK there: the leave page", uiKstView() == KstView::Leave);

	// LEAVE, then a second tap in the same rectangle: RESTART DSHOT is
	// drawn disabled while the pin is let go and takes no press.
	tapId(UI_FIX(KstCtl::Leave));
	checkTrue("LEAVE: the page releasing, RESTART DSHOT in LEAVE's place",
	          uiKstView() == KstView::Leave && ctlPresent(UI_FIX(KstCtl::Restart)) &&
	          ctlRect(UI_FIX(KstCtl::Restart)).y == UI_LEAVE_WIDE.y &&
	          ctlRect(UI_FIX(KstCtl::Restart)).w == UI_LEAVE_WIDE.w &&
	          !ctlEnabled(UI_FIX(KstCtl::Restart)) && !ctlEnabled(UI_FIX(KstCtl::NextServo)));
	checkTrue("step 2 is current from LEAVE on, step 1 dim",
	          leaveStepIs(66, '1', "POWER THE SERVO OFF, THEN LEAVE.", false) &&
	          leaveStepIs(104, '2', "UNPLUG THE SERVO.", true));
	checkTrue("the note while the pin goes: RELEASING THE PIN, dim",
	          job().supplyUsed || leaveNoteIs(0, "RELEASING THE PIN", C_DIM));
	checkControlTable("KST leave, releasing");
	tapId(UI_FIX(KstCtl::Restart), IdExpect::Disabled);
	checkTrue("a double tap on LEAVE restarts nothing", uiCurrentScreen() == UiScreenId::Kst &&
	          pinClaimState() == ClaimState::Leaving);
	checkTrue("after 100 ms of low line the pin is let go", untilEnabled(UI_FIX(KstCtl::NextServo), 200) &&
	                                                        !kstLineIsOpen());
	checkTrue("released: NEXT SERVO: POWER IT AFTER THE TAP., dim",
	          leaveNoteIs(0, KST_UI_NEXT_POWER, C_DIM));
	fakeDumpFrame("shot_leave_kst_released.ppm");
	// A tap in the wait does nothing, and a press that landed in the wait
	// does nothing when it lifts after the wait.
	tapId(UI_FIX(KstCtl::Restart), IdExpect::Disabled);
	checkTrue("a tap in the 1000 ms wait: nothing", uiCurrentScreen() == UiScreenId::Kst &&
	          pinClaimState() == ClaimState::Leaving);
	pressId(UI_FIX(KstCtl::Restart), IdExpect::Disabled);
	holdId(UI_FIX(KstCtl::Restart), 44);
	checkTrue("held past the wait: RESTART DSHOT live, not drawn pressed",
	          ctlEnabled(UI_FIX(KstCtl::Restart)) && leaveWideIs("RESTART DSHOT", true));
	fakeRelease();
	kstFrames(2);
	checkTrue("its lift restarts nothing", uiCurrentScreen() == UiScreenId::Kst &&
	          pinClaimState() == ClaimState::Leaving);
	tapId(UI_FIX(KstCtl::Restart));
	kstFrames(8);
	checkTrue("RESTART DSHOT: MENU, the pin back to DShot",
	          uiCurrentScreen() == UiScreenId::Menu && pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	kstFrames(2);
}

static void testLossAndCard() {
	section("KST screen: a lost servo, no card, a card that takes no file");
	freshKst();
	checkTrue("to the field list", toList());
	sim_t *sim = fakeKstSim();
	sim->powered = false;
	sim->in_mode = false;
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the servo gone: the search again", untilView(KstView::Main, 400) &&
	                                              untilCtl(UI_FIX(KstCtl::StopSearch), 200));
	sim->powered = true;
	checkTrue("powered again: the list", untilView(KstView::List, 400));
	leaveAll();

	freshKst();
	noCard();
	checkTrue("no card: to the field list", toList());
	unsigned writes = fakeKstSim()->n_writes;
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("no card: the write runs", untilCtl(UI_FIX(KstCtl::WriteOk), 400) &&
	                                     job().result == KST_SES_OK &&
	                                     fakeKstSim()->n_writes > writes);
	checkInt("and nothing reaches FatFs", (long)fakeFatCalls(FakeFatCall::Open), 0);
	tapId(UI_FIX(KstCtl::WriteOk));
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("FILES without a card: RETRY MOUNT", ctlEnabled(UI_FIX(KstCtl::RetryMount)));
	tapId(UI_FIX(KstCtl::RetryMount));
	checkTrue("a card appears: SAVE AS", ctlPresent(UI_FIX(KstCtl::SaveAs)));
	fakeMscSet(MscState::Serving, 0);
	kstFrames(1);
	checkTrue("held by USB: no RETRY MOUNT that acts", !ctlEnabled(UI_FIX(KstCtl::RetryMount)));
	fakeMscSet(MscState::Idle, 0);
	leaveAll();

	freshKst();
	fakeFatWriteProtect(true);
	fakeFatFail(FakeFatCall::MkDir, FR_DISK_ERR);
	checkTrue("a card that takes no file: to the field list", toList());
	writes = fakeKstSim()->n_writes;
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the card took no file: OK, no write", ctlPresent(UI_FIX(KstCtl::WriteOk)) &&
	                                                 fakeKstSim()->n_writes == writes);
	kstFrames(40);
	checkInt("nothing reaches the servo", (long)fakeKstSim()->n_writes, (long)writes);
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

static void writeDeadBand();

static void testReadOnlyAndRecord() {
	section("KST screen: an unknown layout, and a record found at entry");
	freshKst();
	fakeKstSim()->reg[0x06] = 0x05;  // the fingerprint wants 04
	checkTrue("to the field list", toList());
	checkTrue("read, the fingerprint fails", !job().fingerprintOk);
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("READ ONLY: no write", !ctlEnabled(UI_FIX(KstCtl::Write)));
	leaveAll();

	freshKst();
	kst_image_t rec = sim_bench_image();
	static char text[KST_FILE_MAX];
	size_t n = kst_file_format(&rec, text, sizeof(text));
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	checkTrue("a record on the card: to the field list", toList());
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("FILES lists the record first", ctlPresent(UI_ROW(0, Row)));
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(KstCtl::Load));
	checkTrue("it loads like a file", uiKstView() == KstView::List);

	// A verified write keeps the old record under a number and removes its
	// own: after a loss, Main names no interrupted write.
	writeDeadBand();
	checkTrue("a verified write, the record gone", job().result == KST_SES_OK &&
	                                              !fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	fakeKstSim()->powered = false;
	fakeKstSim()->in_mode = false;
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the servo lost: the search", untilView(KstView::Main, 400) &&
	                                       untilCtl(UI_FIX(KstCtl::StopSearch), 200));
	kstFrames(2);
	checkTrue("no interrupted write named: nothing amber on the search but the headline",
	          amberBesideHeadline() == 0);
	tapId(UI_FIX(UI_BACK));
	kstFrames(4);
	checkTrue("BACK on the search after a loss: the leave page stays",
	          uiKstView() == KstView::Leave);
	kstFrames(4);
	checkTrue("and the search stopped", job().job == KstJobState::SearchEnded);
	unsigned frames = fakeKstSim()->n_frames;
	fakeKstSim()->powered = true;
	kstFrames(40);
	checkTrue("a servo powered up behind the leave page gets no frame",
	          fakeKstSim()->n_frames == frames && uiKstView() == KstView::Leave);
	leaveAll();
}

static void testRefusalsAndFailures() {
	section("KST screen: an ESC on SIG, a bind that fails, a timeout, a failed read");
	freshKst(false);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	openKst();
	checkTrue("an ESC answers: no claim", pinClaimHolder() == PinUser::None &&
	                                      !ctlPresent(UI_FIX(KstCtl::StopSearch)));
	checkTrue("its warning in amber", fakeCountColour(C_AMBER) > 0);
	checkTrue("AN ESC ANSWERS ON SIG GP0., amber", lineIs(80, "AN ESC ANSWERS ON SIG GP0.", C_AMBER));
	checkTrue("the strip: CHECKING SIG GP0 FOR AN ESC",
	          lineIs(UI_STRIP_Y + 4, "CHECKING SIG GP0 FOR AN ESC", C_DIM));
	checkControlTable("KST, an ESC answers");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK before a claim: MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	kstFrames(2);

	// Another claim holds the pins: MENU refuses the row, so KST SERVO is
	// opened by hand here.
	freshKst(false);
	checkTrue("another claim holds the pins", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	{
		TouchState none;
		memset(&none, 0, sizeof(none));
		UI_KST.enter();
		UI_KST.tick(&none);
		checkTrue("KST refused: WAIT: DSHOT RESTARTING, amber",
		          pinClaimHolder() == PinUser::Am32 && lineIs(80, UI_WAIT_RESTART_TEXT, C_AMBER));
	}
	pinClaimEnd(PinUser::Am32);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) kstFrames(1);

	freshKst(false);
	fakeKstLineRefuse(KST_LINE_ERR_DMA);
	openKst();
	checkTrue("a bind that fails: RETRY", untilCtl(UI_FIX(KstCtl::Retry), 80) &&
	                                      pinClaimJobError() == CORE1_JOB_DMA);
	checkTrue("SIG GP0 COULD NOT BE TAKEN (DMA), amber",
	          lineIs(80, "SIG GP0 COULD NOT BE TAKEN (DMA)", C_AMBER));
	checkControlTable("KST, a bind error");
	fakeKstLineRefuse(0);
	tapId(UI_FIX(KstCtl::Retry));
	checkTrue("RETRY binds and searches", untilCtl(UI_FIX(KstCtl::StopSearch), 80));

	// 30 s of search, 1200 frames of 25 ms.
	checkTrue("no servo in 30 s: RETRY", untilCtl(UI_FIX(KstCtl::Retry), 1300) &&
	                                     job().seek == KstSeek::TimedOut);
	checkControlTable("KST, timed out");
	leaveAll();

	freshKst();
	// Register 05 answers nothing on any of its reads.
	sim_add_fault(fakeKstSim(), SIM_READ, 0x05, 0, 1000, SIM_F_NO_REPLY, 0);
	openKst();
	checkTrue("a register without 3 equal reads: READ AGAIN",
	          untilCtl(UI_FIX(KstCtl::ReadAgain), 400) && uiKstView() == KstView::Main);
	checkTrue("the job names the register", (job().badRegs & (1u << 5)) != 0);
	checkControlTable("KST, a failed read");
	kstFrames(1);
	fakeDumpFrame("shot_kst_readfail.ppm");
	// Registers 00 to 0B silent in passes 1 to 3 of 5: 12 registers have 2
	// equal reads, more than one line of Main names. A fault counts its skip
	// only on the reads the faults before it let pass, 20 of each 32.
	fakeKstSim()->n_faults = 0;
	for (unsigned pass = 0; pass < 3; pass++)
		sim_add_fault(fakeKstSim(), SIM_READ, -1, pass * 20u, 12, SIM_F_NO_REPLY, 0);
	tapId(UI_FIX(KstCtl::ReadAgain));
	checkTrue("12 registers without 3 equal reads: READ AGAIN",
	          untilCtl(UI_FIX(KstCtl::ReadAgain), 400) && job().badRegs == 0xFFFu);
	kstFrames(1);
	checkTrue("Main names the first 9",
	          lineIs(94, "REGISTERS 00 01 02 03 04 05 06 07 08", C_AMBER));
	checkTrue("and counts the rest", lineIs(112, "AND 3 MORE", C_AMBER));
	fakeKstSim()->n_faults = 0;
	tapId(UI_FIX(KstCtl::ReadAgain));
	checkTrue("READ AGAIN reads it", untilView(KstView::List, 400));
	leaveAll();
}

/** @brief @p img as a file on the card under @p name. */
static void putImage(const char *name, const kst_image_t *img) {
	static char text[KST_FILE_MAX];
	size_t n = kst_file_format(img, text, sizeof(text));
	fakeFatPut(name, text, n);
}

static void testLoadRefusals() {
	section("KST screen: LOAD names why a file does not load");
	freshKst();
	fakeFatMkdir("KST");
	kst_image_t v8 = sim_bench_image();
	v8.r[0x04] &= (uint8_t)~0x40;  // 20k_sel clear: a V8 servo's file
	putImage("KST/A_V8.TXT", &v8);
	kst_image_t duty = sim_bench_image();
	duty.r[0x01] = duty.r[0x02] = 255;  // MAX. DUTY above 98 %
	putImage("KST/B_DUTY.TXT", &duty);
	fakeFatPut("KST/C_HEAD.TXT", "KST servo\n", 10);
	checkTrue("to the field list", toList());
	tapId(UI_FIX(KstCtl::Files));
	const char *what[] = { "a V8 file on a V6 servo: refused",
	                       "a file above a hard limit: refused",
	                       "a file without the header: refused" };
	for (int i = 0; i < 3; i++) {
		tapId(UI_ROW(i, Row));
		tapId(UI_FIX(KstCtl::Load));
		checkTrue(what[i], uiKstView() == KstView::Files && fakeCountColour(C_AMBER) > 0 &&
		                   !ctlEnabled(UI_FIX(KstCtl::Restore)));
	}
	tapId(UI_FIX(UI_BACK));
	checkTrue("nothing pending after the refusals", !ctlEnabled(UI_FIX(KstCtl::Write)));
	leaveAll();
}

static void testManyChangesAndLeave() {
	section("KST screen: five changes on the write page, the leave page's warnings");
	freshKst();
	checkTrue("to the field list", toList());
	const kst_field_id_t f[] = { KST_F_DUTY, KST_F_BOOST, KST_F_SPD, KST_F_DEAD_BAND, KST_F_NEUTRAL };
	for (kst_field_id_t x : f) {
		selectField(x);
		tapId(UI_FIX(ctlEnabled(UI_FIX(KstCtl::EditMinus)) ? KstCtl::EditMinus : KstCtl::EditPlus));
	}
	checkTrue("a hold opens the write page", tapUntilGone(UI_FIX(KstCtl::Write)) &&
	                                        uiKstView() == KstView::Write);
	kstFrames(1);
	checkTrue("the strip counts 5 CHANGES", fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "5 CHANGES", C_DIM, C_BG, 1));
	checkTrue("4 rows, then AND 1 MORE at y 142", lineIs(UI_BODY_Y + 4 * 20, "AND 1 MORE", C_DIM));
	fakeDumpFrame("shot_kst_write_many.ppm");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	checkTrue("the leave page with five changes pending", uiKstView() == KstView::Leave &&
	                                                     ctlEnabled(UI_FIX(KstCtl::Leave)));
	kstFrames(1);
	checkTrue("the note names them in amber", leaveNoteIs(0, "5 CHANGES NOT WRITTEN.", C_AMBER));
	fakeDumpFrame("shot_leave_kst_pending.ppm");
	tapId(UI_FIX(KstCtl::Stay));
	tapId(UI_FIX(KstCtl::Revert));

	// A write in doubt: the leave page says so, and LEAVE still leaves.
	sim_add_fault(fakeKstSim(), SIM_WRITE, 0x03, 0, 1, SIM_F_WRITE_ALSO, 0x13);
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
	tapId(UI_FIX(KstCtl::WriteOk));
	tapId(UI_FIX(UI_BACK));
	checkTrue("locked: the leave page", uiKstView() == KstView::Leave && job().locked);
	checkTrue("the note: THE SERVO IS IN DOUBT: RESTORE FIRST., amber",
	          leaveNoteIs(0, "THE SERVO IS IN DOUBT: RESTORE FIRST.", C_AMBER));
	tapId(UI_FIX(KstCtl::Leave));
	checkTrue("LEAVE leaves a locked session", uiKstView() == KstView::Leave &&
	          ctlPresent(UI_FIX(KstCtl::Restart)) && pinClaimState() == ClaimState::Leaving);
	checkTrue("the record stays on the card", fakeFatExists("KST/KST.TXT"));
	leaveAll();
}

/** @brief Edit DEAD BAND once and run the write to its OK. */
static void writeDeadBand() {
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
}

static void testSecondServoAndLateCard() {
	section("KST screen: a second servo gets its own backup; a card mounted after a write");
	freshKst();
	checkTrue("to the field list", toList());
	sim_t *sim = fakeKstSim();
	// The first servo's write is left in doubt: its backup and record stay.
	sim_add_fault(sim, SIM_WRITE, 0x03, 0, 1, SIM_F_WRITE_ALSO, 0x13);
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
	checkTrue("the first servo in doubt", job().locked && fakeFatExists("BACKUP/KST001.TXT") &&
	                                     fakeFatExists("KST/KST.TXT"));
	static char rec1[KST_FILE_MAX];
	int32_t rec1Len = fakeFatGet("KST/KST.TXT", rec1, sizeof(rec1));
	tapId(UI_FIX(KstCtl::WriteOk));
	// It goes; another servo comes, with another register 0B.
	sim->powered = false;
	sim->in_mode = false;
	sim->n_faults = 0;
	tapId(UI_FIX(KstCtl::Files));
	tapUntilGone(UI_FIX(KstCtl::Restore));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the first servo lost during the restore: the search", untilView(KstView::Main, 400));
	checkTrue("the interrupted write is named", fakeCountColour(C_AMBER) > 0);
	kst_image_t second = sim_bench_image();
	second.r[0x0B] ^= 0x01;
	memcpy(sim->reg, second.r, KST_REG_COUNT);
	sim->powered = true;
	checkTrue("the second servo read", untilView(KstView::List, 400) && !job().locked);
	writeDeadBand();
	checkTrue("its write verified", job().result == KST_SES_OK);
	// The old record is renamed to the next number first, then the backup
	// takes the one after: KST002 and KST003, in either role.
	static char want[KST_FILE_MAX], k[KST_FILE_MAX];
	size_t wn = kst_file_format(&second, want, sizeof(want));
	bool own = false, kept = false;
	for (int i = 2; i <= 4; i++) {
		char name[24];
		snprintf(name, sizeof(name), "BACKUP/KST%03d.TXT", i);
		int32_t n = fakeFatGet(name, k, sizeof(k));
		if (n == (int32_t)wn && memcmp(k, want, wn) == 0) own = true;
		if (n == rec1Len && memcmp(k, rec1, (size_t)n) == 0) kept = true;
	}
	checkTrue("its own backup, the second servo as read", own);
	checkTrue("the first servo's record kept as a KSTnnn", kept);
	checkTrue("and no record left", !fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();

	freshKst();
	noCard();
	checkTrue("no card: to the field list", toList());
	writeDeadBand();
	checkTrue("no card: the write verified", job().result == KST_SES_OK);
	tapId(UI_FIX(KstCtl::WriteOk));
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	kstFrames(1);
	checkTrue("still no card: the old values lost at LEAVE, amber",
	          lineIs(236, "NO CARD: OLD VALUES LOST AT LEAVE", C_AMBER));
	checkTrue("it fits the line", UI_MARGIN + gfxTextW("CARD ON USB: OLD VALUES LOST AT LEAVE", 1) <=
	                              GFX_W - UI_MARGIN);
	fakeDumpFrame("shot_kst_write_nocard.ppm");
	fakeMscSet(MscState::Serving, 0);
	kstFrames(1);
	checkTrue("a card held by USB: the same cost, amber",
	          lineIs(236, "CARD ON USB: OLD VALUES LOST AT LEAVE", C_AMBER));
	fakeMscSet(MscState::Idle, 0);
	card();
	kstFrames(1);
	checkTrue("a card mounted on the page: it names the backup it saves",
	          lineIs(236, "BACKUP FIRST: BACKUP/KST001.TXT", C_DIM));
	n_cardCalls = 0;
	fakeFatOnCall(countCardCall);
	kstFrames(10);
	fakeFatOnCall(nullptr);
	checkTrue("and looks the name up once, not each frame", n_cardCalls == 0);
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
	static char b1[KST_FILE_MAX];
	int32_t n1 = fakeFatGet("BACKUP/KST001.TXT", b1, sizeof(b1));
	kst_image_t bench = sim_bench_image();
	wn = kst_file_format(&bench, want, sizeof(want));
	checkTrue("the second write saves the session's backup, the servo as first read",
	          n1 == (int32_t)wn && memcmp(b1, want, wn) == 0);
	checkTrue("verified, the record removed", job().result == KST_SES_OK &&
	                                         !fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

static void testAbortQueueAndLapse() {
	section("KST screen: BACK during the read, BACK during a write, a loss with the heartbeat lapsed");
	freshKst(false);
	openKst();
	untilCtl(UI_FIX(KstCtl::StopSearch), 40);
	fakeKstSim()->powered = true;
	for (int i = 0; i < 400 && job().job != KstJobState::Busy; i++) kstFrame();
	checkTrue("found: the read runs", job().job == KstJobState::Busy);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK during the read: the leave page", uiKstView() == KstView::Leave);
	kstFrames(20);
	tapId(UI_FIX(KstCtl::Stay));
	checkTrue("STAY there: READ AGAIN", untilCtl(UI_FIX(KstCtl::ReadAgain), 100) &&
	                                    ctlEnabled(UI_FIX(KstCtl::ReadAgain)));
	tapId(UI_FIX(KstCtl::ReadAgain));
	checkTrue("it reads", untilView(KstView::List, 400));

	// BACK during a write queues nothing: the result stays up, and the
	// next write shows its own.
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("BACK during the write: the result on the page, no leave page",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && uiKstView() == KstView::Write);
	tapId(UI_FIX(KstCtl::WriteOk));
	writeDeadBand();
	checkTrue("the next write shows its result", uiKstView() == KstView::Write &&
	                                            ctlPresent(UI_FIX(KstCtl::WriteOk)));
	tapId(UI_FIX(KstCtl::WriteOk));

	// The servo goes during a write and the screen stalls: core1 alone.
	sim_t *sim = fakeKstSim();
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	sim->powered = false;
	sim->in_mode = false;
	for (int i = 0; i < 3000 && job().job == KstJobState::Busy; i++) {
		fakeAdvance(1);
		core1JobPoll();
	}
	for (int i = 0; i < 400; i++) { fakeAdvance(1); core1JobPoll(); }
	checkTrue("the job ended the search: no screen", job().job == KstJobState::SearchEnded);
	kstFrames(2);
	checkTrue("the screen back: Main with RETRY", uiKstView() == KstView::Main &&
	                                             ctlPresent(UI_FIX(KstCtl::Retry)));
	sim->powered = true;
	tapId(UI_FIX(KstCtl::Retry));
	checkTrue("RETRY finds and reads it", untilView(KstView::List, 400));
	leaveAll();
}

static void testSaveKeepsSelection() {
	section("KST screen: SAVE AS keeps the selected file");
	freshKst();
	fakeFatMkdir("KST");
	kst_image_t bench = sim_bench_image();
	putImage("KST/WING.TXT", &bench);
	checkTrue("to the field list", toList());
	tapId(UI_FIX(KstCtl::Files));
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(KstCtl::SaveAs));
	checkTrue("KST001 sorts in ahead of WING", fakeFatExists("KST/KST001.TXT"));
	// WING is row 1 now and stays selected: LOAD loads it, a file equal to
	// the servo, so the list shows nothing pending.
	checkTrue("LOAD acts", ctlEnabled(UI_FIX(KstCtl::Load)));
	kstFrames(1);
	uint32_t row1 = fakeRegionHash(0, 104, GFX_W, 40);
	tapId(UI_ROW(1, Row));
	kstFrames(1);
	checkTrue("row 1, WING, was the selected row", fakeRegionHash(0, 104, GFX_W, 40) == row1);
	leaveAll();
}

/** @brief fakeFatOnCall(): core0 blocked 30 ms in every card call. */
static void slowCardCall() { fakeAdvance(30); }

/** @brief fakeFatOnCall(): core0 blocked 300 ms in every card call. */
static void verySlowCardCall() { fakeAdvance(300); }

static void testSlowCard() {
	section("KST screen: a slow card, a refused backup");
	freshKst();
	checkTrue("to the field list", toList());
	// The first write's card calls block core0 for well over the 250 ms
	// heartbeat: the write still goes out, after them.
	fakeFatOnCall(slowCardCall);
	writeDeadBand();
	fakeFatOnCall(nullptr);
	checkTrue("a first write after slow card calls: verified",
	          job().result == KST_SES_OK && job().refused == KstRefusal::None);
	checkTrue("the record removed", !fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();

	// A lookup of 300 ms per call on finding a second servo: its read still
	// goes out, and the list is that servo's.
	freshKst();
	checkTrue("to the field list", toList());
	sim_t *sim = fakeKstSim();
	sim->powered = false;
	sim->in_mode = false;
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the first servo lost", untilView(KstView::Main, 400));
	kst_image_t second = sim_bench_image();
	second.r[0x0B] ^= 0x01;
	memcpy(sim->reg, second.r, KST_REG_COUNT);
	fakeFatOnCall(verySlowCardCall);
	sim->powered = true;
	checkTrue("the second servo read after a slow lookup", untilView(KstView::List, 400));
	fakeFatOnCall(nullptr);
	checkTrue("the list holds the second servo, with its backup",
	          job().hasBackup && memcmp(job().image.r, second.r, KST_REG_COUNT) == 0);
	leaveAll();

	// The card refuses the backup's write and takes the record.
	freshKst();
	checkTrue("to the field list", toList());
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	writeDeadBand();
	checkTrue("the backup refused, the record taken: the write verified",
	          job().result == KST_SES_OK && !fakeFatExists("BACKUP/KST001.TXT"));
	kstFrames(1);
	checkTrue("the page names no backup file: lost at LEAVE, amber",
	          lineIs(236, "NO BACKUP ON THE CARD: LOST AT LEAVE", C_AMBER));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

/** @brief From the list: BACK, LEAVE, then NEXT SERVO, the next session's search. */
static void unplugAndBack() {
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Leave));
	untilEnabled(UI_FIX(KstCtl::NextServo), 200);
	tapId(UI_FIX(KstCtl::NextServo));
	kstFrames(2);
}

static void testRefusedOps() {
	section("KST screen: a read and a write the job refuses, and a stopped read");
	freshKst();
	// A first read that fails, so READ AGAIN is offered.
	sim_add_fault(fakeKstSim(), SIM_READ, 0x05, 0, 1000, SIM_F_NO_REPLY, 0);
	openKst();
	checkTrue("a failed read: READ AGAIN", untilCtl(UI_FIX(KstCtl::ReadAgain), 400));
	fakeKstSim()->n_faults = 0;
	// READ AGAIN, then core0 stalls 300 ms before core1 takes the read.
	tapId(UI_FIX(KstCtl::ReadAgain));
	fakeAdvance(300);
	core1JobPoll();
	checkTrue("the job refused the read", job().refused == KstRefusal::NoUi);
	kstFrames(2);
	checkTrue("Main: the screen stalled, READ AGAIN",
	          uiKstView() == KstView::Main && ctlEnabled(UI_FIX(KstCtl::ReadAgain)) &&
	          lineIs(80, "THE SCREEN STALLED. READ THE SERVO", C_AMBER));
	checkTrue("and no register of the read before", lineIs(112, "", C_AMBER));

	// READ AGAIN, BACK during it, STAY on the leave page: the read stopped.
	tapId(UI_FIX(KstCtl::ReadAgain));
	for (int i = 0; i < 40 && job().job != KstJobState::Busy; i++) kstFrame();
	checkTrue("the read runs", job().job == KstJobState::Busy);
	tapId(UI_FIX(UI_BACK));
	kstFrames(20);
	tapId(UI_FIX(KstCtl::Stay));
	kstFrames(2);
	checkTrue("BACK, then STAY: the read aborted, not the stall of the read before",
	          uiKstView() == KstView::Main &&
	          lineIs(80, "ABORTED. READ THE SERVO AGAIN.", C_AMBER));
	checkTrue("and no register of a read before", lineIs(94, "", C_AMBER) &&
	                                              lineIs(112, "", C_AMBER));
	tapId(UI_FIX(KstCtl::ReadAgain));
	checkTrue("READ AGAIN reads it", untilView(KstView::List, 400));

	// The write's hold fires, then core0 stalls 300 ms before core1 takes it.
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	unsigned writes = fakeKstSim()->n_writes;
	int x, y;
	ctlCentre(UI_FIX(KstCtl::WriteHold), &x, &y);
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 80 && ctlPresent(UI_FIX(KstCtl::WriteHold)); i++) { fakeHold(x, y); kstFrame(); }
	fakeAdvance(300);
	core1JobPoll();
	fakeRelease();
	checkTrue("the job refused the write", job().refused == KstRefusal::NoUi);
	checkTrue("its result: the screen stalled", untilCtl(UI_FIX(KstCtl::WriteOk), 40) &&
	          lineIs(196, "THE SCREEN STALLED. NOTHING WAS", C_AMBER));
	checkTrue("no write frame, the record kept", fakeKstSim()->n_writes == writes &&
	                                            fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	unplugAndBack();
	checkTrue("LEAVE from a servo in a known state removes the record",
	          !fakeFatExists("KST/KST.TXT") && fakeFatExists("BACKUP/KST001.TXT"));
	checkTrue("the next session names no interrupted write", amberBesideHeadline() == 0);
	leaveAll();
}

static void testRecordKeptAtLeave() {
	section("KST screen: LEAVE removes only the session's own record");
	// A record an earlier session left.
	freshKst();
	kst_image_t rec = sim_bench_image();
	static char text[KST_FILE_MAX];
	size_t n = kst_file_format(&rec, text, sizeof(text));
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	checkTrue("an earlier record: to the field list", toList());
	leaveAll();
	checkTrue("LEAVE without a write keeps it", fakeFatExists("KST/KST.TXT"));

	// The session's first write cut off by a loss, the servo found again.
	freshKst();
	checkTrue("to the field list", toList());
	sim_t *sim = fakeKstSim();
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	sim->powered = false;
	sim->in_mode = false;
	checkTrue("the write cut off: the search", untilView(KstView::Main, 400) &&
	                                           fakeFatExists("KST/KST.TXT"));
	sim->powered = true;
	checkTrue("found and read again", untilView(KstView::List, 400) && !job().locked);
	leaveAll();
	checkTrue("LEAVE keeps the cut-off write's record", fakeFatExists("KST/KST.TXT"));

	// A verified write, then the card swapped for one with a record.
	freshKst();
	checkTrue("to the field list", toList());
	writeDeadBand();
	checkTrue("verified, its record removed", job().result == KST_SES_OK &&
	                                         !fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	noCard();
	kstFrames(2);
	card();
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	kstFrames(2);
	leaveAll();
	checkTrue("LEAVE keeps the other card's record", fakeFatExists("KST/KST.TXT"));

	// An earlier record that does not move, and a backup the card refuses:
	// the card took neither file.
	freshKst();
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	checkTrue("an earlier record: to the field list", toList());
	// The first write is the marker's for the record's rename, the second the
	// marker's for the backup.
	fakeFatFail(FakeFatCall::Rename, FR_DISK_ERR);
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR, 1);
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the card took no file", ctlPresent(UI_FIX(KstCtl::WriteOk)) &&
	                                   !fakeFatExists("BACKUP/KST001.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
	checkTrue("LEAVE keeps the record that stayed", fakeFatExists("KST/KST.TXT"));

	// A verified write, then a card swapped in with no unmount the logger
	// sees: its record holds another servo.
	freshKst();
	checkTrue("to the field list", toList());
	writeDeadBand();
	checkTrue("verified", job().result == KST_SES_OK && !fakeFatExists("KST/KST.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	kst_image_t other = sim_bench_image();
	other.r[0x0B] ^= 0x01;
	static char otherText[KST_FILE_MAX];
	size_t otherLen = kst_file_format(&other, otherText, sizeof(otherText));
	fakeFatPut("KST/KST.TXT", otherText, otherLen);
	leaveAll();
	checkTrue("LEAVE keeps the swapped card's record", fakeFatExists("KST/KST.TXT"));

	// A full card: the earlier record moves to KST001, the backup and the
	// new record do not fit. The next session names no interrupted write.
	freshKst();
	fakeFatReset((uint32_t)n + 64u);
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	checkTrue("a full card: to the field list", toList());
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the card took no file, the record moved",
	          ctlPresent(UI_FIX(KstCtl::WriteOk)) && !fakeFatExists("KST/KST.TXT") &&
	          fakeFatExists("BACKUP/KST001.TXT"));
	checkTrue("the page says why, that the servo is unchanged, and what next",
	          lineIs(196, "THE CARD TOOK NO FILE (FULL OR", C_AMBER) &&
	          lineIs(208, "REMOVED?). NOTHING WENT TO THE SERVO.", C_AMBER) &&
	          lineIs(220, "CHECK THE CARD, TAP OK, WRITE AGAIN.", C_AMBER));
	tapId(UI_FIX(KstCtl::WriteOk));
	unplugAndBack();
	checkTrue("the next session: no interrupted write named", amberBesideHeadline() == 0);
	leaveAll();
}

static void testFlushWait() {
	section("KST screen: the write waits for the backup's flush to the card");
	fakeCacheReset();
	freshKst();
	SdLogStatus logging;
	memset(&logging, 0, sizeof(logging));
	logging.state = SdLogState::Logging;
	fakeSdLogSet(&logging);
	checkTrue("to the field list", toList());
	checkTrue("the claim turned the cache on", fakeCacheCalls().begins >= 1 &&
	                                          cardCacheState() == CardCache::Idle);
	checkTrue("and stopped the running log", !sdLogActive());
	{
		SdLogStatus ended;
		sdLogStatus(&ended);
		checkTrue("SD LOG learns KST SERVO stopped it", ended.end == SdLogEnd::ByKst);
	}
	sim_t *sim = fakeKstSim();
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	unsigned writes = sim->n_writes;
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(5);
	checkTrue("the backup queued: no write frame yet", sim->n_writes == writes &&
	                                                  fakeFatExists("BACKUP/KST001.TXT"));
	checkTrue("the page says so", lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER) &&
	                              lineIs(210, "BLOCKS LEFT: 12", C_TEXT));
	checkTrue("no hold to press meanwhile", !ctlPresent(UI_FIX(KstCtl::WriteHold)));
	fakeCacheSet(CardCache::Flushing, 3);
	kstFrames(1);
	checkTrue("the count follows the flush", lineIs(210, "BLOCKS LEFT: 3", C_TEXT));
	fakeCacheSet(CardCache::Idle, 0);
	checkTrue("flushed: the write runs and verifies",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && job().result == KST_SES_OK &&
	          sim->n_writes > writes);
	tapId(UI_FIX(KstCtl::WriteOk));
	unsigned ends = fakeCacheCalls().ends;
	leaveAll();
	checkTrue("the claim's end turns the cache off", fakeCacheCalls().ends > ends &&
	                                                cardCacheState() == CardCache::Off);

	// A flush that fails: nothing reaches the servo.
	freshKst();
	checkTrue("again to the field list", toList());
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = sim->n_writes;
	unsigned recovers = fakeCacheCalls().recovers;
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	fakeCacheSet(CardCache::Failed, 0);
	kstFrames(2);
	checkTrue("a failed flush: no write frame, the card mounted again",
	          sim->n_writes == writes && fakeCacheCalls().recovers == recovers + 1);
	checkTrue("and the page says why, and what next, in three lines",
	          ctlPresent(UI_FIX(KstCtl::WriteOk)) &&
	          lineIs(196, "THE CARD DID NOT TAKE THE BACKUP", C_AMBER) &&
	          lineIs(208, "(REMOVED OR SWAPPED?). NOTHING WENT TO", C_AMBER) &&
	          lineIs(220, "THE SERVO. TAP OK, WRITE AGAIN.", C_AMBER));
	fakeDumpFrame("shot_kst_flush_failed.ppm");
	tapId(UI_FIX(KstCtl::WriteOk));
	tapUntilGone(UI_FIX(KstCtl::Write));
	kstFrames(1);
	// The fake card kept the files the fake cache failed to flush: the
	// next number comes from them.
	char next[BACKUP_NAME_MAX], want[48];
	kstFilesNextBackup(next);
	snprintf(want, sizeof(want), "BACKUP FIRST: BACKUP/%s", next);
	checkTrue("the next write saves the backup again", lineIs(236, want, C_DIM));
	// Holding again retries: the backup is saved again, then the write runs.
	char again[BACKUP_NAME_MAX + 8];
	snprintf(again, sizeof(again), "BACKUP/%s", next);
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("held again: the backup on the card, the write sent",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 800) && fakeFatExists(again) &&
	          sim->n_writes > writes && job().result == KST_SES_OK);
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();

	// BACK during the flush is drawn disabled and does nothing: the page
	// waits for the card, then writes. Then a SAVE AS whose flush fails: the
	// card is mounted again, and the first write starts over.
	freshKst();
	checkTrue("a third time to the field list", toList());
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = sim->n_writes;
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	checkTrue("BACK during the flush: disabled, and drawn so", uiKstView() == KstView::Write &&
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	checkControlTable("KST write page, waiting for the flush");
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("a tap on it: the page waits, no write frame", uiKstView() == KstView::Write &&
	          sim->n_writes == writes && lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER));
	fakeCacheSet(CardCache::Idle, 0);
	checkTrue("the flush ends: the write runs and verifies", untilCtl(UI_FIX(KstCtl::WriteOk), 800) &&
	          sim->n_writes > writes && job().result == KST_SES_OK);
	tapId(UI_FIX(KstCtl::WriteOk));
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapId(UI_FIX(KstCtl::Files));
	fakeCacheFlushTo(CardCache::Failed);
	recovers = fakeCacheCalls().recovers;
	tapId(UI_FIX(KstCtl::SaveAs));
	kstFrames(1);
	checkTrue("SAVE AS: the flush fails, the card mounted again",
	          fakeCacheCalls().recovers == recovers + 1 && cardCacheState() == CardCache::Idle);
	fakeCacheFlushTo(CardCache::Idle);
	tapId(UI_FIX(UI_BACK));
	tapUntilGone(UI_FIX(KstCtl::Write));
	kstFrames(1);
	kstFilesNextBackup(next);
	snprintf(want, sizeof(want), "BACKUP FIRST: BACKUP/%s", next);
	checkTrue("the write after it saves the backup again", lineIs(236, want, C_DIM));
	tapId(UI_FIX(UI_BACK));

	// A batch that failed off the write page, with no first write pending:
	// the next write mounts the card again before its files.
	fakeCacheSet(CardCache::Failed, 0);
	recovers = fakeCacheCalls().recovers;
	writes = sim->n_writes;
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("a failed batch before the write: recovered, the write runs",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && fakeCacheCalls().recovers == recovers + 1 &&
	          job().result == KST_SES_OK && sim->n_writes > writes);
	tapId(UI_FIX(KstCtl::WriteOk));

	// Another card in the slot since the backup reached the card: its serial
	// differs, so the card is mounted again and the backup saved to it.
	fakeVolumeSet(2);
	recovers = fakeCacheCalls().recovers;
	kstFilesNextBackup(next);
	writes = sim->n_writes;
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	char path[CARD_PATH_MAX];
	snprintf(path, sizeof(path), "BACKUP/%s", next);
	checkTrue("another card: mounted again, the backup saved to it, then the write",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && fakeCacheCalls().recovers == recovers + 1 &&
	          fakeFatExists(path) && job().result == KST_SES_OK && sim->n_writes > writes);
	tapId(UI_FIX(KstCtl::WriteOk));
	// The same card, a batch still queued: the page waits for it without
	// blocking, the check runs as it ends; no remount, no second backup.
	fakeCacheSet(CardCache::Flushing, 2);
	recovers = fakeCacheCalls().recovers;
	kstFilesNextBackup(next);
	writes = sim->n_writes;
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(3);
	checkTrue("a batch still queued: the saving page, no write frame",
	          lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER) && sim->n_writes == writes);
	fakeCacheSet(CardCache::Idle, 0);
	snprintf(path, sizeof(path), "BACKUP/%s", next);
	checkTrue("flushed, the same card: the write, no remount, no backup",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && fakeCacheCalls().recovers == recovers &&
	          !fakeFatExists(path) && sim->n_writes > writes);
	tapId(UI_FIX(KstCtl::WriteOk));

	// Another card swapped in while a batch was queued: the check at the
	// flush's end mounts it again, and the backup goes to it first.
	fakeCacheSet(CardCache::Flushing, 2);
	recovers = fakeCacheCalls().recovers;
	kstFilesNextBackup(next);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	fakeVolumeSet(3);
	fakeCacheSet(CardCache::Idle, 0);
	snprintf(path, sizeof(path), "BACKUP/%s", next);
	checkTrue("swapped during the wait: mounted again, the backup saved, then the write",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && fakeCacheCalls().recovers == recovers + 1 &&
	          fakeFatExists(path) && job().result == KST_SES_OK);
	tapId(UI_FIX(KstCtl::WriteOk));

	// A serial number that never reads: one remount and one backup for the
	// write, then the write; no loop of backups.
	fakeVolumeFailNext(1000);
	recovers = fakeCacheCalls().recovers;
	kstFilesNextBackup(next);
	writes = sim->n_writes;
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("a serial that never reads: one remount, one backup, the write",
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && fakeCacheCalls().recovers == recovers + 1 &&
	          sim->n_writes > writes);
	char after[BACKUP_NAME_MAX];
	kstFilesNextBackup(after);
	checkTrue("and only one new backup", strcmp(after, next) != 0 &&
	          backupNumber(after, nullptr) == backupAfter(backupNumber(next, nullptr)));
	tapId(UI_FIX(KstCtl::WriteOk));
	fakeVolumeFailNext(0);

	// An earlier batch still queued, and it fails: the page names a file, not
	// the backup, and nothing went to the servo.
	fakeCacheSet(CardCache::Flushing, 2);
	writes = sim->n_writes;
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	fakeCacheSet(CardCache::Failed, 0);
	kstFrames(2);
	checkTrue("an earlier batch failed: A FILE, nothing to the servo, in three lines",
	          ctlPresent(UI_FIX(KstCtl::WriteOk)) && sim->n_writes == writes &&
	          lineIs(196, "THE CARD DID NOT TAKE A FILE (REMOVED", C_AMBER) &&
	          lineIs(208, "OR SWAPPED?). NOTHING WENT TO THE", C_AMBER) &&
	          lineIs(220, "SERVO. TAP OK, WRITE AGAIN.", C_AMBER));
	tapId(UI_FIX(KstCtl::WriteOk));

	// LOAD after a batch failed in the background: the card is mounted
	// again, the list read again, and nothing loads until a row is chosen.
	tapId(UI_FIX(KstCtl::Files));
	tapId(UI_ROW(0, Row));
	fakeCacheSet(CardCache::Failed, 0);
	recovers = fakeCacheCalls().recovers;
	tapId(UI_FIX(KstCtl::Load));
	checkTrue("LOAD after a failed batch: mounted again, FILES stays, no row chosen",
	          uiKstView() == KstView::Files && fakeCacheCalls().recovers == recovers + 1 &&
	          !ctlEnabled(UI_FIX(KstCtl::Load)));
	tapId(UI_ROW(0, Row));
	tapId(UI_FIX(KstCtl::Load));
	checkTrue("the row chosen again: it loads", uiKstView() == KstView::List);
	// The file may equal the servo as it stands: then nothing is pending.
	tapId(UI_FIX(KstCtl::Revert), IdExpect::Any);

	// FILES after a batch failed in the background: mounted again first,
	// since the failed cache refuses every read.
	fakeCacheSet(CardCache::Failed, 0);
	recovers = fakeCacheCalls().recovers;
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("FILES after a failed batch: recovered, the list read",
	          uiKstView() == KstView::Files && fakeCacheCalls().recovers == recovers + 1 &&
	          cardCacheState() == CardCache::Idle);
	tapId(UI_FIX(UI_BACK));

	// NEXT SERVO keeps the claim: the cache is on again.
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Leave));
	checkTrue("LEAVE: the page releasing, the cache off",
	          ctlPresent(UI_FIX(KstCtl::Restart)) && cardCacheState() == CardCache::Off);
	untilEnabled(UI_FIX(KstCtl::NextServo), 200);
	unsigned begins = fakeCacheCalls().begins;
	tapId(UI_FIX(KstCtl::NextServo));
	checkTrue("NEXT SERVO: the cache on again",
	          uiKstView() == KstView::Main && fakeCacheCalls().begins == begins + 1 &&
	          cardCacheState() == CardCache::Idle);
	leaveAll();
	fakeCacheReset();
}

/** @brief The strip's text, as uiStripText() draws it at scale 1. */
static bool stripIs(const char *text, uint16_t col) { return lineIs(UI_STRIP_Y + 4, text, col); }

/**
 * @brief A switched search that finds no servo in 30 s names the converter
 *        and the setting a servo on its own supply needs: OWN.
 */
static void testSwitchedTimeout() {
	section("KST screen: a switched search with no servo names SERVO SUPPLY AT OWN");
	freshKst(false);
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	openKst();
	checkTrue("30 s without a servo: RETRY", untilCtl(UI_FIX(KstCtl::Retry), 1300) &&
	                                         job().seek == KstSeek::TimedOut);
	checkTrue("the converter on GP28",
	          lineIs(80, "CHECK THE CONVERTER ON GP28, ITS", C_TEXT));
	checkTrue("and a servo on its own supply: SERVO SUPPLY AT OWN",
	          lineIs(112, "A SERVO ON ITS OWN SUPPLY NEEDS", C_TEXT) &&
	          lineIs(126, "SETTINGS > SERVO SUPPLY AT OWN.", C_TEXT));
	leaveAll();
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
}

static void testSwitchedSupply() {
	section("KST screen: the switched supply");
	freshKst(false);
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	supplySimInit(&g_sup, fakeKstSim(), time_us_32());
	g_supOn = true;
	openKst();
	kstFrames(20);
	checkTrue("the switched search: SWITCHED SUPPLY, the ESC warning in amber",
	          job().job == KstJobState::Searching && job().supplyFitted &&
	          lineIs(100, "THE BENCH SWITCHES THE SERVO OFF FOR", C_DIM) &&
	          lineIs(164, "AN ESC ON THIS LEAD GETS THE", C_AMBER));
	char want[48];
	unsigned ds = job().supplyLeftDs;
	snprintf(want, sizeof(want), "SIG GP0  TRY 1  SUPPLY OFF %u.%u S", ds / 10, ds % 10);
	checkTrue("the strip counts the off phase down", stripIs(want, C_DIM));
	fakeDumpFrame("shot_kst_switched.ppm");
	checkTrue("found on the first power-up: the field list", untilView(KstView::List, 400) &&
	          enHigh() && job().attempts == 1);

	// The leave page offers REBOOT; a hold reboots and reads again.
	tapId(UI_FIX(UI_BACK));
	checkTrue("the leave page: LEAVE SWITCHES THE SUPPLY OFF., HOLD TO REBOOT",
	          uiKstView() == KstView::Leave &&
	          leaveStepIs(66, '1', "LEAVE SWITCHES THE SUPPLY OFF.", true) &&
	          leaveStepIs(80, '2', "UNPLUG THE SERVO.", false) &&
	          leaveStepIs(118, '3', "RESTART DSHOT.", false) &&
	          ctlEnabled(UI_FIX(KstCtl::Reboot)) && ctlEnabled(UI_FIX(KstCtl::Stay)));
	{
		UiRect r = ctlRect(UI_FIX(KstCtl::Reboot));
		checkTrue("HOLD TO REBOOT right of STAY, at scale 1",
		          r.x == UI_LEAVE_RIGHT.x && r.y == UI_LEAVE_RIGHT.y && r.w == UI_LEAVE_RIGHT.w &&
		          fakeTextIs(r.x + (r.w - gfxTextW("HOLD TO REBOOT", 1)) / 2, r.y + (r.h - 7) / 2,
		                     "HOLD TO REBOOT", C_TEXT, C_PANEL, 1));
	}
	checkControlTable("KST leave, switched");
	fakeDumpFrame("shot_leave_kst_switched.ppm");
	noCard();
	kstFrames(4);
	checkTrue("no card: the two notes above HOLD TO REBOOT name the backup in RAM",
	          leaveNoteIs(0, "NO CARD: REBOOT LOSES THE BACKUP", C_AMBER) &&
	          leaveNoteIs(1, "IN RAM.", C_AMBER));
	card();
	kstFrames(4);
	checkTrue("HOLD TO REBOOT fires", holdUntilGone(UI_FIX(KstCtl::Reboot)));
	kstFrames(4);
	checkTrue("rebooting: the supply off, the main view says so",
	          job().job == KstJobState::Rebooting && !enHigh() &&
	          lineIs(80, "REBOOT: DO NOT UNPLUG THE SERVO.", C_AMBER));
	checkTrue("then on, an entry, a read: the field list again",
	          untilView(KstView::List, 300) && job().hasBackup && enHigh());

	// LEAVE: the page counts the 2 s off down, then RESTART DSHOT 1 s later.
	tapId(UI_FIX(UI_BACK));
	uint32_t t0 = time_us_32();
	tapId(UI_FIX(KstCtl::Leave));
	kstFrames(4);
	checkTrue("LEAVE: the supply off, the page releasing",
	          uiKstView() == KstView::Leave && !enHigh() && !ctlEnabled(UI_FIX(KstCtl::Restart)) &&
	          !ctlEnabled(UI_FIX(KstCtl::NextServo)) && !ctlPresent(UI_FIX(KstCtl::Reboot)));
	{
		char want[48];
		unsigned left = job().supplyLeftDs;
		snprintf(want, sizeof(want), "SUPPLY OFF FOR 2.0 S: %u.%u S LEFT.", left / 10, left % 10);
		checkTrue("the note counts the 2 s down, dim", left > 0 && leaveNoteIs(0, want, C_DIM));
	}
	fakeDumpFrame("shot_leave_kst_supply_off.ppm");
	checkTrue("BACK disabled, and drawn so",
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	checkControlTable("KST leave, the supply off");
	checkTrue("NEXT SERVO once the supply has been off 2 s",
	          untilEnabled(UI_FIX(KstCtl::NextServo), 200) && time_us_32() - t0 >= 2000000u);
	checkTrue("with the switch no NEXT SERVO line: the bench powers it", leaveNoteBlank(0));
	checkTrue("RESTART DSHOT 1000 ms later", !ctlEnabled(UI_FIX(KstCtl::Restart)) &&
	          untilEnabled(UI_FIX(KstCtl::Restart), 60) && time_us_32() - t0 >= 3000000u);
	checkTrue("BACK stays disabled, and drawn so",
	          !ctlEnabled(UI_FIX(UI_BACK)) && backLook() == BackLook::Disabled);
	FakePin p = fakePin(servoSupplyPinOf(1));
	checkTrue("GP28 an undriven input after the claim", !p.output && p.inputOff);
	tapId(UI_FIX(KstCtl::Restart));
	kstFrames(8);
	tapId(UI_FIX(UI_BACK));
	kstFrames(2);

	// The stalled screen: core1 runs, the UI does not.
	openKst();
	untilView(KstView::List, 400);
	for (int i = 0; i < 400; i++) core1Pass();
	kstFrames(2);
	checkTrue("a stall: the supply off, the stalled screen, RETRY",
	          !enHigh() && job().stalled && uiKstView() == KstView::Main &&
	          stripIs("SIG GP0  SUPPLY OFF", C_DIM) && ctlEnabled(UI_FIX(KstCtl::Retry)));
	fakeDumpFrame("shot_kst_stalled.ppm");
	tapId(UI_FIX(KstCtl::Retry));
	checkTrue("RETRY powers it up and reads it", untilView(KstView::List, 400) && enHigh());

	// BACK during REBOOT closes as LEAVE does: the leave page opens at its
	// unplug step, with no step before LEAVE.
	tapId(UI_FIX(UI_BACK));
	checkTrue("HOLD TO REBOOT fires again", holdUntilGone(UI_FIX(KstCtl::Reboot)));
	kstFrames(2);
	checkTrue("rebooting", job().job == KstJobState::Rebooting && uiKstView() == KstView::Main);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK during REBOOT: the leave page, releasing, the supply off",
	          uiKstView() == KstView::Leave && !ctlPresent(UI_FIX(KstCtl::Leave)) &&
	          !ctlPresent(UI_FIX(KstCtl::Stay)) && ctlPresent(UI_FIX(KstCtl::Restart)) &&
	          !ctlEnabled(UI_FIX(KstCtl::Restart)) && !enHigh() &&
	          leaveStepIs(UI_LEAVE_STEP_Y, '1', "UNPLUG THE SERVO.", true));
	checkTrue("NEXT SERVO once the supply has been off 2 s",
	          untilEnabled(UI_FIX(KstCtl::NextServo), 200));
	leaveAll();
	g_supOn = false;
	servoSupplyConfigure(0);
	settings()->servoSupply = 0;
}

/**
 * @brief A write that waits for the card's flush ends with its page: a stall
 *        while it waits takes the screen to Main, and the next write opens
 *        on a tap and sends nothing until its own hold, whatever the card
 *        does meanwhile.
 */
static bool anyLine(const char *text, uint16_t col);

static void testFlushWaitEndsWithThePage() {
	section("KST screen: a stall during the flush wait; the next write needs its hold");
	fakeCacheReset();
	freshKst(false);
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	supplySimInit(&g_sup, fakeKstSim(), time_us_32());
	g_supOn = true;
	SdLogStatus logging;
	memset(&logging, 0, sizeof(logging));
	logging.state = SdLogState::Logging;
	fakeSdLogSet(&logging);
	openKst();
	checkTrue("the switched servo read: the field list", untilView(KstView::List, 400));
	sim_t *sim = fakeKstSim();
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	unsigned writes = sim->n_writes;
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	checkTrue("the hold waits for the flush: no write frame",
	          uiKstView() == KstView::Write && sim->n_writes == writes &&
	          lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER));
	// The UI stops for 400 ms: the heartbeat lapses and the job switches the
	// servo off. The card still flushes.
	for (int i = 0; i < 400; i++) core1Pass();
	kstFrames(2);
	checkTrue("a stall during the wait: Main with RETRY, no write frame",
	          uiKstView() == KstView::Main && ctlEnabled(UI_FIX(KstCtl::Retry)) &&
	          sim->n_writes == writes);
	tapId(UI_FIX(KstCtl::Retry));
	checkTrue("RETRY reads the servo again", untilView(KstView::List, 400));
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	writes = sim->n_writes;
	checkTrue("a tap on WRITE...: the write page", tapUntilGone(UI_FIX(KstCtl::Write)) &&
	          uiKstView() == KstView::Write);
	fakeCacheSet(CardCache::Idle, 0);
	kstFrames(40);
	checkTrue("the card idle: no write frame without the page's hold, HOLD TO WRITE waits",
	          sim->n_writes == writes && ctlEnabled(UI_FIX(KstCtl::WriteHold)) &&
	          !ctlPresent(UI_FIX(KstCtl::WriteOk)));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the hold writes, and the write verifies", untilCtl(UI_FIX(KstCtl::WriteOk), 800) &&
	          sim->n_writes > writes && job().result == KST_SES_OK);
	tapId(UI_FIX(KstCtl::WriteOk));

	// A write left in doubt, then a stall: the stalled screen names the
	// steps that restore the servo, the backup by name. The session after
	// RETRY saved its backup as KST003, after the record that took KST002.
	sim_add_fault(sim, SIM_WRITE, 0x03, 0, 1, SIM_F_WRITE_ALSO, 0x13);
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the write ends in doubt", untilCtl(UI_FIX(KstCtl::WriteOk), 800) && job().locked);
	tapId(UI_FIX(KstCtl::WriteOk));
	for (int i = 0; i < 400; i++) core1Pass();
	kstFrames(2);
	checkTrue("a stall in doubt: LOAD the backup in FILES, then WRITE... and the hold",
	          uiKstView() == KstView::Main &&
	          anyLine("THE LAST WRITE IS IN DOUBT. AFTER THE", C_AMBER) &&
	          anyLine("READ, LOAD KST003 IN FILES, TAP", C_AMBER) &&
	          anyLine("WRITE... AND HOLD.", C_AMBER));
	sim->n_faults = 0;
	leaveAll();
	g_supOn = false;
	servoSupplyConfigure(0);
	settings()->servoSupply = 0;
	fakeCacheReset();
	card();
}

/**
 * @brief With CURRENT SENSE at INA219, the strip reads the current after
 *        SUPPLY ON; with NONE it does not.
 */
static void testSupplyCurrent() {
	section("KST screen: the current after SUPPLY ON");
	freshKst(false);
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	settings()->currentSense = 1;
	senseConfigure(1);
	fakeI2cReset();
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	ina->currentA = 0.08;
	supplySimInit(&g_sup, fakeKstSim(), time_us_32());
	g_supOn = true;
	openKst();
	int i = 0;
	for (; i < 400 && !(job().job == KstJobState::Searching && job().supplyOn); i++) kstFrame();
	kstFrames(1);
	char want[48];
	snprintf(want, sizeof(want), "SIG GP0  TRY %u  SUPPLY ON 0.08 A", (unsigned)job().attempts);
	checkTrue("the switched search, supply on: SUPPLY ON 0.08 A",
	          job().supplyOn && uiKstView() == KstView::Main && stripIs(want, C_DIM));
	ina->currentA = 0.12;
	kstFrames(1);
	snprintf(want, sizeof(want), "SIG GP0  TRY %u  SUPPLY ON 0.12 A", (unsigned)job().attempts);
	checkTrue("a new reading repaints the strip", !job().supplyOn || stripIs(want, C_DIM));
	leaveAll();
	g_supOn = false;
	servoSupplyConfigure(0);
	settings()->servoSupply = 0;

	// SEARCH STOPPED on the servo's own supply: the strip shows no current,
	// so a new reading repaints nothing.
	freshKst(false);
	settings()->currentSense = 1;
	senseConfigure(1);
	openKst();
	untilCtl(UI_FIX(KstCtl::StopSearch), 40);
	tapId(UI_FIX(KstCtl::StopSearch));
	checkTrue("SEARCH STOPPED: RETRY", untilCtl(UI_FIX(KstCtl::Retry), 40));
	kstFrames(3);
	ina->currentA = 0.31;
	kstFrame();
	checkInt("a new reading while the strip shows none: nothing repainted", fakeLastFlushBands(), 0);
	leaveAll();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
}

/** @brief Card calls made with the wait hook set, and without. */
static unsigned n_hooked = 0, n_unhooked = 0;

/** @brief fakeFatOnCall(): count the calls by the hook. */
static void hookSeenCall() { (sdh_wait_hook ? n_hooked : n_unhooked)++; }

/** @brief fakeFatOnCall(): core0 300 ms in the driver's waits, which call the hook. */
static void stampedSlowCall() {
	hookSeenCall();
	for (int i = 0; i < 300; i++) {
		if (sdh_wait_hook) sdh_wait_hook();
		core1Pass();
	}
}

/** @brief fakeFatOnCall(): core0 300 ms in a wait that never calls the hook. */
static void unstampedSlowCall() {
	n_cardCalls++;
	for (int i = 0; i < 300; i++) core1Pass();
}

static void testCardPhase() {
	section("KST screen, switched: card calls inside a card phase");
	freshKst(false);
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	supplySimInit(&g_sup, fakeKstSim(), time_us_32());
	g_supOn = true;
	fakeCardWaitPass(core1Pass);
	checkTrue("to the field list", toList() && enHigh());
	// The first write's card calls each keep core0 300 ms, past the 250 ms
	// heartbeat; the driver's waits stamp, and the supply stays on.
	n_hooked = n_unhooked = 0;
	fakeFatOnCall(stampedSlowCall);
	writeDeadBand();
	fakeFatOnCall(nullptr);
	kstFrames(1);
	checkTrue("300 ms card calls that stamp: the write verified, the supply on",
	          job().result == KST_SES_OK && enHigh() && !job().stalled && !job().cardPhase);
	checkTrue("the backup on the card", fakeFatExists("BACKUP/KST001.TXT"));
	checkTrue("every card call of the write ran in a phase, the hook set",
	          n_hooked > 0 && n_unhooked == 0);
	tapId(UI_FIX(KstCtl::WriteOk));
	kstFrames(2);
	checkTrue("no card call open: the hook cleared", sdh_wait_hook == nullptr);
	leaveAll();

	// The same calls with no stamps: the job counts a lapse and switches off.
	openKst();
	checkTrue("to the field list again", untilView(KstView::List, 400));
	n_cardCalls = 0;
	uint32_t offBefore = g_sup.lastOffUs;
	fakeFatOnCall(unstampedSlowCall);
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	fakeFatOnCall(nullptr);
	kstFrames(10);
	checkTrue("300 ms card calls that do not stamp: the supply switched off",
	          n_cardCalls > 0 && g_sup.lastOffUs != offBefore && !enHigh());
	leaveAll();
	fakeCardWaitPass(nullptr);
	g_supOn = false;
	servoSupplyConfigure(0);
	settings()->servoSupply = 0;

	// NONE: no phase, the hook never set.
	freshKst();
	checkTrue("NONE: to the field list", toList());
	n_hooked = n_unhooked = 0;
	fakeFatOnCall(hookSeenCall);
	writeDeadBand();
	fakeFatOnCall(nullptr);
	checkTrue("NONE: the first write's card calls run without the hook",
	          n_unhooked > 0 && n_hooked == 0 && !job().cardPhase);
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

static bool anyLine(const char *text, uint16_t col) {
	for (int y = 60; y < 240; y++) if (lineIs(y, text, col)) return true;
	return false;
}
static void testRecordWarningOnSearch() {
	section("KST screen: the interrupted-write warning on the search, by hand and switched");
	kst_image_t rec = sim_bench_image();
	static char text[KST_FILE_MAX];
	size_t n = kst_file_format(&rec, text, sizeof(text));
	freshKst(false);
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	openKst();
	kstFrames(10);
	checkTrue("by hand: the search names the interrupted write",
	          job().job == KstJobState::Searching && anyLine("A WRITE WAS INTERRUPTED. KST/KST.TXT", C_AMBER));
	leaveAll();
	freshKst(false);
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	supplySimInit(&g_sup, fakeKstSim(), time_us_32());
	g_supOn = true;
	openKst();
	kstFrames(10);
	checkTrue("switched: the search still names it, ahead of the explanations",
	          job().job == KstJobState::Searching && anyLine("A WRITE WAS INTERRUPTED. KST/KST.TXT", C_AMBER));
	checkTrue("with its second line", anyLine("HOLDS THE SERVO AS IT WAS.", C_AMBER));
	checkTrue("the failsafe caution stays",
	          anyLine("A POWERED SERVO ON THIS PIN GETS NO", C_DIM) &&
	          anyLine("PULSES AND MAY GO TO ITS FAILSAFE.", C_DIM));
	checkTrue("and the converter warning",
	          anyLine("AN ESC ON THIS LEAD GETS THE", C_AMBER) &&
	          anyLine("CONVERTER'S OUTPUT ON ITS BEC WIRE.", C_AMBER));
	checkTrue("the how-it-works pair gives way",
	          !anyLine("THE BENCH SWITCHES THE SERVO OFF FOR", C_DIM));
	leaveAll();
	g_supOn = false;
	servoSupplyConfigure(0);
	settings()->servoSupply = 0;
}

/** @brief The board's voltage at @p volts, held for enough frames to settle. */
static void boardVolts(float volts) {
	fakeAdcSet((uint16_t)(volts / 3.0f / 3.3f * 4095.0f + 0.5f));
	kstFrames(60);
}

/** @brief Hold @p id for 50 frames, 1250 ms, and let go. */
static void holdFor(UiControlId id) {
	int x, y;
	ctlCentre(id, &x, &y);
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 50; i++) { fakeHold(x, y); kstFrame(); }
	fakeRelease();
	kstFrames(2);
}

/**
 * @brief The BOARD voltage red refuses the write page's hold, a write and a
 *        restore: no file and no frame, two amber lines. Amber, no cell and
 *        the text colour write. A write that waits for its backup's flush
 *        is refused when the voltage turns red meanwhile.
 */
static void testBoardLow() {
	section("KST screen: no write or restore while the BOARD voltage is red");
	fakeCacheReset();
	freshKst();
	checkTrue("to the field list", toList());
	uint16_t deadBand = simField(KST_F_DEAD_BAND);
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("the write page", tapUntilGone(UI_FIX(KstCtl::Write)) && uiKstView() == KstView::Write);
	boardVolts(3.20f);
	checkTrue("3.20 V: red", uiBoardCellNow() == BoardCell::Critical);
	unsigned writes = fakeKstSim()->n_writes;
	holdFor(UI_FIX(KstCtl::WriteHold));
	checkTrue("the hold writes nothing: no backup, no record, the servo as it was",
	          !fakeFatExists("BACKUP/KST001.TXT") && !fakeFatExists("KST/KST.TXT") &&
	          simField(KST_F_DEAD_BAND) == deadBand && fakeKstSim()->n_writes == writes);
	checkTrue(UI_BOARD_LOW_1 ", amber, y 196", lineIs(196, UI_BOARD_LOW_1, C_AMBER));
	checkTrue(UI_BOARD_LOW_2 ", amber, y 208", lineIs(208, UI_BOARD_LOW_2, C_AMBER));
	int x0 = 0, x1 = 0;
	checkTrue("both inside the panel's 8 px margins",
	          fakeInkExtent(196, 20, C_BG, &x0, &x1) && x0 >= 8 && x1 < GFX_W - 8);
	checkTrue("HOLD TO WRITE stays", ctlEnabled(UI_FIX(KstCtl::WriteHold)));
	checkControlTable("KST write page, board low");
	fakeDumpFrame("shot_kst_board_low.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: the field list", uiKstView() == KstView::List);
	checkTrue("the write page again: no refusal lines before a hold",
	          tapUntilGone(UI_FIX(KstCtl::Write)) && uiKstView() == KstView::Write &&
	          !lineIs(196, UI_BOARD_LOW_1, C_AMBER) && !lineIs(208, UI_BOARD_LOW_2, C_AMBER));

	// Amber, no cell to judge, the text colour: each writes.
	const float volts[] = { 3.40f, 1.00f, 3.92f };
	const BoardCell band[] = { BoardCell::Low, BoardCell::NoCell, BoardCell::Ok };
	const char *what[] = { "3.40 V, amber: the hold writes", "1.00 V, no cell: the hold writes",
	                       "3.92 V: the hold writes" };
	for (int k = 0; k < 3; k++) {
		boardVolts(volts[k]);
		uint16_t was = simField(KST_F_DEAD_BAND);
		if (k > 0) {
			selectField(KST_F_DEAD_BAND);
			tapId(UI_FIX(KstCtl::EditPlus));
			tapUntilGone(UI_FIX(KstCtl::Write));
		}
		checkTrue(what[k], uiBoardCellNow() == band[k] && holdUntilGone(UI_FIX(KstCtl::WriteHold)) &&
		          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && simField(KST_F_DEAD_BAND) != was &&
		          !lineIs(196, UI_BOARD_LOW_1, C_AMBER));
		tapId(UI_FIX(KstCtl::WriteOk));
	}
	checkTrue("the backup went first", fakeFatExists("BACKUP/KST001.TXT"));

	// RESTORE is a write: refused the same way, from FILES.
	uint16_t written = simField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::Files));
	boardVolts(3.20f);
	checkTrue("RESTORE... acts", ctlEnabled(UI_FIX(KstCtl::Restore)));
	checkTrue("a tap opens the restore page", tapUntilGone(UI_FIX(KstCtl::Restore)) &&
	          uiKstView() == KstView::Write);
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(4);
	checkTrue("its hold at 3.20 V restores nothing", simField(KST_F_DEAD_BAND) == written &&
	          lineIs(196, UI_BOARD_LOW_1, C_AMBER) && ctlPresent(UI_FIX(KstCtl::WriteHold)));
	boardVolts(3.92f);
	checkTrue("3.92 V: the hold restores", holdUntilGone(UI_FIX(KstCtl::WriteHold)) &&
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && simField(KST_F_DEAD_BAND) == deadBand);
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();

	section("KST screen: the BOARD voltage turns red while the backup flushes");
	fakeCacheReset();
	freshKst();
	SdLogStatus logging;
	memset(&logging, 0, sizeof(logging));
	logging.state = SdLogState::Logging;
	fakeSdLogSet(&logging);
	checkTrue("to the field list", toList());
	deadBand = simField(KST_F_DEAD_BAND);
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = fakeKstSim()->n_writes;
	checkTrue("the hold at 3.92 V queues the backup", holdUntilGone(UI_FIX(KstCtl::WriteHold)) &&
	          lineIs(196, "SAVING THE BACKUP TO THE CARD.", C_AMBER));
	boardVolts(3.20f);
	fakeCacheSet(CardCache::Idle, 0);
	kstFrames(5);
	checkTrue("red when the flush ends: no write frame, the two lines",
	          fakeKstSim()->n_writes == writes && simField(KST_F_DEAD_BAND) == deadBand &&
	          lineIs(196, UI_BOARD_LOW_1, C_AMBER) && ctlEnabled(UI_FIX(KstCtl::WriteHold)));
	boardVolts(3.92f);
	checkTrue("3.92 V: the next hold writes", holdUntilGone(UI_FIX(KstCtl::WriteHold)) &&
	          untilCtl(UI_FIX(KstCtl::WriteOk), 400) && simField(KST_F_DEAD_BAND) != deadBand);
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
	fakeCacheReset();
	fakeAdcSet(fakeAdcDefault());
}

/** @brief The framebuffer's pixel at (@p x, @p y). */
static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/** @brief Whether any pixel of the rectangle is @p col. */
static bool regionHas(int x, int y, int w, int h, uint16_t col) {
	const uint16_t *fb = gfxBuffer();
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (fb[j * GFX_W + i] == col) return true;
	return false;
}

/**
 * @brief The colour roles on KST SERVO (docs/design/menu-b.md, decisions 8
 *        and 11): the headline amber, PROG MODE on blue in a square chip, a
 *        row's value by its state with its bar, the editor bar's value
 *        likewise, REVERT amber, IN DOUBT red, the write page's new value
 *        cyan beside the old one dim.
 */
static void testColourRoles() {
	section("KST screen: the colour roles");
	freshKst(false);
	openKst();
	checkTrue("the search", untilCtl(UI_FIX(KstCtl::StopSearch), 40));
	kstFrames(2);
	checkTrue("POWER UP / THE SERVO NOW in amber, the current step",
	          fakeTextCentredIs(70, "POWER UP", C_AMBER, C_BG, 2) &&
	          fakeTextCentredIs(88, "THE SERVO NOW", C_AMBER, C_BG, 2));
	fakeKstSim()->powered = true;
	checkTrue("the field list", untilView(KstView::List, 400));
	kstFrames(1);
	const int cw = gfxTextW("PROG MODE", 1) + 12, cx = GFX_W - UI_MARGIN - cw;
	checkTrue("PROG MODE on blue, a square 65 px chip from x 167",
	          cx == 167 && fakeTextIs(cx + 6, 44, "PROG MODE", C_ONACCENT, C_BLUE, 1) &&
	          pixel(cx, 42) == C_BLUE && pixel(cx + cw - 1, 53) == C_BLUE &&
	          pixel(cx, 41) == C_BG && pixel(cx, 54) == C_BG);
	checkTrue("the strip's caption ends 4 px or more before the chip",
	          !regionHas(cx - 4, UI_STRIP_Y, 4, UI_STRIP_H, C_DIM));
	checkTrue("the strip names the backup in RAM: V6  GP0  BACKUP IN RAM",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "V6  GP0  BACKUP IN RAM", C_DIM, C_BG, 1));

	selectField(KST_F_DEAD_BAND);
	kstFrames(1);
	int row = kstRowOf(KST_F_DEAD_BAND);
	UiRect r = ctlRect(UI_ROW(row, Row));
	// The value is right-aligned to x 231 at scale 2, y + 13 to y + 26.
	auto valueHas = [&](uint16_t col) { return regionHas(120, r.y + 13, 112, 14, col); };
	checkTrue("selected, as read: the value in the text colour, never lime; no bar",
	          valueHas(C_TEXT) && !valueHas(C_LIME) && !valueHas(C_CYAN) && pixel(1, r.y + 5) == C_PANEL);
	checkTrue("the editor bar's value in the text colour",
	          regionHas(52, 234, 136, 14, C_TEXT) && !regionHas(52, 234, 136, 14, C_LIME));
	const UiRect rv = ctlRect(UI_FIX(KstCtl::Revert));
	tapId(UI_FIX(KstCtl::EditPlus));
	kstFrames(1);
	r = ctlRect(UI_ROW(row, Row));
	checkTrue("+: changed, the value cyan and a 3 px cyan bar",
	          valueHas(C_CYAN) && !valueHas(C_TEXT) && pixel(0, r.y + 5) == C_CYAN &&
	          pixel(2, r.y + 5) == C_CYAN && pixel(3, r.y + 5) == C_PANEL);
	checkTrue("the editor bar's value cyan",
	          regionHas(52, 234, 136, 14, C_CYAN) && !regionHas(52, 234, 136, 14, C_LIME));
	checkTrue("REVERT's label amber",
	          fakeTextIs(rv.x + (rv.w - gfxTextW("REVERT", 1)) / 2, rv.y + (rv.h - 7) / 2,
	                     "REVERT", C_AMBER, C_PANEL, 1));
	tapId(UI_FIX(KstCtl::EditMinus));
	tapId(UI_FIX(KstCtl::EditMinus));
	kstFrames(1);
	r = ctlRect(UI_ROW(row, Row));
	checkTrue("- below the backup narrows the band, a soft rule: the bar amber, the value cyan",
	          valueHas(C_CYAN) && pixel(1, r.y + 5) == C_AMBER);
	checkTrue("the rule's top line in cyan, the new value", regionHas(52, 220, 136, 7, C_CYAN));
	int lockRow = kstRowOf(KST_F_LOCK);
	revealRow(lockRow);
	kstFrames(1);
	UiRect lr = ctlRect(UI_ROW(lockRow, Row));
	checkTrue("a LOCKED row: read-only, the value and the label grey",
	          regionHas(120, lr.y + 13, 112, 14, C_GRID) && !regionHas(120, lr.y + 13, 112, 14, C_TEXT) &&
	          !regionHas(UI_MARGIN, lr.y + 21, 60, 7, C_DIM));

	tapId(UI_FIX(KstCtl::Revert));
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("the write page", tapUntilGone(UI_FIX(KstCtl::Write)) && uiKstView() == KstView::Write);
	kstFrames(1);
	checkTrue("the change at y 62: the old value and the arrow dim, the new value cyan",
	          regionHas(120, UI_BODY_Y, 112, 7, C_DIM) && regionHas(120, UI_BODY_Y, 112, 7, C_CYAN) &&
	          !regionHas(0, UI_BODY_Y, GFX_W, 17, C_LIME));
	leaveAll();

	// A write left in doubt: the chip reads IN DOUBT on red, a failure.
	freshKst();
	checkTrue("to the field list again", toList());
	sim_add_fault(fakeKstSim(), SIM_WRITE, 0x03, 0, 1, SIM_F_WRITE_ALSO, 0x13);
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	tapUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("in doubt", untilCtl(UI_FIX(KstCtl::WriteOk), 400) && job().locked);
	tapId(UI_FIX(KstCtl::WriteOk));
	kstFrames(1);
	const int dw = gfxTextW("IN DOUBT", 1) + 12;
	checkTrue("IN DOUBT on red, square",
	          fakeTextIs(GFX_W - UI_MARGIN - dw + 6, 44, "IN DOUBT", C_ONACCENT, C_RED, 1) &&
	          pixel(GFX_W - UI_MARGIN - dw, 42) == C_RED);
	leaveAll();
}

void runKstUiTests() {
	testRecordWarningOnSearch();
	testOpenAndSearch();
	testEditAndWrite();
	testFiles();
	testLockedAndRestore();
	testLeaveAndBack();
	testLossAndCard();
	testReadOnlyAndRecord();
	testRefusalsAndFailures();
	testLoadRefusals();
	testManyChangesAndLeave();
	testSecondServoAndLateCard();
	testAbortQueueAndLapse();
	testSaveKeepsSelection();
	testSlowCard();
	testRefusedOps();
	testRecordKeptAtLeave();
	testFlushWait();
	testFlushWaitEndsWithThePage();
	testSwitchedSupply();
	testSwitchedTimeout();
	testSupplyCurrent();
	testCardPhase();
	testBoardLow();
	testColourRoles();
	fakeKstLineReset();
	noCard();
	fakeFatReset();
}
