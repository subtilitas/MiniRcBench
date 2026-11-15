// Host-test stub for the header pioasm generates from src/bridge_line.pio.
//
// On device that header is written into the build tree by
// pico_generate_pio_header() and carries the assembled program. Nothing here
// needs the instructions -- bridge_line_port.cpp is typechecked, never linked
// -- so this declares the two symbols the generator exports and stops there.
// test/pio_bridge_emu.py runs the program itself.
#pragma once
#include "hardware/pio.h"

extern const pio_program_t bridge_tx_program;
pio_sm_config bridge_tx_program_get_default_config(uint offset);
