/**
 * @file test_diag.cpp
 * @brief The restart reason's decoding, its capture through the SDK fakes,
 *        and the uptime's text. @see diag.h
 *
 * The screen side, the DIAGNOSTICS rows on SETTINGS, is in test_menu.cpp.
 */

#include "check.h"
#include "diag.h"
#include "fakes.h"

#include <string.h>

/** @brief Each CHIP_RESET flag alone, with no watchdog reason. */
static void testEachFlag() {
	section("Diagnostics: each CHIP_RESET flag alone");
	struct { uint32_t bits; const char *text; } cases[] = {
		{ DIAG_CR_HAD_POR,                         "POWER-ON" },
		{ DIAG_CR_HAD_BOR,                         "BROWN-OUT" },
		{ DIAG_CR_HAD_RUN_LOW,                     "RUN PIN" },
		{ DIAG_CR_HAD_DP_RESET_REQ,                "DEBUGGER" },
		{ DIAG_CR_HAD_RESCUE,                      "DEBUGGER" },
		{ DIAG_CR_HAD_HZD_SYS_RESET_REQ,           "DEBUGGER" },
		{ DIAG_CR_HAD_GLITCH_DETECT,               "GLITCH" },
		{ DIAG_CR_HAD_WATCHDOG_RESET_RSM,          "WATCHDOG" },
		{ DIAG_CR_HAD_WATCHDOG_RESET_SWCORE,       "WATCHDOG" },
		{ DIAG_CR_HAD_WATCHDOG_RESET_POWMAN,       "WATCHDOG" },
		{ DIAG_CR_HAD_WATCHDOG_RESET_POWMAN_ASYNC, "WATCHDOG" },
		{ DIAG_CR_HAD_SWCORE_PD,                   "POWER-DOWN" },
		{ 0,                                       "UNKNOWN" },
		// RESCUE_FLAG and DOUBLE_TAP are not reasons.
		{ 0x00000011u,                             "UNKNOWN" },
	};
	for (const auto &c : cases) {
		char what[48];
		snprintf(what, sizeof(what), "0x%08X", (unsigned)c.bits);
		checkStr(what, diagRestartText(diagDecodeRestart(c.bits, false, false)), c.text);
	}
}

/** @brief The order: the watchdog functions first, then POR before the rest. */
static void testOrder() {
	section("Diagnostics: the decoding order");
	const uint32_t all = DIAG_CR_HAD_POR | DIAG_CR_HAD_BOR | DIAG_CR_HAD_RUN_LOW |
	                     DIAG_CR_HAD_DP_RESET_REQ | DIAG_CR_HAD_GLITCH_DETECT |
	                     DIAG_CR_HAD_WATCHDOG_RESET_RSM | DIAG_CR_HAD_SWCORE_PD;
	checkStr("every flag", diagRestartText(diagDecodeRestart(all, false, false)), "POWER-ON");
	checkStr("all but POR", diagRestartText(diagDecodeRestart(all & ~DIAG_CR_HAD_POR, false, false)),
	         "BROWN-OUT");
	checkStr("RUN and debugger",
	         diagRestartText(diagDecodeRestart(DIAG_CR_HAD_RUN_LOW | DIAG_CR_HAD_RESCUE, false, false)),
	         "RUN PIN");
	checkStr("debugger and glitch",
	         diagRestartText(diagDecodeRestart(DIAG_CR_HAD_RESCUE | DIAG_CR_HAD_GLITCH_DETECT, false, false)),
	         "DEBUGGER");
	checkStr("glitch and watchdog",
	         diagRestartText(diagDecodeRestart(DIAG_CR_HAD_GLITCH_DETECT | DIAG_CR_HAD_WATCHDOG_RESET_RSM,
	                                           false, false)),
	         "GLITCH");
	checkStr("watchdog and power-down",
	         diagRestartText(diagDecodeRestart(DIAG_CR_HAD_WATCHDOG_RESET_POWMAN | DIAG_CR_HAD_SWCORE_PD,
	                                           false, false)),
	         "WATCHDOG");
	// A watchdog reset that reaches only the power-on state machine can leave
	// CHIP_RESET as the power-on left it: the SDK's answer decides.
	checkStr("POR with watchdog_caused_reboot()",
	         diagRestartText(diagDecodeRestart(DIAG_CR_HAD_POR, true, false)), "REBOOT");
	checkStr("POR with watchdog_enable_caused_reboot()",
	         diagRestartText(diagDecodeRestart(DIAG_CR_HAD_POR, true, true)), "WATCHDOG");
	checkStr("enable caused without caused",
	         diagRestartText(diagDecodeRestart(0, false, true)), "WATCHDOG");
	checkStr("caused, nothing in CHIP_RESET",
	         diagRestartText(diagDecodeRestart(0, true, false)), "REBOOT");
}

