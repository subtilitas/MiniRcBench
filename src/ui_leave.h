/**
 * @file ui_leave.h
 * @brief The leave page: one page for leaving a tool that holds the signal
 *        pins, from the last step on the device to DShot running again
 *        (docs/design/menu-b.md, "The leave page").
 *
 * No screen uses it yet. Slice 6 of the note moves KST SERVO, HITEC SERVO
 * and SERVO TESTER onto it in place of their leave and unplug pages. The page
 * has three phases:
 *
 * - **Before LEAVE.** The first step is current. The left button reads STAY
 *   and the wide button LEAVE. A tap on LEAVE moves the page to Releasing.
 *   While @ref UiLeave::leaveEnabled is clear, LEAVE is drawn disabled and
 *   takes no tap: the screen clears it while it writes to the device. LEAVE
 *   takes only a press that lands while it is enabled
 *   (@ref UiLeave::leavePress) and only while it is still enabled at the
 *   lift. A press that landed while it was disabled draws no pressed look
 *   and does nothing on its lift, even if the screen enables LEAVE again
 *   under the finger.
 * - **Releasing.** The pin is being let go. The step after the first is
 *   current. The left button reads the screen's next label, NEXT SERVO or
 *   BACK TO SERVO, and the wide button RESTART DSHOT, both drawn disabled.
 * - **Released**, from the frame the screen reports pinClaimUnplugDue(). The
 *   left button is live. RESTART DSHOT takes only a press that lands
 *   @ref UiLeave::waitMs or more after this phase began, and is drawn
 *   disabled until then. A press that landed earlier draws no pressed look.
 *
 * A page opened in Releasing has no step before LEAVE, so its first step is
 * current from the start.
 *
 * The instructions are numbered steps in one style: scale 1, the number at
 * x 8 and the text at x 20, the current step in @ref UI_COL_WARN with a
 * 3 px bar at x 0, the other steps in `C_DIM`. Notes follow at y 158, in
 * @ref UI_COL_WARN before LEAVE and in `C_DIM` after it.
 *
 * The screen draws the header and the strip; BACK on the page is the
 * screen's (open question 3 of the note). A right-hand button, HOLD TO REBOOT
 * or READ AGAIN, is the screen's too: it sits at `UI_LEAVE_RIGHT` before
 * LEAVE and is not drawn from the tap on LEAVE on.
 *
 * Pure but for uiLeaveDraw(), which draws through gfx.h.
 */

#pragma once

#include <stdint.h>

#include "ui_widgets.h"

struct TouchState;

/**
 * @defgroup ui_leave_layout Leave page layout
 * @{
 */
/** @brief Steps on the page. */
#define UI_LEAVE_STEPS       3
/** @brief Lines a step holds: its numbered line and two detail lines. */
#define UI_LEAVE_STEP_LINES  3
/** @brief Note lines under the steps. */
#define UI_LEAVE_NOTES       4
/** @brief Top of the first step's first line. */
#define UI_LEAVE_STEP_Y     66
/** @brief Line pitch of the steps and the notes, px. */
#define UI_LEAVE_LINE_H     12
/** @brief Extra space between two steps, px: 14 px from line to line. */
#define UI_LEAVE_STEP_GAP    2
/** @brief Indent of a step's text, px: two characters at scale 1. */
#define UI_LEAVE_TEXT_X     (UI_MARGIN + 12)
/** @brief Width of the current step's bar, px. */
#define UI_LEAVE_BAR_W       3
/**
 * @brief Top of the first note line. A step line is drawn only while its
 *        foot leaves 1 px free above it: three steps hold seven lines.
 */
#define UI_LEAVE_NOTE_Y    158
/** @brief Top of the left and right buttons. */
#define UI_LEAVE_BTN_Y     214
/** @brief Height of every button on the page, px. */
#define UI_LEAVE_BTN_H      46
/** @} */

/** @brief The left button: STAY, then NEXT SERVO or BACK TO SERVO. */
constexpr UiRect UI_LEAVE_LEFT  = { UI_EDGE, UI_LEAVE_BTN_Y, 110, UI_LEAVE_BTN_H };
/** @brief The screen's right-hand button, before LEAVE only. */
constexpr UiRect UI_LEAVE_RIGHT = { 122, UI_LEAVE_BTN_Y, 112, UI_LEAVE_BTN_H };
/** @brief LEAVE, then RESTART DSHOT, at scale 2. */
constexpr UiRect UI_LEAVE_WIDE  = { UI_EDGE, UI_FOOT_Y, 240 - 2 * UI_EDGE, UI_FOOT_H };

static_assert(UI_LEAVE_NOTE_Y + (UI_LEAVE_NOTES - 1) * UI_LEAVE_LINE_H + 7 < UI_LEAVE_BTN_Y - 4,
              "the notes run into the right button's hold bar");
