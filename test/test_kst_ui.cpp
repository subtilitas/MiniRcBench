/**
 * @file test_kst_ui.cpp
 * @brief KST SERVO through the real UI: MENU, the claim, the search, the
 *        field list, the write, FILES, the leave and the unplug page.
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
#include "ui.h"
#include "ui_kst.h"
#include "ui_menu.h"
#include "ui_screen.h"
#include "usb_msc.h"
#include "plat.h"

#include <stdio.h>
#include <string.h>

/** @brief One UI frame: 25 passes of core1, one of the pump, then the UI. */
static void kstFrame() {
	for (int i = 0; i < 25; i++) {
		fakeAdvance(1);
		core1JobPoll();
	}
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
	escTaskConfigure(29, 600, false, 28);
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

/** @brief From any view: leave, restart DShot, back to the tester. */
static void leaveAll() {
	for (int i = 0; i < 8 && uiCurrentScreen() == UiScreenId::Kst; i++) {
		KstView v = uiKstView();
		if (v == KstView::Unplug) {
			if (untilCtl(UI_FIX(KstCtl::Restart), 200)) tapId(UI_FIX(KstCtl::Restart));
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
	checkTrue("on SIG", fakeKstLinePin() == 29);
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

	// HOLD TO WRITE: not after 30 frames, 750 ms; not on a slide off.
	int x, y;
	ctlCentre(UI_FIX(KstCtl::Write), &x, &y);
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 30; i++) { fakeHold(x, y); kstFrame(); }
	checkTrue("750 ms of hold: still the list", uiKstView() == KstView::List);
	for (int i = 0; i < 20; i++) { fakeHold(x, y - 60); kstFrame(); }
	fakeRelease();
	kstFrame();
	checkTrue("a slide off: no write page", uiKstView() == KstView::List);
	checkTrue("a hold of 1000 ms: the write page", holdUntilGone(UI_FIX(KstCtl::Write)) &&
	                                              uiKstView() == KstView::Write);
	checkControlTable("KST write page");
	kstFrames(1);
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
	checkTrue("the result page still lists the change", fakeCountColour(C_LIME) > 0);
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	checkTrue("HOLD TO RESTORE acts", ctlEnabled(UI_FIX(KstCtl::Restore)));
	checkTrue("the hold runs the restore", holdUntilGone(UI_FIX(KstCtl::Restore)) &&
	                                      uiKstView() == KstView::Write);
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

static void testLeaveAndBack() {
	section("KST screen: leave, the unplug page, BACK in every view");
	freshKst(false);
	openKst();
	untilCtl(UI_FIX(KstCtl::StopSearch), 40);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK in a search that found nothing: the unplug page",
	          uiKstView() == KstView::Unplug);
	checkTrue("RESTART DSHOT once the pin is let go", untilCtl(UI_FIX(KstCtl::Restart), 200));
	checkControlTable("KST unplug");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK on the unplug page: the search again", uiKstView() == KstView::Main &&
	                                                       untilCtl(UI_FIX(KstCtl::StopSearch), 200));
	fakeKstSim()->powered = true;
	checkTrue("found and read", untilView(KstView::List, 400));
	tapId(UI_FIX(KstCtl::Files));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK in FILES: the list", uiKstView() == KstView::List);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK in the list: the leave page", uiKstView() == KstView::Leave);
	checkControlTable("KST leave");
	kstFrames(1);
	fakeDumpFrame("shot_kst_leave.ppm");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK on the leave page: the list", uiKstView() == KstView::List);

	// BACK during a write is queued: the leave page follows the result.
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	holdUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK during the write: the page stays", uiKstView() == KstView::Write);
	checkTrue("the leave page after the result", untilView(KstView::Leave, 400));
	checkTrue("the write ended verified", job().result == KST_SES_OK);
	tapId(UI_FIX(KstCtl::Leave));
	checkTrue("LEAVE: the unplug page", uiKstView() == KstView::Unplug);
	checkTrue("after 100 ms of low line the pin is let go", untilCtl(UI_FIX(KstCtl::Restart), 200) &&
	                                                        !kstLineIsOpen());
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the servo lost: the search", untilView(KstView::Main, 400) &&
	                                       untilCtl(UI_FIX(KstCtl::StopSearch), 200));
	kstFrames(2);
	checkTrue("no interrupted write named: nothing amber on the search",
	          fakeCountColour(C_AMBER) == 0);
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
	checkControlTable("KST, an ESC answers");
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK before a claim: MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	kstFrames(2);

	freshKst(false);
	fakeKstLineRefuse(KST_LINE_ERR_DMA);
	openKst();
	checkTrue("a bind that fails: RETRY", untilCtl(UI_FIX(KstCtl::Retry), 80) &&
	                                      pinClaimJobError() == CORE1_JOB_DMA);
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
	checkTrue("a hold opens the write page", holdUntilGone(UI_FIX(KstCtl::Write)) &&
	                                        uiKstView() == KstView::Write);
	kstFrames(1);
	fakeDumpFrame("shot_kst_write_many.ppm");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	checkTrue("the leave page with five changes pending", uiKstView() == KstView::Leave &&
	                                                     ctlEnabled(UI_FIX(KstCtl::Leave)));
	kstFrames(1);
	fakeDumpFrame("shot_kst_leave_pending.ppm");
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Revert));

	// A write in doubt: the leave page says so, and LEAVE still leaves.
	sim_add_fault(fakeKstSim(), SIM_WRITE, 0x03, 0, 1, SIM_F_WRITE_ALSO, 0x13);
	selectField(KST_F_BOOST);
	tapId(UI_FIX(KstCtl::EditMinus));
	holdUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
	tapId(UI_FIX(KstCtl::WriteOk));
	tapId(UI_FIX(UI_BACK));
	checkTrue("locked: the leave page", uiKstView() == KstView::Leave && job().locked);
	tapId(UI_FIX(KstCtl::Leave));
	checkTrue("LEAVE leaves a locked session", uiKstView() == KstView::Unplug);
	checkTrue("the record stays on the card", fakeFatExists("KST/KST.TXT"));
	leaveAll();
}

