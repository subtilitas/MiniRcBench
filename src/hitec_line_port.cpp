/**
 * @file hitec_line_port.cpp
 * @brief The Hitec line on rcbench's RP2350 driver. @see hitec_line_port.h
 */

#include "hitec_line_port.h"
#include "hitec_line.h"

static hitec_line_t s_line;
/** @brief hitec_line_init() is owed once, before the first open. */
static bool         s_init = false;

uint8_t hitecLineOpen(uint8_t gpio, uint8_t family) {
	if (!s_init) {
		hitec_line_init(&s_line);
		s_init = true;
	}
	if (family == HITEC_CLASSIC)
		return (uint8_t)hitec_line_open(&s_line, gpio, family, HITEC_DRIVE_OPEN_DRAIN);
	return (uint8_t)hitec_line_open_rx(&s_line, gpio, family, HITEC_DRIVE_PUSH_PULL,
	                                   HITEC_RX_PRECHARGE);
}

void hitecLineClose() {
	if (s_init) hitec_line_close(&s_line);
}

bool hitecLineIsOpen() { return s_init && hitec_line_is_open(&s_line); }

bool hitecLineStart(const hitec_xfer_t *xfer) { return hitec_line_start(&s_line, xfer); }

bool hitecLinePoll(hitec_capture_t *out) { return hitec_line_poll(&s_line, out); }

void hitecLineAbort() { hitec_line_abort(&s_line); }
