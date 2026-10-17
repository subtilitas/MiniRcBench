/**
 * @file esc_task.h
 * @brief Bidirectional DShot driver task. Runs on core1.
 *
 * Core1 owns the DShot pin driver (`dshot/out_dshot.h`) and does nothing but
 * push frames at a steady @ref DSHOT_PERIOD_US and drain telemetry. That keeps
 * DShot timing immune to the multi-millisecond SPI DMA bursts the display does
 * on core0.
 *
 * The ESC's answer to a frame lands 30 us after that frame ends (fixed by the
 * bidirectional-DShot spec, independent of speed), so each slot first drains
 * the previous frame's reply, then sends the next one.
 *
 * Core0 talks to it through a small critical-section-guarded state block:
 * escSetThrottle() and escSetArmed() to command, escSnapshot() to read back.
 *
 * @warning Core0 must call escHeartbeat() regularly. If it stops for longer
 *          than @ref UI_HEARTBEAT_TIMEOUT_MS, core1 assumes the UI has died and
 *          forces the throttle to zero on its own.
 */

#pragma once

#include <stdint.h>

#include "config.h"
#include "dshot/dshot.h"

/**
 * @brief Everything core1 has decoded from the ESC.
 *
 * A field whose arrival time is 0 was never sent by this ESC — plain eRPM always
 * works on bidirectional DShot, but the rest requires Extended DShot Telemetry
 * support in the ESC firmware. A field whose arrival time has simply fallen
 * behind is a different thing: that ESC does send it, and has stopped. Use
 * escFieldFresh() rather than reading the values bare.
 */
struct EscTelemetry {
	uint32_t erpm;          /**< Raw electrical RPM from the ESC. */
	uint32_t rpm;           /**< Mechanical RPM: `erpm / (poles / 2)`. */
	float    volts;         /**< EDT voltage frame, converted from 250 mV steps. */
	float    amps;          /**< EDT current frame, 1 A steps. */
	int16_t  tempC;         /**< EDT temperature frame, degrees Celsius. */
	uint8_t  stress;        /**< EDT stress frame, 0..255. */
	uint8_t  statusRaw;     /**< EDT status frame, undecoded. */
	uint8_t  maxStress;     /**< Status bits 3..0: max stress level, 0..15. */
	bool     alert;         /**< Status bit 7. Meaning is ESC-firmware specific. */
	bool     warning;       /**< Status bit 6. Meaning is ESC-firmware specific. */
	bool     error;         /**< Status bit 5. Meaning is ESC-firmware specific. */

	uint32_t goodPackets;   /**< Frames decoded successfully since boot. */
	uint32_t badPackets;    /**< Checksum failures since boot. */
	uint32_t noPackets;     /**< Frames the ESC did not answer at all. */
	uint16_t packetRate;    /**< Good packets per second, updated once a second. */
	uint8_t  errPercent;    /**< Checksum error rate over the last second. */

	/**
	 * @name Arrival times
	 *
	 * When each kind of frame last arrived, as millis(); 0 means never. These
	 * are timestamps rather than `have` flags on purpose. A sticky boolean can
	 * only ever go true, so unplugging the ESC — or swapping it for another —
	 * left the last voltage, current and temperature on screen indefinitely,
	 * looking exactly like live data from hardware that was no longer there.
	 *
	 * EDT frame types are interleaved by the ESC and each cycles far quicker
	 * than @ref EDT_STALE_MS, so per-field expiry costs nothing in practice and
	 * localises the loss when only one frame type stops.
	 *
	 * @{
	 */
	uint32_t lastRpmMs;     /**< Last valid eRPM frame. */
	uint32_t edtVoltsMs;    /**< Last EDT voltage frame. */
	uint32_t edtAmpsMs;     /**< Last EDT current frame. */
	uint32_t edtTempMs;     /**< Last EDT temperature frame. */
	uint32_t edtStressMs;   /**< Last EDT stress frame. */
	uint32_t edtStatusMs;   /**< Last EDT status frame. */
	/** @} */

	/**
	 * The DShot driver refused the pin: no PIO block had two adjacent free
	 * state machines and 16 free instructions, or the pad did not take the
	 * output inversion. Nothing is sent while it is set, and the status bar
	 * shows `DSERR` in place of the bitrate.
	 */
	bool     initError;

