/**
 * @file test_move_timer.cpp
 * @brief The servo tester's move timing, src/move_timer.cpp, as pure
 *        functions: when a step is timed, the level before and the holding
 *        level it hands rcbench's servo_move.c, every end of a move and of a
 *        hold at its boundary, and the SET line.
 */

#include <math.h>
#include <string.h>

#include "check.h"
#include "move_timer.h"
#include "servo_job.h"

/** @brief The timer, its clock, its readings and the pulses on SIG and AUX. */
struct Rig {
	MoveTimer t;
	uint32_t  seq;
	uint32_t  now;
	uint16_t  out[2];
	SenseState state;
	SenseSample last;   /**< The sample of the last pass: it stays until a new reading. */
};

/** @brief A rig at 1000 ms with no pass yet, SIG at CENTRE, the sensor Online. */
static void rigInit(Rig *r) {
	memset(r, 0, sizeof(*r));
	moveTimerReset(&r->t, true);
	r->now = 1000;
	r->out[0] = 1500;
	r->state = SenseState::Online;
}

/**
 * @brief One pass @p dt ms after the last, with a new reading of @p a taken
 *        in it when @p fresh, and @p step for a step command.
 */
static void tick(Rig *r, uint32_t dt, float a, bool step = false, bool fresh = true,
                 uint8_t flags = 0) {
	r->now += dt;
	if (fresh) {
		r->last.seq = ++r->seq;
		r->last.takenMs = r->now;
		r->last.currentUa = (int32_t)lroundf(a * 1e6f);
		r->last.busMv = 5000;
		r->last.flags = flags;
	}
	r->last.state = (uint8_t)r->state;
	MoveIn in;
	memset(&in, 0, sizeof(in));
	in.sample = r->last;
	in.out[0] = r->out[0];
	in.out[1] = r->out[1];
	in.step = step;
	moveTimerStep(&r->t, &in, r->now);
}

/** @brief Readings of @p a every @p every ms for @p ms. */
static void hold(Rig *r, uint32_t ms, float a, uint32_t every = 25) {
	for (uint32_t t = 0; t < ms; t += every) tick(r, every, a);
}

/** @brief A change of the pulses not timed to its end keeps the servo from rest this long, ms. */
static const uint32_t SETTLE = 3043;

/** @brief The pulses start with no reading, and @ref SETTLE ms later, still none: at rest from here. */
static void settle(Rig *r) {
	tick(r, 0, 0.0f, false, false);
	tick(r, SETTLE, 0.0f, false, false);
}

/**
 * @brief The pulses start, and from the servo's rest on readings of @p a:
 *        one at once, then @p ms of them. With 475 ms the next pass 25 ms
 *        on is 500 ms after the rest.
 */
static void startAt(Rig *r, float a, uint32_t ms = 475) {
	settle(r);
	tick(r, 0, a);
	hold(r, ms, a);
}

/** @brief A step of SIG to @p us 25 ms after the last pass, with a reading of @p a at it. */
static void stepTo(Rig *r, uint16_t us, float a, uint32_t dt = 25) {
	r->out[0] = us;
	tick(r, dt, a, true);
}

static bool stateIs(const Rig &r, MoveState s, MoveVoid why = MoveVoid::None) {
	return r.t.res.state == s && r.t.res.why == why;
}

