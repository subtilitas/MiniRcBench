/**
 * @file test_servo_supply.cpp
 * @brief The switched servo supply on GP28 against the fake pads: undriven
 *        outside a claim, low at bind, high and low between bind and
 *        release, an undriven input after it, and untouched with no switch.
 */

#include "check.h"
#include "fakes.h"
#include "servo_supply.h"
#include "settings.h"

#include "hardware/gpio.h"

/** @brief GP28 as the converter's pull-down alone sees it: SIO, not driven, no pulls, input off. */
static bool undriven(const FakePin &p) {
	return p.function == GPIO_FUNC_SIO && !p.output && p.pull == FakePull::None && p.inputOff;
}

void runServoSupplyTests() {
	section("Servo supply: GP28 with no switch, then with one");
	// Runs before the UI suite: nothing has configured the module yet.
	fakePwmReset();
	servoSupplyConfigure(0);
	FakePin p = fakePin(servoSupplyPinOf(1));
	checkTrue("none from boot: GP28 untouched",
	          p.function == 0 && p.pull == FakePull::Untouched && !p.output && p.puts == 0 &&
	          !p.inputOff);
	checkTrue("not fitted", !servoSupplyFitted());
	servoSupplyBind();
	servoSupplyOn();
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("none: bind and on drive nothing", !p.output && p.puts == 0 && !servoSupplyIsOn());
	servoSupplyRelease();
	checkTrue("and release touches nothing", fakePin(servoSupplyPinOf(1)).function == 0);

	settings()->servoSupply = 1;
	servoSupplyInit();
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("init with GP28: an undriven input", undriven(p) && p.puts == 0);
	checkTrue("fitted", servoSupplyFitted());
	servoSupplyOn();
	checkTrue("on before bind: nothing", !fakePin(servoSupplyPinOf(1)).output && !servoSupplyIsOn());

	servoSupplyBind();
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("bind: an output, low", p.output && !p.high && !servoSupplyIsOn());
	checkInt("never made an output while set high", p.highEnables, 0);
	servoSupplyOn();
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("on: high", p.output && p.high && servoSupplyIsOn());
	servoSupplyOff();
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("off: low", p.output && !p.high && !servoSupplyIsOn());
	servoSupplyOn();
	servoSupplyRelease();
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("release with the supply on: low, then an undriven input",
	          !p.high && undriven(p) && !servoSupplyIsOn());
	int puts = p.puts;
	servoSupplyOn();
	servoSupplyOff();
	checkTrue("after release: nothing driven", fakePin(servoSupplyPinOf(1)).puts == puts);

	servoSupplyBind();
	servoSupplyConfigure(1);
	servoSupplyRelease();
	servoSupplyConfigure(0);
	p = fakePin(servoSupplyPinOf(1));
	checkTrue("a change to none: GP28 stays an undriven input", undriven(p) && !servoSupplyFitted());
	servoSupplyBind();
	checkTrue("and bind drives nothing", !fakePin(servoSupplyPinOf(1)).output);

	// The pin of each mode, and a value this build does not know.
	checkTrue("modes to pins: none, GP28, GP29, none",
	          servoSupplyPinOf(0) == SERVO_SUPPLY_NO_PIN && servoSupplyPinOf(1) == 28 &&
	          servoSupplyPinOf(2) == 29 && servoSupplyPinOf(3) == SERVO_SUPPLY_NO_PIN);

	// GP29, and a change from GP28 to GP29: each pin an undriven input, and
	// only the configured one switched.
	fakePwmReset();
	servoSupplyConfigure(1);
	servoSupplyConfigure(2);
	checkTrue("GP28 then GP29: both undriven inputs",
	          undriven(fakePin(28)) && undriven(fakePin(29)) && servoSupplyPin() == 29);
	int puts28 = fakePin(28).puts;
	servoSupplyBind();
	servoSupplyOn();
	checkTrue("GP29 switched on", fakePin(29).output && fakePin(29).high && servoSupplyIsOn());
	checkTrue("GP28 not touched", fakePin(28).puts == puts28 && !fakePin(28).output);
	servoSupplyRelease();
	checkTrue("released: GP29 low and undriven again", undriven(fakePin(29)) && !fakePin(29).high);
	servoSupplyConfigure(0);

	settings()->servoSupply = 0;
	fakePwmReset();
}
