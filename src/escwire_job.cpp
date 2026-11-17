/**
 * @file escwire_job.cpp
 * @brief The ESC settings job on core1. @see escwire_job.h
 */

#include "cs_shared.h"
#include "escwire_job.h"
#include "escwire_port.h"
#include "ew_line.h"

#include "plat.h"
#include <hardware/gpio.h>
#include <pico/critical_section.h>
#include <string.h>

/** @brief Guards the shared block below. */
static critical_section_t s_cs;

/**
 * @defgroup escwire_job_shared Shared block (under s_cs)
 * @brief Core0 writes the command; core1 writes the status.
 * @{
 */
static uint16_t    s_cmdSeq = 0;           /**< The held command's seq. */
static EwJobOp     s_cmdOp = EwJobOp::Stop; /**< The held command. */
static uint8_t     s_cmdDraft[EW_PAGE_MAX]; /**< The draft a Write holds. */
static uint16_t    s_cmdLen = 0;           /**< Its length. */
static EwJobStatus s_status;               /**< What core1 last published. */
/** @} */

/**
 * @defgroup escwire_job_core1 Core1 only
 * @{
 */
static ew_session_t s_ses;           /**< The session. */
static EwJobState   s_state;         /**< Where the job is. */
static EwJobRefusal s_refused;       /**< Of the last command taken. */
static uint16_t     s_ackSeq;        /**< Seq of the last command taken. */
static uint8_t      s_lineError;     /**< `ew_line_err_t` of the bind. */
static EwJobOp      s_lastOp;        /**< The last operation started. */
static uint8_t      s_result;        /**< `ew_ses_t` of the last operation that ended. */
static bool         s_stalled;       /**< A lapse stopped an operation. */
static bool         s_left;          /**< The leave ran to its end. */
static uint8_t      s_draft[EW_PAGE_MAX]; /**< The draft of the Write taken: the session judges and copies it. */
static uint16_t     s_draftLen;      /**< Its length. */
/**
 * @brief Something the status shows changed since the last publish: a
 *        command taken, an operation's end, a stop by the lapse. The page
 *        changes only with an operation's end, so the spin lock is not held
 *        for its copy on every pass.
 */
static bool         s_changed;
/** @brief The session's `writing` and `dirty` as last published: a change publishes. */
static bool         s_pubWriting, s_pubDirty;
/** @} */

/** @brief Copy the job's state to the shared status. */
static void publish() {
	s_changed = false;
	critical_section_enter_blocking(&s_cs);
	s_status.ackSeq = s_ackSeq;
	s_status.job = s_state;
	s_status.refused = s_refused;
	s_status.lineError = s_lineError;
	s_status.lastOp = s_lastOp;
	s_status.result = s_state == EwJobState::Busy ? (uint8_t)EW_SES_BUSY : s_result;
	s_status.inMode = s_ses.in_mode;
	s_status.haveRead = s_ses.have_read;
	s_status.stalled = s_stalled;
	s_status.left = s_left;
	s_status.reread = s_ses.reread;
	s_status.writing = s_pubWriting = s_ses.writing;
	s_status.dirty = s_pubDirty = s_ses.dirty;
	s_status.refuse = s_ses.refuse;
	s_status.badOff = s_ses.bad_off;
	s_status.contact = s_ses.contact;
	s_status.id = s_ses.id;
	memcpy(s_status.page, s_ses.orig, sizeof(s_status.page));
	critical_section_exit(&s_cs);
}

/** @brief The running operation ended with @p r. */
static void ended(ew_ses_t r) {
	s_changed = true;
	s_result = (uint8_t)r;
	if (s_lastOp == EwJobOp::Leave && r == EW_SES_OK) s_left = true;
	s_state = s_ses.in_mode ? EwJobState::InMode : EwJobState::Ended;
}

/**
 * @brief What the session said to @p op: started, refused before anything
 *        went out, or run and ended at once, when the driver refused its
 *        first transaction or the guard its frame.
 */
static void started(EwJobOp op, ew_ses_t r) {
	if (r == EW_SES_ERR_STATE || r == EW_SES_ERR_ARG || r == EW_SES_ERR_DIRTY ||
	    r == EW_SES_ERR_ERASED) {
		// The session as it was.
		s_refused = EwJobRefusal::State;
		return;
	}
	s_lastOp = op;
	if (r == EW_SES_BUSY) s_state = EwJobState::Busy;
	else ended(r);
}

/** @brief Take a command from core0. */
static void take(EwJobOp op, bool uiAlive) {
	s_changed = true;
	s_refused = EwJobRefusal::None;
	if (op == EwJobOp::Stop) {
		// Outside a write a stop cuts at once and parks the line; the next
		// step reports the cut operation's end.
		ew_session_stop(&s_ses);
		if (s_state != EwJobState::Busy) s_state = EwJobState::Ended;
		return;
	}
	if (!uiAlive) {
		s_refused = EwJobRefusal::NoUi;
		return;
	}
	switch (op) {
		case EwJobOp::Enter:
			if (s_state != EwJobState::Opening) break;
			started(op, ew_session_enter(&s_ses, EW_HOLD_MIN_MS));
			return;
		case EwJobOp::Read:
			if (s_state != EwJobState::InMode) break;
			started(op, ew_session_read(&s_ses));
			return;
		case EwJobOp::Leave:
			if (s_state != EwJobState::InMode) break;
			started(op, ew_session_leave(&s_ses));
			return;
		case EwJobOp::Write:
			// The guard judges the draft: only editable fields of a written
			// layout change, each to one of its values.
			if (s_state != EwJobState::InMode) break;
			started(op, ew_session_write(&s_ses, s_draft, s_draftLen));
			return;
		case EwJobOp::Stop:
			break;
	}
	s_refused = EwJobRefusal::State;
}

