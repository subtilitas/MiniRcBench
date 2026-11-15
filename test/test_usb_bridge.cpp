/**
 * @file test_usb_bridge.cpp
 * @brief The USB bridge's USB side (slice 2 of docs/design/usb-bridge.md):
 *        usb_bridge.h on the CDC fake, the claim, the job and the line fake,
 *        with DshotDisplay's bridge tests ported to it.
 *
 * A pass here is 250 µs of the virtual clock: one pass of the fake DShot
 * pump, one of core1's job and one of usbBridgeTask(), core0's. A test that
 * says core0 was away for a repaint runs core1 alone for that long. The PC
 * is test/fake_cdc.h: 64-byte packets, the board's 64-byte receive buffer
 * and its 256-byte transmit buffer. The ESC is the line fake's model ESC,
 * whose cycle counts test/pio_bridge_emu.py measures. The tools are models
 * of their read rules, from the lines the design note cites. Nothing here
 * has run on the chip or against a tool.
 */

#include "check.h"
#include "fakes.h"
#include "fake_cdc.h"
#include "fake_bridge_line.h"
#include "usb_bridge.h"
#include "bridge_wire.h"
#include "bridge_line_port.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "ui.h"
#include "plat.h"
#include "tusb.h"
#include "usb_dev.h"
#include "clock.h"
#include "fake_i2c.h"
#include "board_pins.h"

#include <string.h>

// ---------------------------------------------------------------------------
// harness
// ---------------------------------------------------------------------------

/** @brief Times a PC byte reached the wire while the claim was not Running for the bridge. */
static int g_strayBytes = 0;
/** @brief The wire's byte count at the last check. */
static int g_wireSeen = 0;
/** @brief Times DShot was up while the bridge's line was open. */
static int g_together = 0;
/** @brief Core0's pass runs the real usbDevTask(), the clock's line with it, not tud_task() alone. */
static bool g_devTask = false;

/** @brief One pass of the three: the pump, core1's job, then core0's bridge task. */
static void pass(bool core0 = true) {
	fakeAdvanceUs(250);
	fakePumpPoll();
	core1JobPoll();
	if (core0) {
		if (g_devTask) usbDevTask();
		else tud_task();
		usbBridgeTask();
	}
	if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
	int w = fakeBridgeLineWireCount();
	if (w != g_wireSeen && !(pinClaimHolder() == PinUser::Bridge && bridgeLineIsOpen()))
		g_strayBytes += w - g_wireSeen;
	g_wireSeen = w;
}

/** @brief Passes for @p ms; core0 runs in each unless @p core0 is false. */
static void run(uint32_t ms, bool core0 = true) {
	for (uint32_t i = 0; i < ms * 4; i++) pass(core0);
}

/** @brief The pump bound on SIG GP0, no ESC answering. */
static void freshPump() {
	escTaskConfigure(0, 600, true, 1);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
}

/** @brief A bridge claim on SIG to Running, as the screen polls it; the bridge open. */
static bool openBridge() {
	if (pinClaimBegin(PinUser::Bridge, PIN_SIG) != ClaimRefusal::None) return false;
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Running; k++) {
		pass();
		pinClaimPoll();
	}
	pass();
	return usbBridgeMode() != BridgeMode::Closed;
}

/** @brief BACK, and passes until Free or @p ms ms. */
static void endBridge(uint32_t ms = 400) {
	pinClaimEnd(PinUser::Bridge);
	for (uint32_t i = 0; i < ms * 4 && pinClaimState() != ClaimState::Free; i++) pass();
	pass();
}

/** @brief Line, PC and claim fresh; the model ESC answers with @p r. */
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
	g_wireSeen = fakeBridgeLineWireCount();
}

/** @brief The PC's side, all of it. */
static int hostAll(uint8_t *buf, int room) { return fakeCdcHostRead(buf, room); }

/** @brief The bytes on the wire from index @p from equal @p want, as one frame. */
static bool wireFrame(int from, const uint8_t *want, int n) {
	if (fakeBridgeLineWireCount() < from + n) return false;
	for (int i = 0; i < n; i++) {
		uint8_t b;
		uint64_t s;
		bool keep;
		fakeBridgeLineWire(from + i, &b, &s, &keep);
		if (b != want[i] || keep != (i + 1 < n)) return false;
	}
	return true;
}

/** @brief Frames on the wire since index @p from: bytes after which the pad was let go. */
static int wireFrames(int from) {
	int n = 0;
	for (int i = from; i < fakeBridgeLineWireCount(); i++) {
		uint8_t b;
		uint64_t s;
		bool keep;
		fakeBridgeLineWire(i, &b, &s, &keep);
		n += !keep;
	}
	return n;
}

static UsbBridgeStats bstats() {
	UsbBridgeStats s;
	usbBridgeStats(&s);
	return s;
}

// ---------------------------------------------------------------------------
// model ESCs
// ---------------------------------------------------------------------------

/** @brief Its ACK, 0x30, 300 µs after the frame. */
static int escAck(const uint8_t *, int, uint8_t *r, uint32_t *d) {
	r[0] = 0x30;
	*d = 300;
	return 1;
}

/** @brief The reply the next escReply() gives, and its delay. */
static uint8_t  g_reply[1100];
static int      g_replyLen = 0;
static uint32_t g_replyUs = 300;
static int escReply(const uint8_t *, int, uint8_t *r, uint32_t *d) {
	memcpy(r, g_reply, g_replyLen);
	*d = g_replyUs;
	return g_replyLen;
}
static void willReply(const uint8_t *d, int n, uint32_t us) {
	if (n) memcpy(g_reply, d, n);
	g_replyLen = n;
	g_replyUs = us;
}

/**
 * @brief A model AM32 bootloader as far as the tools' rules need it: READ
 *        answers its count of bytes, its CRC and the ACK; SET BUFFER
 *        nothing, and the frame after it, its payload, the ACK; anything
 *        else the ACK. 300 µs after the frame.
 */
static bool g_blPayload = false;
static int escBootloader(const uint8_t *f, int n, uint8_t *r, uint32_t *d) {
	*d = 300;
	if (g_blPayload) {
		g_blPayload = false;
		r[0] = 0x30;
		return 1;
	}
	if (f[0] == 0xFE && n == 6) {
		g_blPayload = true;
		return 0;
	}
	if (f[0] == 0x03 && n == 4) {
		int c = f[1] ? f[1] : 256;
		for (int i = 0; i < c; i++) r[i] = (uint8_t)(i * 3 + 1);
		r[c] = 0x12;
		r[c + 1] = 0x34;
		r[c + 2] = 0x30;
		return c + 3;
	}
	r[0] = 0x30;
	return 1;
}

