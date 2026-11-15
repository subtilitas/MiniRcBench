/**
 * @file servo_job.cpp
 * @brief The servo tester's core1 job. @see servo_job.h
 */

#include "servo_job.h"
#include "esc_task.h"
#include "servo_pwm.h"
#include "servo_supply.h"
#include "touch.h"

#include "plat.h"
#include <pico/critical_section.h>
#include <string.h>

// ---------------------------------------------------------------------------
// the pure core
// ---------------------------------------------------------------------------

/** @brief Index of servo A, on SIG, and of servo B, on AUX. */
enum : uint8_t { SERVO_A = 0, SERVO_B = 1 };

void servoCoreReset(ServoCore *c, const ServoProfile *p, bool boundA,
                    bool boundB, uint16_t ackSeq, bool tripped) {
	// Copied first: @p p may point into @p c.
	ServoProfile prof = *p;
	memset(c, 0, sizeof(*c));
	c->prof = prof;
	c->bound[SERVO_A] = boundA;
	c->bound[SERVO_B] = boundB;
	c->ackSeq = ackSeq;
	c->tripped = tripped;
	for (int i = 0; i < 2; i++) {
		c->target[i] = SERVO_CMD_CENTRE;
		c->pos[i] = SERVO_CMD_CENTRE * 1000u;
	}
}

/**
 * @brief Point the servos in @p mask at @p cmd. With both in the mask and
 *        REVERSE B on, B gets the mirror.
 */
static void aim(ServoCore *c, uint8_t mask, uint16_t cmd) {
	if (mask & SERVO_MASK_A) c->target[SERVO_A] = cmd;
	if (mask & SERVO_MASK_B)
		c->target[SERVO_B] = (mask == SERVO_MASK_AB && c->prof.reverseB)
		                   ? servoMirror(cmd) : cmd;
}

bool servoCoreTake(ServoCore *c, const ServoCmd *cmd, uint32_t nowMs,
                   bool uiAlive) {
	if (cmd->seq == c->ackSeq) return false;
	c->ackSeq = cmd->seq;
	switch (cmd->kind) {
		case ServoCmdKind::Safe:
			c->running = false;
			c->sweeping = false;
			break;
		case ServoCmdKind::Start:
			if (!uiAlive || !c->bound[SERVO_A]) break;
			c->running = true;
			c->tripped = false;
			c->sweeping = false;
			c->slewPerS = cmd->slewPerS;
			c->lastMs = nowMs;
			// CENTRE on every pin at once, whatever was commanded before:
			// no slew from a position nobody chose this session.
			for (int i = 0; i < 2; i++) {
				c->target[i] = SERVO_CMD_CENTRE;
				c->pos[i] = SERVO_CMD_CENTRE * 1000u;
			}
			break;
		case ServoCmdKind::Position:
			// While SAFE the target set here moves nothing, and Start replaces
			// it. With the heartbeat lapsed, servoCoreStep() ends the pulses
			// in the same pass.
			c->sweeping = false;
			c->slewPerS = cmd->slewPerS;
			aim(c, cmd->mask, cmd->command);
			break;
		case ServoCmdKind::Sweep:
			if (!c->running) break;
			c->sweeping = sweep_start(&c->sweep, &cmd->sweep, nowMs);
			c->sweepMask = cmd->mask;
			c->slewPerS = cmd->slewPerS;
			break;
		case ServoCmdKind::SweepStop:
			// The targets are where the curve was on the last pass.
			c->sweeping = false;
			break;
		case ServoCmdKind::Slew:
			c->slewPerS = cmd->slewPerS;
			break;
	}
	return true;
}

/** @brief Move @p pos towards @p target by at most @p step. */
static uint32_t approach(uint32_t pos, uint32_t target, uint64_t step) {
	if (pos < target) return (uint64_t)(target - pos) <= step ? target : pos + (uint32_t)step;
	return (uint64_t)(pos - target) <= step ? target : pos - (uint32_t)step;
}

