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
#include "hardware/gpio.h"

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
	escTaskStart();
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
	static const uint8_t JOB[] = { CORE1_JOB_PIO, CORE1_JOB_DMA, CORE1_JOB_CLOCK, CORE1_JOB_PIN };
	static const char *const NAMES[] = { "PIO", "DMA", "CLOCK", "ARG as PIN" };
	for (int i = 0; i < 4; i++) {
		g_alive = true;
		escHeartbeat();
		suspend();
		fakeEwReset(0xB2, true);
		fakeEwRefuse(ERRS[i]);
		// The AM32 transport leaves the pin floating.
		gpio_disable_pulls(SIG);
		core1JobStart(Core1Job::EscWire, SIG, 0xFF);
		core1JobPoll();
		char what[64];
		snprintf(what, sizeof(what), "a refused open: %s, nothing driven", NAMES[i]);
		checkTrue(what, core1JobError() == JOB[i] && core1JobPhase() == Core1JobPhase::Idle &&
		          fakeEwSim()->line_calls == 0 && status().lineError == ERRS[i]);
		bool arg = ERRS[i] == EW_LINE_ERR_ARG;
		snprintf(what, sizeof(what), arg ? "%s: the pin left alone" : "%s: SIG on its pull-up", NAMES[i]);
		checkTrue(what, fakePin(SIG).pull == (arg ? FakePull::None : FakePull::Up));
		escTaskStart();
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
	escTaskStart();
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
	escTaskStart();
	fakePumpPoll();

	checkTrue("in mode", startInMode(0xB2, true));
	int starts = fakeEwSim()->starts;
	core1JobStop();
	passes(1);
	checkTrue("in mode, idle: closed at once, nothing sent", core1JobPhase() == Core1JobPhase::Idle &&
	          fakeEwSim()->starts == starts && !fakeEwOpen());
	escTaskStart();
	fakePumpPoll();
}

/** @brief Start, enter, read: InMode with the whole page. */
static bool startRead(uint8_t sig, bool bluejay) {
	if (!startInMode(sig, bluejay)) return false;
	uint16_t seq = escwireJobCommand(EwJobOp::Read);
	return taken(seq) && until(EwJobState::InMode, 3000) && status().haveRead;
}

/** @brief The simulated ESC's settings page. */
static uint8_t *simPage() { return fakeEwSim()->flash + fakeEwSim()->base; }

/** @brief The page as read, with @p off set to @p v. */
static const uint8_t *draftOf(uint8_t off, uint8_t v) {
	static uint8_t d[EW_PAGE_MAX];
	memcpy(d, status().page, sizeof(d));
	d[off] = v;
	return d;
}

/** @brief Send Write of @p draft and run it to its end, at most @p maxMs. */
static EwJobStatus writeRun(const uint8_t *draft, uint32_t maxMs = 6000) {
	uint16_t n = status().id.len;
	uint16_t seq = escwireJobWrite(draft, n);
	taken(seq);
	for (uint32_t i = 0; i < maxMs && status().job == EwJobState::Busy; i++) passes(1);
	return status();
}

/** @brief The first byte of each transaction from @p from on: the commands, in order. */
static bool commandsAre(int from, const uint8_t *cmds, int n) {
	sim_t *e = fakeEwSim();
	if (e->starts - from != n) return false;
	for (int i = 0; i < n; i++)
		if (e->log[from + i].b[0] != cmds[i]) return false;
	return true;
}

/** @brief Passes until the simulated ESC has started @p n transactions, at most 20 s. */
static bool untilStarts(int n) {
	for (int i = 0; i < 20000 && fakeEwSim()->starts < n; i++) passes(1);
	return fakeEwSim()->starts == n;
}

