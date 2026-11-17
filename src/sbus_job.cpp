/**
 * @file sbus_job.cpp
 * @brief The S.BUS monitor's core1 job. @see sbus_job.h
 */

#include "cs_shared.h"
#include "sbus_job.h"
#include "pin_claim.h"

#include "plat.h"
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <pico/critical_section.h>
#include <string.h>

static_assert(SBUS_DR_FE == UART_UARTDR_FE_BITS, "UARTDR framing error bit");
static_assert(SBUS_DR_PE == UART_UARTDR_PE_BITS, "UARTDR parity error bit");
static_assert(SBUS_DR_BE == UART_UARTDR_BE_BITS, "UARTDR break bit");
static_assert(SBUS_DR_OE == UART_UARTDR_OE_BITS, "UARTDR overrun bit");
static_assert(SBUS_INTERVALS <= 255u, "the ring's count is a byte");

// ---------------------------------------------------------------------------
// the pure core
// ---------------------------------------------------------------------------

void sbusCoreReset(SbusCore *c, uint32_t nowUs) {
	memset(c, 0, sizeof(*c));
	sbus_decoder_reset(&c->dec);
	c->wordPassUs = nowUs;
}

/** @brief Add one interval between frames, or start the mean again after a lost link. */
static void addInterval(SbusCore *c, uint32_t us) {
	if (us >= SBUS_LOST_MS * 1000u) {
		c->ivCount = 0;
		c->ivNext = 0;
		c->st.intervalUs = 0;
		return;
	}
	c->iv[c->ivNext] = us;
	c->ivNext = (uint8_t)((c->ivNext + 1u) % SBUS_INTERVALS);
	if (c->ivCount < SBUS_INTERVALS) c->ivCount++;
	// At most 16 intervals under 100,000 us: the sum fits.
	uint32_t sum = 0;
	for (uint8_t i = 0; i < c->ivCount; i++) sum += c->iv[i];
	c->st.intervalUs = (sum + c->ivCount / 2u) / c->ivCount;
}

void sbusCoreFeed(SbusCore *c, uint16_t dr, uint32_t nowUs) {
	c->passWord = true;
	// Before a quiet line the word may be the tail of a frame, or a
	// character the UART caught in the middle or at 8N1: not counted.
	if (!c->synced) {
		if (c->quietUs < SBUS_GAP_US) return;
		c->synced = true;
	}
	c->changed = true;
	if (dr & SBUS_DR_OE) c->st.overrun++;
	if (dr & SBUS_DR_BE) { c->st.breakErr++; return; }
	if (dr & SBUS_DR_FE) { c->st.framingErr++; return; }
	if (dr & SBUS_DR_PE) { c->st.parityErr++; return; }

	c->st.bytes++;
	c->fedByte = true;
	sbus_frame_t f;
	bool frame = sbus_decode_byte(&c->dec, (uint8_t)dr, nowUs, &f);
	c->st.resyncs = c->dec.resyncs;
	c->st.badFooter = c->dec.bad_footer;
	if (!frame) return;

	if (c->st.frames) addInterval(c, nowUs - c->lastFrameUs);
	c->lastFrameUs = nowUs;
	c->fedFrame = true;
	c->st.frames = c->dec.frames;
	memcpy(c->st.channel, f.channel, sizeof(c->st.channel));
	c->st.ch17 = f.ch17;
	c->st.ch18 = f.ch18;
	c->st.frameLost = f.frame_lost;
	c->st.failsafe = f.failsafe;
	if (f.frame_lost) c->st.lostFrames++;
	if (f.failsafe) c->st.failsafeFrames++;
}

bool sbusCoreStep(SbusCore *c, uint32_t nowUs, uint32_t nowMs) {
	if (c->passWord) {
		c->wordPassUs = nowUs;
		c->quietUs = 0;
	} else {
		c->quietUs = nowUs - c->wordPassUs;
	}
	c->passWord = false;
	if (c->fedByte) c->st.lastByteMs = nowMs;
	if (c->fedFrame) c->st.lastFrameMs = nowMs;
	bool changed = c->changed;
	c->fedByte = c->fedFrame = c->changed = false;
	return changed;
}

