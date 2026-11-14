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

struct SdLogStatus;

/**
 * @brief SD LOG's note for how the last log ended, two lines, or false.
 *
 * Shown above START from the end (@ref SdLogEnd): a write that found the
 * card full, a write, sync or close that failed, a file that did not open,
 * a start that found `LOG99999.BFL`, until the next start or mount; a log
 * a tool stopped when it took the card, until the next start. A refusal of
 * USB DRIVE outranks it for its 4000 ms.
 * A tool's stop has one line: a mount hint takes the second.
 *
 * @param st The logger's status.
 * @param[out] line1 What happened.
 * @param[out] line2 The next step, or an empty string.
 * @return False when there is nothing to say.
 */
bool uiSdLogEndNote(const SdLogStatus *st, const char **line1, const char **line2);

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
