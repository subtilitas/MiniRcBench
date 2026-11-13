/**
 * @file sense.h
 * @brief The servo supply's current sensor: an INA219 on J3, the board's
 *        shared I2C (Inter-Integrated Circuit) bus I2C1, read on core0 once
 *        per UI (user interface) frame after the touch read.
 *
 * Optional. `CURRENT SENSE` in SETTINGS (@ref Settings::currentSense) says
 * whether a sensor is declared: @ref SenseMode::None, the default, sends
 * nothing to @ref INA219_ADDR; @ref SenseMode::Ina219 probes the part at boot
 * and again every 1000 ms while it is not online.
 *
 * The INA219 measures the voltage across its shunt and the voltage at IN-,
 * the servo's side of the shunt, against its GND. The bench writes one
 * configuration, @ref INA219_CONFIG_BENCH, leaves the calibration register at
 * its power-on value and computes the current from the shunt code on core0:
 * 10 µV a code over @ref INA219_SHUNT_MOHM.
 *
 * rcbench's sense_bus.h (lib/rcbench/shared/sense/) makes the bus and part
 * decisions: three failed transactions take an online part offline, two
 * timeouts in a row or a line held low mark the bus stuck, a recovery runs at
 * once and every 100 ms while it is stuck. This file supplies its wire over
 * the SDK (software development kit), runs the recovery, and reports the touch
 * driver's transfers to the same record (senseNoteTouch()).
 *
 * Each reading is published as a @ref SenseSample under a critical section
 * of this module's own; senseSample() copies it on either core. A reading
 * @ref SENSE_STALE_MS old or older counts as none (senseFresh()).
 *
 * @see docs/design/current-sense.md
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sense_bus.h"

/**
 * @defgroup sense_ina219 INA219 constants
 * @brief Registers, configuration and scale of Texas Instruments' INA219,
 *        SBOS448G.
 * @{
 */

/** @brief The sensor's 7-bit address: A0 and A1 at GND, the common module's default. */
#define INA219_ADDR          0x40u

/**
 * @brief The shunt, in milliohms: the common module's R100.
 *
 * Open question Q2 of docs/design/current-sense.md: a 0.05 Ω or 0.02 Ω
 * shunt soldered on the module changes this value and nothing else.
 */
#define INA219_SHUNT_MOHM    100u

#define INA219_REG_CONFIG    0x00u  /**< Configuration. */
#define INA219_REG_SHUNT     0x01u  /**< Shunt voltage, signed, 10 µV a code. */
#define INA219_REG_BUS       0x02u  /**< Bus voltage, bits 15 to 3, 4 mV a code. */

#define INA219_CONFIG_RST    0x8000u  /**< RST: the part to its power-on values. */

/**
 * @brief The configuration after a power-on reset: /8, 32 V, 12 bit,
 *        continuous. The INA219 has no identity register, so the probe takes
 *        this value, read after RST, as its identity.
 */
#define INA219_CONFIG_POR    0x399Fu

/**
 * @brief Fields of the configuration register, packed.
 * @param brng BRNG, bit 13: 0 for 16 V, 1 for 32 V.
 * @param pg   PG, bits 12 to 11: 0 to 3 for /1 to /8.
 * @param badc BADC, bits 10 to 7: the bus ADC (analogue-to-digital converter) setting.
 * @param sadc SADC, bits 6 to 3: the shunt ADC setting.
 * @param mode MODE, bits 2 to 0.
 * @return The register value, RST clear.
 */
constexpr uint16_t ina219Config(unsigned brng, unsigned pg, unsigned badc, unsigned sadc,
                                unsigned mode) {
	return (uint16_t)(((brng & 1u) << 13) | ((pg & 3u) << 11) | ((badc & 15u) << 7) |
	                  ((sadc & 15u) << 3) | (mode & 7u));
}

/**
 * @brief The bench's configuration, written at every probe: 16 V, /8
 *        (±320 mV), bus 12 bit (532 µs), shunt 32 samples (17.02 ms), shunt
 *        and bus continuous. 19EFh.
 *
 * Decision 6 of docs/design/current-sense.md, which the owner has not made:
 * one configuration for every use.
 */
