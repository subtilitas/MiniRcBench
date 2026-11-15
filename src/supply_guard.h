/**
 * @file supply_guard.h
 * @brief The servo supply's protection: the cuts on the current sensor's
 *        readings (docs/design/current-sense.md, "Protection").
 *
 * Pure: a job on core1 hands the guard each pass's sample (senseSample())
 * and the time, and acts on the cut it returns. The guard reads only new
 * readings: a sample whose seq it has seen, or one @ref SENSE_STALE_MS old
 * or older (senseFresh()), is never taken as a reading.
 *
 * | Cut | Condition | Rule |
 * |---|---|---|
 * | Hard | one reading at or above @ref GUARD_HARD_MA, either sign, or clipped | always |
 * | Stall | readings above @ref GUARD_STALL_MA, either sign, for @ref GUARD_STALL_MS, none at or below it between | GuardRules::stall |
 * | Collapse | after the start-up reading, readings under @ref GUARD_LOW_MV for @ref GUARD_LOW_MS, none at or above it between | GuardRules::collapse |
 * | Lost | the sensor not online, or no reading for @ref SENSE_STALE_MS since the arm | GuardRules::lost |
 *
 * A run of the stall or the collapse rule also ends at a reading taken
 * @ref SENSE_STALE_MS or more after the one before it: the reading before
 * the gap had gone stale, so the run starts again at the new one. The
 * start-up reading is the first fresh reading taken more than
 * @ref SUPPLY_READ_LAG_MS after the arm at or above @ref SUPPLY_ON_MIN_MV,
 * the servo tester's rule for a supply that has come up
 * (supplyReadingAfter()).
 *
 * @see docs/design/current-sense.md, "Slice 4 as built"
 */

#pragma once

#include <stdint.h>

#include "sense.h"

/**
 * @defgroup supply_guard_rules The cuts' thresholds
 * @brief Decision 13 of docs/design/current-sense.md: rcbench's limit run,
 *        `SERVO_LIMRUN_HARD_MA`, `SERVO_LIMRUN_STALL_MA` and
 *        `SERVO_LIMRUN_STALL_MS` in `servo_limrun.h`; the collapse rule is
 *        the note's.
 * @{
 */
/** @brief Hard: one reading at or above this, either sign, mA. */
#define GUARD_HARD_MA   3000u
/** @brief Stall: readings above this, either sign, mA, ... */
#define GUARD_STALL_MA  1000u
/** @brief ... for this long, ms. */
#define GUARD_STALL_MS  400u
/** @brief Collapse: readings under this, mV, after the start-up reading, ... */
#define GUARD_LOW_MV    4000u
/** @brief ... for this long, ms. */
#define GUARD_LOW_MS    1000u
/** @} */

/**
 * @defgroup supply_readback The switched supply's start-up reading
 * @brief Shared by the servo tester's read-back (servo_job.h) and the
 *        collapse rule.
 * @{
 */
/** @brief On: after the switch-on, the first reading at or above this, mV. */
#define SUPPLY_ON_MIN_MV    4500u
/**
 * @brief A reading counts after a switch command only when core0 took it
 *        more than this long after the command, ms.
 *
 * SenseSample::takenMs is when core0 read the result, not when the INA219
 * measured it: the bus sample of the bench's configuration is up to one
 * conversion cycle, 17.55 ms, older than the read. A reading taken this
 * long or less after the command can still carry the output's voltage from
 * before it. 18 is 17.55 ms rounded up; "more than" covers the part of a
 * millisecond that millis() hides.
 */
#define SUPPLY_READ_LAG_MS  18u
/** @} */

/** @brief What the guard cuts on. */
enum class GuardCut : uint8_t {
	None,      /**< No cut. */
	Hard,      /**< A reading at or above 3.0 A, or clipped. */
	Stall,     /**< Readings above 1.0 A for 400 ms. */
	Collapse,  /**< Readings under 4.0 V for 1000 ms after start-up. */
	Lost,      /**< The sensor offline, the bus stuck, or no reading for 250 ms. */
};

/** @brief Which rules besides the hard cut apply. */
struct GuardRules {
	bool stall;     /**< The stall rule: the servo tester. */
	bool collapse;  /**< The collapse rule: a supply the bench switches. */
	bool lost;      /**< The sensor-lost rule: the servo tester on the switched supply. */
};

/** @brief The guard's state. Plain data; supplyGuardArm() sets every field. */
struct SupplyGuard {
	bool     armed;   /**< Readings are judged. */
	uint32_t armMs;   /**< millis() of the arm: the switch-on, or START with OWN. */
	uint32_t seq;     /**< Seq of the last reading taken, or of the sample at the arm. */
	bool     taken;   /**< A reading was taken since the arm. */
	uint32_t lastMs;  /**< Its takenMs. */
	bool     up;      /**< The start-up reading came: the collapse rule counts. */
	bool     over;    /**< The stall rule's run is under way ... */
	uint32_t overMs;  /**< ... since this reading's takenMs. */
	bool     low;     /**< The collapse rule's run is under way ... */
	uint32_t lowMs;   /**< ... since this reading's takenMs. */
};

/**
 * @brief Whether @p s is a fresh reading taken more than
 *        @ref SUPPLY_READ_LAG_MS after @p sinceMs, across a wrap: one that
 *        cannot carry the voltage from before a switch command there.
 * @param s       The sample.
 * @param sinceMs millis() of the command.
 * @param nowMs   millis() now.
 * @return true for such a reading.
 */
bool supplyReadingAfter(const SenseSample *s, uint32_t sinceMs, uint32_t nowMs);

/**
 * @brief Arm the guard at @p nowMs: no run, no start-up reading, and the
 *        reading in @p s already seen.
 * @param[out] g The guard.
 * @param s      The sample at the arm.
 * @param nowMs  millis(): the switch-on, or START.
 */
void supplyGuardArm(SupplyGuard *g, const SenseSample *s, uint32_t nowMs);

/**
 * @brief Disarm the guard: supplyGuardStep() cuts nothing until the next arm.
 * @param[out] g The guard.
 */
void supplyGuardDisarm(SupplyGuard *g);

/**
 * @brief One pass: the cut @p s calls for under @p r, or GuardCut::None.
 *
 * Nothing while disarmed. With @p r's lost rule, a sensor not online, or
 * no reading taken within @ref SENSE_STALE_MS of @p nowMs, counted from the
 * arm at the earliest, is Lost. Then a new fresh reading, in this order:
 * Hard, Stall, Collapse. The guard stays armed after a cut; the caller
 * disarms it with the switch-off.
 *
 * @param[in,out] g The guard.
 * @param s         senseSample() on this pass.
 * @param r         The rules that apply.
 * @param nowMs     millis(). The clock may wrap.
 * @return The cut.
 */
GuardCut supplyGuardStep(SupplyGuard *g, const SenseSample *s, const GuardRules *r,
                         uint32_t nowMs);
