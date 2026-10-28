/**
 * @file kst_job.cpp
 * @brief The KST programmer's core1 job. @see kst_job.h
 */

#include "kst_job.h"
#include "kst_line.h"
#include "kst_line_port.h"
#include "servo_supply.h"

#include "plat.h"
#include <pico/critical_section.h>
#include <string.h>

/** @brief Guards the shared block below. */
static critical_section_t s_cs;

/**
 * @defgroup kst_job_shared Shared block (under s_cs)
 * @brief Core0 writes the command; core1 writes the status.
 * @{
 */
static KstCmd    s_cmd;     /**< The command held for core1. */
static KstStatus s_status;  /**< What core1 last published. */
/** @} */

/**
 * @defgroup kst_job_core1 Core1 only
 * @{
 */
static kst_session_t s_ses;          /**< The session, 856 bytes. */
static KstSeekState  s_seek;         /**< The search. */
static kst_driver_t  s_line;         /**< The port's driver, under the counting one. */
static KstJobState   s_state;        /**< Where the job is. */
static KstRefusal    s_refused;      /**< Of the last command taken. */
static uint16_t      s_ackSeq;       /**< Seq of the last command taken. */
static uint8_t       s_lineError;    /**< `kst_line_err_t` of the bind. */
static uint8_t       s_result;       /**< `kst_ses_t` of the last operation. */
static uint32_t      s_frames;       /**< Frames of the running operation. */
static bool          s_stalled;      /**< A lapse ended the last search. */
static bool          s_drain;        /**< Leaving: the aborted operation still runs. */
static bool          s_quiet;        /**< Leaving: the low time runs from s_quietUs. */
/** @brief The driver's clock when the leave's low time began, us: millis()
 *         rounds down and could shorten the 100 ms by up to 1 ms. */
static uint32_t      s_quietUs;
static KstStatus     s_published;    /**< The status last copied to s_status. */
static bool          s_switched;     /**< The bench switches the servo's supply, from the bind. */
static bool          s_supplyUsed;   /**< The supply was on in this claim: a close waits. */
static uint32_t      s_offMs;        /**< Clock of the last switch-off command. */
static KstReboot     s_reboot;       /**< Where a Reboot is. */
/** @brief The heartbeat lapsed while the supply was on: once what runs has
 *         ended, the supply goes off, whatever the heartbeat does meanwhile. */
static bool          s_lapsed;
static bool          s_stallLocked;  /**< The session was in doubt when the last stall power-cycled it. */
static bool          s_stallWrote;   /**< It had written a register then. */
static uint32_t      s_rebootMs;     /**< Clock when the Reboot's phase began. */
/** @} */

// ---------------------------------------------------------------------------
// the driver, counting frames
// ---------------------------------------------------------------------------

static bool lineStart(void *, const kst_frame_t *frame, uint32_t windowNs) {
	bool ok = s_line.start(s_line.ctx, frame, windowNs);
	if (ok) s_frames++;
	return ok;
}

static bool linePoll(void *, kst_capture_t *out) { return s_line.poll(s_line.ctx, out); }

static uint32_t lineNow(void *) { return s_line.now_us(s_line.ctx); }

static const kst_driver_t COUNTING_DRIVER = { nullptr, lineStart, linePoll, lineNow };

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
	}
}

/** @brief The level the search wants. */
static void syncSeek(uint32_t nowMs) {
	if (s_switched) supplySet(s_seek.supply, nowMs);
}

/** @brief A lapse with the supply on: off, the session power-cycled, the stalled screen. */
static void stall(uint32_t nowMs) {
	// The power cycle drops the lock and the written registers; the stalled
	// screen still has to say whether the servo is in doubt.
	s_stallLocked = s_ses.locked != 0;
	s_stallWrote = s_ses.wrote_regs != 0;
	s_lapsed = false;
	supplySet(false, nowMs);
	kstSeekStalled(&s_seek, &s_ses, nowMs);
	s_reboot = KstReboot::None;
	s_stalled = true;
	s_state = KstJobState::SearchEnded;
}

