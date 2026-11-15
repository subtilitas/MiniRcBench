/**
 * @file servo_job.h
 * @brief The servo tester's core1 job: PWM pulses on SIG, and on AUX for two
 *        servos, while the pin claim holds them.
 *
 * Core0 owns the screen and sends commands; core1 owns the pins
 * (core1_job.h). The two share a command block, a profile block and a status
 * block under one critical section, in the pattern of escSnapshot(). Every
 * decision core1 makes about the pulses is in servoCoreTake() and
 * servoCoreStep(), pure functions the host suite runs as they ship; the job
 * around them copies the blocks and writes the PWM levels (servo_pwm.h).
 *
 * | State | Pins | Leaves it |
 * |---|---|---|
 * | SAFE | bound to PWM at level 0: no edges, the line low | Start |
 * | RUNNING | the pulse each servo's command maps to | Safe, a heartbeat lapse, a cut of the switched supply |
 * | Tripped | SAFE, latched after a heartbeat lapse, a silent touch controller or a stuck bus while RUNNING; kept across rebinds | Start, or a new screen session (servoJobNewSession()) |
 *
 * Every pass of `loop1()`, the job takes a new profile while SAFE, takes a new
 * command, checks the heartbeat, computes each servo's target (the sweep curve
 * at millis(), or the position), slews the output towards it at SPEED, and
 * writes the compare register when the pulse changed. The slice latches it at
 * the end of the frame, so a pulse on the wire completes.
 *
 * The pins are let go only once the last pulse is over: a release, and the
 * rebind a new profile needs, first put level 0 on every pin and then wait
 * servoQuietMs() of the profile's rate, across passes, without blocking.
 * Stopping or freezing a slice mid-pulse would cut the pulse short or leave
 * the line high.
 *
 * Servo A is on SIG (index 0, mask bit 0), servo B on AUX (index 1, mask
 * bit 1).
 *
 * With `SERVO SUPPLY` at GP28 or GP29 the job switches the servo's supply
 * (servo_supply.h) as decision 20 of servo-tester.md and decision 12 of
 * current-sense.md read: off while SAFE, on at START, off at STOP, at a
 * heartbeat lapse, at a cut and at the release. servoPowerStep(), pure, makes
 * every decision about it; the job drives the enable pin from its phase.
 * With `OWN` the job drives no enable pin and servoPowerStep() changes
 * nothing.
 *
 * | Supply | Enable pin | Pulses |
 * |---|---|---|
 * | Off | low | level 0 |
 * | Starting, after START | high | level 0 until a reading at or above @ref SUPPLY_ON_MIN_MV, or @ref SUPPLY_START_MAX_MS without a sensor |
 * | On | high | the servos' pulses |
 *
 * @see docs/design/servo-tester.md
 * @see docs/design/current-sense.md, "The servo tester on the switched supply"
 */

#pragma once

#include <stdint.h>

#include "core1_job.h"
#include "sense.h"
#include "servo_model.h"
#include "servo_sweep.h"

/**
 * @defgroup servo_power_rules The switched supply's read-back
 * @brief The bus voltage at the sensor's IN-, the servo's side of the shunt
 *        (docs/design/current-sense.md, "Read-back from the bus voltage").
 * @{
 */
/** @brief On: after the switch-on, the first reading at or above this, mV. */
#define SUPPLY_ON_MIN_MV    4500u
/**
 * @brief Did not come on: no reading at or above @ref SUPPLY_ON_MIN_MV this
 *        long after the switch-on, ms; without a sensor, the pulses start
 *        this long after it. Until bench test B6 measures the start-up time.
 */
#define SUPPLY_START_MAX_MS 500u
/** @brief Off: after the switch-off, the first reading at or below this, mV. */
#define SUPPLY_OFF_MAX_MV   1000u
/** @brief Not off: a reading above this, mV, ... */
#define SUPPLY_NOT_OFF_MV   2500u
/** @brief ... taken this long or longer after the switch-off, ms. */
#define SUPPLY_NOT_OFF_MS   2000u
/** @brief ServoPower::upMs while no start-up time was measured. */
#define SUPPLY_UP_NONE      0xFFFFu
/** @} */

/** @brief Mask bit of servo A, on SIG. */
#define SERVO_MASK_A  0x01u
/** @brief Mask bit of servo B, on AUX. */
#define SERVO_MASK_B  0x02u
/** @brief Both servos. */
#define SERVO_MASK_AB (SERVO_MASK_A | SERVO_MASK_B)