/** @brief When a step is timed: the rest, the readings, the reversed sensor. */
static void testWhenTimed() {
	section("Move timing: when a step is timed");

	Rig r;
	rigInit(&r);
	checkTrue("nothing before a pass", stateIs(r, MoveState::None));
	tick(&r, 0, 0.10f);
	hold(&r, 3500, 0.10f);
	checkTrue("pulses at CENTRE: no step yet", stateIs(r, MoveState::None) && r.t.on);
	stepTo(&r, 2000, 0.10f, 42);
	checkTrue("a step 3542 ms after the pulses started: not at rest",
	          stateIs(r, MoveState::Void, MoveVoid::NotAtRest) && !r.t.timing);

	rigInit(&r);
	tick(&r, 0, 0.10f);
	hold(&r, 3500, 0.10f);
	stepTo(&r, 2000, 0.10f, 43);
	checkTrue("3543 ms: timed", stateIs(r, MoveState::Measuring) && r.t.timing);
	checkTrue("the level before and the destination's level: 0.10 A",
	          fabsf(r.t.mv.cfg.rise_a - 0.10f) < 1e-6f && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f);
	checkTrue("no noise: the threshold is 20 mA", fabsf(r.t.mv.cfg.move_a - SERVO_MOVE_MIN_A) < 1e-6f);
	checkInt("the window: 3000 ms and the lag", (int)r.t.mv.cfg.window_t, 3043);
	checkTrue("one reading at a time, settled over two, rcbench's band",
	          r.t.mv.cfg.filter_n == 1 && r.t.mv.cfg.settle_n == 2 &&
	          fabsf(r.t.mv.cfg.band_a - SERVO_MOVE_BAND_A) < 1e-6f);
	checkInt("the command at the pass", (int)r.t.mv.cfg.cmd_t, (int)r.now);

	// The noise: 0.09 and 0.11 A in turn, 10 mA, a threshold of 30 mA.
	rigInit(&r);
	settle(&r);
	for (int i = 0; i < 20; i++) tick(&r, 25, (i & 1) ? 0.09f : 0.11f);
	stepTo(&r, 2000, 0.10f);
	checkTrue("3 times the noise where above 20 mA", stateIs(r, MoveState::Measuring) &&
	          fabsf(r.t.mv.cfg.move_a - 0.030f) < 0.001f);

	// A step during a move: not at rest; the servo moves, so the next step
	// waits for the window.
	stepTo(&r, 1000, 0.9f, 600);
	checkTrue("a step while a move is under way, 600 ms on: not at rest",
	          stateIs(r, MoveState::Void, MoveVoid::NotAtRest));
	hold(&r, 3500, 0.10f);
	stepTo(&r, 1500, 0.10f, 42);
	checkTrue("then 3542 ms on: not at rest", stateIs(r, MoveState::Void, MoveVoid::NotAtRest));
	hold(&r, 3500, 0.10f);
	stepTo(&r, 1000, 0.10f, 43);
	checkTrue("3543 ms on: timed", stateIs(r, MoveState::Measuring));

	// SPEED below 100 %.
	rigInit(&r);
	startAt(&r, 0.10f);
	MoveIn in;
	memset(&in, 0, sizeof(in));
	r.now += 25;
	in.sample = r.last;
	in.out[0] = 1500;
	in.step = true;
	in.slewed = true;
	moveTimerStep(&r.t, &in, r.now);
	checkTrue("a step at SPEED below 100 %, the pulse not moved yet: a slew, not timed",
	          stateIs(r, MoveState::Void, MoveVoid::Slewed) && !r.t.timing);
	r.out[0] = 1510;
	tick(&r, 25, 0.10f);
	hold(&r, 3500, 0.10f);
	stepTo(&r, 1000, 0.10f, 42);
	checkTrue("then 3542 ms on: not at rest", stateIs(r, MoveState::Void, MoveVoid::NotAtRest));

	// A slewed step during a move ends it; one that moves nothing still
	// starts the wait for rest.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	memset(&in, 0, sizeof(in));
	r.now += 25;
	in.sample = r.last;
	in.out[0] = 2000;
	in.step = true;
	in.slewed = true;
	moveTimerStep(&r.t, &in, r.now);
	r.out[0] = 1990;
	tick(&r, 25, 0.9f);
	checkTrue("a slewed step during a move: void as a slew, the move over",
	          stateIs(r, MoveState::Void, MoveVoid::Slewed) && !r.t.timing);
	rigInit(&r);
	startAt(&r, 0.10f);
	hold(&r, 1000, 0.10f);
	memset(&in, 0, sizeof(in));
	r.now += 25;
	in.sample = r.last;
	in.out[0] = 1500;
	in.step = true;
	in.slewed = true;
	moveTimerStep(&r.t, &in, r.now);
	hold(&r, 3500, 0.10f);
	stepTo(&r, 2000, 0.10f, 42);
	checkTrue("a slewed step that moves nothing: 3542 ms on, not at rest",
	          stateIs(r, MoveState::Void, MoveVoid::NotAtRest));

	// Readings: 10 in the 500 ms before, 9.
	rigInit(&r);
	settle(&r);
	tick(&r, 0, 0.10f);
	hold(&r, 450, 0.10f, 50);   // 0 to 450 ms after the rest: 10 readings
	stepTo(&r, 2000, 0.10f, 50);
	checkTrue("10 readings, the oldest 500 ms old: timed", stateIs(r, MoveState::Measuring));
	rigInit(&r);
	settle(&r);
	tick(&r, 0, 0.10f);
	hold(&r, 450, 0.10f, 50);
	stepTo(&r, 2000, 0.10f, 51);
	checkTrue("the oldest 501 ms old: 9, too few", stateIs(r, MoveState::Void, MoveVoid::Few));
	rigInit(&r);
	settle(&r);
	hold(&r, 450, 0.10f, 50);   // 50 to 450 ms: 9 readings
	stepTo(&r, 2000, 0.10f, 50);
	checkTrue("9 readings: too few", stateIs(r, MoveState::Void, MoveVoid::Few));

	// A reversed sensor.
	rigInit(&r);
	startAt(&r, -0.021f);
	stepTo(&r, 2000, -0.021f);
	checkTrue("-21 mA before: reversed, not timed", stateIs(r, MoveState::Void, MoveVoid::Reversed));
	rigInit(&r);
	startAt(&r, -0.020f);
	stepTo(&r, 2000, -0.020f);
	checkTrue("-20 mA: timed", stateIs(r, MoveState::Measuring));

	// Only a step that changes the pulses.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 1500, 0.10f);
	checkTrue("a step to the pulse already there: nothing", stateIs(r, MoveState::None));
	r.out[0] = 2000;
	tick(&r, 25, 0.10f);
	checkTrue("a change without the step flag: nothing timed", stateIs(r, MoveState::None));

	// The ring keeps the newest 32 readings.
	rigInit(&r);
	settle(&r);
	tick(&r, 0, 0.5f);
	for (uint32_t t = 10; t <= 520; t += 10) tick(&r, 10, t < 210 ? 0.5f : 0.10f);
	stepTo(&r, 2000, 0.10f, 10);
	checkTrue("readings every 10 ms: the newest 32, all 0.10 A",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.rise_a - 0.10f) < 1e-6f);

	// AUX's pulse is part of the position.
	rigInit(&r);
	r.out[1] = 1500;
	startAt(&r, 0.10f);
	r.out[1] = 2000;
	tick(&r, 25, 0.10f, true);
	checkTrue("a step of AUX alone: timed", stateIs(r, MoveState::Measuring));
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	hold(&r, 500, 0.30f);       // B pushing at 2000
	stepTo(&r, 1000, 0.30f);    // A to 1000: leaves (1500, 2000), not CENTRE
	checkTrue("leaving SIG's CENTRE with AUX away: the idle level stays CENTRE's, 0.10 A",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f);

	rigInit(&r);
	r.out[1] = 1500;
	startAt(&r, 0.10f);
	stepTo(&r, 1000, 0.10f);    // to (1000, 1500)
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	hold(&r, 500, 0.40f);
	r.out[1] = 2000;
	tick(&r, 25, 0.40f, true);  // to (1000, 2000), never held
	checkTrue("(1000, 2000) is not (1000, 1500): the idle level, not 0.40 A",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f);

	// Readings while the sensor is not online, and a reading seen twice.
	rigInit(&r);
	settle(&r);
	r.state = SenseState::Absent;
	hold(&r, 475, 0.10f);
	r.state = SenseState::Online;
	stepTo(&r, 2000, 0.10f);
	checkTrue("20 readings while not online: none counts", stateIs(r, MoveState::Void, MoveVoid::Few));
	rigInit(&r);
	settle(&r);
	for (int i = 0; i < 9; i++) {
		tick(&r, 25, 0.10f);
		tick(&r, 25, 0.10f, false, false);
	}
	stepTo(&r, 2000, 0.10f, 50);
	checkTrue("9 readings, each seen twice: 9", stateIs(r, MoveState::Void, MoveVoid::Few));
	rigInit(&r);
	settle(&r);
	for (int i = 0; i < 20; i++) {   // each new, taken 250 ms before its pass
		r.now += 25;
		r.last.seq = ++r.seq;
		r.last.takenMs = r.now - 250;
		r.last.currentUa = 100000;
		r.last.state = (uint8_t)SenseState::Online;
		MoveIn in;
		memset(&in, 0, sizeof(in));
		in.sample = r.last;
		in.out[0] = r.out[0];
		moveTimerStep(&r.t, &in, r.now);
	}
	stepTo(&r, 2000, 0.10f);
	checkTrue("readings 250 ms old at their pass: stale, none counts",
	          stateIs(r, MoveState::Void, MoveVoid::Few));

	// A clipped reading has no value: not in the level before.
	rigInit(&r);
	settle(&r);
	tick(&r, 0, 3.2f, false, true, SENSE_FLAG_CLIPPED);
	hold(&r, 475, 0.10f);
	stepTo(&r, 2000, 0.10f);
	checkTrue("19 readings and a clipped one: the level of the 19",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.rise_a - 0.10f) < 1e-6f);
}