/** @brief Edit DEAD BAND once and run the write to its OK. */
static void writeDeadBand() {
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Restore));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
	kstFrames(1);
	checkTrue("still no card: the backup in memory",
	          lineIs(236, "NO CARD: BACKUP IN MEMORY", C_DIM));
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
	section("KST screen: BACK during the read, a queued leave once, a loss with the heartbeat lapsed");
	freshKst(false);
	openKst();
	untilCtl(UI_FIX(KstCtl::StopSearch), 40);
	fakeKstSim()->powered = true;
	for (int i = 0; i < 400 && job().job != KstJobState::Busy; i++) kstFrame();
	checkTrue("found: the read runs", job().job == KstJobState::Busy);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK during the read: the leave page", uiKstView() == KstView::Leave);
	kstFrames(20);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK there: READ AGAIN", untilCtl(UI_FIX(KstCtl::ReadAgain), 100) &&
	                                    ctlEnabled(UI_FIX(KstCtl::ReadAgain)));
	tapId(UI_FIX(KstCtl::ReadAgain));
	checkTrue("it reads", untilView(KstView::List, 400));

	// A leave queued during one write does not follow the next.
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	holdUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	tapId(UI_FIX(UI_BACK));
	checkTrue("the queued leave follows the result", untilView(KstView::Leave, 400));
	tapId(UI_FIX(UI_BACK));
	writeDeadBand();
	checkTrue("the next write shows its result", uiKstView() == KstView::Write &&
	                                            ctlPresent(UI_FIX(KstCtl::WriteOk)));
	tapId(UI_FIX(KstCtl::WriteOk));

	// The servo goes during a write and the screen stalls: core1 alone.
	sim_t *sim = fakeKstSim();
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	checkTrue("the page names no backup file", lineIs(236, "NO BACKUP ON THE CARD: IN MEMORY", C_DIM));
	tapId(UI_FIX(KstCtl::WriteOk));
	leaveAll();
}

