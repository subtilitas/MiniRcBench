/**
 * @file hitec_view.cpp
 * @brief What the HITEC SERVO screen shows of a servo. @see hitec_view.h
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 */

#include "hitec_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
#include <string.h>

// ---------------------------------------------------------------------------
// rows
// ---------------------------------------------------------------------------

/** @brief Every row of both series, in screen order; a layout shows those it has. */
static const HitecRow ALL_ROWS[] = {
	{ "MOTION",  "SPEED",          HITEC_P_D_SPEED },
	{ "MOTION",  "DEAD BAND",      HITEC_P_D_DEAD_BAND },
	{ "MOTION",  "SOFT START",     HITEC_P_D_SOFT_START },
	{ "MOTION",  "SPEED",          HITEC_P_C_SPEED },
	{ "MOTION",  "DEAD BAND",      HITEC_P_C_DEAD_BAND },
	{ "MOTION",  "RESOLUTION",     HITEC_P_C_RESOLUTION },
	{ "TRAVEL",  "NEUTRAL",        HITEC_P_D_NEUTRAL },
	{ "TRAVEL",  "CENTRE",         HITEC_P_C_CENTRE },
	{ "TRAVEL",  "LEFT END",       HITEC_P_D_LEFT_END },
	{ "TRAVEL",  "RIGHT END",      HITEC_P_D_RIGHT_END },
	{ "TRAVEL",  "LEFT END",       HITEC_P_C_LEFT_END },
	{ "TRAVEL",  "RIGHT END",      HITEC_P_C_RIGHT_END },
	{ "TRAVEL",  "DIRECTION",      HITEC_P_D_DIRECTION },
	{ "TRAVEL",  "DIRECTION",      HITEC_P_C_DIRECTION },
	{ "PROTECT", "FAILSAFE",       HITEC_P_D_FAILSAFE },
	{ "PROTECT", "FAILSAFE",       HITEC_P_C_FAILSAFE },
	{ "PROTECT", "FAILSAFE POS.",  HITEC_P_C_FS_POSITION },
	{ "PROTECT", "OVERLOAD",       HITEC_P_D_OVERLOAD },
	{ "PROTECT", "OVERLOAD",       HITEC_P_C_OVERLOAD },
	{ "PROTECT", "SMART SENSOR",   HITEC_P_D_SMART_SENSOR },
	{ "PROTECT", "SENSITIVITY",    HITEC_P_D_SENSITIVITY },
	{ "INFO",    "MODEL",          HITEC_P_D_MODEL },
	{ "INFO",    "ID",             HITEC_P_D_ID },
	{ "INFO",    "POSITION",       HITEC_P_D_POSITION },
	{ "INFO",    "R04",            HITEC_P_D_R04 },
	{ "INFO",    "R22",            HITEC_P_D_R22 },
	{ "INFO",    "R8A",            HITEC_P_D_R8A },
	{ "INFO",    "R8C",            HITEC_P_D_R8C },
	{ "INFO",    "RD4",            HITEC_P_D_RD4 },
	{ "INFO",    "RD6",            HITEC_P_D_RD6 },
	{ "INFO",    "VERSION",        HITEC_P_C_VERSION },
	{ "INFO",    "DATE",           HITEC_P_C_DATE },
	{ "INFO",    "SERIAL",         HITEC_P_C_SERIAL },
	{ "INFO",    "CHECKSUM",       HITEC_P_C_CHECKSUM },
};

int hitecRows(uint8_t layout, HitecRow *out) {
	int n = 0;
	for (const HitecRow &r : ALL_ROWS)
		if (hitec_param(layout, r.param) && n < HITEC_ROWS_MAX) out[n++] = r;
	return n;
}

bool hitecEditable(uint8_t layout, uint8_t param) {
	const hitec_param_info_t *p = hitec_param(layout, param);
	return p && p->access == HITEC_RW;
}

bool hitecWritten(uint8_t layout, uint8_t param) {
	return hitecEditable(layout, param) || (param == HITEC_P_C_CHECKSUM && hitec_param(layout, param));
}

// ---------------------------------------------------------------------------
// values
// ---------------------------------------------------------------------------

