/**
 * @file ui_esc_cmd.h
 * @brief ESC COMMANDS: the DShot commands on one page, the first entry of
 *        the hub ESC CONFIG (ui_esc_config.h). BACK returns to the hub.
 *
 * BEEP 1 to 5 are taps, each one beacon. NORMAL and REVERSED, and 3D OFF and
 * 3D ON, are 1000 ms holds (ui_hold.h): each sends its command, 7, 8, 9 or
 * 10, then 12, the save (decisions 2 and 3 of docs/design/esc-commands.md).
 * The commands go through core1's command slot (esc_task.h): escRequestCommand()
 * hands a request over, escCommandReady() says whether core1 would start one,
 * and escCommandState() follows the sequence to its outcome.
 *
 * The page claims no pin and needs DShot running on SIG, so it opens only
 * while the pin claim is Free (`freePins`). It opens through uiOpenTool(),
 * which disarms first, and it has no arm control.
 *
 * A control is live while escCommandReady() reads EscCmdReady::Ready or
 * EscCmdReady::Busy and the page has no request of its own waiting or going
 * out. While no ESC answers, the motor turns or the ESC has answered for
 * less than 1500 ms, the controls are drawn disabled and the message lines
 * say why in amber; the commands are refused there (decision 5). While the
 * page's request waits or its sequence goes out, every control and BACK are
 * drawn disabled. At Busy otherwise, an EDT enable or a gap after the page's
 * sequence, a request is taken and waits in the slot, at most
 * @ref ESC_CMD_WAIT_MS. The enable's retry every 1000 ms with an ESC that
 * sends no EDT reads Busy for about 11 ms, and a hold that saw it as a
 * disabled frame would end. A completed 3D ON, or one
 * that stopped after the ESC may have taken it, sets the tester's 3D note
 * (esc_3d.h); 3D OFF leaves it (decision 4).
 *
 * @see docs/design/esc-commands.md, "The page" and "Screen"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;

/** @brief The header's title and the hub entry's label: 12 characters, 142 px at scale 2. */
#define UI_ESC_CMD_TITLE "ESC COMMANDS"
/**
 * @brief The strip's caption, `C_DIM`, with SIG's GPIO and the DShot rate in
 *        kbit/s: `SIG GP0  DSHOT600`; `SIG GP1  DSHOT1200` at its widest, 18
 *        characters, 107 px.
 */
#define UI_ESC_CMD_STRIP_FMT "SIG GP%u  DSHOT%u"

/**
 * @defgroup ui_esc_cmd_texts ESC COMMANDS' message lines
 * @brief Scale 1 from x 8, at most 37 characters (221 px) each.
 * @{
 */
/** @brief Idle, `C_DIM`, line 1 of 6. */
#define UI_ESC_CMD_IDLE_1 "HOLD 1 S: SETS IT, THEN SAVES IT."
/** @brief Idle, line 2. */
#define UI_ESC_CMD_IDLE_2 "THE ESC SENDS NO ANSWER TO A COMMAND:"
/** @brief Idle, line 3; after a direction sent, line 3, or its lost save, line 2. */
#define UI_ESC_CMD_CHECK  "CHECK IT ON THE TESTER WITH 3D OFF."
/** @brief Idle, line 5. */
#define UI_ESC_CMD_IDLE_5 "AM32 BEEPS AS BEEP 1 ON SAVE (NORMAL)"
/** @brief Idle, line 6. */
#define UI_ESC_CMD_IDLE_6 "OR AS BEEP 2 (REVERSED)."
/** @brief A change going out, `C_TEXT`: its command and name. */
#define UI_ESC_CMD_SENDING_FMT "SENDING %u (%s), THEN 12 (SAVE)."
/** @brief A change sent, `C_TEXT`: its command and name. */
#define UI_ESC_CMD_SENT_FMT "SENT %u (%s), THEN 12 (SAVE)."
/** @brief A beacon going out, `C_TEXT`: the command and the beacon, the same number. */
#define UI_ESC_CMD_BEEP_SENDING_FMT "SENDING %u (BEEP %u)."
/** @brief A beacon sent, `C_TEXT`. */
#define UI_ESC_CMD_BEEP_SENT_FMT "SENT %u (BEEP %u)."
/** @brief After a direction, `C_TEXT`, line 2. */
#define UI_ESC_CMD_UNCONFIRMED "NOTHING CONFIRMS THE ESC TOOK THEM."
/** @brief After 3D ON, `C_TEXT`, line 2, and after its lost save, `C_AMBER`. */
#define UI_ESC_CMD_3D_NOW "THE TESTER SHOWS THE 3D NOTE NOW."
/** @brief After 3D ON, `C_AMBER`, line 4. */
#define UI_ESC_CMD_3D_LOW "IN 3D, 0 TO 50% RUNS REVERSED AT 2X;"
/** @brief After 3D ON, `C_AMBER`, line 5. */
#define UI_ESC_CMD_3D_HIGH "ABOVE 50% THE SET WAY, FROM LOW."
/**
 * @brief After 3D ON, `C_TEXT`, line 3: what ends the note, which is not
 *        stored (decision 4): a restart, or ESC SETTINGS reading 3D MODE off
 *        in a list or verifying a write of it off. 35 characters, 209 px.
 */
