/**
 * @file pin_claim.h
 * @brief A screen takes the ESC and KISS pins from DShot while it is open.
 *
 * The RP2350-Touch-LCD-2.8 brings out two signal pins. Both belong to the
 * ESC task, the DShot pump on one and the KISS receiver on the other, until a
 * screen that needs them claims them. The claim names pins by role, because
 * the GPIO number is a setting:
 *
 * | Role | Pin | Claimed by |
 * |---|---|---|
 * | @ref PIN_SIG | the ESC signal wire, `settings()->dshotPin` | AM32 config, servo A, KST programming |
 * | @ref PIN_AUX | the KISS wire, `settings()->kissPin` | servo B, two-servo mode |
 *
 * Every claim is escTaskSuspend(): with the signal pin gone the ESC hears no
 * frames and disarms, and with no frames there is no KISS request, so both
 * pins are free while a claim holds. The roles decide only which pins the
 * screen's core1 job binds.
 *
 * Core0 runs the claim as a state machine, one step per UI frame, from the
 * screen that holds it. Core1 makes every PIO change on the two signal pins:
 * the pump's drain and teardown (esc_task.cpp) and the job's bind and release
 * (core1_job.cpp). The AM32 transport, on core0, is the exception: it binds
 * SIG itself (am32_bl.cpp).
 *
 * | State | What happens | Next |
 * |---|---|---|
 * | Free | DShot runs on both pins | pinClaimBegin() |
 * | Stopping | core1 drains, stops and releases the pump | Held when escTaskSuspended() |
 * | Held | both pins free; the GPIOs are latched; SERVO and KST start their job | Running once it binds |
 * | Running | the job holds its pins | Leaving on pinClaimEnd() |
 * | Leaving | the job releases its pins; SERVO and KST show the unplug page | Restoring |
 * | Restoring | core1 rebuilds the pump from the latched wiring | Free |
 *
 * AM32 has no core1 job: its bootloader transport runs on core0
 * (am32_bl.cpp) and binds the pin itself, so an AM32 claim stays in Held
 * until pinClaimEnd().
 *
 * @see docs/design/pin-claim.md
 */

#pragma once

#include <stdint.h>

#include "config.h"
#include "esc_merge.h"

/** @brief The ESC signal pin: escTaskDshotPin(). */
#define PIN_SIG 0x01u
/** @brief The KISS pin: escTaskKissPin(), whether or not KISS is on. */
#define PIN_AUX 0x02u

/** @brief GPIO of a role that is not latched, or was not asked for. */
#define PIN_CLAIM_NO_GPIO 0xFFu

/** @brief The screen that holds the claim. */
enum class PinUser : uint8_t {
	None,   /**< No claim. */
	Am32,   /**< AM32 CONFIG: SIG, no core1 job. */
	Servo,  /**< SERVO: SIG, and AUX for two servos. */
	Kst,    /**< KST SERVO: SIG. */
};

/** @brief Where the claim is. See the table in this file's description. */
enum class ClaimState : uint8_t {
	Free,       /**< No claim; DShot owns the pins. */
	Stopping,   /**< The pump drains and releases. */
	Held,       /**< Both pins free, no job bound. */
	Running,    /**< The job holds its pins. */
	Leaving,    /**< The job releases; the unplug page for SERVO and KST. */
	Restoring,  /**< The pump rebuilds. */
};

/** @brief Why pinClaimBegin() refused. */
enum class ClaimRefusal : uint8_t {
	None,          /**< Not refused; the claim is Stopping. */
	Armed,         /**< The tester is armed. */
	Busy,          /**< A claim holds, or is still restoring. */
	EscAnswering,  /**< An eRPM frame within @ref ESC_LINK_STALE_MS (500 ms). SERVO and KST; AM32 is exempt. */
	KissLive,      /**< A KISS frame within @ref KISS_STALE_MS (500 ms), AUX asked for. */
	SamePin,       /**< AUX asked for, and SIG and AUX are the same GPIO. */
};

/** @brief What pinClaimCheck() judges a claim against. */
struct ClaimFacts {
	bool     armed;       /**< uiArmed(). */
	bool     held;        /**< A claim is not Free. */
	uint32_t nowMs;       /**< millis(). */
	uint32_t lastRpmMs;   /**< EscTelemetry::lastRpmMs. */
	uint32_t kissLastMs;  /**< EscTelemetry::kissLastMs. */
	uint8_t  sigGpio;     /**< escTaskDshotPin(). */
	uint8_t  auxGpio;     /**< escTaskKissPin(). */
};

