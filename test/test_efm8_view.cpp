/**
 * @file test_efm8_view.cpp
 * @brief The rows and texts ESC SETTINGS shows of a Bluejay or BLHeli_S
 *        page (efm8_view.h), against rcbench's maps and identification and
 *        the pages of rcbench's simulated ESC.
 *
 * BLHeli_S and Bluejay are names of their owners' firmware, used here only
 * to say which ESCs this module works with.
 */

#include "check.h"
#include "efm8_view.h"
#include "gfx.h"
#include "ui_am32.h"
#include "ui_widgets.h"
#include "ui_list.h"

#include "rcbench/ew_sim.h"

#include <initializer_list>
#include <stdio.h>
#include <string.h>

/** @brief The rows' labels, joined by `|`. */
static const char *rowNames(uint8_t fw, uint8_t layout) {
	static char out[512];
	Efm8Row r[EFM8_ROWS_MAX];
	int n = efm8Rows(fw, layout, r, EFM8_ROWS_MAX);
	out[0] = '\0';
	for (int i = 0; i < n; i++) {
		if (i) strcat(out, "|");
		strcat(out, r[i].f->name);
	}
	return out;
}

/** @brief The rows' groups, joined by `|`. */
static const char *rowGroups(uint8_t fw, uint8_t layout) {
	static char out[512];
	Efm8Row r[EFM8_ROWS_MAX];
	int n = efm8Rows(fw, layout, r, EFM8_ROWS_MAX);
	out[0] = '\0';
	for (int i = 0; i < n; i++) {
		if (i) strcat(out, "|");
		strcat(out, r[i].group);
	}
	return out;
}

/** @brief A line's width at scale 1, (6n - 1) px. */
static int w1(const char *s) { return gfxTextW(s, 1); }

/** @brief Identify @p page as found behind signature 0xE8 @p sig. */
static void ident(uint8_t sig, const uint8_t *page, ew_contact_t *c, ew_ident_t *id) {
	const uint8_t ans[9] = { 0x34, 0x37, 0x31, 0x64, 0xE8, sig, 0x06, 0x01, 0x30 };
	ew_id_contact(ans, c);
	ew_id_efm8(c, page, id);
}

static void testRows() {
	section("EFM8 list: the rows, by group and offset, no revision bytes");
	checkStr("BLHeli_S 33", rowNames(EW_FW_S, 33),
	         "STARTUP POWER|DIRECTION|TIMING|DEMAG|BRAKE ON STOP|TEMPERATURE|LOW-SPEED PROTECTION|"
	         "BEEP STRENGTH|BEACON STRENGTH|BEACON DELAY|STICK PROGRAMMING|MIN THROTTLE|MAX THROTTLE|"
	         "CENTRE THROTTLE|LED");
	checkStr("its groups", rowGroups(EW_FW_S, 33),
	         "MOTOR|MOTOR|MOTOR|MOTOR|BRAKE|LIMITS|LIMITS|SOUND|SOUND|SOUND|INPUT|INPUT|INPUT|INPUT|OTHER");
	checkStr("Bluejay 208", rowNames(EW_FW_BJ, 208),
	         "DIRECTION|TIMING|DEMAG|MIN STARTUP POWER|STARTUP SOUND|MAX STARTUP POWER|POWER SLOPE|"
	         "BRAKING|BRAKE ON STOP|TEMPERATURE|POWER RATING|BEEP STRENGTH|BEACON STRENGTH|BEACON DELAY|"
	         "PWM INDICATOR|LED|SAFETY ARM");
	checkStr("its groups", rowGroups(EW_FW_BJ, 208),
	         "MOTOR|MOTOR|MOTOR|STARTUP|STARTUP|STARTUP|STARTUP|BRAKE|BRAKE|LIMITS|LIMITS|SOUND|SOUND|"
	         "SOUND|OTHER|OTHER|OTHER");
	checkStr("Bluejay 200: DITHERING a motor row, STARTUP POWER a startup row", rowNames(EW_FW_BJ, 200),
	         "DITHERING|DIRECTION|TIMING|DEMAG|STARTUP BOOST|STARTUP SOUND|STARTUP POWER|BRAKE ON STOP|"
	         "TEMPERATURE|LOW-SPEED PROTECTION|BEEP STRENGTH|BEACON STRENGTH|BEACON DELAY|LED");
	checkStr("Bluejay 33", rowGroups(EW_FW_BJ, 33),
	         "MOTOR|MOTOR|MOTOR|STARTUP|BRAKE|LIMITS|LIMITS|SOUND|SOUND|SOUND|OTHER");
	checkStr("BLHeli_S 32: TEMPERATURE on or off", rowNames(EW_FW_S, 32),
	         rowNames(EW_FW_S, 33));
	Efm8Row r[EFM8_ROWS_MAX];
	checkInt("AM32: no rows", efm8Rows(EW_FW_AM, 3, r, EFM8_ROWS_MAX), 0);
	checkInt("no firmware: no rows", efm8Rows(EW_FW_NONE, 33, r, EFM8_ROWS_MAX), 0);
	checkInt("room for 4: 4 rows", efm8Rows(EW_FW_BJ, 208, r, 4), 4);

	// Every layout of both: every field but the revisions, none cut.
	bool all = true;
	for (uint8_t fw : { (uint8_t)EW_FW_S, (uint8_t)EW_FW_BJ }) {
		for (int lay = 0; lay < 256; lay++) {
			int want = 0;
			for (size_t i = 0; ew_map_nth(fw, (uint8_t)lay, i); i++)
				if (ew_map_nth(fw, (uint8_t)lay, i)->off > 0x02) want++;
			if (efm8Rows(fw, (uint8_t)lay, r, EFM8_ROWS_MAX) != want || want > EFM8_ROWS_MAX) all = false;
		}
	}
	checkTrue("every layout: every field listed, none cut", all);
}