static void testWrite() {
	section("ESC settings job: Bluejay 208 written and read back");
	checkTrue("read", startRead(0xB2, true));
	sim_t *e = fakeEwSim();
	uint8_t draft[EW_PAGE_MAX];
	memcpy(draft, status().page, sizeof(draft));
	draft[0x15] = 2;   // TIMING MEDIUM LOW
	draft[0x0B] = 2;   // DIRECTION REVERSED
	int from = e->starts;
	uint16_t seq = escwireJobWrite(draft, 255);
	checkTrue("Write taken: Busy", taken(seq) && status().job == EwJobState::Busy &&
	          status().lastOp == EwJobOp::Write && status().refused == EwJobRefusal::None);
	checkTrue("ends: InMode, verified", until(EwJobState::InMode, 6000) && status().result == EW_SES_OK);
	EwJobStatus st = status();
	checkTrue("the page the ESC holds is the draft", memcmp(simPage(), draft, 255) == 0);
	checkTrue("and the status's page with it", memcmp(st.page, draft, 255) == 0 && !st.dirty);
	checkTrue("the tags as read", memcmp(simPage() + 0x40, "#BOARD_NAME_TWO#", 16) == 0 &&
	          memcmp(simPage() + 0x60, "Bluejay (.1 RC1)", 16) == 0);
	static const uint8_t ORDER[] = {
		EW_A_CMD_ADDRESS, EW_A_CMD_READ, EW_A_CMD_ADDRESS, EW_A_CMD_ERASE, EW_A_CMD_ADDRESS,
		EW_A_CMD_ANNOUNCE, EW_A_CMD_PROGRAM, EW_A_CMD_ADDRESS, EW_A_CMD_READ,
	};
	checkTrue("rcbench's order: re-read, erase, data, program, read-back", commandsAre(from, ORDER, 9));
	checkInt("one erase", e->erases, 1);
	checkInt("one program", e->programs, 1);
	checkTrue("nothing but the settings page erased or programmed", fakeEwOnlySettingsPage());
	checkTrue("no invalid transaction, none on a parked line, no line() in one",
	          e->invalid == 0 && e->parked_tx == 0 && e->line_in_xfer == 0);
	checkTrue("the ESC still in mode: LEAVE goes", status().inMode);
	seq = escwireJobCommand(EwJobOp::Leave);
	checkTrue("left", taken(seq) && until(EwJobState::Ended, 2000) && status().left);
	finish();

	section("ESC settings job: BLHeli_S 33 written");
	checkTrue("read", startRead(0xB1, false));
	e = fakeEwSim();
	st = writeRun(draftOf(0x1F, 3));  // DEMAG HIGH
	checkTrue("verified, 112 bytes: the ESC's DEMAG HIGH", st.result == EW_SES_OK && simPage()[0x1F] == 3 &&
	          st.id.len == 112);
	checkTrue("one erase, one program, the settings page only",
	          e->erases == 1 && e->programs == 1 && fakeEwOnlySettingsPage());
	finish();

	section("ESC settings job: Bluejay on EFM8BB51, its page at 0x3000");
	checkTrue("read", startRead(0xB5, true));
	e = fakeEwSim();
	st = writeRun(draftOf(0x1B, 100));  // BEEP STRENGTH
	checkTrue("verified", st.result == EW_SES_OK && simPage()[0x1B] == 100 && e->base == 0x3000);
	checkTrue("the settings page only", e->erases == 1 && fakeEwOnlySettingsPage());
	finish();

	section("ESC settings job: a draft equal to the page sends nothing");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	int n = e->starts;
	st = writeRun(status().page);
	checkTrue("OK at once, nothing sent", st.result == EW_SES_OK && st.lastOp == EwJobOp::Write &&
	          e->starts == n && st.job == EwJobState::InMode);
	finish();
}

