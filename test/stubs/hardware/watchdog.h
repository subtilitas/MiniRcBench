// Host-test stub for hardware/watchdog.h.
//
// Nothing in the firmware calls watchdog_reboot(): every setting applies
// live. fakes.cpp defines it and counts the calls, so the suite can assert
// that no save reboots.
//
// watchdog_enable_caused_reboot() is what diag.cpp reads at boot; fakes.cpp
// returns what a test set with fakeSetWatchdog().
#pragma once
#include <stdint.h>

extern "C" {
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);
bool watchdog_enable_caused_reboot(void);
}
