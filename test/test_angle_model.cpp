/**
 * @file test_angle_model.cpp
 * @brief The angle meter's model (src/angle_model.*) on synthetic gravity
 *        vectors: the axis map, `theta` and the tilt, TOO STEEP and amber,
 *        the calibration and the flip, the frames of four samples, stale,
 *        ZERO, MAX, the wrap at ±180°, the travel, the temperature since the
 *        zero, NaN and degenerate inputs, and the texts in tenths.
 *
 * A gravity vector is what the accelerometer reads of a board at rest: the
 * world's up in the board's axes, 1 g long. The tests build it by turning a
 * board about a tilted line, map it to the chip's axes, add the chip's errors,
 * and feed it as the driver's samples, 4 to a frame.
 */

#include "check.h"
#include "angle_model.h"
#include "board_pins.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static constexpr float kRad = 3.14159265358979f / 180.0f;

/** @brief Assert a float lies within @p tol of @p want. */
static void checkNear(const char *what, float got, float want, float tol) {
	const bool ok = fabsf(got - want) <= tol;
	if (!ok) g_failures++;
	printf("  %-44s %10.5f  want %10.5f  %s\n", what, got, want, ok ? "ok" : "FAIL");
}

/** @brief @p d wrapped to -180 up to 180. */
static float wrap(float d) {
	while (d >= 180.0f) d -= 360.0f;
	while (d < -180.0f) d += 360.0f;
	return d;
}

/**
 * @brief The gravity vector, board axes, of a board whose `r` edge lies on a
 *        line tilted @p alpha up, turned @p delta about that line, built by
 *        turning the board's `u` and normal about the line (Rodrigues).
 */
static void boardVector(AngleAlong along, float alpha, float delta, float out[3]) {
	// The line, and the board's u and normal at delta 0, in the world.
	const float ca = cosf(alpha * kRad), sa = sinf(alpha * kRad);
	const float L[3] = { ca, 0.0f, sa };
	const float u0[3] = { 0.0f, 1.0f, 0.0f };
	const float z0[3] = { -sa, 0.0f, ca };
	const float cd = cosf(delta * kRad), sd = sinf(delta * kRad);
	float u[3], z[3];
	for (int i = 0; i < 3; ++i) {
		// L x u0 = z0 and L x z0 = -u0; both are across L.
		u[i] = u0[i] * cd + z0[i] * sd;
		z[i] = z0[i] * cd - u0[i] * sd;
	}
	// The world's up along each board axis.
	const float ar = L[2], au = u[2], az = z[2];
	const int r = (along == AngleAlong::Long) ? 0 : 1;
	out[r] = ar;
	out[1 - r] = au;
	out[2] = az;
}

/** @brief A board vector into the chip's axes under @p axes, the inverse of angleMapAxes(). */
static void chipOf(const int8_t axes[3], const float board[3], float chip[3]) {
	for (int i = 0; i < 3; ++i) {
		const int a = axes[i] < 0 ? -axes[i] : axes[i];
		chip[a - 1] = axes[i] < 0 ? -board[i] : board[i];
	}
}

/** @brief A vector in a buffer of its own, for an argument; four in turn. */
static const float *vec(float x, float y, float z) {
	static float buf[4][3];
	static int k = 0;
	k = (k + 1) % 4;
	buf[k][0] = x;
	buf[k][1] = y;
	buf[k][2] = z;
	return buf[k];
}

static uint32_t s_seq = 0;

/** @brief One sample of @p chip, g, at @p ms, temperature @p tempRaw. */
static void sampleAt(AngleModel *m, const float chip[3], uint32_t ms, int16_t tempRaw = 6400) {
	ImuSample s;
	memset(&s, 0, sizeof(s));
	if (++s_seq == 0) s_seq = 1;
	s.seq = s_seq;
	s.takenMs = ms;
	s.tempRaw = tempRaw;
	for (int i = 0; i < 3; ++i) {
		long c = lroundf(chip[i] * ANGLE_COUNTS_PER_G);
		if (c > 32767) c = 32767;
		if (c < -32768) c = -32768;
		s.acc[i] = static_cast<int16_t>(c);
	}
	angleSample(m, &s);
}

/** @brief One frame of @p chip ending at @p ms: four samples 32 ms apart. */
static void frameAt(AngleModel *m, const float chip[3], uint32_t ms, int16_t tempRaw = 6400) {
	for (int k = 3; k >= 0; --k) sampleAt(m, chip, ms - 32u * static_cast<uint32_t>(k), tempRaw);
}

/** @brief @p n frames of @p chip, 128 ms apart, from @p t on; returns the time after the last. */
static uint32_t frames(AngleModel *m, const float chip[3], uint32_t t, int n, int16_t tempRaw = 6400) {
	for (int i = 0; i < n; ++i, t += 128u) frameAt(m, chip, t, tempRaw);
	return t;
}

/** @brief The chip vector of a board at @p alpha, @p delta under the model's setup. */
static void chipAt(const AngleModel *m, float alpha, float delta, float chip[3]) {
	float b[3];
	boardVector(m->set.along, alpha, delta, b);
	chipOf(m->set.axes, b, chip);
}

static AngleReading reading(const AngleModel *m) {
	AngleReading r;
	angleRead(m, &r);
	return r;
}

/** @brief A model at @p along, opened, with the axis map @p axes. */
static void openWith(AngleModel *m, AngleAlong along, bool invert = false,
                     const int8_t *axes = nullptr) {
	AngleSetup s;
	angleSetupDefaults(&s);
	s.along = along;
	s.invert = invert;
	if (axes) memcpy(s.axes, axes, 3);
	angleOpen(m, &s);
}

/** @brief Zeroed at @p alpha, delta 0: still frames, ZERO armed, more still frames. */
static uint32_t zeroedAt(AngleModel *m, float alpha, uint32_t t) {
	float c[3];
	chipAt(m, alpha, 0.0f, c);
	t = frames(m, c, t, 10);
	angleZeroArm(m, t);
	t = frames(m, c, t, 10);
	return t;
}

