/**
 * @file ui_settings.cpp
 * @brief SETTINGS: one list in sections, LINK, the note line and HOLD TO
 *        SAVE. @see ui_settings.h
 *
 * The rules that matter more than the widgets:
 *
 * - **Only legal values can be selected.** The `-`/`+` buttons step through
 *   the board's free-GPIO mask and past whichever pin the other wire is on,
 *   so there is no invalid pin to reject and no error state to design.
 *   @see stepPinAvoiding()
 * - **Changes apply live; only persistence needs the hold.** The one-second
 *   hold is about writing flash, which parks core1, not about the value.
 * - **A press on a control never scrolls the list.** A drag scrolls only
 *   when it starts off a control, on a caption, a label, a value or a gap.
 */

#include "ui_settings.h"
#include "ui_menu.h"
#include "settings.h"
#include "esc_task.h"
#include "ui.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_list.h"
#include "ui_widgets.h"
#include "gfx.h"
#include "st7789.h"
#include "theme.h"
#include "touch.h"
#include "config.h"
#include "board_desc.h"
#include "board_pins.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

/**
 * @defgroup settings_layout SETTINGS layout (240 x 320 portrait)
 * @brief The list under the header, then LINK, the note line and the footer,
 *        which do not move.
 * @{
 */
#define VIEW_Y0     UI_STRIP_Y  /**< First row of the list, y 40: SETTINGS has no strip. */
#define VIEW_H      200         /**< To y 239. */
#define CAP_H       20          /**< A section caption. */
#define ROW_H       44          /**< A row's controls. */
#define ROW_PITCH   50          /**< Row top to row top. */
#define VAL_R       132         /**< A stepper's value ends here. */
#define BTN_W       44          /**< `-` and `+`. */
#define BTN_M_X     138
#define BTN_P_X     188
#define TOGGLE_W    94          /**< From BTN_M_X to x 231. */
#define RO_R        232         /**< A read-only value ends here. */
#define BAR_X       235         /**< The scroll bar. */
#define LINK_Y      242         /**< Live link readout. */
#define NOTE_Y      254         /**< One line saying what state the settings are in. */
#define FOOT_Y      268
#define FOOT_H      48
#define SAVE_X      8
#define SAVE_W      150
#define DEF_X       162
#define DEF_W       70
/** @} */

static_assert(VIEW_Y0 >= UI_HDR_H, "the list overlaps the header");
static_assert(VIEW_Y0 + VIEW_H <= LINK_Y, "the list overlaps LINK");
static_assert(ROW_PITCH > ROW_H, "SETTINGS rows have no gap");
static_assert(ROW_H >= UI_LIST_TAP_MIN && BTN_W >= UI_LIST_TAP_MIN &&
              TOGGLE_W >= UI_LIST_TAP_MIN && FOOT_H >= UI_LIST_TAP_MIN,
              "a SETTINGS control is smaller than the list floor");
static_assert(NOTE_Y >= LINK_Y + 7, "the note overlaps LINK");
static_assert(FOOT_Y >= NOTE_Y + 7 + UI_HOLD_BAR_H, "the footer overlaps the note");
static_assert(FOOT_Y + FOOT_H <= GFX_H, "the footer runs off the panel");
static_assert(BTN_M_X + BTN_W < BTN_P_X, "the row buttons overlap");
static_assert(BTN_P_X + BTN_W < BAR_X && BTN_M_X + TOGGLE_W < BAR_X,
              "a row button runs into the scroll bar");
static_assert(DEF_X + DEF_W <= GFX_W - 8, "RESET runs off the panel");

/** @brief Backlight adjustment step. 8 levels across the usable range. */
#define BACKLIGHT_STEP 32

static const UiRect BTN_SAVE     = {SAVE_X, FOOT_Y, SAVE_W, FOOT_H};
static const UiRect BTN_DEFAULTS = {DEF_X,  FOOT_Y, DEF_W,  FOOT_H};

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static bool     s_redraw = true;
/** @brief A row's step or toggle changed a setting this frame. */
static bool     s_changed = false;
/**
 * @brief HOLD TO SAVE. It fires once per press (ui_hold.h): a finger kept on
 *        the button does not save again a second later, and each save is a
 *        flash erase.
 */
