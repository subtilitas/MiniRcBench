/**
 * @file test_hitec_job.cpp
 * @brief The Hitec programmer's core1 job, from the job table, against the
 *        simulated servo of fake_hitec_line.h on the virtual clock.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * The job runs as it ships: core1JobStart() after the fake DShot pump is
 * down, and core1JobPoll() once a millisecond, which the test calls itself in
 * core1's place. The servo follows GP0 on every pass.
 */

#include "check.h"
#include "fakes.h"
#include "fake_hitec_line.h"
#include "core1_job.h"
#include "esc_task.h"
#include "hitec_job.h"
#include "hitec_line.h"
#include "kst_seek.h"
#include "servo_supply.h"
#include "settings.h"

#include <pico/time.h>
#include <stdio.h>
#include <string.h>

/** @brief The SIG GPIO the job is started on. */
static const uint8_t SIG = 29;

/** @brief Whether the heartbeat is fed on each pass. */
static bool g_alive = true;

/** @brief Whether the card stamp is fed on each pass. */
static bool g_stamp = false;

/** @brief The level the bench drives on GP0. */
static bool gp0High() {
	FakePin p = fakePin(SERVO_SUPPLY_PIN);
	return p.output && p.high;
}

/** @brief @p ms passes of core1, one a millisecond. */
static void passes(uint32_t ms) {
	for (uint32_t i = 0; i < ms; i++) {
		fakeAdvance(1);
		if (g_alive) escHeartbeat();
		if (g_stamp) hitecCardStamp();
		fakeHitecSimUpdate(gp0High(), time_us_32());
		core1JobPoll();
		fakeHitecSimUpdate(gp0High(), time_us_32());
	}
}

static HitecStatus status() {
	HitecStatus st;
	hitecJobStatus(&st);
	return st;
}

/** @brief Passes until the job is in @p want, at most @p maxMs. */
static bool until(HitecJobState want, uint32_t maxMs) {
	for (uint32_t i = 0; i < maxMs; i++) {
		if (status().job == want) return true;
		passes(1);
	}
	return status().job == want;
}

static uint16_t send(HitecOp op, const hitec_image_t *target = nullptr) {
	HitecCmd c;
	memset(&c, 0, sizeof(c));
	c.op = op;
	if (target) c.target = *target;
	return hitecJobCommand(&c);
}

/** @brief DShot down: the fake pump drained and suspended. */
static void suspend() {
	escTaskSuspend();
	for (int i = 0; i < 4 && !escTaskSuspended(); i++) {
		fakeAdvance(100);
		fakePumpPoll();
	}
}

/** @brief SERVO SUPPLY at GP0 (@p switched) or NONE. */
static void supply(bool switched) {
	settings()->servoSupply = switched ? 1 : 0;
	servoSupplyConfigure(switched ? 1 : 0);
}

/** @brief DShot down, a fresh servo of @p family, the job started and bound. */
static void start(uint8_t family, bool switched) {
	g_alive = true;
	g_stamp = false;
	escHeartbeat();
	suspend();
	supply(switched);
	fakeHitecLineReset(family);
	fakeHitecSim()->userSupply = !switched;
	hitecJobSetFamily(family);
	core1JobStart(Core1Job::Hitec, SIG, 0xFF);
	core1JobPoll();
}

/** @brief Release the job, give DShot the pin back, SERVO SUPPLY back to NONE. */
static void finish() {
	g_alive = true;
	g_stamp = false;
	core1JobStop();
	for (int i = 0; i < 3000 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	escTaskResume();
	fakePumpPoll();
	supply(false);
}

/** @brief Start, enter and read: InMode with a backup. */
static bool startInMode(uint8_t family, bool switched) {
	start(family, switched);
	send(HitecOp::Enter);
	passes(1);
	return until(HitecJobState::Busy, 4000) && until(HitecJobState::InMode, 20000) &&
	       status().result == HITEC_SES_OK && status().hasBackup;
}

/** @brief The image with one setting changed, as the screen's draft would be. */
static hitec_image_t draftOf(const hitec_image_t *img, uint8_t family) {
	hitec_image_t t = *img;
	if (family == HITEC_CLASSIC)
		checkTrue("the draft takes the classic speed",
		          hitec_param_set(&t, HITEC_LAYOUT_HS5, HITEC_P_C_SPEED, 50) == HITEC_SET_OK);
	else
		checkTrue("the draft takes the D series speed",
		          hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, 30) == HITEC_SET_OK);
	return t;
}

