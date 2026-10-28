/**
 * @file test_hitec_view.cpp
 * @brief What the HITEC SERVO screen shows of a servo (hitec_view.h), on
 *        rcbench's bench images (hitec_reg_fix.h).
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * The rows of each layout in their groups, each value with its unit and its
 * stored value, `RAW` values, the stepper against the encodings and the
 * D series order, the coarse step, each soft rule and its text, a text for
 * every `hitec_ses_t` and `hitec_rx_t`, and the model texts.
 */

#include "check.h"
#include "hitec_view.h"

// rcbench's fixture brings its test harness, whose reporting is not used here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "rcbench/hitec_reg_fix.h"
#pragma GCC diagnostic pop

#include <initializer_list>
#include <stdio.h>
#include <string.h>

/** @brief @p param of @p img as the screen shows it. */
static const char *shown(const hitec_image_t *img, uint8_t layout, uint8_t param, bool *raw = nullptr) {
	static char b[32];
	hitecValueText(img, layout, param, b, sizeof(b), raw);
	return b;
}

/** @brief Whether the rows of @p layout are @p labels in that order, in @p groups. */
static bool rowsAre(uint8_t layout, const char *const *labels, const char *const *groups, int n) {
	HitecRow r[HITEC_ROWS_MAX];
	if (hitecRows(layout, r) != n) return false;
	for (int i = 0; i < n; i++)
		if (strcmp(r[i].label, labels[i]) != 0 || strcmp(r[i].group, groups[i]) != 0) return false;
	return true;
}

static void testRows() {
	section("Hitec view: the rows of each layout");
	static const char *const D[] = {
		"SPEED", "DEAD BAND", "SOFT START", "NEUTRAL", "LEFT END", "RIGHT END", "DIRECTION",
		"FAILSAFE", "OVERLOAD", "SMART SENSOR", "SENSITIVITY", "MODEL", "ID", "POSITION",
		"R04", "R22", "R8A", "R8C", "RD4", "RD6",
	};
	static const char *const DG[] = {
		"MOTION", "MOTION", "MOTION", "TRAVEL", "TRAVEL", "TRAVEL", "TRAVEL",
		"PROTECT", "PROTECT", "PROTECT", "PROTECT", "INFO", "INFO", "INFO",
		"INFO", "INFO", "INFO", "INFO", "INFO", "INFO",
	};
	checkTrue("D series: 20 rows, MOTION, TRAVEL, PROTECT, INFO", rowsAre(HITEC_LAYOUT_D, D, DG, 20));
	static const char *const H5[] = {
		"SPEED", "DEAD BAND", "CENTRE", "LEFT END", "RIGHT END", "DIRECTION", "FAILSAFE",
		"FAILSAFE POS.", "OVERLOAD", "VERSION", "DATE", "SERIAL", "CHECKSUM",
	};
	static const char *const H5G[] = {
		"MOTION", "MOTION", "TRAVEL", "TRAVEL", "TRAVEL", "TRAVEL", "PROTECT",
		"PROTECT", "PROTECT", "INFO", "INFO", "INFO", "INFO",
	};
	checkTrue("HS-5xxx: 13 rows", rowsAre(HITEC_LAYOUT_HS5, H5, H5G, 13));
	static const char *const H7[] = {
		"SPEED", "DEAD BAND", "RESOLUTION", "CENTRE", "LEFT END", "RIGHT END", "DIRECTION",
		"FAILSAFE", "FAILSAFE POS.", "OVERLOAD", "VERSION", "DATE", "SERIAL", "CHECKSUM",
	};
	static const char *const H7G[] = {
		"MOTION", "MOTION", "MOTION", "TRAVEL", "TRAVEL", "TRAVEL", "TRAVEL",
		"PROTECT", "PROTECT", "PROTECT", "INFO", "INFO", "INFO", "INFO",
	};
	checkTrue("HS-7xxx: 14 rows with RESOLUTION", rowsAre(HITEC_LAYOUT_HS7, H7, H7G, 14));
	checkTrue("editable: SPEED and ID; read only: MODEL, SENSITIVITY",
	          hitecEditable(HITEC_LAYOUT_D, HITEC_P_D_SPEED) && hitecEditable(HITEC_LAYOUT_D, HITEC_P_D_ID) &&
	          !hitecEditable(HITEC_LAYOUT_D, HITEC_P_D_MODEL) &&
	          !hitecEditable(HITEC_LAYOUT_D, HITEC_P_D_SENSITIVITY));
	checkTrue("FAILSAFE POS. editable on HS-5xxx, read only on HS-7xxx",
	          hitecEditable(HITEC_LAYOUT_HS5, HITEC_P_C_FS_POSITION) &&
	          !hitecEditable(HITEC_LAYOUT_HS7, HITEC_P_C_FS_POSITION));
	checkTrue("a parameter of another layout is not editable",
	          !hitecEditable(HITEC_LAYOUT_D, HITEC_P_C_SPEED));
}

