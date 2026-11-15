/**
 * @file sbus_out_job.cpp
 * @brief The S.BUS output's core1 job. @see sbus_out_job.h
 */

#include "sbus_out_job.h"
#include "sbus_job.h"
#include "pin_claim.h"
#include "settings.h"
#include "config.h"
#include "escwire_job.h"
#include "escwire_port.h"
#include "greet_cycle.h"
#include "ew_a.h"

#include "plat.h"
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/structs/pads_bank0.h>
#include <pico/critical_section.h>
#include <string.h>

static_assert(SBUS_OUT_RAW_LO < SBUS_OUT_RAW_MID && SBUS_OUT_RAW_MID < SBUS_OUT_RAW_HI,
              "the START frame's centre lies inside the range");
static_assert(SBUS_OUT_RAW_HI <= SBUS_RAW_MAX, "every clamped value fits 11 bits");
static_assert(SBUS_OUT_THR_DEFAULT < 4u, "the throttle is one of channels 1 to 4");
static_assert(SBUS_OUT_FRAME_US + SBUS_GAP_US <= SBUS_OUT_MIN_US,
              "frames at the shortest spacing keep the decoder's gap");
static_assert(SBUS_OUT_MIN_US <= SBUS_OUT_PERIOD_US, "the spacing bound is below the period");
static_assert(SBUS_FRAME_BYTES <= 32u, "a frame fits the PL011's 32-entry TX FIFO");
static_assert(SBUS_OUT_PROBE_RX_MAX <= EW_RX_MAX, "the probe's capture is one the driver takes");
static_assert(AM32_GREET_TMO_MS * 1000u <= EW_FIRST_US_MAX, "the wait for an answer is one the driver takes");

// ---------------------------------------------------------------------------
// the pure core
// ---------------------------------------------------------------------------

/** @brief @p v within raw 172 to @p hi. */
static uint16_t clampRaw(uint16_t v, uint16_t hi) {
	if (v < SBUS_OUT_RAW_LO) return SBUS_OUT_RAW_LO;
	return v > hi ? hi : v;
}

void sbusOutCoreReset(SbusOutCore *c, uint16_t ackSeq) {
	memset(c, 0, sizeof(*c));
	c->ackSeq = ackSeq;
	c->thrCh = SBUS_OUT_THR_DEFAULT;
	c->ceilRaw = SBUS_OUT_RAW_LO;
	sbusOutStartFrame(&c->frame, c->thrCh);
}

void sbusOutStartFrame(sbus_frame_t *f, uint8_t thrCh) {
	memset(f, 0, sizeof(*f));
	for (unsigned i = 0; i < SBUS_CHANNELS; i++)
		f->channel[i] = i < 4u && i != thrCh ? SBUS_OUT_RAW_MID : SBUS_OUT_RAW_LO;
}

bool sbusOutCoreTake(SbusOutCore *c, const SbusOutCmd *cmd, uint32_t nowUs,
                     bool uiAlive, uint16_t ceilMax) {
	if (cmd->seq == c->ackSeq) return false;
	c->ackSeq = cmd->seq;
	switch (cmd->kind) {
		case SbusOutCmdKind::Safe:
			c->running = false;
			break;
		case SbusOutCmdKind::Start:
			if (!uiAlive || cmd->thrCh > 3u) break;
			c->running = true;
			c->tripped = false;
			c->refused = false;
			c->frames = 0;
			c->thrCh = cmd->thrCh;
			// The screen's ceiling, never above MAX THROTTLE's.
			c->ceilRaw = clampRaw(cmd->ceilRaw, clampRaw(ceilMax, SBUS_OUT_RAW_HI));
			// core1's frame, whatever the screen sent before: no value set
			// while SAFE reaches the FC.
			sbusOutStartFrame(&c->frame, c->thrCh);
			c->nextUs = nowUs;
			break;
		case SbusOutCmdKind::Frame:
			if (!c->running) break;
			for (unsigned i = 0; i < SBUS_CHANNELS; i++)
				c->frame.channel[i] = clampRaw(cmd->channel[i],
				                               i == c->thrCh ? c->ceilRaw : SBUS_OUT_RAW_HI);
			c->frame.ch17 = cmd->ch17;
			c->frame.ch18 = cmd->ch18;
			c->frame.failsafe = cmd->failsafe;
			c->frame.frame_lost = cmd->frameLost;
			break;
	}
	return true;
}

