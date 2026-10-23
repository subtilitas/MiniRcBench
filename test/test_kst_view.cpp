/**
 * @file test_kst_view.cpp
 * @brief What the KST screen shows of a servo: rows, values, the stepper and
 *        the texts, against rcbench's bench image and limits as they ship.
 */

#include "check.h"
#include "kst_view.h"
#include "rcbench/kst_sim.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void testRows() {
	section("KST view: the rows");
	int n;
	const KstRow *r = kstRows(&n);
	checkInt("27 rows, one per named field", n, 27);
	int editable = 0;
	bool ordered = true, unique = true;
	static const char *const GROUPS[] = { "TUNE", "PULSE", "FAILSAFE", "LOCKED" };
	int g = 0;
	for (int i = 0; i < n; i++) {
		while (g < 4 && strcmp(r[i].group, GROUPS[g]) != 0) g++;
		if (g == 4) ordered = false;
		if (kstRowEditable(i)) editable++;
		bool wantEdit = kst_field(r[i].field)->access == KST_ACCESS_EDITABLE;
		if (kstRowEditable(i) != wantEdit || (strcmp(r[i].group, "LOCKED") == 0) == wantEdit)
			ordered = false;
		for (int k = 0; k < i; k++)
			if (r[k].field == r[i].field) unique = false;
		if (kstRowOf(r[i].field) != i) unique = false;
	}
	checkTrue("TUNE, PULSE, FAILSAFE, then LOCKED; LOCKED is the read-only rows", ordered);
	checkInt("16 editable rows", editable, 16);
	checkTrue("each field once", unique);
	int unnamed = 0;
	for (int f = 0; f < KST_F_COUNT; f++)
		if (kst_field((kst_field_id_t)f)->name[0] && kstRowOf((kst_field_id_t)f) < 0) unnamed++;
	checkInt("no named field without a row", unnamed, 0);
	checkInt("an unnamed field has no row", kstRowOf(KST_F_R06), -1);
	checkTrue("a row out of range is not editable", !kstRowEditable(-1) && !kstRowEditable(27));
}

static void valueIs(kst_field_id_t f, uint16_t raw, const char *want) {
	char buf[24], what[64];
	kstValueText(f, raw, buf, sizeof(buf));
	snprintf(what, sizeof(what), "field %d raw %u", (int)f, (unsigned)raw);
	checkStr(what, buf, want);
}

static void testValues() {
	section("KST view: values with their unit");
	kst_image_t b = sim_bench_image();
	valueIs(KST_F_DUTY, kst_field_get(&b, KST_F_DUTY), "96 %");
	valueIs(KST_F_BOOST, kst_field_get(&b, KST_F_BOOST), "300.0 US");
	valueIs(KST_F_DEAD_BAND, kst_field_get(&b, KST_F_DEAD_BAND), "2.0 US");
	valueIs(KST_F_NEUTRAL, kst_field_get(&b, KST_F_NEUTRAL), "1500.2 US");
	valueIs(KST_F_STRETCH, kst_field_get(&b, KST_F_STRETCH), "X1");
	valueIs(KST_F_SOFT_START, kst_field_get(&b, KST_F_SOFT_START), "ON");
	valueIs(KST_F_REVERSION, kst_field_get(&b, KST_F_REVERSION), "OFF");
	valueIs(KST_F_UNCONT_TIME, kst_field_get(&b, KST_F_UNCONT_TIME), "2.0 S");
	valueIs(KST_F_SPD, kst_field_get(&b, KST_F_SPD), "1.0 %");
	valueIs(KST_F_PROT_POT_ERR, kst_field_get(&b, KST_F_PROT_POT_ERR), "0 OF 12");
	valueIs(KST_F_LEFT_ANGLE, kst_field_get(&b, KST_F_LEFT_ANGLE), "150");
	valueIs(KST_F_STRETCH, 6, "RAW 6");

	char x[24], y[24];
	kstValuePair(KST_F_DUTY, 245, 244, x, y, sizeof(x));
	checkTrue("two raw values that read the same carry their raw value",
	          strcmp(x, "96 % (245)") == 0 && strcmp(y, "96 % (244)") == 0);
	kstValuePair(KST_F_DUTY, 245, 250, x, y, sizeof(x));
	checkTrue("two that read apart do not", strcmp(x, "96 %") == 0 && strcmp(y, "98 %") == 0);
	kstValuePair(KST_F_DUTY, 245, 245, x, y, sizeof(x));
	checkTrue("nor one value", strcmp(x, "96 %") == 0 && strcmp(y, "96 %") == 0);
}

