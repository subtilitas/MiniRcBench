/**
 * @file test_claim.cpp
 * @brief The pin claim and the core1 job dispatch, against the fake pump.
 *
 * pin_claim.cpp is linked as it ships. core1_job.cpp is included rather than
 * linked, so the tests can put fake jobs in its job table: no job is built
 * yet, and the claim's job path is exercised with jobs that record what the
 * claim and the dispatch did to them. The Makefile therefore must not compile
 * core1_job.cpp separately.
 *
 * The fake pump (fakes.cpp) drains and suspends on the shipped escDrainDone()
 * rule and rebuilds at once on escTaskResume(). Core1 is not a thread here: a
 * test runs core1's side by calling core1JobPoll() itself, which is how the
 * order of the two cores' steps is put under test.
 */

#include "check.h"
#include "fakes.h"
#include "config.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "settings.h"
#include "ui.h"
#include "ui_widgets.h"
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
static void servoRelease() { g_servoJob.releases++; }
static uint8_t kstBind(uint8_t sig, uint8_t aux) { return bindInto(&g_kstJob, sig, aux); }
static void kstStep(uint32_t, bool alive) { g_kstJob.steps++; g_kstJob.uiAlive = alive; }
static void kstRelease() { g_kstJob.releases++; }

static const Core1JobOps FAKE_SERVO = { servoBind, servoStep, servoRelease };
static const Core1JobOps FAKE_KST   = { kstBind, kstStep, kstRelease };

/** @brief Build both jobs, with fresh records. */
static void installJobs() {
	memset(&g_servoJob, 0, sizeof(g_servoJob));
	memset(&g_kstJob, 0, sizeof(g_kstJob));
	s_ops[(uint8_t)Core1Job::Servo] = &FAKE_SERVO;
	s_ops[(uint8_t)Core1Job::Kst]   = &FAKE_KST;
}

/** @brief Back to what ships: no job built. */
static void removeJobs() {
	s_ops[(uint8_t)Core1Job::Servo] = nullptr;
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

/** @brief The board's default wiring, KISS on, and long past any drain. */
static void freshPump() {
	escTaskConfigure(SIG_GPIO, 600, true, AUX_GPIO);
	quiet();
	fakeAdvance(1000);
}

/**
 * @brief Drive any claim back to Free, playing both cores and the unplug
 *        page, and report whether it got there.
 */
static bool toFree() {
	PinUser who = pinClaimHolder();
	pinClaimEnd(who);
	for (int i = 0; i < 4 && pinClaimState() != ClaimState::Free; i++) {
		core1JobPoll();
		pinClaimPoll();
		pinClaimRestart(who);
	}
	return pinClaimState() == ClaimState::Free;
}

/** @brief One UI frame of the real UI. */
static void frames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); uiTick(); }
}

static void tap(int x, int y) {
	fakePress(x, y); frames(2); fakeRelease(); frames(1);
}

// Coordinates from ui.cpp and ui_widgets.h.
#define C_ARM_X    58
#define C_ARM_Y   299
#define C_HOLD_X  143
#define C_CFG_X   205
#define C_THR_Y   260
#define C_AM32_X   39   // BTN_AM32 spans x 14..63
#define C_NAV_Y   282   // the destination row spans y 258..305
#define C_BACK_X  (UI_BACK_X + UI_BACK_W / 2)
#define C_BACK_Y  (UI_BACK_Y + UI_BACK_H / 2)

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
}