static const uint8_t SET_ADDR[] = {0xFF, 0x00, 0x7E, 0x00, 0x3C, 0x8B};
static const uint8_t SET_BUF[]  = {0xFE, 0x00, 0x01, 0x02, 0x8A, 0x1C};
static const uint8_t PROG[]     = {0x01, 0x01, 0x11, 0x22};
static const uint8_t ERASE[]    = {0x02, 0x01, 0x11, 0x22};
static const uint8_t RUN[]      = {0x00, 0x00, 0x00, 0x00};   // 00 00 and its CRC, 0000h
static const uint8_t INIT21[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};

// ---------------------------------------------------------------------------
// the pure rules
// ---------------------------------------------------------------------------

static void testNaming() {
	section("USB bridge: a PC frame named by its first byte");
	checkTrue("no bytes: None", usbBridgeCmdOf(SET_ADDR, 0, false) == BridgeCmd::None);
	checkTrue("0xFF SET ADDRESS", usbBridgeCmdOf(SET_ADDR, 6, false) == BridgeCmd::SetAddress);
	checkTrue("0xFE SET BUFFER", usbBridgeCmdOf(SET_BUF, 6, false) == BridgeCmd::SetBuffer);
	const uint8_t rd[] = {0x03, 0x40, 0, 0}, ka[] = {0xFD, 0x00, 0, 0}, other[] = {0x42, 1};
	checkTrue("0x03 READ", usbBridgeCmdOf(rd, 4, false) == BridgeCmd::Read);
	checkTrue("0x01 PROGRAM", usbBridgeCmdOf(PROG, 4, false) == BridgeCmd::Program);
	checkTrue("0x02 ERASE", usbBridgeCmdOf(ERASE, 4, false) == BridgeCmd::Erase);
	checkInt("CRC-16/ARC of 123456789", usbBridgeCrc16((const uint8_t *)"123456789", 9), 0xBB3D);
	checkInt("of BLHeli: the greeting's F4 7D", usbBridgeCrc16((const uint8_t *)"BLHeli", 6), 0x7DF4);
	checkTrue("0x00 0x00 and its CRC: RUN", usbBridgeCmdOf(RUN, 4, false) == BridgeCmd::Run);
	const uint8_t runBad[] = {0x00, 0x00, 0x11, 0x22}, run5[] = {0x00, 0x00, 0x00, 0x00, 0x00};
	const uint8_t run01[] = {0x00, 0x01, 0xC1, 0xC0};
	checkTrue("a wrong CRC: no RUN", usbBridgeCmdOf(runBad, 4, false) == BridgeCmd::Other);
	checkTrue("5 bytes: no RUN", usbBridgeCmdOf(run5, 5, false) == BridgeCmd::Other);
	checkTrue("3 bytes: no RUN", usbBridgeCmdOf(run5, 3, false) == BridgeCmd::Other);
	checkTrue("00 01 and its CRC: RUN", usbBridgeCrc16(run01, 2) == 0xC0C1 &&
	          usbBridgeCmdOf(run01, 4, false) == BridgeCmd::Run);
	checkTrue("0xFD KEEP ALIVE", usbBridgeCmdOf(ka, 4, false) == BridgeCmd::KeepAlive);
	checkTrue("0x42: FRAME", usbBridgeCmdOf(other, 2, false) == BridgeCmd::Other);
	checkTrue("the 21-byte greeting, twelve 0x00 first: INIT, not RUN",
	          usbBridgeCmdOf(INIT21, 21, false) == BridgeCmd::Init);
	checkTrue("the 17-byte greeting: INIT", usbBridgeCmdOf(INIT21 + 4, 17, false) == BridgeCmd::Init);
	checkTrue("the marker one byte early: not INIT", usbBridgeCmdOf(INIT21, 20, false) == BridgeCmd::Other);
	checkTrue("its last 9 bytes alone: INIT", usbBridgeCmdOf(INIT21 + 12, 9, false) == BridgeCmd::Init);
	checkTrue("8 of them: not", usbBridgeCmdOf(INIT21 + 13, 8, false) == BridgeCmd::Other);
	checkTrue("after SET BUFFER, any frame is its PAYLOAD",
	          usbBridgeCmdOf(SET_ADDR, 6, true) == BridgeCmd::Payload &&
	          usbBridgeCmdOf(RUN, 4, true) == BridgeCmd::Payload &&
	          usbBridgeCmdOf(INIT21, 21, true) == BridgeCmd::Payload);
	checkTrue("write-class: PROGRAM, ERASE, PAYLOAD and nothing else",
	          usbBridgeCmdWrites(BridgeCmd::Program) && usbBridgeCmdWrites(BridgeCmd::Erase) &&
	          usbBridgeCmdWrites(BridgeCmd::Payload) && !usbBridgeCmdWrites(BridgeCmd::SetBuffer) &&
	          !usbBridgeCmdWrites(BridgeCmd::SetAddress) && !usbBridgeCmdWrites(BridgeCmd::Read) &&
	          !usbBridgeCmdWrites(BridgeCmd::Run) && !usbBridgeCmdWrites(BridgeCmd::Init) &&
	          !usbBridgeCmdWrites(BridgeCmd::KeepAlive) && !usbBridgeCmdWrites(BridgeCmd::Other) &&
	          !usbBridgeCmdWrites(BridgeCmd::None));
	checkTrue("names", strcmp(usbBridgeCmdName(BridgeCmd::SetAddress), "SET ADDRESS") == 0 &&
	          strcmp(usbBridgeCmdName(BridgeCmd::Payload), "PAYLOAD") == 0 &&
	          strcmp(usbBridgeCmdName(BridgeCmd::Init), "INIT") == 0 &&
	          strcmp(usbBridgeCmdName(BridgeCmd::KeepAlive), "KEEP ALIVE") == 0 &&
	          strcmp(usbBridgeCmdName(BridgeCmd::Other), "FRAME") == 0 &&
	          strcmp(usbBridgeCmdName(BridgeCmd::None), "") == 0);
}

// ---------------------------------------------------------------------------
// DshotDisplay's eight sections, ported
// ---------------------------------------------------------------------------

/** @brief A SET ADDRESS frame reaches the wire whole, and the PC gets its echo, then the ACK. */
static void testIntact() {
	section("USB bridge: a frame reaches the wire intact (DshotDisplay)");
	fresh(escAck);
	bridgeWireKeepEcho(false);   // as the 4-way interface of slice 3 leaves it
	checkTrue("the bridge opens on Running", openBridge());
	checkTrue("Waiting for the PC's first byte", usbBridgeMode() == BridgeMode::Waiting);
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(20);
	checkTrue("the frame on the wire, whole and back to back", wireFrame(w0, SET_ADDR, sizeof(SET_ADDR)));
	checkInt("one frame", wireFrames(w0), 1);
	uint8_t host[64];
	int hn = hostAll(host, sizeof(host));
	checkInt("the PC gets the echo and the ACK", hn, (long)sizeof(SET_ADDR) + 1);
	checkTrue("the echo first: the open keeps echoes, whatever was set before",
	          memcmp(host, SET_ADDR, sizeof(SET_ADDR)) == 0);
	checkInt("the ACK last", host[hn - 1], 0x30);
	checkTrue("Passthrough", usbBridgeMode() == BridgeMode::Passthrough);
	UsbBridgeStats s = bstats();
	checkTrue("counted: 6 bytes to the ESC, 7 to the PC, one frame",
	          s.toEsc == 6 && s.toHost == 7 && s.frames == 1 && s.lost == 0 && s.collisions == 0);
	BridgeLast l = usbBridgeLast();
	checkTrue("named SET ADDRESS 7E00, 6 bytes", l.cmd == BridgeCmd::SetAddress && l.addr == 0x7E00 &&
	          l.len == 6 && l.first == 0xFF);
	endBridge();
}

