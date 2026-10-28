/**
 * @file hitec_job.cpp
 * @brief The Hitec programmer's core1 job. @see hitec_job.h
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 */

#include "hitec_job.h"
#include "hitec_line.h"
#include "hitec_line_port.h"
#include "kst_seek.h"
#include "servo_supply.h"
#include "esc_task.h"

#include "plat.h"
#include <pico/critical_section.h>
#include <string.h>

/** @brief Guards the shared block below. */
static critical_section_t s_cs;

/**
 * @defgroup hitec_job_shared Shared block (under s_cs)
 * @brief Core0 writes the command and the family; core1 writes the status.
 * @{
 */
static HitecCmd    s_cmd;                     /**< The command held for core1. */
static HitecStatus s_status;                  /**< What core1 last published. */
static uint8_t     s_family = HITEC_D;        /**< The family the next bind opens. */
/** @} */

/**
 * @defgroup hitec_job_core1 Core1 only
 * @{
 */
static hitec_session_t s_ses;          /**< The session. */
static HitecJobState   s_state;        /**< Where the job is. */
static HitecRefusal    s_refused;      /**< Of the last command taken. */
static uint16_t        s_ackSeq;       /**< Seq of the last command taken. */
static uint8_t         s_bound;        /**< The family of this claim. */
static uint8_t         s_lineError;    /**< `hitec_line_err_t` of the bind. */
static uint8_t         s_result;       /**< `hitec_ses_t` of the last entry or operation. */
static bool            s_lastWrite;    /**< The last operation was a write. */
static bool            s_opWrite;      /**< The running operation is a write. */
static bool            s_firstRead;    /**< The running read follows the entry: its image is the backup. */
static bool            s_hasImage;     /**< A full read has completed since the entry. */
static bool            s_hasBackup;    /**< @c s_backup is valid. */
static hitec_image_t   s_backup;       /**< The first full read after the entry. */
static bool            s_wrote;        /**< A write ran since the entry. */
static bool            s_out;          /**< A transaction is on the line. */
static bool            s_aborting;     /**< hitec_session_abort() was called for the running operation. */
static HitecPower      s_power;        /**< Where an entry's POWER step is. */
static bool            s_stalled;      /**< A lapse ended the session. */
static bool            s_stallWrote;   /**< The stalled session had written. */
static bool            s_drain;        /**< Leaving: the running operation still runs. */
static bool            s_rebooting;    /**< Leaving with NONE: the D series reboot runs. */
static bool            s_rebooted;     /**< Leaving with NONE: the reboot has run or is not owed. */
static bool            s_switched;     /**< The bench switches the servo's supply, from the bind. */
static bool            s_supplyUsed;   /**< The supply was on in this claim: a close waits. */
static uint32_t        s_offMs;        /**< Clock of the last switch-off command, or of the bind. */
/** @brief The heartbeat lapsed while the supply was on: once what runs has
 *         ended, the supply goes off, whatever the heartbeat does meanwhile. */
static bool            s_lapsed;
static bool            s_cardPhase;    /**< A card phase is open. */
static uint32_t        s_cardPhaseMs;  /**< Clock when it was acknowledged. */
static HitecStatus     s_published;    /**< The status last copied to s_status. */
/** @} */

/** @brief Core0 writes it, core1 reads it: millis() of the last card stamp. */
static volatile uint32_t s_cardStampMs;

void hitecCardStamp() { s_cardStampMs = millis(); }

// ---------------------------------------------------------------------------
// the switched supply
// ---------------------------------------------------------------------------

/** @brief Drive the supply @p on; a switch-off records its time. Nothing without a switch. */
static void supplySet(bool on, uint32_t nowMs) {
	if (!s_switched || on == servoSupplyIsOn()) return;
	if (on) {
		servoSupplyOn();
		s_supplyUsed = true;
	} else {
		servoSupplyOff();
		s_offMs = nowMs;
		// The lapse latch guards a supply left on: a switch-off ends it.
		s_lapsed = false;
	}
}

