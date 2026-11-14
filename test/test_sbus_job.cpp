/**
 * @file test_sbus_job.cpp
 * @brief The S.BUS monitor's core1 job: the pure core, and the job as it
 *        ships on the pin claim and the UART0 fake.
 *
 * The core runs on UARTDR words from the line model of sbus_line.h, each fed
 * at the time its byte is in. The job runs from the job table, through the
 * pin claim and the fake DShot pump, on the UART0 fake of fakes.cpp, which
 * hears the line only while UART0 is on, GP1 is on the UART function with
 * its input inverted and the format is 8 data bits with even parity. The
 * test plays core1's side by calling core1JobPoll() itself, one pass every
 * so many microseconds of the virtual clock. What the PL011 and the pad do
 * on the chip is not tested here; the inversion on the pin is not measured.
 */

#include "check.h"
#include "fakes.h"
#include "sbus_line.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "sbus_job.h"
#include "plat.h"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <string.h>

// ---------------------------------------------------------------------------
// the core
// ---------------------------------------------------------------------------

/**
 * @brief Feed @p f to @p c, each byte at the time the PL011 loads it, and end
 *        the pass at the last byte's millisecond.
 */
static void feedFrame(SbusCore *c, const SbusLineFrame &f, uint32_t startUs) {
	uint8_t b[SBUS_FRAME_BYTES];
	sbusLinePack(f, b);
	for (int i = 0; i < f.bytes; i++) {
		SbusFault fault = i == f.faultAt ? f.fault : SbusFault::None;
		sbusCoreFeed(c, sbusLineWord(b[i], fault),
		             startUs + (uint32_t)i * SBUS_LINE_BYTE_US + SBUS_LINE_LOAD_US);
	}
}

/** @brief @p n frames of @p f, one every @p periodUs from @p startUs. */
static uint32_t feedStream(SbusCore *c, const SbusLineFrame &f, uint32_t startUs,
                           uint32_t periodUs, int n) {
	for (int i = 0; i < n; i++) feedFrame(c, f, startUs + (uint32_t)i * periodUs);
	return startUs + (uint32_t)n * periodUs;
}

/** @brief The channels of the status equal those of @p f. */
static bool channelsAre(const SbusStatus &st, const SbusLineFrame &f) {
	return memcmp(st.channel, f.ch, sizeof(st.channel)) == 0;
}

/** @brief The line model against the decoder's own unpacking. */
static void testLineModel() {
	section("S.BUS line model: packing and the PL011's flags");

	uint8_t b[SBUS_FRAME_BYTES];
	SbusLineFrame f = sbusLineFrame(172);
	f.ch[15] = 2047;
	f.flags = SBUS_FLAG_FAILSAFE;
	sbusLinePack(f, b);
	sbus_decoder_t d;
	sbus_decoder_reset(&d);
	sbus_frame_t out;
	bool got = false;
	for (unsigned i = 0; i < SBUS_FRAME_BYTES; i++)
		got = sbus_decode_byte(&d, b[i], 120u * i, &out);
	checkTrue("rcbench's decoder reads the model's frame", got &&
	          memcmp(out.channel, f.ch, sizeof(out.channel)) == 0 && out.failsafe);
	checkInt("even parity of 0x0F: 0", sbusLineParity(0x0F), 0);
	checkInt("even parity of 0x07: 1", sbusLineParity(0x07), 1);
	checkInt("a clean byte: the byte alone", sbusLineWord(0xA5, SbusFault::None), 0xA5);
	checkInt("parity flipped: PE", sbusLineWord(0xA5, SbusFault::Parity), 0xA5 | SBUS_DR_PE);
	checkInt("stop bit low: FE", sbusLineWord(0xA5, SbusFault::Framing), 0xA5 | SBUS_DR_FE);
	checkInt("low throughout: BE and FE, data 0", sbusLineWord(0xA5, SbusFault::Break),
	         SBUS_DR_BE | SBUS_DR_FE);
}

