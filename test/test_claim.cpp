/**
 * @file test_claim.cpp
 * @brief The pin claim and the core1 job dispatch, against the fake pump.
 *
 * pin_claim.cpp is linked as it ships. core1_job.cpp is included rather than
 * linked, so the tests can put fake jobs in its job table: the claim's job
 * path is exercised with jobs that record what the claim and the dispatch did
 * to them, and with no job at all. The servo job the table ships with is
 * tested in test_servo.cpp. The Makefile therefore must not compile
 * core1_job.cpp separately.
 *
 * The fake pump (fakes.cpp) makes escTaskPoll()'s handover and frame
 * decisions with the shipped functions: escHandoverStep(), escDrainSlot(),
 * escPumpSlot(), escPumpFrameSent() and escPumpBind(). fakes.cpp lists what it
 * does not model. Core1 is not a thread here: a test runs
 * core1's side by calling fakePumpPoll() and core1JobPoll() itself, which is
 * how the order of the two cores' steps is put under test. testHandoverSlots()
 * runs the same functions one 1 ms slot at a time.
 */

#include "check.h"
#include "fakes.h"
#include "config.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "settings.h"
#include "ui.h"
#include "ui_widgets.h"
#include "ui_am32.h"
#include "plat.h"

#include "../src/core1_job.cpp"   // NOLINT -- see the note above

#include <string.h>

// ---------------------------------------------------------------------------
// fake jobs
// ---------------------------------------------------------------------------

/** @brief What one fake job saw. */
struct FakeJob {
	int      binds;          /**< bind() calls. */
	int      steps;          /**< step() calls. */
	int      releases;       /**< release() calls. */
	uint8_t  bindResult;     /**< What bind() returns. */
	bool     suspendedAtBind;/**< escTaskSuspended() inside the last bind(). */
	uint8_t  sig;            /**< SIG GPIO of the last bind(). */
	uint8_t  aux;            /**< AUX GPIO of the last bind(). */
	bool     uiAlive;        /**< uiAlive of the last step(). */
	bool     stopInBind;     /**< Core0 asks for a stop while bind() runs. */
	int      releaseWaits;   /**< release() returns false this many times first. */
};

static FakeJob g_servoJob, g_kstJob;

static uint8_t bindInto(FakeJob *j, uint8_t sig, uint8_t aux) {
	j->binds++;
	j->suspendedAtBind = escTaskSuspended();
	j->sig = sig;
	j->aux = aux;
	if (j->stopInBind) core1JobStop();
	return j->bindResult;
}
static uint8_t servoBind(uint8_t sig, uint8_t aux) { return bindInto(&g_servoJob, sig, aux); }
static void servoStep(uint32_t, bool alive) { g_servoJob.steps++; g_servoJob.uiAlive = alive; }
/** @brief One release() call: false while a wait is owed, then true. */
static bool releaseFrom(FakeJob *j) {
	if (j->releaseWaits > 0) { j->releaseWaits--; return false; }
	j->releases++;
	return true;
}
static bool servoRelease(uint32_t) { return releaseFrom(&g_servoJob); }
static uint8_t kstBind(uint8_t sig, uint8_t aux) { return bindInto(&g_kstJob, sig, aux); }
static void kstStep(uint32_t, bool alive) { g_kstJob.steps++; g_kstJob.uiAlive = alive; }
static bool kstRelease(uint32_t) { return releaseFrom(&g_kstJob); }

static const Core1JobOps FAKE_SERVO = { servoBind, servoStep, servoRelease, nullptr };
static const Core1JobOps FAKE_KST   = { kstBind, kstStep, kstRelease, nullptr };
static int g_inits = 0;
static void kstInit() { g_inits++; }
static const Core1JobOps FAKE_KST_INIT = { kstBind, kstStep, kstRelease, kstInit };

/** @brief Build both jobs, with fresh records. */
static void installJobs() {
	memset(&g_servoJob, 0, sizeof(g_servoJob));
	memset(&g_kstJob, 0, sizeof(g_kstJob));
	s_ops[(uint8_t)Core1Job::Servo] = &FAKE_SERVO;
	s_ops[(uint8_t)Core1Job::Kst]   = &FAKE_KST;
}

/** @brief No job in the table, as KST ships. */
static void removeJobs() {
	s_ops[(uint8_t)Core1Job::Servo] = nullptr;
	s_ops[(uint8_t)Core1Job::Kst]   = nullptr;
}

/** @brief The table as it ships: the servo job, and no KST job. */
static void shippedJobs() {
	s_ops[(uint8_t)Core1Job::Servo] = &SERVO_JOB_OPS;
	s_ops[(uint8_t)Core1Job::Kst]   = nullptr;
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

#define SIG_GPIO 29
#define AUX_GPIO 28

/** @brief No ESC on either wire. */
static void quiet() {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
}

/** @brief An ESC whose last eRPM frame is @p ageMs old. */
static void escAnswered(uint32_t ageMs) {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis() - ageMs;
	fakeSetTelemetry(&t);
}

/** @brief A KISS wire whose last frame is @p ageMs old. */
static void kissAnswered(uint32_t ageMs) {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.kissLastMs = millis() - ageMs;
	t.haveKiss = true;
	fakeSetTelemetry(&t);
}

/** @brief One pass of core1's pump. */
static void pump() { fakePumpPoll(); }

/** @brief The board's default wiring, KISS on, the pump bound, past any drain. */
static void freshPump() {
	escTaskConfigure(SIG_GPIO, 600, true, AUX_GPIO);
	quiet();
	pump();
	fakeAdvance(1000);
	pump();
}

/**
 * @brief Drive any claim back to Free, playing both cores and the unplug
 *        page, and report whether it got there.
 */
static bool toFree() {
	PinUser who = pinClaimHolder();
	pinClaimEnd(who);
	for (int i = 0; i < 4 && pinClaimState() != ClaimState::Free; i++) {
		pump();
		core1JobPoll();
		pinClaimPoll();
		pinClaimRestart(who);
	}
	return pinClaimState() == ClaimState::Free;
}

/** @brief One UI frame of the real UI, with core1's pump pass before it. */
static void frames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); pump(); uiTick(); }
}

#define UI_IDS_FRAMES(n) frames(n)
#include "ui_ids.h"

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

