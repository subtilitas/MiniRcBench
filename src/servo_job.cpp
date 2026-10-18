/**
 * @file servo_job.cpp
 * @brief The servo tester's core1 job. @see servo_job.h
 */

#include "servo_job.h"
#include "servo_pwm.h"

#include "plat.h"
#include <pico/critical_section.h>
#include <string.h>

// ---------------------------------------------------------------------------
// the pure core
// ---------------------------------------------------------------------------

/** @brief Index of servo A, on SIG, and of servo B, on AUX. */
enum : uint8_t { SERVO_A = 0, SERVO_B = 1 };

void servoCoreReset(ServoCore *c, const ServoProfile *p, bool boundA,
                    bool boundB, uint16_t ackSeq) {
	memset(c, 0, sizeof(*c));
	c->prof = *p;
	c->bound[SERVO_A] = boundA;
	c->bound[SERVO_B] = boundB;
	c->ackSeq = ackSeq;
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
	if (!a || (wantB && !b)) {
		if (a) servoPwmRelease(s_pin[SERVO_A]);
		servoCoreReset(&s_core, p, false, false, ackSeq);
		return CORE1_JOB_PWM;
	}
	servoCoreReset(&s_core, p, true, b, ackSeq);
	return CORE1_JOB_OK;
}

/** @brief Write each pin whose pulse changed, then publish the status. */
static void output(uint32_t nowMs) {
	for (int i = 0; i < 2; i++) {
		if (!s_core.bound[i] || s_core.outUs[i] == s_written[i]) continue;
		servoPwmWrite(s_pin[i], s_core.outUs[i]);
		s_written[i] = s_core.outUs[i];
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
	critical_section_enter_blocking(&s_cs);
	s_status = st;
	critical_section_exit(&s_cs);
}

static void servoJobInit() {
	critical_section_init(&s_cs);
	servoProfileDefault(&s_profile);
}

static uint8_t servoJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	critical_section_enter_blocking(&s_cs);
	ServoProfile p = s_profile;
	s_profileTaken = s_profileSeq;
	uint16_t seq = s_cmd.seq;
	critical_section_exit(&s_cs);

	s_pin[SERVO_A] = sigGpio;
	s_pin[SERVO_B] = auxGpio;
	s_written[0] = s_written[1] = 0;
	s_bindError = CORE1_JOB_OK;
	uint8_t err = bindAt(&p, seq);
	output(millis());
	return err;
}

static void servoJobStep(uint32_t nowMs, bool uiAlive) {
	critical_section_enter_blocking(&s_cs);
	ServoCmd cmd = s_cmd;
	ServoProfile p = s_profile;
	uint16_t pseq = s_profileSeq;
	critical_section_exit(&s_cs);

	// core0 hands over a profile only while SAFE with no command pending,
	// so a profile taken here never moves a running servo.
	if (pseq != s_profileTaken) {
		s_profileTaken = pseq;
		unbind();
		s_written[0] = s_written[1] = 0;
		s_bindError = bindAt(&p, s_core.ackSeq);
	}
	servoCoreTake(&s_core, &cmd, nowMs, uiAlive);
	servoCoreStep(&s_core, nowMs, uiAlive);
	output(nowMs);
}

static void servoJobRelease() {
	unbind();
	servoCoreReset(&s_core, &s_core.prof, false, false, s_core.ackSeq);
	s_written[0] = s_written[1] = 0;
	s_bindError = CORE1_JOB_OK;
	output(millis());
}

const Core1JobOps SERVO_JOB_OPS = {
	servoJobBind, servoJobStep, servoJobRelease, servoJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

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