/** @brief The first address bit set in @p diff; -1 for none. */
static int firstDiff(const uint8_t *diff) {
	for (int a = 0; a < 256; a++)
		if (diff[a / 8] & (1u << (a % 8))) return a;
	return -1;
}

static void testBind() {
	section("Hitec job: the bind");
	start(HITEC_D, false);
	checkTrue("D series: Opening, the line open for the D series",
	          status().job == HitecJobState::Opening && fakeHitecLineOpens() == 1 &&
	          fakeHitecLineFamily() == HITEC_D && status().family == HITEC_D);
	finish();
	start(HITEC_CLASSIC, true);
	checkTrue("classic: open for classic, GP0 driven low",
	          fakeHitecLineFamily() == HITEC_CLASSIC && fakePin(SERVO_SUPPLY_PIN).output &&
	          !gp0High() && status().supplyFitted);
	finish();

	static const uint8_t ERRS[] = { HITEC_LINE_ERR_PIO, HITEC_LINE_ERR_DMA, HITEC_LINE_ERR_CLOCK };
	static const uint8_t JOB[] = { CORE1_JOB_PIO, CORE1_JOB_DMA, CORE1_JOB_CLOCK };
	static const char *const NAMES[] = { "PIO", "DMA", "CLOCK" };
	for (int i = 0; i < 3; i++) {
		g_alive = true;
		escHeartbeat();
		suspend();
		supply(true);
		fakeHitecLineReset(HITEC_D);
		fakeHitecLineRefuse(ERRS[i]);
		hitecJobSetFamily(HITEC_D);
		core1JobStart(Core1Job::Hitec, SIG, 0xFF);
		core1JobPoll();
		char what[64];
		snprintf(what, sizeof(what), "a refused open: %s, GP0 not driven", NAMES[i]);
		checkTrue(what, core1JobError() == JOB[i] && !fakePin(SERVO_SUPPLY_PIN).output);
		escTaskResume();
		fakePumpPoll();
		supply(false);
	}

	// A command sent before the bind is not taken by it.
	g_alive = true;
	escHeartbeat();
	suspend();
	fakeHitecLineReset(HITEC_D);
	fakeHitecSim()->userSupply = true;
	hitecJobSetFamily(HITEC_D);
	send(HitecOp::Enter);
	core1JobStart(Core1Job::Hitec, SIG, 0xFF);
	core1JobPoll();
	passes(5);
	checkTrue("a command held at the bind is not taken", status().job == HitecJobState::Opening);
	finish();
}