void servoCoreStep(ServoCore *c, uint32_t nowMs, bool uiAlive) {
	if (!uiAlive && c->running) {
		c->running = false;
		c->sweeping = false;
		c->tripped = true;
	}
	if (c->running && c->sweeping) {
		uint16_t curve;
		c->sweeping = sweep_step(&c->sweep, nowMs, &curve);
		// A sweep that has made its movements reports the centre, so the
		// servos go back to it.
		aim(c, c->sweepMask, curve);
	}
	// Thousandths of a unit a millisecond is units a second.
	uint64_t step = (uint64_t)c->slewPerS * (uint32_t)(nowMs - c->lastMs);
	c->lastMs = nowMs;
	for (int i = 0; i < 2; i++) {
		uint32_t want = (uint32_t)c->target[i] * 1000u;
		c->pos[i] = c->slewPerS == 0 ? want : approach(c->pos[i], want, step);
		c->outUs[i] = (c->running && c->bound[i])
		            ? servoCmdToUs(&c->prof, (uint16_t)((c->pos[i] + 500u) / 1000u))
		            : 0;
	}
}

bool servoStepCmd(const ServoCmd *cmd) {
	return cmd->kind == ServoCmdKind::Position && cmd->step;
}

// ---------------------------------------------------------------------------
// the switched supply, pure
// ---------------------------------------------------------------------------

void servoPowerReset(ServoPower *p, bool switched, bool fresh) {
	ServoPowerFault fault = fresh ? ServoPowerFault::None : p->fault;
	uint16_t up = fresh ? (uint16_t)SUPPLY_UP_NONE : p->upMs;
	memset(p, 0, sizeof(*p));
	p->switched = switched;
	p->phase = ServoPowerPhase::Off;
	p->offCheck = ServoOffCheck::None;
	// A cut's fault is kept with OWN too, as the trip it latched is.
	p->fault = fault;
	p->upMs = switched ? up : (uint16_t)SUPPLY_UP_NONE;
	// The memset has disarmed the guard.
}

/** @brief Whether @p in declares a sensor: CURRENT SENSE at INA219. */
static bool sensed(const ServoPowerIn *in) {
	return in->sample.state != (uint8_t)SenseState::None;
}

/** @brief supplyReadingAfter() of @p in's sample. */
static bool readingAfter(const ServoPowerIn *in, uint32_t sinceMs, uint32_t nowMs) {
	return supplyReadingAfter(&in->sample, sinceMs, nowMs);
}

ServoStartBlock servoPowerStartBlock(const ServoPower *p, const ServoPowerIn *in) {
	if (!p->switched) return ServoStartBlock::None;
	if (!in->touchOk) return ServoStartBlock::TouchSilent;
	SenseState st = (SenseState)in->sample.state;
	if (st == SenseState::Stuck) return ServoStartBlock::BusStuck;
	if (st != SenseState::None && st != SenseState::Online) return ServoStartBlock::NoSensor;
	return ServoStartBlock::None;
}

void servoPowerOff(ServoPower *p, bool withSensor, uint32_t nowMs) {
	if (p->phase == ServoPowerPhase::Off) return;
	p->phase = ServoPowerPhase::Off;
	p->offMs = nowMs;
	p->offCheck = withSensor ? ServoOffCheck::Waiting : ServoOffCheck::None;
}

/** @brief End the pulses and the sweep; @p trip latches the trip. */
static void stopPulses(ServoCore *c, bool trip) {
	c->running = false;
	c->sweeping = false;
	if (trip) c->tripped = true;
}

/** @brief The fault a cut of the guard records. */
static ServoPowerFault faultOf(GuardCut cut) {
	switch (cut) {
		case GuardCut::Hard:     return ServoPowerFault::Overcurrent;
		case GuardCut::Stall:    return ServoPowerFault::Stall;
		case GuardCut::Collapse: return ServoPowerFault::LowSupply;
		case GuardCut::Lost:     return ServoPowerFault::NoSensor;
		default:                 return ServoPowerFault::None;
	}
}