static void testValues() {
	section("EFM8 list: values in capitals, DIRECTION's 3D words, RAW");
	const ew_field_t *dir = ew_map_find(EW_FW_BJ, 208, 0x0B);
	char v[32];
	static const char *const DIR[] = { "RAW 0", "NORMAL", "REVERSED", "3D", "3D REVERSED", "RAW 5" };
	for (int raw = 0; raw <= 5; raw++) {
		bool ok = efm8ValueText(dir, (uint8_t)raw, v, sizeof(v));
		char what[48];
		snprintf(what, sizeof(what), "DIRECTION %d: %s", raw, DIR[raw]);
		checkTrue(what, strcmp(v, DIR[raw]) == 0 && ok == (raw >= 1 && raw <= 4));
	}
	checkTrue("3D REVERSED: 11 characters, 65 px", w1("3D REVERSED") == 65);
	efm8ValueText(ew_map_find(EW_FW_BJ, 208, 0x1D), 4, v, sizeof(v));
	checkStr("BEACON DELAY 4", v, "10MIN");
	efm8ValueText(ew_map_find(EW_FW_S, 33, 0x19), 37, v, sizeof(v));
	checkStr("MIN THROTTLE 37", v, "1148US");
	efm8ValueText(ew_map_find(EW_FW_BJ, 208, 0x10), 255, v, sizeof(v));
	checkStr("BRAKING 255", v, "100%");
	efm8ValueText(ew_map_find(EW_FW_BJ, 208, 0x23), 7, v, sizeof(v));
	checkStr("TEMPERATURE 7", v, "140C");
	efm8ValueText(ew_map_find(EW_FW_BJ, 208, 0x29), 2, v, sizeof(v));
	checkStr("POWER RATING 2", v, "2S AND ABOVE");
	bool ok = efm8ValueText(ew_map_find(EW_FW_BJ, 208, 0x29), 17, v, sizeof(v));
	checkTrue("POWER RATING 17: RAW 17, not a value", strcmp(v, "RAW 17") == 0 && !ok);
	ok = efm8ValueText(ew_map_find(EW_FW_S, 33, 0x1B), 0, v, sizeof(v));
	checkTrue("BLHeli_S BEEP STRENGTH 0: RAW 0", strcmp(v, "RAW 0") == 0 && !ok);
	ok = efm8ValueText(ew_map_find(EW_FW_BJ, 208, 0x1B), 0, v, sizeof(v));
	checkTrue("Bluejay BEEP STRENGTH 0: 0", strcmp(v, "0") == 0 && ok);
	checkTrue("no room: false and nothing written", !efm8ValueText(dir, 1, v, 0));

	// Each value of each row beside its label, at the scale the row draws it:
	// at least 6 px between them.
	int worst = 1000;
	char worstAt[64] = "";
	for (uint8_t fw : { (uint8_t)EW_FW_S, (uint8_t)EW_FW_BJ }) {
		for (int lay = 0; lay < 256; lay++) {
			Efm8Row r[EFM8_ROWS_MAX];
			int n = efm8Rows(fw, (uint8_t)lay, r, EFM8_ROWS_MAX);
			for (int i = 0; i < n; i++) {
				for (int raw = 0; raw < 256; raw++) {
					efm8ValueText(r[i].f, (uint8_t)raw, v, sizeof(v));
					int sc = gfxTextW(v, 2) <= UI_FIELD_VALUE_W2 ? 2 : 1;
					int gap = (GFX_W - UI_MARGIN - gfxTextW(v, sc)) - (UI_MARGIN + w1(r[i].f->name));
					if (gap < worst) {
						worst = gap;
						snprintf(worstAt, sizeof(worstAt), "%s %s", r[i].f->name, v);
					}
				}
			}
		}
	}
	printf("  the narrowest gap: %s\n", worstAt);
	checkInt("label to value: the narrowest gap, px", worst, 23);
}

