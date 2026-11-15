/**
 * @file usb_bridge.cpp
 * @brief The USB half of the USB bridge. @see usb_bridge.h
 */

#include "usb_bridge.h"
#include "bridge_wire.h"
#include "bridge_log.h"
#include "bridge_msp.h"
#include "bridge_4way.h"
#include "bl_answer.h"
#include "esc_3d.h"
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
static bool           s_unfinished = false;      /**< A write-class frame and no RUN since the open. */
static bool           s_cue = false;             /**< MENU's run cue (usbBridgeRunCue()). */
static uint32_t       s_cueMs = 0;               /**< millis() when it was set. */
static bool           s_writeWait = false;
static uint32_t       s_writeAtMs = 0;           /**< millis() of the write-class frame's handover. */
static uint32_t       s_writeMark = 0;           /**< Bytes received once its echo is in: one more is its answer. */
static bool           s_engineWrite = false;     /**< The write waits on the 4-way command, not on the wire's count. */
static bool           s_motor = false;           /**< An MSP_SET_MOTOR was refused ... */
static uint32_t       s_motorMs = 0;             /**< ... at this millis(). */

/**
 * @defgroup usb_bridge_proto MSP and the 4-way interface
 * @{
 */
static BridgeMspReader  s_msp;
static Bridge4wayReader s_4w;
static uint8_t          s_out[B4W_ANSWER_MAX];   /**< An answer on its way to the PC. */
static uint16_t         s_outLen = 0;
static uint16_t         s_outPos = 0;            /**< Its bytes written so far. */
static bool             s_exitAfterOut = false;  /**< Leave the 4-way interface once it is out. */
static uint32_t         s_protoByteUs = 0;       /**< micros() of the last byte of a frame being read. */
/** @brief Waiting after MSP or the 4-way interface: what the wire brings belongs to them and is dropped. */
static bool             s_dropRx = false;
/** @} */

/**
 * @defgroup usb_bridge_tap Passthrough's 3D note (decision 12)
 * @{
 */
static uint16_t s_tapAddr = 0;      /**< The last SET ADDRESS. */
static uint16_t s_tapSettings = 0;  /**< The AM32 settings page from the last greeting's answer; 0 for none. */
static uint8_t  s_tapData[256];     /**< The last payload, its CRC checked ... */
static uint16_t s_tapDataN = 0;     /**< ... its length, 0 for none ... */
static uint16_t s_tapDataAddr = 0;  /**< ... and its address. */
static uint8_t  s_cap[259];         /**< The answer to the last frame, its echo skipped. */
static uint16_t s_capN = 0;
static uint16_t s_capWant = 0;      /**< Bytes it has: 0 for an answer not looked at. */
static uint16_t s_capSkip = 0;      /**< Echo bytes still to skip. */
static BridgeCmd s_capCmd = BridgeCmd::None;
/** @} */

// ---------------------------------------------------------------------------
// naming a frame
// ---------------------------------------------------------------------------

uint16_t usbBridgeCrc16(const uint8_t *d, uint16_t n) {
	uint16_t crc = 0;
	for (uint16_t i = 0; i < n; i++) {
		crc ^= d[i];
		for (int b = 0; b < 8; b++) crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
	}
	return crc;
}

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
		case 0x00:
			// RUN is 4 bytes with its CRC: a payload's tail after a USB gap
			// can start with 0x00 too.
			if (n == 4 && usbBridgeCrc16(f, 2) == (uint16_t)(f[2] | f[3] << 8)) return BridgeCmd::Run;
			return BridgeCmd::Other;
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
		case BridgeCmd::Msp:        return "MSP";
		case BridgeCmd::FourWay:    return "4-WAY";
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

/**
 * @brief Back to Waiting: a new session starts with the PC's next byte. The
 *        4-way interface's command stops where it is and its device is
 *        dropped; an answer not yet out is dropped; echoes go to the PC again.
 */
