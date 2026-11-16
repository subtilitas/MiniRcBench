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
 * Core0 commands it with single volatile writes, escSetThrottle() and
 * escSetArmed(), and reads it back with escSnapshot(), which copies the
 * telemetry under the shared critical section. escRequestCommand(),
 * escCancelCommand() and escCommandState() take that section too.
 *
 * @warning Core0 must call escHeartbeat() regularly. If it stops for longer
 *          than @ref UI_HEARTBEAT_TIMEOUT_MS, core1 assumes the UI has died and
 *          forces the throttle to zero on its own.
 */

#pragma once

#include <stdint.h>

#include <hardware/gpio.h>

#include "config.h"
#include "dshot/dshot.h"
#include "kiss_telem.h"

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
	 * shows `DSERR` in place of the bitrate. A teardown clears it, so it
	 * names the current bind only. @see escTelTeardown()
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
 * @brief Safe to call from core0. The commands are single volatile writes;
 *        the command requests and escSnapshot() take the shared critical
 *        section.
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
 * @brief millis() of the last escHeartbeat(). Read by core1.
 * @return The stamp escUiAlive() is judged against.
 */
uint32_t escHeartbeatMs();

/**
 * @brief Whether core1 would start a command now, as it last published.
 *
 * In the order core1 tests them. @see escCmdReadiness()
 */
enum class EscCmdReady : uint8_t {
	NoAnswer, /**< No eRPM answer younger than @ref ESC_LINK_STALE_MS. */
	Turning,  /**< The last eRPM answer is not 0. */
	Settling, /**< The ESC has answered for less than @ref ESC_CMD_SETTLE_MS. */
	Busy,     /**< An EDT enable, a command sequence or its gap goes out. */
	Ready,    /**< A request starts on the next frame. */
};

/** @brief Where the last command sequence stands. @see escCommandState() */
enum class EscCmdState : uint8_t {
	Idle,     /**< No sequence since the bench started. */
	Waiting,  /**< A request waits for the pump. */
	Sending,  /**< The sequence goes out. */
	Sent,     /**< Every block went out, each checked block answered. */
	NotSent,  /**< Stopped before frame 6 of the first block went out. */
	SaveLost, /**< Stopped after it: the change may sit in the ESC's RAM. */
	Expired,  /**< The request never started: not ready within the wait, or an arm. */
};

/**
 * @brief Whether ESC command @p cmd with @p save is one the command slot
 *        sends. Pure.
 *
 * Beacons 1 to 5 and command 12, the save, go alone. Commands 7 to 10,
 * direction and 3D mode, go only with the save after them: they change the
 * ESC's RAM, and without 12 the change is gone at the ESC's next power cycle
 * (decision 2 of `docs/design/esc-commands.md`). 6 is left out (decision 8),
 * 13 and 14 are the EDT enable's own, and the rest are not offered.
 *
 * @param cmd  DShot command.
 * @param save Send 12 after it.
 * @return     True for a sequence escPumpSlot() can run.
 */
static inline bool escCmdValid(uint8_t cmd, bool save) {
	if (cmd >= DSHOT_CMD_BEEP1 && cmd <= DSHOT_CMD_BEEP5) return !save;
	if (cmd >= DSHOT_CMD_SPIN_DIRECTION_1 && cmd <= DSHOT_CMD_3D_MODE_ON) return save;
	return cmd == DSHOT_CMD_SAVE_SETTINGS && !save;
}

/**
 * @brief Whether escRequestCommand() takes a request. Pure.
 *
 * Refused while armed, at EscCmdReady::NoAnswer, Turning and Settling, and
 * for a sequence escCmdValid() refuses. At Busy the request waits, at most
 * @ref ESC_CMD_WAIT_MS.
 *
 * @param cmd   DShot command.
 * @param save  Send 12 after it.
 * @param armed Arm state.
 * @param ready escCommandReady().
 * @return      True when the request is to be handed to core1.
 */
static inline bool escCmdAccept(uint8_t cmd, bool save, bool armed,
                                EscCmdReady ready) {
	return escCmdValid(cmd, save) && !armed &&
	       (ready == EscCmdReady::Busy || ready == EscCmdReady::Ready);
}

/**
 * @brief A command request, as core0 hands it to core1.
 *
 * Under the shared critical section in esc_task.cpp. A @c cmd of 0 is no
 * request.
 */
struct EscCmdRequest {
	uint8_t  cmd;   /**< DShot command; 0 none. */
	bool     save;  /**< Send 12 after it. */
	uint32_t atMs;  /**< millis() of the request; the wait counts from it. */
};

/**
 * @brief Request an ESC command from core1, with the save after it.
 *
 * escCmdAccept() decides, on the arm state and escCommandReady(). Core1
 * starts the sequence on the first frame its own readiness reads
 * EscCmdReady::Ready, and drops a request it could not start within
 * @ref ESC_CMD_WAIT_MS. A request replaces one that still waits.
 *
 * @param cmd  1 to 5 (a beacon), 12, or 7 to 10 with @p save.
 * @param save Send 12 after @p cmd.
 * @return     True if the request was taken. The caller says so either way:
 *             a command silently dropped is a button that does not work.
 */
bool escRequestCommand(uint8_t cmd, bool save);

/**
 * @brief Drop a request that still waits. A sequence already going out goes
 *        on.
 */
void escCancelCommand();

/**
 * @brief Whether core1 would start a command now.
 * @return What core1 published on its last frame slot; EscCmdReady::NoAnswer
 *         before the first and while the pump is torn down.
 */
EscCmdReady escCommandReady();