	/**
	 * @name KISS telemetry
	 * From the separate telemetry wire, not the DShot signal line. Kept
	 * distinct from the EDT fields above rather than overwriting them, so the
	 * two can be compared against each other during bring-up — which is how
	 * the KISS decoder gets validated against a source already trusted.
	 * @see escMerge(), docs/design/kiss-telemetry.md
	 * @{
	 */
	float    kissVolts;     /**< Pack voltage, 0.01 V resolution. */
	float    kissAmps;      /**< Current, 0.01 A resolution. */
	int16_t  kissTempC;     /**< ESC temperature, degrees Celsius. */
	uint16_t kissMah;       /**< Consumption since ESC power-on, not since arm. */
	uint32_t kissErpm;      /**< Electrical RPM, 100 eRPM steps. */
	uint32_t kissLastMs;    /**< millis() of the last CRC-valid frame. */
	uint32_t kissGood;      /**< Frames that passed CRC since boot. */
	uint32_t kissBad;       /**< Frames that failed CRC since boot. */
	uint32_t kissTimeouts;  /**< Requests that drew no complete reply. */
	bool     haveKiss;      /**< True once any frame has been decoded. */
	/** @} */
};

/**
 * @defgroup esc_core1 Core1 entry points
 * @{
 */

/**
 * @brief Initialise the cross-core state. Call from **core0**, before launching core1.
 *
 * Separate from escTaskBegin() because of an ordering hazard that is invisible
 * until it bites: escSnapshot() takes a critical section, core0 calls it on its
 * very first UI frame, and a critical section must be initialised before anyone
 * enters it. If that initialisation lived on core1, core0 would reach
 * escSnapshot() first — core1 takes a moment to come up — and block forever on
 * an uninitialised spin lock.
 *
 * The symptom is a board that shows the splash and then nothing at all.
 */
void escTaskInit();

/**
 * @brief Start the frame pump. Call from core1.
 *
 * Does not itself claim the PIO state machines: the driver is bound on the
 * first escTaskPoll(), from whatever configuration escTaskConfigure() last
 * supplied, and that same path binds it again after a suspend or a pin change.
 * One construction site rather than two is the point: two sites can disagree
 * about the order of the bind and the `initError` it reports.
 *
 * @pre escTaskInit() has been called on core0.
 */
void escTaskBegin();

/** @brief Service one frame slot. Call from loop1() as fast as possible. */
void escTaskPoll();

/** @} */

/**
 * @defgroup esc_core0 Core0 API
 * @brief Safe to call from core0; each takes the shared critical section.
 * @{
 */

/**
 * @brief Set the commanded throttle.
 * @param throttle0to2000 Throttle, clamped to 0..2000. 0 is motor stop.
 *                        Ignored by core1 while disarmed.
 */
void escSetThrottle(uint16_t throttle0to2000);

/**
 * @brief Arm or disarm.
 * @param armed false immediately zeroes the throttle as well.
 */
void escSetArmed(bool armed);

/**
 * @brief Set the motor pole count used for the eRPM to RPM conversion.
 * @param poles Magnet count; values below 2 are clamped.
 */
void escSetPoles(uint8_t poles);

/**
 * @brief Tell core1 that core0 is still alive.
 * @see UI_HEARTBEAT_TIMEOUT_MS
 */
void escHeartbeat();

/**
 * @brief Queue a beacon command so the motor beeps.
 *
 * @param n Beacon 1..5, clamped.
 * @return False if the request was refused because the ESC is armed. The
 *         caller is expected to say so: a command that is silently dropped is
 *         indistinguishable from a button that does not work, which is exactly
 *         how it was reported.
 */
bool escRequestBeep(uint8_t n);

/** @brief What the automatic EDT enable should do this frame. */
enum class EdtAutoAction : uint8_t {
	None,   /**< Nothing to do: no ESC, EDT already working, or too soon. */
	Send,   /**< An ESC is answering and is not sending EDT. Ask it again. */
	Rearm,  /**< The ESC went away; the next one starts from a clean slate. */
};

