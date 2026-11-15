/**
 * @file angle_model.cpp
 * @brief The angle meter's model (angle_model.h): samples into frames, the
 *        calibration, the axis map, `theta` and the tilt, rcbench's IMU model,
 *        ZERO, MAX, the travel and the flip.
 *
 * @see docs/design/angle-meter.md
 */

#include "angle_model.h"
#include "board_pins.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/** @brief Radians to degrees. */
static constexpr float kDegPerRad = 57.29577951308232f;

static_assert(ANGLE_FRAME_SAMPLES == 4u, "the note's frames are four samples");

void angleSetupDefaults(AngleSetup *s) {
	if (!s) return;
	static constexpr int8_t kAxes[3] = { BOARD_IMU_AXES };
	memset(s, 0, sizeof(*s));
	s->along = AngleAlong::Long;
	s->invert = false;
	s->lengthMm = ANGLE_LEN_OFF;
	s->cal.valid = false;
	for (int i = 0; i < 3; ++i) {
		s->cal.offG[i] = 0.0f;
		s->cal.gain[i] = 1.0f;
		s->axes[i] = kAxes[i];
	}
}

bool angleCalValid(const AngleCal *cal) {
	if (!cal || !cal->valid) return false;
	for (int i = 0; i < 3; ++i) {
		// Written so that NaN fails each test.
		if (!(fabsf(cal->offG[i]) <= ANGLE_CAL_OFFSET_MAX_G)) return false;
		if (!(cal->gain[i] >= ANGLE_CAL_GAIN_MIN && cal->gain[i] <= ANGLE_CAL_GAIN_MAX)) return false;
	}
	return true;
}

void angleCalApply(const AngleCal *cal, const float *chip, float *out) {
	const bool on = angleCalValid(cal);
	for (int i = 0; i < 3; ++i) {
		out[i] = on ? (chip[i] - cal->offG[i]) / cal->gain[i] : chip[i];
	}
}

void angleMapAxes(const int8_t *axes, const float *chip, float *board) {
	for (int i = 0; i < 3; ++i) {
		const int a = axes[i];
		const int idx = (a < 0 ? -a : a) - 1;
		if (idx < 0 || idx > 2) {
			board[i] = NAN;
			continue;
		}
		board[i] = (a < 0) ? -chip[idx] : chip[idx];
	}
}

/** @brief The board's `u` and `r` axes for @p along: 0 the long edge, 1 the short. */
static int uAxis(AngleAlong along) { return along == AngleAlong::Long ? 1 : 0; }
/** @copydoc uAxis */
static int rAxis(AngleAlong along) { return along == AngleAlong::Long ? 0 : 1; }

float angleTheta(const float *board, AngleAlong along) {
	return atan2f(board[uAxis(along)], board[2]) * kDegPerRad;
}

float angleTilt(const float *board, AngleAlong along) {
	const float u = board[uAxis(along)];
	return atan2f(board[rAxis(along)], sqrtf(u * u + board[2] * board[2])) * kDegPerRad;
}

bool angleSteepWarn(float tiltDeg) { return fabsf(tiltDeg) >= ANGLE_STEEP_WARN_DEG; }

bool angleTooSteep(float tiltDeg) { return fabsf(tiltDeg) >= ANGLE_TOO_STEEP_DEG; }

float angleBetween(const float *a, const float *b) {
	const float cx = a[1] * b[2] - a[2] * b[1];
	const float cy = a[2] * b[0] - a[0] * b[2];
	const float cz = a[0] * b[1] - a[1] * b[0];
	const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	return atan2f(sqrtf(cx * cx + cy * cy + cz * cz), dot) * kDegPerRad;
}

/** @brief The normal's acceleration of a chip vector, in the board's sense. */
static float normalOf(const float *chip, const int8_t *axes) {
	float b[3];
	angleMapAxes(axes, chip, b);
	return b[2];
}

