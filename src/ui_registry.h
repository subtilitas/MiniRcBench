/**
 * @file ui_registry.h
 * @brief The MENU rows, in MENU order, and the three hubs' entries.
 *
 * A new device class is a MENU row; a new brand, protocol or tool is a
 * `UiScreen` in a file of its own and one line in its hub's table in
 * `ui_registry.cpp` (docs/design/menu-c.md, "The proposed structure"). A
 * MENU row holds its label and its widest status, and a build-time check
 * keeps every label clear of its status, or of the mark `>` of a row that
 * opens a hub, on the 240 px panel. A tool's persisted settings rows,
 * `UiScreen::settings`, join SETTINGS between SERVO SUPPLY and DISPLAY in
 * the order of the rows, a hub's entries in the hub's place.
 *
 * MENU has six rows and does not scroll (docs/design/menu-c.md owner
 * decisions 1 and 4, slices 3 and 4): ESC TESTER, the tester; ESC CONFIG,
 * SERVO and RX SIGNALS, which open their hubs and carry `>`; SD LOG and
 * SETTINGS. The hubs' entries are `UI_ESC_CFG_ENTRIES` (ui_esc_config.h):
 * ESC COMMANDS, ESC SETTINGS, USB BRIDGE; `UI_SERVO_ENTRIES`
 * (ui_servo_hub.h): SERVO TESTER, ANGLE METER, KST / CHASERVO, HITEC D
 * SERIES, HITEC CLASSIC; and `UI_RX_ENTRIES` (ui_rx_signals.h): MONITOR,
 * OUTPUT.
 *
 * The hubs are three tables of `UiHubEntry` on one widget (ui_hub.h), each
 * checked at build time by hubEntriesFit(), the one check that an entry's
 * texts fit the hub.
 *
 * @see docs/design/menu-c.md, "MENU" and "The hub"; docs/design/menu.md,
 *      "Code"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

/** @brief The MENU rows. */
extern const UiMenuEntry UI_MENU[];
/** @brief How many. */
extern const uint8_t UI_MENU_COUNT;

struct UiHub;

/**
 * @brief The hub that @p s shows: MENU draws `>` on the row of such a
 *        screen, and SETTINGS walks its entries for a tool's section.
 * @param s A screen.
 * @return Its hub; null for a screen that is not a hub.
 */
const UiHub *uiHubOf(const UiScreen *s);
