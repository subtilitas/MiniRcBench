/**
 * @file ui_kst.h
 * @brief KST and Chaservo programming screen. Not built yet.
 *
 * KST and Chaservo servo programming on the ESC pin. Entry: CFG → SETTINGS →
 * KST. The design is docs/design/kst-programming.md, and slice 3 of
 * docs/design/pin-claim.md replaces this stub whole. Until then
 * uiKstAvailable() is false, the CFG button is drawn disabled and does
 * not act, and the screen is unreachable.
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Whether the screen is built.
 * @return false: the CFG button is drawn disabled and does not act.
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