#define UI_ESC_CMD_3D_ENDS "A RESTART OR 3D OFF READ CLEARS IT."
/** @brief After 3D OFF, `C_TEXT`, line 2, while the note stands (decision 4). */
#define UI_ESC_CMD_3D_STAYS "THE 3D NOTE STAYS ON THE TESTER."
/**
 * @brief After a lost save of 3D OFF, `C_AMBER`, line 2: the change may sit
 *        in the ESC's RAM only.
 */
#define UI_ESC_CMD_3D_OFF_LOST "AFTER A POWER CYCLE IT MAY RUN IN 3D."
/** @brief EscCmdReady::NoAnswer, `C_AMBER`, with SIG's GPIO (decision 5). */
#define UI_ESC_CMD_NO_ANSWER_FMT "NO ESC ANSWERS ON SIG GP%u."
/** @brief EscCmdReady::Turning, `C_AMBER`. */
#define UI_ESC_CMD_TURNING "MOTOR TURNING: WAIT UNTIL IT STOPS."
/** @brief EscCmdReady::Settling, `C_AMBER`: @ref ESC_CMD_SETTLE_MS. */
#define UI_ESC_CMD_SETTLING "ESC STARTING: WAIT 1.5 S."
/** @brief EscCmdState::NotSent, `C_AMBER`: no ESC acted. */
#define UI_ESC_CMD_NOT_SENT "NOT SENT: THE ESC STOPPED ANSWERING."
/** @brief EscCmdState::SaveLost, `C_AMBER`, line 1; a line for the change follows. */
#define UI_ESC_CMD_SAVE_LOST "SAVE NOT SENT: POWER-CYCLE THE ESC."
/** @brief EscCmdState::Expired, `C_AMBER`. */
#define UI_ESC_CMD_EXPIRED "NOT SENT: THE ESC WAS NOT READY."
/** @} */

/**
 * @defgroup ui_esc_cmd_layout ESC COMMANDS' layout
 * @brief The note's "Screen" table: x 6 to 233 (`UI_EDGE`), y from 62.
 * @{
 */
#define UI_ESC_CMD_BEEP_CAP_Y   62   /**< `BEEP`, `C_DIM`. */
#define UI_ESC_CMD_BEEP_Y       72   /**< BEEP 1 to 5, 40 x 40 px. */
#define UI_ESC_CMD_BEEP_SIZE    40   /**< Their side. */
#define UI_ESC_CMD_BEEP_PITCH   47   /**< Left edge to left edge. */
#define UI_ESC_CMD_DIR_CAP_Y   120   /**< `DIRECTION`, `C_DIM`. */
#define UI_ESC_CMD_DIR_Y       130   /**< NORMAL and REVERSED. */
#define UI_ESC_CMD_3D_CAP_Y    182   /**< `3D MODE`, `C_DIM`. */
#define UI_ESC_CMD_3D_Y        192   /**< 3D OFF and 3D ON. */
#define UI_ESC_CMD_HOLD_W      111   /**< A hold's width. */
#define UI_ESC_CMD_HOLD_H       44   /**< A hold's height; its bar follows under it. */
#define UI_ESC_CMD_HOLD_X2     123   /**< The right-hand hold's x. */
#define UI_ESC_CMD_LINE_Y      244   /**< The first message line. */
#define UI_ESC_CMD_LINE_H       12   /**< Line pitch. */
#define UI_ESC_CMD_LINES         6   /**< Message lines. */
#define UI_ESC_CMD_LINE_CHARS   37   /**< A line's most characters, 221 px. */
/** @} */

/** @brief ESC COMMANDS' controls besides BACK. */
enum class EscCmdCtl : uint8_t {
	Beep1 = 1,  /**< BEEP 1, a tap: beacon 1. */
	Beep2,      /**< BEEP 2. */
	Beep3,      /**< BEEP 3. */
	Beep4,      /**< BEEP 4. */
	Beep5,      /**< BEEP 5. */
	Normal,     /**< NORMAL, a 1000 ms hold: 7, then 12. */
	Reversed,   /**< REVERSED, a hold: 8, then 12. */
	Off3d,      /**< 3D OFF, a hold: 9, then 12. */
	On3d,       /**< 3D ON, a hold: 10, then 12. */
};

/** @brief Open the page: idle, no hold running, no outcome of its own shown. */
void uiEscCmdEnter();

/**
 * @brief Run one frame of the page.
 * @param t This frame's touch.
 * @return false once BACK has left it.
 */
bool uiEscCmdTick(const TouchState *t);

/**
 * @brief The page's controls on the current frame: BACK, BEEP 1 to 5 and the
 *        four holds, each enabled on the rule it is drawn and gated on.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiEscCmdControls(UiControl *out, uint8_t max);

/** @brief ESC COMMANDS, a constant: ESC CONFIG's table checks it at build time. */
inline constexpr const UiScreen UI_ESC_CMD = {
	UiScreenId::EscCmd, UI_ESC_CMD_TITLE, PinUser::None, nullptr,
	uiEscCmdEnter, uiEscCmdTick, nullptr, uiEscCmdControls, nullptr, true,
};
