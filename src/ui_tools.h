/**
 * @file ui_tools.h
 * @brief TOOLS: the chooser behind MENU's sixth row, with ANGLE METER
 *        (docs/design/angle-meter.md, decision 1; docs/design/menu-b.md,
 *        "MENU").
 *
 * The chooser follows PROGRAMMER's pattern (ui_servo_prog.h), as ESC CONFIG
 * (ui_esc_config.h) and RX SIGNALS (ui_rx_signals.h) do: one 228 x 46 px
 * button per entry at an 80 px pitch, with its lines under it. It claims
 * nothing: DShot runs on SIG while it shows, and the strip says so. A tap
 * on ANGLE METER opens the meter (ui_angle.h) through uiChooseTool(). BACK
 * returns to MENU, and so does the meter's BACK.
 *
 * The screen opens only while the pin claim is Free (`freePins`), as the
 * other choosers: MENU draws the row disabled under
 * `DSHOT RESTARTING ON SIG GP0` meanwhile.
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief The header's title, MENU's label: 5 characters, 58 px at scale 2. */
#define UI_TOOLS_TITLE "TOOLS"
/**
 * @brief The strip, `C_DIM`: the tools claim no pin, and DShot keeps SIG.
 *        `DSHOT RUNS ON SIG GP0`, 21 characters, 125 px.
 */
#define UI_TOOLS_STRIP_FMT "DSHOT RUNS ON SIG GP%u"

/** @brief Lines under an entry. */
#define UI_TOOLS_LINES 2

/** @brief One entry on the chooser. */
struct UiToolsEntry {
	const char     *label;                  /**< Scale 2 on the button, at most 18 characters. */
	const char     *lines[UI_TOOLS_LINES];  /**< Scale 1 under the button, `C_DIM`; null ends them. */
	const UiScreen *screen;                 /**< The screen the tap opens. */
};

/** @brief TOOLS' controls besides BACK. */
enum class ToolsCtl : uint8_t {
	Angle = 1,  /**< ANGLE METER: opens the meter. */
};

/** @brief The entries, top to bottom: ANGLE METER. */
extern const UiToolsEntry UI_TOOLS_ENTRIES[];
/** @brief How many. */
extern const uint8_t UI_TOOLS_ENTRIES_COUNT;

/** @brief Open the chooser. */
void uiToolsEnter();

/**
 * @brief Run one frame of the chooser.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiToolsTick(const TouchState *t);

/**
 * @brief The chooser's controls on the current frame: BACK and one per entry.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiToolsControls(UiControl *out, uint8_t max);

/** @brief TOOLS, the chooser. */
extern const UiScreen UI_TOOLS;
