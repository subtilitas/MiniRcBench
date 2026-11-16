/**
 * @file test_bridge.cpp
 * @brief The USB bridge's wire (slice 1 of docs/design/usb-bridge.md): the
 *        clock divider, the frame slot, the ring, the job on the line fake,
 *        and the job through the pin claim and the fake DShot pump.
 *
 * The job runs from BRIDGE_JOB_OPS directly in the first sections, one pass
 * every so many microseconds of the virtual clock, and through the job table
 * and the claim in the last, with core1's side played by fakePumpPoll() and
 * core1JobPoll() and core0's by pinClaimPoll(). The line is
 * test/fake_bridge_line.h, whose cycle counts test/pio_bridge_emu.py
 * measures on the PIO programs. Nothing here has run on the chip.
 */

#include "check.h"
#include "fakes.h"
#include "fake_bridge_line.h"
#include "bridge_wire.h"
#include "bridge_line_port.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "ui.h"
#include "plat.h"

#include <string.h>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/** @brief AM32's 21-byte bootloader greeting (am32_bl.cpp), a frame of 21 bytes. */
static const uint8_t INIT[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                 0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};

/** @brief The model ESC answers every frame with its ACK, 0x30, after 300 µs. */
static int replyAck(const uint8_t *, int, uint8_t *r, uint32_t *delayUs) {
	r[0] = 0x30;
	*delayUs = 300;
	return 1;
}

/** @brief Bytes the next replyMany() sends. */
static int g_many = 0;
/** @brief The model ESC answers with g_many bytes counting up from 0x40, after 300 µs. */
static int replyMany(const uint8_t *, int, uint8_t *r, uint32_t *delayUs) {
	for (int i = 0; i < g_many; i++) r[i] = (uint8_t)(0x40 + i);
	*delayUs = 300;
	return g_many;
}

/** @brief The job bound on @p gpio against a fresh line, echoes kept. */
static void bindJob(uint8_t gpio = 0) {
	fakeBridgeLineReset();
	bridgeWireKeepEcho(true);
	BRIDGE_JOB_OPS.bind(gpio, 0xFF);
}

/** @brief @p n passes of the job, @p us apart. */
static void steps(int n, uint32_t us, bool alive = true) {
	for (int i = 0; i < n; i++) {
		fakeAdvanceUs(us);
		BRIDGE_JOB_OPS.step(millis(), alive);
	}
}

/** @brief Release passes 1 ms apart until the job lets go; how many it took. */
static int releaseJob() {
	int n = 1;
	while (!BRIDGE_JOB_OPS.release(millis()) && n < 1000) {
		fakeAdvanceUs(1000);
		n++;
	}
	return n;
}

/** @brief The job's counts. */
static BridgeWireStats stats() {
	BridgeWireStats s;
	bridgeWireStats(&s);
	return s;
}

/** @brief Everything in the ring. */
static int recvAll(uint8_t *buf, int room) {
	int n = 0;
	for (uint16_t got; (got = bridgeWireRecv(buf + n, (uint16_t)(room - n))) != 0;) n += got;
	return n;
}