/**
 * @brief The guard's pass: armed at the first pass RUNNING with a sensor,
 *        disarmed at the first pass not. @return The fault of its cut.
 */
static ServoPowerFault guardPass(ServoPower *p, const ServoCore *c, const ServoPowerIn *in,
                                 uint32_t nowMs) {
	if (!c->running || !sensed(in)) {
		supplyGuardDisarm(&p->guard);
		return ServoPowerFault::None;
	}
	if (!p->guard.armed) supplyGuardArm(&p->guard, &in->sample, nowMs);
	// With OWN the bench knows neither the supply's voltage nor whether it
	// runs through the sensor: hard and stall only.
	GuardRules rules = { true, p->switched, p->switched };
	return faultOf(supplyGuardStep(&p->guard, &in->sample, &rules, nowMs));
}

void servoPowerStep(ServoPower *p, ServoCore *c, const ServoPowerIn *in, uint32_t nowMs) {
	bool withSensor = sensed(in);

	if (!p->switched) {
		// OWN: nothing to switch; a cut ends the pulses in this pass.
		if (c->running) p->fault = ServoPowerFault::None;
		ServoPowerFault f = guardPass(p, c, in, nowMs);
		if (f != ServoPowerFault::None) {
			stopPulses(c, true);
			p->fault = f;
			c->outUs[0] = c->outUs[1] = 0;
		}
		return;
	}

	// 1. The cuts: with touch silent or the bus stuck, STOP is out of reach;
	//    then the guard's.
	if (c->running) {
		ServoPowerFault f = ServoPowerFault::None;
		if (!in->touchOk)                                         f = ServoPowerFault::TouchSilent;
		else if (in->sample.state == (uint8_t)SenseState::Stuck) f = ServoPowerFault::BusStuck;
		else                                                      f = guardPass(p, c, in, nowMs);
		if (f != ServoPowerFault::None) {
			stopPulses(c, true);
			p->fault = f;
		}
	}
	if (!c->running) supplyGuardDisarm(&p->guard);

	// 2. Off whenever not RUNNING; on at START.
	if (!c->running) {
		servoPowerOff(p, withSensor, nowMs);
	} else if (p->phase == ServoPowerPhase::Off) {
		p->phase = ServoPowerPhase::Starting;
		p->onMs = nowMs;
		p->fault = ServoPowerFault::None;
		p->offCheck = ServoOffCheck::None;
		p->upMs = SUPPLY_UP_NONE;
	}

	// 3. The supply comes up: by the bus voltage, or by the time alone.
	if (p->phase == ServoPowerPhase::Starting) {
		uint32_t since = nowMs - p->onMs;
		if (withSensor) {
			if (readingAfter(in, p->onMs, nowMs) && in->sample.busMv >= SUPPLY_ON_MIN_MV) {
				uint32_t up = in->sample.takenMs - p->onMs;
				p->phase = ServoPowerPhase::On;
				p->upMs = (uint16_t)(up < SUPPLY_UP_NONE ? up : SUPPLY_UP_NONE - 1u);
			} else if (since >= SUPPLY_START_MAX_MS) {
				stopPulses(c, false);
				p->fault = ServoPowerFault::NoStart;
				servoPowerOff(p, withSensor, nowMs);
			}
		} else if (since >= SUPPLY_START_MAX_MS) {
			p->phase = ServoPowerPhase::On;
		}
	}

	// 4. No pulse until the supply is up, and CENTRE when it comes.
	if (p->phase == ServoPowerPhase::Starting) {
		c->sweeping = false;
		for (int i = 0; i < 2; i++) {
			c->target[i] = SERVO_CMD_CENTRE;
			c->pos[i] = SERVO_CMD_CENTRE * 1000u;
		}
	}
	if (p->phase != ServoPowerPhase::On) c->outUs[0] = c->outUs[1] = 0;

	// 5. The read-back after a switch-off: a warning, never a cut.
	if ((p->offCheck == ServoOffCheck::Waiting || p->offCheck == ServoOffCheck::NotOff) &&
	    readingAfter(in, p->offMs, nowMs)) {
		if (in->sample.busMv <= SUPPLY_OFF_MAX_MV)
			p->offCheck = ServoOffCheck::Off;
		else if (in->sample.busMv > SUPPLY_NOT_OFF_MV &&
		         in->sample.takenMs - p->offMs >= SUPPLY_NOT_OFF_MS)
			p->offCheck = ServoOffCheck::NotOff;
	}
}

