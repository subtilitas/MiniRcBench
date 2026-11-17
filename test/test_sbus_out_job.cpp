/**
 * @file test_sbus_out_job.cpp
 * @brief The S.BUS output's core1 job: the pure core, and the job as it
 *        ships on the pin claim, the UART0 fake's transmitter and the GPIO
 *        fake's control and pad registers.
 *
 * The core runs on its own, pass by pass. The job runs from the job table,
 * through the pin claim and the fake DShot pump. The test plays core1's side
 * by calling core1JobPoll() itself, one pass every so many microseconds of
 * the virtual clock, and core0's by sending commands and stamping the
 * heartbeat. Every write of GP0's control register is recorded with FUNCSEL
 * and OUTOVER as one value, and every UARTDR write with its time on the
 * wire, which rcbench's decoder reads back. What the PL011 and the pad do on
 * the chip is not tested here; the levels on the pin are not measured.
 */

#include "check.h"
#include "fakes.h"
#include "fake_escwire_port.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "sbus_out_job.h"
#include "settings.h"
#include "ui.h"
#include "ui_am32.h"
#include "ui_widgets.h"
#include "gfx.h"
#include "plat.h"
#include "ew_line.h"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/structs/pads_bank0.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <regex>
#include <string>
#include <vector>

/** @brief GP0's control register on UART0's TX with its output inverted. */
static const uint32_t CTRL_UART_INV =
	((uint32_t)GPIO_FUNC_UART << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB) |
	((uint32_t)GPIO_OVERRIDE_INVERT << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB);

// ---------------------------------------------------------------------------
// the core
// ---------------------------------------------------------------------------

/** @brief A command of @p kind with @p seq, every channel at raw @p raw. */
static SbusOutCmd cmdOf(SbusOutCmdKind kind, uint16_t seq, uint16_t raw = 992,
                        uint8_t thrCh = SBUS_OUT_THR_DEFAULT, uint16_t ceilRaw = 499) {
	SbusOutCmd c;
	memset(&c, 0, sizeof(c));
	c.seq = seq;
	c.kind = kind;
	for (unsigned i = 0; i < SBUS_CHANNELS; i++) c.channel[i] = raw;
	c.thrCh = thrCh;
	c.ceilRaw = ceilRaw;
	return c;
}

/** @brief sbus_encode() of @p f. */
static void encode(const sbus_frame_t &f, uint8_t out[SBUS_FRAME_BYTES]) {
	sbus_encode(&f, out);
}

/** @brief Ceiling and roles. */
static void testHelpers() {
	section("S.BUS output: the ceiling and the roles of each wiring");

	checkInt("ceiling at MAX THROTTLE 0: raw 172", sbusOutCeilRaw(0), 172);
	checkInt("at 100: 253", sbusOutCeilRaw(100), 253);
	checkInt("at 400, the default: 499", sbusOutCeilRaw(400), 499);
	checkInt("at 1000: 991", sbusOutCeilRaw(1000), 991);
	checkInt("at 1999: 1810", sbusOutCeilRaw(1999), 1810);
	checkInt("at 2000: 1811", sbusOutCeilRaw(2000), 1811);
	checkInt("at 2001: still 1811", sbusOutCeilRaw(2001), 1811);
	checkInt("at 65535: still 1811", sbusOutCeilRaw(65535), 1811);
	checkInt("rcbench reads 499 as 1200 us", sbus_to_us(sbusOutCeilRaw(400)), 1200);

	checkInt("SIG GP0, AUX GP1: SIG", sbusOutClaimRoles(0, 1), PIN_SIG);
	checkInt("SIG GP0, AUX GP0, KISS off: SIG", sbusOutClaimRoles(0, 0), PIN_SIG);
	checkInt("SIG GP1, AUX GP0: SIG and AUX", sbusOutClaimRoles(1, 0), PIN_SIG | PIN_AUX);
	checkInt("SIG GP1, AUX GP1, KISS off: none", sbusOutClaimRoles(1, 1), 0);

	const PinUserInfo &u = pinUserInfo(PinUser::SbusOut);
	checkTrue("the pin user runs the output's job", u.who == PinUser::SbusOut &&
	                                                u.job == Core1Job::SbusOut);
	checkTrue("leave page from the start, the ESC check, no deferred job",
	          u.unplug && u.escCheck && !u.deferJob);
}

/** @brief SAFE, START and the START frame, for each throttle channel. */
static void testCoreStart() {
	section("S.BUS output core: SAFE sends nothing, START sends core1's frame");

	SbusOutCore c;
	uint8_t out[SBUS_FRAME_BYTES];
	sbusOutCoreReset(&c, 7);
	checkTrue("reset: SAFE, no trip, no refusal", !c.running && !c.tripped && !c.refused);
	bool any = false;
	for (uint32_t t = 0; t < 100000; t += 500) any |= sbusOutCoreStep(&c, t, true, true, out);
	checkTrue("SAFE: no frame in 100 ms", !any);
	SbusOutCmd old = cmdOf(SbusOutCmdKind::Start, 7);
	checkTrue("a command of the seq at the reset is not taken",
	          !sbusOutCoreTake(&c, &old, 100000, true, 1811) && !c.running);
	SbusOutCmd fr = cmdOf(SbusOutCmdKind::Frame, 8, 1700);
	fr.ch17 = fr.failsafe = true;
	checkTrue("a Frame while SAFE is taken", sbusOutCoreTake(&c, &fr, 100000, true, 1811));
	checkTrue("and moves nothing", !c.running && c.ackSeq == 8 &&
	                               !sbusOutCoreStep(&c, 100000, true, true, out));
	sbus_frame_t start;
	sbusOutStartFrame(&start, SBUS_OUT_THR_DEFAULT);
	checkTrue("the frame core1 holds is still the START frame",
	          memcmp(&c.frame, &start, sizeof(start)) == 0);

	for (uint8_t thr = 0; thr < 4; thr++) {
		sbusOutCoreReset(&c, 0);
		SbusOutCmd f = cmdOf(SbusOutCmdKind::Frame, 1, 1811);
		sbusOutCoreTake(&c, &f, 0, true, 1811);
		SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 2, 1811, thr, 1811);
		s.ch17 = s.ch18 = s.failsafe = s.frameLost = true;
		checkTrue("Start taken", sbusOutCoreTake(&c, &s, 1000, true, 1811) && c.running);
		memset(out, 0xAA, sizeof(out));
		checkTrue("the first frame on the pass that took Start",
		          sbusOutCoreStep(&c, 1000, true, true, out));
		sbus_frame_t want;
		memset(&want, 0, sizeof(want));
		for (unsigned i = 0; i < SBUS_CHANNELS; i++) want.channel[i] = 172;
		for (unsigned i = 0; i < 4; i++) if (i != thr) want.channel[i] = 992;
		uint8_t wb[SBUS_FRAME_BYTES];
		encode(want, wb);
		char what[64];
		snprintf(what, sizeof(what), "throttle CH%u: START frame byte for byte", thr + 1u);
		checkTrue(what, memcmp(out, wb, sizeof(out)) == 0);
		sbus_decoder_t d;
		sbus_decoder_reset(&d);
		sbus_frame_t got;
		bool frame = false;
		for (unsigned i = 0; i < SBUS_FRAME_BYTES; i++)
			frame = sbus_decode_byte(&d, out[i], 120u * i, &got);
		snprintf(what, sizeof(what), "CH%u decodes at 1000 us, the flags off", thr + 1u);
		checkTrue(what, frame && sbus_to_us(got.channel[thr]) == 1000 && !got.ch17 &&
		                !got.ch18 && !got.failsafe && !got.frame_lost);
	}
	checkInt("the throttle channel at raw 172", c.frame.channel[3], 172);
	checkInt("the other sticks at raw 992, 1500 us", sbus_to_us(c.frame.channel[0]), 1500);
	checkInt("channels 5 to 16 at raw 172", c.frame.channel[4] == 172 && c.frame.channel[15] == 172, 1);
	checkInt("one frame counted", c.frames, 1);

	// Refusals: the heartbeat lapsed, a throttle index past channel 4.
	sbusOutCoreReset(&c, 0);
	SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 1);
	checkTrue("Start with the heartbeat lapsed: taken", sbusOutCoreTake(&c, &s, 0, false, 1811));
	checkTrue("and refused: SAFE", !c.running && c.ackSeq == 1 &&
	                               !sbusOutCoreStep(&c, 0, true, true, out));
	s = cmdOf(SbusOutCmdKind::Start, 2, 992, 4);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	checkTrue("Start with throttle index 4: refused", !c.running && c.ackSeq == 2);
	s = cmdOf(SbusOutCmdKind::Start, 3, 992, 3);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	checkTrue("index 3: taken", c.running && c.thrCh == 3);
	s.seq = 3;
	s.kind = SbusOutCmdKind::Safe;
	checkTrue("a Safe of the taken seq: not new", !sbusOutCoreTake(&c, &s, 0, true, 1811) && c.running);
	s.seq = 4;
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	checkTrue("Safe: no frame on the same pass, though one is due",
	          !c.running && !sbusOutCoreStep(&c, 0, true, true, out));
}

/** @brief Every channel clamped, the throttle to the ceiling, for each throttle channel. */
static void testCoreClamp() {
	section("S.BUS output core: every channel clamped, the throttle to its ceiling");

	static const uint16_t RAW[] = { 0, 171, 172, 499, 500, 1811, 1812, 2047, 2048, 65535 };
	for (uint8_t thr = 0; thr < 4; thr++) {
		SbusOutCore c;
		sbusOutCoreReset(&c, 0);
		SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 1, 992, thr, 499);
		sbusOutCoreTake(&c, &s, 0, true, 1811);
		bool ok = true;
		uint16_t seq = 2;
		for (uint16_t raw : RAW) {
			SbusOutCmd f = cmdOf(SbusOutCmdKind::Frame, seq++, raw, (uint8_t)((thr + 1) % 4), 1811);
			sbusOutCoreTake(&c, &f, 0, true, 1811);
			uint16_t lo = raw < 172 ? 172 : raw;
			for (unsigned i = 0; i < SBUS_CHANNELS; i++) {
				uint16_t hi = i == thr ? 499 : 1811;
				uint16_t want = lo > hi ? hi : lo;
				ok &= c.frame.channel[i] == want;
			}
		}
		char what[72];
		snprintf(what, sizeof(what), "throttle CH%u: 172 to 1811, CH%u to 499", thr + 1u, thr + 1u);
		checkTrue(what, ok);
	}
	SbusOutCore c;
	sbusOutCoreReset(&c, 0);
	SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 1, 992, 2, 5000);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	checkInt("a ceiling above 1811 is taken as 1811", c.ceilRaw, 1811);
	{
		SbusOutCore m;
		sbusOutCoreReset(&m, 0);
		SbusOutCmd w = cmdOf(SbusOutCmdKind::Start, 1, 992, 2, 1811);
		sbusOutCoreTake(&m, &w, 0, true, 499);
		checkInt("a screen's ceiling of 1811 at MAX THROTTLE 400: 499", m.ceilRaw, 499);
		SbusOutCmd f2 = cmdOf(SbusOutCmdKind::Frame, 2, 2047);
		sbusOutCoreTake(&m, &f2, 0, true, 499);
		checkInt("the throttle at 2047 sends 499", m.frame.channel[2], 499);
		w = cmdOf(SbusOutCmdKind::Start, 3, 992, 2, 300);
		sbusOutCoreTake(&m, &w, 0, true, 499);
		checkInt("a lower ceiling from the screen is kept: 300", m.ceilRaw, 300);
		w = cmdOf(SbusOutCmdKind::Start, 4, 992, 2, 500);
		sbusOutCoreTake(&m, &w, 0, true, 499);
		checkInt("500 at MAX THROTTLE's 499: 499", m.ceilRaw, 499);
		w = cmdOf(SbusOutCmdKind::Start, 5, 992, 2, 3000);
		sbusOutCoreTake(&m, &w, 0, true, 5000);
		checkInt("3000 at a maximum of 5000: 1811", m.ceilRaw, 1811);
	}
	SbusOutCmd f = cmdOf(SbusOutCmdKind::Frame, 2, 2047);
	sbusOutCoreTake(&c, &f, 0, true, 1811);
	checkInt("the throttle at 2047 sends 1811", c.frame.channel[2], 1811);
	s = cmdOf(SbusOutCmdKind::Start, 3, 992, 2, 100);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	checkInt("a ceiling below 172 is taken as 172", c.ceilRaw, 172);
	f = cmdOf(SbusOutCmdKind::Frame, 4, 900);
	sbusOutCoreTake(&c, &f, 0, true, 1811);
	checkInt("the throttle stays at 172", c.frame.channel[2], 172);
	checkInt("the others move", c.frame.channel[0], 900);
	f = cmdOf(SbusOutCmdKind::Frame, 5, 900, 2, 1811);
	f.ch17 = true;
	f.frameLost = true;
	sbusOutCoreTake(&c, &f, 0, true, 1811);
	checkTrue("a Frame's ceiling is ignored", c.frame.channel[2] == 172);
	checkTrue("its flags taken", c.frame.ch17 && !c.frame.ch18 && c.frame.frame_lost &&
	                             !c.frame.failsafe);
	f.ch17 = false;
	f.ch18 = true;
	f.failsafe = true;
	f.frameLost = false;
	f.seq = 6;
	sbusOutCoreTake(&c, &f, 0, true, 1811);
	checkTrue("each flag on its own", !c.frame.ch17 && c.frame.ch18 && !c.frame.frame_lost &&
	                                  c.frame.failsafe);
}