static void testEntryAndRead() {
	section("Hitec job: entries and the full read");
	start(HITEC_D, false);
	int puts = fakePin(SERVO_SUPPLY_PIN).puts;
	send(HitecOp::Enter);
	passes(1);
	const hitec_xfer_t *x = fakeHitecLastXfer();
	checkTrue("D series, NONE: no POWER, the read of 0x00 at once",
	          fakeHitecStarts() == 1 && x->kind == HITEC_XFER_SEND && x->n_tx == HITEC_D_READ_LEN &&
	          x->tx[2] == HITEC_D_REG_MODEL);
	checkTrue("then the full read, with its progress",
	          until(HitecJobState::Busy, 100) && status().total > 0);
	bool inMode = until(HitecJobState::InMode, 20000);
	HitecStatus rd = status();
	checkTrue("the read ends InMode with the image and the backup",
	          inMode && rd.result == HITEC_SES_OK && rd.hasImage && rd.hasBackup && !rd.unknown &&
	          memcmp(&rd.image, &rd.backup, sizeof(hitec_image_t)) == 0);
	send(HitecOp::ReadAll);
	passes(1);
	checkTrue("READ AGAIN reads", status().job == HitecJobState::Busy &&
	          until(HitecJobState::InMode, 20000) && status().result == HITEC_SES_OK);
	send(HitecOp::Leave);
	until(HitecJobState::Closed, 3000);
	checkTrue("NONE: GP0 untouched through the claim", fakePin(SERVO_SUPPLY_PIN).puts == puts);
	finish();

	start(HITEC_D, true);
	uint32_t t0 = time_us_32();
	send(HitecOp::Enter);
	passes(1990);
	checkTrue("D series with GP0: 1.99 s into the entry, off, nothing sent",
	          !gp0High() && fakeHitecStarts() == 0 && status().power == HitecPower::Off &&
	          status().supplyLeftDs >= 1);
	checkTrue("on after more than 2000 ms off, from the bind",
	          until(HitecJobState::Busy, 3000) && fakeHitecSim()->switchOns == 1 &&
	          fakeHitecSim()->onUs - t0 > 2000000u);
	checkTrue("the model read 1000 ms after the switch-on",
	          until(HitecJobState::InMode, 20000) && status().hasBackup && gp0High());
	finish();

	supply(false);
	start(HITEC_CLASSIC, false);
	send(HitecOp::Enter);
	passes(1);
	checkTrue("classic without the switch: Enter refused",
	          status().refused == HitecRefusal::State && status().job == HitecJobState::Opening);
	finish();

	start(HITEC_CLASSIC, true);
	send(HitecOp::Enter);
	passes(2100);
	checkTrue("classic: the LISTEN starts with the switch-on, answering `+`",
	          fakeHitecSim()->switchOns == 1 && fakeHitecLastXfer()->kind == HITEC_XFER_LISTEN &&
	          fakeHitecLastXfer()->answer);
	checkTrue("the second `+`, `67`, the full read: InMode",
	          until(HitecJobState::InMode, 30000) && status().result == HITEC_SES_OK &&
	          status().image.version == fakeHitecSim()->version && status().hasBackup);
	finish();

	start(HITEC_CLASSIC, true);
	fakeHitecSim()->dead = true;
	send(HitecOp::Enter);
	checkTrue("classic, no `+`: EntryEnded with the supply off",
	          until(HitecJobState::EntryEnded, 6000) && !gp0High() &&
	          status().result != HITEC_SES_OK && !status().stalled);
	fakeHitecSim()->dead = false;
	send(HitecOp::Enter);
	passes(1);
	checkTrue("RETRY enters again", status().job == HitecJobState::Entering &&
	          until(HitecJobState::InMode, 30000) && status().result == HITEC_SES_OK);
	finish();
}