static void testAxesMap() {
	section("Angle model: the axis map");
	static constexpr int8_t kBoard[3] = { BOARD_IMU_AXES };
	AngleSetup s;
	angleSetupDefaults(&s);
	checkTrue("defaults take BOARD_IMU_AXES",
	          s.axes[0] == kBoard[0] && s.axes[1] == kBoard[1] && s.axes[2] == kBoard[2]);
	checkTrue("defaults: LONG, NORMAL, OFF, no calibration",
	          s.along == AngleAlong::Long && !s.invert && s.lengthMm == ANGLE_LEN_OFF && !s.cal.valid &&
	          s.cal.gain[0] == 1.0f && s.cal.gain[2] == 1.0f && s.cal.offG[1] == 0.0f);
	const float chip[3] = { 0.1f, -0.2f, 0.9f };
	float b[3];
	const int8_t id[3] = { 1, 2, 3 };
	angleMapAxes(id, chip, b);
	checkTrue("1, 2, 3: X long, Y short, Z normal", b[0] == 0.1f && b[1] == -0.2f && b[2] == 0.9f);
	const int8_t turned[3] = { -2, 3, 1 };
	angleMapAxes(turned, chip, b);
	checkTrue("-2, 3, 1: -Y long, Z short, X normal", b[0] == 0.2f && b[1] == 0.9f && b[2] == 0.1f);
	const int8_t neg[3] = { -1, -2, -3 };
	angleMapAxes(neg, chip, b);
	checkTrue("-1, -2, -3: each turned", b[0] == -0.1f && b[1] == 0.2f && b[2] == -0.9f);
	const int8_t bad[3] = { 0, 4, -4 };
	angleMapAxes(bad, chip, b);
	checkTrue("0, 4 and -4 map to NaN", isnan(b[0]) && isnan(b[1]) && isnan(b[2]));

	// The same board turned the same way reads the same through any map.
	for (int along = 0; along < 2; ++along) {
		const AngleAlong a = along ? AngleAlong::Short : AngleAlong::Long;
		AngleModel m1, m2;
		openWith(&m1, a, false, id);
		openWith(&m2, a, false, turned);
		float c1[3], c2[3];
		chipAt(&m1, 20.0f, 33.0f, c1);
		chipAt(&m2, 20.0f, 33.0f, c2);
		frames(&m1, c1, 1000, 30);
		frames(&m2, c2, 1000, 30);
		const AngleReading r1 = reading(&m1), r2 = reading(&m2);
		char what[64];
		snprintf(what, sizeof what, "%s: maps 1,2,3 and -2,3,1 agree", along ? "SHORT" : "LONG");
		checkNear(what, r2.deg, r1.deg, 0.01f);
		checkNear("  and read the turn, 33 deg", r1.deg, 33.0f, 0.01f);
		checkNear("  and the tilt, 20 deg", r2.tiltDeg, 20.0f, 0.01f);
	}
}

static void testFormula() {
	section("Angle model: theta and the tilt of a turn about a tilted line");
	static const float kAlpha[] = { 0.0f, 10.0f, 30.0f, 45.0f, 60.0f, 69.9f, 70.0f };
	static const float kDelta[] = { -170.0f, -90.0f, -25.0f, -5.0f, 0.1f, 5.0f, 25.0f, 90.0f, 179.0f };
	for (int along = 0; along < 2; ++along) {
		const AngleAlong a = along ? AngleAlong::Short : AngleAlong::Long;
		float worstTheta = 0.0f, worstTilt = 0.0f;
		for (float alpha : kAlpha) {
			float b0[3];
			boardVector(a, alpha, 0.0f, b0);
			const float t0 = angleTheta(b0, a);
			for (float delta : kDelta) {
				float b[3];
				boardVector(a, alpha, delta, b);
				const float e = fabsf(wrap(angleTheta(b, a) - t0) - delta);
				const float et = fabsf(angleTilt(b, a) - alpha);
				if (e > worstTheta) worstTheta = e;
				if (et > worstTilt) worstTilt = et;
			}
		}
		char what[64];
		snprintf(what, sizeof what, "%s: theta's change is delta, alpha 0 to 70", along ? "SHORT" : "LONG");
		checkNear(what, worstTheta, 0.0f, 0.001f);
		snprintf(what, sizeof what, "%s: the tilt is alpha", along ? "SHORT" : "LONG");
		checkNear(what, worstTilt, 0.0f, 0.001f);
	}
	// The sense: +u edge up is positive, +r end up is positive.
	const float shortUp[3] = { 0.0f, 0.5f, 0.866f }, longUp[3] = { 0.5f, 0.0f, 0.866f };
	checkNear("LONG: the short edge up is +30", angleTheta(shortUp, AngleAlong::Long), 30.0f, 0.01f);
	checkNear("LONG: the long end up tilts +30", angleTilt(longUp, AngleAlong::Long), 30.0f, 0.01f);
	checkNear("SHORT: the long edge up is +30", angleTheta(longUp, AngleAlong::Short), 30.0f, 0.01f);
	checkNear("SHORT: the short end up tilts +30", angleTilt(shortUp, AngleAlong::Short), 30.0f, 0.01f);
	checkNear("LONG: no theta for a turn about u", angleTheta(longUp, AngleAlong::Long), 0.0f, 0.0f);

	// Through the samples, each ALONG AXIS and SIGN: zeroed at delta 0, then
	// turned by delta. The 16-bit counts hold it to 0.005 deg / cos(tilt).
	static const float kModelAlpha[] = { 0.0f, 30.0f, 60.0f, 69.9f };
	static const float kModelDelta[] = { -40.0f, -5.0f, 12.5f, 90.0f };
	for (int along = 0; along < 2; ++along) {
		for (int inv = 0; inv < 2; ++inv) {
			float worst = 0.0f, worstRel = 0.0f;
			for (float alpha : kModelAlpha) {
				for (float delta : kModelDelta) {
					AngleModel m;
					openWith(&m, along ? AngleAlong::Short : AngleAlong::Long, inv != 0);
					uint32_t t = zeroedAt(&m, alpha, 1000);
					float c[3];
					chipAt(&m, alpha, delta, c);
					frames(&m, c, t, 40);
					const AngleReading r = reading(&m);
					const float e = fabsf(r.deg - (inv ? -delta : delta));
					const float rel = e * cosf(alpha * kRad);
					if (!r.zeroed || !r.hasDeg) worst = 99.0f;
					if (e > worst) worst = e;
					if (rel > worstRel) worstRel = rel;
				}
			}
			char what[64];
			snprintf(what, sizeof what, "%s %s: delta read, x cos(tilt)",
			         along ? "SHORT" : "LONG", inv ? "INVERT" : "NORMAL");
			checkNear(what, worstRel, 0.0f, 0.005f);
		}
	}
}

