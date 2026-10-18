/**
 * @file ui_servo.h
 * @brief Servo tester screen. Not built yet.
 *
 * Servo PWM on the ESC pin, and on the KISS pin for two servos. Entry:
 * CFG → SETTINGS → SERVO. The design is docs/design/servo-tester.md, and
 * slice 2 of docs/design/pin-claim.md replaces this stub whole. Until then
 * uiServoAvailable() is false, the CFG button is drawn disabled and does
 * not act, and the screen is unreachable.
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Whether the screen is built.
 * @return false: the CFG button is drawn disabled and does not act.
 */
bool uiServoAvailable();

/** @brief Open the screen. Claims nothing. */
void uiServoEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false: the screen leaves on its first frame.
 */
bool uiServoTick(const TouchState *t);