/** @brief A READ's reply sliced from its end, as the Python tool slices it. */
static void testFraming() {
	section("USB bridge: the reply's offsets from the end (DshotDisplay)");
	fresh(escReply);
	openBridge();
	const uint8_t cmd[] = {0x03, 0x30, 0x00, 0x00};
	const uint8_t rep[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x30};
	willReply(rep, sizeof(rep), 300);
	fakeCdcHostWrite(cmd, sizeof(cmd));
	run(20);
	uint8_t host[64];
	int hn = hostAll(host, sizeof(host));
	checkInt("echo and reply", hn, (long)(sizeof(cmd) + sizeof(rep)));
	checkTrue("[-1] the ACK, [-5] the ESC type slot, [-8] and [-7] the data",
	          hn == 12 && host[hn - 1] == 0x30 && host[hn - 5] == 0xDD && host[hn - 8] == 0xAA &&
	          host[hn - 7] == 0xBB);
	endBridge();
}

/** @brief SET BUFFER has no answer: its echo arrives, nothing waits, the payload follows. */
static void testNoStall() {
	section("USB bridge: an unacknowledged SET BUFFER does not stall (DshotDisplay)");
	fresh(escReply);
	openBridge();
	willReply(nullptr, 0, 300);
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(SET_BUF, sizeof(SET_BUF));
	run(7);
	uint8_t host[400];
	checkInt("the echo within 7 ms, no answer", hostAll(host, sizeof(host)), (long)sizeof(SET_BUF));
	checkTrue("named SET BUFFER", usbBridgeLast().cmd == BridgeCmd::SetBuffer);
	uint8_t payload[258];
	for (int i = 0; i < 258; i++) payload[i] = (uint8_t)(i * 7 + 3);
	willReply((const uint8_t *)"\x30", 1, 300);
	fakeCdcHostWrite(payload, sizeof(payload));
	run(4);
	checkTrue("the payload went to the slot 4 ms after the PC sent it",
	          usbBridgeLast().cmd == BridgeCmd::Payload && usbBridgeLast().len == 258);
	run(160);
	checkTrue("on the wire as its own frame, after the header's",
	          wireFrame(w0, SET_BUF, 6) && wireFrame(w0 + 6, payload, 258) && wireFrames(w0) == 2);
	int hn = hostAll(host, sizeof(host));
	checkTrue("its echo and the ACK", hn == 259 && memcmp(host, payload, 258) == 0 && host[258] == 0x30);
	endBridge();
}

/** @brief PROGRAM answered 20 ms late, core0 away 40 ms: the ring keeps it. */
static void testLateRepaint() {
	section("USB bridge: a late reply during a repaint (DshotDisplay)");
	fresh(escReply);
	openBridge();
	willReply((const uint8_t *)"\x30", 1, 20000);
	fakeCdcHostWrite(PROG, sizeof(PROG));
	run(3);
	run(40, false);
	checkInt("nothing lost on the wire", (long)bstats().lost, 0);
	checkInt("the PC has nothing yet: core0 was away", fakeCdcHostPending(), 0);
	pass();
	uint8_t host[16];
	int hn = hostAll(host, sizeof(host));
	checkTrue("echo, then the late ACK, in one pass of core0",
	          hn == 5 && memcmp(host, PROG, 4) == 0 && host[4] == 0x30);
	endBridge();
}

/** @brief A 25-byte reply 25 ms late, core0 away 40 ms: the ring keeps all of it. */
static void testLateWhole() {
	section("USB bridge: the ring absorbs a whole late reply (DshotDisplay)");
	fresh(escReply);
	openBridge();
	uint8_t page[25];
	for (int i = 0; i < 25; i++) page[i] = (uint8_t)(0xA0 + i);
	willReply(page, sizeof(page), 25000);
	const uint8_t cmd[] = {0x03, 0x16, 0x00, 0x00};
	fakeCdcHostWrite(cmd, sizeof(cmd));
	run(3);
	run(60, false);
	pass();
	uint8_t host[64];
	int hn = hostAll(host, sizeof(host));
	checkTrue("echo and the whole reply, in order",
	          hn == 29 && memcmp(host, cmd, 4) == 0 && memcmp(host + 4, page, 25) == 0);
	checkInt("nothing lost", (long)bstats().lost, 0);
	endBridge();
}

/**
 * @brief The Python tool's `_receive_ack()`: every 25 ms `read_all()`; a
 *        read of more than one byte ending in 0x30 is the ACK, any other
 *        read is thrown away (DshotDisplay's test_bridge.cpp lines 44 to 145).
 */
static bool pythonAck(uint32_t maxMs, int *reads, int *discards) {
	*reads = *discards = 0;
	for (uint32_t t = 0; t < maxMs; t += 25) {
		run(25);
		uint8_t buf[600];
		int got = hostAll(buf, sizeof(buf));
		if (got == 0) continue;
		(*reads)++;
		if (got > 1 && buf[got - 1] == 0x30) return true;
		(*discards)++;
	}
	return false;
}

static void testToolAckRule() {
	section("USB bridge: the Python tool's own ACK rule (DshotDisplay)");
	fresh(escReply);
	openBridge();
	uint8_t payload[258];
	for (int i = 0; i < 258; i++) payload[i] = (uint8_t)(i * 7 + 3);
	willReply((const uint8_t *)"\x30", 1, 4000);
	fakeCdcHostWrite(payload, sizeof(payload));
	int reads, discards;
	bool acked = pythonAck(1250, &reads, &discards);
	checkTrue("the tool takes the 256-byte write's ACK", acked);
	checkTrue("its reads saw the echo's tail with the ACK", reads >= 2 && discards == reads - 1);
	checkInt("nothing lost", (long)bstats().lost, 0);

	// A bare ACK of a short frame: the echo makes the read longer than 1.
	fresh(escAck);
	openBridge();
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	checkTrue("SET ADDRESS's ACK too, in the first read", pythonAck(50, &reads, &discards) && reads == 1);
	endBridge();
}