/** @brief Time left of the off wait under way, ms; 0 for none. */
static uint32_t supplyLeftMs(uint32_t nowMs) {
	bool waiting = (s_state == HitecJobState::Entering && s_power == HitecPower::Off) ||
	               (s_state == HitecJobState::Leaving && s_supplyUsed && !servoSupplyIsOn());
	if (!waiting) return 0;
	uint32_t gone = nowMs - s_offMs;
	// The waits end after more than KST_SUPPLY_OFF_MS whole ms: count that last ms.
	return gone > KST_SUPPLY_OFF_MS ? 0 : KST_SUPPLY_OFF_MS - gone + 1u;
}

// ---------------------------------------------------------------------------
// running the session's steps
// ---------------------------------------------------------------------------

/** @brief Feed the step that is out a result with no capture. */
static void feedRun(uint8_t run, uint32_t nowMs) {
	hitec_ses_result_t res = { run, (uint8_t)HITEC_RX_OK, 0 };
	(void)hitec_session_feed(&s_ses, nowMs, &res);
}

/** @brief Decode @p cap by the step's decoder, as rcbench's HITEC page does, and feed it. */
static void feedCapture(const hitec_ses_step_t *st, const hitec_capture_t *cap, uint32_t nowMs) {
	hitec_reply_t rep;
	memset(&rep, 0, sizeof(rep));
	hitec_rx_t rx = HITEC_RX_OK;
	switch (st->decoder) {
		case HITEC_DEC_C: {
			hitec_c_expect_t e = HITEC_C_EXPECT_VALUE;
			(void)hitec_c_expect_of(st->dec_arg, &e);
			rx = hitec_c_decode(cap, e, &rep);
			break;
		}
		case HITEC_DEC_D:      rx = hitec_d_decode(cap, st->dec_arg, &rep); break;
		case HITEC_DEC_LISTEN: rx = hitec_listen_decode(cap, st->dec_arg, &rep); break;
		default:               break;
	}
	hitec_ses_result_t res;
	res.run = cap->aborted ? (uint8_t)HITEC_SES_CUT : (uint8_t)HITEC_SES_RAN;
	res.rx = (uint8_t)rx;
	res.value = rep.value;
	(void)hitec_session_feed(&s_ses, nowMs, &res);
}

/** @brief Poll the transaction on the line; its capture is fed when it comes. */
static void pollStep(const hitec_ses_step_t *st, uint32_t nowMs) {
	hitec_capture_t cap;
	if (!hitecLinePoll(&cap)) return;
	s_out = false;
	s_power = HitecPower::None;
	feedCapture(st, &cap, nowMs);
}

/** @brief One pass of a POWER step: off for its off time, then on and its LISTEN. */
static void powerStep(const hitec_ses_step_t *st, uint32_t nowMs, bool uiAlive) {
	if (!s_switched) {
		feedRun(HITEC_SES_FAILED, nowMs);
		return;
	}
	if (s_power == HitecPower::None) {
		supplySet(false, nowMs);
		s_power = HitecPower::Off;
	}
	if (s_power == HitecPower::Off) {
		// A lapse or an abort ends the off phase with the supply never on.
		if (s_aborting || !uiAlive) {
			s_power = HitecPower::None;
			feedRun(HITEC_SES_FAILED, nowMs);
			return;
		}
		// "<=": the clock counts whole ms, so off_ms ticks can be a fraction less.
		if (nowMs - s_offMs <= st->off_ms) return;
		supplySet(true, nowMs);
		if (!hitecLineStart(&st->xfer)) {
			s_power = HitecPower::None;
			feedRun(HITEC_SES_FAILED, nowMs);
			return;
		}
		s_out = true;
		s_power = HitecPower::Listen;
	}
	pollStep(st, nowMs);
}

