/**
 * @file pin_claim.cpp
 * @brief The pin claim's state machine on core0. @see pin_claim.h
 */

#include "pin_claim.h"
#include "core1_job.h"
#include "esc_task.h"
#include "ui.h"

#include "plat.h"

/**
 * @defgroup pin_claim_state Claim state (core0 only)
 * @{
 */
static ClaimState s_state = ClaimState::Free;   /**< Where the claim is. */
static PinUser    s_who   = PinUser::None;      /**< Who holds it. */
static uint8_t    s_roles = 0;                  /**< Roles asked for. */
static uint8_t    s_sig   = PIN_CLAIM_NO_GPIO;  /**< SIG GPIO, latched at Held. */
static uint8_t    s_aux   = PIN_CLAIM_NO_GPIO;  /**< AUX GPIO, latched at Held. */
static uint8_t    s_jobError = CORE1_JOB_OK;    /**< Why the job did not bind. */
/** @brief A start went to core1 and has not been answered. */
static bool       s_jobPending = false;
/** @} */

/** @brief The core1 job a screen runs; None for AM32. */
static Core1Job jobFor(PinUser who) {
	switch (who) {
		case PinUser::Servo: return Core1Job::Servo;
		case PinUser::Kst:   return Core1Job::Kst;
		default:             return Core1Job::None;
	}
}

/** @brief SERVO and KST show the unplug page before DShot restarts. */
static bool needsUnplug(PinUser who) {
	return who == PinUser::Servo || who == PinUser::Kst;
}

/** @brief Hand the job its pins. AM32 has no job and starts nothing. */
static void startJob() {
	s_jobError = CORE1_JOB_OK;
	s_jobPending = false;
	Core1Job job = jobFor(s_who);
	if (job == Core1Job::None) return;
	uint8_t aux = (s_roles & PIN_AUX) ? s_aux : PIN_CLAIM_NO_GPIO;
	if (core1JobStart(job, s_sig, aux)) s_jobPending = true;
	else                                s_jobError = CORE1_JOB_REFUSED;
}

/** @brief DShot back: the pump rebuilds from the wiring esc_task.cpp latched. */
static void restore() {
	escTaskResume();
	s_state = ClaimState::Restoring;
}

/**
 * @brief Leaving: restore, unless the unplug page is due.
 *
 * SERVO and KST wait for RESTART DSHOT, which pinClaimUnplugDue() offers only
 * once their job is idle. AM32 has neither a job nor the page, so its claim
 * restores within the call that ended it.
 */
static void leaveStep() {
	if (needsUnplug(s_who)) return;
	restore();
}

/** @brief Restoring ends when the pump runs again. Nothing is left to do. */
static void settle() {
	if (s_state != ClaimState::Restoring || escTaskSuspended()) return;
	s_state = ClaimState::Free;
	s_who   = PinUser::None;
	s_roles = 0;
	s_sig = s_aux = PIN_CLAIM_NO_GPIO;
	s_jobPending = false;
}

ClaimRefusal pinClaimBegin(PinUser who, uint8_t roles) {
	settle();
	EscTelemetry tel;
	escSnapshot(&tel);
	ClaimFacts f;
	f.armed      = uiArmed();
	f.held       = s_state != ClaimState::Free;
	f.nowMs      = millis();
	f.lastRpmMs  = tel.lastRpmMs;
	f.kissLastMs = tel.kissLastMs;
	f.sigGpio    = escTaskDshotPin();
	f.auxGpio    = escTaskKissPin();
	ClaimRefusal r = pinClaimCheck(who, roles, &f);
	if (r != ClaimRefusal::None) return r;

	s_who = who;
	s_roles = roles;
	s_sig = s_aux = PIN_CLAIM_NO_GPIO;
	s_jobError = CORE1_JOB_OK;
	s_jobPending = false;
	s_state = ClaimState::Stopping;
	// Disarms again, as the backstop under the Armed rule.
	escTaskSuspend();
	return ClaimRefusal::None;
}

ClaimState pinClaimPoll() {
	settle();
	switch (s_state) {
		case ClaimState::Stopping:
			if (escTaskSuspended()) {
				// Read once, here: SETUP is unreachable during a claim, so
				// these cannot change before it ends.
				s_sig = escTaskDshotPin();
				s_aux = escTaskKissPin();
				s_state = ClaimState::Held;
				startJob();
			}
			break;

		case ClaimState::Held:
			if (!s_jobPending) break;
			if (core1JobPhase() == Core1JobPhase::Running) {
				s_jobPending = false;
				s_state = ClaimState::Running;
			} else if (core1JobPhase() == Core1JobPhase::Idle) {
				// Idle again without running: the bind failed and took nothing.
				s_jobPending = false;
				s_jobError = core1JobError();
			}
			break;

		case ClaimState::Leaving:
			leaveStep();
			break;

		case ClaimState::Free:
		case ClaimState::Running:
		case ClaimState::Restoring:
			break;
	}
	return s_state;
}

ClaimState pinClaimState() {
	settle();
	return s_state;
}

PinUser pinClaimHolder() {
	settle();
	return s_who;
}

uint8_t pinClaimGpio(uint8_t role) {
	if (role == PIN_SIG && (s_roles & PIN_SIG)) return s_sig;
	if (role == PIN_AUX && (s_roles & PIN_AUX)) return s_aux;
	return PIN_CLAIM_NO_GPIO;
}

uint8_t pinClaimJobError() { return s_jobError; }

void pinClaimEnd(PinUser who) {
	settle();
	if (who == PinUser::None || who != s_who) return;
	switch (s_state) {
		case ClaimState::Stopping:
			// No job started and the pins went to nobody: DShot comes back at
			// once, drained or not.
			restore();
			break;
		case ClaimState::Held:
		case ClaimState::Running:
			core1JobStop();
			s_jobPending = false;
			s_state = ClaimState::Leaving;
			leaveStep();
			break;
		case ClaimState::Free:
		case ClaimState::Leaving:
		case ClaimState::Restoring:
			break;
	}
}

bool pinClaimUnplugDue() {
	return s_state == ClaimState::Leaving && needsUnplug(s_who) &&
	       core1JobPhase() == Core1JobPhase::Idle;
}

void pinClaimRestart(PinUser who) {
	if (who != s_who || !pinClaimUnplugDue()) return;
	restore();
}

void pinClaimRetry(PinUser who) {
	// A start that is pending has no error, so this also refuses a RETRY
	// while one is under way.
	if (who != s_who || s_state != ClaimState::Held || s_jobError == CORE1_JOB_OK)
		return;
	startJob();
}

void pinClaimKeep(PinUser who) {
	if (who != s_who || !pinClaimUnplugDue()) return;
	s_state = ClaimState::Held;
	startJob();
}
