/**
 * @file ui_servo_prog.h
 * @brief PROGRAMMER: the hub behind MENU's SERVO PROGRAMMER row.
 *
 * PROGRAMMER is a hub (ui_hub.h) of the servo programmers of
 * `UI_PROGRAMMERS` (ui_registry.h), one 228 x 46 px button each at an 80 px
 * pitch, with the models, the power line or, while an entry is disabled, the
 * amber lines that say why under it. It claims nothing: DShot runs on SIG
 * while it shows. A tap on an entry opens the entry's tool through
 * uiChooseTool(), whose own entry point runs on that frame and claims SIG
 * there. BACK returns to MENU. The tools' BACK before a claim held and their
 * RESTART DSHOT return here, to PROGRAMMER.
 *
 * The screen opens whatever the pin claim's state. While the claim is not
 * Free it draws its entries disabled under `DSHOT RESTARTING ON SIG GP0`
 * (uiHubEntryActs()).
 *
 * @see docs/design/menu-b.md, "PROGRAMMER, the chooser"
 */

#pragma once

#include <stdint.h>

#include "ui_hub.h"
#include "ui_screen.h"

struct TouchState;

/** @brief The header's title: `SERVO PROGRAMMER`, 190 px, would reach past BACK. */
#define UI_PROG_TITLE "PROGRAMMER"

/**
 * @brief The strip, `C_DIM`, with SIG's GPIO for the first `%u` and the
 *        supply's value for `%s`: `SIG GP0  SERVO SUPPLY OWN`, or `GP28`,
 *        26 characters at most.
 */
#define UI_PROG_STRIP_FMT "SIG GP%u  SERVO SUPPLY %s"

/**
 * @brief An entry's power line with SERVO SUPPLY at OWN: the tap takes SIG
 *        before the servo is powered (docs/design/menu-b.md, decision 34,
 *        for HITEC D SERIES), 33 characters.
 */
#define UI_PROG_POWER_AFTER "POWER THE SERVO UP AFTER THE TAP."
/** @brief An entry's power line with the switched supply, `%u` its enable GPIO: 30 characters. */
#define UI_PROG_SWITCHED_FMT "THE BENCH SWITCHES IT ON GP%u."

/** @brief PROGRAMMER's controls besides BACK: entry i is `PROG_ENTRY(i)`. */
enum class ServoProgCtl : uint8_t {
	Entry = (uint8_t)HubCtl::Entry,  /**< The first entry; entry i is Entry + i. */
};

/** @brief The control of entry @p i of `UI_PROGRAMMERS`. */
#define PROG_ENTRY(i) HUB_ENTRY(i)

/** @brief PROGRAMMER's entries, top to bottom. */
extern const UiHubEntry UI_PROGRAMMERS[];
/** @brief How many. */
extern const uint8_t UI_PROGRAMMERS_COUNT;
/** @brief PROGRAMMER as a hub: its title, uiServoProgStrip() and `UI_PROGRAMMERS`. */
extern const UiHub UI_PROG_HUB;

/**
 * @brief PROGRAMMER's strip: @ref UI_PROG_STRIP_FMT with DShot's pin, which
 *        is SIG while the claim is Free, and `OWN` or the supply's `GP28`.
 * @param buf The text.
 * @param n   Room in @p buf.
 */
void uiServoProgStrip(char *buf, size_t n);

/** @brief Open PROGRAMMER. */
void uiServoProgEnter();

/**
 * @brief Run one frame of PROGRAMMER.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiServoProgTick(const TouchState *t);

/**
 * @brief PROGRAMMER's controls on the current frame: BACK and the entries,
 *        each enabled as its `enabled` says.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiServoProgControls(UiControl *out, uint8_t max);

/** @brief PROGRAMMER. */
extern const UiScreen UI_SERVO_PROG;
