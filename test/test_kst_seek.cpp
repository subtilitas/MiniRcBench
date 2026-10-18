/**
 * @file test_kst_seek.cpp
 * @brief The KST search against rcbench's simulated servo.
 *
 * Every check runs kst_seek.cpp and rcbench's session as they ship, on the
 * simulator's own clock, stepped every 100 us. The simulator counts what a
 * master must never do; the search's frames are read from its log.
 */

#include "check.h"
#include "kst_seek.h"
#include "rcbench/kst_sim.h"

#include <string.h>

/** @brief One servo on one wire, a session on it and a search. */
struct Bench {
	sim_t         sim;
	kst_session_t ses;
	KstSeekState  seek;
};

static Bench g_b;
/** @brief Added to the simulator's clock to give the search's ms clock. */
static uint32_t g_msOffset = 0;

static uint32_t nowMs() { return g_b.sim.now_us / 1000u + g_msOffset; }

/** @brief A fresh bench; the servo powered or not. */
static void bench(bool powered) {
	sim_init(&g_b.sim);
	g_b.sim.powered = powered;
	kst_driver_t d = sim_driver(&g_b.sim);
	kst_session_init(&g_b.ses, &d);
	memset(&g_b.seek, 0, sizeof(g_b.seek));
}

/** @brief One step of the search, then 100 us. */
static KstSeek step(bool uiAlive = true) {
	KstSeek k = kstSeekStep(&g_b.seek, &g_b.ses, uiAlive, nowMs());
	g_b.sim.now_us += 100u;
	return k;
}

/**
 * @brief Step the search until it ends or the clock reaches @p untilUs.
 *        The servo is powered at @p powerAtUs; 0 leaves it as it is.
 */
static KstSeek run(uint32_t untilUs, uint32_t powerAtUs = 0, bool uiAlive = true) {
	KstSeek k = g_b.seek.state;
	while (g_b.sim.now_us < untilUs) {
		if (powerAtUs && g_b.sim.now_us >= powerAtUs) g_b.sim.powered = true;
		k = step(uiAlive);
		if (k != KstSeek::Searching) break;
	}
	return k;
}

/** @brief Frames of one kind in the simulator's log. */
static unsigned framesOf(sim_kind_t kind) {
	unsigned n = 0;
	for (unsigned i = 0; i < g_b.sim.n_frames && i < SIM_LOG_MAX; i++)
		if (g_b.sim.log[i].kind == kind) n++;
	return n;
}

/**
 * @brief A servo powered at @p powerAtMs is found by the end of the first
 *        attempt that starts after it.
 */
static void foundAfterPowerUp(uint32_t powerAtMs) {
	bench(false);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	uint32_t powerAtUs = powerAtMs * 1000u;
	KstSeek k = KstSeek::Searching;
	if (powerAtUs) k = run(powerAtUs);
	uint32_t attemptsAtPower = g_b.seek.attempts;
	g_b.sim.powered = true;
	k = run(powerAtUs + 1000000u);
	char msg[96];
	snprintf(msg, sizeof(msg), "powered at %u ms: found", (unsigned)powerAtMs);
	checkTrue(msg, k == KstSeek::Found && g_b.ses.in_mode);
	snprintf(msg, sizeof(msg), "powered at %u ms: on the next attempt", (unsigned)powerAtMs);
	checkTrue(msg, g_b.seek.attempts <= attemptsAtPower + 1);
	snprintf(msg, sizeof(msg), "powered at %u ms: result OK", (unsigned)powerAtMs);
	checkInt(msg, g_b.seek.result, KST_SES_OK);
}

static void testFound() {
	section("KST search: a servo powered up is found");
	bench(true);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	checkTrue("begun: searching, attempt 1",
	          g_b.seek.state == KstSeek::Searching && g_b.seek.attempts == 1);
	KstSeek k = run(1000000u);
	checkTrue("a powered servo: found on attempt 1",
	          k == KstSeek::Found && g_b.seek.attempts == 1);
	checkInt("one sync took it", framesOf(SIM_SYNC), 1);
	checkTrue("found again on every later step", step() == KstSeek::Found);

	foundAfterPowerUp(0);
	foundAfterPowerUp(3000);
	foundAfterPowerUp(29900);
	checkTrue("29.9 s is inside the search, not past it",
	          kstSeekMs(&g_b.seek, nowMs()) < KST_SEEK_TIMEOUT_MS + 119);
}

