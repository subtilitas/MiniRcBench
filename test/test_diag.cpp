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
		checkStr(what, diagRestartText(diagDecodeRestart(c.bits, 0, false, 0)), c.text);
	}
}

/** @brief Shorthand: the text CHIP_RESET @p cr decodes to, no watchdog reason. */
static const char *flags(uint32_t cr) { return diagRestartText(diagDecodeRestart(cr, 0, false, 0)); }

/**
 * @brief The order: the watchdog first, then the flags of resets that leave
 *        the power manager alone, then those of resets that reset it.
 */
static void testOrder() {
	section("Diagnostics: the decoding order");
	const uint32_t all = DIAG_CR_HAD_POR | DIAG_CR_HAD_BOR | DIAG_CR_HAD_RUN_LOW |
	                     DIAG_CR_HAD_DP_RESET_REQ | DIAG_CR_HAD_RESCUE |
	                     DIAG_CR_HAD_HZD_SYS_RESET_REQ | DIAG_CR_HAD_GLITCH_DETECT |
	                     DIAG_CR_HAD_WATCHDOG_RESET_RSM | DIAG_CR_HAD_WATCHDOG_RESET_SWCORE |
	                     DIAG_CR_HAD_WATCHDOG_RESET_POWMAN | DIAG_CR_HAD_WATCHDOG_RESET_POWMAN_ASYNC |
	                     DIAG_CR_HAD_SWCORE_PD;
	checkStr("every flag: GLITCH, a newer reset", flags(all), "GLITCH");
	// A reset that leaves the power manager alone beside an older flag.
	checkStr("POR and GLITCH", flags(DIAG_CR_HAD_POR | DIAG_CR_HAD_GLITCH_DETECT), "GLITCH");
	checkStr("POR and WATCHDOG RSM", flags(DIAG_CR_HAD_POR | DIAG_CR_HAD_WATCHDOG_RESET_RSM), "WATCHDOG");
	checkStr("POR and WATCHDOG SWCORE", flags(DIAG_CR_HAD_POR | DIAG_CR_HAD_WATCHDOG_RESET_SWCORE), "WATCHDOG");
	checkStr("BOR and HZD debugger", flags(DIAG_CR_HAD_BOR | DIAG_CR_HAD_HZD_SYS_RESET_REQ), "DEBUGGER");
	checkStr("RUN and SWCORE_PD", flags(DIAG_CR_HAD_RUN_LOW | DIAG_CR_HAD_SWCORE_PD), "POWER-DOWN");
	// Among the resets that leave the power manager alone.
	checkStr("GLITCH and WATCHDOG RSM",
	         flags(DIAG_CR_HAD_GLITCH_DETECT | DIAG_CR_HAD_WATCHDOG_RESET_RSM), "GLITCH");
	checkStr("WATCHDOG RSM and HZD",
	         flags(DIAG_CR_HAD_WATCHDOG_RESET_RSM | DIAG_CR_HAD_HZD_SYS_RESET_REQ), "WATCHDOG");
	checkStr("HZD and SWCORE_PD",
	         flags(DIAG_CR_HAD_HZD_SYS_RESET_REQ | DIAG_CR_HAD_SWCORE_PD), "DEBUGGER");
	// Among the resets that reset it.
	const uint32_t pm = DIAG_CR_HAD_POR | DIAG_CR_HAD_BOR | DIAG_CR_HAD_RUN_LOW |
	                    DIAG_CR_HAD_DP_RESET_REQ | DIAG_CR_HAD_WATCHDOG_RESET_POWMAN;
	checkStr("the powman resets: POR first", flags(pm), "POWER-ON");
	checkStr("all but POR", flags(pm & ~DIAG_CR_HAD_POR), "BROWN-OUT");
	checkStr("RUN and RESCUE", flags(DIAG_CR_HAD_RUN_LOW | DIAG_CR_HAD_RESCUE), "RUN PIN");
	checkStr("DP and WATCHDOG POWMAN",
	         flags(DIAG_CR_HAD_DP_RESET_REQ | DIAG_CR_HAD_WATCHDOG_RESET_POWMAN), "DEBUGGER");

	// The watchdog's REASON and the boot type decide before any flag.
	auto dec = [](uint32_t cr, uint32_t reason, bool en, int boot) {
		return diagRestartText(diagDecodeRestart(cr, reason, en, boot));
	};
	checkStr("enable caused, REASON timer", dec(DIAG_CR_HAD_POR, 1, true, 0), "WATCHDOG");
	checkStr("enable caused after a flash update", dec(DIAG_CR_HAD_POR, 1, true, DIAG_BOOT_TYPE_FLASH_UPDATE),
	         "WATCHDOG");
	checkStr("REASON timer, flash update boot", dec(DIAG_CR_HAD_POR, 1, false, DIAG_BOOT_TYPE_FLASH_UPDATE),
	         "UPDATE");
	checkStr("REASON force, flash update boot", dec(0, 2, false, DIAG_BOOT_TYPE_FLASH_UPDATE), "UPDATE");
	checkStr("REASON timer, normal boot", dec(DIAG_CR_HAD_POR, 1, false, 0), "REBOOT");
	checkStr("REASON timer, BOOTSEL boot type 2", dec(0, 1, false, 2), "REBOOT");
	checkStr("REASON timer, boot type 5", dec(0, 1, false, DIAG_BOOT_TYPE_FLASH_UPDATE + 1), "REBOOT");
	checkStr("REASON timer, boot type an error", dec(0, 1, false, -1), "REBOOT");
	checkStr("no REASON, flash update boot type", dec(DIAG_CR_HAD_RUN_LOW, 0, false, DIAG_BOOT_TYPE_FLASH_UPDATE),
	         "RUN PIN");
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

/** @brief diagCaptureRestart() reads both registers and both SDK calls. */
static void testCapture() {
	section("Diagnostics: the capture reads the fakes");
	fakeSetChipReset(DIAG_CR_HAD_BOR);
	fakeSetWatchdog(0, false);
	fakeSetBootType(0);
	diagCaptureRestart();
	checkTrue("CHIP_RESET HAD_BOR: BROWN-OUT", diagLastRestart() == RestartReason::BrownOut);
	fakeSetWatchdog(1, false);
	diagCaptureRestart();
	checkTrue("REASON set, normal boot: REBOOT", diagLastRestart() == RestartReason::Reboot);
	fakeSetBootType(DIAG_BOOT_TYPE_FLASH_UPDATE);
	diagCaptureRestart();
	checkTrue("REASON set, flash update boot: UPDATE", diagLastRestart() == RestartReason::Update);
	fakeSetWatchdog(1, true);
	diagCaptureRestart();
	checkTrue("watchdog_enable_caused_reboot(): WATCHDOG", diagLastRestart() == RestartReason::Watchdog);
	fakeSetWatchdog(0, false);
	fakeSetBootType(0);
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
