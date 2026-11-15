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
 * contact once, as an ESC that has just entered its bootloader, unless
 * fakeEwAnswerEach() asks it to answer every one.
 *
 * The pad: the fake writes what rcbench's driver writes to GP0's pad and
 * control register (ew_line.c) to the GPIO fake, so a test sees the pulls,
 * the input buffer and the function in one order with the job's own
 * writes. The open: the pull-down, the PIO function (`GPIO_FUNC_PIO2`), the
 * input off. `EW_LINE_UP` and the start of a transaction: the pull-up, the
 * input on. `EW_LINE_PARK`, an abort and a capture marked aborted or
 * overflow: the pull-down, the input off. The close: the pull-down,
 * gpio_init()'s SIO input, low, then escwire_port.cpp's pull-up. The levels
 * the state machine drives are not modelled.
 */

#pragma once

#include <stdint.h>

#include "rcbench/ew_sim.h"

/** @brief The simulated ESC; tests read and set its fields. */
sim_t *fakeEwSim();

/**
 * @brief A fresh EFM8 ESC with signature 0xE8 @p sig, Bluejay 0.21's page
 *        when @p bluejay, else BLHeli_S 16.7's (ew_sim.h); the line closed,
 *        the counts at 0, opens accepted. Every byte outside the settings
 *        page holds a pattern with no 0xFF, and the flash is snapshotted.
 */
void fakeEwReset(uint8_t sig, bool bluejay);

/**
 * @brief A fresh AM32 ESC in its bootloader, rcbench's model (ew_sim.h): its
 *        answer to the first contact is AM32's, family 0x35 at the fifth
 *        byte from the end. The line and the counts as fakeEwReset() leaves
 *        them.
 */
void fakeEwResetAm32();

/**
 * @brief While @p on, every first contact is answered as the first: the
 *        bootloader reads the contact's leading 0x00 as a restart and the
 *        rest as a new greeting (docs/design/bluejay-blheli-s.md, "The
 *        greeting already fits"). Off after fakeEwReset().
 */
void fakeEwAnswerEach(bool on);

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

/** @brief Take the simulated ESC's whole flash as the one fakeEwOnlySettingsPage() compares with. */
void fakeEwSnapshot();

/**
 * @brief Whether the simulated ESC's flash outside its settings page, the
 *        bootloader's area and the firmware among it, is byte for byte as
 *        at the last fakeEwReset() or fakeEwSnapshot(), and whether every
 *        erase and program frame the ESC saw came after an address frame
 *        naming the settings page with no read between: nothing but the
 *        settings page was erased or programmed. False as well when the ESC
 *        saw more transactions than its log holds (`SIM_LOG_MAX`).
 */
bool fakeEwOnlySettingsPage();

/**
 * @brief Whether transaction @p index sent a leave: 0x00 0x01 and their CRC.
 * @param index The transaction, from 0 since fakeEwReset().
 */
bool fakeEwSentLeave(int index);
