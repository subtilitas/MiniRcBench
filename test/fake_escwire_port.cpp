/**
 * @file fake_escwire_port.cpp
 * @brief rcbench's simulated ESC behind escwire_port.h. @see fake_escwire_port.h
 */

#include "fake_escwire_port.h"
#include "escwire_port.h"

#include <hardware/gpio.h>
#include <pico/time.h>
#include <string.h>

static sim_t   g_sim;
static uint8_t g_refuse = 0;
static int     g_opens = 0;
static int     g_closes = 0;
static uint8_t g_gpio = 0xFF;
static bool    g_open = false;
static uint32_t g_closedUs = 0;
static bool    g_answerEach = false;
/** @brief The flash fakeEwOnlySettingsPage() compares with. */
static uint8_t g_flash[SIM_FLASH];

sim_t *fakeEwSim() { return &g_sim; }

/** @brief The counts and the line as a reset leaves them. */
static void resetPort() {
	g_refuse = 0;
	g_opens = g_closes = 0;
	g_gpio = 0xFF;
	g_open = false;
	g_closedUs = 0;
	g_answerEach = false;
}

void fakeEwResetAm32() {
	sim_init_am(&g_sim, 21u, 1u);
	resetPort();
}

void fakeEwAnswerEach(bool on) { g_answerEach = on; }

void fakeEwReset(uint8_t sig, bool bluejay) {
	sim_init_efm8(&g_sim, sig, bluejay);
	// Every byte outside the settings page, the firmware's and the
	// bootloader's among them, holds a pattern with no 0xFF in it, so an
	// erase or a program there changes bytes the snapshot sees.
	for (uint32_t a = 0; a < SIM_FLASH; a++)
		if (a < g_sim.base || a >= (uint32_t)g_sim.base + g_sim.page)
			g_sim.flash[a] = (uint8_t)((a * 7u + 3u) % 255u);
	fakeEwSnapshot();
	resetPort();
}

void fakeEwRefuse(uint8_t err) { g_refuse = err; }
int fakeEwOpens() { return g_opens; }
int fakeEwCloses() { return g_closes; }
uint8_t fakeEwGpio() { return g_gpio; }
bool fakeEwOpen() { return g_open; }
uint32_t fakeEwClosedUs() { return g_closedUs; }

void fakeEwSnapshot() { memcpy(g_flash, g_sim.flash, sizeof(g_flash)); }

bool fakeEwOnlySettingsPage() {
	const uint32_t from = g_sim.base, to = (uint32_t)g_sim.base + g_sim.page;
	for (uint32_t a = 0; a < SIM_FLASH; a++)
		if ((a < from || a >= to) && g_sim.flash[a] != g_flash[a]) return false;
	// The frames themselves: every one must be in the log to be judged.
	if (g_sim.starts > (int)SIM_LOG_MAX) return false;
	// An erase or a program on the address the frame before set: a read
	// leaves the address in doubt until the next address frame.
	int32_t addr = -1;
	for (int i = 0; i < g_sim.starts; i++) {
		const sim_log_t &l = g_sim.log[i];
		if (l.n == 0) continue;
		if (l.b[0] == EW_A_CMD_ADDRESS && l.n >= 4) addr = (l.b[2] << 8) | l.b[3];
		else if (l.b[0] == EW_A_CMD_READ) addr = -1;
		else if ((l.b[0] == EW_A_CMD_ERASE || l.b[0] == EW_A_CMD_PROGRAM) && l.n == EW_A_SHORT_FRAME &&
		         addr != (int32_t)g_sim.base)
			return false;
	}
	return true;
}

bool fakeEwSentLeave(int index) {
	uint8_t f[EW_A_SHORT_FRAME];
	size_t n = ew_a_short(f, EW_A_CMD_LEAVE, 1u);
	return index >= 0 && index < (int)SIM_LOG_MAX && g_sim.log[index].n == n &&
	       memcmp(g_sim.log[index].b, f, n) == 0;
}

// The pad, as ew_line.c sets it. EW_LINE_PARK: the pull-down, the input off.
static void padPark() {
	gpio_pull_down(g_gpio);
	gpio_set_input_enabled(g_gpio, false);
}
// A reply, the rest after a transaction, EW_LINE_UP: the pull-up, the input on.
static void padListen() {
	gpio_pull_up(g_gpio);
	gpio_set_input_enabled(g_gpio, true);
}

// The driver: each call sets the simulator's clock to the virtual one first.
static void sync() { g_sim.now = time_us_32(); }
static bool fStart(void *ctx, const ew_xfer_t *x) {
	sync();
	if (g_answerEach && x->n_tx == EW_A_CONTACT_LEN) {
		uint8_t c[EW_A_CONTACT_LEN];
		(void)ew_a_contact(c);
		if (memcmp(x->tx, c, sizeof(c)) == 0) g_sim.contacted = false;
	}
	if (!sim_start(ctx, x)) return false;
	padListen();
	return true;
}
static bool fPoll(void *ctx, ew_capture_t *out) {
	sync();
	if (!sim_poll(ctx, out)) return false;
	if (out->aborted || out->overflow) padPark();
	return true;
}
static void fAbort(void *ctx) { sync(); sim_abort(ctx); padPark(); }
static void fLine(void *ctx, ew_line_t level) {
	sync();
	sim_line(ctx, level);
	if (level == EW_LINE_UP || level == EW_LINE_HIGH) padListen();
	else padPark();
}
static uint32_t fNow(void *) { return time_us_32(); }

uint8_t escwirePortOpen(uint8_t gpio, ew_driver_t *out) {
	if (g_refuse) return g_refuse;
	g_opens++;
	g_gpio = gpio;
	g_open = true;
	// The open parks the pad, as ew_line_open() does: the pull first, then
	// the pad to the state machine, its input off.
	g_sim.line = EW_LINE_PARK;
	gpio_pull_down(gpio);
	gpio_set_function(gpio, GPIO_FUNC_PIO2);
	gpio_set_input_enabled(gpio, false);
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
	// ew_line_close(): the pull-down first, then gpio_init(), an SIO input,
	// low; then escwire_port.cpp's pull-up.
	gpio_pull_down(g_gpio);
	gpio_set_dir(g_gpio, GPIO_IN);
	gpio_put(g_gpio, 0);
	gpio_set_function(g_gpio, GPIO_FUNC_SIO);
	gpio_pull_up(g_gpio);
}
