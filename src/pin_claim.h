/**
 * @file pin_claim.h
 * @brief A screen takes the ESC and KISS pins from DShot while it is open.
 *
 * The RP2350-Touch-LCD-2.8 brings out two signal pins. Both belong to the
 * ESC task, the DShot pump on one and the KISS receiver on the other, until a
 * screen that needs them claims them. The pump is parked unless ESC TESTER
 * or ESC COMMANDS is open: SIG driven low, AUX on its pull-down
 * (docs/design/dshot-lifetime.md). The claim names pins by role, because
 * the GPIO number is a setting:
 *
 * | Role | Pin | Claimed by |
 * |---|---|---|
 * | @ref PIN_SIG | the ESC signal wire, `settings()->dshotPin` | ESC settings (AM32), servo A, KST and Hitec programming; the S.BUS monitor; the USB bridge; the S.BUS output |
 * | @ref PIN_AUX | the KISS wire, `settings()->kissPin` | servo B, two-servo mode; the S.BUS monitor when AUX is GP1; the S.BUS output when AUX is GP0 |
 *
 * The claim alone asks core1 for the pump's three modes: Run and Park through
 * pinClaimDshot(), which the UI calls once a frame with uiDshotWanted(), and
 * Release through every claim, escTaskSuspend(). The pump a claim releases is
 * parked, or, within 250 ms of leaving a running tester, still finishing the
 * drain that leaving started; with no frames there is no KISS request, so
 * both pins are free while a claim holds. The roles decide only which pins
 * the screen's core1 job binds.
 *
 * Core0 runs the claim as a state machine, one step per UI frame, from the
 * screen that holds it. Core1 makes every PIO change on the two signal pins:
 * the pump's drain and teardown (esc_task.cpp) and the job's bind and release
 * (core1_job.cpp). The AM32 transport, on core0, is the exception: it binds
 * SIG itself (am32_bl.cpp) until an EFM8 bootloader answers; ESC SETTINGS'
 * job, rcbench's escwire, then takes SIG on core1 (escwire_job.h).
 *
 * | State | What happens | Next |
 * |---|---|---|
 * | Free | DShot parked, or running for ESC TESTER and ESC COMMANDS (pinClaimDshot()) | pinClaimBegin(), not while Run is wanted |
 * | Stopping | core1 releases the parked pump, after the rest of a drain if one is owed | Held when escTaskSuspended() |
 * | Held | both pins free; the GPIOs are latched; SERVO, KST, the S.BUS monitor, the USB bridge and the S.BUS output start their job; HITEC as its entry asks, ESC SETTINGS when an EFM8 bootloader answered (pinClaimStartJob()) | Running once it binds |
 * | Running | the job holds its pins | Leaving on pinClaimEnd() |
 * | Leaving | the job releases its pins; SERVO, KST, HITEC, the S.BUS monitor and the S.BUS output show the leave page (ui_leave.h), and AM32 after an EFM8 bootloader answered | Parking: on the leave page's last button, pinClaimRelease(), else once the job is idle |
 * | Parking | core1 parks SIG low and AUX on its pull-down, from the latched wiring | Free when escTaskParked() |
 *
 * ESC SETTINGS (PinUser::Am32) defers its job: its AM32 bootloader
 * transport runs on core0 (am32_bl.cpp) and binds the pin itself, so the
 * claim stays in Held with no job until an EFM8 bootloader answers the
 * greeting. The screen then ends the transport and calls
 * pinClaimStartJob(): Core1Job::EscWire takes SIG on core1
 * (docs/design/bluejay-blheli-s.md, slice 3). pinClaimKeep() returns it to
 * Held with no job, for the transport to greet again.
 *
 * The leave page belongs to the claim, not only to its user: a user's
 * PinUserInfo::unplug sets it when the claim begins, and
 * pinClaimWantLeavePage() sets it for the rest of a claim that began
 * without it. ESC SETTINGS does so when an EFM8 bootloader, Bluejay's or
 * BLHeli_S's, answers its greeting: that bootloader waits for commands with
 * no timeout, and DShot frames could pass as one
 * (docs/design/bluejay-blheli-s.md, "Safety"). The USB bridge is to do so
 * after a write-class frame with no RUN behind it
 * (docs/design/usb-bridge.md, decision 10); nothing opens the bridge yet.
 *
 * A claim without the leave page parks the pins only once its job is idle:
 * the job lets its pins and state machines go first, so the park never
 * touches a pin a job still drives. The USB bridge is the one such user
 * with a job; its release finishes the frame under way, at most
 * @ref BRIDGE_RELEASE_MAX_MS. Every call that reads or changes the claim
 * moves Leaving on, pinClaimState() among them, so the pins park once the
 * job is idle whether or not the screen still polls. An AM32 claim no EFM8
 * bootloader answered has started no job and parks at once.
 *
 * HITEC defers its job too: the entry tap on SERVO, the hub, claims SIG, and the
 * screen calls pinClaimStartJob() at once for CLASSIC and for D SERIES with
 * the switched supply, and on READ for D SERIES on OWN, for each servo:
 * pinClaimKeep() leaves it unasked again. Until then the
 * claim stays in Held with no job, the pad an input with the pull-up
 * esc_task.cpp leaves on it. BACK from there is pinClaimEnd(): the pin went
 * to a servo, so the leave page comes before the pins park.
 *
 * @see docs/design/pin-claim.md
 */