static void testValues() {
	section("Hitec view: values with their unit and stored value");
	hitec_image_t d, c, h7;
	d485(&d);
	hs5645(&c);
	hs7955(&h7);
	bool raw = true;
	checkStr("D SPEED: percent and stored", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_SPEED, &raw), "100 % (4095)");
	checkTrue("not RAW", !raw);
	checkStr("D DEAD BAND: a step", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_DEAD_BAND), "STEP 1 (1)");
	checkStr("D DIRECTION: a choice", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION), "CW (0)");
	checkStr("D FAILSAFE: OFF", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE), "OFF (1)");
	checkStr("D OVERLOAD: OFF", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_OVERLOAD), "OFF (100)");
	checkStr("D SMART SENSOR: ON", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_SMART_SENSOR), "ON (0)");
	checkStr("D MODEL: the number alone", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_MODEL), "34485");
	checkStr("D POSITION: counts alone", shown(&d, HITEC_LAYOUT_D, HITEC_P_D_POSITION), "8129");
	checkStr("classic CENTRE: microseconds", shown(&c, HITEC_LAYOUT_HS5, HITEC_P_C_CENTRE), "1501 US (3952)");
	checkStr("classic DATE", shown(&c, HITEC_LAYOUT_HS5, HITEC_P_C_DATE), "2015-12-01");
	checkStr("classic CHECKSUM: no stored value", shown(&c, HITEC_LAYOUT_HS5, HITEC_P_C_CHECKSUM), "OK");
	checkStr("HS-7xxx RESOLUTION", shown(&h7, HITEC_LAYOUT_HS7, HITEC_P_C_RESOLUTION), "NORMAL (0)");
	checkStr("HS-7xxx DIRECTION", shown(&h7, HITEC_LAYOUT_HS7, HITEC_P_C_DIRECTION), "CCW (0)");
	checkStr("a classic DEAD BAND no step encodes: RAW",
	         shown(&c, HITEC_LAYOUT_HS5, HITEC_P_C_DEAD_BAND, &raw), "RAW 2");
	checkTrue("and flagged RAW", raw);
	checkStr("a classic OVERLOAD no table value: RAW", shown(&c, HITEC_LAYOUT_HS5, HITEC_P_C_OVERLOAD), "RAW 255");
	hitec_image_t none;
	memset(&none, 0, sizeof(none));
	none.series = HITEC_D;
	checkStr("a byte not read: ?", shown(&none, HITEC_LAYOUT_D, HITEC_P_D_SPEED), "?");
	checkStr("a parameter of another layout: ?", shown(&d, HITEC_LAYOUT_D, HITEC_P_C_SPEED), "?");

	// Each unit's text on a value set through the model.
	hitec_image_t t = d;
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, HITEC_FS_LIMP);
	checkStr("FAILSAFE LIMP", shown(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE), "LIMP (0)");
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 1500);
	char want[32];
	hitec_value_t v;
	hitec_param_get(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, &v);
	snprintf(want, sizeof(want), "1500 US (%lu)", (unsigned long)v.raw);
	checkStr("FAILSAFE at 1500 us", shown(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE), want);
	hitec_image_t q = c;
	hitec_param_set(&q, HITEC_LAYOUT_HS5, HITEC_P_C_FS_POSITION, 6001);
	hitec_param_get(&q, HITEC_LAYOUT_HS5, HITEC_P_C_FS_POSITION, &v);
	if (v.value % 4) {
		snprintf(want, sizeof(want), "%ld.%02ld US (%lu)", (long)(v.value / 4), (long)(v.value % 4) * 25,
		         (unsigned long)v.raw);
		checkStr("a quarter microsecond shows its fraction", shown(&q, HITEC_LAYOUT_HS5, HITEC_P_C_FS_POSITION), want);
	}
	hitec_image_t o = d;
	hitec_param_set(&o, HITEC_LAYOUT_D, HITEC_P_D_OVERLOAD, 50);
	hitec_param_get(&o, HITEC_LAYOUT_D, HITEC_P_D_OVERLOAD, &v);
	snprintf(want, sizeof(want), "%ld %% (%lu)", (long)v.value, (unsigned long)v.raw);
	checkStr("OVERLOAD as a percent", shown(&o, HITEC_LAYOUT_D, HITEC_P_D_OVERLOAD), want);
}

