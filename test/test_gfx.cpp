/**
 * @file test_gfx.cpp
 * @brief Dirty-band bookkeeping, glyph rendering and the rounded corners.
 *
 * The dirty bands decide how much SPI traffic a frame costs, and their merge
 * and saturation paths are pure logic -- exactly the kind of code that is easy
 * to break silently and cheap to pin down here.
 */

#include "check.h"
#include "gfx.h"
#include "theme.h"

#include <string.h>

/**
 * @brief A fixed foreground colour for the renderer tests.
 *
 * Deliberately a literal rather than a palette name. These checks assert the
 * exact word written into specific framebuffer cells, so they must not move
 * when a theme does -- the renderer is what is under test here, not the
 * palette. @see test_settings.cpp for the theme-dependent side.
 */
static const uint16_t TEST_FG = 0xFFFF;

/** @brief Reset the framebuffer and band list to a clean, undirty slate. */
static void resetGfx() {
	gfxInit();
	gfxClearDirty();
}

static void testBands() {
	section("Dirty bands");

	resetGfx();
	checkInt("clean slate has no bands", gfxDirtyCount(), 0);

	gfxMarkDirty(10, 20);
	checkInt("one band", gfxDirtyCount(), 1);

	int y0, y1;
	gfxDirtyBand(0, &y0, &y1);
	checkInt("band starts where marked", y0, 10);
	checkInt("band ends where marked", y1, 20);

	// A disjoint band must stay disjoint.
	gfxMarkDirty(100, 110);
	checkInt("disjoint band stays separate", gfxDirtyCount(), 2);

	// Overlapping bands merge into one.
	resetGfx();
	gfxMarkDirty(10, 20);
	gfxMarkDirty(15, 25);
	checkInt("overlapping bands merge", gfxDirtyCount(), 1);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("merge keeps the lower edge", y0, 10);
	checkInt("merge keeps the upper edge", y1, 25);

	// Adjacent bands merge too: 21 is exactly one row past 20.
	resetGfx();
	gfxMarkDirty(10, 20);
	gfxMarkDirty(21, 30);
	checkInt("adjacent bands merge", gfxDirtyCount(), 1);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("adjacent merge spans both", y1, 30);

	// A band bridging two existing bands absorbs both.
	resetGfx();
	gfxMarkDirty(10, 15);
	gfxMarkDirty(40, 45);
	gfxMarkDirty(14, 41);
	checkInt("bridging band absorbs both", gfxDirtyCount(), 1);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("bridge starts at the low band", y0, 10);
	checkInt("bridge ends at the high band", y1, 45);

	// Inverted arguments are normalised, and off-screen rows clipped.
	resetGfx();
	gfxMarkDirty(50, 40);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("swapped bounds normalised low", y0, 40);
	checkInt("swapped bounds normalised high", y1, 50);

	resetGfx();
	gfxMarkDirty(-100, 5);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("negative rows clip to 0", y0, 0);
	checkInt("clip keeps the visible end", y1, 5);

	resetGfx();
	gfxMarkDirty(GFX_H + 10, GFX_H + 20);
	checkInt("fully off-screen band is dropped", gfxDirtyCount(), 0);

	resetGfx();
	gfxMarkDirty(-300, -5);
	checkInt("fully negative band is dropped", gfxDirtyCount(), 0);

	// Fill every slot, then push one more: the whole set must collapse into a
	// single spanning band rather than overflowing the fixed arrays.
	resetGfx();
	for (int i = 0; i < 8; i++) gfxMarkDirty(i * 30, i * 30 + 5);
	checkInt("eight disjoint bands all kept", gfxDirtyCount(), 8);
	gfxMarkDirty(300, 319);
	checkInt("ninth band collapses the set", gfxDirtyCount(), 1);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("collapse starts at the lowest band", y0, 0);
	checkInt("collapse ends at the new band", y1, 319);
}

