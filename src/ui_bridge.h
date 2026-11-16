/**
 * @file ui_bridge.h
 * @brief USB BRIDGE: a desktop ESC tool on the board's USB serial port
 *        reaches the ESC's bootloader on SIG (docs/design/usb-bridge.md,
 *        "Screen", slices 2 and 3).
 *
 * Entry: MENU → ESC CONFIG → USB BRIDGE (ui_esc_config.h, decision 1). The
 * screen claims SIG for PinUser::Bridge on its first frame. The claim has no
 * ESC check: the bridge needs the ESC on the pin. While USB DRIVE holds the
 * card the claim is not begun and the screen reads
 * `EJECT THE USB DRIVE FIRST.` (decision 11); it asks again every frame, as
 * it does after a refusal of the claim, which it shows as ESC SETTINGS does.
 *
 * Once the claim is Running, usb_bridge.h copies bytes between the port and
 * the wire; this screen draws what it reports and never touches the port.
 * Counters repaint at most every @ref BRIDGE_UI_COUNT_MS. The mode reads
 * `WAITING`, `PASSTHROUGH` or, in MSP and the 4-way interface,
 * `4-WAY INTERFACE`. The last line names the PC's last frame: a bootloader
 * command in passthrough, `LAST MSP SET PASSTHROUGH` for MSP,
 * `LAST READ 7C00` for a 4-way command with its address. In the 4-way
 * interface the counters count the bytes sent on SIG and the bytes of the
 * answers written to the PC, and FRAMES the bootloader frames sent.
 *
 * **BACK.**
 *
 * | When | BACK |
 * |---|---|
 * | no claim: refused, USB DRIVE | to ESC CONFIG |
 * | Stopping, or Held after a failed bind | pinClaimEnd(): the pins park at once; to ESC CONFIG, and MENU's run cue stands from the claim's begin |
 * | a write-class frame waits for its answer, at most 3000 ms | drawn disabled (decision 7) |
 * | Held, the line binding | pinClaimEnd(), to ESC CONFIG: the pins park once the job is idle, by the claim's rule, whether or not a screen polls it |
 * | Running | pinClaimWantLeavePage(), then pinClaimEnd(): the leave page, as the servo tools give it (decision 10 as revised): the ESC may wait in its bootloader whatever the tool sent |
 *
 * Nothing is sent at BACK, no RUN among it (decision 6). The leave page is
 * ui_leave.h's, opened releasing, as the job lets go of the line at once:
 * `POWER THE ESC OFF.`, `BACK TO ESC CONFIG.` and, after a write in this claim
 * with no RUN behind it, the note `FLASH THE ESC AGAIN BEFORE USE.`
 * (usbBridgeWriteUnfinished()). Its left button, `BRIDGE`, keeps the claim
 * and returns to the bridge, its counts at 0 (pinClaimKeep()); the
 * unfinished write stays, and the next BACK's page carries the note until a
 * RUN. The last button, live 1000 ms after the line is let go, clears MENU's
 * run cue (usbBridgeRunCueClear()) and the unfinished write
 * (usbBridgeWriteUnfinishedClear()), parks the pins and returns to ESC
 * CONFIG, the screen that opened the bridge: the
 * next entry starts with no note.
 * BACK on the page is drawn disabled. A claim that has had the page keeps it
 * until it is Free.
 *
 * Nothing on the screen but BACK, and RETRY after a failed bind, takes a tap.
 *
 * @see docs/design/usb-bridge.md, "Screen", "Safety"
 */

#pragma once

#include <stdint.h>

#include "ui_screen.h"

struct TouchState;
struct UiControl;
struct UiScreen;

/** @brief The header's title: 10 characters, 118 px at scale 2, x 8 to 125. */
#define BRIDGE_UI_TITLE "USB BRIDGE"

/**
 * @defgroup ui_bridge_layout USB BRIDGE's layout
 * @{
 */
#define BRIDGE_UI_MODE_CAP_Y   62   /**< `MODE`, C_DIM, @ref UI_BODY_Y. */
#define BRIDGE_UI_MODE_Y       72   /**< The mode at scale 2. */
#define BRIDGE_UI_COUNT_CAP_Y  98   /**< `PC > ESC` and `ESC > PC`, C_DIM. */
#define BRIDGE_UI_COUNT_Y     108   /**< The two counters at scale 2. */
#define BRIDGE_UI_RIGHT_X     122   /**< The right-hand counter and its caption. */
#define BRIDGE_UI_FRAMES_Y    134   /**< `FRAMES 412  LOST 0`. */
#define BRIDGE_UI_LAST_Y      148   /**< `LAST SET ADDRESS 7E00`, C_DIM. */
#define BRIDGE_UI_MSG_Y       168   /**< Message line 0. */
#define BRIDGE_UI_MSG_H        12   /**< Message line to line. */
#define BRIDGE_UI_MSG_LINES     6   /**< Message lines, y 168 to y 235. */
#define BRIDGE_UI_COUNT_MS    250   /**< The counters repaint at most this often, ms. */
#define BRIDGE_UI_COUNT_MAX 9999999u /**< A counter shows at most this, `9999999 B`. */
/** @} */

/**
 * @defgroup ui_bridge_text USB BRIDGE's texts
 * @{
 */
