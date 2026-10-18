/**
 * @file test_kst_job.cpp
 * @brief The KST programmer's core1 job, from the job table, against
 *        rcbench's simulated servo on the virtual clock.
 *
 * The job runs as it ships: core1JobStart() after the fake DShot pump is
 * down, and core1JobPoll() once a millisecond, which the test calls itself in
 * core1's place. The line is the fake of fake_kst_line.h.
 *
 * kst_job.cpp is included rather than linked, with kst_session_write(),
 * kst_plan_edit() and kst_plan_restore() renamed to counters before the
 * session's headers are read: a call to one of them anywhere in the job
 * counts, and the counts stay at 0. The Makefile therefore must not compile
 * kst_job.cpp separately.
 */

#define kst_session_write kstForbiddenSessionWrite
#define kst_plan_edit     kstForbiddenPlanEdit
#define kst_plan_restore  kstForbiddenPlanRestore
#include "../src/kst_job.cpp"   // NOLINT -- see the note above
#undef kst_session_write
#undef kst_plan_edit
#undef kst_plan_restore

#include "check.h"
#include "fakes.h"
#include "fake_kst_line.h"
#include "esc_task.h"

static int g_forbidden = 0;

extern "C" kst_ses_t kstForbiddenSessionWrite(kst_session_t *, const kst_plan_t *, bool) {
	g_forbidden++;
	return KST_SES_ERR_ARG;
}
extern "C" kst_plan_result_t kstForbiddenPlanEdit(const kst_image_t *, const kst_image_t *,
                                                  kst_field_set_t, kst_plan_t *, kst_rules_t *) {
	g_forbidden++;
	return KST_PLAN_ERR_ARG;
}
extern "C" kst_plan_result_t kstForbiddenPlanRestore(const kst_image_t *, const kst_image_t *,
                                                     kst_plan_t *) {
	g_forbidden++;
	return KST_PLAN_ERR_ARG;
}

/** @brief The SIG GPIO the job is started on. */
static const uint8_t SIG = 29;

/** @brief Whether the heartbeat is fed on each pass. */
static bool g_alive = true;

/** @brief @p ms passes of core1, one a millisecond. */
static void passes(uint32_t ms) {
	for (uint32_t i = 0; i < ms; i++) {
		fakeAdvance(1);
		if (g_alive) escHeartbeat();
		core1JobPoll();
	}
}

static KstStatus status() {
	KstStatus st;
	kstJobStatus(&st);
	return st;
}

/** @brief Passes until the job is in @p want, at most @p maxMs. */
static bool until(KstJobState want, uint32_t maxMs) {
	for (uint32_t i = 0; i < maxMs; i++) {
		if (status().job == want) return true;
		passes(1);
	}
	return status().job == want;
}

static uint16_t send(KstOp op) {
	KstCmd c;
	memset(&c, 0, sizeof(c));
	c.op = op;
	return kstJobCommand(&c);
}

/** @brief DShot down: the fake pump drained and suspended. */
static void suspend() {
	escTaskSuspend();
	for (int i = 0; i < 4 && !escTaskSuspended(); i++) {
		fakeAdvance(100);
		fakePumpPoll();
	}
}

/** @brief DShot down, a fresh line and servo, the job started and bound. */
static void start(bool powered) {
	g_alive = true;
	escHeartbeat();
	suspend();
	fakeKstLineReset();
	fakeKstSim()->powered = powered;
	core1JobStart(Core1Job::Kst, SIG, 0xFF);
	core1JobPoll();
}

