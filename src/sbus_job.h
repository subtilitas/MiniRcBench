/**
 * @file sbus_job.h
 * @brief The S.BUS monitor's core1 job: UART0 receives on GP1, the pad's
 *        input inverted, while the pin claim holds it.
 *
 * S.BUS is Futaba's serial receiver bus: an inverted UART (universal
 * asynchronous receiver-transmitter) at 100,000 baud, 8 data bits, even
 * parity and 2 stop bits (8E2), 16 channels of 11 bits and two digital ones
 * in a 25-byte frame. The job only listens. It sets GP1 to the UART function
 * with the pad's input inverted (INOVER) and the pull-down on, and never makes
 * a pin an output: UART0's transmit pin is GP0, whose function it leaves
 * alone, so the transmitter reaches no pad.
 *
 * Every pass of `loop1()` the job reads micros() once, drains the receive
 * FIFO (first-in, first-out buffer) while UARTFR.RXFE is clear, hands each
 * UARTDR word to sbusCoreFeed() with that time, and publishes the status
 * under its critical section when a counted word arrived. A byte's time is
 * the time of the pass that drains it. A bind can land in the middle of a
 * frame: the bind empties the FIFO after uart_set_format(), and nothing is
 * counted or fed until the line has been seen quiet for 2000 us
 * (sbusCoreReset()). The decoder, rcbench's sbus_decode_byte()
 * (lib/rcbench/protocols/sbus/), frames on a gap of 2000 us (SBUS_GAP_US).
 *
 * | UARTDR flag | Count | The byte |
 * |---|---|---|
 * | BE, break | SbusStatus::breakErr | not fed |
 * | FE, framing | SbusStatus::framingErr | not fed |
 * | PE, parity | SbusStatus::parityErr | not fed |
 * | OE, overrun | SbusStatus::overrun | fed, unless another flag is set |
 *
 * One bad word is one count: a break outranks a framing error, which
 * outranks a parity error. Parity is the line's only check, so a flagged
 * byte never becomes a channel value; the frame it belonged to is short when
 * the gap comes, and the decoder's gap rule discards it and counts a resync.
 * An overrun's byte arrived intact; the bytes lost before it leave the frame
 * short, which the gap rule discards the same way.
 *
 * The heartbeat stops nothing: the job sends nothing. The release turns
 * UART0 off, leaves GP1 an input with no inversion and the pull-down on, and
 * publishes a cleared status, at once.
 *
 * Not run on hardware: the inversion on the pin and the PL011's flags on a
 * real line are not measured.
 *
 * @see docs/design/rx-signals.md
 */

#pragma once

#include <stdint.h>

#include "core1_job.h"
#include "sbus.h"

/** @brief UART0's receive pin, the one GPIO the job binds. */
#define SBUS_GPIO 1u
/** @brief The line's rate, baud. */
#define SBUS_BAUD 100000u
/** @brief The bind refuses a rate further than this from @ref SBUS_BAUD, in percent. */
#define SBUS_BAUD_TOL_PCT 1u
/** @brief Intervals between frames that SbusStatus::intervalUs averages. */
#define SBUS_INTERVALS 16u
/**
 * @brief No frame for this long is a lost link, ms: seven frames at 14 ms.
 *
 * The screen's link state uses it (docs/design/rx-signals.md), and an
 * interval this long or longer is no frame period: it starts the mean of
 * SbusStatus::intervalUs again.
 */
#define SBUS_LOST_MS 100u

/** @brief UARTDR flags: framing, parity, break and overrun error (PL011). */
#define SBUS_DR_FE 0x100u
#define SBUS_DR_PE 0x200u  /**< Parity error. */
#define SBUS_DR_BE 0x400u  /**< Break. */
#define SBUS_DR_OE 0x800u  /**< Overrun: the FIFO was full and a byte was lost before this one. */

/**
 * @brief What the job publishes for core0. Every count starts at 0 at each
 *        bind and counts from the first quiet line after it; the release
 *        clears it all.
 */
struct SbusStatus {
	uint16_t channel[SBUS_CHANNELS];  /**< Raw 0 to 2047, from the last frame. */
	bool     ch17;            /**< The last frame's digital channel 17. */
	bool     ch18;            /**< The last frame's digital channel 18. */
	bool     failsafe;        /**< The last frame carried the failsafe bit. */
	bool     frameLost;       /**< The last frame carried the frame-lost bit. */
	uint32_t frames;          /**< Frames decoded since the bind. */
	uint32_t lastFrameMs;     /**< millis() of the pass that decoded the last frame; 0 before the first. */
	uint32_t intervalUs;      /**< Mean of the last @ref SBUS_INTERVALS intervals between frames; 0 before two. */
	uint32_t lostFrames;      /**< Frames that carried the frame-lost bit. */
	uint32_t failsafeFrames;  /**< Frames that carried the failsafe bit. */
	uint32_t bytes;           /**< Clean bytes fed to the decoder. */
	uint32_t lastByteMs;      /**< millis() of the pass that fed the last clean byte; 0 before the first. */
	uint32_t parityErr;       /**< Words with PE and neither BE nor FE. */
	uint32_t framingErr;      /**< Words with FE and not BE. */
	uint32_t breakErr;        /**< Words with BE. */
	uint32_t overrun;         /**< Words with OE. */
	uint32_t resyncs;         /**< The decoder's resyncs: bytes thrown away looking for a frame. */
	uint32_t badFooter;       /**< The decoder's frames of the right length with a footer it refuses. */
};