/**
 * @brief Decide whether to send the automatic EDT enable. Pure; host-testable.
 *
 * This rule has been wrong twice, in the same direction both times, so it is
 * worth writing down what it is actually for.
 *
 * First it went out once, on a timer, 1.5 s after boot — fine for an ESC
 * already plugged in and powered, useless for one connected afterwards,
 * power-cycled, or swapped. Then it went out once, on the first eRPM frame,
 * which fixed the "connected afterwards" case and kept the deeper mistake: it
 * was still a one-shot, and it fired at the earliest instant an ESC could
 * possibly be heard from. An ESC answers eRPM within milliseconds of power-up
 * and will not act on a DShot command until it has seen a run of valid
 * zero-throttle frames, so the single attempt was made at close to the least
 * likely moment for it to be accepted. Nothing tried again.
 *
 * Both versions failed the same way — an ESC reporting RPM and nothing else,
 * which is exactly what an ESC without EDT support looks like — and both were
 * masked by the same accident: changing any wiring setting rebuilds the pump,
 * which cleared the flag and sent a second enable at a moment the ESC was ready
 * for. "EDT only comes on if I toggle KISS" is that accident, reported.
 *
 * The third version made the condition "is it working" rather than "have we
 * asked", and retried on an interval — and still went off sometimes, with a
 * disarm and re-arm as the reliable way to get it back. That is a tell, because
 * an arm transition is the one event that clears the `tried` flag, so what it
 * bought was an attempt *now* instead of one up to @ref EDT_RETRY_MS away. The
 * attempts themselves were the problem, not their number:
 *
 * - the first went out on the first eRPM frame, before the ESC would take it,
 *   and still counted as an attempt (@ref EDT_SETTLE_MS);
 * - a burst could be truncated by the beacon command sharing the same queue
 *   slot, and a truncated burst also counted (@ref EDT_ENABLE_REPEATS);
 * - and one could go out with the motor still coasting down after a disarm,
 *   which no ESC executes.
 *
 * Hence @p canCommand: the caller asserts the ESC is in a state where the
 * enable can actually be executed and where a burst will not be cut short. The
 * interval then governs genuine attempts rather than counting discarded ones.
 * @see EDT_RETRY_MS
 *
 * Split out as a pure function so the rule is testable — esc_task.cpp links
 * the PIO driver and cannot be linked into the host suite.
 *
 * @param linkUp     True if an eRPM frame arrived within @ref ESC_LINK_STALE_MS.
 * @param edtFresh   True if any EDT frame arrived within @ref EDT_STALE_MS.
 * @param canCommand True if the ESC would act on a command sent now: disarmed,
 *                   motor stopped, link settled, and no burst already going out.
 * @param tried      True if a scheduled enable, one this function asked for, has
 *                   gone out in full to the ESC that is connected. The enable a
 *                   bind sends does not count. @see escEdtBind()
 * @param sinceMs    Milliseconds since that enable. Ignored when @p tried is false.
 * @param retryMs    How long an unanswered enable is left before repeating it.
 * @return           What to do.
 */
static inline EdtAutoAction edtAutoAction(bool linkUp, bool edtFresh,
                                          bool canCommand, bool tried,
                                          uint32_t sinceMs, uint32_t retryMs) {
	// No ESC. Forget what was sent to the last one, once.
	if (!linkUp) return tried ? EdtAutoAction::Rearm : EdtAutoAction::None;
	// It is working. That is the whole success condition; nothing else to do
	// until the link drops.
	if (edtFresh) return EdtAutoAction::None;
	// Deliberately ahead of the interval check, and it leaves `tried` alone:
	// a moment the ESC cannot be commanded in is not an attempt, and must not
	// start the clock on the next one.
	if (!canCommand) return EdtAutoAction::None;
	if (tried && sinceMs < retryMs) return EdtAutoAction::None;
	return EdtAutoAction::Send;
}

/**
 * @brief Whether core1 reads a reply with EDT decoding on: for every reply.
 *
 * The decoder cannot tell an EDT frame from an eRPM frame by its bits alone
 * (see dshot_telem_from_value()), so the pump has to say which to expect.
 * The answer is EDT, from the first reply after a bind and through any loss
 * of the link:
 *
 * - The first frames after every bind are the EDT enable (escEdtBind()), so
 *   the first reply already answers an enable.
 * - An ESC that kept EDT on across a board restart, a pump rebuild after a
 *   SETUP change, or an AM32 settings session sends EDT frames from its
 *   first reply. Read as eRPM, a voltage frame of 66 counts is a period of
 *   264 us, 227,272 eRPM, and that is the RPM the display shows.
 * - An ESC without EDT, or with EDT off, sends eRPM with its exponent
 *   normalised: bit 8 of the payload is set whenever the exponent is not 0.
 *   Such a frame reads as eRPM with EDT decoding on, and a frame with
 *   exponent 0 has type nibble 0, which reads as eRPM too. The host suite
 *   checks every normalised payload.
 *
 * Only an eRPM frame with an exponent above 0 and bit 8 clear reads
 * differently, as an EDT type.
 */
static constexpr bool ESC_EDT_DECODE = true;