/**
 * @brief Where the last command sequence stands.
 * @param[out] cmd The sequence's command, or the waiting request's; may be
 *                 null.
 * @return EscCmdState::Waiting while a request waits, else the state core1
 *         published for the last sequence.
 */
EscCmdState escCommandState(uint8_t *cmd);

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
 *   SETTINGS change, or an AM32 settings session sends EDT frames from its
 *   first reply. Read as eRPM, a voltage frame of 66 counts (16.5 V) is a
 *   period of 264 us: a false reading of 227,272 eRPM, 32,467 RPM at
 *   14 poles.
 * - An ESC without EDT, or with EDT off, sends eRPM frames only. AM32 and
 *   Bluejay encode every eRPM frame normalised, as the EDT specification
 *   describes, with EDT on or off: bit 8 of the payload is set whenever the
 *   exponent is not 0 (AM32 `Src/dshot.c`, make_dshot_package(), at commit
 *   961cb84; Bluejay `src/Modules/DShot.asm`, dshot_12bit_encode, at commit
 *   0368d11). Such a frame reads as eRPM with EDT decoding on, and a frame
 *   with exponent 0 has type nibble 0, which reads as eRPM too. The host
 *   suite checks every normalised payload. Whether BLHeli_32 and other ESC
 *   firmware normalise with EDT off is unknown. rcbench, where the DShot
 *   driver comes from, has been tested with AM32 only.
 *
 * Only an eRPM frame with an exponent above 0 and bit 8 clear reads
 * differently, as an EDT type: from an ESC that does not normalise, such a
 * frame updates an EDT field and not the RPM.
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
 * abandons the burst. A command request and the first KISS request wait for
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
 * @param ready    The motor is stopped (eRPM 0) and no command sequence or
 *                 its gap is going out.
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
 *   command must not be consumed by a frame that cannot deliver it. A command
 *   still owed at an arm is dropped by its owner instead: escEdtStep() for the
 *   EDT enable, escPumpSlot() for a command sequence.
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
 * @brief The 11-bit DShot value and the telemetry bit frame @p f goes out
 *        with. Pure.
 *
 * A command frame has the telemetry bit set. Bluejay and BLHeli_S clear a
 * command, and its count, on a command frame with the bit clear, so a
 * command sent without it never reaches their repeat count; AM32 acts on
 * either. A throttle frame carries the bit only when it asks for a KISS
 * reply. @see kissBuildDshotPayload()
 *
 * @param f              The frame.
 * @param[out] telemetry The telemetry bit.
 * @return               The value, 0 to 2047.
 */
static inline uint16_t escFrameWire(const EscFrame *f, bool *telemetry) {
	if (f->sendCommand) {
		*telemetry = true;
		return f->command;
	}
	uint16_t payload = kissBuildDshotPayload(f->throttle, f->requestKiss);
	*telemetry = (payload & 1u) != 0;
	return (uint16_t)(payload >> 1);
}

/**
 * @brief Core1's command sequence: a command, its repeats, and the save after
 *        it.
 *
 * A sequence is one block, or two when it saves. A block is its command's
 * repeats, @ref BEEP_REPEATS frames for a beacon and `DSHOT_CMD_REPEATS`, 10,
 * for 7 to 10 and 12, then its gap of value 0 frames, @ref ESC_CMD_BEEP_GAP_MS
 * or @ref ESC_CMD_GAP_MS. With @c save a block of 12 follows the first gap.
 * A hold for direction or 3D therefore sends 10 frames of the command, 300 of
 * 0, 10 of 12 and 300 of 0: 620 ms. The design and its sources are in
 * `docs/design/esc-commands.md`.
 *
 * Written only by escCmdStart(), escCmdCheckReply(), escCmdFrameSent(),
 * escCmdInterrupt() and escCmdExpire(), which are pure.
 */
struct EscCmdSeq {
	uint8_t     cmd;      /**< The first block's command; 0 while no sequence runs. */
	bool        save;     /**< A block of 12 follows the first. */
	bool        saving;   /**< The block going out is that one. */
	bool        stopped;  /**< The block stopped on a missing reply. @see escCmdCheckReply() */
	uint8_t     sent;     /**< Frames of the block gone out. */
	uint8_t     misses;   /**< Replies missing or bad among its frames 1 to 5. */
	uint16_t    gapLeft;  /**< Frames of value 0 still owed after the block. */
	EscCmdState state;    /**< Sending while it runs, then its outcome. */
	uint8_t     stateCmd; /**< The command @ref state is about. */
};

/**
 * @brief Frames of a block of 7 to 10 or 12 whose replies are checked.
 *
 * AM32 acts on the sixth equal frame and Bluejay and BLHeli_S on the seventh,
 * so no ESC has acted before the sixth has gone out. Replies after it are not
 * read: AM32 writes its flash while it decodes the save's sixth frame, and
 * Bluejay may stop answering while it writes from the seventh.
 */
static constexpr uint8_t ESC_CMD_CHECKED_FRAMES = 6;

/** @brief Replies to frames 1 to 5 of a block that may be missing or bad. */
static constexpr uint8_t ESC_CMD_MISSES_ALLOWED = 1;

/** @brief Whether @p cmd, 1 or above, is a beacon. Pure. */
static inline bool escCmdIsBeacon(uint8_t cmd) { return cmd <= DSHOT_CMD_BEEP5; }

/** @brief The command of the block going out. Pure. */
static inline uint8_t escCmdBlockCmd(const EscCmdSeq *s) {
	return s->saving ? (uint8_t)DSHOT_CMD_SAVE_SETTINGS : s->cmd;
}

