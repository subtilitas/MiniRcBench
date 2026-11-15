/**
 * @file bridge_wire.h
 * @brief The USB bridge's wire: core1's job on the ESC signal pin, and the
 *        frame slot and the byte ring that carry its bytes between the
 *        cores.
 *
 * The USB bridge (docs/design/usb-bridge.md) connects a desktop ESC tool on
 * the board's USB serial port to the ESC's bootloader on SIG, at 19200 baud
 * 8N1. Core0 owns USB; core1 owns the pin. This file is the core1 half and
 * the two buffers between them, ported from DshotDisplay's `bridge_wire.*`
 * at 6693084 with the blocking calls taken out and the bit-banged transport
 * replaced by a PIO line (bridge_line_port.h).
 *
 * | Part | Writer | Reader | Size |
 * |---|---|---|---|
 * | Frame slot to the wire | core0, bridgeWireSend() | core1, the job | @ref BRIDGE_FRAME_MAX bytes, one frame |
 * | Ring from the wire | core1, the job | core0, bridgeWireRecv() | @ref BRIDGE_RX_RING bytes |
 * | Counts (BridgeWireStats) | core1 | core0, bridgeWireStats() | |
 *
 * A frame goes to the wire whole: core1 takes it from the slot and keeps the
 * transmit FIFO filled until its last byte is in, so its bytes go out back to
 * back, 82 state machine cycles, 10.25 bit times, apart (bridge_line.pio).
 * After a pass at most 8 bytes, 4.27 ms at 82 cycles each, are on the wire
 * or queued; a later pass leaves the pad driven high between two bytes. The slot is free again when the last
 * byte is in the FIFO. The ring absorbs core0's repaints: 1024 bytes are
 * 533 ms of wire traffic.
 *
 * Both buffers have one writer per index or flag and no lock: each store
 * that publishes data is a release, each load that reads it an acquire, as
 * DshotDisplay's `WIRE_PUBLISH()` and `WIRE_OBSERVE()` do.
 *
 * **The echo.** The receiver samples the pad while the transmitter drives
 * it, so each byte sent comes back. The job keeps the bytes it put in the
 * FIFO, at most @ref BRIDGE_ECHO_MAX, and takes the next bytes received as
 * their echo, in order: one that differs from its byte makes its frame a
 * collision, the ESC driving the line while the bench did. Once the line is
 * idle (bridgeLineIdle()), every echo has been received, so an echo still
 * owed never came, and makes its frame a collision too. A frame counts
 * once: an echo lost inside a frame shifts the rest of it. When core1 comes
 * to an idle line late, with the ESC's first answer byte already in the
 * FIFO, 0.81 ms after the release for an ESC that answers after 0.3 ms, a
 * lost echo is not found at idle: that byte is taken for it, and in the
 * 4-way interface dropped. With bridgeWireKeepEcho(true),
 * passthrough's setting and the default, an echo goes into the ring like any
 * byte; with false, the 4-way interface's, it does not.
 *
 * **The job.** Core1Job::Bridge, PinUser::Bridge's job (pin_claim.h):
 *
 * | Call | What it does |
 * |---|---|
 * | bind | empties both buffers, zeroes the counts, opens the line on SIG; sends are taken from then on |
 * | step | receives, counts, writes off missing echoes at idle, feeds the transmit FIFO from the slot |
 * | release | refuses sends, drops a frame not started, finishes the one under way, then closes the line once it is idle, or after @ref BRIDGE_RELEASE_MAX_MS |
 *
 * The heartbeat (core1UiAlive()) changes nothing: DShot is off, and the job
 * sends only what core0 put in the slot, so with core0 gone nothing is sent.
 *
 * Nothing on core0 opens the bridge yet: slice 2 of the design note adds the
 * USB side and the screen.
 *
 * @see docs/design/usb-bridge.md, "Cores and buffers"
 */

#pragma once

#include <stdint.h>

#include "core1_job.h"

/** @brief The wire's rate, baud, 8N1. */
#define BRIDGE_BAUD 19200u

/** @brief State machine cycles per bit, fixed by bridge_line.pio and pio_uart_rx.pio. */
#define BRIDGE_CYCLES_PER_BIT 8u

/** @brief The largest frame the slot takes, bytes: a 256-byte payload with its headers. */
#define BRIDGE_FRAME_MAX 320u

/** @brief The ring from the wire, bytes. A power of two. */
#define BRIDGE_RX_RING 1024u
static_assert((BRIDGE_RX_RING & (BRIDGE_RX_RING - 1u)) == 0, "the ring's index masks");

