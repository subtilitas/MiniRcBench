/**
 * @file servo_pwm.h
 * @brief Servo pulses from the RP2350's hardware PWM slices. Core1.
 *
 * Adapted from rcbench firmware/iomcu/src/out_pwm.c, commit 453cf334, MIT.
 * Kept: the bind, release and write rules and the sharing rules between pins.
 * Changed: two bindings instead of the bank's slot count, for the servo
 * tester's SIG and AUX; rcbench's `link_pages.h` and `outputs.h` are not
 * included, and the stamped write is left out.
 *
 * PWM is pulse-width modulation. A slice is two channels, A and B, counting
 * against one wrap register, so the two pins of a slice share a frame rate; a
 * channel is one compare register, which is the pulse width. The counter runs
 * at 1 MHz (`clk_sys` divided down), so one count is 1 us and a pulse in us is
 * a compare value. lib/rcbench/shared/outputs/out_pwm_map.h has the GPIO
 * (general-purpose input/output) to slice and channel fold.
 *
 * A bound pin starts at level 0: no edges, the line low. That is the servo
 * tester's SAFE output.
 *
 * Every function here runs on core1, the core that owns the claimed pins
 * (core1_job.h). None of them blocks.
 */

#pragma once

#include <stdint.h>

/** @brief Pins that may be bound at once: SIG and AUX. */
#define SERVO_PWM_BINDINGS 2

/** @brief The PWM counter's rate, Hz: one count per us. */
#define SERVO_PWM_TICK_HZ 1000000u

/**
 * @brief Take @p pin for servo pulses at @p rateHz frames a second.
 *
 * Refused for a pin the package does not have, for a rate whose period does
 * not fit the 16-bit counter, for a pin whose compare register a bound pin
 * holds, for a pin on a slice bound at another rate, when both bindings are
 * in use, and when the SDK's slice or channel for the pin differs from
 * out_pwm_map.h's. Binding a bound pin at its own rate succeeds and changes
 * nothing. The pin starts at level 0. A slice that carries the other channel
 * is left running, so the servo on it does not step.
 *
 * @param pin    GPIO.
 * @param rateHz Frame rate, Hz.
 * @return True when the pin is bound.
 */
bool servoPwmBind(uint8_t pin, uint16_t rateHz);

/**
 * @brief Give the pin back: level 0, the slice stopped unless its other
 *        channel is still bound, and the pin an input with the pad pull-down
 *        on. Nothing for a pin that is not bound.
 *
 * The level-0 write takes effect only at the end of the frame under way, so a
 * caller that must not cut a pulse short writes level 0 and waits
 * servoQuietMs() first, as the servo job does.
 * @param pin GPIO.
 */
void servoPwmRelease(uint8_t pin);

/**
 * @brief Emit @p pulseUs on every frame from the next one. The compare
 *        register is double-buffered, so a pulse on the wire completes.
 *
 * 0 stops the pin edging: the line stays low. A pulse longer than the frame
 * is clamped to the frame's last count, so there is always one count of low.
 * Nothing for a pin that is not bound.
 *
 * @param pin     GPIO.
 * @param pulseUs Pulse, us; 0 for none.
 */
void servoPwmWrite(uint8_t pin, uint16_t pulseUs);

/**
 * @brief Whether @p pin is bound.
 * @param pin GPIO.
 * @return True between a successful servoPwmBind() and servoPwmRelease().
 */
bool servoPwmBound(uint8_t pin);
