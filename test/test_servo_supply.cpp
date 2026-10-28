/**
 * @file test_servo_supply.cpp
 * @brief The switched servo supply on GP0 against the fake pads: undriven
 *        outside a claim, low at bind, high and low between bind and
 *        release, an undriven input after it, and untouched with no switch.
 */

#include "check.h"
#include "fakes.h"
#include "servo_supply.h"
#include "settings.h"

#include "hardware/gpio.h"

/** @brief GP0 as the converter's pull-down alone sees it: SIO, not driven, no pulls, input off. */
static bool undriven(const FakePin &p) {
	return p.function == GPIO_FUNC_SIO && !p.output && p.pull == FakePull::None && p.inputOff;
}

void runServoSupplyTests() {
	section("Servo supply: GP0 with no switch, then with one");
	// Runs before the UI suite: nothing has configured the module yet.
	fakePwmReset();
	servoSupplyConfigure(0);
	FakePin p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("none from boot: GP0 untouched",
	          p.function == 0 && p.pull == FakePull::Untouched && !p.output && p.puts == 0 &&
	          !p.inputOff);
	checkTrue("not fitted", !servoSupplyFitted());
	servoSupplyBind();
	servoSupplyOn();
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("none: bind and on drive nothing", !p.output && p.puts == 0 && !servoSupplyIsOn());
	servoSupplyRelease();
	checkTrue("and release touches nothing", fakePin(SERVO_SUPPLY_PIN).function == 0);

	settings()->servoSupply = 1;
	servoSupplyInit();
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("init with GP0: an undriven input", undriven(p) && p.puts == 0);
	checkTrue("fitted", servoSupplyFitted());
	servoSupplyOn();
	checkTrue("on before bind: nothing", !fakePin(SERVO_SUPPLY_PIN).output && !servoSupplyIsOn());

	servoSupplyBind();
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("bind: an output, low", p.output && !p.high && !servoSupplyIsOn());
	checkInt("never made an output while set high", p.highEnables, 0);
	servoSupplyOn();
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("on: high", p.output && p.high && servoSupplyIsOn());
	servoSupplyOff();
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("off: low", p.output && !p.high && !servoSupplyIsOn());
	servoSupplyOn();
	servoSupplyRelease();
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("release with the supply on: low, then an undriven input",
	          !p.high && undriven(p) && !servoSupplyIsOn());
	int puts = p.puts;
	servoSupplyOn();
	servoSupplyOff();
	checkTrue("after release: nothing driven", fakePin(SERVO_SUPPLY_PIN).puts == puts);

	servoSupplyBind();
	servoSupplyConfigure(1);
	servoSupplyRelease();
	servoSupplyConfigure(0);
	p = fakePin(SERVO_SUPPLY_PIN);
	checkTrue("a change to none: GP0 stays an undriven input", undriven(p) && !servoSupplyFitted());
	servoSupplyBind();
	checkTrue("and bind drives nothing", !fakePin(SERVO_SUPPLY_PIN).output);

	settings()->servoSupply = 0;
	fakePwmReset();
}