static void endSession() {
	s_dropRx = s_mode == BridgeMode::Msp || s_mode == BridgeMode::FourWay;
	s_mode = BridgeMode::Waiting;
	s_afterSetBuffer = false;
	bridge4wayReset();
	bridgeMspReset(&s_msp);
	bridge4wayReaderReset(&s_4w);
	s_outLen = s_outPos = 0;
	s_exitAfterOut = false;
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
	// The ESC may wait in its bootloader from now on, whatever the tool
	// sends: MENU's cue until an eRPM frame or RESTART DSHOT.
	s_cue = true;
	s_cueMs = millis();
	s_writeWait = false;
	s_engineWrite = false;
	s_motor = false;
	s_tapAddr = s_tapSettings = 0;
	s_tapDataN = 0;
	s_capWant = 0;
	s_dtr = tud_cdc_connected();
	endSession();
	s_dropRx = false;
	bridgeWireKeepEcho(true);
	usbDevConsole(false);
	usbDevClockLineEnable(false);
	s_console = true;
	// Console text TinyUSB still holds would reach the tool before the first
	// echo.
	tud_cdc_write_clear();
	// What the PC sent before the open belongs to whatever came before, and
	// would reach the ESC as noise (DshotDisplay's usbBridgeBegin()).
	uint8_t junk[64];
	while (tud_cdc_available() && tud_cdc_read(junk, sizeof(junk)) != 0) {}
	bridgeLogReset();
	bridgeLogNote("bridge opened");
}

/**
 * @brief The close, at BACK: nothing more is read or sent; the log's dump
 *        begins. A frame being gathered is never handed over: the next open
 *        starts an empty one.
 */
static void closeBridge() {
	s_open = false;
	endSession();
	s_mode = BridgeMode::Closed;
	bridgeLogNote("bridge closed");
	bridgeLogDumpBegin();
}

/**
 * @brief Passthrough's view of a frame for the 3D note (decision 12): the
 *        address of SET ADDRESS, a payload whose CRC fits, and which answer
 *        to read: the greeting's 9 bytes, READ's data, CRC and ACK,
 *        PROGRAM's ACK. The answer follows the frame's echo, which
 *        passthrough keeps.
 */
static void tapFrame(BridgeCmd c) {
	s_capCmd = c;
	s_capN = 0;
	s_capSkip = s_len;
	s_capWant = 0;
	if (c == BridgeCmd::SetAddress && s_len == 6) {
		s_tapAddr = (uint16_t)(s_buf[2] << 8 | s_buf[3]);
	} else if (c == BridgeCmd::Payload) {
		const uint16_t n = (uint16_t)(s_len - 2);
		s_tapDataN = 0;
		if (s_len > 2 && n <= sizeof(s_tapData) &&
		    usbBridgeCrc16(s_buf, n) == (uint16_t)(s_buf[n] | s_buf[n + 1] << 8)) {
			memcpy(s_tapData, s_buf, n);
			s_tapDataN = n;
			s_tapDataAddr = s_tapAddr;
		}
	} else if (c == BridgeCmd::Init) {
		s_capWant = BL_ANSWER_EFM8_LEN;
	} else if (c == BridgeCmd::Read && s_len == 4) {
		s_capWant = (uint16_t)((s_buf[1] ? s_buf[1] : 256) + 3);
	} else if (c == BridgeCmd::Program) {
		s_capWant = 1;
	}
}

/** @brief The answer of a tapped frame is whole. */
static void tapAnswer() {
	if (s_capCmd == BridgeCmd::Init) {
		BlAnswer a = blAnswerClassify(s_cap, s_capN);
		s_tapSettings = a.kind == BlAnswerKind::Am32 ? blAnswerAm32Settings(a.family) : 0;
	} else if (s_capCmd == BridgeCmd::Read) {
		const uint16_t n = (uint16_t)(s_capN - 3);
		if (usbBridgeCrc16(s_cap, (uint16_t)(n + 2)) != 0 || s_cap[n + 2] != 0x30) return;
		int v = bridgeAm32Page3d(s_tapAddr, s_tapSettings, s_cap, n);
		if (v >= 0) esc3dSetNote(v == 1);
	} else if (s_capCmd == BridgeCmd::Program) {
		// An accepted write with 3D on sets the note; one with it off leaves
		// it for the read-back (esc_3d.h).
		if (s_cap[0] == 0x30 && s_tapDataN &&
		    bridgeAm32Page3d(s_tapDataAddr, s_tapSettings, s_tapData, s_tapDataN) == 1)
			esc3dSetNote(true);
	}
}

