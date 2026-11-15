/**
 * @file qmi8658.h
 * @brief The board's IMU (inertial measurement unit), a QMI8658 at
 *        @ref QMI8658_I2C_ADDR on the shared I2C (Inter-Integrated Circuit)
 *        bus I2C1, read on core0 once per UI (user interface) frame after the
 *        touch read and the current sensor, and only while the angle meter is
 *        open.
 *
 * The angle meter opens the driver with qmi8658Start() and closes it with
 * qmi8658Stop(); qmi8658Tick() runs every frame from uiTick() and sends
 * nothing while the driver is closed. Opening it reads WHO_AM_I and
 * REVISION_ID, writes a soft reset, waits for the reset to end, writes CTRL1,
 * CTRL2, CTRL5 and CTRL7 one byte each and reads each back, and drops the
 * samples of the first @ref QMI8658_WARMUP_MS. It reads nothing for that time,
 * then takes the time stamp of the first whole burst as the baseline, so the
 * first sample taken is latched after the warm-up. Closing it writes CTRL7
 * 0x00, which turns the accelerometer off. The gyroscope stays off
 * throughout.
 *
 * Each frame reads one burst of 11 bytes from TIMESTAMP_LOW (0x30) to AZ_H
 * (0x3A), then the time stamp again. A burst whose time stamp differs from
 * the one read after it is dropped: a sample changed during the burst. A
 * burst whose time stamp equals the last one taken is the same sample and is
 * not counted again.
 *
 * Errors: each transfer's result goes to senseNote() (sense.h), which maps it
 * as the current sensor's own and notes it to the shared bus record. While
 * that record shows the bus stuck the driver sends nothing; the current
 * sensor's tick runs the recovery and the touch read tests the bus.
 * @ref QMI8658_FAILS failed transactions in a row take the IMU offline, and it
 * is probed again every @ref QMI8658_RETRY_MS.
 *
 * Register values and timings are from QST's QMI8658A datasheet (Rev A) and
 * QMI8658C datasheet (Rev 0.6) as docs/design/angle-meter.md gives them; none
 * is verified on the board.
 *
 * @see docs/design/angle-meter.md
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup qmi8658_regs QMI8658 registers and values
 * @{
 */

#define QMI8658_REG_WHO_AM_I    0x00u  /**< WHO_AM_I: 0x05 on the A and the C part. */
#define QMI8658_REG_REVISION    0x01u  /**< REVISION_ID: read and kept, not checked. */
#define QMI8658_REG_CTRL1       0x02u  /**< CTRL1: serial interface and pins. */
#define QMI8658_REG_CTRL2       0x03u  /**< CTRL2: accelerometer range and ODR (output data rate). */
#define QMI8658_REG_CTRL5       0x06u  /**< CTRL5: low-pass filters. */
#define QMI8658_REG_CTRL7       0x08u  /**< CTRL7: sensors on and off. */
#define QMI8658_REG_TIMESTAMP_L 0x30u  /**< TIMESTAMP_LOW: the first byte of the burst. */
#define QMI8658_REG_DQY_L       0x4Du  /**< dQY_L: 0x80 after a reset that ended, A part only. */
#define QMI8658_REG_RESET       0x60u  /**< RESET: @ref QMI8658_RESET_CMD starts a soft reset. */

#define QMI8658_WHO_AM_I_VALUE  0x05u  /**< WHO_AM_I of a QMI8658. */
#define QMI8658_RESET_CMD       0xB0u  /**< The soft reset, as the A datasheet's RESET register gives it. */
#define QMI8658_RESET_DONE      0x80u  /**< dQY_L after a reset that ended. */

/** @brief CTRL1: address auto-increment on (bit 6), little-endian (bit 5 at 0), INT1 and INT2 off. */
#define QMI8658_CTRL1_VALUE     0x40u
/** @brief CTRL2: ±2 g (aFS 000), ODR 31.25 Hz (aODR 1000), no self-test. */
#define QMI8658_CTRL2_VALUE     0x08u
/** @brief CTRL5: accelerometer LPF (low-pass filter) on (bit 0), mode 11 (bits 2 to 1). */
#define QMI8658_CTRL5_VALUE     0x07u
/** @brief CTRL7: accelerometer on, gyroscope off. */
#define QMI8658_CTRL7_ACCEL     0x01u
/** @brief CTRL7: both sensors off, written on closing. */
#define QMI8658_CTRL7_OFF       0x00u

/** @brief Bytes of the burst: 3 of time stamp, 2 of temperature, 6 of acceleration. */
#define QMI8658_BURST_BYTES     11u
/** @brief Bytes of the time stamp read after the burst. */
#define QMI8658_STAMP_BYTES     3u

/** @} */

/**
 * @defgroup qmi8658_timing Timing and error rules
 * @{
 */

/** @brief The A part's reset time, ms: dQY_L is read this long after the reset. */
#define QMI8658_RESET_A_MS      15u
/** @brief The C part's system turn-on, ms: with no 0x80 in dQY_L, the wait from the reset. */
#define QMI8658_RESET_C_MS      1750u
/** @brief The shortest time between two resets, ms. */
#define QMI8658_RESET_GAP_MS    2000u
/**
 * @brief Nothing read for this long after CTRL7 turns the accelerometer on,
 *        ms: its turn-on, 3 ms + 3/ODR, is 99 ms. The first whole burst after
 *        it is the baseline, so the first sample taken is latched after it.
 */