#pragma once

#include <stdint.h>

#include "config.h"
#include "esc_merge.h"
#include "core1_job.h"

/** @brief The ESC signal pin: escTaskDshotPin(). */
#define PIN_SIG 0x01u
/** @brief The KISS pin: escTaskKissPin(), whether or not KISS is on. */
#define PIN_AUX 0x02u

/** @brief GPIO of a role that is not latched, or was not asked for. */
#define PIN_CLAIM_NO_GPIO 0xFFu

/** @brief The screen that holds the claim. */
enum class PinUser : uint8_t {
	None,   /**< No claim. */
	Am32,   /**< ESC SETTINGS: SIG; its core1 job only after an EFM8 bootloader answered. */
	Servo,  /**< SERVO: SIG, and AUX for two servos. */
	Kst,    /**< KST SERVO: SIG. */
	Hitec,  /**< HITEC SERVO: SIG. */
	Sbus,   /**< The S.BUS monitor: the role on GP1, AUX with SIG or SIG alone (sbus_job.h). */
	Bridge, /**< The USB bridge: SIG (bridge_wire.h). */
	SbusOut,  /**< The S.BUS output: the role on GP0, SIG alone or with AUX (sbus_out_job.h); S.BUS OUTPUT claims it. */
	Count,  /**< How many pin users; not a user. */
};

/** @brief What the claim does for one user: one row per user in @ref PIN_USERS. */
struct PinUserInfo {
	PinUser  who;
	Core1Job job;       /**< The core1 job; None: no job. */
	bool     unplug;    /**< The leave page comes before the pins park, from the claim's start; pinClaimWantLeavePage() sets it later. */
	bool     escCheck;  /**< Refused while an ESC answers on SIG (@ref ClaimRefusal::EscAnswering). */
	bool     deferJob;  /**< Held starts no job until pinClaimStartJob(). */
};

/** @brief Every pin user. A new user is one enum value and one line here. */
static constexpr PinUserInfo PIN_USERS[] = {
	{ PinUser::None,  Core1Job::None,  false, false, false },
	{ PinUser::Am32,  Core1Job::EscWire, false, false, true },
	{ PinUser::Servo, Core1Job::Servo, true,  true,  false },
	{ PinUser::Kst,   Core1Job::Kst,   true,  true,  false },
	{ PinUser::Hitec, Core1Job::Hitec, true,  true,  true  },
	// The monitor drives nothing; an ESC on SIG only loses its frames. When
	// GP1 is SIG its screen makes its own ESC check (ui_sbus.h).
	{ PinUser::Sbus,  Core1Job::Sbus,  true,  false, false },
	// The bridge needs the ESC on the pin and answering, as AM32 does. BACK
	// of a running bridge gives the leave page through
	// pinClaimWantLeavePage(); BACK before the first bind parks the pins,
	// but a claim that has had the page keeps it, so BACK after a failed
	// rebind from the page's BRIDGE gives the page again
	// (docs/design/usb-bridge.md, "The pin", decision 10).
	{ PinUser::Bridge, Core1Job::Bridge, false, false, false },
	// The output drives GP0: no ESC stays on the bench while it runs, in
	// either wiring (docs/design/sbus-output.md, "The ESC check").
	{ PinUser::SbusOut, Core1Job::SbusOut, true, true, false },
};

/**
 * @brief The row of @p who in @ref PIN_USERS.
 * @param who A pin user.
 * @return Its row; the None row for a value without one.
 */
static constexpr const PinUserInfo &pinUserInfo(PinUser who) {
	for (const PinUserInfo &u : PIN_USERS)
		if (u.who == who) return u;
	return PIN_USERS[0];
}

