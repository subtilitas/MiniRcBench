/**
 * @file usb_dev.h
 * @brief The USB device: a serial port and a card reader on one cable.
 *
 * @section usbdev_why Why this file exists at all
 *
 * The board used to get USB serial for free from `pico_enable_stdio_usb(1)`.
 * That is not a library you add things to — it owns the USB descriptors and
 * runs its own polling loop, and the SDK says so: pico_stdio_usb requires that
 * the user not be using TinyUSB directly. Adding a mass storage interface means
 * taking the descriptors over, and taking the descriptors over means providing
 * the serial port ourselves.
 *
 * So this file is the price of the USB drive feature, and it is worth naming
 * plainly: about a hundred lines of descriptor tables and a forty-line stdio
 * driver, replacing one line of CMake. In exchange the device is ours, and a
 * second interface costs a descriptor entry rather than a rewrite.
 *
 * @section usbdev_layout What the host sees
 *
 * One composite device, two functions:
 *
 * - **CDC ACM** on interfaces 0 and 1 — the serial port, exactly as before, so
 *   `SD_DEBUG` output still arrives and picocom still works. It reads one
 *   line command, the clock's (clock_line.h).
 * - **MSC** on interface 2 — a removable drive that reports *no medium* until
 *   the user asks for it. @see usb_msc.h
 *
 * The IAD descriptor in front of the CDC pair is what stops Windows treating a
 * composite device as one broken function; without it the serial port
 * enumerates and the drive does not, or the other way round depending on the
 * host's mood.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Bring USB up: TinyUSB, the descriptors, and stdio over CDC.
 *
 * Call instead of `stdio_init_all()`, before anything printf()s. Returns
 * immediately; enumeration happens in the background and usbDevTask() drives
 * it.
 */
void usbDevInit();

/**
 * @brief Service USB. Call from the core0 loop, as often as convenient.
 *
 * Must be called from core0 only, and must keep being called while a host is
 * reading the card — this is what moves the data. It is bounded work: TinyUSB
 * processes what has arrived and returns. Then the bytes the host sent on the
 * serial port go to the clock's line command, up to one line a call, which
 * a set of the clock answers after 770 µs on the bus (clock_line.h).
 */
void usbDevTask();

/**
 * @brief Turn the clock's line command on or off; on from power-up.
 *
 * For a user that takes the serial port's bytes over, the USB bridge: off
 * when it takes the port, on when it gives it back. While off,
 * usbDevTask() reads no byte of the host's, so the bridge gets them all and
 * no `CLOCK` answer lands in its stream. Turning it on forgets the part of a
 * line read before it was turned off. Core0 only, as usbDevTask().
 * @param on On.
 */
void usbDevClockLineEnable(bool on);

/**
 * @brief Give the serial port to the console, or take it from it.
 *
 * With @p on false the stdio driver is disabled, so `printf()` writes
 * nowhere, the telemetry line of main.cpp included, and the port's bytes in
 * both directions belong to the caller: the USB bridge (usb_bridge.h), from
 * its open until the claim is Free (docs/design/usb-bridge.md, "USB side").
 * With true the driver is enabled again. On at start-up; a call that
 * changes nothing does nothing.
 *
 * @param on True: the console has the port.
 */
void usbDevConsole(bool on);

/** @brief Whether the console has the serial port. @return True from start-up until usbDevConsole(false). */
bool usbDevConsoleOn();