bool angleFlipSolve(const float *p1, const float *p2, const float *p3,
                    const int8_t *axes, AngleCal *out) {
	if (!out) return false;
	out->valid = false;
	// Written so that NaN, in a pose or from an axis map that is not one,
	// fails each test.
	if (!(normalOf(p1, axes) > ANGLE_FLIP_POSE_G) || !(normalOf(p2, axes) > ANGLE_FLIP_POSE_G) ||
	    !(normalOf(p3, axes) < -ANGLE_FLIP_POSE_G)) {
		return false;
	}
	const int n = (axes[2] < 0 ? -axes[2] : axes[2]) - 1;
	float s2 = 0.0f;
	for (int i = 0; i < 3; ++i) {
		if (i == n) continue;
		out->offG[i] = (p1[i] + p2[i]) / 2.0f;
		out->gain[i] = 1.0f;
		const float s = (p1[i] - p2[i]) / 2.0f;
		s2 += s * s;
	}
	// A slope of 90 deg or more gives an infinite or NaN gain, refused below.
	out->offG[n] = (p1[n] + p3[n]) / 2.0f;
	out->gain[n] = fabsf(p1[n] - p3[n]) / 2.0f / sqrtf(1.0f - s2);
	out->valid = true;
	if (!angleCalValid(out)) {
		out->valid = false;
		return false;
	}
	return true;
}

void angleFlipStart(AngleFlip *f) {
	if (!f) return;
	memset(f, 0, sizeof(*f));
	f->step = FlipStep::Up;
}

/** @brief A still stretch begins at @p chip; one that was open broke, a move. */
static void flipRestart(AngleFlip *f, const float *chip, uint32_t ms) {
	if (f->n > 0) f->moved = true;
	for (int i = 0; i < 3; ++i) {
		f->ref[i] = chip[i];
		f->sum[i] = chip[i];
	}
	f->n = 1;
	f->fromMs = ms;
	f->lastMs = ms;
}

void angleFlipFrame(AngleFlip *f, const float *chip, const int8_t *axes,
                    uint32_t ms) {
	if (!f || !chip) return;
	const float nz = normalOf(chip, axes);
	if (f->step == FlipStep::Back) {
		if (nz > ANGLE_FLIP_POSE_G) {
			f->step = angleFlipSolve(f->pose[0], f->pose[1], f->pose[2], axes, &f->result)
			              ? FlipStep::Done : FlipStep::Failed;
		}
		return;
	}
	if (f->step != FlipStep::Up && f->step != FlipStep::Turn && f->step != FlipStep::Down) return;
	const bool inPose = (f->step == FlipStep::Down) ? nz < -ANGLE_FLIP_POSE_G
	                                                : nz > ANGLE_FLIP_POSE_G;
	if (!inPose) {
		// Out of the pose: no stretch, and a move however short.
		f->n = 0;
		f->moved = true;
		return;
	}
	if (f->n == 0 || ms - f->lastMs >= IMU_STALE_MS || angleBetween(chip, f->ref) > IMU_STILL_DEG) {
		flipRestart(f, chip, ms);
		return;
	}
	for (int i = 0; i < 3; ++i) f->sum[i] += chip[i];
	++f->n;
	f->lastMs = ms;
	if (ms - f->fromMs < ANGLE_FLIP_HOLD_MS) return;
	if (f->step == FlipStep::Turn && !f->moved) return;
	const int k = static_cast<int>(f->step);
	for (int i = 0; i < 3; ++i) f->pose[k][i] = f->sum[i] / static_cast<float>(f->n);
	f->step = static_cast<FlipStep>(k + 1);
	f->moved = false;
	f->n = 0;
}

const char *angleTenthsText(char *buf, size_t n, float v, bool plus) {
	if (!buf || n == 0) return buf;
	if (!isfinite(v) || fabsf(v) > 99999.0f) {
		snprintf(buf, n, "---");
		return buf;
	}
	const long q = lroundf(v * 10.0f);
	const unsigned long a = static_cast<unsigned long>(q < 0 ? -q : q);
	snprintf(buf, n, "%s%lu.%lu", q < 0 ? "-" : (plus ? "+" : ""), a / 10ul, a % 10ul);
	return buf;
}

