// Host-test stub for pico/multicore.h.
//
// The launch is a no-op here: the host suite drives core0's logic directly and
// there is no second core to start. Nothing in the tests depends on core1
// running, which is deliberate -- escSnapshot() is the only way across, so the
// UI is testable with core1 absent.
#pragma once
#include <stddef.h>
#include <stdint.h>
void multicore_launch_core1(void (*entry)(void));
/** @brief The SDK's default core1 stack size, 2 KB. */
#ifndef PICO_CORE1_STACK_SIZE
#define PICO_CORE1_STACK_SIZE 0x800
#endif
void multicore_launch_core1_with_stack(void (*entry)(void), uint32_t *stack_bottom, size_t stack_size_bytes);
void multicore_reset_core1(void);

// The lockout API, used by settings_flash.cpp to park core1 across a flash
// erase. Declared so that file typechecks; never called, because it is not
// linked here.
void multicore_lockout_victim_init(void);
void multicore_lockout_start_blocking(void);
void multicore_lockout_end_blocking(void);
