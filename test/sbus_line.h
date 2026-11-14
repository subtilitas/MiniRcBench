/**
 * @file sbus_line.h
 * @brief A model of an S.BUS receiver's output, put on the UART0 fake.
 *
 * S.BUS is an inverted UART at 100,000 baud, 8 data bits, even parity and 2
 * stop bits (8E2): 12 bits of 10 us, 120 us a byte. A frame is 25 bytes:
 * the header 0x0F, 16 channels of 11 bits packed least significant bit first
 * across bytes 1 to 22, the flag byte and the footer. The model builds each
 * byte's bits after the start bit as the pad sees them once INOVER has
 * turned the line back the right way up, computes the even parity bit, and
 * turns them into the UARTDR word a PL011 receiving 8E2 loads: PE on a
 * parity mismatch, FE on a low first stop bit, BE and FE on a character
 * that is low throughout. The PL011 loads a character after its first stop
 * bit, 110 us after its start bit; the second stop bit it does not check.
 *
 * What the model can do wrong on purpose: a parity, framing or break fault
 * on one byte, a frame cut short, any footer and any flag byte. Overrun is
 * the UART fake's own, from a FIFO left full.
 *
 * @see docs/design/rx-signals.md, "Testing"
 */

#pragma once

#include "fakes.h"
#include "sbus.h"
#include "sbus_job.h"

#include <stdint.h>
#include <string.h>

/** @brief One byte on the line: 12 bits of 10 us. */
#define SBUS_LINE_BYTE_US 120u
/** @brief From a byte's start bit to the PL011 loading it: start, 8 data, parity, first stop. */
#define SBUS_LINE_LOAD_US 110u
/** @brief The two frame periods receivers send, us. */
#define SBUS_LINE_7MS  7000u
#define SBUS_LINE_14MS 14000u  /**< The slower period. */

/** @brief A fault the model puts on one byte. */
enum class SbusFault : uint8_t {
	None,     /**< The byte as sent. */
	Parity,   /**< The parity bit flipped: PE. */
	Framing,  /**< The first stop bit low: FE. */
	Break,    /**< The line low for the whole character: BE and FE, data 0. */
};

/** @brief One frame as a receiver sends it. */
struct SbusLineFrame {
	uint16_t  ch[SBUS_CHANNELS];  /**< Raw 0 to 2047. */
	uint8_t   flags;              /**< Byte 23: SBUS_FLAG_* bits. */
	uint8_t   footer;             /**< Byte 24; 0x00 by the specification. */
	int       faultAt;            /**< Byte that carries @c fault; -1 for none. */
	SbusFault fault;              /**< What is wrong with it. */
	int       bytes;              /**< Bytes sent: 25, or fewer for a cut frame. */
};

/**
 * @brief A whole, clean frame: channel i at @p base + 10 i, no flags, footer 0.
 * @param base Raw value of channel 1.
 */
static inline SbusLineFrame sbusLineFrame(uint16_t base) {
	SbusLineFrame f;
	memset(&f, 0, sizeof(f));
	for (unsigned i = 0; i < SBUS_CHANNELS; i++) f.ch[i] = (uint16_t)((base + 10u * i) & 0x7FFu);
	f.faultAt = -1;
	f.bytes = (int)SBUS_FRAME_BYTES;
	return f;
}

/** @brief The 25 bytes of @p f: header, packed channels, flags, footer. */
static inline void sbusLinePack(const SbusLineFrame &f, uint8_t out[SBUS_FRAME_BYTES]) {
	memset(out, 0, SBUS_FRAME_BYTES);
	out[0] = SBUS_HEADER;
	for (unsigned ch = 0; ch < SBUS_CHANNELS; ch++)
		for (unsigned b = 0; b < 11u; b++)
			if (f.ch[ch] >> b & 1u) {
				unsigned bit = ch * 11u + b;
				out[1u + bit / 8u] |= (uint8_t)(1u << (bit % 8u));
			}
	out[23] = f.flags;
	out[24] = f.footer;
}

/** @brief The even parity bit of @p b: 1 when @p b has an odd number of ones. */
static inline uint8_t sbusLineParity(uint8_t b) {
	uint8_t p = 0;
	for (; b; b &= (uint8_t)(b - 1u)) p ^= 1u;
	return p;
}

/**
 * @brief The UARTDR word a PL011 at 8E2 loads for one character.
 * @param data   The 8 data bits.
 * @param parity The parity bit as received.
 * @param stop   The first stop bit as received: 1 is correct.
 */
static inline uint16_t sbusLineReceive(uint8_t data, uint8_t parity, uint8_t stop) {
	if (data == 0 && parity == 0 && stop == 0) return SBUS_DR_BE | SBUS_DR_FE;
	uint16_t w = data;
	if (!stop) w |= SBUS_DR_FE;
	if (parity != sbusLineParity(data)) w |= SBUS_DR_PE;
	return w;
}

/** @brief The UARTDR word for byte @p b of a frame, with @p fault on it. */
static inline uint16_t sbusLineWord(uint8_t b, SbusFault fault) {
	uint8_t parity = sbusLineParity(b);
	switch (fault) {
		case SbusFault::Parity:  return sbusLineReceive(b, parity ^ 1u, 1);
		case SbusFault::Framing: return sbusLineReceive(b, parity, 0);
		case SbusFault::Break:   return sbusLineReceive(0, 0, 0);
		case SbusFault::None:    break;
	}
	return sbusLineReceive(b, parity, 1);
}

/**
 * @brief Put @p f on the UART0 fake, its start bit at @p startUs, one byte
 *        every 120 us.
 * @return The time the last byte sent ends.
 */
static inline uint64_t sbusLineSend(uint64_t startUs, const SbusLineFrame &f) {
	uint8_t b[SBUS_FRAME_BYTES];
	sbusLinePack(f, b);
	for (int i = 0; i < f.bytes && i < (int)SBUS_FRAME_BYTES; i++) {
		SbusFault fault = i == f.faultAt ? f.fault : SbusFault::None;
		fakeUartLine(startUs + (uint64_t)i * SBUS_LINE_BYTE_US + SBUS_LINE_LOAD_US,
		             sbusLineWord(b[i], fault));
	}
	return startUs + (uint64_t)f.bytes * SBUS_LINE_BYTE_US;
}

/**
 * @brief Put @p n copies of @p f on the line, one every @p periodUs from
 *        @p startUs.
 * @return The start of the frame that would follow.
 */
static inline uint64_t sbusLineStream(uint64_t startUs, uint32_t periodUs, int n,
                                      const SbusLineFrame &f) {
	for (int i = 0; i < n; i++) sbusLineSend(startUs + (uint64_t)i * periodUs, f);
	return startUs + (uint64_t)n * periodUs;
}