/**
 * @brief Core1's EDT enable state for the ESC on the pump's pin.
 *
 * Written only by escEdtBind(), escEdtStep() and escEdtFrameSent(), which are
 * pure and host-testable for the same reason as edtAutoAction().
 *
 * Two kinds of burst go out, and both are @ref EDT_ENABLE_REPEATS frames of
 * DSHOT_CMD_EDT_ENABLE:
 *
 * - the bind burst, the first frames after every bind;
 * - scheduled bursts, which edtAutoAction() asks for once the ESC has been
 *   answering for @ref EDT_SETTLE_MS, and again every @ref EDT_RETRY_MS while
 *   no EDT frame arrives.
 *
 * Only a scheduled burst counts as an attempt and starts the retry clock. The
 * bind burst goes out before the ESC has been heard from, and an ESC that is
 * not yet ready for commands ignores it. Counting it would hold the first
 * scheduled burst until 1009 ms after the bind; uncounted, that burst goes
 * out 300 ms after link-up.
 */
struct EscEdtState {
	uint8_t  reps;          /**< Enable repeats still owed; 0 means none queued. */
	bool     scheduled;     /**< The burst under way is a scheduled one. */
	bool     tried;         /**< A scheduled burst has gone out in full to this ESC. */
	uint32_t lastTryMs;     /**< millis() when that burst's last frame went out. */
	bool     armedWas;      /**< Arm state escEdtStep() last saw. */
	bool     sent;          /**< An enable has gone out to this ESC; escEdtRequested(). */
	uint32_t linkUpSinceMs; /**< When this ESC started answering; 0 while it is not. */
};

/**
 * @brief A new bind: queue the enable as the pump's first frames. Pure.
 *
 * Called once per bind, before the first frame. Everything else starts from
 * nothing having been tried with the ESC on the pin. The bind also forces a
 * disarm, so the arm state is recorded as disarmed, and an arm transition
 * seen before the bind does not abandon the burst.
 *
 * At the 1 kHz frame rate the burst takes 10 ms. Nothing waits on it: a
 * frame while armed carries the throttle (escFrameAction()), and an arm
 * abandons the burst. A beacon request and the first KISS request wait for
 * its end, at most 10 ms.
 *
 * @param e    State to reset.
 * @param reps Frames per enable, @ref EDT_ENABLE_REPEATS.
 */
static inline void escEdtBind(EscEdtState *e, uint8_t reps) {
	e->reps          = reps;
	e->scheduled     = false;
	e->tried         = false;
	e->lastTryMs     = 0;
	e->armedWas      = false;
	e->sent          = true;
	e->linkUpSinceMs = 0;
}

/**
 * @brief Decide the EDT enable for one frame slot and apply it. Pure.
 *
 * The rule is edtAutoAction(); this supplies its inputs from @p e and records
 * its outcome:
 *
 * - The settle window starts at the first slot with @p linkUp, and a slot
 *   without it resets the window.
 * - @c canCommand is the window having lasted @p settleMs, the ESC
 *   disarmed, @p ready, and no enable burst under way.
 * - An arm transition abandons the burst under way, scheduled or bind, and
 *   clears @c tried: a broken burst reaches no ESC's repeat count, so it is
 *   no attempt.
 * - A loss of the link after the bind burst is also a new ESC, once the burst
 *   has finished. The bind burst sets no @c tried, so edtAutoAction() alone
 *   would not report it.
 *
 * On EdtAutoAction::Rearm the caller also clears the EDT arrival times, so
 * the last EDT frames of the ESC that left do not suppress the enable for
 * the next one.
 *
 * @param e        State to update.
 * @param linkUp   An eRPM frame arrived within @ref ESC_LINK_STALE_MS.
 * @param edtFresh An EDT frame arrived within @ref EDT_STALE_MS.
 * @param armed    Arm state.
 * @param ready    The motor is stopped (eRPM 0) and no beacon command is
 *                 going out.
 * @param nowMs    millis().
 * @param settleMs @ref EDT_SETTLE_MS.
 * @param retryMs  @ref EDT_RETRY_MS.
 * @param reps     Frames per enable, @ref EDT_ENABLE_REPEATS.
 * @return What edtAutoAction() decided, or Rearm after the bind burst.
 */