/**
 * @brief Decide whether a claim may start. Pure; host-testable.
 *
 * The rules, in order:
 *
 * - **Armed.** Refused, and nothing is disarmed on the caller's behalf. Every
 *   entry point is on the SETTINGS screen, which disarms on entry, so through
 *   the UI this never fires; a route added later that reaches a claim while
 *   armed shows up as a refusal in a test, not as a motor stopped by
 *   navigation.
 * - **Busy.** One claim at a time, and not before the last one has restored
 *   DShot.
 * - **EscAnswering**, SERVO and KST. Servo pulses of 1000 to 2000 us are a
 *   valid PWM throttle signal, and a KST entry sequence sends frames into
 *   whatever is on the wire. The screen keeps asking; the claim follows by
 *   itself 500 ms after the ESC goes quiet. AM32 is exempt: it needs the ESC
 *   on the pin.
 * - **KissLive**, AUX asked for. An ESC telemetry output drives AUX, and servo
 *   PWM would drive against it.
 * - **SamePin**, AUX asked for. With KISS off, SETUP can put the ESC pin on
 *   the KISS pin's GPIO, and two servos on one pin are one compare register
 *   driven twice.
 *
 * Freshness is escFieldFresh(), the one definition the readouts use.
 *
 * @param who   The screen asking.
 * @param roles @ref PIN_SIG, or @ref PIN_SIG | @ref PIN_AUX.
 * @param f     The facts at the time of the call.
 * @return      ClaimRefusal::None, or the first rule that refuses.
 */
static inline ClaimRefusal pinClaimCheck(PinUser who, uint8_t roles,
                                         const ClaimFacts *f) {
	if (f->armed) return ClaimRefusal::Armed;
	if (f->held)  return ClaimRefusal::Busy;
	if (who != PinUser::Am32 &&
	    escFieldFresh(f->lastRpmMs, f->nowMs, ESC_LINK_STALE_MS))
		return ClaimRefusal::EscAnswering;
	if (roles & PIN_AUX) {
		if (escFieldFresh(f->kissLastMs, f->nowMs, KISS_STALE_MS))
			return ClaimRefusal::KissLive;
		if (f->sigGpio == f->auxGpio) return ClaimRefusal::SamePin;
	}
	return ClaimRefusal::None;
}

/**
 * @brief Claim the pins for a screen. Core0.
 *
 * Judges the claim with pinClaimCheck() against the live state. On a refusal
 * nothing changes and DShot keeps running, so a screen may ask again every
 * frame. Otherwise it calls escTaskSuspend() and the claim is Stopping.
 *
 * @param who   The screen.
 * @param roles @ref PIN_SIG, or @ref PIN_SIG | @ref PIN_AUX.
 * @return ClaimRefusal::None when the claim started.
 */
ClaimRefusal pinClaimBegin(PinUser who, uint8_t roles);

/**
 * @brief Advance the claim one step. Core0, once per UI frame, by the holder.
 * @return The state after the step.
 */
ClaimState pinClaimPoll();

/**
 * @brief The claim's state, without a step. Settles Restoring into Free once
 *        the pump runs again.
 * @return The state.
 */
ClaimState pinClaimState();

/**
 * @brief Who holds the claim.
 * @return The holder, or PinUser::None while Free.
 */
PinUser pinClaimHolder();

/**
 * @brief The GPIO a role names, latched once at Held.
 * @param role @ref PIN_SIG or @ref PIN_AUX.
 * @return The GPIO, or @ref PIN_CLAIM_NO_GPIO before Held, after Free, or for a
 *         role the claim did not ask for.
 */
uint8_t pinClaimGpio(uint8_t role);

/**
 * @brief Why the job did not bind.
 * @return A Core1JobError from core1_job.h; 0 for none.
 */
uint8_t pinClaimJobError();

/**
 * @brief BACK: leave the claim. Core0. The poll finishes it.
 *
 * From Stopping, DShot is resumed at once. From Held and Running the job is
 * asked to stop and the claim is Leaving. AM32, which has no job and no
 * unplug page, resumes DShot within this call. A call from a screen that does
 * not hold the claim does nothing.
 *
 * @param who The screen leaving.
 */
void pinClaimEnd(PinUser who);

/**
 * @brief RETRY after a failed bind: start the job again. Core0.
 *
 * Acts only while @p who holds the claim, the claim is Held and
 * pinClaimJobError() names a failure, which also means no start is pending.
 * The claim and its pins are kept; DShot stays down. BACK instead is
 * pinClaimEnd(), which shows the unplug page before DShot restarts.
 *
 * @param who The screen.
 */
void pinClaimRetry(PinUser who);

/**
 * @brief True while SERVO or KST must show the unplug page.
 *
 * Leaving, with the job idle and DShot not yet restarted. DShot idles the
 * line high and pulls it low for about 27 us per 1000 us frame, which a PWM
 * servo reads as a pulse of about 970 us at 1000 Hz.
 *
 * @return True while RESTART DSHOT is what the claim waits for.
 */
bool pinClaimUnplugDue();

/**
 * @brief RESTART DSHOT on the unplug page: resume DShot. Core0.
 *
 * Acts only while pinClaimUnplugDue() is true and @p who holds the claim.
 *
 * @param who The screen.
 */
void pinClaimRestart(PinUser who);

/**
 * @brief BACK on the unplug page: keep the claim. Core0.
 *
 * Acts only while pinClaimUnplugDue() is true and @p who holds the claim. The
 * claim returns to Held and the job starts again.
 *
 * @param who The screen.
 */
void pinClaimKeep(PinUser who);
