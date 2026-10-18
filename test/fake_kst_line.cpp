/**
 * @file fake_kst_line.cpp
 * @brief The host suite's KST line. @see fake_kst_line.h
 */

#include "fake_kst_line.h"
#include "kst_line_port.h"

#include <pico/time.h>

static sim_t    g_sim;
static bool     g_open = false;
static uint8_t  g_refuse = 0;
static int      g_opens = 0;
static int      g_closes = 0;
static uint8_t  g_pin = 0xFF;
static uint32_t g_closedUs = 0;

sim_t *fakeKstSim() { return &g_sim; }

void fakeKstLineReset() {
	sim_init(&g_sim);
	g_open = false;
	g_refuse = 0;
	g_opens = 0;
	g_closes = 0;
	g_pin = 0xFF;
	g_closedUs = 0;
}

void fakeKstLineRefuse(uint8_t err) { g_refuse = err; }
int fakeKstLineOpens() { return g_opens; }
int fakeKstLineCloses() { return g_closes; }
uint8_t fakeKstLinePin() { return g_pin; }
uint32_t fakeKstLineClosedUs() { return g_closedUs; }

uint8_t kstLineOpen(uint8_t gpio) {
	if (g_refuse) return g_refuse;
	g_open = true;
	g_opens++;
	g_pin = gpio;
	return 0;
}

void kstLineClose() {
	if (!g_open) return;
	g_open = false;
	g_closes++;
	g_closedUs = time_us_32();
}

bool kstLineIsOpen() { return g_open; }

static bool fakeStart(void *, const kst_frame_t *frame, uint32_t windowNs) {
	g_sim.now_us = time_us_32();
	return sim_start(&g_sim, frame, windowNs);
}

static bool fakePoll(void *, kst_capture_t *out) {
	g_sim.now_us = time_us_32();
	return sim_poll(&g_sim, out);
}

static uint32_t fakeNow(void *) { return time_us_32(); }

kst_driver_t kstLineDriver() {
	kst_driver_t d = { &g_sim, fakeStart, fakePoll, fakeNow };
	return d;
}