// ---------------------------------------------------------------------------
// the job on core1, and the blocks it shares with core0
// ---------------------------------------------------------------------------

/** @brief Guards the shared block below. */
static critical_section_t s_cs;

/**
 * @defgroup servo_job_shared Shared block (under s_cs)
 * @brief Core0 writes the command and the profile; core1 writes the status.
 * @{
 */
static ServoCmd     s_cmd;         /**< The command held for core1. */
static ServoProfile s_profile;     /**< The profile core0 last handed over. */
static uint16_t     s_profileSeq;  /**< Bumped with each profile accepted. */
static ServoStatus  s_status;      /**< What core1 last published. */
static bool         s_fresh;       /**< A new screen session: the next bind forgets the trip. */
/** @} */

/**
 * @defgroup servo_job_core1 Core1 only
 * @{
 */
static ServoCore s_core;                   /**< The pulses' state. */
static uint8_t   s_pin[2] = {0xFF, 0xFF};  /**< SIG and AUX GPIO; 0xFF for none. */
static uint16_t  s_profileTaken;           /**< s_profileSeq of the profile bound. */
static uint16_t  s_written[2];             /**< The level last written per pin. */
static uint8_t   s_bindError;              /**< Why the last rebind failed. */
/** @brief A non-zero level was written since the pins last went quiet. */
static bool      s_pulsed;
static uint32_t  s_zeroMs;                 /**< millis() of the last level-0 write. */
static ServoPower s_power;                 /**< The switched supply's state. */
static ServoStartBlock s_startBlock;       /**< Why START is refused, as of the last pass. */
static MoveTimer s_move;                   /**< The move timing of the steps. */
/** @} */

/** @brief Release whatever is bound. */
static void unbind() {
	for (int i = 0; i < 2; i++)
		if (s_core.bound[i]) servoPwmRelease(s_pin[i]);
	s_core.bound[SERVO_A] = s_core.bound[SERVO_B] = false;
}

/**
 * @brief Bind SIG, and AUX for two servos, at @p p's rate, into SAFE. A
 *        refused bind releases what it took.
 * @return CORE1_JOB_OK or CORE1_JOB_PWM.
 */
static uint8_t bindAt(const ServoProfile *p, uint16_t ackSeq) {
	bool wantB = p->servos == 2 && s_pin[SERVO_B] != 0xFF;
	bool a = servoPwmBind(s_pin[SERVO_A], p->rateHz);
	bool b = a && wantB && servoPwmBind(s_pin[SERVO_B], p->rateHz);
	bool tripped = s_core.tripped;
	if (!a || (wantB && !b)) {
		if (a) servoPwmRelease(s_pin[SERVO_A]);
		servoCoreReset(&s_core, p, false, false, ackSeq, tripped);
		return CORE1_JOB_PWM;
	}
	servoCoreReset(&s_core, p, true, b, ackSeq, tripped);
	return CORE1_JOB_OK;
}

/**
 * @brief Put level 0 on every bound pin that carries a pulse, and say
 *        whether the last pulse is over: servoQuietDone() since the last
 *        level-0 write.
 */
static bool quiet(uint32_t nowMs) {
	for (int i = 0; i < 2; i++) {
		if (!s_core.bound[i] || s_written[i] == 0) continue;
		servoPwmWrite(s_pin[i], 0);
		s_written[i] = 0;
		s_zeroMs = nowMs;
	}
	if (!servoQuietDone(s_pulsed, s_zeroMs, nowMs, s_core.prof.rateHz)) return false;
	s_pulsed = false;
	return true;
}

