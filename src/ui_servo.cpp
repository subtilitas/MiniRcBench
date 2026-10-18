/**
 * @file ui_servo.cpp
 * @brief Servo tester screen, not built yet. @see ui_servo.h
 *
 * A stub. The CFG entry is drawn disabled while uiServoAvailable() is false,
 * so nothing calls the other two.
 */

#include "ui_servo.h"

bool uiServoAvailable() { return false; }

void uiServoEnter() {}

bool uiServoTick(const TouchState *t) {
	(void)t;
	return false;
}