/** @brief Each @ref PinUser has exactly one row in @ref PIN_USERS. */
static constexpr bool pinUsersComplete() {
	for (int w = 0; w < (int)PinUser::Count; w++) {
		int n = 0;
		for (const PinUserInfo &u : PIN_USERS) n += (int)u.who == w;
		if (n != 1) return false;
	}
	return true;
}
static_assert(pinUsersComplete(), "a pin user has no row in PIN_USERS, or two");

/** @brief Where the claim is. See the table in this file's description. */
enum class ClaimState : uint8_t {
	Free,       /**< No claim; the pump owns the pins, parked or running. */
	Stopping,   /**< The pump releases, after the rest of a drain if one is owed. */
	Held,       /**< Both pins free, no job bound; HITEC on OWN waits here for READ. */
	Running,    /**< The job holds its pins. */
	Leaving,    /**< The job releases; a claim with the leave page shows it (pinClaimLeavePage()). */
	Parking,    /**< Core1 parks the pins: SIG low, AUX on its pull-down. */
};

/** @brief Why pinClaimBegin() refused. */
enum class ClaimRefusal : uint8_t {
	None,          /**< Not refused; the claim is Stopping. */
	Armed,         /**< The tester is armed. */
	Busy,          /**< A claim holds or is still parking, or Run is wanted (pinClaimDshot()). */
	EscAnswering,  /**< An eRPM frame within @ref ESC_LINK_STALE_MS (500 ms). SERVO, KST, HITEC and the S.BUS output; AM32, the S.BUS monitor and the USB bridge are exempt. */
	KissLive,      /**< A KISS frame within @ref KISS_STALE_MS (500 ms), AUX asked for. */
	SamePin,       /**< AUX asked for, and SIG and AUX are the same GPIO. */
};

