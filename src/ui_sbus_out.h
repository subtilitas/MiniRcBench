/**
 * @file ui_sbus_out.h
 * @brief S.BUS OUTPUT: S.BUS frames on GP0 to a flight controller (FC), 16
 *        channels on sliders, CH17, CH18 and the two flags on SET, the
 *        throttle channel under MAX THROTTLE.
 *
 * S.BUS is Futaba's serial bus for receiver channels. Entry: MENU → RX
 * SIGNALS → OUTPUT (ui_rx_signals.h). The screen claims the role that is GP0
 * (sbusOutClaimRoles(): SIG in the default wiring, SIG and AUX when GP0 is
 * AUX) for PinUser::SbusOut, whose row has the claim's ESC check, and core1's
 * S.BUS output job (sbus_out_job.h) probes GP0 for an ESC's bootloader, then
 * sends the frames. The screen draws through the pure formatter of
 * sbus_out_view.h.
 *
 * **START only through the probe.** HOLD TO START, 1000 ms on ui_hold, is
 * live only while the claim runs the job, the job reports no bind error, the
 * frames are off, and sbusOutProbePassed() holds: a probe cycle with no
 * answer has passed in this bind, which is this visit. While the probe runs
 * the rows' area reads `CHECKING GP0 FOR AN ESC.`; while a bootloader
 * answers it asks for the ESC's battery and lead to come off. Core1 takes no
 * command before the probe passes either (sbus_out_job.h).
 *
 * **The frames.** START sends core1's START frame whatever the sliders
 * showed and resets them to it. A slider moves in 5 us steps by delta drag;
 * a press alone commands nothing. Each change goes to core1 as a Frame, every
 * channel held to raw 172 to 1811 and the throttle channel to the ceiling of
 * MAX THROTTLE, sbusOutCeilRaw(): the throttle row's knob stops at the
 * ceiling's mark. Core1 clamps both again. STOP acts on press, on the main
 * view and on SET. Frames stop when the UI stops for 250 ms (the job's
 * heartbeat: `STALLED`), and when no touch came for more than 30 s while
 * the frame is off its START values on the throttle, channels 5 to 16, CH17
 * or CH18 (sbusOutIdleDue()). Either needs a new HOLD TO START.
 *
 * **The warnings.** While SAFE the main view's two notes read
 * `GP0 GOES TO A FLIGHT CONTROLLER.` `NEVER TO AN ESC. PROPS OFF.`, and
 * SET's footer the second; RX SIGNALS carries them under OUTPUT. The probe
 * misses an ESC that is unpowered during it or plugged in after it.
 *
 * **Leaving.** BACK on the main view is the leave: the job lets GP0 go
 * after the frame on the wire, and the leave page (ui_leave.h) opens
 * releasing, `UNPLUG THE FLIGHT CONTROLLER.`. After an EFM8 bootloader
 * answered the probe (sbusOutProbeEfm8()), BACK first stops the frames and
 * opens the page before LEAVE with the first step of decision 8 of
 * docs/design/bluejay-blheli-s.md, `UNPLUG THE ESC BATTERY, THEN LEAVE.`;
 * STAY returns to the screen. BACK TO OUTPUT keeps the claim: the job binds
 * again, a new visit with a new probe. RESTART DSHOT, live 1000 ms after GP0
 * is let go, calls sbusOutProbeRestart() and returns to MENU. BACK before a
 * claim held, or while Stopping, returns to MENU with DShot resumed.
 *
 * @see docs/design/sbus-output.md, "Screen"
 */

#pragma once

#include <stdint.h>

struct TouchState;
struct UiControl;
struct UiScreen;

/** @brief The header's title: 12 characters, 142 px at scale 2, x 8 to 149. */
#define SBOUT_UI_TITLE "S.BUS OUTPUT"
/** @brief SET's title: 10 characters, 118 px at scale 2. */
#define SBOUT_UI_SET_TITLE "OUTPUT SET"

/**
 * @defgroup ui_sbus_out_layout S.BUS OUTPUT's layout
 * @{
 */
#define SBOUT_UI_NOTE1_Y    62   /**< Note 1, @ref UI_BODY_Y. */
#define SBOUT_UI_NOTE2_Y    70   /**< Note 2, ending at y 76. */
#define SBOUT_UI_ROW_Y      80   /**< The first row's top. */
#define SBOUT_UI_ROW_PITCH  25   /**< Row top to row top. */
#define SBOUT_UI_ROW_H      22   /**< A row: @ref UI_TAP_MIN. */
#define SBOUT_UI_ROWS       8    /**< Rows on a page; two pages. */
#define SBOUT_UI_TEXT_DY    8    /**< A row's label and value from its top. */
#define SBOUT_UI_TRACK_X    54   /**< The track: 1000 us at x 54. */
#define SBOUT_UI_TRACK_W    146  /**< ...2000 us at x 199. */
#define SBOUT_UI_TRACK_DY   6    /**< The drawn track from the row's top. */
#define SBOUT_UI_TRACK_H    10   /**< The drawn track's height; the knob stands 3 px proud. */
#define SBOUT_UI_VALUE_END  232  /**< A value ends before this x. */
#define SBOUT_UI_BAR_Y      281  /**< HOLD TO START's progress bar, 3 px. */
#define SBOUT_UI_BOT_Y      285  /**< HOLD TO START or STOP, the page, SET. */
#define SBOUT_UI_BOT_H      32   /**< ...to y 316. */
#define SBOUT_UI_MSG_Y      80   /**< The first line of a message in place of the notes and rows. */
#define SBOUT_UI_MSG_H      14   /**< Message line to line. */
#define SBOUT_UI_FOOT_TEXT_Y 287 /**< SET's footer warning while SAFE. */
/** @} */

