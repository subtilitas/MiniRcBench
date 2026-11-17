/**
 * @file boot_diag.cpp
 * @brief The boot diagnostic's blinks on GP16. @see boot_diag.h
 *
 * Compiled only with `-DBOOT_DIAG=ON`, which defines `BOOT_DIAG=1`. The
 * guard below keeps CI's host syntax check of the sources, which has no
 * hardware register headers, off this file.
 */

#include "boot_diag.h"

#if BOOT_DIAG

#include <pico.h>
#include <hardware/gpio.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/structs/pads_bank0.h>
#include <hardware/structs/sio.h>
#include <hardware/structs/timer.h>

/**
 * @brief The backlight pin. A literal rather than g_board->lcdBl, so the
 *        fault handler reads nothing from flash it does not have to.
 */
#define DIAG_PIN 16u

#define DIAG_ON_US    150000u   /**< One blink's high time. */
#define DIAG_OFF_US   150000u   /**< One blink's low time. */
#define DIAG_PAUSE_US 1000000u  /**< Low time after a group. */
#define DIAG_FAULT_US 50000u    /**< High and low time of the fault blink. */

/** @brief Wait @p us microseconds on TIMER0's raw count. */
static void __not_in_flash_func(diagWait)(uint32_t us) {
	uint32_t t0 = timer0_hw->timerawl;
	while ((uint32_t)(timer0_hw->timerawl - t0) < us) {
	}
}

/** @brief GP16 to high (@p on) or low, through SIO. */
__force_inline static void diagLevel(bool on) {
	if (on) sio_hw->gpio_set = 1u << DIAG_PIN;
	else    sio_hw->gpio_clr = 1u << DIAG_PIN;
}

void bootDiagStep(unsigned step) {
	gpio_function_t was = gpio_get_function(DIAG_PIN);
	diagLevel(false);
	gpio_set_function(DIAG_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(DIAG_PIN, GPIO_OUT);
	for (unsigned i = 0; i < step; i++) {
		diagLevel(true);
		diagWait(DIAG_ON_US);
		diagLevel(false);
		diagWait(DIAG_OFF_US);
	}
	diagWait(DIAG_PAUSE_US);
	if (was != GPIO_FUNC_SIO) gpio_set_function(DIAG_PIN, was);
}

void bootDiagDone() {
	diagLevel(true);
	gpio_set_function(DIAG_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(DIAG_PIN, GPIO_OUT);
}

/**
 * @brief The hard fault handler: GP16 blinks 50 ms on, 50 ms off, forever.
 *
 * Replaces the SDK's weak isr_hardfault. In RAM, and by register only, so it
 * runs with flash or the SDK's state broken. MemManage, BusFault and
 * UsageFault are not enabled, so they escalate to here, as does the
 * breakpoint panic() ends in with no debugger attached.
 */
extern "C" void __not_in_flash_func(isr_hardfault)(void) {
	// The pad: input on, output on, isolation off (RP2350). Then SIO.
	pads_bank0_hw->io[DIAG_PIN] = (pads_bank0_hw->io[DIAG_PIN] &
	                               ~(PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS)) |
	                              PADS_BANK0_GPIO0_IE_BITS;
	io_bank0_hw->io[DIAG_PIN].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
	sio_hw->gpio_oe_set = 1u << DIAG_PIN;
	for (;;) {
		diagLevel(true);
		diagWait(DIAG_FAULT_US);
		diagLevel(false);
		diagWait(DIAG_FAULT_US);
	}
}

#endif  // BOOT_DIAG