/** @brief One pass of the session. @return True when no operation runs. */
static bool runSession(uint32_t nowMs, bool uiAlive) {
	hitec_ses_step_t st;
	switch (hitec_session_next(&s_ses, nowMs, &st)) {
		case HITEC_SES_STEP_DONE:
			s_power = HitecPower::None;
			return true;
		case HITEC_SES_STEP_XFER:
			if (!s_out) {
				if (!hitecLineStart(&st.xfer)) {
					feedRun(HITEC_SES_FAILED, nowMs);
					return false;
				}
				s_out = true;
			}
			pollStep(&st, nowMs);
			return false;
		case HITEC_SES_STEP_POWER:
			powerStep(&st, nowMs, uiAlive);
			return false;
		default:
			return false;
	}
}

/**
 * @brief Abort the running operation: a LISTEN is cut, a SEND finishes, an
 *        off phase ends at its next pass. The session ends it with
 *        `HITEC_SES_ERR_ABORTED` once the step's result is fed.
 */
static void abortOp(uint32_t nowMs) {
	if (s_aborting) return;
	s_aborting = true;
	if (!hitec_session_abort(&s_ses) || !s_out) return;
	hitec_ses_step_t st;
	(void)hitec_session_next(&s_ses, nowMs, &st);
	if (s_power == HitecPower::Listen || st.xfer.kind == HITEC_XFER_LISTEN) hitecLineAbort();
}

// ---------------------------------------------------------------------------
// core1
// ---------------------------------------------------------------------------

/** @brief True while the image lacks an address a write needs. */
static bool imageUnknown() {
	if (!s_hasImage) return false;
	for (unsigned a = 0; a < HITEC_MEM_SIZE; a++) {
		bool needed = hitec_writable(s_bound, (uint8_t)a) ||
		              (s_bound == HITEC_CLASSIC && a == HITEC_C_SUM_LAST);
		if (needed && !hitec_image_known(&s_ses.image, (uint8_t)a)) return true;
	}
	return false;
}

static void publish(uint32_t nowMs) {
	HitecStatus st;
	memset(&st, 0, sizeof(st));
	st.ackSeq       = s_ackSeq;
	st.job          = s_state;
	st.refused      = s_refused;
	st.family       = s_bound;
	st.lineError    = s_lineError;
	st.result       = s_result;
	st.lastWrite    = s_lastWrite;
	st.lastRx       = s_ses.last_rx;
	st.failAddr     = s_ses.fail_addr;
	st.tries        = s_ses.tries;
	st.done         = s_ses.done;
	st.total        = s_ses.total;
	st.stepIndex    = s_ses.step_index;
	st.planSteps    = s_ses.plan.n;
	st.planResult   = s_ses.plan_result;
	st.inMode       = s_ses.in_mode != 0;
	st.hasImage     = s_hasImage;
	st.hasBackup    = s_hasBackup;
	st.unknown      = imageUnknown();
	st.power        = s_power;
	st.supplyFitted = s_switched;
	st.supplyOn     = servoSupplyIsOn();
	st.supplyUsed   = s_supplyUsed;
	st.supplyLeftDs = (uint16_t)((supplyLeftMs(nowMs) + 99u) / 100u);
	st.stalled      = s_stalled;
	st.stallWrote   = s_stallWrote;
	st.cardPhase    = s_cardPhase;
	memcpy(st.diff, s_ses.diff, sizeof(st.diff));
	st.image        = s_ses.image;
	if (s_hasBackup) st.backup = s_backup;
	// Both copies are built from a cleared struct, so the padding compares too.
	if (memcmp(&st, &s_published, sizeof(st)) == 0) return;
	s_published = st;
	critical_section_enter_blocking(&s_cs);
	s_status = st;
	critical_section_exit(&s_cs);
}

/** @brief A new session: not in mode, no image, no backup. */
static void freshSession() {
	hitec_session_init(&s_ses, s_bound, HITEC_SES_CLOCK_MS);
	s_hasImage = false;
	s_hasBackup = false;
	s_wrote = false;
	s_out = false;
	s_aborting = false;
	s_power = HitecPower::None;
}

