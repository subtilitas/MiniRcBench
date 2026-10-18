/**
 * @file servo_pwm.cpp
 * @brief Hardware PWM servo outputs. @see servo_pwm.h
 *
 * Adapted from rcbench firmware/iomcu/src/out_pwm.c, commit 453cf334, MIT.
 */

#include "servo_pwm.h"

#include "out_pwm_map.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

/** @brief One bound pin: what the sharing rules and a release need. */
struct Binding {
	bool     used;     /**< The entry holds a pin. */
	uint8_t  pin;      /**< GPIO. */
	uint16_t rateHz;   /**< Its slice's frame rate, Hz. */
	uint16_t wrap;     /**< The frame's last count, which is the longest pulse. */
};

/** @brief The bindings. Core1 only. */
static Binding s_bound[SERVO_PWM_BINDINGS];

/** @brief The entry holding @p pin, or null. */
static Binding *find(uint8_t pin) {
	for (Binding &b : s_bound)
		if (b.used && b.pin == pin) return &b;
	return nullptr;
}

bool servoPwmBound(uint8_t pin) { return find(pin) != nullptr; }

bool servoPwmBind(uint8_t pin, uint16_t rateHz) {
	// A pin this package does not have: NUM_BANK0_GPIOS is 30 on the
	// RP2350A, and the SDK's mapping is defined only below it.
	if (pin >= NUM_BANK0_GPIOS) return false;
	// The frame's last count, which is also the longest pulse the frame
	// carries: one function answers both. 0 for a rate of 0 and for a period
	// the counter cannot hold.
	uint16_t wrap = out_pwm_pulse_max(rateHz);
	if (wrap == 0) return false;

	// Rebinding a pin at its own rate is a reconfiguration that did not move
	// it; any other change releases first.
	Binding *existing = find(pin);
	if (existing) return existing->rateHz == rateHz;

	// The sharing rules below reason about out_pwm_map.h's fold. The SDK's
	// mapping is what pwm_init() and pwm_set_gpio_level() act on, so the two
	// are compared on every bind and a pin they disagree about is refused.
	uint8_t slice = out_pwm_slice_of(pin);
	if (slice != (uint8_t)pwm_gpio_to_slice_num(pin) ||
	    out_pwm_channel_of(pin) != (uint8_t)pwm_gpio_to_channel(pin))
		return false;

	Binding *slot = nullptr;
	bool running = false;
	for (Binding &b : s_bound) {
		if (!b.used) {
			if (!slot) slot = &b;
			continue;
		}
		if (!out_pwm_same_slice(b.pin, pin)) continue;
		// One compare register is one pulse width: a second pin on it would
		// carry the first one's pulse.
		if (out_pwm_same_compare(b.pin, pin)) return false;
		// One wrap register is one frame rate: a second rate would retime
		// the bound pin.
		if (b.rateHz != rateHz) return false;
		running = true;
	}
	if (!slot) return false;

	// Level 0 first, so muxing the pin cannot put out one frame of whatever
	// the compare register held.
	pwm_set_gpio_level(pin, 0);
	if (!running) {
		pwm_config c = pwm_get_default_config();
		pwm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) /
		                          (float)SERVO_PWM_TICK_HZ);
		pwm_config_set_wrap(&c, wrap);
		pwm_init(slice, &c, true);
	}
	// A slice carrying the other channel is left running: pwm_init()
	// resets its counter, which would step the servo on it.
	gpio_disable_pulls(pin);
	gpio_set_function(pin, GPIO_FUNC_PWM);

	slot->used   = true;
	slot->pin    = pin;
	slot->rateHz = rateHz;
	slot->wrap   = wrap;
	return true;
}

void servoPwmRelease(uint8_t pin) {
	Binding *b = find(pin);
	if (!b) return;
	pwm_set_gpio_level(pin, 0);

	// The slice keeps running while its other channel is still bound.
	bool shared = false;
	for (Binding &o : s_bound)
		if (o.used && &o != b && out_pwm_same_slice(o.pin, pin)) shared = true;
	if (!shared) pwm_set_enabled(out_pwm_slice_of(pin), false);

	// An input held low by the pad pull-down: a pin left on the PWM function
	// would hold whatever level the slice stopped at, and a pin with no pull
	// would float under the servo still on it. DShot sets its own pulls when
	// it binds again.
	gpio_set_function(pin, GPIO_FUNC_SIO);
	gpio_set_dir(pin, GPIO_IN);
	gpio_pull_down(pin);
	b->used = false;
}

void servoPwmWrite(uint8_t pin, uint16_t pulseUs) {
	const Binding *b = find(pin);
	if (!b) return;
	// A level above the wrap is never reached by the counter: the pin would
	// sit high, which is no pulse at all. Clamped to the wrap, the output
	// keeps one count of low and stays a pulse train. The servo model keeps
	// MAX below the wrap (servoMaxPulseFor()), so no profile reaches this.
	pwm_set_gpio_level(pin, pulseUs > b->wrap ? b->wrap : pulseUs);
}
