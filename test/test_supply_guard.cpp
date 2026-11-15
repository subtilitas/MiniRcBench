/**
 * @file test_supply_guard.cpp
 * @brief The servo supply's protection, src/supply_guard.cpp, as pure
 *        functions: every cut at its boundary, the readings it takes and the
 *        ones it does not.
 */

#include <string.h>

#include "check.h"
#include "supply_guard.h"

/** @brief A sample: @p ua at @p mv, taken at @p takenMs, reading @p seq. */
static SenseSample sampleOf(uint32_t seq, uint32_t takenMs, int32_t ua, uint16_t mv = 5000,
                            SenseState state = SenseState::Online, uint8_t flags = 0) {
	SenseSample s;
	memset(&s, 0, sizeof(s));
	s.seq = seq;
	s.takenMs = takenMs;
	s.currentUa = ua;
	s.busMv = mv;
	s.flags = flags;
	s.state = (uint8_t)state;
	return s;
}

static const GuardRules HARD_ONLY = { false, false, false };
static const GuardRules STALL = { true, false, false };
static const GuardRules COLLAPSE = { false, true, false };
static const GuardRules LOST = { false, false, true };
static const GuardRules ALL = { true, true, true };

/**
 * @brief Feed @p g readings of @p ua and @p mv every @p stepMs from
 *        @p fromMs to @p toMs inclusive, each judged in its own ms, and
 *        return the first cut, with its reading's time in @p atMs.
 */
static GuardCut run(SupplyGuard *g, const GuardRules *r, uint32_t *seq, uint32_t fromMs,
                    uint32_t toMs, uint32_t stepMs, int32_t ua, uint16_t mv,
                    uint32_t *atMs = nullptr) {
	for (uint32_t t = fromMs; (int32_t)(t - toMs) <= 0; t += stepMs) {
		SenseSample s = sampleOf(++*seq, t, ua, mv);
		GuardCut c = supplyGuardStep(g, &s, r, t);
		if (c != GuardCut::None) {
			if (atMs) *atMs = t;
			return c;
		}
	}
	return GuardCut::None;
}

static void testReadingAfter() {
	section("Supply guard: the start-up reading's time rule");
	SenseSample s = sampleOf(1, 1018, 0);
	checkTrue("taken 18 ms after the command: not after", !supplyReadingAfter(&s, 1000, 1018));
	s.takenMs = 1019;
	checkTrue("19 ms after: after", supplyReadingAfter(&s, 1000, 1019));
	checkTrue("249 ms old: fresh", supplyReadingAfter(&s, 1000, 1019 + SENSE_STALE_MS - 1));
	checkTrue("250 ms old: stale, not after", !supplyReadingAfter(&s, 1000, 1019 + SENSE_STALE_MS));
	s = sampleOf(1, 0x00000010u, 0);
	checkTrue("across the wrap: 32 ms after", supplyReadingAfter(&s, 0xFFFFFFF0u, 0x00000011u));
	s.seq = 0;
	checkTrue("no reading yet: never", !supplyReadingAfter(&s, 0xFFFFFFF0u, 0x00000011u));
}

static void testArm() {
	section("Supply guard: armed, disarmed, the reading at the arm");
	SupplyGuard g;
	memset(&g, 0xA5, sizeof(g));
	SenseSample big = sampleOf(7, 990, 3200000, 5000, SenseState::Online, SENSE_FLAG_CLIPPED);
	supplyGuardArm(&g, &big, 1000);
	checkTrue("armed at 1000, the sample's seq taken as seen",
	          g.armed && g.armMs == 1000 && g.seq == 7 && !g.taken && !g.up && !g.over && !g.low);
	checkTrue("the clipped reading at the arm: not new, no cut",
	          supplyGuardStep(&g, &big, &ALL, 1001) == GuardCut::None);
	supplyGuardDisarm(&g);
	SenseSample next = sampleOf(8, 1010, 3200000);
	checkTrue("disarmed: a new 3.2 A reading cuts nothing",
	          supplyGuardStep(&g, &next, &ALL, 1011) == GuardCut::None && !g.armed);
	SenseSample gone = sampleOf(8, 0, 0, 0, SenseState::Offline);
	checkTrue("disarmed: a lost sensor cuts nothing",
	          supplyGuardStep(&g, &gone, &ALL, 5000) == GuardCut::None);
	supplyGuardArm(&g, &big, 2000);
	checkTrue("armed again: the same reading is not new",
	          supplyGuardStep(&g, &big, &HARD_ONLY, 2001) == GuardCut::None);
	SenseSample none = sampleOf(0, 0, 3200000);
	supplyGuardArm(&g, &none, 3000);
	checkTrue("seq 0, no reading yet: nothing taken",
	          supplyGuardStep(&g, &none, &HARD_ONLY, 3001) == GuardCut::None && !g.taken);
}

