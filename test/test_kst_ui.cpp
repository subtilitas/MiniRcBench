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
#include "core1_job.h"
#include "esc_task.h"
#include "kst_job.h"
#include "kst_line_port.h"
#include "kst_line.h"
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
	tapId(UI_FIX(KstCtl::EditPlus));
	checkTrue("a step: WRITE and REVERT act", ctlEnabled(UI_FIX(KstCtl::Write)) &&
	                                         ctlEnabled(UI_FIX(KstCtl::Revert)));
	tapId(UI_FIX(KstCtl::Revert));
	checkTrue("REVERT: nothing to write", !ctlEnabled(UI_FIX(KstCtl::Write)));
	tapId(UI_FIX(KstCtl::EditMinus));
	checkTrue("a step down: WRITE acts", ctlEnabled(UI_FIX(KstCtl::Write)));
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

	unsigned writes = sim->n_writes;
	ctlCentre(UI_FIX(KstCtl::WriteHold), &x, &y);
	fakePress(x, y);
	kstFrame();
	for (int i = 0; i < 80 && ctlPresent(UI_FIX(KstCtl::WriteHold)); i++) { fakeHold(x, y); kstFrame(); }
	fakeRelease();
	checkTrue("the hold starts the write", !ctlPresent(UI_FIX(KstCtl::WriteHold)));
	checkTrue("the backup is on the card first", fakeFatExists("KST/KST001.TXT"));
	checkTrue("and the record", fakeFatExists("KST/KST.TXT"));
	checkInt("before the first write frame", (long)sim->n_writes, (long)writes);
	checkTrue("the result comes", untilCtl(UI_FIX(KstCtl::WriteOk), 400));
	checkTrue("verified", job().result == KST_SES_OK);
	checkTrue("the servo holds the new dead band", simField(KST_F_DEAD_BAND) != deadBand);
	checkTrue("the record is removed after the verify", !fakeFatExists("KST/KST.TXT"));
	checkTrue("the backup stays", fakeFatExists("KST/KST001.TXT"));
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
	holdUntilGone(UI_FIX(KstCtl::Write));
	holdUntilGone(UI_FIX(KstCtl::WriteHold));
	untilCtl(UI_FIX(KstCtl::WriteOk), 400);
	checkInt("the servo holds 98 %, raw 250, and no more", simField(KST_F_DUTY), KST_DUTY_RAW_MAX);
	checkTrue("a second write writes no new record", !fakeFatExists("KST/KST.TXT") &&
	                                                !fakeFatExists("KST/KST002.TXT"));
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
	fakeKstLineReset();
	noCard();
	fakeFatReset();
}
