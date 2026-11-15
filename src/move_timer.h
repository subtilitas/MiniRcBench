/**
 * @file move_timer.h
 * @brief The servo tester's move timing: the time a servo takes to reach a
 *        commanded position, judged from the current sensor's readings by
 *        rcbench's servo_move.c (docs/design/current-sense.md, "Move timing
 *        in the servo tester").
 *
 * Pure: the servo job (servo_job.h) hands the timer each pass's sample
 * (senseSample()), the pulses it puts on SIG and AUX, and whether the
 * command it took was a step, MIN, CENTRE or MAX, and at SPEED 100 %. The
 * timer reads; it changes no pulse and no cut.
 *
 * | Rule | Value |
 * |---|---|
 * | Timed | a step at SPEED 100 % that changes the pulses, @ref MOVE_REST_MS or more after the servo came to rest, no move under way |
 * | At rest | from the reading that showed the last timed move's arrival, or the end of its window when it went late or unseen; after any other change of the pulses (START, a drag, a sweep, SPEED below 100 %, a step not timed or voided) 3000 ms plus @ref MOVE_LAG_MS later, the longest move servo_move.c times |
 * | Level before | the mean of the readings taken in the @ref MOVE_REST_MS before the command, at least @ref MOVE_MIN_READINGS of them, at or above -@ref MOVE_REVERSED_MA |
 * | Threshold | servo_move_threshold_a() of those readings' noise: the larger of 20 mA and 3 times their standard deviation |
 * | Holding level | the level before the last timed step that left the destination's pulses this run; while none is known, the idle level: the level before the last timed step that left the pulses the run started with, CENTRE; while neither is known, the level before |
 * | Movement, arrival, late, unseen | servo_move.c's rules, one reading at a time (filter_n 1), settle_n 2, a window of 3000 ms plus @ref MOVE_LAG_MS |
 * | Hold | the mean of the readings taken in the @ref MOVE_HOLD_MS after the reading that ended the move, at least @ref MOVE_MIN_READINGS of them |
 * | Void | a step not timed, or a move ended by STOP, a cut, a new pulse or a lost sensor; a hold so ended, or with a clipped reading, is not shown |
 *
 * A reading counts once: a new seq, fresh by senseFresh(), with the sensor
 * Online. SenseSample::takenMs is its time. The level before takes the
 * readings by their age alone: the servo was at rest for @ref MOVE_REST_MS
 * and the pulses unchanged, so none of them was taken before the pulses'
 * last change. A move ends only at a reading: one at or past the window's
 * end ends it late or unseen. With no reading for @ref SENSE_STALE_MS, counted
 * from the command or the arrival at the earliest, or with the sensor not
 * Online, a move under way is void and a hold is not shown.
 *
 * A position this run has not left by a timed step is judged against the
 * idle level. A servo that holds there harder than at CENTRE, against a
 * stop, does not come back to that level: the move reads late, until the
 * servo has left the position once and its level is known (rcbench's
 * automatic test places both ends first for this reason).
 *
 * The time is an upper bound on when the current shows the arrival: a
 * shunt conversion that straddles the arrival averages moving and holding
 * current, the first one wholly after it ends up to one cycle and one
 * conversion, 17.55 ms and 17.02 ms, later, and core0 reads it up to 25 ms
 * after that; the pulse leaves at the next PWM frame after the command. The
 * current can show an arrival before the horn stops: a servo that eases its
 * drive near the target can fall to its holding current while the horn
 * still creeps. Not measured on hardware (bench test B10).
 *
 * With CURRENT SENSE at NONE nothing is timed: a step leaves the result as
 * it is, a move under way is void as Lost, and a hold under way is not
 * shown.
 *
 * @see docs/design/current-sense.md, "Slice 5 as built"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sense.h"
#include "servo_move.h"

/**
 * @defgroup move_timer_rules The move timing's constants
 * @{
 */
/** @brief The level before: the readings of this long before the command, ms; the pulses unchanged for as long. */
#define MOVE_REST_MS       500u
/** @brief Readings the level before and the hold each need: half the 20 the sensor takes in 500 ms. */
#define MOVE_MIN_READINGS  10u
/**
 * @brief The meter's lag, ms: one read, 25 ms, and two conversion cycles,
 *        2 × 17.55 ms, rounded up: the cycle a conversion straddling the
 *        arrival ends in, then the first conversion wholly after it.
 *        servo_move_window_ms() adds it to the window.
 */
#define MOVE_LAG_MS        61u
/** @brief The hold: the readings of this long after the move ended, ms. */
#define MOVE_HOLD_MS       500u
/** @brief A level before below minus this, mA, is a reversed sensor: not timed (rcbench's `SERVO_TEST_NEG_IDLE_A`). */
#define MOVE_REVERSED_MA   20
/** @brief Readings the ring keeps: 500 ms at 40 a second, and room. */
#define MOVE_RING_N        32u
/** @brief Holding levels kept per run, by the pair of pulses. */
#define MOVE_HOLDS         9u
/** @} */

/** @brief Where the last step stands. */
enum class MoveState : uint8_t {
	None,       /**< No step since the pulses started. */
	Measuring,  /**< Timed, not over. */
	Arrived,    /**< Arrived, or settled at an end held harder than it moves. */
	Late,       /**< Movement, no arrival in the window. */
	Unseen,     /**< No movement in the window. */
	Void,       /**< Not timed, or ended before it was over. */
};