uint8_t sbusClaimRoles(uint8_t sigGpio, uint8_t auxGpio) {
	if (sigGpio == SBUS_GPIO) return PIN_SIG;
	if (auxGpio == SBUS_GPIO) return PIN_SIG | PIN_AUX;
	return 0;
}

// ---------------------------------------------------------------------------
// the job on core1, and the block it shares with core0
// ---------------------------------------------------------------------------

/** @brief Guards the shared block below. */
static critical_section_t s_cs;
/** @brief What core1 last published. Under s_cs. */
static SbusStatus s_status;

/** @brief Core1 only: the line's state. */
static SbusCore s_core;

/** @brief Hand the status to core0. */
static void publish() {
	critical_section_enter_blocking(&s_cs);
	s_status = s_core.st;
	critical_section_exit(&s_cs);
}

/**
 * @brief GP1 back to a plain input: no inversion, the pull-down on, as the
 *        servo job leaves its pins. gpio_set_function() zeroes the override
 *        as well; it is cleared first anyway, so no step relies on that.
 */
static void padRelease() {
	gpio_set_inover(SBUS_GPIO, GPIO_OVERRIDE_NORMAL);
	gpio_set_function(SBUS_GPIO, GPIO_FUNC_SIO);
	gpio_set_dir(SBUS_GPIO, GPIO_IN);
	gpio_pull_down(SBUS_GPIO);
}

/** @brief Read and discard every word in the receive FIFO. */
static void drain() {
	uart_hw_t *hw = uart_get_hw(uart0);
	while (!(hw->fr & UART_UARTFR_RXFE_BITS)) (void)(uint32_t)hw->dr;
}

static void sbusJobInit() {
	csInitShared(&s_cs);
}

static uint8_t sbusJobBind(uint8_t sigGpio, uint8_t auxGpio) {
	if (sigGpio != SBUS_GPIO && auxGpio != SBUS_GPIO) return CORE1_JOB_NO_GP1;

	// The order matters: gpio_set_function() writes the whole control
	// register and zeroes the overrides (SDK 2.1.1, gpio.c), so INOVER comes
	// after it. The pull-down holds the idle line low with nothing plugged in.
	gpio_set_function(SBUS_GPIO, GPIO_FUNC_UART);
	gpio_set_inover(SBUS_GPIO, GPIO_OVERRIDE_INVERT);
	gpio_pull_down(SBUS_GPIO);
	if (!sbusBaudOk(uart_init(uart0, SBUS_BAUD))) {
		uart_deinit(uart0);
		padRelease();
		return CORE1_JOB_CLOCK;
	}
	uart_set_format(uart0, 8, 2, UART_PARITY_EVEN);
	// Whatever arrived before the format took effect was read at 8N1, or
	// from the middle of a character: away with it.
	drain();

	sbusCoreReset(&s_core, micros());
	publish();
	return CORE1_JOB_OK;
}

static void sbusJobStep(uint32_t nowMs, bool) {
	// One time for every byte of the pass: a pass is far shorter than the
	// 2000 us gap, so the bytes of one frame never look a gap apart.
	uint32_t nowUs = micros();
	uart_hw_t *hw = uart_get_hw(uart0);
	while (!(hw->fr & UART_UARTFR_RXFE_BITS))
		sbusCoreFeed(&s_core, (uint16_t)hw->dr, nowUs);
	if (sbusCoreStep(&s_core, nowUs, nowMs)) publish();
}

static bool sbusJobRelease(uint32_t) {
	// Nothing is driven, so nothing needs time: let go at once.
	uart_deinit(uart0);
	padRelease();
	// No frame outlives the claim: a status read after it reads empty.
	sbusCoreReset(&s_core, micros());
	publish();
	return true;
}

const Core1JobOps SBUS_JOB_OPS = {
	sbusJobBind, sbusJobStep, sbusJobRelease, sbusJobInit,
};

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

void sbusJobStatus(SbusStatus *out) {
	critical_section_enter_blocking(&s_cs);
	*out = s_status;
	critical_section_exit(&s_cs);
}
