/**
 * @file test_bridge_log.cpp
 * @brief The USB bridge's traffic log with its build option on
 *        (`USB_BRIDGE_LOG` 1, docs/design/usb-bridge.md decision 8): the
 *        ring, the dump's every line, a PC that stops reading, and the dump
 *        before the console in usb_bridge.cpp.
 *
 * A program of its own, as test_usb_msc_cb.cpp is: the suite builds the
 * firmware's default, the log off. This one builds bridge_log.cpp and
 * usb_bridge.cpp with it on, against test/fake_cdc.cpp, a clock of its own,
 * a claim that is a variable and a wire that is a loopback. It ports
 * DshotDisplay's test_dump.cpp, which counts the lines that reach the PC,
 * not the code that writes them. It passes by its exit status.
 */

#include "bridge_log.h"
#include "usb_bridge.h"
#include "bridge_wire.h"
#include "pin_claim.h"
#include "fake_cdc.h"
#include "tusb.h"

#include <stdio.h>
#include <string.h>

static_assert(USB_BRIDGE_LOG == 1, "build this program with -DUSB_BRIDGE_LOG=1");

static int g_failed = 0;

static void check(const char *what, bool ok) {
	printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
	if (!ok) g_failed++;
}

// --- the clock ---
static uint64_t g_us = 1000000;
uint64_t time_us_64() { return g_us; }
uint32_t time_us_32() { return (uint32_t)g_us; }
void sleep_ms(uint32_t ms) { g_us += 1000u * ms; }
void sleep_us(uint64_t us) { g_us += us; }

// --- the claim: Running for the bridge, or Free ---
static bool g_running = false;
static bool g_held = false;
ClaimState pinClaimState() {
	return g_running ? ClaimState::Running : g_held ? ClaimState::Leaving : ClaimState::Free;
}
PinUser pinClaimHolder() { return g_held ? PinUser::Bridge : PinUser::None; }

// --- the wire: a loopback, each frame echoed whole, then the ACK ---
static uint8_t  g_ring[2048];
static int      g_ringLen = 0;
bool bridgeWireOpen() { return g_running; }
bool bridgeWireSend(const uint8_t *d, uint16_t n) {
	if (!g_running) return false;
	memcpy(g_ring + g_ringLen, d, n);
	g_ringLen += n;
	g_ring[g_ringLen++] = 0x30;
	return true;
}
uint16_t bridgeWireRecv(uint8_t *buf, uint16_t max) {
	int n = g_ringLen < max ? g_ringLen : max;
	memcpy(buf, g_ring, n);
	memmove(g_ring, g_ring + n, g_ringLen - n);
	g_ringLen -= n;
	return (uint16_t)n;
}
void bridgeWireKeepEcho(bool) {}
void bridgeWireStats(BridgeWireStats *out) { memset(out, 0, sizeof(*out)); }

/** @brief Newlines among the bytes that reached the PC since the last call. */
static int linesDelivered() {
	static uint8_t buf[65536];
	int n = fakeCdcHostRead(buf, sizeof(buf));
	int lines = 0;
	for (int i = 0; i < n; i++) lines += buf[i] == '\n';
	return lines;
}

/** @brief Steps of the dump, 1 ms apart, until it is done; how many. */
static int dumpAll(int max = 10000) {
	int n = 0;
	while (n < max) {
		n++;
		if (bridgeLogDumpStep()) break;
		g_us += 1000;
	}
	return n;
}