/** @brief What a command asks of the job. */
enum class ServoCmdKind : uint8_t {
	Safe,       /**< STOP: level 0 from the next pass, the sweep ends. */
	Start,      /**< HOLD TO START: every bound pin to CENTRE at once. */
	Position,   /**< Drive the servos in the mask towards @c command. */
	Sweep,      /**< Run @c sweep on the servos in the mask. */
	SweepStop,  /**< End the sweep; the servos stay where the curve was. */
	Slew,       /**< SPEED changed: take @c slewPerS and nothing else. */
};

/** @brief One command from core0. */
struct ServoCmd {
	uint16_t     seq;       /**< Set by servoJobCommand(); core1 echoes it. */
	ServoCmdKind kind;      /**< What to do. */
	uint8_t      mask;      /**< @ref SERVO_MASK_A, @ref SERVO_MASK_B or both. */
	uint16_t     command;   /**< Position, 0 to @ref SERVO_SPAN. */
	uint16_t     slewPerS;  /**< Command units a second; 0 for none. */
	sweep_cfg_t  sweep;     /**< The curve, for ServoCmdKind::Sweep. */
};

/** @brief Where the switched supply is. */
enum class ServoPowerPhase : uint8_t {
	Off,       /**< The enable pin low, or no switch. */
	Starting,  /**< The enable pin high; the pulses wait for the supply. */
	On,        /**< The enable pin high and the supply up: the pulses run. */
};

/**
 * @brief Why the supply went off other than by STOP, BACK or a heartbeat
 *        lapse. Latched until START, or a new screen session.
 */
enum class ServoPowerFault : uint8_t {
	None,         /**< No fault latched. */
	NoStart,      /**< No reading at or above 4.5 V within 500 ms of the switch-on. */
	TouchSilent,  /**< The touch controller did not answer for 250 ms: STOP is out of reach. */
	BusStuck,     /**< The sensor found the shared bus stuck: STOP is out of reach. */
};

/** @brief The read-back after a switch-off, with a sensor declared. */
enum class ServoOffCheck : uint8_t {
	None,     /**< No switch-off yet, or no sensor. */
	Waiting,  /**< Switched off; no reading at or below 1.0 V yet. */
	Off,      /**< A reading at or below 1.0 V after the switch-off. */
	NotOff,   /**< A reading above 2.5 V 2000 ms or more after it: a warning, not a cut. */
};

/** @brief Why START is refused with the switch; None when it is not. */
enum class ServoStartBlock : uint8_t {
	None,         /**< START is allowed. */
	NoSensor,     /**< CURRENT SENSE is INA219 and the part is not online. */
	TouchSilent,  /**< The touch controller has not answered for 250 ms. */
	BusStuck,     /**< The shared bus is stuck. */
};

/** @brief What core1 reports. */
struct ServoStatus {
	uint16_t ackSeq;      /**< Seq of the last command taken. */
	bool     running;     /**< RUNNING: START taken; the pins carry pulses once the supply is up. */
	bool     tripped;     /**< A heartbeat lapse, a silent touch controller or a stuck bus stopped the pulses. */
	bool     sweeping;    /**< A sweep runs. */
	uint8_t  bindError;   /**< 0, or the Core1JobError of a refused rebind. */
	uint16_t outUs[2];    /**< What each pin carries, us; 0 for none. */
	uint32_t sweepMoves;  /**< Ends the running sweep has reached. */
	bool     switched;    /**< SERVO SUPPLY at GP28 or GP29 at the bind. */
	ServoPowerPhase power;       /**< The supply's phase. */
	ServoPowerFault powerFault;  /**< The latched cause of a switch-off. */
	ServoOffCheck   offCheck;    /**< The read-back after the last switch-off. */
	ServoStartBlock startBlock;  /**< Why START is refused now. */
	uint16_t upMs;        /**< The last switch-on's start-up time, ms; @ref SUPPLY_UP_NONE for none. */
};

/**
 * @brief Core1's state for the pulses. Plain data, so the host suite can
 *        drive servoCoreTake() and servoCoreStep() directly.
 */
