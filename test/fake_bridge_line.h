/**
 * @file fake_bridge_line.h
 * @brief The host suite's USB bridge line: bridge_line_port.h on a wire of
 *        the virtual clock, with the echo and a model ESC.
 *
 * Time is counted in state machine cycles, 8 a bit at 19200 baud, from
 * time_us_32(): a cycle is 6.51 µs. The cycle counts are the ones
 * test/pio_bridge_emu.py measures on the two PIO programs:
 *
 * | Event | Cycle |
 * |---|---|
 * | the transmitter takes a word | when it is on `pull`, and not before the cycle after the word arrives |
 * | its start bit | 1 cycle later: s |
 * | the echo in the receive FIFO | s + 78 |
 * | the pad let go, for a word without @ref BRIDGE_LINE_KEEP | s + 80 |
 * | the transmitter on `pull` again | s + 81 |
 *
 * The model ESC answers each frame, at the pad's release, with what the
 * responder returns, its first start bit the responder's delay later and
 * the rest back to back, 80 cycles apart; each byte reaches the receive
 * FIFO 78 cycles after its start bit. A byte that finds the receive FIFO
 * full is lost and sets the overflow flag. The fake counts a cycle in
 * which the ESC and the bridge both drive the pad as a conflict. It knows
 * nothing of the bits inside a byte: the PIO model does.
 */

#pragma once

#include <stdint.h>

/**
 * @brief The model ESC: given the frame just sent, write the reply into
 *        @p reply (room for 1100 bytes) and its delay after the release
 *        into @p delayUs.
 * @return The reply's length; 0 for none.
 */
typedef int (*FakeEscReply)(const uint8_t *frame, int len, uint8_t *reply, uint32_t *delayUs);

/** @brief Closed, no responder, nothing recorded, opens accepted. */
void fakeBridgeLineReset();

/** @brief Make the next opens fail with @p err, a Core1JobError; 0 accepts. */
void fakeBridgeLineRefuse(uint8_t err);

/** @brief The model ESC's responder; null: it never answers. */
void fakeBridgeLineResponder(FakeEscReply r);

/** @brief The echo of byte @p n sent since the open reads @p value. */
void fakeBridgeLineCorruptEcho(int n, uint8_t value);

/** @brief The echo of byte @p n sent since the open never comes; -1 for every byte. */
void fakeBridgeLineDropEcho(int n);

/** @brief The transmitter stops taking words while @p on, so the line never goes idle. */
void fakeBridgeLineStall(bool on);

/** @brief Opens that succeeded since the reset. */
int fakeBridgeLineOpens();

/** @brief Closes of an open line since the reset. */
int fakeBridgeLineCloses();

/** @brief The GPIO of the last open. */
uint8_t fakeBridgeLinePin();

/** @brief Closes that cut a byte on the wire or left words in the FIFO. */
int fakeBridgeLineCuts();

/** @brief Cycles in which the ESC and the bridge both drove the pad. */
int fakeBridgeLineConflicts();

/** @brief Words put while the line was closed: must stay 0. */
int fakeBridgeLineMisuse();

/** @brief Bytes the bridge has put on the wire since the reset. */
int fakeBridgeLineWireCount();

/**
 * @brief One byte the bridge put on the wire.
 * @param i       Its index since the reset.
 * @param b       Out: the byte.
 * @param startCy Out: its start bit's cycle.
 * @param keep    Out: whether the pad stayed driven after it.
 */
void fakeBridgeLineWire(int i, uint8_t *b, uint64_t *startCy, bool *keep);

/** @brief Times the pad was let go after a frame since the reset. */
int fakeBridgeLineReleases();

/** @brief The cycle count now, as the fake counts it. */
uint64_t fakeBridgeLineCycle();