static inline EdtAutoAction escEdtStep(EscEdtState *e, bool linkUp, bool edtFresh,
                                       bool armed, bool ready, uint32_t nowMs,
                                       uint32_t settleMs, uint32_t retryMs,
                                       uint8_t reps) {
	// Zero means "not answering"; `nowMs ? nowMs : 1` keeps that sentinel
	// distinct from a millis() of 0 at boot.
	if (!linkUp)                 e->linkUpSinceMs = 0;
	else if (!e->linkUpSinceMs)  e->linkUpSinceMs = nowMs ? nowMs : 1;
	bool settled = e->linkUpSinceMs &&
	               (uint32_t)(nowMs - e->linkUpSinceMs) >= settleMs;
	bool canCommand = settled && !armed && ready && e->reps == 0;

	if (armed != e->armedWas) {
		e->armedWas = armed;
		e->tried = false;
		e->reps = 0;
	}

	EdtAutoAction a = edtAutoAction(linkUp, edtFresh, canCommand, e->tried,
	                                nowMs - e->lastTryMs, retryMs);
	if (a == EdtAutoAction::None && !linkUp && e->sent && e->reps == 0)
		a = EdtAutoAction::Rearm;

	if (a == EdtAutoAction::Send) {
		// Only the burst is queued here. `tried` and the retry clock are set
		// by escEdtFrameSent() when the last repetition leaves.
		e->reps      = reps;
		e->scheduled = true;
		e->sent      = true;
	} else if (a == EdtAutoAction::Rearm) {
		e->tried = false;
		e->sent  = false;
		e->reps  = 0;
	}
	return a;
}

/**
 * @brief One enable frame has gone out. Pure.
 *
 * A scheduled burst is stamped by its last frame, so a burst cut short by an
 * arm never holds the retry off. The bind burst stamps nothing.
 *
 * @pre @c e->reps is not 0.
 * @param e     State to update.
 * @param nowMs millis() of the frame.
 */
static inline void escEdtFrameSent(EscEdtState *e, uint32_t nowMs) {
	if (--e->reps == 0 && e->scheduled) {
		e->tried     = true;
		e->lastTryMs = nowMs;
	}
}

/** @brief What core1 should put on the wire in one frame slot. */
struct EscFrame {
	bool     sendCommand;  /**< Send @ref command instead of a throttle value. */
	uint8_t  command;      /**< DShot command, meaningful when @ref sendCommand. */
	uint16_t throttle;     /**< Throttle to send, when not sending a command. */
	bool     requestKiss;  /**< Set the frame's telemetry-request bit. */
};

/**
 * @brief Decide what one DShot frame carries. Pure; host-testable.
 *
 * Extracted for the same reason edtAutoAction() was: esc_task.cpp links the
 * PIO driver and the KISS receiver and cannot be linked into the host suite, so
 * any rule left inside it is a rule no test can reach. Three of the rules here
 * are safety interlocks, and all three were untested — a mutation deleting the
 * heartbeat check, the disarm-zeroing, or the refusal to send commands while
 * armed passed the entire suite.
 *
 * The rules, in the order they matter:
 *
 * - **A dead UI means zero throttle.** If core0 has stopped calling
 *   escHeartbeat(), core1 stops believing the throttle it was last given. This
 *   is the backstop against a hung display leaving a motor running, and it is
 *   the only one that does not depend on core0 being well enough to act.
 * - **Disarmed means zero throttle**, regardless of what was last commanded.
 * - **Commands only go out while disarmed.** An ESC ignores them while armed
 *   anyway, so sending one is at best noise in the frame stream; and a queued
 *   command must not be consumed by a frame that cannot deliver it, or it is
 *   silently lost.
 *
 * @param armed       Arm state.
 * @param uiAlive     Core0 has checked in within @ref UI_HEARTBEAT_TIMEOUT_MS.
 * @param throttle    Throttle core0 last commanded, 0..2000.
 * @param pendingCmd  Queued DShot command.
 * @param pendingReps Repeats still owed for @p pendingCmd; 0 means none queued.
 * @param kissWanted  This slot is due to request KISS telemetry.
 * @return What to send.
 */
static inline EscFrame escFrameAction(bool armed, bool uiAlive, uint16_t throttle,
                                      uint8_t pendingCmd, uint8_t pendingReps,
                                      bool kissWanted) {
	EscFrame f = {false, 0, 0, false};
	if (pendingReps > 0 && !armed) {
		f.sendCommand = true;
		f.command = pendingCmd;
		return f;
	}
	f.throttle = (!armed || !uiAlive) ? 0 : throttle;
	// Never alongside a command: a command frame carries the request bit set
	// anyway, and a reply arriving mid-sequence is noise the decoder cannot
	// distinguish from a real one.
	f.requestKiss = kissWanted && pendingReps == 0;
	return f;
}

