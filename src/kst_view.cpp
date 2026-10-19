/**
 * @file kst_view.cpp
 * @brief What the KST screen shows of a servo. @see kst_view.h
 */

#include "kst_view.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// rows
// ---------------------------------------------------------------------------

/** @brief rcbench's pages, then the read-only fields. */
static const KstRow ROWS[] = {
	{ "TUNE",     "MAX. DUTY",          KST_F_DUTY },
	{ "TUNE",     "BOOST",              KST_F_BOOST },
	{ "TUNE",     "STRETCH",            KST_F_STRETCH },
	{ "TUNE",     "SOFT START",         KST_F_SOFT_START },
	{ "TUNE",     "SPEED LIMIT",        KST_F_SPD_SEL },
	{ "TUNE",     "SPEED",              KST_F_SPD },
	{ "TUNE",     "DEAD BAND",          KST_F_DEAD_BAND },
	{ "PULSE",    "PULSE LOWER",        KST_F_PULSE_LOWER },
	{ "PULSE",    "PULSE UPPER",        KST_F_PULSE_UPPER },
	{ "PULSE",    "NEUTRAL",            KST_F_NEUTRAL },
	{ "PULSE",    "REVERSION",          KST_F_REVERSION },
	{ "PULSE",    "LEFT ANGLE",         KST_F_LEFT_ANGLE },
	{ "PULSE",    "RIGHT ANGLE",        KST_F_RIGHT_ANGLE },
	{ "FAILSAFE", "ALLOW UNCONT",       KST_F_ALLOW_UNCONT },
	{ "FAILSAFE", "UNCONTROLLED POS",   KST_F_UNCONT_POS },
	{ "FAILSAFE", "UNCONTROLLED TIME",  KST_F_UNCONT_TIME },
	{ "LOCKED",   "20K SEL",            KST_F_20K_SEL },
	{ "LOCKED",   "LOCK",               KST_F_LOCK },
	{ "LOCKED",   "PROT1 CANCLE",       KST_F_PROT1_CANCLE },
	{ "LOCKED",   "PROT1 EN",           KST_F_PROT1_EN },
	{ "LOCKED",   "PROT1 TIME",         KST_F_PROT1_TIME },
	{ "LOCKED",   "PROT1 PWM",          KST_F_PROT1_PWM },
	{ "LOCKED",   "PROT POT ERR",       KST_F_PROT_POT_ERR },
	{ "LOCKED",   "POT SAM TIMES",      KST_F_POT_SAM_TIMES },
	{ "LOCKED",   "PROT PWM ERR",       KST_F_PROT_PWM_ERR },
	{ "LOCKED",   "NODE ADDR",          KST_F_NODE_ADDR },
	{ "LOCKED",   "SERVO / GYRO",       KST_F_GYRO },
};
static constexpr int ROW_COUNT = (int)(sizeof(ROWS) / sizeof(ROWS[0]));
static_assert(ROW_COUNT == 27, "the 27 named fields of the field table");

const KstRow *kstRows(int *count) {
	*count = ROW_COUNT;
	return ROWS;
}

bool kstRowEditable(int i) {
	return i >= 0 && i < ROW_COUNT &&
	       kst_field(ROWS[i].field)->access == KST_ACCESS_EDITABLE;
}

int kstRowOf(kst_field_id_t field) {
	for (int i = 0; i < ROW_COUNT; i++)
		if (ROWS[i].field == field) return i;
	return -1;
}

// ---------------------------------------------------------------------------
// values
// ---------------------------------------------------------------------------

/** @brief A tenth as `12.3`, with its sign. */
static void tenths(int32_t v, char *buf, size_t n, const char *unit) {
	const char *sign = v < 0 ? "-" : "";
	int32_t a = v < 0 ? -v : v;
	snprintf(buf, n, "%s%ld.%ld%s", sign, (long)(a / 10), (long)(a % 10), unit);
}