static void testStrip() {
	section("EFM8 list: the strip's caption and read-only lines");
	uint8_t page[256];
	ew_contact_t c;
	ew_ident_t id;
	char l1[EFM8_LINE_CHARS + 1];
	const char *l2 = nullptr;

	sim_bj_page(page);
	ident(0xB2, page, &c, &id);
	efm8Caption(&id, page, c.signature, l1, sizeof(l1));
	checkStr("Bluejay 0.21", l1, "BLUEJAY 0.21  LAYOUT 208  E8B2");
	checkTrue("30 characters, 179 px", strlen(l1) == 30 && w1(l1) == 179);
	checkTrue("a list", efm8ViewOf(&id) == Efm8View::List);
	checkTrue("not read-only", !efm8ReadOnlyLines(&id, page, l1, &l2));
	checkTrue("DIRECTION 1: no 3D", !efm8Is3d(page));
	page[0x0B] = 3;
	checkTrue("DIRECTION 3: 3D", efm8Is3d(page));
	page[0x0B] = 4;
	checkTrue("DIRECTION 4: 3D", efm8Is3d(page));
	page[0x0B] = 2;
	checkTrue("DIRECTION 2: no 3D", !efm8Is3d(page));

	sim_s_page(page);
	ident(0xB1, page, &c, &id);
	efm8Caption(&id, page, c.signature, l1, sizeof(l1));
	checkStr("BLHeli_S 16.7", l1, "BLHELI_S 16.7  LAYOUT 33  E8B1");
	ew_ident_t big = id;
	uint8_t p2[256];
	memcpy(p2, page, sizeof(p2));
	p2[0] = p2[1] = p2[2] = 255;
	efm8Caption(&big, p2, 0xE8B5, l1, sizeof(l1));
	checkTrue("255.255, layout 255: 34 characters, in the line", strlen(l1) == 34);

	// An older layout: Bluejay 0.19, layout 206.
	sim_bj_page(page);
	page[1] = 19;
	page[2] = 206;
	ident(0xB2, page, &c, &id);
	checkTrue("Bluejay 206: read-only", efm8ViewOf(&id) == Efm8View::ReadOnly &&
	          efm8ReadOnlyLines(&id, page, l1, &l2));
	checkStr("its first line", l1, "BLUEJAY 0.19, LAYOUT 206: READ-ONLY.");
	checkTrue("36 characters, 215 px", strlen(l1) == 36 && w1(l1) == 215);
	checkStr("its second line", l2, EFM8_UI_SHOWN_ONLY);
	checkTrue("28 characters, 167 px", w1(l2) == 167);
	page[0] = 255;
	page[1] = 255;
	efm8ReadOnlyLines(&id, page, l1, &l2);
	checkStr("main and sub 255: the version left out", l1, "BLUEJAY LAYOUT 206: READ-ONLY.");
	// BLHeli_S 16.2, layout 32.
	sim_s_page(page);
	page[1] = 2;
	page[2] = 32;
	ident(0xB2, page, &c, &id);
	efm8ReadOnlyLines(&id, page, l1, &l2);
	checkStr("BLHeli_S 32", l1, "BLHELI_S 16.2, LAYOUT 32: READ-ONLY.");
	// Lost tags: BLHeli_S 33 with its tags 0xFF.
	sim_s_page(page);
	memset(page + 0x40, 0xFF, 48);
	ident(0xB2, page, &c, &id);
	checkTrue("tags 0xFF: read-only", efm8ViewOf(&id) == Efm8View::ReadOnly &&
	          id.why == EW_WHY_RESET_PAGE && efm8ReadOnlyLines(&id, page, l1, &l2));
	checkStr("its first line", l1, EFM8_UI_LOST_TAGS);
	checkStr("its second line", l2, EFM8_UI_REFLASH);
	checkTrue("29 and 33 characters, 173 and 197 px", w1(l1) == 173 && w1(l2) == 197);
	// The marker.
	sim_bj_page(page);
	page[0x0E] = 0x00;
	ident(0xB2, page, &c, &id);
	checkTrue("the marker wrong: read-only", efm8ViewOf(&id) == Efm8View::ReadOnly &&
	          id.why == EW_WHY_MARKER && efm8ReadOnlyLines(&id, page, l1, &l2));
	checkStr("its first line", l1, EFM8_UI_MARKER);
	checkStr("its second line", l2, EFM8_UI_SHOWN_ONLY);
	checkTrue("33 characters, 197 px", w1(l1) == 197);
	checkStr("the names", efm8FwName(EW_FW_AM), "EFM8");
}