/** @brief Write each pin whose pulse changed, then publish the status. */
static void output(uint32_t nowMs) {
	for (int i = 0; i < 2; i++) {
		if (!s_core.bound[i] || s_core.outUs[i] == s_written[i]) continue;
		servoPwmWrite(s_pin[i], s_core.outUs[i]);
		s_written[i] = s_core.outUs[i];
		if (s_written[i]) s_pulsed = true;
		else              s_zeroMs = nowMs;
	}
	ServoStatus st;
	st.ackSeq     = s_core.ackSeq;
	st.running    = s_core.running;
	st.tripped    = s_core.tripped;
	st.sweeping   = s_core.sweeping;
	st.bindError  = s_bindError;
	st.outUs[0]   = s_core.outUs[0];
	st.outUs[1]   = s_core.outUs[1];
	st.sweepMoves = s_core.sweeping ? sweep_moves(&s_core.sweep, nowMs) : 0;
	st.switched   = s_power.switched;
	st.power      = s_power.phase;
	st.powerFault = s_power.fault;
	st.offCheck   = s_power.offCheck;
	st.startBlock = s_startBlock;
	st.upMs       = s_power.upMs;
	st.move       = s_move.res;
	critical_section_enter_blocking(&s_cs);
	s_status = st;
	critical_section_exit(&s_cs);
}

/** @brief This pass's inputs of the switched supply: the sensor's reading and the touch stamp. */
static ServoPowerIn powerIn(uint32_t nowMs) {
	ServoPowerIn in;
	senseSample(&in.sample);
	// The touch controller's own stamp, by the heartbeat's rule: an answer
	// within 250 ms.
	in.touchOk = escUiAlive(nowMs, touchAnsweredMs());
	return in;
}

/** @brief The enable pin at the level the supply's phase asks for. */
static void drivePower() {
	if (!s_power.switched) return;
	bool on = s_power.phase != ServoPowerPhase::Off;
	if (on != servoSupplyIsOn()) {
		if (on) servoSupplyOn();
		else    servoSupplyOff();
	}
}

static void servoJobInit() {
	critical_section_init(&s_cs);
	servoProfileDefault(&s_profile);
	s_status.upMs = SUPPLY_UP_NONE;
}

static uint8_t servoJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	critical_section_enter_blocking(&s_cs);
	ServoProfile p = s_profile;
	s_profileTaken = s_profileSeq;
	uint16_t seq = s_cmd.seq;
	bool fresh = s_fresh;
	s_fresh = false;
	critical_section_exit(&s_cs);

	// A new screen session forgets an old trip; a bind after BACK TO SERVO on
	// the leave page keeps it until START.
	if (fresh) s_core.tripped = false;
	s_pin[SERVO_A] = sigGpio;
	s_pin[SERVO_B] = auxGpio;
	s_written[0] = s_written[1] = 0;
	s_pulsed = false;
	s_bindError = CORE1_JOB_OK;
	uint8_t err = bindAt(&p, seq);
	// The supply comes up off: its enable pin driven low until the release.
	// A refused bind is never released, so it leaves the pin undriven.
	servoPowerReset(&s_power, err == CORE1_JOB_OK && servoSupplyFitted(), fresh);
	moveTimerReset(&s_move, fresh);
	if (s_power.switched) servoSupplyBind();
	uint32_t now = millis();
	ServoPowerIn in = powerIn(now);
	s_startBlock = servoPowerStartBlock(&s_power, &in);
	output(now);
	return err;
}

