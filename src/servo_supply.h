/**
 * @file servo_supply.h
 * @brief The programmed servo's switched supply, for the KST and Hitec
 *        jobs: the enable input, EN, of a DC-DC converter on GP0, J5 pin 2.
 *
 * Optional. `SERVO SUPPLY` in SETTINGS (@ref Settings::servoSupply) says
 * whether a converter is fitted: @ref ServoSupplyMode::None, the default, or
 * @ref ServoSupplyMode::Gp0, an active-high enable. With None nothing here
 * touches GP0, and GP0 keeps its reset state.
 *
 * GP0 is driven only between servoSupplyBind() and servoSupplyRelease(), on
 * core1, inside a claim that uses the supply. Outside it GP0 is an input with
 * both pulls off and its input disabled, so the converter's external
 * pull-down alone holds EN low: 10 kΩ to 100 kΩ for an EN with no internal
 * pull-up (docs/design/kst-programming.md, "Hardware"). Two cores write GP0's
 * pad, never at once: core0 at boot and at a SETTINGS change, which no claim
 * overlaps, and core1 inside a claim.
 *
 * There is no read-back: servoSupplyIsOn() is the level the bench drives,
 * not the voltage on the servo.
 *
 * @see docs/design/kst-programming.md, "Switched servo supply"
 */

#pragma once

#include <stdint.h>

/** @brief The enable pin, GP0. */
#define SERVO_SUPPLY_PIN 0u

/** @brief What SETTINGS says is fitted. */
enum class ServoSupplyMode : uint8_t {
	None = 0,  /**< No switch: GP0 is never driven. */
	Gp0  = 1,  /**< An active-high enable on GP0. */
};

/**
 * @brief Core0, in `setup()` directly after settingsLoad(), before core1
 *        starts: with Gp0, GP0 to an undriven input as servoSupplyConfigure()
 *        sets it; with None, nothing.
 */
void servoSupplyInit();

/**
 * @brief Core0, from SETTINGS outside every claim: GP0 to an undriven input,
 *        SIO function, output disabled, both pulls off, input disabled, for
 *        @p mode. A change to None leaves GP0 that way; None from boot
 *        leaves GP0 untouched.
 * @param mode The value of @ref Settings::servoSupply, 0 or 1.
 */
void servoSupplyConfigure(uint8_t mode);

/** @brief Whether a switch is fitted: @ref ServoSupplyMode::Gp0. */
bool servoSupplyFitted();

/**
 * @brief Core1, from the KST or Hitec job's bind, with a switch fitted:
 *        GP0 driven low, the supply off. Nothing with None.
 */
void servoSupplyBind();

/** @brief Core1, between bind and release: GP0 high, the supply on. Nothing unbound. */
void servoSupplyOn();

/** @brief Core1, between bind and release: GP0 low, the supply off. Nothing unbound. */
void servoSupplyOff();

/**
 * @brief Core1, from the KST or Hitec job's release, at every claim end:
 *        GP0 low, then an input with both pulls off and its input disabled.
 *        Nothing unbound.
 */
void servoSupplyRelease();

/** @brief The level the bench drives: true while bound and switched on. */
bool servoSupplyIsOn();
