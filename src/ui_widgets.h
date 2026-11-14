/**
 * @file ui_widgets.h
 * @brief One button, one header, one row. Every screen draws through these.
 *
 * This file exists because four screens had grown four answers to the same
 * questions, and the differences were not decisions — nobody chose a 6 px
 * corner radius for the main screen and a 5 px one for AM32, or a 26 px header
 * on SETTINGS, a 30 px one on SETUP and a 50 px one on AM32. They were three
 * copies of the same twelve-line function drifting apart one edit at a time,
 * and the most visible symptom was BACK: top-right on two screens, a thin strip
 * along the bottom on two others, and nowhere at all on the main one.
 *
 * The rules the copies now share, and why each is the way it is:
 *
 * - **BACK is always the same rectangle in the header, top right.** Its
 *   position is a habit, and a habit that is wrong half the time is worse than
 *   no habit. @see UI_BACK_X
 * - **A pressed button is drawn additively** — a brighter frame and the label
 *   nudged — never a swapped fill. Buttons here pick a label colour to suit
 *   their own fill (white on red, cyan on panel, background on a flash), so a
 *   pressed state that replaced the fill would have to know about all of them
 *   to stay legible. Adding to what is there cannot break any pairing.
 * - **Touch targets have a floor.** @ref UI_TAP_MIN is what a fingertip needs;
 *   the 18 px BACK strips this replaces were below it.
 * - **A control that is drawn unavailable does not act.** Pass `enabled=false`
 *   to uiButton() and gate its hit test on the same expression. Before this
 *   there were three answers on three screens: SETTINGS' KISS PIN steppers looked
 *   live and silently moved a pin that was not in use, the logging screen's
 *   START looked dead and started a log anyway, and AM32's REVERT looked dead
 *   and ran. A greyed control that still fires is worse than either, because
 *   the screen has told you it will not.
 *
 * Hit testing is not duplicated here: it is inputPressing() and inputTapped()
 * in ui_input.h, and the wrappers below only spare callers from spelling out
 * four members of a rectangle they already have.
 *
 * @see ui_input.h for the tap, press and repeat rules these are drawn from.
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @defgroup uiw_metrics Shared metrics
 * @brief The numbers that were four different numbers.
 * @{
 */
/** @brief Height of the header band on every screen. */
#define UI_HDR_H        36
/** @brief Left margin for titles, labels and the first control in a row. */
#define UI_MARGIN        8
/** @brief Corner radius for every button and chip. */
#define UI_RADIUS        5
/**
 * @brief Smallest side of a touch target outside a list: the tester's button
 *        row, the SERVO screen's controls, SD LOG's RETRY and USB DRIVE and
 *        AM32's OK. Their move to @ref UI_LIST_TAP_MIN is a later change.
 */
#define UI_TAP_MIN      22
/**
 * @brief Smallest height of a list row and of a control in a list row, px:
 *        about 7.1 mm on this panel. A row or control cut by the viewport's
 *        edge takes a press only while this much of it shows.
 */
#define UI_LIST_TAP_MIN 40

/**
 * @brief Button edge, px: a button starts at x 6 and ends at x 234, as BACK
 *        does. List rows keep x 0 to 239 and text keeps @ref UI_MARGIN.
 */
#define UI_EDGE          6
/** @brief Top of a screen's footer row of buttons, y 268. */
#define UI_FOOT_Y      268
/** @brief Height of a footer button, 46 px: the footer ends at y 313. */
#define UI_FOOT_H       46
/** @brief Width of an editor bar's - and + buttons, 44 px. */
#define UI_EDIT_BTN_W   44

/** @brief BACK button size, 64 x 32 px, on every screen with a header. */
#define UI_BACK_W       64
#define UI_BACK_H       32
/** @brief BACK button position: top right of the header, on every screen. */
#define UI_BACK_X       (240 - UI_BACK_W - UI_EDGE)
#define UI_BACK_Y       ((UI_HDR_H - UI_BACK_H) / 2)

/**
 * @brief Top of the optional strip under the header.
 *
 * Where a screen puts the one or two things that qualify what the title says —
 * AM32's device name, MENU's pin and rate caption and EDT chip. Below the header
 * rather than inside it, because the header is full: a title at scale 2 and a
 * BACK button leave no room for a second line that is not cramped.
 */
#define UI_STRIP_Y      (UI_HDR_H + 4)
/** @brief Height of that strip. Sized to hold a chip. */
#define UI_STRIP_H      16
/** @brief First row a screen's own content may use. */
#define UI_BODY_Y       (UI_STRIP_Y + UI_STRIP_H + 6)
/** @brief Top of the first of the strip's two lines, y 42. @see uiStripTwoLines() */
#define UI_STRIP_LINE1_Y (UI_STRIP_Y + 2)
/** @brief Top of the second line, y 52: it ends at y 58, above the body. */
#define UI_STRIP_LINE2_Y (UI_STRIP_Y + 12)
/** @} */

