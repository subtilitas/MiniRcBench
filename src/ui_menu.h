/**
 * @file ui_menu.h
 * @brief MENU: one list of six rows, from the registry.
 *
 * The root and the first screen after the splash (docs/design/menu-c.md
 * owner decisions 1 and 4, slices 3 and 4). Its header draws no BACK:
 * nothing lies behind the root. Its six rows are ESC TESTER, ESC PROGRAM,
 * SERVO, RECEIVER, LOGS and SETUP, at a 42 px pitch from y 62: they
 * take 252 px of the 258 px under the strip, so MENU does not scroll and
 * every row is whole. ESC TESTER opens the tester, whose header's BACK
 * disarms and returns here. ESC PROGRAM, SERVO and RECEIVER each open a
 * hub (ui_hub.h) and carry the mark `>` at the row's right, where LOGS
 * and SETUP carry a status.
 *
 * Each row is a registry entry (ui_registry.h): a screen opens, and an
 * action row, a screen with `action` set, would act on the row; `UI_MENU`
 * holds none. A row fires on release and a press that slides off cancels.
 * A registry with more rows than fit scrolls by a vertical drag; `UI_MENU`
 * does not, and a host test sets such a registry (uiMenuUseRegistry()).
 *
 * A screen that opens only while the signal pins are free (the tester and
 * SETUP) is refused while the pin claim is not Free: its row is drawn
 * disabled under its own status, a tap opens nothing, and the strip reads
 * the busy caption (uiBusyCaption()): `RELEASING SIG GP0` while the USB
 * bridge lets the pin go, `SETTING SIG GP0 LOW` while the claim parks the
 * pins after the leave page. The hubs' rows are never refused: each hub
 * gates its own entries (ui_hub.h).
 *
 * MENU draws no EDT chip: DShot is parked on MENU
 * (docs/design/dshot-lifetime.md, slice D2), and ESC COMMANDS keeps the
 * chip.
 *
 * @see docs/design/menu-c.md, "MENU"; docs/design/menu.md, "MENU"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief A MENU row's label, scale 2, starts at this x. */
#define UI_MENU_LABEL_X  16
/** @brief A MENU row's status, scale 1, ends at this x. */
#define UI_MENU_STATUS_R 222
/** @brief Least space between a MENU label and its widest status or its mark, px. */
#define UI_MENU_LABEL_GAP 8
/**
 * @brief The mark of a row that opens a hub, at scale 2 in `C_CYAN`, or
 *        `C_GRID` on a row drawn disabled (docs/design/ui-theme.md, "The
 *        widgets of menu-c.md"): 10 px wide.
 */
#define UI_MENU_MARK   ">"
/** @brief The mark starts at this x; its last column is x 222. */
#define UI_MENU_MARK_X 213

/**
 * @brief The strip's caption, with SIG's GPIO, AUX's GPIO and the DShot rate
 *        in kbit/s for the three `%u`: `SIG GP0  AUX GP1  DSHOT600`, 26
 *        characters; `SIG GP1  AUX GP0  DSHOT1200` at its widest, 27, 161 px.
 */
#define UI_MENU_CAPTION_FMT "SIG GP%u  AUX GP%u  DSHOT%u"
/**
 * @defgroup ui_busy The busy captions
 * @brief MENU's and the hubs' caption while the pin claim is not Free, with
 *        SIG's GPIO for `%u`, in @ref UI_COL_WARN. The rows and entries that
 *        claim or change the wiring are drawn disabled meanwhile
 *        (docs/design/dshot-lifetime.md, "States").
 * @{
 */
/** @brief While the claim is Leaving, or any state but Parking: 17 characters, 101 px. */
#define UI_BUSY_RELEASING_FMT "RELEASING SIG GP%u"
/** @brief While the claim is Parking: core1 drives SIG low. 19 characters, 113 px. */
#define UI_BUSY_PARKING_FMT   "SETTING SIG GP%u LOW"
/** @} */

/**
 * @brief The busy caption for the claim's state: @ref UI_BUSY_PARKING_FMT
 *        while Parking, else @ref UI_BUSY_RELEASING_FMT, with SIG's GPIO.
 * @param[out] buf Room for the caption.
 * @param n        Size of @p buf, 24 bytes or more.
 */
void uiBusyCaption(char *buf, size_t n);

/**
 * @defgroup ui_menu_chip The EDT chip's texts
 * @brief ESC COMMANDS' chip (ui_esc_cmd.h); MENU draws none since slice D2
 *        of docs/design/dshot-lifetime.md. Green while EDT frames arrive,
 *        red while eRPM arrives without them, dim while no eRPM arrives at
 *        all.
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
