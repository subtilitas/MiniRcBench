/**
 * @file ui_rx_signals.h
 * @brief RX SIGNALS: the hub behind MENU's RX SIGNALS row, with MONITOR
 *        and OUTPUT (docs/design/rx-signals.md, decision 4).
 *
 * RX SIGNALS is a hub (ui_hub.h): one 228 x 46 px button per tool at an
 * 80 px pitch, with its lines under it. It claims
 * nothing: DShot runs on SIG while it shows. A tap on MONITOR opens S.BUS
 * MONITOR (ui_sbus.h), a tap on OUTPUT S.BUS OUTPUT (ui_sbus_out.h), each
 * through uiChooseTool(); the tool's own entry point asks for the pin claim
 * on that frame. OUTPUT's lines carry the output's warning,
 * `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.`.
 * BACK returns to MENU. The tools' BACK before a claim held and their
 * RESTART DSHOT return here, to RX SIGNALS.
 *
 * The screen opens whatever the pin claim's state. While the claim is not
 * Free it draws its entries disabled under `DSHOT RESTARTING ON SIG GP0`
 * (uiHubEntryActs()).
 *
 * @see docs/design/rx-signals.md, "MENU"
 */

#pragma once

#include <stdint.h>

#include "ui_hub.h"
#include "ui_screen.h"

struct TouchState;

/** @brief The header's title, MENU's label: 10 characters, 118 px at scale 2. */
#define UI_RX_TITLE "RX SIGNALS"
/** @brief The strip, `C_DIM`, with SIG's and AUX's GPIO: `SIG GP0  AUX GP1`, 16 characters. */
#define UI_RX_STRIP_FMT "SIG GP%u  AUX GP%u"

/** @brief RX SIGNALS' controls besides BACK. */
enum class RxCtl : uint8_t {
	Monitor = (uint8_t)HubCtl::Entry,  /**< MONITOR: opens S.BUS MONITOR. */
	Output,       /**< OUTPUT: opens S.BUS OUTPUT. */
};

/** @brief The entries, top to bottom: MONITOR, OUTPUT. */
extern const UiHubEntry UI_RX_ENTRIES[];
/** @brief How many. */
extern const uint8_t UI_RX_ENTRIES_COUNT;
/** @brief RX SIGNALS as a hub: its title, uiRxSignalsStrip() and `UI_RX_ENTRIES`. */
extern const UiHub UI_RX_HUB;

/**
 * @brief RX SIGNALS' strip: @ref UI_RX_STRIP_FMT with DShot's and KISS
 *        telemetry's pins, which are SIG and AUX.
 * @param buf The text.
 * @param n   Room in @p buf.
 */
void uiRxSignalsStrip(char *buf, size_t n);

/** @brief Open the hub. */
void uiRxSignalsEnter();

/**
 * @brief Run one frame of the hub.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiRxSignalsTick(const TouchState *t);

/**
 * @brief The hub's controls on the current frame: BACK, MONITOR and
 *        OUTPUT, each acting.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiRxSignalsControls(UiControl *out, uint8_t max);

/** @brief RX SIGNALS, the hub. */
extern const UiScreen UI_RX_SIGNALS;