static void testGlyphs() {
	section("Glyph rendering");

	// A fully off-screen character must not draw or dirty anything.
	resetGfx();
	gfxText(10, GFX_H + 20, "X", TEST_FG, 1);
	checkInt("off-screen text dirties nothing", gfxDirtyCount(), 0);
	gfxText(-6, 10, "X", TEST_FG, 1);   // entirely left of the framebuffer
	checkInt("off-left text dirties nothing", gfxDirtyCount(), 0);

	// ' ' at scale 1 is five empty columns: no pixels, no dirty band.
	resetGfx();
	gfxText(10, 10, " ", TEST_FG, 1);
	checkInt("space dirties nothing", gfxDirtyCount(), 0);

	// '0' is {0x3E,0x51,0x49,0x45,0x3E}: column 0 lights rows 1..5, but column 1
	// lights rows 0, 4 and 6, so the glyph as a whole occupies all seven rows.
	//
	// This used to expect 11, and that was the bug rather than the rule. The
	// band was taken from the first lit row of the first non-empty column, so
	// the pixel column 1 puts on row 0 was written to the framebuffer and left
	// outside the dirty band -- drawn, and never flushed to the panel. Only
	// visible on a glyph whose first column starts lower than a later one, and
	// only for the top row of it, which is why it survived this long.
	resetGfx();
	gfxText(10, 10, "0", TEST_FG, 1);
	checkInt("glyph creates one band", gfxDirtyCount(), 1);
	int y0, y1;
	gfxDirtyBand(0, &y0, &y1);
	checkInt("glyph band starts on its first lit row", y0, 10);
	checkInt("glyph band ends on its last row", y1, 16);

	// Partially visible at the bottom edge: clipped, not dropped.
	resetGfx();
	gfxText(10, GFX_H - 3, "0", TEST_FG, 1);
	checkInt("bottom-clipped glyph still draws", gfxDirtyCount(), 1);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("clipped band stops at the edge", y1, GFX_H - 1);

	// A lit pixel actually lands in the framebuffer, an unlit one stays clear.
	resetGfx();
	gfxText(0, 0, "0", TEST_FG, 1);
	// '0' = {0x3E,0x51,0x49,0x45,0x3E}: column 0 rows 1..5 lit, row 0 clear.
	checkInt("lit pixel written", gfxBuffer()[1 * GFX_W + 0], TEST_FG);
	checkInt("unlit pixel untouched", gfxBuffer()[0 * GFX_W + 0], 0);

	// Scale 2: one glyph pixel is a 2x2 block.
	resetGfx();
	gfxText(0, 0, "0", TEST_FG, 2);
	checkInt("scaled pixel block, top-left", gfxBuffer()[2 * GFX_W + 0], TEST_FG);
	checkInt("scaled pixel block, bottom-right", gfxBuffer()[3 * GFX_W + 1], TEST_FG);
	gfxDirtyBand(0, &y0, &y1);
	// Row 0 of column 1 again, two pixels tall at this scale.
	checkInt("scaled band starts on its first lit row", y0, 0);
	checkInt("scaled band ends on its last row", y1, 13);

	// Left edge: '0' at x = -2 drops its first two columns, so screen
	// column 0 shows glyph column 2 (0x49: rows 0, 3 and 6 lit).
	resetGfx();
	gfxText(-2, 0, "0", TEST_FG, 1);
	checkInt("clipped-left glyph column, row 0", gfxBuffer()[0 * GFX_W + 0], TEST_FG);
	checkInt("clipped-left glyph column, row 6", gfxBuffer()[6 * GFX_W + 0], TEST_FG);
	checkInt("clipped-left glyph column, row 1", gfxBuffer()[1 * GFX_W + 0], 0);
	checkInt("clipped-left glyph still dirties", gfxDirtyCount(), 1);

	// Left clip at scale 2: '0' at x = -2 drops its first column, so screen
	// column 0 shows glyph column 1 (0x51: rows 0, 4 and 6 lit) as a 2x2 block.
	resetGfx();
	gfxText(-2, 0, "0", TEST_FG, 2);
	checkInt("scaled left-clip dirties", gfxDirtyCount(), 1);
	checkInt("scaled left-clip column, row 0", gfxBuffer()[0 * GFX_W + 0], TEST_FG);
	checkInt("scaled left-clip column, row 8", gfxBuffer()[8 * GFX_W + 0], TEST_FG);
	checkInt("scaled left-clip column, row 12", gfxBuffer()[12 * GFX_W + 0], TEST_FG);
	checkInt("scaled left-clip column, row 2", gfxBuffer()[2 * GFX_W + 0], 0);

	// Lowercase folds to uppercase, unknown characters render as space.
	resetGfx();
	gfxText(0, 0, "a", TEST_FG, 1);            // 'A' has lit pixels
	checkInt("lowercase draws as uppercase", gfxDirtyCount(), 1);
	resetGfx();
	gfxText(0, 0, "\x7F", TEST_FG, 1);          // DEL -> space
	checkInt("unknown char draws nothing", gfxDirtyCount(), 0);
}

