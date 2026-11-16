/**
 * @file theme.cpp
 * @brief The two palettes and the stroke-weight rules that go with them.
 */

#include "theme.h"

/**
 * @brief The indoor palette: rcbench's dark theme on this panel's 17 entries.
 *
 * Each value is the 8-bit triple of docs/design/ui-theme.md, "The Dark
 * table, proposed", and rgb() packs it as rcbench's `GFX_RGB()` does, so the
 * same triple gives the same RGB565 word on both benches. The comment names
 * the rcbench role an entry takes; "own" is a value rcbench has no role for.
 *
 * Three rules hold the table together:
 *
 * - **Every fill takes dark ink.** `C_ONACCENT` is near black here, and every
 *   accent is bright enough to carry it at a WCAG (Web Content Accessibility
 *   Guidelines) contrast ratio of 4.5 or more (ui-theme.md, decision 3).
 * - **`TC_PAPER` equals `TC_BG`.** It is the one pair of equal words.
 * - **Every other word differs from every other.** The host tests find a
 *   text by its colour, so two roles need two words.
 *
 * Not seen on the panel: the values are computed for sRGB, and the panel's
 * gamma, primaries and backlight are not measured.
 */
static const uint16_t THEME_DARK[TC_COUNT] = {
	rgb(  2,   5,   9), /* TC_BG       UI_C_BG                     */
	rgb( 26,  40,  58), /* TC_PANEL    UI_C_PANEL_HI: control face */
	rgb( 72, 106, 134), /* TC_GRID     UI_C_TEXT_FAINT             */
	rgb(132, 172, 202), /* TC_DIM      UI_C_TEXT_DIM               */
	rgb(232, 248, 255), /* TC_TEXT     UI_C_TEXT                   */
	rgb(255, 255, 255), /* TC_INK      white                       */
	rgb(  2,   5,   9), /* TC_PAPER    == TC_BG                    */
	rgb(  4,   8,  14), /* TC_ONACCENT UI_C_TEXT_ON_LIGHT: dark    */
	rgb(  0, 255, 148), /* TC_GREEN    UI_C_OK                     */
	rgb(132, 255,  56), /* TC_LIME     UI_C_RPM                    */
	rgb(255, 186,   0), /* TC_AMBER    UI_C_WARN                   */
	rgb(255,  40,  72), /* TC_RED      UI_C_DANGER                 */
	rgb( 96,  16,  32), /* TC_REDDARK  own: the red at about 37 %  */
	rgb( 14,  22,  33), /* TC_GHOST    UI_C_PANEL                  */
	rgb( 72, 136, 255), /* TC_BLUE     own                         */
	rgb(  0, 240, 255), /* TC_CYAN     UI_C_ACCENT                 */
	rgb(255,   0, 255), /* TC_MAGENTA  own                         */
};

/**
 * @brief The sunlight palette: black on white, with every accent darkened.
 *
 * Two rules produced these values, and both matter more than taste:
 *
 * - **Every accent must take white text.** `C_ONACCENT` is white in this
 *   theme, so red, green and blue here are dark enough to carry it. A bright
 *   lime that reads well on black is invisible on white *and* will not take
 *   white text, so it becomes a dark green rather than being left alone.
 * - **The greys collapse.** `C_DIM` and `C_GRID` are nearly the same dark grey.
 *   Three distinguishable greys is an indoor luxury; in sunlight a mid-grey
 *   label is simply gone, so the label tier gives up its distinctness and keeps
 *   its legibility.
 *
 * `TC_REDDARK` inverts direction rather than value. Its job is "lit, but this
 * reading is dead" — a *dark* red on black, so a *pale* red on white.
 */
static const uint16_t THEME_HIGH_CONTRAST[TC_COUNT] = {
	rgb(255, 255, 255), /* TC_BG       paper white                */
	rgb(206, 212, 220), /* TC_PANEL    tile fill, clearly not BG   */
	rgb( 56,  62,  72), /* TC_GRID     outlines                    */
	rgb( 68,  74,  84), /* TC_DIM      labels, still black-ish     */
	rgb(  0,   0,   0), /* TC_TEXT                                 */
	rgb(  0,   0,   0), /* TC_INK                                  */
	rgb(255, 255, 255), /* TC_PAPER    == TC_BG                    */
	rgb(255, 255, 255), /* TC_ONACCENT                             */
	rgb(  0, 110,  52), /* TC_GREEN    safe, done, takes white     */
	rgb(  0, 122,  40), /* TC_LIME     live readings, fills        */
	rgb(174,  88,   0), /* TC_AMBER    warning, takes white        */
	rgb(190,   0,  24), /* TC_RED      output on, takes white      */
	rgb(244, 190, 190), /* TC_REDDARK  dead digits: pale, not dark */
	rgb(238, 241, 245), /* TC_GHOST    unlit ghost 8s: barely there  */
	rgb(  0,  58, 168), /* TC_BLUE     latched, takes white        */
	rgb(  0,  88, 122), /* TC_CYAN     the accent                  */
	rgb(150,   0, 140), /* TC_MAGENTA  alert                       */
};

static_assert(sizeof(THEME_DARK) / sizeof(THEME_DARK[0]) == TC_COUNT,
              "dark palette must have one entry per ThemeColor");
static_assert(sizeof(THEME_HIGH_CONTRAST) / sizeof(THEME_HIGH_CONTRAST[0]) == TC_COUNT,
              "high-contrast palette must have one entry per ThemeColor");

const uint16_t *g_theme = THEME_DARK;

static Theme s_theme = Theme::Dark;

void themeSet(Theme t) {
	s_theme = t;
	g_theme = (t == Theme::HighContrast) ? THEME_HIGH_CONTRAST : THEME_DARK;
}

Theme themeGet() { return s_theme; }

bool themeBold() { return s_theme == Theme::HighContrast; }

int themeStroke() { return s_theme == Theme::HighContrast ? 2 : 1; }

int themeSegStroke(int base) {
	// +2 rather than a multiplier: the digit cell is a fixed 36x62 and the
	// segment geometry stops meeting cleanly once the stroke approaches a
	// quarter of the height.
	return s_theme == Theme::HighContrast ? base + 2 : base;
}

uint8_t themeBacklight(uint8_t preferred) {
	return s_theme == Theme::HighContrast ? 255 : preferred;
}