/**
 * @defgroup uiw_roles Colour roles
 * @brief One meaning per colour: the role names of the `C_*` palette entries
 *        (docs/design/menu-b.md, decision 11).
 *
 * Each name expands to its palette entry, so it follows the active theme.
 * The role "changed", a value edited and not yet written, has no name here:
 * its colour is open question 4 of the note. The neutral role is three
 * colours, `C_TEXT`, `C_DIM` and `C_GRID`, and keeps their names.
 *
 * Each role is described by its meaning. The examples after "Target:" are
 * the note's target for the screens; no screen uses the names yet, and some
 * of today's screens draw those elements in another colour. Slice 2 of the
 * note, "Colour roles", moves the screens onto the names.
 * @{
 */
/** @brief Safe or done. Target: the SAFE badge and chip, EDT ON, WRITE
 *         VERIFIED, SAVED. */
#define UI_COL_SAFE     C_GREEN
/** @brief A live reading. Target: RPM, the throttle fill, hold progress bars,
 *         LINK. */
#define UI_COL_LIVE     C_LIME
/** @brief Output on, or a failure. Target: ARMED, RUNNING, a running hold's
 *         fill. */
#define UI_COL_ON       C_RED
/** @brief A warning: text to read before the next action. Target: the
 *         current step of the leave page, soft-rule bars. */
#define UI_COL_WARN     C_AMBER
/** @brief An alert the device or the bench raised, which needs a look.
 *         Target: the ESC status alert, STALLED. */
#define UI_COL_ALERT    C_MAGENTA
/** @brief A setting that stays on until it is turned off. Target: LATCH on,
 *         a toggle that is on, servo B. */
#define UI_COL_LATCHED  C_BLUE
/** @} */

static_assert(UI_BACK_H <= UI_HDR_H, "BACK does not fit in the header band");
static_assert(UI_BACK_W >= UI_TAP_MIN, "BACK is narrower than a fingertip");
static_assert(UI_BACK_H >= UI_TAP_MIN, "BACK is lower than a fingertip");
static_assert(UI_FOOT_Y + UI_FOOT_H <= 320, "the footer runs off the panel");
static_assert(UI_FOOT_H >= UI_LIST_TAP_MIN && UI_EDIT_BTN_W >= UI_LIST_TAP_MIN,
              "a footer or editor button is smaller than the list floor");
static_assert(UI_STRIP_LINE2_Y + 7 <= UI_BODY_Y, "the strip's second line runs into the body");

/** @brief A touch target and its drawn rectangle, which are always the same. */
struct UiRect {
	int16_t x, y, w, h;
};

/**
 * @brief True while @p r is touched by a press that began inside it.
 * @param r Rectangle.
 * @param t This frame's touch snapshot; null is treated as no touch.
 * @return true for pressed-state drawing and held repeats.
 */
bool uiPressing(const UiRect &r, const TouchState *t);

/**
 * @brief True on the frame a tap of @p r completes.
 * @param r Rectangle.
 * @param t This frame's touch snapshot; null is treated as no touch.
 * @return true exactly once per completed tap.
 */
bool uiTapped(const UiRect &r, const TouchState *t);

/**
 * @brief Draw a rounded button with its label centred.
 *
 * @param r       Rectangle.
 * @param label   Text, centred.
 * @param fill    Resting fill colour.
 * @param fg      Label colour, chosen against @p fill.
 * @param scale   Text scale.
 * @param pressed True while a finger is on it, from uiPressing(). Ignored when
 *                @p enabled is false -- a control that cannot act cannot show a
 *                press either.
 * @param enabled False draws the unavailable look: an outline with no fill,
 *                which reads as "not a thing you press" at arm's length in a
 *                way that a dimmed label does not. @p fill and @p fg are then
 *                ignored, so callers can keep passing the colours the control
 *                has when it is live.
 *
 * Drawing a button disabled is half of it. The caller must gate the hit test on
 * the same expression; see the rule at the top of this file.
 */
void uiButton(const UiRect &r, const char *label, uint16_t fill, uint16_t fg,
              int scale, bool pressed, bool enabled = true);

/**
 * @brief Draw the header band: title on the left, BACK on the right.
 *
 * Draws BACK on every screen that has a header, which is what stops it being a
 * control that exists on some of them and is remembered on all of them.
 *
 * @param title       Screen name, drawn at scale 2.
 * @param t           This frame's touch snapshot, for BACK's pressed state.
 *                    May be null.
 * @param backEnabled False draws BACK unavailable, as uiButton() draws a
 *                    disabled control: an outline, a grey label and no pressed
 *                    look. Pass the expression the screen's control table
 *                    reports for `UI_FIX(UI_BACK)` and gates the tap on, and
 *                    key the repaint on it: the unplug pages of SERVO, KST
 *                    SERVO and HITEC SERVO, whose BACK waits for
 *                    pinClaimUnplugDue().
 */
