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