/** @brief One step of the running operation. */
static void run() {
	ew_ses_t r = ew_session_step(&s_ses);
	if (r != EW_SES_BUSY) ended(r);
}

static void escwireJobInit() {
	csInitShared(&s_cs);
}

uint8_t escwireJobBindError(uint8_t lineErr) {
	switch (lineErr) {
		case EW_LINE_ERR_CLOCK: return CORE1_JOB_CLOCK;
		case EW_LINE_ERR_DMA:   return CORE1_JOB_DMA;
		case EW_LINE_ERR_ARG:   return CORE1_JOB_PIN;
		default:                return CORE1_JOB_PIO;
	}
}

static uint8_t escwireJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	(void)auxGpio;
	critical_section_enter_blocking(&s_cs);
	uint16_t seq = s_cmdSeq;
	critical_section_exit(&s_cs);

	s_ackSeq = seq;
	s_refused = EwJobRefusal::None;
	s_lastOp = EwJobOp::Enter;
	s_result = (uint8_t)EW_SES_OK;
	s_stalled = false;
	s_left = false;
	ew_driver_t d;
	s_lineError = escwirePortOpen(sigGpio, &d);
	if (s_lineError != 0) {
		// The AM32 transport left the pin floating and the refused open did
		// not touch it: the pull-up holds the line high, as between
		// greetings, while an EFM8 bootloader waits on it. A pin the chip
		// does not have is left alone.
		if (s_lineError != EW_LINE_ERR_ARG) gpio_pull_up(sigGpio);
		s_state = EwJobState::Closed;
		memset(&s_ses, 0, sizeof(s_ses));
		publish();
		return escwireJobBindError(s_lineError);
	}
	(void)ew_session_init(&s_ses, &d);
	// The open parked the pad on its pull-down: high again at once.
	(void)ew_session_hold(&s_ses);
	s_state = EwJobState::Opening;
	publish();
	return CORE1_JOB_OK;
}

static void escwireJobStep(uint32_t nowMs, bool uiAlive) {
	(void)nowMs;
	critical_section_enter_blocking(&s_cs);
	uint16_t seq = s_cmdSeq;
	EwJobOp op = s_cmdOp;
	if (seq != s_ackSeq && op == EwJobOp::Write) {
		memcpy(s_draft, s_cmdDraft, sizeof(s_draft));
		s_draftLen = s_cmdLen;
	}
	critical_section_exit(&s_cs);

	if (seq != s_ackSeq) {
		s_ackSeq = seq;
		take(op, uiAlive);
	}
	if (s_state == EwJobState::Busy) {
		// A lapse stops an entry, a read or a write: a write that has sent
		// its erase or program runs to its read-back, then parks. A leave's
		// frame has gone, and the rest is the line low, so it finishes.
		if (!uiAlive && s_lastOp != EwJobOp::Leave) {
			ew_session_stop(&s_ses);
			s_stalled = true;
			s_changed = true;
		}
		run();
	}
	// The erase or program went: core0 sees the page in doubt at once.
	if (s_ses.writing != s_pubWriting || s_ses.dirty != s_pubDirty) s_changed = true;
	if (s_changed) publish();
}

static bool escwireJobRelease(uint32_t nowMs) {
	(void)nowMs;
	if (s_state == EwJobState::Busy) {
		// An entry, a read or a write before its erase is cut; a write that
		// has sent its erase or program, and a leave, run to their end first.
		if (s_lastOp != EwJobOp::Leave) ew_session_stop(&s_ses);
		run();
		if (s_state == EwJobState::Busy) {
			publish();
			return false;
		}
	}
	escwirePortClose();
	s_state = EwJobState::Closed;
	publish();
	return true;
}

const Core1JobOps ESCWIRE_JOB_OPS = {
	escwireJobBind, escwireJobStep, escwireJobRelease, escwireJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

/** @brief Hold @p op, with @p n bytes of @p draft for a Write. */
static uint16_t post(EwJobOp op, const uint8_t *draft, uint16_t n) {
	critical_section_enter_blocking(&s_cs);
	bool pending = s_status.ackSeq != s_cmdSeq;
	if (!(pending && s_cmdOp == EwJobOp::Stop)) {
		s_cmdSeq = (uint16_t)(s_cmdSeq + 1);
		s_cmdOp = op;
		if (draft) {
			if (n > sizeof(s_cmdDraft)) n = sizeof(s_cmdDraft);
			memcpy(s_cmdDraft, draft, n);
			s_cmdLen = n;
		}
	}
	uint16_t held = s_cmdSeq;
	critical_section_exit(&s_cs);
	return held;
}

uint16_t escwireJobCommand(EwJobOp op) { return post(op, nullptr, 0); }

uint16_t escwireJobWrite(const uint8_t *draft, uint16_t n) { return post(EwJobOp::Write, draft, n); }

void escwireJobStatus(EwJobStatus *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_status;
	critical_section_exit(&s_cs);
}
