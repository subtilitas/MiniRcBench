#include "esc_task.h"
#include "config.h"
#include "esc_merge.h"
#include "kiss_telem.h"
#include "pio_uart_rx.h"
#include "settings.h"

#include "dshot/out_dshot.h"

#include "plat.h"
#include <string.h>
#include <pico/critical_section.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>

/**
 * @file esc_task.cpp
 * @brief Core1 DShot frame pump, telemetry decode and cross-core state.
 */

#if PICO_ON_DEVICE
#include <hardware/pio.h>
#include "dshot.pio.h"
#include "pio_uart_rx.pio.h"

/*
 * The PIO budget. Core1 runs two PIO users: bidirectional DShot, a transmitter
 * and a receiver on adjacent state machines of one block, and the KISS
 * receiver on one more. Both claim from whichever block has room, and the SDK
 * searches pio2 first. pio1 carries the SDIO card driver on the 2.8" board,
 * pio0 the backlight (1 state machine, 6 words), and nothing else uses pio2, so
 * the DShot bind always finds pio2 with no other user. These two checks are
 * what makes the KISS receiver fit beside it in that block, on every board and
 * in every configuration. Checked in the firmware build only: the host stubs
 * carry no PIO programs.
 */
static_assert(2 + 1 <= NUM_PIO_STATE_MACHINES,
              "DShot and the KISS receiver need 3 state machines in one PIO block");
static_assert(sizeof(dshot_bidir_tx_program_instructions) / sizeof(uint16_t) +
              sizeof(dshot_rx_program_instructions) / sizeof(uint16_t) +
              sizeof(pio_uart_rx_program_instructions) / sizeof(uint16_t) <=
              PIO_INSTRUCTION_COUNT,
              "DShot and the KISS receiver do not fit one PIO block's instruction memory");
#endif

/**
 * @defgroup esc_pump Pin driver state (core1 only)
 * @{
 */
/** Bound since the last teardown, or refused. Core0 reads it. */
static volatile bool s_pumpUp = false;
static uint8_t s_pumpPin = 0;     /**< GPIO the driver was bound to. */
/**
 * The mode core1 last reached; Run with the pump down before the first bind
 * and after a teardown. Core0 reads it. @see escModeReported()
 */
static volatile EscPinMode s_reached = EscPinMode::Run;
/**
 * millis() at the end of the last bind that succeeded, 0 made 1; 0 while the
 * pump is down. Core0 reads it. @see escTaskBoundMs()
 */
static volatile uint32_t s_boundMs = 0;
/** SIG the pins were last bound or parked on; @ref ESC_NO_GPIO before that. */
static uint8_t s_heldSig = ESC_NO_GPIO;
/** AUX the pins were last bound or parked on; @ref ESC_NO_GPIO before that. */
static uint8_t s_heldAux = ESC_NO_GPIO;
/** @} */

/** @brief Guards @ref s_tel against concurrent access from core0. */
static critical_section_t s_cs;

/**
 * @defgroup esc_cmd Command state (core0 writes, core1 reads)
 *
 * There are two command sources and they no longer share a queue slot. They
 * used to, and the failure was silent in exactly the way that makes it hard to
 * find: a DShot command is only executed after it arrives in a run of identical
 * frames, so a beacon press landing in the middle of an EDT enable replaced the
 * tail of it and the ESC counted neither to completion. Nothing reported
 * anything; EDT simply stayed off, sometimes.
 *
 * So a command is a request core0 raises and core1 latches, the EDT burst is
 * core1's alone, and EDT takes the wire first. @see escTaskPoll()
 * @{
 */
static volatile uint16_t s_throttle    = 0;   /**< Commanded throttle, 0..2000. */
static volatile bool     s_armed       = false; /**< Arm state. */
static volatile uint8_t  s_poles       = DEFAULT_MOTOR_POLES; /**< Pole count. */
static volatile uint32_t s_heartbeatMs = 0;   /**< millis() of the last heartbeat. */
/** The command core0 wants; cmd 0 none. Under s_cs. */
static EscCmdRequest     s_cmdReq      = {};
/** Counts the requests, so core1 clears only the one it read. Under s_cs. */
static uint8_t           s_cmdGen      = 0;
/** Core1's EscCmdSeq::state. Under s_cs. */
static EscCmdState       s_cmdState    = EscCmdState::Idle;
/** Core1's EscCmdSeq::stateCmd. Under s_cs. */
static uint8_t           s_cmdStateCmd = 0;
/** Core1's EscPump::ready, published once per frame slot. */
static volatile EscCmdReady s_cmdReady = EscCmdReady::NoAnswer;
static volatile bool     s_edtRequested = false; /**< Mirror of EscEdtState::sent. */
/** @} */

