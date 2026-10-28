/**
 * @file kst_card_wait.cpp
 * @brief kstCardWaitPoll() on the firmware: core1 runs alongside, so the
 *        wait for the job's acknowledge only spins. The host suite links its
 *        own, which runs core1. @see kst_job.h
 */

#include "kst_job.h"

#include "pico/stdlib.h"

void kstCardWaitPoll() { tight_loop_contents(); }