/** @brief Clean streams at 14 ms and 7 ms: frames, interval, rate, stamps. */
static void testCoreFrames() {
	section("S.BUS core: clean streams at 14 ms and 7 ms");

	SbusCore c;
	sbusCoreReset(&c);
	checkTrue("a step with no word: nothing to publish", !sbusCoreStep(&c, 5));
	SbusLineFrame f = sbusLineFrame(992);
	feedFrame(&c, f, 1000);
	checkTrue("a word: publish", sbusCoreStep(&c, 7));
	checkTrue("the next step with none: nothing", !sbusCoreStep(&c, 8));
	checkInt("one frame", c.st.frames, 1);
	checkTrue("its channels", channelsAre(c.st, f));
	checkInt("interval 0 before two frames", c.st.intervalUs, 0);
	checkInt("lastFrameMs: the step's millis()", c.st.lastFrameMs, 7);
	checkInt("lastByteMs: the step's millis()", c.st.lastByteMs, 7);
	checkInt("25 clean bytes", c.st.bytes, 25);

	feedStream(&c, f, 1000 + SBUS_LINE_14MS, SBUS_LINE_14MS, 19);
	checkInt("20 frames at 14 ms", c.st.frames, 20);
	checkInt("interval 14,000 us", c.st.intervalUs, 14000);
	checkInt("71.4 Hz", sbusRateDeciHz(c.st.intervalUs), 714);
	checkInt("no resync", c.st.resyncs, 0);
	checkInt("no bad footer", c.st.badFooter, 0);
	checkInt("500 clean bytes", c.st.bytes, 500);

	sbusCoreReset(&c);
	checkTrue("a reset clears every count", c.st.frames == 0 && c.st.bytes == 0 &&
	                                        c.st.intervalUs == 0);
	feedStream(&c, f, 0, SBUS_LINE_7MS, 20);
	checkInt("20 frames at 7 ms", c.st.frames, 20);
	checkInt("interval 7,000 us", c.st.intervalUs, 7000);
	checkInt("142.9 Hz", sbusRateDeciHz(c.st.intervalUs), 1429);
	checkInt("no rate for no interval", sbusRateDeciHz(0), 0);

	// The mean is over the last 16 intervals: 16 at 7 ms, then 14 ms ones.
	uint32_t t = feedStream(&c, f, 200000, SBUS_LINE_7MS, 17);
	checkInt("16 intervals at 7 ms", c.st.intervalUs, 7000);
	t = feedStream(&c, f, t - SBUS_LINE_7MS + SBUS_LINE_14MS, SBUS_LINE_14MS, 15);
	checkInt("15 at 14 ms and one at 7 ms: 13,563 us", c.st.intervalUs, 13563);
	feedFrame(&c, f, t);
	checkInt("16 at 14 ms: the 7 ms one is out", c.st.intervalUs, 14000);
}

/** @brief An interval of 100 ms or more starts the mean again. */
static void testCoreLostLink() {
	section("S.BUS core: the mean starts again after 100 ms without a frame");

	SbusCore c;
	sbusCoreReset(&c);
	SbusLineFrame f = sbusLineFrame(500);
	uint32_t t = feedStream(&c, f, 0, SBUS_LINE_14MS, 3);
	checkInt("14 ms", c.st.intervalUs, 14000);
	feedFrame(&c, f, t - SBUS_LINE_14MS + 99999);
	checkInt("99,999 us is averaged", c.st.intervalUs, (2 * 14000 + 99999 + 1) / 3);
	t = t - SBUS_LINE_14MS + 99999;
	feedFrame(&c, f, t + 100000);
	checkInt("100,000 us: interval 0 again", c.st.intervalUs, 0);
	checkInt("the frame still counts", c.st.frames, 5);
	feedFrame(&c, f, t + 100000 + SBUS_LINE_14MS);
	checkInt("the next frame: 14 ms", c.st.intervalUs, 14000);

	// Across the micros() wrap.
	sbusCoreReset(&c);
	feedStream(&c, f, 0xFFFFFFFFu - 20000u, SBUS_LINE_14MS, 4);
	checkInt("across the micros() wrap: 4 frames", c.st.frames, 4);
	checkInt("and 14 ms", c.st.intervalUs, 14000);
	checkInt("no resync across the wrap", c.st.resyncs, 0);
}