/** @brief Run the core with a pass every @p stepUs; record each load's time. */
static std::vector<uint32_t> coreRun(SbusOutCore *c, uint32_t fromUs, uint32_t toUs,
                                     uint32_t stepUs) {
	std::vector<uint32_t> at;
	uint8_t out[SBUS_FRAME_BYTES];
	for (uint32_t t = fromUs; (int32_t)(toUs - t) > 0; t += stepUs)
		if (sbusOutCoreStep(c, t, true, true, out)) at.push_back(t);
	return at;
}

/** @brief The 14 ms schedule, a late pass, the FIFO, the micros() wrap. */
static void testCoreSchedule() {
	section("S.BUS output core: a frame every 14,000 us");

	SbusOutCore c;
	sbusOutCoreReset(&c, 0);
	SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 1);
	sbusOutCoreTake(&c, &s, 1000, true, 1811);
	// Passes every 333 us: a deadline lands between passes.
	std::vector<uint32_t> at = coreRun(&c, 1000, 1000 + 100 * 14000, 333);
	checkInt("100 frames in 1.4 s", (long)at.size(), 100);
	bool grid = true;
	for (size_t i = 0; i < at.size(); i++) {
		uint32_t due = 1000 + (uint32_t)i * 14000;
		grid &= at[i] >= due && at[i] - due < 333;
	}
	checkTrue("each on the first pass at or after its deadline", grid);
	checkInt("frames counted", c.frames, 100);

	// A pass 30 ms late: one frame, then a period from that pass.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	at = coreRun(&c, 0, 14000, 500);
	uint8_t out[SBUS_FRAME_BYTES];
	checkTrue("a pass 30 ms late sends", sbusOutCoreStep(&c, 44000, true, true, out));
	checkTrue("one frame, not two", !sbusOutCoreStep(&c, 44000, true, true, out) &&
	                                !sbusOutCoreStep(&c, 44500, true, true, out));
	at = coreRun(&c, 45000, 87000, 500);
	checkTrue("the next 14 ms after that pass, then on that grid",
	          at.size() == 3 && at[0] == 58000 && at[1] == 72000 && at[2] == 86000);
	// Exactly a period late: also a new grid.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	coreRun(&c, 0, 1, 1);
	checkTrue("a pass one period late sends", sbusOutCoreStep(&c, 28000, true, true, out));
	at = coreRun(&c, 28500, 43000, 500);
	checkTrue("the next a period from it", at.size() == 1 && at[0] == 42000);

	// A pass 13 ms late: the grid holds, but never under 7 ms apart.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	coreRun(&c, 0, 1, 1);
	checkTrue("a pass 13 ms late sends", sbusOutCoreStep(&c, 27000, true, true, out));
	at = coreRun(&c, 27500, 56500, 500);
	checkTrue("the next 7 ms on, not at 28 ms: then the grid",
	          at.size() == 3 && at[0] == 34000 && at[1] == 42000 && at[2] == 56000);
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	coreRun(&c, 0, 1, 1);
	sbusOutCoreStep(&c, 27000, true, true, out);
	at = coreRun(&c, 27001, 34002, 1);
	checkTrue("loaded at 27,000 us: the next at 34,000 us, not 33,999 us",
	          at.size() == 1 && at[0] == 34000);

	// The FIFO not yet empty at the deadline.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	checkTrue("FIFO not empty at the deadline: waits", !sbusOutCoreStep(&c, 0, true, false, out));
	checkTrue("still not: waits", !sbusOutCoreStep(&c, 500, true, false, out));
	checkTrue("empty: sends", sbusOutCoreStep(&c, 1000, true, true, out));
	at = coreRun(&c, 1500, 15000, 500);
	checkTrue("the next on the grid of the deadline", at.size() == 1 && at[0] == 14000);

	// Across the micros() wrap.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0xFFFFC000u, true, 1811);
	at = coreRun(&c, 0xFFFFC000u, 0xFFFFC000u + 5 * 14000, 500);
	grid = at.size() == 5;
	for (size_t i = 0; grid && i < at.size(); i++) grid = at[i] == 0xFFFFC000u + (uint32_t)i * 14000;
	checkTrue("five frames across the wrap, on the grid", grid);

	// STOP and START again soon: 7 ms after the last frame.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	coreRun(&c, 0, 1, 1);
	SbusOutCmd st = cmdOf(SbusOutCmdKind::Safe, 2);
	sbusOutCoreTake(&c, &st, 2000, true, 1811);
	SbusOutCmd again = cmdOf(SbusOutCmdKind::Start, 3);
	sbusOutCoreTake(&c, &again, 3000, true, 1811);
	at = coreRun(&c, 3000, 8000, 500);
	checkTrue("START 3 ms after a frame: the first 7 ms after it", at.size() == 1 && at[0] == 7000);
	checkInt("the count starts again at START", c.frames, 1);
}

/** @brief The heartbeat and the encoder's refusal. */
static void testCoreStops() {
	section("S.BUS output core: the heartbeat trip and a refused frame");

	SbusOutCore c;
	uint8_t out[SBUS_FRAME_BYTES];
	sbusOutCoreReset(&c, 0);
	SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 1);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	sbusOutCoreStep(&c, 0, true, true, out);
	checkTrue("a lapsed heartbeat while RUNNING: no frame", !sbusOutCoreStep(&c, 14000, false, true, out));
	checkTrue("SAFE, tripped", !c.running && c.tripped);
	std::vector<uint32_t> at = coreRun(&c, 14500, 60000, 500);
	checkTrue("the heartbeat back: still nothing, still tripped", at.empty() && c.tripped);
	SbusOutCmd lapsed = cmdOf(SbusOutCmdKind::Start, 2);
	sbusOutCoreTake(&c, &lapsed, 60000, false, 1811);
	checkTrue("Start while lapsed: refused, the trip kept", !c.running && c.tripped);
	SbusOutCmd ok = cmdOf(SbusOutCmdKind::Start, 3);
	sbusOutCoreTake(&c, &ok, 60500, true, 1811);
	checkTrue("Start: RUNNING, the trip cleared", c.running && !c.tripped);
	sbusOutCoreReset(&c, 0);
	checkTrue("a lapse while SAFE trips nothing",
	          !sbusOutCoreStep(&c, 0, false, true, out) && !c.tripped);

	// A frame the encoder refuses: only by a channel the clamp did not make.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true, 1811);
	c.frame.channel[9] = 2048;
	memset(out, 0xAA, sizeof(out));
	checkTrue("a channel at 2048: no frame", !sbusOutCoreStep(&c, 0, true, true, out));
	bool untouched = true;
	for (uint8_t b : out) untouched &= b == 0xAA;
	checkTrue("SAFE, refused, the bytes unwritten", !c.running && c.refused && untouched);
	checkInt("nothing counted", c.frames, 0);
	SbusOutCmd again = cmdOf(SbusOutCmdKind::Start, 2);
	sbusOutCoreTake(&c, &again, 100, true, 1811);
	checkTrue("START clears the refusal and sends", !c.refused &&
	                                                sbusOutCoreStep(&c, 100, true, true, out));
	c.frame.channel[0] = 2047;
	checkTrue("2047 is sent", sbusOutCoreStep(&c, 14100, true, true, out) && !c.refused);
}

// ---------------------------------------------------------------------------
// the job, on the claim, the UART0 fake and the GPIO fake
// ---------------------------------------------------------------------------

/** @brief The wiring, the pump bound and past any drain, no ESC, no KISS. */
static void freshPump(uint8_t sig, uint8_t aux, bool kiss = true) {
	// Run not wanted, so a claim is not Busy (docs/design/dshot-lifetime.md).
	// The pump itself runs, as a claim found it before slice D2: the job's
	// own pad writes are under test here, and the fake's teardown writes no
	// pad. A claim from the parked pump is tested in test_claim.cpp.
	pinClaimDshot(false);
	escTaskStart();
	escTaskConfigure(sig, 600, kiss, aux);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
	// The pads the pump's park or bind wrote are not the job's.
	fakePinsReset();
}

/** @brief SIG as the pump's teardown leaves it: an SIO input with the pull-up (esc_task.cpp). */
static void teardownState(unsigned gpio) {
	gpio_set_function(gpio, GPIO_FUNC_SIO);
	gpio_pull_up(gpio);
	gpio_set_dir(gpio, GPIO_IN);
}

/** @brief AUX as the KISS receiver's end leaves it: the NULL function, the pulls off (pio_uart_rx.cpp). */
static void kissEndState(unsigned gpio) {
	gpio_set_function(gpio, GPIO_FUNC_NULL);
	gpio_disable_pulls(gpio);
}

/** @brief An S.BUS output claim with @p roles through to the job's bind. */
static ClaimState claimToBind(uint8_t roles) {
	if (pinClaimBegin(PinUser::SbusOut, roles) != ClaimRefusal::None) return ClaimState::Free;
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	return pinClaimPoll();
}

/** @brief Whether the heartbeat is stamped on each pass of runTo(). */
static bool g_beat = true;

/**
 * @brief Passes of core1 every @p stepUs until the clock reaches @p untilUs,
 *        the heartbeat stamped on each while @ref g_beat.
 */
static void runTo(uint64_t untilUs, uint32_t stepUs) {
	while (time_us_64() < untilUs) {
		fakeAdvanceUs(stepUs);
		if (g_beat) escHeartbeat();
		core1JobPoll();
	}
}

/** @brief Run passes every @p stepUs until a frame more than @p frames is loaded; false after 100 ms. */
static bool runToFrame(uint32_t frames, uint32_t stepUs) {
	uint64_t until = time_us_64() + 100000;
	SbusOutStatus st;
	do {
		fakeAdvanceUs(stepUs);
		if (g_beat) escHeartbeat();
		core1JobPoll();
		sbusOutJobStatus(&st);
	} while (st.frames <= frames && time_us_64() < until);
	return st.frames > frames;
}

/** @brief No ESC on GP0: rcbench's model, unplugged; the line's counts at 0. */
static void noEsc() {
	fakeEwReset(0xB2, true);
	fakeEwSim()->dead = true;
}

/** @brief Passes every 500 us until the probe passed; false after 3 s. */
static bool probeToSafe() {
	uint64_t until = time_us_64() + 3000000;
	while (!sbusOutProbePassed() && time_us_64() < until) {
		fakeAdvanceUs(500);
		if (g_beat) escHeartbeat();
		core1JobPoll();
	}
	return sbusOutProbePassed();
}

/**
 * @brief An S.BUS output claim with @p roles, no ESC on GP0, through the
 *        probe to SAFE: Running once the probe passed, else Free.
 */
static ClaimState claimToSafe(uint8_t roles) {
	noEsc();
	ClaimState s = claimToBind(roles);
	if (s != ClaimState::Running || !probeToSafe()) return ClaimState::Free;
	return s;
}

/** @brief Send a command of @p kind; returns its seq. */
static uint16_t send(SbusOutCmdKind kind, uint16_t raw = 992, uint8_t thrCh = SBUS_OUT_THR_DEFAULT) {
	SbusOutCmd c = cmdOf(kind, 0, raw, thrCh, sbusOutCeilRaw(400));
	return sbusOutJobCommand(&c);
}

/** @brief BACK: the release, the unplug page due, RESTART DSHOT, Free. */
static bool claimEnd() {
	pinClaimEnd(PinUser::SbusOut);
	runTo(time_us_64() + 5000, 500);
	pinClaimPoll();
	bool due = pinClaimUnplugDue();
	pinClaimRelease(PinUser::SbusOut);
	fakePumpPoll();
	return due && pinClaimState() == ClaimState::Free;
}