void kstValueText(kst_field_id_t field, uint16_t raw, char *buf, size_t n) {
	const kst_field_t *f = kst_field(field);
	int32_t d = 0;
	if (!f || !kst_field_display(field, raw, &d)) {
		snprintf(buf, n, "RAW %u", (unsigned)raw);
		return;
	}
	switch ((kst_unit_t)f->unit) {
		case KST_UNIT_BOOL:          snprintf(buf, n, "%s", d ? "ON" : "OFF"); return;
		case KST_UNIT_TENTH_US:      tenths(d, buf, n, " US"); return;
		case KST_UNIT_PERCENT:       snprintf(buf, n, "%ld %%", (long)d); return;
		case KST_UNIT_TENTH_PERCENT: tenths(d, buf, n, " %"); return;
		case KST_UNIT_TENTH_S:       tenths(d, buf, n, " S"); return;
		case KST_UNIT_FACTOR:        snprintf(buf, n, "X%ld", (long)d); return;
		case KST_UNIT_COUNT_OF:
			snprintf(buf, n, "%ld OF %u", (long)d, (unsigned)(f->lo_bits + f->hi_bits));
			return;
		default:                     snprintf(buf, n, "%ld", (long)d); return;
	}
}

void kstValuePair(kst_field_id_t field, uint16_t from, uint16_t to, char *a, char *b, size_t n) {
	kstValueText(field, from, a, n);
	kstValueText(field, to, b, n);
	if (from == to || strcmp(a, b) != 0) return;
	size_t la = strlen(a), lb = strlen(b);
	snprintf(a + la, n - la, " (%u)", (unsigned)from);
	snprintf(b + lb, n - lb, " (%u)", (unsigned)to);
}

// ---------------------------------------------------------------------------
// the stepper
// ---------------------------------------------------------------------------

kst_rules_t kstHardRules(const kst_image_t *backup, const kst_image_t *target) {
	return kst_limits_edit(backup, target, 0, nullptr) & KST_RULES_HARD;
}

kst_rules_t kstSoftRules(const kst_image_t *backup, const kst_image_t *target) {
	return kst_limits_edit(backup, target, 0, nullptr) & ~KST_RULES_HARD;
}

/**
 * @brief One step of @p field in @p dir: the next raw value that adds no hard
 *        rule. False, and @p target unchanged, when none lies ahead.
 */
static bool stepOnce(const kst_image_t *backup, kst_image_t *target, kst_field_id_t field,
                     int dir) {
	const kst_field_t *f = kst_field(field);
	if (!f || f->access != KST_ACCESS_EDITABLE) return false;
	kst_rules_t now = kstHardRules(backup, target);
	int32_t max = kst_field_raw_max(field);
	for (int32_t raw = (int32_t)kst_field_get(target, field) + dir; raw >= 0 && raw <= max;
	     raw += dir) {
		kst_image_t c = *target;
		int32_t d;
		// A raw value whose display is not established is never offered.
		if (!kst_field_display(field, (uint16_t)raw, &d)) continue;
		if (!kst_field_edit(&c, field, (uint16_t)raw)) continue;
		if (kstHardRules(backup, &c) & ~now) continue;
		*target = c;
		return true;
	}
	return false;
}

int kstStep(const kst_image_t *backup, kst_image_t *target, kst_field_id_t field, int steps) {
	int dir = steps < 0 ? -1 : 1;
	int n = steps < 0 ? -steps : steps;
	int taken = 0;
	while (taken < n && stepOnce(backup, target, field, dir)) taken++;
	return taken;
}

bool kstCanStep(const kst_image_t *backup, const kst_image_t *target, kst_field_id_t field,
                int dir) {
	kst_image_t c = *target;
	return stepOnce(backup, &c, field, dir < 0 ? -1 : 1);
}

int kstCoarse(kst_field_id_t field) {
	int max = kst_field_raw_max(field);
	int c = (max + 10) / 11;
	return c < 1 ? 1 : c;
}

kst_field_set_t kstChanged(const kst_image_t *a, const kst_image_t *b) {
	kst_field_set_t s = 0;
	for (int i = 0; i < ROW_COUNT; i++) {
		kst_field_id_t f = ROWS[i].field;
		if (kst_field(f)->access == KST_ACCESS_EDITABLE && kst_field_get(a, f) != kst_field_get(b, f))
			s |= KST_FIELD_BIT(f);
	}
	return s;
}

// ---------------------------------------------------------------------------
// texts
// ---------------------------------------------------------------------------

