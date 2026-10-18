/**
 * @file kst_line_port.h
 * @brief The one KST line the KST job drives: rcbench's RP2350 line driver
 *        (`kst_line.h`) on the SIG pin.
 *
 * kst_line_port.cpp is the firmware's: one `kst_line_t`, one PIO state
 * machine and two DMA channels while open. The host suite links a fake in its
 * place, a simulated servo on the virtual clock (test/fake_kst_line.h). The
 * KST job calls these on core1 only, between its bind and its release.
 */

#pragma once

#include <stdint.h>

#include "kst_session.h"

/**
 * @brief Take @p gpio and drive it low.
 * @return 0, `KST_LINE_OK`, or the `kst_line_err_t` that refused it; on an
 *         error nothing is claimed and the pin is not touched.
 */
uint8_t kstLineOpen(uint8_t gpio);

/**
 * @brief Give back what the open took and leave the pad an input with its
 *        pull-down on. A transaction on the wire is cut. Safe while closed.
 */
void kstLineClose();

/** @brief Whether the line is open. */
bool kstLineIsOpen();

/** @brief The session's driver on the line. Valid while open. */
kst_driver_t kstLineDriver();
