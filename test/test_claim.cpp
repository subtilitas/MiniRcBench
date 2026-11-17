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
#include "ui_menu.h"
#include "plat.h"

#include <hardware/gpio.h>
#include <hardware/structs/pads_bank0.h>

#include "../src/core1_job.cpp"   // NOLINT -- see the note above

#include <dirent.h>
#include <string.h>
#include <string>
#include <initializer_list>

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

/** @brief No job in the table. */
static void removeJobs() {
	s_ops[(uint8_t)Core1Job::Servo] = nullptr;
	s_ops[(uint8_t)Core1Job::Kst]   = nullptr;
	s_ops[(uint8_t)Core1Job::Hitec] = nullptr;
	s_ops[(uint8_t)Core1Job::Sbus]  = nullptr;
	s_ops[(uint8_t)Core1Job::EscWire] = nullptr;
}

/** @brief The table as it ships: the servo, KST, Hitec, S.BUS and ESC settings jobs. */
static void shippedJobs() {
	s_ops[(uint8_t)Core1Job::Servo] = &SERVO_JOB_OPS;
	s_ops[(uint8_t)Core1Job::Kst]   = &KST_JOB_OPS;
	s_ops[(uint8_t)Core1Job::Hitec] = &HITEC_JOB_OPS;
	s_ops[(uint8_t)Core1Job::Sbus]  = &SBUS_JOB_OPS;
	s_ops[(uint8_t)Core1Job::EscWire] = &ESCWIRE_JOB_OPS;
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

#define SIG_GPIO 0
#define AUX_GPIO 1

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
	// A hub's frame: Run not wanted, so a claim is not Busy
	// (docs/design/dshot-lifetime.md).
	pinClaimDshot(false);
	escTaskConfigure(SIG_GPIO, 600, true, AUX_GPIO);
	quiet();
	pump();
	fakeAdvance(1000);
	pump();
}

static bool padLow(unsigned gpio);
static bool padOff(unsigned gpio);

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
		pinClaimRelease(who);
	}
	return pinClaimState() == ClaimState::Free;
}

/** @brief One UI frame of the real UI, with core1's pump pass before it. */
static void frames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); pump(); uiTick(); }
}

#define UI_IDS_FRAMES(n) frames(n)
#include "ui_ids.h"

/**
 * @brief Whether MENU row @p id is drawn refused: an outline with the
 *        background inside, where a live row has the panel fill, and the
 *        strip reads the busy caption in amber.
 */
