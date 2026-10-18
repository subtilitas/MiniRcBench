/**
 * @file test_servo.cpp
 * @brief The Servo tester suite, for a screen that is a stub.
 *
 * Slice 2 of docs/design/pin-claim.md replaces ui_servo.cpp and this file
 * whole, with the tests in docs/design/servo-tester.md.
 */

#include "check.h"
#include "pin_claim.h"
#include "ui_servo.h"

void runServoTests() {
	section("Servo tester: the stub screen");

	checkTrue("the screen is not built", !uiServoAvailable());
	uiServoEnter();
	checkTrue("opening it claims nothing", pinClaimHolder() == PinUser::None);
	checkTrue("it leaves on its first frame", !uiServoTick(nullptr));
}
