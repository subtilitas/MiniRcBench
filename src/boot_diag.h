/**
 * @file boot_diag.h
 * @brief The boot diagnostic: blink counts on the backlight pin, GP16.
 *
 * Built only with `-DBOOT_DIAG=ON` (CMakeLists.txt), which compiles
 * boot_diag.cpp and defines `BOOT_DIAG=1`. The default build compiles none
 * of it, and main.cpp's BOOT_STEP() expands to nothing there.
 *
 * Before each step of setup() up to uiInit(), bootDiagStep() takes GP16 as a
 * plain SIO (single-cycle IO) output, blinks the step's number, waits 1 s
 * with the pin low, and gives the pin back to the function it had. A step
 * that never returns leaves the panel dark after its group. bootDiagDone()
 * drives GP16 high for good once setup() returns. A hard fault, which a
 * panic() reaches through its breakpoint, blinks 50 ms on and 50 ms off
 * forever.
 *
 * The timing reads TIMER0's raw 1 MHz count. The SDK's runtime
 * starts that timer before main(), so nothing setup() initialises is needed.
 *
 * README.md, "Boot diagnostic", has the table of steps.
 */

#pragma once

#include <stdint.h>

/** @brief The last step number: the blinks before uiInit(). */
#define BOOT_DIAG_STEPS 12

/**
 * @brief Blink @p step times on GP16, then wait 1 s with the pin low.
 *
 * 150 ms high and 150 ms low per blink. The pin's function is restored
 * afterwards, so a step after st7789Init() finds the backlight's state
 * machine on the pin again.
 *
 * @param step The step about to run, 1 to @ref BOOT_DIAG_STEPS.
 */
void bootDiagStep(unsigned step);

/** @brief Drive GP16 high as a plain output from here on: setup() returned. */
void bootDiagDone();
