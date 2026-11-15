/**
 * @file escwire_port.cpp
 * @brief The escwire line on rcbench's RP2350 driver. @see escwire_port.h
 */

#include "escwire_port.h"
#include "ew_line.h"

#include <hardware/gpio.h>

static ew_pin_line_t s_line;
/** @brief ew_line_init() is owed once, before the first open. */
static bool          s_init = false;
/** @brief The GPIO of the open line; 0xFF while closed. */
static uint8_t       s_gpio = 0xFF;

uint8_t escwirePortOpen(uint8_t gpio, ew_driver_t *out) {
	if (!s_init) {
		ew_line_init(&s_line);
		s_init = true;
	}
	ew_line_err_t e = ew_line_open(&s_line, gpio);
	if (e != EW_LINE_OK) return (uint8_t)e;
	s_gpio = gpio;
	*out = ew_line_driver(&s_line);
	return (uint8_t)EW_LINE_OK;
}

void escwirePortClose() {
	if (!s_init) return;
	ew_line_close(&s_line);
	if (s_gpio != 0xFF) gpio_pull_up(s_gpio);
	s_gpio = 0xFF;
}
