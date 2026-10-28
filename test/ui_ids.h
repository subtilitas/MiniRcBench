/**
 * @file ui_ids.h
 * @brief Test helpers that address controls by id, through uiControls().
 *
 * A helper looks the control up on the current frame and acts at the centre
 * of its rectangle. A control that is absent fails the test, naming the id,
 * and the helper then does nothing. A tap or a press checks that the control
 * is enabled unless the call says the test expects it disabled, so a tap on
 * a disabled control is a statement of the test and not an accident.
 *
 * The including file defines `UI_IDS_FRAMES(n)` before the include: the
 * function that runs n UI frames of 25 ms.
 * @see src/ui_screen.h
 */

#pragma once

#include "check.h"
#include "fakes.h"
#include "ui_screen.h"
#include "ui_menu.h"
#include "gfx.h"

#include <stdio.h>

#ifndef UI_IDS_FRAMES
#error "define UI_IDS_FRAMES(n) before including ui_ids.h"
#endif

/**
 * @brief Where a helper reports a failure; null reports it through
 *        checkTrue(). A test of the helpers themselves points it at a counter.
 */
static void (*g_idFailHook)(const char *what) = nullptr;

/** @brief Report a helper's failure. */
static inline void idFail(const char *what) {
	if (g_idFailHook) g_idFailHook(what);
	else checkTrue(what, false);
}


/** @brief Whether the control expects to act. */
enum class IdExpect : uint8_t { Enabled, Disabled, Any };

/** @brief The id as text: `row 3 Plus` or `fixed 2`. */
static inline const char *idText(UiControlId id) {
	static char b[32];
	static const char *PARTS[] = { "Row", "Minus", "Plus", "Toggle" };
	if (id.row == UI_FIXED) snprintf(b, sizeof(b), "fixed %u", id.part);
	else snprintf(b, sizeof(b), "row %u %s", id.row, id.part < 4 ? PARTS[id.part] : "?");
	return b;
}

/** @brief The control @p id on the current frame, if it is there. */
static inline bool ctlFind(UiControlId id, UiControl *out) {
	UiControl t[UI_CONTROLS_MAX];
	uint8_t n = uiControls(t, UI_CONTROLS_MAX);
	for (uint8_t i = 0; i < n; i++) {
		if (t[i].id == id) { if (out) *out = t[i]; return true; }
	}
	return false;
}

/** @brief Whether the control @p id is on the current frame. */
static inline bool ctlPresent(UiControlId id) { return ctlFind(id, nullptr); }

/** @brief Whether the control @p id is on the current frame and acts. */
static inline bool ctlEnabled(UiControlId id) {
	UiControl c;
	return ctlFind(id, &c) && c.enabled;
}

/**
 * @brief The control @p id, failing the test when it is absent or does not
 *        meet @p want.
 * @return false when the caller should not act.
 */
static inline bool ctlGet(UiControlId id, UiControl *c, IdExpect want) {
	char what[64];
	if (!ctlFind(id, c)) {
		snprintf(what, sizeof(what), "control %s is on the screen", idText(id));
		idFail(what);
		return false;
	}
	if (want == IdExpect::Any) return true;
	bool ok = c->enabled == (want == IdExpect::Enabled);
	if (!ok) {
		snprintf(what, sizeof(what), "control %s is %s", idText(id),
		         want == IdExpect::Enabled ? "enabled" : "disabled");
		idFail(what);
	}
	return true;
}

/** @brief The rectangle of @p id; a zero rectangle when it is absent. */
static inline UiRect ctlRect(UiControlId id) {
	UiControl c;
	if (!ctlGet(id, &c, IdExpect::Any)) return UiRect{ 0, 0, 0, 0 };
	return c.rect;
}

/** @brief The centre of @p id; (-1, -1) when it is absent. */
static inline void ctlCentre(UiControlId id, int *x, int *y) {
	UiControl c;
	if (!ctlGet(id, &c, IdExpect::Any)) { *x = -1; *y = -1; return; }
	*x = c.rect.x + c.rect.w / 2;
	*y = c.rect.y + c.rect.h / 2;
}

/** @brief Put a finger down on @p id and run one frame. */
static inline void pressId(UiControlId id, IdExpect want = IdExpect::Enabled) {
	UiControl c;
	if (!ctlGet(id, &c, want)) return;
	fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
	UI_IDS_FRAMES(1);
}

/**
 * @brief Keep a finger on @p id for @p n frames, at the centre it had when
 *        the call began.
 */
static inline void holdId(UiControlId id, int n) {
	int x, y;
	ctlCentre(id, &x, &y);
	if (x < 0) return;
	for (int i = 0; i < n; i++) { fakeHold(x, y); UI_IDS_FRAMES(1); }
}

/** @brief Tap @p id: down for two frames, then up for one. */
static inline void tapId(UiControlId id, IdExpect want = IdExpect::Enabled) {
	UiControl c;
	if (!ctlGet(id, &c, want)) return;
	fakePress(c.rect.x + c.rect.w / 2, c.rect.y + c.rect.h / 2);
	UI_IDS_FRAMES(2);
	fakeRelease();
	UI_IDS_FRAMES(1);
}

