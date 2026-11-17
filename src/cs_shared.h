/**
 * @file cs_shared.h
 * @brief Critical sections on the SDK's shared spin locks, so the firmware
 *        claims none of the eight it may claim.
 *
 * `critical_section_init()` claims a spin lock of its own through
 * `spin_lock_claim_unused(true)`. The SDK 2.1.1 hands out only spin locks
 * 24 to 31 that way, eight in all, and panics when none is left. TinyUSB's
 * device event queue takes one in usbDevInit(). The pump's and the core1
 * jobs' critical sections were eight more: the ninth claim, in
 * core1JobInit(), panicked, and the board stopped in setup() before the
 * panel lit (boot diagnostic step 6, then the fault blink).
 *
 * csInitShared() takes one of the striped spin locks, 16 to 23, through
 * `next_striped_spin_lock_num()`, as the SDK's own mutexes and semaphores
 * do. A striped lock may be shared with other critical sections; each
 * section here holds it for a few register or variable accesses and calls
 * nothing that takes another critical section, so sharing costs a short
 * wait and cannot deadlock. Every critical section in `src/` is set up
 * through it: test_claim.cpp checks the sources for `critical_section_init(`.
 */

#pragma once

#include "pico/critical_section.h"
#include "hardware/sync.h"

/**
 * @brief Set up @p cs on the next striped spin lock. Claims no spin lock.
 * @param cs The critical section, not yet initialised.
 */
static inline void csInitShared(critical_section_t *cs) {
	critical_section_init_with_lock_num(cs, next_striped_spin_lock_num());
}
