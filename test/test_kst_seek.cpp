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
#include "kst_supply_sim.h"

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
	checkTrue("about 258 attempts, 8.6 a second",
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

	// A stop while the sync is on the wire to a servo that answers it: the
	// session takes the abort first, the servo is in the mode, and the next
	// search's first read finds it.
	bench(true);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	while (!(g_b.sim.busy && g_b.sim.n_frames == 2)) step();
	checkInt("the sync is on the wire", g_b.sim.log[1].kind, SIM_SYNC);
	kstSeekStop(&g_b.seek, &g_b.ses);
	k = run(g_b.sim.now_us + 100000u);
	checkTrue("a stop during a sync the servo answers: stopped, aborted",
	          k == KstSeek::Stopped && g_b.seek.result == KST_SES_ERR_ABORTED);
	checkTrue("the servo is in the mode, the session does not know it",
	          g_b.sim.in_mode && !g_b.ses.in_mode);
	checkInt("no frame after the sync", g_b.sim.n_frames, 2);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	k = run(g_b.sim.now_us + 1000000u);
	checkTrue("RETRY: found by its first read, no sync",
	          k == KstSeek::Found && g_b.sim.n_frames == 3 && g_b.sim.n_syncs == 1);

	// A stop during a frame whose capture never comes: the session ends the
	// attempt on the driver's slack, and the search names that result.
	bench(true);
	sim_add_fault(&g_b.sim, SIM_SYNC, -1, 0, 1, SIM_F_STALL, 0);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	while (!(g_b.sim.busy && g_b.sim.n_frames == 2)) step();
	kstSeekStop(&g_b.seek, &g_b.ses);
	k = run(g_b.sim.now_us + 100000u);
	checkTrue("a stop during a frame that never finishes: failed, not stopped",
	          k == KstSeek::Failed && g_b.seek.result == KST_SES_ERR_DRIVER);
	checkInt("and no attempt after it", g_b.seek.attempts, 1);

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
	g_b.sim.powered = false;
	k = run(g_b.sim.now_us + 1000000u);
	checkTrue("then silence for 1 s: still searching", k == KstSeek::Searching);
	checkInt("and the refusal is kept over the silence", g_b.seek.rx, KST_RX_RATE);

	// A new search does not take the refusal an earlier one left in the
	// session's last reply.
	bench(true);
	sim_add_fault(&g_b.sim, SIM_ANY, -1, 0, 100000, SIM_F_SHAPE_HALF, 27500);
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	run(400000u);
	kstSeekStop(&g_b.seek, &g_b.ses);
	run(g_b.sim.now_us + 100000u);
	checkTrue("stopped with a refused reply last",
	          g_b.seek.state == KstSeek::Stopped && g_b.ses.last_reply.result == KST_RX_RATE);
	g_b.sim.powered = false;
	g_b.sim.n_faults = 0;
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs());
	step();
	checkInt("RETRY against silence: no refusal after its first step", g_b.seek.rx, KST_RX_NO_REPLY);
	run(g_b.sim.now_us + 1000000u);
	checkInt("nor after 1 s of silence", g_b.seek.rx, KST_RX_NO_REPLY);

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

// ---------------------------------------------------------------------------
// the switched supply
// ---------------------------------------------------------------------------

static SupplySim g_sup;

/** @brief A switched search on a servo with no supply of its own. */
static void switchedBench(uint32_t startupUs, uint32_t windowUs, int32_t delayMs = 0) {
	bench(false);
	supplySimInit(&g_sup, &g_b.sim, g_b.sim.now_us);
	g_sup.startupUs = startupUs;
	g_sup.windowUs = windowUs;
	kstSeekBegin(&g_b.seek, &g_b.ses, nowMs(), true);
	g_b.seek.delayMs = delayMs;
}

