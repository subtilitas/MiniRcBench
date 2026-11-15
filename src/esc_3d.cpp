/**
 * @file esc_3d.cpp
 * @brief The tester's 3D note. @see esc_3d.h
 */

#include "esc_3d.h"

/** @brief The note stands; RAM only, so a restart clears it. */
static bool s_note = false;

bool esc3dNote() { return s_note; }

void esc3dSetNote(bool on) { s_note = on; }