/** @brief pinClaimBegin() against the live state: a refusal changes nothing. */
static void testBeginRefusals() {
	section("Pin claim: refusals change nothing");

	uiInit();
	freshPump();
	removeJobs();

	// Armed through the UI, which is the arm state the claim reads.
	fakePress(C_ARM_X, C_ARM_Y); frames(1);
	for (int i = 0; i < 60; i++) { fakeHold(C_ARM_X, C_ARM_Y); frames(1); }
	fakeRelease(); frames(2);
	checkTrue("armed through the UI", uiArmed());
	int resumes = fakeResumeCount();
	checkTrue("armed: refused", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::Armed);
	checkTrue("the claim stays free", pinClaimState() == ClaimState::Free);
	checkTrue("DShot keeps the pins", fakePinReturned());
	checkTrue("and the claim does not disarm", fakeArmed() && uiArmed());
	fakePress(C_ARM_X, C_ARM_Y); frames(1); fakeRelease(); frames(2);
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

/** @brief The zero-throttle drain before the pump lets go of the pin. */
static void testDrain() {
	section("Pin claim: the drain before teardown");

	checkTrue("no throttle frame since the bind: no drain",
	          escDrainDone(0, 12345, ARM_ZERO_THROTTLE_MS));
	checkTrue("249 ms after a throttle frame: draining",
	          !escDrainDone(5000, 5249, ARM_ZERO_THROTTLE_MS));
	checkTrue("250 ms after: done", escDrainDone(5000, 5250, ARM_ZERO_THROTTLE_MS));
	checkTrue("across the millis() rollover: draining at 249 ms",
	          !escDrainDone(0xFFFFFF00u, 0xFFFFFF00u + 249u, ARM_ZERO_THROTTLE_MS));
	checkTrue("and done at 250 ms",
	          escDrainDone(0xFFFFFF00u, 0xFFFFFF00u + 250u, ARM_ZERO_THROTTLE_MS));

	EscFrame zero = {false, 0, 0, false};
	EscFrame thr  = {false, 0, 1, false};
	EscFrame cmd  = {true, DSHOT_CMD_BEEP1, 0, false};
	checkInt("a zero frame keeps the stamp", escThrottleStamp(&zero, 77, 900), 77);
	checkInt("a command frame keeps it", escThrottleStamp(&cmd, 77, 900), 77);
	checkInt("a throttle frame stamps it", escThrottleStamp(&thr, 77, 900), 900);
	checkInt("a throttle frame at millis() 0 stamps 1", escThrottleStamp(&thr, 0, 0), 1);

	// Through the claim, for a caller that skips the UI: a throttle frame,
	// then a claim at once.
	uiInit();
	freshPump();
	removeJobs();
	escSetArmed(true);
	escSetThrottle(300);
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("the claim forces a disarm", !fakeArmed() && fakeThrottle() == 0);
	checkTrue("stopping", pinClaimPoll() == ClaimState::Stopping);
	fakeAdvance(249);
	checkTrue("no teardown 249 ms after the throttle frame",
	          pinClaimPoll() == ClaimState::Stopping && !escTaskSuspended());
	checkTrue("no GPIO latched yet", pinClaimGpio(PIN_SIG) == PIN_CLAIM_NO_GPIO);
	fakeAdvance(1);
	checkTrue("held at 250 ms", pinClaimPoll() == ClaimState::Held);
	checkTrue("back to free", toFree());

	fakeAdvance(5000);
	checkTrue("after a long disarm", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("held on the first step", pinClaimPoll() == ClaimState::Held);
	checkTrue("back to free", toFree());
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
	checkTrue("restoring is settled once the pump runs",
	          pinClaimPoll() == ClaimState::Free);
	checkTrue("nothing held", pinClaimHolder() == PinUser::None);
	checkInt("the GPIO is no longer latched", pinClaimGpio(PIN_SIG), PIN_CLAIM_NO_GPIO);

	// KST, SIG only: the job gets no AUX pin.
	checkTrue("KST claims", pinClaimBegin(PinUser::Kst, PIN_SIG) == ClaimRefusal::None);
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

	// AM32: no job, no unplug page, DShot back within the BACK call.
	resumes = fakeResumeCount();
	checkTrue("AM32 claims", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	checkTrue("held with no job", core1JobRunning() == Core1Job::None);
	pinClaimPoll();
	checkTrue("AM32 stays held", pinClaimState() == ClaimState::Held);
	pinClaimEnd(PinUser::Am32);
	checkTrue("AM32 gets no unplug page", !pinClaimUnplugDue());
	checkInt("DShot resumes within BACK", fakeResumeCount(), resumes + 1);
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
	checkTrue("DShot down", escTaskSuspended());
	checkTrue("Core1Job::None: refused", !core1JobStart(Core1Job::None, SIG_GPIO, 0xFF));
	checkTrue("DShot down: started", core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	checkTrue("a second start while one is up: refused",
	          !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	core1JobPoll();
	core1JobStop();
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
	escTaskResume();

	removeJobs();
	escTaskSuspend();
	checkTrue("no job built: refused", !core1JobStart(Core1Job::Servo, SIG_GPIO, 0xFF));
	checkTrue("nor KST", !core1JobStart(Core1Job::Kst, SIG_GPIO, 0xFF));
	escTaskResume();
	checkTrue("idle, no error", core1JobRunning() == Core1Job::None &&
	                            core1JobError() == CORE1_JOB_OK);
}

/** @brief BACK in every state of a SERVO claim. */
static void testBackEveryState() {
	section("Pin claim: BACK in every state");

	uiInit();
	freshPump();
	installJobs();
	int resumes;

	// Stopping: a throttle frame just went out, so the drain holds the claim.
	escSetArmed(true);
	escSetThrottle(200);
	resumes = fakeResumeCount();
	checkTrue("SERVO claims during a drain",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	checkTrue("stopping", pinClaimPoll() == ClaimState::Stopping);
	pinClaimEnd(PinUser::Servo);
	checkInt("BACK while stopping: DShot back at once", fakeResumeCount(), resumes + 1);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	checkInt("and no job was ever bound", g_servoJob.binds, 0);
	fakeAdvance(1000);

	// Held, the job asked for and not yet bound.
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
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
	checkTrue("free", pinClaimPoll() == ClaimState::Free);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK while free does nothing", pinClaimState() == ClaimState::Free);

	// Held, the bind failed.
	g_servoJob.bindResult = CORE1_JOB_PWM;
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pinClaimPoll();
	core1JobPoll();
	checkTrue("the bind failed: still held", pinClaimPoll() == ClaimState::Held);
	checkInt("and says why", pinClaimJobError(), CORE1_JOB_PWM);
	checkTrue("binds nothing, so releases nothing", g_servoJob.releases == 0 &&
	                                               core1JobPhase() == Core1JobPhase::Idle);
	resumes = fakeResumeCount();
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK after a failed bind: the unplug page", pinClaimUnplugDue());
	pinClaimRestart(PinUser::Servo);
	checkInt("then DShot", fakeResumeCount(), resumes + 1);
	checkTrue("free", pinClaimPoll() == ClaimState::Free);
	g_servoJob.bindResult = CORE1_JOB_OK;

	// Held, the start refused: what ships, with no servo job built.
	removeJobs();
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	checkTrue("held", pinClaimPoll() == ClaimState::Held);
	checkInt("the start was refused", pinClaimJobError(), CORE1_JOB_REFUSED);
	pinClaimEnd(PinUser::Servo);
	checkTrue("BACK: the unplug page", pinClaimUnplugDue());
	pinClaimRestart(PinUser::Servo);
	checkTrue("free", pinClaimPoll() == ClaimState::Free);
	installJobs();

	// Running, then BACK on the unplug page: the claim is kept and the job
	// starts again.
	checkTrue("claims", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	pinClaimPoll();
	core1JobPoll();
	checkTrue("running", pinClaimPoll() == ClaimState::Running);
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
	FakeWiring before = fakeWiring();
	int configures = fakeConfigureCount();
	escSetArmed(true);   // the fake pump armed, the UI not

	checkTrue("two servos claim",
	          pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX) == ClaimRefusal::None);
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
	checkTrue("across the rollover at 249 ms: alive",
	          escUiAlive(0xFFFFFF9Cu + 249u, 0xFFFFFF9Cu));
	checkTrue("and dead at 250 ms", !escUiAlive(0xFFFFFF9Cu + 250u, 0xFFFFFF9Cu));

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

	tap(C_CFG_X, C_ARM_Y);
	frames(2);
	tap(C_AM32_X, C_NAV_Y);
	checkTrue("AM32 holds the claim", pinClaimHolder() == PinUser::Am32);
	frames(4);
	checkInt("the transport gets the pin the claim latched", fakeAm32Pin(),
	         escTaskDshotPin());
	checkTrue("no core1 job", core1JobRunning() == Core1Job::None);
	tap(C_BACK_X, C_BACK_Y);
	checkTrue("BACK gives DShot the pin, no unplug page",
	          fakePinReturned() && !pinClaimUnplugDue());
	checkInt("resumed once", fakeResumeCount(), resumes + 1);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	tap(C_BACK_X, C_BACK_Y);
	frames(2);

	// Straight from a running motor: CFG disarms, and AM32 is opened before
	// 250 ms have passed since the last throttle frame. Its one change of
	// behaviour on the claim is that the handover waits for the drain.
	fakePress(C_ARM_X, C_ARM_Y); frames(1);
	for (int i = 0; i < 60; i++) { fakeHold(C_ARM_X, C_ARM_Y); frames(1); }
	fakeRelease(); frames(2);
	tap(C_HOLD_X, C_ARM_Y);
	fakePress(120, C_THR_Y); frames(1);
	for (int x = 120; x <= 180; x += 10) { fakeHold(x, C_THR_Y); frames(1); }
	fakeRelease(); frames(1);
	checkTrue("armed with a throttle", uiArmed() && fakeThrottle() > 0);
	tap(C_CFG_X, C_ARM_Y);
	checkTrue("CFG disarms", !uiArmed() && !fakeArmed());
	tap(C_AM32_X, C_NAV_Y);
	// The last throttle frame went out on the second frame of the CFG press,
	// 100 ms before the claim: three 25 ms frames for each tap.
	checkTrue("AM32 waits for the drain", pinClaimState() == ClaimState::Stopping);
	frames(5);
	checkTrue("still waiting 225 ms after the throttle frame",
	          pinClaimState() == ClaimState::Stopping);
	frames(1);
	checkTrue("and takes the pin at 250 ms", pinClaimState() == ClaimState::Held);
	tap(C_BACK_X, C_BACK_Y);
	checkTrue("free", pinClaimState() == ClaimState::Free);

	// Another claim already holds: not reachable through the UI, since only
	// SETTINGS opens a claiming screen. AM32 says so and asks again each frame.
	checkTrue("a SERVO claim holds",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	tap(C_AM32_X, C_NAV_Y);
	checkTrue("AM32 is refused", pinClaimHolder() == PinUser::Servo);
	frames(2);
	checkTrue("and waits", pinClaimHolder() == PinUser::Servo);
	tap(C_BACK_X, C_BACK_Y);
	checkTrue("BACK from AM32 leaves the other claim alone",
	          pinClaimHolder() == PinUser::Servo);
	tap(C_AM32_X, C_NAV_Y);
	toFree();
	frames(1);
	checkTrue("and claims the frame after the other one ends",
	          pinClaimHolder() == PinUser::Am32);
	tap(C_BACK_X, C_BACK_Y);
	checkTrue("free", pinClaimState() == ClaimState::Free);
	tap(C_BACK_X, C_BACK_Y);
	frames(2);
}

void runClaimTests() {
	core1JobInit();
	removeJobs();

	testCheckRules();
	testBeginRefusals();
	testDrain();
	testOrder();
	testJobGuard();
	testBackEveryState();
	testRestore();
	testHeartbeat();
	testNoSaveDuringClaim();
	testAm32Screen();

	removeJobs();
	quiet();
}
