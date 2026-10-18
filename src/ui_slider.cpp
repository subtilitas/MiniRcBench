/**
 * @file ui_slider.cpp
 * @brief The slider. @see ui_slider.h
 *
 * Adapted from rcbench shared/ui/ui_slider.c, commit 453cf334, MIT.
 */

#include "ui_slider.h"
#include "gfx.h"
#include "touch.h"

#include <string.h>

/** @brief Width of the thumb, in pixels. */
#define THUMB_W 14

static float clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static bool contains(const UiRect &r, int x, int y) {
	return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void uiSliderInit(UiSlider *s, UiRect track, float min, float max) {
	memset(s, 0, sizeof(*s));
	s->track = track;
	s->min = min;
	s->max = max;
	s->value = min;
	s->pressedPreset = -1;
}

void uiSliderSetPresets(UiSlider *s, const float *values,
                        const char *const *labels, int count, UiRect row) {
	if (count > UI_SLIDER_MAX_PRESETS) count = UI_SLIDER_MAX_PRESETS;
	if (count < 0) count = 0;
	s->presetCount = count;
	if (count == 0) return;
	const int gap = 4;
	int w = (row.w - (count - 1) * gap) / count;
	for (int i = 0; i < count; i++) {
		s->presetValue[i] = values[i];
		s->presetLabel[i] = labels[i];
		s->presets[i] = UiRect{(int16_t)(row.x + i * (w + gap)), row.y,
		                       (int16_t)w, row.h};
	}
}

void uiSliderSet(UiSlider *s, float value) {
	s->value = clampf(value, s->min, s->max);
	// The drag measures from where the press landed, so a value set from
	// outside during a drag would be undone by the very next move.
	if (s->dragging) s->dragValue = s->value;
}

void uiSliderSetTapToSet(UiSlider *s, bool on) { s->tapToSet = on; }

void uiSliderRelease(UiSlider *s) {
	s->dragging = false;
	s->pressedPreset = -1;
}

/**
 * @brief The value the drag started from, moved by as far as the finger has
 *        moved. Not the value under the finger: a press commands nothing.
 */
static bool byDelta(UiSlider *s, int x) {
	if (s->track.w <= 1) return false;
	float perPx = (s->max - s->min) / (float)(s->track.w - 1);
	float was = s->value;
	s->value = clampf(s->dragValue + (float)(x - s->dragX) * perPx, s->min, s->max);
	return s->value != was;
}

bool uiSliderEvent(UiSlider *s, const TouchState *t) {
	if (!t) return false;
	int x = t->x, y = t->y;

	if (t->pressed) {
		// Any press ends a drag still latched: a release that went missing
		// would otherwise leave an origin from an old gesture, and the delta
		// model would apply the new distance to it.
		uiSliderRelease(s);
		if (contains(s->track, x, y)) {
			s->dragging = true;
			s->dragX = (int16_t)x;
			s->dragValue = s->value;
			// A press alone commands nothing, and a track 1 px wide has no
			// position to read.
			if (!s->tapToSet || s->track.w <= 1) return false;
			float frac = clampf((float)(x - s->track.x) / (float)(s->track.w - 1),
			                    0.0f, 1.0f);
			float was = s->value;
			s->value = s->min + frac * (s->max - s->min);
			s->dragValue = s->value;
			return s->value != was;
		}
		for (int i = 0; i < s->presetCount; i++) {
			if (contains(s->presets[i], x, y)) {
				s->pressedPreset = i;
				return false;
			}
		}
		return false;
	}

	if (s->dragging) {
		// Deliberately not requiring the finger to stay on the track: a press
		// that began there owns the value until it lets go.
		if (t->released) s->dragging = false;
		if (t->down || t->released) return byDelta(s, x);
		return false;
	}

	if (s->pressedPreset >= 0 && t->released) {
		int i = s->pressedPreset;
		s->pressedPreset = -1;
		// A press that slid off its preset is not a tap on it.
		if (contains(s->presets[i], x, y)) {
			float was = s->value;
			s->value = clampf(s->presetValue[i], s->min, s->max);
			return s->value != was;
		}
	}
	return false;
}

UiRect uiSliderPaintedRect(const UiSlider *s) {
	int x0 = s->track.x, y0 = s->track.y - UI_SLIDER_OVERHANG;
	int x1 = s->track.x + s->track.w;
	int y1 = s->track.y + s->track.h + UI_SLIDER_OVERHANG;
	for (int i = 0; i < s->presetCount; i++) {
		const UiRect &p = s->presets[i];
		if (p.x < x0) x0 = p.x;
		if (p.y < y0) y0 = p.y;
		if (p.x + p.w > x1) x1 = p.x + p.w;
		if (p.y + p.h > y1) y1 = p.y + p.h;
	}
	return UiRect{(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
}

void uiSliderDraw(const UiSlider *s, uint16_t fill) {
	const UiRect &r = s->track;
	float span = s->max - s->min;
	float frac = span > 0.0f ? (s->value - s->min) / span : 0.0f;
	int rad = r.h / 2;

	gfxRoundRect(r.x, r.y, r.w, r.h, rad, C_PANEL);
	gfxRoundFrame(r.x, r.y, r.w, r.h, rad, C_GRID);

	uint16_t fillc = s->dim ? C_GRID : fill;
	int filled = (int)(frac * (float)r.w + 0.5f);
	// A rounded fill narrower than its own height draws as a lens, so the
	// fill is at least one track height wide.
	if (filled > 0 && filled < r.h) filled = r.h;
	if (filled > 0) gfxRoundRect(r.x, r.y, filled, r.h, rad, fillc);

	// The thumb stands UI_SLIDER_OVERHANG px proud of the track and stays
	// over it at both ends.
	int hx = r.x + (int)(frac * (float)r.w + 0.5f);
	if (hx < r.x + THUMB_W / 2) hx = r.x + THUMB_W / 2;
	if (hx > r.x + r.w - THUMB_W / 2) hx = r.x + r.w - THUMB_W / 2;
	int tx = hx - THUMB_W / 2, ty = r.y - UI_SLIDER_OVERHANG;
	int th = r.h + 2 * UI_SLIDER_OVERHANG;
	uint16_t thumb = s->dim ? C_DIM : C_TEXT;
	gfxRoundRect(tx, ty, THUMB_W, th, 4, thumb);
	gfxRoundFrame(tx, ty, THUMB_W, th, 4, C_GRID);
	// Two grip lines, so the thumb reads as a control and not a marker.
	gfxVLine(hx - 2, ty + th / 2 - 5, 11, C_GRID);
	gfxVLine(hx + 2, ty + th / 2 - 5, 11, C_GRID);

	for (int i = 0; i < s->presetCount; i++)
		uiButton(s->presets[i], s->presetLabel[i], C_PANEL, C_TEXT, 1,
		         s->pressedPreset == i, true);
}
