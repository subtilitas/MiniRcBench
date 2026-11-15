/**
 * @file angle_model.h
 * @brief The angle meter's model, pure: the chip's samples into four-sample
 *        frames, the calibration, the axis map, `theta` and the tilt from the
 *        accelerometer alone, the frames into rcbench's IMU (inertial
 *        measurement unit) model, ZERO, MAX, the travel in mm, the temperature
 *        since the zero and the steps of the three-pose flip.
 *
 * Nothing here touches hardware. The driver (qmi8658.h) gives the samples;
 * the screen gives each new one to angleSample() and reads angleRead().
 *
 * A sample's acceleration is in the chip's axes, X, Y and Z. Four samples are
 * averaged into a frame, about 7.8 Hz at 31.25 Hz. The frame is converted to
 * g, calibrated per chip axis (`a' = (a - o) / k`), and mapped to the board's
 * axes by @ref BOARD_IMU_AXES: the long edge, the short edge and the normal.
 * ALONG AXIS names the edge along the axis of the measurement, `r`; the other
 * in-plane edge is `u` and the normal `z`. Then
 *
 *     theta = atan2(a_u, a_z)                   about r
 *     tilt  = atan2(a_r, sqrt(a_u^2 + a_z^2))   of r against the horizontal
 *
 * `theta` is positive when the board's `+u` edge rises, the tilt when its
 * `+r` end rises. Each frame goes into rcbench's imu.c as one angle,
 * imu_feed_angles(): `theta` on X, the tilt on Y, 0 on Z, through its
 * low-pass at @ref ANGLE_FILTER, its still time and its age. The reading is
 * settled while the still time of X is @ref ANGLE_SETTLED_MS or more and the
 * axis is not too steep. ZERO and MAX are rcbench's servo_rudder.c: a
 * deflection from the zero wrapped to -180° to 180°, the largest settled one
 * each side, the travel `L x sin(theta)` at the trailing edge.
 *
 * SIGN turns the reading and swaps the MAX sides; the zero and MAX are kept
 * unturned, so SIGN can change without losing them. A new ALONG AXIS or a new
 * calibration changes what `theta` is: both drop the zero and MAX and seed
 * the filter afresh.
 *
 * @see docs/design/angle-meter.md
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "imu.h"
#include "qmi8658.h"
#include "servo_rudder.h"

/**
 * @defgroup angle_rules Rules
 * @{
 */

/** @brief Counts a g at ±2 g, the driver's range (QMI8658 CTRL2 aFS 000). */
#define ANGLE_COUNTS_PER_G     16384.0f
/** @brief Counts a °C of the temperature registers. */
#define ANGLE_COUNTS_PER_C     256.0f
/** @brief Samples averaged into one frame. */
#define ANGLE_FRAME_SAMPLES    4u
/** @brief IMU FILTER of rcbench's model: each frame moves the filtered angle a third of the rest. */
#define ANGLE_FILTER           3
/** @brief The still time of `theta` that makes a reading settled, ms. */
#define ANGLE_SETTLED_MS       1000u
/** @brief The tilt from which the strip turns amber, °. */
#define ANGLE_STEEP_WARN_DEG   45.0f
/** @brief The tilt from which the reading is `---` with `TOO STEEP`, °. */
#define ANGLE_TOO_STEEP_DEG    70.0f
/** @brief A frame of less acceleration than this, in g, has no direction to take and is dropped. */
#define ANGLE_MIN_G            0.5f
/** @brief A change of the IMU's temperature since the zero from which it warns, °C. */
#define ANGLE_TEMP_WARN_C      2.0f
/** @brief ZERO waits this long for a settled reading after the finger lifts, ms. */
#define ANGLE_ZERO_WAIT_MS     10000u
/** @brief A pose of the flip is the mean of the frames of a still stretch this long, ms. */
#define ANGLE_FLIP_HOLD_MS     2000u
/** @brief The normal's acceleration a flip pose needs, g: above it screen up, below its negative screen down. */
#define ANGLE_FLIP_POSE_G      0.8f
/** @brief The largest offset a calibration takes, g: twice the datasheets' ±100 mg. */
#define ANGLE_CAL_OFFSET_MAX_G 0.2f
/** @brief The smallest gain a calibration takes: twice the datasheets' -6 %. */
#define ANGLE_CAL_GAIN_MIN     0.88f
/** @brief The largest gain a calibration takes: twice the datasheets' +6 %. */
#define ANGLE_CAL_GAIN_MAX     1.12f
/** @brief LENGTH at `OFF`: no travel in mm. */
#define ANGLE_LEN_OFF          0u

