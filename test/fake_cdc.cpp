/**
 * @file fake_cdc.cpp
 * @brief TinyUSB's CDC calls and the console switch, against a PC model.
 *        @see fake_cdc.h
 */

#include "fake_cdc.h"
#include "tusb.h"

#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/unique_id.h>

#include <string.h>

/** @brief The PC's packets not yet in the board's buffer, as lengths, and their bytes. */
static uint8_t  g_hostQ[65536];
static int      g_hostQLen = 0;
static uint16_t g_pkt[4096];
static uint64_t g_pktAt[4096];   /**< time_us_64() from which each packet may move in. */
static int      g_pktN = 0;

static uint8_t  g_rx[CFG_TUD_CDC_RX_BUFSIZE];
static int      g_rxLen = 0;
static int      g_rxPos = 0;

static uint8_t  g_tx[CFG_TUD_CDC_TX_BUFSIZE];
static int      g_txLen = 0;

static uint8_t  g_host[65536];
static int      g_hostLen = 0;
static long     g_hostTotal = 0;
static bool     g_stall = false;
static bool     g_dtr = false;

static bool     g_console = true;
static int      g_switches = 0;
static long     g_consoleBackAt = 0;

void fakeCdcReset() {
	g_hostQLen = g_pktN = 0;
	g_rxLen = g_rxPos = 0;
	g_txLen = 0;
	g_hostLen = 0;
	g_hostTotal = 0;
	g_stall = false;
	g_dtr = false;
	g_console = true;
	g_switches = 0;
	g_consoleBackAt = 0;
}

void fakeCdcDtr(bool on) { g_dtr = on; }

/** @brief Queue @p n bytes as packets that may move in from @p atUs on. */
static void queue(const uint8_t *d, int n, uint64_t atUs) {
	while (n > 0) {
		int p = n > 64 ? 64 : n;
		if (g_hostQLen + p > (int)sizeof(g_hostQ) || g_pktN >= (int)(sizeof(g_pkt) / sizeof(g_pkt[0])))
			return;
		memcpy(g_hostQ + g_hostQLen, d, p);
		g_hostQLen += p;
		g_pktAt[g_pktN] = atUs;
		g_pkt[g_pktN++] = (uint16_t)p;
		d += p;
		n -= p;
	}
}

void fakeCdcHostWrite(const uint8_t *d, int n) { queue(d, n, 0); }

void fakeCdcHostWriteGap(const uint8_t *d, int n, int split, uint32_t gapUs) {
	queue(d, split, 0);
	queue(d + split, n - split, time_us_64() + gapUs);
}

/** @brief The board's buffer empty: the next packet moves in. Once per tud_task(). */
static void takePacket() {
	if (g_rxPos < g_rxLen || g_pktN == 0 || time_us_64() < g_pktAt[0]) return;
	int p = g_pkt[0];
	memcpy(g_rx, g_hostQ, p);
	g_rxLen = p;
	g_rxPos = 0;
	memmove(g_hostQ, g_hostQ + p, g_hostQLen - p);
	g_hostQLen -= p;
	memmove(g_pkt, g_pkt + 1, (g_pktN - 1) * sizeof(g_pkt[0]));
	memmove(g_pktAt, g_pktAt + 1, (g_pktN - 1) * sizeof(g_pktAt[0]));
	g_pktN--;
}

int fakeCdcHostQueued() { return g_hostQLen + (g_rxLen - g_rxPos); }

int fakeCdcHostRead(uint8_t *buf, int max) {
	int n = g_hostLen < max ? g_hostLen : max;
	if (buf) memcpy(buf, g_host, n);
	memmove(g_host, g_host + n, g_hostLen - n);
	g_hostLen -= n;
	return n;
}

int fakeCdcHostPending() { return g_hostLen; }

long fakeCdcHostTotal() { return g_hostTotal; }

/** @brief Move the transmit buffer to the PC, unless it stalls. */
static void deliver() {
	if (g_stall || g_txLen == 0) return;
	int room = (int)sizeof(g_host) - g_hostLen;
	int n = g_txLen < room ? g_txLen : room;
	memcpy(g_host + g_hostLen, g_tx, n);
	g_hostLen += n;
	g_hostTotal += n;
	memmove(g_tx, g_tx + n, g_txLen - n);
	g_txLen -= n;
}

void fakeCdcHostStall(bool on) {
	g_stall = on;
	if (!on) deliver();
}

bool fakeConsoleOn() { return g_console; }
int fakeConsoleSwitches() { return g_switches; }
long fakeConsoleBackAt() { return g_consoleBackAt; }

// --- TinyUSB ---

bool tud_cdc_connected(void) { return g_dtr; }

void tud_task(void) { takePacket(); }

uint32_t tud_cdc_available(void) {
	return (uint32_t)(g_rxLen - g_rxPos);
}

uint32_t tud_cdc_read(void *buffer, uint32_t bufsize) {
	uint32_t n = (uint32_t)(g_rxLen - g_rxPos);
	if (n > bufsize) n = bufsize;
	memcpy(buffer, g_rx + g_rxPos, n);
	g_rxPos += (int)n;
	return n;
}

uint32_t tud_cdc_write_available(void) { return (uint32_t)((int)sizeof(g_tx) - g_txLen); }

uint32_t tud_cdc_write(const void *buffer, uint32_t bufsize) {
	uint32_t room = tud_cdc_write_available();
	uint32_t n = bufsize < room ? bufsize : room;
	memcpy(g_tx + g_txLen, buffer, n);
	g_txLen += (int)n;
	return n;
}

bool tud_cdc_write_clear(void) {
	g_txLen = 0;
	return true;
}

uint32_t tud_cdc_write_flush(void) {
	int before = g_txLen;
	deliver();
	return (uint32_t)(before - g_txLen);
}

bool tusb_init(void) { return true; }

// --- the SDK calls of usb_dev.cpp ---

void stdio_set_driver_enabled(stdio_driver_t *, bool enabled) {
	if (enabled == g_console) return;
	g_console = enabled;
	g_switches++;
	if (enabled) g_consoleBackAt = g_hostTotal;
}

void pico_get_unique_board_id_string(char *id_out, unsigned int len) {
	if (len) id_out[0] = '\0';
}

extern "C" void usbDescSetSerial(const char *) {}

void fakeCdcConsoleText(const char *s) {
	tud_cdc_write(s, (uint32_t)strlen(s));
}