/** @brief The zero and MAX dropped, ZERO not armed, the filter to seed. */
static void restart(AngleModel *m) {
	imu_reseed(&m->imu);
	servo_rudder_init(&m->rud);
	m->zero = AngleZero::None;
}

void angleOpen(AngleModel *m, const AngleSetup *s) {
	if (!m) return;
	memset(m, 0, sizeof(*m));
	if (s) {
		m->set = *s;
	} else {
		angleSetupDefaults(&m->set);
	}
	imu_init(&m->imu);
	imu_set_filter(&m->imu, ANGLE_FILTER);
	servo_rudder_init(&m->rud);
	// The memset leaves ZERO at None and the flip at step 1.
}

/** @brief Whether the model's reading is settled now. */
static bool settled(const AngleModel *m) {
	const imu_reading_t *r = &m->imu.r;
	// imu_still_ms() is 0 while the reading is not valid.
	return !angleTooSteep(r->angle[IMU_AXIS_Y]) && imu_still_ms(r, IMU_AXIS_X) >= ANGLE_SETTLED_MS;
}

/** @brief ZERO armed for @ref ANGLE_ZERO_WAIT_MS by @p ms: timed out. */
static void zeroTimeout(AngleModel *m, uint32_t ms) {
	if (m->zero == AngleZero::Armed && ms - m->armMs >= ANGLE_ZERO_WAIT_MS) {
		m->zero = AngleZero::TimedOut;
	}
}

/** @brief A frame: chip axes, uncalibrated, g; its temperature, °C. */
static void frameIn(AngleModel *m, const float *chip, float tempC, uint32_t ms) {
	if (m->flipOn) angleFlipFrame(&m->flip, chip, m->set.axes, ms);
	m->tempC = tempC;
	m->haveTemp = true;
	float cal[3], b[3];
	angleCalApply(&m->set.cal, chip, cal);
	angleMapAxes(m->set.axes, cal, b);
	m->faceDown = b[2] < 0.0f;
	const float g = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
	if (!(g >= ANGLE_MIN_G)) {
		imu_tick(&m->imu, ms);
		zeroTimeout(m, ms);
		return;
	}
	imu_feed_angles(&m->imu, angleTheta(b, m->set.along), angleTilt(b, m->set.along), 0.0f, ms);
	zeroTimeout(m, ms);
	if (!settled(m)) return;
	const float theta = m->imu.r.angle[IMU_AXIS_X];
	if (m->zero == AngleZero::Armed && ms - m->armMs >= ANGLE_SETTLED_MS) {
		servo_rudder_zero(&m->rud, theta);
		m->zeroTempC = tempC;
		m->zero = AngleZero::Taken;
	}
	servo_rudder_track(&m->rud, servo_rudder_defl(&m->rud, theta));
}

void angleSample(AngleModel *m, const ImuSample *s) {
	if (!m || !s || s->seq == 0 || s->seq == m->lastSeq) return;
	if (s->takenMs - m->lastSampleMs >= IMU_STALE_MS) {
		m->count = 0;
	}
	if (m->count == 0) {
		memset(m->accSum, 0, sizeof(m->accSum));
		m->tempSum = 0;
	}
	m->lastSeq = s->seq;
	m->lastSampleMs = s->takenMs;
	for (int i = 0; i < 3; ++i) m->accSum[i] += s->acc[i];
	m->tempSum += s->tempRaw;
	if (++m->count < ANGLE_FRAME_SAMPLES) return;
	m->count = 0;
	const float k = static_cast<float>(ANGLE_FRAME_SAMPLES);
	float chip[3];
	for (int i = 0; i < 3; ++i) chip[i] = static_cast<float>(m->accSum[i]) / k / ANGLE_COUNTS_PER_G;
	frameIn(m, chip, static_cast<float>(m->tempSum) / k / ANGLE_COUNTS_PER_C, s->takenMs);
}