static void testHard() {
	section("Supply guard: the hard cut at 3.0 A, either sign, or clipped");
	SupplyGuard g;
	SenseSample at = sampleOf(1, 0, 0);
	struct { int32_t ua; uint8_t flags; GuardCut want; const char *what; } cases[] = {
		{ 2999999, 0, GuardCut::None, "2999.999 mA: no cut" },
		{ 3000000, 0, GuardCut::Hard, "3000 mA: cut" },
		{ -2999999, 0, GuardCut::None, "-2999.999 mA: no cut" },
		{ -3000000, 0, GuardCut::Hard, "-3000 mA, a reversed sensor: cut" },
		{ INT32_MIN, 0, GuardCut::Hard, "the most negative value: cut" },
		{ 100, SENSE_FLAG_CLIPPED, GuardCut::Hard, "clipped, whatever the value: cut" },
		{ 3200000, SENSE_FLAG_RESET, GuardCut::Hard, "3.2 A with the reset flag: cut" },
		{ 2999999, SENSE_FLAG_RESET, GuardCut::None, "the reset flag alone: no cut" },
	};
	for (auto &k : cases) {
		supplyGuardArm(&g, &at, 1000);
		SenseSample s = sampleOf(2, 1030, k.ua, 5000, SenseState::Online, k.flags);
		checkTrue(k.what, supplyGuardStep(&g, &s, &HARD_ONLY, 1031) == k.want);
	}
	// Every rule set has the hard cut, and it comes before the stall.
	supplyGuardArm(&g, &at, 1000);
	uint32_t seq = 1;
	run(&g, &ALL, &seq, 1025, 1400, 25, 1500000, 5000);
	SenseSample s = sampleOf(++seq, 1425, 3000000);
	checkTrue("3 A at the stall's 400 ms: Hard, not Stall",
	          supplyGuardStep(&g, &s, &ALL, 1425) == GuardCut::Hard);
	// Off with the sensor out of the rules: the hard cut still reads a reading.
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(2, 1030, 3000000, 5000, SenseState::Offline);
	checkTrue("a fresh 3 A reading from a part now offline, without the lost rule: Hard",
	          supplyGuardStep(&g, &s, &HARD_ONLY, 1031) == GuardCut::Hard);
}

static void testNewReadingsOnly() {
	section("Supply guard: only new, fresh readings count");
	SupplyGuard g;
	SenseSample at = sampleOf(1, 1000, 0);
	supplyGuardArm(&g, &at, 1000);
	SenseSample s = sampleOf(2, 1000, 3000000);
	checkTrue("a 3 A reading 250 ms old: stale, no cut",
	          supplyGuardStep(&g, &s, &HARD_ONLY, 1000 + SENSE_STALE_MS) == GuardCut::None);
	checkTrue("not taken either", !g.taken && g.seq == 1);
	s.takenMs = 1001;
	checkTrue("the same reading 249 ms old: fresh, Hard",
	          supplyGuardStep(&g, &s, &HARD_ONLY, 1001 + SENSE_STALE_MS - 1) == GuardCut::Hard);
	// One reading of 1.2 A seen on every pass for 600 ms: one reading, no stall.
	supplyGuardArm(&g, &at, 2000);
	s = sampleOf(2, 2010, 1200000);
	GuardCut c = GuardCut::None;
	for (uint32_t t = 2010; t < 2250 && c == GuardCut::None; t++) c = supplyGuardStep(&g, &s, &STALL, t);
	checkTrue("one 1.2 A reading seen for 240 ms: no stall", c == GuardCut::None);
	// A reading core0 took in the ms after core1 read the clock: stale by
	// the wrap of senseFresh(), so not taken until the next pass.
	supplyGuardArm(&g, &at, 3000);
	s = sampleOf(2, 3011, 3000000);
	checkTrue("taken 1 ms after now: not taken yet",
	          supplyGuardStep(&g, &s, &HARD_ONLY, 3010) == GuardCut::None && g.seq == 1);
	checkTrue("the next pass: Hard", supplyGuardStep(&g, &s, &HARD_ONLY, 3011) == GuardCut::Hard);
}

