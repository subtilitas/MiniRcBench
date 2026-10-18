/**
 * @file ui_kst.h
 * @brief KST and Chaservo programming screen. Not built yet.
 *
 * KST and Chaservo servo programming on the ESC pin. Entry: MENU → KST SERVO.
 * The design is docs/design/kst-programming.md, and slice 3 of
 * docs/design/pin-claim.md replaces this stub whole and adds the registry
 * line. Until then MENU has no KST row, uiKstAvailable() is false, and the
 * screen is unreachable.
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Whether the screen is built.
 * @return false: MENU has no KST row. Slice 3 adds the registry line.
 */
bool uiKstAvailable();

/** @brief Open the screen. Claims nothing. */
void uiKstEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false: the screen leaves on its first frame.
 */
bool uiKstTick(const TouchState *t);

struct UiScreen;
/** @brief KST SERVO, not built: no MENU row until slice 3. */
extern const UiScreen UI_KST;
