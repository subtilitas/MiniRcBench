// Host-test stub for the header pioasm generates from src/backlight.pio.
//
// On device that header is written into the build tree by
// pico_generate_pio_header() and carries the assembled program. Nothing here
// needs the instructions -- st7789.cpp is typechecked, never linked -- so
// this declares the two symbols the generator exports and stops there.
#pragma once
#include "hardware/pio.h"

extern const pio_program_t backlight_program;
pio_sm_config backlight_program_get_default_config(uint offset);