static UiHold   s_saveHold;

/** @brief What the last save attempt did. Drives the note line. */
enum class SaveOutcome : uint8_t { None, Ok, Failed, FailedArmed };
static SaveOutcome s_saveOutcome = SaveOutcome::None;

/** @brief True when validation last had to repair something the user asked for. */
static bool s_repaired = false;
/** @brief True when that repair was specifically KISS being switched off. */
static bool s_repairKiss = false;

/** @brief Cached LINK values, so the live row repaints only when it changes. */
static int s_shownRate = -1, s_shownErr = -1;

/** @brief This frame's touch. */
static const TouchState *s_t = nullptr;

/** @brief The list: pixel content, scrolled by a drag off the controls. */
static UiList s_list;

/** @brief One item of the list: a caption or a row, at a content y. */
struct Item {
	const SettingsSection *sec;
	const SettingsRow     *row;   /**< Null for the section's caption. */
	int16_t                y;     /**< Content y of its top. */
	uint8_t                index; /**< The row's number for UI_ROW(); 0 for a caption. */
};

#define MAX_SECTIONS 12  /**< 2 core sections, up to 8 tool sections, DISPLAY and BOARD. */
#define MAX_ITEMS 40     /**< Captions and rows; items past it are not shown. */
#define MAX_ROWS  24     /**< Rows; rows past it are not shown. */
static Item    s_items[MAX_ITEMS];
static uint8_t s_itemCount = 0;
static const SettingsSection *s_secs[MAX_SECTIONS];
static uint8_t s_secCount = 0;
/** @brief Held-repeat state, one per row. @see ui_input.h */
static Repeat  s_rep[MAX_ROWS];

/** @brief The press landed on a live control: the list does not see it. */
static bool    s_ctlPress = false;
static uint8_t s_pressRow = 0;

// ---------------------------------------------------------------------------
// the rows
// ---------------------------------------------------------------------------

/**
 * @brief Step a pin, stepping past the one the other wire is on.
 *
 * The ESC and the telemetry wire cannot share a GPIO. On the 2.8" board
 * exactly two pins are free, so whichever one you are not on is the other
 * wire's. The validator's answer to a collision is to switch KISS off, which
 * is right for a stored block nobody chose and wrong for a stepper.
 *
 * @param from   Current pin.
 * @param dir    +1 or -1.
 * @param avoid  The other wire's pin.
 * @param active Whether the other wire is in use.
 * @return The next legal pin, or @p from when the board offers no other.
 */
static uint8_t stepPinAvoiding(uint8_t from, int dir, uint8_t avoid, bool active) {
	uint8_t p = settingsNextPin(from, dir);
	if (active && p == avoid) p = settingsNextPin(p, dir);
	// Two free pins and both spoken for: stay put rather than landing on the
	// pin this function exists to avoid.
	if (active && p == avoid) return from;
	return p;
}

static void polesValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "%u", settings()->poles); *c = C_TEXT; }
static void polesStep(int dir) {
	int p = (int)settings()->poles + 2 * dir;
	if (p < MIN_MOTOR_POLES || p > MAX_MOTOR_POLES) return;
	settings()->poles = (uint8_t)p;
	s_changed = true;
}

static void maxValue(char *b, size_t n, uint16_t *c) {
	snprintf(b, n, "%u%%", (unsigned)((uint32_t)settings()->maxThrottle * 100 / 2000));
	*c = C_AMBER;
}
static void maxStep(int dir) {
	int t = (int)settings()->maxThrottle + MAX_THROTTLE_STEP * dir;
	if (t < MAX_THROTTLE_STEP || t > MAX_THROTTLE_CEILING) return;
	settings()->maxThrottle = (uint16_t)t;
	s_changed = true;
}