/** @brief rcbench's texts of the rules (`ui_text.def`), in upper case. */
static const char *const RULE_TEXT[KST_RULE_COUNT] = {
	"MAX. DUTY IS ABOVE ITS LIMIT.",
	"MAX. DUTY IS BELOW ITS LIMIT.",
	"THE SERVO'S TWO DUTY REGISTERS DIFFER.",
	"BOOST IS ABOVE ITS LIMIT.",
	"THIS STRETCH VALUE IS NOT CONFIRMED.",
	"SPEED IS ABOVE ITS LIMIT.",
	"THE SPEED LIMIT NEEDS A SPEED ABOVE THE MINIMUM.",
	"FAILSAFE POSITION IS BELOW ITS LIMIT.",
	"FAILSAFE POSITION IS ABOVE ITS LIMIT.",
	"FAILSAFE POSITION IS OUTSIDE PULSE LOWER TO UPPER.",
	"FAILSAFE TIME IS BELOW ITS LIMIT.",
	"FAILSAFE TIME IS ABOVE ITS LIMIT.",
	"PULSE LOWER IS BELOW ITS LIMIT.",
	"PULSE LOWER IS TOO CLOSE TO NEUTRAL.",
	"PULSE UPPER IS ABOVE ITS LIMIT.",
	"PULSE UPPER IS TOO CLOSE TO NEUTRAL.",
	"PULSE LOWER AND UPPER ARE TOO CLOSE TOGETHER.",
	"NEUTRAL IS BELOW ITS LIMIT.",
	"NEUTRAL IS ABOVE ITS LIMIT.",
	"LEFT ANGLE CANNOT BE 0.",
	"RIGHT ANGLE CANNOT BE 0.",
	"GYRO MODE: ITS LIMITS ARE NOT KNOWN. NO WRITE.",
	"A LOCKED VALUE DIFFERS. IT IS NOT WRITTEN.",
	"A VALUE THAT IS NEVER WRITTEN DIFFERS.",
	"HIGHER DUTY RAISES STALL CURRENT AND HEAT.",
	"LOW DUTY WEAKENS THE SERVO; IT MAY NOT HOLD THE SURFACE.",
	"HIGHER BOOST CAN MAKE THE SERVO BUZZ AND RUN HOT.",
	"HIGHER STRETCH CAN MAKE THE SERVO OSCILLATE.",
	"WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.",
	"SPEED LIMIT ACTIVE: THE SERVO MOVES AT THE SET SPEED.",
	"A SLOW SERVO CAN LOSE CONTROL OF THE MODEL.",
	"A NARROW DEAD BAND CAN CAUSE HUNTING AND HEAT.",
	"A VERY NARROW DEAD BAND: HUNTING AND HEAT ARE LIKELY.",
	"ON SIGNAL LOSS THE SERVO MOVES TO THE FAILSAFE POSITION.",
	"FAILSAFE POSITION IS FAR FROM CENTRE.",
	"SHORT DROPOUTS WILL TRIGGER THE FAILSAFE MOVE.",
	"END POINT CLOSE TO NEUTRAL: LITTLE TRAVEL ON THAT SIDE.",
	"END POINT CHANGED: TRAVEL PER STICK MOVEMENT CHANGES.",
	"END POINT CLOSE TO NEUTRAL: LITTLE TRAVEL ON THAT SIDE.",
	"END POINT CHANGED: TRAVEL PER STICK MOVEMENT CHANGES.",
	"CENTRE MOVES; ONE SIDE COMES CLOSER TO THE STOP.",
	"DIRECTION IS REVERSED. CHECK EVERY SURFACE BEFORE FLIGHT.",
	"MORE TRAVEL CAN DRIVE THE SERVO INTO ITS STOP AND BURN IT.",
	"SMALL ANGLE: LITTLE TRAVEL FOR THE FULL STICK.",
	"MORE TRAVEL CAN DRIVE THE SERVO INTO ITS STOP AND BURN IT.",
	"SMALL ANGLE: LITTLE TRAVEL FOR THE FULL STICK.",
	"PULSE STANDARD CHANGES. A WRONG STANDARD RUNS TO THE STOP.",
	"NARROW DEAD BAND WITH RAISED GAIN: RISK OF OSCILLATION.",
	"HIGH DUTY WITH MORE TRAVEL: A STALL CAN DESTROY THE SERVO.",
};
static_assert(sizeof(RULE_TEXT) / sizeof(RULE_TEXT[0]) == KST_RULE_COUNT, "one text per rule");