static void testWriteGuard() {
	section("ESC settings job: the guard refuses a draft, nothing sent");
	checkTrue("read", startRead(0xB2, true));
	sim_t *e = fakeEwSim();
	int n = e->starts;
	EwJobStatus st = writeRun(draftOf(0x45, 'X'));
	checkTrue("a layout tag changed: refused", st.result == EW_SES_ERR_REFUSED &&
	          st.refuse == EW_REF_CHANGED_BYTE && st.badOff == 0x45);
	st = writeRun(draftOf(0x65, 'x'));
	checkTrue("the name tag changed: refused", st.result == EW_SES_ERR_REFUSED && st.badOff == 0x65);
	st = writeRun(draftOf(0x0D, 0x00));
	checkTrue("the marker changed: refused", st.result == EW_SES_ERR_REFUSED && st.badOff == 0x0D);
	st = writeRun(draftOf(0x02, 207));
	checkTrue("the layout revision changed: refused", st.result == EW_SES_ERR_REFUSED && st.badOff == 0x02);
	st = writeRun(draftOf(0x0A, 24));
	checkTrue("PWM INDICATOR, shown only: refused", st.result == EW_SES_ERR_REFUSED && st.badOff == 0x0A);
	st = writeRun(draftOf(0x0B, 9));
	checkTrue("DIRECTION 9, no value of its own: refused", st.result == EW_SES_ERR_REFUSED &&
	          st.refuse == EW_REF_VALUE);
	st = writeRun(draftOf(0x80, 0));
	checkTrue("a melody byte: refused", st.result == EW_SES_ERR_REFUSED && st.badOff == 0x80);
	checkTrue("nothing sent, the ESC still in mode", e->starts == n && st.inMode &&
	          st.job == EwJobState::InMode);
	uint16_t seq = escwireJobWrite(status().page, 112);
	checkTrue("a draft of another length: refused, nothing sent",
	          taken(seq) && status().refused == EwJobRefusal::State && e->starts == n);
	finish();

	section("ESC settings job: BLHeli_S' throttle bytes are shown only");
	checkTrue("read", startRead(0xB1, false));
	st = writeRun(draftOf(0x19, 40));
	checkTrue("MIN THROTTLE: refused", st.result == EW_SES_ERR_REFUSED && st.badOff == 0x19);
	checkTrue("nothing erased", fakeEwSim()->erases == 0);
	finish();

	section("ESC settings job: read-only pages are never written");
	struct Ro {
		const char *what;
		bool bluejay;
		uint8_t off, v;
	};
	static const Ro RO[] = {
		{ "Bluejay layout 206", true, 0x02, 206 },
		{ "BLHeli_S layout 32", false, 0x02, 32 },
		{ "lost tags", false, 0x40, 0xFF },
		{ "a wrong marker", true, 0x0E, 0x00 },
	};
	for (const Ro &r : RO) {
		start(0xB2, r.bluejay);
		if (r.off == 0x40) memset(simPage() + 0x40, 0xFF, 48);
		else simPage()[r.off] = r.v;
		if (r.off == 0x02 && r.bluejay) simPage()[0x01] = 19;
		fakeEwSnapshot();
		uint16_t s0 = escwireJobCommand(EwJobOp::Enter);
		taken(s0);
		until(EwJobState::InMode, 3000);
		s0 = escwireJobCommand(EwJobOp::Read);
		taken(s0);
		until(EwJobState::InMode, 3000);
		e = fakeEwSim();
		n = e->starts;
		char what[80];
		snprintf(what, sizeof(what), "%s: read, not writable", r.what);
		checkTrue(what, status().haveRead && !status().id.writable);
		st = writeRun(draftOf(0x15, 2));
		snprintf(what, sizeof(what), "%s: a write refused, nothing sent", r.what);
		checkTrue(what, st.result == EW_SES_ERR_NOT_WRITABLE && e->starts == n && e->erases == 0 &&
		          e->programs == 0 && fakeEwOnlySettingsPage());
		finish();
	}

	section("ESC settings job: no write before a read");
	checkTrue("in mode", startInMode(0xB2, true));
	n = fakeEwSim()->starts;
	uint8_t page[EW_PAGE_MAX];
	memcpy(page, simPage(), sizeof(page));
	st = writeRun(page);
	checkTrue("refused: no read, nothing sent", st.result == EW_SES_ERR_NO_READ && fakeEwSim()->starts == n);
	finish();

	section("ESC settings job: Write before Enter");
	start(0xB2, true);
	seq = escwireJobWrite(simPage(), 255);
	checkTrue("refused, nothing sent", taken(seq) && status().refused == EwJobRefusal::State &&
	          fakeEwSim()->starts == 0);
	finish();
}