static void testStall() {
	section("Supply guard: the stall cut, above 1.0 A for 400 ms");
	SupplyGuard g;
	SenseSample at = sampleOf(1, 0, 0);
	uint32_t seq = 1, cutAt = 0;
	supplyGuardArm(&g, &at, 1000);
	checkTrue("1000 mA for 2 s: not above, no cut",
	          run(&g, &STALL, &seq, 1000, 3000, 25, 1000000, 5000) == GuardCut::None);
	supplyGuardArm(&g, &at, 1000);
	checkTrue("1000.1 mA from 1000 ms: cut by its reading at 1400 ms",
	          run(&g, &STALL, &seq, 1000, 3000, 25, 1000100, 5000, &cutAt) == GuardCut::Stall &&
	          cutAt == 1400);
	supplyGuardArm(&g, &at, 1000);
	checkTrue("above for 399 ms: no cut",
	          run(&g, &STALL, &seq, 1000, 1399, 1, 1500000, 5000) == GuardCut::None);
	SenseSample s = sampleOf(++seq, 1400, 1500000);
	checkTrue("400 ms: Stall", supplyGuardStep(&g, &s, &STALL, 1400) == GuardCut::Stall);
	supplyGuardArm(&g, &at, 1000);
	checkTrue("-1.5 A, either sign: Stall at 400 ms",
	          run(&g, &STALL, &seq, 1000, 2000, 25, -1500000, 5000, &cutAt) == GuardCut::Stall &&
	          cutAt == 1400);
	supplyGuardArm(&g, &at, 1000);
	checkTrue("without the stall rule: no cut in 2 s",
	          run(&g, &COLLAPSE, &seq, 1000, 3000, 25, 2900000, 5000) == GuardCut::None);

	// A reading at or below 1.0 A between ends the run.
	supplyGuardArm(&g, &at, 1000);
	run(&g, &STALL, &seq, 1000, 1375, 25, 1500000, 5000);
	s = sampleOf(++seq, 1390, 1000000);
	checkTrue("one reading of 1000 mA at 390 ms", supplyGuardStep(&g, &s, &STALL, 1390) == GuardCut::None);
	checkTrue("the run starts again: no cut until 400 ms after 1400",
	          run(&g, &STALL, &seq, 1400, 1799, 1, 1500000, 5000) == GuardCut::None);
	s = sampleOf(++seq, 1800, 1500000);
	checkTrue("800 ms after the first: Stall", supplyGuardStep(&g, &s, &STALL, 1800) == GuardCut::Stall);

	// A gap: the reading before it had gone stale.
	supplyGuardArm(&g, &at, 1000);
	run(&g, &STALL, &seq, 1000, 1100, 25, 1500000, 5000);
	s = sampleOf(++seq, 1349, 1500000);
	checkTrue("next reading 249 ms later: the run goes on, no cut",
	          supplyGuardStep(&g, &s, &STALL, 1349) == GuardCut::None);
	s = sampleOf(++seq, 1400, 1500000);
	checkTrue("then at 400 ms: Stall", supplyGuardStep(&g, &s, &STALL, 1400) == GuardCut::Stall);
	supplyGuardArm(&g, &at, 1000);
	run(&g, &STALL, &seq, 1000, 1100, 25, 1500000, 5000);
	s = sampleOf(++seq, 1350, 1500000);
	checkTrue("next reading 250 ms later: no cut",
	          supplyGuardStep(&g, &s, &STALL, 1350) == GuardCut::None);
	s = sampleOf(++seq, 1400, 1500000);
	checkTrue("400 ms after the first: the run began again at 1350, no cut",
	          supplyGuardStep(&g, &s, &STALL, 1400) == GuardCut::None);
	checkTrue("cut at 400 ms after 1350",
	          run(&g, &STALL, &seq, 1425, 1800, 25, 1500000, 5000, &cutAt) == GuardCut::Stall &&
	          cutAt == 1750);

	// Across the clock's wrap.
	const uint32_t t0 = 0xFFFFFF00u;
	supplyGuardArm(&g, &at, t0);
	checkTrue("across the wrap: Stall at 400 ms",
	          run(&g, &STALL, &seq, t0, t0 + 1000, 25, 1500000, 5000, &cutAt) == GuardCut::Stall &&
	          cutAt == t0 + 400);
}