/** @brief Tallest visible row of the open screen's list: a whole row's height. */
static inline int listRowH() {
	UiControl t[UI_CONTROLS_MAX];
	uint8_t n = uiControls(t, UI_CONTROLS_MAX);
	int h = 0;
	for (uint8_t i = 0; i < n; i++)
		if (t[i].id.row != UI_FIXED && t[i].rect.h > h) h = t[i].rect.h;
	return h;
}

/**
 * @brief Drag the open screen's list until row @p row is whole on screen.
 *
 * Each drag starts on a whole row, moves 24 px vertically and lifts: the
 * axis locks after 6 px, so it scrolls and selects nothing. Fails the test
 * when 40 drags do not bring the row in.
 */
static inline void revealRow(int row) {
	for (int attempt = 0; attempt < 40; attempt++) {
		UiControl t[UI_CONTROLS_MAX];
		uint8_t n = uiControls(t, UI_CONTROLS_MAX);
		int full = listRowH();
		int lo = 255, hi = -1, top = 0, bot = 0, gx = -1, gy = -1;
		UiControl me = {};
		bool found = false;
		for (uint8_t i = 0; i < n; i++) {
			const UiControl &c = t[i];
			if (c.id.row == UI_FIXED || c.id.part != (uint8_t)UiPart::Row) continue;
			if (c.id.row < lo) { lo = c.id.row; top = c.rect.y; }
			if (c.id.row > hi) { hi = c.id.row; bot = c.rect.y + c.rect.h; }
			if (c.rect.h == full && gx < 0) { gx = c.rect.x + c.rect.w / 2; gy = c.rect.y + c.rect.h / 2; }
			if (c.id.row == row) { me = c; found = true; }
		}
		if (found && me.rect.h == full) return;
		if (hi < 0 || gx < 0) break;
		// Further down the list, or cut at the viewport's bottom: move the
		// finger up, which scrolls the list down.
		bool down = row > hi || (found && me.rect.y + me.rect.h == bot && me.rect.y > top);
		int dy = down ? -4 : 4;
		fakePress(gx, gy); UI_IDS_FRAMES(1);
		for (int k = 1; k <= 6; k++) { fakeHold(gx, gy + k * dy); UI_IDS_FRAMES(1); }
		fakeRelease(); UI_IDS_FRAMES(1);
	}
	char what[48];
	snprintf(what, sizeof(what), "list row %d can be scrolled whole", row);
	idFail(what);
}

/**
 * @brief The MENU row that opens @p s: `UI_ROW(n, Row)`. With MENU open, the
 *        list is first dragged until that row is whole (revealRow()), as a
 *        finger reaches a row below the fold.
 */
static inline UiControlId menuRow(const UiScreen *s) {
	int row = uiMenuRowOf(s);
	if (uiCurrentScreen() == UiScreenId::Menu) revealRow(row);
	return UI_ROW(row, Row);
}

/**
 * @brief The open screen's table on the current frame: ids unique, every
 *        rectangle inside the panel and not empty, no two controls
 *        overlapping, and BACK at uiBackRect() on every screen but the
 *        tester.
 * @param where Names the frame in the check's text.
 */
static inline void checkControlTable(const char *where) {
	UiControl t[UI_CONTROLS_MAX];
	uint8_t n = uiControls(t, UI_CONTROLS_MAX);
	char what[96];
	bool ok = n < UI_CONTROLS_MAX;
	snprintf(what, sizeof(what), "%s: the table fits UI_CONTROLS_MAX", where);
	checkTrue(what, ok);
	bool inside = true, unique = true, apart = true;
	for (uint8_t i = 0; i < n; i++) {
		const UiRect &r = t[i].rect;
		if (r.w <= 0 || r.h <= 0 || r.x < 0 || r.y < 0 || r.x + r.w > GFX_W || r.y + r.h > GFX_H)
			inside = false;
		for (uint8_t j = i + 1; j < n; j++) {
			const UiRect &s = t[j].rect;
			if (t[i].id == t[j].id) unique = false;
			if (r.x < s.x + s.w && s.x < r.x + r.w && r.y < s.y + s.h && s.y < r.y + r.h)
				apart = false;
		}
	}
	snprintf(what, sizeof(what), "%s: every control lies inside the panel", where);
	checkTrue(what, inside);
	snprintf(what, sizeof(what), "%s: no id appears twice", where);
	checkTrue(what, unique);
	snprintf(what, sizeof(what), "%s: no two controls overlap", where);
	checkTrue(what, apart);
	if (uiCurrentScreen() != UiScreenId::Tester) {
		UiControl b;
		UiRect br = uiBackRect();
		bool back = ctlFind(UI_FIX(UI_BACK), &b) && b.rect.x == br.x && b.rect.y == br.y &&
		            b.rect.w == br.w && b.rect.h == br.h;
		snprintf(what, sizeof(what), "%s: BACK is in the table at its rectangle", where);
		checkTrue(what, back);
	}
}