bool sbusOutCoreStep(SbusOutCore *c, uint32_t nowUs, bool uiAlive, bool txEmpty,
                     uint8_t *out) {
	if (c->running && !uiAlive) {
		c->running = false;
		c->tripped = true;
	}
	if (!c->running) return false;
	uint32_t late = nowUs - c->nextUs;
	// A deadline ahead of the pass reads as more than half the clock late.
	if (late >= 0x80000000u) return false;
	if (c->sentAny && nowUs - c->lastUs < SBUS_OUT_MIN_US) return false;
	if (!txEmpty) return false;
	if (!sbus_encode(&c->frame, out)) {
		// Unreachable with every channel clamped; the second answer
		// rcbench's contract allows: send nothing more.
		c->running = false;
		c->refused = true;
		return false;
	}
	c->nextUs = late >= SBUS_OUT_PERIOD_US ? nowUs + SBUS_OUT_PERIOD_US
	                                       : c->nextUs + SBUS_OUT_PERIOD_US;
	c->lastUs = nowUs;
	c->sentAny = true;
	c->frames++;
	return true;
}

uint16_t sbusOutCeilRaw(uint16_t maxThrottle) {
	uint32_t mt = maxThrottle > 2000u ? 2000u : maxThrottle;
	return (uint16_t)(SBUS_OUT_RAW_LO +
	                  (SBUS_OUT_RAW_HI - SBUS_OUT_RAW_LO) * mt / 2000u);
}

bool sbusOutProbeAnswer(const BlAnswer &a, bool *efm8) {
	*efm8 = a.kind == BlAnswerKind::Efm8 || a.fragment;
	return *efm8 || a.kind == BlAnswerKind::Am32;
}

uint8_t sbusOutClaimRoles(uint8_t sigGpio, uint8_t auxGpio) {
	if (sigGpio == SBUS_OUT_GPIO) return PIN_SIG;
	if (auxGpio == SBUS_OUT_GPIO) return PIN_SIG | PIN_AUX;
	return 0;
}

// ---------------------------------------------------------------------------
// the job on core1, and the block it shares with core0
// ---------------------------------------------------------------------------

/** @brief Guards the shared block below. */
static critical_section_t s_cs;

/**
 * @defgroup sbus_out_job_shared Shared block (under s_cs)
 * @brief Core0 writes the command; core1 writes the status.
 * @{
 */
static SbusOutCmd    s_cmd;     /**< The command held for core1. */
static SbusOutStatus s_status;  /**< What core1 last published. */
/** @} */

/**
 * @defgroup sbus_out_job_core1 Core1 only
 * @{
 */
static SbusOutCore  s_core;        /**< The frames' state. */
static sbus_frame_t s_sent;        /**< The frame last loaded. */
static uint32_t     s_lastMs;      /**< millis() of the pass that loaded it. */
static bool         s_releasing;   /**< The release has begun. */
static uint32_t     s_releaseMs;   /**< millis() of the release's first pass. */
static SbusOutPhase s_phase = SbusOutPhase::Off;  /**< Where the bind is. */
/** @} */

/**
 * @defgroup sbus_out_job_probe The probe (core1 only)
 * @{
 */
static ew_driver_t s_line;          /**< The escwire line on GP0, from the bind to the switch. */
static GreetCycle  s_cycle;         /**< ESC SETTINGS' connect cycle. */
static bool        s_greeting;      /**< A greeting is on the wire. */
/**
 * @brief Every greeting of the running cycle went out and came back with no
 *        byte and its capture whole: at the cycle's end the UART takes GP0.
 */
static bool        s_clean;
static bool        s_answered;      /**< A bootloader answered in this bind. */
static bool        s_efm8;          /**< An EFM8 bootloader, or a piece of its answer, in this bind. */
static uint16_t    s_signature;     /**< The last EFM8 answer's signature. */
static uint32_t    s_cycles;        /**< Cycles ended in this bind. */
static decltype(SbusOutStatus::answerSeq) s_answerSeq; /**< Answers since the start; never cleared. */
static decltype(SbusOutStatus::efm8Seq)   s_efm8Seq;   /**< EFM8 answers and pieces since the start; never cleared. */
static uint32_t    s_answerMs;      /**< millis() of the last answer; never cleared. */
static uint8_t     s_greet[EW_A_CONTACT_LEN];     /**< rcbench's first contact. */
static uint8_t     s_reply[SBUS_OUT_PROBE_RX_MAX]; /**< What came back. */
/** @} */

