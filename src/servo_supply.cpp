/**
 * @file servo_supply.cpp
 * @brief The programmed servo's switched supply on GP28 or GP29. @see servo_supply.h
 */

#include "servo_supply.h"
#include "settings.h"

#include "hardware/gpio.h"

static uint8_t s_pin   = SERVO_SUPPLY_NO_PIN;  /**< The enable pin; none with None. */
static bool    s_bound = false;
static bool    s_on    = false;

/** @brief @p pin to an undriven input: the external pull-down alone holds EN low. */
static void undriven(uint8_t pin) {
	// Selecting SIO clears the pad's isolation and output disable and turns
	// its input on (Pico SDK 2.1.1, gpio.c), so the input is disabled after it.
	// The input goes off before the pulls, so the pad never has its input on
	// with no pull (erratum RP2350-E9).
	gpio_set_function(pin, GPIO_FUNC_SIO);
	gpio_set_input_enabled(pin, false);
	gpio_set_dir(pin, GPIO_IN);
	gpio_disable_pulls(pin);
}

uint8_t servoSupplyPinOf(uint8_t mode) {
	switch ((ServoSupplyMode)mode) {
		case ServoSupplyMode::Gp28: return 28;
		case ServoSupplyMode::Gp29: return 29;
		default:                    return SERVO_SUPPLY_NO_PIN;
	}
}

uint8_t servoSupplyPin() { return s_pin; }

void servoSupplyInit() { servoSupplyConfigure(settings()->servoSupply); }

void servoSupplyConfigure(uint8_t mode) {
	uint8_t pin = servoSupplyPinOf(mode);
	// None from boot leaves both pins in their reset state; the pin of the
	// mode before stays the undriven input it was set to.
	if (pin != SERVO_SUPPLY_NO_PIN) undriven(pin);
	s_pin = pin;
}

bool servoSupplyFitted() { return s_pin != SERVO_SUPPLY_NO_PIN; }

void servoSupplyBind() {
	if (!servoSupplyFitted()) return;
	// The level first, so the output comes up low.
	gpio_put(s_pin, 0);
	gpio_set_dir(s_pin, GPIO_OUT);
	s_bound = true;
	s_on = false;
}

void servoSupplyOn() {
	if (!s_bound) return;
	gpio_put(s_pin, 1);
	s_on = true;
}

void servoSupplyOff() {
	if (!s_bound) return;
	gpio_put(s_pin, 0);
	s_on = false;
}

void servoSupplyRelease() {
	if (!s_bound) return;
	gpio_put(s_pin, 0);
	undriven(s_pin);
	s_bound = false;
	s_on = false;
}

bool servoSupplyIsOn() { return s_on; }