static void testStepper() {
	section("Hitec view: the stepper and the coarse step");
	hitec_image_t d, c;
	d485(&d);
	hs5645(&c);
	checkTrue("SPEED at 100 %: down only",
	          hitecCanStep(&d, HITEC_LAYOUT_D, HITEC_P_D_SPEED, -1) &&
	          !hitecCanStep(&d, HITEC_LAYOUT_D, HITEC_P_D_SPEED, +1));
	hitec_image_t t = d;
	checkInt("SPEED steps down 3", hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, -3), 3);
	hitec_value_t a, b;
	hitec_param_get(&d, HITEC_LAYOUT_D, HITEC_P_D_SPEED, &a);
	hitec_param_get(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, &b);
	checkTrue("to a lower value the model takes", b.value < a.value &&
	          hitec_param_ok(HITEC_LAYOUT_D, HITEC_P_D_SPEED, b.value));
	t = d;
	int taken = hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, -1000);
	checkTrue("a step past the end stops at it", taken > 0 && taken < 1000 &&
	          !hitecCanStep(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, -1));
	checkTrue("a read-only row does not step", !hitecCanStep(&d, HITEC_LAYOUT_D, HITEC_P_D_MODEL, +1) &&
	          hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_MODEL, 1) == 0);
	checkTrue("DIRECTION CW: + to CCW, no -",
	          hitecCanStep(&d, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION, +1) &&
	          !hitecCanStep(&d, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION, -1));

	// The D series order: LEFT END below NEUTRAL below RIGHT END.
	t = d;
	int n = hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_LEFT_END, 20000);
	hitec_value_t left, mid;
	hitec_param_get(&t, HITEC_LAYOUT_D, HITEC_P_D_LEFT_END, &left);
	hitec_param_get(&t, HITEC_LAYOUT_D, HITEC_P_D_NEUTRAL, &mid);
	checkTrue("LEFT END stops below NEUTRAL", n > 0 && n < 20000 && left.value < mid.value);

	// FAILSAFE: LIMP, OFF, then the positions.
	t = d;
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, HITEC_FS_LIMP);
	checkTrue("FAILSAFE LIMP: no -", !hitecCanStep(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, -1));
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 1);
	checkStr("+ from LIMP: OFF", shown(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE), "OFF (1)");
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 1);
	hitec_param_get(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, &a);
	const hitec_param_info_t *fs = hitec_param(HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE);
	checkInt("+ from OFF: the lowest position", a.value, fs->min);
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, -1);
	checkStr("- from the lowest position: OFF", shown(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE), "OFF (1)");
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, -1);
	checkStr("- from OFF: LIMP", shown(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE), "LIMP (0)");

	// A RAW value: the first step puts a table value in its place.
	t = c;
	checkTrue("a RAW DEAD BAND steps", hitecStep(&t, HITEC_LAYOUT_HS5, HITEC_P_C_DEAD_BAND, 1) == 1);
	bool raw = true;
	shown(&t, HITEC_LAYOUT_HS5, HITEC_P_C_DEAD_BAND, &raw);
	checkTrue("to a value that is no longer RAW", !raw);

	checkInt("coarse SPEED: one step", hitecCoarse(HITEC_LAYOUT_D, HITEC_P_D_SPEED), 1);
	checkInt("coarse NEUTRAL: an eleventh of its range", hitecCoarse(HITEC_LAYOUT_D, HITEC_P_D_NEUTRAL), 1490);
	checkInt("coarse classic CENTRE", hitecCoarse(HITEC_LAYOUT_HS5, HITEC_P_C_CENTRE), 437);
	checkInt("coarse of a parameter of another layout: 1", hitecCoarse(HITEC_LAYOUT_D, HITEC_P_C_CENTRE), 1);

	t = d;
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, -1);
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION, 1);
	checkTrue("differs: SPEED and DIRECTION, not MODEL",
	          hitecDiffers(&d, &t, HITEC_LAYOUT_D, HITEC_P_D_SPEED) &&
	          hitecDiffers(&d, &t, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION) &&
	          !hitecDiffers(&d, &t, HITEC_LAYOUT_D, HITEC_P_D_MODEL));
	// The reversal mirrors NEUTRAL and the end points, which count as changed.
	checkTrue("SPEED, DIRECTION and the rows the reversal mirrors changed",
	          hitecChangedCount(&d, &t, HITEC_LAYOUT_D) > 2);
	t = d;
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, -1);
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_SOFT_START, 1);
	checkInt("SPEED and SOFT START: two changed", hitecChangedCount(&d, &t, HITEC_LAYOUT_D), 2);
	checkInt("none against itself", hitecChangedCount(&d, &d, HITEC_LAYOUT_D), 0);
}

