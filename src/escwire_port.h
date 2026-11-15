/**
 * @file escwire_port.h
 * @brief The one escwire line the ESC settings job drives: rcbench's RP2350
 *        line driver (`ew_line.h`) on the SIG pin.
 *
 * escwire_port.cpp is the firmware's: one `ew_pin_line_t`, one PIO state
 * machine of 25 words and two DMA channels while open. The host suite links
 * a fake in its place, rcbench's simulated ESC on the virtual clock
 * (test/fake_escwire_port.h). The job calls these on core1 only, between its
 * bind and its release.
 */

#pragma once

#include <stdint.h>

#include "ew_wire.h"

/**
 * @brief Take @p gpio: from the return on the pad is an input with its
 *        pull-down (`EW_LINE_PARK`), until the session drives it.
 * @param gpio The SIG GPIO.
 * @param[out] out The driver for the session, valid until the close.
 * @return 0, `EW_LINE_OK`, or the `ew_line_err_t` that refused it; on an
 *         error nothing is claimed and the pin is not touched.
 */
uint8_t escwirePortOpen(uint8_t gpio, ew_driver_t *out);

/**
 * @brief Give back what the open took and leave the pad an input with its
 *        pull-up, the level the line rests at between the bench's other
 *        users: DShot's teardown leaves the same (esc_task.cpp). A
 *        transaction on the wire is cut. Safe while closed.
 */
void escwirePortClose();