static void testWrite() {
	section("Hitec job: writes");
	static const uint8_t FAMILIES[] = { HITEC_D, HITEC_CLASSIC };
	for (uint8_t family : FAMILIES) {
		const char *name = family == HITEC_D ? "D series" : "classic";
		char what[96];
		checkTrue(family == HITEC_D ? "D series in the mode" : "classic in the mode",
		          startInMode(family, true));
		HitecStatus st = status();
		hitec_image_t t = draftOf(&st.image, family);
		send(HitecOp::Write, &t);
		passes(1);
		snprintf(what, sizeof(what), "%s: the write runs", name);
		checkTrue(what, status().job == HitecJobState::Busy && status().lastWrite);
		until(HitecJobState::InMode, 20000);
		st = status();
		bool same = true, changed = false;
		for (int a = 0; a < 256; a++) {
			if (t.mem[a] == st.backup.mem[a]) continue;
			changed = true;
			same = same && st.image.mem[a] == t.mem[a] && fakeHitecSim()->mem[a] == t.mem[a];
		}
		snprintf(what, sizeof(what), "%s: written and verified, servo and image hold the draft", name);
		checkTrue(what, st.result == HITEC_SES_OK && st.stepIndex > 0 && changed && same);
		finish();
	}

	// A read-back that differs.
	startInMode(HITEC_D, true);
	HitecStatus st = status();
	hitec_image_t t = draftOf(&st.image, HITEC_D);
	int addr = -1;
	for (int a = 0; a < 256 && addr < 0; a += 2)
		if (t.mem[a] != st.image.mem[a] || t.mem[a + 1] != st.image.mem[a + 1]) addr = a;
	fakeHitecSim()->ignoreAddr = addr;
	send(HitecOp::Write, &t);
	passes(1);
	until(HitecJobState::InMode, 20000);
	st = status();
	checkTrue("a read-back that differs: ERR_VERIFY, diff names the register",
	          st.result == HITEC_SES_ERR_VERIFY && firstDiff(st.diff) == addr && !st.unknown);
	finish();

	// A servo lost after a plan step: the image is not known until a read.
	startInMode(HITEC_D, true);
	st = status();
	t = draftOf(&st.image, HITEC_D);
	fakeHitecSim()->dieAfterWrites = 1;
	send(HitecOp::Write, &t);
	passes(1);
	until(HitecJobState::InMode, 20000);
	st = status();
	checkTrue("a D series write cut by a lost servo: an error, the image unknown",
	          st.result != HITEC_SES_OK && st.unknown);
	send(HitecOp::Write, &t);
	passes(1);
	checkTrue("Write refused as Unknown", status().refused == HitecRefusal::Unknown);
	fakeHitecSim()->dead = false;
	send(HitecOp::ReadAll);
	passes(1);
	until(HitecJobState::InMode, 20000);
	checkTrue("after a read the image is known again",
	          status().result == HITEC_SES_OK && !status().unknown);
	send(HitecOp::Write, &t);
	passes(1);
	until(HitecJobState::InMode, 20000);
	checkTrue("and the write is taken", status().refused == HitecRefusal::None &&
	          status().result == HITEC_SES_OK);
	finish();
}

static void testAbort() {
	section("Hitec job: Abort");
	start(HITEC_CLASSIC, true);
	send(HitecOp::Enter);
	passes(500);
	send(HitecOp::Abort);
	passes(5);
	checkTrue("in the off phase: EntryEnded, never switched on",
	          status().job == HitecJobState::EntryEnded && fakeHitecSim()->switchOns == 0 &&
	          status().result == HITEC_SES_ERR_ABORTED);
	finish();

	start(HITEC_CLASSIC, true);
	fakeHitecSim()->plusMs = 1500;
	send(HitecOp::Enter);
	passes(2100);
	unsigned cuts = fakeHitecAborts();
	send(HitecOp::Abort);
	passes(5);
	checkTrue("in the LISTEN: cut, EntryEnded, the supply off",
	          fakeHitecAborts() == cuts + 1 && status().job == HitecJobState::EntryEnded &&
	          status().result == HITEC_SES_ERR_ABORTED && !gp0High());
	finish();

	startInMode(HITEC_D, false);
	send(HitecOp::ReadAll);
	passes(2);
	unsigned starts = fakeHitecStarts();
	cuts = fakeHitecAborts();
	send(HitecOp::Abort);
	passes(20);
	checkTrue("during a read: the SEND finishes, nothing more starts",
	          fakeHitecAborts() == cuts && fakeHitecStarts() == starts &&
	          status().job == HitecJobState::InMode && status().result == HITEC_SES_ERR_ABORTED);
	send(HitecOp::ReadAll);
	passes(1);
	until(HitecJobState::InMode, 20000);
	HitecStatus st = status();
	hitec_image_t t = draftOf(&st.image, HITEC_D);
	send(HitecOp::Write, &t);
	passes(1);
	send(HitecOp::Abort);
	passes(1);
	checkTrue("Abort refused during a write", status().refused == HitecRefusal::State &&
	          status().job == HitecJobState::Busy);
	until(HitecJobState::InMode, 20000);
	finish();
}