static void pinValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "GP%u", settings()->dshotPin); *c = C_LIME; }
static void pinStep(int dir) {
	Settings *s = settings();
	uint8_t p = stepPinAvoiding(s->dshotPin, dir, s->kissPin, s->kissEnable != 0);
	if (p == s->dshotPin) return;
	s->dshotPin = p;
	s_changed = true;
}

static void speedValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "%u", settings()->dshotKbaud); *c = C_TEXT; }
static void speedStep(int dir) {
	Settings *s = settings();
	if (dir < 0) s->dshotKbaud = (uint16_t)(s->dshotKbaud <= 150 ? 1200 : s->dshotKbaud / 2);
	else         s->dshotKbaud = (uint16_t)(s->dshotKbaud >= 1200 ? 150 : s->dshotKbaud * 2);
	s_changed = true;
}

static void kissValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "%s", settings()->kissEnable ? "ON" : "OFF"); *c = C_TEXT; }
static bool kissOn() { return settings()->kissEnable != 0; }
static void kissToggle() {
	// Turning KISS on moves it off the ESC's pin rather than enabling it
	// against a collision and letting validation switch it straight back off.
	Settings *s = settings();
	if (s->kissEnable) {
		s->kissEnable = 0;
	} else {
		s->kissEnable = 1;
		if (s->kissPin == s->dshotPin) s->kissPin = settingsNextPin(s->kissPin, +1);
	}
	s_changed = true;
}

static void kissPinValue(char *b, size_t n, uint16_t *c) {
	// With KISS off this row has nothing to set: the value reads "--" and the
	// buttons are drawn unavailable and do not act. @see #23
	if (settings()->kissEnable) { snprintf(b, n, "GP%u", settings()->kissPin); *c = C_CYAN; }
	else                        { snprintf(b, n, "--"); *c = C_DIM; }
}
static void kissPinStep(int dir) {
	Settings *s = settings();
	uint8_t p = stepPinAvoiding(s->kissPin, dir, s->dshotPin, true);
	if (p == s->kissPin) return;
	s->kissPin = p;
	s_changed = true;
}

static void contrastValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "%s", settings()->highContrast ? "HIGH" : "NORMAL"); *c = C_TEXT; }
static bool contrastOn() { return settings()->highContrast != 0; }
static void contrastToggle() {
	settings()->highContrast = settings()->highContrast ? 0 : 1;
	s_changed = true;
}

static void backlightValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "%u", settings()->backlight); *c = C_TEXT; }
static void backlightStep(int dir) {
	Settings *s = settings();
	uint8_t b;
	if (dir < 0) {
		// Stops at 16, the validator's floor: 40 - 32 would be 8, which the
		// validator lifts back with an ADJUSTED note.
		b = (uint8_t)(s->backlight < 16 + BACKLIGHT_STEP ? 16 : s->backlight - BACKLIGHT_STEP);
	} else {
		int v = s->backlight + BACKLIGHT_STEP;
		b = (uint8_t)(v > 255 ? 255 : v);
	}
	if (b == s->backlight) return;
	s->backlight = b;
	s_changed = true;
}

static void boardValue(char *b, size_t n, uint16_t *c) { snprintf(b, n, "%s", g_board->label); *c = C_GRID; }

