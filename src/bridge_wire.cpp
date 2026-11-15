/**
 * @file bridge_wire.cpp
 * @brief The USB bridge's core1 job, frame slot and byte ring.
 *        @see bridge_wire.h
 */

#include "bridge_wire.h"
#include "bridge_line_port.h"

#include <string.h>

/*
 * Memory ordering, as DshotDisplay's bridge_wire.cpp at 6693084 lines 41 to
 * 51: each index and flag has one writer per transition; the data must be
 * visible before the index or flag that publishes it, which a release store
 * paired with an acquire load guarantees and `volatile` alone does not.
 */
#define WIRE_PUBLISH(var, val) __atomic_store_n(&(var), (val), __ATOMIC_RELEASE)
#define WIRE_OBSERVE(var)      __atomic_load_n(&(var), __ATOMIC_ACQUIRE)

/**
 * @defgroup bridge_wire_shared Shared between the cores
 * @{
 */
/** @brief The line is bound and sends are taken. Core1 writes. */
static bool     s_open = false;
/** @brief The frame slot. Core0 writes it while @ref s_slotFull is false. */
static uint8_t  s_slot[BRIDGE_FRAME_MAX];
/** @brief The frame's length. Written with the slot. */
static uint16_t s_slotLen = 0;
/** @brief A frame waits or goes out: core0 sets it, core1 clears it. */
static bool     s_slotFull = false;
/** @brief The ring from the wire. */
static uint8_t  s_ring[BRIDGE_RX_RING];
/** @brief Bytes ever put in the ring. Core1 writes. */
static uint32_t s_head = 0;
/** @brief Bytes ever taken from the ring. Core0 writes. */
static uint32_t s_tail = 0;
/** @brief Echoes go into the ring. Core0 writes. */
static bool     s_keepEcho = true;
/** @brief The counts. Core1 writes. */
static BridgeWireStats s_stats;
/** @} */

/**
 * @defgroup bridge_wire_core1 Core1 only
 * @{
 */
static bool     s_txActive = false;   /**< A frame from the slot is going out. */
static uint16_t s_txPos = 0;          /**< Its bytes in the FIFO so far. */
static uint8_t  s_echo[BRIDGE_ECHO_MAX]; /**< Bytes sent, their echo owed, oldest first. */
static uint32_t s_echoFrame[BRIDGE_ECHO_MAX]; /**< The frame of each, its number from the bind. */
static uint8_t  s_echoFirst = 0;      /**< Index of the oldest. */
static uint8_t  s_echoCount = 0;      /**< How many are owed. */
static uint32_t s_txFrame = 0;        /**< The number of the frame going out. */
/** @brief The last frame counted as a collision, its number plus 1; 0 for none. */
static uint32_t s_collided = 0;
static bool     s_releasing = false;  /**< The release has begun. */
static uint32_t s_releaseMs = 0;      /**< millis() at its first call. */
/** @} */

/** @brief Add @p n to one count, for core0 to read whole. */
static void count(uint32_t *field, uint32_t n) {
	WIRE_PUBLISH(*field, *field + n);
}

// The largest `clk_sys` a uint32_t holds needs a divider below 65536.
static_assert((uint64_t)UINT32_MAX * 256u / (BRIDGE_BAUD * BRIDGE_CYCLES_PER_BIT) <
              65536u * 256u, "a divider of 65536 or more");

bool bridgeLineClock(uint32_t sysHz, uint16_t *divInt, uint8_t *divFrac) {
	const uint64_t perSec = (uint64_t)BRIDGE_BAUD * BRIDGE_CYCLES_PER_BIT;
	// In 1/256 steps, rounded to the nearest.
	const uint64_t div = ((uint64_t)sysHz * 256u + perSec / 2u) / perSec;
	if (div < 256u) return false;
	*divInt = (uint16_t)(div >> 8);
	*divFrac = (uint8_t)(div & 0xFFu);
	return true;
}

// ---------------------------------------------------------------------------
// core0
// ---------------------------------------------------------------------------

bool bridgeWireOpen() { return WIRE_OBSERVE(s_open); }

bool bridgeWireSlotFree() { return bridgeWireOpen() && !WIRE_OBSERVE(s_slotFull); }

bool bridgeWireSend(const uint8_t *data, uint16_t len) {
	if (len == 0 || len > BRIDGE_FRAME_MAX || !bridgeWireSlotFree()) return false;
	memcpy(s_slot, data, len);
	s_slotLen = len;
	WIRE_PUBLISH(s_slotFull, true);  // publishes the slot and its length
	return true;
}

uint16_t bridgeWireAvailable() {
	if (!bridgeWireOpen()) return 0;
	return (uint16_t)(WIRE_OBSERVE(s_head) - s_tail);
}

uint16_t bridgeWireRecv(uint8_t *buf, uint16_t maxLen) {
	// Closed: the next bind resets both indices, so core0 keeps off them.
	if (!bridgeWireOpen()) return 0;
	const uint32_t head = WIRE_OBSERVE(s_head);  // pairs with the job's publish
	uint32_t tail = s_tail;
	uint16_t n = 0;
	while (tail != head && n < maxLen) {
		buf[n++] = s_ring[tail & (BRIDGE_RX_RING - 1u)];
		tail++;
	}
	WIRE_PUBLISH(s_tail, tail);                  // frees the room for core1
	return n;
}

void bridgeWireKeepEcho(bool keep) { WIRE_PUBLISH(s_keepEcho, keep); }