/** @brief The counts, what the PC queued before the open, the console. */
static void testHousekeeping() {
	section("USB bridge: housekeeping (DshotDisplay)");
	fresh(escAck);
	const uint8_t junk[] = {1, 2, 3};
	fakeCdcHostWrite(junk, sizeof(junk));
	checkInt("before the claim: the PC's bytes not read", fakeCdcHostQueued(), 3);
	checkTrue("the console has the port", fakeConsoleOn());
	pinClaimBegin(PinUser::Bridge, PIN_SIG);
	pass();
	pass();
	checkTrue("Stopping and Held: nothing read, the console still on",
	          fakeCdcHostQueued() == 3 && fakeConsoleOn() && usbBridgeMode() == BridgeMode::Closed);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Running; k++) {
		pass();
		pinClaimPoll();
	}
	const int w0 = fakeBridgeLineWireCount();
	pass();
	checkTrue("Running: open, the console off", usbBridgeMode() == BridgeMode::Waiting && !fakeConsoleOn());
	checkInt("what the PC queued before the open dropped", fakeCdcHostQueued(), 0);
	run(10);
	checkInt("and never sent", fakeBridgeLineWireCount(), w0);
	checkTrue("Waiting still: dropped bytes start no session", usbBridgeMode() == BridgeMode::Waiting);
	fakeCdcHostWrite(junk, sizeof(junk));
	run(10);
	UsbBridgeStats s = bstats();
	checkTrue("counts: 3 to the ESC, 4 to the PC", s.toEsc == 3 && s.toHost == 4 && s.frames == 1);
	endBridge();
	checkTrue("BACK: closed, the console back at Free", usbBridgeMode() == BridgeMode::Closed &&
	          fakeConsoleOn() && pinClaimState() == ClaimState::Free);
	checkInt("one switch off, one on", fakeConsoleSwitches(), 2);
	checkInt("no PC byte on the wire outside the claim", g_strayBytes, 0);
}

// ---------------------------------------------------------------------------
// the AM32 configurator's Direct mode
// ---------------------------------------------------------------------------

/**
 * @brief The AM32 configurator in Direct mode (`direct.ts` lines 248 to
 *        265): send, wait up to 2000 ms for its own length plus the reply's,
 *        strip its own length from the front. Reads every 1 ms.
 * @return The reply's length after the strip; -1 when the wait ran out.
 */
static int directSend(const uint8_t *cmd, int n, int want, uint8_t *reply) {
	fakeCdcHostWrite(cmd, n);
	static uint8_t acc[700];
	int got = 0;
	for (int t = 0; t < 2000 && got < n + want; t++) {
		run(1);
		got += hostAll(acc + got, (int)sizeof(acc) - got);
	}
	if (got < n + want) return -1;
	if (memcmp(acc, cmd, n) != 0) return -2;
	memcpy(reply, acc + n, got - n);
	return got - n;
}

static void testDirect() {
	section("USB bridge: the AM32 configurator's Direct rule");
	fresh(escBootloader);
	g_blPayload = false;
	openBridge();
	uint8_t reply[700];
	int r = directSend(SET_ADDR, sizeof(SET_ADDR), 1, reply);
	checkTrue("SET ADDRESS: its ACK after the strip", r == 1 && reply[0] == 0x30);
	const uint8_t read64[] = {0x03, 0x40, 0x12, 0x34};
	r = directSend(read64, sizeof(read64), 67, reply);
	checkTrue("a read of 64 bytes: 64, the CRC and the ACK",
	          r == 67 && reply[0] == 1 && reply[63] == (uint8_t)(63 * 3 + 1) && reply[66] == 0x30);
	r = directSend(SET_BUF, sizeof(SET_BUF), 0, reply);
	checkInt("SET BUFFER: no reply, only its echo stripped", r, 0);
	uint8_t payload[258];
	for (int i = 0; i < 258; i++) payload[i] = (uint8_t)(255 - i);
	r = directSend(payload, sizeof(payload), 1, reply);
	checkTrue("the 256-byte write and its CRC: the ACK", r == 1 && reply[0] == 0x30);
	r = directSend(PROG, sizeof(PROG), 1, reply);
	checkTrue("PROGRAM: the ACK", r == 1 && reply[0] == 0x30);
	checkInt("nothing lost", (long)bstats().lost, 0);
	endBridge();
}

// ---------------------------------------------------------------------------
// the gather, the modes, the port
// ---------------------------------------------------------------------------

static void testGather() {
	section("USB bridge: the gather");
	fresh(nullptr);
	openBridge();
	uint8_t payload[258];
	for (int i = 0; i < 258; i++) payload[i] = (uint8_t)(i + 1);
	int w0 = fakeBridgeLineWireCount();
	// Five packets; core0 reads one a pass. After the third it is away 30 ms.
	fakeCdcHostWrite(payload, sizeof(payload));
	pass();
	pass();
	pass();
	checkInt("three packets read", fakeCdcHostQueued(), 258 - 192);
	run(30, false);
	run(200);
	checkTrue("a 30 ms pass between packets 3 and 4: one frame of 258 on the wire",
	          wireFrame(w0, payload, 258) && wireFrames(w0) == 1);
	checkInt("one frame counted", (long)bstats().frames, 1);

	// Core0 passes every 250 µs while the PC is silent.
	const uint8_t a[] = {0x11, 0x22, 0x33}, b[] = {0x44, 0x55};
	w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(a, 3);
	run(3);
	fakeCdcHostWrite(b, 2);
	run(20);
	checkTrue("two writes 3 ms apart: two frames", wireFrames(w0) == 2 && wireFrame(w0, a, 3) &&
	          wireFrame(w0 + 3, b, 2));
	w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(a, 3);
	run(1);
	fakeCdcHostWrite(b, 2);
	run(20);
	const uint8_t ab[] = {0x11, 0x22, 0x33, 0x44, 0x55};
	checkTrue("1 ms apart: one frame of both", wireFrames(w0) == 1 && wireFrame(w0, ab, 5));

	// The 2 ms: the pass 1999 µs after the last byte keeps gathering, the one at 2000 hands over.
	uint32_t frames = bstats().frames;
	fakeCdcHostWrite(a, 3);
	pass();
	for (int k = 0; k < 7; k++) pass();
	fakeAdvanceUs(249);
	tud_task();
	usbBridgeTask();
	checkTrue("1999 µs after the last byte: still gathering", bstats().frames == frames);
	fakeAdvanceUs(1);
	tud_task();
	usbBridgeTask();
	checkTrue("2000 µs: handed over", bstats().frames == frames + 1 && usbBridgeLast().len == 3);
	run(20);

	// More than a slot's 320 bytes in one write: 320, then the rest.
	uint8_t big[400];
	for (int i = 0; i < 400; i++) big[i] = (uint8_t)(i * 5);
	w0 = fakeBridgeLineWireCount();
	frames = bstats().frames;
	fakeCdcHostWrite(big, sizeof(big));
	for (int k = 0; k < 5; k++) pass();
	checkTrue("a full slot goes at once: 5 packets, 1.25 ms, no 2 ms wait",
	          bstats().frames == frames + 1 && usbBridgeLast().len == 320);
	run(400);
	checkTrue("400 bytes: a full slot of 320, then 80", wireFrame(w0, big, 320) &&
	          wireFrame(w0 + 320, big + 320, 80) && wireFrames(w0) == 2);
	endBridge();
}