/** @brief Two records of a pin are equal, field by field. */
static bool samePin(const FakePin &a, const FakePin &b) {
	return a.function == b.function && a.level == b.level && a.output == b.output &&
	       a.pull == b.pull && a.high == b.high && a.inputOff == b.inputOff &&
	       a.puts == b.puts && a.inover == b.inover && a.outover == b.outover &&
	       a.ctrl == b.ctrl && a.dirAt == b.dirAt && a.pullAt == b.pullAt &&
	       a.functionAt == b.functionAt;
}

/** @brief No write of GP0's control register put it on the UART without OUTOVER inverted. */
static bool neverUartPlain() {
	for (int i = 0; i < fakeGpioCtrlWrites(0); i++) {
		uint32_t v = fakeGpioCtrlWrite(0, i).value;
		if ((v & IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS) == GPIO_FUNC_UART && v != CTRL_UART_INV)
			return false;
	}
	return true;
}

/** @brief GP0 let go: SIO, no override, an input, low, the pull-down. */
static bool gp0Released() {
	FakePin p = fakePin(0);
	return p.function == GPIO_FUNC_SIO && p.outover == GPIO_OVERRIDE_NORMAL &&
	       p.inover == GPIO_OVERRIDE_NORMAL && !p.output && !p.high && p.pull == FakePull::Down;
}

/** @brief The default wiring: the bind, the probe's line and the switch, register by register. */
static void testJobBind() {
	section("S.BUS output job: the bind, the probe's line and the switch, register by register");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	// OD and ISO set by hand: no firmware path leaves the pad so, and the
	// bind must not rely on either being clear.
	pads_bank0_hw->io[0].value |= PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS;
	// SIO left driving GP0 high by an earlier owner.
	gpio_set_dir(0, GPIO_OUT);
	gpio_put(0, 1);
	FakePin gp1 = fakePin(1);
	noEsc();
	int ctrl0 = fakeGpioCtrlWrites(0);
	checkTrue("the claim runs the shipped job",
	          claimToBind(sbusOutClaimRoles(0, 1)) == ClaimState::Running &&
	          core1JobRunning() == Core1Job::SbusOut);
	FakePin p = fakePin(0);
	FakeUart u = fakeUart();
	checkTrue("SIO's output enable off and its output low first",
	          !p.output && !p.high && p.dirAt < u.initAt && p.putAt < u.initAt);
	checkInt("100,000 baud from 150 MHz", u.baud, 100000);
	checkTrue("8E2", u.dataBits == 8 && u.stopBits == 2 && u.parity == UART_PARITY_EVEN);
	checkTrue("then the receiver off: RXE clear, TXE and UARTEN set",
	          u.formatAt > u.initAt && u.crAt > u.formatAt && !u.rxe && u.txe && u.enabled);
	checkTrue("the probe's line opened on GP0", fakeEwOpen() && fakeEwOpens() == 1 &&
	          fakeEwGpio() == 0);
	checkInt("one write of GP0's control register: the line's", fakeGpioCtrlWrites(0) - ctrl0, 1);
	FakeCtrlWrite pw = fakeGpioCtrlWrite(0, ctrl0);
	checkInt("FUNCSEL PIO2, no override", pw.value, GPIO_FUNC_PIO2);
	checkTrue("after the UART is set up", pw.at > u.crAt);
	checkTrue("then the pull-up, the input on: the line rests high",
	          p.pull == FakePull::Up && p.pullAt > pw.at && !p.inputOff &&
	          fakeEwSim()->line == EW_LINE_UP);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("the probe: not passed, nothing sent", st.phase == SbusOutPhase::Probe &&
	          !sbusOutProbePassed() && fakeEwSim()->starts == 0 && !st.running);
	checkTrue("GP1 untouched", samePin(fakePin(1), gp1) && fakeGpioCtrlWrites(1) == 0);

	// The switch, once a cycle passed with no answer.
	checkTrue("the probe passes", probeToSafe());
	p = fakePin(0);
	checkInt("three writes of GP0's control register in all", fakeGpioCtrlWrites(0) - ctrl0, 3);
	FakeCtrlWrite sio = fakeGpioCtrlWrite(0, ctrl0 + 1);
	FakeCtrlWrite w = fakeGpioCtrlWrite(0, ctrl0 + 2);
	checkInt("the line closed: GP0 on SIO", sio.value, GPIO_FUNC_SIO);
	checkTrue("the line closed once", !fakeEwOpen() && fakeEwCloses() == 1);
	checkInt("then FUNCSEL UART and OUTOVER inverted in one write", w.value, CTRL_UART_INV);
	checkTrue("SIO's output enable off and its output low before it",
	          !p.output && !p.high && p.dirAt < w.at && p.putAt < w.at);
	checkTrue("the pull-down between them: the line falls to the UART's idle level",
	          p.pull == FakePull::Down && p.pullAt > sio.at && p.pullAt < w.at);
	checkTrue("the pad first: IE on, OD off", p.padMaskAt > p.pullAt && p.padMaskAt < w.at);
	checkTrue("then isolation cleared", p.padIsoAt > w.at);
	uint32_t pad = fakePad(0);
	checkTrue("the pad: IE set, OD and ISO clear",
	          (pad & PADS_BANK0_GPIO0_IE_BITS) &&
	          !(pad & (PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS)));
	checkTrue("GP0 never on the UART without the inversion", neverUartPlain());
	checkTrue("GP1 still untouched", samePin(fakePin(1), gp1) && fakeGpioCtrlWrites(1) == 0);
	checkTrue("UART0 set up once, at the bind", fakeUart().inits == 1 && fakeUart().enabled);
	sbusOutJobStatus(&st);
	checkTrue("SAFE: not running, nothing sent", st.phase == SbusOutPhase::Uart &&
	          !st.running && !st.tripped && st.frames == 0);
	runTo(time_us_64() + 100000, 500);
	checkInt("SAFE for 100 ms: no character on the wire", fakeUartTxCount(), 0);
	checkTrue("GP0 still bound, inverted", fakePin(0).ctrl == CTRL_UART_INV);
	checkTrue("back to free", claimEnd());
	checkTrue("GP0 parked: SIG driven low", fakePin(0).function == GPIO_FUNC_SIO &&
	          fakePin(0).outover == GPIO_OVERRIDE_NORMAL && fakePin(0).output && !fakePin(0).high);
	checkTrue("still never on the UART without the inversion", neverUartPlain());

	// A command sent before the bind is not taken.
	freshPump(0, 1);
	send(SbusOutCmdKind::Start);
	checkTrue("a START sent before the claim", claimToSafe(PIN_SIG) == ClaimState::Running);
	runTo(time_us_64() + 50000, 500);
	sbusOutJobStatus(&st);
	checkTrue("is not taken: SAFE, no frame", !st.running && fakeUartTxCount() == 0);
	checkTrue("back to free", claimEnd());
}

/** @brief One decoded frame and the time its last byte was in. */
struct LineFrame { sbus_frame_t f; uint64_t endUs; uint64_t startUs; };

/** @brief rcbench's decoder over every character the fake sent, at the time each was in. */
static std::vector<LineFrame> decodeLine(int from, bool *gapsOk, bool *onPad) {
	std::vector<LineFrame> out;
	sbus_decoder_t d;
	sbus_decoder_reset(&d);
	*gapsOk = true;
	*onPad = true;
	uint64_t frameStart = 0;
	for (int i = from; i < fakeUartTxCount(); i++) {
		FakeTxByte b = fakeUartTxByte(i);
		*onPad &= b.onPad && !b.cut && b.bits == 12 && b.endUs - b.startUs == 120;
		if (i > from) {
			FakeTxByte prev = fakeUartTxByte(i - 1);
			uint64_t gap = b.startUs - prev.endUs;
			// Inside a frame back to back; between frames 2000 us or more.
			bool inFrame = (i - from) % SBUS_FRAME_BYTES != 0;
			*gapsOk &= inFrame ? gap == 0 : gap >= SBUS_GAP_US;
		}
		if ((i - from) % SBUS_FRAME_BYTES == 0) frameStart = b.startUs;
		sbus_frame_t f;
		if (sbus_decode_byte(&d, b.value, (uint32_t)b.endUs, &f))
			out.push_back(LineFrame{f, b.endUs, frameStart});
	}
	return out;
}

/** @brief Two frames carry the same channels and flags. */
static bool sameFrame(const sbus_frame_t &a, const sbus_frame_t &b) {
	return memcmp(a.channel, b.channel, sizeof(a.channel)) == 0 && a.ch17 == b.ch17 &&
	       a.ch18 == b.ch18 && a.failsafe == b.failsafe && a.frame_lost == b.frame_lost;
}

/** @brief START, frames, the line read back, STOP. */
static void testJobLine() {
	section("S.BUS output job: the line read back by rcbench's decoder");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	g_beat = true;
	escHeartbeat();
	uint16_t seq = send(SbusOutCmdKind::Start);
	// Passes every 500 us; a new Frame every 25 ms, as the UI frame sends.
	std::vector<sbus_frame_t> built;
	SbusOutStatus st;
	uint32_t seen = 0;
	uint64_t t0 = time_us_64();
	for (int ui = 0; ui < 24; ui++) {
		if (ui > 0) {
			SbusOutCmd c = cmdOf(SbusOutCmdKind::Frame, 0, (uint16_t)(172 + ui * 60));
			c.channel[2] = 2047;  // the throttle: held at the ceiling
			c.ch17 = ui % 2;
			c.failsafe = ui % 3 == 0;
			c.frameLost = ui % 5 == 0;
			sbusOutJobCommand(&c);
		}
		for (int k = 0; k < 50; k++) {
			fakeAdvanceUs(500);
			escHeartbeat();
			core1JobPoll();
			sbusOutJobStatus(&st);
			if (st.frames != seen) {
				seen = st.frames;
				built.push_back(st.sent);
			}
		}
	}
	checkInt("START taken", st.ackSeq >= seq, 1);
	checkInt("43 frames in 600 ms at 14 ms", (long)built.size(), 43);
	checkInt("the status counts them", st.frames, 43);
	checkInt("lastFrameMs: the pass of the last load", st.lastFrameMs,
	         (long)(uint32_t)(fakeUartTxByte(fakeUartTxCount() - 1).writeUs / 1000));
	bool gapsOk, onPad;
	std::vector<LineFrame> line = decodeLine(0, &gapsOk, &onPad);
	checkInt("the decoder reads every frame", (long)line.size(), (long)built.size());
	bool same = line.size() == built.size();
	for (size_t i = 0; same && i < line.size(); i++) same = sameFrame(line[i].f, built[i]);
	checkTrue("each the frame the core built", same);
	checkTrue("the first: START, the throttle at 1000 us", line.size() &&
	          sbus_to_us(line[0].f.channel[2]) == 1000 && sbus_to_us(line[0].f.channel[0]) == 1500);
	bool ceil = true;
	for (size_t i = 2; i < line.size(); i++) ceil &= line[i].f.channel[2] == 499;
	checkTrue("the throttle at 2047 asked: 499, 1200 us", ceil);
	checkTrue("every character on GP0, inverted, 12 bits of 10 us", onPad);
	checkTrue("back to back inside a frame, 2000 us or more between", gapsOk);
	bool period = true;
	for (size_t i = 1; i < line.size(); i++)
		period &= line[i].startUs - line[i - 1].startUs == 14000;
	checkTrue("frame starts 14,000 us apart", period);
	checkTrue("the first within a pass of START", line.size() && line[0].startUs - t0 <= 500);
	checkInt("no character lost or cut", fakeUart().txLost + fakeUart().txCut, 0);

	// STOP: no frame after the one on the wire.
	runToFrame(st.frames, 500);
	int n = fakeUartTxCount();
	send(SbusOutCmdKind::Safe);
	fakeAdvanceUs(500);
	escHeartbeat();
	core1JobPoll();
	sbusOutJobStatus(&st);
	checkTrue("STOP on the next pass: SAFE", !st.running);
	runTo(time_us_64() + 100000, 500);
	checkInt("no frame after it", fakeUartTxCount(), n);
	checkInt("and the one on the wire completed", fakeUart().txCut, 0);
	checkTrue("GP0 still bound and inverted: idle low", fakePin(0).ctrl == CTRL_UART_INV);

	// A STOP not yet taken is never replaced by a later command.
	send(SbusOutCmdKind::Start);
	checkTrue("START again: frames", runToFrame(0, 500));
	uint16_t stop = send(SbusOutCmdKind::Safe);
	checkInt("a START sent before core1 took the STOP: dropped", send(SbusOutCmdKind::Start), stop);
	n = fakeUartTxCount();
	runTo(time_us_64() + 50000, 500);
	sbusOutJobStatus(&st);
	checkTrue("SAFE, no frame after it", !st.running && st.ackSeq == stop && fakeUartTxCount() == n);
	checkTrue("back to free", claimEnd());
}