static constexpr SettingsRow ROWS_TESTER[] = {
	{ "MOTOR POLES",  SettingsKind::Stepper, 2, polesValue, nullptr, polesStep, nullptr, nullptr },
	{ "MAX THROTTLE", SettingsKind::Stepper, 4, maxValue,   nullptr, maxStep,   nullptr, nullptr },
};
static constexpr SettingsRow ROWS_WIRING[] = {
	{ "ESC PIN",      SettingsKind::Stepper, 4, pinValue,     nullptr, pinStep,     nullptr,    nullptr },
	{ "DSHOT KBAUD",  SettingsKind::Stepper, 4, speedValue,   nullptr, speedStep,   nullptr,    nullptr },
	{ "KISS TELEM",   SettingsKind::Toggle,  3, kissValue,    nullptr, nullptr,     kissToggle, kissOn  },
	{ "KISS PIN",     SettingsKind::Stepper, 4, kissPinValue, kissOn,  kissPinStep, nullptr,    nullptr },
};
static constexpr SettingsRow ROWS_DISPLAY[] = {
	{ "CONTRAST",     SettingsKind::Toggle,  6, contrastValue,  nullptr, nullptr,       contrastToggle, contrastOn },
	{ "BACKLIGHT",    SettingsKind::Stepper, 3, backlightValue, nullptr, backlightStep, nullptr,        nullptr },
};
/** @brief BUILT FOR's widest value: the board's label, characters. */
#define BUILT_FOR_WIDEST 20
static_assert(sizeof(BOARD_LABEL) - 1 <= BUILT_FOR_WIDEST,
              "the board label is wider than BUILT FOR's value column");
static constexpr SettingsRow ROWS_BOARD[] = {
	{ "BUILT FOR", SettingsKind::ReadOnly, BUILT_FOR_WIDEST, boardValue, nullptr, nullptr, nullptr, nullptr },
};

#define COUNT(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))
static constexpr SettingsSection SEC_TESTER  = { "ESC TESTER", ROWS_TESTER,  COUNT(ROWS_TESTER) };
static constexpr SettingsSection SEC_WIRING  = { "WIRING",     ROWS_WIRING,  COUNT(ROWS_WIRING) };
static constexpr SettingsSection SEC_DISPLAY = { "DISPLAY",    ROWS_DISPLAY, COUNT(ROWS_DISPLAY) };
static constexpr SettingsSection SEC_BOARD   = { "BOARD",      ROWS_BOARD,   COUNT(ROWS_BOARD) };

/** @brief Characters of @p s. */
static constexpr int textLen(const char *s) {
	int n = 0;
	while (s[n]) n++;
	return n;
}

/** @brief gfxTextW()'s rule: (6 x characters - 1) x scale, 0 for none. */
static constexpr int textW(int chars, int scale) {
	return chars > 0 ? (6 * chars - 1) * scale : 0;
}

/**
 * @brief Every row's label, at scale 1 from x 8, ends at least 4 px before
 *        its widest value or its control begins.
 * @param s A section.
 */
static constexpr bool settingsSectionFits(const SettingsSection &s) {
	for (uint8_t i = 0; i < s.count; i++) {
		const SettingsRow &r = s.rows[i];
		int start = r.kind == SettingsKind::Stepper ? VAL_R - textW(r.widest, 2)
		          : r.kind == SettingsKind::Toggle  ? BTN_M_X
		                                            : RO_R - textW(r.widest, 1);
		if (UI_MARGIN + textW(textLen(r.label), 1) + 4 > start) return false;
		if (r.kind == SettingsKind::Toggle && textW(r.widest, 2) > TOGGLE_W - 8) return false;
	}
	return true;
}
static_assert(settingsSectionFits(SEC_TESTER), "an ESC TESTER label runs into its value");
static_assert(settingsSectionFits(SEC_WIRING), "a WIRING label runs into its value");
static_assert(settingsSectionFits(SEC_DISPLAY), "a DISPLAY label runs into its value");
static_assert(settingsSectionFits(SEC_BOARD), "a BOARD label runs into its value");

// ---------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------