/** @brief A lapse: the supply off, the session gone, the stalled view. */
static void stall(uint32_t nowMs) {
	s_stallWrote = s_wrote;
	s_lapsed = false;
	supplySet(false, nowMs);
	freshSession();
	s_stalled = true;
	s_state = HitecJobState::EntryEnded;
}

/** @brief Start the full read; the one after the entry becomes the backup. */
static void startRead(bool first) {
	s_opWrite = false;
	s_lastWrite = false;
	s_firstRead = first;
	hitec_ses_t r = hitec_session_read_all(&s_ses);
	if (r == HITEC_SES_BUSY) {
		s_state = HitecJobState::Busy;
	} else {
		s_result = (uint8_t)r;
		s_state = HitecJobState::InMode;
	}
}

/** @brief Leave from wherever the job is: a write finishes, a read or an entry is cut. */
static void beginLeave(uint32_t nowMs) {
	s_cardPhase = false;
	s_drain = s_state == HitecJobState::Entering || s_state == HitecJobState::Busy;
	if (s_drain && !s_opWrite) abortOp(nowMs);
	s_rebooting = false;
	s_rebooted = false;
	s_state = HitecJobState::Leaving;
}

/** @brief One pass of a leave. @return True once the line is closed. */
static bool leaveStep(uint32_t nowMs) {
	if (s_state == HitecJobState::Closed) return true;
	if (s_drain) {
		if (!runSession(nowMs, true)) return false;
		s_result = (uint8_t)hitec_session_result(&s_ses);
		s_aborting = false;
		s_drain = false;
	}
	// With NONE a D series servo in the mode restarts from its saved values:
	// the reboot. With the switch the supply's off time does that.
	if (!s_rebooted) {
		if (!s_rebooting) {
			bool owed = !s_switched && s_bound == HITEC_D && s_ses.in_mode;
			s_rebooting = owed && hitec_session_leave(&s_ses) == HITEC_SES_BUSY;
		}
		if (s_rebooting && !runSession(nowMs, true)) return false;
		s_rebooting = false;
		s_rebooted = true;
	}
	supplySet(false, nowMs);
	// The close rule: a servo that was powered in this claim is off for
	// KST_SUPPLY_OFF_MS before DShot can reach it. Time off before counts.
	if (s_supplyUsed && nowMs - s_offMs <= KST_SUPPLY_OFF_MS) return false;
	hitecLineClose();
	s_state = HitecJobState::Closed;
	return true;
}