/** @brief Time left of the supply wait under way, ms; 0 for none. */
static uint32_t supplyLeftMs(uint32_t nowMs) {
	uint32_t total = 0, gone = 0;
	switch (s_state) {
		case KstJobState::Searching:
			return kstSeekOffLeftMs(&s_seek, nowMs);
		case KstJobState::Rebooting:
			if (s_reboot == KstReboot::Off)     total = KST_SUPPLY_OFF_MS;
			else if (s_reboot == KstReboot::On) total = KST_SUPPLY_WAIT_MS;
			gone = nowMs - s_rebootMs;
			break;
		case KstJobState::Leaving:
			if (!s_supplyUsed || servoSupplyIsOn()) return 0;
			total = KST_SUPPLY_OFF_MS;
			gone = nowMs - s_offMs;
			break;
		default:
			return 0;
	}
	return gone >= total ? 0 : total - gone;
}

// ---------------------------------------------------------------------------
// core1
// ---------------------------------------------------------------------------

static void publish(uint32_t nowMs) {
	KstStatus st;
	memset(&st, 0, sizeof(st));
	st.ackSeq        = s_ackSeq;
	st.job           = s_state;
	st.refused       = s_refused;
	st.lineError     = s_lineError;
	st.seek          = s_seek.state;
	st.attempts      = s_seek.attempts;
	st.searchMs      = kstSeekMs(&s_seek, nowMs);
	st.seekRx        = s_seek.state == KstSeek::Idle ? (uint8_t)KST_RX_NO_REPLY : s_seek.rx;
	st.result        = s_result;
	st.lastRx        = s_ses.last_reply.result;
	st.inMode        = s_ses.in_mode != 0;
	st.hasImage      = s_ses.has_image != 0;
	st.hasBackup     = s_ses.has_backup != 0;
	st.fingerprintOk = s_ses.fingerprint_ok != 0;
	st.locked        = s_ses.locked != 0;
	st.planning      = s_ses.planning != 0;
	st.planResult    = s_ses.plan_result;
	st.stepIndex     = s_ses.step_index;
	st.badRegs       = s_ses.bad_regs;
	st.diffRegs      = s_ses.diff_regs;
	st.wroteRegs     = s_ses.wrote_regs;
	st.frames        = s_frames;
	st.stalled       = s_stalled;
	st.supplyFitted  = s_switched;
	st.supplyOn      = servoSupplyIsOn();
	st.supplyUsed    = s_supplyUsed;
	st.supplyLeftDs  = (uint16_t)((supplyLeftMs(nowMs) + 99u) / 100u);
	st.reboot        = s_reboot;
	st.stallLocked   = s_stallLocked;
	st.stallWrote    = s_stallWrote;
	st.image         = s_ses.image;
	st.backup        = s_ses.backup;
	st.stats         = s_ses.stats;
	// Both copies are built from a cleared struct, so the padding compares too.
	if (memcmp(&st, &s_published, sizeof(st)) == 0) return;
	s_published = st;
	critical_section_enter_blocking(&s_cs);
	s_status = st;
	critical_section_exit(&s_cs);
}

/** @brief Start an operation in the mode; a refusal by the session is its result. */
static void startOp(const KstCmd *cmd) {
	kst_ses_t r = KST_SES_ERR_ARG;
	s_frames = 0;
	switch (cmd->op) {
		case KstOp::ReadAll: r = kst_session_read_all(&s_ses); break;
		case KstOp::Verify:  r = kst_session_verify(&s_ses, &cmd->target); break;
		case KstOp::Edit:    r = kst_session_edit(&s_ses, &cmd->target, cmd->unlocked); break;
		case KstOp::Restore: r = kst_session_restore(&s_ses, cmd->confirmUnchecked); break;
		default: break;
	}
	if (r == KST_SES_BUSY) s_state = KstJobState::Busy;
	else                   s_result = (uint8_t)r;
}

/** @brief Leave from wherever the job is: a running operation is aborted first. */
static void beginLeave() {
	bool rebootOp = s_state == KstJobState::Rebooting &&
	                (s_reboot == KstReboot::Enter || s_reboot == KstReboot::Read);
	s_drain = s_state == KstJobState::Searching || s_state == KstJobState::Busy || rebootOp;
	if (s_state == KstJobState::Searching) kstSeekStop(&s_seek, &s_ses);
	else if (s_state == KstJobState::Busy || rebootOp) kst_session_abort(&s_ses);
	s_reboot = KstReboot::None;
	s_quiet = false;
	s_state = KstJobState::Leaving;
}