/** @brief A ceiling the screen gets wrong: core1 holds MAX THROTTLE's. */
static void testJobCeiling() {
	section("S.BUS output job: core1 holds the throttle to MAX THROTTLE's ceiling");

	uint16_t saved = settings()->maxThrottle;
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	g_beat = true;
	static const uint16_t MT[] = { 400, 1000 };
	for (uint16_t mt : MT) {
		settings()->maxThrottle = mt;
		SbusOutCmd c = cmdOf(SbusOutCmdKind::Start, 0, 992, 2, 1811);
		sbusOutJobCommand(&c);
		runTo(time_us_64() + 2000, 500);
		c = cmdOf(SbusOutCmdKind::Frame, 0, 2047);
		sbusOutJobCommand(&c);
		SbusOutStatus st;
		sbusOutJobStatus(&st);
		checkTrue("a frame", runToFrame(st.frames, 500));
		sbusOutJobStatus(&st);
		char what[72];
		snprintf(what, sizeof(what), "MAX THROTTLE %u, the screen's ceiling 1811: CH3 at", mt);
		checkInt(what, st.sent.channel[2], sbusOutCeilRaw(mt));
		send(SbusOutCmdKind::Safe);
		runTo(time_us_64() + 2000, 500);
	}
	settings()->maxThrottle = saved;
	checkTrue("back to free", claimEnd());
}

