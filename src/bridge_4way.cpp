/**
 * @file bridge_4way.cpp
 * @brief The USB bridge's 4-way interface. @see bridge_4way.h
 *
 * Written from the protocol as docs/design/usb-bridge.md describes it;
 * Betaflight's GPL code is not copied. The bootloader frames are rcbench's
 * escwire builders (`ew_a.h`), copied into lib/rcbench/ by item (6).
 */

#include "bridge_4way.h"
#include "bridge_wire.h"
#include "bl_answer.h"
#include "am32_eeprom.h"
#include "esc_3d.h"
#include "version.h"

#include "ew_a.h"

#include <string.h>

// ---------------------------------------------------------------------------
// frames
// ---------------------------------------------------------------------------

uint16_t bridge4wayCrc(const uint8_t *d, uint16_t n, uint16_t crc) {
	for (uint16_t i = 0; i < n; i++) {
		crc ^= (uint16_t)(d[i] << 8);
		for (int b = 0; b < 8; b++)
			crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
	}
	return crc;
}

void bridge4wayReaderReset(Bridge4wayReader *r) {
	r->got = 0;
	r->need = 0;
}

bool bridge4wayInFrame(const Bridge4wayReader *r) { return r->got != 0; }

Bridge4wayFeed bridge4wayFeed(Bridge4wayReader *r, uint8_t b) {
	if (r->got == 0) {
		if (b != B4W_PC_START) return Bridge4wayFeed::Dropped;
		r->need = 0;
	}
	r->buf[r->got++] = b;
	// The header's last byte: the parameters' count, 0 meaning 256.
	if (r->got == 5) r->need = (uint16_t)(5 + (b ? b : 256) + 2);
	if (r->need == 0 || r->got < r->need) return Bridge4wayFeed::More;
	r->got = 0;
	const uint16_t n = (uint16_t)(r->need - 2);
	const uint16_t want = (uint16_t)(r->buf[n] << 8 | r->buf[n + 1]);
	return bridge4wayCrc(r->buf, n) == want ? Bridge4wayFeed::Frame : Bridge4wayFeed::BadCrc;
}

/** @brief An answer: 0x2E, the header, @p n parameters, the ACK and the CRC. */
static uint16_t answer(uint8_t *out, uint8_t cmd, uint16_t addr, const uint8_t *par, uint16_t n,
                       uint8_t ack) {
	out[0] = B4W_IF_START;
	out[1] = cmd;
	out[2] = (uint8_t)(addr >> 8);
	out[3] = (uint8_t)addr;
	out[4] = (uint8_t)n;  // 256 is sent as 0
	memcpy(out + 5, par, n);
	out[5 + n] = ack;
	const uint16_t crc = bridge4wayCrc(out, (uint16_t)(6 + n));
	out[6 + n] = (uint8_t)(crc >> 8);
	out[7 + n] = (uint8_t)crc;
	return (uint16_t)(8 + n);
}

uint16_t bridge4wayCrcAnswer(const Bridge4wayReader *r, uint8_t *out) {
	static const uint8_t zero = 0;
	return answer(out, r->buf[1], (uint16_t)(r->buf[2] << 8 | r->buf[3]), &zero, 1,
	              B4W_ACK_I_INVALID_CRC);
}

int bridgeAm32Page3d(uint16_t addr, uint16_t settings, const uint8_t *d, uint16_t n) {
	if (settings == 0 || addr != settings || n < AM32_SETTINGS_SIZE) return -1;
	if (!am32Plausible(d)) return -1;
	return am32Is3d(d) ? 1 : 0;
}

const char *bridge4wayName(uint8_t cmd) {
	switch (cmd) {
		case B4W_TEST_ALIVE:       return "TEST ALIVE";
		case B4W_PROTOCOL_VERSION: return "PROTOCOL VERSION";
		case B4W_GET_NAME:         return "GET NAME";
		case B4W_GET_VERSION:      return "GET VERSION";
		case B4W_EXIT:             return "EXIT";
		case B4W_RESET:            return "RESET";
		case B4W_INIT_FLASH:       return "INIT FLASH";
		case B4W_ERASE_ALL:        return "ERASE ALL";
		case B4W_PAGE_ERASE:       return "PAGE ERASE";
		case B4W_READ:             return "READ";
		case B4W_WRITE:            return "WRITE";
		case B4W_C2CK_LOW:         return "C2CK LOW";
		case B4W_READ_EEPROM:      return "READ EEPROM";
		case B4W_WRITE_EEPROM:     return "WRITE EEPROM";
		case B4W_SET_MODE:         return "SET MODE";
		case B4W_VERIFY:           return "VERIFY";
		default:                   return nullptr;
	}
}