/** @brief One pass of a leave. @return True once the line is closed. */
static bool leaveStep(uint32_t nowMs) {
	if (s_state == KstJobState::Closed) return true;
	if (s_drain) {
		// The frame on the wire finishes; kst_session_abort() never cuts one.
		if (s_seek.state == KstSeek::Searching) {
			KstSeek k = kstSeekStep(&s_seek, &s_ses, true, nowMs);
			syncSeek(nowMs);
			if (k == KstSeek::Searching) return false;
			s_result = s_seek.result;
		} else {
			kst_ses_t r = kst_session_step(&s_ses);
			if (r == KST_SES_BUSY) return false;
			s_result = (uint8_t)r;
		}
		s_drain = false;
	}
	// The supply goes off once the frame on the wire has finished.
	supplySet(false, nowMs);
	if (!s_quiet) {
		s_quiet = true;
		s_quietUs = lineNow(nullptr);
	}
	if (lineNow(nullptr) - s_quietUs < KST_LEAVE_LOW_MS * 1000u) return false;
	// The close rule: a servo that was powered in this claim is off for
	// KST_SUPPLY_OFF_MS before DShot can reach it. Time off before the leave
	// counts.
	// "<=": the clock counts whole ms, so 2000 ticks can be 1999.x ms.
	if (s_supplyUsed && nowMs - s_offMs <= KST_SUPPLY_OFF_MS) return false;
	kstLineClose();
	s_state = KstJobState::Closed;
	return true;
}

static void take(const KstCmd *cmd, uint32_t nowMs, bool uiAlive) {
	s_ackSeq = cmd->seq;
	s_refused = KstRefusal::None;
	bool leaving = s_state == KstJobState::Leaving || s_state == KstJobState::Closed;
	bool starts = cmd->op == KstOp::Search || cmd->op == KstOp::ReadAll ||
	              cmd->op == KstOp::Verify || cmd->op == KstOp::Edit ||
	              cmd->op == KstOp::Restore || cmd->op == KstOp::Reboot;
	bool ready = false;
	switch (cmd->op) {
		case KstOp::Search:
			ready = s_state == KstJobState::Opening || s_state == KstJobState::SearchEnded;
			break;
		case KstOp::ReadAll:
		case KstOp::Verify:
		case KstOp::Edit:
		case KstOp::Restore:
			ready = s_state == KstJobState::InMode;
			break;
		case KstOp::StopSearch:
			ready = s_state == KstJobState::Searching;
			break;
		case KstOp::Abort:
			ready = s_state == KstJobState::Searching || s_state == KstJobState::Busy;
			break;
		case KstOp::Leave:
			ready = !leaving;
			break;
		case KstOp::Reboot:
			ready = s_switched && s_state == KstJobState::InMode;
			break;
	}
	if (!ready) {
		s_refused = KstRefusal::State;
		return;
	}
	if (starts && (!uiAlive || s_lapsed)) {
		s_refused = KstRefusal::NoUi;
		return;
	}
	switch (cmd->op) {
		case KstOp::Search:
			s_frames = 0;
			s_stalled = false;
			kstSeekBegin(&s_seek, &s_ses, nowMs, s_switched);
			syncSeek(nowMs);
			s_state = KstJobState::Searching;
			break;
		case KstOp::Reboot:
			// The servo leaves the mode with its supply: the backup, the
			// written registers and the lock drop, and the read after the
			// reboot is the new backup.
			s_frames = 0;
			s_stalled = false;
			kst_session_power_cycled(&s_ses);
			supplySet(false, nowMs);
			s_offMs = nowMs;
			s_reboot = KstReboot::Off;
			s_rebootMs = nowMs;
			s_state = KstJobState::Rebooting;
			break;
		case KstOp::StopSearch:
			kstSeekStop(&s_seek, &s_ses);
			break;
		case KstOp::Abort:
			if (s_state == KstJobState::Searching) kstSeekStop(&s_seek, &s_ses);
			else                                   kst_session_abort(&s_ses);
			break;
		case KstOp::Leave:
			beginLeave();
			break;
		default:
			s_stalled = false;
			startOp(cmd);
			break;
	}
}

