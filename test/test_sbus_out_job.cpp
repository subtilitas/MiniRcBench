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
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "sbus_out_job.h"
#include "plat.h"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/structs/pads_bank0.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

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
	          !sbusOutCoreTake(&c, &old, 100000, true) && !c.running);
	SbusOutCmd fr = cmdOf(SbusOutCmdKind::Frame, 8, 1700);
	fr.ch17 = fr.failsafe = true;
	checkTrue("a Frame while SAFE is taken", sbusOutCoreTake(&c, &fr, 100000, true));
	checkTrue("and moves nothing", !c.running && c.ackSeq == 8 &&
	                               !sbusOutCoreStep(&c, 100000, true, true, out));
	sbus_frame_t start;
	sbusOutStartFrame(&start, SBUS_OUT_THR_DEFAULT);
	checkTrue("the frame core1 holds is still the START frame",
	          memcmp(&c.frame, &start, sizeof(start)) == 0);

	for (uint8_t thr = 0; thr < 4; thr++) {
		sbusOutCoreReset(&c, 0);
		SbusOutCmd f = cmdOf(SbusOutCmdKind::Frame, 1, 1811);
		sbusOutCoreTake(&c, &f, 0, true);
		SbusOutCmd s = cmdOf(SbusOutCmdKind::Start, 2, 1811, thr, 1811);
		s.ch17 = s.ch18 = s.failsafe = s.frameLost = true;
		checkTrue("Start taken", sbusOutCoreTake(&c, &s, 1000, true) && c.running);
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
	checkTrue("Start with the heartbeat lapsed: taken", sbusOutCoreTake(&c, &s, 0, false));
	checkTrue("and refused: SAFE", !c.running && c.ackSeq == 1 &&
	                               !sbusOutCoreStep(&c, 0, true, true, out));
	s = cmdOf(SbusOutCmdKind::Start, 2, 992, 4);
	sbusOutCoreTake(&c, &s, 0, true);
	checkTrue("Start with throttle index 4: refused", !c.running && c.ackSeq == 2);
	s = cmdOf(SbusOutCmdKind::Start, 3, 992, 3);
	sbusOutCoreTake(&c, &s, 0, true);
	checkTrue("index 3: taken", c.running && c.thrCh == 3);
	s.seq = 3;
	s.kind = SbusOutCmdKind::Safe;
	checkTrue("a Safe of the taken seq: not new", !sbusOutCoreTake(&c, &s, 0, true) && c.running);
	s.seq = 4;
	sbusOutCoreTake(&c, &s, 0, true);
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
		sbusOutCoreTake(&c, &s, 0, true);
		bool ok = true;
		uint16_t seq = 2;
		for (uint16_t raw : RAW) {
			SbusOutCmd f = cmdOf(SbusOutCmdKind::Frame, seq++, raw, (uint8_t)((thr + 1) % 4), 1811);
			sbusOutCoreTake(&c, &f, 0, true);
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
	sbusOutCoreTake(&c, &s, 0, true);
	checkInt("a ceiling above 1811 is taken as 1811", c.ceilRaw, 1811);
	SbusOutCmd f = cmdOf(SbusOutCmdKind::Frame, 2, 2047);
	sbusOutCoreTake(&c, &f, 0, true);
	checkInt("the throttle at 2047 sends 1811", c.frame.channel[2], 1811);
	s = cmdOf(SbusOutCmdKind::Start, 3, 992, 2, 100);
	sbusOutCoreTake(&c, &s, 0, true);
	checkInt("a ceiling below 172 is taken as 172", c.ceilRaw, 172);
	f = cmdOf(SbusOutCmdKind::Frame, 4, 900);
	sbusOutCoreTake(&c, &f, 0, true);
	checkInt("the throttle stays at 172", c.frame.channel[2], 172);
	checkInt("the others move", c.frame.channel[0], 900);
	f = cmdOf(SbusOutCmdKind::Frame, 5, 900, 2, 1811);
	f.ch17 = true;
	f.frameLost = true;
	sbusOutCoreTake(&c, &f, 0, true);
	checkTrue("a Frame's ceiling is ignored", c.frame.channel[2] == 172);
	checkTrue("its flags taken", c.frame.ch17 && !c.frame.ch18 && c.frame.frame_lost &&
	                             !c.frame.failsafe);
	f.ch17 = false;
	f.ch18 = true;
	f.failsafe = true;
	f.frameLost = false;
	f.seq = 6;
	sbusOutCoreTake(&c, &f, 0, true);
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
	sbusOutCoreTake(&c, &s, 1000, true);
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
	sbusOutCoreTake(&c, &s, 0, true);
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
	sbusOutCoreTake(&c, &s, 0, true);
	coreRun(&c, 0, 1, 1);
	checkTrue("a pass one period late sends", sbusOutCoreStep(&c, 28000, true, true, out));
	at = coreRun(&c, 28500, 43000, 500);
	checkTrue("the next a period from it", at.size() == 1 && at[0] == 42000);

	// A pass 13 ms late: the grid holds, but never under 7 ms apart.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true);
	coreRun(&c, 0, 1, 1);
	checkTrue("a pass 13 ms late sends", sbusOutCoreStep(&c, 27000, true, true, out));
	at = coreRun(&c, 27500, 56500, 500);
	checkTrue("the next 7 ms on, not at 28 ms: then the grid",
	          at.size() == 3 && at[0] == 34000 && at[1] == 42000 && at[2] == 56000);
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true);
	coreRun(&c, 0, 1, 1);
	sbusOutCoreStep(&c, 27000, true, true, out);
	at = coreRun(&c, 27001, 34002, 1);
	checkTrue("loaded at 27,000 us: the next at 34,000 us, not 33,999 us",
	          at.size() == 1 && at[0] == 34000);

	// The FIFO not yet empty at the deadline.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true);
	checkTrue("FIFO not empty at the deadline: waits", !sbusOutCoreStep(&c, 0, true, false, out));
	checkTrue("still not: waits", !sbusOutCoreStep(&c, 500, true, false, out));
	checkTrue("empty: sends", sbusOutCoreStep(&c, 1000, true, true, out));
	at = coreRun(&c, 1500, 15000, 500);
	checkTrue("the next on the grid of the deadline", at.size() == 1 && at[0] == 14000);

	// Across the micros() wrap.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0xFFFFC000u, true);
	at = coreRun(&c, 0xFFFFC000u, 0xFFFFC000u + 5 * 14000, 500);
	grid = at.size() == 5;
	for (size_t i = 0; grid && i < at.size(); i++) grid = at[i] == 0xFFFFC000u + (uint32_t)i * 14000;
	checkTrue("five frames across the wrap, on the grid", grid);

	// STOP and START again soon: 7 ms after the last frame.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true);
	coreRun(&c, 0, 1, 1);
	SbusOutCmd st = cmdOf(SbusOutCmdKind::Safe, 2);
	sbusOutCoreTake(&c, &st, 2000, true);
	SbusOutCmd again = cmdOf(SbusOutCmdKind::Start, 3);
	sbusOutCoreTake(&c, &again, 3000, true);
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
	sbusOutCoreTake(&c, &s, 0, true);
	sbusOutCoreStep(&c, 0, true, true, out);
	checkTrue("a lapsed heartbeat while RUNNING: no frame", !sbusOutCoreStep(&c, 14000, false, true, out));
	checkTrue("SAFE, tripped", !c.running && c.tripped);
	std::vector<uint32_t> at = coreRun(&c, 14500, 60000, 500);
	checkTrue("the heartbeat back: still nothing, still tripped", at.empty() && c.tripped);
	SbusOutCmd lapsed = cmdOf(SbusOutCmdKind::Start, 2);
	sbusOutCoreTake(&c, &lapsed, 60000, false);
	checkTrue("Start while lapsed: refused, the trip kept", !c.running && c.tripped);
	SbusOutCmd ok = cmdOf(SbusOutCmdKind::Start, 3);
	sbusOutCoreTake(&c, &ok, 60500, true);
	checkTrue("Start: RUNNING, the trip cleared", c.running && !c.tripped);
	sbusOutCoreReset(&c, 0);
	checkTrue("a lapse while SAFE trips nothing",
	          !sbusOutCoreStep(&c, 0, false, true, out) && !c.tripped);

	// A frame the encoder refuses: only by a channel the clamp did not make.
	sbusOutCoreReset(&c, 0);
	sbusOutCoreTake(&c, &s, 0, true);
	c.frame.channel[9] = 2048;
	memset(out, 0xAA, sizeof(out));
	checkTrue("a channel at 2048: no frame", !sbusOutCoreStep(&c, 0, true, true, out));
	bool untouched = true;
	for (uint8_t b : out) untouched &= b == 0xAA;
	checkTrue("SAFE, refused, the bytes unwritten", !c.running && c.refused && untouched);
	checkInt("nothing counted", c.frames, 0);
	SbusOutCmd again = cmdOf(SbusOutCmdKind::Start, 2);
	sbusOutCoreTake(&c, &again, 100, true);
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
	escTaskConfigure(sig, 600, kiss, aux);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
}