static void testSteep() {
	section("Angle model: TOO STEEP and amber");
	checkTrue("70.0 too steep, -70.0 too", angleTooSteep(70.0f) && angleTooSteep(-70.0f));
	checkTrue("69.9 and -69.9 not", !angleTooSteep(69.9f) && !angleTooSteep(-69.9f));
	checkTrue("45.0 amber, -45.0 too", angleSteepWarn(45.0f) && angleSteepWarn(-45.0f));
	checkTrue("44.9 and -44.9 not", !angleSteepWarn(44.9f) && !angleSteepWarn(-44.9f));
	struct { float alpha; bool steep, warn; } k[] = {
		{ 69.9f, false, true }, { 70.1f, true, true }, { -70.1f, true, true },
		{ 44.9f, false, false }, { 45.1f, false, true }, { -45.1f, false, true },
	};
	for (const auto &c : k) {
		AngleModel m;
		openWith(&m, AngleAlong::Long);
		float v[3];
		chipAt(&m, c.alpha, 10.0f, v);
		frames(&m, v, 1000, 30);
		const AngleReading r = reading(&m);
		char what[64];
		snprintf(what, sizeof what, "tilt %.1f: steep %d amber %d, angle %d", static_cast<double>(c.alpha),
		         c.steep, c.warn, !c.steep);
		checkTrue(what, r.valid && r.tooSteep == c.steep && r.steepWarn == c.warn && r.hasDeg == !c.steep &&
		                    (c.steep ? isnan(r.deg) && !r.settled : r.settled));
	}
	// Too steep: no zero is taken and no MAX tracked, as the reading is ---.
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	float v[3];
	chipAt(&m, 75.0f, 0.0f, v);
	uint32_t t = frames(&m, v, 1000, 10);
	angleZeroArm(&m, t);
	chipAt(&m, 75.0f, 20.0f, v);
	t = frames(&m, v, t, 20);
	AngleReading r = reading(&m);
	checkTrue("too steep: ZERO stays armed, no MAX", r.zero == AngleZero::Armed && !r.zeroed &&
	                                                   !r.hasMax[0] && !r.hasMax[1]);
}

static void testFrames() {
	section("Angle model: frames of four samples, stale");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	float up[3];
	chipAt(&m, 0.0f, 0.0f, up);
	sampleAt(&m, up, 1000);
	sampleAt(&m, up, 1032);
	sampleAt(&m, up, 1064);
	checkTrue("three samples: no frame", !reading(&m).valid && !reading(&m).hasTemp);
	sampleAt(&m, up, 1096);
	checkTrue("the fourth makes a frame", reading(&m).valid && m.imu.r.taken_ms == 1096);

	// The mean of four, not the last: theta 0, 10, 20, 30 deg give 15 deg's
	// direction only approximately; the mean of the vectors is what counts.
	openWith(&m, AngleAlong::Long);
	float v[4][3], mean[3] = { 0, 0, 0 };
	for (int i = 0; i < 4; ++i) {
		chipAt(&m, 0.0f, 10.0f * static_cast<float>(i), v[i]);
		for (int a = 0; a < 3; ++a) mean[a] += roundf(v[i][a] * ANGLE_COUNTS_PER_G) / 4.0f / ANGLE_COUNTS_PER_G;
	}
	for (int i = 0; i < 4; ++i) sampleAt(&m, v[i], 2000 + 32u * static_cast<uint32_t>(i), static_cast<int16_t>(6400 + 256 * i));
	checkNear("the frame is the mean of its four", m.imu.r.angle[IMU_AXIS_X], angleTheta(mean, AngleAlong::Long), 1e-4f);
	checkNear("its temperature the mean, 26.5 C", reading(&m).tempC, 26.5f, 1e-4f);

	// A sample is taken once: seq 0 and a repeat are not counted.
	openWith(&m, AngleAlong::Long);
	ImuSample s;
	memset(&s, 0, sizeof(s));
	s.acc[2] = 16384;
	s.takenMs = 3000;
	s.seq = 0;
	for (int i = 0; i < 4; ++i) angleSample(&m, &s);
	checkTrue("seq 0 four times: no frame", m.count == 0 && !reading(&m).valid);
	s.seq = 7;
	for (int i = 0; i < 4; ++i) angleSample(&m, &s);
	checkTrue("seq 7 four times: one sample", m.count == 1 && !reading(&m).valid);
	for (uint32_t q = 8; q <= 10; ++q) {
		s.seq = q;
		angleSample(&m, &s);
	}
	checkTrue("8, 9, 10 after it: a frame", reading(&m).valid);

	// A gap of 300 ms drops the frame begun; 299 ms does not.
	openWith(&m, AngleAlong::Long);
	sampleAt(&m, up, 5000);
	sampleAt(&m, up, 5032);
	sampleAt(&m, up, 5332);
	checkTrue("a 300 ms gap: the frame begins again", m.count == 1);
	sampleAt(&m, up, 5631);
	sampleAt(&m, up, 5663);
	sampleAt(&m, up, 5695);
	checkTrue("then 299 ms: kept, a frame", m.count == 0 && reading(&m).valid && m.imu.r.taken_ms == 5695);

	// Stale 300 ms after the last frame.
	angleTick(&m, 5695 + 299);
	checkTrue("299 ms after the frame: valid", reading(&m).valid);
	angleTick(&m, 5695 + 300);
	AngleReading r = reading(&m);
	checkTrue("300 ms after it: stale, ---", !r.valid && !r.hasDeg && isnan(r.deg) && !r.settled && !r.hasMm);
}

static void testSettled() {
	section("Angle model: SETTLED");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	float v[3];
	chipAt(&m, 0.0f, 5.0f, v);
	// The first frame seeds the filter; the still time counts from it.
	frameAt(&m, v, 1000);
	checkTrue("the first frame: not settled", !reading(&m).settled);
	for (uint32_t t = 1250; t < 2000; t += 250) frameAt(&m, v, t);
	frameAt(&m, v, 1999);
	checkTrue("999 ms still: not settled", !reading(&m).settled);
	frameAt(&m, v, 2000);
	checkTrue("1000 ms still: settled", reading(&m).settled);
	float w[3];
	chipAt(&m, 0.0f, 5.6f, w);
	frameAt(&m, w, 2128);
	checkTrue("a move of 0.6 deg: not settled", !reading(&m).settled);
	checkInt("ANGLE_FILTER", m.imu.filter, 3);
}