/** @} */

/** @brief ALONG AXIS: the board's edge that lies along the axis of the measurement. */
enum class AngleAlong : uint8_t {
	Long,   /**< `LONG`, the default: `r` is the long edge, `u` the short edge. */
	Short,  /**< `SHORT`: `r` is the short edge, `u` the long edge. */
};

/** @brief A calibration, per chip axis X, Y and Z: `a' = (a - o) / k`. */
struct AngleCal {
	bool  valid;    /**< A calibration was kept; false: none applied. */
	float offG[3];  /**< The offset `o` of each chip axis, g. */
	float gain[3];  /**< The gain `k` of each chip axis; the flip finds the normal's, the others stay 1. */
};

/** @brief What SETUP and the board set: ALONG AXIS, SIGN, LENGTH, the calibration, the axis map. */
struct AngleSetup {
	AngleAlong along;     /**< ALONG AXIS. */
	bool       invert;    /**< SIGN at `INVERT`: the reading turned. */
	uint16_t   lengthMm;  /**< LENGTH, hinge line to trailing edge, mm; @ref ANGLE_LEN_OFF for `OFF`. */
	AngleCal   cal;       /**< The calibration. */
	int8_t     axes[3];   /**< The chip axis along the long edge, the short edge and the normal, ±1 to ±3 (@ref BOARD_IMU_AXES). */
};

/** @brief ZERO's state. */
enum class AngleZero : uint8_t {
	None,      /**< Not armed since the visit began. */
	Armed,     /**< Armed: the next settled reading is the zero, `ZERO WHEN SETTLED: HOLD STILL`. */
	Taken,     /**< The zero was taken. */
	TimedOut,  /**< None settled within @ref ANGLE_ZERO_WAIT_MS: `NOT SETTLED IN 10 S: NO ZERO`, the old zero kept. */
};

/** @brief The flip's step. */
enum class FlipStep : uint8_t {
	Up,      /**< Step 1: screen up, still for @ref ANGLE_FLIP_HOLD_MS: P1. */
	Turn,    /**< Step 2: turned end for end, moved, then still: P2. */
	Down,    /**< Step 3: screen down, still: P3. */
	Back,    /**< Step 3 taken: waiting for screen up again. */
	Done,    /**< The result is in AngleFlip::result. */
	Failed,  /**< The poses gave no calibration within the limits. */
};

/** @brief The flip's state: the step, the still stretch it is in, the poses. */
struct AngleFlip {
	FlipStep step;        /**< The step. */
	bool     moved;       /**< A still stretch broke since the last pose. */
	float    ref[3];      /**< The stretch's first frame, chip axes, g. */
	float    sum[3];      /**< The sum of the stretch's frames, chip axes, g. */
	uint32_t n;           /**< Frames in the stretch; 0 for none. */
	uint32_t fromMs;      /**< The stretch's first frame's time. */
	uint32_t lastMs;      /**< The stretch's last frame's time. */
	float    pose[3][3];  /**< P1, P2 and P3, the means, chip axes, uncalibrated, g. */
	AngleCal result;      /**< The calibration, once Done. */
};