/** @brief The flag byte: channels 17 and 18, frame lost, failsafe. */
static void testCoreFlags() {
	section("S.BUS core: the flag byte and its counts");

	SbusCore c;
	sbusCoreReset(&c);
	SbusLineFrame f = sbusLineFrame(1000);
	f.flags = SBUS_FLAG_CH17;
	feedFrame(&c, f, 0);
	checkTrue("CH17 on, CH18 off", c.st.ch17 && !c.st.ch18 && !c.st.failsafe && !c.st.frameLost);
	f.flags = SBUS_FLAG_CH18 | SBUS_FLAG_FRAME_LOST;
	feedFrame(&c, f, 14000);
	checkTrue("CH18 and frame lost", !c.st.ch17 && c.st.ch18 && c.st.frameLost && !c.st.failsafe);
	checkInt("one lost frame", c.st.lostFrames, 1);
	f.flags = SBUS_FLAG_FAILSAFE | SBUS_FLAG_FRAME_LOST;
	f.ch[2] = 172;
	feedFrame(&c, f, 28000);
	checkTrue("failsafe", c.st.failsafe && c.st.frameLost);
	checkInt("its values are the frame's", c.st.channel[2], 172);
	checkInt("two lost frames", c.st.lostFrames, 2);
	checkInt("one failsafe frame", c.st.failsafeFrames, 1);
	f.flags = 0;
	feedFrame(&c, f, 42000);
	checkTrue("a clean frame clears the last frame's flags",
	          !c.st.failsafe && !c.st.frameLost && !c.st.ch17 && !c.st.ch18);
	checkTrue("the counts stay", c.st.lostFrames == 2 && c.st.failsafeFrames == 1);
}

/**
 * @brief A flagged byte is counted by its kind and not fed: its frame is
 *        lost, the next decodes.
 */
static void testCoreErrors() {
	section("S.BUS core: parity, framing and break bytes are not fed");

	const struct { SbusFault fault; const char *name; } kinds[] = {
		{ SbusFault::Parity,  "parity" },
		{ SbusFault::Framing, "framing" },
		{ SbusFault::Break,   "break" },
	};
	for (const auto &k : kinds) {
		SbusCore c;
		sbusCoreReset(&c);
		SbusLineFrame f = sbusLineFrame(300);
		feedFrame(&c, f, 0);
		SbusLineFrame bad = f;
		bad.ch[0] = 1700;
		bad.faultAt = 5;
		bad.fault = k.fault;
		feedFrame(&c, bad, 14000);
		char what[64];
		snprintf(what, sizeof(what), "%s in byte 5: that frame is lost", k.name);
		checkInt(what, c.st.frames, 1);
		checkTrue("no value of it reaches the status", c.st.channel[0] == 300);
		checkTrue("a step after a bad word still publishes", sbusCoreStep(&c, 20));
		checkInt("24 clean bytes of it fed", c.st.bytes, 25 + 24);
		feedFrame(&c, f, 28000);
		checkInt("the next frame decodes", c.st.frames, 2);
		checkInt("the short frame is one resync", c.st.resyncs, 1);
		checkInt("parity count", c.st.parityErr, k.fault == SbusFault::Parity);
		checkInt("framing count", c.st.framingErr, k.fault == SbusFault::Framing);
		checkInt("break count", c.st.breakErr, k.fault == SbusFault::Break);
		checkInt("no overrun", c.st.overrun, 0);
	}

	// One count per bad word: break outranks framing, framing parity.
	SbusCore c;
	sbusCoreReset(&c);
	sbusCoreFeed(&c, SBUS_DR_BE | SBUS_DR_FE | SBUS_DR_PE, 0);
	sbusCoreFeed(&c, 0x0F | SBUS_DR_FE | SBUS_DR_PE, 120);
	checkTrue("BE, FE and PE: break only", c.st.breakErr == 1);
	checkTrue("FE and PE: framing only", c.st.framingErr == 1 && c.st.parityErr == 0);
	checkInt("none fed", c.st.bytes, 0);
	checkTrue("a pass of bad words stamps no byte", sbusCoreStep(&c, 9) && c.st.lastByteMs == 0);

	// Overrun: counted, and its byte fed unless another flag is set.
	sbusCoreReset(&c);
	SbusLineFrame f = sbusLineFrame(700);
	uint8_t b[SBUS_FRAME_BYTES];
	sbusLinePack(f, b);
	sbusCoreFeed(&c, b[0] | SBUS_DR_OE, 0);
	for (unsigned i = 1; i < SBUS_FRAME_BYTES; i++) sbusCoreFeed(&c, b[i], 120 * i);
	checkInt("OE on the header: counted", c.st.overrun, 1);
	checkInt("and fed: the frame decodes", c.st.frames, 1);
	checkInt("25 clean bytes", c.st.bytes, 25);
	sbusCoreFeed(&c, 0x0F | SBUS_DR_OE | SBUS_DR_PE, 20000);
	checkTrue("OE with PE: both counted, not fed",
	          c.st.overrun == 2 && c.st.parityErr == 1 && c.st.bytes == 25);
}