/** @brief One move from 0.10 A at rest, 0.90 A moving, back at 0.10 A at @p arriveDt after the step. */
static void moveOnce(Rig *r, uint32_t arriveDt = 325) {
	startAt(r, 0.10f);
	stepTo(r, 2000, 0.10f);
	for (uint32_t t = 25; t < arriveDt; t += 25) tick(r, 25, 0.90f);
	tick(r, 25, 0.10f);
}

/** @brief Arrival, the peak and the hold, at their boundaries. */
static void testArrivalAndHold() {
	section("Move timing: arrival, peak and hold");

	Rig r;
	rigInit(&r);
	moveOnce(&r);
	checkTrue("back at the level after passing it: arrived", stateIs(r, MoveState::Arrived) &&
	          !r.t.res.settled);
	checkInt("the command to the arrival, ms", r.t.res.ms, 325);
	checkTrue("the peak, 900 mA", r.t.res.peakOk && r.t.res.peakMa == 900);
	checkTrue("no hold yet", !r.t.res.holdOk && r.t.holding);
	hold(&r, 475, 0.12f);
	checkTrue("475 ms of the hold: not yet", !r.t.res.holdOk && r.t.holding);
	tick(&r, 25, 0.12f);
	checkTrue("the reading 500 ms after the arrival closes it: 120 mA",
	          r.t.res.holdOk && r.t.res.holdMa == 120 && !r.t.holding);
	tick(&r, 25, 0.50f);
	checkInt("then nothing changes it", r.t.res.holdMa, 120);

	// 10 and 9 readings in the hold.
	rigInit(&r);
	moveOnce(&r);
	hold(&r, 500, 0.12f, 50);   // 10 readings, the 10th at 500 ms closes
	checkTrue("9 readings before the close: no hold", !r.t.res.holdOk && !r.t.holding);
	rigInit(&r);
	moveOnce(&r);
	tick(&r, 25, 0.12f);
	hold(&r, 475, 0.12f, 50);   // 25, 75 ... 475: 10 readings; then 525 closes
	checkTrue("10 readings: the hold", r.t.res.holdOk && r.t.res.holdMa == 120);

	// A clipped reading in the hold.
	rigInit(&r);
	moveOnce(&r);
	tick(&r, 25, 3.2f, false, true, SENSE_FLAG_CLIPPED);
	hold(&r, 500, 0.12f);
	checkTrue("a clipped reading: no hold", r.t.res.state == MoveState::Arrived && !r.t.res.holdOk);

	// A hold cut short leaves nothing in the next one.
	rigInit(&r);
	moveOnce(&r);
	hold(&r, 100, 0.50f);       // 4 readings of the hold
	r.out[0] = 1500;
	tick(&r, 25, 0.50f);        // a drag ends it
	hold(&r, SETTLE + 500, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	hold(&r, 500, 0.12f);
	checkTrue("the next hold: its own readings, 120 mA", stateIs(r, MoveState::Arrived) &&
	          r.t.res.holdOk && r.t.res.holdMa == 120);

	// A negative hold or peak is not shown.
	rigInit(&r);
	moveOnce(&r);
	hold(&r, 500, -0.01f);
	checkTrue("a hold below 0: not shown", !r.t.res.holdOk);

	// A first visit to an end held harder than CENTRE: judged against the
	// idle level, it never comes back to it.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	checkTrue("the first step: the idle level, CENTRE's, is the level before",
	          r.t.idleOk && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f);
	tick(&r, 25, 0.90f);
	hold(&r, 3025, 0.50f);
	checkTrue("pushing on a stop at 0.50 A, a first visit: late", stateIs(r, MoveState::Late));
	hold(&r, 500, 0.50f);
	stepTo(&r, 1500, 0.50f);
	checkTrue("back to CENTRE: its level, 0.10 A, from the 500 ms before the first step",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f &&
	          fabsf(r.t.mv.cfg.rise_a - 0.50f) < 1e-6f);
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	checkTrue("arrived", stateIs(r, MoveState::Arrived));
	hold(&r, 500, 0.10f);
	stepTo(&r, 2000, 0.10f);
	checkTrue("to 2000 again: its level from the step that left it, 0.50 A",
	          fabsf(r.t.mv.cfg.ref_a - 0.50f) < 1e-6f && fabsf(r.t.mv.cfg.rise_a - 0.10f) < 1e-6f);
	uint32_t cmd = r.now;
	tick(&r, 25, 0.30f);
	tick(&r, 25, 0.30f);
	tick(&r, 25, 0.50f);
	checkTrue("one reading at the level: not yet", stateIs(r, MoveState::Measuring));
	tick(&r, 25, 0.50f);
	checkTrue("two in a row: settled, at the first", stateIs(r, MoveState::Arrived) &&
	          r.t.res.settled && r.t.res.ms == 75 && r.now - cmd == 100);
	checkTrue("the peak of the move: 500 mA", r.t.res.peakOk && r.t.res.peakMa == 500);
	// The hold counts from the reading that ended the move, the run's second.
	tick(&r, 25, 0.50f);
	hold(&r, 450, 0.50f, 50);   // 75 to 475 ms after it: with 25 ms, 10 readings
	tick(&r, 50, 0.50f);        // 525 ms closes
	checkTrue("settled: the hold from the run's second reading, 10 readings, 500 mA",
	          r.t.res.holdOk && r.t.res.holdMa == 500);

	// A position never left this run: the idle level.
	hold(&r, 500, 0.50f);
	stepTo(&r, 1000, 0.50f);
	checkTrue("to 1000, never left: the idle level",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f);

	// A position left twice: its newest level.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);    // CENTRE's level 0.10 A
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	hold(&r, 500, 0.10f);
	stepTo(&r, 1500, 0.10f);
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	hold(&r, 500, 0.20f);
	stepTo(&r, 2000, 0.20f);    // CENTRE's level now 0.20 A
	tick(&r, 25, 0.90f);
	tick(&r, 25, 0.10f);
	hold(&r, 500, 0.10f);
	stepTo(&r, 1500, 0.10f);
	checkTrue("back to CENTRE: its newest level, 0.20 A, in its own slot",
	          fabsf(r.t.mv.cfg.ref_a - 0.20f) < 1e-6f);

	// The idle level is the run's: with the run started away from the level
	// before, and none known, the level before.
	rigInit(&r);
	startAt(&r, 0.10f);
	r.out[0] = 1600;
	tick(&r, 25, 0.10f);   // a drag: nothing timed, nothing stored
	hold(&r, SETTLE + 475, 0.20f);
	stepTo(&r, 2000, 0.20f);
	checkTrue("no idle level yet: the level before", !r.t.idleOk &&
	          fabsf(r.t.mv.cfg.ref_a - 0.20f) < 1e-6f);

	// The levels are this run's: STOP and START forget them.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 25, 0.9f);
	tick(&r, 25, 0.1f);
	hold(&r, 500, 0.30f);
	stepTo(&r, 1500, 0.30f);
	r.out[0] = 0;
	tick(&r, 25, 0.0f);
	r.out[0] = 1500;
	startAt(&r, 0.20f);
	stepTo(&r, 2000, 0.20f);
	checkTrue("after STOP and START: 2000's 0.30 A forgotten, the new idle level",
	          fabsf(r.t.mv.cfg.ref_a - 0.20f) < 1e-6f);
	r.out[0] = 0;
	tick(&r, 25, 0.0f);
	r.out[0] = 1500;
	startAt(&r, 0.20f);
	r.out[0] = 1600;
	tick(&r, 25, 0.25f);   // a drag first: CENTRE is left untimed
	hold(&r, SETTLE + 500, 0.25f);
	stepTo(&r, 2000, 0.25f);
	checkTrue("and the idle level: none this run, the level before",
	          !r.t.idleOk && fabsf(r.t.mv.cfg.ref_a - 0.25f) < 1e-6f);

	// More positions than slots: the oldest goes. Position 1000 + 10 i holds
	// at 0.10 + 0.005 (i + 1) A; each move is unseen before the next step.
	rigInit(&r);
	startAt(&r, 0.10f);
	for (unsigned i = 1; i <= MOVE_HOLDS + 2; i++) {
		stepTo(&r, (uint16_t)(1000 + 10 * i), 0.10f + 0.005f * (float)i);
		hold(&r, 3600, 0.10f + 0.005f * (float)(i + 1));
	}
	checkInt("11 levels stored", (int)r.t.stores, (int)MOVE_HOLDS + 2);
	stepTo(&r, 1010, 0.16f);
	checkTrue("to 1010, the second stored: gone, the idle level",
	          stateIs(r, MoveState::Measuring) && fabsf(r.t.mv.cfg.ref_a - 0.10f) < 1e-6f);
	hold(&r, 3600, 0.16f);
	stepTo(&r, 1040, 0.16f);
	checkTrue("to 1040 after 13 stores: the 5th, kept, 0.125 A",
	          fabsf(r.t.mv.cfg.ref_a - 0.125f) < 1e-4f);
}