#define INA219_CONFIG_BENCH  ina219Config(0u, 3u, 0x3u, 0xDu, 7u)

static_assert(INA219_CONFIG_BENCH == 0x19EFu, "the bench configuration is not 19EFh");

/** @brief Shunt voltage per code, µV, at every PG setting. */
#define INA219_SHUNT_UV      10u
/** @brief Bus voltage per code, mV. */
#define INA219_BUS_MV        4u

/**
 * @brief A shunt code at or beyond this, either sign, is clipped: /8's end,
 *        320 mV. What the part reports past it is not known, so such a code
 *        is never a value.
 */
#define INA219_SHUNT_END     32000

/** @} */

/**
 * @defgroup sense_timing Bus and reading rules
 * @{
 */

/** @brief Each sensor transfer's timeout, µs: eight times its 400 kHz length. */
#define SENSE_TIMEOUT_US     1000u

/** @brief A reading this old or older counts as none, ms: the heartbeat's own timeout. */
#define SENSE_STALE_MS       250u

/** @brief The configuration is read back this often, ms. */
#define SENSE_READBACK_MS    1000u

/** @brief Read-backs in a row of another value than the bench's: the part reset. */
#define SENSE_READBACK_MISSES 2u

/** @brief Clocks on SCL in a recovery. */
#define SENSE_RECOVER_CLOCKS 9u

/** @brief Half a recovery clock, µs: about 100 kHz. */
#define SENSE_RECOVER_HALF_US 5u

/** @} */

/** @brief What SETTINGS says is fitted: the value of @ref Settings::currentSense. */
enum class SenseMode : uint8_t {
	None   = 0,  /**< No sensor: nothing is sent to its address. */
	Ina219 = 1,  /**< An INA219 at @ref INA219_ADDR. */
};

/** @brief The highest @ref SenseMode value. */
#define SENSE_MODE_MAX 1u

/** @brief The sensor as a reading's consumer sees it. */
enum class SenseState : uint8_t {
	None,     /**< CURRENT SENSE is NONE. */
	Absent,   /**< Declared, not found: no answer, another part, or not probed yet. */
	Online,   /**< Answering on the bench's configuration. */
	Offline,  /**< Was online; failed transactions or a reset took it off. */
	Stuck,    /**< The bus is stuck: nothing is sent until a recovery frees it. */
};

/** @brief In @ref SenseSample::flags, the shunt code was at its end; the current is the end. */
#define SENSE_FLAG_CLIPPED 0x01u
/** @brief In @ref SenseSample::flags, the first reading after the part was found reset. */
#define SENSE_FLAG_RESET   0x02u

/** @brief One reading, as core0 publishes it. */
struct SenseSample {
	uint32_t seq;        /**< +1 per reading; 0 for none yet. */
	uint32_t takenMs;    /**< millis() at the read, core0. */
	int32_t  currentUa;  /**< Shunt code × 10 µV over the shunt, signed, µA. */
	uint16_t busMv;      /**< Bus code × 4 mV, at IN-. */
	uint8_t  flags;      /**< @ref SENSE_FLAG_CLIPPED, @ref SENSE_FLAG_RESET. */
	uint8_t  state;      /**< A @ref SenseState, now, not at the read. */
};

/**
 * @defgroup sense_codec Codec
 * @brief Pure; host-tested.
 * @{
 */

/**
 * @brief The current a shunt register value stands for.
 * @param raw       The shunt voltage register, as read.
 * @param shuntMohm The shunt, mΩ, not 0.
 * @return µA, truncated towards zero; SENSE_CLIP_HIGH or SENSE_CLIP_LOW and
 *         a value of 0 at or beyond ±@ref INA219_SHUNT_END.
 */
sense_value_t ina219CurrentUa(uint16_t raw, uint32_t shuntMohm);

/**
 * @brief The bus voltage a bus register value stands for.
 * @param raw The bus voltage register, as read; bits 2 to 0 are flags.
 * @return mV.
 */
uint16_t ina219BusMv(uint16_t raw);

