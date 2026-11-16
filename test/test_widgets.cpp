/**
 * @file test_widgets.cpp
 * @brief The shared widgets, driven directly with touch snapshots:
 *        src/ui_list.cpp, src/ui_hold.cpp, src/ui_leave.cpp and
 *        src/ui_widgets.cpp as shipped.
 *
 * The screens that use them, ESC SETTINGS, SERVO, SERVO SET and SETUP, are
 * tested through the UI in test_ui.cpp and test_servo.cpp. This file holds
 * each rule at its boundary.
 */

#include "check.h"
#include "config.h"
#include "fakes.h"
#include "gfx.h"
#include "touch.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_leave.h"
#include "ui_list.h"
#include "ui_widgets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// touch snapshots
// ---------------------------------------------------------------------------

/** @brief The press under way, as the driver reports it. */
static TouchState s_touch;

/** @brief The frame a finger lands at (@p x, @p y). */
static const TouchState *press(int x, int y) {
	memset(&s_touch, 0, sizeof(s_touch));
	s_touch.down = s_touch.pressed = true;
	s_touch.x = s_touch.downX = (int16_t)x;
	s_touch.y = s_touch.downY = (int16_t)y;
	return &s_touch;
}

/** @brief A frame with the finger at (@p x, @p y), still down. */
static const TouchState *move(int x, int y) {
	s_touch.pressed = s_touch.released = false;
	s_touch.down = true;
	s_touch.x = (int16_t)x;
	s_touch.y = (int16_t)y;
	return &s_touch;
}

/** @brief The frame the finger lifts, where it last was. */
static const TouchState *lift() {
	s_touch.pressed = false;
	s_touch.down = false;
	s_touch.released = true;
	return &s_touch;
}

/** @brief A frame with no finger and no edge. */
static const TouchState *idle() {
	s_touch.pressed = s_touch.released = s_touch.down = false;
	return &s_touch;
}

static bool isEv(const UiListEvent &e, UiListEv kind, int row = -1, int steps = 0) {
	return e.kind == kind && e.row == row && e.steps == steps;
}

// ---------------------------------------------------------------------------
// the list
// ---------------------------------------------------------------------------

/** @brief Today's AM32 and SERVO SET viewport: y 56 to 215, 26 px rows. */
static const UiRect VIEW = {0, 56, 240, 160};

/** @brief Twelve rows of 26 px: 312 px of content, 152 px of scroll. */
static void list12(UiList *l) { uiListInit(l, VIEW, 26, 12); }

static void testListGeometry() {
	section("List: geometry, scroll range and clamp");

	UiList l;
	list12(&l);
	checkInt("a new list is scrolled to 0 px", l.scrollPx, 0);
	checkInt("scroll range: content less viewport", uiListMaxScroll(&l), 12 * 26 - 160);
	checkInt("scroll to -1 px clamps to 0", uiListScrollTo(&l, -1), 0);
	checkInt("scroll to the end stays there", uiListScrollTo(&l, 152), 152);
	checkInt("scroll 1 px past the end clamps to it", uiListScrollTo(&l, 153), 152);
	checkInt("and the list holds the clamped value", l.scrollPx, 152);
	uiListInit(&l, VIEW, 26, 6);
	checkInt("six rows fit 160 px: no scroll", uiListMaxScroll(&l), 0);
	checkInt("and any scroll clamps to 0", uiListScrollTo(&l, 40), 0);
	uiListInit(&l, VIEW, 0, -3);
	checkTrue("a row height under 1 px reads as 1, rows under 0 as 0",
	          l.rowH == 1 && l.rows == 0 && uiListMaxScroll(&l) == 0);
	uiListInit(&l, VIEW, 26, 0);
	checkInt("an empty list takes no press", uiListRowAt(&l, 100, 100), -1);
}

static void testListPressRule() {
	section("List: a press takes the row whose visible part it is on");

	UiList l;
	list12(&l);
	uiListScrollTo(&l, 25);   // row 0 shows 1 px, at y 56
	checkInt("top edge, y 56: row 0 by its last visible pixel", uiListRowAt(&l, 100, 56), 0);
	checkInt("y 55, above the viewport: no row", uiListRowAt(&l, 100, 55), -1);
	checkInt("y 57: row 1", uiListRowAt(&l, 100, 57), 1);
	uiListScrollTo(&l, 0);    // row 6 shows 4 px, y 212 to 215
	checkInt("bottom edge, y 215: row 6 by a 4 px sliver", uiListRowAt(&l, 100, 215), 6);
	checkInt("y 211: row 5", uiListRowAt(&l, 100, 211), 5);
	checkInt("y 212: row 6", uiListRowAt(&l, 100, 212), 6);
	checkInt("y 216, below the viewport: no row", uiListRowAt(&l, 100, 216), -1);
	checkTrue("x 0 and x 239 are inside: row 1 at y 100",
	          uiListRowAt(&l, 0, 100) == 1 && uiListRowAt(&l, 239, 100) == 1);
	checkInt("x 240 is outside", uiListRowAt(&l, 240, 100), -1);
	UiList n;
	uiListInit(&n, UiRect{20, 56, 100, 160}, 26, 12);
	checkInt("a viewport from x 20: x 19 is outside", uiListRowAt(&n, 19, 100), -1);
	checkInt("x 20 is inside", uiListRowAt(&n, 20, 100), 1);
	checkInt("x 119 is inside, x 120 outside",
	         uiListRowAt(&n, 119, 100) * 10 + (uiListRowAt(&n, 120, 100) + 1), 10);
	uiListInit(&n, VIEW, 26, 3);
	checkInt("below the last row of a short list: no row", uiListRowAt(&n, 100, 56 + 78), -1);
	checkInt("its last pixel row: row 2", uiListRowAt(&n, 100, 56 + 77), 2);
}

static void testListRowRect() {
	section("List: a row's rectangle, clipped to the viewport");

	UiList l;
	list12(&l);
	uiListScrollTo(&l, 13);
	UiRect r = uiListRowRect(&l, 0);
	checkTrue("row 0 cut at the top: y 56, 13 px", r.x == 0 && r.y == 56 && r.w == 240 && r.h == 13);
	r = uiListRowRect(&l, 1);
	checkTrue("row 1 whole: y 69, 26 px", r.y == 69 && r.h == 26);
	r = uiListRowRect(&l, 6);
	checkTrue("row 6 cut at the bottom: y 199, 17 px", r.y == 199 && r.h == 17);
	checkInt("row 7 does not show", uiListRowRect(&l, 7).h, 0);
	checkInt("nor does row -1", uiListRowRect(&l, -1).h, 0);
	checkInt("nor row 12, past the end", uiListRowRect(&l, 12).h, 0);
	// A row past the end of a short list lies inside the viewport, so only
	// the row check keeps it from having a rectangle.
	UiList s;
	uiListInit(&s, VIEW, 26, 3);
	checkInt("row 3 of a 3-row list does not exist", uiListRowRect(&s, 3).h, 0);
	checkInt("nor does row -1 at scroll 0", uiListRowRect(&s, -1).h, 0);
	uiListScrollTo(&l, 30);
	checkInt("row 0 scrolled off the top does not show", uiListRowRect(&l, 0).h, 0);
	// The press rule and the rectangle agree on every pixel row of the
	// viewport, at a scroll that cuts a row at each edge.
	uiListScrollTo(&l, 13);
	bool agree = true;
	for (int y = VIEW.y; y < VIEW.y + VIEW.h; y++) {
		int row = uiListRowAt(&l, 100, y);
		UiRect rr = uiListRowRect(&l, row);
		if (row < 0 || y < rr.y || y >= rr.y + rr.h) agree = false;
	}
	checkTrue("each pixel row is in the rectangle of the row it presses", agree);
}