/** @brief The strip's caption, SIG's GPIO for `%u`: `SIG GP0  19200 8N1`, 18 characters, 107 px. */
#define BRIDGE_UI_CAPTION_FMT   "SIG GP%u  19200 8N1"
/** @brief The strip while the claim stops DShot: `RELEASING SIG GP0`, 17 characters, 101 px. */
#define BRIDGE_UI_RELEASING_FMT "RELEASING SIG GP%u"
/** @brief The chip while DTR is set, on @ref UI_COL_LIVE. */
#define BRIDGE_UI_PORT_OPEN     "PORT OPEN"
/** @brief The chip while DTR is clear, the neutral chip: 11 characters, a 77 px chip from x 155. */
#define BRIDGE_UI_PORT_CLOSED   "PORT CLOSED"
/** @brief The mode's caption. */
#define BRIDGE_UI_MODE          "MODE"
/** @brief The mode before the PC's first byte. */
#define BRIDGE_UI_WAITING       "WAITING"
/** @brief The mode once it has sent one: 11 characters, 130 px at scale 2. */
#define BRIDGE_UI_PASSTHROUGH   "PASSTHROUGH"
/** @brief The mode in MSP and the 4-way interface: 15 characters, 178 px at scale 2, to x 185. */
#define BRIDGE_UI_4WAY          "4-WAY INTERFACE"
/** @brief The left counter's caption: bytes from the PC to the ESC. */
#define BRIDGE_UI_TO_ESC        "PC > ESC"
/** @brief The right counter's caption: bytes from the ESC to the PC, the echo included. */
#define BRIDGE_UI_TO_PC         "ESC > PC"
/** @brief The frames line: frames, then LOST, amber above 0. */
#define BRIDGE_UI_FRAMES_FMT    "FRAMES %lu  "
/** @brief Its second part. */
#define BRIDGE_UI_LOST_FMT      "LOST %lu"
/** @brief Message line 0 while the bridge waits or runs: 37 characters, 221 px. */
#define BRIDGE_UI_LINE_0        "OPEN THIS PORT IN A DESKTOP ESC TOOL."
/** @brief Line 1. */
#define BRIDGE_UI_LINE_1        "THE ESC RUNS ON ITS OWN BATTERY."
/** @brief Line 2. */
#define BRIDGE_UI_LINE_2        "PROPELLER OFF. DSHOT IS OFF."
/** @brief Line 4; line 3 is blank. */
#define BRIDGE_UI_LINE_4        "POWER-CYCLE THE ESC IF THE TOOL"
/** @brief Line 5. */
#define BRIDGE_UI_LINE_5        "FINDS NO ESC, AND AFTER A FLASH."
/** @brief Line 0, amber, while a write-class frame waits for its answer. */
#define BRIDGE_UI_WRITING       "WRITING: DO NOT UNPLUG ESC OR USB."
/** @brief Line 1, amber, once a byte was lost. */
#define BRIDGE_UI_LOST          "BYTES LOST: THE BRIDGE FELL BEHIND."
/** @brief Line 3, blank otherwise, amber, for @ref BRIDGE_MOTOR_NOTE_MS after an `MSP_SET_MOTOR`: 34 characters, 203 px. */
#define BRIDGE_UI_MOTOR         "MOTOR COMMAND FROM THE PC REFUSED."
/** @brief Line 0, dim, after the PC closed the port. */
#define BRIDGE_UI_CLOSED        "THE PC CLOSED THE PORT."
/** @brief Line 0, amber, while USB DRIVE holds the card (decision 11). */
#define BRIDGE_UI_EJECT         "EJECT THE USB DRIVE FIRST."
/** @brief Line 0, amber, after a failed bind: SIG's GPIO and the reason. `(CLOCK)` is the widest, 34 characters. */
#define BRIDGE_UI_NOT_TAKEN_FMT "SIG GP%u COULD NOT BE TAKEN (%s)"
/** @brief The leave page's first step: 18 characters, 107 px from x 20. */
#define BRIDGE_UI_LEAVE_1       "POWER THE ESC OFF."
/** @brief The leave page's note at y 158: 31 characters, 185 px. */
#define BRIDGE_UI_LEAVE_NOTE    "FLASH THE ESC AGAIN BEFORE USE."
/** @brief The leave page's left button after BACK. */
#define BRIDGE_UI_BACK_TO       "BRIDGE"
/** @} */

/** @brief USB BRIDGE's controls besides BACK. @see ui_screen.h */
enum class BridgeCtl : uint8_t {
	Retry = 1,  /**< RETRY, the wide button, after a failed bind. */
	Bridge,     /**< BRIDGE on the leave page: the claim kept, the bridge open again. */
	Release,    /**< The hub's title on the leave page: parks the pins and returns to the hub, live 1000 ms after the line is let go. */
};

/** @brief Open the screen and ask for SIG, unless USB DRIVE holds the card. */
void uiBridgeEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left.
 */
bool uiBridgeTick(const TouchState *t);

/**
 * @brief The controls on the current frame: BACK, disabled while a write
 *        waits and on the leave page; RETRY after a failed bind; BRIDGE
 *        and the last button on the leave page.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiBridgeControls(UiControl *out, uint8_t max);

/** @brief USB BRIDGE. */
inline constexpr const UiScreen UI_BRIDGE = {
	UiScreenId::Bridge, BRIDGE_UI_TITLE, PinUser::Bridge, nullptr,
	uiBridgeEnter, uiBridgeTick, nullptr, uiBridgeControls, nullptr, false,
};