struct ServoCore {
	ServoProfile prof;       /**< The profile the pins are bound at. */
	bool     bound[2];       /**< Pin A, pin B bound. */
	bool     running;        /**< RUNNING. */
	bool     tripped;        /**< Latched by a heartbeat lapse. */
	bool     sweeping;       /**< The sweep sets the targets. */
	uint8_t  sweepMask;      /**< The servos the sweep drives. */
	sweep_t  sweep;          /**< The curve. */
	uint32_t pos[2];         /**< Each output, in thousandths of a command unit. */
	uint16_t target[2];      /**< Where each output goes, command units. */
	uint16_t slewPerS;       /**< Command units a second; 0 for none. */
	uint32_t lastMs;         /**< millis() of the last step. */
	uint16_t ackSeq;         /**< Seq of the last command taken. */
	uint16_t outUs[2];       /**< The pulse each pin is to carry, us; 0 for none. */
};

/**
 * @brief Put @p c into SAFE after a bind: no pulses, no sweep, commands up
 *        to @p ackSeq treated as taken, and the trip latch as given.
 * @param[out] c   The state.
 * @param p        The profile the pins were bound at; may point into @p c.
 * @param boundA   Pin A bound.
 * @param boundB   Pin B bound.
 * @param ackSeq   Seq of the command block at the bind, so no command sent
 *                 before it is taken.
 * @param tripped  The trip latch to keep: a rebind does not clear it.
 */
void servoCoreReset(ServoCore *c, const ServoProfile *p, bool boundA,
                    bool boundB, uint16_t ackSeq, bool tripped);

/**
 * @brief Take a command. Pure.
 *
 * A command whose seq equals the last one taken is not new and changes
 * nothing. Start is refused while @p uiAlive is false and while pin A
 * is not bound; Sweep is refused while SAFE. The job passes @p uiAlive
 * false while servoPowerStartBlock() refuses START as well. A Position while SAFE sets a
 * target that moves nothing, and Start replaces it. A Position or Sweep taken
 * with the heartbeat lapsed is ended by servoCoreStep() in the same pass, as
 * servo_job.cpp calls the two. A refused command is still acknowledged.
 *
 * Start puts every bound output at CENTRE at once, whatever was commanded
 * before, and clears the trip. Position and Sweep end a sweep and a Sweep
 * starts a new one; with both servos in the mask and REVERSE B on, B gets the
 * mirrored command (servoMirror()).
 *
 * @param[in,out] c The state.
 * @param cmd       The command block.
 * @param nowMs     millis(), for a sweep's start.
 * @param uiAlive   core1UiAlive(), and false while the supply refuses START.
 * @return True when the command was new.
 */
bool servoCoreTake(ServoCore *c, const ServoCmd *cmd, uint32_t nowMs,
                   bool uiAlive);

/**
 * @brief One pass: the heartbeat, the sweep, the slew, the pulses. Pure.
 *
 * A lapsed heartbeat while RUNNING ends the pulses and the sweep and latches
 * the trip; the heartbeat coming back restarts nothing. A running sweep sets
 * the targets of its servos from the curve at @p nowMs; a sweep that has made
 * its movements ends with the targets at the centre. Each output moves
 * towards its target by the slew owed since the last pass, or jumps with no
 * slew. outUs is servoCmdToUs() of each bound output while RUNNING, else 0.
 *
 * @param[in,out] c The state.
 * @param nowMs     millis(). The clock may wrap.
 * @param uiAlive   core1UiAlive().
 */
void servoCoreStep(ServoCore *c, uint32_t nowMs, bool uiAlive);

/**
 * @brief Core1's state for the switched supply. Plain data, so the host
 *        suite can drive servoPowerStep() directly.
 */
struct ServoPower {
	bool            switched;  /**< SERVO SUPPLY at GP28 or GP29 at the bind. */
	ServoPowerPhase phase;     /**< Off, Starting, On. */
	ServoPowerFault fault;     /**< Latched until START or a new screen session. */
	ServoOffCheck   offCheck;  /**< The read-back after the last switch-off. */
	uint32_t        onMs;      /**< millis() of the last switch-on. */
	uint32_t        offMs;     /**< millis() of the last switch-off. */
	uint16_t        upMs;      /**< The last start-up time, ms; @ref SUPPLY_UP_NONE for none. */
};

/** @brief What servoPowerStep() reads on one pass. */
struct ServoPowerIn {
	SenseSample sample;   /**< senseSample() on this pass; its state None with CURRENT SENSE at NONE. */
	bool        touchOk;  /**< The touch controller answered within 250 ms (touchAnsweredMs()). */
};