/** @brief The reading as the screen shows it. */
struct AngleReading {
	bool  valid;         /**< A frame within IMU_STALE_MS. */
	float tiltDeg;       /**< The filtered tilt of `r`, °. */
	bool  steepWarn;     /**< The tilt @ref ANGLE_STEEP_WARN_DEG or more: amber. */
	bool  tooSteep;      /**< The tilt @ref ANGLE_TOO_STEEP_DEG or more: `TOO STEEP`. */
	bool  settled;       /**< `SETTLED`: valid, not too steep, still for @ref ANGLE_SETTLED_MS. */
	bool  zeroed;        /**< A zero is taken: the reading is from it, else from level. */
	bool  hasDeg;        /**< The angle shows: valid and not too steep. */
	float deg;           /**< The angle from the zero or level, SIGN applied, -180° to 180°; NaN without hasDeg. */
	bool  hasMm;         /**< The travel shows: hasDeg and LENGTH 10 to 500 mm. */
	float mm;            /**< The trailing edge's travel, mm; NaN without hasMm. */
	bool  hasMax[2];     /**< A MAX each side, [0] negative, [1] positive, as shown. */
	float maxDeg[2];     /**< The MAX each side, signed, °. */
	bool  hasMaxMm[2];   /**< Its travel shows: a MAX that side and LENGTH 10 to 500 mm. */
	float maxMm[2];      /**< The MAX's travel each side, signed, mm. */
	bool  hasTemp;       /**< A frame gave a temperature. */
	float tempC;         /**< The IMU's temperature, °C, the last frame's mean. */
	float tempSinceZero; /**< The change since the zero, °C; 0 while not zeroed. */
	bool  tempWarn;      /**< Zeroed and the change @ref ANGLE_TEMP_WARN_C or more either way. */
	bool  faceDown;      /**< The last frame's normal below 0: screen down, touches ignored. */
	AngleZero zero;      /**< ZERO's state. */
};

/** @brief The model of one visit to the meter. */
struct AngleModel {
	AngleSetup     set;           /**< ALONG AXIS, SIGN, LENGTH, the calibration, the axis map. */
	imu_t          imu;           /**< rcbench's IMU model: `theta` on X, the tilt on Y. */
	servo_rudder_t rud;           /**< rcbench's zero and MAX, in `theta`'s own sign. */
	int32_t        accSum[3];     /**< The frame's samples so far, summed, counts. */
	int32_t        tempSum;       /**< Their temperatures, summed, counts. */
	uint8_t        count;         /**< Samples in the frame so far. */
	uint32_t       lastSeq;       /**< The last sample taken; 0 for none this visit. */
	uint32_t       lastSampleMs;  /**< Its time. */
	bool           haveTemp;      /**< A frame gave a temperature. */
	float          tempC;         /**< The last frame's temperature, °C. */
	float          zeroTempC;     /**< The temperature of the frame the zero came from, °C. */
	bool           faceDown;      /**< The last frame's normal below 0. */
	AngleZero      zero;          /**< ZERO's state. */
	uint32_t       armMs;         /**< When ZERO was armed. */
	bool           flipOn;        /**< The flip takes the frames. */
	AngleFlip      flip;          /**< The flip. */
};

/**
 * @defgroup angle_pure Pure functions
 * @{
 */

/**
 * @brief ALONG AXIS `LONG`, SIGN `NORMAL`, LENGTH `OFF`, no calibration, the
 *        axis map of @ref BOARD_IMU_AXES.
 * @param s The setup; NULL does nothing.
 */
void angleSetupDefaults(AngleSetup *s);

/**
 * @brief Whether @p cal is applied: kept, every value finite, each offset
 *        within @ref ANGLE_CAL_OFFSET_MAX_G and each gain within
 *        @ref ANGLE_CAL_GAIN_MIN to @ref ANGLE_CAL_GAIN_MAX.
 * @param cal The calibration; NULL is none.
 * @return true when it is applied.
 */
bool angleCalValid(const AngleCal *cal);