static void take(const HitecCmd *cmd, uint32_t nowMs, bool uiAlive) {
	s_ackSeq = cmd->seq;
	s_refused = HitecRefusal::None;
	// Any command closes a card phase.
	if (cmd->op != HitecOp::CardBegin) s_cardPhase = false;
	bool leaving = s_state == HitecJobState::Leaving || s_state == HitecJobState::Closed;
	bool starts = cmd->op == HitecOp::Enter || cmd->op == HitecOp::ReadAll ||
	              cmd->op == HitecOp::Write;
	bool ready = false;
	switch (cmd->op) {
		case HitecOp::Enter:
			ready = (s_state == HitecJobState::Opening || s_state == HitecJobState::EntryEnded) &&
			        (s_bound == HITEC_D || s_switched);
			break;
		case HitecOp::ReadAll:
			ready = s_state == HitecJobState::InMode;
			break;
		case HitecOp::Write:
			ready = s_state == HitecJobState::InMode && s_hasImage;
			break;
		case HitecOp::Abort:
			ready = s_state == HitecJobState::Entering ||
			        (s_state == HitecJobState::Busy && !s_opWrite);
			break;
		case HitecOp::Leave:
			ready = !leaving;
			break;
		case HitecOp::CardBegin:
			ready = s_state == HitecJobState::Opening || s_state == HitecJobState::EntryEnded ||
			        s_state == HitecJobState::InMode;
			break;
		case HitecOp::CardEnd:
			ready = true;
			break;
	}
	if (!ready) {
		s_refused = HitecRefusal::State;
		return;
	}
	if (starts && (!uiAlive || s_lapsed)) {
		s_refused = HitecRefusal::NoUi;
		return;
	}
	if (cmd->op == HitecOp::Write && imageUnknown()) {
		s_refused = HitecRefusal::Unknown;
		return;
	}
	switch (cmd->op) {
		case HitecOp::Enter: {
			s_stalled = false;
			s_stallWrote = false;
			s_lastWrite = false;
			freshSession();
			// Classic always cycles the supply; the D series does with the switch.
			hitec_ses_t r = hitec_session_enter(&s_ses, s_switched, KST_SUPPLY_OFF_MS);
			if (r == HITEC_SES_BUSY) s_state = HitecJobState::Entering;
			else                     s_result = (uint8_t)r;
			break;
		}
		case HitecOp::ReadAll:
			startRead(false);
			break;
		case HitecOp::Write: {
			s_lastWrite = true;
			s_opWrite = true;
			hitec_ses_t r = hitec_session_write(&s_ses, &cmd->target);
			if (r == HITEC_SES_BUSY) {
				s_wrote = true;
				s_state = HitecJobState::Busy;
			} else {
				s_opWrite = false;
				s_result = (uint8_t)r;
			}
			break;
		}
		case HitecOp::Abort:
			abortOp(nowMs);
			break;
		case HitecOp::Leave:
			beginLeave(nowMs);
			break;
		case HitecOp::CardBegin:
			if (!s_cardPhase) s_cardPhaseMs = nowMs;
			s_cardPhase = true;
			break;
		case HitecOp::CardEnd:
			break;
	}
}

static void advance(uint32_t nowMs, bool uiAlive) {
	switch (s_state) {
		case HitecJobState::Entering: {
			// A lapse cuts the entry: an off phase never switches on.
			if (!uiAlive) abortOp(nowMs);
			if (!runSession(nowMs, uiAlive)) break;
			s_aborting = false;
			s_result = (uint8_t)hitec_session_result(&s_ses);
			if (!uiAlive || s_lapsed) {
				stall(nowMs);
			} else if (s_result == HITEC_SES_OK) {
				startRead(true);
			} else {
				// An entry that failed leaves the servo unpowered.
				supplySet(false, nowMs);
				s_state = HitecJobState::EntryEnded;
			}
			break;
		}
		case HitecJobState::Busy: {
			if (!runSession(nowMs, uiAlive)) break;
			s_aborting = false;
			s_result = (uint8_t)hitec_session_result(&s_ses);
			if (!s_opWrite && s_result == HITEC_SES_OK) {
				s_hasImage = true;
				if (s_firstRead) {
					s_backup = s_ses.image;
					s_hasBackup = true;
				}
			}
			s_opWrite = false;
			s_firstRead = false;
			s_state = HitecJobState::InMode;
			// The operation has finished; a lapse during it switches off now.
			if ((!uiAlive || s_lapsed) && servoSupplyIsOn()) stall(nowMs);
			break;
		}
		case HitecJobState::InMode:
			if ((!uiAlive || s_lapsed) && servoSupplyIsOn()) stall(nowMs);
			break;
		case HitecJobState::Leaving:
			leaveStep(nowMs);
			break;
		default:
			break;
	}
}

static void hitecJobInit() {
	critical_section_init(&s_cs);
}

/** @brief The `hitec_line_err_t` of a refused open as the bind failure a screen names. */
static uint8_t bindError(uint8_t lineErr) {
	switch (lineErr) {
		case HITEC_LINE_ERR_CLOCK: return CORE1_JOB_CLOCK;
		case HITEC_LINE_ERR_DMA:   return CORE1_JOB_DMA;
		// HITEC_LINE_ERR_ARG: the port's fixed arguments never cause it.
		default:                   return CORE1_JOB_PIO;
	}
}