/** @brief One switched step: the search, the supply it wants, then 100 us. */
static KstSeek switchedStep(bool uiAlive = true) {
	supplySimUpdate(&g_sup, g_b.seek.supply, g_b.sim.now_us);
	KstSeek k = kstSeekStep(&g_b.seek, &g_b.ses, uiAlive, nowMs());
	supplySimUpdate(&g_sup, g_b.seek.supply, g_b.sim.now_us);
	g_b.sim.now_us += 100u;
	return k;
}

static KstSeek switchedRun(uint32_t untilUs, bool uiAlive = true) {
	KstSeek k = g_b.seek.state;
	while (g_b.sim.now_us < untilUs) {
		k = switchedStep(uiAlive);
		if (k != KstSeek::Searching) break;
	}
	return k;
}

/** @brief The first frame at or after @p fromUs, or UINT32_MAX. */
static uint32_t firstFrameFrom(uint32_t fromUs) {
	for (unsigned i = 0; i < g_b.sim.n_frames && i < SIM_LOG_MAX; i++)
		if (g_b.sim.log[i].t_us >= fromUs) return g_b.sim.log[i].t_us;
	return UINT32_MAX;
}

static void testSwitchedFound() {
	section("KST search, switched: a window that covers the first sync");
	switchedBench(5000, 300000);
	checkTrue("begun: supply off, no attempt yet",
	          !g_b.seek.supply && g_b.seek.attempts == 0 && g_b.seek.phase == KstSupplyPhase::Off);
	switchedRun(1999000u);
	checkTrue("1999 ms: still off, no frame", !g_b.seek.supply && g_b.sim.n_frames == 0);
	KstSeek k = switchedRun(5000000u);
	checkTrue("found on the first power-up, the supply on",
	          k == KstSeek::Found && g_b.seek.attempts == 1 && g_b.seek.supply && g_b.ses.in_mode);
	checkTrue("switched on 2000 ms after the off command",
	          g_sup.switchOns == 1 && g_sup.lastOnUs >= 2000000u && g_sup.lastOnUs < 2000200u);
	checkTrue("the first read about 100 ms after the switch-on",
	          firstFrameFrom(0) - g_sup.lastOnUs >= KST_ENTRY_LOW_US &&
	          firstFrameFrom(0) - g_sup.lastOnUs < KST_ENTRY_LOW_US + 1000u);
	checkInt("off-phase time left after the find", kstSeekOffLeftMs(&g_b.seek, nowMs()), 0);
}

static void testSwitchedTimeout() {
	section("KST search, switched: a window that closes before the first sync");
	// The sync goes out about 106 ms after the switch-on; a 50 ms window
	// never sees one.
	switchedBench(0, 50000);
	KstSeek k = switchedRun(40000000u);
	checkTrue("timed out", k == KstSeek::TimedOut);
	checkInt("14 power-ups in 30 s at a delay of 0 ms", g_b.seek.attempts, 14);
	checkInt("one switch-on each", g_sup.switchOns, 14);
	checkTrue("the supply off at the end", !g_b.seek.supply && !g_sup.level);
	// The search counts whole milliseconds: an off phase is 2000 ms on its
	// clock, at least 1999 ms in microseconds.
	checkTrue("off for 2000 ms before every switch-on, to the millisecond",
	          g_sup.minOffUs >= 1999000u);
	uint32_t ms = kstSeekMs(&g_b.seek, nowMs());
	checkTrue("ended at 30 s, in an off phase", ms >= KST_SEEK_TIMEOUT_MS && ms < KST_SEEK_TIMEOUT_MS + 2);
	checkTrue("no frame outside an on phase", g_b.sim.n_frames == 14u * 3u);
}

