/**
 * @file greet_cycle.cpp
 * @brief The connect cycle. @see greet_cycle.h
 */

#include "greet_cycle.h"

void greetCycleStart(GreetCycle *c, uint32_t nowMs) {
	c->quietFromMs = nowMs;
	c->sent = 0;
}

GreetPhase greetCyclePhase(const GreetCycle *c, uint32_t nowMs) {
	// A burst under way began after the quiet phase, which starts again only
	// after its last greeting.
	return nowMs - c->quietFromMs >= ESC_GREET_QUIET_MS ? GreetPhase::Greet : GreetPhase::Quiet;
}

void greetCycleSent(GreetCycle *c, uint32_t nowMs) {
	if (++c->sent < ESC_GREET_BURST) return;
	greetCycleStart(c, nowMs);
}