/** @brief Each refusal rule of pinClaimCheck(), at its 500 ms boundary. */
static void testCheckRules() {
	section("Pin claim: refusal rules");

	const uint32_t now = 100000;
	ClaimFacts f = {false, false, now, 0, 0, SIG_GPIO, AUX_GPIO};
	const uint8_t both = PIN_SIG | PIN_AUX;

	checkTrue("quiet and disarmed: SERVO claims",
	          pinClaimCheck(PinUser::Servo, both, &f) == ClaimRefusal::None);

	f.armed = true;
	checkTrue("armed: AM32 refused", pinClaimCheck(PinUser::Am32, PIN_SIG, &f) == ClaimRefusal::Armed);
	checkTrue("armed: SERVO refused", pinClaimCheck(PinUser::Servo, PIN_SIG, &f) == ClaimRefusal::Armed);
	checkTrue("armed: KST refused", pinClaimCheck(PinUser::Kst, PIN_SIG, &f) == ClaimRefusal::Armed);
	f.held = true;
	checkTrue("armed outranks busy", pinClaimCheck(PinUser::Am32, PIN_SIG, &f) == ClaimRefusal::Armed);
	f.armed = false;
	checkTrue("a claim holds: busy", pinClaimCheck(PinUser::Am32, PIN_SIG, &f) == ClaimRefusal::Busy);
	f.lastRpmMs = now;
	checkTrue("busy outranks an answering ESC",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &f) == ClaimRefusal::Busy);
	f.held = false;

	f.lastRpmMs = now - 499;
	checkTrue("eRPM 499 ms ago: SERVO waits",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &f) == ClaimRefusal::EscAnswering);
	checkTrue("eRPM 499 ms ago: KST waits",
	          pinClaimCheck(PinUser::Kst, PIN_SIG, &f) == ClaimRefusal::EscAnswering);
	checkTrue("eRPM 499 ms ago: AM32 is exempt",
	          pinClaimCheck(PinUser::Am32, PIN_SIG, &f) == ClaimRefusal::None);
	f.lastRpmMs = now - 500;
	checkTrue("eRPM 500 ms ago: SERVO claims",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &f) == ClaimRefusal::None);
	ClaimFacts boot = {false, false, 0, 0, 0, SIG_GPIO, AUX_GPIO};
	checkTrue("no eRPM ever, at millis() 0: claims",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &boot) == ClaimRefusal::None);
	f.lastRpmMs = 0;

	f.kissLastMs = now - 499;
	checkTrue("KISS 499 ms ago, AUX asked: refused",
	          pinClaimCheck(PinUser::Servo, both, &f) == ClaimRefusal::KissLive);
	checkTrue("KISS 499 ms ago, SIG only: claims",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &f) == ClaimRefusal::None);
	f.kissLastMs = now - 500;
	checkTrue("KISS 500 ms ago, AUX asked: claims",
	          pinClaimCheck(PinUser::Servo, both, &f) == ClaimRefusal::None);
	f.kissLastMs = 0;

	f.auxGpio = SIG_GPIO;
	checkTrue("one GPIO for both, AUX asked: refused",
	          pinClaimCheck(PinUser::Servo, both, &f) == ClaimRefusal::SamePin);
	checkTrue("one GPIO for both, SIG only: claims",
	          pinClaimCheck(PinUser::Servo, PIN_SIG, &f) == ClaimRefusal::None);

	f.kissLastMs = now - 10;
	checkTrue("KISS live outranks one GPIO for both",
	          pinClaimCheck(PinUser::Servo, both, &f) == ClaimRefusal::KissLive);
	f.auxGpio = AUX_GPIO;
	f.lastRpmMs = now - 10;
	checkTrue("an answering ESC outranks KISS live",
	          pinClaimCheck(PinUser::Servo, both, &f) == ClaimRefusal::EscAnswering);
}