static void testHeartbeat() {
	section("Hitec job: the heartbeat");
	start(HITEC_D, true);
	send(HitecOp::Enter);
	passes(500);
	g_alive = false;
	passes(3000);
	checkTrue("a lapse in the off phase: never on, stalled",
	          fakeHitecSim()->switchOns == 0 && status().job == HitecJobState::EntryEnded &&
	          status().stalled);
	send(HitecOp::Enter);
	passes(1);
	checkTrue("RETRY with the heartbeat lapsed: refused", status().refused == HitecRefusal::NoUi);
	g_alive = true;
	finish();

	start(HITEC_CLASSIC, true);
	fakeHitecSim()->plusMs = 1500;
	send(HitecOp::Enter);
	passes(2100);
	g_alive = false;
	passes(400);
	checkTrue("a lapse in the classic LISTEN: cut, off, stalled",
	          !gp0High() && status().job == HitecJobState::EntryEnded && status().stalled);
	g_alive = true;
	finish();

	startInMode(HITEC_D, true);
	HitecStatus st = status();
	hitec_image_t t = draftOf(&st.image, HITEC_D);
	send(HitecOp::Write, &t);
	passes(1);
	g_alive = false;
	passes(300);
	checkTrue("a lapse during a write: it runs on, the supply on",
	          status().job == HitecJobState::Busy && gp0High());
	until(HitecJobState::EntryEnded, 20000);
	st = status();
	checkTrue("the write finished, then off, stalled, out of the mode, stallWrote",
	          !gp0High() && st.stalled && st.stallWrote && !st.inMode && !st.hasBackup &&
	          fakeHitecSim()->writes > 0);
	g_alive = true;
	finish();

	startInMode(HITEC_D, true);
	g_alive = false;
	passes(300);
	checkTrue("a lapse in the mode: off, stalled, not written",
	          !gp0High() && status().stalled && !status().stallWrote);
	g_alive = true;
	finish();

	startInMode(HITEC_D, false);
	g_alive = false;
	passes(400);
	checkTrue("NONE: a lapse leaves the session in the mode",
	          status().job == HitecJobState::InMode && status().inMode && !status().stalled);
	send(HitecOp::ReadAll);
	passes(1);
	checkTrue("no read with the heartbeat lapsed", status().refused == HitecRefusal::NoUi);
	g_alive = true;
	send(HitecOp::ReadAll);
	passes(1);
	checkTrue("back: READ AGAIN reads", status().job == HitecJobState::Busy);
	until(HitecJobState::InMode, 20000);
	finish();
}

static void testLeave() {
	section("Hitec job: leave and close");
	startInMode(HITEC_D, true);
	int closes = fakeHitecLineCloses();
	send(HitecOp::Leave);
	passes(1);
	uint32_t offUs = time_us_32();
	checkTrue("LEAVE with GP0: off at once, the line still open",
	          !gp0High() && status().job == HitecJobState::Leaving &&
	          fakeHitecLineCloses() == closes && status().supplyLeftDs > 0);
	passes(1990);
	checkTrue("1.99 s later: still open", fakeHitecLineCloses() == closes);
	checkTrue("closed 2000 ms after the switch-off",
	          until(HitecJobState::Closed, 50) && fakeHitecLineClosedUs() - offUs >= 2000000u);
	finish();
	FakePin p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("released: GP0 an undriven input", !p.output && p.inputOff);

	startInMode(HITEC_D, false);
	send(HitecOp::Leave);
	checkTrue("LEAVE with NONE: the D series reboot, then closed",
	          until(HitecJobState::Closed, 3000) && fakeHitecSim()->reboots == 1);
	finish();

	startInMode(HITEC_CLASSIC, true);
	core1JobStop();
	uint32_t stopUs = time_us_32();
	for (int i = 0; i < 3000 && core1JobPhase() != Core1JobPhase::Idle; i++) passes(1);
	checkTrue("the release waits 2000 ms off before the line closes",
	          core1JobPhase() == Core1JobPhase::Idle && fakeHitecLineClosedUs() - stopUs >= 2000000u);
	escTaskResume();
	fakePumpPoll();
	supply(false);

	// A leave during a write: the write finishes first.
	startInMode(HITEC_D, true);
	HitecStatus st = status();
	hitec_image_t t = draftOf(&st.image, HITEC_D);
	send(HitecOp::Write, &t);
	passes(1);
	send(HitecOp::Leave);
	passes(1);
	checkTrue("LEAVE during a write: Leaving, the supply still on",
	          status().job == HitecJobState::Leaving && gp0High());
	until(HitecJobState::Closed, 20000);
	checkTrue("the write ran to its end before the close",
	          status().result == HITEC_SES_OK && !gp0High());
	finish();
}