/**
 * @brief Core1's state for the line. Plain data, so the host suite drives
 *        sbusCoreFeed() and sbusCoreStep() directly.
 */
struct SbusCore {
	sbus_decoder_t dec;                  /**< rcbench's decoder. */
	SbusStatus     st;                   /**< What the next publish carries. */
	uint32_t       lastFrameUs;          /**< Pass time of the last frame. */
	uint32_t       iv[SBUS_INTERVALS];   /**< The last intervals, a ring. */
	uint8_t        ivNext;               /**< Where the next interval goes. */
	uint8_t        ivCount;              /**< Intervals in the ring. */
	bool           fedByte;              /**< A clean byte since the last step. */
	bool           fedFrame;             /**< A frame since the last step. */
	bool           changed;              /**< A counted word since the last step. */
	bool           synced;               /**< A quiet line of `SBUS_GAP_US` (sbus.h) was seen since the reset. */
	bool           passWord;             /**< A word arrived since the last step. */
	uint32_t       wordPassUs;           /**< Pass time of the last pass with a word, or of the reset. */
	uint32_t       quietUs;              /**< Line seen quiet since then, us: from that pass to the last pass with none. */
};

/**
 * @brief Clear @p c: the decoder, the status and every count, and wait for
 *        a quiet line. Pure.
 *
 * A bind can land in the middle of a frame, and on the chip uart_init()
 * receives at 8N1 until uart_set_format() takes 8E2. So nothing is counted
 * or fed until the line has been seen quiet for `SBUS_GAP_US` (sbus.h),
 * 2000 us: the time from the last pass with a word, or from the reset, to a
 * pass with none. The first word after that starts a frame. A line that is never
 * quiet for 2000 us is never counted.
 *
 * @param[out] c The state.
 * @param nowUs  micros() at the reset.
 */
void sbusCoreReset(SbusCore *c, uint32_t nowUs);

/**
 * @brief Take one UARTDR word. Pure.
 *
 * Before the line has been seen quiet (sbusCoreReset()), the word is neither
 * counted nor fed. After that it counts its flags as the table in this
 * file's description says; feeds a clean byte, and one with OE alone, to
 * sbus_decode_byte() at @p nowUs; on a frame, stores its channels and flags,
 * counts the frame-lost and failsafe bits and adds the interval since the
 * last frame. An interval of @ref SBUS_LOST_MS or more empties the ring of
 * intervals instead.
 *
 * @param[in,out] c The state.
 * @param dr        The word: the byte in bits 0 to 7, FE, PE, BE and OE in
 *                  bits 8 to 11.
 * @param nowUs     micros() of the pass. The clock may wrap.
 */
void sbusCoreFeed(SbusCore *c, uint16_t dr, uint32_t nowUs);

/**
 * @brief End a pass. Pure.
 *
 * Stamps SbusStatus::lastByteMs and SbusStatus::lastFrameMs with @p nowMs
 * when the pass fed a clean byte or a frame. Until the line has been seen
 * quiet, records how long it has been: a pass with no word extends the quiet
 * time to @p nowUs, a pass with one starts it again.
 *
 * @param[in,out] c The state.
 * @param nowUs     micros() of the pass, as given to sbusCoreFeed().
 * @param nowMs     millis() of the pass.
 * @return True when a counted word arrived since the last step: the status
 *         changed and is to be published.
 */
bool sbusCoreStep(SbusCore *c, uint32_t nowUs, uint32_t nowMs);

/**
 * @brief Whether a rate uart_init() returned is within @ref SBUS_BAUD_TOL_PCT
 *        of @ref SBUS_BAUD. Pure.
 * @param baud The rate, baud; 0 when the UART has no clock.
 * @return True from 99,000 to 101,000 baud.
 */
static inline bool sbusBaudOk(uint32_t baud) {
	uint32_t off = baud > SBUS_BAUD ? baud - SBUS_BAUD : SBUS_BAUD - baud;
	return off * 100u <= SBUS_BAUD * SBUS_BAUD_TOL_PCT;
}

/**
 * @brief The frame rate for a mean interval, in tenths of a hertz, rounded
 *        to the nearest. Pure.
 * @param intervalUs SbusStatus::intervalUs.
 * @return 714 at 14,000 us, 1429 at 7,000 us; 0 for an interval of 0.
 */
static inline uint32_t sbusRateDeciHz(uint32_t intervalUs) {
	return intervalUs ? (10000000u + intervalUs / 2u) / intervalUs : 0u;
}

/**
 * @brief The roles an S.BUS claim asks for: the role that is GP1. Pure.
 *
 * | Wiring | Roles |
 * |---|---|
 * | SIG GP0, AUX GP1 | PIN_SIG \| PIN_AUX |
 * | SIG GP1, AUX GP0 | PIN_SIG |
 * | SIG GP1, AUX GP1 (KISS off) | PIN_SIG |
 * | SIG GP0, AUX GP0 (KISS off) | 0: no claim; the screen of slice 3 is to say S.BUS needs GP1 |
 *
 * @param sigGpio escTaskDshotPin().
 * @param auxGpio escTaskKissPin().
 * @return PIN_SIG | PIN_AUX, PIN_SIG, or 0 (pin_claim.h).
 */
uint8_t sbusClaimRoles(uint8_t sigGpio, uint8_t auxGpio);

/**
 * @brief Read the job's status. Core0, under the job's critical section.
 * @param[out] out The status as core1 last published it.
 */
void sbusJobStatus(SbusStatus *out);

/** @brief The S.BUS job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps SBUS_JOB_OPS;