static const uint8_t INIT21[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
static const uint8_t INFO[9] = {0x34, 0x37, 0x31, 0x02, 0x35, 0x06, 0x06, 0x02, 0x30};

static void testDump() {
	printf("\nBridge log: the dump is complete (DshotDisplay's test_dump.cpp)\n");
	fakeCdcReset();
	check("no dump begun: a step is done at once", bridgeLogDumpStep());
	bridgeLogReset();
	bridgeLogNote("bridge opened");
	for (int i = 0; i < 6; i++) {
		g_us += 500000;
		bridgeLogAdd(BLOG_HOST_TO_ESC, INIT21, sizeof(INIT21));
		bridgeLogAdd(BLOG_ESC_TO_HOST, INFO, sizeof(INFO));
	}
	bridgeLogNote("bridge closed");
	check("14 records held", bridgeLogCount() == 14 && !bridgeLogOverflowed());
	bridgeLogDumpBegin();
	int steps = dumpAll();
	check("more than one step: the 256-byte buffer paces it", steps > 1);
	check("every record reaches the PC: 14 lines and 5 of frame", linesDelivered() == 14 + 5);

	fakeCdcReset();
	bridgeLogReset();
	for (int i = 0; i < 400; i++) {
		g_us += 5000;
		bridgeLogAdd(BLOG_HOST_TO_ESC, INIT21, sizeof(INIT21));
	}
	uint16_t held = bridgeLogCount();
	check("a long session: the oldest dropped", bridgeLogOverflowed() && held > 100 && held < 400);
	check("4096 bytes of 28-byte records: 146 held", held == 4096 / 28);
	bridgeLogDumpBegin();
	dumpAll();
	check("the long dump complete too", linesDelivered() == held + 5);

	// The text: the title, a long transfer's count, the gap since the last.
	fakeCdcReset();
	bridgeLogReset();
	uint8_t big[100];
	for (int i = 0; i < 100; i++) big[i] = (uint8_t)i;
	bridgeLogAdd(BLOG_HOST_TO_ESC, big, sizeof(big));
	g_us += 7000;
	bridgeLogAdd(BLOG_ESC_TO_HOST, INFO, 1);
	bridgeLogDumpBegin();
	dumpAll();
	static char text[4096];
	int n = fakeCdcHostRead((uint8_t *)text, sizeof(text) - 1);
	text[n] = '\0';
	check("the title", strstr(text, "\n--- USB BRIDGE LOG: 2 record(s) ---\n") == text);
	check("64 of 100 bytes kept and said so", strstr(text, "H>E  100  00 01 02 ") != nullptr &&
	      strstr(text, "3F ...(64 of 100 shown)\n") != nullptr);
	check("the gap since the record before", strstr(text, "E>H    1  34   (+7ms)\n") != nullptr);
	check("the end", strstr(text, "--- END ---\n\n") != nullptr);
}

/** @brief A slow PC: a dump longer than 400 ms that keeps going is not abandoned. */
static void testSlowPc() {
	printf("\nBridge log: a slow PC, and a transfer longer than 255 bytes\n");
	fakeCdcReset();
	bridgeLogReset();
	for (int i = 0; i < 100; i++) bridgeLogAdd(BLOG_HOST_TO_ESC, INIT21, sizeof(INIT21));
	bridgeLogDumpBegin();
	int steps = 0;
	while (steps < 1000) {
		steps++;
		if (bridgeLogDumpStep()) break;
		g_us += 50000;
	}
	check("steps 50 ms apart, more than 400 ms in all: every line out",
	      steps > 9 && linesDelivered() == 100 + 5);
	fakeCdcReset();
	bridgeLogReset();
	static uint8_t big[300];
	bridgeLogAdd(BLOG_HOST_TO_ESC, big, sizeof(big));
	bridgeLogDumpBegin();
	dumpAll();
	static char text[2048];
	int n = fakeCdcHostRead((uint8_t *)text, sizeof(text) - 1);
	text[n] = '\0';
	check("300 bytes: counted as 255, the field's top, 64 shown",
	      strstr(text, "H>E  255  00 ") != nullptr && strstr(text, "...(64 of 255 shown)") != nullptr);
}

static void testStall() {
	printf("\nBridge log: a PC that stops reading\n");
	fakeCdcReset();
	bridgeLogReset();
	for (int i = 0; i < 60; i++) bridgeLogAdd(BLOG_HOST_TO_ESC, INIT21, sizeof(INIT21));
	fakeCdcHostStall(true);
	bridgeLogDumpBegin();
	check("the first step fills the buffer and is not done", !bridgeLogDumpStep());
	g_us += (BRIDGE_LOG_STALL_MS - 1) * 1000ull;
	check("399 ms with nothing taken: still going", !bridgeLogDumpStep());
	g_us += 1000;
	check("400 ms: abandoned", bridgeLogDumpStep());
	check("and stays done", bridgeLogDumpStep());
	fakeCdcHostStall(false);
	check("what was in the buffer reaches the PC, no more", fakeCdcHostTotal() == CFG_TUD_CDC_TX_BUFSIZE);
}

static void testBridgeOrder() {
	printf("\nBridge log: the dump before the console, in usb_bridge.cpp\n");
	fakeCdcReset();
	fakeCdcDtr(true);
	g_running = g_held = true;
	usbBridgeTask();
	check("open: the console off", !fakeConsoleOn() && usbBridgeMode() == BridgeMode::Waiting);
	fakeCdcHostWrite(INIT21, sizeof(INIT21));
	tud_task();
	usbBridgeTask();
	g_us += 3000;
	usbBridgeTask();
	usbBridgeTask();
	check("the greeting forwarded, its echo and ACK back", fakeCdcHostTotal() == 22);
	check("recorded: the open, the frame, the bytes back", bridgeLogCount() == 3);
	fakeCdcHostRead(nullptr, 1000);
	g_running = false;   // BACK: Leaving
	usbBridgeTask();
	for (int i = 0; i < 50; i++) {
		g_us += 1000;
		usbBridgeTask();
	}
	check("the dump out while the claim is Leaving, the console still off",
	      !fakeConsoleOn() && linesDelivered() == 4 + 5);
	long dumped = fakeCdcHostTotal();
	g_held = false;      // Free
	usbBridgeTask();
	check("Free: the console back after the dump's last byte",
	      fakeConsoleOn() && fakeConsoleBackAt() == dumped);

	// Free while the dump still runs: the console waits for its end.
	fakeCdcReset();
	g_running = g_held = true;
	usbBridgeTask();
	for (int i = 0; i < 40; i++) {
		fakeCdcHostWrite(INIT21, sizeof(INIT21));
		tud_task();
		usbBridgeTask();
		g_us += 3000;
		usbBridgeTask();
		usbBridgeTask();
	}
	fakeCdcHostRead(nullptr, 65536);
	g_running = g_held = false;
	usbBridgeTask();
	check("Free at once: the dump first, the console off", !fakeConsoleOn());
	int k = 0;
	while (!fakeConsoleOn() && k++ < 1000) usbBridgeTask();
	check("the console back once the dump is out", fakeConsoleOn() && linesDelivered() > 80 &&
	      fakeConsoleBackAt() == fakeCdcHostTotal());
}

int main() {
	testDump();
	testSlowPc();
	testStall();
	testBridgeOrder();
	printf("test_bridge_log: %s\n", g_failed ? "FAILED" : "all checks passed");
	return g_failed ? 1 : 0;
}