/** @brief The word of a CHOICE value of @p param. */
static const char *choiceWord(uint8_t param, int32_t v) {
	switch (param) {
		case HITEC_P_D_DIRECTION:
		case HITEC_P_C_DIRECTION:  return v == HITEC_CCW ? "CCW" : "CW";
		case HITEC_P_C_RESOLUTION: return v == HITEC_RES_HIGH ? "HIGH" : "NORMAL";
		case HITEC_P_C_CHECKSUM:   return v == HITEC_SUM_OK ? "OK" : "BAD";
		default:                   return v == HITEC_ON ? "ON" : "OFF";
	}
}

/** @brief A decoded value of @p param as text, without the stored value. */
static void plainText(const hitec_param_info_t *p, uint8_t param, int32_t v, char *buf, size_t n) {
	switch (p->unit) {
		case HITEC_UNIT_PERCENT:  snprintf(buf, n, "%ld %%", (long)v); break;
		case HITEC_UNIT_STEP:     snprintf(buf, n, "STEP %ld", (long)v); break;
		case HITEC_UNIT_CHOICE:   snprintf(buf, n, "%s", choiceWord(param, v)); break;
		case HITEC_UNIT_FS_US:
			if (v == HITEC_FS_LIMP)     snprintf(buf, n, "LIMP");
			else if (v == HITEC_FS_OFF) snprintf(buf, n, "OFF");
			else                        snprintf(buf, n, "%ld US", (long)v);
			break;
		case HITEC_UNIT_OVERLOAD:
			if (v == HITEC_OVERLOAD_OFF) snprintf(buf, n, "OFF");
			else                         snprintf(buf, n, "%ld %%", (long)v);
			break;
		case HITEC_UNIT_QUARTER_US:
			if (v % 4) snprintf(buf, n, "%ld.%02ld US", (long)(v / 4), (long)(v % 4) * 25);
			else       snprintf(buf, n, "%ld US", (long)(v / 4));
			break;
		case HITEC_UNIT_DATE:
			snprintf(buf, n, "%04ld-%02ld-%02ld", (long)(v / 10000), (long)(v / 100 % 100), (long)(v % 100));
			break;
		default:                  snprintf(buf, n, "%ld", (long)v); break;
	}
}

/** @brief Whether a value of @p p shows its stored value beside it. */
static bool showsRaw(const hitec_param_info_t *p, uint8_t param) {
	switch (p->unit) {
		case HITEC_UNIT_RAW:
		case HITEC_UNIT_COUNTS:
		case HITEC_UNIT_DATE:
		case HITEC_UNIT_NUMBER: return false;
		default:                return param != HITEC_P_C_CHECKSUM;
	}
}

void hitecValueText(const hitec_image_t *img, uint8_t layout, uint8_t param, char *buf,
                    size_t n, bool *raw) {
	if (raw) *raw = false;
	const hitec_param_info_t *p = hitec_param(layout, param);
	hitec_value_t v;
	hitec_get_t g = p ? hitec_param_get(img, layout, param, &v) : HITEC_GET_ERR_ARG;
	if (g == HITEC_GET_RAW) {
		snprintf(buf, n, "RAW %lu", (unsigned long)v.raw);
		if (raw) *raw = true;
		return;
	}
	if (g != HITEC_GET_OK) {
		snprintf(buf, n, "?");
		return;
	}
	char plain[32];
	plainText(p, param, v.value, plain, sizeof(plain));
	if (showsRaw(p, param)) snprintf(buf, n, "%s (%lu)", plain, (unsigned long)v.raw);
	else                    snprintf(buf, n, "%s", plain);
}

// ---------------------------------------------------------------------------
// the stepper
// ---------------------------------------------------------------------------

/** @brief The value an edit of @p p starts from: the decoded one, or a raw one clamped. */
static bool startValue(const hitec_image_t *img, uint8_t layout, uint8_t param, int32_t *out) {
	hitec_value_t v;
	hitec_get_t g = hitec_param_get(img, layout, param, &v);
	if (g == HITEC_GET_OK) { *out = v.value; return true; }
	if (g == HITEC_GET_RAW) { *out = hitec_param_clamp(layout, param, v.value); return true; }
	return false;
}