/**
 * @brief Core1's command state between frame slots: the EDT enable, the
 *        beacon and the KISS request cadence.
 *
 * Written only by escPumpBind(), escPumpSlot() and escPumpFrameSent(), which
 * are pure. escTaskPoll() keeps one of these and does the I/O around them;
 * the host suite drives the same three functions against a model ESC.
 */
struct EscPump {
	EscEdtState edt;           /**< The EDT enable. @see escEdtStep() */
	uint8_t     beepCmd;       /**< Latched beacon command. */
	uint8_t     beepReps;      /**< Beacon repeats still owed; 0 means none queued. */
	uint16_t    kissCountdown; /**< Frames left before the next KISS request. */
};

/** @brief What escPumpSlot() decided for one frame slot. */
struct EscSlot {
	EscFrame frame;     /**< What to put on the wire. */
	bool     forgetEdt; /**< The ESC is gone: clear the EDT arrival times. @see escEdtForget() */
	bool     beepTaken; /**< The beacon request was latched: clear it. */
};

/**
 * @brief A new bind: the EDT enable first, no beacon owed, and the first
 *        KISS request on the first free frame. Pure.
 *
 * Called once per bind, before the first frame. @see escEdtBind()
 *
 * @param p State to reset.
 */
static inline void escPumpBind(EscPump *p) {
	escEdtBind(&p->edt, EDT_ENABLE_REPEATS);
	p->beepReps = 0;
	p->kissCountdown = 0;
}

/**
 * @brief Whether core1 reads the reply to the last frame with EDT decoding
 *        on. Pure.
 *
 * @ref ESC_EDT_DECODE, whatever the state of @p p: every reply is read with
 * EDT decoding on, including the first after a bind and the first after a
 * loss of the link.
 *
 * @param p Pump state. Not read.
 * @return  True.
 */
static inline bool escPumpDecodeEdt(const EscPump *p) {
	(void)p;
	return ESC_EDT_DECODE;
}

/**
 * @brief Clear the EDT arrival times of an ESC that has gone. Pure.
 *
 * Whatever replaces it is a different ESC and starts from nothing having been
 * sent to it, including the timestamps, or the old one's last second of
 * telemetry would make the new one look like it had EDT on and suppress the
 * enable. Core1 calls it under the lock when escPumpSlot() sets
 * EscSlot::forgetEdt.
 *
 * @param tel Block to update.
 */
static inline void escEdtForget(EscTelemetry *tel) {
	tel->edtVoltsMs = tel->edtAmpsMs = tel->edtTempMs = 0;
	tel->edtStressMs = tel->edtStatusMs = 0;
}

/**
 * @brief Decide one frame slot. Pure.
 *
 * In order:
 *
 * 1. The EDT enable, escEdtStep(). The link is up while an eRPM frame is
 *    younger than @ref ESC_LINK_STALE_MS, and EDT is fresh while any EDT
 *    frame is younger than @ref EDT_STALE_MS. That is the fact the header
 *    chip shows, so the retry stops when the display says EDT ON. The ESC is
 *    ready for a command when the motor is stopped (eRPM 0) and no beacon is
 *    going out. DShot commands execute only with the motor stopped, and a
 *    disarm leaves a prop coasting for seconds.
 * 2. The beacon. A request is latched only when the wire is free, no enable
 *    and no beacon going out. EDT goes first: it is the one the user cannot
 *    re-request, and a beep deferred by ten frames is a beep nobody notices
 *    was deferred.
 * 3. The KISS request. Every @ref KISS_REQUEST_EVERY_N frames while the
 *    receiver runs and no command is owed: a command frame goes out with the
 *    telemetry bit set anyway, and a reply arriving mid-command sequence is
 *    noise.
 * 4. The frame, escFrameAction(), with the enable ahead of the beacon.
 *
 * Every telemetry field read here is written by core1, so core1 reads them
 * without the lock.
 *
 * @param p        State to update.
 * @param tel      Telemetry block, after this slot's reply.
 * @param armed    Arm state.
 * @param uiAlive  Core0 has checked in within @ref UI_HEARTBEAT_TIMEOUT_MS.
 * @param throttle Throttle core0 last commanded, 0..2000.
 * @param beepReq  Beacon 1..5 core0 asked for; 0 is none.
 * @param kissRx   The KISS receiver is running.
 * @param nowMs    millis().
 * @return What to send, and what to clear.
 */