/**
 * @brief The clip box, tested the way the AM32 list uses it.
 *
 * That list scrolls by the pixel, so its top and bottom rows are always partly
 * outside the viewport, and a partial row drawn in full lands on the header or
 * the editor bar. Every check here is that shape: draw something that overhangs
 * the box and confirm nothing outside it moved.
 */
static void testClip() {
	section("Clip rectangle");

	// A fill that straddles the box keeps the part inside and drops the rest.
	resetGfx();
	gfxSetClip(0, 100, GFX_W, 20);          // rows 100..119
	gfxRect(0, 90, GFX_W, 40, TEST_FG);     // rows 90..129
	gfxClearClip();
	checkInt("row above the clip is untouched", gfxBuffer()[99 * GFX_W], 0);
	checkInt("first row inside is drawn",       gfxBuffer()[100 * GFX_W], TEST_FG);
	checkInt("last row inside is drawn",        gfxBuffer()[119 * GFX_W], TEST_FG);
	checkInt("row below the clip is untouched", gfxBuffer()[120 * GFX_W], 0);

	// And the dirty band follows the clip, not the requested rectangle: a band
	// covering rows nothing was written to would flush stale framebuffer.
	int y0, y1;
	checkInt("clipped fill makes one band", gfxDirtyCount(), 1);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("band starts at the clip top", y0, 100);
	checkInt("band ends at the clip bottom", y1, 119);

	// Horizontal clipping, which the list needs for nothing but which a caller
	// setting a narrow box would rely on.
	resetGfx();
	gfxSetClip(50, 0, 10, GFX_H);           // columns 50..59
	gfxRect(0, 0, GFX_W, 4, TEST_FG);
	gfxClearClip();
	checkInt("column left of the clip is untouched", gfxBuffer()[49], 0);
	checkInt("column inside the clip is drawn",      gfxBuffer()[50], TEST_FG);
	checkInt("column right of the clip is untouched", gfxBuffer()[60], 0);

	// Text obeys it too. This is the half that matters: a row's caption is text,
	// and clipping only the fills would leave labels floating over the header.
	resetGfx();
	gfxSetClip(0, 4, GFX_W, GFX_H - 4);
	gfxText(0, 0, "0", TEST_FG, 1);          // rows 0..6, so rows 0..3 are cut
	gfxClearClip();
	checkInt("glyph row above the clip is cut", gfxBuffer()[1 * GFX_W + 0], 0);
	checkInt("glyph row inside the clip draws", gfxBuffer()[4 * GFX_W + 0], TEST_FG);
	gfxDirtyBand(0, &y0, &y1);
	checkInt("clipped glyph band starts at the clip", y0, 4);

	// Text entirely outside draws nothing and dirties nothing, so a scrolled-off
	// row costs no flush.
	resetGfx();
	gfxSetClip(0, 100, GFX_W, 20);
	gfxText(0, 0, "0", TEST_FG, 1);
	gfxClearClip();
	checkInt("text outside the clip dirties nothing", gfxDirtyCount(), 0);

	// gfxFill() deliberately ignores the box: every caller uses it to start a
	// screen from nothing, and a clip left set would make that a partial wipe.
	resetGfx();
	gfxSetClip(0, 100, GFX_W, 20);
	gfxFill(TEST_FG);
	gfxClearClip();
	checkInt("fill ignores the clip, top", gfxBuffer()[0], TEST_FG);
	checkInt("fill ignores the clip, bottom", gfxBuffer()[(GFX_H - 1) * GFX_W], TEST_FG);

	// Clearing it restores the whole panel.
	resetGfx();
	gfxSetClip(0, 100, GFX_W, 20);
	gfxClearClip();
	gfxRect(0, 0, GFX_W, 4, TEST_FG);
	checkInt("cleared clip draws anywhere again", gfxBuffer()[0], TEST_FG);

	// A degenerate box clips everything rather than inverting into a fill of the
	// whole panel, which is what unchecked reversed bounds would do.
	resetGfx();
	gfxSetClip(0, 100, GFX_W, 0);
	gfxRect(0, 0, GFX_W, GFX_H, TEST_FG);
	gfxClearClip();
	checkInt("an empty clip draws nothing", gfxDirtyCount(), 0);
	checkInt("and leaves the buffer alone", gfxBuffer()[100 * GFX_W], 0);
}