/** @brief The value after @p v in @p dir, in the encoding's order; false at its end. */
static bool nextValue(const hitec_param_info_t *p, int32_t v, int dir, int32_t *out) {
	int32_t step = p->step ? p->step : 1;
	if (p->unit == HITEC_UNIT_FS_US) {
		// LIMP, OFF, then the positions.
		if (dir > 0) {
			if (v == HITEC_FS_LIMP) { *out = HITEC_FS_OFF; return true; }
			if (v == HITEC_FS_OFF)  { *out = p->min; return true; }
		} else {
			if (v == HITEC_FS_LIMP) return false;
			if (v == HITEC_FS_OFF)  { *out = HITEC_FS_LIMP; return true; }
			if (v <= p->min)        { *out = HITEC_FS_OFF; return true; }
		}
	}
	int32_t w = v + dir * step;
	if (w < p->min || w > p->max) return false;
	*out = w;
	return true;
}

/** @brief One step of @p param in @p dir on @p draft. @return false when none is taken. */
static bool stepOnce(hitec_image_t *draft, uint8_t layout, uint8_t param, int dir) {
	const hitec_param_info_t *p = hitec_param(layout, param);
	if (!p || p->access != HITEC_RW) return false;
	hitec_value_t before;
	hitec_get_t g = hitec_param_get(draft, layout, param, &before);
	int32_t v;
	if (!startValue(draft, layout, param, &v)) return false;
	// A raw value: the first step puts the nearest table value in its place.
	if (g == HITEC_GET_RAW) {
		hitec_image_t t = *draft;
		if (hitec_param_set(&t, layout, param, v) == HITEC_SET_OK) {
			*draft = t;
			return true;
		}
	}
	int32_t span = p->step ? (p->max - p->min) / p->step + 3 : 3;
	for (int32_t i = 0; i < span; i++) {
		if (!nextValue(p, v, dir, &v)) return false;
		hitec_image_t t = *draft;
		if (hitec_param_set(&t, layout, param, v) == HITEC_SET_OK) {
			*draft = t;
			return true;
		}
	}
	return false;
}

int hitecStep(hitec_image_t *draft, uint8_t layout, uint8_t param, int steps) {
	int dir = steps < 0 ? -1 : 1, taken = 0;
	for (int i = 0; i < abs(steps); i++, taken++)
		if (!stepOnce(draft, layout, param, dir)) break;
	return taken;
}

bool hitecCanStep(const hitec_image_t *draft, uint8_t layout, uint8_t param, int dir) {
	hitec_image_t t = *draft;
	return stepOnce(&t, layout, param, dir < 0 ? -1 : 1);
}

int hitecCoarse(uint8_t layout, uint8_t param) {
	const hitec_param_info_t *p = hitec_param(layout, param);
	if (!p || !p->step) return 1;
	int32_t steps = (p->max - p->min) / p->step;
	int c = (int)((steps + 10) / 11);
	return c < 1 ? 1 : c;
}

bool hitecDiffers(const hitec_image_t *a, const hitec_image_t *b, uint8_t layout, uint8_t param) {
	hitec_value_t va, vb;
	hitec_get_t ga = hitec_param_get(a, layout, param, &va);
	hitec_get_t gb = hitec_param_get(b, layout, param, &vb);
	return ga != gb || va.raw != vb.raw || va.value != vb.value;
}

int hitecChangedCount(const hitec_image_t *a, const hitec_image_t *b, uint8_t layout) {
	HitecRow rows[HITEC_ROWS_MAX];
	int n = hitecRows(layout, rows), c = 0;
	for (int i = 0; i < n; i++)
		if (hitecWritten(layout, rows[i].param) && hitecDiffers(a, b, layout, rows[i].param)) c++;
	return c;
}

// ---------------------------------------------------------------------------
// soft rules
// ---------------------------------------------------------------------------

/** @brief The decoded value of @p param; false when it is not one. */
static bool valueOf(const hitec_image_t *img, uint8_t layout, uint8_t param, int32_t *v) {
	hitec_value_t x;
	if (!hitec_param(layout, param) || hitec_param_get(img, layout, param, &x) != HITEC_GET_OK)
		return false;
	*v = x.value;
	return true;
}

