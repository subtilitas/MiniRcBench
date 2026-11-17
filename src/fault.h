/**
 * @file fault.h
 * @brief The hard fault handler: it marks the fault, blinks the backlight
 *        pin and restarts the board.
 *
 * Every build links it in place of the SD library's handler in `crash.c`,
 * which CMakeLists.txt renames out. That handler calls `__breakpoint()`
 * before its reset; with no debugger attached a breakpoint inside the
 * hard fault handler locks the core up, so the board would hang until a
 * power cycle, with a dark panel when the fault comes before st7789Init().
 *
 * On a hard fault, on either core, the handler:
 *
 * 1. writes @ref DIAG_FAULT_MAGIC to watchdog scratch register 0 and the
 *    faulting program counter (PC) to scratch register 1, which survive a
 *    watchdog reset;
 * 2. takes GP16, the backlight, as a plain SIO (single-cycle IO) output and
 *    blinks it 50 ms on and 50 ms off, @ref FAULT_BLINKS times, 2 s in all;
 * 3. triggers a watchdog reset of everything but the oscillators.
 *
 * With `-DBOOT_DIAG=ON` it blinks forever instead of step 3, so the boot
 * diagnostic's last group stays readable.
 *
 * diagCaptureRestart() reads the mark at the next boot, and SETUP shows
 * LAST RESTART `FAULT` with the PC as the row's caption. A fault that
 * repeats at every boot blinks for 2 s, restarts, and blinks again.
 *
 * MemManage, BusFault and UsageFault are not enabled, so they escalate to
 * the hard fault, as does the breakpoint a panic() ends in. Runs from RAM
 * and by register only, so it works with flash or the SDK's state broken.
 * Not tested on hardware. The firmware builds it; the host suite tests the
 * mark's decoding in diag.cpp.
 */

#pragma once

/** @brief Blinks before the restart, 50 ms on and 50 ms off each: 2 s. */
#define FAULT_BLINKS 20