bool bridge4wayNameHasAddress(uint8_t cmd) {
	return cmd == B4W_PAGE_ERASE || cmd == B4W_READ || cmd == B4W_WRITE ||
	       cmd == B4W_READ_EEPROM || cmd == B4W_WRITE_EEPROM || cmd == B4W_VERIFY;
}

// ---------------------------------------------------------------------------
// the engine
// ---------------------------------------------------------------------------

/** @brief What a bootloader frame waits for. */
enum OpKind : uint8_t {
	K_SEND,     /**< Nothing: done once out (RUN). */
	K_ACK,      /**< One byte, Op::expect the good one. */
	K_SILENT,   /**< No byte for Op::waitUs: SET BUFFER's header. */
	K_READ,     /**< Op::want bytes: data, their CRC, the ACK. */
	K_CONTACT,  /**< The greeting's 9 bytes: `471`, five more, the ACK. */
};

/** @brief One bootloader frame of a command. */
struct Op {
	uint16_t off;     /**< Its bytes in s_tx. */
	uint16_t len;
	uint8_t  kind;    /**< An OpKind. */
	uint8_t  expect;  /**< K_ACK: the good byte. */
	uint16_t want;    /**< K_READ, K_CONTACT: bytes to read. */
	uint32_t waitUs;  /**< The wait once out. */
	bool     writes;  /**< Write-class: the data after SET BUFFER, PROGRAM, ERASE. */
	bool     run;     /**< RUN. */
};

/** @brief The greeting: four 0x00 and escwire's 17 bytes, 21 as Betaflight's classic builds send it. */
#define GREETING_LEN (4u + EW_A_CONTACT_LEN)

static Bridge4wayDevice s_dev = { false, 0, 0, 0 };
static uint8_t  s_infoC = 0;          /**< The greeting answer's fourth byte, after `471`. */
static bool     s_busy = false;
static uint8_t  s_cmd = 0;
static uint16_t s_addr = 0;           /**< The request's address. */
static uint16_t s_plen = 0;           /**< Its parameters' count, 1 to 256. */
static uint8_t  s_par[256];           /**< Its parameters. */
static uint8_t  s_tx[6 + 6 + 258 + 4];/**< The command's bootloader frames. */
static Op       s_ops[4];
static uint8_t  s_nOps = 0;
static uint8_t  s_op = 0;             /**< The frame under way. */
static uint8_t  s_phase = 0;          /**< 0 to hand over, 1 going out, 2 waiting for its answer. */
static uint8_t  s_tries = 0;          /**< Greetings sent beyond the first. */
static uint32_t s_handUs = 0;         /**< micros() at the handover. */
static uint32_t s_outUs = 0;          /**< micros() when it was seen out. */
static uint32_t s_recv0 = 0;          /**< The wire's received count before the handover. */
static uint8_t  s_rx[EW_RX_MAX];      /**< The bootloader's answer so far. */
static uint16_t s_rxN = 0;
/** @brief plan()'s answer for a command that sends nothing. */
#define NO_ACK_YET 0xFFu
static uint8_t  s_nowAck = NO_ACK_YET;  /**< The ACK of a command that ends without the wire. */
static uint8_t  s_ans[B4W_ANSWER_MAX];
static uint16_t s_ansLen = 0;

void bridge4wayReset() {
	s_dev = Bridge4wayDevice{ false, 0, 0, 0 };
	s_busy = false;
	s_ansLen = 0;
}

bool bridge4wayBusy() { return s_busy; }

Bridge4wayDevice bridge4wayDevice() { return s_dev; }

uint16_t bridge4wayTakeAnswer(uint8_t *out) {
	uint16_t n = s_ansLen;
	if (n) memcpy(out, s_ans, n);
	s_ansLen = 0;
	return n;
}

/** @brief Add a frame of @p len bytes at s_tx + @p off. @return The op, for its waits. */
static Op *addOp(uint16_t off, uint16_t len, uint8_t kind, uint32_t waitUs) {
	Op *o = &s_ops[s_nOps++];
	memset(o, 0, sizeof(*o));
	o->off = off;
	o->len = len;
	o->kind = kind;
	o->expect = EW_A_ACK;
	o->waitUs = waitUs;
	return o;
}