/** @brief A switched search from a Reboot whose entry got no answer, or the stall. */
static void rebootSearch(uint32_t nowMs, bool uiAlive) {
	s_reboot = KstReboot::None;
	if (!uiAlive) {
		stall(nowMs);
		return;
	}
	kstSeekBegin(&s_seek, &s_ses, nowMs, true);
	syncSeek(nowMs);
	s_state = KstJobState::Searching;
}

/** @brief One pass of a Reboot. */
static void rebootStep(uint32_t nowMs, bool uiAlive) {
	switch (s_reboot) {
		case KstReboot::Off:
			if (!uiAlive) { stall(nowMs); return; }
			if (nowMs - s_rebootMs <= KST_SUPPLY_OFF_MS) return;
			supplySet(true, nowMs);
			s_reboot = KstReboot::On;
			s_rebootMs = nowMs;
			return;
		case KstReboot::On: {
			if (!uiAlive) { stall(nowMs); return; }
			if (nowMs - s_rebootMs < KST_SUPPLY_WAIT_MS) return;
			kst_ses_t r = kst_session_enter(&s_ses);
			if (r == KST_SES_BUSY) { s_reboot = KstReboot::Enter; return; }
			s_result = (uint8_t)r;
			rebootSearch(nowMs, uiAlive);
			return;
		}
		case KstReboot::Enter: {
			kst_ses_t r = kst_session_step(&s_ses);
			if (r == KST_SES_BUSY) return;
			s_result = (uint8_t)r;
			// The entry finished; a lapse during it switches off now.
			if (!uiAlive || s_lapsed) { stall(nowMs); return; }
			if (r != KST_SES_OK) { rebootSearch(nowMs, uiAlive); return; }
			r = kst_session_read_all(&s_ses);
			if (r == KST_SES_BUSY) { s_reboot = KstReboot::Read; return; }
			s_result = (uint8_t)r;
			s_reboot = KstReboot::None;
			s_state = KstJobState::InMode;
			return;
		}
		case KstReboot::Read: {
			kst_ses_t r = kst_session_step(&s_ses);
			if (r == KST_SES_BUSY) return;
			s_result = (uint8_t)r;
			s_reboot = KstReboot::None;
			s_state = KstJobState::InMode;
			if (kstSeekLost(&s_seek, &s_ses, r, uiAlive, nowMs, true)) {
				syncSeek(nowMs);
				s_stalled = !uiAlive;
				s_state = s_seek.state == KstSeek::Searching ? KstJobState::Searching
				                                             : KstJobState::SearchEnded;
			} else if (!uiAlive || s_lapsed) {
				stall(nowMs);
			}
			return;
		}
		default:
			return;
	}
}

static void advance(uint32_t nowMs, bool uiAlive) {
	switch (s_state) {
		case KstJobState::Searching: {
			KstSeek k = kstSeekStep(&s_seek, &s_ses, uiAlive, nowMs);
			syncSeek(nowMs);
			if (k == KstSeek::Searching) break;
			s_result = s_seek.result;
			s_stalled = k == KstSeek::NoUi;
			s_state = k == KstSeek::Found ? KstJobState::InMode : KstJobState::SearchEnded;
			break;
		}
		case KstJobState::Busy: {
			kst_ses_t r = kst_session_step(&s_ses);
			if (r == KST_SES_BUSY) break;
			s_result = (uint8_t)r;
			s_state = KstJobState::InMode;
			// The loss rule: a servo in the mode that stops answering was
			// without power. The new search counts frames from 0.
			if (kstSeekLost(&s_seek, &s_ses, r, uiAlive, nowMs, s_switched)) {
				syncSeek(nowMs);
				s_frames = 0;
				s_stalled = !uiAlive;
				s_state = s_seek.state == KstSeek::Searching ? KstJobState::Searching
				                                             : KstJobState::SearchEnded;
			} else if ((!uiAlive || s_lapsed) && servoSupplyIsOn()) {
				// The operation has finished; a lapse during it switches off
				// now, even if the heartbeat came back meanwhile.
				stall(nowMs);
			}
			break;
		}
		case KstJobState::InMode:
			if ((!uiAlive || s_lapsed) && servoSupplyIsOn()) stall(nowMs);
			break;
		case KstJobState::Rebooting:
			rebootStep(nowMs, uiAlive);
			break;
		case KstJobState::Leaving:
			leaveStep(nowMs);
			break;
		default:
			break;
	}
}