static void testStepper() {
	section("KST view: the stepper reaches legal values only");
	const kst_image_t backup = sim_bench_image();
	kst_image_t t = backup;
	checkInt("MAX. DUTY from 96 %: 5 steps to 98 %", kstStep(&backup, &t, KST_F_DUTY, 100), 5);
	checkInt("raw 250, the hard limit", kst_field_get(&t, KST_F_DUTY), KST_DUTY_RAW_MAX);
	checkTrue("no step further up", !kstCanStep(&backup, &t, KST_F_DUTY, +1));
	checkTrue("down still steps", kstCanStep(&backup, &t, KST_F_DUTY, -1));
	checkTrue("the duty copy follows", t.r[1] == t.r[2]);
	checkTrue("no hard rule after stepping", kstHardRules(&backup, &t) == 0);
	checkTrue("a soft rule instead: duty raised",
	          (kstSoftRules(&backup, &t) & KST_RULE_BIT(KST_S_DUTY_RAISED)) != 0);

	t = backup;
	checkInt("STRETCH from x1: 5 steps to x32, index 6 and 7 skipped",
	         kstStep(&backup, &t, KST_F_STRETCH, 10), 5);
	checkInt("index 5", kst_field_get(&t, KST_F_STRETCH), KST_STRETCH_INDEX_MAX);

	t = backup;
	int down = kstStep(&backup, &t, KST_F_DUTY, -1000);
	checkInt("MAX. DUTY down to 10 %", kst_field_get(&t, KST_F_DUTY), KST_DUTY_RAW_MIN);
	checkInt("in 219 steps", down, 245 - (int)KST_DUTY_RAW_MIN);

	t = backup;
	kstStep(&backup, &t, KST_F_UNCONT_POS, 4000);
	uint16_t upper = kst_field_get(&t, KST_F_PULSE_UPPER);
	checkTrue("UNCONTROLLED POS stops at PULSE UPPER",
	          kst_field_get(&t, KST_F_UNCONT_POS) == upper && kstHardRules(&backup, &t) == 0);

	// A hard rule the image already breaks does not stop another field.
	t = backup;
	kst_field_edit(&t, KST_F_LEFT_ANGLE, 0);
	kst_rules_t before = kstHardRules(&backup, &t);
	checkTrue("LEFT ANGLE 0 breaks a hard rule", (before & KST_RULE_BIT(KST_H_LEFT_ZERO)) != 0);
	checkInt("MAX. DUTY still steps", kstStep(&backup, &t, KST_F_DUTY, -1), 1);
	checkTrue("and adds no hard rule", kstHardRules(&backup, &t) == before);

	t = backup;
	checkInt("a LOCKED field does not step", kstStep(&backup, &t, KST_F_LOCK, 1), 0);
	checkTrue("nor a field with no edit path", !kstCanStep(&backup, &t, KST_F_NODE_ADDR, -1));
	checkTrue("nothing changed", memcmp(t.r, backup.r, KST_REG_COUNT) == 0);

	// + raises the value shown, whatever way the raw value goes.
	t = backup;
	checkInt("DEAD BAND +: one step", kstStep(&backup, &t, KST_F_DEAD_BAND, 1), 1);
	char was[24], is[24];
	kstValueText(KST_F_DEAD_BAND, kst_field_get(&backup, KST_F_DEAD_BAND), was, sizeof(was));
	kstValueText(KST_F_DEAD_BAND, kst_field_get(&t, KST_F_DEAD_BAND), is, sizeof(is));
	checkTrue("DEAD BAND +: 2.0 US to 2.2 US, the raw value down",
	          strcmp(was, "2.0 US") == 0 && strcmp(is, "2.2 US") == 0 &&
	          kst_field_get(&t, KST_F_DEAD_BAND) < kst_field_get(&backup, KST_F_DEAD_BAND));
	int rising = 0, steppable = 0;
	int nrows;
	const KstRow *rows = kstRows(&nrows);
	for (int i = 0; i < nrows; i++) {
		if (!kstRowEditable(i)) continue;
		for (int dir = -1; dir <= 1; dir += 2) {
			kst_image_t u = backup;
			if (kstStep(&backup, &u, rows[i].field, dir) != 1) continue;
			steppable++;
			int32_t d0 = 0, d1 = 0;
			kst_field_display(rows[i].field, kst_field_get(&backup, rows[i].field), &d0);
			kst_field_display(rows[i].field, kst_field_get(&u, rows[i].field), &d1);
			// One raw step of MAX. DUTY can round to the same percent.
			if ((dir > 0 && d1 >= d0) || (dir < 0 && d1 <= d0)) rising++;
		}
	}
	checkTrue("every field: + never lowers and - never raises the value shown", steppable > 16 && rising == steppable);

	checkInt("MAX. DUTY's coarse step: 24 raw", kstCoarse(KST_F_DUTY), 24);
	checkInt("a one-bit field's: 1", kstCoarse(KST_F_SOFT_START), 1);

	t = backup;
	kstStep(&backup, &t, KST_F_DEAD_BAND, 1);
	kstStep(&backup, &t, KST_F_BOOST, -1);
	checkTrue("kstChanged names the two fields",
	          kstChanged(&backup, &t) == (KST_FIELD_BIT(KST_F_DEAD_BAND) | KST_FIELD_BIT(KST_F_BOOST)));
	checkTrue("and nothing for equal images", kstChanged(&backup, &backup) == 0);
}

