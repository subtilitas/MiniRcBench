/**
 * @file ui_screen.h
 * @brief The screens and their controls, by id.
 *
 * Every screen reports the controls it draws on the current frame: an id,
 * the rectangle the control is drawn at, and whether it acts. A drag
 * surface's grab margin beyond that rectangle is in its enum's doc. The
 * table is built from the same rectangles and the same expressions the
 * screen draws and hit-tests with, so a control the table lists at a
 * rectangle is the control a press there reaches.
 *
 * A control's id is a pair. A list row's control is `UI_ROW(n, part)`: row
 * n of the screen's list and a @ref UiPart. A control outside any list is
 * `UI_FIX(c)`, with c from the screen's own enum; `UI_FIX(UI_BACK)` is
 * BACK on every screen with a header.
 *
 * The host tests tap controls through this table rather than through
 * coordinates, so a layout change moves the table and not the tests.
 * @see docs/design/menu.md, "Tests by control id"
 */

#pragma once

#include <stdint.h>
#include "ui_widgets.h"

/** @brief The screens. SERVO and its SET view are one screen. */
enum class UiScreenId : uint8_t {
	Tester,  /**< The root: throttle, ARM, HOLD, CFG. */
	Cfg,     /**< SETTINGS, reached through CFG. */
	Setup,   /**< SETUP. */
	SdLog,   /**< SD LOG. */
	Am32,    /**< AM32 CONFIG. */
	Servo,   /**< SERVO, its SET view and its unplug page. */
	Kst,     /**< KST SERVO, not built: never open. */
};

/** @brief Row of a control outside any list. */
enum : uint8_t { UI_FIXED = 0xFF };

/** @brief BACK, the fixed control 0 of every screen with a header. */
enum : uint8_t { UI_BACK = 0 };

/** @brief Part of a list row. */
enum class UiPart : uint8_t {
	Row,     /**< The row itself: a tap selects it. */
	Minus,   /**< The row's -. */
	Plus,    /**< The row's +. */
	Toggle,  /**< The row's toggle. */
};

/** @brief Which control: a list row and its part, or UI_FIXED and the screen's own value. */
struct UiControlId {
	uint8_t row;   /**< List row n, or `UI_FIXED`. */
	uint8_t part;  /**< A `UiPart` for a list row; the screen's own value for UI_FIXED. */
};

/** @brief Part @p p of list row @p n. */
#define UI_ROW(n, p) (UiControlId{ (uint8_t)(n), (uint8_t)UiPart::p })
/** @brief The fixed control @p c. */
#define UI_FIX(c) (UiControlId{ UI_FIXED, (uint8_t)(c) })

/** @brief Same control. */
inline bool operator==(UiControlId a, UiControlId b) {
	return a.row == b.row && a.part == b.part;
}

/** @brief One control on the current frame. */
struct UiControl {
	UiControlId id;
	UiRect      rect;     /**< Where it takes a press, clipped to its list's viewport; a drag surface's as drawn, with its margin in its enum's doc. */
	bool        enabled;  /**< Acts; false where it is drawn disabled and gated on the same value. */
};

/** @brief Most controls one screen reports. */
#define UI_CONTROLS_MAX 32

/** @brief A screen: its id and its control table. */
struct UiScreen {
	UiScreenId id;
	/** @brief Writes at most max controls of the current frame to out, and returns how many. */
	uint8_t (*controls)(UiControl *out, uint8_t max);
};

/** @brief The open screen. */
UiScreenId uiCurrentScreen();

/**
 * @brief The open screen's controls on the current frame.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiControls(UiControl *out, uint8_t max);

/**
 * @brief Append a control to a table, if there is room.
 * @param out Table.
 * @param n   Controls in it; advanced by one when there is room.
 * @param max Room in @p out.
 * @param id  The control.
 * @param r   Its rectangle.
 * @param enabled Whether it acts.
 */
inline void uiControlAdd(UiControl *out, uint8_t *n, uint8_t max,
                         UiControlId id, UiRect r, bool enabled) {
	if (*n < max) out[(*n)++] = UiControl{ id, r, enabled };
}
