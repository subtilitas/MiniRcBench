/**
 * @file fake_kst_line.h
 * @brief The host suite's KST line: rcbench's simulated servo
 *        (rcbench/kst_sim.h) behind kst_line_port.h, on the virtual clock.
 *
 * The simulator's clock is set from time_us_32() before every call into it,
 * so it moves when fakeAdvance() moves the suite's clock and in whole
 * milliseconds. The servo is powered and outside programming mode after
 * fakeKstLineReset(); a test powers it off and on through fakeKstSim().
 */

#pragma once

#include <stdint.h>

#include "rcbench/kst_sim.h"

/** @brief The simulated servo and wire. */
sim_t *fakeKstSim();

/** @brief A fresh simulator, the line closed, the counts at 0, opens accepted. */
void fakeKstLineReset();

/** @brief Make the next opens fail with @p err, a `kst_line_err_t`; 0 accepts. */
void fakeKstLineRefuse(uint8_t err);

/** @brief Opens that succeeded since fakeKstLineReset(). */
int fakeKstLineOpens();

/** @brief Closes of an open line since fakeKstLineReset(). */
int fakeKstLineCloses();

/** @brief The GPIO of the last open. */
uint8_t fakeKstLinePin();

/** @brief time_us_32() at the last close of an open line. */
uint32_t fakeKstLineClosedUs();

/**
 * @brief Each transaction started from now on stays on the wire @p us
 *        longer than the simulator's frame and window, so a test can land a
 *        cut while one is out; 0, the default after fakeKstLineReset(), for
 *        none.
 */
void fakeKstFrameHold(uint32_t us);

/** @brief A transaction is on the wire: started and not yet polled back. */
bool fakeKstLineBusy();