/**
 * @brief What a transfer of the SDK came to, for sense_bus.h.
 * @param result The SDK's return: the byte count or a PICO_ERROR_ code.
 * @param want   The byte count asked.
 * @return SENSE_OK, SENSE_TIMEOUT for PICO_ERROR_TIMEOUT, SENSE_NACK otherwise.
 */
sense_err_t senseErrOf(int result, size_t want);

/**
 * @brief Whether a reading counts: one taken, and less than
 *        @ref SENSE_STALE_MS ago, across a millis() wrap.
 * @param s     The sample.
 * @param nowMs millis() now.
 * @return true for a fresh reading.
 */
bool senseFresh(const SenseSample *s, uint32_t nowMs);

/**
 * @brief The readout of a current: `0.42 A`, `-0.05 A`, `>3.2 A` clipped
 *        high, `<-3.2 A` clipped low, `-- A` for no fresh reading.
 * @param s     The sample.
 * @param nowMs millis() now.
 * @param buf   At least @ref SENSE_TEXT_MAX + 1 bytes.
 * @param n     Room in @p buf.
 * @return true when the reading is clipped, which reads in amber.
 */
bool senseCurrentText(const SenseSample *s, uint32_t nowMs, char *buf, size_t n);

/**
 * @brief The readout of the bus voltage: `5.02 V`, or `-- V` for no fresh
 *        reading.
 * @param s     The sample.
 * @param nowMs millis() now.
 * @param buf   At least @ref SENSE_TEXT_MAX + 1 bytes.
 * @param n     Room in @p buf.
 */
void senseVoltText(const SenseSample *s, uint32_t nowMs, char *buf, size_t n);

/**
 * @brief The widest readout text, characters: `-3.20 A`, `<-3.2 A` and
 *        `32.76 V` on 0.1 Ω, `<-16.0 A` on 0.02 Ω.
 */
#define SENSE_TEXT_MAX 8

/** @brief The current at the shunt code's end, µA: 3,200,000 on 0.1 Ω. @return µA. */
int32_t senseEndUa();

/** @} */

/**
 * @defgroup sense_core0 Core0
 * @{
 */

/**
 * @brief In `setup()` after touchInit(), which has set the bus up: the
 *        mode from the settings, and with @ref SenseMode::Ina219 the probe.
 */
void senseInit();

/**
 * @brief From SETTINGS: the mode of @p mode. A new mode with
 *        @ref SenseMode::Ina219 is probed on the next senseTick().
 * @param mode The value of @ref Settings::currentSense; another value reads
 *             as None.
 */
void senseConfigure(uint8_t mode);

/**
 * @brief Every UI frame, directly after touchPoll(): a recovery when one is
 *        due, a probe when one is due, else a read-back of the configuration
 *        once a second and a reading of the shunt and bus registers.
 *        Nothing with @ref SenseMode::None.
 * @param nowMs millis() now.
 */
void senseTick(uint32_t nowMs);

/**
 * @brief The touch driver's transfer on the shared bus, for the stuck
 *        decision: a timeout counts, a NACK (not acknowledged) shows the bus
 *        working.
 * @param result The SDK's return.
 * @param want   The byte count asked.
 */
void senseNoteTouch(int result, size_t want);

/** @brief Whether CURRENT SENSE declares a sensor. @return Mode not None. */
bool senseDeclared();

/**
 * @brief The CURRENT SENSE row's caption: `FOUND`, `NOT FOUND` or
 *        `OTHER PART`; null with @ref SenseMode::None.
 * @return The text, or null.
 */
const char *senseCaption();

/**
 * @brief Append ` 0.08 A` to a strip that shows the supply's state, with a
 *        sensor declared. Nothing with @ref SenseMode::None.
 * @param buf A terminated string.
 * @param n   Room in @p buf.
 */
void senseAppendCurrent(char *buf, size_t n);

/** @brief The bus and fault record, for the host tests. @return The record. */
const sense_bus_t *senseBus();

/** @brief The part record, for the host tests. @return The record. */
const sense_part_t *sensePart();

/** @} */

/**
 * @brief Either core: copy the newest reading and the state, under the lock.
 * @param[out] out The sample.
 */
void senseSample(SenseSample *out);