static void testModes() {
	section("USB bridge: the mode, the idle time, the port");
	fresh(escAck);
	openBridge();
	const uint8_t first[] = {'$', 'M', '<', 0, 1, 1};
	fakeCdcHostWrite(first, sizeof(first));
	run(10);
	checkTrue("a `$` first: passthrough in slice 2", usbBridgeMode() == BridgeMode::Passthrough);
	run(1980);
	checkTrue("1990 ms on: still", usbBridgeMode() == BridgeMode::Passthrough);
	UsbBridgeStats s = bstats();
	uint32_t idleFrom = s.lastMs;
	while (millis() - idleFrom < BRIDGE_IDLE_MS - 1) pass();
	checkTrue("1999 ms with no byte: Passthrough", usbBridgeMode() == BridgeMode::Passthrough);
	while (millis() - idleFrom < BRIDGE_IDLE_MS) pass();
	checkTrue("2000 ms: Waiting", usbBridgeMode() == BridgeMode::Waiting);

	const uint8_t slash[] = {'/', 0x37, 0, 0, 1, 0, 0, 0};
	fakeCdcHostWrite(slash, sizeof(slash));
	run(10);
	checkTrue("a `/` first: passthrough", usbBridgeMode() == BridgeMode::Passthrough);
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(first, sizeof(first));
	run(10);
	checkTrue("a later `$` switches nothing and is forwarded",
	          usbBridgeMode() == BridgeMode::Passthrough && wireFrame(w0, first, sizeof(first)));

	// The port: closed only when DTR falls after it was set.
	fakeCdcDtr(false);
	run(5);
	checkTrue("DTR falls: Waiting, the closed line", usbBridgeMode() == BridgeMode::Waiting &&
	          usbBridgePortClosed() && !usbBridgePortOpen());
	fakeCdcHostWrite(INIT21, sizeof(INIT21));
	run(20);
	checkTrue("a tool that never sets DTR: passthrough, the line gone",
	          usbBridgeMode() == BridgeMode::Passthrough && !usbBridgePortClosed());
	checkTrue("named INIT", usbBridgeLast().cmd == BridgeCmd::Init);
	run(500);
	checkTrue("DTR clear throughout: the mode holds", usbBridgeMode() == BridgeMode::Passthrough);
	run(1600);
	checkTrue("until the idle time", usbBridgeMode() == BridgeMode::Waiting && !usbBridgePortClosed());
	fakeCdcDtr(true);
	run(2);
	fakeCdcDtr(false);
	run(2);
	checkTrue("a close in Waiting after traffic: the line", usbBridgePortClosed());
	const uint8_t sb[] = {0xFE, 0x00, 0x01, 0x02, 0x8A, 0x1C}, rd[] = {0x03, 0x10, 0x00, 0x00};
	fakeCdcHostWrite(sb, sizeof(sb));
	run(10);
	fakeCdcDtr(true);
	run(2);
	fakeCdcDtr(false);
	run(2);
	fakeCdcHostWrite(rd, sizeof(rd));
	run(10);
	checkTrue("a SET BUFFER, the port closed, then READ: a new session, READ, no payload",
	          usbBridgeLast().cmd == BridgeCmd::Read && !usbBridgeWriteUnfinished());
	endBridge();

	fresh(escAck);
	openBridge();
	fakeCdcDtr(false);
	run(2);
	checkTrue("a close before any traffic: no line", !usbBridgePortClosed() &&
	          usbBridgeMode() == BridgeMode::Waiting);
	endBridge();

	// Bytes from the ESC count as traffic: an answer 1.8 s late keeps the
	// session past 2000 ms from the PC's last byte.
	fresh(escReply);
	openBridge();
	static uint8_t late[600];
	for (int i = 0; i < 600; i++) late[i] = (uint8_t)i;
	willReply(late, sizeof(late), 1800000);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(2300);
	checkTrue("2300 ms after the PC's frame, the ESC still answering: Passthrough",
	          usbBridgeMode() == BridgeMode::Passthrough && bstats().toHost > 300);
	endBridge();

	// A frame that waits for the slot is no idle time: the line stalls under
	// the frame before it for 2.5 s.
	fresh(escAck);
	openBridge();
	uint8_t big[300];
	for (int i = 0; i < 300; i++) big[i] = (uint8_t)i;
	fakeCdcHostWrite(big, sizeof(big));
	run(10);
	fakeBridgeLineStall(true);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(2500);
	checkTrue("a frame waiting 2.5 s for the slot: still Passthrough",
	          usbBridgeMode() == BridgeMode::Passthrough && bstats().frames == 1);
	fakeBridgeLineStall(false);
	run(300);
	checkTrue("the line moving again: it goes out", bstats().frames == 2);
	endBridge();

	// 0x00 and other first bytes.
	const uint8_t firsts[] = {0x00, 0xFF, 0x2E, 0x7F};
	for (uint8_t f : firsts) {
		fresh(escAck);
		openBridge();
		fakeCdcHostWrite(&f, 1);
		run(5);
		checkTrue("another first byte: passthrough", usbBridgeMode() == BridgeMode::Passthrough);
		endBridge();
	}
}

/** @brief A half-gathered frame at the port's close is dropped. */
static void testCloseMidFrame() {
	section("USB bridge: the port closes while a frame is gathered");
	fresh(escAck);
	openBridge();
	uint8_t big[200];
	for (int i = 0; i < 200; i++) big[i] = (uint8_t)i;
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(big, 128);
	pass();
	pass();
	fakeCdcDtr(false);
	pass();
	run(30);
	checkInt("nothing of it sent", fakeBridgeLineWireCount(), w0);
	checkTrue("Waiting, the claim kept", usbBridgeMode() == BridgeMode::Waiting &&
	          pinClaimState() == ClaimState::Running);
	endBridge();
}

// ---------------------------------------------------------------------------
// write-class frames, BACK, the console
// ---------------------------------------------------------------------------

