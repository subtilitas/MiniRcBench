/**
 * @file ui_registry.h
 * @brief The MENU rows, in MENU order: one line per tool or action.
 *
 * A new tool is a `UiScreen` in a file of its own and one line in
 * `ui_registry.cpp`. The line holds the row's label and its widest status,
 * and a build-time check keeps every label clear of its status on the
 * 240 px panel. A tool's persisted settings rows, `UiScreen::settings`, join
 * SETTINGS between WIRING and DISPLAY in this order.
 *
 * @see docs/design/menu.md, "Code"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

/** @brief The MENU rows. */
extern const UiMenuEntry UI_MENU[];
/** @brief How many. */
extern const uint8_t UI_MENU_COUNT;