static bool upper(const char *s) {
	for (; *s; s++)
		if (islower((unsigned char)*s)) return false;
	return true;
}

static void testTexts() {
	section("KST view: every rule, result and reply code has its text");
	bool rules = true;
	for (int i = 0; i < KST_RULE_COUNT; i++) {
		const char *t = kstRuleText((kst_rule_t)i);
		if (!t[0] || strlen(t) > 58 || !upper(t)) rules = false;
	}
	checkTrue("49 rule texts, upper case, 58 characters at most", rules);
	checkStr("soft start's", kstRuleText(KST_S_SOFT_START_OFF),
	         "WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.");
	checkStr("outside the rules: none", kstRuleText((kst_rule_t)KST_RULE_COUNT), "");

	bool results = true;
	for (int r = 0; r <= KST_SES_ERR_NO_PLAN; r++) {
		KstResultText t = kstResultText((kst_ses_t)r, KST_PLAN_OK);
		if (!t.text || !t.text[0] || !upper(t.text)) results = false;
	}
	checkTrue("a text for every kst_ses_t", results);
	checkStr("OK", kstResultText(KST_SES_OK, 0).text, "WRITE VERIFIED");
	checkTrue("OK is done", kstResultText(KST_SES_OK, 0).tone == KstTone::Done);
	checkTrue("a torn write is in doubt", kstResultText(KST_SES_ERR_TORN, 0).tone == KstTone::Doubt);
	checkTrue("a roll-back wrote nothing that stays",
	          kstResultText(KST_SES_ERR_ROLLED_BACK, 0).tone == KstTone::Nothing);
	checkStr("no plan: by the planner's result",
	         kstResultText(KST_SES_ERR_NO_PLAN, KST_PLAN_ERR_TOO_COMPLEX).text,
	         "TOO MANY CHANGES AT ONCE: WRITE FEWER. NOTHING WAS WRITTEN.");
	checkStr("no path", kstResultText(KST_SES_ERR_NO_PLAN, KST_PLAN_ERR_NO_PATH).text,
	         "NO SAFE ORDER OF WRITES LEADS THERE. NOTHING WAS WRITTEN.");
	checkStr("a value out of range", kstResultText((kst_ses_t)99, 0).text, "UNKNOWN RESULT");

	checkStr("a late reply", kstRxWord(KST_RX_LATE), "LATE");
	checkStr("a bad rate", kstRxWord(KST_RX_RATE), "RATE");
	checkStr("a valid reply: no word", kstRxWord(KST_RX_OK), "");
	checkStr("out of range", kstRxWord(200), "UNKNOWN");

	bool files = true;
	for (int e = KST_FILE_ERR_ARG; e <= KST_FILE_ERR_TYPE; e++)
		if (!kstFileErrText((kst_file_err_t)e)[0]) files = false;
	checkTrue("a text for every kst_file_err_t", files);

	kst_image_t b = sim_bench_image();
	static char text[KST_FILE_MAX];
	size_t len = kst_file_format(&b, text, sizeof(text));
	char name[32];
	kstFileLineName(text, len, 3, name, sizeof(name));
	checkStr("line 3 of a file names MAX. DUTY", name, "MAX. DUTY");
	kstFileLineName(text, len, 6, name, sizeof(name));
	checkStr("line 6: SOFT START, its underscore a space", name, "SOFT START");
	kstFileLineName(text, len, 1, name, sizeof(name));
	checkStr("the header line names no field", name, "");
	kstFileLineName(text, len, 999, name, sizeof(name));
	checkStr("a line past the end names none", name, "");
}

