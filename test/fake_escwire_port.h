/**
 * @file fake_escwire_port.h
 * @brief The host suite's escwire line: rcbench's simulated ESC (ew_sim.h)
 *        behind escwire_port.h, on the virtual clock.
 *
 * The simulator answers protocol A byte for byte from its flash, as an EFM8
 * part with Bluejay's or BLHeli_S's page (ew_sim.h). Its clock is
 * time_us_32(): every call of the driver reads it first, so a transaction
 * ends at the virtual time its answer is done, and the job's passes, one a
 * millisecond, see it then. The simulator does not model the greeting of
 * ESC SETTINGS or a second first contact: it answers escwire's first
 * contact once, as an ESC that has just entered its bootloader.
 */

#pragma once

#include <stdint.h>

#include "rcbench/ew_sim.h"

/** @brief The simulated ESC; tests read and set its fields. */
sim_t *fakeEwSim();

/**
 * @brief A fresh EFM8 ESC with signature 0xE8 @p sig, Bluejay 0.21's page
 *        when @p bluejay, else BLHeli_S 16.7's (ew_sim.h); the line closed,
 *        the counts at 0, opens accepted.
 */
void fakeEwReset(uint8_t sig, bool bluejay);

/** @brief Make the next opens fail with @p err, an `ew_line_err_t`; 0 accepts. */
void fakeEwRefuse(uint8_t err);

/** @brief Opens that succeeded since fakeEwReset(). */
int fakeEwOpens();

/** @brief Closes of an open line since fakeEwReset(). */
int fakeEwCloses();

/** @brief The GPIO of the last open. */
uint8_t fakeEwGpio();

/** @brief Whether the line is open. */
bool fakeEwOpen();

/** @brief time_us_32() at the last close of an open line. */
uint32_t fakeEwClosedUs();

/**
 * @brief Whether transaction @p index sent a leave: 0x00 0x01 and their CRC.
 * @param index The transaction, from 0 since fakeEwReset().
 */
bool fakeEwSentLeave(int index);