/**
 * @defgroup esc_wiring Live wiring (core0 writes, core1 latches on rebuild)
 * @brief What escTaskConfigure() last set. Read only while building the driver.
 *
 * Latched at construction rather than read per frame, so a pin change cannot
 * take effect halfway: the driver is destroyed, these are read once, and the
 * new driver is built from a consistent set.
 * @{
 */
// Seeded properly by escTaskInit() from the stored settings. These initialisers
// only cover the window before that runs, which is why they are the safest
// values rather than any particular board's: pin 0 drives nothing on either
// board until escTaskInit() replaces it.
static volatile uint8_t  s_dshotPin   = 0;                      /**< ESC signal GPIO. */
static volatile uint16_t s_dshotKbaud = DSHOT_SPEED_KBAUD;      /**< DShot bitrate. */
static volatile uint8_t  s_kissEnable = 0;                      /**< KISS wire expected. */
static volatile uint8_t  s_kissPin    = 0;                      /**< KISS RX GPIO. */
static volatile bool     s_rebuildReq = false;                  /**< Rebuild pending. */
/** @} */

/** @brief Telemetry block. Core1 writes, core0 reads via escSnapshot(). */
static EscTelemetry s_tel;

/**
 * @defgroup esc_priv Core1 private state
 * @{
 */
static uint32_t s_nextSendUs = 0;   /**< Deadline for the next frame. */
static uint32_t s_lastRateMs = 0;   /**< Last time the rate counters rolled up. */
static uint32_t s_rateGoodMark = 0; /**< goodPackets at the last roll-up. */
static uint32_t s_rateBadMark = 0;  /**< badPackets at the last roll-up. */
static EscPump  s_pump = {};        /**< Command state. @see escPumpSlot() */
/** @} */

#if KISS_TELEM_ENABLE
/**
 * @defgroup esc_kiss KISS telemetry state (core1 only)
 * @{
 */
static KissDecoder s_kiss;              /**< Frame assembler. */
static bool        s_kissPending = false; /**< A reply is outstanding. */
static uint32_t    s_kissReqMs = 0;     /**< millis() of the outstanding request. */

/**
 * @brief True while a PIO receiver is running on the KISS pin.
 *
 * The receiver used to be one of the two hardware UARTs, picked from the pin,
 * which meant the pin had to be one of the eight the RP2350 can receive on —
 * and on the 2.8" board exactly one of those is free and it is the one the ESC
 * signal wire is on. @see pio_uart_rx.h for what replaced it and why.
 */
static bool s_kissRx = false;

/**
 * @brief Start receiving KISS on @p pin.
 *
 * Receive only: the ESC talks and we never answer, and driving a line the ESC
 * is already driving is not a thing to do by accident.
 *
 * A refusal — every state machine already claimed — leaves @ref s_kissRx false,
 * and the effect is the same as an unplugged telemetry wire: no KISS frames,
 * the merged reading falls back to EDT, and the tiles say so. That is the right
 * failure for something that cannot happen on a board this firmware builds for
 * (see the PIO budget at the top of this file) and must still not fault.
 *
 * @param pin GPIO to receive on. Any of them.
 */
static void kissUartBegin(uint8_t pin) {
	memset(&s_kiss, 0, sizeof(s_kiss));
	s_kissPending = false;
	s_kissRx = pioUartRxBegin(pin, KISS_BAUD);
}

/** @brief Release the KISS receiver and its pin, if one is running. */
static void kissUartEnd() {
	if (!s_kissRx) return;
	pioUartRxEnd();
	s_kissRx = false;
	s_kissPending = false;
}