/** @brief Hand the status to core0. */
static void publish() {
	SbusOutStatus st;
	st.phase = s_phase;
	st.answered = s_answered;
	st.efm8 = s_efm8;
	st.signature = s_signature;
	st.cycles = s_cycles;
	st.answerSeq = s_answerSeq;
	st.efm8Seq = s_efm8Seq;
	st.answerMs = s_answerMs;
	st.ackSeq = s_core.ackSeq;
	st.running = s_core.running;
	st.tripped = s_core.tripped;
	st.refused = s_core.refused;
	st.frames = s_core.frames;
	st.lastFrameMs = s_lastMs;
	st.sent = s_sent;
	critical_section_enter_blocking(&s_cs);
	s_status = st;
	critical_section_exit(&s_cs);
}

/** @brief Core1's state after a bind, the switch or a release: SAFE, nothing sent. */
static void clearState(uint16_t ackSeq) {
	sbusOutCoreReset(&s_core, ackSeq);
	memset(&s_sent, 0, sizeof(s_sent));
	s_lastMs = 0;
	s_releasing = false;
}

/** @brief The probe's state for a new bind: a quiet phase from @p nowMs, nothing answered. */
static void probeReset(uint32_t nowMs) {
	greetCycleStart(&s_cycle, nowMs);
	s_greeting = false;
	s_clean = true;
	s_answered = false;
	s_efm8 = false;
	s_signature = 0;
	s_cycles = 0;
}

/** @brief The seq of the command block: commands up to it are not taken. */
static uint16_t heldSeq() {
	critical_section_enter_blocking(&s_cs);
	uint16_t seq = s_cmd.seq;
	critical_section_exit(&s_cs);
	return seq;
}

/**
 * @brief GP0 to UART0 with its output inverted, gpio_set_function()'s three
 *        steps with OUTOVER in the same control-register write as FUNCSEL.
 */
static void padToUart() {
	// Input enable on, output disable off, as gpio_set_function() does.
	hw_write_masked(&pads_bank0_hw->io[SBUS_OUT_GPIO], PADS_BANK0_GPIO0_IE_BITS,
	                PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
	// One write: the pad never carries the UART's high idle.
	io_bank0_hw->io[SBUS_OUT_GPIO].ctrl =
		((uint32_t)GPIO_FUNC_UART << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB) |
		((uint32_t)GPIO_OVERRIDE_INVERT << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB);
	// The UART now drives the pad: isolation off.
	hw_clear_bits(&pads_bank0_hw->io[SBUS_OUT_GPIO], PADS_BANK0_GPIO0_ISO_BITS);
}

/**
 * @brief GP0 to a plain input, low: SIO's output enable off and its output
 *        low before the pad leaves the UART, then SIO, which zeroes OUTOVER,
 *        then the pull-down.
 */
static void padRelease() {
	gpio_set_dir(SBUS_OUT_GPIO, GPIO_IN);
	gpio_put(SBUS_OUT_GPIO, 0);
	gpio_set_function(SBUS_OUT_GPIO, GPIO_FUNC_SIO);
	gpio_pull_down(SBUS_OUT_GPIO);
}

static void sbusOutJobInit() {
	critical_section_init(&s_cs);
}

static uint8_t sbusOutJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	if (sigGpio != SBUS_OUT_GPIO && auxGpio != SBUS_OUT_GPIO) return CORE1_JOB_NO_GP0;

	// SIO drives nothing, whatever an earlier owner left. As SIG, the pump's
	// teardown leaves GP0 an SIO input with the pull-up (esc_task.cpp,
	// escTaskPoll()'s teardown); as AUX with KISS on, the KISS receiver's
	// end leaves it on the NULL function with the pulls off (pio_uart_rx.cpp,
	// pioUartRxEnd()).
	gpio_set_dir(SBUS_OUT_GPIO, GPIO_IN);
	gpio_put(SBUS_OUT_GPIO, 0);
	// The UART from the bind, with no pad on it: a clock it cannot use
	// refuses the bind before the probe drives anything.
	if (!sbusBaudOk(uart_init(uart0, SBUS_BAUD))) {
		uart_deinit(uart0);
		// The line where the pump's teardown and the probe hold it: high.
		gpio_pull_up(SBUS_OUT_GPIO);
		return CORE1_JOB_CLOCK;
	}
	uart_set_format(uart0, 8, 2, UART_PARITY_EVEN);
	// No pad carries UART0's RX during this claim.
	uart_hw_t *hw = uart_get_hw(uart0);
	hw->cr = hw->cr & ~UART_UARTCR_RXE_BITS;

	// The probe's line: the open parks the pad on its pull-down, the pull-up
	// follows in the same call, so the line rests high as between greetings.
	uint8_t err = escwirePortOpen(SBUS_OUT_GPIO, &s_line);
	if (err != 0) {
		uart_deinit(uart0);
		gpio_pull_up(SBUS_OUT_GPIO);
		return escwireJobBindError(err);
	}
	s_line.line(s_line.ctx, EW_LINE_UP);
	(void)ew_a_contact(s_greet);
	probeReset(millis());
	s_phase = SbusOutPhase::Probe;
	clearState(heldSeq());
	publish();
	return CORE1_JOB_OK;
}