static void testAttempts() {
	section("KST search: attempts, gaps and the timeout");
	bench(false);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	KstSeek k = run(40000000u);
	checkTrue("no servo: timed out", k == KstSeek::TimedOut);
	checkInt("ended by no servo", g_b.seek.result, KST_SES_ERR_NO_SERVO);
	uint32_t ms = kstSeekMs(&g_b.seek, nowMs());
	checkTrue("at 30 s, inside the attempt that crossed it",
	          ms >= KST_SEEK_TIMEOUT_MS && ms < KST_SEEK_TIMEOUT_MS + 119);
	checkTrue("frozen after the end", kstSeekMs(&g_b.seek, nowMs() + 5000) == ms);
	checkTrue("about 250 attempts, 8.4 a second",
	          g_b.seek.attempts >= 250 && g_b.seek.attempts <= 260);
	checkInt("each attempt: read, sync, read",
	         (long)g_b.sim.n_frames, (long)g_b.seek.attempts * 3);
	checkTrue("no frame closer than 3 ms to the last window",
	          g_b.sim.min_gap_us >= KST_GAP_US);
	checkInt("no frame that is no frame", g_b.sim.n_bad_frames, 0);
	checkInt("silence is not a reply", g_b.seek.rx, KST_RX_NO_REPLY);

	// The attempt length, first read to first read: the 100 ms low, three
	// transactions and two 3 ms gaps.
	uint32_t shortest = 0xFFFFFFFFu, longest = 0;
	for (unsigned i = 3; i + 3 <= g_b.sim.n_frames && i < SIM_LOG_MAX; i += 3) {
		uint32_t len = g_b.sim.log[i].t_us - g_b.sim.log[i - 3].t_us;
		if (len < shortest) shortest = len;
		if (len > longest) longest = len;
	}
	checkTrue("an attempt is 116.6 ms or longer", shortest >= 116600);
	checkTrue("and 118.8 ms or shorter", longest <= 118800);
	checkTrue("the first read of each attempt 100 ms after its start",
	          g_b.sim.log[0].t_us >= KST_ENTRY_LOW_US);

	// The clock may wrap inside a search.
	g_msOffset = 0xFFFFFFFFu - 10000u;
	bench(false);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	k = run(40000000u);
	ms = kstSeekMs(&g_b.seek, nowMs());
	checkTrue("across a clock wrap: timed out at 30 s",
	          k == KstSeek::TimedOut && ms >= KST_SEEK_TIMEOUT_MS && ms < KST_SEEK_TIMEOUT_MS + 119);
	g_msOffset = 0;

	KstSeekState idle;
	memset(&idle, 0, sizeof(idle));
	checkInt("never begun: 0 ms", kstSeekMs(&idle, 12345), 0);
}

static void testStop() {
	section("KST search: STOP SEARCH and the heartbeat");
	bench(false);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	// Into the sync burst of attempt 2.
	run(117000u + 104000u);
	while (!g_b.sim.busy) step();
	unsigned frames = g_b.sim.n_frames;
	uint32_t doneUs = g_b.sim.done_us;
	kstSeekStop(&g_b.seek, &g_b.ses);
	checkTrue("a stop mid-frame: still searching while the frame runs",
	          step() == KstSeek::Searching && g_b.seek.stopping);
	KstSeek k = run(g_b.sim.now_us + 100000u);
	checkTrue("stopped once the frame and its window are over",
	          k == KstSeek::Stopped && g_b.sim.now_us >= doneUs && !g_b.sim.busy);
	checkInt("the frame was finished, no other started", (long)g_b.sim.n_frames, (long)frames);
	checkInt("ended as an abort", g_b.seek.result, KST_SES_ERR_ABORTED);
	uint32_t ms = kstSeekMs(&g_b.seek, nowMs());
	kstSeekStop(&g_b.seek, &g_b.ses);
	checkTrue("a stop after the end changes nothing",
	          g_b.seek.state == KstSeek::Stopped && kstSeekMs(&g_b.seek, nowMs() + 1000) == ms);

	// A lapse after the first read: the sync and the second read still go
	// out, then nothing.
	bench(false);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	run(103000u);
	checkInt("the first read is out", g_b.sim.n_frames, 1);
	k = run(1000000u, 0, false);
	checkTrue("a heartbeat lapse: the attempt ends, no other starts",
	          k == KstSeek::NoUi && g_b.seek.attempts == 1);
	checkInt("its sync and its last read went out", g_b.sim.n_frames, 3);
	run(2000000u);
	checkInt("and nothing after", g_b.sim.n_frames, 3);

	// The lapse is kept once seen, even if the heartbeat comes back.
	bench(false);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	step(false);
	k = run(1000000u);
	checkTrue("a lapse that ends inside the attempt still ends the search",
	          k == KstSeek::NoUi && g_b.seek.attempts == 1);
}