/** @brief Footers, a cut frame and stray bytes: the decoder's own counts. */
static void testCoreFraming() {
	section("S.BUS core: footers, a cut frame, the gap");

	SbusCore c;
	sbusCoreReset(&c);
	SbusLineFrame f = sbusLineFrame(1200);
	f.footer = 0x01;
	feedFrame(&c, f, 0);
	checkInt("footer 0x01: refused", c.st.frames, 0);
	checkInt("one bad footer", c.st.badFooter, 1);
	f.footer = 0x14;
	feedFrame(&c, f, 14000);
	checkInt("footer 0x14: decoded, as rcbench's decoder takes it", c.st.frames, 1);

	sbusCoreReset(&c);
	f = sbusLineFrame(1200);
	SbusLineFrame cut = f;
	cut.bytes = 20;
	feedFrame(&c, cut, 0);
	feedFrame(&c, f, 14000);
	checkInt("a frame cut at 20 bytes is dropped at the gap", c.st.frames, 1);
	checkInt("one resync", c.st.resyncs, 1);
	checkInt("no bad footer", c.st.badFooter, 0);

	// Bytes before any header are thrown away one by one.
	sbusCoreReset(&c);
	sbusCoreFeed(&c, 0x55, 0);
	sbusCoreFeed(&c, 0xAA, 120);
	checkInt("two stray bytes: two resyncs", c.st.resyncs, 2);
	feedFrame(&c, f, 5000);
	checkInt("then a frame", c.st.frames, 1);

	// A frame split by a gap of 2000 us is lost; 1999 us holds it together.
	sbusCoreReset(&c);
	uint8_t b[SBUS_FRAME_BYTES];
	sbusLinePack(f, b);
	for (unsigned i = 0; i < SBUS_FRAME_BYTES; i++) sbusCoreFeed(&c, b[i], i < 12 ? 0 : 1999);
	checkInt("bytes 1,999 us apart: one frame", c.st.frames, 1);
	for (unsigned i = 0; i < SBUS_FRAME_BYTES; i++)
		sbusCoreFeed(&c, b[i], i < 12 ? 10000 : 12000);
	checkInt("2,000 us apart: split, no frame", c.st.frames, 1);
}

/** @brief The pure helpers: the baud check and the roles of the wiring. */
static void testHelpers() {
	section("S.BUS: the baud check and the roles of each wiring");

	checkTrue("100,000 baud", sbusBaudOk(100000));
	checkTrue("99,000 baud: 1 %", sbusBaudOk(99000));
	checkTrue("98,999: refused", !sbusBaudOk(98999));
	checkTrue("101,000 baud: 1 %", sbusBaudOk(101000));
	checkTrue("101,001: refused", !sbusBaudOk(101001));
	checkTrue("0, no clock: refused", !sbusBaudOk(0));

	checkInt("SIG GP0, AUX GP1: SIG and AUX", sbusClaimRoles(0, 1), PIN_SIG | PIN_AUX);
	checkInt("SIG GP1, AUX GP0: SIG", sbusClaimRoles(1, 0), PIN_SIG);
	checkInt("SIG GP1, AUX GP1: SIG", sbusClaimRoles(1, 1), PIN_SIG);
	checkInt("SIG GP0, AUX GP0: none", sbusClaimRoles(0, 0), 0);

	const PinUserInfo &u = pinUserInfo(PinUser::Sbus);
	checkTrue("the pin user runs the S.BUS job", u.who == PinUser::Sbus && u.job == Core1Job::Sbus);
	checkTrue("the unplug page before DShot restarts", u.unplug);
	checkTrue("no claim ESC check: the monitor drives nothing", !u.escCheck);
	checkTrue("the job starts at Held", !u.deferJob);
}

// ---------------------------------------------------------------------------
// the job, on the claim and the UART0 fake
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

/** @brief An S.BUS claim with @p roles through to the job's bind. */
static ClaimState claimToBind(uint8_t roles) {
	if (pinClaimBegin(PinUser::Sbus, roles) != ClaimRefusal::None) return ClaimState::Free;
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	return pinClaimPoll();
}

/**
 * @brief Passes of core1 every @p stepUs until the clock reaches @p untilUs.
 *        The last pass may go past it, so the passes stay on one grid.
 */
static void runTo(uint64_t untilUs, uint32_t stepUs) {
	while (time_us_64() < untilUs) {
		fakeAdvanceUs(stepUs);
		core1JobPoll();
	}
}