/**
 * @brief Drain the receiver and fold any complete frame into @ref s_tel.
 *
 * Called from escTaskPoll() on core1, on every pass rather than once per frame
 * slot. Bounded work either way — the FIFO is eight bytes deep and this never
 * blocks — but eight bytes is 700 us at 115200 baud, against the 1 ms between
 * DShot frames, so draining at the frame rate would lose the tail of most
 * replies. The PL011 this replaced held 32 and could afford to wait.
 *
 * Doing it here at all, rather than handing the receiver to core0, is the same
 * argument as before: it is a FIFO read and a byte of state machine, and core1
 * has the cycles between frames.
 */
static void kissDrain() {
	KissFrame f;
	if (!s_kissRx) return;

	while (pioUartRxReadable()) {
		uint8_t c = pioUartRxGetc();
		if (kissFeed(&s_kiss, c, &f)) {
			s_kissPending = false;
			critical_section_enter_blocking(&s_cs);
			s_tel.kissVolts  = f.volts;
			s_tel.kissAmps   = f.amps;
			s_tel.kissTempC  = f.tempC;
			s_tel.kissMah    = f.mah;
			s_tel.kissErpm   = f.erpm;
			s_tel.kissLastMs = millis();
			s_tel.kissGood   = s_kiss.good;
			s_tel.haveKiss   = true;
			critical_section_exit(&s_cs);
		}
	}

	// A reply that never completed. Count it and drop the partial frame, or the
	// next request's bytes would append to it and decode at the wrong offset.
	if (s_kissPending && (uint32_t)(millis() - s_kissReqMs) > KISS_REPLY_TIMEOUT_MS) {
		s_kissPending = false;
		kissExpectFrame(&s_kiss);
		critical_section_enter_blocking(&s_cs);
		s_tel.kissTimeouts++;
		s_tel.kissBad = s_kiss.bad;
		critical_section_exit(&s_cs);
	} else if (s_kiss.bad) {
		critical_section_enter_blocking(&s_cs);
		s_tel.kissBad = s_kiss.bad;
		critical_section_exit(&s_cs);
	}
}
/** @} */
#endif
void escSetThrottle(uint16_t t) {
	if (t > 2000) t = 2000;
	s_throttle = t;
}

void escSetArmed(bool armed) {
	if (!armed) s_throttle = 0;
	s_armed = armed;
}

void escSetPoles(uint8_t poles) {
	if (poles < 2) poles = 2;
	s_poles = poles;
}

void escHeartbeat() {
	s_heartbeatMs = millis();
}

uint32_t escHeartbeatMs() { return s_heartbeatMs; }

bool escRequestCommand(uint8_t cmd, bool save) {
	if (!escCmdAccept(cmd, save, s_armed, s_cmdReady)) return false;
	// A request, not a queue write. Core1 starts it when the wire is free and
	// its own readiness reads Ready, so a press cannot land in the middle of an
	// EDT enable and truncate it. An arm drops it. @see esc_cmd
	uint32_t now = millis();
	critical_section_enter_blocking(&s_cs);
	s_cmdReq.cmd  = cmd;
	s_cmdReq.save = save;
	s_cmdReq.atMs = now;
	s_cmdGen = (uint8_t)(s_cmdGen + 1);
	critical_section_exit(&s_cs);
	return true;
}

void escCancelCommand() {
	critical_section_enter_blocking(&s_cs);
	s_cmdReq.cmd = 0;
	s_cmdGen = (uint8_t)(s_cmdGen + 1);
	critical_section_exit(&s_cs);
}

EscCmdReady escCommandReady() { return s_cmdReady; }

EscCmdState escCommandState(uint8_t *cmd) {
	critical_section_enter_blocking(&s_cs);
	bool waiting = s_cmdReq.cmd != 0;
	EscCmdState st = waiting ? EscCmdState::Waiting : s_cmdState;
	uint8_t c = waiting ? s_cmdReq.cmd : s_cmdStateCmd;
	critical_section_exit(&s_cs);
	if (cmd) *cmd = c;
	return st;
}

/**
 * @brief Publish the pump's command state to core0. Core1 only.
 * @param taken   The request core1 read was started, expired or dropped.
 * @param readGen s_cmdGen when core1 read it: only that request is cleared.
 */
