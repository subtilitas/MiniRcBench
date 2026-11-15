/**
 * @file ui_rx_signals.h
 * @brief RX SIGNALS: the chooser behind MENU's RX SIGNALS row, with MONITOR
 *        and OUTPUT (docs/design/rx-signals.md, decision 4).
 *
 * The chooser follows PROGRAMMER's pattern (ui_servo_prog.h): one 228 x 46 px
 * button per tool at an 80 px pitch, with its lines under it. It claims
 * nothing: DShot runs on SIG while it shows. A tap on MONITOR opens S.BUS
 * MONITOR (ui_sbus.h) through uiChooseTool(); the monitor's own entry point
 * asks for the pin claim on that frame. OUTPUT, the S.BUS output, is not
 * designed: it is drawn disabled, reported disabled and opens nothing, with
 * `NOT BUILT` under it, as MENU drew the row itself before (menu-b.md,
 * decision 35). BACK returns to MENU, and so do the monitor's BACK before a
 * claim held and its RESTART DSHOT.
 *
 * The screen opens only while the pin claim is Free (`freePins`): MENU draws
 * the row disabled under `DSHOT RESTARTING ON SIG GP0` meanwhile, as it does
 * PROGRAMMER's.
 *
 * @see docs/design/rx-signals.md, "MENU"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief The header's title, MENU's label: 10 characters, 118 px at scale 2. */
#define UI_RX_TITLE "RX SIGNALS"
/** @brief The strip, `C_DIM`, with SIG's and AUX's GPIO: `SIG GP0  AUX GP1`, 16 characters. */
#define UI_RX_STRIP_FMT "SIG GP%u  AUX GP%u"
/** @brief The status of a tool that is not built, `C_DIM`, under its button: 9 characters. */
#define UI_RX_NOT_BUILT "NOT BUILT"

/**
 * @defgroup ui_rx_layout RX SIGNALS' layout
 * @brief PROGRAMMER's: @ref UI_PROG_ENTRY_Y, @ref UI_PROG_ENTRY_PITCH,
 *        @ref UI_PROG_ENTRY_W, @ref UI_PROG_ENTRY_H, its lines from
 *        @ref UI_PROG_LINE_DY at @ref UI_PROG_LINE_H.
 * @{
 */
/** @brief Lines under an entry. */
#define UI_RX_LINES 2
/** @} */

/** @brief One tool on the chooser. */
struct UiRxEntry {
	const char     *label;               /**< Scale 2 on the button, at most 18 characters. */
	const char     *lines[UI_RX_LINES];  /**< Scale 1 under the button, `C_DIM`; null ends them. */
	const UiScreen *screen;              /**< The tool the tap opens; null: not built, drawn disabled. */
};

/** @brief RX SIGNALS' controls besides BACK. */
enum class RxCtl : uint8_t {
	Monitor = 1,  /**< MONITOR: opens S.BUS MONITOR. */
	Output,       /**< OUTPUT: drawn disabled, `NOT BUILT`. */
};

/** @brief The entries, top to bottom: MONITOR, OUTPUT. */
extern const UiRxEntry UI_RX_ENTRIES[];
/** @brief How many. */
extern const uint8_t UI_RX_ENTRIES_COUNT;

/** @brief Open the chooser. */
void uiRxSignalsEnter();

/**
 * @brief Run one frame of the chooser.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiRxSignalsTick(const TouchState *t);

/**
 * @brief The chooser's controls on the current frame: BACK, MONITOR and
 *        OUTPUT, the last disabled.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiRxSignalsControls(UiControl *out, uint8_t max);

/** @brief RX SIGNALS, the chooser. */
extern const UiScreen UI_RX_SIGNALS;