void angleTick(AngleModel *m, uint32_t nowMs) {
	if (!m) return;
	imu_tick(&m->imu, nowMs);
	zeroTimeout(m, nowMs);
}

/** @brief Whether LENGTH gives a travel. */
static bool lengthOk(uint16_t mm) {
	return mm >= SERVO_RUDDER_LEN_MIN_MM && mm <= SERVO_RUDDER_LEN_MAX_MM;
}

void angleRead(const AngleModel *m, AngleReading *out) {
	if (!out) return;
	memset(out, 0, sizeof(*out));
	out->deg = NAN;
	out->mm = NAN;
	if (!m) return;
	const imu_reading_t *r = &m->imu.r;
	const bool inv = m->set.invert;
	const bool len = lengthOk(m->set.lengthMm);
	const float L = static_cast<float>(m->set.lengthMm);
	out->valid = r->valid;
	out->tiltDeg = r->angle[IMU_AXIS_Y];
	out->steepWarn = r->valid && angleSteepWarn(out->tiltDeg);
	out->tooSteep = r->valid && angleTooSteep(out->tiltDeg);
	out->settled = settled(m);
	out->zeroed = m->rud.zeroed;
	out->hasDeg = r->valid && !out->tooSteep;
	if (out->hasDeg) {
		const float d = servo_rudder_defl(&m->rud, r->angle[IMU_AXIS_X]);
		// -180 turned is 180: kept as -180, as imu_deg() keeps it.
		out->deg = (inv && d > -180.0f) ? -d : d;
		out->hasMm = len;
		if (len) out->mm = servo_rudder_mm(out->deg, L);
	}
	for (int side = 0; side < 2; ++side) {
		const int raw = inv ? 1 - side : side;
		out->hasMax[side] = m->rud.have[raw];
		out->maxDeg[side] = (side == 0 ? -1.0f : 1.0f) * m->rud.max_deg[raw];
		out->hasMaxMm[side] = out->hasMax[side] && len;
		out->maxMm[side] = out->hasMaxMm[side] ? servo_rudder_mm(out->maxDeg[side], L) : 0.0f;
	}
	out->hasTemp = m->haveTemp;
	out->tempC = m->tempC;
	if (m->rud.zeroed) {
		out->tempSinceZero = m->tempC - m->zeroTempC;
		out->tempWarn = fabsf(out->tempSinceZero) >= ANGLE_TEMP_WARN_C;
	}
	out->faceDown = m->faceDown;
	out->zero = m->zero;
}

void angleZeroArm(AngleModel *m, uint32_t nowMs) {
	if (!m) return;
	m->zero = AngleZero::Armed;
	m->armMs = nowMs;
}

void angleResetMax(AngleModel *m) {
	if (m) servo_rudder_reset_max(&m->rud);
}

void angleSetAlong(AngleModel *m, AngleAlong along) {
	if (!m || m->set.along == along) return;
	m->set.along = along;
	restart(m);
}

void angleSetInvert(AngleModel *m, bool invert) {
	if (m) m->set.invert = invert;
}

void angleSetLength(AngleModel *m, uint16_t lengthMm) {
	if (m) m->set.lengthMm = lengthMm;
}

/** @brief Whether two calibrations apply the same: both none, or the same values. */
static bool calSame(const AngleCal *a, const AngleCal *b) {
	const bool on = angleCalValid(a);
	if (on != angleCalValid(b)) return false;
	if (!on) return true;
	for (int i = 0; i < 3; ++i) {
		if (a->offG[i] != b->offG[i] || a->gain[i] != b->gain[i]) return false;
	}
	return true;
}

void angleSetCal(AngleModel *m, const AngleCal *cal) {
	if (!m || calSame(&m->set.cal, cal)) return;
	if (cal) {
		m->set.cal = *cal;
	} else {
		m->set.cal.valid = false;
	}
	restart(m);
}

void angleFlipBegin(AngleModel *m) {
	if (!m) return;
	angleFlipStart(&m->flip);
	m->flipOn = true;
}

void angleFlipEnd(AngleModel *m) {
	if (m) m->flipOn = false;
}