/** @brief BACK: the release, the unplug page due, RESTART DSHOT, Free. */
static bool claimEnd() {
	pinClaimEnd(PinUser::Sbus);
	core1JobPoll();
	pinClaimPoll();
	bool due = pinClaimUnplugDue();
	pinClaimRestart(PinUser::Sbus);
	fakePumpPoll();
	return due && pinClaimState() == ClaimState::Free;
}

/** @brief Whether GP1 is UART0's input: the function, INOVER, the pull-down, no output. */
static bool gp1Listens() {
	FakePin p = fakePin(SBUS_GPIO);
	return p.function == GPIO_FUNC_UART && p.inover == GPIO_OVERRIDE_INVERT &&
	       p.pull == FakePull::Down && !p.output && p.puts == 0;
}

/** @brief Whether GP1 is a plain input: SIO, no inversion, the pull-down, no output. */
static bool gp1Plain() {
	FakePin p = fakePin(SBUS_GPIO);
	return p.function == GPIO_FUNC_SIO && p.inover == GPIO_OVERRIDE_NORMAL &&
	       p.pull == FakePull::Down && !p.output && p.puts == 0;
}

/** @brief GP0's record is what it was: the job never touched it. */
static bool gp0Untouched(const FakePin &before) {
	FakePin p = fakePin(0);
	return memcmp(&p, &before, sizeof(p)) == 0;
}