const char *kstRuleText(kst_rule_t rule) {
	return (unsigned)rule < KST_RULE_COUNT ? RULE_TEXT[rule] : "";
}

/** @brief One line per `kst_ses_t`, in its order. */
static const KstResultText RESULT_TEXT[] = {
	{ "WRITE VERIFIED",                                     KstTone::Done },     // OK
	{ "THE OPERATION RUNS",                                 KstTone::Nothing },  // BUSY
	{ "ANOTHER OPERATION RUNS. NOTHING WAS WRITTEN.",       KstTone::Nothing },  // ERR_BUSY
	{ "THE SESSION REFUSED THE REQUEST. NOTHING WAS WRITTEN.", KstTone::Nothing },  // ERR_ARG
	{ "NO SERVO ANSWERS. CHECK THE PLUG AND THE SUPPLY.",   KstTone::Nothing },  // NO_SERVO
	{ "THE SERVO IS NOT IN PROGRAMMING MODE.",              KstTone::Nothing },  // NOT_IN_MODE
	{ "READ FAILED: NO 3 EQUAL READS",                      KstTone::Nothing },  // READ
	{ "NO BACKUP YET: READ THE SERVO FIRST.",               KstTone::Nothing },  // NO_BACKUP
	{ "READ ONLY: UNKNOWN LAYOUT. NOTHING WAS WRITTEN.",    KstTone::Nothing },  // FINGERPRINT
	{ "IN DOUBT: ONLY A RESTORE IS TAKEN.",                 KstTone::Doubt },    // LOCKED
	{ "THE SESSION REFUSED THE PLAN. NOTHING WAS WRITTEN.", KstTone::Nothing },  // BAD_PLAN
	{ "THE UNCHECKED RESTORE NEEDS A SECOND HOLD.",         KstTone::Nothing },  // UNCONFIRMED
	{ "THE SERVO DIFFERS FROM THE READ. NOTHING WAS WRITTEN.", KstTone::Nothing },  // CHANGED
	{ "A WRITE DID NOT TAKE. NOTHING WAS WRITTEN.",         KstTone::Nothing },  // WRITE_FAILED
	{ "ROLLED BACK: THE SERVO HOLDS ITS OLD VALUES",        KstTone::Nothing },  // ROLLED_BACK
	{ "IN DOUBT: THE ROLL-BACK FAILED. RESTORE FROM FILES.", KstTone::Doubt },  // TORN
	{ "IN DOUBT: A REGISTER CHANGED THAT WAS NOT WRITTEN. RESTORE FROM FILES.", KstTone::Doubt },  // UNINTENDED
	{ "IN DOUBT: THE SERVO DOES NOT HOLD THE VALUES. RESTORE FROM FILES.", KstTone::Doubt },  // VERIFY
	{ "THE LINE FAILED. READ THE SERVO AGAIN.",             KstTone::Doubt },    // DRIVER
	{ "ABORTED. READ THE SERVO AGAIN.",                     KstTone::Doubt },    // ABORTED
	{ "NO PLAN. NOTHING WAS WRITTEN.",                      KstTone::Nothing },  // NO_PLAN
};
static_assert(sizeof(RESULT_TEXT) / sizeof(RESULT_TEXT[0]) == KST_SES_ERR_NO_PLAN + 1,
              "one text per kst_ses_t");

KstResultText kstResultText(kst_ses_t result, uint8_t planResult) {
	if (result == KST_SES_ERR_NO_PLAN) {
		switch (planResult) {
			case KST_PLAN_ERR_RULES:
				return { "THE VALUES BREAK A LIMIT. NOTHING WAS WRITTEN.", KstTone::Nothing };
			case KST_PLAN_ERR_R00:
				return { "REGISTER 00 DIFFERS. IT IS NEVER WRITTEN.", KstTone::Nothing };
			case KST_PLAN_ERR_PAIRING:
				return { "THE PAIRING VALUE DIFFERS. NOTHING WAS WRITTEN.", KstTone::Nothing };
			case KST_PLAN_ERR_NO_PATH:
				return { "NO SAFE ORDER OF WRITES LEADS THERE. NOTHING WAS WRITTEN.",
				         KstTone::Nothing };
			case KST_PLAN_ERR_TOO_COMPLEX:
				return { "TOO MANY CHANGES AT ONCE: WRITE FEWER. NOTHING WAS WRITTEN.",
				         KstTone::Nothing };
			default: break;
		}
	}
	if ((unsigned)result <= KST_SES_ERR_NO_PLAN) return RESULT_TEXT[result];
	return { "UNKNOWN RESULT", KstTone::Doubt };
}

