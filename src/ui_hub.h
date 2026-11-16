/**
 * @file ui_hub.h
 * @brief The hub: the one chooser widget under ESC CONFIG, SERVO and RX
 *        SIGNALS, the three MENU rows that carry `>`.
 *
 * A hub is a title, a strip and a table of entries (@ref UiHub). Each entry
 * (@ref UiHubEntry) is one 228 x 46 px button at x 6 with its label at
 * scale 2, at an 80 px pitch from y 62, and its lines at scale 1 from x 8,
 * starting 50 px under the entry's top at a 12 px pitch.
 *
 * Up to @ref UI_HUB_PAGE_ENTRIES entries fill the page. With more, the hub
 * is a list on ui_list.h in the 258 px under the strip: 80 px rows with a
 * 46 px body, content of 80 px an entry and 4 px (uiHubContentPx()), and the
 * scroll bar at x 235. A vertical drag scrolls it. An entry cut by the
 * panel's edge below @ref UI_LIST_TAP_MIN px of its button is listed in the
 * control table and takes no press. SERVO, five entries, scrolls 146 px.
 *
 * A hub claims nothing and opens whatever the pin claim's state. A tap on
 * an entry opens the entry's tool through uiChooseTool(), whose entry point
 * runs on that frame, after the hub's tick; the tool is pushed over the hub,
 * and its BACK and its leave page's last button return to the hub, which
 * resumes as it was left, at its scroll (uiHubResume()). BACK leaves the
 * hub.
 *
 * An entry is drawn, reported in the control table and tapped on one value,
 * uiHubEntryActs(): its own gate holds and uiPinsRefuse() does not refuse
 * its tool. An entry whose own gate fails is drawn disabled with its `why`
 * lines in amber. While the claim refuses, the refused entries are drawn
 * disabled with their own lines, and the strip reads
 * @ref UI_MENU_RESTARTING_FMT in amber, as MENU's does. A disabled entry
 * reports disabled and takes no tap.
 *
 * The three screens keep a file each (ui_esc_config.h, ui_servo_hub.h,
 * ui_rx_signals.h) with the title, the strip and the `UiScreen`; their
 * tables are in ui_registry.cpp, each checked by hubEntriesFit() at build
 * time.
 *
 * The hub is slices 1, 2 and 4 of menu structure C (docs/design/menu-c.md,
 * "The hub").
 *
 * @see docs/design/menu-b.md, "PROGRAMMER, the chooser"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gfx.h"
#include "ui_screen.h"

struct TouchState;

/**
 * @defgroup ui_hub_layout The hub's layout
 * @{
 */
#define UI_HUB_ENTRY_Y      62   /**< The first entry's top, @ref UI_BODY_Y. */
#define UI_HUB_ENTRY_PITCH  80   /**< Entry top to entry top. */
#define UI_HUB_ENTRY_W      228  /**< From x 6, @ref UI_EDGE, to x 233. */
#define UI_HUB_ENTRY_H      46
#define UI_HUB_LINE_DY      50   /**< An entry's first line, from its top: 4 px under it. */
#define UI_HUB_LINE_H       12   /**< Line to line, scale 1. */
#define UI_HUB_LINE_CHARS   37   /**< A line from x 8 at scale 1: 221 px, to x 228. */
#define UI_HUB_LABEL_CHARS  18   /**< A label at scale 2: 214 px in the 228 px button. */
#define UI_HUB_LINES        2    /**< Plain lines of an entry. */
#define UI_HUB_WHY          2    /**< Lines that say why an entry is disabled. */
#define UI_HUB_PAGE_ENTRIES 3    /**< Entries on the page: the third button ends at y 268. More scroll. */
#define UI_HUB_MAX_ENTRIES  8    /**< Most entries of a hub: the control table holds them beside BACK. */
#define UI_HUB_TAIL         4    /**< Content under the last entry's 80 px: its third line ends 81 px under its top. */
#define UI_HUB_BAR_X        235  /**< The scroll bar, as MENU's. */
/** @} */

/**
 * @brief One entry of a hub: a button that opens a tool, with the lines
 *        under it. A new brand, protocol or tool is one line in its hub's
 *        table (ui_registry.cpp).
 *
 * Under the button, top to bottom: the plain @ref lines; then, while the
 * entry acts, the power line, and while it is disabled, the @ref why lines
 * in its place.
 */
struct UiHubEntry {
	const char     *label;                /**< Scale 2 on the button, at most @ref UI_HUB_LABEL_CHARS characters. */
	const char     *lines[UI_HUB_LINES];  /**< Scale 1 under the button, `C_DIM`; null ends them. */
	const UiScreen *screen;               /**< The tool the tap opens. */
	void          (*enter)();             /**< Run in place of the tool's own `enter` for this route; null: the tool's. */
	bool          (*enabled)();           /**< The entry's own gate; null: it always acts. */
	const char     *why[UI_HUB_WHY];      /**< `UI_COL_WARN` lines in place of the power line while disabled; null for none. */
	/** @brief The power line while SERVO SUPPLY is OWN, `C_DIM`; null for none. */
	const char     *powerOwn;
	/**
	 * @brief The power line with the switched supply, `C_DIM`, `%u` its
	 *        enable GPIO; null: the entry has no power line.
	 */
	const char     *powerSwitchedFmt;
};

/** @brief A hub: what its screen shows. */
struct UiHub {
	const char       *title;  /**< Header text, scale 2, ending 4 px or more before BACK. */
	/** @brief Writes the strip's text, `C_DIM`, to buf, at most n bytes. */
	void            (*strip)(char *buf, size_t n);
	const UiHubEntry *entries;  /**< Top to bottom. */
	uint8_t           count;    /**< How many, 1 to @ref UI_HUB_MAX_ENTRIES. */
};