/** @brief The bytes on the wire since the reset, from index @p from, equal @p want. */
static bool wireIs(int from, const uint8_t *want, int n) {
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

/** @brief Cycles between the start bits of wire bytes @p i and @p i + 1. */
static long gapAfter(int i) {
	uint8_t b;
	uint64_t a, c;
	bool keep;
	fakeBridgeLineWire(i, &b, &a, &keep);
	fakeBridgeLineWire(i + 1, &b, &c, &keep);
	return (long)(c - a);
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

/** @brief bridgeLineClock() at 150 MHz, 125 MHz and its lower bound. */
static void testClock() {
	section("USB bridge: the line's clock divider");

	uint16_t i = 0;
	uint8_t f = 0;
	checkTrue("150 MHz: a divider", bridgeLineClock(150000000u, &i, &f));
	checkInt("150 MHz: 976", i, 976);
	checkInt("and 144/256: 976.5625, exact", f, 144);
	checkTrue("125 MHz: a divider", bridgeLineClock(125000000u, &i, &f));
	checkInt("125 MHz: 813", i, 813);
	checkInt("and 205/256, rounded from 205.33", f, 205);
	checkTrue("153,600 Hz: 1", bridgeLineClock(153600u, &i, &f) && i == 1 && f == 0);
	checkTrue("153,300 Hz: rounds to 1", bridgeLineClock(153300u, &i, &f) && i == 1 && f == 0);
	i = 7;
	f = 9;
	checkTrue("153,299 Hz: below 1, refused", !bridgeLineClock(153299u, &i, &f));
	checkTrue("and the outputs not written", i == 7 && f == 9);
	checkTrue("0 Hz: refused", !bridgeLineClock(0, &i, &f));
	checkTrue("4,294,967,295 Hz: a divider", bridgeLineClock(0xFFFFFFFFu, &i, &f) && i == 27962);
}

/** @brief The frame slot: refusals, the handover, the slot free again. */
static void testSlot() {
	section("USB bridge: the frame slot");

	fakeBridgeLineReset();
	uint8_t frame[BRIDGE_FRAME_MAX + 1];
	for (unsigned k = 0; k < sizeof(frame); k++) frame[k] = (uint8_t)(k * 7 + 3);
	checkTrue("before the bind: not open", !bridgeWireOpen() && !bridgeWireSlotFree());
	checkTrue("and a send refused", !bridgeWireSend(frame, 4));

	bindJob();
	checkTrue("bound: open, the slot free", bridgeWireOpen() && bridgeWireSlotFree());
	checkInt("the line opened on SIG", fakeBridgeLinePin(), 0);
	checkTrue("an empty frame refused", !bridgeWireSend(frame, 0));
	checkTrue("321 bytes refused", !bridgeWireSend(frame, BRIDGE_FRAME_MAX + 1));
	checkTrue("320 bytes taken", bridgeWireSend(frame, BRIDGE_FRAME_MAX));
	checkTrue("the slot full", !bridgeWireSlotFree());
	checkTrue("a second frame refused while it is", !bridgeWireSend(frame, 2));
	checkInt("nothing on the wire before core1's pass", fakeBridgeLineWireCount(), 0);

	steps(1, 1000);
	checkInt("one pass: 8 words in, as many as echoes may be owed", (long)stats().sent, 8);
	checkTrue("the slot still full", !bridgeWireSlotFree());
	int passes = 1;
	while (!bridgeWireSlotFree() && passes < 400) {
		steps(1, 1000);
		passes++;
	}
	const BridgeWireStats s = stats();
	checkInt("the slot free once all 320 are in the FIFO", (long)s.sent, 320);
	checkTrue("while the last 8 still go out", fakeBridgeLineWireCount() >= 312 &&
	                                           fakeBridgeLineWireCount() < 320);
	checkInt("one frame", (long)s.frames, 1);
	steps(10, 1000);
	checkTrue("the whole frame on the wire, in order", wireIs(0, frame, BRIDGE_FRAME_MAX));
	checkInt("the pad let go once, after the last byte", fakeBridgeLineReleases(), 1);

	checkTrue("the next frame taken", bridgeWireSend(frame + 5, 3));
	steps(5, 1000);
	checkTrue("and sent", wireIs(BRIDGE_FRAME_MAX, frame + 5, 3));
	checkInt("two frames", (long)stats().frames, 2);

	releaseJob();
	checkTrue("released: closed, a send refused", !bridgeWireOpen() && !bridgeWireSend(frame, 1));
	checkInt("the line closed once", fakeBridgeLineCloses(), 1);
	checkInt("no word put while closed", fakeBridgeLineMisuse(), 0);
}

/** @brief A frame on the wire: back to back, the echo, the answer, the counts. */
static void testLine() {
	section("USB bridge: the line, the echo and the answer");

	bindJob();
	fakeBridgeLineResponder(replyAck);
	checkTrue("the greeting taken", bridgeWireSend(INIT, sizeof(INIT)));
	steps(30, 1000);
	checkTrue("on the wire whole, every byte but the last keeping the pad",
	          wireIs(0, INIT, sizeof(INIT)));
	long worst = 0, least = 1000;
	for (int k = 0; k + 1 < (int)sizeof(INIT); k++) {
		worst = gapAfter(k) > worst ? gapAfter(k) : worst;
		least = gapAfter(k) < least ? gapAfter(k) : least;
	}
	checkInt("start bits 82 cycles apart at most, a pass every 1 ms", worst, 82);
	checkInt("and at least", least, 82);
	checkInt("the pad let go once", fakeBridgeLineReleases(), 1);
	uint8_t ring[1100];
	int n = recvAll(ring, sizeof(ring));
	checkInt("the ring: the echo, then the ACK", n, 22);
	checkTrue("the echo first, byte for byte", memcmp(ring, INIT, sizeof(INIT)) == 0);
	checkInt("then the ACK", ring[21], 0x30);
	BridgeWireStats s = stats();
	checkInt("sent 21", (long)s.sent, 21);
	checkInt("received 22", (long)s.received, 22);
	checkInt("no collision", (long)s.collisions, 0);
	checkInt("nothing lost", (long)s.lost, 0);
	checkInt("the pad never driven by both", fakeBridgeLineConflicts(), 0);

	// A pass every 5 ms: 8 bytes last 4.27 ms, so the transmitter waits for
	// core1, the pad driven, and the frame stays one frame. The receive FIFO
	// holds the 8 echoes.
	uint8_t big[64];
	for (int k = 0; k < 64; k++) big[k] = (uint8_t)(0xA0 + k);
	bridgeWireSend(big, sizeof(big));
	steps(20, 5000);
	checkTrue("64 bytes with a pass every 5 ms: whole", wireIs(21, big, 64));
	checkInt("still one release for the frame", fakeBridgeLineReleases(), 2);
	worst = 0;
	least = 1000;
	for (int k = 21; k < 21 + 63; k++) {
		worst = gapAfter(k) > worst ? gapAfter(k) : worst;
		least = gapAfter(k) < least ? gapAfter(k) : least;
	}
	checkTrue("late bytes wait on a driven pad", worst > 82);
	checkInt("and none comes sooner than 82 cycles", least, 82);
	recvAll(ring, sizeof(ring));

	// The heartbeat changes nothing: the job sends what core0 put.
	bridgeWireSend(INIT, 2);
	steps(10, 1000, false);
	checkTrue("a lapsed heartbeat still sends", wireIs(85, INIT, 2));
	recvAll(ring, sizeof(ring));

	// The 4-way interface's setting: the echo dropped, the answer kept.
	bridgeWireKeepEcho(false);
	bridgeWireSend(INIT, sizeof(INIT));
	steps(30, 1000);
	n = recvAll(ring, sizeof(ring));
	checkInt("echo dropped: the ACK alone", n, 1);
	checkInt("the ACK", ring[0], 0x30);
	checkInt("still no collision", (long)stats().collisions, 0);
	bridgeWireKeepEcho(true);

	// An echo that differs: the ESC drove the line.
	fakeBridgeLineCorruptEcho(85 + 2 + 21 + 3, 0x55);
	bridgeWireSend(INIT, sizeof(INIT));
	steps(30, 1000);
	n = recvAll(ring, sizeof(ring));
	checkInt("a differing echo: one collision", (long)stats().collisions, 1);
	checkTrue("and it reaches the ring as it came", n == 22 && ring[3] == 0x55 && ring[21] == 0x30);

	// An echo that never comes, in the middle: the rest of the frame shifts,
	// and the frame counts once.
	const int base = 85 + 2 + 21 + 21;
	fakeBridgeLineDropEcho(base + 5);
	bridgeWireSend(INIT, sizeof(INIT));
	steps(30, 1000);
	n = recvAll(ring, sizeof(ring));
	checkInt("a lost echo: one more collision", (long)stats().collisions, 2);
	checkInt("the ring: 20 echoes and the ACK", n, 21);
	checkTrue("the ACK last, not taken for the echo", ring[20] == 0x30);

	// The last echo lost: found when the line is idle, before the answer.
	// Passes 250 µs apart, so one finds the line idle before the ACK, which
	// is in the FIFO 0.81 ms after the release.
	fakeBridgeLineDropEcho(base + 21 + 20);
	bridgeWireKeepEcho(false);
	bridgeWireSend(INIT, sizeof(INIT));
	steps(48, 250);
	checkInt("the last echo lost: a collision at idle", (long)stats().collisions, 3);
	steps(25, 1000);
	n = recvAll(ring, sizeof(ring));
	checkTrue("and the ACK still in the ring, echoes dropped", n == 1 && ring[0] == 0x30);
	bridgeWireKeepEcho(true);
	checkInt("seven frames", (long)stats().frames, 7);
	checkInt("still nothing lost", (long)stats().lost, 0);
	releaseJob();
	checkInt("opens and closes balance", fakeBridgeLineOpens(), fakeBridgeLineCloses());
}

/** @brief Overruns: the receive FIFO, the ring; the ring's wrap; the echo limit. */
static void testOverruns() {
	section("USB bridge: overruns, the ring's wrap, the echo limit");

	uint8_t ring[1100];
	bindJob();
	fakeBridgeLineResponder(replyMany);
	g_many = 20;
	bridgeWireKeepEcho(false);
	bridgeWireSend(INIT, 2);
	steps(1, 1000);
	// No pass for 20 ms: 20 bytes arrive, the FIFO holds 8.
	fakeAdvanceUs(20000);
	steps(1, 1000);
	BridgeWireStats s = stats();
	checkInt("a full receive FIFO: counted once", (long)s.lost, 1);
	int n = recvAll(ring, sizeof(ring));
	checkInt("what it held: 2 echoes dropped, 6 of the answer", n, 6);
	checkTrue("the first 6 of the answer", ring[0] == 0x40 && ring[5] == 0x45);
	releaseJob();

	// The ring: 1030 bytes and core0 reading nothing.
	bindJob();
	bridgeWireKeepEcho(false);
	fakeBridgeLineResponder(replyMany);
	g_many = 1030;
	bridgeWireSend(INIT, 1);
	steps(600, 1000);
	checkInt("the ring holds 1024", bridgeWireAvailable(), BRIDGE_RX_RING);
	s = stats();
	checkInt("6 bytes lost", (long)s.lost, 6);
	checkInt("received 1031: the echo and 1030", (long)s.received, 1031);
	uint16_t got = bridgeWireRecv(ring, 1000);
	checkInt("1000 read", got, 1000);
	checkTrue("oldest first", ring[0] == 0x40 && ring[999] == (uint8_t)(0x40 + 999));
	g_many = 100;
	bridgeWireSend(INIT, 1);
	steps(80, 1000);
	n = recvAll(ring, sizeof(ring));
	checkInt("24 left and 100 more, across the wrap", n, 124);
	checkTrue("in order", ring[23] == (uint8_t)(0x40 + 1023) && ring[24] == 0x40 &&
	                      ring[123] == (uint8_t)(0x40 + 99));
	bridgeWireSend(INIT, 1);
	steps(80, 1000);
	checkInt("100 more, left unread", bridgeWireAvailable(), 100);
	releaseJob();
	checkInt("closed: none to read", bridgeWireAvailable(), 0);
	checkInt("and none taken", bridgeWireRecv(ring, sizeof(ring)), 0);
	BRIDGE_JOB_OPS.bind(0, 0xFF);
	checkInt("bound again: the ring empty", bridgeWireAvailable(), 0);
	releaseJob();

	// Every echo lost: 8 owed stop the feed until the line is idle.
	bindJob();
	fakeBridgeLineDropEcho(-1);
	uint8_t big[64];
	for (int k = 0; k < 64; k++) big[k] = (uint8_t)(k + 1);
	bridgeWireSend(big, sizeof(big));
	steps(60, 1000);
	checkTrue("every echo lost: the frame goes out whole", wireIs(0, big, 64));
	checkTrue("waiting after 8 bytes for the line to idle", gapAfter(7) > 82 && gapAfter(6) == 82);
	checkInt("one release: the pad driven through the wait", fakeBridgeLineReleases(), 1);
	checkInt("one collision for the frame", (long)stats().collisions, 1);
	releaseJob();
	bridgeWireKeepEcho(true);
}

/** @brief The job's release: the frame under way finished, one not started dropped, the cap. */
static void testRelease() {
	section("USB bridge: the release");

	uint8_t big[100];
	for (int k = 0; k < 100; k++) big[k] = (uint8_t)k;
	bindJob();
	bridgeWireSend(big, sizeof(big));
	steps(10, 1000);
	const int before = fakeBridgeLineWireCount();
	checkTrue("10 ms in: part of the frame out", before > 10 && before < 100);
	int n = releaseJob();
	checkTrue("the release finishes the frame", wireIs(0, big, 100));
	checkInt("and cuts nothing", fakeBridgeLineCuts(), 0);
	checkTrue("in the passes it takes, 1 ms apart, under 250 ms", n > 30 && n < 250);
	checkTrue("closed", !bridgeLineIsOpen() && fakeBridgeLineCloses() == 1);

	bindJob();
	bridgeWireSend(big, 10);
	n = releaseJob();
	checkInt("a frame not started: nothing sent", fakeBridgeLineWireCount(), 0);
	checkInt("released in one pass", n, 1);

	bindJob();
	bridgeWireSend(big, 10);
	steps(1, 1000);
	fakeBridgeLineStall(true);
	const uint32_t t0 = millis();
	bool done = BRIDGE_JOB_OPS.release(t0);
	checkTrue("a line that never idles: held", !done);
	fakeAdvance(249);
	checkTrue("still held at 249 ms", !BRIDGE_JOB_OPS.release(millis()));
	fakeAdvance(1);
	checkTrue("closed at 250 ms", BRIDGE_JOB_OPS.release(millis()));
	checkInt("the cut counted", fakeBridgeLineCuts(), 1);
	checkTrue("the slot not open after it", !bridgeWireOpen());
	fakeBridgeLineStall(false);
}

// ---------------------------------------------------------------------------
// the job on the claim
// ---------------------------------------------------------------------------

/** @brief The wiring, the pump bound and past any drain, no ESC answering. */
static void freshPump(uint8_t sig, uint8_t aux, bool kiss) {
	// A hub's frame: Run not wanted, so a claim is not Busy
	// (docs/design/dshot-lifetime.md).
	pinClaimDshot(false);
	escTaskConfigure(sig, 600, kiss, aux);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
}

/** @brief Times DShot was bound while the bridge's line was open. */
static int g_together = 0;

/** @brief One pass of both cores, 1 ms on: the pump, the job, the claim. */
static ClaimState pass() {
	fakeAdvanceUs(1000);
	fakePumpPoll();
	core1JobPoll();
	if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
	ClaimState s = pinClaimPoll();
	if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
	return s;
}

/**
 * @brief One pass of both cores, 1 ms on, with core0 only looking at the
 *        claim: a screen that ended the claim and closed, and MENU's
 *        pinClaimState() behind it.
 */
static ClaimState passLooking() {
	fakeAdvanceUs(1000);
	fakePumpPoll();
	core1JobPoll();
	if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
	ClaimState s = pinClaimState();
	if (bridgeLineIsOpen() && !escTaskSuspended()) g_together++;
	return s;
}

/** @brief A bridge claim on SIG to Running, or the state it stopped in. */
static ClaimState claimToRunning() {
	if (pinClaimBegin(PinUser::Bridge, PIN_SIG) != ClaimRefusal::None) return ClaimState::Free;
	for (int k = 0; k < 4 && pinClaimState() != ClaimState::Running; k++) pass();
	return pinClaimState();
}

/** @brief The pin user and the claim's rules for it. */
static void testPinUser() {
	section("USB bridge: the pin user");

	const PinUserInfo &u = pinUserInfo(PinUser::Bridge);
	checkTrue("the pin user runs the bridge's job", u.who == PinUser::Bridge &&
	                                                u.job == Core1Job::Bridge);
	checkTrue("no leave page from its row", !u.unplug);
	checkTrue("no claim ESC check: it needs the ESC", !u.escCheck);
	checkTrue("the job starts at Held", !u.deferJob);

	const uint32_t now = 100000;
	ClaimFacts f = {false, false, now, 0, 0, 0, 1};
	checkTrue("free, disarmed, no ESC: claims",
	          pinClaimCheck(PinUser::Bridge, PIN_SIG, &f) == ClaimRefusal::None);
	f.lastRpmMs = now - 10;
	checkTrue("an ESC answering: claims all the same",
	          pinClaimCheck(PinUser::Bridge, PIN_SIG, &f) == ClaimRefusal::None);
	f.kissLastMs = now - 10;
	checkTrue("KISS live: SIG alone asked, claims",
	          pinClaimCheck(PinUser::Bridge, PIN_SIG, &f) == ClaimRefusal::None);
	f.armed = true;
	checkTrue("armed: refused", pinClaimCheck(PinUser::Bridge, PIN_SIG, &f) == ClaimRefusal::Armed);
	f.armed = false;
	f.held = true;
	checkTrue("another claim: refused", pinClaimCheck(PinUser::Bridge, PIN_SIG, &f) == ClaimRefusal::Busy);
}

/** @brief Begin to Free through the job table, DShot never with the line. */
static void testClaimCycle() {
	section("USB bridge: the claim, Stopping to Free");

	uiInit();
	fakeBridgeLineReset();
	bridgeWireKeepEcho(true);
	freshPump(0, 1, true);
	g_together = 0;
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis() - 10;
	fakeSetTelemetry(&t);
	checkTrue("an ESC answering on SIG: the claim begins",
	          pinClaimBegin(PinUser::Bridge, PIN_SIG) == ClaimRefusal::None);
	checkTrue("Stopping", pinClaimState() == ClaimState::Stopping);
	checkTrue("the slot closed while Stopping", !bridgeWireOpen());
	pass();
	pass();
	checkTrue("Running", pinClaimState() == ClaimState::Running);
	checkTrue("the bridge's job runs", core1JobRunning() == Core1Job::Bridge);
	checkInt("its line on the latched SIG, GP0", fakeBridgeLinePin(), 0);
	checkTrue("open", bridgeWireOpen());

	uint8_t big[100];
	for (int k = 0; k < 100; k++) big[k] = (uint8_t)(k ^ 0x5A);
	bridgeWireSend(big, sizeof(big));
	for (int k = 0; k < 10; k++) pass();
	const int resumes = fakeResumeCount();
	pinClaimEnd(PinUser::Bridge);
	checkTrue("BACK: Leaving", pinClaimState() == ClaimState::Leaving);
	checkInt("DShot not asked back while the job holds the pin", fakeResumeCount(), resumes);
	checkTrue("no leave page", !pinClaimLeavePage() && !pinClaimUnplugDue());
	int leaving = 0;
	while (pinClaimState() == ClaimState::Leaving && leaving < 300) {
		pass();
		leaving++;
	}
	checkTrue("Leaving lasts while the frame goes out", leaving > 30 && leaving < 250);
	checkTrue("the frame whole on the wire", wireIs(0, big, 100));
	checkInt("nothing cut", fakeBridgeLineCuts(), 0);
	checkInt("DShot asked back once", fakeResumeCount(), resumes + 1);
	checkTrue("with the job idle", fakeResumeJobIdle());
	for (int k = 0; k < 3 && pinClaimState() != ClaimState::Free; k++) pass();
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	checkTrue("DShot up", !escTaskSuspended());
	checkTrue("the line opened and closed once", fakeBridgeLineOpens() == 1 &&
	                                             fakeBridgeLineCloses() == 1);
	checkInt("DShot never bound while the line was open", g_together, 0);

	// BACK, and the screen gone: nothing polls the claim, only looks at it.
	freshPump(0, 1, true);
	checkTrue("again: Running", claimToRunning() == ClaimState::Running);
	bridgeWireSend(big, sizeof(big));
	for (int k = 0; k < 10; k++) pass();
	const int r1 = fakeResumeCount();
	pinClaimEnd(PinUser::Bridge);
	int looks = 0;
	while (pinClaimState() != ClaimState::Free && looks < 300) {
		passLooking();
		looks++;
	}
	checkTrue("no poll after BACK: Free all the same", pinClaimState() == ClaimState::Free);
	checkTrue("once the frame was out", looks > 30 && looks < 250 && wireIs(100, big, 100));
	checkInt("DShot asked back once", fakeResumeCount(), r1 + 1);
	checkTrue("with the job idle, and DShot up", fakeResumeJobIdle() && !escTaskSuspended());
	checkInt("DShot never with the line", g_together, 0);

	// SIG on GP1: the line follows the latched wiring.
	freshPump(1, 0, false);
	checkTrue("SIG on GP1: Running", claimToRunning() == ClaimState::Running);
	checkInt("the line on GP1", fakeBridgeLinePin(), 1);
	pinClaimEnd(PinUser::Bridge);
	for (int k = 0; k < 5 && pinClaimState() != ClaimState::Free; k++) pass();
	checkTrue("an idle line: Free within 5 passes", pinClaimState() == ClaimState::Free);
	freshPump(0, 1, true);

	// BACK before the bind: no line, DShot at once.
	fakeBridgeLineReset();
	pinClaimBegin(PinUser::Bridge, PIN_SIG);
	const int r2 = fakeResumeCount();
	pinClaimEnd(PinUser::Bridge);
	checkInt("BACK while Stopping: DShot at once", fakeResumeCount(), r2 + 1);
	for (int k = 0; k < 3 && pinClaimState() != ClaimState::Free; k++) pass();
	checkTrue("Free, the line never opened", pinClaimState() == ClaimState::Free &&
	                                         fakeBridgeLineOpens() == 0);
	checkInt("DShot never with the line", g_together, 0);
}

/** @brief The leave page per claim, and RETRY after a refused bind. */
static void testLeavePageAndRetry() {
	section("USB bridge: the leave page per claim, RETRY");

	fakeBridgeLineReset();
	freshPump(0, 1, true);
	g_together = 0;
	checkTrue("Running", claimToRunning() == ClaimState::Running);
	pinClaimWantLeavePage(PinUser::Sbus);
	checkTrue("asked by another screen: no page", !pinClaimLeavePage());
	pinClaimWantLeavePage(PinUser::Bridge);
	checkTrue("an unfinished write asks for the page", pinClaimLeavePage());
	const int resumes = fakeResumeCount();
	pinClaimEnd(PinUser::Bridge);
	checkTrue("BACK: Leaving", pinClaimState() == ClaimState::Leaving);
	checkTrue("the page not due while the job lets go", !pinClaimUnplugDue());
	for (int k = 0; k < 20; k++) pass();
	checkTrue("due once the job is idle", pinClaimUnplugDue() && !bridgeLineIsOpen());
	checkInt("DShot still down", fakeResumeCount(), resumes);
	checkTrue("Leaving, suspended", pinClaimState() == ClaimState::Leaving && escTaskSuspended());

	// The page's BRIDGE: keep the claim and return to the bridge.
	pinClaimKeep(PinUser::Bridge);
	pass();
	pass();
	checkTrue("kept: Running again", pinClaimState() == ClaimState::Running);
	checkInt("the line opened a second time", fakeBridgeLineOpens(), 2);
	checkTrue("the page kept with the claim", pinClaimLeavePage());
	pinClaimEnd(PinUser::Bridge);
	for (int k = 0; k < 20; k++) pass();
	checkTrue("due again", pinClaimUnplugDue());
	pinClaimRelease(PinUser::Sbus);
	checkInt("RESTART from another screen: nothing", fakeResumeCount(), resumes);
	pinClaimRelease(PinUser::Bridge);
	checkInt("RESTART DSHOT resumes", fakeResumeCount(), resumes + 1);
	for (int k = 0; k < 3 && pinClaimState() != ClaimState::Free; k++) pass();
	checkTrue("Free, the page gone", pinClaimState() == ClaimState::Free && !pinClaimLeavePage());
	checkInt("DShot never with the line", g_together, 0);

	// A refused bind: Held with the error, RETRY, BACK.
	fakeBridgeLineReset();
	fakeBridgeLineRefuse(CORE1_JOB_PIO);
	checkTrue("a refused bind stays Held", claimToRunning() == ClaimState::Held);
	checkInt("with the PIO error", pinClaimJobError(), CORE1_JOB_PIO);
	checkTrue("not open", !bridgeWireOpen());
	fakeBridgeLineRefuse(0);
	pinClaimRetry(PinUser::Bridge);
	pass();
	pass();
	checkTrue("RETRY: Running", pinClaimState() == ClaimState::Running && bridgeWireOpen());
	pinClaimEnd(PinUser::Bridge);
	for (int k = 0; k < 5 && pinClaimState() != ClaimState::Free; k++) pass();
	checkTrue("Free", pinClaimState() == ClaimState::Free);

	fakeBridgeLineRefuse(CORE1_JOB_CLOCK);
	checkTrue("a clock refused: Held", claimToRunning() == ClaimState::Held &&
	                                   pinClaimJobError() == CORE1_JOB_CLOCK);
	const int r2 = fakeResumeCount();
	pinClaimEnd(PinUser::Bridge);
	checkInt("BACK from a refused bind: DShot within the call", fakeResumeCount(), r2 + 1);
	for (int k = 0; k < 3 && pinClaimState() != ClaimState::Free; k++) pass();
	checkTrue("Free", pinClaimState() == ClaimState::Free);
	checkInt("opens and closes balance", fakeBridgeLineOpens(), fakeBridgeLineCloses());
	checkInt("DShot never with the line", g_together, 0);
	fakeBridgeLineRefuse(0);
}

void runBridgeTests() {
	FakeWiring w = fakeWiring();
	testClock();
	testSlot();
	testLine();
	testOverruns();
	testRelease();
	testPinUser();
	testClaimCycle();
	testLeavePageAndRetry();
	fakeBridgeLineReset();
	freshPump(w.dshotPin, w.kissPin, w.kissEnable);
	escTaskConfigure(w.dshotPin, w.dshotKbaud, w.kissEnable, w.kissPin);
}