static void testZero() {
	section("Angle model: ZERO");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	float v[3];
	chipAt(&m, 0.0f, 12.0f, v);
	uint32_t t = frames(&m, v, 1000, 20);
	AngleReading r = reading(&m);
	checkTrue("not zeroed: from level, 12", !r.zeroed && r.zero == AngleZero::None && fabsf(r.deg - 12.0f) < 0.01f);
	// Settled long before the arm: the frame 999 ms after it is not the zero,
	// the one 1000 ms after it is.
	const uint32_t arm = t;
	angleZeroArm(&m, arm);
	checkTrue("armed", reading(&m).zero == AngleZero::Armed);
	for (uint32_t s = arm + 125; s < arm + 999; s += 125) frameAt(&m, v, s);
	frameAt(&m, v, arm + 999);
	checkTrue("999 ms after the lift: still armed", reading(&m).zero == AngleZero::Armed);
	frameAt(&m, v, arm + 1000);
	r = reading(&m);
	checkTrue("1000 ms after it: taken, reads 0", r.zero == AngleZero::Taken && r.zeroed && fabsf(r.deg) < 1e-4f);
	checkTrue("the zero is the filtered theta", m.rud.zero_deg == m.imu.r.angle[IMU_AXIS_X]);

	// 10 s: a settled frame at 9999 ms takes the zero, one at 10000 ms not.
	for (int edge = 0; edge < 2; ++edge) {
		openWith(&m, AngleAlong::Long);
		chipAt(&m, 0.0f, 0.0f, v);
		uint32_t s = zeroedAt(&m, 0.0f, 1000);
		const float oldZero = m.rud.zero_deg;
		// Moving: two angles 0.6 deg apart in turn, never still; then still
		// 0.6 deg or more from both, the filter settled within 8 frames.
		float a[3], b[3], c[3];
		chipAt(&m, 0.0f, 20.0f, a);
		chipAt(&m, 0.0f, 20.6f, b);
		chipAt(&m, 0.0f, 21.2f, c);
		const uint32_t armAt = s;
		angleZeroArm(&m, armAt);
		const uint32_t stillFrom = armAt + (edge == 0 ? 8999u : 9000u);
		for (uint32_t mv = armAt, i = 0; mv < stillFrom; mv += 125, ++i) frameAt(&m, (i & 1) ? b : c, mv);
		for (uint32_t q = stillFrom; q < stillFrom + 999; q += 125) frameAt(&m, a, q);
		frameAt(&m, a, stillFrom + 999);
		checkTrue(edge == 0 ? "still from 8999 ms, 999 ms on: armed" : "still from 9000 ms, 999 ms on: armed",
		          reading(&m).zero == AngleZero::Armed && !reading(&m).settled);
		frameAt(&m, a, stillFrom + 1000);
		r = reading(&m);
		if (edge == 0) {
			checkTrue("settled at 9999 ms: taken", r.zero == AngleZero::Taken && fabsf(r.deg) < 1e-3f);
		} else {
			checkTrue("settled at 10000 ms: NO ZERO, the old one kept",
			          r.zero == AngleZero::TimedOut && m.rud.zero_deg == oldZero && fabsf(r.deg - 20.0f) < 0.05f);
		}
	}
	// With no frames at all, the tick times it out.
	openWith(&m, AngleAlong::Long);
	angleZeroArm(&m, 500);
	angleTick(&m, 500 + 9999);
	checkTrue("no frames, 9999 ms: armed", reading(&m).zero == AngleZero::Armed);
	angleTick(&m, 500 + 10000);
	checkTrue("10000 ms: NO ZERO", reading(&m).zero == AngleZero::TimedOut && !reading(&m).zeroed);
	// Across the clock's wrap.
	openWith(&m, AngleAlong::Long);
	angleZeroArm(&m, 0xFFFFF000u);
	angleTick(&m, 0xFFFFF000u + 9999u);
	checkTrue("across the wrap, 9999 ms: armed", reading(&m).zero == AngleZero::Armed);
}

static void testWrap() {
	section("Angle model: the wrap at 180 deg");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	// Nearly upside down: theta 170 deg, the zero; then -170 deg.
	float v[3];
	chipAt(&m, 0.0f, 170.0f, v);
	uint32_t t = frames(&m, v, 1000, 10);
	angleZeroArm(&m, t);
	t = frames(&m, v, t, 10);
	checkTrue("zeroed at 170", reading(&m).zero == AngleZero::Taken);
	chipAt(&m, 0.0f, -170.0f, v);
	t = frames(&m, v, t, 40);
	checkNear("170 to -170 reads +20, not -340", reading(&m).deg, 20.0f, 0.01f);
	angleSetInvert(&m, true);
	checkNear("INVERT: -20", reading(&m).deg, -20.0f, 0.01f);
	// Exactly half a turn: -180, either sign.
	openWith(&m, AngleAlong::Long);
	const float down[3] = { 0.0f, 0.0f, -1.0f };
	frames(&m, down, 1000, 5);
	checkNear("unzeroed, screen down: theta -180", reading(&m).deg, -180.0f, 0.0f);
	angleSetInvert(&m, true);
	checkNear("INVERT keeps -180, not 180", reading(&m).deg, -180.0f, 0.0f);
	checkTrue("screen down", reading(&m).faceDown);
	angleSetInvert(&m, false);
	frames(&m, vec(0.0f, 0.0f, 1.0f), 2000, 2);
	checkTrue("screen up", !reading(&m).faceDown);
}

static void testMax() {
	section("Angle model: MAX");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	uint32_t t = zeroedAt(&m, 10.0f, 1000);
	float v[3];
	// Moving through 30 deg: not settled, not tracked.
	chipAt(&m, 10.0f, 30.0f, v);
	t = frames(&m, v, t, 3);
	AngleReading r = reading(&m);
	checkTrue("a moving reading: no MAX", !r.settled && !r.hasMax[0] && !r.hasMax[1]);
	chipAt(&m, 10.0f, -5.0f, v);
	t = frames(&m, v, t, 3);
	checkTrue("still for 256 ms: no MAX", !reading(&m).hasMax[0]);
	chipAt(&m, 10.0f, 21.7f, v);
	t = frames(&m, v, t, 40);
	chipAt(&m, 10.0f, -18.2f, v);
	t = frames(&m, v, t, 40);
	chipAt(&m, 10.0f, 15.0f, v);
	t = frames(&m, v, t, 40);
	r = reading(&m);
	checkTrue("both sides", r.hasMax[0] && r.hasMax[1]);
	checkNear("MAX -18.2", r.maxDeg[0], -18.2f, 0.01f);
	checkNear("MAX +21.7, not the later 15", r.maxDeg[1], 21.7f, 0.01f);
	checkTrue("LENGTH OFF: no MAX travel", !r.hasMaxMm[0] && !r.hasMaxMm[1] && r.maxMm[0] == 0.0f);
	angleSetLength(&m, 40);
	r = reading(&m);
	checkNear("40 mm: -12.5 mm", r.maxMm[0], 40.0f * sinf(-18.2f * kRad), 1e-3f);
	checkNear("40 mm: +14.8 mm", r.maxMm[1], 40.0f * sinf(21.7f * kRad), 1e-3f);
	angleSetInvert(&m, true);
	r = reading(&m);
	checkNear("INVERT: the sides swap, -21.7", r.maxDeg[0], -21.7f, 0.01f);
	checkNear("  and +18.2", r.maxDeg[1], 18.2f, 0.01f);
	checkNear("  and the travel, +12.5 mm", r.maxMm[1], 40.0f * sinf(18.2f * kRad), 1e-3f);
	checkTrue("INVERT keeps the zero", r.zeroed && fabsf(r.deg + 15.0f) < 0.01f);
	angleResetMax(&m);
	r = reading(&m);
	checkTrue("RESET MAX: none, the zero kept", !r.hasMax[0] && !r.hasMax[1] && r.zeroed);
	t = frames(&m, v, t, 2);
	checkTrue("then the next settled frame", reading(&m).hasMax[0] && !reading(&m).hasMax[1]);
	angleZeroArm(&m, t);
	t = frames(&m, v, t, 10);
	r = reading(&m);
	checkTrue("ZERO clears MAX", r.zero == AngleZero::Taken && !r.hasMax[0] && !r.hasMax[1]);
	// A new ALONG AXIS or calibration drops the zero and MAX and seeds.
	chipAt(&m, 10.0f, 25.0f, v);
	t = frames(&m, v, t, 20);
	angleSetAlong(&m, AngleAlong::Long);
	checkTrue("the same ALONG AXIS: kept", reading(&m).zeroed && reading(&m).hasMax[0]);
	angleSetAlong(&m, AngleAlong::Short);
	r = reading(&m);
	checkTrue("SHORT: no zero, no MAX, not armed", !r.zeroed && !r.hasMax[0] && !r.hasMax[1] &&
	                                                 r.zero == AngleZero::None);
	frameAt(&m, v, t);
	checkNear("  the filter seeded: SHORT's theta at once", m.imu.r.angle[IMU_AXIS_X],
	          atan2f(v[0], v[2]) / kRad, 0.01f);
	openWith(&m, AngleAlong::Long);
	t = zeroedAt(&m, 0.0f, 1000);
	AngleCal cal;
	memset(&cal, 0, sizeof(cal));
	cal.valid = true;
	cal.gain[0] = cal.gain[1] = cal.gain[2] = 1.0f;
	cal.offG[1] = 0.1f;
	angleSetCal(&m, &cal);
	checkTrue("a calibration drops the zero", !reading(&m).zeroed && m.set.cal.valid);
	chipAt(&m, 0.0f, 0.0f, v);
	frameAt(&m, v, t);
	checkNear("  and seeds: -5.7 deg at once", m.imu.r.angle[IMU_AXIS_X], -atanf(0.1f) / kRad, 0.01f);
	angleSetCal(&m, nullptr);
	checkTrue("none: not applied", !m.set.cal.valid);
}

