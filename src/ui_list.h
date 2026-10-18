/**
 * @file ui_list.h
 * @brief The scrolling list: rows of one height in a clipped viewport, and
 *        the one list gesture.
 *
 * A screen keeps a UiList, feeds it each frame's touch with uiListTouch() and
 * acts on the event it returns. The list owns the scroll and the gesture; the
 * screen owns the selection and what a row shows. The rules, in the order a
 * gesture meets them:
 *
 * - **A press commits nothing.** It records the row under it as a candidate.
 *   The row under a press is the row whose visible body holds the point. A
 *   row is a body at the top of its pitch, and a press in the gap below the
 *   body names no row. A row cut by the viewport's edge takes a press only
 *   while at least @ref UiList::pressMin px of its body shows.
 * - **Only a gesture that began inside the viewport counts.** A press on an
 *   editor bar or a footer never scrolls the list or steps a row.
 * - **After @ref UI_AXIS_LOCK_PX of travel the gesture locks to one axis.**
 *   Sideways wins a tie. Vertical: the list follows the finger 1:1 from the
 *   press point, clamped to its range, and selects nothing. Sideways: the
 *   candidate is reported as the swipe's row, and every
 *   @ref UI_SWIPE_STEP_PX of travel is one coarse step.
 * - **A tap fires on release.** A press that never locked and is released
 *   inside the viewport is a tap on its candidate; released outside, it
 *   cancels.
 *
 * A coarse step is a number of the row's own steps, a factor the row gives.
 * uiSwipeApply() takes them one at a time while the row allows them, so a
 * coarse step that would pass a rail stops at the rail, on the row's step.
 * The swipe's anchor moves on by whole coarse steps whether or not the row
 * moved, so travel past a rail is not stored, and a swipe back moves the
 * value off the rail after one coarse step of travel back past the anchor.
 *
 * A list set up with uiListInitPx() has content of a given height and no
 * rows: it scrolls and reports no tap or swipe. A screen with rows of
 * several heights, SETTINGS, hit-tests its own rows on it.
 *
 * Pure but for uiListDraw(), which draws through gfx.h.
 */

#pragma once

#include <stdint.h>

#include "ui_widgets.h"

struct TouchState;

/** @brief Travel before a list gesture locks to one axis, px. */
#define UI_AXIS_LOCK_PX   6
/** @brief Sideways travel per coarse step of a swipe, px. */
#define UI_SWIPE_STEP_PX 16

/** @brief The axis a gesture has locked to. */
enum class UiListMode : uint8_t {
	None,      /**< No axis yet: the press has not travelled far enough.
	                Between gestures the list keeps the last axis, unread
	                until the next press sets None again. */
	Vertical,  /**< The list follows the finger. */
	Sideways,  /**< The swipe steps the candidate row. */
};

/** @brief What one frame of touch did to a list. */
enum class UiListEv : uint8_t {
	None,      /**< Nothing for the screen to act on. */
	Scrolled,  /**< The scroll changed. */
	Tap,       /**< A tap on @ref UiListEvent::row. */
	Swipe,     /**< A sideways swipe on @ref UiListEvent::row, by
	                @ref UiListEvent::steps coarse steps: 0 on the frame the
	                gesture locks, so the screen can select the row first. */
};

/** @brief A list event and its row and steps. */
struct UiListEvent {
	UiListEv kind;   /**< What happened. */
	int      row;    /**< The row, for Tap and Swipe; -1 otherwise. */
	int      steps;  /**< Coarse steps, signed, for Swipe; 0 otherwise. */
};

/** @brief A list. Set it up with uiListInit() before use. */
struct UiList {
	UiRect     view;           /**< The viewport, on the panel. */
	int16_t    rowH;           /**< A row's pitch, px. */
	int16_t    bodyH;          /**< A row's body at the top of its pitch, px. */
	int16_t    pressMin;       /**< Body that must show for a cut row to take a press, px. */
	int16_t    rows;           /**< Rows in the list; 0 for pixel content. */
	int        contentPx;      /**< Height of the content, px. */
	int        scrollPx;       /**< Content rows above the viewport, px. */
	UiListMode mode;           /**< The axis the gesture locked to. */
	bool       inside;         /**< The gesture began inside the viewport. */
	int16_t    candidate;      /**< Row under the press, or -1. */
	int        scrollAtPress;  /**< @ref UiList::scrollPx when the press landed. */
	int16_t    anchorX;        /**< Where the swipe's next coarse step counts from. */
};

/**
 * @brief Set up a list scrolled to 0 px, with no gesture under way.
 *
 * A screen calls it when it opens, and again when its content changes kind.
 *
 * @param[out] l The list.
 * @param view   The viewport.
 * @param rowH   Height of a row, px; at least 1.
 * @param rows   Rows in the list.
 */