static void testSwitchedDelay() {
	section("KST search, switched: the entry delay");
	// -40 ms: the entry starts first, the switch-on 40 ms into its low.
	switchedBench(20000, 0, -40);
	KstSeek k = switchedRun(5000000u);
	checkTrue("a negative delay: found", k == KstSeek::Found && g_b.seek.attempts == 1);
	uint32_t entryUs = 2000000u;
	checkTrue("switched on about 40 ms into the entry",
	          g_sup.lastOnUs >= entryUs + 40000u && g_sup.lastOnUs < entryUs + 40200u);
	checkTrue("no frame before the switch-on plus the start-up time",
	          firstFrameFrom(0) >= g_sup.lastOnUs + g_sup.startupUs);

	// +300 ms: the entry waits after the switch-on.
	switchedBench(0, 0, 300);
	k = switchedRun(5000000u);
	checkTrue("a positive delay: found", k == KstSeek::Found);
	checkTrue("the first read 300 ms plus the low after the switch-on",
	          firstFrameFrom(0) - g_sup.lastOnUs >= 300000u + KST_ENTRY_LOW_US);
}

static void testSwitchedStops() {
	section("KST search, switched: a stop, a lapse and the timeout in a wait");
	switchedBench(0, 0);
	switchedRun(1000000u);
	kstSeekStop(&g_b.seek, &g_b.ses);
	checkTrue("STOP in the off phase: Stopped at once, never switched on",
	          switchedStep() == KstSeek::Stopped && g_sup.switchOns == 0 && !g_b.seek.supply);

	switchedBench(0, 0);
	switchedRun(1500000u);
	checkTrue("a lapse in the off phase: NoUi at once, never switched on",
	          switchedStep(false) == KstSeek::NoUi && g_sup.switchOns == 0);

	switchedBench(0, 0, 500);
	switchedRun(2100000u);
	checkTrue("a positive delay's wait: on", g_b.seek.supply && g_b.seek.phase == KstSupplyPhase::Delay);
	checkTrue("a lapse there: NoUi, the supply off",
	          switchedStep(false) == KstSeek::NoUi && !g_b.seek.supply);

	// A stop during the entry: the frame on the wire finishes, then off.
	switchedBench(0, 50000);
	switchedRun(2050000u);
	checkTrue("in the entry, the supply on", g_b.seek.phase == KstSupplyPhase::Entry && g_b.seek.supply);
	kstSeekStop(&g_b.seek, &g_b.ses);
	KstSeek k = switchedRun(3000000u);
	checkTrue("a stop in the entry: Stopped, the supply off", k == KstSeek::Stopped && !g_b.seek.supply);
	checkInt("no frame cut", g_b.sim.n_bad_frames, 0);

	// The off-phase time left, for the strip.
	switchedBench(0, 0);
	switchedRun(600000u);
	uint32_t left = kstSeekOffLeftMs(&g_b.seek, nowMs());
	checkTrue("0.6 s into the off phase: 1.4 s left", left >= 1399 && left <= 1400);
}

static void testSwitchedLoss() {
	section("KST search, switched: a loss starts a switched search");
	switchedBench(0, 0);
	switchedRun(5000000u);
	checkTrue("found", g_b.seek.state == KstSeek::Found && g_b.ses.in_mode);
	bool lost = kstSeekLost(&g_b.seek, &g_b.ses, KST_SES_ERR_NO_SERVO, true, nowMs(), true);
	checkTrue("lost: a new switched search, the supply wanted off",
	          lost && g_b.seek.state == KstSeek::Searching && g_b.seek.switched && !g_b.seek.supply &&
	          g_b.seek.phase == KstSupplyPhase::Off);
	kstSeekStalled(&g_b.seek, &g_b.ses, nowMs());
	checkTrue("stalled: NoUi, the supply off, the session power-cycled",
	          g_b.seek.state == KstSeek::NoUi && !g_b.seek.supply && !g_b.ses.in_mode);
}

void runKstSeekTests() {
	testFound();
	testAttempts();
	testStop();
	testFailures();
	testLoss();
	testSwitchedFound();
	testSwitchedTimeout();
	testSwitchedDelay();
	testSwitchedStops();
	testSwitchedLoss();
}