static void testCollapse() {
	section("Supply guard: the collapse cut, under 4.0 V for 1000 ms after start-up");
	SupplyGuard g;
	SenseSample at = sampleOf(1, 990, 0, 0);
	uint32_t seq = 1, cutAt = 0;
	supplyGuardArm(&g, &at, 1000);
	checkTrue("never up: 3.0 V for 3 s cuts nothing",
	          run(&g, &COLLAPSE, &seq, 1000, 4000, 25, 0, 3000) == GuardCut::None && !g.up);
	supplyGuardArm(&g, &at, 1000);
	SenseSample s = sampleOf(++seq, 1018, 0, 5000);
	supplyGuardStep(&g, &s, &COLLAPSE, 1018);
	checkTrue("5 V taken 18 ms after the arm: not the start-up reading", !g.up);
	s = sampleOf(++seq, 1030, 0, 4499);
	supplyGuardStep(&g, &s, &COLLAPSE, 1030);
	checkTrue("4499 mV: not up", !g.up);
	s = sampleOf(++seq, 1040, 0, 4500);
	supplyGuardStep(&g, &s, &COLLAPSE, 1040);
	checkTrue("4500 mV taken 40 ms after: up", g.up);
	checkTrue("4000 mV for 3 s: no cut",
	          run(&g, &COLLAPSE, &seq, 1050, 4050, 25, 0, 4000) == GuardCut::None);
	checkTrue("3999 mV for 999 ms: no cut",
	          run(&g, &COLLAPSE, &seq, 5000, 5999, 1, 0, 3999) == GuardCut::None);
	s = sampleOf(++seq, 6000, 0, 3999);
	checkTrue("1000 ms: Collapse", supplyGuardStep(&g, &s, &COLLAPSE, 6000) == GuardCut::Collapse);

	// A reading at 4.0 V between ends the run.
	supplyGuardArm(&g, &at, 1000);
	run(&g, &COLLAPSE, &seq, 1100, 1100, 25, 0, 5000);
	run(&g, &COLLAPSE, &seq, 2000, 2975, 25, 0, 3000);
	s = sampleOf(++seq, 2990, 0, 4000);
	supplyGuardStep(&g, &s, &COLLAPSE, 2990);
	checkTrue("4000 mV at 990 ms: the run starts again",
	          run(&g, &COLLAPSE, &seq, 3000, 3999, 1, 0, 3000) == GuardCut::None);
	checkTrue("cut 1000 ms after 3000",
	          run(&g, &COLLAPSE, &seq, 4000, 4100, 25, 0, 3000, &cutAt) == GuardCut::Collapse &&
	          cutAt == 4000);
	// A gap of 250 ms starts the run again.
	supplyGuardArm(&g, &at, 1000);
	run(&g, &COLLAPSE, &seq, 1100, 1100, 25, 0, 5000);
	run(&g, &COLLAPSE, &seq, 2000, 2700, 25, 0, 3000);
	checkTrue("no reading from 2700 to 2950, then low: no cut at 2000 + 1000",
	          run(&g, &COLLAPSE, &seq, 2950, 3925, 25, 0, 3000) == GuardCut::None);
	checkTrue("cut 1000 ms after 2950",
	          run(&g, &COLLAPSE, &seq, 3950, 4000, 25, 0, 3000, &cutAt) == GuardCut::Collapse &&
	          cutAt == 3950);
	// Without the rule nothing, after start-up too.
	supplyGuardArm(&g, &at, 1000);
	run(&g, &STALL, &seq, 1100, 1100, 25, 0, 5000);
	checkTrue("without the collapse rule: 0 V for 3 s cuts nothing",
	          run(&g, &STALL, &seq, 1200, 4200, 25, 0, 0) == GuardCut::None && !g.up);
	// The current's rules run on the same readings.
	supplyGuardArm(&g, &at, 1000);
	run(&g, &ALL, &seq, 1100, 1100, 25, 0, 5000);
	checkTrue("3.2 V at 1.5 A: the stall comes first, at 400 ms",
	          run(&g, &ALL, &seq, 1200, 3000, 25, 1500000, 3200, &cutAt) == GuardCut::Stall &&
	          cutAt == 1600);
}

