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
#include <hardware/structs/sio.h>
#include <hardware/structs/timer.h>

/**
 * @brief The backlight pin. A literal rather than g_board->lcdBl, so the
 *        steps read no board table.
 */
#define DIAG_PIN 16u

#define DIAG_ON_US    150000u   /**< One blink's high time. */
#define DIAG_OFF_US   150000u   /**< One blink's low time. */
#define DIAG_PAUSE_US 1000000u  /**< Low time after a group. */

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

#endif  // BOOT_DIAG
