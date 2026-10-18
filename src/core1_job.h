/**
 * @file core1_job.h
 * @brief Work core1 runs on the claimed pins while the DShot pump is down.
 *
 * Core1 makes every PIO change on the two signal pins, because the core that
 * claims a PIO state machine is the core that frees it; the AM32 transport,
 * on core0, is the exception. While the pin claim holds (pin_claim.h),
 * escTaskPoll() returns at once and core1 runs one job instead: the servo
 * tester or the KST programmer. `loop1()` calls escTaskPoll() and then
 * core1JobPoll(); with no job, core1JobPoll() returns at once too.
 *
 * Core1Job::Servo and Core1Job::Kst name the two jobs of
 * docs/design/servo-tester.md and docs/design/kst-programming.md: servo_job.h
 * and kst_job.h.
 *
 * Core0 starts and stops a job; core1 binds, steps and releases it. The
 * request and the job's phase are shared under one critical section, in the
 * pattern of escSnapshot().
 *
 * @see docs/design/pin-claim.md
 */

#pragma once

#include <stdint.h>

/** @brief Which job core1 runs. */
enum class Core1Job : uint8_t {
	None,   /**< No job; core1JobPoll() returns at once. */
	Servo,  /**< Servo PWM on SIG, and AUX in two-servo mode (servo_job.h). */
	Kst,    /**< KST and Chaservo programming on SIG (kst_job.h). */
};

/** @brief Where a job is between core1JobStart() and idle. */
enum class Core1JobPhase : uint8_t {
	Idle,      /**< No job. core1JobError() says how the last one ended. */
	Starting,  /**< Asked for; core1 has not bound it yet. */
	Running,   /**< Bound; core1 steps it on every pass of `loop1()`. */
	Stopping,  /**< Asked to stop; core1 has not released it yet. */
};

/**
 * @brief Why a job did not bind. 0 is none.
 *
 * The bind failures name the resource, as the screens show them: `GP29 COULD
 * NOT BE TAKEN (PWM)`, `(PIO)`, `(DMA)` or `(CLOCK)`.
 */
enum Core1JobError : uint8_t {
	CORE1_JOB_OK      = 0,  /**< Bound, or nothing started. */
	CORE1_JOB_REFUSED = 1,  /**< core1JobStart() refused it; pinClaimJobError() only. */
	CORE1_JOB_PWM     = 2,  /**< No PWM slice or channel for the pin. */
	CORE1_JOB_PIO     = 3,  /**< No PIO block with room for the program. */
	CORE1_JOB_DMA     = 4,  /**< No free DMA channel. */
	CORE1_JOB_CLOCK   = 5,  /**< `clk_sys` is outside what the job accepts. */
};

/**
 * @brief What one job does on core1. All three run on core1.
 *
 * A job's slice defines one of these. core1_job.cpp calls them; nothing else
 * does.
 */
struct Core1JobOps {
	/**
	 * Take the pins: the SIG GPIO, and the AUX GPIO or 0xFF when the claim
	 * did not ask for AUX. Returns CORE1_JOB_OK, or the Core1JobError that
	 * stopped it, and binds nothing when it fails.
	 */
	uint8_t (*bind)(uint8_t sigGpio, uint8_t auxGpio);
	/**
	 * One pass of `loop1()`, given millis() and core1UiAlive(), the job's
	 * heartbeat rule.
	 */
	void (*step)(uint32_t nowMs, bool uiAlive);
	/**
	 * Let the pins go, given millis(). Called on every pass while the job is
	 * Stopping, until it returns true; it never blocks. A job that needs time
	 * before it lets go, as the servo job does for the last pulse, returns
	 * false until then. The job stays Stopping, and so the claim stays
	 * Leaving, until it returns true.
	 */
	bool (*release)(uint32_t nowMs);
	/**
	 * Initialise the job's shared state. Core0, from core1JobInit(), before
	 * core1 is launched; null for a job with nothing to initialise.
	 */
	void (*init)();
};

/**
 * @brief Initialise the shared state, and each built job's through its
 *        Core1JobOps::init. Call from **core0**, before launching core1.
 *
 * For the same reason as escTaskInit(): core0 can reach the critical section
 * before core1 has booted.
 */
void core1JobInit();

/**
 * @brief Ask core1 to start a job on the given pins. Core0.
 *
 * Refused unless escTaskSuspended() is true, so the DShot pump and a job never
 * drive a pin together. Refused too for Core1Job::None, for a job that is not
 * built, and while a job is not idle.
 *
 * @param job     The job.
 * @param sigGpio GPIO of the SIG role, from pinClaimGpio().
 * @param auxGpio GPIO of the AUX role, 0xFF when the claim did not ask for it.
 * @return True when core1 will bind it on its next pass.
 */
bool core1JobStart(Core1Job job, uint8_t sigGpio, uint8_t auxGpio);

/** @brief Ask core1 to release the job. Core0. A no-op while idle. */
void core1JobStop();

/**
 * @brief Which job core1 holds. Core0.
 * @return The job from core1JobStart() until it is idle; None once idle.
 */
Core1Job core1JobRunning();

/** @brief The job's phase. Core0. @return Idle, Starting, Running or Stopping. */
Core1JobPhase core1JobPhase();

/**
 * @brief Why the last job did not bind. Core0.
 * @return CORE1_JOB_PWM, CORE1_JOB_PIO, CORE1_JOB_DMA or CORE1_JOB_CLOCK from
 *         the job's bind; CORE1_JOB_OK when it bound or none was started.
 *         Never CORE1_JOB_REFUSED: a refused start is reported by
 *         pinClaimJobError().
 */
uint8_t core1JobError();

/**
 * @brief Bind, step or release the job. Core1, from `loop1()` after escTaskPoll().
 *
 * Returns at once while no job is asked for. Never blocks.
 */
void core1JobPoll();

/**
 * @brief Whether core0 is alive, by the DShot pump's rule. Core1.
 *
 * escUiAlive() against escHeartbeatMs(): an escHeartbeat() within
 * @ref UI_HEARTBEAT_TIMEOUT_MS, 250 ms. One rule decides "the UI is alive" for
 * the pump and for every job.
 *
 * @return True while the heartbeat is fresh.
 */
bool core1UiAlive();