/** @brief The window: late and unseen at 3043 ms and not at 3042. */
static void testWindow() {
	section("Move timing: late and unseen");

	Rig r;
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	for (int i = 0; i < 120; i++) tick(&r, 25, 0.90f);   // to 3000 ms
	tick(&r, 42, 0.90f);
	checkTrue("moving at 3042 ms: under way", stateIs(r, MoveState::Measuring));
	tick(&r, 1, 0.90f);
	checkTrue("3043 ms: late", stateIs(r, MoveState::Late));
	checkTrue("late: the peak, no hold", r.t.res.peakOk && r.t.res.peakMa == 900 &&
	          !r.t.res.holdOk && !r.t.holding);

	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	for (int i = 0; i < 121; i++) tick(&r, 25, 0.10f);
	checkTrue("no movement at 3025 ms: under way", stateIs(r, MoveState::Measuring));
	tick(&r, 18, 0.10f);
	checkTrue("3043 ms: unseen, no peak", stateIs(r, MoveState::Unseen) && !r.t.res.peakOk);

	// No reading at or past the end: the move waits for one, then the
	// stale rule.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	for (int i = 0; i < 121; i++) tick(&r, 25, 0.10f);
	tick(&r, 100, 0.10f, false, false);
	checkTrue("no reading at 3125 ms: still under way", stateIs(r, MoveState::Measuring));
}

