/**
 * @file core1_job.cpp
 * @brief Core1 job dispatch: start, bind, step, stop. @see core1_job.h
 */

#include "core1_job.h"
#include "esc_task.h"

#include "plat.h"
#include <pico/critical_section.h>

/**
 * @brief The jobs that are built, by Core1Job.
 *
 * Empty: no job is built yet. The servo tester and the KST programmer each
 * add their Core1JobOps here, and core1JobStart() refuses a job with none.
 */
static const Core1JobOps *s_ops[] = {
	nullptr,  // Core1Job::None
	nullptr,  // Core1Job::Servo
	nullptr,  // Core1Job::Kst
};
static_assert(sizeof(s_ops) / sizeof(s_ops[0]) == (unsigned)Core1Job::Kst + 1,
              "one entry per Core1Job");

/** @brief Guards the shared block below. */
static critical_section_t s_cs;

/**
 * @defgroup core1_job_shared Shared block (under s_cs)
 * @brief Core0 writes the request and the stop; core1 writes the outcome.
 * @{
 */
static Core1Job      s_job   = Core1Job::None;      /**< Job asked for, or held. */
/** @brief Where it is. Volatile for core1JobPoll()'s unlocked idle check. */
static volatile Core1JobPhase s_phase = Core1JobPhase::Idle;
static uint8_t       s_sig   = 0xFF;                /**< SIG GPIO to bind. */
static uint8_t       s_aux   = 0xFF;                /**< AUX GPIO to bind. */
static uint8_t       s_error = CORE1_JOB_OK;        /**< Last bind failure. */
/** @} */

/** @brief Core1 only: the job's bind succeeded and release is owed. */
static bool s_bound = false;

void core1JobInit() {
	critical_section_init(&s_cs);
}

bool core1JobStart(Core1Job job, uint8_t sigGpio, uint8_t auxGpio) {
	// The guard. The pump frees both pins before it reports suspended, so a
	// job bound after this point never shares a pin with a DShot frame.
	if (!escTaskSuspended()) return false;
	if (s_ops[(uint8_t)job] == nullptr) return false;

	critical_section_enter_blocking(&s_cs);
	bool idle = s_phase == Core1JobPhase::Idle;
	if (idle) {
		s_job   = job;
		s_sig   = sigGpio;
		s_aux   = auxGpio;
		s_error = CORE1_JOB_OK;
		s_phase = Core1JobPhase::Starting;
	}
	critical_section_exit(&s_cs);
	return idle;
}

void core1JobStop() {
	critical_section_enter_blocking(&s_cs);
	if (s_phase == Core1JobPhase::Starting || s_phase == Core1JobPhase::Running)
		s_phase = Core1JobPhase::Stopping;
	critical_section_exit(&s_cs);
}

Core1Job core1JobRunning() {
	critical_section_enter_blocking(&s_cs);
	Core1Job job = s_job;
	critical_section_exit(&s_cs);
	return job;
}

Core1JobPhase core1JobPhase() {
	critical_section_enter_blocking(&s_cs);
	Core1JobPhase ph = s_phase;
	critical_section_exit(&s_cs);
	return ph;
}

uint8_t core1JobError() {
	critical_section_enter_blocking(&s_cs);
	uint8_t err = s_error;
	critical_section_exit(&s_cs);
	return err;
}

bool core1UiAlive() {
	return escUiAlive(millis(), escHeartbeatMs());
}

void core1JobPoll() {
	// Unlocked, and only to return early: an idle loop1() pass takes no lock.
	// A start that lands after this read is taken on the next pass.
	if (s_phase == Core1JobPhase::Idle) return;

	critical_section_enter_blocking(&s_cs);
	Core1JobPhase ph = s_phase;
	const Core1JobOps *ops = s_ops[(uint8_t)s_job];
	uint8_t sig = s_sig, aux = s_aux;
	critical_section_exit(&s_cs);

	if (ph == Core1JobPhase::Starting) {
		uint8_t err = ops->bind(sig, aux);
		s_bound = err == CORE1_JOB_OK;
		critical_section_enter_blocking(&s_cs);
		if (!s_bound) {
			s_error = err;
			s_job   = Core1Job::None;
			s_phase = Core1JobPhase::Idle;
		} else if (s_phase == Core1JobPhase::Starting) {
			s_phase = Core1JobPhase::Running;
		}
		// A stop that arrived during the bind stays Stopping; the next pass
		// releases what the bind took.
		critical_section_exit(&s_cs);
		return;
	}

	if (ph == Core1JobPhase::Stopping) {
		if (s_bound) ops->release();
		s_bound = false;
		critical_section_enter_blocking(&s_cs);
		s_job   = Core1Job::None;
		s_phase = Core1JobPhase::Idle;
		critical_section_exit(&s_cs);
		return;
	}

	ops->step(millis(), core1UiAlive());
}