void bridgeWireStats(BridgeWireStats *out) {
	out->sent       = WIRE_OBSERVE(s_stats.sent);
	out->received   = WIRE_OBSERVE(s_stats.received);
	out->frames     = WIRE_OBSERVE(s_stats.frames);
	out->lost       = WIRE_OBSERVE(s_stats.lost);
	out->collisions = WIRE_OBSERVE(s_stats.collisions);
}

// ---------------------------------------------------------------------------
// core1
// ---------------------------------------------------------------------------

/** @brief One byte into the ring, or counted lost when it is full. */
static void ringPut(uint8_t b) {
	const uint32_t head = s_head;
	if (head - WIRE_OBSERVE(s_tail) >= BRIDGE_RX_RING) {
		count(&s_stats.lost, 1);
		return;
	}
	s_ring[head & (BRIDGE_RX_RING - 1u)] = b;
	WIRE_PUBLISH(s_head, head + 1u);
}

/**
 * @brief Count frame @p frame as a collision, once: a lost echo shifts the
 *        rest of its frame, and each later byte would differ too.
 */
static void collided(uint32_t frame) {
	if (s_collided == frame + 1u) return;
	s_collided = frame + 1u;
	count(&s_stats.collisions, 1);
}

/** @brief One byte off the wire: the next echo owed, or the ESC's. */
static void takeByte(uint8_t b) {
	count(&s_stats.received, 1);
	if (s_echoCount == 0) {
		ringPut(b);
		return;
	}
	const uint8_t sent = s_echo[s_echoFirst];
	const uint32_t frame = s_echoFrame[s_echoFirst];
	s_echoFirst = (uint8_t)((s_echoFirst + 1u) % BRIDGE_ECHO_MAX);
	s_echoCount--;
	if (b != sent) collided(frame);
	if (WIRE_OBSERVE(s_keepEcho)) ringPut(b);
}

/**
 * @brief Keep the transmit FIFO filled from the slot.
 * @param takeNew Start a frame waiting in the slot; false finishes only the
 *                one under way.
 */
static void feed(bool takeNew) {
	if (!s_txActive) {
		if (!takeNew || !WIRE_OBSERVE(s_slotFull)) return;
		s_txActive = true;
		s_txPos = 0;
		s_txFrame++;
		count(&s_stats.frames, 1);
	}
	const uint16_t len = s_slotLen;
	while (s_txPos < len && s_echoCount < BRIDGE_ECHO_MAX) {
		const uint8_t b = s_slot[s_txPos];
		const uint16_t word = (uint16_t)(b | (s_txPos + 1u < len ? BRIDGE_LINE_KEEP : 0u));
		if (!bridgeLinePut(word)) break;
		const unsigned at = (s_echoFirst + s_echoCount) % BRIDGE_ECHO_MAX;
		s_echo[at] = b;
		s_echoFrame[at] = s_txFrame;
		s_echoCount++;
		s_txPos++;
		count(&s_stats.sent, 1);
	}
	if (s_txPos == len) {
		s_txActive = false;
		WIRE_PUBLISH(s_slotFull, false);  // core0 may fill it again
	}
}

/**
 * @brief Receive, count, and write off missing echoes. The idle state is
 *        read before the FIFO: once idle, every echo is in it.
 */
static void receive() {
	const bool idle = bridgeLineIdle();
	uint8_t b;
	while (bridgeLineGet(&b)) takeByte(b);
	if (bridgeLineTakeOverflow()) count(&s_stats.lost, 1);
	for (; idle && s_echoCount; s_echoCount--) {
		collided(s_echoFrame[s_echoFirst]);
		s_echoFirst = (uint8_t)((s_echoFirst + 1u) % BRIDGE_ECHO_MAX);
	}
}

static uint8_t bridgeJobBind(uint8_t sigGpio, uint8_t /*auxGpio*/) {
	WIRE_PUBLISH(s_open, false);
	s_txActive = false;
	s_txPos = 0;
	s_echoFirst = s_echoCount = 0;
	s_txFrame = 0;
	s_collided = 0;
	s_releasing = false;
	WIRE_PUBLISH(s_slotFull, false);
	// Core0 reads the ring only while the line is open (bridgeWireRecv()),
	// so both indices are core1's to reset here.
	WIRE_PUBLISH(s_tail, 0u);
	WIRE_PUBLISH(s_head, 0u);
	BridgeWireStats zero = {0, 0, 0, 0, 0};
	s_stats = zero;
	const uint8_t err = bridgeLineOpen(sigGpio);
	if (err != CORE1_JOB_OK) return err;
	WIRE_PUBLISH(s_open, true);
	return CORE1_JOB_OK;
}

static void bridgeJobStep(uint32_t /*nowMs*/, bool /*uiAlive*/) {
	receive();
	feed(true);
}

static bool bridgeJobRelease(uint32_t nowMs) {
	if (!s_releasing) {
		s_releasing = true;
		s_releaseMs = nowMs;
		WIRE_PUBLISH(s_open, false);
		// A frame not started stays unsent: BACK sends nothing new
		// (docs/design/usb-bridge.md, decision 6).
		if (!s_txActive) WIRE_PUBLISH(s_slotFull, false);
	}
	receive();
	feed(false);
	if ((s_txActive || !bridgeLineIdle()) && nowMs - s_releaseMs < BRIDGE_RELEASE_MAX_MS)
		return false;
	bridgeLineClose();
	s_txActive = false;
	WIRE_PUBLISH(s_slotFull, false);
	s_echoCount = 0;
	s_releasing = false;
	return true;
}

const Core1JobOps BRIDGE_JOB_OPS = { bridgeJobBind, bridgeJobStep, bridgeJobRelease, nullptr };