/** @brief The ends of a move other than its own: another pulse, STOP, a lost sensor, a clip. */
static void testVoid() {
	section("Move timing: a move or a hold ended from outside");

	Rig r;
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 25, 0.9f);
	r.out[0] = 1900;
	tick(&r, 25, 0.9f);
	checkTrue("a drag or a sweep changes the pulse: void", stateIs(r, MoveState::Void, MoveVoid::Changed));

	rigInit(&r);
	moveOnce(&r);
	hold(&r, 100, 0.12f);
	r.out[0] = 1900;
	tick(&r, 25, 0.12f);
	hold(&r, 500, 0.12f);
	checkTrue("a change in the hold: the time kept, no hold",
	          stateIs(r, MoveState::Arrived) && r.t.res.ms == 325 && !r.t.res.holdOk);

	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	r.out[0] = 0;
	tick(&r, 25, 0.9f);
	checkTrue("STOP, a trip or a cut: void", stateIs(r, MoveState::Void, MoveVoid::Ended) && !r.t.on);
	tick(&r, 25, 0.0f);
	checkTrue("kept while SAFE", stateIs(r, MoveState::Void, MoveVoid::Ended));
	r.out[0] = 1500;
	tick(&r, 25, 0.10f);
	checkTrue("the pulses start again: a new run, nothing", stateIs(r, MoveState::None));

	rigInit(&r);
	moveOnce(&r);
	r.out[0] = 0;
	tick(&r, 25, 0.0f);
	checkTrue("STOP in the hold: arrived, no hold", stateIs(r, MoveState::Arrived) &&
	          !r.t.res.holdOk && !r.t.holding);

	// The sensor.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	r.state = SenseState::Offline;
	tick(&r, 25, 0.9f);
	checkTrue("the sensor offline: void", stateIs(r, MoveState::Void, MoveVoid::Lost));

	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 100, 0.9f);
	tick(&r, 249, 0.9f, false, false);
	checkTrue("249 ms since the last reading: under way", stateIs(r, MoveState::Measuring));
	tick(&r, 1, 0.9f, false, false);
	checkTrue("250 ms: void", stateIs(r, MoveState::Void, MoveVoid::Lost));

	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	r.last.takenMs = r.now - 100;   // the step's reading taken before it
	tick(&r, 249, 0.9f, false, false);
	checkTrue("counted from the command at the earliest: 249 ms", stateIs(r, MoveState::Measuring));
	tick(&r, 1, 0.9f, false, false);
	checkTrue("250 ms after the command: void", stateIs(r, MoveState::Void, MoveVoid::Lost));

	rigInit(&r);
	moveOnce(&r);
	tick(&r, 250, 0.12f, false, false);
	checkTrue("no reading for 250 ms in the hold: no hold", !r.t.holding && !r.t.res.holdOk &&
	          stateIs(r, MoveState::Arrived));
	rigInit(&r);
	moveOnce(&r);
	tick(&r, 249, 0.12f, false, false);
	checkTrue("249 ms: the hold goes on", r.t.holding);

	// A clipped reading in the move.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 25, 3.2f, false, true, SENSE_FLAG_CLIPPED);
	tick(&r, 25, 0.9f);
	tick(&r, 25, 0.10f);
	checkTrue("a clipped reading: movement, arrival, no peak",
	          stateIs(r, MoveState::Arrived) && !r.t.res.peakOk);
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 25, -3.2f, false, true, SENSE_FLAG_CLIPPED);
	checkTrue("clipped low: below the level by its bound, movement", r.t.mv.state == SERVO_MOVE_MOVING);

	// A negative peak.
	rigInit(&r);
	r.out[0] = 2000;
	startAt(&r, -0.01f);
	stepTo(&r, 1500, -0.01f);
	tick(&r, 25, -0.04f);
	tick(&r, 25, -0.02f);
	checkTrue("the peak below 0: not shown", stateIs(r, MoveState::Arrived) && !r.t.res.peakOk);

	// A reading seen twice, and a stale one, are not readings.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	tick(&r, 25, 0.9f);
	tick(&r, 25, 0.9f);
	tick(&r, 25, 0.10f, false, false);   // the 0.9 A again
	checkTrue("the same reading twice: one", stateIs(r, MoveState::Measuring));

	// The bind.
	rigInit(&r);
	startAt(&r, 0.10f);
	stepTo(&r, 2000, 0.10f);
	moveTimerReset(&r.t, false);
	checkTrue("a bind ends a move under way", stateIs(r, MoveState::Void, MoveVoid::Ended) && !r.t.timing);
	rigInit(&r);
	moveOnce(&r);
	moveTimerReset(&r.t, false);
	checkTrue("a rebind keeps a result", stateIs(r, MoveState::Arrived) && r.t.res.ms == 325);
	moveTimerReset(&r.t, true);
	checkTrue("a new session drops it", stateIs(r, MoveState::None));
}