static void testBadRegs() {
	section("KST view: the registers a read could not settle");
	char line[48], more[48];
	kstBadRegsText(0, line, more, sizeof(line));
	checkTrue("none: no line", !line[0] && !more[0]);
	kstBadRegsText((1u << 3) | (1u << 0x0A), line, more, sizeof(line));
	checkStr("two", line, "REGISTERS 03 0A");
	checkStr("and nothing more", more, "");
	kstBadRegsText(0x1FFEu, line, more, sizeof(line));  // 01 to 0C
	checkStr("twelve: the first nine", line, "REGISTERS 01 02 03 04 05 06 07 08 09");
	checkTrue("within 37 characters", strlen(line) <= 37);
	checkStr("the rest counted", more, "AND 3 MORE");
	kstBadRegsText(0xFFFFFFFFu, line, more, sizeof(line));
	checkStr("all 32", more, "AND 23 MORE");
}

static void testWrap() {
	section("KST view: lines broken at spaces");
	char l[3][48];
	int n = kstWrap("WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.", 23, l, 3);
	checkInt("three lines", n, 3);
	checkStr("first", l[0], "WITHOUT SOFT START THE");
	checkStr("second", l[1], "SERVO JUMPS AT");
	checkStr("third", l[2], "POWER-ON.");
	n = kstWrap("ONE TWO THREE FOUR FIVE SIX SEVEN", 9, l, 2);
	checkInt("two lines at most", n, 2);
	checkStr("the last ends in dots", l[1], "THREE...");
	n = kstWrap("ABCDEFGHIJKLMNOP", 8, l, 3);
	checkStr("a word longer than the line is cut", l[0], "ABCDEFGH");
	checkInt("empty text: no line", kstWrap("", 10, l, 3), 0);
	n = kstWrap("ABCDEFGH IJKL", 1, l, 1);
	checkStr("a width below 4 counts as 4", l[0], "A...");
}

void runKstViewTests() {
	testRows();
	testValues();
	testStepper();
	testTexts();
	testWrap();
	testBadRegs();
}
