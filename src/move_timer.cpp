/**
 * @file move_timer.cpp
 * @brief The servo tester's move timing. @see move_timer.h
 */

#include "move_timer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Every current the sensor reads, in mA, fits the SET line's `9.99 A`.
static_assert((uint32_t)INA219_SHUNT_END * INA219_SHUNT_UV / INA219_SHUNT_MOHM < 10000u,
              "a current of 10 A or more does not fit the SET line");

void moveTimerReset(MoveTimer *t, bool fresh) {
	MoveResult res = t->res;
	memset(t, 0, sizeof(*t));
	if (!fresh) t->res = res;
	// A measurement under way does not survive a bind.
	if (t->res.state == MoveState::Measuring) {
		t->res.state = MoveState::Void;
		t->res.why = MoveVoid::Ended;
	}
}

/** @brief A result that is Void for @p why. */
static MoveResult voidOf(MoveVoid why) {
	MoveResult r;
	memset(&r, 0, sizeof(r));
	r.state = MoveState::Void;
	r.why = why;
	return r;
}

/** @brief End a move under way as Void for @p why, and a hold. */
static void stop(MoveTimer *t, MoveVoid why) {
	if (t->timing) t->res = voidOf(why);
	t->timing = false;
	t->holding = false;
}

/** @brief How long a change of the pulses that is not timed to its end keeps the servo from rest, ms: the window. */
static uint32_t settleMs() { return servo_move_window_ms(MOVE_LAG_MS); }

/** @brief @p a A in whole mA, rounded. */
static int32_t milliamps(float a) { return (int32_t)lroundf(a * 1000.0f); }

/**
 * @brief The stored holding level at the pulses @p out, or null. An empty
 *        slot holds SIG's pulse 0, which @p out never has: SIG carries a
 *        pulse whenever the pulses run.
 */
static const MoveHold *holdAt(const MoveTimer *t, const uint16_t out[2]) {
	for (unsigned i = 0; i < MOVE_HOLDS; i++) {
		const MoveHold &h = t->holds[i];
		if (h.out[0] == out[0] && h.out[1] == out[1]) return &h;
	}
	return nullptr;
}

/** @brief Store @p a as the holding level at @p out: its own slot, an empty one, or the oldest. */
static void storeHold(MoveTimer *t, const uint16_t out[2], float a) {
	MoveHold *slot = &t->holds[0];
	for (unsigned i = 0; i < MOVE_HOLDS; i++) {
		MoveHold &h = t->holds[i];
		if (h.out[0] == out[0] && h.out[1] == out[1]) { slot = &h; break; }
		if (h.stamp < slot->stamp) slot = &h;
	}
	slot->out[0] = out[0];
	slot->out[1] = out[1];
	slot->a = a;
	slot->stamp = ++t->stores;
}

/**
 * @brief A step changes the pulses at @p nowMs: time it when the servo was
 *        at rest, else a Void result. The ring still holds the readings of
 *        the pulses before it.
 */