/** @brief Frames of a block of @p cmd. Pure. */
static inline uint8_t escCmdReps(uint8_t cmd) {
	return escCmdIsBeacon(cmd) ? (uint8_t)BEEP_REPEATS : (uint8_t)DSHOT_CMD_REPEATS;
}

/** @brief Frames of value 0 after a block of @p cmd. Pure. */
static inline uint16_t escCmdGapFrames(uint8_t cmd) {
	static const uint16_t beacon[5] = ESC_CMD_BEEP_GAP_MS;
	uint32_t ms = escCmdIsBeacon(cmd) ? beacon[cmd - 1] : ESC_CMD_GAP_MS;
	return (uint16_t)(ms * 1000u / DSHOT_PERIOD_US);
}

/** @brief A block's command frames are still going out. Pure. */
static inline bool escCmdInBlock(const EscCmdSeq *s) {
	return s->cmd && !s->stopped && s->sent < escCmdReps(escCmdBlockCmd(s));
}

/**
 * @brief The outcome of a sequence that stops where it stands. Pure.
 *
 * A finished outcome stays: a beacon is EscCmdState::Sent from its first
 * frame, every open firmware acting on it. Otherwise NotSent while frame 6 of
 * the first block has not gone out, as no ESC has acted; SaveLost after it,
 * as AM32 may have acted, and before the save block's frame 6 its save
 * cannot be known.
 */
static inline EscCmdState escCmdStopState(const EscCmdSeq *s) {
	if (s->state != EscCmdState::Sending) return s->state;
	return !s->saving && s->sent < ESC_CMD_CHECKED_FRAMES ? EscCmdState::NotSent
	                                                      : EscCmdState::SaveLost;
}

/**
 * @brief Start a sequence. Pure.
 * @pre escCmdValid(@p cmd, @p save).
 */
static inline void escCmdStart(EscCmdSeq *s, uint8_t cmd, bool save) {
	s->cmd      = cmd;
	s->save     = save;
	s->saving   = false;
	s->stopped  = false;
	s->sent     = 0;
	s->misses   = 0;
	s->gapLeft  = 0;
	s->state    = EscCmdState::Sending;
	s->stateCmd = cmd;
}

/**
 * @brief Drop the sequence at once, with no gap: an arm, a bind, a teardown,
 *        or a block a drain frame broke. Pure.
 */
static inline void escCmdInterrupt(EscCmdSeq *s) {
	if (!s->cmd) return;
	s->state = escCmdStopState(s);
	s->cmd = 0;
}

/**
 * @brief Check the reply to the block's last frame. Pure.
 *
 * Called on each slot before the next frame, with what out_dshot_poll()
 * returned for the frame before. A reply is DSHOT_REPLY_GOOD, an eRPM or an
 * EDT frame alike. Frames 1 to 6 of a block of 7 to 10 or 12 need one each;
 * one reply to frames 1 to 5 may be missing or bad, frame 6's may not. A
 * missing reply beyond that stops the block at once: no command frame more,
 * the block's gap of 0 frames, and the sequence ends after it with
 * escCmdStopState(). A beacon's block is not checked.
 */
static inline void escCmdCheckReply(EscCmdSeq *s, dshot_reply_t reply) {
	if (!escCmdInBlock(s) || escCmdIsBeacon(s->cmd)) return;
	if (s->sent == 0 || s->sent > ESC_CMD_CHECKED_FRAMES) return;
	if (reply == DSHOT_REPLY_GOOD) return;
	if (s->sent < ESC_CMD_CHECKED_FRAMES && ++s->misses <= ESC_CMD_MISSES_ALLOWED)
		return;
	s->state   = escCmdStopState(s);
	s->stopped = true;
	s->gapLeft = escCmdGapFrames(escCmdBlockCmd(s));
}

/**
 * @brief Count one frame against the sequence. Pure.
 *
 * In a block, a command frame counts against it; any other frame, which only
 * a drain sends there, breaks it, and escCmdInterrupt() ends it. A beacon is
 * Sent from its first frame; a sequence of 7 to 10 or 12 when its last block's
 * last frame has gone out. In a gap, every frame counts against it, and its
 * end starts the save block or ends the sequence.
 *
 * @pre The frame is not an EDT enable frame. @see escPumpFrameSent()
 */
static inline void escCmdFrameSent(EscCmdSeq *s, const EscFrame *f) {
	if (!s->cmd) return;
	if (escCmdInBlock(s)) {
		if (!f->sendCommand) {
			escCmdInterrupt(s);
			return;
		}
		if (escCmdIsBeacon(s->cmd)) s->state = EscCmdState::Sent;
		if (++s->sent < escCmdReps(escCmdBlockCmd(s))) return;
		s->gapLeft = escCmdGapFrames(escCmdBlockCmd(s));
		if (s->saving || !s->save) s->state = EscCmdState::Sent;
		return;
	}
	if (--s->gapLeft) return;
	if (s->save && !s->saving && !s->stopped) {
		s->saving = true;
		s->sent   = 0;
		s->misses = 0;
	} else {
		s->cmd = 0;
	}
}

/**
 * @brief Whether a command would start now. Pure.
 *
 * In order: no eRPM answer younger than @ref ESC_LINK_STALE_MS; the last
 * eRPM answer not 0, a motor turning or coasting; the ESC answering for less
 * than @ref ESC_CMD_SETTLE_MS, as AM32 takes commands only after more than
 * 1 s of zero input; an EDT enable, a sequence or its gap going out.
 *
 * @param linkUp      An eRPM frame arrived within @ref ESC_LINK_STALE_MS.
 * @param erpm        The last eRPM answer.
 * @param linkUpSince EscEdtState::linkUpSinceMs; 0 while not answering.
 * @param nowMs       millis().
 * @param busy        An EDT enable, a sequence or its gap goes out.
 * @return            The readiness.
 */
