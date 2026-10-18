/**
 * @file servo_model.cpp
 * @brief The servo tester's pure rules. @see servo_model.h
 */

#include "servo_model.h"

#include <math.h>

static const uint16_t STD_RATES[]  = { 50, 60, 100, 150, 200, 250, 300, 333 };
static const uint16_t CYC_RATES[]  = { 50, 120, 200, 333 };
static const uint16_t TAIL_RATES[] = { 200, 333, 560 };

#define RATES(a) (a), (uint8_t)(sizeof(a) / sizeof((a)[0]))

/** @brief rcbench's profile table, in ServoType order. */
static const ServoTypeInfo TYPES[SERVO_TYPE_COUNT] = {
	{ "STANDARD PWM",  1000, 1500, 2000, RATES(STD_RATES),  50,  0,   false },
	{ "NARROW 760",    660,  760,  860,  RATES(STD_RATES),  50,  0,   false },
	{ "WIDE",          800,  1500, 2200, RATES(STD_RATES),  50,  0,   false },
	{ "HELI CYCLIC",   820,  1520, 2220, RATES(CYC_RATES),  333, 333, true  },
	{ "HELI TAIL 760", 410,  760,  1110, RATES(TAIL_RATES), 560, 560, true  },
};

const ServoTypeInfo *servoTypeInfo(ServoType t) {
	unsigned i = (unsigned)t;
	return &TYPES[i < SERVO_TYPE_COUNT ? i : 0];
}

void servoProfileLoadType(ServoProfile *p, ServoType t) {
	const ServoTypeInfo *ti = servoTypeInfo(t);
	p->type     = (unsigned)t < SERVO_TYPE_COUNT ? t : ServoType::Standard;
	p->minUs    = ti->minUs;
	p->centreUs = ti->centreUs;
	p->maxUs    = ti->maxUs;
	p->rateHz   = ti->defaultHz;
}

void servoProfileDefault(ServoProfile *p) {
	p->servos = 1;
	p->reverseB = false;
	servoProfileLoadType(p, ServoType::Standard);
}

/** @brief The pause a type keeps after each pulse, us. */
static uint16_t pauseOf(ServoType t) {
	return servoTypeInfo(t)->heli ? SERVO_HELI_PAUSE_US : SERVO_PAUSE_US;
}

uint16_t servoMaxRateFor(ServoType t, uint16_t maxUs) {
	const ServoTypeInfo *ti = servoTypeInfo(t);
	uint32_t hz = 1000000u / ((uint32_t)maxUs + pauseOf(t));
	if (ti->maxHz != 0 && hz > ti->maxHz) hz = ti->maxHz;
	return (uint16_t)hz;
}

uint16_t servoMaxPulseFor(ServoType t, uint16_t rateHz) {
	if (rateHz == 0) return 0;
	uint32_t period = 1000000u / rateHz;
	uint32_t pause = pauseOf(t);
	uint32_t top = period > pause ? period - pause : 0;
	return (uint16_t)(top > SERVO_CEILING_US ? SERVO_CEILING_US : top);
}

bool servoDangerous(ServoType t, uint16_t rateHz) {
	return servoTypeInfo(t)->heli || rateHz > SERVO_SAFE_RATE_HZ;
}

/** @brief Whether the type offers @p hz. */
static bool offered(const ServoTypeInfo *ti, uint16_t hz) {
	for (uint8_t i = 0; i < ti->rateCount; i++)
		if (ti->rates[i] == hz) return true;
	return false;
}

bool servoProfileValid(const ServoProfile *p) {
	if (!p || (unsigned)p->type >= SERVO_TYPE_COUNT) return false;
	if (p->servos != 1 && p->servos != 2) return false;
	if (p->rateHz < SERVO_RATE_MIN_HZ || p->rateHz > SERVO_RATE_MAX_HZ) return false;
	if (!offered(servoTypeInfo(p->type), p->rateHz)) return false;
	if (p->minUs < SERVO_FLOOR_US) return false;
	if (p->minUs + SERVO_STEP_US > p->centreUs) return false;
	if (p->centreUs + SERVO_STEP_US > p->maxUs) return false;
	return p->maxUs <= servoMaxPulseFor(p->type, p->rateHz);
}