/** @brief SET ADDRESS at the front of s_tx, unless the address is 0xFFFF. @return The bytes used. */
static uint16_t addAddress(uint16_t addr) {
	if (addr == 0xFFFFu) return 0;
	uint16_t n = (uint16_t)ew_a_address(s_tx, addr);
	addOp(0, n, K_ACK, EW_A_ADDRESS_US);
	return n;
}

/** @brief SET BUFFER's header, its pause and the data with their CRC, from s_tx + @p at. */
static uint16_t addBuffer(uint16_t at, bool writes) {
	uint16_t h = (uint16_t)ew_a_announce(s_tx + at, s_plen);
	addOp(at, h, K_SILENT, B4W_BUFFER_PAUSE_US);
	uint16_t d = (uint16_t)ew_a_data(s_tx + at + h, s_par, s_plen);
	addOp((uint16_t)(at + h), d, K_ACK, EW_A_BUFFER_US)->writes = writes;
	return (uint16_t)(at + h + d);
}

/** @brief The interface mode is one whose memory commands the board runs. */
static bool modeRuns() { return s_dev.mode == B4W_MODE_SILABS || s_dev.mode == B4W_MODE_ARM; }

/** @brief The command's frames. @return The ACK of a command that ends at once; @ref NO_ACK_YET for one that goes to the wire. */
static uint8_t plan() {
	s_nOps = 0;
	uint16_t at;
	switch (s_cmd) {
		case B4W_TEST_ALIVE:
			if (!s_dev.connected) return B4W_ACK_OK;
			addOp(0, (uint16_t)ew_a_short(s_tx, EW_A_CMD_KEEP, 0), K_ACK, EW_A_ADDRESS_US)->expect =
				EW_A_REF_UNKNOWN;
			return NO_ACK_YET;
		case B4W_PROTOCOL_VERSION:
		case B4W_GET_NAME:
		case B4W_GET_VERSION:
		case B4W_EXIT:
			return B4W_ACK_OK;
		case B4W_RESET:
			if (s_par[0] != 0) return B4W_ACK_I_INVALID_CHANNEL;
			addOp(0, (uint16_t)ew_a_short(s_tx, EW_A_CMD_LEAVE, 0), K_SEND, 0)->run = true;
			return NO_ACK_YET;
		case B4W_INIT_FLASH:
			s_dev.connected = false;
			if (s_par[0] != 0) return B4W_ACK_I_INVALID_CHANNEL;
			memset(s_tx, 0, 4);
			ew_a_contact(s_tx + 4);
			addOp(0, GREETING_LEN, K_CONTACT, EW_A_CONTACT_US)->want = EW_A_ANSWER_LEN;
			return NO_ACK_YET;
		case B4W_SET_MODE:
			if (s_par[0] != B4W_MODE_SILABS && s_par[0] != B4W_MODE_ARM) return B4W_ACK_I_INVALID_PARAM;
			s_dev.mode = s_par[0];
			return B4W_ACK_OK;
		case B4W_PAGE_ERASE: {
			if (!modeRuns()) return B4W_ACK_I_INVALID_CMD;
			if (!s_dev.connected) return B4W_ACK_D_GENERAL_ERROR;
			// Page × 1024 on ARM, × 512 on SiLabs, in the high byte as
			// Betaflight computes it.
			s_addr = (uint16_t)((uint8_t)(s_par[0] << (s_dev.mode == B4W_MODE_ARM ? 2 : 1)) << 8);
			at = addAddress(s_addr);
			addOp(at, (uint16_t)ew_a_short(s_tx + at, EW_A_CMD_ERASE, 1), K_ACK, EW_A_ERASE_US)->writes = true;
			return NO_ACK_YET;
		}
		case B4W_READ: {
			if (!modeRuns()) return B4W_ACK_I_INVALID_CMD;
			if (!s_dev.connected) return B4W_ACK_D_GENERAL_ERROR;
			const uint16_t n = s_par[0] ? s_par[0] : 256;
			at = addAddress(s_addr);
			Op *o = addOp(at, (uint16_t)ew_a_read(s_tx + at, n), K_READ,
			              EW_A_READ_US + (uint32_t)(n + 3) * EW_A_GAP_US);
			o->want = (uint16_t)(n + 3);
			return NO_ACK_YET;
		}
		case B4W_WRITE:
			if (!modeRuns()) return B4W_ACK_I_INVALID_CMD;
			if (!s_dev.connected) return B4W_ACK_D_GENERAL_ERROR;
			at = addBuffer(addAddress(s_addr), true);
			addOp(at, (uint16_t)ew_a_short(s_tx + at, EW_A_CMD_PROGRAM, 1), K_ACK, EW_A_PROGRAM_US)->writes = true;
			return NO_ACK_YET;
		case B4W_VERIFY:
			if (s_dev.mode != B4W_MODE_ARM) return B4W_ACK_I_INVALID_CMD;
			if (!s_dev.connected) return B4W_ACK_D_GENERAL_ERROR;
			at = addBuffer(addAddress(s_addr), false);
			// AM32's and BLHeli_32's verify command, 0x04.
			addOp(at, (uint16_t)ew_a_short(s_tx + at, 0x04, 1), K_ACK, B4W_VERIFY_US);
			return NO_ACK_YET;
		default:
			// EraseAll, C2CK_LOW, ReadEEprom, WriteEEprom: Atmel and C2
			// interfaces are not offered; and every unknown command.
			return B4W_ACK_I_INVALID_CMD;
	}
}