static inline EscCmdReady escCmdReadiness(bool linkUp, uint32_t erpm,
                                          uint32_t linkUpSince, uint32_t nowMs,
                                          bool busy) {
	if (!linkUp) return EscCmdReady::NoAnswer;
	if (erpm != 0) return EscCmdReady::Turning;
	if (!linkUpSince || (int32_t)(nowMs - linkUpSince) < (int32_t)ESC_CMD_SETTLE_MS)
		return EscCmdReady::Settling;
	if (busy) return EscCmdReady::Busy;
	return EscCmdReady::Ready;
}

/**
 * @brief Core1's command state between frame slots: the EDT enable, the
 *        command sequence and the KISS request cadence.
 *
 * Written only by escPumpBind(), escPumpRelease(), escPumpTeardown(),
 * escPumpSlot() and escPumpFrameSent(), which are pure. escTaskPoll() keeps
 * one of these and does the I/O around them; the host suite drives the same
 * functions against model ESCs.
 */
struct EscPump {
	EscEdtState edt;           /**< The EDT enable. @see escEdtStep() */
	EscCmdSeq   seq;           /**< The command sequence. @see EscCmdSeq */
	EscCmdReady ready;         /**< Readiness after the last slot; escCommandReady(). */
	uint16_t    kissCountdown; /**< Frames left before the next KISS request. */
	/** escThrottleStamp() of the last frame; 0 for none since the bind. */
	uint32_t    lastThrottleMs;
};

/** @brief What escPumpSlot() decided for one frame slot. */
struct EscSlot {
	EscFrame frame;     /**< What to put on the wire. */
	bool     forgetEdt; /**< The ESC is gone: clear the EDT arrival times. @see escEdtForget() */
	bool     cmdTaken;  /**< The request started, expired or was dropped by an arm: clear it. */
};

/**
 * @brief The pump lets go of the pin: a sequence going out stops where it
 *        stands, and nothing is ready. Pure.
 *
 * escPumpTeardown() calls it at a teardown, and escPumpBind() at every
 * bind. @see escCmdInterrupt()
 *
 * @param p State to update.
 */
static inline void escPumpRelease(EscPump *p) {
	escCmdInterrupt(&p->seq);
	p->ready = EscCmdReady::NoAnswer;
}

/**
 * @brief A request ends unstarted: the outcome reads EscCmdState::Expired,
 *        about @p cmd. Pure.
 * @param s   The sequence whose outcome core0 reads.
 * @param cmd The request's command.
 */
static inline void escCmdExpire(EscCmdSeq *s, uint8_t cmd) {
	s->state    = EscCmdState::Expired;
	s->stateCmd = cmd;
}

/**
 * @brief A teardown: the pump lets go of the pin, and a request that still
 *        waits goes with it. Pure.
 *
 * escPumpRelease(), then the request: left waiting, it would start on the
 * next bind's ESC once that settled within @ref ESC_CMD_WAIT_MS, on whatever
 * screen is open by then. It is dropped either way. With no sequence
 * running it reads EscCmdState::Expired about its command; a sequence the
 * teardown cut keeps its outcome, which says more about the ESC than the
 * request that waited behind it: NotSent or SaveLost in a block or its
 * first gap, and Sent in the last gap, which still counts as running.
 *
 * @param p   State to update.
 * @param req The request core0 made; @c cmd 0 for none.
 * @return True when a request was dropped: clear it.
 */
static inline bool escPumpTeardown(EscPump *p, const EscCmdRequest *req) {
	bool running = p->seq.cmd != 0;
	escPumpRelease(p);
	if (!req->cmd) return false;
	if (!running) escCmdExpire(&p->seq, req->cmd);
	return true;
}

/**
 * @brief A new bind: disarmed at throttle zero, the EDT enable first, no
 *        command sequence, no throttle frame sent, and the first KISS request
 *        on the first free frame. Pure.
 *
 * Called once per bind, before the first frame. A bind is a new ESC as far
 * as the pump is concerned: after a SETTINGS change, and after a pin claim. It
 * starts disarmed whatever core0 last commanded, so arming again takes the
 * full hold on the tester screen. @see escEdtBind(), escPumpRelease()
 *
 * @param p        State to reset.
 * @param armed    Core0's arm command, cleared.
 * @param throttle Core0's throttle command, zeroed.
 */