static void testListTap() {
	section("List: a tap fires on release, and a slide off cancels it");

	UiList l;
	list12(&l);
	checkTrue("the press commits nothing", isEv(uiListTouch(&l, press(100, 100)), UiListEv::None));
	checkTrue("6 px of travel: still a candidate", isEv(uiListTouch(&l, move(106, 106)), UiListEv::None));
	checkTrue("released inside: a tap on the row pressed",
	          isEv(uiListTouch(&l, lift()), UiListEv::Tap, (100 - 56) / 26));
	checkTrue("the frame after: nothing", isEv(uiListTouch(&l, idle()), UiListEv::None));

	// A row boundary crossed without locking: the press slid off its row.
	uiListTouch(&l, press(100, 56 + 25));
	uiListTouch(&l, move(100, 56 + 29));
	checkTrue("4 px across a row edge: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));
	uiListTouch(&l, press(100, 56 + 20));
	uiListTouch(&l, move(100, 56 + 24));
	checkTrue("4 px within the row: a tap on it", isEv(uiListTouch(&l, lift()), UiListEv::Tap, 0));

	uiListTouch(&l, press(100, 213));
	uiListTouch(&l, move(100, 216));
	checkTrue("released 3 px below the viewport: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));
	uiListTouch(&l, press(100, 58));
	uiListTouch(&l, move(100, 55));
	checkTrue("released above it: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));
	uiListTouch(&l, press(100, 230));
	uiListTouch(&l, move(100, 210));
	checkTrue("a press below the viewport, released inside: no tap",
	          isEv(uiListTouch(&l, lift()), UiListEv::None));
	uiListTouch(&l, press(100, 100));
	uiListTouch(&l, move(100, 107));
	checkTrue("released after a 7 px drag: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));
	uiListTouch(&l, press(100, 100));
	uiListTouch(&l, move(107, 100));
	checkTrue("released after a 7 px swipe: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));
	checkTrue("no touch at all: nothing", isEv(uiListTouch(&l, nullptr), UiListEv::None));
}

static void testListScroll() {
	section("List: the axis lock at 6 px, and a scroll 1:1 from the press");

	UiList l;
	list12(&l);
	uiListScrollTo(&l, 50);
	uiListTouch(&l, press(100, 150));
	checkTrue("6 px up: no lock, no scroll",
	          isEv(uiListTouch(&l, move(100, 144)), UiListEv::None) && l.scrollPx == 50);
	checkTrue("7 px up: scrolled", isEv(uiListTouch(&l, move(100, 143)), UiListEv::Scrolled));
	checkInt("by all 7 px, from the press point", l.scrollPx, 57);
	checkTrue("sideways travel after the lock: still a scroll",
	          isEv(uiListTouch(&l, move(130, 140)), UiListEv::Scrolled) && l.scrollPx == 60);
	checkTrue("a frame that moves nothing: no event", isEv(uiListTouch(&l, move(130, 140)), UiListEv::None));
	checkTrue("down past the press: the scroll falls", isEv(uiListTouch(&l, move(130, 170)), UiListEv::Scrolled) &&
	          l.scrollPx == 30);
	uiListTouch(&l, move(100, 260));
	checkInt("clamped at 0 px", l.scrollPx, 0);
	checkTrue("further down at the top: no event", isEv(uiListTouch(&l, move(100, 300)), UiListEv::None));
	uiListTouch(&l, move(100, -150));
	checkInt("up past the end: clamped at 152 px", l.scrollPx, 152);
	checkTrue("released after the drag: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));

	uiListScrollTo(&l, 50);
	uiListTouch(&l, press(100, 230));
	uiListTouch(&l, move(100, 100));
	checkInt("a press that began below the viewport does not scroll", l.scrollPx, 50);
	uiListTouch(&l, lift());
	uiListTouch(&l, press(100, 30));
	uiListTouch(&l, move(100, 100));
	checkInt("nor one that began above it", l.scrollPx, 50);
	uiListTouch(&l, lift());
	uiListTouch(&l, press(100, 150));
	uiListTouch(&l, move(100, 160));
	uiListInit(&l, VIEW, 26, 12);
	checkTrue("a list set up again mid-drag ignores the rest of it",
	          isEv(uiListTouch(&l, move(100, 190)), UiListEv::None) && l.scrollPx == 0);
	uiListTouch(&l, lift());

	// A finger down before the list is set up: no press, no gesture.
	uiListInit(&l, VIEW, 26, 12);
	move(100, 100);
	s_touch.downX = 100; s_touch.downY = 150;
	checkTrue("a finger down with no press on this list: nothing",
	          isEv(uiListTouch(&l, &s_touch), UiListEv::None) && l.scrollPx == 0);
	uiListTouch(&l, lift());
}

static void testListSwipe() {
	section("List: the sideways swipe, 16 px per coarse step");

	UiList l;
	list12(&l);
	int row = (100 - 56) / 26;
	uiListTouch(&l, press(100, 100));
	checkTrue("6 px sideways: no lock", isEv(uiListTouch(&l, move(106, 100)), UiListEv::None));
	checkTrue("7 px: locks, the row with 0 steps",
	          isEv(uiListTouch(&l, move(107, 100)), UiListEv::Swipe, row, 0));
	checkTrue("15 px past the lock: no step", isEv(uiListTouch(&l, move(122, 100)), UiListEv::None));
	checkTrue("16 px: one coarse step", isEv(uiListTouch(&l, move(123, 100)), UiListEv::Swipe, row, 1));
	checkTrue("15 px further: none", isEv(uiListTouch(&l, move(138, 100)), UiListEv::None));
	checkTrue("16 px further: one more", isEv(uiListTouch(&l, move(139, 100)), UiListEv::Swipe, row, 1));
	checkTrue("48 px in one frame: three", isEv(uiListTouch(&l, move(187, 100)), UiListEv::Swipe, row, 3));
	checkTrue("vertical travel after the lock scrolls nothing",
	          isEv(uiListTouch(&l, move(187, 30)), UiListEv::None) && l.scrollPx == 0);
	checkTrue("15 px back: none", isEv(uiListTouch(&l, move(172, 100)), UiListEv::None));
	checkTrue("16 px back: one step down", isEv(uiListTouch(&l, move(171, 100)), UiListEv::Swipe, row, -1));
	checkTrue("released: no tap", isEv(uiListTouch(&l, lift()), UiListEv::None));

	uiListTouch(&l, press(100, 100));
	checkTrue("7 px left locks too", isEv(uiListTouch(&l, move(93, 100)), UiListEv::Swipe, row, 0));
	checkTrue("16 px left: one step down", isEv(uiListTouch(&l, move(77, 100)), UiListEv::Swipe, row, -1));
	uiListTouch(&l, lift());

	uiListTouch(&l, press(100, 100));
	checkTrue("7 px sideways and 7 px down: sideways wins the tie",
	          isEv(uiListTouch(&l, move(107, 107)), UiListEv::Swipe, row, 0));
	uiListTouch(&l, lift());
	uiListScrollTo(&l, 50);
	uiListTouch(&l, press(100, 100));
	checkTrue("7 px sideways and 8 px down: a scroll",
	          isEv(uiListTouch(&l, move(107, 108)), UiListEv::Scrolled));
	uiListTouch(&l, lift());

	uiListInit(&l, VIEW, 26, 3);
	uiListTouch(&l, press(100, 200));
	checkTrue("a swipe below the last row: locks, no event",
	          isEv(uiListTouch(&l, move(110, 200)), UiListEv::None));
	checkTrue("and steps nothing", isEv(uiListTouch(&l, move(150, 200)), UiListEv::None));
	uiListTouch(&l, lift());
}

/** @brief A row for uiSwipeApply(): a value with rails and a step. */
struct FakeRow {
	int value, lo, hi, step;
	int calls;
};

/** @brief One step of a FakeRow, refused, changing nothing, past a rail. */
static bool fakeStep(void *ctx, int dir) {
	FakeRow *r = (FakeRow *)ctx;
	r->calls++;
	int n = r->value + dir * r->step;
	if (n < r->lo || n > r->hi) return false;
	r->value = n;
	return true;
}

static void testSwipeApply() {
	section("List: a coarse step is the row's factor, and stops at a rail");

	FakeRow r = {10, 0, 31, 2, 0};
	checkInt("one coarse step, factor 3: 3 steps taken", uiSwipeApply(1, 3, fakeStep, &r), 3);
	checkInt("of the row's step, 2: +6", r.value, 16);
	checkInt("two coarse steps down: 6 steps", uiSwipeApply(-2, 3, fakeStep, &r), 6);
	checkInt("-12", r.value, 4);
	checkInt("five up would pass 31: 13 steps", uiSwipeApply(5, 3, fakeStep, &r), 13);
	checkInt("stops at 30, the rail on the row's step", r.value, 30);
	r.calls = 0;
	checkInt("at the rail: none taken", uiSwipeApply(1, 3, fakeStep, &r), 0);
	checkInt("after one refused step", r.calls, 1);
	checkInt("down to the other rail", uiSwipeApply(-20, 3, fakeStep, &r), 15);
	checkInt("at 0", r.value, 0);
	checkInt("factor 0: nothing", uiSwipeApply(5, 0, fakeStep, &r), 0);
	checkInt("0 coarse steps: nothing", uiSwipeApply(0, 3, fakeStep, &r), 0);

	// The swipe end to end: past the rail, then back. The anchor moves on by
	// whole coarse steps while the value sits at the rail, so the way back
	// starts at the next coarse step back.
	UiList l;
	list12(&l);
	r = FakeRow{20, 0, 30, 1, 0};
	uiListTouch(&l, press(40, 100));
	uiListTouch(&l, move(47, 100));
	for (int x = 47; x <= 47 + 10 * 16; x += 8) {
		UiListEvent e = uiListTouch(&l, move(x, 100));
		if (e.kind == UiListEv::Swipe && e.steps) uiSwipeApply(e.steps, 5, fakeStep, &r);
	}
	checkInt("ten coarse steps of 5 from 20: at the rail, 30", r.value, 30);
	UiListEvent e = uiListTouch(&l, move(47 + 9 * 16 + 1, 100));
	checkTrue("15 px back past the last step: nothing", isEv(e, UiListEv::None));
	e = uiListTouch(&l, move(47 + 9 * 16, 100));
	checkTrue("16 px back: one coarse step down", isEv(e, UiListEv::Swipe, 1, -1));
	uiSwipeApply(e.steps, 5, fakeStep, &r);
	checkInt("off the rail by one coarse step: 25", r.value, 25);
	uiListTouch(&l, lift());
}

/** @brief Which rows uiListDraw() drew, and where. */
static int s_drawn[16], s_drawnY[16], s_drawCount;

static void recordRow(int index, int y, void *ctx) {
	if (s_drawCount < 16) { s_drawn[s_drawCount] = index; s_drawnY[s_drawCount] = y; }
	s_drawCount++;
	gfxRect(0, y, 240, 26, *(const uint16_t *)ctx);
}

static uint16_t px(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

static void testListDraw() {
	section("List: drawing clips to the viewport");

	UiList l;
	list12(&l);
	uiListScrollTo(&l, 13);
	gfxFill(C_RED);
	uint16_t ink = C_LIME;
	s_drawCount = 0;
	uiListDraw(&l, recordRow, &ink);
	checkInt("rows 0 to 6 drawn, at a scroll that cuts 0 and 6", s_drawCount, 7);
	checkTrue("in order, from row 0 at y 43",
	          s_drawn[0] == 0 && s_drawnY[0] == 43 && s_drawn[6] == 6 && s_drawnY[6] == 199);
	checkTrue("y 55, above the viewport, untouched", px(10, 55) == C_RED);
	checkTrue("y 216, below it, untouched", px(10, 216) == C_RED);
	checkTrue("y 56 and y 215 drawn", px(10, 56) == C_LIME && px(10, 215) == C_LIME);
	uiListScrollTo(&l, 22);   // row 7's top is y 216, the viewport's end
	s_drawCount = 0;
	uiListDraw(&l, recordRow, &ink);
	checkTrue("a row whose top is the viewport's end is not drawn",
	          s_drawCount == 7 && s_drawn[6] == 6);
	uiListScrollTo(&l, 30);   // row 0 ends at y 51, above the viewport
	s_drawCount = 0;
	uiListDraw(&l, recordRow, &ink);
	checkTrue("a row wholly above the viewport is not drawn: from row 1",
	          s_drawCount == 7 && s_drawn[0] == 1 && s_drawnY[0] == 52);
	uiListInit(&l, VIEW, 26, 3);
	gfxFill(C_RED);
	s_drawCount = 0;
	uiListDraw(&l, recordRow, &ink);
	checkInt("a short list draws its 3 rows", s_drawCount, 3);
	checkTrue("and clears the rest of the viewport", px(10, 56 + 78) == C_BG && px(10, 215) == C_BG);
	gfxRect(0, 20, 10, 10, C_LIME);
	checkTrue("the clip is lifted afterwards", px(5, 25) == C_LIME);
	gfxFill(C_BG);
}

// ---------------------------------------------------------------------------
// the hold
// ---------------------------------------------------------------------------

static const UiRect CTRL = {20, 200, 100, 40};

static void testHoldTiming() {
	section("Hold: fires once at 1000 ms, not at 999");

	checkInt("UI_HOLD_MS is the arm hold, 1000 ms", UI_HOLD_MS, 1000);
	checkInt("ARM_HOLD_MS", ARM_HOLD_MS, 1000);
	UiHold h;
	uiHoldReset(&h);
	checkTrue("the press frame starts the timer",
	          !uiHoldUpdate(&h, CTRL, press(50, 210), true, 5000) && h.holding && h.startMs == 5000);
	checkTrue("999 ms: not fired", !uiHoldUpdate(&h, CTRL, move(50, 210), true, 5999) && h.holding);
	checkTrue("1000 ms: fired", uiHoldUpdate(&h, CTRL, move(50, 210), true, 6000));
	checkTrue("and stopped timing", !h.holding);
	bool again = false;
	for (uint32_t t = 6025; t <= 9000; t += 25) again |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("held 3000 ms more: does not fire again", !again && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 9025);
	uiHoldUpdate(&h, CTRL, idle(), true, 9050);
	checkTrue("a new press times again", !uiHoldUpdate(&h, CTRL, press(50, 210), true, 9100) && h.holding);
	checkTrue("and fires 1000 ms after it", uiHoldUpdate(&h, CTRL, move(50, 210), true, 10100));
	// A new press is timed even with no frame between the lift and the press.
	uiHoldUpdate(&h, CTRL, move(50, 210), true, 10125);
	TouchState t2 = *press(50, 210);
	t2.released = true;
	checkTrue("a press on the frame of a lift: timed", !uiHoldUpdate(&h, CTRL, &t2, true, 10150) && h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 10175);
	// A press whose first frame the hold did not see is never timed.
	uiHoldUpdate(&h, CTRL, press(50, 210), true, 11000);
	uiHoldUpdate(&h, CTRL, move(50, 210), true, 12000);
	uiHoldUpdate(&h, CTRL, lift(), true, 12025);
	press(50, 210);
	bool fired = false;
	for (uint32_t t = 12050; t <= 14000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("a press seen only from its second frame: never timed", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 14025);
	uiHoldUpdate(&h, CTRL, press(50, 210), true, 15000);
	uiHoldUpdate(&h, CTRL, move(50, 210), true, 15500);
	uiHoldUpdate(&h, CTRL, lift(), true, 15525);
	press(50, 210);
	fired = false;
	for (uint32_t t = 15550; t <= 17500; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("lifted at 500 ms, then a press seen from its second frame: never timed", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 17525);
	// A press seen landing elsewhere, then its lift and the first frame of a
	// press on the control not fed: the press is told by where it landed.
	uiHoldUpdate(&h, CTRL, press(50, 100), true, 18000);
	uiHoldUpdate(&h, CTRL, move(50, 100), true, 18025);
	press(50, 210);
	fired = false;
	for (uint32_t t = 18100; t <= 20000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("a press elsewhere, then an unseen press on the control: never timed", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 20025);
	uiHoldUpdate(&h, CTRL, press(10, 210), true, 21000);   // left of the control, same row
	press(50, 210);
	fired = false;
	for (uint32_t t = 21100; t <= 23000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("the same, the first press beside it on the same row: never timed", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 23025);
	checkTrue("across the millis() wrap: 999 ms, not fired",
	          !uiHoldUpdate(&h, CTRL, press(50, 210), true, 0xFFFFFF00u) &&
	          !uiHoldUpdate(&h, CTRL, move(50, 210), true, 0xFFFFFF00u + 999));
	checkTrue("1000 ms: fired", uiHoldUpdate(&h, CTRL, move(50, 210), true, 0xFFFFFF00u + 1000));
	uiHoldUpdate(&h, CTRL, lift(), true, 0);
}

static void testHoldReset() {
	section("Hold: a lift or a slide off resets it, and only a press on it counts");

	UiHold h;
	uiHoldReset(&h);
	uiHoldUpdate(&h, CTRL, press(50, 210), true, 1000);
	uiHoldUpdate(&h, CTRL, move(50, 210), true, 1500);
	uiHoldUpdate(&h, CTRL, lift(), true, 1525);
	checkTrue("lifted at 500 ms: reset", !h.holding);
	uiHoldUpdate(&h, CTRL, press(50, 210), true, 2000);
	checkTrue("pressed again: 999 ms from the new press does not fire",
	          !uiHoldUpdate(&h, CTRL, move(50, 210), true, 2999));
	checkTrue("1000 ms from it fires", uiHoldUpdate(&h, CTRL, move(50, 210), true, 3000));
	uiHoldUpdate(&h, CTRL, lift(), true, 3025);

	uiHoldUpdate(&h, CTRL, press(50, 210), true, 4000);
	uiHoldUpdate(&h, CTRL, move(50, 210), true, 4500);
	checkTrue("slid off at 500 ms: reset", !uiHoldUpdate(&h, CTRL, move(50, 260), true, 4525) && !h.holding);
	checkTrue("back on: the timer starts again",
	          !uiHoldUpdate(&h, CTRL, move(50, 210), true, 4600) && h.holding && h.startMs == 4600);
	checkTrue("999 ms later: not fired", !uiHoldUpdate(&h, CTRL, move(50, 210), true, 5599));
	checkTrue("1000 ms later: fired", uiHoldUpdate(&h, CTRL, move(50, 210), true, 5600));
	uiHoldUpdate(&h, CTRL, lift(), true, 5625);

	uiHoldUpdate(&h, CTRL, press(50, 100), true, 6000);
	bool fired = false;
	for (uint32_t t = 6025; t <= 8000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("a press that began elsewhere and slid on: never", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 8025);
	checkTrue("edges count: x 20 and y 200 are on it",
	          !uiHoldUpdate(&h, CTRL, press(20, 200), true, 9000) && h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 9025);
	checkTrue("x 120 is not", !uiHoldUpdate(&h, CTRL, press(120, 210), true, 9050) && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 9075);
	checkTrue("no touch: nothing", !uiHoldUpdate(&h, CTRL, nullptr, true, 9100) && !h.holding);
}

static void testHoldEnabled() {
	section("Hold: only a press that lands on the live control is timed");

	UiHold h;
	uiHoldReset(&h);
	checkTrue("disabled: a press starts nothing", !uiHoldUpdate(&h, CTRL, press(50, 210), false, 1000) && !h.holding);
	bool fired = false;
	for (uint32_t t = 1025; t <= 3000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("enabled under the finger: that press is never timed", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 3025);
	checkTrue("a new press on the live control is timed",
	          !uiHoldUpdate(&h, CTRL, press(50, 210), true, 4000) && h.holding && h.startMs == 4000);
	checkTrue("disabled at 500 ms: reset", !uiHoldUpdate(&h, CTRL, move(50, 210), false, 4500) && !h.holding);
	fired = false;
	for (uint32_t t = 4525; t <= 7000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("enabled again under the same press: never timed again", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 7025);

	uiHoldUpdate(&h, CTRL, press(50, 210), true, 8000);
	uiHoldUpdate(&h, CTRL, move(50, 210), true, 8500);
	uiHoldReset(&h);
	checkTrue("uiHoldReset() mid-hold stops the timer", !h.holding && !h.live);
	fired = false;
	for (uint32_t t = 8525; t <= 11000; t += 25) fired |= uiHoldUpdate(&h, CTRL, move(50, 210), true, t);
	checkTrue("and the press under way is not timed again", !fired && !h.holding);
	uiHoldUpdate(&h, CTRL, lift(), true, 11025);
	checkTrue("the next press is", !uiHoldUpdate(&h, CTRL, press(50, 210), true, 12000) &&
	          uiHoldUpdate(&h, CTRL, move(50, 210), true, 13000));
	uiHoldUpdate(&h, CTRL, lift(), true, 13025);
}

static void testHoldBar() {
	section("Hold: the progress bar");

	UiHold h;
	uiHoldReset(&h);
	checkInt("idle: 0 %", uiHoldPercent(&h, 1234), 0);
	uiHoldUpdate(&h, CTRL, press(50, 210), true, 1000);
	checkInt("at the press: 0 %", uiHoldPercent(&h, 1000), 0);
	checkInt("500 ms: 50 %", uiHoldPercent(&h, 1500), 50);
	checkInt("999 ms: 99 %", uiHoldPercent(&h, 1999), 99);
	checkInt("drawn later than it fires: capped at 100 %", uiHoldPercent(&h, 2500), 100);

	gfxFill(C_RED);
	uiHoldDrawBar(&h, CTRL, 196, 1500);
	checkTrue("50 %: lime from x 20 to 69", px(20, 196) == C_LIME && px(69, 198) == C_LIME);
	checkTrue("then the cleared track to x 119", px(70, 196) == C_BG && px(119, 198) == C_BG);
	checkTrue("3 px tall: y 199 untouched", px(50, 199) == C_RED && px(50, 195) == C_RED);
	checkTrue("nothing left of x 20 or right of x 119", px(19, 197) == C_RED && px(120, 197) == C_RED);
	uiHoldDrawBar(&h, CTRL, 196, 1999);
	checkTrue("99 %: lime to x 118, x 119 cleared", px(118, 197) == C_LIME && px(119, 197) == C_BG);
	uiHoldUpdate(&h, CTRL, lift(), true, 2000);
	uiHoldDrawBar(&h, CTRL, 196, 2000);
	checkTrue("not holding: the whole track cleared", px(20, 197) == C_BG && px(119, 197) == C_BG);
	gfxFill(C_BG);
}

static void testNullTouch() {
	section("Widgets: a null touch is no touch");

	UiRect r = {0, 0, 240, 320};
	checkTrue("inputPressing(), inputTapped(): false",
	          !inputPressing(nullptr, 0, 0, 240, 320) && !inputTapped(nullptr, 0, 0, 240, 320));
	checkTrue("uiPressing(), uiTapped(), uiBackTapped(): false",
	          !uiPressing(r, nullptr) && !uiTapped(r, nullptr) && !uiBackTapped(nullptr));
	uiHeader("NULL", nullptr);
	checkTrue("uiHeader() draws BACK unpressed", px(UI_BACK_X + 3, UI_BACK_Y + UI_BACK_H / 2) == C_PANEL);
	gfxFill(C_BG);
}

/** @brief A copy of the header band, for comparing two draws of it. */
static void headerCopy(uint16_t *out) {
	memcpy(out, gfxBuffer(), sizeof(uint16_t) * GFX_W * UI_HDR_H);
}

static void testHeaderBackDisabled() {
	section("Widgets: uiHeader() draws a disabled BACK as uiButton() does");

	static uint16_t live[GFX_W * UI_HDR_H], off[GFX_W * UI_HDR_H], ref[GFX_W * UI_HDR_H];
	const UiRect b = uiBackRect();
	gfxFill(C_RED);
	uiHeader("UNPLUG", idle());
	headerCopy(live);
	gfxFill(C_RED);
	uiHeader("UNPLUG", idle(), false);
	headerCopy(off);
	// The reference: the live header with BACK drawn over it disabled.
	gfxFill(C_RED);
	uiHeader("UNPLUG", idle());
	uiButton(b, "BACK", C_PANEL, C_TEXT, 1, false, false);
	headerCopy(ref);
	checkTrue("disabled: the band matches the live header with a disabled uiButton() BACK",
	          memcmp(off, ref, sizeof(off)) == 0);
	checkTrue("and differs from the live one", memcmp(off, live, sizeof(off)) != 0);
	bool sameOutside = true;
	for (int y = 0; y < UI_HDR_H; y++)
		for (int x = 0; x < GFX_W; x++) {
			bool in = x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
			if (!in && off[y * GFX_W + x] != live[y * GFX_W + x]) sameOutside = false;
		}
	checkTrue("only inside BACK's rectangle", sameOutside);
	checkTrue("an outline with C_BG inside",
	          off[(b.y + b.h / 2) * GFX_W + b.x + 4] == C_BG && off[b.y * GFX_W + b.x + b.w / 2] == C_GRID);
	checkTrue("the default is live", live[(b.y + b.h / 2) * GFX_W + b.x + 4] == C_PANEL);

	gfxFill(C_RED);
	uiHeader("UNPLUG", press(b.x + b.w / 2, b.y + b.h / 2), false);
	headerCopy(live);
	checkTrue("a finger on a disabled BACK draws no pressed look", memcmp(live, off, sizeof(off)) == 0);
	idle();
	gfxFill(C_BG);
}

/**
 * @brief The header's rule (slice 8 of menu-c.md's build plan): 1 px of
 *        `C_GRID` in the band's last row, y 35, in both themes, with BACK
 *        live or disabled.
 */
static void testHeaderRule() {
	section("Widgets: uiHeader() draws its rule at y 35");

	checkInt("the rule's row", UI_HDR_RULE_Y, 35);
	checkTrue("BACK ends at y 33, above it; the strip starts at y 40, below it",
	          UI_BACK_Y + UI_BACK_H - 1 == 33 && UI_STRIP_Y == 40);
	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		for (int en = 0; en < 2; en++) {
			gfxFill(C_RED);
			uiHeader("UNPLUG", idle(), en != 0);
			bool rule = true, above = true;
			const UiRect b = uiBackRect();
			for (int x = 0; x < GFX_W; x++) {
				if (px(x, 35) != C_GRID) rule = false;
				// The row above is the band's fill: BACK ends at y 33.
				if (px(x, 34) != C_PANEL) above = false;
			}
			char what[80];
			snprintf(what, sizeof(what), "%s, BACK %s: y 35 is C_GRID from x 0 to x 239",
			         th ? "high contrast" : "dark", en ? "live" : "disabled");
			checkTrue(what, rule);
			snprintf(what, sizeof(what), "%s, BACK %s: y 34 is the band, y 36 is not drawn",
			         th ? "high contrast" : "dark", en ? "live" : "disabled");
			checkTrue(what, above && px(0, 36) == C_RED && px(GFX_W - 1, 36) == C_RED &&
			                px(b.x + b.w / 2, 36) == C_RED);
		}
	}
	themeSet(Theme::Dark);
	gfxFill(C_BG);
}

/**
 * @brief A disabled button's cue in both themes (ui-theme.md, "Slices 7 and
 *        8 as built"): Dark draws the whole frame and a `C_GRID` label on
 *        `C_BG`; High contrast breaks the frame's straight edges into
 *        dashes of @ref UI_DASH px, through both of its 2 px rings.
 */
static void testDisabledFrame() {
	section("Widgets: a disabled button's frame is dashed in High contrast");

	// Four sizes: a narrow button, a hub entry's 228 x 46 px, a footer
	// button's 110 x 46 px and BACK's 64 x 32 px.
	static const UiRect R[] = { {8, 60, 40, 46}, {6, 120, 228, 46}, {6, 180, 110, 46}, {130, 60, 64, 32} };
	static const int HC_PERCENT[] = { 45, 49, 47, 47 };
	checkInt("a dash and a gap are 4 px each", UI_DASH, 4);
	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		const char *tn = th ? "high contrast" : "dark";
		for (int i = 0; i < 4; i++) {
			const UiRect &r = R[i];
			char what[96];
			gfxFill(C_BG);
			uiButton(r, "+", C_PANEL, C_TEXT, 2, false, true);
			snprintf(what, sizeof(what), "%s, %d x %d px, enabled: the frame whole, percent", tn, r.w, r.h);
			checkInt(what, fakeFrameDrawnPercent(r.x, r.y, r.w, r.h, UI_RADIUS, C_GRID), 100);
			gfxFill(C_BG);
			uiButton(r, "+", C_PANEL, C_TEXT, 2, true, false);
			snprintf(what, sizeof(what), "%s, %d x %d px, disabled: the frame drawn, percent", tn, r.w, r.h);
			checkInt(what, fakeFrameDrawnPercent(r.x, r.y, r.w, r.h, UI_RADIUS, C_GRID), th ? HC_PERCENT[i] : 100);
			snprintf(what, sizeof(what), "%s, %d x %d px, disabled: C_BG inside, a C_GRID label", tn, r.w, r.h);
			checkTrue(what, px(r.x + 4, r.y + r.h / 2) == C_BG &&
			                fakeTextIs(r.x + (r.w - gfxTextW("+", 2)) / 2, r.y + (r.h - 14) / 2, "+",
			                           C_GRID, C_BG, 2));
			// The first gap of the top edge: its place, its length, both rings.
			const int len = r.w - 2 * UI_RADIUS, n = (len + UI_DASH) / (2 * UI_DASH);
			const int g = r.x + UI_RADIUS + (len - (2 * n - 1) * UI_DASH) / 2;
			const int t = themeStroke();
			bool gap = true, ends = true;
			for (int k = 0; k < UI_DASH; k++)
				for (int j = 0; j < t; j++)
					gap = gap && px(g + k, r.y + j) == C_BG && px(g + k, r.y + r.h - 1 - j) == C_BG;
			ends = px(g + UI_DASH, r.y) == C_GRID && px(g + UI_DASH, r.y + t - 1) == C_GRID &&
			       px(r.x + r.w - 1 - (g - r.x) - UI_DASH, r.y) == C_GRID;
			if (th) {
				snprintf(what, sizeof(what), "%s, %d x %d px: %d gaps of 4 px on the top edge, centred, %d px deep",
				         tn, r.w, r.h, n, t);
				checkTrue(what, gap && ends && t == 2);
			}
			// The first gap of the left edge: its length and both rings.
			if (th) {
				const int vlen = r.h - 2 * UI_RADIUS, vn = (vlen + UI_DASH) / (2 * UI_DASH);
				const int vg = r.y + UI_RADIUS + (vlen - (2 * vn - 1) * UI_DASH) / 2;
				bool vgap = true;
				for (int k = 0; k < UI_DASH; k++)
					for (int j = 0; j < t; j++)
						vgap = vgap && px(r.x + j, vg + k) == C_BG && px(r.x + r.w - 1 - j, vg + k) == C_BG;
				const bool vends = px(r.x, vg + UI_DASH) == C_GRID && px(r.x + t - 1, vg + UI_DASH) == C_GRID;
				snprintf(what, sizeof(what), "%s, %d x %d px: the left and right gaps are %d px deep", tn, r.w, r.h, t);
				checkTrue(what, vgap && vends);
			}
			// The corners stay whole in both themes.
			snprintf(what, sizeof(what), "%s, %d x %d px, disabled: the four corners drawn", tn, r.w, r.h);
			checkTrue(what, px(r.x, r.y) == C_GRID && px(r.x + r.w - 1, r.y) == C_GRID &&
			                px(r.x, r.y + r.h - 1) == C_GRID && px(r.x + r.w - 1, r.y + r.h - 1) == C_GRID);
		}
	}
	themeSet(Theme::Dark);
	gfxFill(C_BG);
}

// ---------------------------------------------------------------------------
// slice 1 of menu-b.md: roles, constants, the strip, the guard, field rows
// ---------------------------------------------------------------------------

static void testColourRoles() {
	section("Widgets: each colour role names its palette entry, in both themes");

	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		char what[64];
		snprintf(what, sizeof(what), "%s: safe, live, on, warn, alert, changed, latched",
		         th ? "high contrast" : "dark");
		checkTrue(what, UI_COL_SAFE == g_theme[TC_GREEN] && UI_COL_LIVE == g_theme[TC_LIME] &&
		                UI_COL_ON == g_theme[TC_RED] && UI_COL_WARN == g_theme[TC_AMBER] &&
		                UI_COL_ALERT == g_theme[TC_MAGENTA] && UI_COL_CHANGED == g_theme[TC_CYAN] &&
		                UI_COL_LATCHED == g_theme[TC_BLUE]);
	}
	themeSet(Theme::Dark);
	const uint16_t roles[] = { UI_COL_SAFE, UI_COL_LIVE, UI_COL_ON, UI_COL_WARN,
	                           UI_COL_ALERT, UI_COL_CHANGED, UI_COL_LATCHED };
	int pairs = 0, distinct = 0;
	for (int i = 0; i < 7; i++)
		for (int k = i + 1; k < 7; k++, pairs++)
			if (roles[i] != roles[k]) distinct++;
	checkTrue("the seven roles are seven colours: 21 pairs, all distinct",
	          pairs == 21 && distinct == 21);
}

// ---------------------------------------------------------------------------
// slice 7 of menu-c.md's build plan: the palette of docs/design/ui-theme.md
// ---------------------------------------------------------------------------

/** @brief The names of the palette's entries, in the order of ThemeColor. */
static const char *const TC_NAME[TC_COUNT] = {
	"BG", "PANEL", "GRID", "DIM", "TEXT", "INK", "PAPER", "ONACCENT", "GREEN",
	"LIME", "AMBER", "RED", "REDDARK", "GHOST", "BLUE", "CYAN", "MAGENTA",
};

/** @brief An entry's 8-bit value. */
struct Triple { uint8_t r, g, b; };

/** @brief The Dark table of ui-theme.md, "The Dark table, proposed". */
static const Triple DARK_8BIT[TC_COUNT] = {
	{   2,   5,   9 }, {  26,  40,  58 }, {  72, 106, 134 }, { 132, 172, 202 },
	{ 232, 248, 255 }, { 255, 255, 255 }, {   2,   5,   9 }, {   4,   8,  14 },
	{   0, 255, 148 }, { 132, 255,  56 }, { 255, 186,   0 }, { 255,  40,  72 },
	{  96,  16,  32 }, {  14,  22,  33 }, {  72, 136, 255 }, {   0, 240, 255 },
	{ 255,   0, 255 },
};

/** @brief The same table as RGB565 words, the note's own column. */
static const uint16_t DARK_WORD[TC_COUNT] = {
	0x0021, 0x1947, 0x4B50, 0x8579, 0xEFDF, 0xFFFF, 0x0021, 0x0041, 0x07F2,
	0x87E7, 0xFDC0, 0xF949, 0x6084, 0x08A4, 0x4C5F, 0x079F, 0xF81F,
};

/** @brief High contrast's words, which the palette slice leaves as they are. */
static const uint16_t HC_WORD[TC_COUNT] = {
	0xFFFF, 0xCEBB, 0x39E9, 0x424A, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0366,
	0x03C5, 0xAAC0, 0xB803, 0xF5F7, 0xEF9E, 0x01D5, 0x02CF, 0x9011,
};

static void testPaletteValues() {
	section("Palette: each Dark word is its triple of ui-theme.md; High contrast keeps its words");

	themeSet(Theme::Dark);
	for (int i = 0; i < TC_COUNT; i++) {
		char what[64];
		snprintf(what, sizeof(what), "dark TC_%s: rgb(%d, %d, %d)", TC_NAME[i],
		         DARK_8BIT[i].r, DARK_8BIT[i].g, DARK_8BIT[i].b);
		checkInt(what, g_theme[i], rgb(DARK_8BIT[i].r, DARK_8BIT[i].g, DARK_8BIT[i].b));
		snprintf(what, sizeof(what), "dark TC_%s: the note's word", TC_NAME[i]);
		checkInt(what, g_theme[i], DARK_WORD[i]);
	}
	themeSet(Theme::HighContrast);
	for (int i = 0; i < TC_COUNT; i++) {
		char what[64];
		snprintf(what, sizeof(what), "high contrast TC_%s", TC_NAME[i]);
		checkInt(what, g_theme[i], HC_WORD[i]);
	}
	themeSet(Theme::Dark);
}

/**
 * @brief Count the pairs of palette entries with one word, and whether each
 *        such pair is one of @p allowed.
 */
static int equalPairs(const int (*allowed)[2], int nAllowed, bool *onlyAllowed) {
	int equal = 0;
	*onlyAllowed = true;
	for (int i = 0; i < TC_COUNT; i++)
		for (int k = i + 1; k < TC_COUNT; k++) {
			if (g_theme[i] != g_theme[k]) continue;
			equal++;
			bool ok = false;
			for (int a = 0; a < nAllowed; a++)
				if (allowed[a][0] == i && allowed[a][1] == k) ok = true;
			if (!ok) *onlyAllowed = false;
		}
	return equal;
}

static void testPaletteDistinct() {
	section("Palette: the words the colour probes tell apart");

	// A test finds a text by its colour and its background (fakeTextIs()),
	// so two entries with one word cannot be told apart by a probe.
	bool only = false;
	themeSet(Theme::Dark);
	static const int DARK_EQ[][2] = { { TC_BG, TC_PAPER } };
	checkInt("dark: 136 pairs, one equal", equalPairs(DARK_EQ, 1, &only), 1);
	checkTrue("it is TC_PAPER and TC_BG, by theme.h's rule", only && C_PAPER == C_BG);
	checkTrue("dark: the ink of a fill is not the screen's black", C_ONACCENT != C_PAPER);

	// High contrast keeps its values: white is its screen, its paper and its
	// ink on a fill, and black is its text and its strongest mark.
	themeSet(Theme::HighContrast);
	static const int HC_EQ[][2] = { { TC_BG, TC_PAPER }, { TC_BG, TC_ONACCENT },
	                                { TC_PAPER, TC_ONACCENT }, { TC_TEXT, TC_INK } };
	checkInt("high contrast: four pairs equal", equalPairs(HC_EQ, 4, &only), 4);
	checkTrue("BG, PAPER and ONACCENT; TEXT and INK; no other", only);
	themeSet(Theme::Dark);
}

/** @brief One channel of the 8-bit colour an RGB565 word stands for, linear. */
static double channelLinear(int c8) {
	double c = c8 / 255.0;
	return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

/**
 * @brief WCAG 2 relative luminance of an RGB565 word, read back to 8 bits a
 *        channel by bit replication, as docs/design/ui_theme.py does.
 */
static double luminance(uint16_t w) {
	int r5 = (w >> 11) & 31, g6 = (w >> 5) & 63, b5 = w & 31;
	int r = (r5 << 3) | (r5 >> 2), g = (g6 << 2) | (g6 >> 4), b = (b5 << 3) | (b5 >> 2);
	return 0.2126 * channelLinear(r) + 0.7152 * channelLinear(g) + 0.0722 * channelLinear(b);
}

/** @brief WCAG 2 contrast ratio of two words, times 100 and rounded. */
static long contrast100(uint16_t a, uint16_t b) {
	double la = luminance(a), lb = luminance(b);
	if (la < lb) { double t = la; la = lb; lb = t; }
	return lround((la + 0.05) / (lb + 0.05) * 100.0);
}

/** @brief What a pair of the contrast table is. */
enum class PairKind : uint8_t {
	Text,     /**< Text on the screen or on the control face: 4.5 or more. */
	OnFill,   /**< A label on a role's fill: 4.5 or more, no exception. */
	NotText,  /**< A fill or a frame against its ground: no floor. */
};

/** @brief A row of ui-theme.md's contrast table: the ratios times 100. */
struct ContrastRow {
	uint8_t  fg, bg;
	long     dark, hc;
	PairKind kind;
	bool     darkUnder, hcUnder;  /**< A named exception: under 4.5. */
};

static void testPaletteContrast() {
	section("Palette: the contrast table of ui-theme.md, as computed");

	static const ContrastRow ROWS[] = {
		{ TC_TEXT,     TC_BG,      1951, 2100, PairKind::Text,    false, false },
		{ TC_DIM,      TC_BG,       874,  910, PairKind::Text,    false, false },
		// Under 4.5 by intent: the disabled label, which WCAG exempts, and
		// the group captions.
		{ TC_GRID,     TC_BG,       357, 1094, PairKind::Text,    true,  false },
		{ TC_GREEN,    TC_BG,      1541,  650, PairKind::Text,    false, false },
		{ TC_LIME,     TC_BG,      1601,  557, PairKind::Text,    false, false },
		{ TC_AMBER,    TC_BG,      1202,  499, PairKind::Text,    false, false },
		{ TC_RED,      TC_BG,       554,  661, PairKind::Text,    false, false },
		{ TC_CYAN,     TC_BG,      1495,  773, PairKind::Text,    false, false },
		{ TC_BLUE,     TC_BG,       624,  968, PairKind::Text,    false, false },
		{ TC_MAGENTA,  TC_BG,       656,  796, PairKind::Text,    false, false },
		{ TC_TEXT,     TC_PANEL,   1421, 1440, PairKind::Text,    false, false },
		{ TC_DIM,      TC_PANEL,    637,  624, PairKind::Text,    false, false },
		// High contrast: REVERT's and DEFAULTS' labels, 3.42.
		{ TC_AMBER,    TC_PANEL,    876,  342, PairKind::Text,    false, true  },
		{ TC_CYAN,     TC_PANEL,   1089,  530, PairKind::Text,    false, false },
		// High contrast: ESC STATUS OK in its tile, 3.82.
		{ TC_LIME,     TC_PANEL,   1166,  382, PairKind::Text,    false, true  },
		// Dark: ESC STATUS ERROR in its tile, 4.03.
		{ TC_RED,      TC_PANEL,    403,  453, PairKind::Text,    true,  false },
		{ TC_MAGENTA,  TC_PANEL,    478,  546, PairKind::Text,    false, false },
		{ TC_ONACCENT, TC_GREEN,   1515,  650, PairKind::OnFill,  false, false },
		{ TC_ONACCENT, TC_CYAN,    1470,  773, PairKind::OnFill,  false, false },
		{ TC_ONACCENT, TC_LIME,    1574,  557, PairKind::OnFill,  false, false },
		{ TC_ONACCENT, TC_RED,      545,  661, PairKind::OnFill,  false, false },
		{ TC_ONACCENT, TC_BLUE,     614,  968, PairKind::OnFill,  false, false },
		{ TC_ONACCENT, TC_MAGENTA,  645,  796, PairKind::OnFill,  false, false },
		{ TC_PAPER,    TC_AMBER,   1202,  499, PairKind::OnFill,  false, false },
		{ TC_PAPER,    TC_INK,     2057, 2100, PairKind::OnFill,  false, false },
		{ TC_PANEL,    TC_BG,       137,  146, PairKind::NotText, false, false },
		{ TC_GRID,     TC_PANEL,    260,  750, PairKind::NotText, false, false },
		{ TC_REDDARK,  TC_BG,       159,  161, PairKind::NotText, false, false },
		{ TC_GHOST,    TC_BG,       111,  112, PairKind::NotText, false, false },
	};
	checkInt("the table's rows", (long)(sizeof(ROWS) / sizeof(ROWS[0])), 29);

	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		int textUnder = 0, fillUnder = 0, named = 0, namedOver = 0;
		for (const ContrastRow &r : ROWS) {
			long got = contrast100(g_theme[r.fg], g_theme[r.bg]);
			char what[64];
			snprintf(what, sizeof(what), "%s: %s on %s", th ? "high contrast" : "dark",
			         TC_NAME[r.fg], TC_NAME[r.bg]);
			checkInt(what, got, th ? r.hc : r.dark);
			bool under = th ? r.hcUnder : r.darkUnder;
			if (under) {
				named++;
				if (got >= 450) namedOver++;
			} else if (r.kind == PairKind::Text && got < 450) {
				textUnder++;
			} else if (r.kind == PairKind::OnFill && got < 450) {
				fillUnder++;
			}
			if (under && r.kind != PairKind::Text) fillUnder++;
		}
		char what[80];
		snprintf(what, sizeof(what), "%s: text pairs under 4.5 outside the named exceptions",
		         th ? "high contrast" : "dark");
		checkInt(what, textUnder, 0);
		snprintf(what, sizeof(what), "%s: labels on a fill under 4.5", th ? "high contrast" : "dark");
		checkInt(what, fillUnder, 0);
		snprintf(what, sizeof(what), "%s: the named exceptions, each under 4.5",
		         th ? "high contrast" : "dark");
		checkTrue(what, named == 2 && namedOver == 0);
	}
	themeSet(Theme::Dark);

	// Why the neutral chip is a frame and the edit buttons are neutral: the
	// Dark theme's ink on a C_GRID fill.
	checkInt("dark: ONACCENT on a GRID fill, 3.51", contrast100(C_ONACCENT, C_GRID), 351);
	checkInt("dark: DIM on BG, the neutral chip's label, 8.74", contrast100(C_DIM, C_BG), 874);
	checkInt("dark: a frame against the screen, 3.57, over 3", contrast100(C_GRID, C_BG), 357);
}

static void testLayoutConstants() {
	section("Widgets: the layout constants");

	checkInt("button edge: x 6", UI_EDGE, 6);
	checkInt("BACK ends at the button edge, x 234", UI_BACK_X + UI_BACK_W, GFX_W - UI_EDGE);
	checkInt("footer: y 268", UI_FOOT_Y, 268);
	checkInt("46 px tall, to y 313", UI_FOOT_Y + UI_FOOT_H - 1, 313);
	checkInt("editor - and +: 44 px", UI_EDIT_BTN_W, 44);
	checkTrue("strip lines at y 42 and y 52", UI_STRIP_LINE1_Y == 42 && UI_STRIP_LINE2_Y == 52);
	checkTrue("the second ends at y 58, above the body at y 62",
	          UI_STRIP_LINE2_Y + 6 == 58 && UI_BODY_Y == 62);

	// uiChipX() is where uiChip() draws: MODE, 23 px and 6 px each side,
	// from x 197 to the right margin.
	gfxFill(C_BG);
	uiChip("MODE", C_GREEN);
	const int cy = UI_STRIP_Y + UI_STRIP_H / 2;
	checkInt("uiChipX(\"MODE\"): x 197", uiChipX("MODE"), 197);
	checkTrue("the chip drawn from x 197 to x 231", gfxBuffer()[cy * GFX_W + 196] == C_BG &&
	          gfxBuffer()[cy * GFX_W + 198] == C_GREEN && gfxBuffer()[cy * GFX_W + 231] == C_GREEN &&
	          gfxBuffer()[cy * GFX_W + 232] == C_BG);
	checkInt("PROG MODE, 53 px: x 167", uiChipX("PROG MODE"), 167);
}

static void testStripTwoLines() {
	section("Widgets: uiStripTwoLines() at y 42 and y 52");

	gfxFill(C_RED);
	uiStripTwoLines("GP0  50 HZ  1000-2000 US", C_DIM, "GP28 SUPPLY STAYS OFF HERE.", C_AMBER);
	checkTrue("the first line at x 8, y 42",
	          fakeTextIs(UI_MARGIN, 42, "GP0  50 HZ  1000-2000 US", C_DIM, C_BG, 1));
	checkTrue("the second at x 8, y 52, in its colour",
	          fakeTextIs(UI_MARGIN, 52, "GP28 SUPPLY STAYS OFF HERE.", C_AMBER, C_BG, 1));
	checkTrue("y 40 and y 61 cleared", px(200, 40) == C_BG && px(200, 61) == C_BG);
	checkTrue("y 39 and y 62 untouched", px(200, 39) == C_RED && px(200, 62) == C_RED);
	gfxFill(C_RED);
	uiStripTwoLines(nullptr, C_DIM, "ONE", C_AMBER);
	int x0, x1;
	checkTrue("a null first line draws nothing at y 42",
	          !fakeInkExtent(40, 12, C_BG, &x0, &x1) && fakeTextIs(UI_MARGIN, 52, "ONE", C_AMBER, C_BG, 1));
	uiStripTwoLines("ONE", C_DIM, nullptr, C_AMBER);
	checkTrue("a null second line draws nothing at y 52",
	          !fakeInkExtent(50, 12, C_BG, &x0, &x1) && fakeTextIs(UI_MARGIN, 42, "ONE", C_DIM, C_BG, 1));
	gfxFill(C_BG);
}

static void testChip() {
	section("Widgets: uiChip() is square, 12 px tall, y 42 to y 53, ending at x 232");

	gfxFill(C_RED);
	uiStripClear();
	uiChip("PROG MODE", C_BLUE);
	const int w = gfxTextW("PROG MODE", 1) + 12, x = GFX_W - UI_MARGIN - w;
	checkInt("PROG MODE: 53 px of text plus 12, 65 px", w, 65);
	checkTrue("its corners are filled: square, no radius",
	          px(x, 42) == C_BLUE && px(x + w - 1, 42) == C_BLUE &&
	          px(x, 53) == C_BLUE && px(x + w - 1, 53) == C_BLUE);
	checkTrue("x 167 to x 231, nothing at x 166 or x 232",
	          x == 167 && px(x - 1, 47) == C_BG && px(GFX_W - UI_MARGIN, 47) == C_BG);
	checkTrue("y 41 and y 54 are the strip's, not the chip's",
	          px(x + 2, 41) == C_BG && px(x + 2, 54) == C_BG);
	checkTrue("the text 6 px in at y 44, C_ONACCENT on the fill",
	          fakeTextIs(x + 6, 44, "PROG MODE", C_ONACCENT, C_BLUE, 1));
	checkInt("a chip's height", UI_CHIP_H, 12);
	gfxFill(C_BG);
}

/**
 * @brief The neutral chip (slice 8 of menu-c.md's build plan): uiChip() with
 *        `C_GRID` draws a 1 px frame, `C_BG` inside and a `C_DIM` label, in
 *        both themes; every other fill stays a fill.
 */
static void testChipNeutral() {
	section("Widgets: uiChip() with C_GRID is a frame, C_BG inside, a C_DIM label");

	static const char *const TEXTS[] = { "NO ESC", "PORT CLOSED", "MOVING" };
	for (int th = 0; th < 2; th++) {
		themeSet(th ? Theme::HighContrast : Theme::Dark);
		for (const char *text : TEXTS) {
			gfxFill(C_RED);
			uiStripClear();
			uiChip(text, C_GRID);
			const int w = gfxTextW(text, 1) + 12, x = GFX_W - UI_MARGIN - w;
			bool frame = true;
			int grid = 0, bg = 0, dim = 0, other = 0;
			for (int j = 42; j <= 53; j++)
				for (int i = x; i < x + w; i++) {
					bool edge = j == 42 || j == 53 || i == x || i == x + w - 1;
					uint16_t p = px(i, j);
					if (edge) { if (p != C_GRID) frame = false; continue; }
					if (p == C_GRID) grid++;
					else if (p == C_BG) bg++;
					else if (p == C_DIM) dim++;
					else other++;
				}
			char what[96];
			snprintf(what, sizeof(what), "%s, %s: a 1 px C_GRID frame, square, y 42 to y 53",
			         th ? "high contrast" : "dark", text);
			checkTrue(what, frame && x == uiChipX(text) && px(x - 1, 47) == C_BG &&
			                px(GFX_W - UI_MARGIN, 47) == C_BG && px(x + 2, 41) == C_BG &&
			                px(x + 2, 54) == C_BG);
			snprintf(what, sizeof(what), "%s, %s: inside, C_BG and the label's C_DIM only",
			         th ? "high contrast" : "dark", text);
			checkTrue(what, grid == 0 && other == 0 && bg > 0 && dim > 0 && px(x + 1, 43) == C_BG);
			snprintf(what, sizeof(what), "%s, %s: the label 6 px in at y 44, C_DIM on C_BG",
			         th ? "high contrast" : "dark", text);
			checkTrue(what, fakeTextIs(x + 6, 44, text, C_DIM, C_BG, 1) &&
			                !fakeTextIs(x + 6, 44, text, C_ONACCENT, C_GRID, 1));
		}
		// A chip with a state keeps its fill and its ink.
		static const ThemeColor FILLS[] = { TC_GREEN, TC_LIME, TC_RED, TC_BLUE, TC_MAGENTA, TC_AMBER, TC_INK };
		bool filled = true;
		for (ThemeColor f : FILLS) {
			gfxFill(C_BG);
			uiChip("MODE", g_theme[f]);
			uint16_t fg = (f == TC_AMBER || f == TC_INK) ? C_PAPER : C_ONACCENT;
			const int x = uiChipX("MODE");
			if (px(x + 1, 43) != g_theme[f] || !fakeTextIs(x + 6, 44, "MODE", fg, g_theme[f], 1)) filled = false;
		}
		checkTrue(th ? "high contrast: a role's chip stays a fill with its ink"
		             : "dark: a role's chip stays a fill with its ink", filled);
	}
	themeSet(Theme::Dark);
	gfxFill(C_BG);
}

static void testChangeText() {
	section("Widgets: uiChangeText() draws the old value dim, the new one cyan");

	gfxFill(C_BG);
	const char *pair = "24KHZ -> 25KHZ";
	const int x = GFX_W - UI_MARGIN - gfxTextW(pair, 1);
	uiChangeText(x, 100, "24KHZ", "25KHZ");
	checkTrue("24KHZ -> in C_DIM from the left edge",
	          fakeTextIs(x, 100, "24KHZ -> ", C_DIM, C_BG, 1));
	checkTrue("25KHZ in C_CYAN right after it, 6 px a character",
	          fakeTextIs(x + 6 * 9, 100, "25KHZ", C_CYAN, C_BG, 1));
	int x0 = 0, x1 = 0;
	checkTrue("as wide as the pair: its ink from x to x 231",
	          fakeInkExtent(100, 7, C_BG, &x0, &x1) && x0 == x && x1 == GFX_W - UI_MARGIN - 1);
	gfxFill(C_BG);
}

/** @brief A tap on BACK's centre: the press frame, then the lift. */
static UiGuardEv tapBack(UiBackGuard *g, int pending) {
	UiRect b = uiBackRect();
	UiGuardEv e = uiBackGuard(g, press(b.x + b.w / 2, b.y + b.h / 2), pending);
	if (e != UiGuardEv::None) return e;
	return uiBackGuard(g, lift(), pending);
}

static void testBackGuard() {
	section("Widgets: the BACK guard arms on the first BACK, leaves on the second");

	UiBackGuard g = {};
	checkTrue("no change: the first BACK leaves", tapBack(&g, 0) == UiGuardEv::Back && !g.armed);
	checkTrue("two changes: the first BACK arms", tapBack(&g, 2) == UiGuardEv::Armed && g.armed);
	checkTrue("the second leaves and disarms", tapBack(&g, 2) == UiGuardEv::Back && !g.armed);

	tapBack(&g, 1);
	checkTrue("armed: a press elsewhere disarms on the frame it lands",
	          uiBackGuard(&g, press(120, 200), 1) == UiGuardEv::Disarmed && !g.armed);
	checkTrue("its lift does nothing", uiBackGuard(&g, lift(), 1) == UiGuardEv::None);
	checkTrue("so the next BACK arms again", tapBack(&g, 1) == UiGuardEv::Armed);

	UiRect b = uiBackRect();
	checkTrue("armed: a press on BACK keeps it armed",
	          uiBackGuard(&g, press(b.x + 2, b.y + 2), 1) == UiGuardEv::None && g.armed);
	move(120, 200);
	checkTrue("slid off BACK and lifted: no tap, still armed",
	          uiBackGuard(&g, lift(), 1) == UiGuardEv::None && g.armed);
	checkTrue("a frame with no touch keeps it armed",
	          uiBackGuard(&g, nullptr, 1) == UiGuardEv::None && g.armed);
	checkTrue("armed with no change left: disarmed, with or without a touch",
	          uiBackGuard(&g, nullptr, 0) == UiGuardEv::Disarmed && !g.armed);
	tapBack(&g, 1);
	uiBackGuard(&g, press(b.x + b.w / 2, b.y + b.h / 2), 1);
	checkTrue("armed, a BACK tap on the frame the last change goes: Back",
	          uiBackGuard(&g, lift(), 0) == UiGuardEv::Back && !g.armed);
	checkTrue("a press elsewhere while disarmed is nothing",
	          uiBackGuard(&g, press(120, 200), 3) == UiGuardEv::None);
	uiBackGuard(&g, lift(), 3);
	idle();
}

static void testBackGuardDraw() {
	section("Widgets: the guard's two amber lines");

	gfxFill(C_RED);
	uiBackGuardDraw(2, "WRITTEN");
	checkTrue("2 CHANGES NOT WRITTEN. at y 42",
	          fakeTextIs(UI_MARGIN, 42, "2 CHANGES NOT WRITTEN.", C_AMBER, C_BG, 1));
	checkTrue("BACK AGAIN DISCARDS THEM. at y 52",
	          fakeTextIs(UI_MARGIN, 52, "BACK AGAIN DISCARDS THEM.", C_AMBER, C_BG, 1));
	checkInt("131 px wide", gfxTextW("2 CHANGES NOT WRITTEN.", 1), 131);
	uiBackGuardDraw(1, "APPLIED");
	checkTrue("1 CHANGE NOT APPLIED. at y 42",
	          fakeTextIs(UI_MARGIN, 42, "1 CHANGE NOT APPLIED.", C_AMBER, C_BG, 1));
	checkTrue("BACK AGAIN DISCARDS IT. at y 52",
	          fakeTextIs(UI_MARGIN, 52, "BACK AGAIN DISCARDS IT.", C_AMBER, C_BG, 1));
	gfxFill(C_BG);
}

/** @brief Top of the field row under test, and its body: a 40 px row. */
static const int FY = 100, FH = 40;

/** @brief Whether the row's value @p v shows right-aligned to x 231 in @p col over @p bg. */
static bool valueIs(const char *v, uint16_t col, uint16_t bg) {
	int sc = gfxTextW(v, 2) <= UI_FIELD_VALUE_W2 ? 2 : 1;
	return fakeTextIs(GFX_W - UI_MARGIN - gfxTextW(v, sc), FY + (FH - 1 - 7 * sc + 1) / 2, v, col, bg, sc);
}

static void testListDrawField() {
	section("List: uiListDrawField() draws one field row");

	UiField f = { "TUNE", "DEAD BAND", "2.2 US", UiValue::Plain, UiEdge::None, false };
	gfxFill(C_RED);
	uiListDrawField(FY, FH, f);
	checkTrue("filled C_BG over 39 px", px(120, FY) == C_BG && px(120, FY + 38) == C_BG);
	checkTrue("its rule on the 40th line, C_PANEL", px(120, FY + 39) == C_PANEL);
	checkTrue("the pitch's gap untouched", px(120, FY + 40) == C_RED && px(120, FY - 1) == C_RED);
	checkTrue("the group at y + 11, C_GRID", fakeTextIs(UI_MARGIN, FY + 11, "TUNE", C_GRID, C_BG, 1));
	checkTrue("the label at y + 21, C_DIM", fakeTextIs(UI_MARGIN, FY + 21, "DEAD BAND", C_DIM, C_BG, 1));
	checkTrue("Plain: the value C_TEXT at scale 2, y + 13", valueIs("2.2 US", C_TEXT, C_BG) &&
	          fakeTextIs(232 - gfxTextW("2.2 US", 2), FY + 13, "2.2 US", C_TEXT, C_BG, 2));
	checkTrue("no edge bar", px(1, FY + 5) == C_BG);

	f.selected = true;
	uiListDrawField(FY, FH, f);
	checkTrue("selected: filled C_PANEL", px(120, FY) == C_PANEL);
	checkTrue("the label C_TEXT", fakeTextIs(UI_MARGIN, FY + 21, "DEAD BAND", C_TEXT, C_PANEL, 1));
	checkTrue("the value's colour unchanged", valueIs("2.2 US", C_TEXT, C_PANEL));

	f.state = UiValue::Changed;
	f.edge = UiEdge::Changed;
	uiListDrawField(FY, FH, f);
	checkTrue("Changed: the value C_CYAN, UI_COL_CHANGED, selected",
	          valueIs("2.2 US", C_CYAN, C_PANEL));
	checkTrue("the bar C_CYAN, 3 px at x 0, as tall as the fill",
	          px(0, FY) == C_CYAN && px(2, FY + 38) == C_CYAN && px(3, FY + 5) == C_PANEL &&
	          px(0, FY + 39) == C_PANEL);
	f.selected = false;
	uiListDrawField(FY, FH, f);
	checkTrue("not selected: the value and the bar still C_CYAN",
	          valueIs("2.2 US", C_CYAN, C_BG) && px(1, FY + 5) == C_CYAN);

	f.edge = UiEdge::Rule;
	uiListDrawField(FY, FH, f);
	checkTrue("Rule: the bar amber, the value still C_CYAN",
	          px(1, FY + 5) == C_AMBER && valueIs("2.2 US", C_CYAN, C_BG));

	f.state = UiValue::ReadOnly;
	f.edge = UiEdge::None;
	f.selected = true;
	uiListDrawField(FY, FH, f);
	checkTrue("ReadOnly: the value and the label C_GRID, selected or not",
	          valueIs("2.2 US", C_GRID, C_PANEL) &&
	          fakeTextIs(UI_MARGIN, FY + 21, "DEAD BAND", C_GRID, C_PANEL, 1));

	f.state = UiValue::Odd;
	f.selected = false;
	uiListDrawField(FY, FH, f);
	checkTrue("Odd: the value amber, the label as a live row's",
	          valueIs("2.2 US", C_AMBER, C_BG) && fakeTextIs(UI_MARGIN, FY + 21, "DEAD BAND", C_DIM, C_BG, 1));

	// 104 px at scale 2 is 9 characters less one column: "8 CHARS." is 94 px.
	f.state = UiValue::Plain;
	f.group = nullptr;
	f.value = "123456789";
	gfxFill(C_RED);
	uiListDrawField(FY, FH, f);
	checkInt("9 characters at scale 2: 106 px", gfxTextW("123456789", 2), 106);
	checkTrue("over 104 px: scale 1, at y + 16",
	          fakeTextIs(232 - gfxTextW("123456789", 1), FY + 16, "123456789", C_TEXT, C_BG, 1));
	int x0, x1;
	checkTrue("no group: nothing from y + 11 to the value's top at y + 16",
	          !fakeInkExtent(FY + 11, 5, C_BG, &x0, &x1));
	f.value = "12345678";
	uiListDrawField(FY, FH, f);
	checkTrue("8 characters, 94 px: scale 2", gfxTextW("12345678", 2) == 94 && valueIs("12345678", C_TEXT, C_BG) &&
	          fakeTextIs(232 - 94, FY + 13, "12345678", C_TEXT, C_BG, 2));
	gfxFill(C_BG);
}

// ---------------------------------------------------------------------------
// the leave page
// ---------------------------------------------------------------------------

/** @brief A tap at the centre of @p r, fed to the page at @p now; the event of the lift. */
static UiLeaveEv tapLeave(UiLeave *l, const UiRect &r, uint32_t now) {
	UiLeaveEv e = uiLeaveTouch(l, press(r.x + r.w / 2, r.y + r.h / 2), now);
	if (e != UiLeaveEv::None) return e;
	return uiLeaveTouch(l, lift(), now);
}

static void testLeavePhases() {
	section("Leave page: before LEAVE, releasing, released");

	UiLeave l;
	uiLeaveOpen(&l, UiLeavePhase::Before, 1000);
	checkTrue("opened before LEAVE: step 0 current",
	          l.phase == UiLeavePhase::Before && uiLeaveCurrent(&l) == 0);
	checkTrue("STAY: Stay, the page unchanged",
	          tapLeave(&l, UI_LEAVE_LEFT, 0) == UiLeaveEv::Stay && l.phase == UiLeavePhase::Before);
	checkTrue("a tap on the right button is the screen's: nothing",
	          tapLeave(&l, UI_LEAVE_RIGHT, 0) == UiLeaveEv::None);
	checkTrue("LEAVE: Leave, and the page is releasing",
	          tapLeave(&l, UI_LEAVE_WIDE, 0) == UiLeaveEv::Leave && l.phase == UiLeavePhase::Releasing);
	checkInt("the step after the first is current", uiLeaveCurrent(&l), 1);
	checkTrue("releasing: the left and wide buttons do nothing",
	          tapLeave(&l, UI_LEAVE_LEFT, 0) == UiLeaveEv::None &&
	          tapLeave(&l, UI_LEAVE_WIDE, 0) == UiLeaveEv::None);
	uiLeavePoll(&l, false, 4000);
	checkTrue("not due: still releasing", l.phase == UiLeavePhase::Releasing);
	uiLeavePoll(&l, true, 5000);
	checkTrue("due: released from that frame",
	          l.phase == UiLeavePhase::Released && l.releasedMs == 5000 && uiLeaveCurrent(&l) == 1);
	uiLeavePoll(&l, true, 7000);
	checkInt("a later poll keeps the phase's start", (int)l.releasedMs, 5000);
	checkTrue("released: the left button is Next", tapLeave(&l, UI_LEAVE_LEFT, 7000) == UiLeaveEv::Next);
	checkTrue("no touch: nothing", uiLeaveTouch(&l, nullptr, 7000) == UiLeaveEv::None);

	uiLeaveOpen(&l, UiLeavePhase::Releasing, 1000);
	checkTrue("opened releasing: step 0 current, no step before LEAVE",
	          l.phase == UiLeavePhase::Releasing && !l.before && uiLeaveCurrent(&l) == 0);
	uiLeavePoll(&l, true, 10);
	checkInt("and still step 0 once released", uiLeaveCurrent(&l), 0);
	uiLeaveOpen(&l, UiLeavePhase::Released, 1000);
	checkTrue("opened released: releasing until the poll",
	          l.phase == UiLeavePhase::Releasing && !uiLeaveRestartLive(&l, 5000));
	idle();
}

/** @brief The supply note: the time left never reads more than the whole wait. */
static void testLeaveSupplyNote() {
	section("Leave page: the supply note's time left is clamped to the wait");
	char b[48];
	uiLeaveSupplyNote(b, sizeof(b), 2000, 14);
	checkStr("1.4 s left", b, "SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.");
	uiLeaveSupplyNote(b, sizeof(b), 2000, 20);
	checkStr("the whole wait", b, "SUPPLY OFF FOR 2.0 S: 2.0 S LEFT.");
	uiLeaveSupplyNote(b, sizeof(b), 2000, 25);
	checkStr("more than the wait reads the wait", b, "SUPPLY OFF FOR 2.0 S: 2.0 S LEFT.");
	uiLeaveSupplyNote(b, sizeof(b), 2000, 1);
	checkStr("0.1 s left", b, "SUPPLY OFF FOR 2.0 S: 0.1 S LEFT.");
	checkInt("33 characters, within a note line", (int)strlen("SUPPLY OFF FOR 2.0 S: 2.0 S LEFT."),
	         33);
}

static void testLeaveRestartWait() {
	section("Leave page: RESTART DSHOT takes a press 1000 ms into the released phase");

	UiLeave l;
	uiLeaveOpen(&l, UiLeavePhase::Releasing, 1000);
	uiLeavePoll(&l, true, 5000);
	checkTrue("999 ms in: not live", !uiLeaveRestartLive(&l, 5999));
	checkTrue("1000 ms in: live", uiLeaveRestartLive(&l, 6000));
	const UiRect &w = UI_LEAVE_WIDE;
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 5999);
	checkTrue("a press that landed at 999 ms and lifts at 1100 ms: nothing",
	          uiLeaveTouch(&l, lift(), 6100) == UiLeaveEv::None);
	checkTrue("a press that lands at 1000 ms: Restart", tapLeave(&l, w, 6000) == UiLeaveEv::Restart);
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 6000);
	move(w.x + 20, w.y - 40);
	checkTrue("slid off before the lift: nothing", uiLeaveTouch(&l, lift(), 6100) == UiLeaveEv::None);
	uiLeaveTouch(&l, press(UI_LEAVE_LEFT.x + 5, UI_LEAVE_LEFT.y + 5), 6000);
	move(w.x + 20, w.y + 20);
	checkTrue("a press that landed on the left button and lifts on it: nothing",
	          uiLeaveTouch(&l, lift(), 6100) == UiLeaveEv::None);

	uiLeaveOpen(&l, UiLeavePhase::Releasing, 1000);
	uiLeavePoll(&l, true, 0xFFFFFF00u);
	checkTrue("16 ms in, before the wrap: not live", !uiLeaveRestartLive(&l, 0xFFFFFF10u));
	checkTrue("across a millis() wrap: 999 ms not live, 1000 ms live",
	          !uiLeaveRestartLive(&l, 0xFFFFFF00u + 999u) && uiLeaveRestartLive(&l, 0xFFFFFF00u + 1000u));

	uiLeaveOpen(&l, UiLeavePhase::Before, 0);
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 100);
	checkTrue("no wait: LEAVE's tap leaves", uiLeaveTouch(&l, lift(), 150) == UiLeaveEv::Leave);
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 200);
	uiLeavePoll(&l, true, 250);
	checkTrue("live with the released phase", uiLeaveRestartLive(&l, 250));
	checkTrue("but a second press that landed while releasing restarts nothing",
	          uiLeaveTouch(&l, lift(), 300) == UiLeaveEv::None);
	checkTrue("the next press does", tapLeave(&l, w, 400) == UiLeaveEv::Restart);
	idle();
}

/** @brief KST SERVO, servo on its own supply, as the note draws it. */
static UiLeaveText kstOwnText() {
	UiLeaveText t = {};
	t.step[0][0] = "POWER THE SERVO OFF, THEN LEAVE.";
	t.step[0][1] = "IT STAYS IN PROGRAMMING MODE UNTIL";
	t.step[0][2] = "ITS SUPPLY IS OFF.";
	t.step[1][0] = "UNPLUG THE SERVO.";
	t.step[1][1] = "DSHOT RESTARTS ON SIG GP0 AND READS";
	t.step[1][2] = "AS A 970 US PULSE AT 1000 HZ.";
	t.step[2][0] = "RESTART DSHOT.";
	t.note[0] = "2 CHANGES NOT WRITTEN.";
	t.note[1] = "THE SERVO IS IN DOUBT: RESTORE FIRST.";
	t.next = "NEXT SERVO";
	return t;
}

/** @brief Whether button @p r shows @p label, live or drawn disabled. */
static bool buttonIs(const UiRect &r, const char *label, int scale, bool live) {
	int x = r.x + (r.w - gfxTextW(label, scale)) / 2, y = r.y + (r.h - 7 * scale) / 2;
	uint16_t fill = live ? C_PANEL : C_BG;
	return px(r.x + 4, r.y + r.h / 2) == fill && fakeTextIs(x, y, label, live ? C_TEXT : C_GRID, fill, scale);
}

static void testLeaveDraw() {
	section("Leave page: steps, notes and buttons in each phase");

	UiLeave l;
	UiLeaveText t = kstOwnText();
	uiLeaveOpen(&l, UiLeavePhase::Before, 1000);
	gfxFill(C_RED);
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("y 61 untouched, y 62 cleared", px(120, 61) == C_RED && px(120, 62) == C_BG);
	checkTrue("step 1 numbered at x 8, y 66, amber", fakeTextIs(UI_MARGIN, 66, "1", C_AMBER, C_BG, 1));
	checkTrue("its text at x 20", fakeTextIs(20, 66, "POWER THE SERVO OFF, THEN LEAVE.", C_AMBER, C_BG, 1));
	checkTrue("its detail lines at y 78 and y 90",
	          fakeTextIs(20, 78, "IT STAYS IN PROGRAMMING MODE UNTIL", C_AMBER, C_BG, 1) &&
	          fakeTextIs(20, 90, "ITS SUPPLY IS OFF.", C_AMBER, C_BG, 1));
	checkTrue("the bar: 3 px at x 0 from y 66 to y 96",
	          px(0, 66) == C_AMBER && px(2, 96) == C_AMBER && px(3, 80) == C_BG &&
	          px(1, 65) == C_BG && px(1, 97) == C_BG);
	checkTrue("step 2 at y 104, C_DIM, no bar",
	          fakeTextIs(UI_MARGIN, 104, "2", C_DIM, C_BG, 1) &&
	          fakeTextIs(20, 104, "UNPLUG THE SERVO.", C_DIM, C_BG, 1) && px(1, 104) == C_BG);
	checkTrue("its longest line, 35 characters from x 20, ends at x 228",
	          fakeTextIs(20, 116, "DSHOT RESTARTS ON SIG GP0 AND READS", C_DIM, C_BG, 1) &&
	          20 + gfxTextW("DSHOT RESTARTS ON SIG GP0 AND READS", 1) - 1 == 228);
	checkTrue("step 3 at y 142", fakeTextIs(UI_MARGIN, 142, "3", C_DIM, C_BG, 1) &&
	          fakeTextIs(20, 142, "RESTART DSHOT.", C_DIM, C_BG, 1));
	checkTrue("the notes at y 158 and y 170, amber",
	          fakeTextIs(UI_MARGIN, 158, "2 CHANGES NOT WRITTEN.", C_AMBER, C_BG, 1) &&
	          fakeTextIs(UI_MARGIN, 170, "THE SERVO IS IN DOUBT: RESTORE FIRST.", C_AMBER, C_BG, 1));
	checkTrue("STAY live at 6, 214", buttonIs(UI_LEAVE_LEFT, "STAY", 1, true));
	checkTrue("LEAVE live, scale 2, at 6, 268", buttonIs(UI_LEAVE_WIDE, "LEAVE", 2, true));
	checkTrue("the right button's place left clear", px(UI_LEAVE_RIGHT.x + 4, UI_LEAVE_RIGHT.y + 20) == C_BG);

	tapLeave(&l, UI_LEAVE_WIDE, 0);
	t.note[0] = "RELEASING THE PIN";
	t.note[1] = nullptr;
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("releasing: step 1 C_DIM, no bar",
	          fakeTextIs(20, 66, "POWER THE SERVO OFF, THEN LEAVE.", C_DIM, C_BG, 1) && px(1, 66) == C_BG);
	checkTrue("step 2 current: amber, its bar from y 104 to y 134",
	          fakeTextIs(20, 104, "UNPLUG THE SERVO.", C_AMBER, C_BG, 1) &&
	          px(1, 104) == C_AMBER && px(1, 134) == C_AMBER && px(1, 135) == C_BG);
	checkTrue("the notes C_DIM, one line",
	          fakeTextIs(UI_MARGIN, 158, "RELEASING THE PIN", C_DIM, C_BG, 1) && px(UI_MARGIN, 172) == C_BG);
	checkTrue("NEXT SERVO drawn disabled", buttonIs(UI_LEAVE_LEFT, "NEXT SERVO", 1, false));
	checkTrue("RESTART DSHOT drawn disabled", buttonIs(UI_LEAVE_WIDE, "RESTART DSHOT", 2, false));
	const UiRect &w = UI_LEAVE_WIDE;
	uiLeaveDraw(&l, t, press(w.x + 20, w.y + 20), 0);
	checkTrue("a finger on it draws no pressed look", px(w.x + 2, w.y + w.h / 2) == C_BG);

	uiLeavePoll(&l, true, 1000);
	idle();
	uiLeaveDraw(&l, t, idle(), 1999);
	checkTrue("released, 999 ms in: NEXT SERVO live, RESTART DSHOT still disabled",
	          buttonIs(UI_LEAVE_LEFT, "NEXT SERVO", 1, true) && buttonIs(UI_LEAVE_WIDE, "RESTART DSHOT", 2, false));
	uiLeaveDraw(&l, t, idle(), 2000);
	checkTrue("1000 ms in: RESTART DSHOT live", buttonIs(UI_LEAVE_WIDE, "RESTART DSHOT", 2, true));
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 1999);
	uiLeaveTouch(&l, move(w.x + 20, w.y + 20), 2100);
	uiLeaveDraw(&l, t, &s_touch, 2100);
	checkTrue("a press that landed during the wait, held past it: no pressed look",
	          buttonIs(UI_LEAVE_WIDE, "RESTART DSHOT", 2, true) && px(w.x + 2, w.y + w.h / 2) != C_INK);
	uiLeaveTouch(&l, lift(), 2200);
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 2000);
	uiLeaveDraw(&l, t, &s_touch, 2000);
	checkTrue("pressed: the inner ring", px(w.x + 2, w.y + w.h / 2) == C_INK);
	uiLeaveTouch(&l, lift(), 2000);
	idle();

	t.next = nullptr;
	gfxFill(C_RED);
	uiLeaveDraw(&l, t, idle(), 2000);
	checkTrue("no next label: no left button", px(UI_LEAVE_LEFT.x + 4, UI_LEAVE_LEFT.y + 20) == C_BG);
	gfxFill(C_BG);
}

static void testLeaveEnabled() {
	section("Leave page: LEAVE drawn disabled and not tappable while the screen writes");

	UiLeave l;
	UiLeaveText t = kstOwnText();
	uiLeaveOpen(&l, UiLeavePhase::Before, 1000);
	checkTrue("opened: LEAVE live", l.leaveEnabled);
	l.leaveEnabled = false;
	const UiRect &w = UI_LEAVE_WIDE;
	gfxFill(C_RED);
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("cleared: LEAVE drawn disabled, STAY live",
	          buttonIs(UI_LEAVE_WIDE, "LEAVE", 2, false) && buttonIs(UI_LEAVE_LEFT, "STAY", 1, true));
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 0);
	uiLeaveDraw(&l, t, &s_touch, 0);
	checkTrue("a finger on it draws no pressed look", px(w.x + 2, w.y + w.h / 2) == C_BG);
	checkTrue("its tap does nothing, the page stays before LEAVE",
	          uiLeaveTouch(&l, lift(), 0) == UiLeaveEv::None && l.phase == UiLeavePhase::Before);
	checkTrue("STAY still acts", tapLeave(&l, UI_LEAVE_LEFT, 0) == UiLeaveEv::Stay);
	l.leaveEnabled = true;
	checkTrue("set again: LEAVE leaves",
	          tapLeave(&l, w, 0) == UiLeaveEv::Leave && l.phase == UiLeavePhase::Releasing);
	idle();
	gfxFill(C_BG);
}

static void testLeaveLatch() {
	section("Leave page: LEAVE takes only a press that landed while it was enabled");

	UiLeave l;
	UiLeaveText t = kstOwnText();
	const UiRect &w = UI_LEAVE_WIDE;
	uiLeaveOpen(&l, UiLeavePhase::Before, 1000);
	l.leaveEnabled = false;
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 0);
	l.leaveEnabled = true;
	uiLeaveTouch(&l, move(w.x + 20, w.y + 20), 10);
	gfxFill(C_RED);
	uiLeaveDraw(&l, t, &s_touch, 10);
	checkTrue("landed disabled, enabled under the finger: live, no pressed look",
	          buttonIs(w, "LEAVE", 2, true) && px(w.x + 2, w.y + w.h / 2) != C_INK);
	checkTrue("its lift does nothing, the page stays before LEAVE",
	          uiLeaveTouch(&l, lift(), 20) == UiLeaveEv::None && l.phase == UiLeavePhase::Before);
	idle();

	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 30);
	uiLeaveDraw(&l, t, &s_touch, 30);
	checkTrue("a press that lands enabled: the inner ring", px(w.x + 2, w.y + w.h / 2) == C_INK);
	l.leaveEnabled = false;
	checkTrue("disabled before the lift: the lift does nothing",
	          uiLeaveTouch(&l, lift(), 40) == UiLeaveEv::None && l.phase == UiLeavePhase::Before);
	idle();

	l.leaveEnabled = true;
	uiLeaveTouch(&l, press(w.x + 20, w.y + 20), 45);
	uiLeaveOpen(&l, UiLeavePhase::Before, 1000);
	checkTrue("a press from before the page opened again: its lift does nothing",
	          uiLeaveTouch(&l, lift(), 48) == UiLeaveEv::None && l.phase == UiLeavePhase::Before);
	idle();
	checkTrue("the next press that lands enabled leaves",
	          tapLeave(&l, w, 50) == UiLeaveEv::Leave && l.phase == UiLeavePhase::Releasing);
	idle();
	gfxFill(C_BG);
}

static void testLeaveSteps() {
	section("Leave page: step numbers and the room above the notes");

	UiLeave l;
	UiLeaveText t = {};
	t.step[0][0] = "UNPLUG THE SERVO.";
	t.step[1][0] = "RESTART DSHOT.";
	uiLeaveOpen(&l, UiLeavePhase::Releasing, 1000);
	gfxFill(C_RED);
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("a page opened releasing numbers its steps from 1",
	          fakeTextIs(UI_MARGIN, 66, "1", C_AMBER, C_BG, 1) &&
	          fakeTextIs(20, 66, "UNPLUG THE SERVO.", C_AMBER, C_BG, 1));
	checkTrue("the next step 14 px on, at y 80, numbered 2",
	          fakeTextIs(UI_MARGIN, 80, "2", C_DIM, C_BG, 1) && fakeTextIs(20, 80, "RESTART DSHOT.", C_DIM, C_BG, 1));
	checkTrue("a one-line step's bar: y 66 to y 72", px(1, 72) == C_AMBER && px(1, 73) == C_BG);

	// Three steps of three lines: the third step's second line would start
	// at y 154 and end at y 160, past the notes' top at y 158.
	for (int i = 0; i < UI_LEAVE_STEPS; i++)
		for (int k = 0; k < UI_LEAVE_STEP_LINES; k++) t.step[i][k] = "LINE";
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("the third step's first line at y 142",
	          fakeTextIs(20, 142, "LINE", C_DIM, C_BG, 1));
	int x0, x1;
	checkTrue("a line that would reach y 158 is not drawn", !fakeInkExtent(149, 9, C_BG, &x0, &x1));

	t.step[0][1] = t.step[0][2] = nullptr;
	t.step[1][1] = t.step[1][2] = nullptr;
	t.step[2][0] = "THIRD";
	t.step[2][1] = nullptr;
	t.step[2][2] = "NOT AFTER A NULL";
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("a null line ends its step",
	          fakeTextIs(20, 94, "THIRD", C_DIM, C_BG, 1) && !fakeInkExtent(101, 56, C_BG, &x0, &x1));
	t.step[1][0] = nullptr;
	uiLeaveDraw(&l, t, idle(), 0);
	checkTrue("a step with no first line ends the steps", !fakeInkExtent(74, 83, C_BG, &x0, &x1));
	gfxFill(C_BG);
}

void runWidgetTests() {
	testListGeometry();
	testListPressRule();
	testListRowRect();
	testListTap();
	testListScroll();
	testListSwipe();
	testSwipeApply();
	testListDraw();
	testHoldTiming();
	testHoldReset();
	testHoldEnabled();
	testHoldBar();
	testNullTouch();
	testHeaderBackDisabled();
	testHeaderRule();
	testDisabledFrame();
	testColourRoles();
	testPaletteValues();
	testPaletteDistinct();
	testPaletteContrast();
	testLayoutConstants();
	testStripTwoLines();
	testChip();
	testChipNeutral();
	testChangeText();
	testBackGuard();
	testBackGuardDraw();
	testListDrawField();
	testLeavePhases();
	testLeaveRestartWait();
	testLeaveSupplyNote();
	testLeaveDraw();
	testLeaveEnabled();
	testLeaveLatch();
	testLeaveSteps();
}