static void testWriteFaults() {
	section("ESC settings job: a write the ESC changed under");
	checkTrue("read", startRead(0xB2, true));
	sim_t *e = fakeEwSim();
	simPage()[0x1B] = 41;  // the page moved since the read
	fakeEwSnapshot();
	EwJobStatus st = writeRun(draftOf(0x15, 2));
	checkTrue("ESC PAGE CHANGED: nothing erased", st.result == EW_SES_ERR_CHANGED && e->erases == 0 &&
	          e->programs == 0 && !st.dirty && st.inMode);
	uint16_t seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("read again: the page as it is now", taken(seq) && until(EwJobState::InMode, 3000) &&
	          status().page[0x1B] == 41);
	st = writeRun(draftOf(0x15, 2));
	checkTrue("then written", st.result == EW_SES_OK && simPage()[0x15] == 2 && simPage()[0x1B] == 41);
	finish();

	section("ESC settings job: the ESC gone before the erase");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	sim_script(e, F_DEAD, 0, 1, 0);
	st = writeRun(draftOf(0x15, 2), 8000);
	checkTrue("READ FAILED: nothing erased, the page as read", st.result == EW_SES_ERR_READ && e->erases == 0 &&
	          e->programs == 0 && !st.dirty && memcmp(simPage(), st.page, 255) == 0);
	finish();

	section("ESC settings job: the ESC's power cut after the erase");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	uint8_t orig[EW_PAGE_MAX];
	memcpy(orig, status().page, sizeof(orig));
	// From the data on: transactions 0 to 3 are the re-read and the erase.
	sim_script(e, F_DEAD, 5, 1, 0);
	st = writeRun(draftOf(0x15, 2), 8000);
	checkTrue("NO ANSWER IN THE WRITE: lost, half written", st.result == EW_SES_ERR_LOST && st.dirty &&
	          !st.reread && st.inMode);
	checkTrue("the page erased, the tags gone with it", e->erases == 1 && simPage()[0x40] == 0xFF);
	checkTrue("the status keeps the page as first read", memcmp(st.page, orig, 255) == 0);
	checkTrue("nothing else erased", fakeEwOnlySettingsPage());
	seq = escwireJobCommand(EwJobOp::Leave);
	checkTrue("LEAVE refused: the page is half written", taken(seq) &&
	          status().refused == EwJobRefusal::State && status().inMode);
	e->dead = false;
	st = writeRun(draftOf(0x15, 2));
	checkTrue("a write comes after a read only", st.result == EW_SES_ERR_MUST_READ && st.dirty);
	seq = escwireJobCommand(EwJobOp::Read);
	checkTrue("read: half written, read since", taken(seq) && until(EwJobState::InMode, 3000) &&
	          status().result == EW_SES_OK && status().dirty && status().reread);
	checkTrue("the status still keeps the page as first read", memcmp(status().page, orig, 255) == 0);
	uint8_t other[EW_PAGE_MAX];
	memcpy(other, orig, sizeof(other));
	other[0x1B] = 99;
	seq = escwireJobWrite(other, 255);
	checkTrue("a third page: refused while half written", taken(seq) &&
	          status().refused == EwJobRefusal::State);
	st = writeRun(orig);
	checkTrue("the page as read: written back, tags included", st.result == EW_SES_OK && !st.dirty &&
	          memcmp(simPage(), orig, 255) == 0);
	checkTrue("still the settings page only", fakeEwOnlySettingsPage());
	finish();

	section("ESC settings job: a program that does not take, 3 times: half written");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	memcpy(orig, status().page, sizeof(orig));
	sim_script(e, F_NO_TAKE, 0, 40, 0);
	uint8_t draft[EW_PAGE_MAX];
	memcpy(draft, draftOf(0x15, 2), sizeof(draft));
	st = writeRun(draft, 10000);
	checkTrue("TORN: neither page, read back", st.result == EW_SES_ERR_TORN && st.dirty && st.reread);
	checkInt("3 procedures: 3 erases", e->erases, 3);
	checkInt("3 programs", e->programs, 3);
	e->fail = F_NONE;
	st = writeRun(draft);
	checkTrue("the draft at once: written", st.result == EW_SES_OK && memcmp(simPage(), draft, 255) == 0);
	checkTrue("the settings page only", fakeEwOnlySettingsPage());
	finish();

	section("ESC settings job: a corrupt byte once: the procedure again");
	checkTrue("read", startRead(0xB1, false));
	e = fakeEwSim();
	sim_script(e, F_CORRUPT, 6, 1, 0x1F);
	st = writeRun(draftOf(0x1F, 3), 10000);
	checkTrue("verified on the second procedure", st.result == EW_SES_OK && simPage()[0x1F] == 3 &&
	          e->erases == 2 && e->programs == 2);
	finish();

	section("ESC settings job: erases the ESC never hears: the page as read");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	memcpy(orig, status().page, sizeof(orig));
	int base = e->starts;
	seq = escwireJobWrite(draftOf(0x15, 2), 255);
	// Each procedure: address, erase, address, data, program; a failed erase
	// reads the page back, address and read, and starts again.
	static const int ERASES[] = { 3, 7, 11 };
	for (int k : ERASES) {
		untilStarts(base + k);
		sim_script(e, F_SILENT, 0, 1, 0);
	}
	for (int i = 0; i < 20000 && status().job == EwJobState::Busy; i++) passes(1);
	st = status();
	checkTrue("WRITE FAILED: the page as read", st.result == EW_SES_ERR_WRITE_FAILED && !st.dirty &&
	          memcmp(simPage(), orig, 255) == 0 && e->erases == 0 && e->programs == 0);
	finish();

	section("ESC settings job: a driver that fails in the write");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	sim_script(e, F_STUCK, 4, 1, 0);
	st = writeRun(draftOf(0x15, 2), 8000);
	checkTrue("the driver's failure after the erase: the session ended, half written",
	          st.result == EW_SES_ERR_DRIVER && st.dirty && !st.inMode && st.job == EwJobState::Ended);
	checkTrue("the line parked", e->line == EW_LINE_PARK);
	finish();
}

