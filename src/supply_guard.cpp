/**
 * @file supply_guard.cpp
 * @brief The servo supply's protection. @see supply_guard.h
 */

#include "supply_guard.h"

#include <string.h>

bool supplyReadingAfter(const SenseSample *s, uint32_t sinceMs, uint32_t nowMs) {
	return senseFresh(s, nowMs) && (int32_t)(s->takenMs - sinceMs) > (int32_t)SUPPLY_READ_LAG_MS;
}

void supplyGuardArm(SupplyGuard *g, const SenseSample *s, uint32_t nowMs) {
	memset(g, 0, sizeof(*g));
	g->armed = true;
	g->armMs = nowMs;
	g->seq = s->seq;
}

void supplyGuardDisarm(SupplyGuard *g) { g->armed = false; }

/** @brief The size of a current, µA, either sign. */
static uint32_t magnitudeUa(int32_t ua) {
	return ua < 0 ? (uint32_t)(-(int64_t)ua) : (uint32_t)ua;
}

GuardCut supplyGuardStep(SupplyGuard *g, const SenseSample *s, const GuardRules *r,
                         uint32_t nowMs) {
	if (!g->armed) return GuardCut::None;
	if (r->lost) {
		if (s->state != (uint8_t)SenseState::Online) return GuardCut::Lost;
		// No reading for 250 ms, counted from the arm at the earliest: a gap
		// before START is not this run's. Signed: core0 can take a reading
		// in the millisecond after core1 read the clock.
		uint32_t ref = g->armMs;
		if (s->seq != 0 && (int32_t)(s->takenMs - g->armMs) > 0) ref = s->takenMs;
		if ((int32_t)(nowMs - ref) >= (int32_t)SENSE_STALE_MS) return GuardCut::Lost;
	}
	// A new reading: one not seen, and not stale.
	if (s->seq == g->seq || !senseFresh(s, nowMs)) return GuardCut::None;
	g->seq = s->seq;
	// The reading before a gap of 250 ms had gone stale: a run starts again.
	if (g->taken && s->takenMs - g->lastMs >= SENSE_STALE_MS) g->over = g->low = false;
	g->taken = true;
	g->lastMs = s->takenMs;

	uint32_t ua = magnitudeUa(s->currentUa);
	if ((s->flags & SENSE_FLAG_CLIPPED) || ua >= GUARD_HARD_MA * 1000u) return GuardCut::Hard;

	if (r->stall && ua > GUARD_STALL_MA * 1000u) {
		if (!g->over) {
			g->over = true;
			g->overMs = s->takenMs;
		}
		if (s->takenMs - g->overMs >= GUARD_STALL_MS) return GuardCut::Stall;
	} else {
		g->over = false;
	}

	if (!r->collapse) return GuardCut::None;
	if (!g->up) {
		g->up = supplyReadingAfter(s, g->armMs, nowMs) && s->busMv >= SUPPLY_ON_MIN_MV;
	} else if (s->busMv < GUARD_LOW_MV) {
		if (!g->low) {
			g->low = true;
			g->lowMs = s->takenMs;
		}
		if (s->takenMs - g->lowMs >= GUARD_LOW_MS) return GuardCut::Collapse;
	} else {
		g->low = false;
	}
	return GuardCut::None;
}