/** @brief One us step of MIN, CENTRE or MAX, refused outside [lo, hi]. */
static bool stepUs(uint16_t *v, int dir, int lo, int hi) {
	int n = (int)*v + dir * SERVO_STEP_US;
	if (n < lo || n > hi) return false;
	*v = (uint16_t)n;
	return true;
}

/** @brief The next offered rate in @p dir that the range allows. */
static bool stepRate(ServoProfile *p, int dir) {
	const ServoTypeInfo *ti = servoTypeInfo(p->type);
	uint16_t top = servoMaxRateFor(p->type, p->maxUs);
	int at = -1;
	for (int i = 0; i < ti->rateCount; i++)
		if (ti->rates[i] == p->rateHz) at = i;
	for (int i = at + dir; i >= 0 && i < ti->rateCount; i += dir) {
		if (ti->rates[i] <= top) {
			p->rateHz = ti->rates[i];
			return true;
		}
	}
	return false;
}

bool servoProfileStep(ServoProfile *p, ServoField f, int dir) {
	if (dir != -1 && dir != 1) return false;
	switch (f) {
		case ServoField::Servos: {
			uint8_t want = dir > 0 ? 2 : 1;
			if (p->servos == want) return false;
			p->servos = want;
			return true;
		}
		case ServoField::ReverseB: {
			bool want = dir > 0;
			if (p->reverseB == want) return false;
			p->reverseB = want;
			return true;
		}
		case ServoField::Type: {
			int t = (int)p->type + dir;
			if (t < 0 || t >= SERVO_TYPE_COUNT) return false;
			servoProfileLoadType(p, (ServoType)t);
			return true;
		}
		case ServoField::Rate:
			return stepRate(p, dir);
		case ServoField::Min:
			return stepUs(&p->minUs, dir, SERVO_FLOOR_US,
			              p->centreUs - SERVO_STEP_US);
		case ServoField::Centre:
			return stepUs(&p->centreUs, dir, p->minUs + SERVO_STEP_US,
			              p->maxUs - SERVO_STEP_US);
		case ServoField::Max:
			return stepUs(&p->maxUs, dir, p->centreUs + SERVO_STEP_US,
			              servoMaxPulseFor(p->type, p->rateHz));
	}
	return false;
}

bool servoProfileCanStep(const ServoProfile *p, ServoField f, int dir) {
	ServoProfile copy = *p;
	return servoProfileStep(&copy, f, dir);
}

void servoSweepDefault(sweep_cfg_t *c) {
	c->kind = SWEEP_SINE;
	c->mhz = 500;
	c->amplitude = SWEEP_AMPLITUDE_MAX;
	c->dwell_ms = 500;
	c->moves = 0;
}

/** @brief One step of @p v by @p step within [lo, hi], refused outside. */
static bool stepIn(uint16_t *v, int dir, int step, int lo, int hi) {
	int n = (int)*v + dir * step;
	if (n < lo || n > hi) return false;
	*v = (uint16_t)n;
	return true;
}

bool servoSweepStep(sweep_cfg_t *c, SweepField f, int dir) {
	if (dir != -1 && dir != 1) return false;
	switch (f) {
		case SweepField::Shape: {
			int k = (int)c->kind + dir;
			if (k <= (int)SWEEP_OFF || k >= (int)SWEEP_KIND_COUNT) return false;
			c->kind = (sweep_kind_t)k;
			return true;
		}
		case SweepField::Rate:
			return stepIn(&c->mhz, dir, SERVO_SWEEP_MHZ_STEP, SWEEP_MHZ_MIN, SWEEP_MHZ_MAX);
		case SweepField::Amplitude:
			return stepIn(&c->amplitude, dir, SERVO_SWEEP_AMP_STEP, 0, SWEEP_AMPLITUDE_MAX);
		case SweepField::Dwell:
			return stepIn(&c->dwell_ms, dir, SERVO_SWEEP_DWELL_STEP_MS, 0, SWEEP_DWELL_MAX_MS);
		case SweepField::Moves:
			return stepIn(&c->moves, dir, 1, 0, SERVO_SWEEP_MOVES_MAX);
	}
	return false;
}

bool servoSweepCanStep(const sweep_cfg_t *c, SweepField f, int dir) {
	sweep_cfg_t copy = *c;
	return servoSweepStep(&copy, f, dir);
}

