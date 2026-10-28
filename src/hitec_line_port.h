/**
 * @file hitec_line_port.h
 * @brief The one Hitec line the Hitec job drives: rcbench's RP2350 line
 *        driver (`hitec_line.h`) on the SIG pin.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * hitec_line_port.cpp is the firmware's: one `hitec_line_t`, one PIO state
 * machine and two DMA channels while open, a second state machine and a
 * third channel for the D series' PRECHARGE receiver. The host suite links a
 * fake in its place, a simulated servo on the virtual clock
 * (test/fake_hitec_line.h). The Hitec job calls these on core1 only, between
 * its bind and its release.
 */

#pragma once

#include <stdint.h>

#include "hitec_wire.h"

/**
 * @brief Take @p gpio for @p family as a released input with the family's
 *        pull: classic open drain, the D series push-pull with the
 *        PRECHARGE receiver.
 * @return 0, `HITEC_LINE_OK`, or the `hitec_line_err_t` that refused it; on
 *         an error nothing is claimed and the pin is not touched.
 */
uint8_t hitecLineOpen(uint8_t gpio, uint8_t family);

/**
 * @brief Give back what the open took and leave the pad an input with the
 *        family's pull. A transaction on the wire is cut. Safe while closed.
 */
void hitecLineClose();

/** @brief Whether the line is open. */
bool hitecLineIsOpen();

/** @brief Start @p xfer. @return False when it did not start. */
bool hitecLineStart(const hitec_xfer_t *xfer);

/** @brief Poll the running transaction. @return True once, with @p out filled, when it has ended. */
bool hitecLinePoll(hitec_capture_t *out);

/** @brief Cut the running transaction; the next poll returns it marked aborted. */
void hitecLineAbort();