/** @brief What pinClaimCheck() judges a claim against. */
struct ClaimFacts {
	bool     armed;       /**< uiArmed(). */
	bool     held;        /**< A claim is not Free, or Run is wanted. */
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
 *   entry point is on MENU, which disarms on entry, so through
 *   the UI this never fires; a route added later that reaches a claim while
 *   armed shows up as a refusal in a test, not as a motor stopped by
 *   navigation.
 * - **Busy.** One claim at a time, not before the last one has parked the
 *   pins, and not while Run is wanted for ESC TESTER or ESC COMMANDS.
 * - **EscAnswering**, SERVO, KST, HITEC and the S.BUS output. With the pump
 *   parked nothing answers, so it refuses only within 500 ms of the last
 *   answer after ESC TESTER or ESC COMMANDS closed; the hubs SERVO and
 *   RECEIVER warn `NO ESC CHECK: UNPLUG ANY ESC FIRST.` in its place
 *   (docs/design/dshot-lifetime.md, decisions 7 and 11). Servo pulses
 *   of 1000 to 2000 us are a valid PWM throttle signal, a KST or Hitec entry
 *   sends frames into whatever is on the wire, and S.BUS frames must not
 *   reach an ESC: the output checks in either wiring, GP0 as SIG or as AUX,
 *   so no ESC stays on the bench while it runs. The screen keeps asking; the
 *   claim follows by itself 500 ms after the ESC goes quiet. AM32 and the
 *   USB bridge are exempt: they need the ESC on the pin. The S.BUS monitor is
 *   exempt: it drives nothing, and when GP1 is SIG its screen makes the
 *   check itself (ui_sbus.h).
 * - **KissLive**, AUX asked for. An ESC telemetry output drives AUX, and servo
 *   PWM would drive against it; for the S.BUS monitor the telemetry wire is
 *   where the receiver goes.
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
	if (pinUserInfo(who).escCheck &&
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
 * nothing changes, so a screen may ask again every frame. Otherwise it calls
 * escTaskSuspend() and the claim is Stopping.
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
 * @brief The claim's state, without a step. Settles Parking into Free once
 *        the pins are parked.
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
 * From Stopping, the pins park at once. From Held and Running the job is
 * asked to stop and the claim is Leaving. A claim without the leave page
 * (pinClaimLeavePage()) parks the pins once its job is idle: an AM32 claim no
 * EFM8 bootloader answered, which has started no job, within this call; the USB
 * bridge on the first call into the claim after its job let the pin go,
 * pinClaimPoll() or pinClaimState() alike. A call from a screen that does
 * not hold the claim does nothing.
 *
 * @param who The screen leaving.
 */
void pinClaimEnd(PinUser who);

/**
 * @brief Start the job of a claim whose user defers it (PinUserInfo::deferJob):
 *        HITEC's entry, or its READ; ESC SETTINGS' escwire session after an
 *        EFM8 bootloader answered. Core0.
 *
 * Acts only while @p who holds the claim, Stopping or Held. From Stopping
 * the job starts once the claim is Held; from Held, with no job started
 * yet, it starts now. The job stays wanted until pinClaimKeep(), which
 * returns a deferring user to Held with no job, to ask again. A user that
 * does not defer its job has it wanted from pinClaimBegin() and from
 * pinClaimKeep().
 *
 * @param who The screen.
 */
void pinClaimStartJob(PinUser who);

/**
 * @brief RETRY after a failed bind: start the job again. Core0.
 *
 * Acts only while @p who holds the claim, the claim is Held and
 * pinClaimJobError() names a failure, which also means no start is pending.
 * The claim and its pins are kept; the pump stays down. BACK instead is
 * pinClaimEnd(), which shows the leave page before the pins park.
 *
 * @param who The screen.
 */
void pinClaimRetry(PinUser who);

/**
 * @brief Give the claim the leave page: from pinClaimEnd() on, the pins
 *        park only on the page's last button, pinClaimRelease(). Core0.
 *
 * Acts only while @p who holds the claim and it is Stopping, Held or
 * Running. It lasts until the claim is Free. A claim still Stopping has
 * driven nothing on SIG and parks at once. ESC SETTINGS calls it when an
 * EFM8 bootloader answers its greeting; the USB bridge is to call it after a
 * write-class frame (docs/design/usb-bridge.md, decision 10).
 *
 * @param who The screen.
 */
void pinClaimWantLeavePage(PinUser who);

/**
 * @brief Whether the claim shows the leave page before the pins park:
 *        its user's PinUserInfo::unplug, or pinClaimWantLeavePage().
 * @return false while Free.
 */
bool pinClaimLeavePage();

/**
 * @brief True while the job has let the pins go and SERVO, KST, HITEC, the
 *        S.BUS monitor or ESC SETTINGS waits on its leave page for its last
 *        button, the hub's title, or for NEXT SERVO or BACK TO MONITOR.
 *
 * Leaving, with the job idle and the pins not yet parked. ESC TESTER's
 * DShot idles the line high and pulls it low for about 27 us per 1000 us
 * frame, which a PWM servo reads as a pulse of about 970 us at 1000 Hz: the
 * page asks for the servo to be unplugged before it.
 *
 * @return True while the page's last button is what the claim waits for.
 */
bool pinClaimUnplugDue();

/**
 * @brief The leave page's last button, the hub's title: park the pins and
 *        end the claim. Core0. Starts no DShot.
 *
 * Acts only while pinClaimUnplugDue() is true and @p who holds the claim; the
 * claim is then Parking, and Free once core1 has parked the pins. The leave
 * page takes the tap only 1000 ms or more after that turned true.
 *
 * @param who The screen.
 */
void pinClaimRelease(PinUser who);

/**
 * @brief Run or Park, as the UI wants it on this frame: uiTick() calls it
 *        once a frame with uiDshotWanted(), before the screen's tick.
 *        Core0.
 *
 * Acts only while the claim is Free. With @p on, and Run not yet wanted, it
 * asks core1 for Run (escTaskStart()), and from then on Run is wanted.
 * Without @p on, and Run wanted, it asks for Park (escTaskPark(), which
 * disarms): core1 sends zero frames until more than 250 ms after the last
 * throttle frame, then parks SIG low and AUX on its pull-down, and from then
 * on Run is not wanted, whether core1 still drains or has parked. "Run is
 * wanted" is the claim's own record, not the pump's state; pinClaimBegin()
 * refuses as Busy while it holds. @see docs/design/dshot-lifetime.md
 *
 * @param on DShot wanted on this frame: ESC TESTER or ESC COMMANDS is open.
 */
void pinClaimDshot(bool on);

/**
 * @brief Whether DShot runs for ESC TESTER or ESC COMMANDS: the claim is
 *        Free, Run is wanted and core1 reports the pump running.
 * @return false while parked, draining to Park, or under a claim.
 */
bool pinClaimDshotRunning();

/**
 * @brief NEXT SERVO, or BACK TO MONITOR, on the leave page: keep the claim.
 *        Core0. BACK on the leave page is disabled.
 *
 * Acts only while pinClaimUnplugDue() is true and @p who holds the claim. The
 * claim returns to Held. SERVO and KST start their job again; HITEC, which
 * defers its job, returns to Held with no job and asks for it again with
 * pinClaimStartJob(), at once or on READ, as for its first servo.
 *
 * @param who The screen.
 */
void pinClaimKeep(PinUser who);