static void testLost() {
	section("Supply guard: the sensor-lost cut");
	SupplyGuard g;
	SenseSample at = sampleOf(1, 900, 0);
	const SenseState gone[] = { SenseState::Absent, SenseState::Offline, SenseState::Stuck,
	                            SenseState::None };
	for (SenseState st : gone) {
		supplyGuardArm(&g, &at, 1000);
		SenseSample s = sampleOf(1, 900, 0, 5000, st);
		checkTrue("not online: Lost", supplyGuardStep(&g, &s, &LOST, 1001) == GuardCut::Lost);
	}
	supplyGuardArm(&g, &at, 1000);
	SenseSample s = sampleOf(1, 900, 0, 5000, SenseState::Offline);
	checkTrue("not online without the rule: no cut", supplyGuardStep(&g, &s, &STALL, 1001) == GuardCut::None);

	// No reading, counted from the arm: the reading before it is 100 ms old.
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(1, 900, 0);
	checkTrue("249 ms after the arm: no cut", supplyGuardStep(&g, &s, &LOST, 1249) == GuardCut::None);
	checkTrue("250 ms: Lost", supplyGuardStep(&g, &s, &LOST, 1250) == GuardCut::Lost);
	// Counted from the last reading after the arm.
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(2, 1100, 0);
	supplyGuardStep(&g, &s, &LOST, 1100);
	checkTrue("249 ms after the last reading: no cut",
	          supplyGuardStep(&g, &s, &LOST, 1349) == GuardCut::None);
	checkTrue("250 ms: Lost", supplyGuardStep(&g, &s, &LOST, 1350) == GuardCut::Lost);
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(2, 1000, 0);
	checkTrue("a reading in the arm's ms: counted from the arm, 250 ms",
	          supplyGuardStep(&g, &s, &LOST, 1249) == GuardCut::None &&
	          supplyGuardStep(&g, &s, &LOST, 1250) == GuardCut::Lost);
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(2, 1001, 0);
	checkTrue("a reading 1 ms after the arm: counted from it",
	          supplyGuardStep(&g, &s, &LOST, 1250) == GuardCut::None &&
	          supplyGuardStep(&g, &s, &LOST, 1251) == GuardCut::Lost);
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(2, 1011, 0);
	checkTrue("a reading core0 took 1 ms after core1's clock: no cut",
	          supplyGuardStep(&g, &s, &LOST, 1010) == GuardCut::None);
	SenseSample none = sampleOf(0, 0, 0);
	supplyGuardArm(&g, &none, 1000);
	none.takenMs = 5000;
	checkTrue("seq 0: counted from the arm whatever takenMs says",
	          supplyGuardStep(&g, &none, &LOST, 1249) == GuardCut::None &&
	          supplyGuardStep(&g, &none, &LOST, 1250) == GuardCut::Lost);
	// Readings every 25 ms keep it: no cut in 3 s, across the wrap.
	const uint32_t t0 = 0xFFFFFF00u;
	supplyGuardArm(&g, &at, t0);
	uint32_t seq = 1;
	checkTrue("readings every 25 ms across the wrap: no cut",
	          run(&g, &LOST, &seq, t0 + 25, t0 + 3000, 25, 200000, 5000) == GuardCut::None);
	// Lost comes before a reading's cuts.
	supplyGuardArm(&g, &at, 1000);
	s = sampleOf(2, 1010, 3200000, 5000, SenseState::Offline);
	checkTrue("3.2 A from a part now offline: Lost first",
	          supplyGuardStep(&g, &s, &ALL, 1011) == GuardCut::Lost);
}

void runSupplyGuardTests() {
	testReadingAfter();
	testArm();
	testHard();
	testNewReadingsOnly();
	testStall();
	testCollapse();
	testLost();
}