static void testMm() {
	section("Angle model: the travel in mm");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	uint32_t t = zeroedAt(&m, 0.0f, 1000);
	float v[3];
	chipAt(&m, 0.0f, 20.0f, v);
	t = frames(&m, v, t, 40);
	AngleReading r = reading(&m);
	checkTrue("OFF: no travel", !r.hasMm && isnan(r.mm));
	static const struct { uint16_t len; bool has; } k[] = { { 9, false }, { 10, true }, { 500, true }, { 501, false } };
	for (const auto &c : k) {
		angleSetLength(&m, c.len);
		r = reading(&m);
		char what[64];
		snprintf(what, sizeof what, "LENGTH %u: travel %s", c.len, c.has ? "shown" : "none");
		checkTrue(what, r.hasMm == c.has && (c.has ? fabsf(r.mm - servo_rudder_mm(20.0f, c.len)) < 0.01f : isnan(r.mm)));
	}
	angleSetLength(&m, 40);
	checkNear("20 deg on 40 mm: 13.7 mm", reading(&m).mm, 13.68f, 0.01f);
	angleSetInvert(&m, true);
	checkNear("INVERT: -13.7 mm", reading(&m).mm, -13.68f, 0.01f);
}

static void testTemp() {
	section("Angle model: the temperature since the zero");
	for (int k = 0; k < 4; ++k) {
		static const int16_t kRaw[] = { 512, 486, -512, -486 };
		AngleModel m;
		openWith(&m, AngleAlong::Long);
		float v[3];
		chipAt(&m, 0.0f, 0.0f, v);
		uint32_t t = frames(&m, v, 1000, 10, 6400);
		AngleReading r = reading(&m);
		checkTrue("not zeroed: no change, no warning", r.hasTemp && r.tempC == 25.0f && r.tempSinceZero == 0.0f && !r.tempWarn);
		angleZeroArm(&m, t);
		t = frames(&m, v, t, 10, 6400);
		frames(&m, v, t, 1, static_cast<int16_t>(6400 + kRaw[k]));
		r = reading(&m);
		const bool warn = (k % 2) == 0;
		char what[64];
		snprintf(what, sizeof what, "%+.3f C since the zero: %s", kRaw[k] / 256.0, warn ? "warns" : "does not");
		checkTrue(what, r.zeroed && r.tempWarn == warn && fabsf(r.tempSinceZero - kRaw[k] / 256.0f) < 1e-5f);
	}
}

static void testCal() {
	section("Angle model: the calibration and the errors it removes");
	AngleCal c;
	memset(&c, 0, sizeof(c));
	c.valid = true;
	c.offG[0] = 0.05f;
	c.offG[1] = -0.2f;
	c.offG[2] = 0.2f;
	c.gain[0] = 1.0f;
	c.gain[1] = 0.88f;
	c.gain[2] = 1.12f;
	checkTrue("offsets 0.2 g and gains 0.88 and 1.12 apply", angleCalValid(&c));
	const float a[3] = { 0.15f, 0.68f, 1.32f };
	float o[3];
	angleCalApply(&c, a, o);
	checkTrue("(a - o) / k per axis", fabsf(o[0] - 0.1f) < 1e-6f && fabsf(o[1] - 1.0f) < 1e-6f && fabsf(o[2] - 1.0f) < 1e-6f);
	AngleCal bad = c;
	bad.offG[1] = -0.201f;
	checkTrue("offset 0.201 g: refused", !angleCalValid(&bad));
	bad = c;
	bad.gain[2] = 1.121f;
	checkTrue("gain 1.121: refused", !angleCalValid(&bad));
	bad = c;
	bad.gain[1] = 0.879f;
	checkTrue("gain 0.879: refused", !angleCalValid(&bad));
	bad = c;
	bad.gain[0] = NAN;
	checkTrue("a NaN gain: refused", !angleCalValid(&bad));
	bad = c;
	bad.offG[2] = NAN;
	checkTrue("a NaN offset: refused", !angleCalValid(&bad));
	bad = c;
	bad.valid = false;
	checkTrue("not kept: refused, and NULL", !angleCalValid(&bad) && !angleCalValid(nullptr));
	angleCalApply(&bad, a, o);
	checkTrue("a refused calibration leaves the vector", o[0] == a[0] && o[1] == a[1] && o[2] == a[2]);

	// The table under "What a zero cancels": a zero at level, then 5 and 25
	// degrees, with each error alone and no calibration.
	struct { const char *what; float ou, oz, ku; float e5, e25; } k[] = {
		{ "offset of a_u 100 mg", 0.1f, 0.0f, 1.0f, -0.07f, -0.74f },
		{ "offset of a_z 100 mg", 0.0f, 0.1f, 1.0f, -0.45f, -2.22f },
		{ "gain of u 6 % over z", 0.0f, 0.0f, 1.06f, 0.30f, 1.30f },
	};
	for (const auto &e : k) {
		for (int cal = 0; cal < 2; ++cal) {
			float err[2];
			for (int j = 0; j < 2; ++j) {
				const float delta = j ? 25.0f : 5.0f;
				AngleModel m;
				openWith(&m, AngleAlong::Long);
				if (cal) {
					AngleCal fix;
					memset(&fix, 0, sizeof(fix));
					fix.valid = true;
					fix.offG[1] = e.ou;
					fix.offG[2] = e.oz;
					fix.gain[0] = 1.0f;
					fix.gain[1] = e.ku;
					fix.gain[2] = 1.0f;
					angleSetCal(&m, &fix);
				}
				float v[3];
				chipAt(&m, 0.0f, 0.0f, v);
				v[1] = v[1] * e.ku + e.ou;
				v[2] += e.oz;
				uint32_t t = frames(&m, v, 1000, 10);
				angleZeroArm(&m, t);
				t = frames(&m, v, t, 10);
				chipAt(&m, 0.0f, delta, v);
				v[1] = v[1] * e.ku + e.ou;
				v[2] += e.oz;
				frames(&m, v, t, 40);
				err[j] = reading(&m).deg - delta;
			}
			char what[80];
			if (!cal) {
				snprintf(what, sizeof what, "%s: at 5 deg %+.2f", e.what, static_cast<double>(e.e5));
				checkNear(what, err[0], e.e5, 0.006f);
				snprintf(what, sizeof what, "%s: at 25 deg %+.2f", e.what, static_cast<double>(e.e25));
				checkNear(what, err[1], e.e25, 0.006f);
			} else {
				snprintf(what, sizeof what, "  calibrated: at 25 deg 0");
				checkNear(what, err[1], 0.0f, 0.005f);
			}
		}
	}
}