static inline void escPumpBind(EscPump *p, volatile bool *armed,
                               volatile uint16_t *throttle) {
	*armed = false;
	*throttle = 0;
	escEdtBind(&p->edt, EDT_ENABLE_REPEATS);
	escPumpRelease(p);
	p->kissCountdown = 0;
	p->lastThrottleMs = 0;
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
 *    ready for the enable when the motor is stopped (eRPM 0) and no command
 *    sequence or its gap is going out, so no scheduled enable lands between
 *    a command and its save. DShot commands execute only with the motor
 *    stopped, and a disarm leaves a prop coasting for seconds.
 * 2. The command sequence. @p reply checks the last frame
 *    (escCmdCheckReply()). An arm drops the sequence and the request: no
 *    command frame goes out armed, so a kept one would hold back every KISS
 *    request until the disarm, and go out late. Disarmed, a request starts
 *    when escCmdReadiness() reads Ready, so the enable goes first, and
 *    expires once it has waited @ref ESC_CMD_WAIT_MS. The wait is a signed
 *    difference, so a request stamped after this slot's clock reads as new.
 * 3. The KISS request. Every @ref KISS_REQUEST_EVERY_N frames while the
 *    receiver runs and no command frame is owed: a command frame goes out
 *    with the telemetry bit set anyway, and a reply arriving mid-block is
 *    noise. A gap's 0 frames carry requests.
 * 4. The frame, escFrameAction(), with the enable ahead of the sequence.
 * 5. EscPump::ready, escCmdReadiness() after the decision.
 *
 * Every telemetry field read here is written by core1, so core1 reads them
 * without the lock.
 *
 * @param p        State to update.
 * @param tel      Telemetry block, after this slot's reply.
 * @param armed    Arm state.
 * @param uiAlive  Core0 has checked in within @ref UI_HEARTBEAT_TIMEOUT_MS.
 * @param throttle Throttle core0 last commanded, 0..2000.
 * @param req      The request core0 made; @c cmd 0 for none.
 * @param reply    What out_dshot_poll() returned for the last frame.
 * @param kissRx   The KISS receiver is running.
 * @param nowMs    millis().
 * @return What to send, and what to clear.
 */
static inline EscSlot escPumpSlot(EscPump *p, const EscTelemetry *tel,
                                  bool armed, bool uiAlive, uint16_t throttle,
                                  const EscCmdRequest *req, dshot_reply_t reply,
                                  bool kissRx, uint32_t nowMs) {
	EscSlot s = {{false, 0, 0, false}, false, false};

	bool linkUp = tel->lastRpmMs != 0 &&
	              (uint32_t)(nowMs - tel->lastRpmMs) < ESC_LINK_STALE_MS;
	const uint32_t edtMs[] = {tel->edtVoltsMs, tel->edtAmpsMs, tel->edtTempMs,
	                          tel->edtStressMs, tel->edtStatusMs};
	bool edtFresh = false;
	for (uint32_t t : edtMs)
		if (t != 0 && (uint32_t)(nowMs - t) < EDT_STALE_MS) edtFresh = true;
	bool ready = tel->erpm == 0 && p->seq.cmd == 0;

	s.forgetEdt = escEdtStep(&p->edt, linkUp, edtFresh, armed, ready, nowMs,
	                         EDT_SETTLE_MS, EDT_RETRY_MS,
	                         EDT_ENABLE_REPEATS) == EdtAutoAction::Rearm;

	escCmdCheckReply(&p->seq, reply);
	if (armed) {
		escCmdInterrupt(&p->seq);
		if (req->cmd) {
			escCmdExpire(&p->seq, req->cmd);
			s.cmdTaken = true;
		}
	} else if (req->cmd) {
		bool busy = p->edt.reps != 0 || p->seq.cmd != 0;
		if (escCmdReadiness(linkUp, tel->erpm, p->edt.linkUpSinceMs, nowMs, busy) ==
		    EscCmdReady::Ready) {
			escCmdStart(&p->seq, req->cmd, req->save);
			s.cmdTaken = true;
		} else if ((int32_t)(nowMs - req->atMs) >= (int32_t)ESC_CMD_WAIT_MS) {
			escCmdExpire(&p->seq, req->cmd);
			s.cmdTaken = true;
		}
	}

	uint8_t pendingCmd  = 0;
	uint8_t pendingReps = 0;
	if (p->edt.reps) {
		pendingCmd  = DSHOT_CMD_EDT_ENABLE;
		pendingReps = p->edt.reps;
	} else if (escCmdInBlock(&p->seq)) {
		pendingCmd  = escCmdBlockCmd(&p->seq);
		pendingReps = (uint8_t)(escCmdReps(pendingCmd) - p->seq.sent);
	}

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
	p->ready = escCmdReadiness(linkUp, tel->erpm, p->edt.linkUpSinceMs, nowMs,
	                           p->edt.reps != 0 || p->seq.cmd != 0);
	return s;
}

/**
 * @brief When the pump last sent a throttle frame, after frame @p f. Pure.
 *
 * A throttle frame is one that carries a throttle above zero. A zero frame
 * and a command frame leave the stamp as it was. 0 means "none since the
 * bind", so a frame at millis() 0 stamps 1.
 *
 * @param f      The frame that went out.
 * @param prevMs The stamp before it.
 * @param nowMs  millis() of the frame.
 * @return       The stamp after it.
 */
static inline uint32_t escThrottleStamp(const EscFrame *f, uint32_t prevMs,
                                        uint32_t nowMs) {
	if (f->sendCommand || f->throttle == 0) return prevMs;
	return nowMs ? nowMs : 1;
}

/**
 * @brief The frame escPumpSlot() or escDrainSlot() decided has gone out. Pure.
 *
 * A throttle frame stamps EscPump::lastThrottleMs, which the drain before a
 * teardown is measured from. A command frame counts against the enable while
 * one is owed; every other frame counts against the command sequence, its
 * block or its gap. @see escEdtFrameSent(), escCmdFrameSent()
 *
 * @param p     State to update.
 * @param f     The frame, EscSlot::frame.
 * @param nowMs millis() after the frame went out.
 */
static inline void escPumpFrameSent(EscPump *p, const EscFrame *f, uint32_t nowMs) {
	p->lastThrottleMs = escThrottleStamp(f, p->lastThrottleMs, nowMs);
	if (f->sendCommand && p->edt.reps) escEdtFrameSent(&p->edt, nowMs);
	else                               escCmdFrameSent(&p->seq, f);
}

/**
 * @brief Whether core0 has checked in within @ref UI_HEARTBEAT_TIMEOUT_MS.
 *        Pure.
 *
 * The one rule for "the UI is alive": the DShot pump zeroes the throttle on
 * it, and every core1 job reads it through core1UiAlive(). The subtractions
 * are unsigned, so the rule holds across the millis() rollover.
 *
 * Both callers read the stamp before millis(), so the clock is never older
 * than the stamp. As a backstop, a heartbeat newer than @p nowMs by less
 * than the timeout counts as alive: a caller that read the clock first, with
 * core0 stamping between the two reads, would otherwise read the heartbeat
 * as 2^32 - 1 ms old.
 *
 * @param nowMs       millis().
 * @param heartbeatMs millis() of the last escHeartbeat().
 * @return            True while the heartbeat is younger than the timeout,
 *                    or newer than @p nowMs by less than it.
 */
static inline bool escUiAlive(uint32_t nowMs, uint32_t heartbeatMs) {
	return (uint32_t)(nowMs - heartbeatMs) < UI_HEARTBEAT_TIMEOUT_MS ||
	       (uint32_t)(heartbeatMs - nowMs) < UI_HEARTBEAT_TIMEOUT_MS;
}

/**
 * @brief Whether the zero-throttle drain before a teardown is over. Pure.
 *
 * A suspend or a rebuild does not take the pin from an ESC that was given a
 * throttle within the drain. The pump sends zero frames until more than
 * @p drainMs, @ref ARM_ZERO_THROTTLE_MS, have passed since the last throttle
 * frame, and only then stops. Strictly more: the stamp is a whole
 * millisecond, and the frame went out somewhere inside it, so a teardown at
 * stamp + 251 ms is at least 250 ms after the frame. An ESC therefore sees
 * throttle zero for at least 250 ms before the line goes silent. A pump that
 * sent no throttle frame since its bind needs no drain.
 *
 * @param lastThrottleMs EscPump::lastThrottleMs; 0 for none.
 * @param nowMs          millis().
 * @param drainMs        Length of the drain, @ref ARM_ZERO_THROTTLE_MS.
 * @return               True when the teardown may go ahead.
 */
static inline bool escDrainDone(uint32_t lastThrottleMs, uint32_t nowMs,
                                uint32_t drainMs) {
	return lastThrottleMs == 0 || (uint32_t)(nowMs - lastThrottleMs) > drainMs;
}

/**
 * @brief What core0 asks of the pins, and what core1 last reached.
 *
 * Core0 asks for one mode at a time (escTaskStart(), escTaskPark(),
 * escTaskSuspend()); core1 reaches it in escTaskPoll(), on the step
 * escHandoverStep() decides. @see docs/design/dshot-lifetime.md
 */
enum class EscPinMode : uint8_t {
	Run,     /**< The pump is bound and sends its frames. */
	Park,    /**< The pump is down, SIG is driven low and AUX is on its pull-down. */
	Release, /**< The pump is down and both pins are free for the pin claim. */
};

/** @brief What core1 does with the pin on one pass. @see escHandoverStep() */
enum class EscHandover : uint8_t {
	Run,       /**< Run wanted and bound, no rebuild: the pump runs its frame slots. */
	Drain,     /**< Bound, another mode or a rebuild wanted, the drain not over: a zero frame per slot. */
	Teardown,  /**< Bound, another mode or a rebuild wanted, the drain over: let go, then step again. */
	Bind,      /**< Nothing bound and Run wanted: bind from the latched wiring. */
	Park,      /**< Nothing bound, Park wanted, the pins not parked on the latched wiring: park them. */
	Release,   /**< Nothing bound, Release wanted and not reached: free the pins. */
	Hold,      /**< Nothing bound and the wanted mode reached: nothing to do. */
};

/**
 * @brief Decide the pin handover for one pass of escTaskPoll(). Pure.
 *
 * A bound pump leaves Run for Park or Release (the pin claim), or rebuilds
 * (a SETTINGS change), only after the drain: while escDrainDone() is false
 * it keeps its slots and sends escDrainSlot() in each. Then it tears down,
 * and core1 steps again with the pump down, so a rebuild binds in the same
 * pass and a Park or a Release follows the teardown at once.
 *
 * With the pump down nothing is drained, since nothing was sent: Run binds,
 * Park parks unless the pins are parked on the latched wiring already (a
 * rebuild asked in Park parks again on the new wiring), and Release frees
 * the pins unless they are free already. A rebuild asked in Release waits
 * for the next mode: the pins are the claim's.
 *
 * @p reached Run with @p pumpUp false is the state before the first bind and
 * after a teardown: nothing is bound and nothing is parked.
 *
 * @param want           The mode core0 asks for.
 * @param reached        The mode core1 last reached.
 * @param rebuildReq     Core0 asked for a rebuild: the wiring changed.
 * @param pumpUp         The pump is bound, or its bind was refused.
 * @param lastThrottleMs EscPump::lastThrottleMs.
 * @param nowMs          millis().
 * @return               What to do on this pass.
 */
static inline EscHandover escHandoverStep(EscPinMode want, EscPinMode reached,
                                          bool rebuildReq, bool pumpUp,
                                          uint32_t lastThrottleMs, uint32_t nowMs) {
	if (pumpUp) {
		if (want == EscPinMode::Run && !rebuildReq) return EscHandover::Run;
		if (!escDrainDone(lastThrottleMs, nowMs, ARM_ZERO_THROTTLE_MS))
			return EscHandover::Drain;
		return EscHandover::Teardown;
	}
	switch (want) {
		case EscPinMode::Run:
			return EscHandover::Bind;
		case EscPinMode::Park:
			return reached != EscPinMode::Park || rebuildReq ? EscHandover::Park
			                                                 : EscHandover::Hold;
		case EscPinMode::Release:
		default:
			return reached != EscPinMode::Release ? EscHandover::Release
			                                      : EscHandover::Hold;
	}
}

/**
 * @brief Whether core1 reports mode @p m. Pure.
 *
 * Run only with the pump up; Park and Release once core1 reached them, with
 * the pump down. Between a teardown and the next mode, and before the first
 * bind, no mode is reported. escTaskSuspended() is Release alone, so a job
 * never binds on a parked pin.
 *
 * @param reached The mode core1 last reached.
 * @param pumpUp  The pump is bound, or its bind was refused.
 * @param m       The mode asked about.
 * @return        True when @p m is the mode reached.
 */
static inline bool escModeReported(EscPinMode reached, bool pumpUp, EscPinMode m) {
	if (m == EscPinMode::Run) return reached == EscPinMode::Run && pumpUp;
	return reached == m && !pumpUp;
}

/** @brief No GPIO, in an EscPadPlan's inputs: nothing held yet. */
#define ESC_NO_GPIO 0xFFu

/** @brief One pad's state, as escPadSet() sets it. @see escParkPlan() */
enum class EscPad : uint8_t {
	/**
	 * An SIO output driving low at 2 mA, the pad's lowest drive: SIG while
	 * parked. A pull-down alone is not known to hold the line low against
	 * an ESC's own pull-up, and a pad with the pull-down and the input
	 * buffer on can latch high (RP2350-E9, `board_pins.h`).
	 */
	Low,
	/** An input with the pull-down on and the input buffer off, 4 mA: the reset state. */
	Off,
	/** An input with the pull-up on, 4 mA: SIG handed over, as the teardown leaves it. */
	PullUp,
	/** An input with the pulls off, 4 mA: AUX handed over. */
	Input,
};

/** @brief Each GPIO's state for one mode change, in the order to set them. */
struct EscPadPlan {
	uint8_t gpio[4];  /**< The GPIOs, each once; SIG last. */
	EscPad  pad[4];   /**< The state of each. */
	uint8_t count;    /**< Entries used. */
};

/**
 * @brief The pad states of a change to Park, to Release, or out of Park to
 *        Run, with the wiring the pins move to. Pure.
 *
 * - SIG: Park drives it low (EscPad::Low); Release and Run put it on the
 *   pull-up (EscPad::PullUp), as the teardown does, and the bind for Run
 *   takes it from there.
 * - AUX: Park and Run leave it on the pull-down with the buffer off
 *   (EscPad::Off); Release makes it a plain input (EscPad::Input). A KISS
 *   receiver the bind starts takes it from there.
 * - A GPIO the old wiring held and the new one does not goes to EscPad::Off,
 *   the reset state: after a wiring change in Park the old SIG is no longer
 *   driven.
 *
 * Each GPIO appears once, with SIG last, so a swap of SIG and AUX names the
 * old SIG once, as the new AUX. With SIG and AUX on one GPIO, SIG's state
 * wins. @ref ESC_NO_GPIO in the old wiring, before anything was held, adds
 * nothing.
 *
 * @param to     EscPinMode::Park, EscPinMode::Release, or EscPinMode::Run
 *               for the pads before a bind from Park.
 * @param oldSig SIG the pins were last held on, or @ref ESC_NO_GPIO.
 * @param oldAux AUX the pins were last held on, or @ref ESC_NO_GPIO.
 * @param sig    SIG of the latched wiring.
 * @param aux    AUX of the latched wiring.
 * @return       The plan.
 */
static inline EscPadPlan escParkPlan(EscPinMode to, uint8_t oldSig, uint8_t oldAux,
                                     uint8_t sig, uint8_t aux) {
	EscPadPlan p = {};
	const uint8_t order[3] = {oldSig, oldAux, aux};
	for (uint8_t g : order) {
		if (g == ESC_NO_GPIO || g == sig) continue;
		bool seen = false;
		for (uint8_t i = 0; i < p.count; i++) seen = seen || p.gpio[i] == g;
		if (seen) continue;
		p.gpio[p.count] = g;
		p.pad[p.count]  = g == aux && to == EscPinMode::Release ? EscPad::Input
		                                                        : EscPad::Off;
		p.count++;
	}
	p.gpio[p.count] = sig;
	p.pad[p.count]  = to == EscPinMode::Park ? EscPad::Low : EscPad::PullUp;
	p.count++;
	return p;
}

/**
 * @brief Set one pad to @p pad. Core1 only, or core0 before core1 starts.
 *
 * Through the SDK: gpio_init() makes it an SIO input with the output latch
 * low, and the input buffer on. For EscPad::Low the latch is low before the
 * pad turns to an output, so the line is not driven high on the way.
 *
 * @param gpio The pad.
 * @param pad  Its state.
 */
static inline void escPadSet(uint8_t gpio, EscPad pad) {
	gpio_init(gpio);
	switch (pad) {
		case EscPad::Low:
			gpio_disable_pulls(gpio);
			gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_2MA);
			gpio_put(gpio, 0);
			gpio_set_dir(gpio, GPIO_OUT);
			break;
		case EscPad::Off:
			gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_4MA);
			gpio_pull_down(gpio);
			gpio_set_input_enabled(gpio, false);
			break;
		case EscPad::PullUp:
			gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_4MA);
			gpio_pull_up(gpio);
			break;
		case EscPad::Input:
		default:
			gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_4MA);
			gpio_disable_pulls(gpio);
			break;
	}
}