uint32_t hitecSoftRules(const hitec_image_t *servo, const hitec_image_t *draft, uint8_t layout,
                        uint8_t param) {
	uint32_t r = 0;
	auto in = [&](uint8_t p) { return param == HITEC_P_COUNT || param == p; };
	auto pair = [&](uint8_t p, int32_t *a, int32_t *b) {
		return in(p) && valueOf(servo, layout, p, a) && valueOf(draft, layout, p, b) && *a != *b;
	};
	int32_t a, b;
	for (uint8_t p : { (uint8_t)HITEC_P_D_SPEED, (uint8_t)HITEC_P_C_SPEED })
		if (pair(p, &a, &b) && b < 30) r |= HITEC_RULE_BIT(HitecRule::SlowSpeed);
	for (uint8_t p : { (uint8_t)HITEC_P_D_DEAD_BAND, (uint8_t)HITEC_P_C_DEAD_BAND })
		if (pair(p, &a, &b) && b < a) r |= HITEC_RULE_BIT(HitecRule::NarrowDeadBand);
	for (uint8_t p : { (uint8_t)HITEC_P_D_OVERLOAD, (uint8_t)HITEC_P_C_OVERLOAD })
		if (pair(p, &a, &b) && b == HITEC_OVERLOAD_OFF) r |= HITEC_RULE_BIT(HitecRule::OverloadOff);
	for (uint8_t p : { (uint8_t)HITEC_P_D_LEFT_END, (uint8_t)HITEC_P_D_RIGHT_END,
	                   (uint8_t)HITEC_P_C_LEFT_END, (uint8_t)HITEC_P_C_RIGHT_END })
		if (pair(p, &a, &b)) r |= HITEC_RULE_BIT(HitecRule::EndPoint);
	for (uint8_t p : { (uint8_t)HITEC_P_D_NEUTRAL, (uint8_t)HITEC_P_C_CENTRE })
		if (pair(p, &a, &b)) r |= HITEC_RULE_BIT(HitecRule::Centre);
	for (uint8_t p : { (uint8_t)HITEC_P_D_DIRECTION, (uint8_t)HITEC_P_C_DIRECTION })
		if (pair(p, &a, &b)) r |= HITEC_RULE_BIT(HitecRule::Direction);
	if (pair(HITEC_P_D_FAILSAFE, &a, &b) && b > HITEC_FS_OFF && abs(b - 1500) > 300)
		r |= HITEC_RULE_BIT(HitecRule::FailsafeFar);
	if (pair(HITEC_P_C_FS_POSITION, &a, &b) && abs(b / 4 - 1500) > 300)
		r |= HITEC_RULE_BIT(HitecRule::FailsafeFar);
	return r;
}

const char *hitecRuleText(HitecRule r) {
	switch (r) {
		case HitecRule::SlowSpeed:      return "A SLOW SERVO CAN CAUSE LOSS OF CONTROL OF THE MODEL.";
		case HitecRule::NarrowDeadBand: return "A NARROW DEAD BAND CAN CAUSE BACK-AND-FORTH MOVEMENT AND HEAT.";
		case HitecRule::OverloadOff:    return "WITHOUT OVERLOAD PROTECTION A STALLED SERVO HEATS UP.";
		case HitecRule::EndPoint:       return "END POINT CHANGED. THE SERVO TRAVEL PER STICK MOVEMENT CHANGES TOO.";
		case HitecRule::Centre:         return "THE CENTRE SHIFTS. ONE SIDE MOVES CLOSER TO THE MECHANICAL STOP.";
		case HitecRule::Direction:      return "DIRECTION REVERSED. CHECK ALL CONTROL SURFACES BEFORE FLIGHT.";
		case HitecRule::FailsafeFar:    return "FAILSAFE POSITION IS FAR FROM THE CENTRE.";
		default:                        return "";
	}
}

// ---------------------------------------------------------------------------
// results, reply codes and models
// ---------------------------------------------------------------------------

