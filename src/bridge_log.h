/**
 * @file bridge_log.h
 * @brief The USB bridge's traffic log: every transfer with its time, kept in
 *        a RAM ring while the bridge runs and printed as a hex dump on the
 *        serial port after BACK.
 *
 * Ported from DshotDisplay's `bridge_log.*` at 6693084, a build option off
 * by default as `AM32_BRIDGE_LOG` 0 is there (docs/design/usb-bridge.md,
 * decision 8): @ref USB_BRIDGE_LOG in config.h. At 0 every entry point here
 * is an empty inline and nothing is built.
 *
 * The times are the point: a desktop tool that polls its port every 25 ms
 * sees one flush as one read, so a reply 60 ms after its echo is a different
 * failure from a reply that is wrong, and only the times tell them apart.
 *
 * A record in the ring, little-endian: the time since bridgeLogReset() in ms,
 * 4 bytes; the direction, 1; the bytes kept, 1; the bytes transferred, 1,
 * 255 for more; then the bytes kept, at most 64. When the ring is full the
 * oldest records go, so a long session keeps its end.
 *
 * The dump is DshotDisplay's text, with the title `USB BRIDGE LOG`: a blank
 * line, the title with the count, the column heads, one line per record and
 * `--- END ---` with a blank line. It is written with `tud_cdc_write()`, as
 * much as the port takes per call of bridgeLogDumpStep(), one call per pass
 * of core0's loop, so it never waits and never drops a line the way a burst
 * of writes into a full buffer does. A dump that makes no progress for
 * @ref BRIDGE_LOG_STALL_MS, a host that stopped reading, is abandoned.
 *
 * Core0 only.
 */

#pragma once

#include <stdint.h>

#include "config.h"

/** @brief Direction of a record. */
enum BridgeLogDir : uint8_t {
	BLOG_HOST_TO_ESC = 0, /**< A frame from the PC to the wire. */
	BLOG_ESC_TO_HOST = 1, /**< Bytes from the wire to the PC, the echo included. */
	BLOG_NOTE        = 2, /**< A text marker. */
};

/** @brief Bytes of one transfer kept; the rest are counted, not kept. */
#define BRIDGE_LOG_MAX_PAYLOAD 64

/** @brief A record's head: the time, 4 bytes; direction; bytes kept; bytes transferred. */
#define BRIDGE_LOG_HDR 7

/** @brief A dump that writes nothing for this long is abandoned, ms. */
#define BRIDGE_LOG_STALL_MS 400

#if USB_BRIDGE_LOG

/** @brief Empty the ring and start the times from now. */
void bridgeLogReset();

/**
 * @brief Record one transfer; the oldest records go to make room.
 * @param dir  Its direction.
 * @param data The bytes.
 * @param len  How many; the first @ref BRIDGE_LOG_MAX_PAYLOAD are kept.
 */
void bridgeLogAdd(BridgeLogDir dir, const uint8_t *data, uint16_t len);

/** @brief Record a text marker of at most 64 characters. @param text The marker. */
void bridgeLogNote(const char *text);

/** @brief Records held. @return The count. */
uint16_t bridgeLogCount();

/** @brief Whether records went to make room since the reset. @return True once one did. */
bool bridgeLogOverflowed();

/** @brief Start the dump from its first line. The records stay until the next reset. */
void bridgeLogDumpBegin();

/**
 * @brief Write the next part of the dump: as much as `tud_cdc_write_available()`
 *        takes, then a flush. Never waits.
 * @return True once the dump is done, or abandoned after
 *         @ref BRIDGE_LOG_STALL_MS without a byte taken; true with no dump begun.
 */
bool bridgeLogDumpStep();

#else

static inline void bridgeLogReset() {}
static inline void bridgeLogAdd(BridgeLogDir, const uint8_t *, uint16_t) {}
static inline void bridgeLogNote(const char *) {}
static inline uint16_t bridgeLogCount() { return 0; }
static inline bool bridgeLogOverflowed() { return false; }
static inline void bridgeLogDumpBegin() {}
static inline bool bridgeLogDumpStep() { return true; }

#endif
