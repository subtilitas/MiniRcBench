/**
 * @file kst_line_port.cpp
 * @brief The KST line on rcbench's RP2350 driver. @see kst_line_port.h
 */

#include "kst_line_port.h"
#include "kst_line.h"

static kst_line_t s_line;
/** @brief kst_line_init() is owed once, before the first open. */
static bool       s_init = false;

uint8_t kstLineOpen(uint8_t gpio) {
	if (!s_init) {
		kst_line_init(&s_line);
		s_init = true;
	}
	return (uint8_t)kst_line_open(&s_line, gpio);
}

void kstLineClose() {
	if (s_init) kst_line_close(&s_line);
}

bool kstLineIsOpen() { return s_init && kst_line_is_open(&s_line); }

kst_driver_t kstLineDriver() { return kst_line_driver(&s_line); }