/**
 * @brief `a' = (a - o) / k` per chip axis; @p chip unchanged for a
 *        calibration angleCalValid() refuses.
 * @param cal  The calibration.
 * @param chip The acceleration, chip axes, g.
 * @param out  Takes the calibrated acceleration; may be @p chip.
 */
void angleCalApply(const AngleCal *cal, const float chip[3], float out[3]);

/**
 * @brief The chip's axes onto the board's: long edge, short edge, normal.
 * @param axes  For each board axis the chip axis along it, ±1 to ±3, negative
 *              where it points the other way; another value gives NaN.
 * @param chip  The acceleration, chip axes.
 * @param board Takes it in the board's axes.
 */
void angleMapAxes(const int8_t axes[3], const float chip[3], float board[3]);

/**
 * @brief `theta = atan2(a_u, a_z)`: the rotation about `r`.
 * @param board The acceleration, board axes, g.
 * @param along ALONG AXIS.
 * @return `theta`, °, -180 to 180.
 */
float angleTheta(const float board[3], AngleAlong along);

/**
 * @brief `tilt = atan2(a_r, sqrt(a_u^2 + a_z^2))`: `r` against the horizontal.
 * @param board The acceleration, board axes, g.
 * @param along ALONG AXIS.
 * @return The tilt, °, -90 to 90.
 */
float angleTilt(const float board[3], AngleAlong along);

/** @brief Whether a tilt shows amber. @param tiltDeg The tilt, °. @return From @ref ANGLE_STEEP_WARN_DEG either way. */
bool angleSteepWarn(float tiltDeg);

/** @brief Whether a tilt is too steep to read. @param tiltDeg The tilt, °. @return From @ref ANGLE_TOO_STEEP_DEG either way. */
bool angleTooSteep(float tiltDeg);

/**
 * @brief The angle in degrees between two vectors.
 * @param a A vector.
 * @param b Another.
 * @return 0 to 180, °.
 */
float angleBetween(const float a[3], const float b[3]);

/**
 * @brief The flip's calibration from its three poses, chip axes,
 *        uncalibrated, g. With `n` the chip axis along the normal and `p`
 *        each in-plane one: `o_p = (P1p + P2p) / 2`, `o_n = (P1n + P3n) / 2`,
 *        and the normal's gain `|P1n - P3n| / 2` divided by the cosine of
 *        the surface's slope, whose sine is the length of `(P1p - P2p) / 2`.
 *        The in-plane gains stay 1.
 * @param p1   Screen up.
 * @param p2   Screen up, turned end for end.
 * @param p3   Screen down.
 * @param axes The axis map.
 * @param out  Takes the calibration, valid only on success.
 * @return false for a value that is not finite, a pose whose normal is not
 *         past @ref ANGLE_FLIP_POSE_G the right way, a slope of 90° or more,
 *         or a result angleCalValid() refuses.
 */
bool angleFlipSolve(const float p1[3], const float p2[3], const float p3[3],
                    const int8_t axes[3], AngleCal *out);

/**
 * @brief The flip from step 1, no pose taken.
 * @param f The flip; NULL does nothing.
 */
void angleFlipStart(AngleFlip *f);

/**
 * @brief One frame into the flip. A frame within IMU_STILL_DEG of the still
 *        stretch's first, in the step's pose and less than IMU_STALE_MS after
 *        the last, adds to the stretch; any other starts a new one, or none
 *        for a frame out of the pose. A stretch of @ref ANGLE_FLIP_HOLD_MS is
 *        the step's pose, step 2's only after a stretch broke since P1. In
 *        step `Back` a frame screen up solves the poses.
 * @param f    The flip.
 * @param chip The frame, chip axes, uncalibrated, g.
 * @param axes The axis map.
 * @param ms   The frame's time.
 */
void angleFlipFrame(AngleFlip *f, const float chip[3], const int8_t axes[3],
                    uint32_t ms);

