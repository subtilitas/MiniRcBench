/**
 * @file qmi8658.cpp
 * @brief The QMI8658 on the shared I2C1 bus: probe, reset, configuration,
 *        the burst and its time stamp check, start and stop. @see qmi8658.h
 */

#include "qmi8658.h"
#include "board_desc.h"
#include "board_pins.h"
#include "sense.h"

#include <hardware/i2c.h>

#include <string.h>

static_assert(qmi8658AxesValid(BOARD_IMU_AXES), "BOARD_IMU_AXES names each chip axis once");

// ---------------------------------------------------------------------------
// codec
// ---------------------------------------------------------------------------

uint32_t qmi8658Stamp(const uint8_t *b) {
	return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
}

/** @brief A little-endian signed 16-bit value. */
static int16_t le16(const uint8_t *b) {
	return (int16_t)(uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

void qmi8658Decode(const uint8_t *b, ImuSample *out) {
	out->stamp   = qmi8658Stamp(b);
	out->tempRaw = le16(b + 3);
	for (int i = 0; i < 3; i++) out->acc[i] = le16(b + 5 + 2 * i);
}

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

/** @brief Where the driver is. */
enum class Phase : uint8_t {
	Closed,     /**< Nothing is sent. */
	Probe,      /**< WHO_AM_I, REVISION_ID and the reset, due at s_probeAt. */
	ResetWait,  /**< dQY_L, due @ref QMI8658_RESET_A_MS after the reset. */
	BootWait,   /**< WHO_AM_I, due @ref QMI8658_RESET_C_MS after the reset. */
	Warmup,     /**< The first @ref QMI8658_WARMUP_MS after CTRL7: nothing read. */
	Run,        /**< A burst and the time stamp every frame. */
};

static Phase     s_phase = Phase::Closed;
static ImuState  s_state = ImuState::Closed;
static uint32_t  s_probeAt = 0;        /**< When the probe is due. */
static uint32_t  s_resetAt = 0;        /**< When the last reset was written. */
static bool      s_resetEver = false;  /**< A reset has been written since power-on. */
static uint32_t  s_enAt = 0;           /**< When CTRL7 turned the accelerometer on. */
static bool      s_accelMayBeOn = false;  /**< CTRL7 was written since the last reset. */
static uint8_t   s_fails = 0;          /**< Failed transactions in a row, online. */
static bool      s_haveStamp = false;  /**< s_latest holds a sample of this start. */
static uint8_t   s_who = 0;
static uint8_t   s_rev = 0;
static ImuSample s_latest;
static ImuCounts s_counts;

// ---------------------------------------------------------------------------
// the wire
// ---------------------------------------------------------------------------

static i2c_inst_t *bus() { return (i2c_inst_t *)g_board->i2c; }

/** @brief Whether the shared bus record shows the bus stuck: nothing is sent. */
static bool stuck() { return senseBus()->stuck; }

/**
 * @brief @p n bytes from register @p reg: the register address, a repeated
 *        start, the bytes. Each transfer noted to the shared bus record.
 * @return SENSE_STUCK with nothing sent while the bus is stuck, else what
 *         senseNote() made of the first transfer that failed, or SENSE_OK.
 */
static sense_err_t readReg(uint8_t reg, uint8_t *buf, size_t n) {
	if (stuck()) return SENSE_STUCK;
	sense_err_t e = senseNote(
		i2c_write_timeout_us(bus(), QMI8658_I2C_ADDR, &reg, 1, true, SENSE_TIMEOUT_US), 1);
	if (e != SENSE_OK) return e;
	return senseNote(i2c_read_timeout_us(bus(), QMI8658_I2C_ADDR, buf, n, false, SENSE_TIMEOUT_US),
	                 n);
}

/**
 * @brief One byte to register @p reg in one transfer: the QMI8658A takes no
 *        burst write to its configuration registers.
 * @return As readReg().
 */
static sense_err_t writeReg(uint8_t reg, uint8_t v) {
	if (stuck()) return SENSE_STUCK;
	uint8_t b[2] = { reg, v };
	return senseNote(i2c_write_timeout_us(bus(), QMI8658_I2C_ADDR, b, 2, false, SENSE_TIMEOUT_US),
	                 2);
}

/**
 * @brief Whether @p e is a failure on the wire, counted. SENSE_OK is none,
 *        and SENSE_STUCK is none either: nothing was sent.
 */
static bool failed(sense_err_t e) {
	if (e == SENSE_OK || e == SENSE_STUCK) return false;
	s_counts.fails++;
	return true;
}

// ---------------------------------------------------------------------------
// the steps
// ---------------------------------------------------------------------------

/**
 * @brief The earliest time a probe may start from @p t: no reset sooner than
 *        @ref QMI8658_RESET_GAP_MS after the last one.
 */
static uint32_t probeFrom(uint32_t t) {
	if (s_resetEver && (int32_t)(s_resetAt + QMI8658_RESET_GAP_MS - t) > 0)
		return s_resetAt + QMI8658_RESET_GAP_MS;
	return t;
}

/** @brief A start that did not end online: @p st, and a new start @ref QMI8658_RETRY_MS on. */
static void startFailed(uint32_t nowMs, ImuState st) {
	s_state = st;
	s_phase = Phase::Probe;
	s_probeAt = probeFrom(nowMs + QMI8658_RETRY_MS);
}

/**
 * @brief A step of the start that did not answer: `NO IMU` and a new start
 *        for a failure on the wire; nothing for a stuck bus, whose step is
 *        tried again on the next frame.
 * @return true when the step goes on: @p e is SENSE_OK.
 */
static bool startStep(sense_err_t e, uint32_t nowMs) {
	if (e == SENSE_OK) return true;
	if (failed(e)) startFailed(nowMs, ImuState::Absent);
	return false;
}

/** @brief WHO_AM_I, REVISION_ID, then the reset. */
static void probe(uint32_t nowMs) {
	uint8_t v = 0;
	if (!startStep(readReg(QMI8658_REG_WHO_AM_I, &v, 1), nowMs)) return;
	s_who = v;
	// Another part at the address gets nothing but the register address.
	if (v != QMI8658_WHO_AM_I_VALUE) return startFailed(nowMs, ImuState::WrongId);
	if (!startStep(readReg(QMI8658_REG_REVISION, &v, 1), nowMs)) return;
	s_rev = v;
	// The reset counts from its attempt: a write that failed may still have
	// reached the part.
	s_resetAt = nowMs;
	s_resetEver = true;
	s_counts.resets++;
	if (!startStep(writeReg(QMI8658_REG_RESET, QMI8658_RESET_CMD), nowMs)) return;
	s_accelMayBeOn = false;
	s_state = ImuState::Starting;
	s_phase = Phase::ResetWait;
}

/** @brief The four configuration registers, in the order they are written. */
static const uint8_t CONFIG[4][2] = {
	{ QMI8658_REG_CTRL1, QMI8658_CTRL1_VALUE },
	{ QMI8658_REG_CTRL2, QMI8658_CTRL2_VALUE },
	{ QMI8658_REG_CTRL5, QMI8658_CTRL5_VALUE },
	{ QMI8658_REG_CTRL7, QMI8658_CTRL7_ACCEL },
};

/** @brief Each register written alone and read back; CTRL7 last. */
static void configure(uint32_t nowMs) {
	for (const auto &c : CONFIG) {
		if (c[0] == QMI8658_REG_CTRL7) s_accelMayBeOn = true;
		if (!startStep(writeReg(c[0], c[1]), nowMs)) return;
		uint8_t v = 0;
		if (!startStep(readReg(c[0], &v, 1), nowMs)) return;
		if (v != c[1]) return startFailed(nowMs, ImuState::Absent);
	}
	s_enAt = nowMs;
	s_fails = 0;
	s_haveStamp = false;
	s_phase = Phase::Warmup;
}

/**
 * @brief A transaction of the online part that did not answer: counted, and
 *        offline after @ref QMI8658_FAILS in a row; nothing for a stuck bus.
 * @return true when the frame goes on: @p e is SENSE_OK.
 */
static bool runStep(sense_err_t e, uint32_t nowMs) {
	if (e == SENSE_OK) return true;
	if (failed(e) && ++s_fails >= QMI8658_FAILS) startFailed(nowMs, ImuState::Offline);
	return false;
}

/** @brief The burst, the time stamp again, and the sample taken when it is new and whole. */
static void readFrame(uint32_t nowMs) {
	uint8_t b[QMI8658_BURST_BYTES], t[QMI8658_STAMP_BYTES];
	if (!runStep(readReg(QMI8658_REG_TIMESTAMP_L, b, sizeof(b)), nowMs)) return;
	if (!runStep(readReg(QMI8658_REG_TIMESTAMP_L, t, sizeof(t)), nowMs)) return;
	s_fails = 0;
	s_state = ImuState::Online;
	uint32_t stamp = qmi8658Stamp(b);
	if (qmi8658Stamp(t) != stamp) {
		// A new sample arrived during the burst: its bytes may mix two.
		s_counts.torn++;
		return;
	}
	if (s_haveStamp && stamp == s_latest.stamp) {
		s_counts.repeats++;
		return;
	}
	qmi8658Decode(b, &s_latest);
	s_latest.takenMs = nowMs;
	if (++s_latest.seq == 0) s_latest.seq = 1;
	s_haveStamp = true;
	s_counts.samples++;
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void qmi8658Start(uint32_t nowMs) {
	if (s_phase != Phase::Closed) return;
	memset(&s_latest, 0, sizeof(s_latest));
	memset(&s_counts, 0, sizeof(s_counts));
	s_who = 0;
	s_rev = 0;
	s_state = ImuState::Starting;
	s_phase = Phase::Probe;
	s_probeAt = probeFrom(nowMs);
}

void qmi8658Stop() {
	// One write, its result noted and counted; a stuck bus sends nothing,
	// and the next opening's reset turns the accelerometer off.
	if (s_accelMayBeOn) failed(writeReg(QMI8658_REG_CTRL7, QMI8658_CTRL7_OFF));
	s_accelMayBeOn = false;
	s_phase = Phase::Closed;
	s_state = ImuState::Closed;
}

void qmi8658Tick(uint32_t nowMs) {
	// While the bus is stuck each step's first transfer returns SENSE_STUCK
	// unsent, and the step is tried again on the next frame.
	switch (s_phase) {
		case Phase::Closed:
			return;
		case Phase::Probe:
			if ((int32_t)(nowMs - s_probeAt) >= 0) probe(nowMs);
			return;
		case Phase::ResetWait: {
			if (nowMs - s_resetAt < QMI8658_RESET_A_MS) return;
			// Read before CTRL7 is written: enabling the sensors may overwrite it.
			uint8_t v = 0;
			sense_err_t e = readReg(QMI8658_REG_DQY_L, &v, 1);
			if (e == SENSE_STUCK) return;
			if (failed(e) || v != QMI8658_RESET_DONE) {
				// Another value or no answer: the C part, or a reset not
				// ended. Wait out the C part's turn-on.
				s_phase = Phase::BootWait;
				return;
			}
			configure(nowMs);
			return;
		}
		case Phase::BootWait: {
			if (nowMs - s_resetAt < QMI8658_RESET_C_MS) return;
			uint8_t v = 0;
			if (!startStep(readReg(QMI8658_REG_WHO_AM_I, &v, 1), nowMs)) return;
			s_who = v;
			if (v != QMI8658_WHO_AM_I_VALUE) return startFailed(nowMs, ImuState::WrongId);
			configure(nowMs);
			return;
		}
		case Phase::Warmup:
			if (nowMs - s_enAt < QMI8658_WARMUP_MS) return;
			s_phase = Phase::Run;
			readFrame(nowMs);
			return;
		case Phase::Run:
			readFrame(nowMs);
			return;
	}
}

ImuState qmi8658State() { return s_state; }
uint8_t qmi8658WhoAmI() { return s_who; }
uint8_t qmi8658Revision() { return s_rev; }
const ImuSample *qmi8658Latest() { return &s_latest; }
const ImuCounts *qmi8658Counts() { return &s_counts; }