#define QMI8658_WARMUP_MS       100u
/** @brief Failed transactions in a row that take the IMU offline. */
#define QMI8658_FAILS           3u
/** @brief A part not answering, another part, or an IMU offline, probed again this often, ms. */
#define QMI8658_RETRY_MS        1000u

/** @} */

/** @brief The IMU as the angle meter sees it. */
enum class ImuState : uint8_t {
	Closed,    /**< The meter is closed: nothing is sent to the IMU. */
	Starting,  /**< Probing, resetting, configuring, warming up, or before the first frame that answers. */
	Absent,    /**< No answer, or the configuration did not read back: `NO IMU`. */
	WrongId,   /**< WHO_AM_I read another value: `NOT A QMI8658`, nothing written. */
	Online,    /**< Configured and answering: the burst and the time stamp are read every frame. */
	Offline,   /**< Configured, then @ref QMI8658_FAILS failed transactions in a row. */
};

/** @brief One sample of the accelerometer, as the burst gives it. */
struct ImuSample {
	uint32_t seq;      /**< +1 per sample taken; 0 for none yet. */
	uint32_t takenMs;  /**< millis() at the read. */
	uint32_t stamp;    /**< The part's 24-bit sample time stamp. */
	int16_t  tempRaw;  /**< Temperature, 256 LSB (least significant bits) a °C. */
	int16_t  acc[3];   /**< X, Y and Z of the chip, signed counts, 16,384 a g at ±2 g. */
};

/** @brief What the driver has counted since qmi8658Start(), for the tests and the bench. */
struct ImuCounts {
	uint32_t samples;  /**< Samples taken. */
	uint32_t repeats;  /**< Bursts with the time stamp of the baseline or the last sample taken. */
	uint32_t torn;     /**< Bursts dropped: the time stamp changed during the burst. */
	uint32_t resets;   /**< Soft resets written. */
	uint32_t fails;    /**< Failed transactions, all told. */
};

/**
 * @defgroup qmi8658_codec Codec
 * @brief Pure; host-tested.
 * @{
 */

/**
 * @brief A burst decoded, little-endian: the time stamp from bytes 0 to 2,
 *        the temperature from 3 and 4, X, Y and Z from 5 to 10.
 * @param b   @ref QMI8658_BURST_BYTES bytes from TIMESTAMP_LOW on.
 * @param out The sample's stamp, tempRaw and acc; the other fields are left.
 */
void qmi8658Decode(const uint8_t *b, ImuSample *out);

/**
 * @brief A 24-bit time stamp, little-endian.
 * @param b Three bytes, TIMESTAMP_LOW first.
 * @return The time stamp.
 */
uint32_t qmi8658Stamp(const uint8_t *b);

/**
 * @brief Whether an axis map, as @ref BOARD_IMU_AXES gives it, names each of
 *        X, Y and Z once, each with a sign.
 * @param longEdge  The chip axis along the board's long edge: ±1 to ±3.
 * @param shortEdge The chip axis along its short edge.
 * @param normal    The chip axis along its normal.
 * @return true for a valid map.
 */
constexpr bool qmi8658AxesValid(int longEdge, int shortEdge, int normal) {
	return longEdge != 0 && shortEdge != 0 && normal != 0 &&
	       longEdge >= -3 && longEdge <= 3 && shortEdge >= -3 && shortEdge <= 3 &&
	       normal >= -3 && normal <= 3 &&
	       (longEdge < 0 ? -longEdge : longEdge) != (shortEdge < 0 ? -shortEdge : shortEdge) &&
	       (longEdge < 0 ? -longEdge : longEdge) != (normal < 0 ? -normal : normal) &&
	       (shortEdge < 0 ? -shortEdge : shortEdge) != (normal < 0 ? -normal : normal);
}

/** @} */

/**
 * @defgroup qmi8658_core0 Core0
 * @{
 */

/**
 * @brief The angle meter opens: the next qmi8658Tick() probes the IMU.
 *        Nothing for a driver already open.
 * @param nowMs millis() now.
 */
void qmi8658Start(uint32_t nowMs);

/**
 * @brief The angle meter closes: CTRL7 0x00 when the accelerometer may be on
 *        and the bus is not stuck, one write of one byte; then nothing is
 *        sent until qmi8658Start().
 */
void qmi8658Stop();

/**
 * @brief Every UI frame, after senseTick(): the step the driver is at, or a
 *        burst and the time stamp read after it. Nothing while closed or
 *        while the bus is stuck.
 * @param nowMs millis() now.
 */
void qmi8658Tick(uint32_t nowMs);

/** @brief The state now. @return The state. */
ImuState qmi8658State();

/** @brief WHO_AM_I as last read; 0 before a read. @return The value. */
uint8_t qmi8658WhoAmI();

/** @brief REVISION_ID as last read; 0 before a read. @return The value. */
uint8_t qmi8658Revision();

/** @brief The newest sample; seq 0 for none since qmi8658Start(). @return The sample. */
const ImuSample *qmi8658Latest();

/** @brief The counts since qmi8658Start(). @return The counts. */
const ImuCounts *qmi8658Counts();

/** @} */
