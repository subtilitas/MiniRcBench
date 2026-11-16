/**
 * @file ui_servo_hub.h
 * @brief SERVO: the hub behind MENU's SERVO row.
 *
 * SERVO is a hub (ui_hub.h) of the five servo tools of `UI_SERVO_ENTRIES`
 * (ui_registry.h), in the order of use: SERVO TESTER drives, ANGLE METER
 * measures, and KST / CHASERVO, HITEC D SERIES and HITEC CLASSIC program.
 * Each is one 228 x 46 px button at an 80 px pitch with its lines under it:
 * what the tool does or the models, the power line or, while an entry is
 * disabled, the amber lines that say why.
 *
 * Five entries are 404 px of content in the 258 px under the strip, so
 * SERVO scrolls 146 px. At scroll 0 the first three entries are whole and
 * HITEC D SERIES shows 18 px, which is listed and takes no press; it is
 * whole from 28 px of scroll and HITEC CLASSIC from 108 px.
 *
 * SERVO claims nothing: DShot runs on SIG while it shows. A tap on an entry
 * opens the entry's tool through uiChooseTool(), whose own entry point runs
 * on that frame and claims its pins there. BACK returns to MENU. The tools'
 * BACK before a claim held and their RESTART DSHOT return here, to SERVO at
 * the scroll it was left at.
 *
 * The screen opens whatever the pin claim's state. While the claim is not
 * Free it draws its entries disabled under `DSHOT RESTARTING ON SIG GP0`
 * (uiHubEntryActs()).
 *
 * @see docs/design/menu-c.md, "The hub"
 */

#pragma once

#include <stdint.h>

#include "ui_hub.h"
#include "ui_screen.h"

struct TouchState;

/** @brief The header's title, MENU's label: 5 characters, 58 px at scale 2. */
#define UI_SERVO_HUB_TITLE "SERVO"

/**
 * @brief The strip, `C_DIM`, with SIG's GPIO and AUX's for the two `%u` and
 *        the supply's value for `%s`: `SIG GP0  AUX GP1  SERVO SUPPLY OWN`,
 *        or `GP28`, 35 characters at most, 209 px.
 */
#define UI_SERVO_HUB_STRIP_FMT "SIG GP%u  AUX GP%u  SERVO SUPPLY %s"

/**
 * @brief A programmer entry's power line with SERVO SUPPLY at OWN: the tap
 *        takes SIG before the servo is powered (docs/design/menu-b.md,
 *        decision 34, for HITEC D SERIES), 33 characters.
 */
#define UI_PROG_POWER_AFTER "POWER THE SERVO UP AFTER THE TAP."

/** @brief A programmer entry's power line with the switched supply, `%u` its enable GPIO: 30 characters. */
#define UI_PROG_SWITCHED_FMT "THE BENCH SWITCHES IT ON GP%u."

/** @brief SERVO's entries, top to bottom. */
extern const UiHubEntry UI_SERVO_ENTRIES[];
/** @brief How many. */
extern const uint8_t UI_SERVO_ENTRIES_COUNT;
/** @brief SERVO as a hub: its title, uiServoHubStrip() and `UI_SERVO_ENTRIES`. */
extern const UiHub UI_SERVO_HUB;

/**
 * @brief SERVO's strip: @ref UI_SERVO_HUB_STRIP_FMT with DShot's pin, which
 *        is SIG while the claim is Free, the KISS pin, which is AUX, and
 *        `OWN` or the supply's `GP28`.
 * @param buf The text.
 * @param n   Room in @p buf.
 */
void uiServoHubStrip(char *buf, size_t n);

/** @brief Open SERVO, scrolled to 0 px. */
void uiServoHubEnter();

/**
 * @brief Run one frame of SERVO.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiServoHubTick(const TouchState *t);

/**
 * @brief SERVO's controls on the current frame: BACK and the entries that
 *        show, each enabled as uiHubEntryActs() says.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiServoHubControls(UiControl *out, uint8_t max);

/** @brief SERVO, the hub. */
extern const UiScreen UI_SERVO_HUB_SCREEN;