static void publishCommand(bool taken, uint8_t readGen) {
	critical_section_enter_blocking(&s_cs);
	if (taken && s_cmdGen == readGen) s_cmdReq.cmd = 0;
	s_cmdState    = s_pump.seq.state;
	s_cmdStateCmd = s_pump.seq.stateCmd;
	critical_section_exit(&s_cs);
	s_cmdReady = s_pump.ready;
}

bool escEdtRequested() { return s_edtRequested; }

/**
 * @defgroup esc_suspend Pin modes
 * @brief Run, Park and Release: core0 asks, core1 reaches and reports.
 * @see pin_claim.h, escHandoverStep()
 * @{
 */
/**
 * The mode core0 asks for. Park from escTaskInit() on: no DShot frame at
 * power-up (docs/design/dshot-lifetime.md, owner decision 2).
 */
static volatile EscPinMode s_want = EscPinMode::Park;

void escTaskSuspend() {
	escSetArmed(false);
	s_want = EscPinMode::Release;
}

void escTaskPark() {
	escSetArmed(false);
	s_want = EscPinMode::Park;
}

void escTaskStart() {
	s_want = EscPinMode::Run;
}

bool escTaskSuspended() {
	return escModeReported(s_reached, s_pumpUp, EscPinMode::Release);
}
bool escTaskParked() { return escModeReported(s_reached, s_pumpUp, EscPinMode::Park); }
bool escTaskRunning() { return escModeReported(s_reached, s_pumpUp, EscPinMode::Run); }
uint32_t escTaskBoundMs() { return s_boundMs; }
/** @} */

void escSnapshot(EscTelemetry *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_tel;
	critical_section_exit(&s_cs);
}

static void padsTo(EscPinMode to);

void escTaskInit() {
	// Core0 owns this, and must do it before core1 is launched. See the note in
	// esc_task.h: core0's first uiTick() calls escSnapshot(), which enters this
	// critical section, and it will get there long before core1 has booted.
	critical_section_init(&s_cs);
	memset((void *)&s_tel, 0, sizeof(s_tel));

	// Seed the wiring from the stored settings, so core1's very first build uses
	// the user's pins rather than the compiled defaults followed by a rebuild.
	const Settings *cfg = settings();
	s_dshotPin   = cfg->dshotPin;
	s_dshotKbaud = cfg->dshotKbaud;
	s_kissEnable = cfg->kissEnable;
	s_kissPin    = cfg->kissPin;
	s_rebuildReq = false;

	// Parked from here on: no DShot frame at power-up, and SIG driven low
	// at 2 mA, AUX on its pull-down (docs/design/dshot-lifetime.md, "What
	// changes at boot"). Core0 sets the pads itself, as servoSupplyInit()
	// sets its enable pin: core1 is not running yet, and a park is a GPIO
	// setting, not a PIO claim. Core1's first escTaskPoll() then finds Park
	// wanted and reached, and binds nothing.
	s_want = EscPinMode::Park;
	padsTo(EscPinMode::Park);
}

void escTaskConfigure(uint8_t dshotPin, uint16_t dshotKbaud,
                      bool kissEnable, uint8_t kissPin) {
	if (s_dshotPin == dshotPin && s_dshotKbaud == dshotKbaud &&
	    s_kissEnable == (kissEnable ? 1u : 0u) && s_kissPin == kissPin)
		return;

	// Disarm first, and through escSetArmed() so the throttle is zeroed too. The
	// ESC on the old pin is about to stop hearing frames; leaving a non-zero
	// throttle latched for whatever gets built next is not a state to pass
	// through.
	escSetArmed(false);
	s_dshotPin   = dshotPin;
	s_dshotKbaud = dshotKbaud;
	s_kissEnable = kissEnable ? 1u : 0u;
	s_kissPin    = kissPin;
	// The wiring stores land before the request: core1 clears the request,
	// takes its own barrier and then reads the wiring. @see pumpBind()
	__dmb();
	s_rebuildReq = true;
}

uint8_t escTaskDshotPin() { return s_dshotPin; }

