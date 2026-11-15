/**
 * @file ui_registry.h
 * @brief The MENU rows, in MENU order: one line per tool or action.
 *
 * A new tool is a `UiScreen` in a file of its own and one line in
 * `ui_registry.cpp`. The line holds the row's label and its widest status,
 * and a build-time check keeps every label clear of its status on the
 * 240 px panel. A tool's persisted settings rows, `UiScreen::settings`, join
 * SETTINGS between SERVO SUPPLY and DISPLAY in this order.
 *
 * The rows go by device (docs/design/menu-b.md, decisions 16, 23, 32): BEEP
 * THE ESC, AM32 CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG,
 * SETTINGS. A tool row has no status: the strip names the pins (decision
 * 18). RX SIGNALS opens its chooser, whose entries are `UI_RX_ENTRIES`
 * (ui_rx_signals.h), checked to fit as PROGRAMMER's are.
 *
 * A servo programmer is not a MENU row but a line of `UI_PROGRAMMERS`, the
 * entries of PROGRAMMER (ui_servo_prog.h), with a build-time check that
 * its texts fit the chooser.
 *
 * @see docs/design/menu.md, "Code"; docs/design/menu-b.md, "MENU" and
 *      "PROGRAMMER, the chooser"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

/** @brief The MENU rows. */
extern const UiMenuEntry UI_MENU[];
/** @brief How many. */
extern const uint8_t UI_MENU_COUNT;

/** @brief PROGRAMMER's entries, top to bottom. */
extern const UiProgEntry UI_PROGRAMMERS[];
/** @brief How many. */
extern const uint8_t UI_PROGRAMMERS_COUNT;