static void testCommands() {
	section("Hitec job: the command block");
	startInMode(HITEC_D, false);
	send(HitecOp::ReadAll);
	passes(1);
	uint16_t abortSeq = send(HitecOp::Abort);
	uint16_t seq = send(HitecOp::ReadAll);
	checkTrue("a held Abort is not replaced by a later command", seq == abortSeq);
	uint16_t leaveSeq = send(HitecOp::Leave);
	checkTrue("a Leave replaces a held Abort", leaveSeq != abortSeq);
	passes(1);
	checkTrue("and is taken", status().ackSeq == leaveSeq && status().job == HitecJobState::Leaving);
	finish();
}

static void testCardPhase() {
	section("Hitec job: the card phase");
	static const char *const NAMES[] = { "Opening", "EntryEnded", "InMode", "Busy" };
	for (int i = 0; i < 4; i++) {
		start(HITEC_D, false);
		if (i == 1) {
			fakeHitecSim()->dead = true;
			send(HitecOp::Enter);
			until(HitecJobState::EntryEnded, 3000);
		} else if (i >= 2) {
			send(HitecOp::Enter);
			until(HitecJobState::InMode, 20000);
			if (i == 3) {
				send(HitecOp::ReadAll);
				passes(1);
			}
		}
		uint16_t seq = send(HitecOp::CardBegin);
		passes(1);
		HitecStatus st = status();
		bool taken = i < 3;
		char what[64];
		snprintf(what, sizeof(what), "CardBegin in %s: %s", NAMES[i], taken ? "taken" : "refused");
		checkTrue(what, st.ackSeq == seq && (st.refused == HitecRefusal::None) == taken &&
		                st.cardPhase == taken);
		send(HitecOp::CardEnd);
		passes(1);
		checkTrue("CardEnd closes it", !status().cardPhase);
		finish();
	}

	startInMode(HITEC_D, true);
	send(HitecOp::CardBegin);
	passes(1);
	g_alive = false;
	g_stamp = true;
	passes(900);
	checkTrue("900 ms of card stamps: on, in the mode", gp0High() && status().job == HitecJobState::InMode);
	g_stamp = false;
	passes(300);
	checkTrue("the stamps stop: off at the UI timeout", !gp0High() && status().stalled);
	finish();

	startInMode(HITEC_D, true);
	send(HitecOp::CardBegin);
	passes(1);
	g_alive = false;
	g_stamp = true;
	passes(HITEC_CARD_PHASE_MAX_MS - 100);
	checkTrue("stamped 100 ms short of the cap: on", gp0High());
	passes(200);
	checkTrue("stamped past the cap: off", !gp0High() && status().stalled);
	finish();
}

void runHitecJobTests() {
	testBind();
	testEntryAndRead();
	testWrite();
	testAbort();
	testHeartbeat();
	testLeave();
	testCommands();
	testCardPhase();
}