void uiListInit(UiList *l, UiRect view, int rowH, int rows);

/**
 * @brief Give the rows a body shorter than their pitch and a press floor.
 *
 * After uiListInit(), which sets the body to the pitch and the floor to
 * 1 px: any visible part of a row takes a press.
 *
 * @param[in,out] l The list.
 * @param bodyH     Body height, px, 1 to the pitch.
 * @param pressMin  Body that must show for a cut row to take a press, px.
 */
void uiListSetBody(UiList *l, int bodyH, int pressMin);

/**
 * @brief Set up a list of @p contentPx of content and no rows, scrolled to
 *        0 px, with no gesture under way.
 * @param[out] l       The list.
 * @param view         The viewport.
 * @param contentPx    Height of the content, px.
 */
void uiListInitPx(UiList *l, UiRect view, int contentPx);

/**
 * @brief Largest legal scroll: the content's height less the viewport's, or 0
 *        when the content fits.
 * @param l The list.
 * @return Pixels.
 */
int uiListMaxScroll(const UiList *l);

/**
 * @brief Scroll to @p px, clamped to 0 .. uiListMaxScroll().
 * @param[in,out] l The list.
 * @param px        The scroll asked for.
 * @return The scroll that took effect.
 */
int uiListScrollTo(UiList *l, int px);

/**
 * @brief The row whose visible part holds the point (@p x, @p y): the press
 *        rule.
 * @param l   The list.
 * @param x,y A point on the panel.
 * @return The row, or -1 outside the viewport or below the last row.
 */
int uiListRowAt(const UiList *l, int x, int y);

/**
 * @brief A row's rectangle on the current frame, clipped to the viewport.
 * @param l   The list.
 * @param row The row.
 * @return The visible part of the row's body; its height is 0 when none of
 *         it shows.
 */
UiRect uiListRowRect(const UiList *l, int row);

struct UiControl;

/**
 * @brief Append `UI_ROW(n, Row)` for each row n with a visible part, at its
 *        rectangle on the current frame. A row cut below
 *        @ref UiList::pressMin is listed and does not act. @see ui_screen.h
 * @param l       The list.
 * @param out     Control table.
 * @param n       Controls in it; advanced per row added.
 * @param max     Room in @p out.
 * @param enabled Whether a tap on a whole row acts.
 */
void uiListAddRows(const UiList *l, UiControl *out, uint8_t *n, uint8_t max,
                   bool enabled);

/**
 * @brief Feed one frame of touch to the list.
 * @param[in,out] l The list.
 * @param t         This frame's touch; null is treated as no touch.
 * @return The event, @ref UiListEv::None for most frames.
 */
UiListEvent uiListTouch(UiList *l, const TouchState *t);

/**
 * @brief One of a row's own steps, for uiSwipeApply(). It takes the row, as
 *        the caller passed it, and a direction, -1 or +1, and returns true
 *        when it took the step; false, having changed nothing, when the step
 *        would pass a rail or change nothing.
 */
typedef bool (*UiListStepFn)(void *ctx, int dir);

/**
 * @brief Draws one row for uiListDraw(). It takes the row's index, the row's
 *        top edge, which may lie above or below the viewport, and the
 *        caller's context; the clip keeps the overhang off the rest of the
 *        screen.
 */
typedef void (*UiListDrawFn)(int index, int y, void *ctx);

/**
 * @brief Apply a swipe's coarse steps to a row: @p factor of the row's own
 *        steps per coarse step, taken one at a time while @p step allows
 *        them.
 * @param steps  Coarse steps, signed, from @ref UiListEv::Swipe.
 * @param factor The row's steps per coarse step.
 * @param step   Takes one of the row's steps.
 * @param ctx    Passed to @p step.
 * @return The row's steps taken; 0 when the row sits at the rail.
 */
int uiSwipeApply(int steps, int factor, UiListStepFn step, void *ctx);

/**
 * @brief Draw the list: clip to the viewport, clear it to @ref C_BG, draw
 *        each row that shows, in order, and lift the clip.
 * @param l       The list.
 * @param drawRow Draws one row.
 * @param ctx     Passed to @p drawRow.
 */
void uiListDraw(const UiList *l, UiListDrawFn drawRow, void *ctx);

/** @brief Width of the scroll bar, px. */
#define UI_LIST_BAR_W 3

/**
 * @brief Draw the scroll bar at @p x, @ref UI_LIST_BAR_W px wide, beside the
 *        viewport: its length the viewport's share of the content, its
 *        place the scroll's share of the range. Draws nothing while the
 *        content fits.
 * @param l The list.
 * @param x Left edge of the bar.
 */
void uiListDrawBar(const UiList *l, int x);