static void servoJobStep(uint32_t nowMs, bool uiAlive) {
	critical_section_enter_blocking(&s_cs);
	ServoCmd cmd = s_cmd;
	ServoProfile p = s_profile;
	uint16_t pseq = s_profileSeq;
	critical_section_exit(&s_cs);

	// core0 hands over a profile only while SAFE with no command pending,
	// so a profile taken here never moves a running servo. The rebind waits
	// until the last pulse is over, and takes no command before it.
	if (pseq != s_profileTaken) {
		if (!quiet(nowMs)) {
			output(nowMs);
			return;
		}
		s_profileTaken = pseq;
		unbind();
		s_written[0] = s_written[1] = 0;
		s_bindError = bindAt(&p, s_core.ackSeq);
	}
	ServoPowerIn in = powerIn(nowMs);
	s_startBlock = servoPowerStartBlock(&s_power, &in);
	bool taken = servoCoreTake(&s_core, &cmd, nowMs, uiAlive && s_startBlock == ServoStartBlock::None);
	servoCoreStep(&s_core, nowMs, uiAlive);
	servoPowerStep(&s_power, &s_core, &in, nowMs);
	// The move timing reads what the rules above decided, and changes none of it.
	MoveIn mi;
	mi.sample = in.sample;
	mi.out[0] = s_core.outUs[0];
	mi.out[1] = s_core.outUs[1];
	mi.step = taken && servoStepCmd(&cmd);
	mi.slewed = cmd.slewPerS != 0;
	moveTimerStep(&s_move, &mi, nowMs);
	// The enable pin first: at a switch-off it goes low before level 0 on
	// the pins, at a switch-on the pins stay at level 0 until the supply is up.
	drivePower();
	output(nowMs);
}

static bool servoJobRelease(uint32_t nowMs) {
	// First pass: no more pulses, and the supply off. The pins let go once
	// the last pulse is over.
	s_core.running = false;
	s_core.sweeping = false;
	s_core.outUs[0] = s_core.outUs[1] = 0;
	servoPowerOff(&s_power, false, nowMs);
	drivePower();
	MoveIn none;
	memset(&none, 0, sizeof(none));
	moveTimerStep(&s_move, &none, nowMs);
	if (!quiet(nowMs)) {
		output(nowMs);
		return false;
	}
	// The enable pin an input again before the pins return to DShot.
	if (s_power.switched) servoSupplyRelease();
	s_power.switched = false;
	unbind();
	servoCoreReset(&s_core, &s_core.prof, false, false, s_core.ackSeq, s_core.tripped);
	s_written[0] = s_written[1] = 0;
	s_bindError = CORE1_JOB_OK;
	output(nowMs);
	return true;
}

const Core1JobOps SERVO_JOB_OPS = {
	servoJobBind, servoJobStep, servoJobRelease, servoJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

void servoJobNewSession() {
	// The job is idle when a screen opens, so nothing publishes over the
	// cleared latch before the next bind, which forgets the trip as well.
	critical_section_enter_blocking(&s_cs);
	s_fresh = true;
	s_status.tripped = false;
	s_status.powerFault = ServoPowerFault::None;
	s_status.upMs = SUPPLY_UP_NONE;
	memset(&s_status.move, 0, sizeof(s_status.move));
	critical_section_exit(&s_cs);
}

bool servoJobProfile(const ServoProfile *p) {
	if (!servoProfileValid(p)) return false;
	critical_section_enter_blocking(&s_cs);
	bool ok = !s_status.running && s_status.ackSeq == s_cmd.seq;
	if (ok) {
		s_profile = *p;
		s_profileSeq++;
	}
	critical_section_exit(&s_cs);
	return ok;
}

uint16_t servoJobCommand(const ServoCmd *c) {
	critical_section_enter_blocking(&s_cs);
	bool keepSafe = s_cmd.kind == ServoCmdKind::Safe && s_status.ackSeq != s_cmd.seq;
	if (!keepSafe) {
		uint16_t seq = (uint16_t)(s_cmd.seq + 1);
		s_cmd = *c;
		s_cmd.seq = seq;
	}
	uint16_t held = s_cmd.seq;
	critical_section_exit(&s_cs);
	return held;
}

void servoJobStatus(ServoStatus *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_status;
	critical_section_exit(&s_cs);
}