/** @brief The three poses on a surface sloped @p slope toward @p dir, with the chip's errors. */
static void poses(float slope, float dir, const float off[3], float kz, float p[3][3]) {
	const float sx = sinf(slope * kRad) * cosf(dir * kRad), sy = sinf(slope * kRad) * sinf(dir * kRad);
	const float cz = cosf(slope * kRad);
	const float up[3] = { sx, sy, cz };       // screen up
	const float turned[3] = { -sx, -sy, cz }; // turned end for end about the normal
	const float down[3] = { sx, -sy, -cz };   // turned over about the long edge
	const float *g[3] = { up, turned, down };
	for (int i = 0; i < 3; ++i) {
		p[i][0] = g[i][0] + off[0];
		p[i][1] = g[i][1] + off[1];
		p[i][2] = g[i][2] * kz + off[2];
	}
}

static void testFlipSolve() {
	section("Angle model: the flip's arithmetic");
	const int8_t id[3] = { 1, 2, 3 };
	AngleCal c;
	// By hand: o_x = (0.10 - 0.02) / 2 = 0.04, o_y = (-0.05 + 0.07) / 2 =
	// 0.01, o_z = (1.02 - 0.98) / 2 = 0.02; the slope's sine (0.06, -0.06),
	// its square 0.0072; gain (1.02 + 0.98) / 2 / sqrt(0.9928) = 1.0036199.
	const float p1[3] = { 0.10f, -0.05f, 1.02f }, p2[3] = { -0.02f, 0.07f, 1.02f }, p3[3] = { 0.03f, 0.01f, -0.98f };
	checkTrue("solves", angleFlipSolve(p1, p2, p3, id, &c) && c.valid);
	checkNear("o_x 0.04", c.offG[0], 0.04f, 1e-6f);
	checkNear("o_y 0.01", c.offG[1], 0.01f, 1e-6f);
	checkNear("o_z 0.02", c.offG[2], 0.02f, 1e-6f);
	checkNear("gain z 1.0036199", c.gain[2], 1.0036199f, 1e-6f);
	checkTrue("gains x and y 1", c.gain[0] == 1.0f && c.gain[1] == 1.0f);

	// Offsets set in the chip, recovered exactly on a surface level and sloped 5 deg.
	const float off[3] = { 0.083f, -0.061f, 0.047f };
	for (int s = 0; s < 3; ++s) {
		const float slope = s == 0 ? 0.0f : 5.0f, dir = s == 2 ? 130.0f : 0.0f;
		float p[3][3];
		poses(slope, dir, off, 1.043f, p);
		const bool ok = angleFlipSolve(p[0], p[1], p[2], id, &c);
		char what[64];
		snprintf(what, sizeof what, "slope %.0f deg toward %.0f: offsets and gain z", static_cast<double>(slope),
		         static_cast<double>(dir));
		checkTrue(what, ok && fabsf(c.offG[0] - off[0]) < 1e-6f && fabsf(c.offG[1] - off[1]) < 1e-6f &&
		                    fabsf(c.offG[2] - off[2]) < 1e-6f && fabsf(c.gain[2] - 1.043f) < 1e-5f);
	}
	// The normal on chip -Z and the long edge on chip Y: the same poses mapped.
	const int8_t map[3] = { 2, 1, -3 };
	float p[3][3], q[3][3];
	poses(5.0f, 40.0f, off, 0.97f, p);
	for (int i = 0; i < 3; ++i) chipOf(map, p[i], q[i]);
	// chipOf maps the offsets too; the offsets in the chip's own axes:
	float offChip[3];
	chipOf(map, off, offChip);
	checkTrue("map 2, 1, -3: the chip's offsets, gain 0.97",
	          angleFlipSolve(q[0], q[1], q[2], map, &c) && fabsf(c.offG[0] - offChip[0]) < 1e-6f &&
	              fabsf(c.offG[1] - offChip[1]) < 1e-6f && fabsf(c.offG[2] - offChip[2]) < 1e-6f &&
	              fabsf(c.gain[2] - 0.97f) < 1e-5f && c.gain[0] == 1.0f && c.gain[1] == 1.0f);

	// Refusals.
	float a[3] = { 0.0f, 0.0f, 0.8f }, b[3] = { 0.0f, 0.0f, 0.81f }, d[3] = { 0.0f, 0.0f, -0.81f };
	checkTrue("P1's normal at 0.8 g: refused", !angleFlipSolve(a, b, d, id, &c) && !c.valid);
	checkTrue("P2's normal at 0.8 g: refused", !angleFlipSolve(b, a, d, id, &c));
	float e[3] = { 0.0f, 0.0f, -0.8f };
	checkTrue("P3's normal at -0.8 g: refused", !angleFlipSolve(b, b, e, id, &c));
	checkTrue("0.81 and -0.81 g: past the gain's limit", !angleFlipSolve(b, b, d, id, &c));
	float u1[3] = { 0.0f, 0.0f, 0.9f }, u3[3] = { 0.0f, 0.0f, -0.9f };
	checkTrue("0.9 and -0.9 g: gain 0.9", angleFlipSolve(u1, u1, u3, id, &c) && fabsf(c.gain[2] - 0.9f) < 1e-6f);
	float n1[3] = { NAN, 0.0f, 1.0f };
	checkTrue("a NaN: refused", !angleFlipSolve(n1, u1, u3, id, &c));
	float i1[3] = { 0.0f, INFINITY, 1.0f };
	checkTrue("an infinity in P2: refused", !angleFlipSolve(u1, i1, u3, id, &c));
	checkTrue("an infinity in P3: refused", !angleFlipSolve(u1, u1, i1, id, &c));
	float s1[3] = { 1.0f, 0.0f, 0.9f }, s2[3] = { -1.0f, 0.0f, 0.9f };
	checkTrue("a slope of 90 deg: refused", !angleFlipSolve(s1, s2, u3, id, &c));
	float o1[3] = { 0.21f, 0.0f, 1.0f }, o2[3] = { 0.21f, 0.0f, 1.0f }, o3[3] = { 0.0f, 0.0f, -1.0f };
	checkTrue("an offset of 0.21 g: refused", !angleFlipSolve(o1, o2, o3, id, &c) && !c.valid);
	const int8_t bad[3] = { 1, 2, 0 };
	checkTrue("a map with no normal: refused", !angleFlipSolve(p1, p2, p3, bad, &c));
	checkTrue("NULL: refused", !angleFlipSolve(p1, p2, p3, id, nullptr));
}