static void testWriteHeartbeat() {
	section("ESC settings job: a lapse after the erase: the write runs to its read-back");
	checkTrue("read", startRead(0xB2, true));
	sim_t *e = fakeEwSim();
	uint8_t draft[EW_PAGE_MAX];
	memcpy(draft, draftOf(0x15, 2), sizeof(draft));
	uint16_t seq = escwireJobWrite(draft, 255);
	taken(seq);
	for (int i = 0; i < 3000 && e->erases == 0; i++) passes(1);
	checkTrue("erased: writing", e->erases == 1 && status().writing && status().dirty);
	g_alive = false;
	passes(UI_HEARTBEAT_TIMEOUT_MS + 5);
	checkTrue("lapsed: the write runs on", status().job == EwJobState::Busy && status().stalled);
	for (int i = 0; i < 4000 && status().job == EwJobState::Busy; i++) passes(1);
	EwJobStatus st = status();
	checkTrue("read back and verified", st.result == EW_SES_OK && memcmp(simPage(), draft, 255) == 0 &&
	          !st.dirty);
	checkTrue("then the session ended, the line parked", st.job == EwJobState::Ended && !st.inMode &&
	          e->line == EW_LINE_PARK);
	checkInt("one procedure", e->programs, 1);
	finish();

	section("ESC settings job: a lapse before the erase stops the write");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	sim_script(e, F_STUCK, 1, 1, 0);
	seq = escwireJobWrite(draftOf(0x15, 2), 255);
	taken(seq);
	g_alive = false;
	passes(UI_HEARTBEAT_TIMEOUT_MS + 3);
	st = status();
	checkTrue("stopped, nothing erased, the page as read", st.result == EW_SES_ERR_STOPPED &&
	          st.job == EwJobState::Ended && e->erases == 0 && e->programs == 0 && !st.dirty);
	finish();

	section("ESC settings job: a lapse in a write that ends half written");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	sim_script(e, F_NO_TAKE, 0, 40, 0);
	seq = escwireJobWrite(draftOf(0x15, 2), 255);
	taken(seq);
	for (int i = 0; i < 3000 && e->erases == 0; i++) passes(1);
	g_alive = false;
	for (int i = 0; i < 8000 && status().job == EwJobState::Busy; i++) passes(1);
	st = status();
	checkTrue("no second procedure: TORN after one, parked", st.result == EW_SES_ERR_TORN &&
	          e->erases == 1 && e->programs == 1 && st.job == EwJobState::Ended && e->line == EW_LINE_PARK);
	finish();

	section("ESC settings job: the release in a write after its erase");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	memcpy(draft, draftOf(0x15, 2), sizeof(draft));
	seq = escwireJobWrite(draft, 255);
	taken(seq);
	for (int i = 0; i < 3000 && e->erases == 0; i++) passes(1);
	core1JobStop();
	passes(1);
	checkTrue("the job stays until the write ends", core1JobPhase() == Core1JobPhase::Stopping && fakeEwOpen());
	for (int i = 0; i < 4000 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	checkTrue("verified, then closed", core1JobPhase() == Core1JobPhase::Idle && !fakeEwOpen() &&
	          memcmp(simPage(), draft, 255) == 0 && status().result == EW_SES_OK);
	escTaskStart();
	fakePumpPoll();

	section("ESC settings job: the release before the erase cuts the write");
	checkTrue("read", startRead(0xB2, true));
	e = fakeEwSim();
	seq = escwireJobWrite(draftOf(0x15, 2), 255);
	taken(seq);
	core1JobStop();
	passes(1);
	checkTrue("closed in one pass, nothing erased", core1JobPhase() == Core1JobPhase::Idle && e->erases == 0 &&
	          status().result == EW_SES_ERR_STOPPED);
	escTaskStart();
	fakePumpPoll();
}

/** @brief Append a frame of @p n bytes to the simulated ESC's log, as if it went out. */
static void logFrame(const uint8_t *f, size_t n) {
	sim_t *e = fakeEwSim();
	e->log[e->starts].n = (uint16_t)n;
	memcpy(e->log[e->starts].b, f, n);
	e->starts++;
}

static void testRegionCheck() {
	section("ESC settings job: the settings-page check itself");
	fakeEwReset(0xB2, true);
	sim_t *e = fakeEwSim();
	bool noFF = true;
	for (uint32_t a = 0; a < SIM_FLASH; a++)
		if ((a < e->base || a >= (uint32_t)e->base + e->page) && e->flash[a] == 0xFF) noFF = false;
	checkTrue("no 0xFF outside the settings page: firmware and bootloader patterned", noFF);
	checkTrue("a fresh ESC passes", fakeEwOnlySettingsPage());
	uint8_t f[EW_TX_MAX];
	e->contacted = true;
	size_t n = ew_a_address(f, 0x1C00);
	sim_frame(e, f, n);
	n = ew_a_short(f, EW_A_CMD_ERASE, 0x01);
	sim_frame(e, f, n);
	checkTrue("an erase of the bootloader's page, done on the sim: caught by the bytes", !fakeEwOnlySettingsPage());
	fakeEwReset(0xB2, true);
	e = fakeEwSim();
	n = ew_a_address(f, 0x0000);
	logFrame(f, n);
	n = ew_a_short(f, EW_A_CMD_ERASE, 0x01);
	logFrame(f, n);
	checkTrue("an erase frame after an address other than the page: caught by the log", !fakeEwOnlySettingsPage());
	fakeEwReset(0xB2, true);
	e = fakeEwSim();
	n = ew_a_address(f, e->base);
	logFrame(f, n);
	n = ew_a_read(f, 255);
	logFrame(f, n);
	n = ew_a_short(f, EW_A_CMD_PROGRAM, 0x01);
	logFrame(f, n);
	checkTrue("a program after a read with no address frame between: caught", !fakeEwOnlySettingsPage());
	fakeEwReset(0xB2, true);
	e = fakeEwSim();
	n = ew_a_address(f, e->base);
	logFrame(f, n);
	n = ew_a_short(f, EW_A_CMD_ERASE, 0x01);
	logFrame(f, n);
	checkTrue("an erase right after the page's address: passes", fakeEwOnlySettingsPage());
	e->starts = (int)SIM_LOG_MAX + 1;
	checkTrue("more transactions than the log holds: fails", !fakeEwOnlySettingsPage());
	fakeEwReset(0xB2, true);
}

void runEscwireJobTests() {
	testBind();
	testEnterReadLeave();
	testRefusals();
	testDriverRefuses();
	testSilent();
	testHeartbeat();
	testRelease();
	testWrite();
	testWriteGuard();
	testWriteFaults();
	testWriteHeartbeat();
	testRegionCheck();
}
