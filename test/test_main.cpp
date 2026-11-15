/**
 * @file test_main.cpp
 * @brief Entry point for the host test suites.
 *
 * Exit status is 0 or 1, never the failure count. A process exit status is
 * eight bits: returning the count meant that 256 failures reported success, and
 * CI would have gone green on a suite that failed every check it ran.
 */

#include <stdio.h>

#include "check.h"
#include "fakes.h"

int g_failures = 0;

void runAm32Tests();
void runUiTests();
void runKissTests();
void runGfxTests();
void runBlackboxTests();
void runLogRingTests();
void runSettingsTests();
void runServoSupplyTests();
void runSenseTests();
void runSupplyGuardTests();
void runMoveTimerTests();
void runQmi8658Tests();
void runAngleModelTests();
void runAngleUiTests();
void runUsbMscTests();
void runDshotFrameTests();
void runDshotTelemTests();
void runDshotEdtTests();
void runEscCmdTests();
void runKvEstTests();
void runSdioTests();
void runClaimTests();
void runServoTests();
void runKstTests();
void runHitecTests();
void runSbusJobTests();
void runSbusUiTests();
void runSbusOutJobTests();
void runWidgetTests();
void runIdTests();
void runMenuTests();
void runEscCmdUiTests();
void runBackupTests();
void runSdCacheTests();
void runDiagTests();
void runRunSummaryTests();
void runEscwireJobTests();
void runEfm8ViewTests();
void runClockTests();
void runBridgeTests();
void runUsbBridgeTests();
void runBridgeUiTests();
void core1JobInit();

int main() {
	printf("MiniRcBench host tests\n");
	printf("=======================\n");

	// As main.cpp does before core1 starts: the core1 jobs' critical sections
	// are initialised before any suite enters one.
	core1JobInit();

	runAm32Tests();
	// Before the UI suite, whose SETTINGS tests configure the supply.
	runServoSupplyTests();
	runSenseTests();
	runSupplyGuardTests();
	runMoveTimerTests();
	runQmi8658Tests();
	runAngleModelTests();
	runUiTests();
	runKissTests();
	runGfxTests();
	runBlackboxTests();
	runLogRingTests();
	runSettingsTests();
	runUsbMscTests();
	runDshotFrameTests();
	runDshotTelemTests();
	runDshotEdtTests();
	runEscCmdTests();
	runKvEstTests();
	runSdioTests();
	runClaimTests();
	runServoTests();
	runKstTests();
	runHitecTests();
	runSbusJobTests();
	runSbusUiTests();
	runSbusOutJobTests();
	runWidgetTests();
	runIdTests();
	runBackupTests();
	runSdCacheTests();
	runMenuTests();
	runEscCmdUiTests();
	runAngleUiTests();
	runDiagTests();
	runRunSummaryTests();
	runEscwireJobTests();
	runEfm8ViewTests();
	runClockTests();
	runBridgeTests();
	runUsbBridgeTests();
	runBridgeUiTests();

	section("Critical sections");
	checkInt("uses before critical_section_init()", fakeCriticalSectionMisuse(), 0);

	printf("\n=======================\n");
	if (g_failures) printf("%d CHECK(S) FAILED\n", g_failures);
	else            printf("all checks passed\n");
	return g_failures ? 1 : 0;
}
