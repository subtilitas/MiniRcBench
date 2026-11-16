/**
 * @file ui_esc_config.h
 * @brief ESC CONFIG: the hub behind MENU's second row, with ESC COMMANDS,
 *        ESC SETTINGS and USB BRIDGE (docs/design/menu-c.md, "The hub";
 *        docs/design/bluejay-blheli-s.md decision 1,
 *        docs/design/usb-bridge.md decision 1).
 *
 * ESC CONFIG is a hub (ui_hub.h), as SERVO (ui_servo_hub.h) and RX SIGNALS
 * (ui_rx_signals.h) are: one 228 x 46 px button per entry at an 80 px
 * pitch, with its lines under it. Three entries fill the page; it does not
 * scroll. It claims nothing; no DShot runs while it shows. A tap on ESC
 * COMMANDS opens the DShot commands (ui_esc_cmd.h), which claim nothing; a
 * tap on ESC SETTINGS opens the settings screen (ui_am32.h), a tap on USB
 * BRIDGE the bridge (ui_bridge.h), whose own entry points ask for the pin
 * claim on that frame. Each opens through uiChooseTool(). BACK returns to
 * MENU. The tools' BACK and their leave page's last button return here, to
 * ESC CONFIG.
 *
 * The screen opens whatever the pin claim's state. While the claim is not
 * Free it draws its entries disabled under the busy caption,
 * `SETTING SIG GP0 LOW` while the pins park (uiHubEntryActs()).
 */

#pragma once

#include <stdint.h>

#include "ui_hub.h"
#include "ui_screen.h"

struct TouchState;

/** @brief The header's title, MENU's label: 10 characters, 118 px at scale 2. */
#define UI_ESC_CFG_TITLE "ESC CONFIG"
/** @brief The strip, `C_DIM`, with SIG's GPIO: `SIG GP0`, 7 characters. */
#define UI_ESC_CFG_STRIP_FMT "SIG GP%u"
/** @brief ESC CONFIG's controls besides BACK. */
enum class EscCfgCtl : uint8_t {
	Commands = (uint8_t)HubCtl::Entry,  /**< ESC COMMANDS: opens the DShot commands. */
	Settings,      /**< ESC SETTINGS: opens the settings screen. */
	Bridge,        /**< USB BRIDGE: opens the bridge. */
};

/** @brief The entries, top to bottom: ESC COMMANDS, ESC SETTINGS, USB BRIDGE. */
extern const UiHubEntry UI_ESC_CFG_ENTRIES[];
/** @brief How many. */
extern const uint8_t UI_ESC_CFG_ENTRIES_COUNT;
/** @brief ESC CONFIG as a hub: its title, uiEscConfigStrip() and `UI_ESC_CFG_ENTRIES`. */
extern const UiHub UI_ESC_CFG_HUB;

/**
 * @brief ESC CONFIG's strip: @ref UI_ESC_CFG_STRIP_FMT with DShot's pin, which is SIG.
 * @param buf The text.
 * @param n   Room in @p buf.
 */
void uiEscConfigStrip(char *buf, size_t n);

/** @brief Open the hub. */
void uiEscConfigEnter();

/**
 * @brief Run one frame of the hub.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiEscConfigTick(const TouchState *t);

/**
 * @brief The hub's controls on the current frame: BACK, ESC COMMANDS, ESC
 *        SETTINGS and USB BRIDGE, each enabled as uiHubEntryActs() says.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiEscConfigControls(UiControl *out, uint8_t max);

/** @brief ESC CONFIG, the hub. */
extern const UiScreen UI_ESC_CONFIG;