/** @brief GP0 as the DShot teardown leaves it: an SIO input with the pull-up (out_dshot.c). */
static void teardownState(unsigned gpio) {
	gpio_set_function(gpio, GPIO_FUNC_SIO);
	gpio_pull_up(gpio);
	gpio_set_dir(gpio, GPIO_IN);
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
	pinClaimRestart(PinUser::SbusOut);
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

/** @brief The default wiring: the bind, register by register. */
static void testJobBind() {
	section("S.BUS output job: the bind, register by register");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	// The pad isolated with its output disabled, as at reset.
	pads_bank0_hw->io[0].value |= PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS;
	FakePin gp1 = fakePin(1);
	int ctrl0 = fakeGpioCtrlWrites(0);
	checkTrue("the claim runs the shipped job",
	          claimToBind(sbusOutClaimRoles(0, 1)) == ClaimState::Running &&
	          core1JobRunning() == Core1Job::SbusOut);
	FakePin p = fakePin(0);
	FakeUart u = fakeUart();
	checkTrue("the pull-down on GP0", p.pull == FakePull::Down);
	checkTrue("the pull-down before uart_init()", p.pullAt != 0 && p.pullAt < u.initAt);
	checkInt("100,000 baud from 150 MHz", u.baud, 100000);
	checkTrue("8E2", u.dataBits == 8 && u.stopBits == 2 && u.parity == UART_PARITY_EVEN);
	checkTrue("then the receiver off: RXE clear, TXE and UARTEN set",
	          u.formatAt > u.initAt && u.crAt > u.formatAt && !u.rxe && u.txe && u.enabled);
	checkInt("one write of GP0's control register", fakeGpioCtrlWrites(0) - ctrl0, 1);
	FakeCtrlWrite w = fakeGpioCtrlWrite(0, ctrl0);
	checkInt("FUNCSEL UART and OUTOVER inverted in that write", w.value, CTRL_UART_INV);
	checkTrue("after the UART is set up", w.at > u.crAt);
	checkTrue("the pad first: IE on, OD off", p.padMaskAt != 0 && p.padMaskAt < w.at);
	checkTrue("then isolation cleared", p.padIsoAt > w.at);
	uint32_t pad = fakePad(0);
	checkTrue("the pad: IE set, OD and ISO clear",
	          (pad & PADS_BANK0_GPIO0_IE_BITS) &&
	          !(pad & (PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS)));
	checkTrue("GP0 never on the UART without the inversion", neverUartPlain());
	checkTrue("GP1 untouched", samePin(fakePin(1), gp1) && fakeGpioCtrlWrites(1) == 0);
	SbusOutStatus st;
	sbusOutJobStatus(&st);
	checkTrue("SAFE: not running, nothing sent", !st.running && !st.tripped && st.frames == 0);
	runTo(time_us_64() + 100000, 500);
	checkInt("SAFE for 100 ms: no character on the wire", fakeUartTxCount(), 0);
	checkTrue("GP0 still bound, inverted", fakePin(0).ctrl == CTRL_UART_INV);
	checkTrue("back to free", claimEnd());
	checkTrue("GP0 let go low", gp0Released());
	checkTrue("still never on the UART without the inversion", neverUartPlain());

	// A command sent before the bind is not taken.
	freshPump(0, 1);
	send(SbusOutCmdKind::Start);
	checkTrue("a START sent before the claim", claimToBind(PIN_SIG) == ClaimState::Running);
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
	checkTrue("running", claimToBind(PIN_SIG) == ClaimState::Running);
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
	checkTrue("running", claimToBind(PIN_SIG) == ClaimState::Running);
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
	pinClaimRestart(PinUser::SbusOut);
	fakePumpPoll();
	checkTrue("RESTART DSHOT: free", pinClaimState() == ClaimState::Free);
	checkTrue("the job was idle before DShot resumed", fakeResumeJobIdle());

	// A transmitter that never sends: the next frame waits for TXFE.
	freshPump(0, 1);
	checkTrue("running", claimToBind(PIN_SIG) == ClaimState::Running);
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
	checkTrue("running", claimToBind(PIN_SIG) == ClaimState::Running);
	pinClaimEnd(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("SAFE: released in one pass", core1JobPhase() == Core1JobPhase::Idle &&
	                                        gp0Released());
	pinClaimPoll();
	pinClaimRestart(PinUser::SbusOut);
	fakePumpPoll();

	// A BUSY that never clears: the backstop, 10 ms after the first pass.
	freshPump(0, 1);
	checkTrue("running", claimToBind(PIN_SIG) == ClaimState::Running);
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
	pinClaimRestart(PinUser::SbusOut);
	fakePumpPoll();
	checkTrue("free", pinClaimState() == ClaimState::Free);
}

/** @brief The heartbeat through the job, and a new visit. */
static void testJobHeartbeat() {
	section("S.BUS output job: the heartbeat trip at 250 ms, a new visit");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	checkTrue("running", claimToBind(PIN_SIG) == ClaimState::Running);
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
	checkTrue("a new visit", claimToBind(PIN_SIG) == ClaimState::Running);
	sbusOutJobStatus(&st);
	checkTrue("starts SAFE with no trip", !st.running && !st.tripped && !st.refused &&
	                                      st.frames == 0);
	checkTrue("back to free", claimEnd());
}

/** @brief Each wiring, NO GP0, the clock, the ESC check, KISS. */
static void testJobWirings() {
	section("S.BUS output job: each wiring, NO GP0, the clock, the ESC check");

	// ESC GP1, KISS GP0: GP0 is AUX.
	fakePwmReset();
	fakeUartReset();
	freshPump(1, 0);
	teardownState(0);
	FakePin gp1 = fakePin(1);
	checkTrue("SIG GP1, AUX GP0: runs with SIG and AUX",
	          claimToBind(sbusOutClaimRoles(1, 0)) == ClaimState::Running);
	checkTrue("it binds GP0", fakePin(0).ctrl == CTRL_UART_INV);
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
	checkTrue("SIG GP0, AUX GP0: runs with SIG", claimToBind(sbusOutClaimRoles(0, 0)) == ClaimState::Running);
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
	pinClaimRestart(PinUser::SbusOut);
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
	checkTrue("SIG GP0 and AUX GP1 both asked: runs", claimToBind(PIN_SIG | PIN_AUX) == ClaimState::Running);
	checkTrue("GP1 untouched", samePin(fakePin(1), gp1) && fakeGpioCtrlWrites(1) == 0);
	checkTrue("back to free", claimEnd());

	// clk_peri at 1 MHz: the divisors give 62,500 baud.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	teardownState(0);
	gpio_set_dir(0, GPIO_OUT);
	gpio_put(0, 1);
	fakeClockSet(clk_peri, 1000000u);
	checkTrue("clk_peri 1 MHz: held, not running", claimToBind(PIN_SIG) == ClaimState::Held);
	checkInt("CLOCK", pinClaimJobError(), CORE1_JOB_CLOCK);
	checkTrue("UART0 off again", !fakeUart().enabled && fakeUart().deinits == 1);
	checkTrue("GP0 never on the UART", fakePin(0).function != GPIO_FUNC_UART && neverUartPlain());
	bool uartEver = false;
	for (int i = 0; i < fakeGpioCtrlWrites(0); i++)
		uartEver |= (fakeGpioCtrlWrite(0, i).value & IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS) == GPIO_FUNC_UART;
	checkTrue("no control write named the UART", !uartEver);
	checkTrue("GP0 left a low input", gp0Released());
	fakeClockSet(clk_peri, 150000000u);
	pinClaimRetry(PinUser::SbusOut);
	core1JobPoll();
	checkTrue("RETRY at 150 MHz: running", pinClaimPoll() == ClaimState::Running &&
	                                       fakePin(0).ctrl == CTRL_UART_INV);
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
 * @brief Nothing in `src/` but the job, its table and the claim's row names
 *        the output, so no screen and no path can claim or command it
 *        (docs/design/sbus-output.md, decision 1: the probe first).
 */
static void testUnreachable() {
	section("S.BUS output: nothing outside the job, the job table and the claim names it");

	static const char *const ALLOWED[] = {
		"sbus_out_job.h", "sbus_out_job.cpp", "core1_job.h", "core1_job.cpp", "pin_claim.h",
	};
	static const char *const NAMES[] = {
		"PinUser::SbusOut", "Core1Job::SbusOut", "sbusOutJobCommand", "SBUS_OUT_JOB_OPS",
		"sbusOutClaimRoles",
	};
	std::vector<std::string> dirs = { "../src", "../src/dshot", "../src/sdio" };
	int files = 0;
	std::string hits;
	for (const std::string &dir : dirs) {
		DIR *d = opendir(dir.c_str());
		if (!d) continue;
		while (struct dirent *e = readdir(d)) {
			std::string name = e->d_name;
			size_t dot = name.rfind('.');
			if (dot == std::string::npos) continue;
			std::string ext = name.substr(dot);
			if (ext != ".c" && ext != ".cpp" && ext != ".h" && ext != ".cmake" && ext != ".pio")
				continue;
			files++;
			bool allowed = false;
			for (const char *a : ALLOWED) allowed |= dir == "../src" && name == a;
			if (allowed) continue;
			std::string text = readFile(dir + "/" + name);
			for (const char *n : NAMES)
				if (text.find(n) != std::string::npos) hits += " " + name + ":" + n;
		}
		closedir(d);
	}
	checkTrue("the sources were read", files > 100);
	checkTrue("no other source names it", hits.empty());
	if (!hits.empty()) printf("   %s\n", hits.c_str());
	std::string cmake = readFile("../CMakeLists.txt");
	checkTrue("nor CMakeLists.txt", !cmake.empty() && cmake.find("SbusOut") == std::string::npos);
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
	testJobRelease();
	testJobHeartbeat();
	testJobWirings();
	testUnreachable();
	g_beat = true;
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakeUartReset();
	fakePwmReset();
	freshPump(w.dshotPin, w.kissPin, w.kissEnable);
	escTaskConfigure(w.dshotPin, w.dshotKbaud, w.kissEnable, w.kissPin);
}
