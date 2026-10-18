/**
 * @file test_kst.cpp
 * @brief The KST programming suite, for a screen that is a stub.
 *
 * Slice 3 of docs/design/pin-claim.md replaces ui_kst.cpp and this file
 * whole, with the tests in docs/design/kst-programming.md.
 */

#include "check.h"
#include "pin_claim.h"
#include "ui_kst.h"

void runKstTests() {
	section("KST programming: the stub screen");

	checkTrue("the screen is not built", !uiKstAvailable());
	uiKstEnter();
	checkTrue("opening it claims nothing", pinClaimHolder() == PinUser::None);
	checkTrue("it leaves on its first frame", !uiKstTick(nullptr));
}