static void testSoftRules() {
	section("Hitec view: the soft rules");
	hitec_image_t d, c;
	d485(&d);
	hs5645(&c);
	auto rule = [](HitecRule r) { return HITEC_RULE_BIT(r); };
	hitec_image_t t = d;
	checkInt("no edit: no rule", (long)hitecSoftRules(&d, &t, HITEC_LAYOUT_D), 0);
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, 20);
	checkTrue("SPEED below 30 %", hitecSoftRules(&d, &t, HITEC_LAYOUT_D) == rule(HitecRule::SlowSpeed));
	t = d;
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_SPEED, 50);
	checkInt("SPEED at 50 %: none", (long)hitecSoftRules(&d, &t, HITEC_LAYOUT_D), 0);

	hitec_image_t wide = d;
	hitec_param_set(&wide, HITEC_LAYOUT_D, HITEC_P_D_DEAD_BAND, 3);
	t = wide;
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_DEAD_BAND, 2);
	checkTrue("DEAD BAND below the servo's",
	          hitecSoftRules(&wide, &t, HITEC_LAYOUT_D) == rule(HitecRule::NarrowDeadBand));
	checkInt("wider: none", (long)hitecSoftRules(&t, &wide, HITEC_LAYOUT_D), 0);

	hitec_image_t on = d;
	hitec_param_set(&on, HITEC_LAYOUT_D, HITEC_P_D_OVERLOAD, 50);
	checkTrue("OVERLOAD switched off", hitecSoftRules(&on, &d, HITEC_LAYOUT_D) == rule(HitecRule::OverloadOff));

	t = d;
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_RIGHT_END, -5);
	checkTrue("an end point", hitecSoftRules(&d, &t, HITEC_LAYOUT_D) == rule(HitecRule::EndPoint));
	t = d;
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_NEUTRAL, 5);
	checkTrue("NEUTRAL", hitecSoftRules(&d, &t, HITEC_LAYOUT_D) == rule(HitecRule::Centre));
	t = d;
	hitecStep(&t, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION, 1);
	checkTrue("DIRECTION, with the end points the reversal mirrors",
	          (hitecSoftRules(&d, &t, HITEC_LAYOUT_D) & rule(HitecRule::Direction)) != 0);
	checkTrue("and only DIRECTION's rule for its row",
	          hitecSoftRules(&d, &t, HITEC_LAYOUT_D, HITEC_P_D_DIRECTION) == rule(HitecRule::Direction));
	t = d;
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 1100);
	checkTrue("a D FAILSAFE 400 us from the centre", hitecSoftRules(&d, &t, HITEC_LAYOUT_D) == rule(HitecRule::FailsafeFar));
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, 1300);
	checkInt("200 us from it: none", (long)hitecSoftRules(&d, &t, HITEC_LAYOUT_D), 0);
	hitec_param_set(&t, HITEC_LAYOUT_D, HITEC_P_D_FAILSAFE, HITEC_FS_LIMP);
	checkInt("LIMP: none", (long)hitecSoftRules(&d, &t, HITEC_LAYOUT_D), 0);

	t = c;
	hitec_param_set(&t, HITEC_LAYOUT_HS5, HITEC_P_C_FS_POSITION, 4 * 1900);
	checkTrue("a classic FAILSAFE POS. 400 us from the centre",
	          (hitecSoftRules(&c, &t, HITEC_LAYOUT_HS5) & rule(HitecRule::FailsafeFar)) != 0);
	t = c;
	hitec_param_set(&t, HITEC_LAYOUT_HS5, HITEC_P_C_SPEED, 10);
	checkTrue("a classic SPEED", hitecSoftRules(&c, &t, HITEC_LAYOUT_HS5) & rule(HitecRule::SlowSpeed));
	checkInt("a row's filter leaves other rows' rules out",
	         (long)hitecSoftRules(&c, &t, HITEC_LAYOUT_HS5, HITEC_P_C_CENTRE), 0);

	bool texts = true;
	for (int i = 0; i < (int)HitecRule::Count; i++)
		if (!hitecRuleText((HitecRule)i)[0]) texts = false;
	checkTrue("every rule has a text", texts);
	checkStr("the speed rule's text", hitecRuleText(HitecRule::SlowSpeed),
	         "A SLOW SERVO CAN CAUSE LOSS OF CONTROL OF THE MODEL.");
	checkStr("past the rules: none", hitecRuleText(HitecRule::Count), "");
}

