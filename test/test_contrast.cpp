/**
 * @file test_contrast.cpp
 * @brief Slice 13 of docs/design/menu-c.md's build plan: the walk over every
 *        text the suite draws, in both themes, for a pair under the 4.5
 *        contrast ratio that WCAG 2 asks of small text
 *        (docs/design/ui-theme.md, "Contrast" and "Slice 13 as built").
 *
 * gfxText() calls the probe with the pixel under the first cell's gap
 * column, which no glyph lights. Each pair is classified by the theme's
 * table: the listed exceptions pass, every other pair under 4.5 is a
 * failure named by its text, its roles and its theme.
 */
#include "check.h"
#include "gfx.h"
#include "theme.h"

#include <math.h>
#include <set>
#include <string>
#include <stdio.h>
#include <string.h>

namespace {

const char *const ROLE[TC_COUNT] = {
	"BG", "PANEL", "GRID", "DIM", "TEXT", "INK", "PAPER", "ONACCENT", "GREEN",
	"LIME", "AMBER", "RED", "REDDARK", "GHOST", "BLUE", "CYAN", "MAGENTA",
};

/** @brief The role of word @p w in the active theme; TC_PAPER is never named, as it equals TC_BG. */
int roleOf(uint16_t w) {
	for (int i = 0; i < TC_COUNT; i++)
		if (i != TC_PAPER && g_theme[i] == w) return i;
	return -1;
}

double channelLin(int c) {
	double s = c / 255.0;
	return s <= 0.04045 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4);
}

double lum(uint16_t w) {
	int r5 = (w >> 11) & 31, g6 = (w >> 5) & 63, b5 = w & 31;
	int r = (r5 << 3) | (r5 >> 2), g = (g6 << 2) | (g6 >> 4), b = (b5 << 3) | (b5 >> 2);
	return 0.2126 * channelLin(r) + 0.7152 * channelLin(g) + 0.0722 * channelLin(b);
}

double ratio(uint16_t a, uint16_t b) {
	double la = lum(a), lb = lum(b);
	if (la < lb) { double t = la; la = lb; lb = t; }
	return (la + 0.05) / (lb + 0.05);
}

std::set<std::string> s_fails;
long s_texts = 0, s_hc = 0;

}  // namespace

bool contrastPairExcepted(const char *text, int fg, int bg, bool high) {
	// The disabled label, the group captions and the read-only values: under
	// 4.5 by intent in Dark (ui-theme.md, "Contrast").
	if (fg == TC_GRID) return true;
	// ERROR in the tester's ESC STATUS tile, red on the control face, 4.03
	// in Dark: the note's named pair, that word only. REC, RECORDING and a
	// critical BOARD voltage are red badges with dark ink (5.45).
	if (!high && fg == TC_RED && bg == TC_PANEL && strcmp(text, "ERROR") == 0) return true;
	// High contrast's two pairs on its PANEL, 3.42 and 3.82, kept by the note.
	if (high && bg == TC_PANEL && (fg == TC_AMBER || fg == TC_LIME)) return true;
	return false;
}

static void probe(int x, int y, const char *s, uint16_t fg, uint16_t bg, bool known, int scale) {
	(void)scale;
	if (!known) return;
	s_texts++;
	bool high = themeGet() == Theme::HighContrast;
	if (high) s_hc++;
	if (fg == bg) return;   // drawn over its own colour: an erase, not a text
	if (ratio(fg, bg) >= 4.5) return;
	int f = roleOf(fg), b = roleOf(bg);
	if (f >= 0 && b >= 0 && contrastPairExcepted(s, f, b, high)) return;
	char line[160];
	snprintf(line, sizeof(line), "%s: \"%.24s\" %s on %s, %.2f, at x %d y %d (first in: %.60s)",
	         high ? "High contrast" : "Dark", s, f >= 0 ? ROLE[f] : "?", b >= 0 ? ROLE[b] : "?",
	         ratio(fg, bg), x, y, checkSection());
	for (const std::string &k : s_fails)
		if (k.compare(0, strlen(line) - strlen(strstr(line, " (first in"))  , line, strlen(line) - strlen(strstr(line, " (first in"))) == 0) return;
	s_fails.insert(line);
}

void contrastWalkStart() { g_gfxTextProbe = probe; }

void contrastWalkReport() {
	g_gfxTextProbe = nullptr;
	section("Contrast: every text the suite drew, 4.5 or more but the listed exceptions");
	printf("  texts read: %ld, of them in High contrast: %ld\n", s_texts, s_hc);
	checkTrue("the walk read texts in both themes", s_texts > 1000 && s_hc > 100);
	for (const std::string &f : s_fails) checkTrue(f.c_str(), false);
	checkInt("pairs under 4.5 outside the exceptions", (int)s_fails.size(), 0);
}