/** @brief The default wiring: bind, a stream, the heartbeat, BACK. */
static void testJob() {
	section("S.BUS job: on the claim, through the job table, on UART0");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	FakePin gp0 = fakePin(0);
	checkInt("the default wiring claims SIG and AUX", sbusClaimRoles(0, 1), PIN_SIG | PIN_AUX);
	checkTrue("the claim runs the shipped job", claimToBind(PIN_SIG | PIN_AUX) == ClaimState::Running);
	checkTrue("the S.BUS job runs", core1JobRunning() == Core1Job::Sbus);
	checkTrue("GP1: UART function, input inverted, pull-down, never an output", gp1Listens());
	checkInt("INOVER set once", fakePin(SBUS_GPIO).inoverWrites, 1);
	checkTrue("GP0 untouched: UART0's TX reaches no pad", gp0Untouched(gp0));
	FakeUart u = fakeUart();
	checkTrue("UART0 on", u.enabled && u.inits == 1);
	checkInt("100,000 baud from 150 MHz", u.baud, 100000);
	checkTrue("IBRD 93, FBRD 48", u.ibrd == 93 && u.fbrd == 48);
	checkTrue("8E2", u.dataBits == 8 && u.stopBits == 2 && u.parity == UART_PARITY_EVEN);
	SbusStatus st;
	sbusJobStatus(&st);
	checkTrue("status at the bind: nothing yet", st.frames == 0 && st.bytes == 0 &&
	                                             st.lastFrameMs == 0 && st.intervalUs == 0);

	// 30 frames at 14 ms, a pass every 1000 us: each byte drained up to
	// 1000 us late, no frame split.
	SbusLineFrame f = sbusLineFrame(172);
	f.ch[3] = 1811;
	uint64_t t0 = time_us_64() + 337;
	uint64_t end = sbusLineStream(t0, SBUS_LINE_14MS, 30, f);
	escHeartbeat();
	runTo(end, 1000);
	sbusJobStatus(&st);
	checkInt("30 frames at 14 ms", st.frames, 30);
	checkTrue("their channels", channelsAre(st, f));
	checkInt("interval 14,000 us", st.intervalUs, 14000);
	checkInt("no resync", st.resyncs, 0);
	checkInt("750 bytes", st.bytes, 750);
	checkTrue("no error", st.parityErr + st.framingErr + st.breakErr + st.overrun == 0);
	// The pass that drains the last byte: the first on the 1000 us grid at
	// or after the byte is in.
	uint64_t lastIn = end - SBUS_LINE_14MS + 24 * SBUS_LINE_BYTE_US + SBUS_LINE_LOAD_US;
	checkInt("lastFrameMs: the pass of the last frame", st.lastFrameMs,
	         (uint32_t)((lastIn + 999) / 1000));
	checkTrue("the heartbeat lapsed long ago", !core1UiAlive());
	checkTrue("and the job decodes on: it sends nothing", st.frames == 30);
	checkTrue("GP1 still only listens", gp1Listens() && gp0Untouched(gp0));

	// At 7 ms with failsafe frames.
	f.flags = SBUS_FLAG_FAILSAFE | SBUS_FLAG_FRAME_LOST;
	end = sbusLineStream(end, SBUS_LINE_7MS, 10, f);
	runTo(end, 1000);
	sbusJobStatus(&st);
	checkInt("10 more at 7 ms", st.frames, 40);
	checkInt("the mean: 9 intervals of 7 ms, 7 of 14 ms", st.intervalUs,
	         (9u * 7000u + 7u * 14000u + 8u) / 16u);
	checkTrue("failsafe and frame lost", st.failsafe && st.frameLost);
	checkTrue("counted", st.failsafeFrames == 10 && st.lostFrames == 10);

	// A parity fault through the UART: counted, its frame lost.
	SbusLineFrame bad = sbusLineFrame(172);
	bad.faultAt = 9;
	bad.fault = SbusFault::Parity;
	sbusLineSend(end, bad);
	end = sbusLineStream(end + SBUS_LINE_7MS, SBUS_LINE_7MS, 2, f);
	runTo(end, 1000);
	sbusJobStatus(&st);
	checkTrue("parity from UARTDR: counted, the frame lost, the next decode",
	          st.parityErr == 1 && st.frames == 42 && st.resyncs == 1);

	// BACK: the job lets go in one pass, then the unplug page.
	int deinits = fakeUart().deinits;
	pinClaimEnd(PinUser::Sbus);
	core1JobPoll();
	checkTrue("released in one pass", core1JobPhase() == Core1JobPhase::Idle);
	checkInt("UART0 off", fakeUart().deinits, deinits + 1);
	checkTrue("UART0 disabled", !fakeUart().enabled);
	checkTrue("GP1 a plain input, no inversion, pull-down", gp1Plain());
	checkTrue("GP0 untouched", gp0Untouched(gp0));
	pinClaimPoll();
	checkTrue("the unplug page is due", pinClaimUnplugDue());
	checkTrue("DShot waits for RESTART DSHOT", pinClaimState() == ClaimState::Leaving &&
	                                           escTaskSuspended());
	SbusStatus before;
	sbusJobStatus(&before);
	unsigned dropped = fakeUart().dropped;
	uint64_t after = sbusLineStream(time_us_64(), SBUS_LINE_14MS, 2, sbusLineFrame(900));
	runTo(after, 1000);
	checkInt("frames after the release are not heard", fakeUart().dropped, dropped + 50);
	sbusJobStatus(&st);
	checkTrue("the status keeps the last frame", memcmp(&st, &before, sizeof(st)) == 0);
	pinClaimRestart(PinUser::Sbus);
	fakePumpPoll();
	checkTrue("RESTART DSHOT: free", pinClaimState() == ClaimState::Free);
	checkTrue("the job was idle before DShot resumed", fakeResumeJobIdle());

	// A new visit starts its counts at 0.
	freshPump(0, 1);
	checkTrue("a second visit runs", claimToBind(PIN_SIG | PIN_AUX) == ClaimState::Running);
	sbusJobStatus(&st);
	checkTrue("its counts start at 0", st.frames == 0 && st.bytes == 0 && st.parityErr == 0 &&
	                                   st.resyncs == 0 && st.failsafeFrames == 0 &&
	                                   st.lastFrameMs == 0 && !st.failsafe);
	end = sbusLineStream(time_us_64() + 50, SBUS_LINE_14MS, 2, sbusLineFrame(400));
	runTo(end, 1000);
	sbusJobStatus(&st);
	checkTrue("and it decodes", st.frames == 2 && st.channel[0] == 400);
	checkTrue("back to free", claimEnd());
	checkTrue("GP1 plain again", gp1Plain());
}