/** @brief Every text fits DIAG_RESTART_TEXT_WIDEST, and the widest reaches it. */
static void testTextWidths() {
	section("Diagnostics: every reason's text within its widest");
	size_t widest = 0;
	for (int r = 0; r <= (int)RestartReason::Unknown; r++) {
		const char *t = diagRestartText((RestartReason)r);
		if (strlen(t) > widest) widest = strlen(t);
	}
	checkInt("the longest text, characters", (long)widest, DIAG_RESTART_TEXT_WIDEST);
}

/** @brief diagCaptureRestart() reads the register and both SDK calls. */
static void testCapture() {
	section("Diagnostics: the capture reads the fakes");
	fakeSetChipReset(DIAG_CR_HAD_BOR);
	fakeSetWatchdogCaused(false, false);
	diagCaptureRestart();
	checkTrue("CHIP_RESET HAD_BOR: BROWN-OUT", diagLastRestart() == RestartReason::BrownOut);
	fakeSetWatchdogCaused(true, false);
	diagCaptureRestart();
	checkTrue("watchdog_caused_reboot(): REBOOT", diagLastRestart() == RestartReason::Reboot);
	fakeSetWatchdogCaused(true, true);
	diagCaptureRestart();
	checkTrue("watchdog_enable_caused_reboot(): WATCHDOG", diagLastRestart() == RestartReason::Watchdog);
	fakeSetWatchdogCaused(false, false);
	fakeSetChipReset(DIAG_CR_HAD_RUN_LOW);
	diagCaptureRestart();
	checkTrue("HAD_RUN_LOW: RUN PIN", diagLastRestart() == RestartReason::RunPin);
	fakeSetChipReset(0);
	diagCaptureRestart();
	checkTrue("nothing set: UNKNOWN", diagLastRestart() == RestartReason::Unknown);
	// Left as a power-on, as the screenshots show it.
	fakeSetChipReset(DIAG_CR_HAD_POR);
	diagCaptureRestart();
	checkTrue("HAD_POR: POWER-ON", diagLastRestart() == RestartReason::PowerOn);
}

/** @brief The uptime as H:MM:SS, hours not capped. */
static void testUptimeText() {
	section("Diagnostics: the uptime's text");
	char b[24];
	diagUptimeText(b, sizeof(b), 0);
	checkStr("0 s", b, "0:00:00");
	diagUptimeText(b, sizeof(b), 59);
	checkStr("59 s", b, "0:00:59");
	diagUptimeText(b, sizeof(b), 60);
	checkStr("60 s", b, "0:01:00");
	diagUptimeText(b, sizeof(b), 3599);
	checkStr("3599 s", b, "0:59:59");
	diagUptimeText(b, sizeof(b), 3600);
	checkStr("3600 s", b, "1:00:00");
	diagUptimeText(b, sizeof(b), 27 * 3600 + 3 * 60 + 9);
	checkStr("27 h 3 min 9 s", b, "27:03:09");
	diagUptimeText(b, sizeof(b), 0xFFFFFFFFu);
	checkStr("the largest", b, "1193046:28:15");
	checkInt("the largest's characters", (long)strlen(b), DIAG_UPTIME_TEXT_WIDEST);
	uint32_t s0 = diagUptimeSeconds();
	fakeAdvance(999);
	fakeAdvance(1001);
	checkInt("seconds from the timer: 2000 ms is 2 s more",
	         (long)(diagUptimeSeconds() - s0), 2);
}

void runDiagTests() {
	testEachFlag();
	testOrder();
	testTextWidths();
	testCapture();
	testUptimeText();
}