/**
 * @brief The probe passed: GP0 from the escwire line to the UART. The close
 *        leaves an SIO input, low, on the pull-up (escwire_port.h); the
 *        pull-down takes the line to the UART's idle level before the UART
 *        has the pad. No command sent before this pass is taken.
 */
static void toUart() {
	escwirePortClose();
	gpio_pull_down(SBUS_OUT_GPIO);
	padToUart();
	s_phase = SbusOutPhase::Uart;
	clearState(heldSeq());
}

/**
 * @brief Judge the capture of one greeting, read at @p nowMs. Any byte, a
 *        bootloader's answer or not, and a capture not whole make the cycle
 *        prove no silence; only a bootloader's answer sets the cue and the
 *        counts.
 */
static void judge(const ew_capture_t *cap, uint32_t nowMs) {
	// An FC's S.BUS input sends nothing: a byte here is something that
	// transmits on GP0, a bootloader's garbled answer among the candidates.
	if (cap->n != 0 || cap->aborted || cap->overflow) s_clean = false;
	BlAnswer a = blAnswerClassify(s_reply, cap->n);
	bool efm8;
	if (sbusOutProbeAnswer(a, &efm8)) {
		s_answered = true;
		s_answerSeq++;
		s_answerMs = nowMs;
		if (efm8) {
			s_efm8 = true;
			s_efm8Seq++;
			s_signature = a.signature;
		}
	}
}

/** @brief A greeting's wait ended, or it did not go: the cycle counts it. */
static void greeted(uint32_t nowMs) {
	greetCycleSent(&s_cycle, nowMs);
	if (s_cycle.sent != 0) return;
	// The burst's last greeting: the cycle ends.
	s_cycles++;
	if (s_clean) toUart();
	s_clean = true;
}

/** @brief One pass of the probe: the quiet phase, a greeting, or its answer. */
static void probeStep(uint32_t nowMs) {
	if (s_greeting) {
		ew_capture_t cap;
		if (!s_line.poll(s_line.ctx, &cap)) return;
		s_greeting = false;
		// The driver parks the pad on its pull-down after a capture cut or
		// not whole (ew_wire.h): the line back on the pull-up at once, so
		// every quiet phase and every wait between greetings is high.
		s_line.line(s_line.ctx, EW_LINE_UP);
		judge(&cap, nowMs);
		greeted(nowMs);
		return;
	}
	if (greetCyclePhase(&s_cycle, nowMs) != GreetPhase::Greet) return;
	ew_xfer_t x;
	memset(&x, 0, sizeof(x));
	x.rate = EW_RATE_A;
	x.n_tx = EW_A_CONTACT_LEN;
	x.max_rx = SBUS_OUT_PROBE_RX_MAX;
	x.first_us = AM32_GREET_TMO_MS * 1000u;
	x.gap_us = EW_A_GAP_US;
	x.tx = s_greet;
	x.rx = s_reply;
	if (s_line.start(s_line.ctx, &x)) {
		s_greeting = true;
		return;
	}
	// The greeting did not go: this cycle proves nothing.
	s_clean = false;
	greeted(nowMs);
}

static void sbusOutJobStep(uint32_t nowMs, bool uiAlive) {
	if (s_phase == SbusOutPhase::Probe) {
		// No command is taken: START waits for a cycle with no answer.
		probeStep(nowMs);
		publish();
		return;
	}
	critical_section_enter_blocking(&s_cs);
	SbusOutCmd cmd = s_cmd;
	critical_section_exit(&s_cs);

	uint32_t nowUs = micros();
	uart_hw_t *hw = uart_get_hw(uart0);
	// MAX THROTTLE cannot change during a claim: SETTINGS opens only while
	// the claim is Free.
	sbusOutCoreTake(&s_core, &cmd, nowUs, uiAlive, sbusOutCeilRaw(settings()->maxThrottle));
	uint8_t out[SBUS_FRAME_BYTES];
	if (sbusOutCoreStep(&s_core, nowUs, uiAlive, (hw->fr & UART_UARTFR_TXFE_BITS) != 0, out)) {
		// 25 bytes into an empty 32-entry FIFO: the UART sends them back
		// to back.
		for (unsigned i = 0; i < SBUS_FRAME_BYTES; i++) hw->dr = out[i];
		s_sent = s_core.frame;
		s_lastMs = nowMs;
	}
	publish();
}