static void begin(MoveTimer *t, const MoveIn *in, uint32_t nowMs) {
	// A move under way keeps restMs, set at its command, ahead of it: the
	// window, or the stale rule, ends it before the servo counts as at rest.
	bool rest = (int32_t)(nowMs - t->restMs) >= (int32_t)MOVE_REST_MS;
	t->timing = false;
	t->holding = false;
	// The servo moves now; at rest again at the arrival, if one is seen.
	t->restMs = nowMs + settleMs();
	if (!rest) {
		t->res = voidOf(MoveVoid::NotAtRest);
		return;
	}
	// The readings taken in the 500 ms before the command. Each was fresh
	// on a pass at or before this one, so none is younger than 0 ms.
	uint32_t n = 0;
	float sum = 0.0f, sumSq = 0.0f;
	for (unsigned i = 0; i < t->ringN; i++) {
		uint32_t age = nowMs - t->ring[i].t;
		if (age > MOVE_REST_MS) continue;
		sum += t->ring[i].a;
		sumSq += t->ring[i].a * t->ring[i].a;
		n++;
	}
	if (n < MOVE_MIN_READINGS) {
		t->res = voidOf(MoveVoid::Few);
		return;
	}
	float level = sum / (float)n;
	if (milliamps(level) < -MOVE_REVERSED_MA) {
		t->res = voidOf(MoveVoid::Reversed);
		return;
	}
	// The level here is the holding level of the pulses the step leaves; at
	// the pulses the run started with, CENTRE, it is the idle level too.
	storeHold(t, t->out, level);
	if (t->out[0] == t->startOut[0] && t->out[1] == t->startOut[1]) {
		t->idleA = level;
		t->idleOk = true;
	}
	const MoveHold *to = holdAt(t, in->out);
	float ref = to ? to->a : (t->idleOk ? t->idleA : level);

	servo_move_cfg_t cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.cmd_t = nowMs;
	cfg.window_t = servo_move_window_ms(MOVE_LAG_MS);
	cfg.rise_a = level;
	cfg.ref_a = ref;
	cfg.move_a = servo_move_threshold_a(servo_move_noise_a(n, sum, sumSq));
	cfg.band_a = SERVO_MOVE_BAND_A;
	cfg.settle_n = 2u;
	cfg.filter_n = 1u;
	servo_move_begin(&t->mv, &cfg);
	t->timing = true;
	t->refMs = nowMs;
	memset(&t->res, 0, sizeof(t->res));
	t->res.state = MoveState::Measuring;
}

/** @brief The move is over at the reading taken at @p atMs. */
static void finish(MoveTimer *t, uint32_t atMs) {
	const servo_move_t *m = &t->mv;
	t->timing = false;
	MoveResult r;
	memset(&r, 0, sizeof(r));
	switch (m->state) {
		case SERVO_MOVE_ARRIVED:
		case SERVO_MOVE_SETTLED:
			// At rest from here: the next step's 500 ms are the hold.
			t->restMs = atMs;
			r.state = MoveState::Arrived;
			r.settled = m->state == SERVO_MOVE_SETTLED;
			r.ms = (uint16_t)(m->end_t - m->cfg.cmd_t);
			t->holding = true;
			t->holdFromMs = atMs;
			t->holdSum = 0.0f;
			t->holdN = 0;
			// The stale rule counts from this reading, the sample's newest.
			break;
		case SERVO_MOVE_LATE:
			r.state = MoveState::Late;
			break;
		default:
			r.state = MoveState::Unseen;
			break;
	}
	// Movement needs a reading: one with a value, or a clipped one, which
	// marks the move clipped.
	if (r.state != MoveState::Unseen && !m->clipped) {
		r.peakMa = milliamps(m->peak);
		r.peakOk = r.peakMa >= 0;
	}
	t->res = r;
}

/** @brief This pass's reading, if it is a new one: into the ring, the move and the hold. */
static void reading(MoveTimer *t, const SenseSample *s, uint32_t nowMs) {
	if (s->state != (uint8_t)SenseState::Online || s->seq == t->seq || !senseFresh(s, nowMs))
		return;
	t->seq = s->seq;
	float a = (float)s->currentUa / 1e6f;
	servo_move_clip_t clip = SERVO_MOVE_CLIP_NONE;
	if (s->flags & SENSE_FLAG_CLIPPED) clip = s->currentUa < 0 ? SERVO_MOVE_CLIP_LOW : SERVO_MOVE_CLIP_HIGH;

	if (clip == SERVO_MOVE_CLIP_NONE) {
		if (t->ringN == MOVE_RING_N) {
			memmove(&t->ring[0], &t->ring[1], (MOVE_RING_N - 1u) * sizeof(t->ring[0]));
			t->ringN--;
		}
		t->ring[t->ringN].t = s->takenMs;
		t->ring[t->ringN].a = a;
		t->ringN++;
	}

	if (t->timing) {
		servo_move_sample(&t->mv, s->takenMs, a, clip);
		if (servo_move_over(&t->mv)) finish(t, s->takenMs);
		return;
	}
	if (!t->holding) return;
	uint32_t since = s->takenMs - t->holdFromMs;
	if ((int32_t)since <= 0) return;
	if (since < MOVE_HOLD_MS) {
		if (clip != SERVO_MOVE_CLIP_NONE) {
			t->holding = false;   // a clipped reading has no value: no hold
			return;
		}
		t->holdSum += a;
		t->holdN++;
		return;
	}
	t->holding = false;
	if (t->holdN >= MOVE_MIN_READINGS) {
		t->res.holdMa = milliamps(t->holdSum / (float)t->holdN);
		t->res.holdOk = t->res.holdMa >= 0;
	}
}