static void testFlipSteps() {
	section("Angle model: the flip's steps");
	const int8_t id[3] = { 1, 2, 3 };
	AngleFlip f;
	angleFlipStart(&f);
	const float up[3] = { 0.05f, -0.03f, 1.01f };
	// Step 1: a still stretch of 2000 ms, frames 100 ms apart.
	uint32_t t = 1000;
	for (; t <= 2900; t += 100) angleFlipFrame(&f, up, id, t);
	angleFlipFrame(&f, up, id, 2999);
	checkTrue("1999 ms still: step 1", f.step == FlipStep::Up);
	angleFlipFrame(&f, up, id, 3000);
	checkTrue("2000 ms still: P1, step 2", f.step == FlipStep::Turn && fabsf(f.pose[0][2] - 1.01f) < 1e-6f && f.n == 0);
	// Step 2 waits for a move: 3000 ms still is not P2.
	for (t = 3100; t <= 6100; t += 100) angleFlipFrame(&f, up, id, t);
	checkTrue("not moved, 3000 ms still: step 2", f.step == FlipStep::Turn && !f.moved);
	// 0.4 deg off the stretch's first frame is still; 0.6 deg breaks it.
	const float c4 = cosf(0.4f * kRad), s4 = sinf(0.4f * kRad);
	const float tilt4[3] = { up[0], up[1] * c4 - up[2] * s4, up[1] * s4 + up[2] * c4 };
	angleFlipFrame(&f, tilt4, id, 6200);
	checkTrue("0.4 deg: the same stretch", !f.moved && f.n > 30);
	const float c6 = cosf(0.6f * kRad), s6 = sinf(0.6f * kRad);
	const float tilt6[3] = { up[0], up[1] * c6 - up[2] * s6, up[1] * s6 + up[2] * c6 };
	angleFlipFrame(&f, tilt6, id, 6300);
	checkTrue("0.6 deg: moved, a new stretch", f.moved && f.n == 1 && f.fromMs == 6300);
	const float turned[3] = { -0.03f, 0.05f, 1.01f };
	for (t = 6400; t < 8400; t += 100) angleFlipFrame(&f, turned, id, t);
	checkTrue("1900 ms still after the move: step 2", f.step == FlipStep::Turn);
	angleFlipFrame(&f, turned, id, 8400);
	checkTrue("2000 ms: P2, step 3", f.step == FlipStep::Down && fabsf(f.pose[1][0] + 0.03f) < 1e-6f);
	// Step 3: screen up frames do nothing; -0.8 g is not down, -0.81 g is.
	const float edge[3] = { 0.0f, 0.6f, -0.8f };
	for (t = 8500; t <= 11000; t += 100) angleFlipFrame(&f, edge, id, t);
	checkTrue("-0.8 g for 2500 ms: not down", f.step == FlipStep::Down && f.n == 0);
	const float down[3] = { 0.04f, -0.02f, -0.99f };
	// A gap of 300 ms breaks the stretch; 299 ms does not.
	angleFlipFrame(&f, down, id, 11100);
	angleFlipFrame(&f, down, id, 11399);
	checkTrue("299 ms apart: one stretch", f.n == 2);
	angleFlipFrame(&f, down, id, 11699);
	checkTrue("300 ms apart: a new one", f.n == 1 && f.fromMs == 11699);
	for (t = 11799; t <= 13699; t += 100) angleFlipFrame(&f, down, id, t);
	checkTrue("2000 ms screen down: P3, waiting for screen up", f.step == FlipStep::Back && fabsf(f.pose[2][2] + 0.99f) < 1e-6f);
	angleFlipFrame(&f, down, id, 13800);
	angleFlipFrame(&f, edge, id, 13900);
	const float upEdge[3] = { 0.0f, 0.6f, 0.8f };
	angleFlipFrame(&f, upEdge, id, 14000);
	checkTrue("down, then 0.8 g: still waiting", f.step == FlipStep::Back);
	angleFlipFrame(&f, up, id, 14100);
	AngleCal want;
	angleFlipSolve(f.pose[0], f.pose[1], f.pose[2], id, &want);
	bool same = f.result.valid == want.valid;
	for (int i = 0; i < 3; ++i) same = same && f.result.offG[i] == want.offG[i] && f.result.gain[i] == want.gain[i];
	checkTrue("screen up: done, the poses solved", f.step == FlipStep::Done && f.result.valid && same);
	checkNear("  o_x from P1 and P2", f.result.offG[0], 0.01f, 1e-6f);
	checkNear("  o_z from P1 and P3", f.result.offG[2], 0.01f, 1e-6f);
	angleFlipFrame(&f, tilt6, id, 14200);
	checkTrue("done: frames change nothing", f.step == FlipStep::Done);

	// The mean of the stretch, not its last frame.
	angleFlipStart(&f);
	const float a[3] = { 0.0f, 0.0f, 1.0f }, b[3] = { 0.002f, 0.0f, 1.0f };
	for (t = 0; t <= 2000; t += 100) angleFlipFrame(&f, (t / 100) % 2 ? b : a, id, t);
	checkNear("P1 is the stretch's mean", f.pose[0][0], 0.002f * 10.0f / 21.0f, 1e-7f);

	// Poses past the limits: failed.
	angleFlipStart(&f);
	const float far1[3] = { 0.3f, 0.0f, 1.0f }, farD[3] = { 0.3f, 0.0f, -1.0f };
	for (t = 0; t <= 2000; t += 100) angleFlipFrame(&f, far1, id, t);
	angleFlipFrame(&f, vec(0.3f, 0.0f, 0.5f), id, 2100);
	for (t = 2200; t <= 4200; t += 100) angleFlipFrame(&f, far1, id, t);
	for (t = 4300; t <= 6300; t += 100) angleFlipFrame(&f, farD, id, t);
	angleFlipFrame(&f, far1, id, 6400);
	checkTrue("an offset of 0.3 g: failed, no result", f.step == FlipStep::Failed && !f.result.valid);
	angleFlipFrame(&f, far1, id, 6500);
	checkTrue("failed: frames change nothing", f.step == FlipStep::Failed);
	angleFlipFrame(nullptr, up, id, 0);
	angleFlipFrame(&f, nullptr, id, 0);
	angleFlipStart(nullptr);

	// Through the model: the flip takes the frames only while FLIP CAL is open.
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	float v[3] = { 0.02f, 0.01f, 1.0f };
	uint32_t s = frames(&m, v, 1000, 20);
	checkTrue("FLIP CAL closed: the flip takes nothing", !m.flipOn && m.flip.n == 0 && m.flip.step == FlipStep::Up);
	angleFlipBegin(&m);
	s = frames(&m, v, s, 17);
	checkTrue("open: 17 frames, 2048 ms, P1", m.flip.step == FlipStep::Turn);
	angleFlipEnd(&m);
	s = frames(&m, v, s, 2);
	checkTrue("closed again: it takes no more", !m.flipOn && m.flip.n == 0);
	angleFlipBegin(&m);
	checkTrue("open again: from step 1", m.flipOn && m.flip.step == FlipStep::Up);
}

