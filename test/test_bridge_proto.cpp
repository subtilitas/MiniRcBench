/**
 * @file test_bridge_proto.cpp
 * @brief MSP and the 4-way interface of the USB bridge (slice 3 of
 *        docs/design/usb-bridge.md): bridge_msp.h and bridge_4way.h alone,
 *        then through usb_bridge.h on the CDC fake, the claim, the job and
 *        the line fake, against model bootloaders and models of the tools.
 *
 * The harness is test_usb_bridge.cpp's: a pass is 250 µs of the virtual
 * clock, one pass of the fake DShot pump, one of core1's job and one of
 * core0's tud_task() and usbBridgeTask(). The model bootloaders answer on
 * the line fake: an AM32 bootloader (signature 0x1F06, settings at 0x7C00,
 * answers 300 µs after a frame, ends a message on 260 µs of idle line and
 * drops data joined to SET BUFFER's header, keep-alive answered 0xC1) and
 * the EFM8 bootloader of BLHeli_S (signature 0xE8B2, 512-byte pages). The
 * tools are ESC-Configurator's and the AM32 configurator's sequences as the
 * design note cites them, with a reader of each MSP answer that fails on a
 * read past its end, as a JavaScript `DataView` does. Nothing here has run
 * on the chip, against a tool or against an ESC.
 */

#include "check.h"
#include "fakes.h"
#include "fake_cdc.h"
#include "fake_bridge_line.h"
#include "usb_bridge.h"
#include "bridge_msp.h"
#include "bridge_4way.h"
#include "bridge_wire.h"
#include "bridge_line_port.h"
#include "core1_job.h"
#include "esc_task.h"
#include "esc_3d.h"
#include "pin_claim.h"
#include "plat.h"
#include "tusb.h"
#include "version.h"

#include <string.h>

// ---------------------------------------------------------------------------
// harness
// ---------------------------------------------------------------------------

/** @brief Times a byte reached the wire while the claim was not Running for the bridge. */
static int g_stray = 0;
static int g_seen = 0;

static void pass() {
	fakeAdvanceUs(250);
	fakePumpPoll();
	core1JobPoll();
	tud_task();
	usbBridgeTask();
	int w = fakeBridgeLineWireCount();
	if (w != g_seen && !(pinClaimHolder() == PinUser::Bridge && bridgeLineIsOpen())) g_stray += w - g_seen;
	g_seen = w;
}

static void run(uint32_t ms) {
	for (uint32_t i = 0; i < ms * 4; i++) pass();
}

static void freshPump() {
	escTaskConfigure(0, 600, true, 1);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
}

static bool openBridge() {
	if (pinClaimBegin(PinUser::Bridge, PIN_SIG) != ClaimRefusal::None) return false;
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Running; k++) {
		pass();
		pinClaimPoll();
	}
	pass();
	return usbBridgeMode() == BridgeMode::Waiting;
}

static void endBridge() {
	pinClaimEnd(PinUser::Bridge);
	for (int i = 0; i < 2000 && pinClaimState() != ClaimState::Free; i++) {
		pinClaimRestart(PinUser::Bridge);
		pass();
	}
	pass();
}

static void fresh(FakeEscReply r) {
	for (int k = 0; k < 2000 && pinClaimState() != ClaimState::Free; k++) {
		pinClaimEnd(PinUser::Bridge);
		pinClaimRestart(PinUser::Bridge);
		pass();
	}
	usbBridgeTask();
	fakeBridgeLineReset();
	fakeBridgeLineResponder(r);
	fakeCdcReset();
	fakeCdcDtr(true);
	freshPump();
	g_seen = fakeBridgeLineWireCount();
	esc3dSetNote(false);
}

/** @brief The wire byte at @p i and its start bit's cycle. */
static uint8_t wireAt(int i, uint64_t *cy = nullptr) {
	uint8_t b;
	uint64_t s;
	bool keep;
	fakeBridgeLineWire(i, &b, &s, &keep);
	if (cy) *cy = s;
	return b;
}

/** @brief Frames on the wire from @p from: their first bytes in @p first, at most @p max. */
static int wireFrameStarts(int from, uint8_t *first, int *at, int max) {
	int n = 0;
	bool start = true;
	for (int i = from; i < fakeBridgeLineWireCount(); i++) {
		uint8_t b;
		uint64_t s;
		bool keep;
		fakeBridgeLineWire(i, &b, &s, &keep);
		if (start && n < max) {
			first[n] = b;
			at[n] = i;
			n++;
		}
		start = !keep;
	}
	return n;
}

// ---------------------------------------------------------------------------
// model bootloaders
// ---------------------------------------------------------------------------

static uint16_t crcArc(const uint8_t *d, int n) {
	uint16_t c = 0;
	for (int i = 0; i < n; i++) {
		c ^= d[i];
		for (int b = 0; b < 8; b++) c = (c & 1) ? (uint16_t)((c >> 1) ^ 0xA001) : (uint16_t)(c >> 1);
	}
	return c;
}

/** @brief A model bootloader, AM32's or the EFM8 one of BLHeli_S. */
struct ModelBl {
	bool     efm8;
	uint8_t  sigHi, sigLo;
	uint8_t  flash[65536];
	uint16_t addr;
	int      bufWant;        // SET BUFFER's count: data expected next
	uint8_t  buf[256];
	int      bufLen;
	bool     inBoot;         // RUN leaves the bootloader: no answer after it
	uint32_t delayUs;        // after every frame
	uint32_t eraseUs;        // after ERASE
	uint32_t programUs;      // after PROGRAM
	bool     mute;           // answers nothing
	int      greetings, erases, programs, runs, keepAlives, joined, badCrc, verifies;
};
static ModelBl g_bl;

static void blReset(bool efm8) {
	memset(&g_bl, 0, sizeof(g_bl));
	g_bl.efm8 = efm8;
	g_bl.sigHi = efm8 ? 0xE8 : 0x1F;
	g_bl.sigLo = efm8 ? 0xB2 : 0x06;
	memset(g_bl.flash, 0xFF, sizeof(g_bl.flash));
	g_bl.inBoot = true;
	g_bl.delayUs = 300;
	g_bl.eraseUs = 300;
	g_bl.programUs = 300;
}

/** @brief AM32 settings at 0x7C00: boot byte 1, layout 2, 14 poles; byte 0x12 the 3D mode. */
static void blSettings(uint16_t at, bool threeD) {
	memset(g_bl.flash + at, 0, 0xC0);
	g_bl.flash[at + 0] = 1;
	g_bl.flash[at + 1] = 2;
	g_bl.flash[at + 0x1B] = 14;
	g_bl.flash[at + 0x12] = threeD ? 1 : 0;
}

static int blAck(uint8_t *r, uint8_t b) {
	r[0] = b;
	return 1;
}

