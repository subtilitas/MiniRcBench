/**
 * @file servo_supply.h
 * @brief The programmed servo's switched supply, for the KST and Hitec
 *        jobs: the enable input, EN, of a DC-DC converter on GP28 (J4 pin 11)
 *        or GP29 (J4 pin 12).
 *
 * Optional. `SERVO SUPPLY` in SETTINGS (@ref Settings::servoSupply) says
 * whether a converter is fitted and on which pin: @ref ServoSupplyMode::None,
 * the default, @ref ServoSupplyMode::Gp28 or @ref ServoSupplyMode::Gp29, an
 * active-high enable. With None nothing here touches either pin. GP28 and
 * GP29 are ADC pins and not 5 V tolerant; the enable is an output at 3.3 V,
 * and the signal pins are GP0 and GP1.
 *
 * The enable pin is driven only between servoSupplyBind() and
 * servoSupplyRelease(), on core1, inside a claim that uses the supply.
 * Outside it the pin is an input with both pulls off and its input disabled,
 * so the converter's external pull-down alone holds EN low: 10 kΩ to 100 kΩ
 * for an EN with no internal pull-up (docs/design/kst-programming.md,
 * "Hardware"). Two cores write the pin's pad, never at once: core0 at boot
 * and at a SETTINGS change, which no claim overlaps, and core1 inside a claim.
 *
 * There is no read-back: servoSupplyIsOn() is the level the bench drives,
 * not the voltage on the servo.
 *
 * @see docs/design/kst-programming.md, "Switched servo supply"
 */

#pragma once

#include <stdint.h>

/** @brief What SETTINGS says is fitted: the value of @ref Settings::servoSupply. */
enum class ServoSupplyMode : uint8_t {
	None = 0,  /**< No switch: no pin is driven. */
	Gp28 = 1,  /**< An active-high enable on GP28, J4 pin 11. */
	Gp29 = 2,  /**< An active-high enable on GP29, J4 pin 12. */
};

/** @brief The highest @ref ServoSupplyMode value. */
#define SERVO_SUPPLY_MODE_MAX 2u

/** @brief No enable pin: @ref ServoSupplyMode::None. */
#define SERVO_SUPPLY_NO_PIN 0xFFu

/** @brief The enable pin of @p mode, a @ref Settings::servoSupply value; @ref SERVO_SUPPLY_NO_PIN for None. */
uint8_t servoSupplyPinOf(uint8_t mode);

/** @brief The enable pin configured now; @ref SERVO_SUPPLY_NO_PIN with None. */
uint8_t servoSupplyPin();

/**
 * @brief Core0, in `setup()` directly after settingsLoad(), before core1
 *        starts: with an enable pin, that pin to an undriven input as
 *        servoSupplyConfigure() sets it; with None, nothing.
 */
void servoSupplyInit();

/**
 * @brief Core0, from SETTINGS outside every claim: the enable pin of @p mode
 *        to an undriven input, SIO function, output disabled, both pulls off,
 *        input disabled. The pin of the mode before stays that way; None from
 *        boot leaves both pins untouched.
 * @param mode The value of @ref Settings::servoSupply, 0 to 2; another value
 *             reads as None.
 */
void servoSupplyConfigure(uint8_t mode);

/** @brief Whether a switch is fitted: a mode other than @ref ServoSupplyMode::None. */
bool servoSupplyFitted();

/**
 * @brief Core1, from the KST or Hitec job's bind, with a switch fitted:
 *        the enable pin driven low, the supply off. Nothing with None.
 */
void servoSupplyBind();

/** @brief Core1, between bind and release: the enable pin high, the supply on. Nothing unbound. */
void servoSupplyOn();

/** @brief Core1, between bind and release: the enable pin low, the supply off. Nothing unbound. */
void servoSupplyOff();

/**
 * @brief Core1, from the KST or Hitec job's release, at every claim end:
 *        the enable pin low, then an input with both pulls off and its input
 *        disabled.
 *        Nothing unbound.
 */
void servoSupplyRelease();

/** @brief The level the bench drives: true while bound and switched on. */
bool servoSupplyIsOn();