static void testDegenerate() {
	section("Angle model: NaN and degenerate inputs");
	AngleModel m;
	openWith(&m, AngleAlong::Long);
	const float none[3] = { 0.0f, 0.0f, 0.0f };
	frames(&m, none, 1000, 3);
	AngleReading r = reading(&m);
	checkTrue("0 g: dropped, no reading, the temperature kept", !r.valid && !r.hasDeg && r.hasTemp);
	const float half[3] = { 0.0f, 0.3f, 0.4f };
	frames(&m, half, 2000, 1);
	checkTrue("0.5 g: taken", reading(&m).valid);
	openWith(&m, AngleAlong::Long);
	sampleAt(&m, vec(0.0f, 0.0f, 0.0f), 3000);
	sampleAt(&m, vec(0.0f, 0.0f, 0.0f), 3032);
	sampleAt(&m, vec(0.0f, 0.0f, 0.0f), 3064);
	// 0.5 g less one count over the four.
	ImuSample s;
	memset(&s, 0, sizeof(s));
	s.seq = 999999;
	s.takenMs = 3096;
	s.acc[2] = static_cast<int16_t>(4 * 8192 - 1);
	angleSample(&m, &s);
	checkTrue("0.5 g less a count: dropped", !reading(&m).valid);
	// A dropped frame ages the reading and times ZERO out.
	openWith(&m, AngleAlong::Long);
	const float up[3] = { 0.0f, 0.0f, 1.0f };
	frames(&m, up, 1000, 1);
	angleZeroArm(&m, 1000);
	frameAt(&m, none, 1300);
	checkTrue("a dropped frame 300 ms on: stale", !reading(&m).valid);
	frameAt(&m, none, 11000);
	checkTrue("and ZERO timed out by one", reading(&m).zero == AngleZero::TimedOut);
	// A map that is not one gives NaN: every frame dropped.
	const int8_t bad[3] = { 1, 2, 0 };
	openWith(&m, AngleAlong::Long, false, bad);
	frames(&m, up, 1000, 5);
	checkTrue("no normal in the map: no reading", !reading(&m).valid);
	// A calibration with a NaN is not applied.
	openWith(&m, AngleAlong::Long);
	AngleCal c;
	memset(&c, 0, sizeof(c));
	c.valid = true;
	c.gain[0] = c.gain[1] = 1.0f;
	c.gain[2] = NAN;
	angleSetCal(&m, &c);
	frames(&m, up, 1000, 5);
	checkTrue("a NaN gain: not applied, a reading", reading(&m).valid && fabsf(reading(&m).deg) < 1e-4f);
	// NULLs do nothing.
	angleSetupDefaults(nullptr);
	angleOpen(nullptr, nullptr);
	angleSample(nullptr, &s);
	angleSample(&m, nullptr);
	angleTick(nullptr, 0);
	angleRead(&m, nullptr);
	angleZeroArm(nullptr, 0);
	angleResetMax(nullptr);
	angleSetAlong(nullptr, AngleAlong::Short);
	angleSetInvert(nullptr, true);
	angleSetLength(nullptr, 40);
	angleSetCal(nullptr, &c);
	angleFlipBegin(nullptr);
	angleFlipEnd(nullptr);
	angleRead(nullptr, &r);
	checkTrue("NULL model: an empty reading", !r.valid && isnan(r.deg) && isnan(r.mm) && r.zero == AngleZero::None);
	angleOpen(&m, nullptr);
	checkTrue("no setup: the defaults", m.set.along == AngleAlong::Long && m.set.axes[2] != 0 && m.imu.filter == ANGLE_FILTER);
	checkNear("angleBetween: 90 deg", angleBetween(vec(1, 0, 0), vec(0, 1, 0)), 90.0f, 1e-4f);
	checkNear("angleBetween: 180 deg", angleBetween(vec(0, 0, 1), vec(0, 0, -2)), 180.0f, 1e-4f);
}

static void testText() {
	section("Angle model: the texts in tenths");
	static const struct { float v; const char *plain, *plus; } k[] = {
		{ 0.0f, "0.0", "+0.0" },       { -0.0f, "0.0", "+0.0" },       { 0.04f, "0.0", "+0.0" },
		{ -0.04f, "0.0", "+0.0" },     { 0.05f, "0.1", "+0.1" },       { -0.05f, "-0.1", "-0.1" },
		{ 12.35f, "12.4", "+12.4" },   { -12.35f, "-12.4", "-12.4" },  { 12.34f, "12.3", "+12.3" },
		{ 179.96f, "180.0", "+180.0" }, { -180.0f, "-180.0", "-180.0" }, { -500.0f, "-500.0", "-500.0" },
		{ 99999.0f, "99999.0", "+99999.0" }, { 100000.0f, "---", "---" }, { NAN, "---", "---" },
		{ INFINITY, "---", "---" },    { -INFINITY, "---", "---" },
	};
	char b[16];
	for (const auto &c : k) {
		checkStr("plain", angleTenthsText(b, sizeof b, c.v, false), c.plain);
		checkStr("plus", angleTenthsText(b, sizeof b, c.v, true), c.plus);
	}
	char five[5];
	checkStr("cut to 5 bytes", angleTenthsText(five, sizeof five, -180.0f, false), "-180");
	checkTrue("NULL", angleTenthsText(nullptr, 8, 1.0f, false) == nullptr);
	b[0] = 'x';
	checkTrue("size 0: untouched", angleTenthsText(b, 0, 1.0f, false) == b && b[0] == 'x');
}

void runAngleModelTests() {
	testAxesMap();
	testFormula();
	testSteep();
	testFrames();
	testSettled();
	testZero();
	testWrap();
	testMax();
	testMm();
	testTemp();
	testCal();
	testFlipSolve();
	testFlipSteps();
	testDegenerate();
	testText();
}