static bool sbusOutJobRelease(uint32_t nowMs) {
	if (s_phase == SbusOutPhase::Probe) {
		// A greeting on the wire is cut; GP0 stays an SIO input on the
		// pull-up and never reaches the UART.
		escwirePortClose();
		uart_deinit(uart0);
		s_phase = SbusOutPhase::Off;
		clearState(s_core.ackSeq);
		publish();
		return true;
	}
	// No frame is loaded from here on; the one on the wire completes.
	s_core.running = false;
	if (!s_releasing) {
		s_releasing = true;
		s_releaseMs = nowMs;
	}
	if ((uart_get_hw(uart0)->fr & UART_UARTFR_BUSY_BITS) &&
	    nowMs - s_releaseMs < SBUS_OUT_RELEASE_MAX_MS) {
		publish();
		return false;
	}
	// The pad leaves the UART before the UART stops.
	padRelease();
	uart_deinit(uart0);
	// No frame outlives the claim: a status read after it reads empty.
	s_phase = SbusOutPhase::Off;
	clearState(s_core.ackSeq);
	publish();
	return true;
}

const Core1JobOps SBUS_OUT_JOB_OPS = {
	sbusOutJobBind, sbusOutJobStep, sbusOutJobRelease, sbusOutJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

uint16_t sbusOutJobCommand(const SbusOutCmd *c) {
	critical_section_enter_blocking(&s_cs);
	bool keepSafe = s_cmd.kind == SbusOutCmdKind::Safe && s_status.ackSeq != s_cmd.seq;
	if (!keepSafe) {
		uint16_t seq = (uint16_t)(s_cmd.seq + 1);
		s_cmd = *c;
		s_cmd.seq = seq;
	}
	uint16_t held = s_cmd.seq;
	critical_section_exit(&s_cs);
	return held;
}

void sbusOutJobStatus(SbusOutStatus *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_status;
	critical_section_exit(&s_cs);
}

void sbusOutJobHeld(SbusOutCmd *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_cmd;
	critical_section_exit(&s_cs);
}

/**
 * @defgroup sbus_out_job_core0 Core0 only
 * @{
 */
/** @brief efm8Seq at the last sbusOutProbeRestart(): a later one marks the claim. */
static decltype(SbusOutStatus::efm8Seq) s_efm8Mark = 0;
/** @brief answerSeq when the run cue last went: a later one sets it again. */
static decltype(SbusOutStatus::answerSeq) s_cueMark = 0;
/** @} */

bool sbusOutProbePassed() {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	return st.phase == SbusOutPhase::Uart;
}

SbusOutProbeState sbusOutProbePoll() {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	// The claim's row has the leave page from the start; an EFM8 answer asks
	// for it as ESC SETTINGS does, so a row without it would still get it.
	if (st.efm8Seq != s_efm8Mark) pinClaimWantLeavePage(PinUser::SbusOut);
	switch (st.phase) {
		case SbusOutPhase::Off:   return SbusOutProbeState::Off;
		case SbusOutPhase::Uart:  return SbusOutProbeState::Passed;
		case SbusOutPhase::Probe: break;
	}
	return st.answered ? SbusOutProbeState::Answered : SbusOutProbeState::Checking;
}

bool sbusOutProbeEfm8() {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	return st.efm8Seq != s_efm8Mark;
}

bool sbusOutRunCue(uint32_t lastRpmMs) {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	if (st.answerSeq == s_cueMark) return false;
	// An eRPM frame stamped after the last answer: the firmware runs again.
	if (lastRpmMs != 0 && (int32_t)(lastRpmMs - st.answerMs) > 0) {
		s_cueMark = st.answerSeq;
		return false;
	}
	return true;
}

void sbusOutProbeRestart() {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	if (st.efm8Seq == s_efm8Mark) return;
	// The page's first step took the ESC's battery off.
	s_efm8Mark = st.efm8Seq;
	s_cueMark = st.answerSeq;
}
