// Host-test stub for hardware/structs/watchdog.h.
//
// REASON and the scratch registers, the registers diag.cpp reads. watchdog_hw points at a struct
// in fakes.cpp that a test fills before diagCaptureRestart().
#pragma once
#include <stdint.h>

typedef struct {
	volatile uint32_t reason;
	volatile uint32_t scratch[8];
} watchdog_hw_t;

extern watchdog_hw_t g_fakeWatchdog;
#define watchdog_hw (&g_fakeWatchdog)