static void testRefusals() {
	section("EFM8 connect page: a page not taken");
	uint8_t page[256];
	ew_contact_t c;
	ew_ident_t id;
	char st[EFM8_LINE_CHARS + 1];
	const char *l1 = nullptr, *l2 = nullptr;

	memset(page, 0xFF, sizeof(page));
	ident(0xB2, page, &c, &id);
	checkTrue("0xFF throughout: refused", efm8ViewOf(&id) == Efm8View::Refused && efm8PageEmpty(page));
	efm8RefusalLines(&id, &c, page, st, &l1, &l2);
	checkStr("its status", st, "EFM8 E8B2: SETTINGS PAGE EMPTY");
	checkTrue("30 characters, 179 px", w1(st) == 179);
	checkStr("first line", l1, EFM8_UI_EMPTY_1);
	checkStr("second line", l2, EFM8_UI_EMPTY_2);
	checkTrue("33 characters each, 197 px", w1(l1) == 197 && w1(l2) == 197);
	page[EW_MAP_LEN_S - 1] = 0x00;
	checkTrue("one byte of the first 112 not 0xFF: not empty", !efm8PageEmpty(page));
	page[EW_MAP_LEN_S - 1] = 0xFF;
	page[EW_MAP_LEN_S] = 0x00;
	checkTrue("byte 112 does not count", efm8PageEmpty(page));

	sim_bj_page(page);
	ident(0xB1, page, &c, &id);
	efm8RefusalLines(&id, &c, page, st, &l1, &l2);
	checkStr("Bluejay on EFM8BB1", st, "BLUEJAY ON EFM8BB1: NOT TAKEN");
	checkTrue("29 characters, 173 px", w1(st) == 173);
	checkStr("first line", l1, EFM8_UI_REFUSED_1);
	checkStr("second line", l2, EFM8_UI_REFUSED_2);
	checkTrue("33 and 19 characters, 197 and 113 px", w1(l1) == 197 && w1(l2) == 113);

	sim_bj_page(page);
	page[2] = 205;
	ident(0xB2, page, &c, &id);
	efm8RefusalLines(&id, &c, page, st, &l1, &l2);
	checkStr("Bluejay layout 205", st, "BLUEJAY LAYOUT 205: NOT TAKEN");

	sim_s_page(page);
	memset(page + 0x60, 0x20, 16);
	page[1] = 9;
	ident(0xB2, page, &c, &id);
	efm8RefusalLines(&id, &c, page, st, &l1, &l2);
	checkStr("BLHeli_S 16.9", st, "BLHELI_S 16.9: NOT TAKEN");
	checkTrue("24 characters, 143 px", w1(st) == 143);

	sim_s_page(page);
	memcpy(page + 0x60, "SOMETHING ELSE..", 16);
	ident(0xB5, page, &c, &id);
	efm8RefusalLines(&id, &c, page, st, &l1, &l2);
	checkStr("a page not known", st, "EFM8 E8B5: PAGE NOT KNOWN");
	checkTrue("25 characters, 149 px", w1(st) == 149);
	checkStr("first line", l1, EFM8_UI_REFUSED_1);

	memset(page, 0, sizeof(page));
	ident(0xB3, page, &c, &id);
	id.why = c.why;
	efm8RefusalLines(&id, &c, page, st, &l1, &l2);
	checkStr("E8B3: the part", st, "EFM8 E8B3: PART NOT KNOWN");
	checkStr("first line", l1, EFM8_UI_REFUSED_1);
}