void uiHeader(const char *title, const TouchState *t, bool backEnabled = true);

/** @brief Rectangle of the header's BACK button. @return The shared target. */
UiRect uiBackRect();

/**
 * @brief True on the frame the header's BACK button is tapped.
 * @param t This frame's touch snapshot; null is treated as no touch.
 * @return true exactly once per completed tap.
 */
bool uiBackTapped(const TouchState *t);

/**
 * @brief Clear the strip under the header, ready for uiStripText() or uiChip().
 *
 * Separate from the two so a screen can put both a caption and a chip on it
 * without one of them wiping the other.
 */
void uiStripClear();

/**
 * @brief Left-aligned caption on the strip under the header.
 * @param text Caption.
 * @param col  Colour.
 */
void uiStripText(const char *text, uint16_t col);

/**
 * @brief Two lines on the strip, for a strip that says more than one line
 *        holds: the first at y 42 (@ref UI_STRIP_LINE1_Y), the second at
 *        y 52 (@ref UI_STRIP_LINE2_Y), both scale 1 from @ref UI_MARGIN.
 *
 * Clears y 40 to y 61 first, the strip and the gap above the body, since the
 * second line ends at y 58, below the strip. A chip drawn after it sits on
 * the first line: uiChip() is 16 px tall, y 40 to y 55, so it covers rows
 * 52 to 55 of the second line wherever the two share x. A null line is not
 * drawn.
 *
 * @param top       First line, or null.
 * @param topCol    Its colour.
 * @param bottom    Second line, or null.
 * @param bottomCol Its colour.
 */
void uiStripTwoLines(const char *top, uint16_t topCol, const char *bottom, uint16_t bottomCol);

/**
 * @brief The guard on a BACK that drops edits nobody wrote
 *        (docs/design/menu-b.md, decision 7).
 *
 * With changes pending, the first BACK arms the guard and the screen shows
 * uiBackGuardDraw() in place of its strip; a second BACK leaves and drops
 * them. A press that lands anywhere but BACK disarms it. There is no timer.
 * Zero-initialised it is disarmed.
 *
 * No screen uses it yet. Slice 5 of the note, "Write rule and guard", puts
 * it on AM32 CONFIG and SERVO SET.
 */
struct UiBackGuard {
	bool armed;  /**< The first BACK landed; the next one leaves. */
};

/** @brief What one frame of touch did to a @ref UiBackGuard. */
enum class UiGuardEv : uint8_t {
	None,      /**< Nothing for the screen to act on. */
	Armed,     /**< The first BACK with changes pending: show the warning. */
	Disarmed,  /**< A press elsewhere, or no change left: the strip returns. */
	Back,      /**< BACK acts: leave, dropping the changes. */
};

/**
 * @brief Feed one frame of touch to the guard.
 *
 * A BACK tap with no change pending is @ref UiGuardEv::Back at once, also
 * on the frame an armed guard finds no change left. The caller feeds the
 * guard only while BACK is live.
 *
 * @param[in,out] g The guard.
 * @param t         This frame's touch; null is treated as no touch.
 * @param pending   Changes not yet written or applied.
 * @return The event; @ref UiGuardEv::None for most frames.
 */
UiGuardEv uiBackGuard(UiBackGuard *g, const TouchState *t, int pending);

/**
 * @brief Draw the armed guard's two lines on the strip, in
 *        @ref UI_COL_WARN, through uiStripTwoLines(). With two changes
 *        they read `2 CHANGES NOT WRITTEN.` over
 *        `BACK AGAIN DISCARDS THEM.`; with one, `1 CHANGE NOT WRITTEN.`
 *        over `BACK AGAIN DISCARDS IT.`
 * @param pending Changes not yet written or applied, 1 or more.
 * @param verb    The last word but the full stop: `WRITTEN`, `APPLIED`.
 */
void uiBackGuardDraw(int pending, const char *verb);

/**
 * @brief Right-aligned pill badge on the strip under the header.
 *
 * For a fact worth reading at a glance and never worth tapping — EDT on or off,
 * which board this is. Deliberately not button-shaped: nothing here responds to
 * a press, and a control that looks tappable and is not gets tapped.
 *
 * @param text Label, drawn at scale 1.
 * @param fill Pill colour. The label is always @ref C_ONACCENT over it.
 */
void uiChip(const char *text, uint16_t fill);

/**
 * @brief One label / value row: caption left, value right-aligned.
 *
 * @param y     Row top.
 * @param h     Row height. The row's background is cleared over it.
 * @param label Left-hand caption, always scale 1.
 * @param value Right-aligned value.
 * @param vcol  Value colour.
 * @param scale Value text scale.
 */
void uiRow(int y, int h, const char *label, const char *value, uint16_t vcol,
           int scale);
