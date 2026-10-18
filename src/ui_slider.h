/**
 * @file ui_slider.h
 * @brief A draggable value with presets. The servo tester's SPEED control.
 *
 * Adapted from rcbench shared/ui/ui_slider.c and include/ui_slider.h, commit
 * 453cf334, MIT. Kept: the event logic, which is the delta drag, tap-to-set,
 * the presets, and the two contracts about presses that do not end where they
 * began:
 *
 * - a press that starts on the track owns the value until it is released,
 *   wherever it moves, so a finger sliding off the track does not hand the
 *   value to whatever is underneath;
 * - a press that starts anywhere else never becomes a drag, however far it
 *   travels across the track.
 *
 * Changed: rcbench's touch events are this firmware's TouchState, one contact,
 * with `pressed` as the press, `down` without `pressed` as a move and
 * `released` as the release. The drawing is rewritten on gfx.h and the theme
 * colours. rcbench's scale ticks and its unused step are left out.
 *
 * A drag moves the value by as far as the finger moves, from the value the
 * press landed on, so a press commands nothing on its own. With tap-to-set
 * on, the press sets the value under it first and the drag measures from
 * there; that is for a control that commands nothing dangerous, as SPEED does.
 */

#pragma once

#include <stdint.h>

#include "ui_widgets.h"

struct TouchState;

/** @brief Most presets a slider holds. */
#define UI_SLIDER_MAX_PRESETS 8

/** @brief How far the thumb stands above and below the track, in pixels. */
#define UI_SLIDER_OVERHANG 3

/** @brief A slider. Zero it with uiSliderInit() before use. */
struct UiSlider {
	UiRect      track;                                /**< The trough, and the press target. */
	UiRect      presets[UI_SLIDER_MAX_PRESETS];       /**< Preset buttons. */
	float       presetValue[UI_SLIDER_MAX_PRESETS];   /**< What each preset sets. */
	const char *presetLabel[UI_SLIDER_MAX_PRESETS];   /**< Each preset's label. */
	int         presetCount;                          /**< Presets in use. */
	float       min;                                  /**< Value at the left end. */
	float       max;                                  /**< Value at the right end. */
	float       value;                                /**< The value, min..max. */
	bool        tapToSet;                             /**< A press sets the value under it. */
	bool        dim;                                  /**< Drawn dimmed; drawing only. */
	bool        dragging;                             /**< A press on the track owns the value. */
	int16_t     dragX;                                /**< Where the drag measures from. */
	float       dragValue;                            /**< The value it measures from. */
	int         pressedPreset;                        /**< Preset under the press, or -1. */
};

/**
 * @brief Set up a slider at @p min with no presets, tap-to-set off.
 * @param[out] s The slider.
 * @param track  The trough.
 * @param min    Value at the left end.
 * @param max    Value at the right end.
 */
void uiSliderInit(UiSlider *s, UiRect track, float min, float max);

/**
 * @brief Lay presets out in a row inside @p row, with 4 px gaps.
 * @param[in,out] s The slider.
 * @param values    What each preset sets.
 * @param labels    Each preset's label.
 * @param count     How many; above @ref UI_SLIDER_MAX_PRESETS counts as that.
 * @param row       The row they share.
 */
void uiSliderSetPresets(UiSlider *s, const float *values,
                        const char *const *labels, int count, UiRect row);

/**
 * @brief Set the value, clamped to min..max. A drag under way measures from
 *        the new value, so a value set from outside is not undone by the next
 *        move.
 * @param[in,out] s The slider.
 * @param value     The value.
 */
void uiSliderSet(UiSlider *s, float value);

/**
 * @brief Let a press set the value under it. Off by default.
 * @param[in,out] s The slider.
 * @param on        True to let a press set the value.
 */
void uiSliderSetTapToSet(UiSlider *s, bool on);

/**
 * @brief Abandon a drag and a pressed preset that will get no release, for a
 *        screen that is no longer the one under the finger.
 * @param[in,out] s The slider.
 */
void uiSliderRelease(UiSlider *s);

/**
 * @brief Feed one frame of touch to the slider.
 * @param[in,out] s The slider.
 * @param t         This frame's touch; null changes nothing.
 * @return True when the value changed.
 */
bool uiSliderEvent(UiSlider *s, const TouchState *t);

/**
 * @brief Every pixel uiSliderDraw() may touch: the track, the thumb's
 *        overhang and the presets. A caller clears this before drawing.
 * @param s The slider.
 * @return The rectangle.
 */
UiRect uiSliderPaintedRect(const UiSlider *s);

/**
 * @brief Draw the trough, the fill to the value, the thumb and the presets.
 * @param s    The slider.
 * @param fill Colour of the filled part; ignored while dimmed.
 */
void uiSliderDraw(const UiSlider *s, uint16_t fill);
