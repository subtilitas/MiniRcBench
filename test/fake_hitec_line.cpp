/**
 * @file fake_hitec_line.cpp
 * @brief The host suite's Hitec line. @see fake_hitec_line.h
 *
 * Adapted from rcbench test/host/hitec_session_rig.h, commit 3e29dc1b, MIT:
 * the servo's answers.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 */

#include "fake_hitec_line.h"
#include "hitec_line.h"
#include "hitec_line_port.h"

#include "fakes.h"
// rcbench's fixture brings its test harness, whose reporting is not used here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "rcbench/hitec_reg_fix.h"
#pragma GCC diagnostic pop

#include <pico/time.h>
#include <string.h>

static HitecSim        g_sim;
static bool            g_open = false;
static uint8_t         g_refuse = 0;
static int             g_opens = 0;
static int             g_closes = 0;
static uint8_t         g_family = 0xFF;
static uint32_t        g_closedUs = 0;
static unsigned        g_starts = 0;
static unsigned        g_aborts = 0;
static bool            g_busy = false;     /**< A transaction runs. */
static bool            g_cut = false;      /**< hitecLineAbort() cut it. */
static hitec_xfer_t    g_xfer;             /**< The last transaction started. */
static uint32_t        g_startUs = 0;      /**< When it started. */
static uint32_t        g_doneUs = 0;       /**< When its capture is complete. */
static hitec_capture_t g_cap;              /**< Its capture. */

/** @brief 9 classic bit times, ns, as rcbench's rig. */
static const uint32_t C_ARM = 468750u;
/** @brief One D series bit, ns. */
static const uint32_t D_BIT = 8681u;

HitecSim *fakeHitecSim() { return &g_sim; }

void fakeHitecLineReset(uint8_t family) {
	memset(&g_sim, 0, sizeof(g_sim));
	g_sim.family = family;
	hitec_image_t img;
	if (family == HITEC_CLASSIC) {
		hs5645(&img);
		g_sim.version = img.version;
	} else {
		d485(&img);
	}
	memcpy(g_sim.mem, img.mem, sizeof(g_sim.mem));
	g_sim.plusMs = 160;
	g_sim.bootMs = 800;
	g_sim.minOffUs = UINT32_MAX;
	g_sim.ignoreAddr = -1;
	g_sim.dieAfterWrites = -1;
	g_open = false;
	g_refuse = 0;
	g_opens = 0;
	g_closes = 0;
	g_family = 0xFF;
	g_closedUs = 0;
	g_starts = 0;
	g_aborts = 0;
	g_busy = false;
	g_cut = false;
}

void fakeHitecSimUpdate(bool level, uint32_t nowUs) {
	HitecSim *s = &g_sim;
	if (level == s->level) return;
	s->level = level;
	if (level) {
		uint32_t off = nowUs - s->lastOffUs;
		if (s->switchOns && off < s->minOffUs) s->minOffUs = off;
		s->switchOns++;
		s->onUs = nowUs;
	} else {
		s->lastOffUs = nowUs;
	}
	// A classic servo enters serial mode only at a power-up.
	if (!s->userSupply) s->cstate = 0;
}

void fakeHitecLineRefuse(uint8_t err) { g_refuse = err; }
int fakeHitecLineOpens() { return g_opens; }
int fakeHitecLineCloses() { return g_closes; }
uint8_t fakeHitecLineFamily() { return g_family; }
uint32_t fakeHitecLineClosedUs() { return g_closedUs; }
unsigned fakeHitecStarts() { return g_starts; }
const hitec_xfer_t *fakeHitecLastXfer() { return &g_xfer; }
uint32_t fakeHitecLastStartUs() { return g_startUs; }
unsigned fakeHitecAborts() { return g_aborts; }

/** @brief The level the bench drives on GP0, as the fakes record it. */
static bool gp0() {
	FakePin p = fakePin(0);
	return p.output && p.high;
}

/** @brief Whether the servo is powered at @p nowUs. */
static bool powered(uint32_t nowUs) {
	if (g_sim.dead) return false;
	if (g_sim.userSupply) return true;
	return g_sim.level && nowUs - g_sim.onUs >= g_sim.startupMs * 1000u;
}

static void capByte(uint32_t atNs, uint8_t v) {
	g_cap.byte[g_cap.n].at_ns = atNs;
	g_cap.byte[g_cap.n].value = v;
	g_cap.n++;
}

/** @brief A classic reply 513 us after the start of the master's last byte. */
static void cReply(uint8_t a, uint8_t b) {
	capByte(513000u - C_ARM, a);
	capByte(513000u - C_ARM + 520000u, b);
}

/** @brief A write frame taken: count it, and maybe go dead. */
static void tookWrite() {
	g_sim.writes++;
	if (g_sim.dieAfterWrites >= 0 && (int)g_sim.writes >= g_sim.dieAfterWrites) g_sim.dead = true;
}

