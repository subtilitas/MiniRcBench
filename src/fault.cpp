/**
 * @file fault.cpp
 * @brief The hard fault handler. @see fault.h
 *
 * Firmware only: the guard keeps the host suite's syntax check of the
 * sources, which has no hardware register headers, off this file.
 */

#include "fault.h"

#if PICO_ON_DEVICE

#include <pico.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/structs/pads_bank0.h>
#include <hardware/structs/psm.h>
#include <hardware/structs/sio.h>
#include <hardware/structs/timer.h>
#include <hardware/structs/watchdog.h>

#include "diag.h"

/** @brief The backlight pin, a literal: the handler reads no board table. */
#define FAULT_PIN 16u
/** @brief High and low time of one blink. */
#define FAULT_BLINK_US 50000u

/** @brief Wait @p us microseconds on TIMER0's raw count. */
static void __not_in_flash_func(faultWait)(uint32_t us) {
	uint32_t t0 = timer0_hw->timerawl;
	while ((uint32_t)(timer0_hw->timerawl - t0) < us) {
	}
}

/** @brief One blink of GP16: high, then low. */
static void __not_in_flash_func(faultBlink)() {
	sio_hw->gpio_set = 1u << FAULT_PIN;
	faultWait(FAULT_BLINK_US);
	sio_hw->gpio_clr = 1u << FAULT_PIN;
	faultWait(FAULT_BLINK_US);
}

/**
 * @brief The handler's body, with the stacked exception frame. In RAM.
 * @param frame R0, R1, R2, R3, R12, LR, PC, xPSR as the core stacked them.
 */
extern "C" void faultHandlerC(const uint32_t *frame);

extern "C" void __not_in_flash_func(faultHandlerC)(const uint32_t *frame) {
	watchdog_hw->scratch[DIAG_FAULT_SCRATCH_MARK] = DIAG_FAULT_MAGIC;
	watchdog_hw->scratch[DIAG_FAULT_SCRATCH_PC] = frame[6];

	// The pad: input on, output on, isolation off (RP2350). Then SIO.
	pads_bank0_hw->io[FAULT_PIN] = (pads_bank0_hw->io[FAULT_PIN] &
	                                ~(PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS)) |
	                               PADS_BANK0_GPIO0_IE_BITS;
	io_bank0_hw->io[FAULT_PIN].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
	sio_hw->gpio_oe_set = 1u << FAULT_PIN;

#if BOOT_DIAG
	for (;;) faultBlink();
#else
	for (int i = 0; i < FAULT_BLINKS; i++) faultBlink();
	// As watchdog_enable() selects: every block but the two oscillators.
	hw_set_bits(&psm_hw->wdsel, PSM_WDSEL_BITS & ~(PSM_WDSEL_ROSC_BITS | PSM_WDSEL_XOSC_BITS));
	watchdog_hw->ctrl = WATCHDOG_CTRL_TRIGGER_BITS;
	for (;;) {
	}
#endif
}

/**
 * @brief The vector: the stack the core used, then faultHandlerC(). Replaces
 *        the SDK's weak isr_hardfault.
 */
extern "C" __attribute__((naked)) void __not_in_flash_func(isr_hardfault)(void) {
	__asm volatile(
	    " movs r0, #4          \n"
	    " mov  r1, lr          \n"
	    " tst  r0, r1          \n"
	    " beq  1f              \n"
	    " mrs  r0, psp         \n"
	    " b    faultHandlerC   \n"
	    "1:                    \n"
	    " mrs  r0, msp         \n"
	    " b    faultHandlerC   \n");
}

#endif  // PICO_ON_DEVICE
