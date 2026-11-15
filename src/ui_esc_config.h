/**
 * @file ui_esc_config.h
 * @brief ESC CONFIG: the chooser behind MENU's second row, with ESC SETTINGS
 *        and USB BRIDGE (docs/design/bluejay-blheli-s.md decision 1,
 *        docs/design/usb-bridge.md decision 1).
 *
 * The chooser follows PROGRAMMER's pattern (ui_servo_prog.h), as RX
 * SIGNALS does (ui_rx_signals.h): one 228 x 46 px button per entry at an
 * 80 px pitch, with its lines under it. It claims nothing: DShot runs on
 * SIG while it shows. A tap on ESC SETTINGS opens the settings screen
 * (ui_am32.h) through uiChooseTool(), whose own entry point asks for the
 * pin claim on that frame. USB BRIDGE is designed and not built: it is
 * drawn disabled, reported disabled and opens nothing, with `NOT BUILT`
 * under it, as RX SIGNALS' OUTPUT is (the developer's call B of
 * 2026-11-15). BACK returns to MENU, and so do ESC SETTINGS' BACK and its
 * RESTART DSHOT.
 *
 * The screen opens only while the pin claim is Free (`freePins`): MENU draws
 * the row disabled under `DSHOT RESTARTING ON SIG GP0` meanwhile, as it does
 * PROGRAMMER's and RX SIGNALS'.
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief The header's title, MENU's label: 10 characters, 118 px at scale 2. */
#define UI_ESC_CFG_TITLE "ESC CONFIG"
/** @brief The strip, `C_DIM`, with SIG's GPIO: `SIG GP0`, 7 characters. */
#define UI_ESC_CFG_STRIP_FMT "SIG GP%u"
/** @brief The status of an entry that is not built, `C_DIM`, under its button: 9 characters. */
#define UI_ESC_CFG_NOT_BUILT "NOT BUILT"

/** @brief Lines under an entry. */
#define UI_ESC_CFG_LINES 2

/** @brief One entry on the chooser. */
struct UiEscCfgEntry {
	const char     *label;                    /**< Scale 2 on the button, at most 18 characters. */
	const char     *lines[UI_ESC_CFG_LINES];  /**< Scale 1 under the button, `C_DIM`; null ends them. */
	const UiScreen *screen;                   /**< The screen the tap opens; null: not built, drawn disabled. */
};

/** @brief ESC CONFIG's controls besides BACK. */
enum class EscCfgCtl : uint8_t {
	Settings = 1,  /**< ESC SETTINGS: opens the settings screen. */
	Bridge,        /**< USB BRIDGE: drawn disabled, `NOT BUILT`. */
};

/** @brief The entries, top to bottom: ESC SETTINGS, USB BRIDGE. */
extern const UiEscCfgEntry UI_ESC_CFG_ENTRIES[];
/** @brief How many. */
extern const uint8_t UI_ESC_CFG_ENTRIES_COUNT;

/** @brief Open the chooser. */
void uiEscConfigEnter();

/**
 * @brief Run one frame of the chooser.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiEscConfigTick(const TouchState *t);

/**
 * @brief The chooser's controls on the current frame: BACK, ESC SETTINGS
 *        and USB BRIDGE, the last disabled.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiEscConfigControls(UiControl *out, uint8_t max);

/** @brief ESC CONFIG, the chooser. */
extern const UiScreen UI_ESC_CONFIG;