/** @brief The answer's parameters for an ACK of @p ack at once. */
static void endNow(uint8_t ack) {
	uint8_t par[8] = { 0 };
	uint16_t n = 1;
	if (ack == B4W_ACK_OK) {
		if (s_cmd == B4W_PROTOCOL_VERSION) {
			par[0] = B4W_PROTOCOL;
		} else if (s_cmd == B4W_GET_NAME) {
			n = (uint16_t)strlen(B4W_NAME);
			memcpy(par, B4W_NAME, n);
		} else if (s_cmd == B4W_GET_VERSION) {
			// Betaflight's encoding (serial_4way.c lines 73 to 87): main
			// × 1000 + sub 1 × 100 + sub 2, sent as that / 100 and % 100;
			// here major, minor and patch.
			const unsigned v = FW_VERSION_MAJOR * 1000u + FW_VERSION_MINOR * 100u + FW_VERSION_PATCH;
			par[0] = (uint8_t)(v / 100u);
			par[1] = (uint8_t)(v % 100u);
			n = 2;
		}
	}
	s_ansLen = answer(s_ans, s_cmd, s_addr, par, n, ack);
}

/** @brief The device from a good greeting answer in s_rx. @return False for a signature no mode takes. */
static bool takeDevice() {
	const uint8_t hi = s_rx[4], lo = s_rx[5];
	const uint16_t sig = (uint16_t)(hi << 8 | lo);
	uint8_t mode;
	if (sig > 0xE800u && sig < 0xF900u) mode = B4W_MODE_SILABS;
	else if (hi > 0x00u && hi < 0x90u && lo == 0x06u) mode = B4W_MODE_ARM;
	else return false;
	s_dev.connected = true;
	s_dev.mode = mode;
	s_dev.signature = sig;
	s_dev.settings = mode == B4W_MODE_ARM ? blAnswerAm32Settings(hi) : 0;
	s_infoC = s_rx[3];
	return true;
}

/** @brief The command ended, its last frame judged @p ok; @p last its answer's first byte. */
static void endWire(bool ok, uint8_t last) {
	uint8_t ack = ok ? B4W_ACK_OK : B4W_ACK_D_GENERAL_ERROR;
	const uint8_t *par = nullptr;
	uint8_t info[4] = { 0 };
	uint16_t n = 1;
	switch (s_cmd) {
		case B4W_TEST_ALIVE:
			if (!ok) s_dev.connected = false;
			break;
		case B4W_RESET:
			s_dev.connected = false;
			break;
		case B4W_INIT_FLASH:
			if (ok) {
				info[0] = (uint8_t)s_dev.signature;
				info[1] = (uint8_t)(s_dev.signature >> 8);
				info[2] = s_infoC;
				info[3] = s_dev.mode;
			}
			n = 4;
			break;
		case B4W_READ:
			if (!ok) break;
			n = (uint16_t)(s_ops[s_nOps - 1].want - 3);
			par = s_rx;
			{
				int v = bridgeAm32Page3d(s_addr, s_dev.settings, s_rx, n);
				if (v >= 0) esc3dSetNote(v == 1);
			}
			break;
		case B4W_WRITE:
			// An accepted write with 3D on sets the note; one with it off
			// leaves it for the read-back to clear (esc_3d.h).
			if (ok && bridgeAm32Page3d(s_addr, s_dev.settings, s_par, s_plen) == 1) esc3dSetNote(true);
			break;
		case B4W_VERIFY:
			if (!ok && last == EW_A_REF_VERIFY) ack = B4W_ACK_I_VERIFY_ERROR;
			break;
		default:
			break;
	}
	uint8_t zero[4] = { 0 };
	if (!par) par = s_cmd == B4W_INIT_FLASH ? info : zero;
	s_ansLen = answer(s_ans, s_cmd, s_addr, par, n, ack);
	s_busy = false;
}