/** @brief A hub's controls besides BACK: entry i is `HUB_ENTRY(i)`. */
enum class HubCtl : uint8_t {
	Entry = 1,  /**< The first entry; entry i is Entry + i. */
};

/** @brief The control of entry @p i of a hub. */
#define HUB_ENTRY(i) UI_FIX((uint8_t)HubCtl::Entry + (i))

/** @brief Characters of @p s, 0 for null. */
constexpr int uiHubTextLen(const char *s) {
	int n = 0;
	while (s && s[n]) n++;
	return n;
}

/**
 * @brief The height of a hub's content, px: 80 px an entry and
 *        @ref UI_HUB_TAIL px, 404 px for five entries. The hub scrolls by
 *        what of it exceeds the 258 px under the strip.
 * @param count Entries.
 */
constexpr int uiHubContentPx(int count) {
	return count * UI_HUB_ENTRY_PITCH + UI_HUB_TAIL;
}

/**
 * @brief Every entry of @p h fits.
 *
 * An entry fits when it has a label within @ref UI_HUB_LABEL_CHARS
 * characters and a screen, when each of its lines is within
 * @ref UI_HUB_LINE_CHARS characters (the switched power line with a
 * two-digit GPIO for its `%u`), and when the lines under it end 4 px or more
 * above the next entry, the last entry's inside the content. The lines under
 * an entry are its plain lines and the taller of its power line and its
 * `why` lines. Their last line ends at the entry's top + 50 px + 12 px per
 * line after the first + 7 px, which must be at most 76 px below the entry's
 * top (an entry above another: two lines) or at most 84 px, the content's
 * end (the last entry: three lines).
 *
 * It fails for an entry with a gate and no line that says why, which the hub
 * would draw disabled without a reason; for a power line at OWN with none
 * for the switched supply; for a plain line after a null one, which the hub
 * would not draw; and for a hub of no entry or of more than
 * @ref UI_HUB_MAX_ENTRIES.
 *
 * @param h The hub.
 * @return true when every text fits.
 */
constexpr bool hubEntriesFit(const UiHub &h) {
	if (h.count < 1 || h.count > UI_HUB_MAX_ENTRIES) return false;
	for (int i = 0; i < h.count; i++) {
		const UiHubEntry &e = h.entries[i];
		if (uiHubTextLen(e.label) < 1 || uiHubTextLen(e.label) > UI_HUB_LABEL_CHARS) return false;
		if (!e.screen) return false;
		int lines = 0;
		for (int k = 0; k < UI_HUB_LINES; k++) {
			if (!e.lines[k]) continue;
			if (k != lines) return false;
			if (uiHubTextLen(e.lines[k]) > UI_HUB_LINE_CHARS) return false;
			lines++;
		}
		int why = 0;
		for (int k = 0; k < UI_HUB_WHY; k++) {
			if (uiHubTextLen(e.why[k]) > UI_HUB_LINE_CHARS) return false;
			why += e.why[k] != nullptr;
		}
		if (e.enabled && why == 0) return false;
		if (e.powerOwn && !e.powerSwitchedFmt) return false;
		if (uiHubTextLen(e.powerOwn) > UI_HUB_LINE_CHARS) return false;
		if (uiHubTextLen(e.powerSwitchedFmt) > UI_HUB_LINE_CHARS) return false;
		int power = e.powerSwitchedFmt ? 1 : 0;
		int under = lines + (why > power ? why : power);
		// From the content's top: the last line's end, and the room it has.
		int top = i * UI_HUB_ENTRY_PITCH;
		int end = top + UI_HUB_LINE_DY + (under > 0 ? (under - 1) * UI_HUB_LINE_H + 7 : 0);
		int room = i + 1 < h.count ? top + UI_HUB_ENTRY_PITCH - 4 : uiHubContentPx(h.count);
		if (end > room) return false;
	}
	return true;
}

/**
 * @brief Entry @p i's button on the current frame, at the open hub's scroll.
 * @param i The entry, from 0.
 * @return 228 x 46 px at (6, 62 + 80 x i - scroll); it may lie outside the
 *         viewport. The control table carries its visible part.
 */
UiRect uiHubEntryRect(int i);

/**
 * @brief Whether entry @p i of @p h acts: its own gate holds, and
 *        uiPinsRefuse() does not refuse its tool. The hub draws the entry,
 *        reports it and takes its tap on this value.
 * @param h The hub.
 * @param i The entry, from 0.
 * @return false while the entry is drawn disabled.
 */
bool uiHubEntryActs(const UiHub &h, int i);

/**
 * @brief Open @p h: scrolled to 0 px, its next frame repaints.
 * @param h The hub.
 */
void uiHubEnter(const UiHub &h);

/**
 * @brief Show a hub again after the tool it opened has left: its next frame
 *        repaints, and the hub keeps the state it was left in, its scroll
 *        among it, so the entry that was tapped is where it was.
 */
void uiHubResume();

/** @brief The open hub's scroll, px: 0 to uiHubMaxScroll(). */
int uiHubScroll();

/**
 * @brief The open hub's scroll range, px: its content less the 258 px
 *        viewport, 0 for a hub that fills the page.
 */
int uiHubMaxScroll();

/**
 * @brief Run one frame of @p h.
 * @param h The hub.
 * @param t This frame's touch; null for none.
 * @return false once BACK has left it.
 */
bool uiHubTick(const UiHub &h, const TouchState *t);

/**
 * @brief @p h's controls on the current frame: BACK, then each entry with a
 *        visible part, at that part, enabled as uiHubEntryActs() says and
 *        only while @ref UI_LIST_TAP_MIN px or more of its button show.
 * @param h   The hub.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiHubControls(const UiHub &h, UiControl *out, uint8_t max);
