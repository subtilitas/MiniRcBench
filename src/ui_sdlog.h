/**
 * @file ui_sdlog.h
 * @brief The SD LOG screen: the logger's state and counters, START and STOP,
 *        RETRY for a card inserted after boot, and USB DRIVE, which hands the
 *        card to a computer over USB (Universal Serial Bus).
 *
 * Reached from MENU. BACK returns to MENU, also while a host holds the
 * card: leaving the screen does not end the handover. @see sd_log.h,
 * usb_msc.h
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief The SD LOG controls. @see ui_screen.h */
enum class SdLogCtl : uint8_t {
	Toggle = 1,  /**< START or STOP; EJECT while a host holds the card. */
	Retry,       /**< RETRY, absent while a host holds the card. */
	Usb,         /**< USB DRIVE, absent while a host holds the card. */
};

/** @brief Open the screen. */
void uiSdLogEnter();

/**
 * @brief Run one frame of the screen.
 * @param t This frame's touch.
 * @return false once BACK has left.
 */
bool uiSdLogTick(const TouchState *t);

/** @brief SD LOG. */
extern const UiScreen UI_SDLOG;