static uint8_t hitecJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	(void)auxGpio;
	critical_section_enter_blocking(&s_cs);
	uint16_t seq = s_cmd.seq;
	s_bound = s_family;
	critical_section_exit(&s_cs);

	memset(&s_published, 0xFF, sizeof(s_published));
	s_state = HitecJobState::Opening;
	s_refused = HitecRefusal::None;
	s_ackSeq = seq;
	s_result = HITEC_SES_OK;
	s_lastWrite = false;
	s_opWrite = false;
	s_firstRead = false;
	s_stalled = false;
	s_stallWrote = false;
	s_drain = false;
	s_rebooting = false;
	s_rebooted = false;
	// The supply comes up off: GP0 driven low until the release.
	s_switched = servoSupplyFitted();
	s_supplyUsed = false;
	s_offMs = millis();
	s_lapsed = false;
	s_cardPhase = false;
	freshSession();
	s_lineError = hitecLineOpen(sigGpio, s_bound);
	if (s_lineError == HITEC_LINE_OK) {
		// Only a bound job is released: a failed bind leaves GP0 undriven.
		servoSupplyBind();
	} else {
		s_state = HitecJobState::Closed;
	}
	publish(millis());
	return s_lineError == HITEC_LINE_OK ? (uint8_t)CORE1_JOB_OK : bindError(s_lineError);
}

static void hitecJobStep(uint32_t nowMs, bool uiAlive) {
	critical_section_enter_blocking(&s_cs);
	HitecCmd cmd = s_cmd;
	critical_section_exit(&s_cs);

	// In a card phase core0 waits on the card, and the driver's waits stamp:
	// that counts as the heartbeat, for at most HITEC_CARD_PHASE_MAX_MS.
	if (s_cardPhase && !uiAlive) {
		uint32_t stamp = s_cardStampMs;
		uiAlive = escUiAlive(nowMs, stamp) && nowMs - s_cardPhaseMs < HITEC_CARD_PHASE_MAX_MS;
	}
	// A lapse with the supply on is latched until what runs has ended.
	if (!uiAlive && servoSupplyIsOn()) s_lapsed = true;
	if (cmd.seq != s_ackSeq) take(&cmd, nowMs, uiAlive);
	advance(nowMs, uiAlive);
	publish(nowMs);
}

static bool hitecJobRelease(uint32_t nowMs) {
	if (s_state != HitecJobState::Leaving && s_state != HitecJobState::Closed) beginLeave(nowMs);
	bool done = leaveStep(nowMs);
	// GP0 an input again before the pins return to DShot.
	if (done) servoSupplyRelease();
	publish(nowMs);
	return done;
}

const Core1JobOps HITEC_JOB_OPS = {
	hitecJobBind, hitecJobStep, hitecJobRelease, hitecJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

void hitecJobSetFamily(uint8_t family) {
	critical_section_enter_blocking(&s_cs);
	s_family = family == HITEC_CLASSIC ? (uint8_t)HITEC_CLASSIC : (uint8_t)HITEC_D;
	critical_section_exit(&s_cs);
}

/** @brief A command that a later one must not replace before core1 takes it. */
static bool isStop(HitecOp op) { return op == HitecOp::Abort || op == HitecOp::Leave; }

uint16_t hitecJobCommand(const HitecCmd *c) {
	critical_section_enter_blocking(&s_cs);
	bool pending = s_status.ackSeq != s_cmd.seq;
	bool keep = pending && isStop(s_cmd.op) &&
	            !(c->op == HitecOp::Leave && s_cmd.op != HitecOp::Leave);
	if (!keep) {
		uint16_t seq = (uint16_t)(s_cmd.seq + 1);
		s_cmd = *c;
		s_cmd.seq = seq;
	}
	uint16_t held = s_cmd.seq;
	critical_section_exit(&s_cs);
	return held;
}

void hitecJobStatus(HitecStatus *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_status;
	critical_section_exit(&s_cs);
}