static inline EscSlot escPumpSlot(EscPump *p, const EscTelemetry *tel,
                                  bool armed, bool uiAlive, uint16_t throttle,
                                  uint8_t beepReq, bool kissRx, uint32_t nowMs) {
	EscSlot s = {{false, 0, 0, false}, false, false};

	bool linkUp = tel->lastRpmMs != 0 &&
	              (uint32_t)(nowMs - tel->lastRpmMs) < ESC_LINK_STALE_MS;
	const uint32_t edtMs[] = {tel->edtVoltsMs, tel->edtAmpsMs, tel->edtTempMs,
	                          tel->edtStressMs, tel->edtStatusMs};
	bool edtFresh = false;
	for (uint32_t t : edtMs)
		if (t != 0 && (uint32_t)(nowMs - t) < EDT_STALE_MS) edtFresh = true;
	bool ready = tel->erpm == 0 && p->beepReps == 0;

	s.forgetEdt = escEdtStep(&p->edt, linkUp, edtFresh, armed, ready, nowMs,
	                         EDT_SETTLE_MS, EDT_RETRY_MS,
	                         EDT_ENABLE_REPEATS) == EdtAutoAction::Rearm;

	if (!p->edt.reps && !p->beepReps && beepReq) {
		p->beepCmd  = (uint8_t)(DSHOT_CMD_BEEP1 + (beepReq - 1));
		p->beepReps = BEEP_REPEATS;
		s.beepTaken = true;
	}

	uint8_t pendingCmd  = p->edt.reps ? (uint8_t)DSHOT_CMD_EDT_ENABLE : p->beepCmd;
	uint8_t pendingReps = p->edt.reps ? p->edt.reps : p->beepReps;

	bool wantKiss = false;
	if (kissRx && pendingReps == 0) {
		if (p->kissCountdown) {
			p->kissCountdown--;
		} else {
			wantKiss = true;
			p->kissCountdown = KISS_REQUEST_EVERY_N - 1;
		}
	}

	s.frame = escFrameAction(armed, uiAlive, throttle, pendingCmd, pendingReps,
	                         wantKiss);
	return s;
}

/**
 * @brief The frame escPumpSlot() decided has gone out. Pure.
 *
 * A command frame counts against the enable while one is owed, otherwise
 * against the beacon. @see escEdtFrameSent()
 *
 * @param p     State to update.
 * @param f     The frame, EscSlot::frame.
 * @param nowMs millis() after the frame went out.
 */
static inline void escPumpFrameSent(EscPump *p, const EscFrame *f, uint32_t nowMs) {
	if (!f->sendCommand) return;
	if (p->edt.reps)          escEdtFrameSent(&p->edt, nowMs);
	else if (p->beepReps)     p->beepReps--;
}

/** @brief EDT status frame, bits 3..0: the highest stress level seen. */
#define ESC_STATUS_MAX_STRESS_MASK 0x0Fu
/** @brief EDT status frame, bit 5: error. */
#define ESC_STATUS_ERROR_MASK      0x20u
/** @brief EDT status frame, bit 6: warning. */
#define ESC_STATUS_WARNING_MASK    0x40u
/** @brief EDT status frame, bit 7: alert. */
#define ESC_STATUS_ALERT_MASK      0x80u

/**
 * @brief Fold one poll of the DShot driver into a telemetry block. Pure;
 *        host-testable.
 *
 * Split out of esc_task.cpp for the same reason as escFrameAction(): the
 * counters behind the link-quality figure are only as good as this mapping,
 * and the file that calls it links the PIO driver.
 *
 * - No reply counts in `noPackets`. An ESC that is not connected, or does not
 *   speak bidirectional DShot, produces nothing else.
 * - A damaged reply (line code, group code or checksum) counts in
 *   `badPackets`, and so does an eRPM frame with a period of zero. A period of
 *   zero has no speed. An ESC reports a stopped motor with its own code,
 *   which decodes to an eRPM of 0 with a non-zero period.
 * - Every other decoded frame counts in `goodPackets` and stamps its arrival
 *   time. The two debug types are counted and not shown.
 *
 * Voltage arrives in 250 mV steps, current in 1 A steps and temperature in
 * whole degrees Celsius, per the EDT specification.
 *
 * @param tel   Block to update.
 * @param reply What out_dshot_poll() returned.
 * @param t     The decoded frame. Read only when @p reply is DSHOT_REPLY_GOOD.
 * @param poles Motor magnet count, for the eRPM to RPM conversion.
 * @param nowMs millis() to stamp an arrival with.
 */