uint8_t escTaskKissPin() { return s_kissPin; }

uint16_t escTaskDshotKbaud() { return s_dshotKbaud; }

void escTaskBegin() {
	// Nothing is claimed here. The driver and the UART are built by the first
	// escTaskPoll(), which is also the path a suspend or a reconfigure returns
	// through -- one construction site, so the two cannot drift apart.
	s_nextSendUs = time_us_32();
	s_lastRateMs = millis();
}

/**
 * @brief Fold one poll of the driver into @ref s_tel, under the lock.
 * @param reply What out_dshot_poll() returned.
 * @param t     The decoded frame. @see escFoldReply()
 */
static void applyTelemetry(dshot_reply_t reply, const dshot_telem_t *t) {
	uint8_t  poles = s_poles;
	uint32_t now   = millis();

	critical_section_enter_blocking(&s_cs);
	escFoldReply(&s_tel, reply, t, poles, now);
	critical_section_exit(&s_cs);
}

/**
 * @brief Tear the pump down: stop and release the driver, end the KISS
 *        receiver, clear `initError`. Core1 only.
 */
static void pumpTeardown() {
	if (s_pumpUp) {
		// Stop first, cutting any frame in flight (out_dshot_stop() leaves
		// the line at its idle level), then release. The release hands the
		// pad back as a plain input with the pulls off. The pull-up goes
		// back on, so the line holds the bidirectional idle level until its
		// next owner takes it: a claim's user, the park, or nothing after a
		// pin change.
		// The stamp first: core0 never reads a bind stamp for a pump
		// that is going down.
		s_boundMs = 0;
		__dmb();
		out_dshot_stop(s_pumpPin);
		out_dshot_release(s_pumpPin);
		gpio_pull_up(s_pumpPin);
		s_pumpUp = false;  // reported as no mode until the next one is reached
		// A sequence going out stops here, and core0 reads NoAnswer
		// until the next bind has an ESC answering, rather than the
		// last readiness for as long as the pin is away. A request
		// still waiting expires: it does not start on the next bind.
		critical_section_enter_blocking(&s_cs);
		EscCmdRequest req = s_cmdReq;
		uint8_t reqGen = s_cmdGen;
		escTelTeardown(&s_tel);
		critical_section_exit(&s_cs);
		publishCommand(escPumpTeardown(&s_pump, &req), reqGen);
	}
#if KISS_TELEM_ENABLE
	// Whoever owns the pin next, it is not us. A receiver left running
	// would accumulate noise into a decoder with no way to tell it from a
	// reply.
	kissUartEnd();
#endif
}

/**
 * @brief Bind the driver from the latched wiring, and the KISS receiver with
 *        it. Core1 only.
 */
static void pumpBind() {
	// The request is cleared before the wiring is read, so a configure that
	// lands during the bind leaves it set, and the next pass rebuilds on the
	// new wiring. @see esc_wiring
	s_rebuildReq = false;
	__dmb();
	// Latch the wiring once, here. A configure that lands during these reads
	// can be latched half-applied for this one pass, with the ESC disarmed;
	// it leaves the request set, and the next pass rebuilds on its wiring.
	uint8_t  pin   = s_dshotPin;
	uint16_t kbaud = s_dshotKbaud;
	uint8_t  aux   = s_kissPin;
	bool     kiss  = s_kissEnable != 0;
	// Out of Park the pads go back first: SIG on its pull-up at 4 mA, from
	// where the bind takes it, and a GPIO the old wiring held to EscPad::Off.
	// No mode is reported while they change: core0 never reads "parked"
	// with SIG no longer driven low. @see escParkPlan()
	EscPinMode from = s_reached;
	if (from == EscPinMode::Park) {
		s_reached = EscPinMode::Run;
		__dmb();
		EscPadPlan plan = escParkPlan(from, EscPinMode::Run, s_heldSig, s_heldAux,
		                              pin, aux);
		escPadApply(&plan);
	}
	// Bidirectional, and refused rather than degraded: a bind that cannot
	// get two adjacent state machines in one block fails here, and
	// `initError` puts DSERR in the status bar. The pump still runs its
	// slots, so the link counters show an ESC that never answers.
	bool bound = out_dshot_bind(pin, kbaud, true);
	s_pumpPin = pin;
	// Run before the pump is up, so core0 never reads Park with the pump up.
	s_reached = EscPinMode::Run;
	s_pumpUp  = true;
	s_heldSig = pin;
	s_heldAux = aux;
	// A rebuild is a new ESC as far as this pump is concerned: it starts
	// disarmed, and its first frames are the EDT enable. An ESC that kept
	// EDT on across the rebuild is read correctly from its first reply,
	// because every reply is read with EDT decoding on.
	// @see escPumpBind(), ESC_EDT_DECODE, escEdtBind()
	escPumpBind(&s_pump, &s_armed, &s_throttle);
	s_edtRequested = s_pump.edt.sent;
	s_nextSendUs = time_us_32();
#if KISS_TELEM_ENABLE
	if (kiss && aux != pin) kissUartBegin(aux);
#else
	(void)kiss;
#endif
	critical_section_enter_blocking(&s_cs);
	s_tel.initError = !bound;
	critical_section_exit(&s_cs);
	// The bind stamp last, once everything it stands for is in place.
	// @see escTaskBoundMs()
	uint32_t stamp = millis();
	__dmb();
	s_boundMs = bound ? (stamp ? stamp : 1u) : 0u;
}

