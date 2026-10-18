/**
 * @file test_widgets.cpp
 * @brief The shared list and hold widgets, driven directly with touch
 *        snapshots: src/ui_list.cpp and src/ui_hold.cpp as shipped.
 *
 * The screens that use them, AM32 CONFIG, SERVO, SERVO SET and SETUP, are
 * tested through the UI in test_ui.cpp and test_servo.cpp. This file holds
 * each rule at its boundary.
 */

#include "check.h"
#include "config.h"
#include "gfx.h"
#include "touch.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_list.h"
#include "ui_widgets.h"

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

	// A row boundary crossed without locking: the tap names the row pressed.
	uiListTouch(&l, press(100, 56 + 25));
	uiListTouch(&l, move(100, 56 + 29));
	checkTrue("4 px across a row edge: the row pressed", isEv(uiListTouch(&l, lift()), UiListEv::Tap, 0));

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
}
