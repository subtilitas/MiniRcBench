/**
 * @file ui_sbus.h
 * @brief S.BUS MONITOR: a receiver's S.BUS output on GP1, its 16 channels,
 *        two digital channels, link state, frame rate and error counts.
 *
 * S.BUS is Futaba's serial bus for receiver channels. Entry: MENU → RX
 * SIGNALS → MONITOR (ui_rx_signals.h). The screen claims the role that is
 * GP1 (sbusClaimRoles(): SIG and AUX in the default wiring, SIG alone when
 * SIG is GP1) for PinUser::Sbus, and core1's S.BUS job (sbus_job.h) receives
 * on it through UART0 with the pad's input inverted. The screen drives
 * nothing; it reads SbusStatus once a frame and draws it through the pure
 * formatter of sbus_view.h.
 *
 * Before it claims, the screen checks the wiring:
 *
 * - **No role on GP1.** No claim: `S.BUS NEEDS GP1.`
 *   `SET A PIN TO GP1 IN SETTINGS.` Unreachable through SETTINGS on this
 *   board, whose two steppers swap GP0 and GP1; kept as the backstop over
 *   the job's CORE1_JOB_NO_GP1.
 * - **GP1 is SIG.** The claim's ESC check is off for this user (pin_claim.h,
 *   `escCheck`), so the screen makes its own: while an eRPM frame arrived
 *   within @ref ESC_LINK_STALE_MS, 500 ms, it does not claim and shows
 *   `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT: THE RECEIVER GOES THERE.`, and
 *   asks again every frame.
 *
 * BACK leaves through the leave page (ui_leave.h): the job lets GP1 go at
 * once, and the page opens releasing, as SERVO TESTER's does. Its step names
 * where DShot restarts; BACK TO MONITOR keeps the claim and binds the job
 * again, with every count at 0; RESTART DSHOT, live 1000 ms after the pin is
 * let go, returns to MENU. BACK before a claim held, or during Stopping,
 * returns to MENU with DShot resumed.
 *
 * @see docs/design/rx-signals.md, "Screen"
 */

#pragma once

#include <stdint.h>

struct TouchState;
struct UiControl;
struct UiScreen;

/** @brief The header's title: 13 characters, 154 px at scale 2, x 8 to 161. */
#define SBUS_UI_TITLE "S.BUS MONITOR"

/**
 * @defgroup ui_sbus_layout S.BUS MONITOR's layout
 * @{
 */
#define SBUS_UI_ROW_Y      62   /**< Channel 1's row, @ref UI_BODY_Y. */
#define SBUS_UI_ROW_PITCH  12   /**< Row top to row top. */
#define SBUS_UI_ROW_H      11   /**< A row: the bar's outline is 9 px of it. */
#define SBUS_UI_NUM_END    19   /**< The channel number ends before this x: `16` from x 8. */
#define SBUS_UI_BAR_X      24   /**< The bar's 1 px outline. */
#define SBUS_UI_BAR_W      176  /**< ...to x 199. */
#define SBUS_UI_BAR_H      9    /**< From the row's top + 1. */
#define SBUS_UI_FILL_W     (SBUS_UI_BAR_W - 2)  /**< The fill at raw 2047: 174 px from x 25. */
#define SBUS_UI_VALUE_END  232  /**< A value ends before this x. */
#define SBUS_UI_LINE1_Y    258  /**< `CH17 OFF  CH18 OFF` and `14.0 MS  71.4 HZ`. */
#define SBUS_UI_LINE2_Y    272  /**< `FRAMES 123456` and `LOST 9999  FS 9999`. */
#define SBUS_UI_LINE3_Y    286  /**< `PAR 0 FRM 0 BRK 0 OVR 0`. */
#define SBUS_UI_LINE4_Y    300  /**< `RESYNC 0 FOOTER 0`. */
#define SBUS_UI_MSG_Y      80   /**< The first line of a message in place of the channels. */
#define SBUS_UI_MSG_H      14   /**< Message line to line. */
/** @} */

/**
 * @defgroup ui_sbus_text S.BUS MONITOR's texts
 * @{
 */
/**
 * @brief The strip's caption, with GP1's role (`AUX`, or `SIG`) for `%s`:
 *        `AUX GP1  100 KBAUD  8E2`, 23 characters, 137 px, x 8 to 144, 16 px
 *        or more before the widest chip, `FRAME LOST`, at x 161.
 */
#define SBUS_UI_CAPTION_FMT "%s " SBUS_UI_CAPTION_TAIL
/** @brief The caption with no role on GP1: `GP1  100 KBAUD  8E2`. */
#define SBUS_UI_CAPTION_TAIL "GP1  100 KBAUD  8E2"
/** @brief No role on GP1, line 1, amber. */
#define SBUS_UI_NO_GP1_1    "S.BUS NEEDS GP1."
/** @brief ...line 2. */
#define SBUS_UI_NO_GP1_2    "SET A PIN TO GP1 IN SETTINGS."
/** @brief An ESC answers on GP1 while it is SIG, line 1, amber: 26 characters. */
#define SBUS_UI_ESC_1       "AN ESC ANSWERS ON SIG GP1."
/** @brief KISS frames on GP1 while it is AUX, line 1, amber: 29 characters. */
#define SBUS_UI_KISS_1      "KISS WIRE ON AUX GP1 IS LIVE."
/** @brief The second line of both, amber: 35 characters, 209 px. */
#define SBUS_UI_UNPLUG_2    "UNPLUG IT: THE RECEIVER GOES THERE."
/** @brief The leave page's step that unplugs the receiver: 20 characters. */
#define SBUS_UI_UNPLUG      "UNPLUG THE RECEIVER."
/** @brief Its detail line, SIG's GPIO for `%u`, when GP1 is AUX: 26 characters. */
#define SBUS_UI_RESTART_FMT "DSHOT RESTARTS ON SIG GP%u."
/** @brief Its detail lines when GP1 is SIG: 32 characters... */
#define SBUS_UI_RESTART_GP1_1 "DSHOT RESTARTS ON SIG GP1, WHERE"
/** @brief ...and 29. */
#define SBUS_UI_RESTART_GP1_2 "THE RECEIVER DRIVES THE LINE."
/** @brief The leave page's left button after the tap on BACK: 15 characters, 89 px. */
#define SBUS_UI_BACK_TO     "BACK TO MONITOR"
/** @} */

/** @brief S.BUS MONITOR's controls besides BACK. @see ui_screen.h */
enum class SbusCtl : uint8_t {
	Retry = 1,      /**< RETRY, the wide button, after a failed bind. */
	BackToMonitor,  /**< BACK TO MONITOR on the leave page: the claim kept, the job bound again. */
	Restart,        /**< RESTART DSHOT on the leave page, live 1000 ms after the pin is let go. */
};

/**
 * @brief Open the screen: every count at 0, and a claim of the role on GP1,
 *        asked again every frame until it is accepted.
 */
void uiSbusEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left: after RESTART DSHOT on the leave
 *         page, or after BACK when no job had run.
 */
bool uiSbusTick(const TouchState *t);

/**
 * @brief The controls on the current frame: BACK; RETRY under a failed bind;
 *        BACK TO MONITOR and RESTART DSHOT on the leave page.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiSbusControls(UiControl *out, uint8_t max);

/** @brief S.BUS MONITOR. */
extern const UiScreen UI_SBUS;