/** @brief From the list: BACK, LEAVE, then BACK on the unplug page, the next session's search. */
static void unplugAndBack() {
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Leave));
	untilCtl(UI_FIX(KstCtl::Restart), 200);
	tapId(UI_FIX(UI_BACK));
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

	// READ AGAIN, BACK during it, BACK on the leave page: the read stopped.
	tapId(UI_FIX(KstCtl::ReadAgain));
	for (int i = 0; i < 40 && job().job != KstJobState::Busy; i++) kstFrame();
	checkTrue("the read runs", job().job == KstJobState::Busy);
	tapId(UI_FIX(UI_BACK));
	kstFrames(20);
	tapId(UI_FIX(UI_BACK));
	kstFrames(2);
	checkTrue("BACK twice: the read aborted, not the stall of the read before",
	          uiKstView() == KstView::Main &&
	          lineIs(80, "ABORTED. READ THE SERVO AGAIN.", C_AMBER));
	checkTrue("and no register of a read before", lineIs(94, "", C_AMBER) &&
	                                              lineIs(112, "", C_AMBER));
	tapId(UI_FIX(KstCtl::ReadAgain));
	checkTrue("READ AGAIN reads it", untilView(KstView::List, 400));

	// The write's hold fires, then core0 stalls 300 ms before core1 takes it.
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditPlus));
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	checkTrue("the next session names no interrupted write", fakeCountColour(C_AMBER) == 0);
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	checkTrue("the card took no file, the record moved",
	          ctlPresent(UI_FIX(KstCtl::WriteOk)) && !fakeFatExists("KST/KST.TXT") &&
	          fakeFatExists("BACKUP/KST001.TXT"));
	tapId(UI_FIX(KstCtl::WriteOk));
	unplugAndBack();
	checkTrue("the next session: no interrupted write named", fakeCountColour(C_AMBER) == 0);
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
	sim_t *sim = fakeKstSim();
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditMinus));
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = sim->n_writes;
	unsigned recovers = fakeCacheCalls().recovers;
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	fakeCacheSet(CardCache::Failed, 0);
	kstFrames(2);
	checkTrue("a failed flush: no write frame, the card mounted again",
	          sim->n_writes == writes && fakeCacheCalls().recovers == recovers + 1);
	checkTrue("and the page says why", ctlPresent(UI_FIX(KstCtl::WriteOk)) &&
	          lineIs(196, "THE CARD DID NOT TAKE THE BACKUP.", C_AMBER));
	tapId(UI_FIX(KstCtl::WriteOk));
	holdUntilGone(UI_FIX(KstCtl::Write));
	kstFrames(1);
	// The fake card kept the files the fake cache failed to flush: the
	// next number comes from them.
	char next[BACKUP_NAME_MAX], want[48];
	kstFilesNextBackup(next);
	snprintf(want, sizeof(want), "BACKUP FIRST: BACKUP/%s", next);
	checkTrue("the next write saves the backup again", lineIs(236, want, C_DIM));
	leaveAll();

	// BACK during the flush, then a SAVE AS whose flush fails: the batch held
	// the backup too, so the first write starts over.
	freshKst();
	checkTrue("a third time to the field list", toList());
	selectField(KST_F_DEAD_BAND);
	tapId(UI_FIX(KstCtl::EditMinus));
	holdUntilGone(UI_FIX(KstCtl::Write));
	fakeCacheSet(CardCache::Flushing, 12);
	writes = sim->n_writes;
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	kstFrames(2);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK during the flush: the list, no write frame",
	          uiKstView() == KstView::List && sim->n_writes == writes);
	tapId(UI_FIX(KstCtl::Files));
	fakeCacheFlushTo(CardCache::Failed);
	recovers = fakeCacheCalls().recovers;
	tapId(UI_FIX(KstCtl::SaveAs));
	kstFrames(1);
	checkTrue("SAVE AS: the flush fails, the card mounted again",
	          fakeCacheCalls().recovers == recovers + 1 && cardCacheState() == CardCache::Idle);
	fakeCacheFlushTo(CardCache::Idle);
	tapId(UI_FIX(UI_BACK));
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
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
	tapId(UI_FIX(KstCtl::Revert));

	// FILES after a batch failed in the background: mounted again first,
	// since the failed cache refuses every read.
	fakeCacheSet(CardCache::Failed, 0);
	recovers = fakeCacheCalls().recovers;
	tapId(UI_FIX(KstCtl::Files));
	checkTrue("FILES after a failed batch: recovered, the list read",
	          uiKstView() == KstView::Files && fakeCacheCalls().recovers == recovers + 1 &&
	          cardCacheState() == CardCache::Idle);
	tapId(UI_FIX(UI_BACK));

	// BACK on the unplug page keeps the claim: the cache is on again.
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(KstCtl::Leave));
	checkTrue("LEAVE: the unplug page, the cache off",
	          uiKstView() == KstView::Unplug && cardCacheState() == CardCache::Off);
	untilCtl(UI_FIX(KstCtl::Restart), 200);
	unsigned begins = fakeCacheCalls().begins;
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK on the unplug page: the cache on again",
	          uiKstView() == KstView::Main && fakeCacheCalls().begins == begins + 1 &&
	          cardCacheState() == CardCache::Idle);
	leaveAll();
	fakeCacheReset();
}

void runKstUiTests() {
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
	fakeKstLineReset();
	noCard();
	fakeFatReset();
}