/** @brief Why a step is Void. */
enum class MoveVoid : uint8_t {
	None,      /**< Not void. */
	NotAtRest, /**< Within @ref MOVE_REST_MS of the servo coming to rest, or a move was under way. */
	Slewed,    /**< At SPEED below 100 %: the pulse slews, no step. */
	Few,       /**< Fewer than @ref MOVE_MIN_READINGS readings in the @ref MOVE_REST_MS before it. */
	Reversed,  /**< The level before was below -@ref MOVE_REVERSED_MA mA. */
	Ended,     /**< The pulses ended: STOP, a trip or a cut. */
	Changed,   /**< Another command changed the pulses. */
	Lost,      /**< The sensor left Online or CURRENT SENSE went to NONE, or no reading for @ref SENSE_STALE_MS. */
};

/** @brief The last step's result, as the servo job publishes it. */
struct MoveResult {
	MoveState state;    /**< Where it stands. */
	MoveVoid  why;      /**< Why it is Void. */
	bool      settled;  /**< Arrived by the settled rule, at an end held harder than it moves. */
	bool      peakOk;   /**< peakMa is shown: a move over with readings, none clipped, the peak at or above 0. */
	bool      holdOk;   /**< holdMa is shown: the hold's readings are in. */
	uint16_t  ms;       /**< Arrived: the command to the arrival, ms. */
	int32_t   peakMa;   /**< The highest reading from the command to the arrival or the window's end, mA. */
	int32_t   holdMa;   /**< The hold, mA. */
};

/** @brief One reading in the ring. */
struct MoveReading {
	uint32_t t;  /**< Its takenMs. */
	float    a;  /**< Its current, A. */
};

/** @brief A holding level, by the pulses on SIG and AUX. */
struct MoveHold {
	uint16_t out[2];  /**< The pulses, us. */
	float    a;       /**< The level, A. */
	uint32_t stamp;   /**< When it was stored, in stores; 0 for an empty slot. */
};

/** @brief The timer's state. Plain data. */
struct MoveTimer {
	MoveResult  res;               /**< The last step's result. */
	bool        on;                /**< Pulses on the last pass. */
	uint16_t    out[2];            /**< The pulses of the last pass, us. */
	uint32_t    restMs;            /**< When the servo is taken to be at rest. */
	uint32_t    seq;               /**< Seq of the last reading seen. */
	MoveReading ring[MOVE_RING_N]; /**< The newest readings, newest last. */
	uint8_t     ringN;             /**< How many. */
	bool        timing;            /**< A move is under way in mv. */
	servo_move_t mv;               /**< The move. */
	bool        holding;           /**< The hold's readings are coming in. */
	uint32_t    holdFromMs;        /**< The reading that ended the move. */
	float       holdSum;           /**< The hold's readings, A. */
	uint32_t    holdN;             /**< How many. */
	uint32_t    refMs;             /**< The command: the stale rule counts from it, or from the sample's newest reading. */
	MoveHold    holds[MOVE_HOLDS]; /**< Holding levels of this run. */
	uint16_t    startOut[2];       /**< The pulses the run started with, CENTRE, us. */
	float       idleA;             /**< The idle level: the holding level at startOut, A. */
	bool        idleOk;            /**< idleA is known. */
	uint32_t    stores;            /**< Holding levels stored this run. */
};

/** @brief What moveTimerStep() reads on one pass. */
struct MoveIn {
	SenseSample sample;  /**< senseSample() on this pass. */
	uint16_t    out[2];  /**< The pulses on SIG and AUX after this pass, us; 0 for none. */
	bool        step;    /**< This pass took a step command: MIN, CENTRE or MAX (servoStepCmd()). */
	bool        slewed;  /**< ...at SPEED below 100 %: a slew, not timed. */
};

/**
 * @brief Forget everything at a bind; the result too when @p fresh, a new
 *        screen session.
 * @param[out] t The timer.
 * @param fresh  Drop the result.
 */
void moveTimerReset(MoveTimer *t, bool fresh);

/**
 * @brief One pass, after the pulses are decided. Pure.
 *
 * Pulses starting are a new run: the result and the holding levels go. A
 * change of the pulses with @p in's step begins a step; without it a move
 * or a hold under way is void. Then the stale rule, then this pass's
 * reading. Pulses ending void a move under way.
 *
 * @param[in,out] t The timer.
 * @param in        This pass's inputs.
 * @param nowMs     millis(). The clock may wrap.
 */
void moveTimerStep(MoveTimer *t, const MoveIn *in, uint32_t nowMs);

/**
 * @brief The SET line: `MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`; `LATE` or
 *        `UNSEEN` in the time's place; `--` for a figure not shown.
 * @param r   The result.
 * @param buf At least @ref MOVE_TEXT_MAX + 1 bytes.
 * @param n   Room in @p buf.
 * @return false, with an empty @p buf, for MoveState::None.
 */
bool moveTimerText(const MoveResult *r, char *buf, size_t n);

/** @brief The longest SET line, characters: `MOVE 3.04 S  PEAK 3.20 A  HOLD 3.20 A`. */
#define MOVE_TEXT_MAX 37