static void testWriteRules() {
	section("USB bridge: write-class frames, the wait and the unfinished write");
	fresh(escReply);
	openBridge();
	willReply(nullptr, 0, 300);
	checkTrue("at the open: nothing waits, nothing unfinished",
	          !usbBridgeWriteWaiting() && !usbBridgeWriteUnfinished());
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(10);
	checkTrue("SET ADDRESS is not write-class", !usbBridgeWriteWaiting() && !usbBridgeWriteUnfinished());
	fakeCdcHostWrite(PROG, sizeof(PROG));
	for (int k = 0; k < 40 && usbBridgeLast().cmd != BridgeCmd::Program; k++) pass();
	const uint32_t t0 = millis();
	checkTrue("PROGRAM handed over: it waits, unfinished", usbBridgeWriteWaiting() && usbBridgeWriteUnfinished());
	run(5);
	checkTrue("its echo is no answer", usbBridgeWriteWaiting());
	while (millis() - t0 < BRIDGE_WRITE_WAIT_MS - 1) pass();
	checkTrue("a silent ESC: 2999 ms on, it waits", usbBridgeWriteWaiting());
	while (millis() - t0 < BRIDGE_WRITE_WAIT_MS) pass();
	checkTrue("3000 ms: it stops waiting, still unfinished", !usbBridgeWriteWaiting() && usbBridgeWriteUnfinished());

	willReply((const uint8_t *)"\x30", 1, 15000);
	fakeCdcHostWrite(ERASE, sizeof(ERASE));
	for (int k = 0; k < 40 && usbBridgeLast().cmd != BridgeCmd::Erase; k++) pass();
	run(10);
	checkTrue("ERASE: waits while the ESC erases", usbBridgeWriteWaiting());
	run(10);
	checkTrue("its ACK ends the wait", !usbBridgeWriteWaiting() && usbBridgeWriteUnfinished());

	willReply((const uint8_t *)"\x30", 1, 300);
	fakeCdcHostWrite(SET_BUF, sizeof(SET_BUF));
	run(10);
	checkTrue("SET BUFFER: no wait of its own", !usbBridgeWriteWaiting());
	uint8_t payload[10] = {0, 0, 0, 0, 1, 2, 3, 4, 5, 6};
	fakeCdcHostWrite(payload, sizeof(payload));
	for (int k = 0; k < 40 && usbBridgeLast().cmd != BridgeCmd::Payload; k++) pass();
	checkTrue("the payload after it waits, though it starts with 0x00",
	          usbBridgeLast().cmd == BridgeCmd::Payload && usbBridgeWriteWaiting());
	run(20);
	checkTrue("its ACK", !usbBridgeWriteWaiting());
	// A payload whose tail comes 5 ms after its head, 4 ms after its last
	// packet: two frames, the second starting with 0x00, which is no RUN.
	fakeCdcHostWrite(SET_BUF, sizeof(SET_BUF));
	run(10);
	uint8_t split[258];
	for (int i = 0; i < 258; i++) split[i] = (uint8_t)(i + 1);
	split[200] = 0x00;
	split[201] = 0x00;
	const uint32_t f0 = bstats().frames;
	fakeCdcHostWriteGap(split, sizeof(split), 200, 5000);
	run(300);
	checkTrue("a payload split by a 4 ms gap: two frames, the tail named FRAME",
	          bstats().frames == f0 + 2 && usbBridgeLast().cmd == BridgeCmd::Other &&
	          usbBridgeLast().len == 58);
	checkTrue("its 0x00 0x00 no RUN: still unfinished", usbBridgeWriteUnfinished() &&
	          usbBridgeBootloaderReached());
	const uint8_t runBad[] = {0x00, 0x00, 0x11, 0x22};
	fakeCdcHostWrite(runBad, sizeof(runBad));
	run(10);
	checkTrue("00 00 with a wrong CRC: no RUN", usbBridgeWriteUnfinished());
	fakeCdcHostWrite(RUN, sizeof(RUN));
	run(10);
	checkTrue("RUN: the write is finished", usbBridgeLast().cmd == BridgeCmd::Run && !usbBridgeWriteUnfinished() &&
	          !usbBridgeBootloaderReached());
	fakeCdcHostWrite(INIT21, sizeof(INIT21));
	run(20);
	checkTrue("the greeting is no RUN and no write", !usbBridgeWriteUnfinished() && !usbBridgeWriteWaiting());
	fakeCdcHostWrite(PROG, sizeof(PROG));
	run(10);
	fakeCdcDtr(false);
	run(10);
	checkTrue("a port close after a write: still unfinished, the claim kept",
	          usbBridgeWriteUnfinished() && pinClaimState() == ClaimState::Running);
	endBridge();
	checkTrue("after the close: still unfinished until the next open", usbBridgeWriteUnfinished());
	fresh(escAck);
	openBridge();
	checkTrue("the next open clears it", !usbBridgeWriteUnfinished());
	endBridge();
}

/** @brief BACK with the PC connected: nothing more read or sent; the console back at Free. */
static void testBackWithHost() {
	section("USB bridge: BACK with the PC still connected");
	fresh(escReply);
	willReply((const uint8_t *)"\x30", 1, 300);
	openBridge();
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(10);
	uint8_t big[300];
	for (int i = 0; i < 300; i++) big[i] = (uint8_t)(i ^ 0xA5);
	const int wBig = fakeBridgeLineWireCount();
	fakeCdcHostWrite(big, 300);
	run(5);
	const int resumes = fakeResumeCount();
	pinClaimEnd(PinUser::Bridge);
	checkTrue("BACK: Leaving, DShot not yet asked back",
	          pinClaimState() == ClaimState::Leaving && fakeResumeCount() == resumes);
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	// Core0 before core1: the line is still open, the claim already Leaving.
	tud_task();
	usbBridgeTask();
	const int queued = fakeCdcHostQueued();
	checkTrue("core0's pass before core1's: closed, the line still open, nothing read",
	          usbBridgeMode() == BridgeMode::Closed && bridgeWireOpen() &&
	          queued >= (int)sizeof(SET_ADDR));
	pass();
	checkTrue("the next pass: closed, the console still off",
	          usbBridgeMode() == BridgeMode::Closed && !fakeConsoleOn());
	int k = 0;
	while (pinClaimState() != ClaimState::Free && k++ < 2000) pass();
	checkTrue("the frame under way finished, Free", pinClaimState() == ClaimState::Free &&
	          fakeBridgeLineCuts() == 0);
	checkTrue("nothing read from the PC after BACK", fakeCdcHostQueued() == queued);
	checkTrue("no new frame on the wire", wireFrames(w0) <= 1);
	pass();
	checkTrue("the console back at Free", fakeConsoleOn());
	checkTrue("the wire holds the 300-byte frame and nothing after it: no RUN at BACK",
	          wireFrame(wBig, big, 300) && fakeBridgeLineWireCount() == wBig + 300);
	checkInt("DShot never up with the line", g_together, 0);
	checkInt("no PC byte on the wire outside the claim", g_strayBytes, 0);
}