/**
 * @brief Set the pads for Park or Release from the latched wiring, and
 *        report the mode. Core1 only, or core0 in escTaskInit() before
 *        core1 starts.
 *
 * Release touches the pads only out of Park: from Run the teardown left them
 * in today's handover state, and before anything was held they are in the
 * pad's reset state. While the pads change no mode is reported, so core0
 * never reads "parked" or "released" for pads on their way.
 *
 * @param to EscPinMode::Park or EscPinMode::Release.
 */
static void padsTo(EscPinMode to) {
	EscPinMode from = s_reached;
	if (to == EscPinMode::Park || from == EscPinMode::Park) {
		// Cleared before the wiring is read, as in pumpBind(): a configure
		// that lands during the park parks again on the next pass.
		s_rebuildReq = false;
		s_reached = EscPinMode::Run;
		__dmb();
		uint8_t sig = s_dshotPin;
		uint8_t aux = s_kissPin;
		EscPadPlan plan = escParkPlan(from, to, s_heldSig, s_heldAux, sig, aux);
		escPadApply(&plan);
		s_heldSig = sig;
		s_heldAux = aux;
		// The pads are set before the mode that names them is published.
		__dmb();
	}
	s_reached = to;
}

void escTaskPoll() {
	// Handle a mode change or a reconfigure before anything touches the
	// driver. Release, park and bind all happen here so the PIO state
	// machines are released and re-claimed by the core that owns them.
	//
	// A bound pump leaving Run waits for the drain first: zero frames until
	// more than ARM_ZERO_THROTTLE_MS have passed since the last throttle
	// frame, so an ESC never goes from a throttle straight to silence. A
	// pump that is down has sent nothing and drains nothing: a release
	// asked before core1 has ever bound the pin reports suspended at once.
	// The decision is escHandoverStep(), pure and in the header with the
	// rest of the rules; this function does the I/O on it.
	EscPinMode want = s_want;
	EscHandover handover = escHandoverStep(want, s_reached, s_rebuildReq, s_pumpUp,
	                                       s_pump.lastThrottleMs, millis());
	if (handover == EscHandover::Teardown) {
		pumpTeardown();
		handover = escHandoverStep(want, s_reached, false, false, 0, 0);
	}
	switch (handover) {
		case EscHandover::Bind:
			pumpBind();
			break;
		case EscHandover::Park:
		case EscHandover::Release:
			padsTo(handover == EscHandover::Park ? EscPinMode::Park
			                                     : EscPinMode::Release);
			return;
		case EscHandover::Hold:
			return;
		default:
			break;
	}
	if (!s_pumpUp) return;

#if KISS_TELEM_ENABLE
	// Ahead of the frame deadline, not after it: this loop runs far faster than
	// the 1 kHz frame rate, and the receiver only holds eight bytes.
	// @see kissDrain()
	kissDrain();
#endif

	uint32_t now = time_us_32();
	if ((int32_t)(now - s_nextSendUs) < 0) return;
	s_nextSendUs += DSHOT_PERIOD_US;
	// if we fell badly behind (debugger stop, etc.) resync rather than burst
	if ((int32_t)(time_us_32() - s_nextSendUs) > (int32_t)(DSHOT_PERIOD_US * 4)) {
		s_nextSendUs = time_us_32() + DSHOT_PERIOD_US;
	}

	// 1. Drain the reply to the *previous* frame before sending the next one.
	// With EDT decoding on for every reply, the replies to the enable frames
	// included: an ESC already in EDT mode answers them with EDT frames, and
	// AM32 answers the enable itself with a status frame. @see ESC_EDT_DECODE
	// The result is also the command sequence's check of that frame.
	// @see escCmdCheckReply()
	dshot_telem_t t;
	memset(&t, 0, sizeof(t));
	dshot_reply_t reply = out_dshot_poll(s_pumpPin, escPumpDecodeEdt(&s_pump), &t);
	applyTelemetry(reply, &t);

	// 2. Decide what to send. The decision is escPumpSlot(), which is pure and
	// lives in the header so the host suite can reach it. This function only
	// does the I/O. During the drain the slot is escDrainSlot(): a plain
	// zero, with no command and no KISS request.
	// The request and the heartbeat stamp before the clock, so the clock is
	// never older than either; escUiAlive() also accepts a stamp less than
	// 250 ms newer, and escPumpSlot() reads a request's wait signed.
	critical_section_enter_blocking(&s_cs);
	EscCmdRequest req = s_cmdReq;
	uint8_t reqGen = s_cmdGen;
	critical_section_exit(&s_cs);
	uint32_t hb = s_heartbeatMs;
	uint32_t ms = millis();
	bool uiAlive = escUiAlive(ms, hb);
#if KISS_TELEM_ENABLE
	bool kissRx = s_kissRx;
#else
	bool kissRx = false;
#endif
	EscSlot slot = handover == EscHandover::Drain
	             ? escDrainSlot()
	             : escPumpSlot(&s_pump, &s_tel, s_armed, uiAlive, s_throttle,
	                           &req, reply, kissRx, ms);
	if (slot.forgetEdt) {
		critical_section_enter_blocking(&s_cs);
		escEdtForget(&s_tel);
		critical_section_exit(&s_cs);
	}
	s_edtRequested = s_pump.edt.sent;

	// A command frame with the telemetry bit set; a throttle frame with it
	// only on a KISS request. @see escFrameWire()
	const EscFrame &f = slot.frame;
	bool telemetry = false;
	uint16_t value = escFrameWire(&f, &telemetry);
	out_dshot_send(s_pumpPin, value, telemetry);
	// A scheduled attempt is stamped by its last frame, and a throttle frame
	// stamps the drain's start. @see escEdtFrameSent(), escThrottleStamp()
	escPumpFrameSent(&s_pump, &f, millis());
	// Only the request this pass read is cleared: one core0 made since stays.
	publishCommand(slot.cmdTaken, reqGen);

#if KISS_TELEM_ENABLE
	if (f.requestKiss) {
		// Align the assembler to this request: whatever is in the buffer is a
		// stalled partial frame, and the next ten bytes are the reply.
		kissExpectFrame(&s_kiss);
		s_kissPending = true;
		s_kissReqMs = ms;
	}
#endif

	// 3. Once a second, roll up a link-quality figure.
	if ((uint32_t)(ms - s_lastRateMs) >= 1000) {
		s_lastRateMs = ms;
		critical_section_enter_blocking(&s_cs);
		uint32_t good = s_tel.goodPackets - s_rateGoodMark;
		uint32_t bad  = s_tel.badPackets  - s_rateBadMark;
		s_rateGoodMark = s_tel.goodPackets;
		s_rateBadMark  = s_tel.badPackets;
		s_tel.packetRate = (uint16_t)(good > 65535 ? 65535 : good);
		uint32_t total = good + bad;
		s_tel.errPercent = total ? (uint8_t)((bad * 100) / total) : 0;
		critical_section_exit(&s_cs);
	}
}