static void kstJobInit() {
	critical_section_init(&s_cs);
}

/** @brief The `kst_line_err_t` of a refused open as the bind failure a screen names. */
static uint8_t bindError(uint8_t lineErr) {
	switch (lineErr) {
		case KST_LINE_ERR_CLOCK: return CORE1_JOB_CLOCK;
		case KST_LINE_ERR_DMA:   return CORE1_JOB_DMA;
		// KST_LINE_ERR_ARG is a pin the chip does not have, which no PIO
		// block reaches; the claim never hands one over.
		default:                 return CORE1_JOB_PIO;
	}
}

static uint8_t kstJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	(void)auxGpio;
	critical_section_enter_blocking(&s_cs);
	uint16_t seq = s_cmd.seq;
	critical_section_exit(&s_cs);

	memset(&s_seek, 0, sizeof(s_seek));
	memset(&s_published, 0xFF, sizeof(s_published));
	s_state = KstJobState::Opening;
	s_refused = KstRefusal::None;
	s_ackSeq = seq;
	s_result = KST_SES_OK;
	s_frames = 0;
	s_stalled = false;
	s_drain = false;
	s_quiet = false;
	s_reboot = KstReboot::None;
	// The supply comes up off: GP0 driven low until the release.
	s_switched = servoSupplyFitted();
	s_supplyUsed = false;
	s_offMs = millis();
	s_lapsed = false;
	s_stallLocked = false;
	s_stallWrote = false;
	s_lineError = kstLineOpen(sigGpio);
	if (s_lineError == KST_LINE_OK) {
		// Only a bound job is released: a failed bind leaves GP0 undriven.
		servoSupplyBind();
		s_line = kstLineDriver();
		// The port's driver has all three functions, so the init succeeds.
		kst_session_init(&s_ses, &COUNTING_DRIVER);
	} else {
		memset(&s_ses, 0, sizeof(s_ses));
		s_state = KstJobState::Closed;
	}
	publish(millis());
	return s_lineError == KST_LINE_OK ? (uint8_t)CORE1_JOB_OK : bindError(s_lineError);
}

static void kstJobStep(uint32_t nowMs, bool uiAlive) {
	critical_section_enter_blocking(&s_cs);
	KstCmd cmd = s_cmd;
	critical_section_exit(&s_cs);

	// A lapse with the supply on is latched until what runs has ended.
	if (!uiAlive && servoSupplyIsOn()) s_lapsed = true;
	if (cmd.seq != s_ackSeq) take(&cmd, nowMs, uiAlive);
	advance(nowMs, uiAlive);
	publish(nowMs);
}

static bool kstJobRelease(uint32_t nowMs) {
	if (s_state != KstJobState::Leaving && s_state != KstJobState::Closed) beginLeave();
	bool done = leaveStep(nowMs);
	// GP0 an input again before the pins return to DShot.
	if (done) servoSupplyRelease();
	publish(nowMs);
	return done;
}

const Core1JobOps KST_JOB_OPS = {
	kstJobBind, kstJobStep, kstJobRelease, kstJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

/** @brief A command that a later one must not replace before core1 takes it. */
static bool isStop(KstOp op) {
	return op == KstOp::StopSearch || op == KstOp::Abort || op == KstOp::Leave;
}

uint16_t kstJobCommand(const KstCmd *c) {
	critical_section_enter_blocking(&s_cs);
	bool pending = s_status.ackSeq != s_cmd.seq;
	bool keep = pending && isStop(s_cmd.op) &&
	            !(c->op == KstOp::Leave && s_cmd.op != KstOp::Leave);
	if (!keep) {
		uint16_t seq = (uint16_t)(s_cmd.seq + 1);
		s_cmd = *c;
		s_cmd.seq = seq;
	}
	uint16_t held = s_cmd.seq;
	critical_section_exit(&s_cs);
	return held;
}

void kstJobStatus(KstStatus *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_status;
	critical_section_exit(&s_cs);
}