/** @brief Bytes on their way to the PC: past the frame's echo, the answer a tapped frame waits for. */
static void tapBytes(const uint8_t *d, uint16_t n) {
	for (uint16_t i = 0; i < n && s_capWant; i++) {
		if (s_capSkip) {
			s_capSkip--;
			continue;
		}
		s_cap[s_capN++] = d[i];
		if (s_capN == s_capWant) {
			tapAnswer();
			s_capWant = 0;
		}
	}
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
		s_engineWrite = false;
		s_writeAtMs = now;
		s_writeMark = w.received + s_len;
	} else if (c == BridgeCmd::Run) {
		s_unfinished = false;
	}
	tapFrame(c);
	bridgeLogAdd(BLOG_HOST_TO_ESC, s_buf, s_len);
	s_stats.toEsc += s_len;
	s_stats.frames++;
	s_stats.lastMs = now;
	s_len = 0;
	s_ready = false;
}

// ---------------------------------------------------------------------------
// MSP and the 4-way interface
// ---------------------------------------------------------------------------

/** @brief An MSP frame whose checksum fits: its answer queued. */
static void mspFrame(uint32_t now) {
	BridgeMspEffect e;
	s_outLen = bridgeMspAnswer(&s_msp, s_out, &e);
	s_outPos = 0;
	s_last.cmd = BridgeCmd::Msp;
	s_last.first = s_msp.cmd;
	s_last.addr = 0;
	s_last.len = (uint16_t)(6 + s_msp.len);
	if (e.fourWay) {
		s_mode = BridgeMode::FourWay;
		bridgeLogNote("4-way interface");
	}
	if (e.motorRefused) {
		s_motor = true;
		s_motorMs = now;
	}
}

/**
 * @brief One byte from the PC in MSP or the 4-way interface: into the frame
 *        it belongs to. A byte between frames other than `$` and 0x2F is
 *        dropped, as Betaflight's 4-way loop drops every byte but 0x2F.
 */
static void protoByte(uint8_t b, uint32_t now) {
	s_protoByteUs = micros();
	if (bridgeMspInFrame(&s_msp)) {
		if (bridgeMspFeed(&s_msp, b) == BridgeMspFeed::Frame) mspFrame(now);
		return;
	}
	if (bridge4wayInFrame(&s_4w) || b == B4W_PC_START) {
		Bridge4wayFeed f = bridge4wayFeed(&s_4w, b);
		if (f == Bridge4wayFeed::Frame) {
			s_mode = BridgeMode::FourWay;
			s_last.cmd = BridgeCmd::FourWay;
			s_last.first = s_4w.buf[1];
			s_last.addr = (uint16_t)(s_4w.buf[2] << 8 | s_4w.buf[3]);
			s_last.len = s_4w.need;
			bridgeLogAdd(BLOG_HOST_TO_ESC, s_4w.buf, s_4w.need);
			bridge4wayStart(&s_4w);
		} else if (f == Bridge4wayFeed::BadCrc) {
			// Answered at once; nothing reaches the wire.
			s_outLen = bridge4wayCrcAnswer(&s_4w, s_out);
			s_outPos = 0;
			bridgeLogNote("4-way CRC wrong");
		}
		return;
	}
	if (b == '$') bridgeMspFeed(&s_msp, b);
}

/** @brief A frame from the PC is being read. */
static bool protoInFrame() { return bridgeMspInFrame(&s_msp) || bridge4wayInFrame(&s_4w); }

/**
 * @brief One pass in MSP or the 4-way interface: the command under way, its
 *        answer to the PC, then the PC's next frame, read only once the last
 *        is answered, so the PC is held off by USB flow control meanwhile.
 */