/** @brief The release after the last frame, and its order. */
static void testJobRelease() {
	section("S.BUS output job: the release waits for the last frame");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	// SIO left driving GP0 high by an earlier owner: the release must not
	// hand the pad back to it driving.
	gpio_set_dir(0, GPIO_OUT);
	gpio_put(0, 1);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	g_beat = true;
	escHeartbeat();
	send(SbusOutCmdKind::Start);
	checkTrue("a frame", runToFrame(0, 500));
	checkTrue("a second", runToFrame(1, 500));
	FakeTxByte last = fakeUartTxByte(fakeUartTxCount() - 1);
	uint64_t loadUs = fakeUartTxByte(fakeUartTxCount() - 25).writeUs;
	// BACK 1000 us into the frame.
	fakeAdvanceUs(500);
	int n = fakeUartTxCount();
	pinClaimEnd(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("BUSY: the job stays Stopping", core1JobPhase() == Core1JobPhase::Stopping);
	checkTrue("GP0 still the UART's, inverted", fakePin(0).ctrl == CTRL_UART_INV);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("no longer RUNNING", !st.running);
	uint64_t idleUs = 0;
	for (int k = 0; k < 20 && !idleUs; k++) {
		fakeAdvanceUs(250);
		core1JobPoll();
		if (core1JobPhase() == Core1JobPhase::Idle) idleUs = time_us_64();
	}
	checkTrue("idle once the last stop bit is out", idleUs >= last.endUs && idleUs < last.endUs + 250);
	checkTrue("within 3,000 us of the load, and a pass", idleUs - loadUs <= SBUS_OUT_FRAME_US + 250);
	checkInt("no new frame", fakeUartTxCount(), n);
	checkInt("none cut", fakeUart().txCut, 0);
	FakePin p = fakePin(0);
	FakeUart u = fakeUart();
	int nw = fakeGpioCtrlWrites(0);
	FakeCtrlWrite sio = fakeGpioCtrlWrite(0, nw - 1);
	checkInt("GP0's last control write: SIO, OUTOVER normal", sio.value, GPIO_FUNC_SIO);
	checkTrue("after the last stop bit", sio.us >= last.endUs);
	checkTrue("SIO's output enable off before it", p.dirAt != 0 && p.dirAt < sio.at && !p.output);
	checkTrue("SIO's output low before it", p.putAt != 0 && p.putAt < sio.at && !p.high);
	checkTrue("the pull-down after it", p.pull == FakePull::Down && p.pullAt > sio.at);
	checkTrue("GP0 on SIO before uart_deinit()", u.deinitAt > sio.at && !u.enabled);
	checkTrue("GP0 let go low", gp0Released());
	checkTrue("never on the UART without the inversion", neverUartPlain());
	sbusOutJobStatus(&st);
	checkTrue("the status cleared", !st.running && st.frames == 0 && st.lastFrameMs == 0 &&
	                                st.sent.channel[0] == 0);
	pinClaimPoll();
	checkTrue("the leave page is due", pinClaimUnplugDue() && escTaskSuspended());
	pinClaimRelease(PinUser::SbusOut);
	fakePumpPoll();
	checkTrue("RESTART DSHOT: free", pinClaimState() == ClaimState::Free);
	checkTrue("the job was idle before the park", fakeParkJobIdle());

	// A transmitter that never sends: the next frame waits for TXFE.
	freshPump(0, 1);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	fakeUartTxStuck(true);
	FakeUart su = fakeUart();
	escHeartbeat();
	send(SbusOutCmdKind::Start);
	runTo(time_us_64() + 50000, 500);
	checkTrue("one frame loaded, none after it while the FIFO holds it",
	          fakeUart().txWrites == su.txWrites + 25 && fakeUart().txLost == su.txLost);
	fakeUartTxStuck(false);
	checkTrue("back to free", claimEnd());

	// While SAFE nothing is on the wire: one pass.
	freshPump(0, 1);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	pinClaimEnd(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("SAFE: released in one pass", core1JobPhase() == Core1JobPhase::Idle &&
	                                        gp0Released());
	pinClaimPoll();
	pinClaimRelease(PinUser::SbusOut);
	fakePumpPoll();

	// A BUSY that never clears: the backstop, 10 ms after the first pass.
	freshPump(0, 1);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	fakeUartTxStuck(true);
	pinClaimEnd(PinUser::SbusOut);
	core1JobPoll();
	for (int ms = 1; ms <= 9; ms++) {
		fakeAdvance(1);
		core1JobPoll();
	}
	checkTrue("BUSY stuck: Stopping 9 ms on", core1JobPhase() == Core1JobPhase::Stopping &&
	                                          fakePin(0).ctrl == CTRL_UART_INV);
	fakeAdvance(1);
	core1JobPoll();
	checkTrue("let go 10 ms on", core1JobPhase() == Core1JobPhase::Idle && gp0Released());
	fakeUartTxStuck(false);
	pinClaimPoll();
	pinClaimRelease(PinUser::SbusOut);
	fakePumpPoll();
	checkTrue("free", pinClaimState() == ClaimState::Free);
}

/** @brief The heartbeat through the job, and a new visit. */
static void testJobHeartbeat() {
	section("S.BUS output job: the heartbeat trip at 250 ms, a new visit");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	checkTrue("running", claimToSafe(PIN_SIG) == ClaimState::Running);
	g_beat = true;
	escHeartbeat();
	send(SbusOutCmdKind::Start);
	runTo(time_us_64() + 50000, 1000);
	escHeartbeat();
	uint64_t hbUs = time_us_64();
	g_beat = false;
	runTo(hbUs + 249000, 1000);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("249 ms after the last heartbeat: RUNNING", st.running && !st.tripped);
	uint32_t frames = st.frames;
	runTo(hbUs + 250000, 1000);
	sbusOutJobStatus(&st);
	checkTrue("250 ms: SAFE, tripped", !st.running && st.tripped);
	int n = fakeUartTxCount();
	runTo(time_us_64() + 100000, 1000);
	checkInt("no frame after the trip", fakeUartTxCount(), n);
	checkTrue("frames went out until then", frames >= 20);
	uint16_t seq = send(SbusOutCmdKind::Start);
	runTo(time_us_64() + 2000, 1000);
	sbusOutJobStatus(&st);
	checkTrue("START while lapsed: taken and refused", st.ackSeq == seq && !st.running &&
	                                                   st.tripped);
	g_beat = true;
	send(SbusOutCmdKind::Start);
	runTo(time_us_64() + 2000, 1000);
	sbusOutJobStatus(&st);
	checkTrue("START with the heartbeat: RUNNING, the trip cleared", st.running && !st.tripped);
	g_beat = false;
	runTo(time_us_64() + 300000, 1000);
	sbusOutJobStatus(&st);
	checkTrue("tripped again", st.tripped);
	g_beat = true;
	checkTrue("back to free", claimEnd());
	freshPump(0, 1);
	noEsc();
	checkTrue("a new visit", claimToBind(PIN_SIG) == ClaimState::Running);
	sbusOutJobStatus(&st);
	checkTrue("starts in the probe, with no trip", st.phase == SbusOutPhase::Probe &&
	          !st.running && !st.tripped && !st.refused && st.frames == 0);
	checkTrue("then SAFE with no trip", probeToSafe());
	sbusOutJobStatus(&st);
	checkTrue("not running, nothing sent", !st.running && !st.tripped && st.frames == 0);
	checkTrue("back to free", claimEnd());
}

/** @brief Each wiring, NO GP0, the clock, the ESC check, KISS. */
static void testJobWirings() {
	section("S.BUS output job: each wiring, NO GP0, the clock, the ESC check");

	// ESC GP1, KISS GP0: GP0 is AUX.
	fakePwmReset();
	fakeUartReset();
	freshPump(1, 0);
	kissEndState(0);
	FakePin gp1 = fakePin(1);
	checkTrue("SIG GP1, AUX GP0: runs with SIG and AUX",
	          claimToSafe(sbusOutClaimRoles(1, 0)) == ClaimState::Running);
	checkTrue("it binds GP0", fakePin(0).ctrl == CTRL_UART_INV);
	checkTrue("from the KISS receiver's NULL and no pulls: the pull-down on",
	          fakePin(0).pull == FakePull::Down);
	checkTrue("GP1, SIG here, untouched", samePin(fakePin(1), gp1) && fakeGpioCtrlWrites(1) == 0);
	g_beat = true;
	escHeartbeat();
	send(SbusOutCmdKind::Start);
	checkTrue("and sends", runToFrame(0, 500) && fakeUartTxByte(0).onPad);
	checkTrue("back to free", claimEnd());
	checkTrue("GP0 let go low", gp0Released());

	// ESC GP0, KISS GP0, KISS off.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 0, false);
	checkTrue("SIG GP0, AUX GP0: runs with SIG", claimToSafe(sbusOutClaimRoles(0, 0)) == ClaimState::Running);
	checkTrue("it binds GP0", fakePin(0).ctrl == CTRL_UART_INV);
	checkTrue("back to free", claimEnd());

	// ESC GP1, KISS GP1, KISS off: no role on GP0.
	fakePwmReset();
	fakeUartReset();
	freshPump(1, 1, false);
	FakePin gp0 = fakePin(0);
	gp1 = fakePin(1);
	checkTrue("a claim of SIG anyway: held, not running", claimToBind(PIN_SIG) == ClaimState::Held);
	checkInt("the bind's reason: NO GP0", pinClaimJobError(), CORE1_JOB_NO_GP0);
	checkTrue("UART0 never started", fakeUart().inits == 0);
	checkTrue("neither pin touched", samePin(fakePin(0), gp0) && samePin(fakePin(1), gp1));
	pinClaimEnd(PinUser::SbusOut);
	pinClaimPoll();
	pinClaimRelease(PinUser::SbusOut);
	fakePumpPoll();
	checkTrue("BACK: free", pinClaimState() == ClaimState::Free);
	gp0 = fakePin(0);
	checkInt("the bind itself: SIG GP1, AUX GP1", SBUS_OUT_JOB_OPS.bind(1, 1), CORE1_JOB_NO_GP0);
	checkInt("SIG GP1, AUX not asked", SBUS_OUT_JOB_OPS.bind(1, 0xFF), CORE1_JOB_NO_GP0);
	checkTrue("nothing touched", samePin(fakePin(0), gp0) && fakeUart().inits == 0);
	// The default wiring with AUX alone would be no claim; SIG GP0 with GP1
	// as AUX asked too still binds GP0 only.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	gp1 = fakePin(1);
	checkTrue("SIG GP0 and AUX GP1 both asked: runs", claimToSafe(PIN_SIG | PIN_AUX) == ClaimState::Running);
	checkTrue("GP1 untouched", samePin(fakePin(1), gp1) && fakeGpioCtrlWrites(1) == 0);
	checkTrue("back to free", claimEnd());

	// clk_peri at 1 MHz: the divisors give 62,500 baud.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	// GP0 on the NULL function, as the KISS receiver leaves AUX, and SIO
	// set to drive it high: a refused bind leaves SIO driving nothing and
	// the line on the pull-up.
	kissEndState(0);
	gpio_set_dir(0, GPIO_OUT);
	gpio_put(0, 1);
	noEsc();
	int ctrl0 = fakeGpioCtrlWrites(0);
	fakeClockSet(clk_peri, 1000000u);
	checkTrue("clk_peri 1 MHz: held, not running", claimToBind(PIN_SIG) == ClaimState::Held);
	checkInt("CLOCK", pinClaimJobError(), CORE1_JOB_CLOCK);
	checkTrue("UART0 off again", !fakeUart().enabled && fakeUart().deinits == 1);
	checkTrue("the probe's line never opened", fakeEwOpens() == 0);
	checkInt("GP0's control register not written", fakeGpioCtrlWrites(0) - ctrl0, 0);
	FakePin cp = fakePin(0);
	checkTrue("SIO's output enable off and its output low before uart_init()",
	          !cp.output && !cp.high && cp.dirAt < fakeUart().initAt && cp.putAt < fakeUart().initAt);
	checkTrue("the pull-up after the refusal: the line stays high",
	          cp.pull == FakePull::Up && cp.pullAt > fakeUart().initAt);
	fakeClockSet(clk_peri, 150000000u);
	pinClaimRetry(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("RETRY at 150 MHz: the probe runs", pinClaimPoll() == ClaimState::Running &&
	          fakeEwOpen() && !sbusOutProbePassed());
	checkTrue("then SAFE, GP0 the UART's", probeToSafe() && fakePin(0).ctrl == CTRL_UART_INV);
	checkTrue("back to free", claimEnd());

	// The ESC check, in both wirings.
	for (int swapped = 0; swapped < 2; swapped++) {
		fakePwmReset();
		fakeUartReset();
		uint8_t sig = swapped ? 1 : 0, aux = swapped ? 0 : 1;
		freshPump(sig, aux);
		uint8_t roles = sbusOutClaimRoles(sig, aux);
		EscTelemetry t;
		memset(&t, 0, sizeof(t));
		t.lastRpmMs = millis() - 10;
		fakeSetTelemetry(&t);
		const char *w = swapped ? "GP0 AUX" : "GP0 SIG";
		char what[72];
		snprintf(what, sizeof(what), "%s: an ESC answering on SIG: refused", w);
		checkTrue(what, pinClaimBegin(PinUser::SbusOut, roles) == ClaimRefusal::EscAnswering);
		checkTrue("DShot keeps running", !escTaskSuspended() && pinClaimState() == ClaimState::Free);
		t.lastRpmMs = millis() - 499;
		fakeSetTelemetry(&t);
		checkTrue("499 ms after the last eRPM: refused",
		          pinClaimBegin(PinUser::SbusOut, roles) == ClaimRefusal::EscAnswering);
		t.lastRpmMs = millis() - 500;
		fakeSetTelemetry(&t);
		snprintf(what, sizeof(what), "%s: 500 ms after it: claims", w);
		checkTrue(what, pinClaimBegin(PinUser::SbusOut, roles) == ClaimRefusal::None);
		fakePumpPoll();
		pinClaimPoll();
		core1JobPoll();
		checkTrue("running", pinClaimPoll() == ClaimState::Running);
		memset(&t, 0, sizeof(t));
		fakeSetTelemetry(&t);
		checkTrue("back to free", claimEnd());
	}

	// KISS live on AUX GP0.
	freshPump(1, 0);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.kissLastMs = millis() - 100;
	fakeSetTelemetry(&t);
	checkTrue("KISS frames on AUX GP0: refused",
	          pinClaimBegin(PinUser::SbusOut, sbusOutClaimRoles(1, 0)) == ClaimRefusal::KissLive);
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	checkTrue("while it holds: any other user busy",
	          claimToBind(sbusOutClaimRoles(1, 0)) == ClaimState::Running &&
	          pinClaimBegin(PinUser::Sbus, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("back to free", claimEnd());
}

/** @brief @p n UI frames of 25 ms: core1's passes every 500 us, the pump's pass, the UI. */
static void outFrames(int n) {
	for (int i = 0; i < n; i++) {
		for (int k = 0; k < 50; k++) {
			fakeAdvanceUs(500);
			core1JobPoll();
		}
		fakePumpPoll();
		uiTick();
	}
}

#define UI_IDS_FRAMES(n) outFrames(n)
#include "ui_ids.h"

// ---------------------------------------------------------------------------
// the bootloader probe, on rcbench's simulated ESC behind the escwire line
// ---------------------------------------------------------------------------

/** @brief Passes every 500 us for @p ms, the heartbeat stamped. */
static void passMs(uint32_t ms) { runTo(time_us_64() + (uint64_t)ms * 1000u, 500); }

/** @brief Transaction @p i of the line was rcbench's first contact. */
static bool greetingAt(int i) {
	uint8_t c[EW_A_CONTACT_LEN];
	ew_a_contact(c);
	const sim_t *e = fakeEwSim();
	return i < e->starts && e->log[i].n == EW_A_CONTACT_LEN &&
	       memcmp(e->log[i].b, c, sizeof(c)) == 0;
}

/** @brief No write of GP0's control register named the UART since write @p from. */
static bool neverUart(int from) {
	for (int i = from; i < fakeGpioCtrlWrites(0); i++)
		if ((fakeGpioCtrlWrite(0, i).value & IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS) == GPIO_FUNC_UART)
			return false;
	return true;
}

/** @brief GP0 as the probe leaves it: an SIO input, low, on the pull-up. */
static bool gp0Up() {
	FakePin p = fakePin(0);
	return p.function == GPIO_FUNC_SIO && p.outover == GPIO_OVERRIDE_NORMAL && !p.output &&
	       !p.high && p.pull == FakePull::Up;
}

/** @brief An eRPM frame stamped @p ms after the last answer the job read (negative: before it). */
static uint32_t rpmAfterAnswer(int32_t ms) {
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	return st.answerMs + (uint32_t)ms;
}

/** @brief The rule: which replies are an ESC on GP0. */
static void testProbeRule() {
	section("S.BUS output probe: which replies are an ESC on GP0");

	struct Case { const char *what; uint8_t b[9]; int n; bool answer; bool efm8; };
	static const Case CASES[] = {
		{ "silence: none", {0}, 0, false, false },
		{ "Bluejay's EFM8 answer: an ESC, EFM8",
		  {0x34, 0x37, 0x31, 0x64, 0xE8, 0xB2, 0x06, 0x01, 0x30}, 9, true, true },
		{ "rcbench's AM32 answer: an ESC, not EFM8",
		  {0x34, 0x37, 0x31, 0x14, 0x35, 0x06, 0x06, 0x01, 0x30}, 9, true, false },
		{ "an EFM8 answer with its 4 lost: a piece, EFM8",
		  {0x37, 0x31, 0x64, 0xE8, 0xB2, 0x06, 0x01, 0x30}, 8, true, true },
		{ "bytes that are no answer: none",
		  {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, 9, false, false },
		{ "a 0xC2 alone: none", {0xC2}, 1, false, false },
	};
	// The cue and the mark compare these counts with core0's marks, which
	// take their type: 16 bits would read the cue as gone after exactly
	// 65,536 answers.
	checkTrue("the answer counts are 32 bits wide",
	          sizeof(SbusOutStatus::answerSeq) == 4 && sizeof(SbusOutStatus::efm8Seq) == 4);
	for (const Case &c : CASES) {
		bool efm8 = !c.efm8;
		bool answer = sbusOutProbeAnswer(blAnswerClassify(c.b, c.n), &efm8);
		checkTrue(c.what, answer == c.answer && efm8 == c.efm8);
	}
}

/** @brief No ESC on GP0: one cycle, then SAFE; START impossible before it. */
static void testProbeNoAnswer() {
	section("S.BUS output probe: no answer, one cycle, then SAFE");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	noEsc();
	g_beat = true;
	escHeartbeat();
	uint16_t stop = send(SbusOutCmdKind::Safe);
	checkTrue("bound: the probe", claimToBind(PIN_SIG) == ClaimState::Running &&
	          sbusOutProbePoll() == SbusOutProbeState::Checking && !sbusOutProbePassed());
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("the bind takes a STOP held before it as acknowledged", st.ackSeq == stop);
	uint32_t t0 = millis();
	uint16_t early = send(SbusOutCmdKind::Start);
	checkTrue("so a START sent now is held, not dropped behind it", early == (uint16_t)(stop + 1));
	while (millis() - t0 < 999) runTo(time_us_64() + 500, 500);
	sbusOutJobStatus(&st);
	checkTrue("999 ms: quiet, nothing sent, the line on the pull-up",
	          fakeEwSim()->starts == 0 && fakeEwSim()->line == EW_LINE_UP &&
	          fakePin(0).pull == FakePull::Up && fakePin(0).function == GPIO_FUNC_PIO2);
	checkTrue("the START sent in it: not taken", !st.running && st.ackSeq != early);
	while (fakeEwSim()->starts == 0 && millis() - t0 < 1010) runTo(time_us_64() + 500, 500);
	checkInt("the first greeting 1000 ms after the bind", (long)(millis() - t0), 1000);
	checkTrue("rcbench's first contact", greetingAt(0));
	uint16_t late = 0;
	while (fakeEwSim()->starts < 3 && millis() - t0 < 1200) runTo(time_us_64() + 500, 500);
	late = send(SbusOutCmdKind::Start);
	checkTrue("a START during the third greeting: the probe still runs",
	          !sbusOutProbePassed() && sbusOutProbePoll() == SbusOutProbeState::Checking);
	checkTrue("then SAFE", probeToSafe());
	uint32_t passed = millis() - t0;
	sbusOutJobStatus(&st);
	checkInt("three greetings", fakeEwSim()->starts, 3);
	checkTrue("each the first contact", greetingAt(1) && greetingAt(2));
	checkInt("one cycle", (long)st.cycles, 1);
	checkInt("SAFE once the third greeting's wait ended, ms", (long)passed, 1118, 2);
	checkTrue("Passed: the gate open", sbusOutProbePoll() == SbusOutProbeState::Passed &&
	          sbusOutProbePassed() && st.phase == SbusOutPhase::Uart);
	checkTrue("no answer: no cue, no EFM8 mark", !st.answered && !sbusOutRunCue(0) &&
	          !sbusOutProbeEfm8());
	passMs(50);
	sbusOutJobStatus(&st);
	checkTrue("the START sent during the probe: never taken",
	          st.ackSeq == late && !st.running && fakeUartTxCount() == 0);
	send(SbusOutCmdKind::Start);
	checkTrue("a START now: frames", runToFrame(0, 500));
	// STOP and START again: no probe between them.
	send(SbusOutCmdKind::Safe);
	passMs(20);
	send(SbusOutCmdKind::Start);
	runTo(time_us_64() + 500, 500);
	sbusOutJobStatus(&st);
	checkTrue("STOP, START: RUNNING on the next pass, no probe between",
	          st.running && fakeEwOpens() == 1 && fakeEwSim()->starts == 3);
	checkTrue("back to free", claimEnd());
	checkTrue("the gate shut once the job is off", !sbusOutProbePassed() &&
	          sbusOutProbePoll() == SbusOutProbeState::Off);
}

/** @brief An EFM8 bootloader answers: START refused, the cue, the leave page; then unplugged. */
static void testProbeEfm8() {
	section("S.BUS output probe: an EFM8 bootloader answers, then is unplugged");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	fakeEwReset(0xB2, true);
	fakeEwAnswerEach(true);
	g_beat = true;
	escHeartbeat();
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	uint32_t seq0 = st.answerSeq, efm0 = st.efm8Seq;
	int ctrl0 = fakeGpioCtrlWrites(0);
	checkTrue("bound: the probe", claimToBind(PIN_SIG) == ClaimState::Running);
	passMs(1100);
	sbusOutJobStatus(&st);
	checkTrue("its greeting answered", st.answered && st.efm8 && st.signature == 0xE8B2);
	checkTrue("Answered, the cue, the EFM8 mark",
	          sbusOutProbePoll() == SbusOutProbeState::Answered && sbusOutRunCue(0) &&
	          sbusOutProbeEfm8());
	checkTrue("the claim keeps its leave page", pinClaimLeavePage());
	uint16_t start = send(SbusOutCmdKind::Start);
	passMs(2300);
	sbusOutJobStatus(&st);
	checkInt("three cycles", (long)st.cycles, 3);
	checkInt("nine answers counted", st.answerSeq - seq0, 9);
	checkInt("nine of them EFM8", st.efm8Seq - efm0, 9);
	checkTrue("START refused: still the probe, the gate shut",
	          st.phase == SbusOutPhase::Probe && !sbusOutProbePassed() && !st.running &&
	          st.ackSeq != start);
	checkTrue("GP0 never on the UART, nothing sent on it",
	          neverUart(ctrl0) && fakeUartTxCount() == 0 && fakeUart().txWrites == 0);
	// The ESC unplugged: the next cycle with no answer passes.
	fakeEwSim()->dead = true;
	uint32_t cycles = st.cycles;
	checkTrue("unplugged: SAFE", probeToSafe());
	sbusOutJobStatus(&st);
	checkTrue("after a whole cycle with no answer", st.cycles >= cycles + 1 &&
	          st.cycles <= cycles + 2);
	checkTrue("the START sent during the probe: never taken", !st.running);
	send(SbusOutCmdKind::Start);
	checkTrue("a START now: frames", runToFrame(0, 500));
	checkTrue("back to free", claimEnd());
	checkTrue("the claim over: the cue and the EFM8 mark kept",
	          sbusOutRunCue(0) && sbusOutProbeEfm8());
	sbusOutProbeRestart();
	checkTrue("RESTART DSHOT: both gone", !sbusOutRunCue(0) && !sbusOutProbeEfm8());
	fakeEwAnswerEach(false);
}

/** @brief An AM32 bootloader answers: START refused, the cue, no EFM8 step; MENU's strip. */
static void testProbeAm32() {
	section("S.BUS output probe: an AM32 bootloader answers; MENU's cue");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	fakeEwResetAm32();
	fakeEwAnswerEach(true);
	g_beat = true;
	escHeartbeat();
	checkTrue("bound: the probe", claimToBind(PIN_SIG) == ClaimState::Running);
	uint32_t bindMs = millis();
	uint16_t start = send(SbusOutCmdKind::Start);
	passMs(1200);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("answered, AM32: no EFM8 mark", st.answered && !st.efm8 && st.signature == 0 &&
	          !sbusOutProbeEfm8());
	checkTrue("Answered, the cue", sbusOutProbePoll() == SbusOutProbeState::Answered &&
	          sbusOutRunCue(0));
	checkTrue("START refused", !st.running && st.ackSeq != start && !sbusOutProbePassed());
	fakeEwSim()->dead = true;
	checkTrue("unplugged: SAFE", probeToSafe());
	checkTrue("back to free", claimEnd());
	sbusOutProbeRestart();
	checkTrue("RESTART DSHOT after AM32 alone: the cue stays", sbusOutRunCue(0));
	checkTrue("an eRPM frame stamped at the answer: stays", sbusOutRunCue(rpmAfterAnswer(0)));
	checkTrue("one stamped before it: stays", sbusOutRunCue(rpmAfterAnswer(-5)));
	checkTrue("one from before the probe: stays", sbusOutRunCue(bindMs));

	// MENU shows it.
	fakeFlashClear();
	settingsLoad();
	uiInitTester();
	outFrames(2);
	tapId(UI_FIX(UI_BACK));
	outFrames(2);
	checkTrue("MENU: the strip's second line reads POWER-CYCLE THE ESC TO RUN IT",
	          uiCurrentScreen() == UiScreenId::Menu &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_RUN_CUE, UI_COL_WARN, C_BG, 1));
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	// Stamped now, by the clock: after the answer.
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	outFrames(2);
	checkTrue("an eRPM frame after the answer: the cue gone from MENU", !sbusOutRunCue(0) &&
	          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_RUN_CUE, UI_COL_WARN, C_BG, 1));
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	openTester();
	outFrames(1);
	fakeEwAnswerEach(false);
}

/** @brief A piece of an EFM8 answer, bytes that are none, and a greeting that did not prove silence. */
static void testProbeReplies() {
	section("S.BUS output probe: a piece of an answer, noise, a greeting cut or refused");

	// A piece: the EFM8 answer with its leading 4 lost.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	fakeEwReset(0xB2, true);
	fakeEwAnswerEach(true);
	static const uint8_t PIECE[9] = {0x37, 0x31, 0x64, 0xE8, 0xB2, 0x06, 0x01, 0x30, 0x00};
	memcpy(fakeEwSim()->answer, PIECE, sizeof(PIECE));
	g_beat = true;
	checkTrue("bound", claimToBind(PIN_SIG) == ClaimState::Running);
	passMs(1200);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("a piece of an EFM8 answer: Answered, EFM8, no signature",
	          st.answered && st.efm8 && st.signature == 0 && !sbusOutProbePassed() &&
	          sbusOutProbeEfm8() && sbusOutRunCue(0));
	checkTrue("back to free", claimEnd());
	sbusOutProbeRestart();
	checkTrue("RESTART DSHOT: the mark and the cue gone", !sbusOutProbeEfm8() && !sbusOutRunCue(0));

	// Noise: bytes that are no bootloader's answer. No cue, but the cycle
	// repeats: an FC's S.BUS input sends nothing.
	fakeEwReset(0xB2, true);
	fakeEwAnswerEach(true);
	memset(fakeEwSim()->answer, 0x55, sizeof(fakeEwSim()->answer));
	freshPump(0, 1);
	sbusOutJobStatus(&st);
	uint32_t seq0 = st.answerSeq;
	checkTrue("bound", claimToBind(PIN_SIG) == ClaimState::Running);
	passMs(3500);
	sbusOutJobStatus(&st);
	checkTrue("bytes that are none: three cycles, still the probe",
	          st.cycles == 3 && st.phase == SbusOutPhase::Probe && !sbusOutProbePassed());
	checkTrue("no answer counted, no cue, no mark", st.answerSeq == seq0 && !st.answered &&
	          !sbusOutRunCue(0) && !sbusOutProbeEfm8() &&
	          sbusOutProbePoll() == SbusOutProbeState::Checking);
	fakeEwSim()->dead = true;
	checkTrue("silent: SAFE after the next whole cycle", probeToSafe());
	checkTrue("back to free", claimEnd());
	fakeEwAnswerEach(false);

	// The EFM8 model as it is: its first answer, then 0xC2 to each later
	// contact, its reply to a frame whose CRC fails. The 0xC2 hold START too.
	fakeEwReset(0xB2, true);
	freshPump(0, 1);
	sbusOutJobStatus(&st);
	seq0 = st.answerSeq;
	checkTrue("bound", claimToBind(PIN_SIG) == ClaimState::Running);
	passMs(3500);
	sbusOutJobStatus(&st);
	checkTrue("one answer, then 0xC2 only: three cycles, still the probe",
	          st.cycles == 3 && st.answerSeq == seq0 + 1 && !sbusOutProbePassed());
	fakeEwSim()->dead = true;
	checkTrue("unplugged: SAFE", probeToSafe());
	checkTrue("back to free", claimEnd());
	sbusOutProbeRestart();

	// A greeting refused by the driver: that cycle proves nothing, the next
	// passes.
	freshPump(0, 1);
	noEsc();
	fakeEwSim()->fail = F_START_FALSE;
	fakeEwSim()->fail_at = 1;
	fakeEwSim()->fail_n = 1;
	checkTrue("bound", claimToBind(PIN_SIG) == ClaimState::Running);
	checkTrue("SAFE", probeToSafe());
	sbusOutJobStatus(&st);
	checkTrue("the second greeting refused: two cycles, six greetings",
	          st.cycles == 2 && fakeEwSim()->starts == 6 && !st.answered);
	checkTrue("back to free", claimEnd());

	// A capture cut or not whole: the driver parks the pad on its pull-down;
	// the job puts the pull-up back at once, so the line is high on every
	// pass from the bind to SAFE, through the next quiet phase.
	static const uint8_t FAULTS[] = { F_ABORTED, F_OVERFLOW };
	static const char *const KIND[] = { "cut", "not whole" };
	for (int f = 0; f < 2; f++) {
		for (int at = 0; at < 3; at++) {
			freshPump(0, 1);
			noEsc();
			fakeEwSim()->fail = FAULTS[f];
			fakeEwSim()->fail_at = at;
			fakeEwSim()->fail_n = 1;
			checkTrue("bound", claimToBind(PIN_SIG) == ClaimState::Running);
			bool up = true, quietSeen = false;
			uint64_t until = time_us_64() + 3000000;
			while (!sbusOutProbePassed() && time_us_64() < until) {
				fakeAdvanceUs(500);
				escHeartbeat();
				core1JobPoll();
				if (sbusOutProbePassed()) break;
				up &= fakePin(0).pull == FakePull::Up && !fakePin(0).inputOff &&
				      fakeEwSim()->line == EW_LINE_UP;
				sbusOutJobStatus(&st);
				quietSeen |= st.cycles == 1 && fakeEwSim()->starts == 3;
			}
			sbusOutJobStatus(&st);
			char what[96];
			snprintf(what, sizeof(what), "greeting %d's capture %s: two cycles, six greetings",
			         at + 1, KIND[f]);
			checkTrue(what, sbusOutProbePassed() && st.cycles == 2 &&
			          fakeEwSim()->starts == 6 && !st.answered);
			checkTrue("the pull-up on every pass, the next quiet phase among them",
			          up && quietSeen);
			checkTrue("back to free", claimEnd());
		}
	}
}

/** @brief The release mid-probe, BACK TO OUTPUT, a refused open. */
static void testProbeRelease() {
	section("S.BUS output probe: the release mid-probe, a new visit, a refused open");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	noEsc();
	g_beat = true;
	int ctrl0 = fakeGpioCtrlWrites(0);
	checkTrue("bound", claimToBind(PIN_SIG) == ClaimState::Running);
	passMs(500);
	pinClaimEnd(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("BACK in the quiet phase: let go in one pass",
	          core1JobPhase() == Core1JobPhase::Idle && !fakeEwOpen() && fakeEwCloses() == 1);
	FakePin p = fakePin(0);
	FakeUart u = fakeUart();
	checkTrue("GP0 an SIO input, low, on the pull-up: the probe's level", gp0Up());
	checkTrue("never on the UART", neverUart(ctrl0) && fakeUartTxCount() == 0);
	checkTrue("UART0 stopped after the line closed",
	          !u.enabled && u.deinits == 1 && u.deinitAt > p.functionAt);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("the status: off", st.phase == SbusOutPhase::Off && !sbusOutProbePassed());
	pinClaimPoll();
	checkTrue("the leave page is due", pinClaimUnplugDue() && escTaskSuspended());

	// BACK TO OUTPUT: a new visit, and a new probe.
	pinClaimKeep(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("BACK TO OUTPUT: the probe again", pinClaimPoll() == ClaimState::Running &&
	          fakeEwOpens() == 2 && sbusOutProbePoll() == SbusOutProbeState::Checking);
	uint16_t start = send(SbusOutCmdKind::Start);
	uint64_t until = time_us_64() + 1100000;
	while (fakeEwSim()->starts == 0 && time_us_64() < until) runTo(time_us_64() + 500, 500);
	runTo(time_us_64() + 2000, 500);
	sbusOutJobStatus(&st);
	checkTrue("a greeting on the wire, START not taken",
	          fakeEwSim()->running && !st.running && st.ackSeq != start);
	pinClaimEnd(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("BACK mid-greeting: cut, let go in one pass",
	          core1JobPhase() == Core1JobPhase::Idle && !fakeEwSim()->running && !fakeEwOpen());
	checkTrue("GP0 an SIO input on the pull-up, never the UART's", gp0Up() && neverUart(ctrl0));
	pinClaimPoll();
	pinClaimRelease(PinUser::SbusOut);
	fakePumpPoll();
	checkTrue("RESTART DSHOT: free", pinClaimState() == ClaimState::Free);

	// A refused open of the line: nothing driven, the pull-up, RETRY.
	static const uint8_t ERRS[] = { EW_LINE_ERR_PIO, EW_LINE_ERR_DMA, EW_LINE_ERR_CLOCK };
	static const uint8_t JOB[] = { CORE1_JOB_PIO, CORE1_JOB_DMA, CORE1_JOB_CLOCK };
	static const char *const NAMES[] = { "PIO", "DMA", "CLOCK" };
	for (int i = 0; i < 3; i++) {
		fakePwmReset();
		fakeUartReset();
		freshPump(0, 1);
		kissEndState(0);
		gpio_set_dir(0, GPIO_OUT);
		gpio_put(0, 1);
		noEsc();
		fakeEwRefuse(ERRS[i]);
		int ctrlRef = fakeGpioCtrlWrites(0);
		char what[72];
		snprintf(what, sizeof(what), "the line refused (%s): held, its reason", NAMES[i]);
		checkTrue(what, claimToBind(PIN_SIG) == ClaimState::Held && pinClaimJobError() == JOB[i]);
		p = fakePin(0);
		checkTrue("UART0 off again, GP0's control register not written",
		          !fakeUart().enabled && fakeUart().deinits == 1 &&
		          fakeGpioCtrlWrites(0) == ctrlRef);
		checkTrue("SIO driving nothing, the pull-up on", !p.output && !p.high &&
		          p.pull == FakePull::Up);
		fakeEwRefuse(0);
		pinClaimRetry(PinUser::SbusOut);
		core1JobPoll();
		checkTrue("RETRY: the probe", pinClaimPoll() == ClaimState::Running && fakeEwOpen());
		checkTrue("back to free", claimEnd());
	}
}

/** @brief Each visit probes; START only after this visit's probe passed. */
static void testProbeVisits() {
	section("S.BUS output probe: each visit probes again");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	g_beat = true;
	checkTrue("SAFE after the probe", claimToSafe(PIN_SIG) == ClaimState::Running);
	send(SbusOutCmdKind::Start);
	checkTrue("frames", runToFrame(0, 500));
	pinClaimEnd(PinUser::SbusOut);
	passMs(5);
	pinClaimPoll();
	checkTrue("BACK: the leave page", pinClaimUnplugDue());
	checkTrue("the gate shut", !sbusOutProbePassed());
	pinClaimKeep(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("BACK TO OUTPUT: a new probe", pinClaimPoll() == ClaimState::Running &&
	          !sbusOutProbePassed() && fakeEwOpens() == 2);
	int n = fakeUartTxCount();
	// START every 100 ms of the probe: none is taken.
	bool any = false;
	for (int k = 0; k < 11; k++) {
		send(SbusOutCmdKind::Start);
		passMs(100);
		SbusOutStatus st;
		sbusOutJobStatus(&st);
		any |= st.running;
	}
	checkTrue("a START every 100 ms of the probe: none taken", !any && fakeUartTxCount() == n);
	checkTrue("then SAFE", probeToSafe());
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("the last of them not taken either", !st.running && fakeUartTxCount() == n);
	send(SbusOutCmdKind::Start);
	checkTrue("a START now: frames", runToFrame(0, 500));
	checkTrue("back to free", claimEnd());
}

/** @brief Read a file into a string; empty when it cannot be read. */
static std::string readFile(const std::string &path) {
	std::string s;
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return s;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
	fclose(f);
	return s;
}

/**
 * @brief @p text with its comments blanked, line breaks kept: `//` to the
 *        end of the line and block comments, outside string and character
 *        literals. A comment calls nothing.
 */
static std::string stripComments(const std::string &text) {
	std::string out;
	out.reserve(text.size());
	enum { Code, Line, Block, Str, Chr } st = Code;
	for (size_t i = 0; i < text.size(); i++) {
		char ch = text[i], nx = i + 1 < text.size() ? text[i + 1] : '\0';
		switch (st) {
			case Code:
				if (ch == '/' && nx == '/') { st = Line; i++; continue; }
				if (ch == '/' && nx == '*') { st = Block; i++; continue; }
				if (ch == '"') st = Str;
				else if (ch == '\'') st = Chr;
				out += ch;
				break;
			case Line:
				if (ch == '\n') { st = Code; out += ch; }
				break;
			case Block:
				if (ch == '*' && nx == '/') { st = Code; i++; }
				else if (ch == '\n') out += ch;
				break;
			case Str:
			case Chr:
				out += ch;
				if (ch == '\\' && nx) { out += nx; i++; }
				else if ((st == Str && ch == '"') || (st == Chr && ch == '\'')) st = Code;
				break;
		}
	}
	return out;
}

/** @brief @p s without its leading and trailing blanks. */
static std::string trim(const std::string &s) {
	size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

/** @brief Every source file under @p dir, its subdirectories too. */
static void listSources(const std::string &dir, std::vector<std::string> *out) {
	DIR *d = opendir(dir.c_str());
	if (!d) return;
	while (struct dirent *e = readdir(d)) {
		std::string name = e->d_name;
		if (name == "." || name == "..") continue;
		std::string path = dir + "/" + name;
		DIR *sub = opendir(path.c_str());
		if (sub) {
			closedir(sub);
			listSources(path, out);
			continue;
		}
		size_t dot = name.rfind('.');
		std::string ext = dot == std::string::npos ? "" : name.substr(dot);
		if (ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".cmake" || ext == ".pio" ||
		    name == "CMakeLists.txt")
			out->push_back(path);
	}
	closedir(d);
}

/**
 * @brief Whether @p line reaches a pin user or a core1 job by a number
 *        rather than by its name, or walks the tables that hold them.
 *
 * Caught: a cast to PinUser, Core1Job or SbusOutCmdKind, a pointer or
 * reference to one
 * included, C-style, functional, braced, `static_cast` or
 * `reinterpret_cast`, a `const` among them; and any use of the user rows or
 * the job table, `PIN_USERS` or, in core1_job.cpp where it is static,
 * `s_ops`, an index or a range-for over them that picks a row by its fields.
 * Another file's own `s_ops` (bridge_4way.cpp) is not the job table. A
 * name match alone misses `(PinUser)((int)PinUser::Count - 1)`, which is
 * SbusOut, and `(SbusOutCmdKind)1`, which is a Start.
 *
 * Not caught: a value of the enum made without a cast, through `memcpy()`,
 * a union, a template's deduction or bytes read from storage; a cast hidden
 * behind a macro or a typedef of another name. Review has to find those.
 */
static bool byNumber(const std::string &line, const std::string &file) {
	static const std::regex OPS("\\bs_ops\\b");
	static const std::string TABLE = "/core1_job.cpp";
	if (file.size() >= TABLE.size() &&
	    file.compare(file.size() - TABLE.size(), TABLE.size(), TABLE) == 0 &&
	    std::regex_search(line, OPS))
		return true;
	static const std::regex CAST(
		"\\(\\s*(const\\s+)?(PinUser|Core1Job|SbusOutCmdKind)\\s*(const\\s*)?[*&]?\\s*\\)|"
		"\\b(PinUser|Core1Job|SbusOutCmdKind)\\s*[({]|"
		"\\b(static|reinterpret|const)_cast\\s*<\\s*(const\\s+)?(PinUser|Core1Job|SbusOutCmdKind)\\b|"
		"\\bPIN_USERS\\b");
	return std::regex_search(line, CAST);
}

/** @brief The body of the function whose definition is the line @p head in @p code, to its closing `}` at the line's start. */
static std::string bodyOf(const std::string &code, const std::string &head) {
	size_t a = code.find(head);
	if (a == std::string::npos) return std::string();
	size_t b = code.find("\n}", a);
	return b == std::string::npos ? std::string() : code.substr(a, b - a);
}

/** @brief How often @p what occurs in @p text. */
static int countOf(const std::string &text, const std::string &what) {
	int n = 0;
	for (size_t p = text.find(what); p != std::string::npos; p = text.find(what, p + 1)) n++;
	return n;
}

/**
 * @brief Whether @p code has an assignment: an `=` that is not part of `==`,
 *        `!=`, `<=` or `>=`; `<<=` and `>>=` are assignments.
 */
static bool hasAssignment(const std::string &code) {
	for (size_t i = 0; i < code.size(); i++) {
		if (code[i] != '=') continue;
		if (i + 1 < code.size() && code[i + 1] == '=') {
			i++;   // ==
			continue;
		}
		const char p1 = i >= 1 ? code[i - 1] : ' ';
		const char p2 = i >= 2 ? code[i - 2] : ' ';
		if (p1 == '!') continue;                         // !=
		if ((p1 == '<' || p1 == '>') && p2 != p1) continue;   // <= and >=; <<= and >>= are assignments
		return true;
	}
	return false;
}

/**
 * @brief Whether @p ui, ui_sbus_out.cpp with its comments blanked, has one
 *        path to START: the Start only in startFrames(), whose name appears
 *        twice, at its definition and in HOLD TO START's hold on the line
 *        gated by startAllowed(); and startAllowed()'s one return a chain of
 *        `&&` with no `|`, no `?` and no alternative token (`or`, `and`,
 *        `not`, `bitor`, `bitand`, `compl`, `xor`, `and_eq`, `or_eq`, `xor_eq`,
 *        `not_eq`) and no assignment, that ends in sbusOutProbePassed().
 */
static bool onePathToStart(const std::string &ui) {
	const std::string start = bodyOf(ui, "static void startFrames() {");
	if (countOf(start, "send(SbusOutCmdKind::Start);") != 1 || countOf(ui, "SbusOutCmdKind::Start") != 1)
		return false;
	static const std::regex START_NAME("\\bstartFrames\\b");
	const long names = std::distance(std::sregex_iterator(ui.begin(), ui.end(), START_NAME),
	                                 std::sregex_iterator());
	if (names != 2 ||
	    countOf(ui, "if (uiHoldUpdate(&s_hold, BTN_START, s_t, startAllowed(), now)) startFrames();") != 1)
		return false;
	const std::string gate = bodyOf(ui, "static bool startAllowed() {");
	static const std::regex ALT("\\b(or|and|not|bitor|bitand|compl|xor|or_eq|and_eq|xor_eq|not_eq)\\b");
	return countOf(gate, "return ") == 1 && !std::regex_search(gate, ALT) && !hasAssignment(gate) &&
	       countOf(gate, "sbusOutProbePassed()") == 1 && countOf(gate, "|") == 0 && countOf(gate, "?") == 0 &&
	       countOf(gate, "&& sbusOutProbePassed();") == 1;
}

/**
 * @brief Only S.BUS OUTPUT claims the output and commands it, and its one
 *        START runs through the screen's gate, sbusOutProbePassed()
 *        (docs/design/sbus-output.md, decision 1: the probe, and the screen
 *        after it). The behaviour is tested through the real UI in
 *        test_sbus_out_ui.cpp; this test holds the sources to the one path.
 *
 * Every source file under `src/` is read, with its comments blanked, line by
 * line. A line that claims or names the output's user or job, sends a
 * command, names a START, or reaches a user or a job by a number
 * (byNumber()), must be one of the lines below, in the file named, found as
 * often as the list says: a call anywhere else, an inline function in
 * pin_claim.h that claimed the output, or a cast to a user, a job or a
 * command kind, fails. Then, in ui_sbus_out.cpp: the START is sent only from
 * startFrames(), whose name appears twice, at its definition and in HOLD TO
 * START's hold, on a line gated by startAllowed(), whose one return is a
 * chain of `&&` with no `|`, no `?`, none of the 11 alternative tokens (`or`,
 * `and`, `not`, `bitor`, `bitand`, `compl`, `xor`, `or_eq`, `and_eq`,
 * `xor_eq`, `not_eq`) and no assignment, an `=` that is not part of `==`,
 * `!=`, `<=` or `>=`, that ends in sbusOutProbePassed(). A pointer to
 * startFrames, a cast to a command kind, a ternary, an alternative token and
 * each assignment operator (`=`, `^=`, `&=`, `+=`, `-=`, `*=`, `/=`, `%=`,
 * `<<=`, `>>=`; `|=` holds a `|`) in the gate each fail it. It is a lint of
 * the sources, not a proof: a value made without a cast or a name, through
 * memcpy() or a macro, is not caught; the UI tests (test_sbus_out_ui.cpp)
 * hold the behaviour.
 */
static void testOnlyPath() {
	section("S.BUS output: only S.BUS OUTPUT claims and commands it; its START needs the probe's gate");

	struct Allowed { const char *file; const char *line; int times; };
	static const Allowed ALLOWED[] = {
		{ "../src/pin_claim.h", "{ PinUser::SbusOut, Core1Job::SbusOut, true, true, false },", 1 },
		{ "../src/pin_claim.h", "static constexpr PinUserInfo PIN_USERS[] = {", 1 },
		// pinUserInfo() and pinUsersComplete(): each row by its user.
		{ "../src/pin_claim.h", "for (const PinUserInfo &u : PIN_USERS)", 1 },
		{ "../src/pin_claim.h", "return PIN_USERS[0];", 1 },
		{ "../src/pin_claim.h", "for (const PinUserInfo &u : PIN_USERS) n += (int)u.who == w;", 1 },
		{ "../src/pin_claim.h",
		  "static_assert(pinUsersComplete(), \"a pin user has no row in PIN_USERS, or two\");", 1 },
		{ "../src/core1_job.cpp", "static const Core1JobOps *s_ops[] = {", 1 },
		{ "../src/core1_job.cpp", "&SBUS_OUT_JOB_OPS,", 1 },
		{ "../src/core1_job.cpp",
		  "static_assert(sizeof(s_ops) / sizeof(s_ops[0]) == (unsigned)Core1Job::SbusOut + 1,", 1 },
		// core1JobInit(): each job's init, no bind.
		{ "../src/core1_job.cpp", "for (const Core1JobOps *ops : s_ops)", 1 },
		{ "../src/core1_job.cpp", "if (s_ops[(uint8_t)job] == nullptr) return false;", 1 },
		{ "../src/core1_job.cpp", "const Core1JobOps *ops = s_ops[(uint8_t)s_job];", 1 },
		{ "../src/sbus_out_job.h", "uint16_t sbusOutJobCommand(const SbusOutCmd *c);", 1 },
		{ "../src/sbus_out_job.h", "extern const Core1JobOps SBUS_OUT_JOB_OPS;", 1 },
		{ "../src/sbus_out_job.cpp", "const Core1JobOps SBUS_OUT_JOB_OPS = {", 1 },
		{ "../src/sbus_out_job.cpp", "uint16_t sbusOutJobCommand(const SbusOutCmd *c) {", 1 },
		// Core1 taking a START, in the pure core.
		{ "../src/sbus_out_job.cpp", "case SbusOutCmdKind::Start:", 1 },
		// An EFM8 answer to the probe asks for the leave page: it lets go of
		// nothing and starts nothing.
		{ "../src/sbus_out_job.cpp",
		  "if (st.efm8Seq != s_efm8Mark) pinClaimWantLeavePage(PinUser::SbusOut);", 1 },
		// S.BUS OUTPUT: its claim, its one command call, its one START.
		{ "../src/ui_sbus_out.cpp", "s_refusal = pinClaimBegin(PinUser::SbusOut, s_roles);", 1 },
		{ "../src/ui_sbus_out.cpp", "pinClaimEnd(PinUser::SbusOut);", 2 },
		{ "../src/ui_sbus_out.cpp", "pinClaimRelease(PinUser::SbusOut);", 1 },
		{ "../src/ui_sbus_out.cpp", "pinClaimKeep(PinUser::SbusOut);", 1 },
		{ "../src/ui_sbus_out.cpp", "if (retry && tapped(BTN_RETRY)) pinClaimRetry(PinUser::SbusOut);", 1 },
		{ "../src/ui_sbus_out.h", "UiScreenId::SbusOut, SBOUT_UI_TITLE, PinUser::SbusOut, nullptr,", 1 },
		{ "../src/ui_sbus_out.cpp", "sbusOutJobCommand(&c);", 1 },
		{ "../src/ui_sbus_out.cpp", "send(SbusOutCmdKind::Start);", 1 },
	};

	static const char *const NAMES[] = {
		"PinUser::SbusOut", "Core1Job::SbusOut", "sbusOutJobCommand", "SBUS_OUT_JOB_OPS",
		"SbusOutCmdKind::Start",
	};
	std::vector<std::string> files;
	listSources("../src", &files);
	files.push_back("../CMakeLists.txt");
	bool deep = false;
	for (const std::string &f : files) deep |= f.find("../src/sdio/compat/") == 0;
	int used[sizeof(ALLOWED) / sizeof(ALLOWED[0])] = {};
	std::string hits;
	for (const std::string &f : files) {
		std::string code = stripComments(readFile(f));
		size_t pos = 0;
		int lineNo = 0;
		while (pos <= code.size()) {
			size_t nl = code.find('\n', pos);
			if (nl == std::string::npos) nl = code.size();
			std::string line = trim(code.substr(pos, nl - pos));
			pos = nl + 1;
			lineNo++;
			bool names = byNumber(line, f);
			for (const char *n : NAMES) names |= line.find(n) != std::string::npos;
			if (!names) continue;
			bool ok = false;
			for (size_t a = 0; a < sizeof(ALLOWED) / sizeof(ALLOWED[0]); a++)
				if (f == ALLOWED[a].file && line == ALLOWED[a].line) { ok = true; used[a]++; }
			if (!ok) hits += " " + f + ":" + std::to_string(lineNo);
		}
	}
	checkTrue("the sources were read, subdirectories too", files.size() > 100 && deep);
	checkTrue("no other line names it, sends a START or reaches it by a number", hits.empty());
	if (!hits.empty()) printf("   %s\n", hits.c_str());
	bool each = true;
	for (size_t a = 0; a < sizeof(ALLOWED) / sizeof(ALLOWED[0]); a++) each &= used[a] == ALLOWED[a].times;
	checkTrue("each allowed line found as often as listed: the list is current", each);

	// The one START in S.BUS OUTPUT, behind the gate.
	const std::string ui = stripComments(readFile("../src/ui_sbus_out.cpp"));
	checkTrue("the START is sent in startFrames(), named only at its definition and in HOLD TO START's "
	          "hold, gated by startAllowed(), one chain ending in sbusOutProbePassed()",
	          onePathToStart(ui));
	// The same check on screens that bypass it.
	const std::string defs = "static bool startAllowed() {\n\treturn !s_st.running && sbusOutProbePassed();\n}\n"
	                         "static void startFrames() {\n\tsend(SbusOutCmdKind::Start);\n}\n";
	const std::string hold = "\tif (uiHoldUpdate(&s_hold, BTN_START, s_t, startAllowed(), now)) startFrames();\n";
	checkTrue("a screen of the gate, the START and the hold passes it", onePathToStart(defs + hold));
	static const char *const BYPASS[] = {
		"\tif (x) { void (*go)() = startFrames; go(); }\n",
		"\tif (x) startFrames();\n",
		"\tauto go = &startFrames;\n",
		"\tsend(SbusOutCmdKind::Start);\n",
	};
	bool caught = true;
	for (const char *b : BYPASS) caught &= !onePathToStart(defs + hold + b);
	checkTrue("a pointer to startFrames, a second call and a second START each fail it", caught);
	static const char *const GATES[] = {
		"\treturn true ? true : !s_st.running && sbusOutProbePassed();",
		"\treturn !s_st.running || true && sbusOutProbePassed();",
		"\treturn !s_st.running | 1 && sbusOutProbePassed();",
		"\treturn !s_st.running or 1 && sbusOutProbePassed();",
		"\treturn !s_st.running and 1 && sbusOutProbePassed();",
		"\treturn not s_st.running && sbusOutProbePassed();",
		"\treturn !s_st.running bitor 1 && sbusOutProbePassed();",
		"\treturn !s_st.running bitand 1 && sbusOutProbePassed();",
		"\treturn compl s_st.running && sbusOutProbePassed();",
		"\treturn !s_st.running xor 1 && sbusOutProbePassed();",
		"\treturn s_ok or_eq 1 && sbusOutProbePassed();",
		"\treturn s_ok and_eq 1 && sbusOutProbePassed();",
		"\treturn s_ok xor_eq 1 && sbusOutProbePassed();",
		"\treturn s_ok not_eq 1 && sbusOutProbePassed();",
		"\treturn s_ok = 1 && sbusOutProbePassed();",
		"\treturn s_ok ^= 1 && sbusOutProbePassed();",
		"\treturn s_ok |= 1 && sbusOutProbePassed();",
		"\treturn s_ok &= 1 && sbusOutProbePassed();",
		"\treturn s_ok += 1 && sbusOutProbePassed();",
		"\treturn s_ok -= 1 && sbusOutProbePassed();",
		"\treturn s_ok *= 1 && sbusOutProbePassed();",
		"\treturn s_ok /= 1 && sbusOutProbePassed();",
		"\treturn s_ok %= 1 && sbusOutProbePassed();",
		"\treturn s_ok <<= 1 && sbusOutProbePassed();",
		"\treturn s_ok >>= 1 && sbusOutProbePassed();",
		"\treturn !s_st.running && !sbusOutProbePassed();",
		"\tif (s_st.running) return true;\n\treturn !s_st.running && sbusOutProbePassed();",
	};
	caught = true;
	for (const char *g : GATES) {
		bool failed = !onePathToStart(std::string("static bool startAllowed() {\n") + g + "\n}\n"
		                              "static void startFrames() {\n\tsend(SbusOutCmdKind::Start);\n}\n" + hold);
		if (!failed) printf("   passes the lint: %s\n", g);
		caught &= failed;
	}
	checkTrue("a ternary, an || or a |, each of the 11 alternative tokens, each assignment, a negation "
	          "and a second return in the gate each fail it", caught);
	// Comparisons are not assignments.
	checkTrue("a gate with ==, !=, <= and >= passes it",
	          onePathToStart(std::string("static bool startAllowed() {\n\treturn s_st.n >= 1 && s_st.n <= 5 && "
	                                     "s_st.n != 3 && s_st.n == 2 && sbusOutProbePassed();\n}\n"
	                                     "static void startFrames() {\n\tsend(SbusOutCmdKind::Start);\n}\n") + hold));
	checkTrue("a call in a comment is blanked, one in code is not",
	          stripComments("x(); // pinClaimBegin(PinUser::SbusOut)\n/* a */ y(\"//\");")
	              == "x(); \n y(\"//\");");
	// The matcher on the ways to a user or a job by a number.
	static const char *const BY_NUMBER[] = {
		"pinClaimBegin((PinUser)((int)PinUser::Count - 1), 1);",
		"core1JobStart((Core1Job)7, 0, 0xFF);",
		"core1JobStart(( Core1Job )7, 0, 0xFF);",
		"PinUser u = PinUser(7);",
		"auto j = Core1Job (7);",
		"PinUser u = PinUser{7};",
		"auto u = static_cast<PinUser>(7);",
		"auto j = static_cast< Core1Job >(n);",
		"auto j = reinterpret_cast<Core1Job>(n);",
		"pinClaimBegin(PIN_USERS[7].who, 1);",
		"s_ops[7]->bind(0, 1);",
		"pinClaimBegin(*(PinUser *)&n, 1);",
		"pinClaimBegin(*( const PinUser* )p, 1);",
		"auto u = *reinterpret_cast<PinUser *>(&n);",
		"auto u = *reinterpret_cast<const PinUser*>(p);",
		"for (const PinUserInfo &u : PIN_USERS) if (u.unplug && u.escCheck && !u.deferJob) pinClaimBegin(u.who, 1);",
		"for (const Core1JobOps *o : s_ops) o->bind(0, 1);",
		"send((SbusOutCmdKind)1);",
		"auto k = static_cast<SbusOutCmdKind>(1);",
		"c.kind = SbusOutCmdKind(1);",
		"c.kind = SbusOutCmdKind{1};",
	};
	bool all = true;
	for (const char *l : BY_NUMBER) all &= byNumber(l, "../src/core1_job.cpp");
	checkTrue("each cast and index to a user or a job is caught", all);
	static const char *const BY_NAME[] = {
		"static constexpr const PinUserInfo &pinUserInfo(PinUser who) {",
		"enum class PinUser : uint8_t {",
		"enum class Core1Job : uint8_t {",
		"bool core1JobStart(Core1Job job, uint8_t sigGpio, uint8_t auxGpio);",
		"Core1Job core1JobRunning() {",
		"const PinUserInfo &u = pinUserInfo(who);",
		"uint8_t n = (uint8_t)job;",
		"static void send(SbusOutCmdKind kind) {",
		"enum class SbusOutCmdKind : uint8_t {",
	};
	bool none = true;
	for (const char *l : BY_NAME) none &= !byNumber(l, "../src/core1_job.cpp");
	checkTrue("a type named, a parameter or a loop is not", none);
	checkTrue("another file's own s_ops is not the job table",
	          !byNumber("const Op *o = &s_ops[s_op];", "../src/bridge_4way.cpp") &&
	          byNumber("const Op *o = &s_ops[s_op];", "../src/core1_job.cpp"));
}

void runSbusOutJobTests() {
	FakeWiring w = fakeWiring();
	testHelpers();
	testCoreStart();
	testCoreClamp();
	testCoreSchedule();
	testCoreStops();
	testJobBind();
	testJobLine();
	testJobCeiling();
	testJobRelease();
	testJobHeartbeat();
	testJobWirings();
	testProbeRule();
	testProbeNoAnswer();
	testProbeEfm8();
	testProbeAm32();
	testProbeReplies();
	testProbeRelease();
	testProbeVisits();
	testOnlyPath();
	g_beat = true;
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakeUartReset();
	fakePwmReset();
	freshPump(w.dshotPin, w.kissPin, w.kissEnable);
	escTaskConfigure(w.dshotPin, w.dshotKbaud, w.kissEnable, w.kissPin);
}