/** @brief pinClaimBegin() against the live state: a refusal changes nothing. */
static void testBeginRefusals() {
	section("Pin claim: refusals change nothing");

	uiInit();
	freshPump();
	removeJobs();
	checkControlTable("tester, disarmed");

	// Armed through the UI, which is the arm state the claim reads.
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); frames(2);
	checkTrue("armed through the UI", uiArmed());
	checkControlTable("tester, armed");
	int resumes = fakeResumeCount();
	checkTrue("armed: refused", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Armed);
	checkTrue("the claim stays free", pinClaimState() == ClaimState::Free);
	checkTrue("DShot keeps the pins", fakePinReturned());
	checkTrue("and the claim does not disarm", fakeArmed() && uiArmed());
	pressId(UI_FIX(TesterCtl::Arm));
	fakeRelease(); frames(2);
	checkTrue("disarmed again", !uiArmed());

	fakeAdvance(1000);
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("a second AM32 claim: busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("a SERVO claim beside it: busy",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("the holder is AM32", pinClaimHolder() == PinUser::Am32);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK from a screen that does not hold it does nothing",
	          pinClaimHolder() == PinUser::Am32 && fakeResumeCount() == resumes);
	checkTrue("back to free", toFree());

	escAnswered(499);
	checkTrue("ESC answering: SERVO waits",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::EscAnswering);
	checkTrue("DShot still runs", fakePinReturned());
	fakeAdvance(1);
	checkTrue("500 ms after the last eRPM: it claims",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	checkTrue("back to free", toFree());

	escAnswered(10);
	checkTrue("ESC answering: KST waits",
	          pinClaimBegin(PinUser::Kst, PIN_SIG) == ClaimRefusal::EscAnswering);
	checkTrue("ESC answering: AM32 claims",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("back to free", toFree());

	kissAnswered(499);
	checkTrue("KISS live: two servos wait",
	          pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX) == ClaimRefusal::KissLive);
	checkTrue("KISS live: one servo claims",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	checkTrue("back to free", toFree());

	quiet();
	escTaskConfigure(AUX_GPIO, 600, false, AUX_GPIO);
	checkTrue("ESC pin on the KISS GPIO: two servos refused",
	          pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX) == ClaimRefusal::SamePin);
	checkTrue("one servo claims",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	checkTrue("back to free", toFree());
	freshPump();
}

/** @brief The handover rules in esc_task.h, one by one. */
static void testHandoverRules() {
	section("DShot handover: the rules");

	checkTrue("no throttle frame since the bind: no drain",
	          escDrainDone(0, 12345, ARM_ZERO_THROTTLE_MS));
	checkTrue("250 ms after a throttle frame: still draining",
	          !escDrainDone(5000, 5250, ARM_ZERO_THROTTLE_MS));
	checkTrue("251 ms after: done", escDrainDone(5000, 5251, ARM_ZERO_THROTTLE_MS));
	// Stamped 16 ms before the rollover. At 10 ms the clock has not wrapped
	// but stamp + 250 ms has; the checks at 250 and 251 ms are past the wrap.
	checkTrue("stamp + 250 ms wraps, clock does not: draining at 10 ms",
	          !escDrainDone(0xFFFFFFF0u, 0xFFFFFFF0u + 10u, ARM_ZERO_THROTTLE_MS));
	checkTrue("draining at 250 ms",
	          !escDrainDone(0xFFFFFFF0u, 0xFFFFFFF0u + 250u, ARM_ZERO_THROTTLE_MS));
	checkTrue("and done at 251 ms",
	          escDrainDone(0xFFFFFFF0u, 0xFFFFFFF0u + 251u, ARM_ZERO_THROTTLE_MS));

	EscFrame zero = {false, 0, 0, false};
	EscFrame thr  = {false, 0, 1, false};
	EscFrame cmd  = {true, DSHOT_CMD_BEEP1, 0, false};
	checkInt("a zero frame keeps the stamp", escThrottleStamp(&zero, 77, 900), 77);
	checkInt("a command frame keeps it", escThrottleStamp(&cmd, 77, 900), 77);
	checkInt("a throttle frame stamps it", escThrottleStamp(&thr, 77, 900), 900);
	checkInt("a throttle frame at millis() 0 stamps 1", escThrottleStamp(&thr, 0, 0), 1);

	checkTrue("no request: run, even right after a throttle frame",
	          escHandoverStep(false, false, true, 5000, 5001) == EscHandover::Run);
	checkTrue("a suspend 250 ms after a throttle frame: drain",
	          escHandoverStep(true, false, true, 5000, 5250) == EscHandover::Drain);
	checkTrue("251 ms after: teardown",
	          escHandoverStep(true, false, true, 5000, 5251) == EscHandover::Teardown);
	checkTrue("a rebuild drains too",
	          escHandoverStep(false, true, true, 5000, 5100) == EscHandover::Drain);
	checkTrue("nothing bound: teardown at once",
	          escHandoverStep(true, false, false, 5000, 5001) == EscHandover::Teardown);
	checkTrue("no throttle frame since the bind: teardown at once",
	          escHandoverStep(true, false, true, 0, 5001) == EscHandover::Teardown);

	EscSlot d = escDrainSlot();
	checkTrue("a drain slot is throttle zero, no command, no KISS request",
	          !d.frame.sendCommand && d.frame.throttle == 0 && !d.frame.requestKiss);
	checkTrue("and takes no beacon and forgets no EDT", !d.beepTaken && !d.forgetEdt);

	EscPump p;
	memset(&p, 0xA5, sizeof(p));
	volatile bool armed = true;
	volatile uint16_t throttle = 700;
	escPumpBind(&p, &armed, &throttle);
	checkTrue("a bind disarms", !armed);
	checkInt("and zeroes the throttle", throttle, 0);
	checkInt("and forgets the last throttle frame", p.lastThrottleMs, 0);
	checkInt("and queues the EDT enable", p.edt.reps, EDT_ENABLE_REPEATS);
	escPumpFrameSent(&p, &thr, 900);
	checkInt("a throttle frame sent stamps the pump", p.lastThrottleMs, 900);
	escPumpFrameSent(&p, &zero, 950);
	checkInt("a zero frame does not", p.lastThrottleMs, 900);
}

/**
 * @brief escTaskPoll()'s decisions, one 1 ms slot at a time, without the PIO.
 *
 * The same functions escTaskPoll() calls, in its order: escHandoverStep(),
 * escPumpBind(), escDrainSlot() or escPumpSlot(), escPumpFrameSent(). What is
 * modelled is core0's requests and the clock. Not modelled: the PIO, the
 * replies and their fold into the telemetry, the KISS receiver and the
 * heartbeat (uiAlive is true).
 */
struct HandoverSim {
	EscPump      pump;
	EscTelemetry tel;
	bool     armed;               /**< core0's s_armed. */
	uint16_t throttle;            /**< core0's s_throttle. */
	uint8_t  beepReq;             /**< core0's s_beepReq. */
	bool     suspendReq;          /**< core0's s_suspendReq. */
	bool     rebuildReq;          /**< core0's s_rebuildReq. */
	bool     pumpUp;              /**< s_pumpUp. */
	bool     suspended;           /**< s_suspended. */
	uint32_t ms;                  /**< millis() of the slot about to run. */
	int      binds;               /**< Binds so far. */
	int      teardowns;           /**< Teardowns so far. */
	uint32_t teardownMs;          /**< millis() of the last teardown. */
	uint32_t lastThrottleFrameMs; /**< millis() of the last frame with a throttle. */
	int      drainFrames;         /**< Frames sent while draining. */
	int      badDrainFrames;      /**< ...that carried a throttle, a command or a request. */
	EscFrame last;                /**< The last frame sent. */
};

static void hsSlot(HandoverSim *h) {
	EscHandover act = escHandoverStep(h->suspendReq, h->rebuildReq, h->pumpUp,
	                                  h->pump.lastThrottleMs, h->ms);
	if (act == EscHandover::Teardown) {
		h->pumpUp = false;
		h->rebuildReq = false;
		h->teardowns++;
		h->teardownMs = h->ms;
		if (h->suspendReq) {
			h->suspended = true;
			h->ms++;
			return;
		}
	}
	if (!h->suspendReq && !h->pumpUp) {
		escPumpBind(&h->pump, &h->armed, &h->throttle);
		h->pumpUp = true;
		h->suspended = false;
		h->binds++;
	}
	EscSlot s = act == EscHandover::Drain
	          ? escDrainSlot()
	          : escPumpSlot(&h->pump, &h->tel, h->armed, true, h->throttle,
	                        h->beepReq, false, h->ms);
	if (s.beepTaken) h->beepReq = 0;
	if (act == EscHandover::Drain) {
		h->drainFrames++;
		if (s.frame.sendCommand || s.frame.throttle || s.frame.requestKiss)
			h->badDrainFrames++;
	}
	if (!s.frame.sendCommand && s.frame.throttle) h->lastThrottleFrameMs = h->ms;
	escPumpFrameSent(&h->pump, &s.frame, h->ms);
	h->last = s.frame;
	h->ms++;
}

static void hsRun(HandoverSim *h, int n) {
	for (int i = 0; i < n; i++) hsSlot(h);
}

static HandoverSim s_hs;

static void testHandoverSlots() {
	section("DShot handover: slot by slot on the shipped rules");

	HandoverSim *h = &s_hs;
	memset(h, 0, sizeof(*h));
	h->ms = 10000;
	h->armed = true;      // whatever core0 last commanded
	h->throttle = 900;
	hsSlot(h);
	checkTrue("the bind starts disarmed, whatever core0 last commanded",
	          h->binds == 1 && !h->armed && h->throttle == 0);
	checkTrue("with the EDT enable as its first frame",
	          h->last.sendCommand && h->last.command == DSHOT_CMD_EDT_ENABLE);
	hsRun(h, 20);

	h->armed = true;
	h->throttle = 300;
	hsRun(h, 100);
	checkInt("armed, the pump sends the throttle", h->last.throttle, 300);

	// escTaskSuspend(): escSetArmed(false) zeroes the throttle, then the
	// request. A beacon asked for at the same moment is left for later.
	h->armed = false;
	h->throttle = 0;
	h->beepReq = 1;
	h->suspendReq = true;
	uint32_t last = h->lastThrottleFrameMs;
	for (int i = 0; i < 1000 && !h->suspended; i++) hsSlot(h);
	checkTrue("suspended", h->suspended);
	checkInt("the teardown is 251 ms after the last throttle frame",
	         (long)(h->teardownMs - last), 251);
	checkInt("every slot between is a drain frame", h->drainFrames, 250);
	checkInt("each throttle zero, no command, no KISS request", h->badDrainFrames, 0);
	checkInt("the drain takes no beacon", h->beepReq, 1);

	h->beepReq = 0;
	h->suspendReq = false;
	hsSlot(h);
	checkTrue("a resume binds again, disarmed",
	          h->binds == 2 && !h->armed && !h->suspended);
	checkTrue("with the EDT enable first", h->last.sendCommand &&
	                                       h->last.command == DSHOT_CMD_EDT_ENABLE);

	// A SETUP pin change: escTaskConfigure() disarms, then the rebuild.
	hsRun(h, 20);
	h->armed = true;
	h->throttle = 200;
	hsRun(h, 50);
	h->armed = false;
	h->throttle = 0;
	h->rebuildReq = true;
	h->drainFrames = 0;
	last = h->lastThrottleFrameMs;
	for (int i = 0; i < 1000 && h->teardowns < 2; i++) hsSlot(h);
	checkInt("a rebuild tears down 251 ms after the last throttle frame",
	         (long)(h->teardownMs - last), 251);
	checkInt("after 250 drain frames", h->drainFrames, 250);
	checkTrue("and binds again in the same pass", h->binds == 3 && h->pumpUp &&
	                                              !h->suspended && !h->armed);

	// Nothing to drain: no throttle frame since that bind.
	h->suspendReq = true;
	uint32_t at = h->ms;
	hsSlot(h);
	checkTrue("a suspend with no throttle since the bind tears down at once",
	          h->suspended && h->teardownMs == at);
}

/** @brief The drain, through the claim and the fake pump. */
static void testDrain() {
	section("Pin claim: the drain before teardown");

	uiInit();
	freshPump();
	removeJobs();
	escSetArmed(true);
	escSetThrottle(300);
	pump();
	uint32_t stamp = fakeLastThrottleMs();
	checkInt("one throttle frame out", stamp, millis());
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("the claim forces a disarm", !fakeArmed() && fakeThrottle() == 0);
	pump();
	checkTrue("stopping", pinClaimPoll() == ClaimState::Stopping);
	fakeAdvance(250);
	pump();
	checkTrue("no teardown 250 ms after the throttle frame",
	          pinClaimPoll() == ClaimState::Stopping && !escTaskSuspended());
	checkTrue("no GPIO latched yet", pinClaimGpio(PIN_SIG) == PIN_CLAIM_NO_GPIO);
	fakeAdvance(1);
	pump();
	checkTrue("held at 251 ms", pinClaimPoll() == ClaimState::Held);
	checkTrue("back to free", toFree());

	fakeAdvance(5000);
	checkTrue("after a long disarm", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held after one pump pass", pinClaimPoll() == ClaimState::Held);
	checkTrue("back to free", toFree());
}

/** @brief Restoring lasts until core1 has rebuilt the pump. */
static void testRestoring() {
	section("Pin claim: restoring until the pump runs");

	uiInit();
	freshPump();
	removeJobs();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	int resumes = fakeResumeCount();
	int rebuilds = fakeRebuildCount();
	pinClaimEnd(PinUser::Am32);
	checkInt("BACK asks for DShot", fakeResumeCount(), resumes + 1);
	checkTrue("restoring until core1 rebuilds the pump",
	          pinClaimState() == ClaimState::Restoring);
	checkTrue("still AM32's", pinClaimHolder() == PinUser::Am32);
	checkTrue("a SERVO claim while restoring: busy",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("a new AM32 claim while restoring: busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Busy);
	pinClaimEnd(PinUser::Am32);
	checkTrue("BACK while restoring does nothing",
	          pinClaimState() == ClaimState::Restoring && fakeResumeCount() == resumes + 1);
	checkTrue("nor does a poll", pinClaimPoll() == ClaimState::Restoring);
	pump();
	checkInt("no GPIO once the pump runs, asked before anything else",
	         pinClaimGpio(PIN_SIG), PIN_CLAIM_NO_GPIO);
	checkInt("core1 rebuilds once", fakeRebuildCount(), rebuilds + 1);
	checkTrue("free once the pump runs", pinClaimState() == ClaimState::Free &&
	                                     pinClaimHolder() == PinUser::None);
}

/** @brief The order of the two cores' steps through a SERVO claim. */
static void testOrder() {
	section("Pin claim: order of the handover");

	uiInit();
	freshPump();
	installJobs();
	int resumes = fakeResumeCount();

	checkTrue("two servos claim",
	          pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX) == ClaimRefusal::None);
	checkTrue("no job before the pins are free", core1JobRunning() == Core1Job::None);
	checkTrue("stopping until core1 lets go", pinClaimPoll() == ClaimState::Stopping);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	checkInt("SIG latched", pinClaimGpio(PIN_SIG), SIG_GPIO);
	checkInt("AUX latched", pinClaimGpio(PIN_AUX), AUX_GPIO);
	checkInt("an unknown role names no GPIO", pinClaimGpio(0x04), PIN_CLAIM_NO_GPIO);
	checkTrue("the servo job is asked for", core1JobRunning() == Core1Job::Servo &&
	                                        core1JobPhase() == Core1JobPhase::Starting);
	checkTrue("still held until it binds", pinClaimPoll() == ClaimState::Held);

	core1JobPoll();
	checkInt("core1 binds once", g_servoJob.binds, 1);
	checkTrue("DShot was released before the bind", g_servoJob.suspendedAtBind);
	checkTrue("with both pins", g_servoJob.sig == SIG_GPIO && g_servoJob.aux == AUX_GPIO);
	checkTrue("running", pinClaimPoll() == ClaimState::Running);
	core1JobPoll();
	core1JobPoll();
	checkInt("core1 steps the job every pass", g_servoJob.steps, 2);

	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK: leaving", pinClaimState() == ClaimState::Leaving);
	checkTrue("the job is asked to stop", core1JobPhase() == Core1JobPhase::Stopping);
	checkTrue("leaving until core1 releases", pinClaimPoll() == ClaimState::Leaving);
	checkTrue("no unplug page while the job holds the pins", !pinClaimUnplugDue());
	pinClaimRestart(PinUser::Servo);
	checkInt("RESTART DSHOT before the job is idle does nothing",
	         fakeResumeCount(), resumes);

	core1JobPoll();
	checkInt("core1 releases once", g_servoJob.releases, 1);
	checkTrue("the job is idle", core1JobRunning() == Core1Job::None);
	checkTrue("still leaving", pinClaimPoll() == ClaimState::Leaving);
	checkTrue("the unplug page is due", pinClaimUnplugDue());
	pinClaimPoll();
	pinClaimPoll();
	checkInt("no DShot until RESTART DSHOT", fakeResumeCount(), resumes);
	pinClaimRestart(PinUser::Am32);
	checkInt("RESTART from a screen that does not hold it does nothing",
	         fakeResumeCount(), resumes);

	pinClaimRestart(PinUser::Servo);
	checkInt("RESTART DSHOT resumes the pump", fakeResumeCount(), resumes + 1);
	checkTrue("with the job idle", fakeResumeJobIdle());
	checkTrue("restoring", pinClaimPoll() == ClaimState::Restoring);
	pump();
	checkTrue("free once the pump runs", pinClaimPoll() == ClaimState::Free);
	checkTrue("nothing held", pinClaimHolder() == PinUser::None);
	checkInt("no GPIO is latched after Free", pinClaimGpio(PIN_SIG), PIN_CLAIM_NO_GPIO);

	// KST, SIG only: the job gets no AUX pin.
	checkTrue("KST claims", pinClaimBegin(PinUser::Kst, PIN_SIG) == ClaimRefusal::None);
	pump();
	pinClaimPoll();
	core1JobPoll();
	checkTrue("the KST job binds SIG only",
	          g_kstJob.binds == 1 && g_kstJob.sig == SIG_GPIO &&
	          g_kstJob.aux == PIN_CLAIM_NO_GPIO);
	checkInt("AUX not asked: no GPIO", pinClaimGpio(PIN_AUX), PIN_CLAIM_NO_GPIO);
	checkTrue("running", pinClaimPoll() == ClaimState::Running);
	pinClaimEnd(PinUser::Kst);
	core1JobPoll();
	pinClaimPoll();
	checkTrue("KST gets the unplug page too", pinClaimUnplugDue());
	checkTrue("back to free", toFree());

	// AM32: no job, no unplug page, DShot asked for within the BACK call.
	resumes = fakeResumeCount();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	checkTrue("held with no job", core1JobRunning() == Core1Job::None);
	pinClaimPoll();
	checkTrue("AM32 stays held", pinClaimState() == ClaimState::Held);
	pinClaimEnd(PinUser::Am32);
	checkTrue("AM32 gets no unplug page", !pinClaimUnplugDue());
	checkInt("DShot resumes within BACK", fakeResumeCount(), resumes + 1);
	pump();
	checkTrue("free", pinClaimState() == ClaimState::Free);
	removeJobs();
}

/** @brief core1JobStart() and its guard, the dispatch's own rules. */
static void testJobGuard() {
	section("Core1 jobs: start is refused unless DShot is down");

	uiInit();
	freshPump();
	installJobs();

	checkTrue("DShot running: refused", !core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	core1JobPoll();
	checkInt("and core1 binds nothing", g_servoJob.binds, 0);
	checkTrue("still idle", core1JobPhase() == Core1JobPhase::Idle);
	core1JobStop();
	checkTrue("a stop while idle does nothing", core1JobPhase() == Core1JobPhase::Idle);

	escTaskSuspend();
	checkTrue("asked for, not yet down: refused",
	          !core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	pump();
	checkTrue("DShot down", escTaskSuspended());
	checkTrue("Core1Job::None: refused", !core1JobStart(Core1Job::None, SIG_GPIO, 0xFF));
	checkTrue("DShot down: started", core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	checkTrue("a second start while one is up: refused",
	          !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	core1JobPoll();
	checkTrue("a start while one runs: refused",
	          core1JobPhase() == Core1JobPhase::Running &&
	          !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	core1JobStop();
	checkTrue("a start while one stops: refused",
	          core1JobPhase() == Core1JobPhase::Stopping &&
	          !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	core1JobPoll();
	checkTrue("released and idle", g_servoJob.releases == 1 &&
	                               core1JobPhase() == Core1JobPhase::Idle);

	// A stop that lands while core1 is binding: the bind's pins are released
	// on the next pass, not leaked.
	g_servoJob = FakeJob{};
	g_servoJob.stopInBind = true;
	checkTrue("started again", core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	core1JobPoll();
	checkTrue("a stop during the bind leaves it stopping",
	          g_servoJob.binds == 1 && core1JobPhase() == Core1JobPhase::Stopping);
	core1JobPoll();
	checkTrue("and the next pass releases it", g_servoJob.releases == 1 &&
	                                          core1JobPhase() == Core1JobPhase::Idle);
	checkInt("it never stepped", g_servoJob.steps, 0);

	// A job that needs passes to let go stays Stopping until it says so.
	g_servoJob = FakeJob{};
	g_servoJob.releaseWaits = 2;
	checkTrue("started again", core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	core1JobPoll();
	core1JobStop();
	core1JobPoll();
	core1JobPoll();
	checkTrue("release() said not yet twice: still stopping",
	          core1JobPhase() == Core1JobPhase::Stopping && g_servoJob.releases == 0);
	core1JobPoll();
	checkTrue("the third pass lets go: idle", g_servoJob.releases == 1 &&
	                                          core1JobPhase() == Core1JobPhase::Idle);
	escTaskResume();
	pump();

	removeJobs();
	escTaskSuspend();
	pump();
	checkTrue("no job in the table: refused", !core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	checkTrue("nor KST", !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	shippedJobs();
	checkTrue("as it ships, the servo job starts",
	          core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	core1JobStop();
	core1JobPoll();
	checkTrue("and KST is refused", !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	escTaskResume();
	pump();
	checkTrue("idle, no error", core1JobRunning() == Core1Job::None &&
	                            core1JobError() == CORE1_JOB_OK);
}

/** @brief A SERVO claim into Held and its bind answered. */
static void heldWithBind() {
	pinClaimBegin(PinUser::Servo, PIN_SIG);
	pump();
	pinClaimPoll();
	core1JobPoll();
	pinClaimPoll();
}

/** @brief BACK in every state of a SERVO claim, and RETRY after a failed bind. */
static void testBackEveryState() {
	section("Pin claim: BACK in every state");

	uiInit();
	freshPump();
	installJobs();
	int resumes;

	// Stopping: a throttle frame went out on the last pump pass, so the drain
	// holds the claim.
	escSetArmed(true);
	escSetThrottle(200);
	pump();
	resumes = fakeResumeCount();
	checkTrue("SERVO claims during a drain",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("stopping", pinClaimPoll() == ClaimState::Stopping);
	pinClaimEnd(PinUser::Servo);
	checkInt("BACK while stopping: DShot back at once", fakeResumeCount(), resumes + 1);
	checkTrue("free: the pump never let go", pinClaimState() == ClaimState::Free);
	checkInt("and no job was ever bound", g_servoJob.binds, 0);
	fakeAdvance(1000);

	// Stopping, with the pump torn down before any poll has seen it.
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pump();
	resumes = fakeResumeCount();
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK while stopping after the teardown: restoring",
	          fakeResumeCount() == resumes + 1 &&
	          pinClaimState() == ClaimState::Restoring);
	pump();
	checkTrue("free once the pump runs", pinClaimState() == ClaimState::Free);
	checkInt("still no job bound", g_servoJob.binds, 0);

	// Held, the job asked for and not yet bound.
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK before the bind: leaving", pinClaimState() == ClaimState::Leaving);
	core1JobPoll();
	checkTrue("core1 binds nothing", g_servoJob.binds == 0 && g_servoJob.releases == 0);
	pinClaimPoll();
	checkTrue("unplug page", pinClaimUnplugDue());
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK again while leaving does nothing", pinClaimState() == ClaimState::Leaving);
	pinClaimRestart(PinUser::Servo);
	pump();
	checkTrue("free", pinClaimPoll() == ClaimState::Free);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK while free does nothing", pinClaimState() == ClaimState::Free);

	// Held, the bind failed, then BACK.
	g_servoJob.bindResult = CORE1_JOB_PWM;
	heldWithBind();
	checkTrue("the bind failed: still held", pinClaimState() == ClaimState::Held);
	checkInt("and says why", pinClaimJobError(), CORE1_JOB_PWM);
	checkTrue("binds nothing, so releases nothing", g_servoJob.releases == 0 &&
	                                               core1JobPhase() == Core1JobPhase::Idle);
	checkTrue("and core1 holds no job", core1JobRunning() == Core1Job::None);
	resumes = fakeResumeCount();
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK after a failed bind: the unplug page", pinClaimUnplugDue());
	pinClaimRetry(PinUser::Servo);
	checkTrue("RETRY while leaving does nothing",
	          core1JobPhase() == Core1JobPhase::Idle && pinClaimUnplugDue());
	pinClaimRestart(PinUser::Servo);
	checkInt("then DShot", fakeResumeCount(), resumes + 1);
	pump();
	checkTrue("free", pinClaimPoll() == ClaimState::Free);
	checkTrue("the next claim starts with no error",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None &&
	          pinClaimJobError() == CORE1_JOB_OK);
	checkTrue("back to free", toFree());

	// Held, the bind failed, then RETRY: the claim is kept.
	heldWithBind();
	checkInt("failed again", pinClaimJobError(), CORE1_JOB_PWM);
	int binds = g_servoJob.binds;
	resumes = fakeResumeCount();
	pinClaimRetry(PinUser::Am32);
	checkTrue("RETRY from a screen that does not hold it does nothing",
	          core1JobPhase() == Core1JobPhase::Idle);
	g_servoJob.bindResult = CORE1_JOB_OK;
	pinClaimRetry(PinUser::Servo);
	checkTrue("RETRY asks for the job again", core1JobPhase() == Core1JobPhase::Starting &&
	                                          pinClaimJobError() == CORE1_JOB_OK);
	pinClaimRetry(PinUser::Servo);
	checkTrue("a second RETRY while it starts does nothing",
	          core1JobPhase() == Core1JobPhase::Starting &&
	          pinClaimJobError() == CORE1_JOB_OK);
	core1JobPoll();
	checkTrue("it binds", g_servoJob.binds == binds + 1 &&
	                      pinClaimPoll() == ClaimState::Running);
	checkInt("and the last failure is cleared", core1JobError(), CORE1_JOB_OK);
	checkInt("DShot stayed down", fakeResumeCount(), resumes);
	pinClaimRetry(PinUser::Servo);
	checkTrue("RETRY while running does nothing", pinClaimState() == ClaimState::Running &&
	                                              core1JobPhase() == Core1JobPhase::Running);
	checkTrue("back to free", toFree());

	// Held, the start refused: no job in the table, as KST ships.
	removeJobs();
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	pinClaimPoll();
	pinClaimPoll();
	checkInt("the start was refused, and polls keep the reason",
	         pinClaimJobError(), CORE1_JOB_REFUSED);
	pinClaimRetry(PinUser::Servo);
	pinClaimPoll();
	pinClaimPoll();
	checkInt("RETRY is refused the same way", pinClaimJobError(), CORE1_JOB_REFUSED);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK: the unplug page", pinClaimUnplugDue());
	pinClaimRestart(PinUser::Servo);
	pump();
	checkTrue("free", pinClaimPoll() == ClaimState::Free);
	installJobs();

	// Running, then BACK on the unplug page: the claim is kept and the job
	// starts again.
	heldWithBind();
	checkTrue("running", pinClaimState() == ClaimState::Running);
	pinClaimRetry(PinUser::Servo);
	checkInt("RETRY with no failure does nothing", g_servoJob.binds, 1);
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	pinClaimPoll();
	resumes = fakeResumeCount();
	pinClaimKeep(PinUser::Am32);
	checkTrue("KEEP from a screen that does not hold it does nothing", pinClaimUnplugDue());
	pinClaimKeep(PinUser::Servo);
	checkTrue("BACK on the unplug page: held again", pinClaimState() == ClaimState::Held);
	checkInt("DShot stays down", fakeResumeCount(), resumes);
	pinClaimRestart(PinUser::Servo);
	checkTrue("RESTART is not due while held", pinClaimState() == ClaimState::Held);
	core1JobPoll();
	checkTrue("the job binds again", g_servoJob.binds == 2 &&
	                                 pinClaimPoll() == ClaimState::Running);
	pinClaimKeep(PinUser::Servo);
	checkTrue("KEEP while running does nothing", pinClaimState() == ClaimState::Running);
	checkTrue("back to free", toFree());
	checkInt("every bind released", g_servoJob.releases, 2);
	removeJobs();
}

/** @brief The rebuild after a claim uses the wiring from before it. */
static void testRestore() {
	section("Pin claim: DShot comes back as it was");

	uiInit();
	freshPump();
	installJobs();
	escTaskConfigure(SIG_GPIO, 300, true, AUX_GPIO);
	pump();
	FakeWiring before = fakeWiring();
	int configures = fakeConfigureCount();
	escSetArmed(true);   // the fake pump armed, the UI not

	checkTrue("two servos claim",
	          pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX) == ClaimRefusal::None);
	pump();
	pinClaimPoll();
	core1JobPoll();
	pinClaimPoll();
	checkTrue("back to free", toFree());

	FakeWiring at = fakeResumeWiring();
	checkInt("the rebuild binds the same ESC pin", at.dshotPin, before.dshotPin);
	checkInt("at the same bitrate", at.dshotKbaud, before.dshotKbaud);
	checkTrue("with KISS as it was", at.kissEnable == before.kissEnable);
	checkInt("on the same KISS pin", at.kissPin, before.kissPin);
	checkInt("the claim reconfigures nothing", fakeConfigureCount(), configures);
	checkTrue("the pump starts disarmed", !fakeArmed() && fakeThrottle() == 0);
	checkTrue("and so does the UI", !uiArmed());
	removeJobs();
	freshPump();
}

/** @brief The heartbeat rule, for the pump and for each job, across a rollover. */
static void testHeartbeat() {
	section("Core1 jobs: one heartbeat rule, across the millis() rollover");

	checkTrue("a fresh heartbeat is alive", escUiAlive(1000, 1000));
	checkTrue("249 ms old: alive", escUiAlive(1249, 1000));
	checkTrue("250 ms old: dead", !escUiAlive(1250, 1000));
	// Stamped 100 ms before the rollover. At 10 ms the clock has not wrapped
	// but stamp + 250 ms has; the checks at 249 ms and later are past the wrap.
	checkTrue("stamp + 250 ms wraps, clock does not: alive at 10 ms",
	          escUiAlive(0xFFFFFF9Cu + 10u, 0xFFFFFF9Cu));
	checkTrue("at 249 ms: alive", escUiAlive(0xFFFFFF9Cu + 249u, 0xFFFFFF9Cu));
	checkTrue("dead at 250 ms", !escUiAlive(0xFFFFFF9Cu + 250u, 0xFFFFFF9Cu));
	checkTrue("and dead at 400 ms", !escUiAlive(0xFFFFFF9Cu + 400u, 0xFFFFFF9Cu));
	// A stamp newer than the clock, as when a read order lets core0 stamp
	// between the two reads: alive while less than the timeout ahead, as a
	// backstop.
	checkTrue("a heartbeat 1 ms newer than the clock: alive", escUiAlive(5000, 5001));
	checkTrue("1 ms newer, across the wrap: alive", escUiAlive(0xFFFFFFFFu, 0u));
	checkTrue("249 ms newer: alive", escUiAlive(5000, 5249));
	checkTrue("250 ms newer: dead", !escUiAlive(5000, 5250));
	checkTrue("a stale one across the wrap, 300 ms: dead", !escUiAlive(200u, 0xFFFFFF9Cu));
	// An old stamp is not read as ahead of the clock: 0x90000000 ms is about
	// 28 days, and 2^32 - 251 ms reads as 251 ms ahead, past the timeout.
	checkTrue("a stamp 0x90000000 ms old: dead", !escUiAlive(0x90000000u, 0u));
	checkTrue("a stamp 2^32 - 251 ms old: dead", !escUiAlive(0xFFFFFF05u, 0u));

	uiInit();
	freshPump();
	installJobs();

	// Move the clock to 100 ms before the rollover. The advances below add up
	// to 2^32 ms, so the clock ends where it started.
	uint32_t start = millis();
	uint32_t toEdge = (0u - 100u) - start;
	fakeAdvance(toEdge);

	const PinUser users[] = {PinUser::Servo, PinUser::Kst};
	FakeJob *jobs[] = {&g_servoJob, &g_kstJob};
	const char *alive[] = {"servo job: alive 249 ms after, across the wrap",
	                       "KST job: alive 249 ms after, across the wrap"};
	const char *dead[]  = {"servo job: dead at 250 ms",
	                       "KST job: dead at 250 ms"};
	uint32_t spent = 0;
	for (int i = 0; i < 2; i++) {
		pinClaimBegin(users[i], PIN_SIG);
		pump();
		pinClaimPoll();
		core1JobPoll();
		pinClaimPoll();
		escHeartbeat();
		fakeAdvance(249);
		checkTrue(i == 0 ? "the clock wrapped" : "still past the wrap", millis() < 1000);
		core1JobPoll();
		checkTrue(alive[i], jobs[i]->uiAlive && core1UiAlive());
		fakeAdvance(1);
		core1JobPoll();
		checkTrue(dead[i], !jobs[i]->uiAlive && !core1UiAlive());
		escHeartbeat();
		core1JobPoll();
		checkTrue("and alive again with the next heartbeat", jobs[i]->uiAlive);
		toFree();
		spent += 250;
	}
	fakeAdvance(0u - toEdge - spent);
	checkInt("the clock is back where it was", millis(), start);
	escHeartbeat();
	removeJobs();
}

/** @brief settingsSave() refuses while a claim holds. */
static void testNoSaveDuringClaim() {
	section("Pin claim: no flash write while it holds");

	uiInit();
	freshPump();
	fakeFlashClear();
	settingsLoad();
	settings()->poles = 12;
	int writes = fakeFlashWrites();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("a save while it holds is refused", !settingsSave());
	checkInt("and writes nothing", fakeFlashWrites(), writes);
	checkTrue("back to free", toFree());
	checkTrue("the save goes through after it", settingsSave());
	fakeFlashClear();
	settingsLoad();
}

/** @brief The AM32 screen on the claim, through the real UI. */
static void testAm32Screen() {
	section("Pin claim: the AM32 screen");

	uiInit();
	freshPump();
	frames(2);
	int resumes = fakeResumeCount();

	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("CFG opens SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
	checkControlTable("SETTINGS");
	frames(2);
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 opens from SETTINGS", uiCurrentScreen() == UiScreenId::Am32);
	checkTrue("AM32 holds the claim", pinClaimHolder() == PinUser::Am32);
	frames(4);
	checkControlTable("AM32, the claim held");
	checkInt("the transport gets the pin the claim latched", fakeAm32Pin(),
	         escTaskDshotPin());
	checkTrue("no core1 job", core1JobRunning() == Core1Job::None);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from AM32 returns to SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("BACK gives DShot the pin, no unplug page",
	          fakePinReturned() && !pinClaimUnplugDue());
	checkTrue("the transport let go before DShot was asked back",
	          fakeAm32EndBeforeResume());
	checkInt("resumed once", fakeResumeCount(), resumes + 1);
	checkTrue("restoring until core1's next pass",
	          pinClaimState() == ClaimState::Restoring);
	frames(1);

	// Out to the tester and into AM32 again, with no claim call from the test
	// in between: only the screens' own calls see Restoring end.
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SETTINGS returns to the tester", uiCurrentScreen() == UiScreenId::Tester);
	tapId(UI_FIX(TesterCtl::Menu));
	frames(2);
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 opens a second time", uiCurrentScreen() == UiScreenId::Am32);
	frames(4);
	checkTrue("AM32 opened again takes the pin again",
	          !fakePinReturned() && escTaskSuspended());
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the second AM32 returns to SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
	frames(1);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SETTINGS returns to the tester again", uiCurrentScreen() == UiScreenId::Tester);
	frames(2);

	// Straight from a running motor: CFG disarms, and AM32 is opened before
	// 250 ms have passed since the last throttle frame. Its one change of
	// behaviour on the claim is that the handover waits for the drain.
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); frames(2);
	tapId(UI_FIX(TesterCtl::Hold));
	// The drag starts mid-track, at the track's vertical centre.
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	const int thrY = thr.y + thr.h / 2;
	fakePress(120, thrY); frames(1);
	for (int x = 120; x <= 180; x += 10) { fakeHold(x, thrY); frames(1); }
	fakeRelease(); frames(1);
	checkTrue("armed with a throttle", uiArmed() && fakeThrottle() > 0);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("CFG opens SETTINGS from a running motor", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("CFG disarms", !uiArmed() && !fakeArmed());
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 opens from SETTINGS after a running motor", uiCurrentScreen() == UiScreenId::Am32);
	uint32_t stamp = fakeLastThrottleMs();
	checkTrue("AM32 opened within 250 ms of the last throttle frame",
	          millis() - stamp <= 250);
	checkTrue("AM32 waits for the drain", pinClaimState() == ClaimState::Stopping);
	for (int i = 0; i < 20 && pinClaimState() == ClaimState::Stopping; i++) frames(1);
	uint32_t waited = millis() - stamp;
	checkTrue("and takes the pin on the first frame more than 250 ms after it",
	          pinClaimState() == ClaimState::Held && waited > 250 && waited <= 275);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the drained AM32 returns to SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
	frames(1);
	checkTrue("free", pinClaimState() == ClaimState::Free);

	// Another claim holds: not reachable through the UI, since only
	// SETTINGS opens a claiming screen. AM32 says so and asks again each frame.
	checkTrue("a SERVO claim holds",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 opens beside the other claim", uiCurrentScreen() == UiScreenId::Am32);
	checkTrue("AM32 is refused", pinClaimHolder() == PinUser::Servo);
	frames(2);
	checkControlTable("AM32, waiting for the other claim");
	checkTrue("and waits", pinClaimHolder() == PinUser::Servo);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the waiting AM32 returns to SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("BACK from AM32 leaves the other claim alone",
	          pinClaimHolder() == PinUser::Servo);
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 opens again beside the other claim", uiCurrentScreen() == UiScreenId::Am32);
	toFree();
	frames(1);
	checkTrue("and claims the frame after the other one ends",
	          pinClaimHolder() == PinUser::Am32);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the last AM32 returns to SETTINGS", uiCurrentScreen() == UiScreenId::Menu);
	frames(1);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SETTINGS returns to the tester at the end", uiCurrentScreen() == UiScreenId::Tester);
	frames(2);
}

void runClaimTests() {
	section("Core1 jobs: the table as it ships");
	checkTrue("the servo job is in the table, KST is not",
	          s_ops[(uint8_t)Core1Job::Servo] == &SERVO_JOB_OPS &&
	          s_ops[(uint8_t)Core1Job::Kst] == nullptr);
	installJobs();
	s_ops[(uint8_t)Core1Job::Kst] = &FAKE_KST_INIT;
	core1JobInit();
	checkInt("core1JobInit() calls each init it finds, once", g_inits, 1);
	shippedJobs();
	core1JobInit();
	removeJobs();

	testCheckRules();
	testBeginRefusals();
	testHandoverRules();
	testHandoverSlots();
	testDrain();
	testRestoring();
	testOrder();
	testJobGuard();
	testBackEveryState();
	testRestore();
	testHeartbeat();
	testNoSaveDuringClaim();
	testAm32Screen();

	shippedJobs();
	quiet();
}
