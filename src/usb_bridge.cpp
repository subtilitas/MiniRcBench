/**
 * @file usb_bridge.cpp
 * @brief The USB half of the USB bridge. @see usb_bridge.h
 */

#include "usb_bridge.h"
#include "bridge_wire.h"
#include "bridge_log.h"
#include "pin_claim.h"
#include "usb_dev.h"
#include "plat.h"

#include "tusb.h"

#include <string.h>

static bool           s_open = false;     /**< Forwarding: the claim Running for the bridge, the line open. */
static bool           s_console = false;  /**< The bridge took the port from the console. */
static BridgeMode     s_mode = BridgeMode::Closed;
static UsbBridgeStats s_stats;
static BridgeLast     s_last;
static bool           s_afterSetBuffer = false;  /**< The last frame was a SET BUFFER. */
static uint8_t        s_buf[BRIDGE_FRAME_MAX];   /**< The PC frame being gathered. */
static uint16_t       s_len = 0;                 /**< Bytes in it. */
static uint32_t       s_lastByteUs = 0;          /**< micros() of its last byte. */
static bool           s_ready = false;           /**< It is whole and waits for the slot. */
static uint8_t        s_rx[256];                 /**< Bytes from the ring on their way to the PC. */
static bool           s_dtr = false;             /**< DTR at the last pass. */
static bool           s_portClosed = false;
static bool           s_unfinished = false;
static bool           s_writeWait = false;
static uint32_t       s_writeAtMs = 0;           /**< millis() of the write-class frame's handover. */
static uint32_t       s_writeMark = 0;           /**< Bytes received once its echo is in: one more is its answer. */

// ---------------------------------------------------------------------------
// naming a frame
// ---------------------------------------------------------------------------

BridgeCmd usbBridgeCmdOf(const uint8_t *f, uint16_t n, bool afterSetBuffer) {
	if (n == 0) return BridgeCmd::None;
	if (afterSetBuffer) return BridgeCmd::Payload;
	static const uint8_t GREETING[] = { 0x0D, 'B', 'L', 'H', 'e', 'l', 'i' };
	if (n >= sizeof(GREETING) + 2 &&
	    memcmp(f + n - 2 - sizeof(GREETING), GREETING, sizeof(GREETING)) == 0)
		return BridgeCmd::Init;
	switch (f[0]) {
		case 0xFF: return BridgeCmd::SetAddress;
		case 0xFE: return BridgeCmd::SetBuffer;
		case 0x03: return BridgeCmd::Read;
		case 0x01: return BridgeCmd::Program;
		case 0x02: return BridgeCmd::Erase;
		case 0x00: return BridgeCmd::Run;
		case 0xFD: return BridgeCmd::KeepAlive;
		default:   return BridgeCmd::Other;
	}
}

bool usbBridgeCmdWrites(BridgeCmd c) {
	return c == BridgeCmd::Program || c == BridgeCmd::Erase || c == BridgeCmd::Payload;
}

const char *usbBridgeCmdName(BridgeCmd c) {
	switch (c) {
		case BridgeCmd::None:       return "";
		case BridgeCmd::Init:       return "INIT";
		case BridgeCmd::SetAddress: return "SET ADDRESS";
		case BridgeCmd::SetBuffer:  return "SET BUFFER";
		case BridgeCmd::Payload:    return "PAYLOAD";
		case BridgeCmd::Read:       return "READ";
		case BridgeCmd::Program:    return "PROGRAM";
		case BridgeCmd::Erase:      return "ERASE";
		case BridgeCmd::Run:        return "RUN";
		case BridgeCmd::KeepAlive:  return "KEEP ALIVE";
		case BridgeCmd::Other:      break;
	}
	return "FRAME";
}

// ---------------------------------------------------------------------------
// the bridge
// ---------------------------------------------------------------------------

/** @brief The claim is Running for the bridge and its line is open. */
static bool claimRunning() {
	return pinClaimHolder() == PinUser::Bridge && pinClaimState() == ClaimState::Running &&
	       bridgeWireOpen();
}

/** @brief Back to Waiting: a new session starts with the PC's next byte. */
static void endSession() {
	s_mode = BridgeMode::Waiting;
	s_afterSetBuffer = false;
}

/** @brief The open: the port from the console, what the PC queued dropped, every count at 0. */
static void openBridge() {
	s_open = true;
	memset(&s_stats, 0, sizeof(s_stats));
	memset(&s_last, 0, sizeof(s_last));
	s_stats.lastMs = millis();
	s_len = 0;
	s_ready = false;
	s_portClosed = false;
	s_unfinished = false;
	s_writeWait = false;
	s_dtr = tud_cdc_connected();
	endSession();
	bridgeWireKeepEcho(true);
	usbDevConsole(false);
	s_console = true;
	// What the PC sent before the open belongs to whatever came before, and
	// would reach the ESC as noise (DshotDisplay's usbBridgeBegin()).
	uint8_t junk[64];
	while (tud_cdc_available() && tud_cdc_read(junk, sizeof(junk)) != 0) {}
	bridgeLogReset();
	bridgeLogNote("bridge opened");
}