/**
 * @brief Put @p p into Off at a bind: the supply off, no read-back. The
 *        fault is kept unless @p fresh, as the trip latch is.
 * @param[out] p    The state.
 * @param switched  SERVO SUPPLY at GP28 or GP29.
 * @param fresh     A new screen session: the fault and the start-up time go.
 */
void servoPowerReset(ServoPower *p, bool switched, bool fresh);

/**
 * @brief Why START is refused now. Pure.
 *
 * With no switch, never. With one: a touch controller silent for 250 ms or
 * a stuck bus, since STOP would be out of reach; and CURRENT SENSE at INA219
 * with the part not online (absent, another part, offline), since the
 * read-back and the cuts of a later slice depend on it.
 *
 * @param p  The state.
 * @param in This pass's inputs.
 * @return The reason, or ServoStartBlock::None.
 */
ServoStartBlock servoPowerStartBlock(const ServoPower *p, const ServoPowerIn *in);

/**
 * @brief The switch-off, from any path: Off, the time, and the read-back
 *        armed with a sensor declared. Nothing while already Off. Pure.
 * @param[in,out] p The state.
 * @param sensed    CURRENT SENSE declares a sensor.
 * @param nowMs     millis().
 */
void servoPowerOff(ServoPower *p, bool sensed, uint32_t nowMs);

/**
 * @brief One pass of the switched supply, after servoCoreStep(). Pure.
 *
 * With no switch it changes nothing. With one:
 *
 * 1. While RUNNING, a silent touch controller or a stuck bus ends the pulses
 *    and the sweep, latches the trip and records the fault.
 * 2. Not RUNNING: the supply off (servoPowerOff()). RUNNING with the supply
 *    off: the switch-on, which clears the fault and the start-up time.
 * 3. Starting: with a sensor, a fresh reading taken after the switch-on at
 *    or above @ref SUPPLY_ON_MIN_MV makes it On and its age at the
 *    switch-on the start-up time; without one, On at
 *    @ref SUPPLY_START_MAX_MS. With a sensor and no such reading by then,
 *    the pulses end, the supply goes off and the fault is NoStart.
 * 4. While Starting both outputs are held at CENTRE and the sweep ends;
 *    while not On, outUs is 0 on both pins.
 * 5. After a switch-off, with a sensor: a fresh reading taken after it at
 *    or below @ref SUPPLY_OFF_MAX_MV is Off; one above
 *    @ref SUPPLY_NOT_OFF_MV taken @ref SUPPLY_NOT_OFF_MS or more after it is
 *    NotOff, until a reading at or below @ref SUPPLY_OFF_MAX_MV.
 *
 * @param[in,out] p The supply's state.
 * @param[in,out] c The pulses' state.
 * @param in        This pass's inputs.
 * @param nowMs     millis(). The clock may wrap.
 */
void servoPowerStep(ServoPower *p, ServoCore *c, const ServoPowerIn *in, uint32_t nowMs);

/**
 * @brief Hand the job a profile. Core0.
 *
 * Refused while RUNNING, while a command is not yet taken, and for a profile
 * servoProfileValid() refuses. Taken on core1's next pass: with the job
 * bound, it releases both pins and binds them again at the new rate; a
 * refused bind leaves both unbound and reports ServoStatus::bindError. With
 * no job running, the next bind uses it.
 *
 * @param p The profile.
 * @return True when the profile was accepted.
 */
bool servoJobProfile(const ServoProfile *p);

/**
 * @brief Start a new screen session. Core0, when the screen opens.
 *
 * The published status forgets a trip and a supply fault latched in an
 * earlier session at once, and the next bind forgets them in core1's state. A bind after BACK TO
 * SERVO on the leave page, and a profile rebind, keep it until START.
 */
void servoJobNewSession();

/**
 * @brief Send a command. Core0.
 *
 * One command is held at a time, and a new one replaces a command core1 has
 * not taken, with one exception: a Safe not yet taken stays, and a command
 * sent after it is dropped. A STOP is never lost to a later command.
 *
 * @param c The command; its seq is ignored.
 * @return The seq of the command held, which ServoStatus::ackSeq echoes once
 *         core1 has taken it: the new command's, or the kept Safe's.
 */
uint16_t servoJobCommand(const ServoCmd *c);

/**
 * @brief Read the job's status. Core0, under the job's critical section.
 * @param[out] out The status.
 */
void servoJobStatus(ServoStatus *out);

/** @brief The servo job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps SERVO_JOB_OPS;
