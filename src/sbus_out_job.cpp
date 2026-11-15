/**
 * @file sbus_out_job.cpp
 * @brief The S.BUS output's core1 job. @see sbus_out_job.h
 */

#include "sbus_out_job.h"
#include "sbus_job.h"
#include "pin_claim.h"

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
                     bool uiAlive) {
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
			c->ceilRaw = clampRaw(cmd->ceilRaw, SBUS_OUT_RAW_HI);
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
/** @} */

/** @brief Hand the status to core0. */
static void publish() {
	SbusOutStatus st;
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

/** @brief Core1's state after a bind or a release: SAFE, nothing sent. */
static void clearState(uint16_t ackSeq) {
	sbusOutCoreReset(&s_core, ackSeq);
	memset(&s_sent, 0, sizeof(s_sent));
	s_lastMs = 0;
	s_releasing = false;
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

	// The DShot teardown leaves GP0 an input with the pull-up on: the
	// pull-down holds the line at its idle level from here on.
	gpio_pull_down(SBUS_OUT_GPIO);
	if (!sbusBaudOk(uart_init(uart0, SBUS_BAUD))) {
		uart_deinit(uart0);
		padRelease();
		return CORE1_JOB_CLOCK;
	}
	uart_set_format(uart0, 8, 2, UART_PARITY_EVEN);
	// No pad carries UART0's RX during this claim.
	uart_hw_t *hw = uart_get_hw(uart0);
	hw->cr = hw->cr & ~UART_UARTCR_RXE_BITS;
	padToUart();

	critical_section_enter_blocking(&s_cs);
	uint16_t seq = s_cmd.seq;
	critical_section_exit(&s_cs);
	clearState(seq);
	publish();
	return CORE1_JOB_OK;
}

static void sbusOutJobStep(uint32_t nowMs, bool uiAlive) {
	critical_section_enter_blocking(&s_cs);
	SbusOutCmd cmd = s_cmd;
	critical_section_exit(&s_cs);

	uint32_t nowUs = micros();
	uart_hw_t *hw = uart_get_hw(uart0);
	sbusOutCoreTake(&s_core, &cmd, nowUs, uiAlive);
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