/**
 * @brief Bytes sent whose echo the job waits for at most: the receive FIFO's
 *        depth, so the line's own echo never fills it while core1 is away.
 *
 * Right after a pass that are the byte on the wire and 7 in the transmit
 * FIFO: 8 bytes, 4.27 ms of sending at 82 cycles each. The job puts no more until echoes come back, or
 * the line is idle and those still owed are written off.
 */
#define BRIDGE_ECHO_MAX 8u

/** @brief Bit 8 of a word for the transmitter: the pad stays driven after the byte. */
#define BRIDGE_LINE_KEEP 0x100u

/**
 * @brief The release closes the line after this long at most, ms, whether or
 *        not the frame under way is out.
 *
 * A full slot, 320 bytes 82 cycles apart at 8 cycles a bit, takes 171 ms;
 * DshotDisplay's `bridgeWireEnd()` waited up to 250 ms for its core1 too.
 */
#define BRIDGE_RELEASE_MAX_MS 250u
static_assert(BRIDGE_RELEASE_MAX_MS * BRIDGE_BAUD * BRIDGE_CYCLES_PER_BIT >
              BRIDGE_FRAME_MAX * 82u * 1000u,
              "the release would cut a full frame");

/** @brief What the job counted since its bind. Core1 writes each, core0 reads. */
struct BridgeWireStats {
	uint32_t sent;        /**< Bytes put in the transmit FIFO. */
	uint32_t received;    /**< Bytes the receiver took off the wire, echoes included. */
	uint32_t frames;      /**< Frames taken from the slot. */
	uint32_t lost;        /**< Bytes lost: the ring full, or the receive FIFO full (one per stall seen). */
	uint32_t collisions;  /**< Frames whose echo differed from the bytes sent, or came short. */
};

/**
 * @brief The PIO clock divider for @ref BRIDGE_BAUD at @ref BRIDGE_CYCLES_PER_BIT
 *        cycles a bit. Pure; host-testable.
 *
 * Rounded to the divider's 1/256 steps: at 150 MHz it is 976 + 144/256,
 * exact.
 *
 * @param sysHz   `clk_sys`, Hz.
 * @param divInt  Out: the integer part, 1 to 27,962 for any `clk_sys`.
 * @param divFrac Out: the fraction, in 1/256.
 * @return False when the divider would be below 1, `clk_sys` below
 *         153,300 Hz; then the outputs are not written.
 */
bool bridgeLineClock(uint32_t sysHz, uint16_t *divInt, uint8_t *divFrac);

/**
 * @brief Whether the line is bound and sends are taken. Core0.
 * @return True from the job's bind to the start of its release.
 */
bool bridgeWireOpen();

/**
 * @brief Hand one frame to core1. Core0.
 *
 * Copies the frame into the slot. Refused while the line is not open, for
 * an empty frame or one longer than @ref BRIDGE_FRAME_MAX, and while the last
 * frame is not all in the transmit FIFO: the caller keeps it and tries again.
 *
 * @param data The frame.
 * @param len  Its length, 1 to @ref BRIDGE_FRAME_MAX.
 * @return True when the slot took it.
 */
bool bridgeWireSend(const uint8_t *data, uint16_t len);

/**
 * @brief Whether the slot takes a frame now. Core0.
 * @return True while the line is open and no frame waits in the slot.
 */
bool bridgeWireSlotFree();

/**
 * @brief Bytes waiting in the ring. Core0.
 * @return 0 to @ref BRIDGE_RX_RING; 0 while the line is not open.
 */
uint16_t bridgeWireAvailable();

/**
 * @brief Take bytes from the ring, oldest first. Core0. Never blocks.
 *
 * Takes nothing while the line is not open: the next bind empties the ring
 * from core1, so core0 keeps off its indices from the release on.
 *
 * @param buf    Where they go.
 * @param maxLen Room in @p buf.
 * @return Bytes copied.
 */
uint16_t bridgeWireRecv(uint8_t *buf, uint16_t maxLen);

/**
 * @brief Whether an echo goes into the ring. Core0; core1 reads it for each
 *        echo. True from start-up; a bind does not change it.
 * @param keep True for passthrough, false for the 4-way interface.
 */
void bridgeWireKeepEcho(bool keep);

/**
 * @brief The job's counts since its bind. Core0. Each count is read whole;
 *        the five are not one snapshot.
 * @param out Where they go.
 */
void bridgeWireStats(BridgeWireStats *out);

/** @brief Core1Job::Bridge: bind, step and release as the table above says. */
extern const Core1JobOps BRIDGE_JOB_OPS;