/** @brief Drains late: up to 1000 us splits nothing, a pass 2000 us late mid-frame does. */
static void testJobLateDrain() {
	section("S.BUS job: drains late, and the FIFO's overrun");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	checkTrue("running", claimToBind(PIN_SIG | PIN_AUX) == ClaimState::Running);
	SbusLineFrame f = sbusLineFrame(1000);
	uint64_t end = sbusLineStream(time_us_64() + 500, SBUS_LINE_7MS, 20, f);
	runTo(end, 1000);
	SbusStatus st;
	sbusJobStatus(&st);
	checkTrue("7 ms, a pass every 1000 us: every frame, no resync",
	          st.frames == 20 && st.resyncs == 0 && st.intervalUs == 7000);

	// No pass from 1500 us into a frame until 2000 us later: the bytes after
	// the hole are stamped 2000 us after those before it, and the gap rule
	// splits the frame. The limit of a late pass, as the design note says.
	uint64_t t = end + 100;
	sbusLineSend(t, f);
	runTo(t + 1500, 500);
	fakeAdvanceUs(2000);
	core1JobPoll();
	runTo(t + 5000, 500);
	sbusJobStatus(&st);
	checkTrue("a pass 2000 us late mid-frame splits it: no frame", st.frames == 20);
	checkTrue("its first part is a resync", st.resyncs >= 1);
	uint32_t resyncs = st.resyncs;
	end = sbusLineStream(t + SBUS_LINE_7MS, SBUS_LINE_7MS, 2, f);
	runTo(end, 1000);
	sbusJobStatus(&st);
	checkTrue("the frames after it decode", st.frames == 22 && st.resyncs >= resyncs);

	// No pass for 11 ms over a 7 ms stream: 32 bytes fill the FIFO, the
	// rest of the second frame is lost, and the first byte that enters
	// after the drain carries OE.
	t = time_us_64() + 200;
	sbusLineStream(t, SBUS_LINE_7MS, 3, f);
	fakeAdvanceUs((uint32_t)(t - time_us_64()) + 11000);
	FakeUart u = fakeUart();
	checkInt("the FIFO full: 32 words", u.fifo, 32);
	checkInt("18 bytes lost", u.lost, 18);
	runTo(t + 3 * SBUS_LINE_7MS + 5000, 1000);
	sbusJobStatus(&st);
	checkInt("one overrun", st.overrun, 1);
	checkInt("the first frame and the third decode", st.frames, 24);
	checkTrue("no byte flagged otherwise", st.parityErr + st.framingErr + st.breakErr == 0);
	checkTrue("back to free", claimEnd());
}

/** @brief Each wiring of the design note's table, and the refusals. */
static void testJobWirings() {
	section("S.BUS job: each wiring, no GP1, the clock check");

	SbusStatus st;
	SbusLineFrame f = sbusLineFrame(800);

	// ESC GP1, KISS GP0: GP1 is SIG.
	fakePwmReset();
	fakeUartReset();
	freshPump(1, 0);
	FakePin gp0 = fakePin(0);
	checkTrue("SIG GP1, AUX GP0: runs with SIG", claimToBind(sbusClaimRoles(1, 0)) == ClaimState::Running);
	checkTrue("it binds GP1", gp1Listens());
	checkTrue("GP0, AUX here, untouched", gp0Untouched(gp0));
	uint64_t end = sbusLineStream(time_us_64() + 10, SBUS_LINE_14MS, 3, f);
	runTo(end, 1000);
	sbusJobStatus(&st);
	checkInt("and decodes", st.frames, 3);
	checkTrue("back to free", claimEnd());
	checkTrue("GP1 plain", gp1Plain());

	// ESC GP1, KISS GP1, KISS off.
	fakePwmReset();
	fakeUartReset();
	freshPump(1, 1, false);
	checkTrue("SIG GP1, AUX GP1: runs with SIG", claimToBind(sbusClaimRoles(1, 1)) == ClaimState::Running);
	checkTrue("it binds GP1", gp1Listens());
	checkTrue("back to free", claimEnd());

	// ESC GP0, KISS GP0, KISS off: no role on GP1. The screen asks no
	// claim; a claim made anyway is refused by the bind and takes nothing.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 0, false);
	gp0 = fakePin(0);
	FakePin gp1 = fakePin(1);
	checkInt("SIG GP0, AUX GP0: no roles", sbusClaimRoles(0, 0), 0);
	checkTrue("a claim of SIG anyway: held, not running", claimToBind(PIN_SIG) == ClaimState::Held);
	checkInt("the bind's reason: NO GP1", pinClaimJobError(), CORE1_JOB_NO_GP1);
	checkTrue("UART0 never started", fakeUart().inits == 0);
	FakePin p1 = fakePin(1);
	checkTrue("neither pin touched", gp0Untouched(gp0) && memcmp(&p1, &gp1, sizeof(p1)) == 0);
	pinClaimEnd(PinUser::Sbus);
	pinClaimPoll();
	pinClaimRestart(PinUser::Sbus);
	fakePumpPoll();
	checkTrue("BACK: free", pinClaimState() == ClaimState::Free);

	// clk_peri at 1 MHz: the divisors give 62,500 baud.
	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	fakeClockSet(clk_peri, 1000000u);
	checkTrue("clk_peri 1 MHz: held, not running", claimToBind(PIN_SIG | PIN_AUX) == ClaimState::Held);
	checkInt("uart_init() gave 62,500 baud", fakeUart().baud, 62500);
	checkInt("the bind's reason: CLOCK", pinClaimJobError(), CORE1_JOB_CLOCK);
	checkTrue("UART0 off again", !fakeUart().enabled && fakeUart().deinits == 1);
	checkTrue("GP1 left plain: no inversion, pull-down", gp1Plain());
	fakeClockSet(clk_peri, 150000000u);
	pinClaimRetry(PinUser::Sbus);
	core1JobPoll();
	checkTrue("RETRY at 150 MHz: running", pinClaimPoll() == ClaimState::Running && gp1Listens());
	checkTrue("back to free", claimEnd());

	// A clk_peri of 0: uart_init() returns 0.
	fakeUartReset();
	freshPump(0, 1);
	fakeClockSet(clk_peri, 0);
	checkTrue("clk_peri 0: held", claimToBind(PIN_SIG | PIN_AUX) == ClaimState::Held);
	checkInt("CLOCK", pinClaimJobError(), CORE1_JOB_CLOCK);
	fakeClockSet(clk_peri, 150000000u);
	pinClaimEnd(PinUser::Sbus);
	pinClaimPoll();
	pinClaimRestart(PinUser::Sbus);
	fakePumpPoll();
	checkTrue("BACK: free", pinClaimState() == ClaimState::Free);
}

