/**
 * @file ui_list.cpp
 * @brief The scrolling list and its gesture. @see ui_list.h
 */

#include "ui_list.h"
#include "ui_screen.h"
#include "gfx.h"
#include "touch.h"

static bool inView(const UiList *l, int x, int y) {
	return x >= l->view.x && x < l->view.x + l->view.w &&
	       y >= l->view.y && y < l->view.y + l->view.h;
}

static int absOf(int v) { return v < 0 ? -v : v; }

void uiListInit(UiList *l, UiRect view, int rowH, int rows) {
	l->view = view;
	l->rowH = (int16_t)(rowH > 0 ? rowH : 1);
	l->bodyH = l->rowH;
	l->pressMin = 1;
	l->rows = (int16_t)(rows > 0 ? rows : 0);
	l->contentPx = (int)l->rows * l->rowH;
	l->scrollPx = 0;
	l->mode = UiListMode::None;
	l->inside = false;
	l->candidate = -1;
	l->scrollAtPress = 0;
	l->anchorX = 0;
}

void uiListSetBody(UiList *l, int bodyH, int pressMin) {
	l->bodyH = (int16_t)(bodyH < 1 ? 1 : (bodyH > l->rowH ? l->rowH : bodyH));
	l->pressMin = (int16_t)(pressMin < 1 ? 1 : pressMin);
}

void uiListInitPx(UiList *l, UiRect view, int contentPx) {
	uiListInit(l, view, 1, 0);
	l->contentPx = contentPx > 0 ? contentPx : 0;
}

int uiListMaxScroll(const UiList *l) {
	int over = l->contentPx - l->view.h;
	return over > 0 ? over : 0;
}

int uiListScrollTo(UiList *l, int px) {
	int max = uiListMaxScroll(l);
	l->scrollPx = px < 0 ? 0 : (px > max ? max : px);
	return l->scrollPx;
}

int uiListRowAt(const UiList *l, int x, int y) {
	if (!inView(l, x, y)) return -1;
	int at = l->scrollPx + y - l->view.y;
	int row = at / l->rowH;
	if (row >= l->rows || at % l->rowH >= l->bodyH) return -1;
	return uiListRowRect(l, row).h >= l->pressMin ? row : -1;
}

UiRect uiListRowRect(const UiList *l, int row) {
	int top = l->view.y + row * l->rowH - l->scrollPx;
	int bot = top + l->bodyH;
	int vBot = l->view.y + l->view.h;
	if (row < 0 || row >= l->rows) bot = top;
	if (top < l->view.y) top = l->view.y;
	if (bot > vBot) bot = vBot;
	if (bot < top) bot = top;
	return UiRect{l->view.x, (int16_t)top, l->view.w, (int16_t)(bot - top)};
}

void uiListAddRows(const UiList *l, UiControl *out, uint8_t *n, uint8_t max,
                   bool enabled) {
	for (int r = 0; r < l->rows; r++) {
		UiRect rr = uiListRowRect(l, r);
		if (rr.h > 0)
			uiControlAdd(out, n, max, UI_ROW(r, Row), rr, enabled && rr.h >= l->pressMin);
	}
}

UiListEvent uiListTouch(UiList *l, const TouchState *t) {
	UiListEvent ev = {UiListEv::None, -1, 0};
	if (!t) return ev;

	if (t->pressed) {
		l->mode = UiListMode::None;
		l->inside = inView(l, t->x, t->y);
		l->candidate = (int16_t)uiListRowAt(l, t->x, t->y);
		l->scrollAtPress = l->scrollPx;
		l->anchorX = t->x;
		return ev;
	}
	if (t->released) {
		// A press that slid off its row cancels, as on every button.
		if (l->mode == UiListMode::None && l->candidate >= 0 &&
		    uiListRowAt(l, t->x, t->y) == l->candidate) {
			ev.kind = UiListEv::Tap;
			ev.row = l->candidate;
		}
		l->inside = false;
		l->candidate = -1;
		return ev;
	}
	if (!t->down || !l->inside) return ev;

	int dx = t->x - t->downX, dy = t->y - t->downY;
	if (l->mode == UiListMode::None) {
		if (absOf(dx) > UI_AXIS_LOCK_PX && absOf(dx) >= absOf(dy)) {
			// The next coarse step counts from here, so the travel spent
			// deciding the axis does not count as a step.
			l->mode = UiListMode::Sideways;
			l->anchorX = t->x;
			if (l->candidate < 0) return ev;
			ev.kind = UiListEv::Swipe;
			ev.row = l->candidate;
			return ev;
		}
		if (absOf(dy) <= UI_AXIS_LOCK_PX) return ev;
		l->mode = UiListMode::Vertical;
	}

	if (l->mode == UiListMode::Vertical) {
		// From the press point, so the travel spent deciding the axis moves
		// the list too. Dragging down shows the rows above: the scroll falls.
		int before = l->scrollPx;
		if (uiListScrollTo(l, l->scrollAtPress - dy) != before) ev.kind = UiListEv::Scrolled;
		return ev;
	}

	if (l->candidate < 0) return ev;
	int steps = (t->x - l->anchorX) / UI_SWIPE_STEP_PX;
	if (steps == 0) return ev;
	l->anchorX = (int16_t)(l->anchorX + steps * UI_SWIPE_STEP_PX);
	ev.kind = UiListEv::Swipe;
	ev.row = l->candidate;
	ev.steps = steps;
	return ev;
}

int uiSwipeApply(int steps, int factor, UiListStepFn step, void *ctx) {
	int dir = steps < 0 ? -1 : +1;
	int want = absOf(steps) * factor;
	int taken = 0;
	while (taken < want && step(ctx, dir)) taken++;
	return taken;
}

void uiListDraw(const UiList *l, UiListDrawFn drawRow, void *ctx) {
	const UiRect &v = l->view;
	gfxSetClip(v.x, v.y, v.w, v.h);
	gfxRect(v.x, v.y, v.w, v.h, C_BG);
	for (int i = l->scrollPx / l->rowH; i < l->rows; i++) {
		int y = v.y + i * l->rowH - l->scrollPx;
		if (y >= v.y + v.h) break;
		drawRow(i, y, ctx);
	}
	gfxClearClip();
}

void uiListDrawBar(const UiList *l, int x) {
	int max = uiListMaxScroll(l);
	if (max <= 0 || l->contentPx <= 0) return;
	const UiRect &v = l->view;
	int len = v.h * v.h / l->contentPx;
	if (len < 8) len = 8;
	int top = v.y + (v.h - len) * l->scrollPx / max;
	gfxRect(x, v.y, UI_LIST_BAR_W, v.h, C_BG);
	gfxRect(x, top, UI_LIST_BAR_W, len, C_GRID);
}