/**
 * @brief Set every pad of @p plan, in its order.
 * @param plan escParkPlan()'s result.
 */
static inline void escPadApply(const EscPadPlan *plan) {
	for (uint8_t i = 0; i < plan->count; i++) escPadSet(plan->gpio[i], plan->pad[i]);
}

/**
 * @brief A teardown's change to the telemetry block. Pure.
 *
 * Clears `initError`, so `DSERR` names the current bind only: a refused bind
 * is not reported once the pump is down. @see EscTelemetry::initError
 *
 * @param tel The block, under the lock on core1.
 */
static inline void escTelTeardown(EscTelemetry *tel) {
	tel->initError = false;
}

/**
 * @brief One frame slot of the drain. Pure.
 *
 * Throttle zero, with no command and no KISS request: the pump is about to
 * stop, and a command burst cut off by the teardown reaches no ESC's repeat
 * count. It takes no command request and forgets no EDT, so nothing core0
 * asked for is consumed by it. Its frame breaks a command block under way
 * (escCmdFrameSent()).
 *
 * @return The slot, as escPumpSlot() returns one.
 */
static inline EscSlot escDrainSlot() {
	EscSlot s = {{false, 0, 0, false}, false, false};
	return s;
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
			tel->lastRpmMs = nowMs ? nowMs : 1;
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
 * @brief Ask core1 for Release: the DShot driver down and both pins free.
 *
 * The pin claim's way in (pin_claim.h): the signal pin and the KISS pin are
 * free while the release holds. Forces a disarm first. From Run, core1 then
 * sends zero frames until more than @ref ARM_ZERO_THROTTLE_MS have passed
 * since the last throttle frame (escHandoverStep()), stops the pump, releases
 * the pin, which leaves it a plain input with the pulls off, puts the
 * pull-up back on it so it idles high, and ends the KISS receiver. From Park
 * nothing was sent and nothing is drained: SIG goes on its pull-up and AUX
 * becomes a plain input on the next pass (escParkPlan()). Poll
 * escTaskSuspended() until it reports true; the teardown happens on core1 so
 * that the PIO state machines are freed by the core that claimed them.
 */
void escTaskSuspend();

/**
 * @brief Ask core1 for Run: the pump bound and sending.
 *
 * If core1 has torn the pump down, it binds it again from the latched wiring:
 * disarmed, with the EDT enable as its first frames, as after a SETTINGS
 * change. From Park it first puts SIG on its pull-up at 4 mA. A start during
 * the drain leaves the running pump as it is. @see escTaskConfigure()
 */
void escTaskStart();

/**
 * @brief Ask core1 for Park: the pump down, SIG driven low, AUX on its
 *        pull-down.
 *
 * Forces a disarm first. From Run, core1 drains as for escTaskSuspend(),
 * tears the pump down and then parks the pins; from Release it parks them on
 * the next pass. SIG is an SIO output driving low at 2 mA, AUX an input with
 * the pull-down on and the input buffer off (escParkPlan(), EscPad). Poll
 * escTaskParked() until it reports true.
 */
void escTaskPark();

/**
 * @brief True only in Release: the driver is torn down and both pins are
 *        free. False while parked, so core1JobStart() never binds a job on a
 *        parked pin.
 */
bool escTaskSuspended();

/** @brief True only in Park: the pump is down and the pins are parked. */
bool escTaskParked();

/** @brief True only in Run: the pump is bound, or its bind was refused. */
bool escTaskRunning();

/**
 * @brief Set the pins and bitrate the DShot pump uses, and rebuild it.
 *
 * Core0 hands the values across rather than core1 reading the settings
 * struct directly: core0 owns that struct and edits it a field at a time while the
 * SETTINGS is open, so core1 reading it would see half-applied
 * combinations — a new pin against an old bitrate, for one frame.
 *
 * Takes effect on the next escTaskPoll(). In Run the driver releases the old
 * pin and binds the new one, after the same zero-throttle drain as a suspend
 * (escHandoverStep()). In Park the pins park again on the new wiring: the old
 * SIG goes to the reset state, input with the pull-down and the buffer off,
 * and the new SIG is driven low (escParkPlan()). In Release the new wiring
 * waits for the next mode. Forces a disarm, because the ESC on the old pin
 * stops hearing frames the moment it is released.
 *
 * Safe to call with unchanged values; it is a no-op if nothing differs, so the
 * SETTINGS can call it freely rather than tracking what it changed.
 *
 * @param dshotPin   GPIO for the ESC signal wire.
 * @param dshotKbaud DShot bitrate.
 * @param kissEnable Start the PIO receiver for KISS telemetry.
 * @param kissPin    GPIO for the telemetry wire. Ignored unless @p kissEnable.
 */
void escTaskConfigure(uint8_t dshotPin, uint16_t dshotKbaud,
                      bool kissEnable, uint8_t kissPin);

/** @brief GPIO the pump is currently driving. @return The live ESC pin. */
uint8_t escTaskDshotPin();

/**
 * @brief GPIO of the KISS wire, whether or not KISS is switched on.
 * @return The KISS pin escTaskConfigure() last set: the AUX pin of a claim.
 */
uint8_t escTaskKissPin();

/**
 * @brief DShot bitrate the pump is currently using.
 * @return 150, 300, 600 or 1200, in kbit/s: the value escTaskConfigure() last
 *         set, which the pump binds at on its next escTaskPoll().
 */
uint16_t escTaskDshotKbaud();

/** @} */
