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

#include <stddef.h>
#include <stdint.h>
#include "ui_widgets.h"
#include "pin_claim.h"

struct TouchState;

/** @brief The screens. SERVO and its SET view are one screen. */
enum class UiScreenId : uint8_t {
	Tester,    /**< The root: throttle, ARM, LATCH, MENU, the two readouts. */
	Menu,      /**< MENU, the list of tools and actions. */
	Settings,  /**< SETTINGS, one list in sections. */
	SdLog,     /**< SD LOG. */
	Am32,      /**< ESC SETTINGS, opened from ESC CONFIG. */
	Servo,     /**< SERVO, its SET view and its leave page. */
	Kst,       /**< KST SERVO: the search, the field list, FILES, the write and leave pages. */
	Hitec,     /**< HITEC SERVO: the entry, the field list, FILES, the write and leave pages. */
	EscCmd,    /**< ESC COMMANDS, MENU's first row: BEEP 1 to 5, direction and 3D mode. */
	ServoProg, /**< PROGRAMMER, the chooser behind MENU's SERVO PROGRAMMER row. */
	RxSignals, /**< RX SIGNALS, the chooser behind MENU's RX SIGNALS row: MONITOR and OUTPUT. */
	Sbus,      /**< S.BUS MONITOR and its leave page, opened from RX SIGNALS. */
	SbusOut,   /**< S.BUS OUTPUT, its SET view and its leave page, opened from RX SIGNALS. */
	EscConfig, /**< ESC CONFIG, the chooser behind MENU's ESC CONFIG row: ESC SETTINGS and USB BRIDGE. */
	Bridge,    /**< USB BRIDGE and its leave page, opened from ESC CONFIG. */
	Tools,     /**< TOOLS, the chooser behind MENU's TOOLS row: ANGLE METER. */
	Angle,     /**< ANGLE METER, its SETUP and its FLIP CAL, opened from TOOLS. */
	Count,     /**< How many screens; not a screen. */
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

/** @brief How a SETTINGS row is set. */
enum class SettingsKind : uint8_t {
	Stepper,   /**< `-` and `+`: a step on touch-down, repeating while held. */
	Toggle,    /**< One wide button, fires on release. */
	ReadOnly,  /**< A value and no control. */
};

/** @brief One SETTINGS row. */
struct SettingsRow {
	const char  *label;   /**< Scale 1. */
	SettingsKind kind;
	uint8_t      widest;  /**< Widest value, characters, for the fit check. */
	/** @brief Writes the value to buf, at most n bytes, and its colour to col. */
	void (*value)(char *buf, size_t n, uint16_t *col);
	/** @brief Whether the row's control acts; null: always. */
	bool (*enabled)();
	/** @brief Stepper: one step, -1 or +1, to legal values only. */
	void (*step)(int dir);
	/** @brief Toggle: flip the setting. */
	void (*toggle)();
	/** @brief Toggle: whether the setting is on, which draws it filled; null: never. */
	bool (*on)();
	/**
	 * @brief A caption under the label, scale 1: writes it to buf, at most n
	 *        bytes, and its colour to col; an empty string for none. Null:
	 *        the row has none, and its label is centred.
	 */
	void (*caption)(char *buf, size_t n, uint16_t *col);
	/**
	 * @brief Stepper: the connector pin under the value, scale 1, right to
	 *        the value's end, such as `J5 PIN 2`: writes it to buf, at most n
	 *        bytes; an empty string for none. Null: the row has none, and
	 *        its value is centred in the row.
	 */
	void (*sub)(char *buf, size_t n);
};

/** @brief A captioned run of SETTINGS rows. */
struct SettingsSection {
	const char        *caption;  /**< ESC TESTER, WIRING, ... */
	const SettingsRow *rows;
	uint8_t            count;
};

/** @brief A screen: what MENU shows of it and how it runs. */
struct UiScreen {
	UiScreenId  id;
	const char *title;    /**< Header text, scale 2. */
	PinUser     pinUser;  /**< The claim it takes; None: claims nothing. */
	/**
	 * @brief Writes the MENU row's status to buf, at most n bytes, its text
	 *        colour to col and the row's fill to fill. Null: no status.
	 */
	void (*status)(char *buf, size_t n, uint16_t *col, uint16_t *fill);
	/** @brief Open the screen; null for an action. */
	void (*enter)();
	/** @brief One frame; false once BACK has left. Null for an action. */
	bool (*tick)(const TouchState *t);
	/** @brief The action of an action row; null for a screen. */
	void (*action)();
	/** @brief Writes at most max controls of the current frame to out, and returns how many. */
	uint8_t (*controls)(UiControl *out, uint8_t max);
	/** @brief The tool's persisted SETTINGS rows, or null. */
	const SettingsSection *settings;
	/**
	 * @brief Opens only while the pin claim is Free, without claiming:
	 *        SETTINGS, which changes the wiring a claim latched; ESC
	 *        COMMANDS, which sends on SIG; and the angle meter, which saves
	 *        to flash as SETTINGS does. A tool with a @ref pinUser waits
	 *        for Free anyway. The hubs open whatever the claim's state and
	 *        gate each entry instead (ui_hub.h).
	 */
	bool freePins;
};

/**
 * @brief Open @p s from the open screen, as MENU opens its rows: after this
 *        frame's tick the navigation pushes @p s over the open screen
 *        through uiOpenTool() in ui.cpp, running @p enter in place of
 *        `s->enter`. BACK there, and the leave page's last button, pop it
 *        back to the open screen (docs/design/menu-c.md owner decision 7).
 *        A hub's entries call it (ui_hub.h).
 * @param s     The tool.
 * @param enter Its entry point for this route; null runs `s->enter`.
 */
void uiChooseTool(const UiScreen *s, void (*enter)());

/** @brief One MENU row. */
struct UiMenuEntry {
	const char     *label;         /**< Scale 2. */
	uint8_t         statusWidest;  /**< Widest status, characters, for the fit check. */
	const UiScreen *screen;
};

/**
 * @brief @p s is refused while the pin claim is not Free: it claims pins
 *        (@ref UiScreen::pinUser), or opens only while they are free
 *        (@ref UiScreen::freePins). MENU refuses its row on it, and the
 *        tester its readouts.
 * @param s The screen.
 * @return true while @p s may not open.
 */
inline bool uiPinsRefuse(const UiScreen *s) {
	return (s->pinUser != PinUser::None || s->freePins) &&
	       pinClaimState() != ClaimState::Free;
}

/** @brief The open screen. */
UiScreenId uiCurrentScreen();

/**
 * @brief Most screens on the navigation stack over the tester: MENU, a hub,
 *        a hub's tool (docs/design/menu-c.md, "Navigation").
 */
#define UI_NAV_MAX 3

/**
 * @brief How many screens are on the navigation stack over the tester: 0
 *        while the tester is open, at most @ref UI_NAV_MAX.
 */
uint8_t uiNavDepth();

/**
 * @brief The screen at depth @p i of the navigation stack, 0 the bottom.
 *        The open screen is at uiNavDepth() - 1.
 * @param i The depth, from 0.
 * @return Its id; `UiScreenId::Tester`, the root, for @p i at or past
 *         uiNavDepth().
 */
UiScreenId uiNavAt(uint8_t i);

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
