/**
 * @file ui_rx_signals.h
 * @brief RX SIGNALS: the chooser behind MENU's RX SIGNALS row, with MONITOR
 *        and OUTPUT (docs/design/rx-signals.md, decision 4).
 *
 * The chooser follows PROGRAMMER's pattern (ui_servo_prog.h): one 228 x 46 px
 * button per tool at an 80 px pitch, with its lines under it. It claims
 * nothing: DShot runs on SIG while it shows. A tap on MONITOR opens S.BUS
 * MONITOR (ui_sbus.h), a tap on OUTPUT S.BUS OUTPUT (ui_sbus_out.h), each
 * through uiChooseTool(); the tool's own entry point asks for the pin claim
 * on that frame. OUTPUT's lines carry the output's warning,
 * `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.`. An entry with
 * no screen would be drawn disabled, reported disabled and open nothing.
 * BACK returns to MENU, and so do the tools' BACK before a claim held and
 * their RESTART DSHOT.
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

/**
 * @defgroup ui_rx_layout RX SIGNALS' layout
 * @brief PROGRAMMER's: @ref UI_PROG_ENTRY_Y, @ref UI_PROG_ENTRY_PITCH,
 *        @ref UI_PROG_ENTRY_W, `UI_PROG_ENTRY_H`, its lines from
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
	Output,       /**< OUTPUT: opens S.BUS OUTPUT. */
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
 *        OUTPUT, each acting.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiRxSignalsControls(UiControl *out, uint8_t max);

/** @brief RX SIGNALS, the chooser. */
extern const UiScreen UI_RX_SIGNALS;