/**
 * @brief A value in tenths, rounded half away from zero: "-12.4", "+0.0"
 *        with @p plus, never "-0.0".
 * @param buf  Takes the text.
 * @param n    Its size, bytes.
 * @param v    The value.
 * @param plus A '+' before a value that is not negative.
 * @return @p buf; "---" for a value that is not finite or beyond ±99999.
 */
const char *angleTenthsText(char *buf, size_t n, float v, bool plus);

/** @} */

/**
 * @defgroup angle_visit A visit
 * @{
 */

/**
 * @brief A visit begins: no zero, no MAX, the filter to seed, no samples.
 * @param m The model; NULL does nothing.
 * @param s The setup; NULL takes angleSetupDefaults().
 */
void angleOpen(AngleModel *m, const AngleSetup *s);

/**
 * @brief A sample from the driver. Taken once per sequence number: seq 0
 *        and the last one taken are not. A sample IMU_STALE_MS or more
 *        after the last drops the frame begun and begins a new one. Every
 *        @ref ANGLE_FRAME_SAMPLES samples make a frame, at the last one's
 *        time: calibrated, mapped, into the flip while it runs, and, with
 *        @ref ANGLE_MIN_G or more, into the IMU model, ZERO and MAX.
 * @param m The model; NULL does nothing.
 * @param s The sample; NULL does nothing.
 */
void angleSample(AngleModel *m, const ImuSample *s);

/**
 * @brief Every UI frame: the reading ages, and ZERO armed for
 *        @ref ANGLE_ZERO_WAIT_MS times out.
 * @param m     The model; NULL does nothing.
 * @param nowMs millis() now.
 */
void angleTick(AngleModel *m, uint32_t nowMs);

/**
 * @brief The reading now.
 * @param m   The model.
 * @param out Takes it; NULL does nothing.
 */
void angleRead(const AngleModel *m, AngleReading *out);

/**
 * @brief ZERO, on the finger's lift: the zero is taken from the first frame
 *        at least @ref ANGLE_SETTLED_MS later that is settled, so the board
 *        has been still for that second; none within
 *        @ref ANGLE_ZERO_WAIT_MS keeps the old zero. Taking it clears MAX.
 * @param m     The model; NULL does nothing.
 * @param nowMs millis() at the lift.
 */
void angleZeroArm(AngleModel *m, uint32_t nowMs);

/** @brief RESET MAX: both sides cleared. @param m The model; NULL does nothing. */
void angleResetMax(AngleModel *m);

/**
 * @brief ALONG AXIS. A change drops the zero and MAX and seeds the filter.
 * @param m     The model; NULL does nothing.
 * @param along The edge along the axis.
 */
void angleSetAlong(AngleModel *m, AngleAlong along);

/**
 * @brief SIGN: turns the reading and swaps the MAX sides; the zero and MAX stay.
 * @param m      The model; NULL does nothing.
 * @param invert `INVERT`.
 */
void angleSetInvert(AngleModel *m, bool invert);

/**
 * @brief LENGTH, mm; @ref ANGLE_LEN_OFF, or a value outside
 *        SERVO_RUDDER_LEN_MIN_MM to SERVO_RUDDER_LEN_MAX_MM, gives no travel.
 * @param m        The model; NULL does nothing.
 * @param lengthMm The length.
 */
void angleSetLength(AngleModel *m, uint16_t lengthMm);

/**
 * @brief A new calibration: drops the zero and MAX and seeds the filter.
 * @param m   The model; NULL does nothing.
 * @param cal The calibration; NULL for none.
 */
void angleSetCal(AngleModel *m, const AngleCal *cal);

/** @brief FLIP CAL opens: the frames go to the flip from step 1 as well. @param m The model; NULL does nothing. */
void angleFlipBegin(AngleModel *m);

/** @brief FLIP CAL closes: the flip takes no more frames. @param m The model; NULL does nothing. */
void angleFlipEnd(AngleModel *m);

/** @}*/