void bridge4wayStart(const Bridge4wayReader *r) {
	s_cmd = r->buf[1];
	s_addr = (uint16_t)(r->buf[2] << 8 | r->buf[3]);
	s_plen = r->buf[4] ? r->buf[4] : 256;
	memcpy(s_par, r->buf + 5, s_plen);
	s_busy = true;
	s_ansLen = 0;
	s_op = 0;
	s_phase = 0;
	s_tries = 0;
	s_nowAck = plan();
}

/** @brief Empty the ring into the answer so far, up to its room. */
static void collect() {
	uint8_t junk[64];
	for (;;) {
		uint16_t room = (uint16_t)(sizeof(s_rx) - s_rxN);
		uint16_t got = room ? bridgeWireRecv(s_rx + s_rxN, room) : bridgeWireRecv(junk, sizeof(junk));
		if (!got) return;
		if (room) s_rxN = (uint16_t)(s_rxN + got);
	}
}

/** @brief Judge the answer of the frame under way. @return 1 good, 0 bad, -1 not yet. */
static int judge(const Op *o, uint32_t nowUs) {
	const bool late = nowUs - s_outUs >= o->waitUs;
	switch (o->kind) {
		case K_SEND:
			return 1;
		case K_SILENT:
			if (s_rxN) return 0;
			return late ? 1 : -1;
		case K_ACK:
			if (s_rxN) return s_rx[0] == o->expect;
			return late ? 0 : -1;
		case K_READ:
			if (s_rxN >= o->want)
				return ew_crc16(s_rx, (size_t)(o->want - 1)) == 0 && s_rx[o->want - 1] == EW_A_ACK;
			return late ? 0 : -1;
		case K_CONTACT:
			if (s_rxN >= o->want)
				return s_rx[0] == '4' && s_rx[1] == '7' && s_rx[2] == '1' &&
				       s_rx[EW_A_ANSWER_LEN - 1] == EW_A_ACK && takeDevice();
			return late ? 0 : -1;
		default:
			return 0;
	}
}

Bridge4wayEvents bridge4wayStep(uint32_t nowUs) {
	Bridge4wayEvents ev;
	memset(&ev, 0, sizeof(ev));
	if (!s_busy) return ev;
	if (s_nowAck != NO_ACK_YET) {
		endNow(s_nowAck);
		s_busy = false;
		ev.done = true;
		ev.exit = s_cmd == B4W_EXIT;
		return ev;
	}
	// At most every frame of the command in one pass: each step either waits
	// or moves on.
	for (int guard = 0; guard < 16 && s_busy; guard++) {
		const Op *o = &s_ops[s_op];
		if (s_phase == 0) {
			// What the ring holds belongs to no frame of this command.
			s_rxN = 0;
			collect();
			s_rxN = 0;
			BridgeWireStats w;
			bridgeWireStats(&w);
			if (!bridgeWireSend(s_tx + o->off, o->len)) return ev;
			s_recv0 = w.received;
			s_handUs = nowUs;
			ev.bytes = (uint16_t)(ev.bytes + o->len);
			ev.frames++;
			ev.write |= o->writes;
			ev.run |= o->run;
			s_phase = 1;
			return ev;  // the frame goes out on core1 from here
		}
		collect();
		if (s_phase == 1) {
			BridgeWireStats w;
			bridgeWireStats(&w);
			if (w.received - s_recv0 < o->len &&
			    nowUs - s_handUs < (uint32_t)o->len * B4W_BYTE_US + B4W_OUT_SLACK_US)
				return ev;
			s_outUs = nowUs;
			s_phase = 2;
		}
		int r = judge(o, nowUs);
		if (r < 0) return ev;
		if (r == 0 && o->kind == K_CONTACT && s_tries + 1u < EW_A_CONTACT_TRIES) {
			s_tries++;
			s_phase = 0;
			continue;
		}
		if (r == 0 || s_op + 1 == s_nOps) {
			endWire(r == 1, s_rxN ? s_rx[0] : 0);
			ev.done = true;
			return ev;
		}
		s_op++;
		s_phase = 0;
	}
	return ev;
}