const char *kstRxWord(uint8_t rx) {
	static const char *const WORDS[] = {
		"", "NO REPLY", "EARLY", "LINE HIGH", "LATE", "CELL", "LENGTH", "RATE",
		"FLAGS", "LONG LAST EDGE", "WINDOW", "OVERFLOW", "CAPTURE",
	};
	static_assert(sizeof(WORDS) / sizeof(WORDS[0]) == KST_RX_CAPTURE + 1, "one word per kst_rx_t");
	return rx <= KST_RX_CAPTURE ? WORDS[rx] : "UNKNOWN";
}

const char *kstFileErrText(kst_file_err_t err) {
	static const char *const TEXT[] = {
		"",                                   // OK
		"NO FILE",                            // ARG
		"THE FILE IS TOO LARGE",              // OVERSIZED
		"NOT A KST FILE",                     // NOT_KST
		"NO FORMAT VERSION",                  // VERSION
		"A NEWER FORMAT THAN THIS FIRMWARE",  // NEWER
		"THE FILE ENDS TOO EARLY",            // TRUNCATED
		"MIXED LINE ENDS",                    // LINE_END
		"A LINE IS NOT IN THE FORMAT",        // MALFORMED
		"NOT 32 REGISTERS",                   // REG_COUNT
		"THE CHECKSUM DOES NOT MATCH",        // CHECKSUM
		"TEXT AFTER THE CHECKSUM",            // TRAILING
		"NO FIELD OF THAT NAME",              // NAME
		"A FIELD GIVEN TWICE",                // DUPLICATE
		"NOT A VALUE",                        // VALUE
		"THE TYPE CONTRADICTS THE REGISTERS", // TYPE
	};
	static_assert(sizeof(TEXT) / sizeof(TEXT[0]) == KST_FILE_ERR_TYPE + 1, "one text per kst_file_err_t");
	return (unsigned)err <= KST_FILE_ERR_TYPE ? TEXT[err] : "UNKNOWN ERROR";
}

void kstFileLineName(const char *text, size_t len, unsigned line, char *buf, size_t n) {
	if (n == 0) return;
	buf[0] = '\0';
	if (!text || line == 0) return;
	size_t i = 0;
	for (unsigned l = 1; l < line && i < len; i++)
		if (text[i] == '\n') l++;
	size_t end = i;
	while (end < len && text[end] != '\n' && text[end] != '=') end++;
	if (end >= len || text[end] != '=') return;
	while (end > i && text[end - 1] == ' ') end--;
	size_t k = 0;
	for (; i < end && k + 1 < n; i++) {
		char c = text[i] == '_' ? ' ' : text[i];
		buf[k++] = (char)toupper((unsigned char)c);
	}
	buf[k] = '\0';
}

int kstWrap(const char *text, int width, char lines[][48], int maxLines) {
	if (width > 47) width = 47;
	int count = 0;
	const char *p = text ? text : "";
	while (*p && count < maxLines) {
		while (*p == ' ') p++;
		if (!*p) break;
		int take = (int)strlen(p);
		if (take > width) {
			take = width;
			while (take > 0 && p[take] != ' ') take--;
			if (take == 0) take = width;
		}
		bool last = count == maxLines - 1;
		bool more = p[take] != '\0' && strspn(p + take, " ") != strlen(p + take);
		if (last && more) {
			int keep = width - 3 < take ? width - 3 : take;
			snprintf(lines[count], 48, "%.*s...", keep, p);
			return count + 1;
		}
		snprintf(lines[count], 48, "%.*s", take, p);
		count++;
		p += take;
	}
	return count;
}