/** @brief KISS live, an ESC on SIG, and what the held claim refuses. */
static void testJobInterlocks() {
	section("S.BUS job: KISS live, an ESC on SIG, the claim held");

	fakePwmReset();
	fakeUartReset();
	freshPump(0, 1);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.kissLastMs = millis() - 100;
	fakeSetTelemetry(&t);
	checkTrue("a KISS frame 100 ms ago, AUX asked: refused",
	          pinClaimBegin(PinUser::Sbus, PIN_SIG | PIN_AUX) == ClaimRefusal::KissLive);
	checkTrue("DShot keeps running", !escTaskSuspended() && pinClaimState() == ClaimState::Free);
	t.kissLastMs = millis() - 500;
	fakeSetTelemetry(&t);
	checkTrue("500 ms ago: claims", pinClaimBegin(PinUser::Sbus, PIN_SIG | PIN_AUX) == ClaimRefusal::None);
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	checkTrue("running", pinClaimPoll() == ClaimState::Running);
	checkTrue("the pump is down: no DShot, no KISS receiver", escTaskSuspended() && !fakePinReturned());
	checkTrue("two servos while it holds AUX: busy",
	          pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX) == ClaimRefusal::Busy);
	checkTrue("any other user: busy", pinClaimBegin(PinUser::Kst, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("the holder is still the monitor", pinClaimHolder() == PinUser::Sbus &&
	                                              core1JobRunning() == Core1Job::Sbus);
	checkTrue("back to free", claimEnd());

	// An ESC answering on SIG: the claim does not wait; the drain sends it
	// zero throttle first, as for every claim.
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis() - 10;
	fakeSetTelemetry(&t);
	ClaimFacts cf = { false, false, millis(), t.lastRpmMs, 0, 0, 1 };
	checkTrue("the rule: the servo tester waits for an ESC on SIG",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &cf) == ClaimRefusal::EscAnswering);
	checkTrue("the monitor does not",
	          pinClaimCheck(PinUser::Sbus, PIN_SIG | PIN_AUX, &cf) == ClaimRefusal::None);
	checkTrue("an ESC answering on SIG: the monitor claims",
	          pinClaimBegin(PinUser::Sbus, PIN_SIG | PIN_AUX) == ClaimRefusal::None);
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	checkTrue("running", pinClaimPoll() == ClaimState::Running);
	checkTrue("back to free", claimEnd());
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
}

void runSbusJobTests() {
	FakeWiring w = fakeWiring();
	testLineModel();
	testCoreFrames();
	testCoreLostLink();
	testCoreFlags();
	testCoreErrors();
	testCoreFraming();
	testHelpers();
	testJob();
	testJobLateDrain();
	testJobWirings();
	testJobInterlocks();
	fakeUartReset();
	fakePwmReset();
	freshPump(w.dshotPin, w.kissPin, w.kissEnable);
	escTaskConfigure(w.dshotPin, w.dshotKbaud, w.kissEnable, w.kissPin);
}
