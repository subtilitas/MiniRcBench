/**
 * @file ui_servo_prog.h
 * @brief PROGRAMMER: the chooser behind MENU's SERVO PROGRAMMER row.
 *
 * The chooser lists the servo programmers of `UI_PROGRAMMERS`
 * (ui_registry.h), one 228 x 46 px button each at an 80 px pitch, with the
 * models, the power line or, while an entry is disabled, the amber lines
 * that say why under it. It claims nothing: DShot runs on SIG while it
 * shows. A tap on an entry opens the entry's tool through uiChooseTool(),
 * whose own entry point runs on that frame and claims SIG there. BACK
 * returns to MENU, and so do the tools' BACK before a claim held and their
 * RESTART DSHOT.
 *
 * The screen opens only while the pin claim is Free (`freePins`).
 *
 * @see docs/design/menu-b.md, "PROGRAMMER, the chooser"
 */

#pragma once

#include <stdint.h>

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

/**
 * @defgroup ui_prog_layout PROGRAMMER's layout
 * @{
 */
#define UI_PROG_ENTRY_Y     62   /**< The first entry's top, @ref UI_BODY_Y. */
#define UI_PROG_ENTRY_PITCH 80   /**< Entry top to entry top. */
#define UI_PROG_ENTRY_W     228  /**< From x 6, @ref UI_EDGE, to x 233. */
#define UI_PROG_ENTRY_H     46
#define UI_PROG_LINE_DY     50   /**< An entry's first line, from its top: 4 px under it. */
#define UI_PROG_LINE_H      12   /**< Line to line, scale 1. */
#define UI_PROG_LINE_CHARS  37   /**< A line from x 8 at scale 1: 221 px, to x 228. */
#define UI_PROG_LABEL_CHARS 18   /**< A label at scale 2: 214 px in the 228 px button. */
/** @} */

/** @brief PROGRAMMER's controls besides BACK: entry i is `PROG_ENTRY(i)`. */
enum class ServoProgCtl : uint8_t {
	Entry = 1,  /**< The first entry; entry i is Entry + i. */
};

/** @brief The control of entry @p i of `UI_PROGRAMMERS`. */
#define PROG_ENTRY(i) UI_FIX((uint8_t)ServoProgCtl::Entry + (i))

/** @brief Open the chooser. */
void uiServoProgEnter();

/**
 * @brief Run one frame of the chooser.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiServoProgTick(const TouchState *t);

/**
 * @brief The chooser's controls on the current frame: BACK and the entries,
 *        each enabled as its `enabled` says.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiServoProgControls(UiControl *out, uint8_t max);

/** @brief PROGRAMMER. */
extern const UiScreen UI_SERVO_PROG;