static int modelBl(const uint8_t *f, int n, uint8_t *r, uint32_t *d) {
	*d = g_bl.delayUs;
	if (g_bl.mute || !g_bl.inBoot) return 0;
	// The greeting: its last 9 bytes 0x0D, `BLHeli` and F4 7D.
	static const uint8_t tail[9] = {0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
	if (n >= 9 && memcmp(f + n - 9, tail, 9) == 0) {
		g_bl.greetings++;
		g_bl.bufWant = 0;
		const uint8_t a[9] = {'4', '7', '1', 'd', g_bl.sigHi, g_bl.sigLo, 0x06, 0x40, 0x30};
		memcpy(r, a, 9);
		return 9;
	}
	if (g_bl.bufWant) {
		// The data after SET BUFFER: a message of its own, 260 µs of idle
		// line after the header at least, or the bootloader takes it as
		// part of the header and the buffer is lost.
		int want = g_bl.bufWant;
		g_bl.bufWant = 0;
		if (fakeBridgeLineGapUs() < 260) {
			g_bl.joined++;
			return 0;
		}
		if (n != want + 2 || crcArc(f, n) != 0) return blAck(r, 0xC2);
		memcpy(g_bl.buf, f, want);
		g_bl.bufLen = want;
		return blAck(r, 0x30);
	}
	if (n < 4 || crcArc(f, n) != 0) {
		g_bl.badCrc++;
		return blAck(r, 0xC2);
	}
	switch (f[0]) {
		case 0xFF:
			g_bl.addr = (uint16_t)(f[2] << 8 | f[3]);
			return blAck(r, 0x30);
		case 0xFE:
			if (n != 6) {
				g_bl.joined++;
				return 0;
			}
			g_bl.bufWant = (f[2] << 8 | f[3]);
			return 0;
		case 0x03: {
			int c = f[1] ? f[1] : 256;
			memcpy(r, g_bl.flash + g_bl.addr, c);
			uint16_t crc = crcArc(r, c);
			r[c] = (uint8_t)crc;
			r[c + 1] = (uint8_t)(crc >> 8);
			r[c + 2] = 0x30;
			return c + 3;
		}
		case 0x01:
			if (!g_bl.bufLen) return blAck(r, 0xC5);
			memcpy(g_bl.flash + g_bl.addr, g_bl.buf, g_bl.bufLen);
			g_bl.programs++;
			*d = g_bl.programUs;
			return blAck(r, 0x30);
		case 0x02: {
			int page = g_bl.efm8 ? 512 : 1024;
			memset(g_bl.flash + (g_bl.addr & ~(page - 1)), 0xFF, page);
			g_bl.erases++;
			*d = g_bl.eraseUs;
			return blAck(r, 0x30);
		}
		case 0x04:
			g_bl.verifies++;
			return blAck(r, memcmp(g_bl.flash + g_bl.addr, g_bl.buf, g_bl.bufLen) == 0 ? 0x30 : 0xC0);
		case 0xFD:
			g_bl.keepAlives++;
			return blAck(r, 0xC1);
		case 0x00:
			g_bl.runs++;
			g_bl.inBoot = false;
			return 0;
		default:
			return blAck(r, 0xC1);
	}
}

// ---------------------------------------------------------------------------
// models of the tools
// ---------------------------------------------------------------------------

/** @brief An MSP answer as a tool reads it. */
struct MspReply {
	bool    ok;           // `$M`, a direction, a length, the command and a good checksum
	uint8_t dir;          // '>' or '!'
	uint8_t cmd;
	uint8_t len;
	uint8_t data[64];
};

/** @brief The tool's `DataView`: a read of @p n bytes at @p at fails past the end. */
static bool fits(const MspReply &r, int at, int n) { return at + n <= r.len; }

/** @brief Send an MSP v1 frame and read its answer as Msp.js reads one, within @p ms. */
static MspReply msp(uint8_t cmd, const uint8_t *payload = nullptr, uint8_t n = 0, uint32_t ms = 50) {
	uint8_t f[64];
	f[0] = '$';
	f[1] = 'M';
	f[2] = '<';
	f[3] = n;
	f[4] = cmd;
	uint8_t chk = (uint8_t)(n ^ cmd);
	for (int i = 0; i < n; i++) {
		f[5 + i] = payload[i];
		chk ^= payload[i];
	}
	f[5 + n] = chk;
	fakeCdcHostWrite(f, 6 + n);
	MspReply r;
	memset(&r, 0, sizeof(r));
	uint8_t in[128];
	int got = 0;
	for (uint32_t t = 0; t < ms * 4; t++) {
		pass();
		got += fakeCdcHostRead(in + got, (int)sizeof(in) - got);
		if (got >= 6 && got >= 6 + in[3]) break;
	}
	if (got < 6 || in[0] != '$' || in[1] != 'M' || got < 6 + in[3]) return r;
	r.dir = in[2];
	r.len = in[3];
	r.cmd = in[4];
	uint8_t c = (uint8_t)(r.len ^ r.cmd);
	for (int i = 0; i < r.len; i++) {
		r.data[i] = in[5 + i];
		c ^= in[5 + i];
	}
	r.ok = c == in[5 + r.len];
	return r;
}

/** @brief A 4-way answer as FourWay.js parses it. */
struct FwReply {
	bool     ok;       // whole, starting 0x2E, its CRC good
	uint8_t  cmd;
	uint16_t addr;
	uint8_t  ack;
	int      n;
	uint8_t  par[256];
	int      bytes;    // its length on the port
};

static uint16_t crcX(const uint8_t *d, int n) { return bridge4wayCrc(d, (uint16_t)n); }

/** @brief The frame FourWay.js's createMessage() builds. */
static int fwFrame(uint8_t *f, uint8_t cmd, uint16_t addr, const uint8_t *par, int n) {
	f[0] = 0x2F;
	f[1] = cmd;
	f[2] = (uint8_t)(addr >> 8);
	f[3] = (uint8_t)addr;
	f[4] = (uint8_t)(n == 256 ? 0 : n);
	memcpy(f + 5, par, n);
	uint16_t c = crcX(f, 5 + n);
	f[5 + n] = (uint8_t)(c >> 8);
	f[6 + n] = (uint8_t)c;
	return 7 + n;
}

/** @brief Read an answer from the PC's side within @p ms. */
static FwReply fwRead(uint32_t ms) {
	FwReply r;
	memset(&r, 0, sizeof(r));
	static uint8_t in[600];
	int got = 0;
	for (uint32_t t = 0; t < ms * 4; t++) {
		pass();
		got += fakeCdcHostRead(in + got, (int)sizeof(in) - got);
		if (got >= 9) {
			int pc = in[4] ? in[4] : 256;
			if (got >= 8 + pc) break;
		}
	}
	r.bytes = got;
	if (got < 9 || in[0] != 0x2E) return r;
	int pc = in[4] ? in[4] : 256;
	if (got < 8 + pc) return r;
	r.cmd = in[1];
	r.addr = (uint16_t)(in[2] << 8 | in[3]);
	r.n = pc;
	memcpy(r.par, in + 5, pc);
	r.ack = in[5 + pc];
	r.ok = crcX(in, 6 + pc) == (uint16_t)(in[6 + pc] << 8 | in[7 + pc]);
	return r;
}

/** @brief Send a 4-way command as FourWay.js does and read its answer within @p ms. */
static FwReply fw(uint8_t cmd, uint16_t addr = 0, const uint8_t *par = nullptr, int n = 1, uint32_t ms = 4000) {
	static uint8_t f[300];
	static const uint8_t zero[1] = {0};
	int len = fwFrame(f, cmd, addr, par ? par : zero, n);
	fakeCdcHostWrite(f, len);
	return fwRead(ms);
}

static FwReply fwOne(uint8_t cmd, uint8_t p0, uint16_t addr = 0, uint32_t ms = 4000) {
	return fw(cmd, addr, &p0, 1, ms);
}

/** @brief The tool's settings write, as FourWay.js's writeSettings() for an ARM ESC: write, then read back. */
static bool am32WriteSettings(uint16_t at, const uint8_t *page, int n) {
	FwReply w = fw(B4W_WRITE, at, page, n);
	if (!w.ok || w.ack != B4W_ACK_OK) return false;
	FwReply r = fwOne(B4W_READ, (uint8_t)n, at);
	return r.ok && r.ack == B4W_ACK_OK && r.n == n && memcmp(r.par, page, n) == 0;
}

// ---------------------------------------------------------------------------
// the pure parts
// ---------------------------------------------------------------------------

/** @brief Feed a whole MSP frame to a reader. */
static BridgeMspFeed mspFeedAll(BridgeMspReader *r, const uint8_t *f, int n) {
	BridgeMspFeed last = BridgeMspFeed::More;
	for (int i = 0; i < n; i++) last = bridgeMspFeed(r, f[i]);
	return last;
}

static void testMspPure() {
	section("USB bridge MSP: frames and answers");
	BridgeMspReader r;
	bridgeMspReset(&r);
	const uint8_t api[] = {'$', 'M', '<', 0, 1, 1};
	checkTrue("an MSP_API_VERSION frame: Frame", mspFeedAll(&r, api, 6) == BridgeMspFeed::Frame);
	uint8_t out[BRIDGE_MSP_ANSWER_MAX];
	BridgeMspEffect e;
	uint16_t n = bridgeMspAnswer(&r, out, &e);
	checkTrue("`$M>`, 3 bytes, 0 1 49, the XOR", n == 9 && out[2] == '>' && out[3] == 3 && out[4] == 1 &&
	          out[5] == 0 && out[6] == 1 && out[7] == 49 && out[8] == (3 ^ 1 ^ 0 ^ 1 ^ 49));
	const uint8_t bad[] = {'$', 'M', '<', 0, 1, 0};
	checkTrue("a wrong checksum: Dropped", mspFeedAll(&r, bad, 6) == BridgeMspFeed::Dropped);
	const uint8_t v2[] = {'$', 'X', '<', 0, 1, 0, 0, 0, 0};
	checkTrue("version 2, `$X<`: Dropped at the X", bridgeMspFeed(&r, '$') == BridgeMspFeed::More &&
	          bridgeMspFeed(&r, 'X') == BridgeMspFeed::Dropped && !bridgeMspInFrame(&r));
	(void)v2;
	const uint8_t out1[] = {'$', 'M', '>'};
	checkTrue("an answer's `>`: Dropped", mspFeedAll(&r, out1, 3) == BridgeMspFeed::Dropped);
	const uint8_t jumbo[] = {'$', 'M', '<', 255};
	checkTrue("the jumbo length 255: Dropped", mspFeedAll(&r, jumbo, 4) == BridgeMspFeed::Dropped);
	checkTrue("a byte other than `$` outside a frame: Dropped, no frame",
	          bridgeMspFeed(&r, 0x00) == BridgeMspFeed::Dropped && !bridgeMspInFrame(&r));

	struct Want { uint8_t cmd; int len; };
	const Want table[] = {
		{MSP_API_VERSION, 3}, {MSP_FC_VARIANT, 4}, {MSP_FC_VERSION, 3}, {MSP_BOARD_INFO, 6},
		{MSP_BUILD_INFO, 19}, {MSP_FEATURE_CONFIG, 4}, {MSP_STATUS, 22}, {MSP_MOTOR, 2},
		{MSP_BATTERY_STATE, 11}, {MSP_MOTOR_CONFIG, 7}, {MSP_UID, 12}, {MSP_SET_PASSTHROUGH, 1},
	};
	for (const Want &w : table) {
		uint8_t f[6] = {'$', 'M', '<', 0, w.cmd, w.cmd};
		bool frame = mspFeedAll(&r, f, 6) == BridgeMspFeed::Frame;
		n = bridgeMspAnswer(&r, out, &e);
		uint8_t chk = 0;
		for (int i = 3; i < n - 1; i++) chk ^= out[i];
		char what[64];
		snprintf(what, sizeof(what), "MSP %u: `$M>`, %d bytes, its checksum", w.cmd, w.len);
		checkTrue(what, frame && n == 6 + w.len && out[2] == '>' && out[3] == w.len && out[4] == w.cmd &&
		          out[n - 1] == chk);
	}
	uint8_t f[8] = {'$', 'M', '<', 0, MSP_FC_VARIANT, MSP_FC_VARIANT};
	mspFeedAll(&r, f, 6);
	bridgeMspAnswer(&r, out, &e);
	checkTrue("FC_VARIANT: MRCB", memcmp(out + 5, "MRCB", 4) == 0);
	f[4] = f[5] = MSP_STATUS;
	mspFeedAll(&r, f, 6);
	bridgeMspAnswer(&r, out, &e);
	checkTrue("STATUS: byte 15 0, the flags at 17 with bit 2, RX_FAILSAFE, set",
	          out[5 + 15] == 0 && (out[5 + 17] & 0x04) && out[5 + 18] == 0 && out[5 + 21] == 0);
	f[4] = f[5] = MSP_UID;
	mspFeedAll(&r, f, 6);
	bridgeMspAnswer(&r, out, &e);
	checkTrue("UID: the chip's 8 bytes, then 4 of 0", out[5] == 0x11 && out[12] == 0x88 && out[13] == 0 &&
	          out[16] == 0);
	f[4] = f[5] = MSP_MOTOR_CONFIG;
	mspFeedAll(&r, f, 6);
	bridgeMspAnswer(&r, out, &e);
	checkTrue("MOTOR_CONFIG: 1000, 2000, 1000, one motor at byte 6",
	          out[5] == 0xE8 && out[6] == 0x03 && out[7] == 0xD0 && out[8] == 0x07 && out[11] == 1);
	f[4] = f[5] = MSP_FC_VERSION;
	mspFeedAll(&r, f, 6);
	bridgeMspAnswer(&r, out, &e);
	checkTrue("FC_VERSION: version.h", out[5] == FW_VERSION_MAJOR && out[6] == FW_VERSION_MINOR &&
	          out[7] == FW_VERSION_PATCH);

	f[4] = f[5] = MSP_SET_MOTOR;
	mspFeedAll(&r, f, 6);
	n = bridgeMspAnswer(&r, out, &e);
	checkTrue("SET_MOTOR: `$M!`, no payload, the refusal flagged", n == 6 && out[2] == '!' && out[3] == 0 &&
	          e.motorRefused && !e.fourWay);
	f[4] = f[5] = 99;
	mspFeedAll(&r, f, 6);
	n = bridgeMspAnswer(&r, out, &e);
	checkTrue("an unknown command: `$M!`", n == 6 && out[2] == '!' && !e.motorRefused);

	// MSP_SET_PASSTHROUGH: no payload or 0xFF the 4-way interface; others 0.
	f[4] = f[5] = MSP_SET_PASSTHROUGH;
	mspFeedAll(&r, f, 6);
	bridgeMspAnswer(&r, out, &e);
	checkTrue("SET_PASSTHROUGH, no payload: 1, the 4-way interface", out[5] == 1 && e.fourWay);
	const uint8_t modes[] = {0xFF, 0xFD, 0x00, 0x01, 0x04};
	for (uint8_t m : modes) {
		uint8_t g[8] = {'$', 'M', '<', 1, MSP_SET_PASSTHROUGH, m, (uint8_t)(1 ^ MSP_SET_PASSTHROUGH ^ m)};
		bool fr = mspFeedAll(&r, g, 7) == BridgeMspFeed::Frame;
		bridgeMspAnswer(&r, out, &e);
		char what[64];
		snprintf(what, sizeof(what), "SET_PASSTHROUGH mode %02X: %s", m, m == 0xFF ? "1" : "0");
		checkTrue(what, fr && out[5] == (m == 0xFF ? 1 : 0) && e.fourWay == (m == 0xFF));
	}
	checkTrue("names", strcmp(bridgeMspName(MSP_SET_PASSTHROUGH), "SET PASSTHROUGH") == 0 &&
	          strcmp(bridgeMspName(MSP_API_VERSION), "API VERSION") == 0 && bridgeMspName(99) == nullptr);
}

/** @brief Feed a whole 4-way frame. */
static Bridge4wayFeed fwFeedAll(Bridge4wayReader *r, const uint8_t *f, int n) {
	Bridge4wayFeed last = Bridge4wayFeed::More;
	for (int i = 0; i < n; i++) last = bridge4wayFeed(r, f[i]);
	return last;
}

static void test4wayPure() {
	section("USB bridge 4-way: frames, the CRC, the 3D page");
	checkInt("CRC-16/XMODEM of 123456789", bridge4wayCrc((const uint8_t *)"123456789", 9), 0x31C3);
	Bridge4wayReader r;
	bridge4wayReaderReset(&r);
	uint8_t f[300];
	const uint8_t p0[1] = {0};
	int n = fwFrame(f, B4W_INIT_FLASH, 0, p0, 1);
	checkTrue("a frame: Frame, its length kept", fwFeedAll(&r, f, n) == Bridge4wayFeed::Frame && r.need == 8 &&
	          !bridge4wayInFrame(&r));
	f[n - 1] ^= 1;
	checkTrue("its CRC wrong: BadCrc", fwFeedAll(&r, f, n) == Bridge4wayFeed::BadCrc);
	uint8_t a[16];
	checkTrue("answered 0x2E, its command and address, one 0, 0x03, a good CRC",
	          bridge4wayCrcAnswer(&r, a) == 9 && a[0] == 0x2E && a[1] == B4W_INIT_FLASH && a[4] == 1 &&
	          a[5] == 0 && a[6] == B4W_ACK_I_INVALID_CRC && crcX(a, 7) == (uint16_t)(a[7] << 8 | a[8]));
	uint8_t big[256];
	for (int i = 0; i < 256; i++) big[i] = (uint8_t)i;
	n = fwFrame(f, B4W_WRITE, 0x1000, big, 256);
	checkTrue("256 parameters, length byte 0: 263 bytes, Frame",
	          n == 263 && f[4] == 0 && fwFeedAll(&r, f, n) == Bridge4wayFeed::Frame && r.need == 263);
	checkTrue("in a frame after its 0x2F", bridge4wayFeed(&r, 0x2F) == Bridge4wayFeed::More && bridge4wayInFrame(&r));
	bridge4wayReaderReset(&r);
	checkTrue("a byte other than 0x2F outside a frame: Dropped", bridge4wayFeed(&r, '$') == Bridge4wayFeed::Dropped);

	uint8_t page[0xC0];
	memset(page, 0, sizeof(page));
	page[0] = 1;
	page[1] = 2;
	page[0x1B] = 14;
	page[0x12] = 1;
	checkInt("3D on at the settings address", bridgeAm32Page3d(0x7C00, 0x7C00, page, 0xB8), 1);
	page[0x12] = 0;
	checkInt("off", bridgeAm32Page3d(0x7C00, 0x7C00, page, 0x30), 0);
	checkInt("29 bytes short of 0x30: not the page", bridgeAm32Page3d(0x7C00, 0x7C00, page, 0x2F), -1);
	checkInt("another address", bridgeAm32Page3d(0x7C10, 0x7C00, page, 0xB8), -1);
	checkInt("no settings page known", bridgeAm32Page3d(0, 0, page, 0xB8), -1);
	page[0] = 0xFF;
	checkInt("an erased page: not settings", bridgeAm32Page3d(0x7C00, 0x7C00, page, 0xB8), -1);
	checkTrue("names", strcmp(bridge4wayName(B4W_INIT_FLASH), "INIT FLASH") == 0 &&
	          strcmp(bridge4wayName(B4W_WRITE_EEPROM), "WRITE EEPROM") == 0 && bridge4wayName(0x50) == nullptr &&
	          bridge4wayNameHasAddress(B4W_READ) && !bridge4wayNameHasAddress(B4W_TEST_ALIVE));
}

// ---------------------------------------------------------------------------
// the tools
// ---------------------------------------------------------------------------

static void testEscConfigurator() {
	section("USB bridge: ESC-Configurator's sequence on a model AM32");
	blReset(false);
	blSettings(0x7C00, false);
	fresh(modelBl);
	checkTrue("open", openBridge());
	const int w0 = fakeBridgeLineWireCount();
	// index.jsx 236 to 294: each answer read as Msp.js reads it.
	MspReply a = msp(MSP_API_VERSION);
	checkTrue("API_VERSION: bytes 0 to 2", a.ok && a.dir == '>' && fits(a, 0, 3) && a.data[1] == 1 &&
	          a.data[2] == 49 && usbBridgeMode() == BridgeMode::Msp);
	a = msp(MSP_FC_VARIANT);
	checkTrue("FC_VARIANT: 4 bytes, MRCB", a.ok && fits(a, 0, 4) && memcmp(a.data, "MRCB", 4) == 0);
	a = msp(MSP_FC_VERSION);
	checkTrue("FC_VERSION: bytes 0 to 2", a.ok && fits(a, 0, 3));
	a = msp(MSP_BUILD_INFO);
	checkTrue("BUILD_INFO: bytes 0 to 18", a.ok && fits(a, 0, 19));
	a = msp(MSP_BOARD_INFO);
	checkTrue("BOARD_INFO: bytes 0 to 5", a.ok && fits(a, 0, 6));
	a = msp(MSP_UID);
	checkTrue("UID: three 32-bit words", a.ok && fits(a, 0, 4) && fits(a, 4, 4) && fits(a, 8, 4));
	a = msp(MSP_MOTOR);
	int above = 0;
	for (int i = 0; i + 2 <= a.len; i += 2) above += (a.data[i] | a.data[i + 1] << 8) > 0;
	checkTrue("MOTOR: one motor above 0", a.ok && above == 1);
	a = msp(MSP_FEATURE_CONFIG);
	checkTrue("FEATURE_CONFIG: bytes 0 to 3, 3D off", a.ok && fits(a, 0, 4) && !(a.data[1] & 0x10));
	a = msp(MSP_STATUS);
	checkTrue("STATUS: bytes 0 to 21, RX_FAILSAFE: not `radio on`",
	          a.ok && fits(a, 16 + a.data[15], 1) && fits(a, 17 + a.data[15], 4) && fits(a, 21 + a.data[15], 1) &&
	          (a.data[17 + a.data[15]] & 0x04));
	a = msp(MSP_BATTERY_STATE);
	checkTrue("BATTERY_STATE: bytes 0 to 10", a.ok && fits(a, 0, 11));
	checkTrue("nothing on the wire for MSP", fakeBridgeLineWireCount() == w0);
	a = msp(MSP_SET_PASSTHROUGH);
	checkTrue("SET_PASSTHROUGH: 1 ESC", a.ok && a.len == 1 && a.data[0] == 1 &&
	          usbBridgeMode() == BridgeMode::FourWay);
	// A reader that throws past the end: a reply cut by a byte fails it.
	MspReply cut = a;
	cut.len = 0;
	checkTrue("the model's reader: a payload one byte short fails", !fits(cut, 0, 1));
	run(1200);  // index.jsx 457 to 466

	FwReply r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("InitFlash: ACK_OK, 4 bytes: 06 1F, the 4th byte of the answer, ARM",
	          r.ok && r.ack == B4W_ACK_OK && r.n == 4 && r.par[0] == 0x06 && r.par[1] == 0x1F &&
	          r.par[2] == 'd' && r.par[3] == B4W_MODE_ARM);
	checkInt("the greeting once", g_bl.greetings, 1);
	r = fwOne(B4W_READ, 0xB8, 0x7C00);
	checkTrue("Read 0x7C00, 184 bytes: the page", r.ok && r.ack == B4W_ACK_OK && r.n == 0xB8 &&
	          r.addr == 0x7C00 && memcmp(r.par, g_bl.flash + 0x7C00, 0xB8) == 0);
	checkTrue("the echo dropped: the PC got the answer only", r.bytes == 8 + 0xB8);
	// The keep-alive: cmd_InterfaceTestAlive after 900 ms with no command.
	for (int k = 0; k < 3; k++) {
		run(900);
		r = fw(B4W_TEST_ALIVE);
		checkTrue("TestAlive: the bootloader's keep-alive, answered", r.ok && r.ack == B4W_ACK_OK);
	}
	checkInt("three keep-alives on the wire", g_bl.keepAlives, 3);
	uint8_t page[0xB8];
	memcpy(page, g_bl.flash + 0x7C00, sizeof(page));
	page[0x12] = 1;
	checkTrue("writeSettings: write, read back the same", am32WriteSettings(0x7C00, page, sizeof(page)));
	checkTrue("the model's flash holds it", memcmp(g_bl.flash + 0x7C00, page, sizeof(page)) == 0 &&
	          g_bl.programs == 1 && g_bl.joined == 0);
	checkTrue("the 3D note: set", esc3dNote());
	checkTrue("a write and no RUN: unfinished", usbBridgeWriteUnfinished() && !usbBridgeWriteWaiting());
	r = fwOne(B4W_RESET, 0);
	checkTrue("Reset: RUN on the wire, answered, the device dropped",
	          r.ok && r.ack == B4W_ACK_OK && g_bl.runs == 1 && !bridge4wayDevice().connected);
	checkTrue("RUN ends the unfinished write", !usbBridgeWriteUnfinished());
	r = fw(B4W_EXIT);
	run(5);
	checkTrue("Exit: answered, Waiting", r.ok && r.ack == B4W_ACK_OK && usbBridgeMode() == BridgeMode::Waiting);
	checkInt("no byte outside the claim", g_stray, 0);
	endBridge();
}

static void testAm32Configurator() {
	section("USB bridge: the AM32 configurator's sequence, its 2000 ms and a 10 s pause");
	blReset(false);
	blSettings(0x7C00, true);
	fresh(modelBl);
	openBridge();
	// SerialDevice.vue 618 to 657; commands.queue.ts 53 to 117.
	MspReply a = msp(MSP_API_VERSION);
	checkTrue("API_VERSION", a.ok && fits(a, 0, 3));
	a = msp(MSP_FC_VARIANT);
	checkTrue("FC_VARIANT: exactly 4 bytes", a.ok && a.len == 4);
	a = msp(MSP_BATTERY_STATE);
	checkTrue("BATTERY_STATE: bytes 0 to 7", a.ok && fits(a, 0, 8));
	a = msp(MSP_MOTOR_CONFIG);
	checkTrue("MOTOR_CONFIG: one motor at byte 6", a.ok && fits(a, 6, 1) && a.data[6] == 1);
	a = msp(MSP_SET_PASSTHROUGH);
	checkTrue("SET_PASSTHROUGH: 1", a.ok && a.data[0] == 1);
	run(2000);
	checkTrue("2000 ms later: the 4-way interface", usbBridgeMode() == BridgeMode::FourWay);
	FwReply r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("InitFlash", r.ok && r.ack == B4W_ACK_OK && bridge4wayDevice().settings == 0x7C00);
	r = fwOne(B4W_READ, 0xB8, 0x7C00);
	checkTrue("its read: 3D on, the note set", r.ok && r.ack == B4W_ACK_OK && esc3dNote());
	run(10000);
	checkTrue("10 s with no keep-alive: the mode and the device kept",
	          usbBridgeMode() == BridgeMode::FourWay && bridge4wayDevice().connected);
	r = fwOne(B4W_READ, 0x10, 0x7C00);
	checkTrue("a read after it", r.ok && r.ack == B4W_ACK_OK && r.n == 16);
	g_bl.flash[0x7C00 + 0x12] = 0;
	r = fwOne(B4W_READ, 0xB8, 0x7C00);
	checkTrue("a read of the page with 3D off clears the note", r.ok && !esc3dNote());
	uint8_t page[0xB8];
	memcpy(page, g_bl.flash + 0x7C00, sizeof(page));
	page[0x12] = 1;
	g_bl.programUs = 600000;  // past PROGRAM's 500 ms: refused by time
	r = fw(B4W_WRITE, 0x7C00, page, sizeof(page));
	checkTrue("a write whose PROGRAM is not answered in 500 ms: 0x0F, no note",
	          r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR && !esc3dNote());
	run(200);  // its late ACK
	g_bl.programUs = 300;
	page[0x12] = 0;
	esc3dSetNote(true);
	r = fw(B4W_WRITE, 0x7C00, page, sizeof(page));
	checkTrue("an accepted write with 3D off leaves the note to the read-back",
	          r.ok && r.ack == B4W_ACK_OK && esc3dNote());
	r = fw(B4W_EXIT);
	run(5);
	checkTrue("its disconnect, Exit alone: Waiting, the write still unfinished",
	          usbBridgeMode() == BridgeMode::Waiting && usbBridgeWriteUnfinished());
	checkTrue("the device dropped at Exit", !bridge4wayDevice().connected);
	endBridge();
}

static void testRecovery() {
	section("USB bridge: the tools' recovery, 4-way frames with no MSP before them");
	blReset(false);
	fresh(modelBl);
	openBridge();
	// index.jsx 232 to 260: InitFlash and Reset for each ESC, then Exit.
	FwReply r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("0x2F first: the 4-way interface at once, InitFlash answered",
	          r.ok && r.ack == B4W_ACK_OK && usbBridgeMode() == BridgeMode::FourWay);
	r = fwOne(B4W_RESET, 0);
	checkTrue("Reset answered", r.ok && r.ack == B4W_ACK_OK);
	r = fw(B4W_EXIT);
	run(5);
	checkTrue("Exit: Waiting", r.ok && usbBridgeMode() == BridgeMode::Waiting);
	// SerialDevice.vue 618 to 621: Exit alone.
	r = fw(B4W_EXIT);
	run(5);
	checkTrue("Exit alone: answered, Waiting", r.ok && r.cmd == B4W_EXIT && usbBridgeMode() == BridgeMode::Waiting);
	endBridge();
}

// ---------------------------------------------------------------------------
// the 4-way commands
// ---------------------------------------------------------------------------

static void testCommands() {
	section("USB bridge 4-way: each command against a model AM32");
	blReset(false);
	fresh(modelBl);
	openBridge();
	int w = fakeBridgeLineWireCount();
	FwReply r = fw(B4W_PROTOCOL_VERSION);
	checkTrue("ProtocolGetVersion: 108", r.ok && r.ack == 0 && r.n == 1 && r.par[0] == 108);
	r = fw(B4W_GET_NAME);
	checkTrue("GetName: MRCB4W", r.ok && r.n == 6 && memcmp(r.par, "MRCB4W", 6) == 0);
	r = fw(B4W_GET_VERSION);
	const unsigned v = FW_VERSION_MAJOR * 100u + FW_VERSION_MINOR * 10u + FW_VERSION_PATCH;
	checkTrue("GetVersion: two bytes", r.ok && r.n == 2 && r.par[0] == v / 100 && r.par[1] == v % 100);
	r = fw(B4W_TEST_ALIVE);
	checkTrue("TestAlive with no device: OK, nothing on the wire", r.ok && r.ack == 0 && fakeBridgeLineWireCount() == w);
	r = fwOne(B4W_READ, 16, 0x7C00);
	checkTrue("Read before InitFlash, mode 0: 0x02, nothing sent", r.ok && r.ack == B4W_ACK_I_INVALID_CMD &&
	          fakeBridgeLineWireCount() == w);
	const uint8_t modes[] = {0, 2, 3, 5};
	for (uint8_t m : modes) {
		r = fwOne(B4W_SET_MODE, m);
		char what[48];
		snprintf(what, sizeof(what), "SetMode %u: 0x09", m);
		checkTrue(what, r.ok && r.ack == B4W_ACK_I_INVALID_PARAM);
	}
	r = fwOne(B4W_SET_MODE, B4W_MODE_ARM);
	checkTrue("SetMode 4: OK", r.ok && r.ack == 0 && bridge4wayDevice().mode == B4W_MODE_ARM);
	r = fwOne(B4W_READ, 16, 0x7C00);
	checkTrue("Read in ARM mode before InitFlash: 0x0F, nothing sent",
	          r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR && fakeBridgeLineWireCount() == w);
	r = fwOne(B4W_INIT_FLASH, 1);
	checkTrue("InitFlash of ESC 1: 0x08, nothing sent", r.ok && r.ack == B4W_ACK_I_INVALID_CHANNEL &&
	          fakeBridgeLineWireCount() == w);
	r = fwOne(B4W_RESET, 1);
	checkTrue("Reset of ESC 1: 0x08, nothing sent", r.ok && r.ack == B4W_ACK_I_INVALID_CHANNEL &&
	          fakeBridgeLineWireCount() == w);
	const uint8_t refused[] = {B4W_ERASE_ALL, B4W_C2CK_LOW, B4W_READ_EEPROM, B4W_WRITE_EEPROM, 0x36, 0x41};
	for (uint8_t c : refused) {
		r = fwOne(c, 0);
		char what[48];
		snprintf(what, sizeof(what), "command %02X: 0x02, nothing sent", c);
		checkTrue(what, r.ok && r.cmd == c && r.ack == B4W_ACK_I_INVALID_CMD && fakeBridgeLineWireCount() == w);
	}
	r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("InitFlash: the 21-byte greeting", r.ok && r.ack == 0 && fakeBridgeLineWireCount() == w + 21 &&
	          wireAt(w) == 0 && wireAt(w + 12) == 0x0D && wireAt(w + 20) == 0x7D);
	w = fakeBridgeLineWireCount();
	r = fw(B4W_TEST_ALIVE);
	checkTrue("TestAlive with a device: FD 00 and its CRC, 0xC1 is good",
	          r.ok && r.ack == 0 && fakeBridgeLineWireCount() == w + 4 && wireAt(w) == 0xFD);
	g_bl.mute = true;
	r = fw(B4W_TEST_ALIVE);
	checkTrue("no answer: 0x0F and the device dropped", r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR &&
	          !bridge4wayDevice().connected);
	g_bl.mute = false;
	fwOne(B4W_INIT_FLASH, 0);
	w = fakeBridgeLineWireCount();
	memset(g_bl.flash + 0x1000, 0x5A, 64);
	r = fwOne(B4W_READ, 0, 0x1000);
	checkTrue("Read of 256 (length 0): SET ADDRESS, READ 00, 256 bytes", r.ok && r.ack == 0 && r.n == 256 &&
	          wireAt(w) == 0xFF && wireAt(w + 6) == 0x03 && wireAt(w + 7) == 0x00 && r.par[0] == 0x5A);
	w = fakeBridgeLineWireCount();
	r = fwOne(B4W_READ, 4, 0xFFFF);
	checkTrue("address 0xFFFF: no SET ADDRESS", r.ok && r.ack == 0 && wireAt(w) == 0x03);
	g_bl.flash[0x2000] = 0x77;
	g_bl.addr = 0;
	uint8_t data[4] = {1, 2, 3, 4};
	r = fw(B4W_WRITE, 0x2000, data, 4);
	checkTrue("Write: OK, the model's flash", r.ok && r.ack == 0 && memcmp(g_bl.flash + 0x2000, data, 4) == 0);
	r = fw(B4W_VERIFY, 0x2000, data, 4);
	checkTrue("Verify of the same: OK", r.ok && r.ack == 0 && g_bl.verifies == 1);
	data[0] = 9;
	r = fw(B4W_VERIFY, 0x2000, data, 4);
	checkTrue("Verify of other bytes: 0x04", r.ok && r.ack == B4W_ACK_I_VERIFY_ERROR);
	w = fakeBridgeLineWireCount();
	r = fwOne(B4W_PAGE_ERASE, 0x1F);
	checkTrue("PageErase 0x1F on ARM: address 0x7C00 in the answer and on the wire, ERASE 02 01",
	          r.ok && r.ack == 0 && r.addr == 0x7C00 && wireAt(w + 2) == 0x7C && wireAt(w + 3) == 0x00 &&
	          wireAt(w + 6) == 0x02 && wireAt(w + 7) == 0x01 && g_bl.erases == 1);
	checkTrue("an erase: unfinished", usbBridgeWriteUnfinished());
	g_bl.addr = 0x1234;
	g_bl.mute = true;
	r = fwOne(B4W_INIT_FLASH, 0, 0, 2000);
	checkTrue("InitFlash with no answer: 3 greetings, 0x0F, 4 bytes of 0, no device",
	          r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR && r.n == 4 && r.par[3] == 0 && !bridge4wayDevice().connected);
	g_bl.mute = false;
	checkInt("no byte outside the claim", g_stray, 0);
	endBridge();
}

static void testSignatures() {
	section("USB bridge 4-way: the interface mode from the signature, at its bounds");
	struct Sig { uint8_t hi, lo, mode; };
	const Sig sigs[] = {
		{0xE8, 0x00, 0}, {0xE8, 0x01, B4W_MODE_SILABS}, {0xF8, 0xFF, B4W_MODE_SILABS}, {0xF9, 0x00, 0},
		{0x00, 0x06, 0}, {0x01, 0x06, B4W_MODE_ARM}, {0x8F, 0x06, B4W_MODE_ARM}, {0x90, 0x06, 0},
		{0x1F, 0x07, 0},
	};
	blReset(false);
	fresh(modelBl);
	openBridge();
	for (const Sig &s : sigs) {
		g_bl.sigHi = s.hi;
		g_bl.sigLo = s.lo;
		g_bl.greetings = 0;
		FwReply r = fwOne(B4W_INIT_FLASH, 0);
		char what[64];
		snprintf(what, sizeof(what), "%02X%02X: %s", s.hi, s.lo,
		         s.mode == B4W_MODE_ARM ? "ARM" : s.mode ? "SiLabs" : "no device, 3 greetings");
		checkTrue(what, r.ok && (s.mode ? r.ack == 0 && r.par[3] == s.mode && g_bl.greetings == 1
		                                : r.ack == B4W_ACK_D_GENERAL_ERROR && g_bl.greetings == 3));
	}
	g_bl.sigHi = 0x2B;
	g_bl.sigLo = 0x06;
	fwOne(B4W_INIT_FLASH, 0);
	checkTrue("AM32 family 0x2B: settings at 0x7E00", bridge4wayDevice().settings == 0x7E00);
	g_bl.sigHi = 0x40;
	fwOne(B4W_INIT_FLASH, 0);
	checkTrue("an ARM family AM32 CONFIG does not know: no settings page", bridge4wayDevice().connected &&
	          bridge4wayDevice().settings == 0);
	endBridge();
}

static void testBlheliS() {
	section("USB bridge 4-way: a model BLHeli_S, as ESC-Configurator writes its settings");
	blReset(true);
	memset(g_bl.flash + 0x1A00, 0x11, 0x70);
	fresh(modelBl);
	openBridge();
	FwReply r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("InitFlash: B2 E8, SiLabs", r.ok && r.ack == 0 && r.par[0] == 0xB2 && r.par[1] == 0xE8 &&
	          r.par[3] == B4W_MODE_SILABS && bridge4wayDevice().settings == 0);
	int w = fakeBridgeLineWireCount();
	r = fwOne(B4W_PAGE_ERASE, 0x0D);
	checkTrue("PageErase 0x0D: 512-byte pages, 0x1A00", r.ok && r.ack == 0 && r.addr == 0x1A00 &&
	          wireAt(w + 2) == 0x1A && g_bl.flash[0x1A00] == 0xFF);
	uint8_t s[0x70];
	for (int i = 0; i < 0x70; i++) s[i] = (uint8_t)(i + 1);
	r = fw(B4W_WRITE, 0x1A00, s, sizeof(s));
	checkTrue("Write 0x1A00", r.ok && r.ack == 0);
	r = fwOne(B4W_READ, 0x70, 0x1A00);
	checkTrue("read back", r.ok && r.n == 0x70 && memcmp(r.par, s, 0x70) == 0);
	r = fw(B4W_VERIFY, 0x1A00, s, 4);
	checkTrue("Verify on SiLabs: 0x02", r.ok && r.ack == B4W_ACK_I_INVALID_CMD);
	checkTrue("no 3D note from an EFM8", !esc3dNote());
	endBridge();
}

// ---------------------------------------------------------------------------
// the wire rules
// ---------------------------------------------------------------------------

static void testWire() {
	section("USB bridge 4-way: the wire, SET BUFFER's pause, the waits");
	blReset(false);
	fresh(modelBl);
	openBridge();
	fwOne(B4W_INIT_FLASH, 0);
	int w = fakeBridgeLineWireCount();
	uint8_t data[256];
	for (int i = 0; i < 256; i++) data[i] = (uint8_t)(255 - i);
	FwReply r = fw(B4W_WRITE, 0x4000, data, 256);
	uint8_t first[8];
	int at[8];
	int nf = wireFrameStarts(w, first, at, 8);
	checkTrue("Write: SET ADDRESS, SET BUFFER, the data, PROGRAM: four frames",
	          r.ok && r.ack == 0 && nf == 4 && first[0] == 0xFF && first[1] == 0xFE && first[3] == 0x01);
	checkTrue("SET BUFFER 256: 01 00", wireAt(at[1] + 2) == 0x01 && wireAt(at[1] + 3) == 0x00);
	checkTrue("the data: 256 bytes and the CRC", at[3] - at[2] == 258);
	uint64_t hdrLast, dataFirst;
	wireAt(at[2] - 1, &hdrLast);
	wireAt(at[2], &dataFirst);
	// The header's last byte ends 80 cycles after its start bit: 4 ms is
	// 614 cycles.
	checkTrue("4 ms or more of idle line between the header and its data",
	          dataFirst >= hdrLast + 80 + 614);
	checkTrue("the model took them as two messages", g_bl.joined == 0 && memcmp(g_bl.flash + 0x4000, data, 256) == 0);
	bool keep;
	uint8_t b;
	uint64_t s0, s1;
	fakeBridgeLineWire(at[2], &b, &s0, &keep);
	fakeBridgeLineWire(at[2] + 1, &b, &s1, &keep);
	checkTrue("no gap within a frame: 82 cycles a byte, the pad kept driven", s1 - s0 == 82 && keep);

	// The model's rule itself: data joined to the header is lost.
	const uint8_t joinedFrame[] = {0xFE, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
	(void)joinedFrame;
	g_bl.bufWant = 1;
	g_bl.joined = 0;
	{
		// Through passthrough, a header and data in one PC write go out as
		// one frame: the model answers nothing and counts it joined.
		endBridge();
		fresh(modelBl);
		openBridge();
		uint8_t hd[9];
		hd[0] = 0xFE; hd[1] = 0; hd[2] = 0; hd[3] = 1;
		uint16_t c = crcArc(hd, 4);
		hd[4] = (uint8_t)c; hd[5] = (uint8_t)(c >> 8);
		hd[6] = 0x42;
		uint16_t c2 = crcArc(hd + 6, 1);
		hd[7] = (uint8_t)c2; hd[8] = (uint8_t)(c2 >> 8);
		g_bl.bufWant = 0;
		fakeCdcHostWrite(hd, 9);
		run(30);
		checkTrue("the model AM32: a header with its data joined is lost", g_bl.joined == 1);
	}
	endBridge();

	// ERASE's 3000 ms and PROGRAM's 500 ms, answered 1 ms before and after.
	blReset(false);
	fresh(modelBl);
	openBridge();
	fwOne(B4W_INIT_FLASH, 0);
	g_bl.eraseUs = 2999000;
	r = fwOne(B4W_PAGE_ERASE, 2, 0, 5000);
	checkTrue("ERASE answered after 2999 ms: OK", r.ok && r.ack == 0);
	g_bl.eraseUs = 3001000;
	r = fwOne(B4W_PAGE_ERASE, 2, 0, 5000);
	checkTrue("after 3001 ms: 0x0F", r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR);
	run(100);
	g_bl.programUs = 499000;
	r = fw(B4W_WRITE, 0x3000, data, 16);
	checkTrue("PROGRAM answered after 499 ms: OK", r.ok && r.ack == 0);
	g_bl.programUs = 501000;
	r = fw(B4W_WRITE, 0x3000, data, 16);
	checkTrue("after 501 ms: 0x0F", r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR);
	run(100);
	endBridge();
}

static void testWriteRules() {
	section("USB bridge 4-way: BACK and the unfinished write");
	blReset(false);
	fresh(modelBl);
	openBridge();
	fwOne(B4W_INIT_FLASH, 0);
	g_bl.programUs = 400000;
	uint8_t data[16] = {0};
	uint8_t f[40];
	int n = fwFrame(f, B4W_WRITE, 0x3000, data, 16);
	checkTrue("before: no write waits", !usbBridgeWriteWaiting() && !usbBridgeWriteUnfinished());
	fakeCdcHostWrite(f, n);
	bool waited = false, sawUnfinished = false;
	for (int i = 0; i < 4 * 100; i++) {
		pass();
		waited |= usbBridgeWriteWaiting();
		sawUnfinished |= usbBridgeWriteUnfinished();
	}
	checkTrue("from the data's handover: BACK disabled, unfinished", waited && sawUnfinished &&
	          usbBridgeWriteWaiting());
	FwReply r = fwRead(1000);
	checkTrue("the answer: BACK live again, still unfinished", r.ok && r.ack == 0 && !usbBridgeWriteWaiting() &&
	          usbBridgeWriteUnfinished());
	// A PROGRAM not answered in time: the command fails at 500 ms, its
	// answer frees BACK.
	g_bl.programUs = 10000000;
	fakeCdcHostWrite(f, n);
	run(300);
	checkTrue("no answer yet: BACK disabled", usbBridgeWriteWaiting());
	r = fwRead(3000);
	checkTrue("0x0F: BACK live", r.ok && r.ack == B4W_ACK_D_GENERAL_ERROR && !usbBridgeWriteWaiting());
	run(10000);
	g_bl.programUs = 300;
	r = fwOne(B4W_INIT_FLASH, 0);
	r = fwOne(B4W_RESET, 0);
	checkTrue("Reset's RUN: finished", !usbBridgeWriteUnfinished());

	// The port closed during a write: the claim kept, nothing more sent.
	g_bl.inBoot = true;  // the ESC power-cycled into its bootloader
	r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("power-cycled: InitFlash again", r.ok && r.ack == 0);
	g_bl.programs = 0;
	n = fwFrame(f, B4W_WRITE, 0x3000, data, 16);
	fakeCdcHostWrite(f, n);
	int w = fakeBridgeLineWireCount();
	// Wait for SET ADDRESS to go, then close.
	for (int i = 0; i < 400 && fakeBridgeLineWireCount() < w + 6; i++) pass();
	fakeCdcDtr(false);
	run(2000);
	checkTrue("DTR falls in the middle of a write: Waiting, the claim kept",
	          usbBridgeMode() == BridgeMode::Waiting && pinClaimState() == ClaimState::Running &&
	          pinClaimHolder() == PinUser::Bridge);
	checkTrue("nothing after the frame under way: no PROGRAM", g_bl.programs == 0 &&
	          fakeBridgeLineWireCount() <= w + 6 + 6);
	checkTrue("nothing reaches the PC, the late ACK of SET ADDRESS dropped", fakeCdcHostPending() == 0);
	endBridge();
}

static void testMalformed() {
	section("USB bridge: malformed and partial MSP and 4-way frames");
	blReset(false);
	fresh(modelBl);
	openBridge();
	const int w = fakeBridgeLineWireCount();
	uint8_t f[300];
	const uint8_t p0[1] = {0};
	int n = fwFrame(f, B4W_INIT_FLASH, 0, p0, 1);
	fakeCdcHostWrite(f, 4);
	run(49);
	checkTrue("half a frame, 49 ms: nothing sent, nothing answered", fakeBridgeLineWireCount() == w &&
	          fakeCdcHostPending() == 0);
	run(2);
	fakeCdcHostWrite(f + 4, n - 4);
	run(30);
	uint8_t junk[64];
	int got = fakeCdcHostRead(junk, sizeof(junk));
	checkTrue("its rest 51 ms later: dropped with the cut frame, nothing sent",
	          got == 0 && fakeBridgeLineWireCount() == w && g_bl.greetings == 0);
	FwReply r = fwOne(B4W_INIT_FLASH, 0);
	checkTrue("then a whole frame works", r.ok && r.ack == 0 && g_bl.greetings == 1);
	// A write whose CRC is wrong: answered 0x03, nothing on the wire.
	int w1 = fakeBridgeLineWireCount();
	uint8_t data[64];
	memset(data, 0xAA, sizeof(data));
	n = fwFrame(f, B4W_WRITE, 0x7C00, data, 64);
	f[n - 2] ^= 0x40;
	fakeCdcHostWrite(f, n);
	r = fwRead(100);
	checkTrue("a write with a wrong CRC: 0x03, nothing on the wire, not unfinished",
	          r.ok && r.ack == B4W_ACK_I_INVALID_CRC && r.cmd == B4W_WRITE && fakeBridgeLineWireCount() == w1 &&
	          !usbBridgeWriteUnfinished());
	// A frame split by a USB gap of 30 ms still goes whole.
	n = fwFrame(f, B4W_WRITE, 0x3000, data, 64);
	fakeCdcHostWriteGap(f, n, 64, 30000);
	r = fwRead(500);
	checkTrue("a write split by a 30 ms USB gap: whole, OK", r.ok && r.ack == 0 &&
	          memcmp(g_bl.flash + 0x3000, data, 64) == 0);
	// Bytes between frames are dropped.
	const uint8_t noise[] = {0x00, 0x13, 0x2E, 0xFF};
	int w2 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(noise, 4);
	run(10);
	r = fw(B4W_PROTOCOL_VERSION);
	checkTrue("noise between frames: dropped, the next frame answered", r.ok && r.par[0] == 108 &&
	          fakeBridgeLineWireCount() == w2 && fakeCdcHostPending() == 0);
	// MSP: a wrong checksum is not answered; a cut frame is dropped.
	const uint8_t badMsp[] = {'$', 'M', '<', 0, 1, 0};
	fakeCdcHostWrite(badMsp, 6);
	run(20);
	checkTrue("MSP with a wrong checksum: no answer", fakeCdcHostPending() == 0);
	const uint8_t cutMsp[] = {'$', 'M', '<', 2, MSP_SET_PASSTHROUGH};
	fakeCdcHostWrite(cutMsp, 5);
	run(60);
	MspReply m = msp(MSP_API_VERSION);
	checkTrue("a cut MSP frame dropped after 50 ms: the next answered", m.ok && m.cmd == MSP_API_VERSION);
	m = msp(MSP_SET_MOTOR, (const uint8_t *)"\xE8\x03\xE8\x03", 4);
	checkTrue("SET_MOTOR: `$M!`, the screen's refusal, nothing on the wire",
	          m.ok && m.dir == '!' && usbBridgeMotorRefused() && fakeBridgeLineWireCount() == w2);
	run(4990);
	checkTrue("the refusal shows 5 s", usbBridgeMotorRefused());
	run(20);
	checkTrue("and goes", !usbBridgeMotorRefused());
	BridgeLast last = usbBridgeLast();
	checkTrue("the last frame: MSP SET_MOTOR", last.cmd == BridgeCmd::Msp && last.first == MSP_SET_MOTOR);
	checkInt("no byte outside the claim", g_stray, 0);
	endBridge();
}

static void testPassthrough3d() {
	section("USB bridge passthrough: the 3D note from AM32 settings pages (decision 12)");
	blReset(false);
	blSettings(0x7C00, true);
	fresh(modelBl);
	openBridge();
	static const uint8_t INIT21[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                                   0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
	uint8_t buf[600];
	fakeCdcHostWrite(INIT21, 21);
	run(30);
	fakeCdcHostRead(buf, sizeof(buf));
	auto frame = [&](uint8_t a, uint8_t b, uint8_t c, uint8_t d, int len, uint8_t *out) {
		out[0] = a; out[1] = b;
		if (len == 6) { out[2] = c; out[3] = d; }
		uint16_t crc = crcArc(out, len - 2);
		out[len - 2] = (uint8_t)crc;
		out[len - 1] = (uint8_t)(crc >> 8);
	};
	uint8_t sa[6], rd[4];
	frame(0xFF, 0, 0x7C, 0x00, 6, sa);
	frame(0x03, 0xB8, 0, 0, 4, rd);
	fakeCdcHostWrite(sa, 6);
	run(30);
	fakeCdcHostWrite(rd, 4);
	run(200);
	checkTrue("greeting, SET ADDRESS 7C00, READ 184 with 3D on: the note set", esc3dNote());
	g_bl.flash[0x7C00 + 0x12] = 0;
	fakeCdcHostWrite(rd, 4);
	run(200);
	checkTrue("read again with it off: cleared", !esc3dNote());
	uint8_t sa2[6];
	frame(0xFF, 0, 0x7B, 0x00, 6, sa2);
	g_bl.flash[0x7B00] = 1;
	g_bl.flash[0x7B01] = 2;
	g_bl.flash[0x7B1B] = 14;
	g_bl.flash[0x7B12] = 1;
	fakeCdcHostWrite(sa2, 6);
	run(30);
	fakeCdcHostWrite(rd, 4);
	run(200);
	checkTrue("the same bytes at another address: no change", !esc3dNote());
	// Write the page with 3D on: SET ADDRESS, SET BUFFER, payload, PROGRAM.
	uint8_t page[0x30 + 2];
	memcpy(page, g_bl.flash + 0x7C00, 0x30);
	page[0x12] = 1;
	uint16_t c = crcArc(page, 0x30);
	page[0x30] = (uint8_t)c;
	page[0x31] = (uint8_t)(c >> 8);
	uint8_t sb[6], pg[4];
	frame(0xFE, 0, 0x00, 0x30, 6, sb);
	frame(0x01, 0x01, 0, 0, 4, pg);
	fakeCdcHostWrite(sa, 6);
	run(30);
	fakeCdcHostWrite(sb, 6);
	run(30);
	fakeCdcHostWrite(page, sizeof(page));
	run(60);
	checkTrue("the payload alone: no note yet", !esc3dNote());
	fakeCdcHostWrite(pg, 4);
	run(30);
	checkTrue("its PROGRAM answered 0x30: the note set", esc3dNote() && g_bl.flash[0x7C12] == 1);
	esc3dSetNote(false);
	g_bl.mute = true;
	fakeCdcHostWrite(sb, 6);
	run(30);
	fakeCdcHostWrite(page, sizeof(page));
	run(60);
	fakeCdcHostWrite(pg, 4);
	run(30);
	checkTrue("a PROGRAM not answered: no note", !esc3dNote());
	g_bl.mute = false;
	endBridge();

	// An EFM8 greeting answer: no AM32 settings page.
	blReset(true);
	blSettings(0x7C00, true);
	fresh(modelBl);
	openBridge();
	fakeCdcHostWrite(INIT21, 21);
	run(30);
	fakeCdcHostWrite(sa, 6);
	run(30);
	fakeCdcHostWrite(rd, 4);
	run(200);
	checkTrue("after an EFM8's answer the same read sets nothing", !esc3dNote());
	endBridge();
}

void runBridgeProtoTests() {
	testMspPure();
	test4wayPure();
	testEscConfigurator();
	testAm32Configurator();
	testRecovery();
	testCommands();
	testSignatures();
	testBlheliS();
	testWire();
	testWriteRules();
	testMalformed();
	testPassthrough3d();
	esc3dSetNote(false);
}
