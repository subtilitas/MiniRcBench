/**
 * @file servo_supply.cpp
 * @brief The programmed servo's switched supply on GP0. @see servo_supply.h
 */

#include "servo_supply.h"
#include "settings.h"

#include "hardware/gpio.h"

static ServoSupplyMode s_mode  = ServoSupplyMode::None;
static bool            s_bound = false;
static bool            s_on    = false;

/** @brief GP0 to an undriven input: the external pull-down alone holds EN low. */
static void undriven() {
	// Selecting SIO clears the pad's isolation and output disable and turns
	// its input on (Pico SDK 2.1.1, gpio.c), so the input is disabled after it.
	// The input goes off before the pulls, so the pad never has its input on
	// with no pull (erratum RP2350-E9).
	gpio_set_function(SERVO_SUPPLY_PIN, GPIO_FUNC_SIO);
	gpio_set_input_enabled(SERVO_SUPPLY_PIN, false);
	gpio_set_dir(SERVO_SUPPLY_PIN, GPIO_IN);
	gpio_disable_pulls(SERVO_SUPPLY_PIN);
}

void servoSupplyInit() { servoSupplyConfigure(settings()->servoSupply); }

void servoSupplyConfigure(uint8_t mode) {
	ServoSupplyMode m = mode == 1 ? ServoSupplyMode::Gp0 : ServoSupplyMode::None;
	// None from boot leaves GP0 in its reset state; a change away from Gp0
	// leaves it the undriven input Gp0 set.
	if (m == ServoSupplyMode::Gp0 || s_mode == ServoSupplyMode::Gp0) undriven();
	s_mode = m;
}

bool servoSupplyFitted() { return s_mode == ServoSupplyMode::Gp0; }

void servoSupplyBind() {
	if (!servoSupplyFitted()) return;
	// The level first, so the output comes up low.
	gpio_put(SERVO_SUPPLY_PIN, 0);
	gpio_set_dir(SERVO_SUPPLY_PIN, GPIO_OUT);
	s_bound = true;
	s_on = false;
}

void servoSupplyOn() {
	if (!s_bound) return;
	gpio_put(SERVO_SUPPLY_PIN, 1);
	s_on = true;
}

void servoSupplyOff() {
	if (!s_bound) return;
	gpio_put(SERVO_SUPPLY_PIN, 0);
	s_on = false;
}

void servoSupplyRelease() {
	if (!s_bound) return;
	gpio_put(SERVO_SUPPLY_PIN, 0);
	undriven();
	s_bound = false;
	s_on = false;
}

bool servoSupplyIsOn() { return s_on; }