/** @brief The leave page: the console stays off until RESTART DSHOT; BRIDGE reopens. */
static void testLeavePageConsole() {
	section("USB bridge: the console through the leave page");
	fresh(escAck);
	openBridge();
	fakeCdcHostWrite(PROG, sizeof(PROG));
	run(10);
	checkTrue("an unfinished write", usbBridgeWriteUnfinished());
	pinClaimWantLeavePage(PinUser::Bridge);
	pinClaimEnd(PinUser::Bridge);
	run(50);
	checkTrue("the page due, the bridge closed, the console off",
	          pinClaimUnplugDue() && usbBridgeMode() == BridgeMode::Closed && !fakeConsoleOn());
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(20);
	checkTrue("nothing forwarded on the page", fakeBridgeLineWireCount() == w0 &&
	          fakeCdcHostQueued() == (int)sizeof(SET_ADDR));
	pinClaimKeep(PinUser::Bridge);
	for (int k = 0; k < 20 && usbBridgeMode() == BridgeMode::Closed; k++) {
		pass();
		pinClaimPoll();
	}
	checkTrue("BRIDGE: open again, Waiting, the counts at 0",
	          usbBridgeMode() == BridgeMode::Waiting && bstats().frames == 0 && !fakeConsoleOn());
	checkInt("what the PC sent on the page dropped at the open", fakeCdcHostQueued(), 0);
	pinClaimEnd(PinUser::Bridge);
	run(50);
	checkTrue("BACK again: the page, the console off", pinClaimUnplugDue() && !fakeConsoleOn());
	pinClaimRestart(PinUser::Bridge);
	for (int k = 0; k < 20 && pinClaimState() != ClaimState::Free; k++) pass();
	pass();
	checkTrue("RESTART DSHOT: Free, the console on", pinClaimState() == ClaimState::Free && fakeConsoleOn());
	checkInt("DShot never up with the line", g_together, 0);
}

/** @brief The model bootloader's answer to the greeting: AM32's, or an EFM8's with E8B2. */
static bool g_efm8 = false;
static int escGreeting(const uint8_t *f, int n, uint8_t *r, uint32_t *d) {
	*d = 300;
	if (usbBridgeCmdOf(f, (uint16_t)n, false) == BridgeCmd::Init) {
		static const uint8_t AM32[] = {0x34, 0x37, 0x31, 0x00, 0x1F, 0x06, 0x06, 0x01, 0x30};
		static const uint8_t EFM8[] = {0x34, 0x37, 0x31, 0x64, 0xE8, 0xB2, 0x06, 0x01, 0x30};
		memcpy(r, g_efm8 ? EFM8 : AM32, 9);
		return 9;
	}
	if (f[0] == 0x03) {
		for (int i = 0; i < 16; i++) r[i] = (uint8_t)i;
		r[16] = 0x12;
		r[17] = 0x34;
		r[18] = 0x30;
		return 19;
	}
	r[0] = 0x30;
	return 1;
}

/**
 * @brief A session that only greets and reads leaves the bootloader
 *        waiting: the flag of the leave page, and MENU's run cue, until a
 *        RUN, an eRPM frame or usbBridgeRunCueClear().
 */
static void testBootloaderReached() {
	section("USB bridge: a bootloader reached by a read-only session");
	for (int e = 0; e < 2; e++) {
		g_efm8 = e == 1;
		fresh(escGreeting);
		openBridge();
		usbBridgeRunCueClear();
		fakeBridgeLineResponder(nullptr);
		fakeCdcHostWrite(INIT21, sizeof(INIT21));
		run(30);
		checkTrue(g_efm8 ? "EFM8: a greeting nobody answers reaches nothing" :
		                   "AM32: a greeting nobody answers reaches nothing",
		          !usbBridgeBootloaderReached() && !usbBridgeRunCue(0));
		fakeBridgeLineResponder(escGreeting);
		fakeCdcHostWrite(INIT21, sizeof(INIT21));
		run(30);
		checkTrue(g_efm8 ? "EFM8: its answer reaches the bootloader, the cue set" :
		                   "AM32: its answer reaches the bootloader, the cue set",
		          usbBridgeBootloaderReached() && usbBridgeRunCue(0) && !usbBridgeWriteUnfinished());
		const uint8_t rd[] = {0x03, 0x10, 0x00, 0x00};
		fakeCdcHostWrite(rd, sizeof(rd));
		run(30);
		fakeCdcDtr(false);
		run(10);
		checkTrue("a read, the port closed: still reached, the cue kept",
		          usbBridgeBootloaderReached() && usbBridgeRunCue(0));
		endBridge();
		checkTrue("kept after the close", usbBridgeBootloaderReached() && usbBridgeRunCue(0));
	}
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis() - 5000;
	checkTrue("an eRPM frame from before the answer: the cue stays", usbBridgeRunCue(t.lastRpmMs));
	t.lastRpmMs = millis();
	fakeAdvance(1);
	checkTrue("one after it: the cue goes", !usbBridgeRunCue(t.lastRpmMs) && !usbBridgeRunCue(0));

	fresh(escGreeting);
	openBridge();
	fakeCdcHostWrite(INIT21, sizeof(INIT21));
	run(30);
	fakeCdcHostWrite(RUN, sizeof(RUN));
	run(10);
	checkTrue("RUN: nothing reached, no cue", !usbBridgeBootloaderReached() && !usbBridgeRunCue(0));
	fakeCdcHostWrite(INIT21, sizeof(INIT21));
	run(30);
	usbBridgeRunCueClear();
	checkTrue("RESTART DSHOT's clear: no cue, the flag kept", !usbBridgeRunCue(0) && usbBridgeBootloaderReached());
	endBridge();
	g_efm8 = false;
}

/** @brief Console text TinyUSB still holds at the open never reaches the tool. */
static void testStaleConsole() {
	section("USB bridge: console text held at the open is dropped");
	fresh(escAck);
	fakeCdcHostStall(true);
	fakeCdcConsoleText("arm=0 thr=   0 rpm=     0\n");
	openBridge();
	fakeCdcHostStall(false);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(20);
	uint8_t host[64];
	int hn = hostAll(host, sizeof(host));
	checkTrue("the PC's first bytes are the echo", hn == 7 && memcmp(host, SET_ADDR, 6) == 0);
	endBridge();
}

/**
 * @brief The clock's serial line of run-summary.md slice 5, through the real
 *        usbDevTask(): a CLOCK line sets the clock while the console has the
 *        port, and goes to the ESC as data while the bridge has it.
 */
static void testClockLine() {
	section("USB bridge: the clock's serial line beside the bridge");
	fakeI2cReset();
	FakePcf85063 *rtc = fakeI2cAddPcf85063(PCF85063_I2C_ADDR);
	clockInit();
	g_devTask = true;
	fresh(escAck);
	static const char LINE[] = "CLOCK 2026-01-01 00:00:00\n";
	const int len = (int)sizeof(LINE) - 1;
	uint32_t w0 = rtc->writes;
	fakeCdcHostWrite((const uint8_t *)LINE, len);
	run(20);
	static char text[512];
	int n = fakeCdcHostRead((uint8_t *)text, sizeof(text) - 1);
	text[n] = '\0';
	ClockReading c;
	clockNow(&c);
	checkTrue("the console's port: the line sets the clock and is answered",
	          rtc->writes > w0 && c.set && c.t.year == 2026 && strstr(text, "CLOCK SET") != nullptr);

	fakePcf85063Set(rtc, 2030, 6, 15, 12, 0, 0);
	clockInit();
	openBridge();
	const int wire0 = fakeBridgeLineWireCount();
	w0 = rtc->writes;
	fakeCdcHostWrite((const uint8_t *)LINE, len);
	run(30);
	n = fakeCdcHostRead((uint8_t *)text, sizeof(text) - 1);
	text[n] = '\0';
	clockNow(&c);
	checkTrue("the bridge's port: the line goes to the wire as data",
	          wireFrame(wire0, (const uint8_t *)LINE, len));
	checkTrue("and does not set the clock", rtc->writes == w0 && c.t.year == 2030);
	checkTrue("the PC gets the echo and the ESC's ACK, no CLOCK answer",
	          n == len + 1 && strstr(text, "CLOCK SET") == nullptr);
	// Half a line read by nobody, then the bridge gives the port back.
	fakeCdcHostWrite((const uint8_t *)"CLOCK 20", 8);
	run(10);
	endBridge();
	checkTrue("the console back", fakeConsoleOn());
	fakeCdcHostWrite((const uint8_t *)LINE, len);
	run(20);
	n = fakeCdcHostRead((uint8_t *)text, sizeof(text) - 1);
	text[n] = '\0';
	clockNow(&c);
	checkTrue("then a line sets the clock again", c.t.year == 2026 && strstr(text, "CLOCK SET") != nullptr);
	g_devTask = false;
	fakeI2cReset();
}