static_assert(UI_LEAVE_LEFT.x + UI_LEAVE_LEFT.w < UI_LEAVE_RIGHT.x &&
              UI_LEAVE_RIGHT.x + UI_LEAVE_RIGHT.w == 240 - UI_EDGE,
              "the two buttons overlap or leave the button edge");
static_assert(UI_LEAVE_BTN_Y + UI_LEAVE_BTN_H < UI_LEAVE_WIDE.y,
              "the two buttons run into the wide one");

/** @brief The page's phase. */
enum class UiLeavePhase : uint8_t {
	Before,     /**< Before the tap on LEAVE. */
	Releasing,  /**< The pin is being let go. */
	Released,   /**< The pin is let go: the device may be unplugged. */
};

/** @brief What one frame of touch did on the page. */
enum class UiLeaveEv : uint8_t {
	None,     /**< Nothing for the screen to act on. */
	Stay,     /**< STAY: back to the tool. */
	Leave,    /**< LEAVE: end the claim. The page is now Releasing. */
	Next,     /**< NEXT SERVO or BACK TO SERVO: keep the claim. */
	Restart,  /**< RESTART DSHOT: give the pins back to DShot. */
};

/** @brief The page's state. Set it up with uiLeaveOpen(). */
struct UiLeave {
	UiLeavePhase phase;         /**< The phase. */
	bool         before;        /**< Opened before LEAVE: step 0 is the step before it. */
	bool         leaveEnabled;  /**< LEAVE is live. uiLeaveOpen() sets it; the screen clears
	                                 it while it writes and sets it again after. */
	bool         leavePress;    /**< The press under way landed while LEAVE was enabled. */
	bool         restartPress;  /**< The press under way landed while RESTART DSHOT was live. */
	uint32_t     releasedMs;    /**< millis() when Released began. */
	uint32_t     waitMs;        /**< RESTART DSHOT's wait after Released began, ms. */
};

/** @brief What the page shows, filled by the screen for each draw. */
struct UiLeaveText {
	/** Each step's numbered line and detail lines; a null line ends the
	    step, a step whose first line is null ends the steps. */
	const char *step[UI_LEAVE_STEPS][UI_LEAVE_STEP_LINES];
	/** The notes; a null line ends them. */
	const char *note[UI_LEAVE_NOTES];
	/** The left button from the tap on LEAVE on: NEXT SERVO or BACK TO
	    SERVO; null draws none. */
	const char *next;
};

/**
 * @brief Open the page.
 * @param[out] l The page.
 * @param phase  @ref UiLeavePhase::Before, or Releasing for a page with no
 *               step before LEAVE; Released opens it as Releasing.
 * @param waitMs RESTART DSHOT's wait, ms; 0 makes it live with the released
 *               phase. Open question 8 of the note.
 */
void uiLeaveOpen(UiLeave *l, UiLeavePhase phase, uint32_t waitMs);

/**
 * @brief Move a releasing page to Released on the first frame the pin is
 *        let go.
 * @param[in,out] l The page.
 * @param due       pinClaimUnplugDue(), read once for the frame.
 * @param nowMs     Current millis().
 */
void uiLeavePoll(UiLeave *l, bool due, uint32_t nowMs);

/**
 * @brief The index of the current step.
 * @param l The page.
 * @return 0 before LEAVE and on a page opened releasing; 1 otherwise.
 */
int uiLeaveCurrent(const UiLeave *l);

/**
 * @brief Whether RESTART DSHOT is live.
 * @param l     The page.
 * @param nowMs Current millis().
 * @return true once Released has lasted @ref UiLeave::waitMs.
 */
bool uiLeaveRestartLive(const UiLeave *l, uint32_t nowMs);

/**
 * @brief Feed one frame of touch to the page.
 * @param[in,out] l The page.
 * @param t         This frame's touch; null is treated as no touch.
 * @param nowMs     Current millis().
 * @return The event; @ref UiLeaveEv::None for most frames.
 */
UiLeaveEv uiLeaveTouch(UiLeave *l, const TouchState *t, uint32_t nowMs);

/**
 * @brief Draw the page from @ref UI_BODY_Y down: the steps, the notes, the
 *        left button and the wide button, over a cleared body.
 *
 * It clears the whole body, y 62 to y 319, the right button's place and its
 * hold bar included, so the screen draws its right button and hold bar after
 * it. Feed the frame's touch to uiLeaveTouch() first: the pressed look of
 * LEAVE follows @ref UiLeave::leavePress, that of RESTART DSHOT
 * @ref UiLeave::restartPress.
 * @param l     The page.
 * @param text  What it shows.
 * @param t     This frame's touch, for the pressed look; may be null.
 * @param nowMs Current millis().
 */
void uiLeaveDraw(const UiLeave *l, const UiLeaveText &text, const TouchState *t,
                 uint32_t nowMs);