/** @brief Release the job and give DShot the pin back. */
static void finish() {
	g_alive = true;
	core1JobStop();
	for (int i = 0; i < 1000 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	escTaskResume();
	fakePumpPoll();
}

/** @brief Start, search, find, read: InMode with a backup. */
static void startInMode() {
	start(true);
	send(KstOp::Search);
	until(KstJobState::InMode, 1000);
	send(KstOp::ReadAll);
	passes(1);
	until(KstJobState::InMode, 2000);
}

static void testBind() {
	section("KST job: bind and the command block");
	uint16_t before = send(KstOp::Search);
	start(true);
	KstStatus st = status();
	checkTrue("bound: running", core1JobPhase() == Core1JobPhase::Running &&
	                            core1JobRunning() == Core1Job::Kst);
	checkTrue("the line is open on SIG", kstLineIsOpen() && fakeKstLinePin() == SIG &&
	                                    fakeKstLineOpens() == 1);
	checkTrue("Opening, the line low, nothing sent",
	          st.job == KstJobState::Opening && fakeKstSim()->n_frames == 0);
	checkInt("a command sent before the bind is not taken", st.ackSeq, before);
	passes(200);
	checkTrue("nor later", status().job == KstJobState::Opening &&
	                       fakeKstSim()->n_frames == 0);
	finish();

	struct { uint8_t err; uint8_t want; const char *what; } refusals[] = {
		{ KST_LINE_ERR_PIO,   CORE1_JOB_PIO,   "no PIO room: CORE1_JOB_PIO" },
		{ KST_LINE_ERR_DMA,   CORE1_JOB_DMA,   "no DMA channels: CORE1_JOB_DMA" },
		{ KST_LINE_ERR_CLOCK, CORE1_JOB_CLOCK, "clk_sys off the half-cell: CORE1_JOB_CLOCK" },
		{ KST_LINE_ERR_ARG,   CORE1_JOB_PIO,   "a pin no PIO reaches: CORE1_JOB_PIO" },
	};
	suspend();
	for (auto &r : refusals) {
		fakeKstLineReset();
		fakeKstLineRefuse(r.err);
		core1JobStart(Core1Job::Kst, SIG, 0xFF);
		core1JobPoll();
		checkTrue(r.what, core1JobError() == r.want &&
		                  core1JobPhase() == Core1JobPhase::Idle && !kstLineIsOpen());
		checkInt("the status names the line's error", status().lineError, r.err);
	}
	fakeKstLineRefuse(0);
	escTaskResume();
	fakePumpPoll();
}

static void testSearch() {
	section("KST job: the search");
	start(false);
	uint16_t seq = send(KstOp::Search);
	passes(1);
	KstStatus st = status();
	checkTrue("Search: taken, searching", st.ackSeq == seq &&
	                                     st.job == KstJobState::Searching &&
	                                     st.refused == KstRefusal::None);
	passes(500);
	st = status();
	checkTrue("no servo: attempts go on", st.seek == KstSeek::Searching && st.attempts >= 4);
	checkInt("searched for 500 ms, from the pass that took it", st.searchMs, 500);
	checkInt("frames counted: read, sync, read an attempt",
	         (long)st.frames, (long)fakeKstSim()->n_frames);
	fakeKstSim()->powered = true;
	checkTrue("powered: in programming mode", until(KstJobState::InMode, 300));
	st = status();
	checkTrue("found, in the mode, no image yet",
	          st.seek == KstSeek::Found && st.inMode && !st.hasImage);
	checkInt("the search's result", st.result, KST_SES_OK);

	seq = send(KstOp::Search);
	passes(1);
	checkTrue("a second search in the mode: refused",
	          status().ackSeq == seq && status().refused == KstRefusal::State);
	send(KstOp::StopSearch);
	passes(1);
	checkTrue("STOP SEARCH in the mode: refused", status().refused == KstRefusal::State);
	finish();

	// STOP SEARCH, then RETRY.
	start(false);
	send(KstOp::Search);
	passes(250);
	send(KstOp::StopSearch);
	checkTrue("STOP SEARCH: the search ends", until(KstJobState::SearchEnded, 20));
	checkTrue("as Stopped", status().seek == KstSeek::Stopped);
	send(KstOp::Search);
	passes(1);
	checkTrue("RETRY: searching again, from attempt 1",
	          status().job == KstJobState::Searching && status().attempts == 1);
	send(KstOp::Abort);
	checkTrue("Abort in a search stops it too",
	          until(KstJobState::SearchEnded, 20) && status().seek == KstSeek::Stopped);
	finish();

	// The timeout.
	start(false);
	send(KstOp::Search);
	checkTrue("no servo for 30 s: the search ends",
	          until(KstJobState::SearchEnded, KST_SEEK_TIMEOUT_MS + 200));
	KstStatus end = status();
	checkTrue("timed out, at 30 s", end.seek == KstSeek::TimedOut &&
	                                end.searchMs >= KST_SEEK_TIMEOUT_MS &&
	                                end.searchMs < KST_SEEK_TIMEOUT_MS + 120);
	checkInt("the result is no servo", end.result, KST_SES_ERR_NO_SERVO);
	checkTrue("not stalled", !end.stalled);
	finish();
}

static void testHeartbeat() {
	section("KST job: the heartbeat");
	start(false);
	send(KstOp::Search);
	passes(150);
	g_alive = false;
	while (core1UiAlive()) passes(1);
	uint32_t attempts = status().attempts;
	checkTrue("a lapse: the search ends after its attempt",
	          until(KstJobState::SearchEnded, 1000));
	KstStatus st = status();
	checkTrue("NoUi, stalled", st.seek == KstSeek::NoUi && st.stalled);
	checkTrue("no attempt after the one that ran", st.attempts == attempts);
	unsigned frames = fakeKstSim()->n_frames;
	passes(500);
	checkInt("nothing after", (long)fakeKstSim()->n_frames, (long)frames);
	send(KstOp::Search);
	passes(1);
	checkTrue("RETRY with the heartbeat lapsed: refused",
	          status().refused == KstRefusal::NoUi && status().job == KstJobState::SearchEnded);
	g_alive = true;
	send(KstOp::Search);
	passes(1);
	checkTrue("back: RETRY searches, the stall cleared",
	          status().job == KstJobState::Searching && !status().stalled);
	fakeKstSim()->powered = true;
	until(KstJobState::InMode, 300);

	g_alive = false;
	passes(300);
	send(KstOp::ReadAll);
	passes(1);
	checkTrue("an operation with the heartbeat lapsed: refused",
	          status().refused == KstRefusal::NoUi && status().job == KstJobState::InMode);
	g_alive = true;
	send(KstOp::ReadAll);
	passes(1);
	g_alive = false;
	checkTrue("a read that started runs to its end without the heartbeat",
	          status().job == KstJobState::Busy && until(KstJobState::InMode, 2000));
	checkTrue("and reads the servo", status().result == KST_SES_OK && status().hasImage);
	finish();
}

static void testOperations() {
	section("KST job: operations in the mode");
	start(true);
	send(KstOp::ReadAll);
	passes(1);
	checkTrue("ReadAll before the servo is found: refused",
	          status().refused == KstRefusal::State && status().job == KstJobState::Opening);
	send(KstOp::Search);
	until(KstJobState::InMode, 1000);

	KstCmd edit;
	memset(&edit, 0, sizeof(edit));
	edit.op = KstOp::Edit;
	edit.target = sim_bench_image();
	kst_field_edit(&edit.target, KST_F_BOOST, 19);
	kstJobCommand(&edit);
	passes(1);
	checkTrue("Edit before a read: the session refuses it, no backup",
	          status().job == KstJobState::InMode && status().result == KST_SES_ERR_NO_BACKUP);

	unsigned before = fakeKstSim()->n_frames;
	send(KstOp::ReadAll);
	passes(1);
	checkTrue("ReadAll: busy", status().job == KstJobState::Busy);
	checkTrue("then in the mode", until(KstJobState::InMode, 2000));
	KstStatus st = status();
	checkTrue("read: image and backup", st.result == KST_SES_OK && st.hasImage && st.hasBackup);
	checkTrue("the image is the servo's",
	          memcmp(st.image.r, fakeKstSim()->reg, KST_REG_COUNT) == 0 &&
	          memcmp(st.backup.r, fakeKstSim()->reg, KST_REG_COUNT) == 0);
	checkTrue("it passes the fingerprint", st.fingerprintOk);
	checkInt("frames: 3 passes of 32 reads",
	         (long)st.frames, (long)(fakeKstSim()->n_frames - before));
	checkInt("which is 96", st.frames, 96);

	KstCmd verify;
	memset(&verify, 0, sizeof(verify));
	verify.op = KstOp::Verify;
	verify.target = st.image;
	kstJobCommand(&verify);
	passes(1);
	until(KstJobState::InMode, 2000);
	checkInt("Verify against the image: OK", status().result, KST_SES_OK);
	verify.target.r[3] ^= 0x01;
	kstJobCommand(&verify);
	passes(1);
	until(KstJobState::InMode, 2000);
	checkTrue("against another: VERIFY, the register named",
	          status().result == KST_SES_ERR_VERIFY && status().diffRegs == (1u << 3));

	kstJobCommand(&edit);
	passes(1);
	checkTrue("Edit: busy", status().job == KstJobState::Busy);
	checkTrue("then in the mode", until(KstJobState::InMode, 5000));
	st = status();
	checkInt("written and read back", st.result, KST_SES_OK);
	checkTrue("the servo holds the target",
	          memcmp(fakeKstSim()->reg, edit.target.r, KST_REG_COUNT) == 0);
	checkTrue("the session names what it wrote", st.wroteRegs != 0 && st.planResult == KST_PLAN_OK);

	KstCmd restore;
	memset(&restore, 0, sizeof(restore));
	restore.op = KstOp::Restore;
	kstJobCommand(&restore);
	passes(1);
	checkTrue("Restore: then in the mode", until(KstJobState::InMode, 5000));
	checkInt("restored", status().result, KST_SES_OK);
	checkTrue("the servo holds the backup",
	          memcmp(fakeKstSim()->reg, status().backup.r, KST_REG_COUNT) == 0);

	send(KstOp::ReadAll);
	passes(5);
	send(KstOp::Abort);
	checkTrue("Abort of a read: back in the mode", until(KstJobState::InMode, 100));
	checkInt("aborted", status().result, KST_SES_ERR_ABORTED);
	send(KstOp::Abort);
	passes(1);
	checkTrue("Abort with nothing running: refused", status().refused == KstRefusal::State);
	checkInt("never kst_session_write(), kst_plan_edit() or kst_plan_restore()", g_forbidden, 0);
	finish();
}

static void testLoss() {
	section("KST job: a lost servo");
	startInMode();
	checkTrue("in the mode with a backup", status().job == KstJobState::InMode &&
	                                       status().hasBackup);
	fakeKstSim()->powered = false;
	fakeKstSim()->in_mode = false;
	send(KstOp::ReadAll);
	passes(1);
	checkTrue("the servo stops answering: the search begins again",
	          until(KstJobState::Searching, 2000));
	KstStatus st = status();
	checkTrue("out of the mode, the backup dropped", !st.inMode && !st.hasBackup);
	checkInt("the read's result is kept", st.result, KST_SES_ERR_NO_SERVO);
	checkTrue("the new search from attempt 1", st.attempts == 1 && st.seek == KstSeek::Searching);
	fakeKstSim()->powered = true;
	checkTrue("powered again: found", until(KstJobState::InMode, 400));
	finish();

	startInMode();
	fakeKstSim()->powered = false;
	fakeKstSim()->in_mode = false;
	send(KstOp::ReadAll);
	passes(1);
	g_alive = false;
	checkTrue("lost with the heartbeat lapsed: no search",
	          until(KstJobState::SearchEnded, 2000));
	st = status();
	checkTrue("NoUi, stalled, power cycled",
	          st.seek == KstSeek::NoUi && st.stalled && !st.inMode && !st.hasBackup);
	unsigned frames = fakeKstSim()->n_frames;
	passes(500);
	checkInt("and nothing sent", (long)fakeKstSim()->n_frames, (long)frames);
	finish();
}

static void testLeave() {
	section("KST job: leave and release");
	startInMode();
	sim_t *sim = fakeKstSim();
	send(KstOp::Leave);
	passes(1);
	checkTrue("Leave: leaving, the line still low", status().job == KstJobState::Leaving &&
	                                               kstLineIsOpen());
	passes(KST_LEAVE_LOW_MS - 1);
	checkTrue("99 ms: still open", kstLineIsOpen());
	passes(1);
	checkTrue("100 ms: closed", !kstLineIsOpen() && status().job == KstJobState::Closed &&
	                            fakeKstLineCloses() == 1);
	checkTrue("100 ms low after the last window",
	          fakeKstLineClosedUs() - sim->end_us >= KST_LEAVE_LOW_MS * 1000u);
	checkTrue("the servo is still in the mode: its supply was not off", status().inMode);
	send(KstOp::Search);
	passes(1);
	checkTrue("a command after the close: refused", status().refused == KstRefusal::State);
	core1JobStop();
	core1JobPoll();
	checkTrue("the release of a closed line: idle at once",
	          core1JobPhase() == Core1JobPhase::Idle && fakeKstLineCloses() == 1);
	escTaskResume();
	fakePumpPoll();

	// Leave inside a frame of the search: the frame finishes, then 100 ms.
	start(false);
	send(KstOp::Search);
	passes(150);
	while (!sim->busy) passes(1);
	unsigned frames = sim->n_frames;
	send(KstOp::Leave);
	passes(1);
	checkTrue("Leave mid-frame: leaving", status().job == KstJobState::Leaving);
	checkTrue("closed in time", until(KstJobState::Closed, 200));
	checkTrue("the frame finished, no other went out", !sim->busy && sim->n_frames == frames);
	checkTrue("and 100 ms low before the close",
	          fakeKstLineClosedUs() - sim->end_us >= KST_LEAVE_LOW_MS * 1000u);
	checkInt("the search ended as stopped", (int)status().seek, (int)KstSeek::Stopped);
	finish();

	// BACK during a search: the release leaves the same way.
	start(false);
	send(KstOp::Search);
	passes(150);
	while (!sim->busy) passes(1);
	frames = sim->n_frames;
	core1JobStop();
	passes(1);
	checkTrue("the release mid-frame: stopping, the line open",
	          core1JobPhase() == Core1JobPhase::Stopping && kstLineIsOpen());
	for (int i = 0; i < 300 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	checkTrue("then idle, the line closed", core1JobPhase() == Core1JobPhase::Idle &&
	                                        !kstLineIsOpen());
	checkTrue("the frame finished, none other", sim->n_frames == frames);
	checkTrue("100 ms low before the close",
	          fakeKstLineClosedUs() - sim->end_us >= KST_LEAVE_LOW_MS * 1000u);
	escTaskResume();
	fakePumpPoll();

	// The release from a read: aborted, finished, 100 ms.
	startInMode();
	send(KstOp::ReadAll);
	passes(5);
	core1JobStop();
	for (int i = 0; i < 300 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	checkTrue("the release during a read: idle, the line closed",
	          core1JobPhase() == Core1JobPhase::Idle && !kstLineIsOpen());
	checkInt("the read ended aborted", status().result, KST_SES_ERR_ABORTED);
	escTaskResume();
	fakePumpPoll();
}

static void testCommands() {
	section("KST job: the command block");
	start(false);
	uint16_t stop = send(KstOp::StopSearch);
	checkInt("a command after an untaken STOP SEARCH is dropped", send(KstOp::Search), stop);
	checkInt("an Abort too: a stop does not replace a stop", send(KstOp::Abort), stop);
	uint16_t leave = send(KstOp::Leave);
	checkInt("a Leave replaces an untaken stop", leave, (uint16_t)(stop + 1));
	checkInt("and an untaken Leave is not replaced", send(KstOp::Abort), leave);
	checkInt("not by a Leave either", send(KstOp::Leave), leave);
	passes(1);
	checkTrue("the Leave is taken", status().ackSeq == leave &&
	                                status().job == KstJobState::Leaving);
	uint16_t abort = send(KstOp::Abort);
	checkInt("an untaken Abort is kept", send(KstOp::Search), abort);
	passes(1);
	uint16_t a = send(KstOp::Search);
	checkInt("a taken command is replaced by the next", a, (uint16_t)(abort + 1));
	uint16_t b = send(KstOp::ReadAll);
	checkInt("an untaken command that is no stop is replaced", b, (uint16_t)(a + 1));
	finish();
}

/** @brief Bring a fresh job to @p want. */
static void reach(KstJobState want) {
	bool found = want == KstJobState::InMode || want == KstJobState::Busy;
	start(found);
	switch (want) {
		case KstJobState::Opening:
			break;
		case KstJobState::Searching:
			send(KstOp::Search);
			passes(10);
			break;
		case KstJobState::SearchEnded:
			send(KstOp::Search);
			passes(10);
			send(KstOp::StopSearch);
			until(KstJobState::SearchEnded, 20);
			break;
		case KstJobState::InMode:
		case KstJobState::Busy:
			send(KstOp::Search);
			until(KstJobState::InMode, 1000);
			if (want == KstJobState::Busy) {
				send(KstOp::ReadAll);
				passes(1);
			}
			break;
		case KstJobState::Leaving:
		case KstJobState::Closed:
			send(KstOp::Leave);
			passes(want == KstJobState::Leaving ? 1 : KST_LEAVE_LOW_MS + 1);
			break;
	}
}

static void testEachOpInEachState() {
	section("KST job: each command in each state");
	static const KstJobState STATES[] = {
		KstJobState::Opening, KstJobState::Searching, KstJobState::SearchEnded,
		KstJobState::InMode, KstJobState::Busy, KstJobState::Leaving, KstJobState::Closed,
	};
	static const char *const STATE_NAMES[] = {
		"Opening", "Searching", "SearchEnded", "InMode", "Busy", "Leaving", "Closed",
	};
	static const KstOp OPS[] = {
		KstOp::Search, KstOp::StopSearch, KstOp::ReadAll, KstOp::Verify,
		KstOp::Edit, KstOp::Restore, KstOp::Abort, KstOp::Leave,
	};
	static const char *const OP_NAMES[] = {
		"Search", "StopSearch", "ReadAll", "Verify", "Edit", "Restore", "Abort", "Leave",
	};
	// Taken, by op and state: Opening, Searching, SearchEnded, InMode, Busy,
	// Leaving, Closed.
	static const bool TAKEN[8][7] = {
		{ 1, 0, 1, 0, 0, 0, 0 },  // Search
		{ 0, 1, 0, 0, 0, 0, 0 },  // StopSearch
		{ 0, 0, 0, 1, 0, 0, 0 },  // ReadAll
		{ 0, 0, 0, 1, 0, 0, 0 },  // Verify
		{ 0, 0, 0, 1, 0, 0, 0 },  // Edit
		{ 0, 0, 0, 1, 0, 0, 0 },  // Restore
		{ 0, 1, 0, 0, 1, 0, 0 },  // Abort
		{ 1, 1, 1, 1, 1, 0, 0 },  // Leave
	};
	for (int si = 0; si < 7; si++) {
		for (int oi = 0; oi < 8; oi++) {
			reach(STATES[si]);
			bool there = status().job == STATES[si];
			uint16_t seq = send(OPS[oi]);
			passes(1);
			KstStatus st = status();
			char what[80];
			snprintf(what, sizeof(what), "%s in %s: %s", OP_NAMES[oi], STATE_NAMES[si],
			         TAKEN[oi][si] ? "taken" : "refused");
			checkTrue(what, there && st.ackSeq == seq &&
			                (st.refused == KstRefusal::None) == TAKEN[oi][si]);
			finish();
		}
	}
}

void runKstJobTests() {
	testBind();
	testSearch();
	testHeartbeat();
	testOperations();
	testLoss();
	testLeave();
	testCommands();
	testEachOpInEachState();
}