/** @brief Lay the sections out: the core ones, each tool's in registry order, then DISPLAY and BOARD. */
static void buildItems() {
	const SettingsSection *secs[MAX_SECTIONS];
	int ns = 0;
	secs[ns++] = &SEC_TESTER;
	secs[ns++] = &SEC_WIRING;
	uint8_t n;
	const UiMenuEntry *e = uiMenuEntries(&n);
	for (uint8_t i = 0; i < n && ns < MAX_SECTIONS - 2; i++)
		if (e[i].screen->settings) secs[ns++] = e[i].screen->settings;
	secs[ns++] = &SEC_DISPLAY;
	secs[ns++] = &SEC_BOARD;
	memcpy(s_secs, secs, sizeof(secs[0]) * (size_t)ns);
	s_secCount = (uint8_t)ns;

	s_itemCount = 0;
	int y = 0;
	uint8_t row = 0;
	for (int i = 0; i < ns; i++) {
		if (s_itemCount >= MAX_ITEMS) break;
		s_items[s_itemCount++] = Item{ secs[i], nullptr, (int16_t)y, 0 };
		y += CAP_H;
		for (uint8_t r = 0; r < secs[i]->count && s_itemCount < MAX_ITEMS && row < MAX_ROWS; r++) {
			s_items[s_itemCount++] = Item{ secs[i], &secs[i]->rows[r], (int16_t)y, row++ };
			y += ROW_PITCH;
		}
	}
	uiListInitPx(&s_list, UiRect{ 0, VIEW_Y0, GFX_W, VIEW_H }, y);
}

/** @brief Top of item @p it on the panel at the current scroll. */
static int topOf(const Item &it) { return VIEW_Y0 + it.y - s_list.scrollPx; }

/** @brief @p r cut to the viewport; its height is 0 when none of it shows. */
static UiRect clipToView(UiRect r) {
	int top = r.y, bot = r.y + r.h;
	if (top < VIEW_Y0) top = VIEW_Y0;
	if (bot > VIEW_Y0 + VIEW_H) bot = VIEW_Y0 + VIEW_H;
	if (bot < top) bot = top;
	return UiRect{ r.x, (int16_t)top, r.w, (int16_t)(bot - top) };
}

/** @brief A row's control: part `Minus`, `Plus` or `Toggle`, as drawn. */
static UiRect partRect(const Item &it, UiPart part) {
	int16_t y = (int16_t)topOf(it);
	switch (part) {
		case UiPart::Minus:  return UiRect{ BTN_M_X, y, BTN_W, ROW_H };
		case UiPart::Plus:   return UiRect{ BTN_P_X, y, BTN_W, ROW_H };
		case UiPart::Toggle: return UiRect{ BTN_M_X, y, TOGGLE_W, ROW_H };
		default:             return UiRect{ 0, y, BTN_M_X, ROW_H };
	}
}

/** @brief Whether a cut control shows enough of itself to take a press. */
static bool showsEnough(const UiRect &clipped) { return clipped.h >= UI_LIST_TAP_MIN; }

static bool rowEnabled(const SettingsRow &r) { return !r.enabled || r.enabled(); }

/**
 * @brief The row whose live control holds the point (@p x, @p y): enabled,
 *        and showing enough of itself. A press anywhere else scrolls.
 * @param x,y      The point.
 * @param[out] row The row's item index.
 * @return true when a control that shows enough of itself holds the point.
 */
