/**
 * @file ui_menu.h
 * @brief MENU: one list of tools and actions, from the registry.
 *
 * Reached from the tester's MENU button, which disarms. Each row is a
 * registry entry (ui_registry.h): a tool opens its screen, an action row,
 * BEEP THE ESC, acts on the row. A row fires on release, a press that
 * slides off cancels, and a vertical drag scrolls once the rows do not fit.
 * BACK returns to the tester.
 *
 * A tool that claims the signal pins is refused while the pin claim is not
 * Free: its row reads `PINS BUSY` and a tap opens nothing. That state is
 * reachable for the time Restoring takes after RESTART DSHOT.
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

/** @brief What a MENU frame asks of the navigation. */
enum class UiMenuResult : uint8_t {
	Stay,  /**< MENU stays open. */
	Back,  /**< BACK: return to the tester. */
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
/** @brief BEEP THE ESC: the DShot beacon, an action row on MENU. */
extern const UiScreen UI_BEEP;
