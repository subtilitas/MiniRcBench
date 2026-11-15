/**
 * @file test_escwire_job.cpp
 * @brief The ESC settings job on core1, from the job table, against
 *        rcbench's simulated ESC behind fake_escwire_port.h on the virtual
 *        clock.
 *
 * BLHeli_S and Bluejay are names of their owners' firmware, used here only
 * to say which ESCs this module works with.
 *
 * The job runs as it ships: core1JobStart() after the fake DShot pump is
 * down, and core1JobPoll() once a millisecond, which the test calls itself in
 * core1's place.
 */

#include "check.h"
#include "fakes.h"
#include "fake_escwire_port.h"
#include "core1_job.h"
#include "esc_task.h"
#include "escwire_job.h"
#include "ew_line.h"

#include <pico/time.h>
#include <stdio.h>
#include <string.h>

/** @brief The SIG GPIO the job is started on. */
static const uint8_t SIG = 0;

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

static EwJobStatus status() {
	static EwJobStatus st;
	escwireJobStatus(&st);
	return st;
}

/** @brief Passes until the job is in @p want, at most @p maxMs. */
static bool until(EwJobState want, uint32_t maxMs) {
	for (uint32_t i = 0; i < maxMs; i++) {
		if (status().job == want) return true;
		passes(1);
	}
	return status().job == want;
}

/** @brief Passes until the job has taken @p seq, at most 10. */
static bool taken(uint16_t seq) {
	for (int i = 0; i < 10 && status().ackSeq != seq; i++) passes(1);
	return status().ackSeq == seq;
}

/** @brief DShot down: the fake pump drained and suspended. */
static void suspend() {
	escTaskSuspend();
	for (int i = 0; i < 4 && !escTaskSuspended(); i++) {
		fakeAdvance(100);
		fakePumpPoll();
	}
}

/** @brief DShot down, a fresh EFM8 ESC, the job started and bound. */
static void start(uint8_t sig, bool bluejay) {
	g_alive = true;
	escHeartbeat();
	suspend();
	fakeEwReset(sig, bluejay);
	core1JobStart(Core1Job::EscWire, SIG, 0xFF);
	core1JobPoll();
}

