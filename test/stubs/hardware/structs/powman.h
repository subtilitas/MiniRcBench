// Host-test stub for hardware/structs/powman.h.
//
// Only CHIP_RESET, the register diag.cpp reads. powman_hw points at a struct
// in fakes.cpp that a test fills before diagCaptureRestart().
#pragma once
#include <stdint.h>

typedef struct {
	volatile uint32_t chip_reset;
} powman_hw_t;

extern powman_hw_t g_fakePowman;
#define powman_hw (&g_fakePowman)