HitecResultText hitecResultText(uint8_t result, int values, uint8_t stepIndex, uint8_t planSteps,
                                const uint8_t *diff, uint8_t failAddr, uint8_t planResult,
                                uint8_t series) {
	HitecResultText t;
	t.tone = HitecTone::Doubt;
	const char *read = "READ THE SERVO BEFORE A WRITE.";
	if (result == HITEC_SES_OK) {
		snprintf(t.text, sizeof(t.text), "%d VALUE%s WRITTEN AND VERIFIED.", values, values == 1 ? "" : "S");
		t.tone = HitecTone::Done;
		return t;
	}
	if (result == HITEC_SES_ERR_VERIFY) {
		char addrs[40] = "";
		int shown = 0;
		for (int a = 0; a < 256 && diff; a++) {
			if (!(diff[a / 8] & (1u << (a % 8)))) continue;
			if (shown == 4) { strcat(addrs, " ..."); break; }
			char one[8];
			snprintf(one, sizeof(one), " 0x%02X", a);
			strcat(addrs, one);
			shown++;
		}
		snprintf(t.text, sizeof(t.text), "THE SERVO DOES NOT HOLD THE VALUES:%s. READ AND CHECK.", addrs);
		return t;
	}
	bool sent = planSteps > 0 && stepIndex >= planSteps;
	switch (result) {
		case HITEC_SES_ERR_WRITE:
			if (sent) break;
			snprintf(t.text, sizeof(t.text), "%s 0x%02X GOT NO ANSWER. %s",
			         series == HITEC_CLASSIC ? "CELL" : "REGISTER", failAddr, read);
			return t;
		case HITEC_SES_ERR_NO_SERVO:
		case HITEC_SES_ERR_CUT:
		case HITEC_SES_ERR_LINK:
		case HITEC_SES_ERR_STEP:
			if (sent) break;
			snprintf(t.text, sizeof(t.text), "THE WRITE STOPPED AT STEP %u OF %u. %s",
			         (unsigned)stepIndex + 1u, (unsigned)planSteps, read);
			return t;
		case HITEC_SES_ERR_READ:
			break;
		case HITEC_SES_ERR_PLAN:
			t.tone = HitecTone::Nothing;
			if (planResult == HITEC_PLAN_ERR_UNKNOWN)
				snprintf(t.text, sizeof(t.text), "NOTHING WAS WRITTEN: READ THE SERVO FIRST.");
			else
				snprintf(t.text, sizeof(t.text), "NOTHING WAS WRITTEN: NO PLAN (CODE %u).", planResult);
			return t;
		default:
			t.tone = HitecTone::Nothing;
			snprintf(t.text, sizeof(t.text), "NOTHING WAS WRITTEN (CODE %u).", result);
			return t;
	}
	snprintf(t.text, sizeof(t.text), "ALL WRITES SENT; READ-BACK FAILED. %s", read);
	return t;
}

const char *hitecRxWord(uint8_t rx) {
	static const char *const WORDS[] = {
		"", "ARG", "CUT", "OVERFLOW", "SILENT", "EARLY", "LATE", "NAK", "LENGTH", "FRAMING",
		"GAP", "NO ACK", "HEADER", "ADDRESS", "SUM",
	};
	return rx < sizeof(WORDS) / sizeof(WORDS[0]) ? WORDS[rx] : "?";
}

void hitecModelText(const hitec_image_t *img, char *buf, size_t n) {
	hitec_model_t m;
	if (!hitec_identify(img, &m)) {
		snprintf(buf, n, "?");
		return;
	}
	const char *name = hitec_model_name(m.model);
	if (name[0])                 snprintf(buf, n, "%s", name);
	else if (img->series == HITEC_D) snprintf(buf, n, "D SERIES %u", (unsigned)m.number);
	else if (m.name[0])          snprintf(buf, n, "%s", m.name);
	else                         snprintf(buf, n, "?");
}

uint8_t hitecLayoutOf(const hitec_image_t *img) {
	hitec_model_t m;
	if (hitec_identify(img, &m)) return m.layout;
	return img->series == HITEC_D ? (uint8_t)HITEC_LAYOUT_D : (uint8_t)HITEC_LAYOUT_HS5;
}

bool hitecModelMeasured(const hitec_image_t *img) {
	hitec_model_t m;
	if (!hitec_identify(img, &m)) return false;
	return m.model == HITEC_MODEL_D485HW || m.model == HITEC_MODEL_HS5645MG ||
	       m.model == HITEC_MODEL_HS7955TG;
}