/** @brief Release the job and give DShot the pin back. */
static void finish() {
	g_alive = true;
	core1JobStop();
	for (int i = 0; i < 3000 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	escTaskResume();
	fakePumpPoll();
}

/** @brief Start, enter: InMode. */
static bool startInMode(uint8_t sig, bool bluejay) {
	start(sig, bluejay);
	uint16_t seq = escwireJobCommand(EwJobOp::Enter);
	return taken(seq) && until(EwJobState::InMode, 5000) && status().result == EW_SES_OK;
}

/** @brief The index of the last transaction the simulated ESC saw. */
static int lastXfer() { return fakeEwSim()->starts - 1; }

static void testBind() {
	section("ESC settings job: the bind");
	start(0xB2, true);
	sim_t *e = fakeEwSim();
	checkTrue("bound: Opening, the line open on SIG", core1JobPhase() == Core1JobPhase::Running &&
	          status().job == EwJobState::Opening && fakeEwOpens() == 1 && fakeEwGpio() == SIG);
	checkTrue("driven high in the bind itself", e->n_line_log == 1 && e->line_log[0] == EW_LINE_HIGH &&
	          e->line == EW_LINE_HIGH);
	checkTrue("nothing sent", e->starts == 0);
	passes(3000);
	checkTrue("3 s later: still Opening, nothing sent, the line high",
	          status().job == EwJobState::Opening && e->starts == 0 && e->line == EW_LINE_HIGH);
	finish();
	checkTrue("the release closes the line", fakeEwCloses() == 1 && !fakeEwOpen() &&
	          status().job == EwJobState::Closed);

	static const uint8_t ERRS[] = { EW_LINE_ERR_PIO, EW_LINE_ERR_DMA, EW_LINE_ERR_CLOCK, EW_LINE_ERR_ARG };
	static const uint8_t JOB[] = { CORE1_JOB_PIO, CORE1_JOB_DMA, CORE1_JOB_CLOCK, CORE1_JOB_PIO };
	static const char *const NAMES[] = { "PIO", "DMA", "CLOCK", "ARG as PIO" };
	for (int i = 0; i < 4; i++) {
		g_alive = true;
		escHeartbeat();
		suspend();
		fakeEwReset(0xB2, true);
		fakeEwRefuse(ERRS[i]);
		core1JobStart(Core1Job::EscWire, SIG, 0xFF);
		core1JobPoll();
		char what[64];
		snprintf(what, sizeof(what), "a refused open: %s, nothing driven", NAMES[i]);
		checkTrue(what, core1JobError() == JOB[i] && core1JobPhase() == Core1JobPhase::Idle &&
		          fakeEwSim()->line_calls == 0 && status().lineError == ERRS[i]);
		escTaskResume();
		fakePumpPoll();
	}

	// A command sent before the bind is not taken by it.
	g_alive = true;
	escHeartbeat();
	suspend();
	fakeEwReset(0xB2, true);
	escwireJobCommand(EwJobOp::Enter);
	core1JobStart(Core1Job::EscWire, SIG, 0xFF);
	core1JobPoll();
	passes(5);
	checkTrue("a command held at the bind is not taken", status().job == EwJobState::Opening);
	finish();
}

static void testEnterReadLeave() {
	section("ESC settings job: Bluejay 208 entered, read and left");
	start(0xB2, true);
	sim_t *e = fakeEwSim();
	uint32_t boundUs = time_us_32();
	uint16_t seq = escwireJobCommand(EwJobOp::Enter);
	checkTrue("Enter taken: Busy", taken(seq) && status().job == EwJobState::Busy &&
	          status().result == EW_SES_BUSY && status().lastOp == EwJobOp::Enter);
	checkTrue("in mode", until(EwJobState::InMode, 3000) && status().result == EW_SES_OK &&
	          status().inMode && !status().haveRead);
	checkTrue("the first frame 500 ms or more after the line went high",
	          e->first_tx_at - boundUs >= 500000u);
	checkTrue("no frame on a parked line, none invalid", e->parked_tx == 0 && e->invalid == 0);
	EwJobStatus st = status();
	checkTrue("the contact: EFM8, E8B2", st.contact.cpu == EW_CPU_EFM8 && st.contact.signature == 0xE8B2);
	checkTrue("Bluejay 208, written layout, 255 bytes", st.id.fw == EW_FW_BJ && st.id.layout == 208 &&
	          st.id.writable && st.id.len == 255);
	checkTrue("the first 112 bytes as the ESC holds them",
	          memcmp(st.page, e->flash + e->base, EW_MAP_LEN_S) == 0);

	seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("Read taken: Busy", taken(seq) && status().job == EwJobState::Busy &&
	          status().lastOp == EwJobOp::Read);
	checkTrue("read", until(EwJobState::InMode, 3000) && status().result == EW_SES_OK && status().haveRead);
	st = status();
	checkTrue("the 255 bytes as the ESC holds them", memcmp(st.page, e->flash + e->base, 255) == 0);
	checkTrue("nothing erased or programmed", e->erases == 0 && e->programs == 0);

	int before = e->starts;
	uint32_t t0 = time_us_32();
	seq = escwireJobCommand(EwJobOp::Leave);
	checkTrue("Leave taken: Busy", taken(seq) && status().job == EwJobState::Busy);
	checkTrue("the leave frame: 00 01 and its CRC", e->starts == before + 1 && fakeEwSentLeave(lastXfer()));
	checkTrue("ends: Ended, left", until(EwJobState::Ended, 2000) && status().left &&
	          status().result == EW_SES_OK && !status().inMode);
	uint32_t took = time_us_32() - t0;
	checkTrue("the line low, then parked", e->line_log[e->n_line_log - 2] == EW_LINE_LOW &&
	          e->line_log[e->n_line_log - 1] == EW_LINE_PARK);
	checkTrue("300 ms of low line after the frame", took >= 300000u && took < 320000u);
	checkTrue("still nothing erased or programmed", e->erases == 0 && e->programs == 0);
	finish();
	checkTrue("closed", fakeEwCloses() == 1);

	section("ESC settings job: BLHeli_S on E8B1");
	checkTrue("in mode", startInMode(0xB1, false));
	st = status();
	checkTrue("BLHeli_S 33, 112 bytes", st.id.fw == EW_FW_S && st.id.layout == 33 && st.id.len == 112 &&
	          st.contact.signature == 0xE8B1);
	seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("read", taken(seq) && until(EwJobState::InMode, 3000) && status().haveRead &&
	          memcmp(status().page, fakeEwSim()->flash + fakeEwSim()->base, 112) == 0);
	finish();
}

static void testRefusals() {
	section("ESC settings job: commands out of turn");
	start(0xB2, true);
	uint16_t seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("Read before Enter: refused, nothing sent",
	          taken(seq) && status().refused == EwJobRefusal::State && fakeEwSim()->starts == 0 &&
	          status().job == EwJobState::Opening);
	seq = escwireJobCommand(EwJobOp::Leave);
	checkTrue("Leave before Enter: refused", taken(seq) && status().refused == EwJobRefusal::State &&
	          fakeEwSim()->starts == 0);
	seq = escwireJobCommand(EwJobOp::Enter);
	checkTrue("Enter: taken", taken(seq) && status().refused == EwJobRefusal::None);
	seq = escwireJobCommand(EwJobOp::Enter);
	checkTrue("Enter again while Busy: refused", taken(seq) && status().refused == EwJobRefusal::State);
	checkTrue("the first entry runs on", until(EwJobState::InMode, 3000));
	seq = escwireJobCommand(EwJobOp::Enter);
	checkTrue("Enter in mode: refused", taken(seq) && status().refused == EwJobRefusal::State);
	finish();

	section("ESC settings job: a Stop not yet taken stays");
	start(0xB2, true);
	uint16_t a = escwireJobCommand(EwJobOp::Stop);
	uint16_t b = escwireJobCommand(EwJobOp::Enter);
	checkTrue("Enter after a held Stop: dropped", a == b);
	passes(2);
	checkTrue("the Stop: Ended, the line parked, nothing sent",
	          status().job == EwJobState::Ended && fakeEwSim()->line == EW_LINE_PARK &&
	          fakeEwSim()->starts == 0);
	uint16_t c = escwireJobCommand(EwJobOp::Enter);
	checkTrue("a command after it takes a new seq", c != b && taken(c) &&
	          status().refused == EwJobRefusal::State);
	finish();

	section("ESC settings job: Stop in mode");
	checkTrue("in mode", startInMode(0xB2, true));
	seq = escwireJobCommand(EwJobOp::Stop);
	checkTrue("Stop: Ended, the line parked, the result kept", taken(seq) &&
	          status().job == EwJobState::Ended && fakeEwSim()->line == EW_LINE_PARK &&
	          status().result == EW_SES_OK && !status().inMode);
	seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("Read after it: refused", taken(seq) && status().refused == EwJobRefusal::State);
	finish();
}

static void testDriverRefuses() {
	section("ESC settings job: a leave whose frame the driver refuses");
	checkTrue("in mode", startInMode(0xB2, true));
	sim_t *e = fakeEwSim();
	int starts = e->starts;
	sim_script(e, F_START_FALSE, 0, 1, 0);
	uint16_t seq = escwireJobCommand(EwJobOp::Leave);
	checkTrue("taken, not refused: it ran and ended at once", taken(seq) &&
	          status().refused == EwJobRefusal::None && status().lastOp == EwJobOp::Leave);
	checkTrue("Ended, the driver's failure, not left", status().job == EwJobState::Ended &&
	          status().result == EW_SES_ERR_DRIVER && !status().left && !status().inMode);
	checkTrue("the driver refused the frame: the ESC heard nothing", e->starts == starts + 1 &&
	          e->contacted && !e->dead);
	finish();

	section("ESC settings job: a refused command keeps the last operation");
	start(0xB2, true);
	seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("Read before Enter: refused, the last operation still Enter's",
	          taken(seq) && status().refused == EwJobRefusal::State && status().lastOp == EwJobOp::Enter);
	finish();
}

static void testSilent() {
	section("ESC settings job: an ESC that does not answer");
	start(0xB2, true);
	fakeEwSim()->dead = true;
	uint16_t seq = escwireJobCommand(EwJobOp::Enter);
	checkTrue("Enter taken", taken(seq));
	checkTrue("ends: Ended, silent", until(EwJobState::Ended, 6000) &&
	          status().result == EW_SES_ERR_SILENT && !status().inMode);
	finish();
}

static void testHeartbeat() {
	section("ESC settings job: the heartbeat");
	checkTrue("in mode", startInMode(0xB2, true));
	sim_t *e = fakeEwSim();
	// The read's transaction never ends: the session waits for its driver
	// about 750 ms, so the lapse comes first.
	sim_script(e, F_STUCK, 1, 1, 0);
	uint16_t seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("Read taken", taken(seq) && status().job == EwJobState::Busy);
	g_alive = false;
	passes(UI_HEARTBEAT_TIMEOUT_MS - 2);
	checkTrue("lapsing: the read runs on", status().job == EwJobState::Busy);
	passes(3);
	checkTrue("lapsed: the read stopped, Ended", status().job == EwJobState::Ended &&
	          status().result == EW_SES_ERR_STOPPED && status().stalled && !status().haveRead);
	checkTrue("the line parked", e->line == EW_LINE_PARK);
	seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("no command while lapsed", taken(seq) && status().refused == EwJobRefusal::NoUi);
	finish();

	section("ESC settings job: a lapse at Enter");
	start(0xB2, true);
	g_alive = false;
	passes(UI_HEARTBEAT_TIMEOUT_MS + 1);
	seq = escwireJobCommand(EwJobOp::Enter);
	checkTrue("refused, nothing sent", taken(seq) && status().refused == EwJobRefusal::NoUi &&
	          fakeEwSim()->starts == 0 && status().job == EwJobState::Opening);
	finish();

	section("ESC settings job: a lapse in the entry");
	start(0xB2, true);
	seq = escwireJobCommand(EwJobOp::Enter);
	taken(seq);
	g_alive = false;
	passes(UI_HEARTBEAT_TIMEOUT_MS + 2);
	checkTrue("the entry stopped in its hold, nothing sent", status().job == EwJobState::Ended &&
	          status().result == EW_SES_ERR_STOPPED && fakeEwSim()->starts == 0);
	finish();

	section("ESC settings job: a lapse in the leave");
	checkTrue("in mode", startInMode(0xB2, true));
	seq = escwireJobCommand(EwJobOp::Leave);
	taken(seq);
	g_alive = false;
	passes(UI_HEARTBEAT_TIMEOUT_MS + 2);
	checkTrue("the leave runs on", status().job == EwJobState::Busy && !status().stalled);
	checkTrue("and ends: left", until(EwJobState::Ended, 1000) && status().left &&
	          status().result == EW_SES_OK);
	finish();
}

static void testRelease() {
	section("ESC settings job: the release");
	checkTrue("in mode", startInMode(0xB2, true));
	uint16_t seq = escwireJobCommand(EwJobOp::Read);
	taken(seq);
	passes(5);
	sim_t *e = fakeEwSim();
	int aborts = e->aborts;
	core1JobStop();
	passes(1);
	checkTrue("during a read: cut, closed in one pass", core1JobPhase() == Core1JobPhase::Idle &&
	          fakeEwCloses() == 1 && e->aborts == aborts + 1);
	checkTrue("its result: stopped", status().result == EW_SES_ERR_STOPPED &&
	          status().job == EwJobState::Closed);
	escTaskResume();
	fakePumpPoll();

	checkTrue("in mode", startInMode(0xB2, true));
	seq = escwireJobCommand(EwJobOp::Leave);
	taken(seq);
	uint32_t t0 = time_us_32();
	core1JobStop();
	passes(1);
	checkTrue("during the leave: the job stays until it ends",
	          core1JobPhase() == Core1JobPhase::Stopping && fakeEwOpen());
	for (int i = 0; i < 1000 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	checkTrue("then closes, 300 ms or more on", core1JobPhase() == Core1JobPhase::Idle &&
	          fakeEwClosedUs() - t0 >= 300000u && status().left);
	escTaskResume();
	fakePumpPoll();

	checkTrue("in mode", startInMode(0xB2, true));
	int starts = fakeEwSim()->starts;
	core1JobStop();
	passes(1);
	checkTrue("in mode, idle: closed at once, nothing sent", core1JobPhase() == Core1JobPhase::Idle &&
	          fakeEwSim()->starts == starts && !fakeEwOpen());
	escTaskResume();
	fakePumpPoll();
}

void runEscwireJobTests() {
	testBind();
	testEnterReadLeave();
	testRefusals();
	testDriverRefuses();
	testSilent();
	testHeartbeat();
	testRelease();
}