static void testResults() {
	section("Hitec view: a text for every result and reply code");
	uint8_t diff[32];
	memset(diff, 0, sizeof(diff));
	HitecResultText r = hitecResultText(HITEC_SES_OK, 1, 2, 2, diff, 0, 0, HITEC_D);
	checkStr("OK, one value", r.text, "1 VALUE WRITTEN AND VERIFIED.");
	checkTrue("in the done tone", r.tone == HitecTone::Done);
	r = hitecResultText(HITEC_SES_OK, 3, 2, 2, diff, 0, 0, HITEC_D);
	checkStr("OK, three", r.text, "3 VALUES WRITTEN AND VERIFIED.");
	diff[0x54 / 8] |= 1u << (0x54 % 8);
	r = hitecResultText(HITEC_SES_ERR_VERIFY, 1, 2, 2, diff, 0, 0, HITEC_D);
	checkStr("VERIFY names the address", r.text, "THE SERVO DOES NOT HOLD THE VALUES: 0x54. READ AND CHECK.");
	checkTrue("in doubt", r.tone == HitecTone::Doubt);
	for (int a = 0; a < 8; a++) diff[a / 8 + 1] |= 1u << (a % 8);
	r = hitecResultText(HITEC_SES_ERR_VERIFY, 1, 2, 2, diff, 0, 0, HITEC_D);
	checkTrue("more than four: the first four and ...", strstr(r.text, " ...") != nullptr);
	r = hitecResultText(HITEC_SES_ERR_WRITE, 1, 0, 3, nullptr, 0x06, 0, HITEC_CLASSIC);
	checkStr("a classic WRITE with no answer", r.text, "CELL 0x06 GOT NO ANSWER. READ THE SERVO BEFORE A WRITE.");
	r = hitecResultText(HITEC_SES_ERR_WRITE, 1, 0, 3, nullptr, 0x54, 0, HITEC_D);
	checkTrue("a D series one names a register", strncmp(r.text, "REGISTER 0x54", 13) == 0);
	for (uint8_t e : { (uint8_t)HITEC_SES_ERR_NO_SERVO, (uint8_t)HITEC_SES_ERR_CUT,
	                   (uint8_t)HITEC_SES_ERR_LINK, (uint8_t)HITEC_SES_ERR_STEP }) {
		r = hitecResultText(e, 1, 1, 3, nullptr, 0, 0, HITEC_D);
		char what[48];
		snprintf(what, sizeof(what), "code %u mid-plan: the step", e);
		checkStr(what, r.text, "THE WRITE STOPPED AT STEP 2 OF 3. READ THE SERVO BEFORE A WRITE.");
		r = hitecResultText(e, 1, 3, 3, nullptr, 0, 0, HITEC_D);
		snprintf(what, sizeof(what), "code %u after every write: the read-back", e);
		checkStr(what, r.text, "ALL WRITES SENT; READ-BACK FAILED. READ THE SERVO BEFORE A WRITE.");
	}
	r = hitecResultText(HITEC_SES_ERR_WRITE, 1, 3, 3, nullptr, 0, 0, HITEC_D);
	checkTrue("WRITE after every write: the read-back", strncmp(r.text, "ALL WRITES SENT", 15) == 0);
	r = hitecResultText(HITEC_SES_ERR_READ, 1, 3, 3, nullptr, 0, 0, HITEC_D);
	checkTrue("READ: the read-back", strncmp(r.text, "ALL WRITES SENT", 15) == 0 && r.tone == HitecTone::Doubt);
	r = hitecResultText(HITEC_SES_ERR_PLAN, 1, 0, 0, nullptr, 0, HITEC_PLAN_ERR_UNKNOWN, HITEC_D);
	checkStr("PLAN with a byte not read", r.text, "NOTHING WAS WRITTEN: READ THE SERVO FIRST.");
	checkTrue("nothing reached the servo", r.tone == HitecTone::Nothing);
	r = hitecResultText(HITEC_SES_ERR_PLAN, 1, 0, 0, nullptr, 0, HITEC_PLAN_ERR_ARG, HITEC_D);
	checkStr("PLAN otherwise", r.text, "NOTHING WAS WRITTEN: NO PLAN (CODE 1).");
	bool nothing = true;
	for (uint8_t e : { (uint8_t)HITEC_SES_BUSY, (uint8_t)HITEC_SES_ERR_BUSY, (uint8_t)HITEC_SES_ERR_ARG,
	                   (uint8_t)HITEC_SES_ERR_NOT_IN_MODE, (uint8_t)HITEC_SES_ERR_ENTRY,
	                   (uint8_t)HITEC_SES_ERR_ABORTED }) {
		r = hitecResultText(e, 1, 0, 0, nullptr, 0, 0, HITEC_D);
		char want[48];
		snprintf(want, sizeof(want), "NOTHING WAS WRITTEN (CODE %u).", e);
		if (strcmp(r.text, want) != 0 || r.tone != HitecTone::Nothing) nothing = false;
	}
	checkTrue("every other code: nothing was written, with its code", nothing);

	static const char *const RX[] = {
		"", "ARG", "CUT", "OVERFLOW", "SILENT", "EARLY", "LATE", "NAK", "LENGTH", "FRAMING",
		"GAP", "NO ACK", "HEADER", "ADDRESS", "SUM",
	};
	bool words = true;
	for (uint8_t i = 0; i <= HITEC_RX_ERR_SUM; i++)
		if (strcmp(hitecRxWord(i), RX[i]) != 0) words = false;
	checkTrue("a word for every hitec_rx_t", words);
	checkStr("past them: ?", hitecRxWord(HITEC_RX_ERR_SUM + 1), "?");
}