static void testNotes() {
	section("EFM8 list: the selected row's notes");
	const char *l1 = nullptr, *l2 = nullptr;
	const ew_field_t *dir = ew_map_find(EW_FW_S, 33, 0x0B);
	checkTrue("DIRECTION 3: the 3D note", efm8RowNote(dir, 3, &l1, &l2) &&
	          strcmp(l1, AM32_UI_3D_1) == 0 && strcmp(l2, AM32_UI_3D_2) == 0);
	checkTrue("DIRECTION 4: the 3D note", efm8RowNote(dir, 4, &l1, &l2) && strcmp(l1, AM32_UI_3D_1) == 0);
	checkTrue("DIRECTION 2: none", !efm8RowNote(dir, 2, &l1, &l2));
	const ew_field_t *t = ew_map_find(EW_FW_BJ, 208, 0x23);
	checkTrue("TEMPERATURE OFF: protection off", efm8RowNote(t, 0, &l1, &l2) &&
	          strcmp(l1, AM32_UI_PROT_1) == 0 && strcmp(l2, AM32_UI_PROT_2) == 0);
	checkTrue("TEMPERATURE 80C: none", !efm8RowNote(t, 1, &l1, &l2));
	const ew_field_t *lsp = ew_map_find(EW_FW_S, 33, 0x24);
	checkTrue("LOW-SPEED PROTECTION OFF: protection off", efm8RowNote(lsp, 0, &l1, &l2) &&
	          strcmp(l1, AM32_UI_PROT_1) == 0);
	checkTrue("LOW-SPEED PROTECTION ON: none", !efm8RowNote(lsp, 1, &l1, &l2));
	const ew_field_t *pr = ew_map_find(EW_FW_BJ, 208, 0x29);
	checkTrue("POWER RATING: how the ESC reads its temperature", efm8RowNote(pr, 2, &l1, &l2) &&
	          strcmp(l1, EFM8_UI_RATING_1) == 0 && strcmp(l2, EFM8_UI_RATING_2) == 0);
	checkTrue("32 and 33 characters, 191 and 197 px", w1(l1) == 191 && w1(l2) == 197);
	checkTrue("BRAKE ON STOP 0: none", !efm8RowNote(ew_map_find(EW_FW_BJ, 208, 0x27), 0, &l1, &l2));
	checkTrue("no field: none", !efm8RowNote(nullptr, 0, &l1, &l2));
}

void runEfm8ViewTests() {
	testRows();
	testValues();
	testStrip();
	testRefusals();
	testNotes();
}
