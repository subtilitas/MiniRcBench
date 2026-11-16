/**
 * @file ui_menu.h
 * @brief MENU: one list of tools and actions, from the registry.
 *
 * The root and the first screen after the splash (docs/design/menu-c.md
 * owner decision 1, slice 3). Its header draws no BACK: nothing lies behind
 * the root. Its first row, ESC TESTER, opens the tester, whose MENU button
 * disarms and returns here. Each row is a registry entry (ui_registry.h): a
 * tool opens its screen, and an action row, a screen with `action` set,
 * would act on the row; `UI_MENU` holds none. A row fires on release, a
 * press that slides off cancels, and a vertical drag scrolls once the rows
 * do not fit: nine rows at a 42 px pitch scroll 120 px.
 *
 * A tool that claims the signal pins, or one that opens only while they are
 * free (the tester, SETTINGS and ESC COMMANDS), is refused while the pin
 * claim is not Free: its row is drawn disabled under its own status, a tap
 * opens nothing, and the strip reads `DSHOT RESTARTING ON SIG GP0`. That
 * state is reachable for the time Restoring takes after RESTART DSHOT. The choosers ESC CONFIG,
 * PROGRAMMER, RX SIGNALS and TOOLS are never refused: each gates its own
 * entries (ui_hub.h).
 *
 * @see docs/design/menu.md, "MENU"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief A MENU row's label, scale 2, starts at this x. */
#define UI_MENU_LABEL_X  16
/** @brief A MENU row's status, scale 1, ends at this x. */
#define UI_MENU_STATUS_R 222
/** @brief Least space between a MENU label and its widest status, px. */
#define UI_MENU_LABEL_GAP 8

/**
 * @brief The strip's caption, with SIG's GPIO, AUX's GPIO and the DShot rate
 *        in kbit/s for the three `%u`: `SIG GP0  AUX GP1  DSHOT600`, 26
 *        characters; `SIG GP1  AUX GP0  DSHOT1200` at its widest, 27, 161 px.
 */
#define UI_MENU_CAPTION_FMT "SIG GP%u  AUX GP%u  DSHOT%u"
/**
 * @brief The strip's caption while the pin claim is not Free, with SIG's
 *        GPIO for `%u`: 27 characters, 161 px. The rows that claim or change
 *        the wiring are drawn disabled meanwhile.
 */
#define UI_MENU_RESTARTING_FMT "DSHOT RESTARTING ON SIG GP%u"

/**
 * @defgroup ui_menu_chip The strip's EDT chip
 * @brief Its three texts: green while EDT frames arrive, red while eRPM
 *        arrives without them, dim while no eRPM arrives at all.
 * @{
 */
#define UI_MENU_EDT_ON  "EDT ON"   /**< On `C_GREEN`. */
#define UI_MENU_EDT_OFF "EDT OFF"  /**< On `C_RED`: the ESC answers, EDT does not. */
#define UI_MENU_NO_ESC  "NO ESC"   /**< The neutral chip: no eRPM within 500 ms. */
/** @} */

/** @brief What a MENU frame asks of the navigation. */
enum class UiMenuResult : uint8_t {
	Stay,  /**< MENU stays open. */
	Open,  /**< Open the tool the tick names. */
};

/** @brief Open MENU, scrolled to 0 px. */
void uiMenuEnter();

/**
 * @brief Run one MENU frame.
 * @param t         This frame's touch.
 * @param[out] open The tool to open, for @ref UiMenuResult::Open.
 * @return What the navigation does next.
 */
UiMenuResult uiMenuTick(const TouchState *t, const UiScreen **open);

/**
 * @brief The MENU row of @p s in the registry in use.
 * @param s A screen.
 * @return Its row, or -1 when it has none.
 */
int uiMenuRowOf(const UiScreen *s);

/**
 * @brief The registry MENU shows: @ref UI_MENU, or the one a test set.
 * @param[out] n Its entries.
 * @return The first entry.
 */
const UiMenuEntry *uiMenuEntries(uint8_t *n);

/**
 * @brief Show another registry, for a host test of MENU's scroll; null
 *        restores @ref UI_MENU. Takes effect at the next uiMenuEnter().
 * @param e Entries.
 * @param n How many.
 */
void uiMenuUseRegistry(const UiMenuEntry *e, uint8_t n);

/** @brief MENU itself. */
extern const UiScreen UI_MENU_SCREEN;
