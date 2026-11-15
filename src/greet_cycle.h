/**
 * @file greet_cycle.h
 * @brief ESC SETTINGS' connect cycle: a quiet phase on the pull-up, then a
 *        short burst of greetings, in turn.
 *
 * The greeting holds the line low for at least 6 ms of every 41 ms when it
 * repeats: Bluejay's 150 ms check for a high line would never pass, and the
 * same low bits start an AM32 firmware inside the 46 ms after its power-up.
 * All three bootloaders, once entered, wait on a high line with no timeout.
 * So the line rests high most of the time: @ref ESC_GREET_QUIET_MS with
 * nothing sent, then @ref ESC_GREET_BURST greetings, then quiet again
 * (docs/design/bluejay-blheli-s.md, "The connect cycle", decision 2). The
 * entry times are the bootloaders' sources' loop counts; none is measured.
 *
 * Pure: the screen asks greetCycleDue() each frame, sends a greeting when it
 * says so and tells the cycle with greetCycleSent(). Host-tested.
 */

#pragma once

#include <stdint.h>

#include "config.h"

/** @brief Where the cycle is. */
enum class GreetPhase : uint8_t {
	Quiet,  /**< The line on the pull-up, nothing sent. */
	Greet,  /**< A burst: the next greeting is due. */
};

/** @brief The cycle. Set it up with greetCycleStart(). */
struct GreetCycle {
	uint32_t quietFromMs;  /**< When the quiet phase began, millis(). */
	uint8_t  sent;         /**< Greetings of the running burst; 0 while quiet. */
};

/**
 * @brief Begin with a quiet phase at @p nowMs.
 * @param c     The cycle.
 * @param nowMs millis().
 */
void greetCycleStart(GreetCycle *c, uint32_t nowMs);

/**
 * @brief Where the cycle is at @p nowMs: Greet once @ref ESC_GREET_QUIET_MS
 *        or more have passed since the quiet phase began, and for the rest
 *        of a burst; across a wrap of millis().
 * @param c     The cycle.
 * @param nowMs millis().
 * @return The phase.
 */
GreetPhase greetCyclePhase(const GreetCycle *c, uint32_t nowMs);

/**
 * @brief A greeting went: the burst counts it, and after its
 *        @ref ESC_GREET_BURST th the quiet phase begins at @p nowMs, when
 *        that greeting's wait for an answer has ended.
 * @param c     The cycle.
 * @param nowMs millis() after the greeting.
 */
void greetCycleSent(GreetCycle *c, uint32_t nowMs);