/** @brief The close, at BACK: nothing more is read or sent; the log's dump begins. */
static void closeBridge() {
	s_open = false;
	s_mode = BridgeMode::Closed;
	s_len = 0;
	s_ready = false;
	bridgeLogNote("bridge closed");
	bridgeLogDumpBegin();
}

/** @brief The gathered frame went to the slot: name it, count it, log it. */
static void handedOver(uint32_t now) {
	BridgeCmd c = usbBridgeCmdOf(s_buf, s_len, s_afterSetBuffer);
	s_afterSetBuffer = c == BridgeCmd::SetBuffer;
	s_last.cmd = c;
	s_last.first = s_buf[0];
	s_last.addr = s_len >= 4 ? (uint16_t)(s_buf[2] << 8 | s_buf[3]) : 0;
	s_last.len = s_len;
	if (usbBridgeCmdWrites(c)) {
		BridgeWireStats w;
		bridgeWireStats(&w);
		s_unfinished = true;
		s_writeWait = true;
		s_writeAtMs = now;
		s_writeMark = w.received + s_len;
	} else if (c == BridgeCmd::Run) {
		s_unfinished = false;
	}
	bridgeLogAdd(BLOG_HOST_TO_ESC, s_buf, s_len);
	s_stats.toEsc += s_len;
	s_stats.frames++;
	s_stats.lastMs = now;
	s_len = 0;
	s_ready = false;
}

/** @brief One pass while open: the port's state, the PC to the slot, the ring to the PC, the idle time. */
static void forward() {
	const uint32_t now = millis();
	bool dtr = tud_cdc_connected();
	if (s_dtr && !dtr) {
		// The PC closed the port, or the cable went: what was being gathered
		// is dropped and nothing more is sent.
		if (s_stats.toEsc + s_stats.toHost) s_portClosed = true;
		s_len = 0;
		s_ready = false;
		endSession();
		bridgeLogNote("port closed");
	}
	s_dtr = dtr;

	// The PC to the slot: what TinyUSB holds first, then the clock.
	if (!s_ready) {
		bool got = false;
		while (s_len < BRIDGE_FRAME_MAX && tud_cdc_available()) {
			uint32_t n = tud_cdc_read(s_buf + s_len, BRIDGE_FRAME_MAX - s_len);
			if (n == 0) break;
			s_len = (uint16_t)(s_len + n);
			got = true;
		}
		if (got) {
			s_lastByteUs = micros();
			s_stats.lastMs = now;
			s_portClosed = false;
			if (s_mode == BridgeMode::Waiting) s_mode = BridgeMode::Passthrough;
		}
		if (s_len && (s_len >= BRIDGE_FRAME_MAX || micros() - s_lastByteUs >= BRIDGE_GATHER_US))
			s_ready = true;
	}
	if (s_ready && bridgeWireSend(s_buf, s_len)) handedOver(now);

	// The ring to the PC: what the port takes; the rest waits in the ring.
	uint32_t room = tud_cdc_write_available();
	if (room > sizeof(s_rx)) room = sizeof(s_rx);
	uint16_t r = room ? bridgeWireRecv(s_rx, (uint16_t)room) : 0;
	if (r) {
		tud_cdc_write(s_rx, r);
		tud_cdc_write_flush();
		bridgeLogAdd(BLOG_ESC_TO_HOST, s_rx, r);
		s_stats.toHost += r;
		s_stats.lastMs = now;
	}

	if (s_mode == BridgeMode::Passthrough && s_len == 0 && now - s_stats.lastMs >= BRIDGE_IDLE_MS) {
		endSession();
		bridgeLogNote("idle: waiting");
	}
}

void usbBridgeTask() {
	bool running = claimRunning();
	if (running && !s_open) openBridge();
	else if (!running && s_open) closeBridge();
	if (s_open) {
		forward();
		return;
	}
	// The console gets the port back once the claim is Free, after the dump.
	if (s_console && bridgeLogDumpStep() && pinClaimHolder() != PinUser::Bridge) {
		usbDevConsole(true);
		s_console = false;
	}
}

BridgeMode usbBridgeMode() { return s_mode; }

void usbBridgeStats(UsbBridgeStats *out) {
	*out = s_stats;
	BridgeWireStats w;
	bridgeWireStats(&w);
	out->lost = w.lost;
	out->collisions = w.collisions;
}

BridgeLast usbBridgeLast() { return s_last; }

bool usbBridgeWriteWaiting() {
	if (!s_writeWait) return false;
	BridgeWireStats w;
	bridgeWireStats(&w);
	if (millis() - s_writeAtMs >= BRIDGE_WRITE_WAIT_MS || (int32_t)(w.received - s_writeMark) > 0)
		s_writeWait = false;
	return s_writeWait;
}

bool usbBridgeWriteUnfinished() { return s_unfinished; }

bool usbBridgePortOpen() { return tud_cdc_connected(); }

bool usbBridgePortClosed() { return s_portClosed; }