void servoCmdRange(const ServoProfile *p, int32_t *lo, int32_t *hi) {
	int32_t below = (int32_t)p->centreUs - p->minUs;
	int32_t above = (int32_t)p->maxUs - p->centreUs;
	int32_t half = below > above ? below : above;
	*lo = (int32_t)p->centreUs - half;
	*hi = (int32_t)p->centreUs + half;
}

uint16_t servoCmdToUs(const ServoProfile *p, uint16_t cmd) {
	int32_t lo, hi;
	servoCmdRange(p, &lo, &hi);
	if (cmd > SERVO_SPAN) cmd = SERVO_SPAN;
	int32_t us = lo + ((hi - lo) * (int32_t)cmd + SERVO_SPAN / 2) / SERVO_SPAN;
	if (us < p->minUs) us = p->minUs;
	if (us > p->maxUs) us = p->maxUs;
	return (uint16_t)us;
}

uint16_t servoUsToCmd(const ServoProfile *p, uint16_t us) {
	int32_t lo, hi;
	servoCmdRange(p, &lo, &hi);
	if (hi <= lo) return SERVO_CMD_CENTRE;
	if (us <= lo) return 0;
	if (us >= hi) return SERVO_SPAN;
	int32_t span = hi - lo;
	return (uint16_t)((((int32_t)us - lo) * SERVO_SPAN + span / 2) / span);
}

/** @brief How far a side of CENTRE reaches: down to MIN or up to MAX, us. */
static float halfTravel(const ServoProfile *p, bool below) {
	return below ? (float)p->centreUs - (float)p->minUs
	             : (float)p->maxUs - (float)p->centreUs;
}

uint16_t servoDegToUs(const ServoProfile *p, float deg) {
	if (deg < -SERVO_DIAL_TRAVEL_DEG) deg = -SERVO_DIAL_TRAVEL_DEG;
	if (deg > SERVO_DIAL_TRAVEL_DEG)  deg = SERVO_DIAL_TRAVEL_DEG;
	float us = (float)p->centreUs +
	           deg / SERVO_DIAL_TRAVEL_DEG * halfTravel(p, deg < 0.0f);
	return (uint16_t)(us + 0.5f);
}

float servoUsToDeg(const ServoProfile *p, uint16_t us) {
	float off = (float)us - (float)p->centreUs;
	float half = halfTravel(p, off < 0.0f);
	if (half <= 0.0f) return 0.0f;
	return off * SERVO_DIAL_TRAVEL_DEG / half;
}

uint16_t servoDegToCmd(const ServoProfile *p, float deg) {
	return servoUsToCmd(p, servoDegToUs(p, deg));
}

uint16_t servoMirror(uint16_t cmd) {
	return (uint16_t)(cmd >= SERVO_SPAN ? 0 : SERVO_SPAN - cmd);
}

uint16_t servoSlewOf(int pct) {
	if (pct >= SERVO_SPEED_MAX_PCT) return 0;
	if (pct < SERVO_SPEED_MIN_PCT) pct = SERVO_SPEED_MIN_PCT;
	return (uint16_t)((unsigned)SERVO_FULL_SPAN_S * (unsigned)pct / 100u);
}

float servoDialAngle(int x, int y) {
	float dx = (float)(x - SERVO_DIAL_X);
	float up = (float)(SERVO_DIAL_Y - y);
	return atan2f(dx, up) * 180.0f / 3.14159265f;
}

// The annulus stops short of the band's first row, so the hit test needs no
// check against it.
static_assert(SERVO_DIAL_Y - SERVO_DIAL_R_OUT >= SERVO_DIAL_Y0,
              "the dial's hit area reaches above the dial band");

bool servoOnDial(int x, int y, float *deg) {
	if (y > SERVO_DIAL_Y1) return false;
	int dx = x - SERVO_DIAL_X, dy = y - SERVO_DIAL_Y;
	int r2 = dx * dx + dy * dy;
	if (r2 < SERVO_DIAL_R_IN * SERVO_DIAL_R_IN ||
	    r2 > SERVO_DIAL_R_OUT * SERVO_DIAL_R_OUT)
		return false;
	float a = servoDialAngle(x, y);
	if (a < -SERVO_DIAL_EDGE_DEG || a > SERVO_DIAL_EDGE_DEG) return false;
	if (deg) *deg = a;
	return true;
}