static void protoPass(uint32_t now) {
	if (bridge4wayBusy()) {
		Bridge4wayEvents ev = bridge4wayStep(micros());
		if (ev.frames) {
			s_stats.toEsc += ev.bytes;
			s_stats.frames += ev.frames;
			s_stats.lastMs = now;
		}
		if (ev.write) {
			// As a write-class frame in passthrough: BACK disabled, the note
			// to flash the ESC again until a RUN.
			s_unfinished = true;
			s_writeWait = true;
			s_engineWrite = true;
			s_writeAtMs = now;
		}
		if (ev.run) s_unfinished = false;
		if (ev.done) {
			if (s_engineWrite) s_writeWait = false;
			s_outLen = bridge4wayTakeAnswer(s_out);
			s_outPos = 0;
			s_exitAfterOut = ev.exit;
		}
	} else {
		// No command waits for the ESC: what the wire brings is no answer.
		uint8_t junk[64];
		while (bridgeWireRecv(junk, sizeof(junk))) {}
	}

	if (s_outPos < s_outLen) {
		uint32_t room = tud_cdc_write_available();
		uint16_t n = (uint16_t)(s_outLen - s_outPos);
		if (room < n) n = (uint16_t)room;
		if (n) {
			tud_cdc_write(s_out + s_outPos, n);
			tud_cdc_write_flush();
			bridgeLogAdd(BLOG_ESC_TO_HOST, s_out + s_outPos, n);
			s_outPos = (uint16_t)(s_outPos + n);
			s_stats.toHost += n;
			s_stats.lastMs = now;
		}
	}
	if (s_outLen && s_outPos == s_outLen) {
		s_outLen = s_outPos = 0;
		if (s_exitAfterOut) {
			endSession();
			bridgeLogNote("4-way exit: waiting");
			return;
		}
	}

	bool got = false;
	uint8_t b;
	while (!bridge4wayBusy() && s_outLen == 0 && tud_cdc_available() && tud_cdc_read(&b, 1) == 1) {
		got = true;
		protoByte(b, now);
	}
	if (got) {
		s_stats.lastMs = now;
		s_portClosed = false;
	}
	// A frame cut short never reaches the wire.
	if (protoInFrame() && micros() - s_protoByteUs >= BRIDGE_PROTO_GAP_US) {
		bridgeMspReset(&s_msp);
		bridge4wayReaderReset(&s_4w);
		bridgeLogNote("frame cut");
	}
	if (s_mode == BridgeMode::Msp && !protoInFrame() && s_outLen == 0 &&
	    now - s_stats.lastMs >= BRIDGE_IDLE_MS) {
		endSession();
		bridgeLogNote("idle: waiting");
	}
}

// ---------------------------------------------------------------------------
// the pass
// ---------------------------------------------------------------------------

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

	// The PC's first byte chooses the mode (decision 3): `$` MSP, 0x2F the
	// 4-way interface, any other passthrough.
	if (s_mode == BridgeMode::Waiting && s_len == 0 && tud_cdc_available()) {
		uint8_t b;
		if (tud_cdc_read(&b, 1) == 1) {
			s_stats.lastMs = now;
			s_portClosed = false;
			s_dropRx = false;
			bridgeWireKeepEcho(b != '$' && b != B4W_PC_START);
			if (b == '$' || b == B4W_PC_START) {
				s_mode = b == '$' ? BridgeMode::Msp : BridgeMode::FourWay;
				bridgeLogNote(b == '$' ? "MSP" : "4-way interface");
				protoByte(b, now);
			} else {
				s_buf[0] = b;
				s_len = 1;
				s_lastByteUs = micros();
				s_mode = BridgeMode::Passthrough;
			}
		}
	}
	if (s_mode == BridgeMode::Msp || s_mode == BridgeMode::FourWay) {
		protoPass(now);
		return;
	}

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
	// After MSP or the 4-way interface, until the next session, late answers
	// to their commands are dropped.
	if (s_dropRx) {
		uint8_t junk[64];
		while (bridgeWireRecv(junk, sizeof(junk))) {}
		return;
	}
	uint32_t room = tud_cdc_write_available();
	if (room > sizeof(s_rx)) room = sizeof(s_rx);
	uint16_t r = room ? bridgeWireRecv(s_rx, (uint16_t)room) : 0;
	if (r) {
		tud_cdc_write(s_rx, r);
		tud_cdc_write_flush();
		tapBytes(s_rx, r);
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
		usbDevClockLineEnable(true);
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
	if (millis() - s_writeAtMs >= BRIDGE_WRITE_WAIT_MS ||
	    (!s_engineWrite && (int32_t)(w.received - s_writeMark) > 0))
		s_writeWait = false;
	return s_writeWait;
}

bool usbBridgeWriteUnfinished() { return s_unfinished; }

bool usbBridgeMotorRefused() { return s_motor && millis() - s_motorMs < BRIDGE_MOTOR_NOTE_MS; }

bool usbBridgeRunCue(uint32_t lastRpmMs) {
	// An eRPM frame stamped after the answer: the firmware runs again.
	if (s_cue && lastRpmMs != 0 && (int32_t)(lastRpmMs - s_cueMs) > 0) s_cue = false;
	return s_cue;
}

void usbBridgeRunCueSet() {
	s_cue = true;
	s_cueMs = millis();
}

void usbBridgeRunCueClear() { s_cue = false; }

bool usbBridgePortOpen() { return tud_cdc_connected(); }

bool usbBridgePortClosed() { return s_portClosed; }