static inline void escFoldReply(EscTelemetry *tel, dshot_reply_t reply,
                                const dshot_telem_t *t, uint8_t poles,
                                uint32_t nowMs) {
	if (reply == DSHOT_REPLY_NONE) {
		tel->noPackets++;
		return;
	}
	if (reply != DSHOT_REPLY_GOOD ||
	    (t->kind == DSHOT_TELEM_ERPM && t->period_us == 0)) {
		tel->badPackets++;
		return;
	}
	switch (t->kind) {
		case DSHOT_TELEM_ERPM:
			tel->erpm = t->erpm;
			tel->rpm  = (poles >= 2) ? (t->erpm / (poles / 2)) : t->erpm;
			tel->lastRpmMs = nowMs;
			break;
		case DSHOT_TELEM_VOLTAGE:
			tel->volts = t->value * 0.25f;
			tel->edtVoltsMs = nowMs;
			break;
		case DSHOT_TELEM_CURRENT:
			tel->amps = (float)t->value;
			tel->edtAmpsMs = nowMs;
			break;
		case DSHOT_TELEM_TEMPERATURE:
			tel->tempC = (int16_t)t->value;
			tel->edtTempMs = nowMs;
			break;
		case DSHOT_TELEM_STRESS:
			tel->stress = t->value;
			tel->edtStressMs = nowMs;
			break;
		case DSHOT_TELEM_STATUS:
			tel->statusRaw = t->value;
			tel->maxStress = (uint8_t)(t->value & ESC_STATUS_MAX_STRESS_MASK);
			tel->error     = (t->value & ESC_STATUS_ERROR_MASK)   != 0;
			tel->warning   = (t->value & ESC_STATUS_WARNING_MASK) != 0;
			tel->alert     = (t->value & ESC_STATUS_ALERT_MASK)   != 0;
			tel->edtStatusMs = nowMs;
			break;
		default:
			break;
	}
	tel->goodPackets++;
}

/**
 * @brief Copy the current telemetry block.
 * @param[out] out Destination. Filled under the critical section.
 */
void escSnapshot(EscTelemetry *out);

/**
 * @brief True while an EDT enable has gone out to the ESC on the pin.
 *
 * Set by the burst every bind sends and by each scheduled enable; cleared
 * when the link is lost. @see EscEdtState
 * @return Request state.
 */
bool escEdtRequested();

/**
 * @brief Ask core1 to tear down the DShot driver and release the signal pin.
 *
 * Needed before the AM32 bootloader transport can use the same pin. Forces a
 * disarm first. Poll escTaskSuspended() until it reports true; the teardown
 * happens on core1 so that the PIO state machines are freed by the core that
 * claimed them.
 */
void escTaskSuspend();

/** @brief Ask core1 to rebuild the DShot driver. Starts disarmed. */
void escTaskResume();

/** @brief True once the driver is torn down and the pin is free. */
bool escTaskSuspended();

/**
 * @brief Set the pins and bitrate the DShot pump uses, and rebuild it.
 *
 * Core0 hands the values across rather than core1 reading the settings
 * struct directly: core0 owns that struct and edits it a field at a time while the
 * setup screen is open, so core1 reading it would see half-applied
 * combinations — a new pin against an old bitrate, for one frame.
 *
 * Takes effect on the next escTaskPoll(): the driver releases the old pin and
 * binds the new one. Forces a disarm, because
 * the ESC on the old pin stops hearing frames the moment it is released.
 *
 * Safe to call with unchanged values; it is a no-op if nothing differs, so the
 * setup screen can call it freely rather than tracking what it changed.
 *
 * @param dshotPin   GPIO for the ESC signal wire.
 * @param dshotKbaud DShot bitrate.
 * @param kissEnable Claim a UART for KISS telemetry.
 * @param kissPin    GPIO for the telemetry wire. Ignored unless @p kissEnable.
 */
void escTaskConfigure(uint8_t dshotPin, uint16_t dshotKbaud,
                      bool kissEnable, uint8_t kissPin);

/** @brief GPIO the pump is currently driving. @return The live ESC pin. */
uint8_t escTaskDshotPin();

/**
 * @brief DShot bitrate the pump is currently using.
 * @return 150, 300, 600 or 1200, in kbit/s: the value escTaskConfigure() last
 *         set, which the pump binds at on its next escTaskPoll().
 */
uint16_t escTaskDshotKbaud();

/** @} */