void moveTimerStep(MoveTimer *t, const MoveIn *in, uint32_t nowMs) {
	// SIG carries a pulse whenever AUX does: START needs it bound.
	bool on = in->out[0] != 0;
	if (!on) {
		stop(t, MoveVoid::Ended);
		t->on = false;
		return;
	}
	// CURRENT SENSE at NONE: nothing is timed, and the result stays.
	bool sensed = in->sample.state != (uint8_t)SenseState::None;
	if (!sensed) t->timing = t->holding = false;
	bool changed = in->out[0] != t->out[0] || in->out[1] != t->out[1];
	if (!t->on) {
		// The pulses start: a new run.
		memset(&t->res, 0, sizeof(t->res));
		memset(t->holds, 0, sizeof(t->holds));
		t->stores = 0;
		t->idleOk = false;
		t->startOut[0] = in->out[0];
		t->startOut[1] = in->out[1];
		t->timing = t->holding = false;
		t->restMs = nowMs + settleMs();
		changed = true;
	} else if (sensed && in->step && in->slewed) {
		// MIN, CENTRE or MAX at SPEED below 100 %: the pulse slews from the
		// next pass on, untimed.
		stop(t, MoveVoid::Slewed);
		t->res = voidOf(MoveVoid::Slewed);
		t->restMs = nowMs + settleMs();
	} else if (changed) {
		if (sensed && in->step) {
			begin(t, in, nowMs);
		} else {
			// A drag, a sweep or a slew: the servo moves, untimed.
			stop(t, MoveVoid::Changed);
			t->restMs = nowMs + settleMs();
		}
	}
	t->on = true;
	t->out[0] = in->out[0];
	t->out[1] = in->out[1];

	// The stale rule: the sensor not Online, or no reading for 250 ms since
	// the command or the arrival at the earliest.
	if (t->timing || t->holding) {
		const SenseSample *s = &in->sample;
		uint32_t ref = t->refMs;
		// A move or a hold has had readings: the sample holds one.
		if ((int32_t)(s->takenMs - ref) > 0) ref = s->takenMs;
		if (s->state != (uint8_t)SenseState::Online ||
		    (int32_t)(nowMs - ref) >= (int32_t)SENSE_STALE_MS)
			stop(t, MoveVoid::Lost);
	}
	reading(t, &in->sample, nowMs);
}

/** @brief @p ma, at or above 0, as `1.21 A` to the nearest 10 mA; `-- A` when not @p ok. */
static void amps(char *buf, size_t n, bool ok, int32_t ma) {
	if (!ok) {
		snprintf(buf, n, "-- A");
		return;
	}
	unsigned cs = ((unsigned)ma + 5u) / 10u;
	snprintf(buf, n, "%u.%02u A", cs / 100u, cs % 100u);
}

bool moveTimerText(const MoveResult *r, char *buf, size_t n) {
	if (n == 0) return false;
	buf[0] = '\0';
	if (r->state == MoveState::None) return false;
	char time[16], peak[16], hold[16];
	switch (r->state) {
		case MoveState::Arrived: {
			unsigned cs = ((unsigned)r->ms + 5u) / 10u;
			snprintf(time, sizeof(time), "%u.%02u S", cs / 100u, cs % 100u);
			break;
		}
		case MoveState::Late:   snprintf(time, sizeof(time), "LATE"); break;
		case MoveState::Unseen: snprintf(time, sizeof(time), "UNSEEN"); break;
		default:                snprintf(time, sizeof(time), "-- S"); break;
	}
	amps(peak, sizeof(peak), r->peakOk, r->peakMa);
	amps(hold, sizeof(hold), r->holdOk, r->holdMa);
	char line[80];
	snprintf(line, sizeof(line), "MOVE %s  PEAK %s  HOLD %s", time, peak, hold);
	snprintf(buf, n, "%s", line);
	return true;
}