static void testModels() {
	section("Hitec view: models and layouts");
	hitec_image_t img;
	char m[24];
	d485(&img);
	hitecModelText(&img, m, sizeof(m));
	checkStr("the D485HW", m, "D485HW");
	checkTrue("D layout, measured", hitecLayoutOf(&img) == HITEC_LAYOUT_D && hitecModelMeasured(&img));
	hitec_image_put_reg(&img, 0x00, 34999);
	hitecModelText(&img, m, sizeof(m));
	checkStr("another D series number", m, "D SERIES 34999");
	checkTrue("D layout, not measured", hitecLayoutOf(&img) == HITEC_LAYOUT_D && !hitecModelMeasured(&img));
	hs5645(&img);
	hitecModelText(&img, m, sizeof(m));
	checkStr("the HS-5645MG", m, "HS-5645MG");
	checkTrue("HS-5xxx layout, measured", hitecLayoutOf(&img) == HITEC_LAYOUT_HS5 && hitecModelMeasured(&img));
	hs7955(&img);
	checkTrue("HS-7955TG: HS-7xxx layout, measured",
	          hitecLayoutOf(&img) == HITEC_LAYOUT_HS7 && hitecModelMeasured(&img));
	hs5955(&img);
	hitecModelText(&img, m, sizeof(m));
	checkStr("the HS-5955TG", m, "HS-5955TG");
	checkTrue("HS-5xxx layout, not measured", hitecLayoutOf(&img) == HITEC_LAYOUT_HS5 && !hitecModelMeasured(&img));
	hitec_image_t none;
	memset(&none, 0, sizeof(none));
	none.series = HITEC_D;
	hitecModelText(&none, m, sizeof(m));
	checkStr("nothing read: ?", m, "?");
	checkTrue("the series' layout, not measured",
	          hitecLayoutOf(&none) == HITEC_LAYOUT_D && !hitecModelMeasured(&none));
	none.series = HITEC_CLASSIC;
	checkTrue("a classic one: HS-5xxx", hitecLayoutOf(&none) == HITEC_LAYOUT_HS5);
}

void runHitecViewTests() {
	testRows();
	testValues();
	testStepper();
	testSoftRules();
	testResults();
	testModels();
}
