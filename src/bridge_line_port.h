/**
 * @file bridge_line_port.h
 * @brief The USB bridge's line on the SIG pin: two PIO state machines in one
 *        block, the transmitter of bridge_line.pio and the receiver of
 *        pio_uart_rx.pio, at 19200 baud 8N1.
 *
 * bridge_line_port.cpp is the firmware's. The host suite links a fake in its
 * place, a wire on the virtual clock with a model ESC
 * (test/fake_bridge_line.h); test/pio_bridge_emu.py runs the two programs
 * themselves. The bridge's job (bridge_wire.h) calls these on core1 only,
 * between its bind and its release.
 *
 * | Resource | Count |
 * |---|---|
 * | PIO state machines | 2, in the block the transmitter lands in |
 * | Instruction words | 7 (`bridge_tx`) + 9 (`pio_uart_rx`) = 16 |
 * | DMA channels | 0 |
 * | FIFOs | the transmitter's joined to 8 words, the receiver's to 8 |
 *
 * Both machines run from one clock divider and are enabled in the same
 * cycle, so the receiver's echo of a byte is in its FIFO, 3 cycles before
 * the transmitter waits for the next byte (pio_bridge_emu.py). The pad is
 * an input with its pull-up whenever the transmitter does not drive it:
 * from the open, between frames, and after the close.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Take @p gpio for the line. The pad stays an input with its pull-up
 *        until the first byte.
 * @param gpio The SIG GPIO.
 * @return CORE1_JOB_OK; CORE1_JOB_CLOCK when bridgeLineClock() refuses
 *         `clk_sys`; CORE1_JOB_PIO when the block that takes the transmitter
 *         has no second state machine or no room for the receiver. On an
 *         error nothing is claimed and the pad is not touched.
 */
uint8_t bridgeLineOpen(uint8_t gpio);

/**
 * @brief Give back what the open took: the pad first, an input with its
 *        pull-up, then both state machines and both programs. A byte on the
 *        wire is cut. Safe while closed.
 */
void bridgeLineClose();

/** @brief Whether the line is open. */
bool bridgeLineIsOpen();

/**
 * @brief Put one word in the transmit FIFO: a byte in bits 0 to 7, and
 *        @ref BRIDGE_LINE_KEEP when another byte of its frame follows.
 * @param word The word.
 * @return False, and nothing put, while the FIFO is full.
 */
bool bridgeLinePut(uint16_t word);

/**
 * @brief Take the oldest received byte.
 * @param out Where it goes.
 * @return False while the receive FIFO is empty.
 */
bool bridgeLineGet(uint8_t *out);

/**
 * @brief Whether nothing is under way: the transmit FIFO empty, the
 *        transmitter waiting on `pull`, the receiver waiting for a start bit.
 *
 * Read the receive FIFO after this call: when it returns true, the echo of
 * every byte sent is in the FIFO already.
 *
 * @return True while the line is idle.
 */
bool bridgeLineIdle();

/**
 * @brief Whether the receiver stalled on a full FIFO since the last call
 *        (FDEBUG's RXSTALL bit, cleared by this call).
 * @return True once per stall seen.
 */
bool bridgeLineTakeOverflow();