/** @brief A refused bind never opens the bridge nor takes the console. */
static void testRefusedBind() {
	section("USB bridge: a refused bind, a claim of another screen");
	fresh(escAck);
	fakeBridgeLineRefuse(CORE1_JOB_PIO);
	pinClaimBegin(PinUser::Bridge, PIN_SIG);
	for (int k = 0; k < 20; k++) {
		pass();
		pinClaimPoll();
	}
	checkTrue("Held with the error: closed, the console on",
	          pinClaimState() == ClaimState::Held && usbBridgeMode() == BridgeMode::Closed && fakeConsoleOn());
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(10);
	checkInt("the PC's bytes not read", fakeCdcHostQueued(), (long)sizeof(SET_ADDR));
	pinClaimEnd(PinUser::Bridge);
	run(5);
	fakeBridgeLineRefuse(0);
	checkTrue("BACK: Free, the console never switched", pinClaimState() == ClaimState::Free &&
	          fakeConsoleSwitches() == 0);

	// Another screen's claim: the bridge's side does nothing.
	pinClaimBegin(PinUser::Sbus, PIN_SIG);
	for (int k = 0; k < 20; k++) {
		pass();
		pinClaimPoll();
	}
	run(10);
	checkTrue("the S.BUS monitor's claim: closed, nothing read",
	          usbBridgeMode() == BridgeMode::Closed && fakeConsoleOn() &&
	          fakeCdcHostQueued() == (int)sizeof(SET_ADDR));
	pinClaimEnd(PinUser::Sbus);
	for (int k = 0; k < 100 && pinClaimState() != ClaimState::Free; k++) {
		pass();
		pinClaimRestart(PinUser::Sbus);
	}
	checkInt("no PC byte on the wire outside the claim", g_strayBytes, 0);
}

/** @brief The ESC drives while the bench does: a collision, counted; nothing else changes. */
static void testCollision() {
	section("USB bridge: a collision is counted, not taken for corruption");
	fresh(escAck);
	openBridge();
	fakeBridgeLineCorruptEcho(2, 0x00);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(20);
	uint8_t host[32];
	int hn = hostAll(host, sizeof(host));
	UsbBridgeStats s = bstats();
	checkTrue("one collision, nothing lost", s.collisions == 1 && s.lost == 0);
	checkTrue("the PC gets the wire as it read: echo with the byte driven over, then the ACK",
	          hn == 7 && host[2] == 0x00 && host[1] == 0x00 && host[3] == 0x00 && host[6] == 0x30);
	checkTrue("the session goes on", usbBridgeMode() == BridgeMode::Passthrough);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(20);
	hn = hostAll(host, sizeof(host));
	checkTrue("the next frame clean", hn == 7 && memcmp(host, SET_ADDR, 6) == 0 &&
	          bstats().collisions == 1 && bstats().frames == 2);

	// The PC sends while the bench still sends its last frame: the frame waits
	// in the gather, whole, and goes out right behind it, where the ESC's ACK
	// of the first meets it on the wire: a collision, counted.
	uint8_t big[300];
	for (int i = 0; i < 300; i++) big[i] = (uint8_t)(i * 9);
	const int w0 = fakeBridgeLineWireCount();
	fakeCdcHostWrite(big, 300);
	run(10);
	fakeCdcHostWrite(SET_ADDR, sizeof(SET_ADDR));
	run(250);
	checkTrue("a frame sent while the bench drives waits and goes out whole after it",
	          wireFrame(w0, big, 300) && wireFrame(w0 + 300, SET_ADDR, 6) && wireFrames(w0) == 2);
	s = bstats();
	checkTrue("the ESC's ACK under it: a collision, nothing lost, the session on",
	          s.collisions == 2 && s.lost == 0 && s.frames == 4 && usbBridgeMode() == BridgeMode::Passthrough);
	hn = hostAll(host, sizeof(host));
	checkInt("every byte the wire read reached the PC", (long)s.toHost, (long)fakeCdcHostTotal());
	endBridge();
}

/** @brief A PC that stops reading: the ring fills and LOST counts. */
static void testLost() {
	section("USB bridge: the PC stops reading, LOST");
	fresh(escReply);
	openBridge();
	static uint8_t lots[1100];
	for (int i = 0; i < 1100; i++) lots[i] = (uint8_t)i;
	willReply(lots, sizeof(lots), 300);
	fakeCdcHostStall(true);
	const uint8_t cmd[] = {0x03, 0x00, 0, 0};
	fakeCdcHostWrite(cmd, sizeof(cmd));
	run(700);
	checkInt("1104 bytes: the PC's buffer and the ring hold them", (long)bstats().lost, 0);
	fakeCdcHostWrite(cmd, sizeof(cmd));
	run(700);
	UsbBridgeStats s = bstats();
	checkTrue("LOST above 0 once the ring is full", s.lost > 0);
	fakeCdcHostStall(false);
	run(20);
	checkTrue("the PC reading again: the bridge goes on", usbBridgeMode() == BridgeMode::Passthrough);
	endBridge();
}

void runUsbBridgeTests() {
	FakeWiring w = fakeWiring();
	uiInit();
	g_strayBytes = 0;
	g_together = 0;
	testNaming();
	testIntact();
	testFraming();
	testNoStall();
	testLateRepaint();
	testLateWhole();
	testToolAckRule();
	testHousekeeping();
	testDirect();
	testGather();
	testModes();
	testCloseMidFrame();
	testWriteRules();
	testBackWithHost();
	testLeavePageConsole();
	testRefusedBind();
	testBootloaderReached();
	testStaleConsole();
	testClockLine();
	testCollision();
	testLost();
	checkInt("over every section: no PC byte outside the claim", g_strayBytes, 0);
	checkInt("over every section: DShot never with the line", g_together, 0);
	fresh(nullptr);
	fakeBridgeLineReset();
	fakeCdcReset();
	escTaskConfigure(w.dshotPin, w.dshotKbaud, w.kissEnable, w.kissPin);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
}
