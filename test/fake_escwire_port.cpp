/**
 * @file fake_escwire_port.cpp
 * @brief rcbench's simulated ESC behind escwire_port.h. @see fake_escwire_port.h
 */

#include "fake_escwire_port.h"
#include "escwire_port.h"

#include <pico/time.h>
#include <string.h>

static sim_t   g_sim;
static uint8_t g_refuse = 0;
static int     g_opens = 0;
static int     g_closes = 0;
static uint8_t g_gpio = 0xFF;
static bool    g_open = false;
static uint32_t g_closedUs = 0;

sim_t *fakeEwSim() { return &g_sim; }

void fakeEwReset(uint8_t sig, bool bluejay) {
	sim_init_efm8(&g_sim, sig, bluejay);
	g_refuse = 0;
	g_opens = g_closes = 0;
	g_gpio = 0xFF;
	g_open = false;
	g_closedUs = 0;
}

void fakeEwRefuse(uint8_t err) { g_refuse = err; }
int fakeEwOpens() { return g_opens; }
int fakeEwCloses() { return g_closes; }
uint8_t fakeEwGpio() { return g_gpio; }
bool fakeEwOpen() { return g_open; }
uint32_t fakeEwClosedUs() { return g_closedUs; }

bool fakeEwSentLeave(int index) {
	uint8_t f[EW_A_SHORT_FRAME];
	size_t n = ew_a_short(f, EW_A_CMD_LEAVE, 1u);
	return index >= 0 && index < (int)SIM_LOG_MAX && g_sim.log[index].n == n &&
	       memcmp(g_sim.log[index].b, f, n) == 0;
}

// The driver: each call sets the simulator's clock to the virtual one first.
static void sync() { g_sim.now = time_us_32(); }
static bool fStart(void *ctx, const ew_xfer_t *x) { sync(); return sim_start(ctx, x); }
static bool fPoll(void *ctx, ew_capture_t *out) { sync(); return sim_poll(ctx, out); }
static void fAbort(void *ctx) { sync(); sim_abort(ctx); }
static void fLine(void *ctx, ew_line_t level) { sync(); sim_line(ctx, level); }
static uint32_t fNow(void *) { return time_us_32(); }

uint8_t escwirePortOpen(uint8_t gpio, ew_driver_t *out) {
	if (g_refuse) return g_refuse;
	g_opens++;
	g_gpio = gpio;
	g_open = true;
	// The open parks the pad, as ew_line_open() does.
	g_sim.line = EW_LINE_PARK;
	*out = ew_driver_t{ &g_sim, fStart, fPoll, fAbort, fLine, fNow };
	return 0;
}

void escwirePortClose() {
	if (!g_open) return;
	g_sim.running = false;
	g_open = false;
	g_closes++;
	g_closedUs = time_us_32();
	g_sim.line = EW_LINE_UP;
}