/** @brief The SET line. */
static void testText() {
	section("Move timing: the SET line");

	char b[MOVE_TEXT_MAX + 1];
	MoveResult m;
	memset(&m, 0, sizeof(m));
	checkTrue("none: no line", !moveTimerText(&m, b, sizeof(b)) && b[0] == '\0');
	m.state = MoveState::Measuring;
	checkTrue("under way", moveTimerText(&m, b, sizeof(b)));
	checkStr("under way: dashes", b, "MOVE -- S  PEAK -- A  HOLD -- A");
	m.state = MoveState::Arrived;
	m.ms = 425;
	m.peakOk = true;
	m.peakMa = 1215;
	moveTimerText(&m, b, sizeof(b));
	checkStr("arrived, the hold not in: 10 ms and 10 mA, rounded", b,
	         "MOVE 0.43 S  PEAK 1.22 A  HOLD -- A");
	m.ms = 424;
	m.peakMa = 1214;
	m.holdOk = true;
	m.holdMa = 5;
	moveTimerText(&m, b, sizeof(b));
	checkStr("and down", b, "MOVE 0.42 S  PEAK 1.21 A  HOLD 0.01 A");
	m.ms = 3042;
	m.peakMa = 3200;
	m.holdMa = 3200;
	moveTimerText(&m, b, sizeof(b));
	checkStr("the widest", b, "MOVE 3.04 S  PEAK 3.20 A  HOLD 3.20 A");
	checkInt("MOVE_TEXT_MAX characters", (int)strlen(b), MOVE_TEXT_MAX);
	m.state = MoveState::Late;
	m.holdOk = false;
	moveTimerText(&m, b, sizeof(b));
	checkStr("late", b, "MOVE LATE  PEAK 3.20 A  HOLD -- A");
	m.state = MoveState::Unseen;
	m.peakOk = false;
	moveTimerText(&m, b, sizeof(b));
	checkStr("unseen", b, "MOVE UNSEEN  PEAK -- A  HOLD -- A");
	m.state = MoveState::Void;
	moveTimerText(&m, b, sizeof(b));
	checkStr("void", b, "MOVE -- S  PEAK -- A  HOLD -- A");
	char small[8];
	moveTimerText(&m, small, sizeof(small));
	checkStr("a short buffer cuts the line", small, "MOVE --");
	checkTrue("no room: nothing", !moveTimerText(&m, small, 0));

	ServoCmd c;
	memset(&c, 0, sizeof(c));
	c.kind = ServoCmdKind::Position;
	c.step = true;
	checkTrue("MIN, CENTRE or MAX: a step", servoStepCmd(&c));
	c.step = false;
	checkTrue("a drag: not", !servoStepCmd(&c));
	c.step = true;
	c.kind = ServoCmdKind::Sweep;
	checkTrue("a sweep: not", !servoStepCmd(&c));
	c.kind = ServoCmdKind::Position;
	c.slewPerS = 400;
	checkTrue("at a lower SPEED: a step, which the timer takes as a slew", servoStepCmd(&c));
}

void runMoveTimerTests() {
	testWhenTimed();
	testArrivalAndHold();
	testWindow();
	testVoid();
	testText();
}