/** @brief Count the pixels of @p c in a rectangle. */
static int countIn(int x, int y, int w, int h, uint16_t c) {
	int n = 0;
	for (int yy = y; yy < y + h; yy++)
		for (int xx = x; xx < x + w; xx++)
			if (gfxBuffer()[yy * GFX_W + xx] == c) n++;
	return n;
}

/** @brief The pixel at (@p x, @p y). */
static uint16_t px(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/** @brief The thick line and the arc of dots the servo dial is drawn with. */
static void testLineAndArc() {
	section("Lines at any angle, and an arc of dots");

	resetGfx();
	gfxLine(10, 20, 19, 20, 1, TEST_FG);
	checkInt("a horizontal line: both ends and all between", countIn(0, 0, 40, 40, TEST_FG), 10);
	checkTrue("from x 10 to x 19", px(10, 20) == TEST_FG && px(19, 20) == TEST_FG &&
	                               px(9, 20) == 0 && px(20, 20) == 0);

	resetGfx();
	gfxLine(30, 15, 30, 5, 1, TEST_FG);
	checkInt("a vertical line drawn upwards: 11 pixels", countIn(0, 0, 40, 40, TEST_FG), 11);
	checkTrue("from y 5 to y 15", px(30, 5) == TEST_FG && px(30, 15) == TEST_FG &&
	                              px(30, 4) == 0 && px(30, 16) == 0);

	resetGfx();
	gfxLine(5, 5, 14, 14, 1, TEST_FG);
	bool diag = true;
	for (int i = 0; i < 10; i++) diag = diag && px(5 + i, 5 + i) == TEST_FG;
	checkTrue("a 45 degree line is the diagonal", diag && countIn(0, 0, 40, 40, TEST_FG) == 10);

	resetGfx();
	gfxLine(20, 10, 0, 0, 1, TEST_FG);
	checkInt("a shallow line right to left: one pixel per column",
	         countIn(0, 0, 40, 40, TEST_FG), 21);
	checkTrue("ends included", px(0, 0) == TEST_FG && px(20, 10) == TEST_FG);

	resetGfx();
	gfxLine(50, 50, 59, 50, 4, TEST_FG);
	checkInt("4 px thick, 10 px long: 13 by 4", countIn(40, 40, 30, 30, TEST_FG), 13 * 4);
	checkTrue("the stroke is centred: rows 48 to 51",
	          px(55, 48) == TEST_FG && px(55, 51) == TEST_FG && px(55, 47) == 0 && px(55, 52) == 0);
	resetGfx();
	gfxLine(50, 50, 50, 50, 0, TEST_FG);
	checkInt("thickness 0 counts as 1: one pixel", countIn(40, 40, 30, 30, TEST_FG), 1);

	resetGfx();
	gfxSetClip(0, 0, GFX_W, 100);
	gfxLine(10, 90, 10, 110, 3, TEST_FG);
	gfxClearClip();
	checkTrue("a line obeys the clip", px(10, 99) == TEST_FG && px(10, 100) == 0);
	int y0, y1;
	gfxDirtyBand(0, &y0, &y1);
	checkTrue("and dirties only what it drew", y0 == 89 && y1 == 99);

	resetGfx();
	gfxArcDots(120, 150, 50, -90, 90, 90, 1, TEST_FG);
	checkInt("an arc from -90 to +90 every 90 deg: three dots",
	         countIn(0, 0, GFX_W, GFX_H, TEST_FG), 3);
	checkTrue("left, up and right of the centre",
	          px(70, 150) == TEST_FG && px(120, 100) == TEST_FG && px(170, 150) == TEST_FG);

	resetGfx();
	gfxArcDots(120, 150, 50, 0, 59, 30, 3, TEST_FG);
	checkInt("0 to 59 deg every 30: two dots of 3 by 3",
	         countIn(0, 0, GFX_W, GFX_H, TEST_FG), 18);
	checkTrue("the 30 deg dot is centred on the arc",
	          px(145, 107) == TEST_FG && px(144, 106) == TEST_FG && px(146, 108) == TEST_FG);

	resetGfx();
	gfxArcDots(120, 150, 50, 0, 3, 0, 0, TEST_FG);
	checkInt("a step and a size of 0 count as 1: four dots of one pixel",
	         countIn(0, 0, GFX_W, GFX_H, TEST_FG), 4);
}

/**
 * @brief The first and the last lit column of row @p y between @p x0 and
 *        @p x1, relative to @p x0; -1 for a row with nothing lit.
 */
static void rowExtent(int x0, int x1, int y, int *first, int *last) {
	*first = -1;
	*last = -1;
	for (int x = x0; x <= x1; x++) {
		if (px(x, y) != TEST_FG) continue;
		if (*first < 0) *first = x - x0;
		*last = x - x0;
	}
}

/**
 * @brief Whether the four corners of the rectangle hold @p want: @p rows
 *        strings of @p cols characters, `#` lit and `.` not, written for the
 *        top left corner and mirrored for the other three.
 */
static bool cornersAre(int x, int y, int w, int h, const char *const *want, int rows, int cols) {
	for (int j = 0; j < rows; j++)
		for (int i = 0; i < cols; i++) {
			const bool lit = want[j][i] == '#';
			if ((px(x + i, y + j) == TEST_FG) != lit) return false;
			if ((px(x + w - 1 - i, y + j) == TEST_FG) != lit) return false;
			if ((px(x + i, y + h - 1 - j) == TEST_FG) != lit) return false;
			if ((px(x + w - 1 - i, y + h - 1 - j) == TEST_FG) != lit) return false;
		}
	return true;
}

/**
 * @brief The corner radius (slice 9 of menu-c.md's build plan): the rows of
 *        a 5 px corner are inset 2, 1 and 0 px, in the fill and in the frame,
 *        at the frame's 1 px and at High contrast's 2 px.
 */
static void testRound() {
	section("Rounded corners: a 5 px radius insets its rows 2, 1, 0");

	themeSet(Theme::Dark);
	const int X = 10, Y = 20, W = 30, H = 20;
	resetGfx();
	gfxRoundRect(X, Y, W, H, 5, TEST_FG);
	static const int INSET5[] = { 2, 1, 0, 0, 0 };
	bool rows = true;
	for (int dy = 0; dy < H; dy++) {
		const int want = dy < 5 ? INSET5[dy] : dy >= H - 5 ? INSET5[H - 1 - dy] : 0;
		int a, b;
		rowExtent(X, X + W - 1, Y + dy, &a, &b);
		if (a != want || b != W - 1 - want) rows = false;
	}
	checkTrue("fill, 5 px: each row starts and ends at its inset, top and bottom", rows);
	static const char *const FILL5[] = { "..###", ".####", "#####", "#####", "#####" };
	checkTrue("fill, 5 px: (0,0), (1,0) and (0,1) of each corner are not lit, the rest is",
	          cornersAre(X, Y, W, H, FILL5, 5, 5));
	checkInt("fill, 5 px: 30 x 20 less 3 pixels a corner", countIn(0, 0, GFX_W, GFX_H, TEST_FG), W * H - 12);
	int y0, y1;
	gfxDirtyBand(0, &y0, &y1);
	checkTrue("it dirties its own rows only", gfxDirtyCount() == 1 && y0 == Y && y1 == Y + H - 1);

	resetGfx();
	gfxRoundFrame(X, Y, W, H, 5, TEST_FG);
	static const char *const FRAME5[] = { "..###", ".#...", "#....", "#....", "#...." };
	checkTrue("frame, 1 px, 5 px: the corner pixels at insets 2, 1, 0, joined to the edges",
	          cornersAre(X, Y, W, H, FRAME5, 5, 5));
	checkInt("frame, 1 px: 60 px of straight edges and 7 a corner",
	         countIn(0, 0, GFX_W, GFX_H, TEST_FG), 2 * (W - 10) + 2 * (H - 10) + 4 * 7);
	checkTrue("frame, 1 px: the edges between the corners, nothing inside",
	          px(X + 5, Y) == TEST_FG && px(X + W - 6, Y + H - 1) == TEST_FG && px(X, Y + 5) == TEST_FG &&
	          px(X + W - 1, Y + H - 6) == TEST_FG && px(X + W / 2, Y + 1) == 0 && px(X + 1, Y + H / 2) == 0);
	// The frame lies on the fill's outermost pixels: none of it outside.
	gfxRoundRect(X, Y, W, H, 5, 0);
	checkInt("frame, 1 px: the fill covers it", countIn(0, 0, GFX_W, GFX_H, TEST_FG), 0);

	themeSet(Theme::HighContrast);
	resetGfx();
	gfxRoundFrame(X, Y, W, H, 5, TEST_FG);
	static const char *const FRAME5X2[] = { "..###", ".####", "###..", "##...", "##..." };
	checkTrue("frame, 2 px, 5 px: the same outline, 2 px thick, no open pixel between its rings",
	          cornersAre(X, Y, W, H, FRAME5X2, 5, 5));
	checkInt("frame, 2 px: 120 px of straight edges and 14 a corner",
	         countIn(0, 0, GFX_W, GFX_H, TEST_FG), 2 * 2 * (W - 10) + 2 * 2 * (H - 10) + 4 * 14);
	gfxRoundRect(X, Y, W, H, 5, 0);
	checkInt("frame, 2 px: the fill covers it", countIn(0, 0, GFX_W, GFX_H, TEST_FG), 0);
	resetGfx();
	gfxRoundFrame(X, Y, 4, 9, 5, TEST_FG);
	checkInt("frame, 2 px, 4 px wide: no room for two strokes, filled", countIn(0, 0, GFX_W, GFX_H, TEST_FG),
	         4 * 9);
	themeSet(Theme::Dark);

	resetGfx();
	gfxRoundRect(X, Y, W, H, 4, TEST_FG);
	static const char *const FILL4[] = { "..##", ".###", "####", "####" };
	checkTrue("fill, 4 px: insets 2, 1, 0, 0", cornersAre(X, Y, W, H, FILL4, 4, 4));
	resetGfx();
	gfxRoundRect(X, Y, W, H, 3, TEST_FG);
	static const char *const FILL3[] = { ".##", "###", "###" };
	checkTrue("fill, 3 px: insets 1, 0, 0", cornersAre(X, Y, W, H, FILL3, 3, 3));
	resetGfx();
	gfxRoundRect(X, Y, W, H, 0, TEST_FG);
	checkInt("fill, radius 0: the whole rectangle", countIn(0, 0, GFX_W, GFX_H, TEST_FG), W * H);
	resetGfx();
	gfxRoundFrame(X, Y, W, H, 0, TEST_FG);
	checkInt("frame, radius 0: the rectangle's outline", countIn(0, 0, GFX_W, GFX_H, TEST_FG), 2 * W + 2 * H - 4);

	// A radius of half the size: the servo's selection dot, 12 x 12 px.
	resetGfx();
	gfxRoundRect(X, Y, 12, 12, 6, TEST_FG);
	static const int DOT[] = { 3, 2, 1, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
	bool dot = true;
	for (int dy = 0; dy < 12; dy++) {
		int a, b;
		rowExtent(X, X + 11, Y + dy, &a, &b);
		if (a != DOT[dy] || b != 11 - DOT[dy]) dot = false;
	}
	checkTrue("fill, 12 x 12 px at 6 px: a disc, rows inset 3, 2, 1, 0", dot);
	resetGfx();
	gfxRoundRect(X, Y, 12, 12, 40, TEST_FG);
	checkInt("a radius over half the size counts as half: the same disc",
	         countIn(0, 0, GFX_W, GFX_H, TEST_FG), 12 * 12 - 4 * 6);
	resetGfx();
	gfxRoundRect(X, Y, 0, 12, 5, TEST_FG);
	gfxRoundFrame(X, Y, 12, 0, 5, TEST_FG);
	checkInt("no width or no height: nothing drawn", countIn(0, 0, GFX_W, GFX_H, TEST_FG), 0);

	resetGfx();
	gfxSetClip(0, 0, GFX_W, Y + 2);
	gfxRoundRect(X, Y, W, H, 5, TEST_FG);
	gfxClearClip();
	checkInt("the fill obeys the clip: its first two rows", countIn(0, 0, GFX_W, GFX_H, TEST_FG),
	         (W - 4) + (W - 2));
}

void runGfxTests() {
	testBands();
	testGlyphs();
	testClip();
	testLineAndArc();
	testRound();
}