/** @brief What the servo does with @p x started at @p nowUs: the capture and its end. */
static void answer(const hitec_xfer_t *x, uint32_t nowUs) {
	memset(&g_cap, 0, sizeof(g_cap));
	g_cap.family = x->family;
	g_cap.window_ns = x->window_us * 1000u;
	uint32_t silentEnd = nowUs + (x->window_us ? x->window_us : 1000u);
	g_doneUs = silentEnd;
	HitecSim *s = &g_sim;
	if (x->kind == HITEC_XFER_LISTEN) {
		// The `+` comes from the power-up, which may come after the LISTEN starts.
		if (x->family != HITEC_CLASSIC || s->dead || s->cstate >= 2) return;
		if (!s->userSupply && !s->level) return;
		uint32_t plusUs = s->cstate == 0 ? s->onUs + (s->startupMs + s->plusMs) * 1000u
		                                 : s->firstPlusUs + 250000u;
		uint32_t at = plusUs - nowUs;
		if ((int32_t)at < 0 || at > x->window_us) return;
		capByte(at * 1000u, HITEC_C_ACK);
		g_cap.answered = x->answer;
		if (s->cstate == 0) s->firstPlusUs = plusUs;
		s->cstate++;
		g_doneUs = plusUs + 1000u;
		return;
	}
	if (!powered(nowUs)) return;
	g_doneUs = nowUs + 8000u;
	if (x->family == HITEC_CLASSIC) {
		if (s->cstate < 2) {
			g_doneUs = silentEnd;
			return;
		}
		uint8_t a = x->tx[1];
		if (x->tx[0] == HITEC_C_FIRST) {
			cReply(s->version, HITEC_C_ACK);
		} else if (x->tx[0] == HITEC_C_READ_EEPROM) {
			cReply(s->mem[a], HITEC_C_ACK);
		} else {
			if (s->ignoreAddr != (int)a) s->mem[a] = x->tx[2];
			tookWrite();
			cReply(HITEC_C_ACK, HITEC_C_ACK);
		}
	} else if (!s->userSupply && nowUs - s->onUs < (s->startupMs + s->bootMs) * 1000u) {
		// Still booting: the request goes unanswered.
		g_doneUs = silentEnd;
	} else if (x->n_tx == HITEC_D_READ_LEN) {
		uint8_t a = x->tx[2];
		uint8_t f[HITEC_D_WRITE_LEN];
		hitec_d_write_frame(0, a, (uint16_t)(s->mem[a] | (s->mem[a + 1u] << 8)), f);
		f[0] = HITEC_D_HEADER_ANSWER;
		for (unsigned i = 0; i < HITEC_D_ANSWER_LEN; ++i) capByte(6000000u - D_BIT + i * 86806u, f[i]);
		capByte(6000000u - D_BIT + 7u * 86806u, 0x00);
	} else {
		uint8_t a = x->tx[2];
		if (a == HITEC_D_REG_REBOOT) s->reboots++;
		if (s->ignoreAddr != (int)a && a != HITEC_D_REG_REBOOT && a != HITEC_D_REG_SAVE) {
			s->mem[a] = x->tx[4];
			s->mem[a + 1u] = x->tx[5];
		}
		tookWrite();
		g_doneUs = nowUs + 1000u;
	}
}

uint8_t hitecLineOpen(uint8_t gpio, uint8_t family) {
	(void)gpio;
	if (g_refuse) return g_refuse;
	g_open = true;
	g_opens++;
	g_family = family;
	g_busy = false;
	return HITEC_LINE_OK;
}

void hitecLineClose() {
	if (!g_open) return;
	g_open = false;
	g_busy = false;
	g_closes++;
	g_closedUs = time_us_32();
}

bool hitecLineIsOpen() { return g_open; }

bool hitecLineStart(const hitec_xfer_t *xfer) {
	uint32_t now = time_us_32();
	fakeHitecSimUpdate(gp0(), now);
	if (!g_open || g_busy || xfer->family != g_family) return false;
	g_xfer = *xfer;
	g_startUs = now;
	g_busy = true;
	g_cut = false;
	g_starts++;
	answer(xfer, now);
	return true;
}

bool hitecLinePoll(hitec_capture_t *out) {
	uint32_t now = time_us_32();
	fakeHitecSimUpdate(gp0(), now);
	if (!g_busy) return false;
	if (g_cut) {
		// The bytes so far: none of an answer that was still due.
		if ((int32_t)(now - g_doneUs) < 0) g_cap.n = 0;
		g_cap.aborted = 1;
	} else if ((int32_t)(now - g_doneUs) < 0) {
		return false;
	}
	g_busy = false;
	*out = g_cap;
	return true;
}

void hitecLineAbort() {
	if (!g_busy) return;
	g_cut = true;
	g_aborts++;
}