/**
 * @defgroup ui_sbus_out_text S.BUS OUTPUT's texts
 * @{
 */
/** @brief The strip's caption, GP0's role for `%s`: `SIG GP0  100 KBAUD  8E2`, 23 characters, 137 px. */
#define SBOUT_UI_CAPTION_FMT "%s " SBOUT_UI_CAPTION_TAIL
/** @brief The caption with no role on GP0. */
#define SBOUT_UI_CAPTION_TAIL "GP0  100 KBAUD  8E2"
/** @brief The page button on page 1: 7 characters, 41 px. */
#define SBOUT_UI_PAGE_2 "CH 9-16"
/** @brief ...on page 2: 6 characters, 35 px. */
#define SBOUT_UI_PAGE_1 "CH 1-8"
/** @brief An ESC answers on SIG, SIG's GPIO for `%u`, amber: 26 characters, 155 px. */
#define SBOUT_UI_ESC_FMT  "AN ESC ANSWERS ON SIG GP%u."
/** @brief ...line 2: 26 characters, 155 px. */
#define SBOUT_UI_ESC_2    "UNPLUG IT: NO ESC STAYS ON"
/** @brief ...line 3: 27 characters, 161 px. */
#define SBOUT_UI_ESC_3    "THE BENCH WHILE S.BUS RUNS."
/** @brief KISS frames on AUX while GP0 is AUX, amber: 29 characters, 173 px. */
#define SBOUT_UI_KISS_1   "KISS WIRE ON AUX GP0 IS LIVE."
/** @brief ...line 2: 26 characters, 155 px. */
#define SBOUT_UI_KISS_2   "UNPLUG IT. PLUG THE FLIGHT"
/** @brief ...line 3, when to plug the FC in: 30 characters, 179 px. */
#define SBOUT_UI_KISS_3   "CONTROLLER IN ONCE SAFE SHOWS."
/** @brief The bootloader probe runs, dim: 24 characters, 143 px. */
#define SBOUT_UI_CHECKING "CHECKING GP0 FOR AN ESC."
/** @brief A bootloader answered the probe, amber: 33 characters, 197 px. */
#define SBOUT_UI_BL_1     "AN ESC BOOTLOADER ANSWERS ON GP0."
/** @brief ...line 2: 32 characters, 191 px. */
#define SBOUT_UI_BL_2     "UNPLUG THE ESC BATTERY AND LEAD."
/** @brief ...line 3: 31 characters, 185 px. */
#define SBOUT_UI_BL_3     "START WAITS UNTIL NONE ANSWERS."
/** @brief No role on GP0, amber: 16 characters. */
#define SBOUT_UI_NO_GP0_1 "S.BUS NEEDS GP0."
/** @brief ...line 2: 29 characters, 173 px. */
#define SBOUT_UI_NO_GP0_2 "SET A PIN TO GP0 IN SETTINGS."
/** @brief SET's note while RUNNING: 27 characters, 161 px. */
#define SBOUT_UI_SET_STOP  "STOP TO CHANGE THE CHANNEL."
/** @brief SET's note while SAFE: 23 characters, 137 px. */
#define SBOUT_UI_SET_START "START TO SET THE FLAGS."
/** @brief The leave page's step that unplugs the FC: 29 characters. */
#define SBOUT_UI_UNPLUG    "UNPLUG THE FLIGHT CONTROLLER."
/** @brief Its detail line, SIG's GPIO for `%u`: 26 characters. */
#define SBOUT_UI_RESTART_FMT "DSHOT RESTARTS ON SIG GP%u."
/** @brief After an EFM8 answer, the last step's detail line: 26 characters. */
#define SBOUT_UI_PLUG_ESC  "THEN PLUG THE ESC BACK IN."
/** @brief The leave page's left button after LEAVE: 14 characters, 83 px. */
#define SBOUT_UI_BACK_TO   "BACK TO OUTPUT"
/** @} */

/** @brief S.BUS OUTPUT's controls besides BACK. SET's rows are `UI_ROW(n, Row)`. */
enum class SbusOutCtl : uint8_t {
	Start = 1,     /**< HOLD TO START, or STOP on press while RUNNING. */
	Page,          /**< CH 9-16 or CH 1-8. */
	Set,           /**< SET. */
	Retry,         /**< RETRY, the wide button, after a failed bind. */
	Row0,          /**< The page's first slider; the page's row i is Row0 + i. */
	EditMinus = Row0 + 8,  /**< SET's editor bar, while a row is selected. */
	EditPlus,
	Stop,          /**< SET's right footer button while RUNNING: STOP on press. */
	Stay,          /**< STAY on the leave page before LEAVE, after an EFM8 answer. */
	Leave,         /**< LEAVE there. */
	BackToOutput,  /**< BACK TO OUTPUT on the leave page: the claim kept, a new probe. */
	Restart,       /**< RESTART DSHOT on the leave page, live 1000 ms after GP0 is let go. */
};

/** @brief Open the screen: the START frame on the sliders, page 1, and a claim of GP0's role. */
void uiSbusOutEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left: after RESTART DSHOT on the leave
 *         page, or after BACK when no job had run.
 */
bool uiSbusOutTick(const TouchState *t);

/**
 * @brief The controls on the current frame. @see ui_screen.h
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiSbusOutControls(UiControl *out, uint8_t max);

/** @brief S.BUS OUTPUT. */
extern const UiScreen UI_SBUS_OUT;