static void testFailures() {
	section("KST search: replies that are not a servo, and other results");
	bench(true);
	// Every reply of the servo at 27.5 us half-cells, outside 24.3 to 26.9 us.
	sim_add_fault(&g_b.sim, SIM_ANY, -1, 0, 100000, SIM_F_SHAPE_HALF, 27500);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	KstSeek k = run(1000000u);
	checkTrue("a reply at a bad rate: not found, still searching",
	          k == KstSeek::Searching && !g_b.ses.in_mode && g_b.seek.attempts > 1);
	checkInt("the refusal is kept: RATE", g_b.seek.rx, KST_RX_RATE);

	bench(true);
	sim_add_fault(&g_b.sim, SIM_ANY, -1, 0, 1, SIM_F_START_FAIL, 0);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	k = run(1000000u);
	checkTrue("the driver refuses a frame: failed", k == KstSeek::Failed);
	checkInt("with the driver's result", g_b.seek.result, KST_SES_ERR_DRIVER);
	checkInt("after one attempt", g_b.seek.attempts, 1);

	bench(true);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	run(1000000u);
	checkInt("a read starts", kst_session_read_all(&g_b.ses), KST_SES_BUSY);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	checkTrue("a search while an operation runs: failed at once, no attempt",
	          g_b.seek.state == KstSeek::Failed && g_b.seek.attempts == 0 &&
	          g_b.seek.result == KST_SES_ERR_BUSY);
	checkTrue("kstSeekStep() says so again", step() == KstSeek::Failed);
}

static void testLoss() {
	section("KST search: a lost servo");
	bench(true);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	run(1000000u);
	checkInt("found, then read", kst_session_read_all(&g_b.ses), KST_SES_BUSY);
	checkInt("the read", sim_run(&g_b.sim, &g_b.ses), KST_SES_OK);
	checkTrue("the first read is the backup", g_b.ses.has_backup);
	checkTrue("an operation that ends well is no loss",
	          !kstSeekLost(&g_b.seek, &g_b.ses, KST_SES_OK, true, nowMs()));

	g_b.sim.powered = false;
	g_b.sim.in_mode = false;
	kst_session_read_all(&g_b.ses);
	kst_ses_t r = sim_run(&g_b.sim, &g_b.ses);
	checkInt("the servo is gone: no servo", r, KST_SES_ERR_NO_SERVO);
	checkTrue("the session still says in the mode", g_b.ses.in_mode);
	checkTrue("a loss", kstSeekLost(&g_b.seek, &g_b.ses, r, true, nowMs()));
	checkTrue("power cycled: out of the mode, the backup dropped",
	          !g_b.ses.in_mode && !g_b.ses.has_backup);
	checkTrue("and a new search runs", g_b.seek.state == KstSeek::Searching &&
	                                   g_b.seek.attempts == 1);
	checkTrue("no second loss without the mode",
	          !kstSeekLost(&g_b.seek, &g_b.ses, r, true, nowMs()));
	KstSeek k = run(g_b.sim.now_us + 1000000u, g_b.sim.now_us + 200000u);
	checkTrue("powered again: found", k == KstSeek::Found);
	kst_session_read_all(&g_b.ses);
	checkInt("read", sim_run(&g_b.sim, &g_b.ses), KST_SES_OK);
	checkTrue("the new read is the new backup", g_b.ses.has_backup);

	// Without the heartbeat the loss starts no search.
	g_b.sim.powered = false;
	g_b.sim.in_mode = false;
	kst_session_read_all(&g_b.ses);
	r = sim_run(&g_b.sim, &g_b.ses);
	unsigned frames = g_b.sim.n_frames;
	checkTrue("a loss with the heartbeat lapsed",
	          kstSeekLost(&g_b.seek, &g_b.ses, r, false, nowMs()));
	checkTrue("ends NoUi, no attempt", g_b.seek.state == KstSeek::NoUi &&
	                                   g_b.seek.attempts == 0 && g_b.seek.noUi);
	checkInt("searched for 0 ms", kstSeekMs(&g_b.seek, nowMs() + 500), 0);
	run(g_b.sim.now_us + 500000u);
	checkInt("and sends nothing", (long)g_b.sim.n_frames, (long)frames);
	checkTrue("the session was power cycled", !g_b.ses.in_mode && !g_b.ses.has_backup);
}

void runKstSeekTests() {
	testFound();
	testAttempts();
	testStop();
	testFailures();
	testLoss();
}