static bool busyShown(UiControlId id) {
	UiRect r = ctlRect(id);
	char want[40];
	uiBusyCaption(want, sizeof(want));
	// On the first line of two while MENU's AM32 run cue stands under it.
	// A hub draws it as the strip's second line.
	return gfxBuffer()[(r.y + 8) * GFX_W + r.x + 5] == C_BG &&
	       (fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_AMBER, C_BG, 1) ||
	        fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, want, C_AMBER, C_BG, 1) ||
	        fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));
}

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

	freshPump();
	uiInitTester();
	removeJobs();
	checkControlTable("tester, disarmed");

	// Armed through the UI, which is the arm state the claim reads.
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); frames(2);
	checkTrue("armed through the UI", uiArmed());
	checkControlTable("tester, armed");
	int parks = fakeParkCount();
	checkTrue("armed: refused", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Armed);
	checkTrue("the claim stays free", pinClaimState() == ClaimState::Free);
	checkTrue("DShot keeps the pins", fakePinReturned() && escTaskRunning());
	checkTrue("and the claim does not disarm", fakeArmed() && uiArmed());
	pressId(UI_FIX(TesterCtl::Arm));
	fakeRelease(); frames(2);
	checkTrue("disarmed again", !uiArmed());
	// Run is wanted while the tester is open: Busy, a backstop, since no
	// route opens a claim from ESC TESTER (docs/design/dshot-lifetime.md).
	checkTrue("disarmed on the tester, Run wanted: busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("DShot runs on", fakePinReturned() && escTaskRunning() && fakeParkCount() == parks);

	// MENU: Run is no longer wanted, and the pump parks.
	tapId(UI_FIX(TesterCtl::Menu));
	frames(2);
	checkTrue("MENU: parked", uiCurrentScreen() == UiScreenId::Menu && escTaskParked() &&
	          fakeParkCount() == parks + 1);
	fakeAdvance(1000);
	parks = fakeParkCount();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("a second AM32 claim: busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("a SERVO claim beside it: busy",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("the holder is AM32", pinClaimHolder() == PinUser::Am32);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK from a screen that does not hold it does nothing",
	          pinClaimHolder() == PinUser::Am32 && fakeParkCount() == parks);
	checkTrue("back to free", toFree());

	// The ESC check: with DShot parked nothing answers, so it refuses only
	// within 500 ms of the last answer (decision 7).
	escAnswered(499);
	checkTrue("ESC answering: SERVO waits",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::EscAnswering);
	checkTrue("DShot stays parked", escTaskParked() && !fakePinReturned());
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

	const EscPinMode RUN = EscPinMode::Run, PARK = EscPinMode::Park,
	                 REL = EscPinMode::Release;
	checkTrue("no request: run, even right after a throttle frame",
	          escHandoverStep(RUN, RUN, false, true, 5000, 5001) == EscHandover::Run);
	checkTrue("a suspend 250 ms after a throttle frame: drain",
	          escHandoverStep(REL, RUN, false, true, 5000, 5250) == EscHandover::Drain);
	checkTrue("251 ms after: teardown",
	          escHandoverStep(REL, RUN, false, true, 5000, 5251) == EscHandover::Teardown);
	checkTrue("a rebuild drains too",
	          escHandoverStep(RUN, RUN, true, true, 5000, 5100) == EscHandover::Drain);
	checkTrue("nothing bound: released at once",
	          escHandoverStep(REL, RUN, false, false, 5000, 5001) == EscHandover::Release);
	checkTrue("no throttle frame since the bind: teardown at once",
	          escHandoverStep(REL, RUN, false, true, 0, 5001) == EscHandover::Teardown);
	checkTrue("a park drains as a suspend does",
	          escHandoverStep(PARK, RUN, false, true, 5000, 5250) == EscHandover::Drain);
	checkTrue("and tears down after it",
	          escHandoverStep(PARK, RUN, false, true, 5000, 5251) == EscHandover::Teardown);
	checkTrue("down after the teardown, Park wanted: park",
	          escHandoverStep(PARK, RUN, false, false, 0, 5251) == EscHandover::Park);
	checkTrue("parked: hold", escHandoverStep(PARK, PARK, false, false, 0, 6000) == EscHandover::Hold);
	checkTrue("parked, the wiring changed: park again",
	          escHandoverStep(PARK, PARK, true, false, 0, 6000) == EscHandover::Park);
	checkTrue("parked, Release wanted: release with no drain",
	          escHandoverStep(REL, PARK, false, false, 5000, 5001) == EscHandover::Release);
	checkTrue("released: hold", escHandoverStep(REL, REL, false, false, 0, 6000) == EscHandover::Hold);
	checkTrue("released, the wiring changed: hold, the pins are the claim's",
	          escHandoverStep(REL, REL, true, false, 0, 6000) == EscHandover::Hold);
	checkTrue("parked, Run wanted: bind",
	          escHandoverStep(RUN, PARK, false, false, 0, 6000) == EscHandover::Bind);
	checkTrue("released, Run wanted: bind",
	          escHandoverStep(RUN, REL, false, false, 0, 6000) == EscHandover::Bind);
	checkTrue("a start during the drain: run on",
	          escHandoverStep(RUN, RUN, false, true, 5000, 5100) == EscHandover::Run);

	EscSlot d = escDrainSlot();
	checkTrue("a drain slot is throttle zero, no command, no KISS request",
	          !d.frame.sendCommand && d.frame.throttle == 0 && !d.frame.requestKiss);
	checkTrue("and takes no command request and forgets no EDT",
	          !d.cmdTaken && !d.forgetEdt);

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
	EscCmdRequest req;            /**< core0's s_cmdReq. */
	bool     suspendReq;          /**< core0 asked for Release, else Run. */
	bool     rebuildReq;          /**< core0's s_rebuildReq. */
	bool     pumpUp;              /**< s_pumpUp. */
	/** escTaskSuspended(): Release reached, the pump down. */
	bool     suspended;
	EscPinMode reached;           /**< s_reached. */
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
	EscPinMode want = h->suspendReq ? EscPinMode::Release : EscPinMode::Run;
	EscHandover act = escHandoverStep(want, h->reached, h->rebuildReq, h->pumpUp,
	                                  h->pump.lastThrottleMs, h->ms);
	if (act == EscHandover::Teardown) {
		h->pumpUp = false;
		h->teardowns++;
		h->teardownMs = h->ms;
		act = escHandoverStep(want, h->reached, false, false, 0, 0);
	}
	if (act == EscHandover::Release) {
		h->reached = EscPinMode::Release;
		h->suspended = true;
		h->ms++;
		return;
	}
	if (act == EscHandover::Bind) {
		escPumpBind(&h->pump, &h->armed, &h->throttle);
		h->pumpUp = true;
		h->rebuildReq = false;
		h->reached = EscPinMode::Run;
		h->suspended = false;
		h->binds++;
	}
	EscSlot s = act == EscHandover::Drain
	          ? escDrainSlot()
	          : escPumpSlot(&h->pump, &h->tel, h->armed, true, h->throttle,
	                        &h->req, DSHOT_REPLY_NONE, false, h->ms);
	if (s.cmdTaken) h->req.cmd = 0;
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
	h->req = {1, false, h->ms};
	h->suspendReq = true;
	uint32_t last = h->lastThrottleFrameMs;
	for (int i = 0; i < 1000 && !h->suspended; i++) hsSlot(h);
	checkTrue("suspended", h->suspended);
	checkInt("the teardown is 251 ms after the last throttle frame",
	         (long)(h->teardownMs - last), 251);
	checkInt("every slot between is a drain frame", h->drainFrames, 250);
	checkInt("each throttle zero, no command, no KISS request", h->badDrainFrames, 0);
	checkInt("the drain takes no beacon", h->req.cmd, 1);

	h->req.cmd = 0;
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

/**
 * @brief The drain after a running tester, through the claim and the fake
 *        pump: Busy while Run is wanted; after pinClaimDshot(false) a claim
 *        waits in Stopping for the rest of the drain, then Held. With no
 *        drain owed a claim is Held on the first pass.
 */
static void testDrain() {
	section("Pin claim: the drain after a running tester");

	uiInitMenu();
	freshPump();
	removeJobs();
	pinClaimDshot(true);
	pump();
	checkTrue("Run: the pump binds", escTaskRunning() && pinClaimDshotRunning());
	checkTrue("Run wanted: a claim is busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Busy);
	escSetArmed(true);
	escSetThrottle(300);
	pump();
	uint32_t stamp = fakeLastThrottleMs();
	checkInt("one throttle frame out", stamp, millis());
	int parks = fakeParkCount();
	fakeAdvance(100);
	pinClaimDshot(false);
	checkTrue("Run no longer wanted: Park asked, the tester disarmed",
	          fakeParkCount() == parks + 1 && !fakeArmed() && fakeThrottle() == 0);
	checkTrue("not running", !pinClaimDshotRunning());
	checkTrue("100 ms after the throttle frame: the claim is not busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("stopping", pinClaimPoll() == ClaimState::Stopping);
	fakeAdvance(150);
	pump();
	checkTrue("still stopping 250 ms after the throttle frame",
	          pinClaimPoll() == ClaimState::Stopping && !escTaskSuspended());
	checkTrue("zero frames meanwhile", fakeLastFrame().throttle == 0 && !fakeLastFrame().sendCommand);
	checkTrue("no GPIO latched yet", pinClaimGpio(PIN_SIG) == PIN_CLAIM_NO_GPIO);
	fakeAdvance(1);
	pump();
	checkTrue("held at 251 ms", pinClaimPoll() == ClaimState::Held);
	checkTrue("back to free", toFree());
	checkTrue("parked", escTaskParked());

	fakeAdvance(5000);
	checkTrue("no drain owed: claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held after one pump pass", pinClaimPoll() == ClaimState::Held);
	checkTrue("back to free", toFree());
}

/** @brief Parking lasts until core1 has parked the pins; no bind. */
static void testRestoring() {
	section("Pin claim: parking until the pins are parked");

	uiInitMenu();
	freshPump();
	removeJobs();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	int parks = fakeParkCount();
	int resumes = fakeResumeCount();
	int rebuilds = fakeRebuildCount();
	pinClaimEnd(PinUser::Am32);
	checkInt("BACK asks for Park", fakeParkCount(), parks + 1);
	checkInt("and not for Run", fakeResumeCount(), resumes);
	checkTrue("parking until core1 parks the pins",
	          pinClaimState() == ClaimState::Parking);
	checkTrue("still AM32's", pinClaimHolder() == PinUser::Am32);
	checkTrue("a SERVO claim while parking: busy",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::Busy);
	checkTrue("a new AM32 claim while parking: busy",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Busy);
	pinClaimDshot(true);
	checkTrue("Run while parking does nothing", fakeResumeCount() == resumes &&
	          pinClaimState() == ClaimState::Parking);
	pinClaimEnd(PinUser::Am32);
	checkTrue("BACK while parking does nothing",
	          pinClaimState() == ClaimState::Parking && fakeParkCount() == parks + 1);
	checkTrue("nor does a poll", pinClaimPoll() == ClaimState::Parking);
	pump();
	checkInt("no GPIO once parked, asked before anything else",
	         pinClaimGpio(PIN_SIG), PIN_CLAIM_NO_GPIO);
	checkInt("core1 binds nothing", fakeRebuildCount(), rebuilds);
	checkTrue("SIG driven low, AUX on its pull-down", escTaskParked() &&
	          padLow(SIG_GPIO) && padOff(AUX_GPIO));
	checkTrue("free once parked", pinClaimState() == ClaimState::Free &&
	                              pinClaimHolder() == PinUser::None);
}

/** @brief The order of the two cores' steps through a SERVO claim. */
static void testOrder() {
	section("Pin claim: order of the handover");

	uiInitMenu();
	freshPump();
	installJobs();
	int resumes = fakeParkCount();

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
	pinClaimRelease(PinUser::Servo);
	checkInt("the last button before the job is idle does nothing",
	         fakeParkCount(), resumes);

	core1JobPoll();
	checkInt("core1 releases once", g_servoJob.releases, 1);
	checkTrue("the job is idle", core1JobRunning() == Core1Job::None);
	checkTrue("still leaving", pinClaimPoll() == ClaimState::Leaving);
	checkTrue("the unplug page is due", pinClaimUnplugDue());
	pinClaimPoll();
	pinClaimPoll();
	checkInt("no park until the last button", fakeParkCount(), resumes);
	pinClaimRelease(PinUser::Am32);
	checkInt("the last button from a screen that does not hold it does nothing",
	         fakeParkCount(), resumes);

	int binds = fakeRebuildCount();
	pinClaimRelease(PinUser::Servo);
	checkInt("the last button parks the pins", fakeParkCount(), resumes + 1);
	checkTrue("with the job idle", fakeParkJobIdle());
	checkTrue("parking", pinClaimPoll() == ClaimState::Parking);
	pump();
	checkTrue("free once parked, no bind", pinClaimPoll() == ClaimState::Free &&
	          escTaskParked() && fakeRebuildCount() == binds);
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

	// AM32: no job, no unplug page, Park asked for within the BACK call.
	resumes = fakeParkCount();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	checkTrue("held with no job", core1JobRunning() == Core1Job::None);
	pinClaimPoll();
	checkTrue("AM32 stays held", pinClaimState() == ClaimState::Held);
	pinClaimEnd(PinUser::Am32);
	checkTrue("AM32 gets no unplug page", !pinClaimUnplugDue());
	checkInt("parks within BACK", fakeParkCount(), resumes + 1);
	pump();
	checkTrue("free", pinClaimState() == ClaimState::Free);
	removeJobs();
}

/** @brief core1JobStart() and its guard, the dispatch's own rules. */
static void testJobGuard() {
	section("Core1 jobs: start is refused unless DShot is down");

	uiInitMenu();
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
	escTaskStart();
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
	checkTrue("and so does the KST job", core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	core1JobStop();
	core1JobPoll();
	checkTrue("stopped before its bind: idle", core1JobPhase() == Core1JobPhase::Idle);
	escTaskStart();
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

	uiInitMenu();
	freshPump();
	installJobs();
	int resumes;

	// Stopping: the tester ran and sent a throttle frame on the last pump
	// pass, and was left, so the drain holds the claim.
	pinClaimDshot(true);
	pump();
	escSetArmed(true);
	escSetThrottle(200);
	pump();
	pinClaimDshot(false);
	resumes = fakeParkCount();
	checkTrue("SERVO claims during a drain",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("stopping", pinClaimPoll() == ClaimState::Stopping);
	pinClaimEnd(PinUser::Servo);
	checkInt("BACK while stopping: Park asked at once", fakeParkCount(), resumes + 1);
	checkTrue("parking while the drain runs on", pinClaimState() == ClaimState::Parking &&
	          !escTaskParked());
	fakeAdvance(251);
	pump();
	checkTrue("free once the drain is out and the pins parked",
	          pinClaimState() == ClaimState::Free && escTaskParked());
	checkInt("and no job was ever bound", g_servoJob.binds, 0);
	fakeAdvance(1000);

	// Stopping, with the pump torn down before any poll has seen it.
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pump();
	resumes = fakeParkCount();
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK while stopping after the release: parking",
	          fakeParkCount() == resumes + 1 &&
	          pinClaimState() == ClaimState::Parking);
	pump();
	checkTrue("free once parked", pinClaimState() == ClaimState::Free);
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
	pinClaimRelease(PinUser::Servo);
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
	resumes = fakeParkCount();
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK after a failed bind: the unplug page", pinClaimUnplugDue());
	pinClaimRetry(PinUser::Servo);
	checkTrue("RETRY while leaving does nothing",
	          core1JobPhase() == Core1JobPhase::Idle && pinClaimUnplugDue());
	pinClaimRelease(PinUser::Servo);
	checkInt("then the park", fakeParkCount(), resumes + 1);
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
	resumes = fakeParkCount();
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
	checkInt("no park meanwhile", fakeParkCount(), resumes);
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
	pinClaimRelease(PinUser::Servo);
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
	resumes = fakeParkCount();
	pinClaimKeep(PinUser::Am32);
	checkTrue("KEEP from a screen that does not hold it does nothing", pinClaimUnplugDue());
	pinClaimKeep(PinUser::Servo);
	checkTrue("BACK on the unplug page: held again", pinClaimState() == ClaimState::Held);
	checkInt("no park", fakeParkCount(), resumes);
	pinClaimRelease(PinUser::Servo);
	checkTrue("the last button is not due while held", pinClaimState() == ClaimState::Held);
	core1JobPoll();
	checkTrue("the job binds again", g_servoJob.binds == 2 &&
	                                 pinClaimPoll() == ClaimState::Running);
	pinClaimKeep(PinUser::Servo);
	checkTrue("KEEP while running does nothing", pinClaimState() == ClaimState::Running);
	checkTrue("back to free", toFree());
	checkInt("every bind released", g_servoJob.releases, 2);
	removeJobs();
}

static FakeJob g_hitecJob;
static uint8_t hitecBind(uint8_t sig, uint8_t aux) { return bindInto(&g_hitecJob, sig, aux); }
static void hitecStep(uint32_t, bool alive) { g_hitecJob.steps++; g_hitecJob.uiAlive = alive; }
static bool hitecRelease(uint32_t) { return releaseFrom(&g_hitecJob); }
static const Core1JobOps FAKE_HITEC = { hitecBind, hitecStep, hitecRelease, nullptr };

/**
 * @brief HITEC defers its job: the claim holds SIG with no job until the
 *        family tap, and BACK before it passes the unplug page.
 */
static void testDeferredJob() {
	section("Pin claim: HITEC holds SIG before its job");

	uiInitMenu();
	freshPump();
	installJobs();
	memset(&g_hitecJob, 0, sizeof(g_hitecJob));
	s_ops[(uint8_t)Core1Job::Hitec] = &FAKE_HITEC;
	checkTrue("HITEC and ESC SETTINGS defer their jobs", pinUserInfo(PinUser::Hitec).deferJob &&
	          !pinUserInfo(PinUser::Servo).deferJob && !pinUserInfo(PinUser::Kst).deferJob &&
	          pinUserInfo(PinUser::Am32).deferJob);
	int resumes = fakeParkCount();
	checkTrue("HITEC claims", pinClaimBegin(PinUser::Hitec, PIN_SIG) == ClaimRefusal::None);
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	core1JobPoll();
	pinClaimPoll();
	checkTrue("no job is asked for at Held", core1JobPhase() == Core1JobPhase::Idle &&
	          g_hitecJob.binds == 0 && pinClaimState() == ClaimState::Held &&
	          pinClaimJobError() == CORE1_JOB_OK);
	checkInt("the pin is latched", pinClaimGpio(PIN_SIG), SIG_GPIO);
	pinClaimStartJob(PinUser::Servo);
	core1JobPoll();
	checkTrue("a start from another screen does nothing", core1JobPhase() == Core1JobPhase::Idle &&
	          g_hitecJob.binds == 0 && pinClaimPoll() == ClaimState::Held);

	// BACK before the tap: the unplug page, as after a session.
	pinClaimEnd(PinUser::Hitec);
	checkTrue("BACK: leaving, the unplug page at once", pinClaimState() == ClaimState::Leaving &&
	          pinClaimUnplugDue());
	pinClaimStartJob(PinUser::Hitec);
	checkTrue("no start while leaving", core1JobPhase() == Core1JobPhase::Idle);
	pinClaimKeep(PinUser::Hitec);
	core1JobPoll();
	checkTrue("BACK on the unplug page: held, still no job",
	          pinClaimPoll() == ClaimState::Held && g_hitecJob.binds == 0 &&
	          core1JobPhase() == Core1JobPhase::Idle);
	checkInt("DShot stayed down", fakeParkCount(), resumes);

	// The tap: the job starts and binds.
	pinClaimStartJob(PinUser::Hitec);
	checkTrue("the tap asks for the job", core1JobPhase() == Core1JobPhase::Starting);
	pinClaimStartJob(PinUser::Hitec);
	core1JobPoll();
	checkTrue("it binds once", g_hitecJob.binds == 1 && pinClaimPoll() == ClaimState::Running);
	pinClaimEnd(PinUser::Hitec);
	core1JobPoll();
	pinClaimPoll();
	pinClaimKeep(PinUser::Hitec);
	core1JobPoll();
	checkTrue("after the tap, NEXT SERVO on the leave page returns to Held with no job",
	          g_hitecJob.binds == 1 && pinClaimPoll() == ClaimState::Held &&
	          core1JobPhase() == Core1JobPhase::Idle);
	pinClaimStartJob(PinUser::Hitec);
	core1JobPoll();
	checkTrue("and the next tap starts it again",
	          g_hitecJob.binds == 2 && pinClaimPoll() == ClaimState::Running);
	checkTrue("back to free", toFree());

	// The tap while the pump still drains: the job follows at Held.
	escSetArmed(true);
	escSetThrottle(200);
	pump();
	checkTrue("claims during a drain", pinClaimBegin(PinUser::Hitec, PIN_SIG) == ClaimRefusal::None);
	pinClaimStartJob(PinUser::Hitec);
	checkTrue("stopping, nothing started", pinClaimPoll() == ClaimState::Stopping &&
	          core1JobPhase() == Core1JobPhase::Idle);
	for (int i = 0; i < 4 && pinClaimState() != ClaimState::Held; i++) {
		fakeAdvance(100);
		pump();
		pinClaimPoll();
	}
	checkTrue("held: the job starts", pinClaimState() == ClaimState::Held &&
	          core1JobPhase() == Core1JobPhase::Starting);
	core1JobPoll();
	checkTrue("and binds", pinClaimPoll() == ClaimState::Running && g_hitecJob.binds == 3);
	checkTrue("back to free", toFree());

	// The next claim defers again.
	checkTrue("claims", pinClaimBegin(PinUser::Hitec, PIN_SIG) == ClaimRefusal::None);
	pump();
	pinClaimPoll();
	core1JobPoll();
	checkTrue("a new claim waits for its tap again", pinClaimPoll() == ClaimState::Held &&
	          g_hitecJob.binds == 3);
	checkTrue("back to free", toFree());
	removeJobs();
}

/** @brief The rebuild after a claim uses the wiring from before it. */
static void testRestore() {
	section("Pin claim: the pins park as they were, and DShot binds them so");

	uiInitMenu();
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
	checkTrue("parked on the same SIG", escTaskParked() && padLow(before.dshotPin));

	// The next ESC TESTER or ESC COMMANDS binds the wiring as it was.
	pinClaimDshot(true);
	pump();
	FakeWiring at = fakeResumeWiring();
	pinClaimDshot(false);
	pump();
	checkInt("the next bind takes the same ESC pin", at.dshotPin, before.dshotPin);
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

	uiInitMenu();
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

/** @brief settingsSave() and settingsSaveAngle() refuse while a claim holds. */
static void testNoSaveDuringClaim() {
	section("Pin claim: no flash write while it holds");

	uiInitMenu();
	freshPump();
	fakeFlashClear();
	settingsLoad();
	settings()->poles = 12;
	int writes = fakeFlashWrites();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("a save while it holds is refused", !settingsSave());
	checkInt("and writes nothing", fakeFlashWrites(), writes);
	settings()->angleLengthMm = 40;
	checkTrue("nor does the angle meter's save", !settingsSaveAngle());
	checkInt("which writes nothing either", fakeFlashWrites(), writes);
	checkTrue("back to free", toFree());
	checkTrue("the angle meter's save goes through after it",
	          settingsSaveAngle() && fakeFlashWrites() == writes + 1);
	checkTrue("the save goes through after it", settingsSave());
	fakeFlashClear();
	settingsLoad();
}

/**
 * @brief The leave page belongs to the claim (docs/design/bluejay-blheli-s.md,
 *        slice 2): an AM32 claim resumes DShot within pinClaimEnd() as
 *        before, and one given the leave page by pinClaimWantLeavePage()
 *        waits for RESTART DSHOT; the page lasts until Free and goes with
 *        the claim; only the holder sets it, and only before it leaves.
 */
static void testLeavePagePerClaim() {
	section("Pin claim: the leave page per claim");

	uiInitMenu();
	freshPump();
	installJobs();

	checkTrue("Free: no leave page", !pinClaimLeavePage());
	pinClaimWantLeavePage(PinUser::Am32);
	checkTrue("asked for while Free: nothing", !pinClaimLeavePage());

	// An AM32 claim as before: DShot within BACK.
	int resumes = fakeParkCount();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("its user's row: no leave page", !pinClaimLeavePage());
	pump();
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	pinClaimWantLeavePage(PinUser::Servo);
	checkTrue("asked for by a screen that does not hold it: nothing", !pinClaimLeavePage());
	pinClaimEnd(PinUser::Am32);
	checkInt("no EFM8 answer: DShot resumes within pinClaimEnd()", fakeParkCount(), resumes + 1);
	checkTrue("restoring, no page due", pinClaimState() == ClaimState::Parking && !pinClaimUnplugDue());
	pinClaimWantLeavePage(PinUser::Am32);
	checkTrue("asked for after the end: nothing", !pinClaimLeavePage());
	pump();
	checkTrue("free", pinClaimState() == ClaimState::Free);

	// The same user, given the page while Held: DShot waits.
	resumes = fakeParkCount();
	checkTrue("AM32 claims again", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pinClaimWantLeavePage(PinUser::Am32);
	checkTrue("given while Stopping: the page", pinClaimLeavePage());
	pump();
	checkTrue("held, the page kept", pinClaimPoll() == ClaimState::Held && pinClaimLeavePage());
	checkTrue("no job", core1JobRunning() == Core1Job::None);
	pinClaimEnd(PinUser::Am32);
	checkTrue("BACK: leaving, DShot not asked", pinClaimState() == ClaimState::Leaving &&
	          fakeParkCount() == resumes);
	checkTrue("the page is due at once: no job to wait for", pinClaimUnplugDue());
	for (int i = 0; i < 3; i++) { pump(); pinClaimPoll(); }
	checkTrue("and stays due, DShot down", pinClaimState() == ClaimState::Leaving &&
	          fakeParkCount() == resumes && escTaskSuspended());
	pinClaimKeep(PinUser::Servo);
	pinClaimRelease(PinUser::Servo);
	checkTrue("RESTART from another screen: nothing", pinClaimState() == ClaimState::Leaving &&
	          fakeParkCount() == resumes);
	pinClaimRelease(PinUser::Am32);
	checkInt("RESTART DSHOT resumes the pump", fakeParkCount(), resumes + 1);
	checkTrue("restoring", pinClaimState() == ClaimState::Parking);
	pump();
	checkTrue("free, the page gone with the claim", pinClaimState() == ClaimState::Free &&
	          !pinClaimLeavePage());

	// The next AM32 claim starts without it.
	resumes = fakeParkCount();
	checkTrue("AM32 claims a third time", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	pump();
	pinClaimPoll();
	checkTrue("no page from the claim before", !pinClaimLeavePage());
	pinClaimEnd(PinUser::Am32);
	checkInt("DShot within pinClaimEnd() again", fakeParkCount(), resumes + 1);
	pump();
	checkTrue("free", pinClaimState() == ClaimState::Free);

	// A user with the page from its row reports it.
	checkTrue("KST claims", pinClaimBegin(PinUser::Kst, PIN_SIG) == ClaimRefusal::None);
	checkTrue("KST's row: the page", pinClaimLeavePage());
	checkTrue("back to free", toFree());
	removeJobs();
}

/** @brief The AM32 screen on the claim, through the real UI. */
static void testAm32Screen() {
	section("Pin claim: the AM32 screen");

	uiInitTester();
	freshPump();
	frames(2);
	int resumes = fakeParkCount();

	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	frames(1);  // MENU's first paint: the table check compares BACK's look.
	checkControlTable("MENU");
	frames(2);
	openEscSettings();
	checkTrue("AM32 opens from MENU", uiCurrentScreen() == UiScreenId::Am32);
	checkTrue("AM32 holds the claim", pinClaimHolder() == PinUser::Am32);
	frames(4);
	checkControlTable("AM32, the claim held");
	checkInt("the transport gets the pin the claim latched", fakeAm32Pin(),
	         escTaskDshotPin());
	checkTrue("no core1 job", core1JobRunning() == Core1Job::None);
	resumes = fakeParkCount();
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from AM32 returns to ESC PROGRAM, its opener",
	          uiCurrentScreen() == UiScreenId::EscConfig);
	checkTrue("BACK parks the pins, no unplug page, no DShot",
	          !fakePinReturned() && !pinClaimUnplugDue());
	checkTrue("the transport let go before the park was asked",
	          fakeAm32EndBeforeResume());
	checkInt("parked once", fakeParkCount(), resumes + 1);
	checkTrue("parking until core1's next pass",
	          pinClaimState() == ClaimState::Parking);
	frames(1);

	// Out to the tester and into AM32 again, with no claim call from the test
	// in between: only the screens' own calls see Parking end.
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from ESC PROGRAM returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	openTester();
	checkTrue("MENU's row ESC TESTER returns to the tester", uiCurrentScreen() == UiScreenId::Tester);
	tapId(UI_FIX(TesterCtl::Menu));
	frames(2);
	openEscSettings();
	checkTrue("AM32 opens a second time", uiCurrentScreen() == UiScreenId::Am32);
	frames(4);
	checkTrue("AM32 opened again takes the pin again",
	          !fakePinReturned() && escTaskSuspended());
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the second AM32 returns to ESC PROGRAM",
	          uiCurrentScreen() == UiScreenId::EscConfig);
	frames(1);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	openTester();
	checkTrue("MENU's row ESC TESTER returns to the tester again", uiCurrentScreen() == UiScreenId::Tester);
	frames(2);

	// Straight from a running motor: MENU disarms, and AM32 is opened before
	// 250 ms have passed since the last throttle frame. Its one change of
	// behaviour on the claim is that the handover waits for the drain.
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); frames(2);
	tapId(UI_FIX(TesterCtl::Latch));
	// The drag starts mid-track, at the track's vertical centre.
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	const int thrY = thr.y + thr.h / 2;
	fakePress(120, thrY); frames(1);
	for (int x = 120; x <= 180; x += 10) { fakeHold(x, thrY); frames(1); }
	fakeRelease(); frames(1);
	checkTrue("armed with a throttle", uiArmed() && fakeThrottle() > 0);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("MENU opens from a running motor", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("MENU disarms", !uiArmed() && !fakeArmed());
	openEscSettings();
	checkTrue("AM32 opens from MENU after a running motor", uiCurrentScreen() == UiScreenId::Am32);
	uint32_t stamp = fakeLastThrottleMs();
	checkTrue("AM32 opened within 250 ms of the last throttle frame",
	          millis() - stamp <= 250);
	checkTrue("AM32 waits for the drain", pinClaimState() == ClaimState::Stopping);
	for (int i = 0; i < 20 && pinClaimState() == ClaimState::Stopping; i++) frames(1);
	uint32_t waited = millis() - stamp;
	checkTrue("and takes the pin on the first frame more than 250 ms after it",
	          pinClaimState() == ClaimState::Held && waited > 250 && waited <= 275);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the drained AM32 returns to ESC PROGRAM",
	          uiCurrentScreen() == UiScreenId::EscConfig);
	frames(1);
	checkTrue("free", pinClaimState() == ClaimState::Free);

	// Another claim holds. ESC PROGRAM refuses AM32 while the claim is not
	// Free: the entry is drawn disabled, does not act, and a tap opens
	// nothing. Once the claim is Free, the entry opens AM32, which claims at
	// once.
	checkTrue("a SERVO claim holds",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	frames(1);
	checkControlTable("ESC PROGRAM, the pins busy");
	const UiControlId entry = UI_FIX(EscCfgCtl::Settings);
	checkTrue("ESC PROGRAM: ESC SETTINGS drawn disabled, the strip's busy caption", busyShown(entry));
	checkTrue("and its entry does not act", !ctlEnabled(entry));
	tapId(entry, IdExpect::Disabled);
	frames(1);
	checkTrue("a tap on it opens nothing", uiCurrentScreen() == UiScreenId::EscConfig);
	checkTrue("and leaves the other claim alone", pinClaimHolder() == PinUser::Servo);
	toFree();
	frames(1);
	checkTrue("the other claim gone: the entry acts again", ctlEnabled(entry) && !busyShown(entry));
	tapId(entry);
	checkTrue("AM32 opens, and claims on the tap",
	          uiCurrentScreen() == UiScreenId::Am32 && pinClaimHolder() == PinUser::Am32);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from the last AM32 returns to ESC PROGRAM", uiCurrentScreen() == UiScreenId::EscConfig);
	frames(1);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	tapId(UI_FIX(UI_BACK));
	openTester();
	checkTrue("MENU's row ESC TESTER returns to the tester at the end", uiCurrentScreen() == UiScreenId::Tester);
	frames(2);
}

// ---------------------------------------------------------------------------
// The pump's three modes (docs/design/dshot-lifetime.md, slice D1)
// ---------------------------------------------------------------------------

#define NEW_GPIO 2

/** @brief The pad's drive field, GPIO_DRIVE_STRENGTH_*. */
static unsigned padDrive(unsigned gpio) {
	return (fakePad(gpio) & PADS_BANK0_GPIO0_DRIVE_BITS) >> PADS_BANK0_GPIO0_DRIVE_LSB;
}

/** @brief SIG while parked: an SIO output driving low at 2 mA. */
static bool padLow(unsigned gpio) {
	FakePin p = fakePin(gpio);
	return p.function == GPIO_FUNC_SIO && p.output && !p.high &&
	       padDrive(gpio) == GPIO_DRIVE_STRENGTH_2MA;
}

/** @brief The reset state: an input, the pull-down on, the buffer off, 4 mA. */
static bool padOff(unsigned gpio) {
	FakePin p = fakePin(gpio);
	return !p.output && p.pull == FakePull::Down && p.inputOff &&
	       padDrive(gpio) == GPIO_DRIVE_STRENGTH_4MA;
}

/** @brief An input with the pull-up on, the buffer on, 4 mA. */
static bool padPullUp(unsigned gpio) {
	FakePin p = fakePin(gpio);
	return !p.output && p.pull == FakePull::Up && !p.inputOff &&
	       padDrive(gpio) == GPIO_DRIVE_STRENGTH_4MA;
}

/**
 * @brief Park from a running pump, past any drain, and report whether it
 *        parked.
 */
static bool toParked() {
	escTaskPark();
	for (int i = 0; i < 400 && !escTaskParked(); i++) { fakeAdvance(1); pump(); }
	return escTaskParked();
}

/** @brief Back to Run on the board's default wiring. */
static void backToRun() {
	escTaskStart();
	freshPump();
}

/** @brief What the GPIO hook saw while the pump wrote the pads. */
struct PadWatch {
	int writes;       /**< GPIO writes. */
	int e9;           /**< A written pad, pull-down on, buffer on. */
	int parkedNotLow; /**< escTaskParked() true with SIG not driven low. */
	int released;     /**< escTaskSuspended() true while the pump wrote a pad. */
	int sigHigh;      /**< SIG an output with the value 1. */
};
static PadWatch g_watch;

/** @brief The GPIO hook: checks each pad write of the pump. */
static void watchPads(uint32_t gpio) {
	g_watch.writes++;
	FakePin p = fakePin(gpio);
	if (p.pull == FakePull::Down && !p.inputOff) g_watch.e9++;
	if (escTaskParked() && !padLow(escTaskDshotPin())) g_watch.parkedNotLow++;
	if (escTaskSuspended()) g_watch.released++;
	FakePin sig = fakePin(escTaskDshotPin());
	if (sig.function == GPIO_FUNC_SIO && sig.output && sig.high) g_watch.sigHigh++;
}

/** @brief One pump pass with the pad writes watched. */
static void watchedPump() {
	fakeSetGpioHook(watchPads);
	pump();
	fakeSetGpioHook(nullptr);
}

/** @brief Check what watchedPump() saw since @ref g_watch was cleared. */
static void checkWatch(const char *what) {
	char line[96];
	snprintf(line, sizeof(line), "%s: the pull-down never on with the buffer on", what);
	checkInt(line, g_watch.e9, 0);
	snprintf(line, sizeof(line), "%s: never parked with SIG not low", what);
	checkInt(line, g_watch.parkedNotLow, 0);
	snprintf(line, sizeof(line), "%s: never released while the pads change", what);
	checkInt(line, g_watch.released, 0);
	snprintf(line, sizeof(line), "%s: SIG never an output at 1", what);
	checkInt(line, g_watch.sigHigh, 0);
}

/** @brief SIG's latch at 1 and its direction in, as a job could leave them. */
static void sigLatchHigh() {
	gpio_set_dir(SIG_GPIO, GPIO_IN);
	gpio_put(SIG_GPIO, 1);
}

/** @brief escParkPlan(), entry by entry. */
static void testParkPlan() {
	section("DShot modes: the pad plan");

	const EscPinMode R = EscPinMode::Run, P = EscPinMode::Park, X = EscPinMode::Release;
	EscPadPlan p = escParkPlan(R, P, ESC_NO_GPIO, ESC_NO_GPIO, 0, 1);
	checkTrue("first park: AUX off, then SIG low",
	          p.count == 2 && p.gpio[0] == 1 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 0 && p.pad[1] == EscPad::Low);
	p = escParkPlan(X, P, 0, 1, 0, 1);
	checkTrue("park from Release: each pad written, a job may have left any",
	          p.count == 2 && p.gpio[0] == 1 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 0 && p.pad[1] == EscPad::Low);
	p = escParkPlan(R, P, 0, 1, 2, 1);
	checkTrue("SIG moved after a teardown: the old SIG off, AUX off, the new SIG low",
	          p.count == 3 && p.gpio[0] == 0 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 1 && p.pad[1] == EscPad::Off &&
	          p.gpio[2] == 2 && p.pad[2] == EscPad::Low);
	p = escParkPlan(P, P, 0, 1, 0, 1);
	checkInt("re-park on unchanged pins: nothing written", p.count, 0);
	p = escParkPlan(P, P, 0, 1, 2, 1);
	checkTrue("SIG moved in Park: the old SIG off, the new SIG low, AUX left off",
	          p.count == 2 && p.gpio[0] == 0 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 2 && p.pad[1] == EscPad::Low);
	p = escParkPlan(P, P, 0, 1, 1, 0);
	checkTrue("a swap: the old SIG once, off as the new AUX; the new SIG low",
	          p.count == 2 && p.gpio[0] == 0 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 1 && p.pad[1] == EscPad::Low);
	p = escParkPlan(P, P, 0, 1, 1, 1);
	checkTrue("SIG and AUX on one GPIO: SIG's state, the old SIG off",
	          p.count == 2 && p.gpio[0] == 0 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 1 && p.pad[1] == EscPad::Low);
	p = escParkPlan(P, P, 1, 1, 1, 0);
	checkTrue("from SIG and AUX on one GPIO: the new AUX off, SIG left low",
	          p.count == 1 && p.gpio[0] == 0 && p.pad[0] == EscPad::Off);
	p = escParkPlan(P, X, 0, 1, 0, 1);
	checkTrue("release: AUX a plain input, SIG on its pull-up",
	          p.count == 2 && p.gpio[0] == 1 && p.pad[0] == EscPad::Input &&
	          p.gpio[1] == 0 && p.pad[1] == EscPad::PullUp);
	p = escParkPlan(P, R, 0, 1, 0, 1);
	checkTrue("before a bind: AUX left off, SIG on its pull-up",
	          p.count == 1 && p.gpio[0] == 0 && p.pad[0] == EscPad::PullUp);
	p = escParkPlan(X, R, 0, 1, 0, 1);
	checkTrue("out of another mode each pad written",
	          p.count == 2 && p.gpio[0] == 1 && p.pad[0] == EscPad::Off &&
	          p.gpio[1] == 0 && p.pad[1] == EscPad::PullUp);

	checkTrue("reported: Run only bound",
	          escModeReported(EscPinMode::Run, true, EscPinMode::Run) &&
	          !escModeReported(EscPinMode::Run, false, EscPinMode::Run));
	checkTrue("Park and Release only reached, the pump down",
	          escModeReported(EscPinMode::Park, false, EscPinMode::Park) &&
	          !escModeReported(EscPinMode::Park, true, EscPinMode::Park) &&
	          escModeReported(EscPinMode::Release, false, EscPinMode::Release));
	checkTrue("parked is not released",
	          !escModeReported(EscPinMode::Park, false, EscPinMode::Release));
	checkTrue("before the first bind no mode is reported",
	          !escModeReported(EscPinMode::Run, false, EscPinMode::Park) &&
	          !escModeReported(EscPinMode::Run, false, EscPinMode::Release));
}

/** @brief Park after a throttle frame drains; with none it parks at once. */
static void testParkDrain() {
	section("DShot modes: Park drains, then parks");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	removeJobs();
	fakePwmReset();
	checkTrue("running", escTaskRunning() && !escTaskParked() && !escTaskSuspended());
	escSetArmed(true);
	escSetThrottle(300);
	pump();
	uint32_t stamp = fakeLastThrottleMs();
	checkInt("one throttle frame out", stamp, millis());
	escTaskPark();
	checkTrue("Park disarms", !fakeArmed() && fakeThrottle() == 0);
	int bad = 0, sent = 0;
	for (int i = 0; i < 250; i++) {
		fakeAdvance(1);
		int before = fakeFrameCount();
		pump();
		EscFrame f = fakeLastFrame();
		sent += fakeFrameCount() - before;
		if (f.sendCommand || f.throttle || f.requestKiss) bad++;
	}
	checkTrue("250 ms after the throttle frame: still running", escTaskRunning() && !escTaskParked());
	checkInt("a frame each pass", sent, 250);
	checkInt("each throttle zero, no command, no KISS request", bad, 0);
	checkTrue("SIG not yet parked", !padLow(SIG_GPIO));
	sigLatchHigh();
	int highs = fakePin(SIG_GPIO).highEnables;
	int before = fakeFrameCount();
	fakeAdvance(1);
	g_watch = PadWatch{};
	watchedPump();
	checkTrue("parked at 251 ms", escTaskParked() && !escTaskRunning() && !escTaskSuspended());
	checkInt("SIG's latch was 1: never made an output while high",
	         fakePin(SIG_GPIO).highEnables, highs);
	checkTrue("the pads written", g_watch.writes > 0);
	checkWatch("the park");
	checkInt("with no frame on that pass", fakeFrameCount() - before, 0);
	checkTrue("SIG driven low at 2 mA", padLow(SIG_GPIO));
	checkTrue("AUX on its pull-down, the buffer off", padOff(AUX_GPIO));
	pump();
	checkInt("parked: no frames", fakeFrameCount() - before, 0);

	// The bind at millis() 0, after the wrap, and the park 100 ms later:
	// a drain judged by the time since a stamp of 0 alone would still run.
	escTaskStart();
	uint64_t nowMs = time_us_64() / 1000u;
	checkTrue("the clock is set past the wrap",
	          fakeClockSetMs(((nowMs >> 32) + 1u) << 32));
	checkInt("the clock at 0", millis(), 0);
	pump();
	checkTrue("running again", escTaskRunning());
	checkInt("bound at millis() 0: the stamp 1", escTaskBoundMs(), 1);
	checkInt("no throttle frame since the bind", fakeLastThrottleMs(), 0);
	fakeAdvance(100);
	before = fakeFrameCount();
	escTaskPark();
	pump();
	checkTrue("no throttle frame since the bind: parked on the first pass, 100 ms in",
	          escTaskParked() && fakeFrameCount() == before);
	backToRun();
}

/**
 * @brief Release from Park on the first pass with no frame; Run from Park
 *        binds once.
 */
static void testParkReleaseRun() {
	section("DShot modes: Release and Run from Park");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	removeJobs();
	fakePwmReset();
	checkTrue("parked", toParked());
	int frames = fakeFrameCount();
	int binds = fakeRebuildCount();
	escTaskSuspend();
	checkTrue("asked for, not yet released", !escTaskSuspended());
	g_watch = PadWatch{};
	watchedPump();
	checkTrue("released on the first pass", escTaskSuspended() && !escTaskParked());
	checkWatch("Release from Park");
	checkInt("with no frame", fakeFrameCount() - frames, 0);
	checkTrue("SIG on its pull-up at 4 mA", padPullUp(SIG_GPIO));
	FakePin aux = fakePin(AUX_GPIO);
	checkTrue("AUX a plain input", !aux.output && aux.pull == FakePull::None && !aux.inputOff);
	checkInt("and no bind", fakeRebuildCount() - binds, 0);

	// A job can leave AUX on its pull-down with the buffer on; the park
	// takes the pull-down off before it turns the buffer on again.
	gpio_pull_down(AUX_GPIO);
	escTaskPark();
	g_watch = PadWatch{};
	watchedPump();
	checkTrue("parked again from Release", escTaskParked() && padLow(SIG_GPIO) && padOff(AUX_GPIO));
	checkWatch("Park from Release");

	escTaskStart();
	g_watch = PadWatch{};
	watchedPump();
	checkTrue("Run from Park binds", escTaskRunning() && !escTaskParked());
	checkWatch("Run from Park");
	checkInt("once", fakeRebuildCount() - binds, 1);
	EscFrame f = fakeLastFrame();
	checkTrue("with the EDT enable first",
	          f.sendCommand && f.command == DSHOT_CMD_EDT_ENABLE);
	checkTrue("SIG back on its pull-up at 4 mA before the bind", padPullUp(SIG_GPIO));
	for (int i = 0; i < 20; i++) { fakeAdvance(1); pump(); }
	checkInt("and only once", fakeRebuildCount() - binds, 1);
	backToRun();
}

/** @brief SIG pad writes while SIG was not driven, and while driven at 1. */
static int g_sigUndriven = 0;
static int g_sigWrites = 0;
/** @brief The GPIO hook: SIG driven means the SIO function and the output on. */
static void watchSigDriven(uint32_t) {
	g_sigWrites++;
	FakePin p = fakePin(SIG_GPIO);
	if (p.function != GPIO_FUNC_SIO || !p.output) g_sigUndriven++;
}

/**
 * @brief Parking a SIG a job left as an SIO output driving low keeps it
 *        driven through every write: with the pulls off and the direction
 *        in, an ESC's pull-up would lift the line.
 */
static void testParkAlreadyLow() {
	section("DShot modes: parking a SIG that is already low");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	removeJobs();
	fakePwmReset();
	escTaskSuspend();
	for (int i = 0; i < 400 && !escTaskSuspended(); i++) { fakeAdvance(1); pump(); }
	checkTrue("released", escTaskSuspended());
	// What a job can leave: SIG an SIO output at 0, the pull-up on, 4 mA.
	gpio_init(SIG_GPIO);
	gpio_put(SIG_GPIO, 0);
	gpio_set_dir(SIG_GPIO, GPIO_OUT);
	gpio_pull_up(SIG_GPIO);
	escTaskPark();
	g_sigUndriven = 0;
	g_sigWrites = 0;
	fakeSetGpioHook(watchSigDriven);
	pump();
	fakeSetGpioHook(nullptr);
	checkTrue("parked, SIG low at 2 mA", escTaskParked() && padLow(SIG_GPIO));
	checkTrue("the pads written", g_sigWrites > 0);
	checkInt("SIG driven through every write", g_sigUndriven, 0);
	checkTrue("the pulls off", fakePin(SIG_GPIO).pull == FakePull::None);

	// From an input with the latch at 1, a direct escPadSet(): no write
	// makes it an output at 1.
	sigLatchHigh();
	gpio_pull_up(SIG_GPIO);
	int highs = fakePin(SIG_GPIO).highEnables;
	escPadSet(SIG_GPIO, EscPad::Low);
	checkInt("from an input at 1: never an output while high",
	         fakePin(SIG_GPIO).highEnables, highs);
	checkTrue("then low at 2 mA, the pulls off",
	          padLow(SIG_GPIO) && fakePin(SIG_GPIO).pull == FakePull::None);
	backToRun();
}

/** @brief The guard of core1JobStart() against a parked pin. */
static void testParkJobGuard() {
	section("DShot modes: a job never binds on a parked pin");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	installJobs();
	checkTrue("parked", toParked());
	checkTrue("escTaskSuspended() is false while parked", !escTaskSuspended());
	checkTrue("core1JobStart() refused", !core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	core1JobPoll();
	checkInt("and core1 binds nothing", g_servoJob.binds, 0);
	escTaskSuspend();
	pump();
	checkTrue("released: started", core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	core1JobStop();
	core1JobPoll();
	checkTrue("idle again", core1JobPhase() == Core1JobPhase::Idle);
	removeJobs();
	backToRun();
}

/**
 * @brief A wiring change while parked parks again; while running it
 *        rebuilds.
 */
static void testParkWiring() {
	section("DShot modes: a wiring change");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	removeJobs();
	fakePwmReset();
	sigLatchHigh();
	int highs = fakePin(SIG_GPIO).highEnables;
	checkTrue("parked", toParked());
	checkInt("SIG's latch was 1: never made an output while high",
	         fakePin(SIG_GPIO).highEnables, highs);
	int binds = fakeRebuildCount();

	// A bitrate-only change: the pins are unchanged, and nothing is written.
	FakePin sig = fakePin(SIG_GPIO), aux = fakePin(AUX_GPIO);
	escTaskConfigure(SIG_GPIO, 300, true, AUX_GPIO);
	g_watch = PadWatch{};
	watchedPump();
	checkTrue("a bitrate-only change: still parked", escTaskParked());
	checkInt("no pad written", g_watch.writes, 0);
	FakePin sig2 = fakePin(SIG_GPIO), aux2 = fakePin(AUX_GPIO);
	checkTrue("SIG still an output driving low at 2 mA, never re-initialised",
	          padLow(SIG_GPIO) && sig2.dirAt == sig.dirAt &&
	          sig2.functionAt == sig.functionAt && sig2.puts == sig.puts);
	checkTrue("AUX untouched, on its pull-down",
	          padOff(AUX_GPIO) && aux2.dirAt == aux.dirAt && aux2.pullAt == aux.pullAt);

	escTaskConfigure(NEW_GPIO, 600, true, AUX_GPIO);
	g_watch = PadWatch{};
	watchedPump();
	checkWatch("SIG moved in Park");
	checkTrue("still parked", escTaskParked());
	checkTrue("the old SIG: input, pull-down, buffer off", padOff(SIG_GPIO));
	checkTrue("the new SIG low", padLow(NEW_GPIO));
	checkTrue("AUX on its pull-down", padOff(AUX_GPIO));
	checkInt("no bind", fakeRebuildCount() - binds, 0);

	escTaskConfigure(SIG_GPIO, 600, true, AUX_GPIO);
	pump();
	checkTrue("back on the default wiring: SIG low", padLow(SIG_GPIO));
	checkTrue("GP2 off", padOff(NEW_GPIO));
	escTaskConfigure(AUX_GPIO, 600, true, SIG_GPIO);
	g_watch = PadWatch{};
	watchedPump();
	checkWatch("a swap in Park");
	checkTrue("a swap: the old SIG is AUX, off", padOff(SIG_GPIO));
	checkTrue("the new SIG low", padLow(AUX_GPIO));
	checkInt("still no bind", fakeRebuildCount() - binds, 0);

	escTaskStart();
	pump();
	checkTrue("Run binds on the swapped wiring",
	          escTaskRunning() && fakeResumeWiring().dshotPin == AUX_GPIO);
	escTaskConfigure(SIG_GPIO, 600, true, AUX_GPIO);
	binds = fakeRebuildCount();
	pump();
	checkTrue("while running a change rebuilds, as testHandoverRules() checks",
	          escTaskRunning() && fakeRebuildCount() - binds == 1 &&
	          fakeResumeWiring().dshotPin == SIG_GPIO);
	backToRun();
}

/** @brief A teardown clears initError, so DSERR names the current bind only. */
static void testTeardownError() {
	section("DShot modes: a teardown clears initError");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	removeJobs();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.initError = true;
	fakeSetTelemetry(&t);
	escTaskPark();
	pump();
	escSnapshot(&t);
	checkTrue("parked, initError clear", escTaskParked() && !t.initError);
	escTaskStart();
	pump();
	t.initError = true;
	fakeSetTelemetry(&t);
	escTaskSuspend();
	pump();
	escSnapshot(&t);
	checkTrue("released, initError clear", escTaskSuspended() && !t.initError);
	quiet();
	backToRun();
}

/** @brief Configures a new SIG, once: a SETUP change landing mid-bind. */
static void configureNewSig() { escTaskConfigure(NEW_GPIO, 600, true, AUX_GPIO); }

/** @brief The GPIO hook form of configureNewSig(): once, on the first write. */
static bool g_configured = false;
static void configureOnWrite(uint32_t) {
	if (g_configured) return;
	g_configured = true;
	configureNewSig();
}

/** @brief A configure that lands during a bind or a park is not lost. */
static void testConfigureMidway() {
	section("DShot modes: a configure during a bind or a park");

	uiInitMenu();
	escTaskStart();  // the pump alone, no claim: Run
	freshPump();
	removeJobs();
	fakePwmReset();
	int binds = fakeRebuildCount();
	escTaskConfigure(SIG_GPIO, 300, true, AUX_GPIO);
	fakeSetBindHook(configureNewSig);
	pump();
	fakeSetBindHook(nullptr);
	checkTrue("the rebuild bound the wiring it read",
	          fakeRebuildCount() - binds == 1 && fakeResumeWiring().dshotPin == SIG_GPIO &&
	          fakeResumeWiring().dshotKbaud == 300);
	pump();
	checkTrue("the configure that landed mid-bind rebuilds on the next pass",
	          fakeRebuildCount() - binds == 2 && fakeResumeWiring().dshotPin == NEW_GPIO);
	checkTrue("running on it", escTaskRunning() && escTaskDshotPin() == NEW_GPIO);

	freshPump();
	escTaskPark();
	g_configured = false;
	fakeSetGpioHook(configureOnWrite);
	pump();
	fakeSetGpioHook(nullptr);
	checkTrue("parked on the wiring it read", g_configured && escTaskParked() &&
	          padLow(SIG_GPIO) && !padLow(NEW_GPIO));
	pump();
	checkTrue("the configure that landed mid-park parks again on the next pass",
	          escTaskParked() && padLow(NEW_GPIO) && padOff(SIG_GPIO));
	backToRun();
}

/** @brief escTaskBoundMs() inside a bind, before its stamp. */
static uint32_t g_boundInBind = 0xFFFFFFFFu;
static void boundInBind() { g_boundInBind = escTaskBoundMs(); }

/** @brief escTaskBoundMs(): the bind's clock, 0 while parked or released. */
static void testBoundMs() {
	section("DShot modes: the bind stamp");

	uiInitMenu();
	fakePumpPowerOn();
	checkInt("0 before the first bind", escTaskBoundMs(), 0);
	checkTrue("and no mode reported",
	          !escTaskRunning() && !escTaskParked() && !escTaskSuspended());
	escTaskStart();
	freshPump();
	removeJobs();
	fakePwmReset();
	escTaskConfigure(SIG_GPIO, 300, true, AUX_GPIO);
	pump();
	uint32_t first = millis();
	checkInt("the fake clock at the bind", escTaskBoundMs(), first);
	fakeAdvance(7);
	pump();
	checkInt("unchanged while running", escTaskBoundMs(), first);
	escTaskConfigure(SIG_GPIO, 600, true, AUX_GPIO);
	fakeSetBindHook(boundInBind);
	pump();
	fakeSetBindHook(nullptr);
	checkInt("0 from the teardown to the rebuild's stamp", g_boundInBind, 0);
	checkTrue("a rebuild after escTaskConfigure() stamps again",
	          escTaskBoundMs() == millis() && escTaskBoundMs() != first);
	checkTrue("parked", toParked());
	checkInt("0 once parked", escTaskBoundMs(), 0);
	escTaskStart();
	fakeAdvance(3);
	pump();
	checkInt("Run from Park stamps", escTaskBoundMs(), millis());
	escTaskSuspend();
	pump();
	checkTrue("released", escTaskSuspended());
	checkInt("0 once released", escTaskBoundMs(), 0);
	escTaskStart();
	pump();
	checkInt("Run from Release stamps", escTaskBoundMs(), millis());
	backToRun();
}

// The firmware sources, wherever the suite runs from: the Makefile passes
// their absolute path; without it, the path next to this file's own.
#ifndef MRB_TEST_SRC_DIR
#define MRB_TEST_SRC_DIR ""
#endif

/** @brief The path of @p name in src/. */
static std::string srcPath(const char *name) {
	std::string dir = MRB_TEST_SRC_DIR;
	if (dir.empty()) {
		std::string here = __FILE__;
		size_t slash = here.find_last_of('/');
		dir = (slash == std::string::npos ? std::string(".") : here.substr(0, slash)) +
		      "/../src";
	}
	return dir + "/" + name;
}

/** @brief The text of @p path; empty when it cannot be read. */
static std::string sourceText(const char *path) {
	std::string s;
	FILE *f = fopen(path, "rb");
	if (!f) return s;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
	fclose(f);
	return s;
}

/** @brief The body of the function @p head defines, to its closing `}`. */
static std::string bodyOf(const std::string &code, const char *head) {
	size_t a = code.find(head);
	if (a == std::string::npos) return std::string();
	size_t b = code.find("\n}", a);
	return b == std::string::npos ? std::string() : code.substr(a, b - a);
}

/** @brief Whether @p a occurs in @p text, and before @p b when @p b is given. */
static bool before(const std::string &text, const char *a, const char *b) {
	size_t i = text.find(a);
	if (i == std::string::npos) return false;
	if (!b) return true;
	size_t j = text.find(b, i);
	return j != std::string::npos;
}

/**
 * @brief Whether each of @p steps occurs in @p text, each after the one
 *        before it. A barrier between two statements is a step between them:
 *        the next `__dmb();` after the first must come before the second.
 */
static bool inOrder(const std::string &text,
                    std::initializer_list<const char *> steps) {
	size_t at = 0;
	for (const char *step : steps) {
		size_t i = text.find(step, at);
		if (i == std::string::npos) return false;
		at = i + strlen(step);
	}
	return true;
}

/** @brief Whether @p a occurs in @p text before the first @p b. */
static bool firstBefore(const std::string &text, const char *a, const char *b) {
	size_t i = text.find(a), j = text.find(b);
	return i != std::string::npos && j != std::string::npos && i < j;
}

/** @brief How often @p what occurs in @p text. */
static int occurrences(const std::string &text, const char *what) {
	int n = 0;
	for (size_t i = text.find(what); i != std::string::npos; i = text.find(what, i + 1)) n++;
	return n;
}

/**
 * @brief escTaskPoll() is compiled only, and fakePumpPoll() runs a copy of
 *        its sequence. This checks the shipped sequence for the steps the
 *        copy relies on, so the two cannot drift apart unseen.
 */
static void testPollSource() {
	section("DShot modes: escTaskPoll()'s sequence, as the fake copies it");

	std::string src = sourceText(srcPath("esc_task.cpp").c_str());
	checkTrue("esc_task.cpp read", !src.empty());
	std::string configure = bodyOf(src, "void escTaskConfigure(");
	std::string teardown = bodyOf(src, "static void pumpTeardown() {");
	std::string bind = bodyOf(src, "static void pumpBind() {");
	std::string pads = bodyOf(src, "static void padsTo(EscPinMode to) {");
	std::string poll = bodyOf(src, "void escTaskPoll() {");
	checkTrue("the stamp 0 before the first bind",
	          before(src, "static volatile uint32_t s_boundMs = 0;", nullptr));
	// Slice D2: escTaskInit() asks for Park and parks on core0, after it
	// seeds the wiring; the fake's escTaskInit() copies it.
	std::string init = bodyOf(src, "void escTaskInit() {");
	checkTrue("escTaskInit() seeds the wiring, then asks for Park and parks the pads",
	          !init.empty() && occurrences(init, "s_want = EscPinMode::Park;") == 1 &&
	          occurrences(init, "padsTo(EscPinMode::Park);") == 1 &&
	          before(init, "s_kissPin    = cfg->kissPin;", "padsTo(EscPinMode::Park);") &&
	          before(init, "s_want = EscPinMode::Park;", "padsTo(EscPinMode::Park);") &&
	          occurrences(init, "EscPinMode::Run") == 0);
	{
		// Core0 reads s_reached and s_pumpUp through one byte: each write of
		// either is published, publishMode() within the next two lines.
		int writes = 0, unpublished = 0;
		size_t at = 0;
		while ((at = src.find('\n', at)) != std::string::npos) {
			size_t end = src.find('\n', at + 1);
			std::string line = src.substr(at + 1, end == std::string::npos ? std::string::npos : end - at - 1);
			at++;
			bool decl = line.find("static") != std::string::npos;
			bool w = !decl && (line.find("s_reached = ") != std::string::npos ||
			                   line.find("s_pumpUp = ") != std::string::npos ||
			                   line.find("s_pumpUp  = ") != std::string::npos);
			if (!w) continue;
			writes++;
			size_t e2 = src.find('\n', end + 1);
			std::string next2 = src.substr(end, e2 == std::string::npos ? std::string::npos : src.find('\n', e2 + 1) - end);
			if (next2.find("publishMode();") == std::string::npos) unpublished++;
		}
		checkTrue("the mode's six writes are each published to core0 as one byte", writes == 6 && unpublished == 0);
		std::string getters = bodyOf(src, "static bool modeReported(EscPinMode m) {");
		checkTrue("the three getters read s_report once", occurrences(getters, "s_report") == 1 &&
		          occurrences(src, "return modeReported(EscPinMode::") == 3);
	}
	checkTrue("a heartbeat lapse while armed disarms before the slot is decided, and latches",
	          before(src, "if (escHeartbeatTrips(s_armed, uiAlive)) {", "escPumpSlot(&s_pump") &&
	          before(src, "s_armed = false;\n\t\t__dmb();\n\t\ts_hbTripped = true;", "escPumpSlot(&s_pump"));
	checkTrue("Park asked from the start, before escTaskInit()",
	          before(src, "static volatile EscPinMode s_want = EscPinMode::Park;", nullptr));
	checkTrue("the four functions found",
	          !teardown.empty() && !bind.empty() && !pads.empty() && !poll.empty());
	checkTrue("pumpTeardown() calls escTelTeardown()",
	          before(teardown, "escTelTeardown(&s_tel);", nullptr));
	checkTrue("pumpTeardown() zeroes the bind stamp before the pump goes down",
	          before(teardown, "s_boundMs = 0;", "s_pumpUp = false;"));
	checkTrue("pumpBind() clears the request before it reads the wiring",
	          occurrences(bind, "s_rebuildReq = false;") == 1 &&
	          before(bind, "s_rebuildReq = false;", "= s_dshotPin;") &&
	          before(bind, "s_rebuildReq = false;", "= s_kissEnable"));
	checkTrue("pumpBind() reports no mode before the pads out of Park",
	          before(bind, "s_reached = EscPinMode::Run;", "escPadApply(&plan);"));
	checkTrue("pumpBind() stamps after the bind",
	          before(bind, "out_dshot_bind(", "s_boundMs = "));
	checkTrue("padsTo() clears the request before it reads the wiring",
	          occurrences(pads, "s_rebuildReq = false;") == 1 &&
	          before(pads, "s_rebuildReq = false;", "= s_dshotPin;"));
	checkTrue("padsTo() reports no mode before the pads, the mode after",
	          before(pads, "s_reached = EscPinMode::Run;", "escPadApply(&plan);") &&
	          before(pads, "escPadApply(&plan);", "s_reached = to;"));
	checkTrue("padsTo() clears the request before it reads AUX",
	          firstBefore(pads, "s_rebuildReq = false;", "s_kissPin") &&
	          firstBefore(pads, "s_rebuildReq = false;", "s_dshotPin"));
	checkTrue("escTaskPoll() clears no request itself",
	          occurrences(poll, "s_rebuildReq") == 1);

	// The barriers, each between the two stores or reads it orders.
	checkTrue("escTaskConfigure(): the wiring, a barrier, then the request",
	          !configure.empty() &&
	          inOrder(configure, {"s_kissPin    = kissPin;", "__dmb();",
	                              "s_rebuildReq = true;"}));
	checkTrue("pumpTeardown(): the stamp 0, a barrier, then the stop",
	          inOrder(teardown, {"s_boundMs = 0;", "__dmb();", "out_dshot_stop("}));
	checkTrue("pumpBind(): the request cleared, a barrier, then the wiring",
	          inOrder(bind, {"s_rebuildReq = false;", "__dmb();", "= s_dshotPin;"}));
	checkTrue("pumpBind(): no mode out of Park, a barrier, then the pads",
	          inOrder(bind, {"s_reached = EscPinMode::Run;", "__dmb();",
	                         "escPadApply(&plan);"}));
	checkTrue("pumpBind(): the bind, a barrier, then the stamp",
	          inOrder(bind, {"s_tel.initError", "__dmb();", "s_boundMs = "}));
	checkTrue("pumpTeardown(): the refused flag cleared, a barrier, then the stamp 0",
	          inOrder(teardown, {"s_bindRefused = false;", "__dmb();", "s_boundMs = 0;"}));
	checkTrue("pumpBind(): the refused flag, a barrier, then the stamp",
	          inOrder(bind, {"s_bindRefused = !bound;", "__dmb();", "s_boundMs = "}));
	checkTrue("padsTo(): no mode, a barrier, then the wiring",
	          inOrder(pads, {"s_reached = EscPinMode::Run;", "__dmb();",
	                         "= s_dshotPin;"}));
	checkTrue("padsTo(): the pads, a barrier, then the mode",
	          inOrder(pads, {"escPadApply(&plan);", "__dmb();", "s_reached = to;"}));
}

/**
 * @brief No source in src/ claims a spin lock (cs_shared.h): no
 *        `critical_section_init(` and no `spin_lock_claim_unused(` outside
 *        cs_shared.h's comment. The SDK hands out eight, TinyUSB takes one,
 *        and the ninth claim panics at boot.
 */
static void testNoSpinLockClaims() {
	section("Spin locks: no source in src/ claims one (cs_shared.h)");
	std::string dir = srcPath("");
	DIR *d = opendir(dir.c_str());
	checkTrue("src/ is readable", d != nullptr);
	int files = 0, claims = 0, shared = 0;
	while (d) {
		struct dirent *e = readdir(d);
		if (!e) break;
		std::string n = e->d_name;
		size_t dot = n.rfind('.');
		if (dot == std::string::npos) continue;
		std::string ext = n.substr(dot);
		if (ext != ".cpp" && ext != ".c" && ext != ".h") continue;
		files++;
		std::string text = sourceText((dir + n).c_str());
		for (size_t i = 0; (i = text.find("csInitShared(&", i)) != std::string::npos; i++) shared++;
		if (n == "cs_shared.h") continue;
		for (const char *call : {"critical_section_init(", "spin_lock_claim_unused("}) {
			for (size_t i = 0; (i = text.find(call, i)) != std::string::npos; i++) {
				claims++;
				char what[96];
				snprintf(what, sizeof(what), "src/%s calls %s", n.c_str(), call);
				checkTrue(what, false);
			}
		}
	}
	if (d) closedir(d);
	checkTrue("the walk read the sources", files > 50);
	checkInt("calls that claim a spin lock", claims, 0);
	checkInt("critical sections on the shared locks: the pump, the job slot, six jobs, the current sensor",
	         shared, 9);
	checkInt("claims made by the suite's critical sections", (long)fakeCsClaims(), 0);
}

void runClaimTests() {
	testNoSpinLockClaims();
	section("Core1 jobs: the table as it ships");
	checkTrue("the servo, KST, Hitec, S.BUS and ESC settings jobs are in the table",
	          s_ops[(uint8_t)Core1Job::Servo] == &SERVO_JOB_OPS &&
	          s_ops[(uint8_t)Core1Job::Kst] == &KST_JOB_OPS &&
	          s_ops[(uint8_t)Core1Job::Hitec] == &HITEC_JOB_OPS &&
	          s_ops[(uint8_t)Core1Job::Sbus] == &SBUS_JOB_OPS &&
	          s_ops[(uint8_t)Core1Job::EscWire] == &ESCWIRE_JOB_OPS);
	checkTrue("and the USB bridge's job",
	          s_ops[(uint8_t)Core1Job::Bridge] == &BRIDGE_JOB_OPS);
	checkTrue("and the S.BUS output's job",
	          s_ops[(uint8_t)Core1Job::SbusOut] == &SBUS_OUT_JOB_OPS);
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
	testDeferredJob();
	testRestore();
	testHeartbeat();
	testNoSaveDuringClaim();
	testLeavePagePerClaim();
	testAm32Screen();
	testParkPlan();
	testParkDrain();
	testParkReleaseRun();
	testParkAlreadyLow();
	testParkJobGuard();
	testParkWiring();
	testTeardownError();
	testConfigureMidway();
	testBoundMs();
	testPollSource();

	shippedJobs();
	quiet();
}