static bool controlAt(int x, int y, uint8_t *row) {
	for (uint8_t i = 0; i < s_itemCount; i++) {
		const Item &it = s_items[i];
		if (!it.row || it.row->kind == SettingsKind::ReadOnly || !rowEnabled(*it.row)) continue;
		const UiPart parts[2] = {
			it.row->kind == SettingsKind::Toggle ? UiPart::Toggle : UiPart::Minus,
			it.row->kind == SettingsKind::Toggle ? UiPart::Toggle : UiPart::Plus,
		};
		for (UiPart p : parts) {
			UiRect c = clipToView(partRect(it, p));
			if (showsEnough(c) && x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
				*row = i;
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------

/** @brief Whether the press on the screen is on control @p c of item @p i. */
static bool controlPressed(uint8_t i, const UiRect &c) {
	return s_ctlPress && s_pressRow == i && uiPressing(c, s_t);
}

static void drawItem(uint8_t i) {
	const Item &it = s_items[i];
	int y = topOf(it);
	if (!it.row) {
		gfxText(UI_MARGIN, y + 8, it.sec->caption, C_CYAN, 1);
		return;
	}
	const SettingsRow &r = *it.row;
	gfxText(UI_MARGIN, y + (ROW_H - 7) / 2, r.label, C_DIM, 1);
	char v[32];
	uint16_t col = C_TEXT;
	r.value(v, sizeof(v), &col);
	bool en = rowEnabled(r);
	switch (r.kind) {
		case SettingsKind::Stepper: {
			gfxText(VAL_R - gfxTextW(v, 2), y + (ROW_H - 14) / 2, v, col, 2);
			UiRect m = partRect(it, UiPart::Minus), p = partRect(it, UiPart::Plus);
			uiButton(m, "-", C_PANEL, C_TEXT, 2, controlPressed(i, clipToView(m)), en);
			uiButton(p, "+", C_PANEL, C_TEXT, 2, controlPressed(i, clipToView(p)), en);
			break;
		}
		case SettingsKind::Toggle: {
			bool on = r.on && r.on();
			UiRect t = partRect(it, UiPart::Toggle);
			uiButton(t, v, on ? C_BLUE : C_PANEL, on ? C_ONACCENT : C_DIM, 2,
			         controlPressed(i, clipToView(t)), en);
			break;
		}
		case SettingsKind::ReadOnly:
			gfxText(RO_R - gfxTextW(v, 1), y + (ROW_H - 7) / 2, v, col, 1);
			break;
	}
}

/**
 * @brief The note line: the single most useful thing to say right now.
 *
 * Ordered by urgency: a failed save outranks everything; a setting the
 * firmware had to repair outranks the saved or unsaved state, because the
 * user asked for something and did not get it.
 *
 * @param[out] col Colour to draw it in.
 * @return The text.
 */
static const char *noteText(uint16_t *col) {
	if (s_saveOutcome == SaveOutcome::FailedArmed) {
		*col = C_RED;
		return "NOT WHILE ARMED - DISARM TO SAVE";
	}
	if (s_saveOutcome == SaveOutcome::Failed) {
		*col = C_RED;
		return "SAVE FAILED - FLASH REFUSED THE WRITE";
	}
	if (s_repaired) {
		*col = C_AMBER;
		return s_repairKiss ? "KISS OFF: NEEDS A PIN OF ITS OWN"
		                    : "ADJUSTED TO THIS BOARD'S LIMITS";
	}
	if (settingsDirty()) {
		*col = C_AMBER;
		return "UNSAVED - HOLD SAVE TO KEEP";
	}
	if (s_saveOutcome == SaveOutcome::Ok) {
		*col = C_LIME;
		return "SAVED";
	}
	*col = C_DIM;
	return settingsStored() ? "SAVED SETTINGS IN USE"
	                        : "COMPILED DEFAULTS IN USE";
}

/** @brief Repaint the live LINK row if its numbers moved. */
static void drawLink(bool force) {
	EscTelemetry t;
	escSnapshot(&t);
	if (!force && s_shownRate == (int)t.packetRate && s_shownErr == (int)t.errPercent)
		return;
	s_shownRate = t.packetRate;
	s_shownErr  = t.errPercent;

	gfxRect(0, LINK_Y, GFX_W, 8, C_BG);
	gfxText(UI_MARGIN, LINK_Y, "LINK", C_DIM, 1);

	char buf[32];
	snprintf(buf, sizeof(buf), "%d/S  %d%% ERR", (int)t.packetRate, (int)t.errPercent);
	// Green as soon as anything is arriving: frames are coming back, so the
	// pin is right.
	uint16_t col = t.packetRate == 0 ? C_DIM
	             : t.errPercent > 5  ? C_AMBER
	                                 : C_LIME;
	gfxText(GFX_W - UI_MARGIN - gfxTextW(buf, 1), LINK_Y, buf, col, 1);
}

static void drawAll() {
	gfxFill(C_BG);
	uiHeader("SETTINGS", s_t);

	gfxSetClip(0, VIEW_Y0, GFX_W, VIEW_H);
	for (uint8_t i = 0; i < s_itemCount; i++) {
		int y = topOf(s_items[i]);
		int h = s_items[i].row ? ROW_H : CAP_H;
		if (y + h <= VIEW_Y0 || y >= VIEW_Y0 + VIEW_H) continue;
		drawItem(i);
	}
	gfxClearClip();
	uiListDrawBar(&s_list, BAR_X);

	drawLink(true);

	uint16_t noteCol;
	const char *note = noteText(&noteCol);
	gfxRect(0, NOTE_Y, GFX_W, 8, C_BG);
	gfxText(UI_MARGIN, NOTE_Y, note, noteCol, 1);

	// Progress fills across the button itself, so the thing you are pressing
	// is the thing that reports.
	bool holding = s_saveHold.holding;
	uiHoldDrawBar(&s_saveHold, BTN_SAVE, FOOT_Y - 4, millis());
	uiButton(BTN_SAVE, "HOLD TO SAVE", holding ? C_BLUE : C_PANEL,
	         holding ? C_ONACCENT : C_TEXT, 1, holding);
	uiButton(BTN_DEFAULTS, "RESET", C_PANEL, C_AMBER, 1,
	         uiPressing(BTN_DEFAULTS, s_t));
}

/**
 * @brief Push the current wiring to core1 and re-apply the display settings.
 *
 * Called after every edit. Each call is a no-op when nothing changed, so it
 * can be unconditional rather than tracking which field moved.
 */
static void applyLive() {
	Settings *s = settings();
	uint8_t kissBefore = s->kissEnable;
	s_repaired = !settingsValidate(s);
	s_repairKiss = s_repaired && kissBefore && !s->kissEnable;

	escTaskConfigure(s->dshotPin, s->dshotKbaud, s->kissEnable != 0, s->kissPin);
	// Pure bookkeeping (the eRPM divisor), so it always follows the settings:
	// RESET restores the pole count the RPM readout divides by.
	escSetPoles(s->poles);
	themeSet(s->highContrast ? Theme::HighContrast : Theme::Dark);
	st7789SetBacklight(themeBacklight(s->backlight));
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void uiSettingsEnter() {
	s_redraw = true;
	s_changed = false;
	uiHoldReset(&s_saveHold);
	s_saveOutcome = SaveOutcome::None;
	s_shownRate = -1;
	s_shownErr = -1;
	s_ctlPress = false;
	memset(s_rep, 0, sizeof(s_rep));
	buildItems();
	applyLive();
}

bool uiSettingsTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	uint32_t now = millis();

	// --- hold to save: every frame, finger-off included, so moving away
	//     cancels; only a press that began on the button counts ---
	bool wasHolding = s_saveHold.holding;
	if (uiHoldUpdate(&s_saveHold, BTN_SAVE, s_t, true, now)) {
		// Refused while armed: erasing flash parks core1, which stops the
		// DShot pump, and the ESC would cut a spinning motor. Unreachable,
		// since MENU disarms, and checked anyway.
		if (uiArmed()) {
			s_saveOutcome = SaveOutcome::FailedArmed;
		} else {
			s_saveOutcome = settingsSave() ? SaveOutcome::Ok : SaveOutcome::Failed;
		}
		s_redraw = true;
	} else if (wasHolding || s_saveHold.holding) {
		s_redraw = true;
	}

	// --- a press on a live control belongs to the control, any other press
	//     in the viewport to the list ---
	if (s_t->pressed) s_ctlPress = controlAt(s_t->x, s_t->y, &s_pressRow);
	if (!s_ctlPress) {
		if (uiListTouch(&s_list, s_t).kind == UiListEv::Scrolled) s_redraw = true;
	}

	s_changed = false;
	for (uint8_t i = 0; i < s_itemCount; i++) {
		const Item &it = s_items[i];
		if (!it.row || it.index >= MAX_ROWS) continue;
		const SettingsRow &r = *it.row;
		bool mine = s_ctlPress && s_pressRow == i && rowEnabled(r);
		if (r.kind == SettingsKind::Stepper) {
			// Stepped through repeatFires() with a zero direction while not
			// held, so the repeat state does not keep an old press.
			int dir = 0;
			if (mine) {
				if (uiPressing(clipToView(partRect(it, UiPart::Minus)), s_t))     dir = -1;
				else if (uiPressing(clipToView(partRect(it, UiPart::Plus)), s_t)) dir = +1;
			}
			if (repeatFires(&s_rep[it.index], dir, now) && dir) r.step(dir);
		} else if (r.kind == SettingsKind::Toggle) {
			if (mine && uiTapped(clipToView(partRect(it, UiPart::Toggle)), s_t)) r.toggle();
		}
	}

	if (uiBackTapped(s_t)) {
		s_ctlPress = false;
		return false;
	} else if (uiTapped(BTN_DEFAULTS, s_t)) {
		// The compiled defaults into the live settings only. Flash is untouched
		// until SAVE, so this is undoable by walking away.
		settingsDefaults(settings());
		s_saveOutcome = SaveOutcome::None;
		s_changed = true;
	}

	if (s_changed) {
		applyLive();
		if (s_saveOutcome == SaveOutcome::Ok) s_saveOutcome = SaveOutcome::None;
		s_redraw = true;
	}
	if (!s_t->down) s_ctlPress = false;

	// Pressed looks appear on touch-down and clear on release, so any frame
	// with a finger involved repaints.
	if (s_t->down || s_t->released) s_redraw = true;

	if (s_redraw) {
		s_redraw = false;
		drawAll();
	} else {
		drawLink(false);
	}
	return true;
}

/**
 * @brief The SETTINGS controls on the current frame. Each row is also
 *        `UI_ROW(n, Row)` over its label and value, drawn and not acting: a
 *        press there scrolls.
 */
static uint8_t settingsControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	for (uint8_t i = 0; i < s_itemCount; i++) {
		const Item &it = s_items[i];
		if (!it.row) continue;
		UiRect body = clipToView(partRect(it, UiPart::Row));
		if (body.h <= 0) continue;
		uiControlAdd(out, &n, max, UiControlId{ it.index, (uint8_t)UiPart::Row }, body, false);
		bool en = rowEnabled(*it.row);
		if (it.row->kind == SettingsKind::Stepper) {
			UiRect m = clipToView(partRect(it, UiPart::Minus));
			UiRect p = clipToView(partRect(it, UiPart::Plus));
			uiControlAdd(out, &n, max, UiControlId{ it.index, (uint8_t)UiPart::Minus }, m, en && showsEnough(m));
			uiControlAdd(out, &n, max, UiControlId{ it.index, (uint8_t)UiPart::Plus }, p, en && showsEnough(p));
		} else if (it.row->kind == SettingsKind::Toggle) {
			UiRect g = clipToView(partRect(it, UiPart::Toggle));
			uiControlAdd(out, &n, max, UiControlId{ it.index, (uint8_t)UiPart::Toggle }, g, en && showsEnough(g));
		}
	}
	uiControlAdd(out, &n, max, UI_FIX(SettingsCtl::Save), BTN_SAVE, true);
	uiControlAdd(out, &n, max, UI_FIX(SettingsCtl::Reset), BTN_DEFAULTS, true);
	return n;
}

uint8_t uiSettingsSections(const SettingsSection **out, uint8_t max) {
	uint8_t n = 0;
	for (; n < s_secCount && n < max; n++) out[n] = s_secs[n];
	return n;
}

/** @brief SETTINGS' MENU status: UNSAVED while the live settings differ from flash. */
static void settingsMenuStatus(char *buf, size_t n, uint16_t *col, uint16_t *) {
	if (settingsDirty()) {
		snprintf(buf, n, "UNSAVED");
		*col = C_AMBER;
	} else {
		buf[0] = '\0';
	}
}

const UiScreen UI_SETTINGS = {
	UiScreenId::Settings, "SETTINGS", PinUser::None, settingsMenuStatus,
	uiSettingsEnter, uiSettingsTick, nullptr, settingsControls, nullptr, true,
};
