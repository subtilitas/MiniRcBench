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
/** @brief Held starts the job: from pinClaimBegin(), or pinClaimStartJob() for a deferring user. */
static bool       s_jobWanted = false;
/**
 * @brief This claim shows the leave page before the pins park: its user's
 *        PinUserInfo::unplug from pinClaimBegin(), or pinClaimWantLeavePage().
 */
static bool       s_unplug = false;
/**
 * @brief Run is wanted: the claim's own record of its last pinClaimDshot()
 *        that acted, not the pump's state. True from pinClaimDshot(true)
 *        to pinClaimDshot(false); a claim is Busy meanwhile.
 */
static bool       s_dshotWanted = false;
/** @} */

/** @brief The core1 job a screen runs. @see PIN_USERS */
static Core1Job jobFor(PinUser who) { return pinUserInfo(who).job; }

/**
 * @brief The claim shows the leave page before the pins park: SERVO, KST,
 *        HITEC and the S.BUS monitor always, AM32 once an EFM8 bootloader
 *        answered, the USB bridge once it asks. @see PIN_USERS,
 *        pinClaimWantLeavePage()
 */
static bool needsUnplug() { return s_unplug; }

/** @brief Hand the job its pins. A user with no job starts nothing. */
static void startJob() {
	s_jobError = CORE1_JOB_OK;
	s_jobPending = false;
	Core1Job job = jobFor(s_who);
	if (job == Core1Job::None) return;
	uint8_t aux = (s_roles & PIN_AUX) ? s_aux : PIN_CLAIM_NO_GPIO;
	if (core1JobStart(job, s_sig, aux)) s_jobPending = true;
	else                                s_jobError = CORE1_JOB_REFUSED;
}

/**
 * @brief The pins back to the pump, parked: core1 parks SIG low and AUX on
 *        its pull-down, and the claim is Free once escTaskParked(). No
 *        DShot starts (docs/design/dshot-lifetime.md, "The pin claim").
 */
static void restore() {
	escTaskPark();
	s_state = ClaimState::Parking;
}

/**
 * @brief Leaving: park once the job is idle, unless the leave page is due.
 *
 * SERVO, KST, HITEC and the S.BUS monitor wait for the page's last button,
 * which
 * pinClaimUnplugDue() offers only once their job is idle; so does an AM32
 * claim after an EFM8 bootloader answered (pinClaimWantLeavePage()), which
 * ESC SETTINGS asks for before it starts its job. Any other AM32 claim has
 * neither a job nor the page, so it parks within the call that ended it.
 * The USB bridge has a job and no page: it parks on the first look at the
 * claim that finds its job idle, settle()'s included, so the park never
 * touches a pin the job's state machines still drive, and follows a screen
 * that ended the claim and closed.
 */
static void leaveStep() {
	if (needsUnplug()) return;
	if (core1JobPhase() != Core1JobPhase::Idle) return;
	restore();
}

/**
 * @brief Leaving moves on without its screen: a claim without the leave page
 *        parks once its job is idle. Parking ends when core1 reports the
 *        pins parked. Run by every call that reads or changes the claim, so a
 *        screen that ended the claim and closed, as ESC SETTINGS does,
 *        leaves no claim behind for MENU's look at it.
 */
static void settle() {
	if (s_state == ClaimState::Leaving) leaveStep();
	if (s_state != ClaimState::Parking || !escTaskParked()) return;
	s_state = ClaimState::Free;
	s_who   = PinUser::None;
	s_roles = 0;
	s_sig = s_aux = PIN_CLAIM_NO_GPIO;
	s_jobWanted = false;
}

ClaimRefusal pinClaimBegin(PinUser who, uint8_t roles) {
	settle();
	EscTelemetry tel;
	escSnapshot(&tel);
	ClaimFacts f;
	f.armed      = uiArmed();
	// Busy while Run is wanted, a backstop: no route opens a claim from ESC
	// TESTER or ESC COMMANDS. After pinClaimDshot(false) the rest of a
	// drain waits in Stopping.
	f.held       = s_state != ClaimState::Free || s_dshotWanted;
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
	s_jobWanted = !pinUserInfo(who).deferJob;
	s_unplug = pinUserInfo(who).unplug;
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
				// Read once, here: SETUP opens only while the claim is
				// Free, so these cannot change before it ends.
				s_sig = escTaskDshotPin();
				s_aux = escTaskKissPin();
				s_state = ClaimState::Held;
				if (s_jobWanted) startJob();
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
		case ClaimState::Parking:
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
	settle();
	// Every claim asks for SIG.
	if (role == PIN_SIG) return s_sig;
	if (role == PIN_AUX && (s_roles & PIN_AUX)) return s_aux;
	return PIN_CLAIM_NO_GPIO;
}

uint8_t pinClaimJobError() { return s_jobError; }

void pinClaimEnd(PinUser who) {
	settle();
	if (who != s_who) return;
	switch (s_state) {
		case ClaimState::Stopping:
			// No job started and the pins went to nobody: they park at once;
			// the rest of a drain, if one is owed, goes out first.
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
		case ClaimState::Parking:
			break;
	}
}

bool pinClaimUnplugDue() {
	return s_state == ClaimState::Leaving && needsUnplug() &&
	       core1JobPhase() == Core1JobPhase::Idle;
}

void pinClaimWantLeavePage(PinUser who) {
	settle();
	if (who != s_who) return;
	if (s_state == ClaimState::Stopping || s_state == ClaimState::Held ||
	    s_state == ClaimState::Running)
		s_unplug = true;
}

bool pinClaimLeavePage() {
	settle();
	return s_state != ClaimState::Free && needsUnplug();
}

void pinClaimRelease(PinUser who) {
	if (who != s_who || !pinClaimUnplugDue()) return;
	restore();
}

void pinClaimDshot(bool on) {
	settle();
	if (s_state != ClaimState::Free) return;
	if (on && !s_dshotWanted) {
		escTaskStart();
		s_dshotWanted = true;
	} else if (!on && s_dshotWanted) {
		escTaskPark();
		s_dshotWanted = false;
	}
}

bool pinClaimDshotRunning() {
	settle();
	return s_state == ClaimState::Free && s_dshotWanted && escTaskRunning();
}

void pinClaimStartJob(PinUser who) {
	settle();
	if (who != s_who || (s_state != ClaimState::Stopping && s_state != ClaimState::Held)) return;
	bool first = !s_jobWanted;
	s_jobWanted = true;
	// Held with no job yet: the tap starts it. Stopping starts it at Held.
	if (first && s_state == ClaimState::Held) startJob();
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
	s_jobError = CORE1_JOB_OK;
	s_jobPending = false;
	// A user that defers its job asks for it again: the next servo may need
	// powering first, as its first did (menu-b.md, decision 34).
	s_jobWanted = !pinUserInfo(s_who).deferJob;
	if (s_jobWanted) startJob();
}
