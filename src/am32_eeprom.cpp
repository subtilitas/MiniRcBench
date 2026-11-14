/**
 * @file am32_eeprom.cpp
 * @brief AM32 settings field table and value formatting.
 *
 * Scaling factors follow the AM32 configurator's conversions. Where a factor
 * was inferred rather than taken from the layout definition it is called out in
 * a comment, because a wrong divisor here shows a plausible-looking but false
 * number rather than failing loudly.
 */

#include "am32_eeprom.h"

#include <stdio.h>
#include <string.h>

static const char *const NAMES_DIRECTION[] = {"NORMAL", "REVERSED"};
// Zero-based, as AM32's `enum inputType` (Inc/common.h, the same order since
// v2.05) and the configurator's protocol list number them: 0 Auto, 1 DShot,
// 2 Servo, 3 Serial, 4 EDT arm; 5 is DroneCAN on DroneCAN builds.
// @see Am32Input
static const char *const NAMES_PROTOCOL[]  = {
	"AUTO", "DSHOT", "SERVO", "SERIAL", "EDT ARM", "DRONECAN"
};
static const char *const NAMES_OFFON[]     = {"OFF", "ON"};
// The configurator's names, shortened to the editor's 112 px at scale 2:
// "Cell based" is CELL, "Brake on stop" BRAKE, "Active brake" ACTIVE.
static const char *const NAMES_LOW_VOLT[]  = {"OFF", "CELL", "ABSOLUTE"};
static const char *const NAMES_PWM_TYPE[]  = {"FIXED", "VARIABLE", "BY RPM"};
static const char *const NAMES_BRAKE[]     = {"OFF", "BRAKE", "ACTIVE"};

/**
 * @brief The firmware from which AM32 reads the byte at @p off: 0x2A to 0x2D
 *        from @ref AM32_FW_READS_2A, 0x2E from @ref AM32_FW_READS_2E, auto
 *        advance (0x2F) from @ref AM32_FW_READS_2F; the rest from every
 *        firmware that writes layout 2, as AlkaMotors 1.78 does.
 */
static constexpr uint16_t am32ReadFw(uint8_t off) {
	return off == 0x2F ? AM32_FW_READS_2F
	     : off == 0x2E ? AM32_FW_READS_2E
	     : (off >= 0x2A && off <= 0x2D) ? AM32_FW_READS_2A : 0;
}

/**
 * @defgroup am32_tablemacros Field table shorthand
 * @brief Keeps the settings table readable; unused columns stay zero.
 *
 * Written out longhand, each row would be fifteen columns of mostly zeroes and
 * the table would stop being scannable — which is the entire point of holding
 * the settings in a table rather than in code.
 *
 * Common arguments: @c grp section heading, @c nm display label, @c off byte
 * offset, @c ver minimum EEPROM layout revision (0 for any).
 * @{
 */

/** @brief Off/on field. */
#define F_BOOL(grp, nm, off, ver)                                              \
	{grp, nm, off, A32_BOOL, 0, 1, 1, ver, 0, 0, 0, 0, "", NAMES_OFFON, 2, 0, 0, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief Plain integer over @c lo..@c hi in steps of @c st, suffixed @c unit. */
#define F_RAW(grp, nm, off, lo, hi, st, unit, ver)                             \
	{grp, nm, off, A32_RAW, lo, hi, st, ver, 0, 0, 0, 0, unit, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief Displayed as `(raw * m + a) / d`, with one decimal when @c d > 1. */
#define F_SCALED(grp, nm, off, lo, hi, st, m, d, a, unit, ver)                 \
	{grp, nm, off, A32_SCALED, lo, hi, st, ver, 0, m, d, a, unit, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief Index into the @c cnt labels of @c tbl. */
#define F_ENUM(grp, nm, off, tbl, cnt, ver)                                    \
	{grp, nm, off, A32_ENUM, 0, (uint8_t)((cnt) - 1), 1, ver, 0, 0, 0, 0, "", tbl, cnt, 0, 0, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/**
 * @brief Index into the @c cnt labels of @c tbl, whose last label exists from
 *        AM32 firmware @c fw (major x 100 + minor) on. @see am32FieldMax()
 */
#define F_CHOICE(grp, nm, off, tbl, cnt, fw)                                   \
	{grp, nm, off, A32_ENUM, 0, (uint8_t)((cnt) - 1), 1, 0, 0, 0, 0, 0, "", tbl, cnt, 0, 0, fw, (uint8_t)((cnt) - 2), 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief As F_RAW(), with values outside @c onLo..@c onHi shown as `OFF`. */
#define F_RAW_LIMIT(grp, nm, off, lo, hi, st, unit, onLo, onHi)                \
	{grp, nm, off, A32_RAW, lo, hi, st, 0, 0, 0, 0, 0, unit, nullptr, 0, onLo, onHi, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief As F_SCALED(), with raw values outside @c onLo..@c onHi shown as `OFF`. */
#define F_SCALED_LIMIT(grp, nm, off, lo, hi, st, m, d, a, unit, onLo, onHi)    \
	{grp, nm, off, A32_SCALED, lo, hi, st, 0, 0, m, d, a, unit, nullptr, 0, onLo, onHi, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief As F_RAW(), with @c hi accepted from AM32 firmware @c fw on and @c old below it. */
#define F_RAW_FW(grp, nm, off, lo, hi, st, unit, fw, old)                     \
	{grp, nm, off, A32_RAW, lo, hi, st, 0, 0, 0, 0, 0, unit, nullptr, 0, 0, 0, fw, old, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @brief As F_ENUM(), with @c cnt labels of which the editor offers 0..@c hi. */
#define F_ENUM_TO(grp, nm, off, tbl, cnt, hi)                                  \
	{grp, nm, off, A32_ENUM, 0, hi, 1, 0, 0, 0, 0, 0, "", tbl, cnt, 0, 0, 0, 0, 0, 0, 0, am32ReadFw(off), 0, 0}
/** @} */

const Am32Field AM32_FIELDS[] = {
	// ---- Motor -----------------------------------------------------------
	F_ENUM  ("MOTOR", "DIRECTION",        0x11, NAMES_DIRECTION, 2, 0),
	// AM32's bi_direction: 3D (reversible) mode, not bidirectional DShot. On
	// DShot it splits 48..2047: 48..1047 runs one way, 1048..2047 the other
	// (setInput() in AM32's Src/main.c).
	F_BOOL  ("MOTOR", "3D MODE",          AM32_OFF_BI_DIRECTION, 0),
	F_RAW   ("MOTOR", "POLES",            0x1B, 2, 36, 2, "", 0),
	// AM32 reads it as raw x 40 + 20 (`motor_kv` in loadEEpromSettings()), and
	// the configurator offers 20..10220 KV: raw 0..255.
	F_SCALED("MOTOR", "KV",               0x1A, 0, 255, 1, 40, 1, 20, "KV", 0),
	F_BOOL  ("MOTOR", "HALL SENSORS",     0x27, 0),

	// ---- Timing and PWM --------------------------------------------------
	// Two encodings, decided by the byte's own value, from 2.19 on; before
	// it the old one alone. The table cannot express that with a single
	// mul/div, so the type carries the rule. @see am32AdvanceDeciDeg()
	{"TIMING", "ADVANCE", AM32_OFF_ADVANCE, A32_ADVANCE, 10, 42, 1, 0, 0,
	 0, 0, 0, "DEG", nullptr, 0, 0, 0, AM32_ADVANCE_NEW_FW, 3, 0, 0, 0, am32ReadFw(AM32_OFF_ADVANCE), 0, 0},
	F_BOOL  ("TIMING", "AUTO ADVANCE",    0x2F, 0),
	// AM32 accepts 8..144 kHz from 2.19 on (`pwm_frequency < 145`), 8..48
	// before (`< 49`, v2.05 to v2.18).
	F_RAW_FW("TIMING", "PWM FREQ",        0x18, 8, 144, 1, "KHZ", 219, 48),
	// variable_pwm: 0 fixed, 1 variable, 2 by RPM. AM32 acts on 2 from 2.18
	// on; 2.16 and older act on 1 alone.
	F_CHOICE("TIMING", "PWM TYPE",        0x15, NAMES_PWM_TYPE, 3, 218),
	F_BOOL  ("TIMING", "COMPLEMENTARY",   0x14, 0),

	// ---- Startup ---------------------------------------------------------
	F_RAW   ("STARTUP", "POWER",          0x19, 50, 150, 1, "", 0),
	F_BOOL  ("STARTUP", "SINE STARTUP",   0x13, 0),
	F_RAW   ("STARTUP", "SINE RANGE",     0x28, 5, 25, 1, "", 0),
	F_RAW   ("STARTUP", "SINE POWER",     0x2D, 1, 10, 1, "", 0),
	F_BOOL  ("STARTUP", "STUCK ROTOR",    0x16, 0),
	F_BOOL  ("STARTUP", "STALL PROTECT",  0x1D, 0),

	// ---- Braking ---------------------------------------------------------
	// brake_on_stop: 0 off, 1 brake on stop, 2 active brake, from 2.19 on.
	// 2.18 brakes on any value but 0; 2.16 and older on 1 alone.
	F_CHOICE("BRAKE", "BRAKE ON STOP",    0x1C, NAMES_BRAKE, 3, 219),
	F_RAW   ("BRAKE", "STOPPED LEVEL",    0x29, 1, 10, 1, "", 0),
	F_RAW   ("BRAKE", "RUNNING LEVEL",    0x2A, 1, 10, 1, "", 0),
	F_BOOL  ("BRAKE", "RC CAR REVERSE",   0x26, 0),

	// ---- Limits ----------------------------------------------------------
	// low_voltage_cut_off: 0 off, 1 per cell (CELL CUTOFF), 2 absolute (ABS
	// VOLT CUT). 2.18 and older cut off on 1 alone. 2.19 and 2.20 have the
	// absolute mode but compare the battery voltage in 10 mV with the raw
	// byte, so it never cuts off; from 2.21 the byte counts 0.5 V. The
	// editor offers ABSOLUTE from 2.21 on.
	F_CHOICE("LIMITS", "LOW VOLT CUTOFF", AM32_OFF_LOW_VOLT_CUTOFF, NAMES_LOW_VOLT, 3, 221),
	// Cell cutoff is stored as hundredths of a volt above 2.50 V.
	F_SCALED("LIMITS", "CELL CUTOFF",     0x25, 0, 100, 1, 1, 100, 250, "V", 0),
	// AM32 takes 70..140 as a limit in degrees C and anything else as none:
	// loadEEpromSettings() in AM32's Src/main.c sets it to 255. The
	// configurator's slider ends at 141, which it shows as disabled.
	F_RAW_LIMIT   ("LIMITS", "TEMP LIMIT",    0x2B, 70, 141, 1, "C", 70, 140),
	// Amps / 2. AM32 limits the current for 1..100 from 2.21 on (`<= 100`),
	// for 1..99 from 1.81 to 2.20 (`> 0 && < 100`, AlkaMotors f917c01, which
	// raises the version to 1.81), for 2..99 at 1.80 (`> 1`, 9909ea7); 0 and
	// anything above mean none. The configurator's slider ends at 202 A, raw
	// 101, which it shows as disabled; the Crawler preset and the reference
	// tool's defaults in test/test_am32.cpp hold 102, so the editor goes to
	// 102.
	{"LIMITS", "CURRENT LIMIT", 0x2C, A32_SCALED, 0, 102, 1, 0, 0, 2, 1, 0, "A", nullptr, 0,
	 1, 100, 0, 0, 221, 99, 0, am32ReadFw(0x2C), 181, 2},

	// ---- Input -----------------------------------------------------------
	// The editor offers 0..4, as the configurator does; a stored DRONECAN, 5,
	// reads by name.
	F_ENUM_TO("INPUT", "PROTOCOL",        AM32_OFF_INPUT_TYPE, NAMES_PROTOCOL, 6, AM32_INPUT_EDT_ARM),
	F_BOOL  ("INPUT", "30MS TELEMETRY",   0x1F, 0),
	F_RAW   ("INPUT", "BEEP VOLUME",      0x1E, 0, 11, 1, "", 0),

	// ---- Servo -----------------------------------------------------------
	// Thresholds are (microseconds - base) / 2; neutral is a plain offset from
	// 1374 us, which is what makes the stock value of 128 read as 1502 us.
	// AM32 takes any byte for the three (v2.16 main.c:728-731, v2.22
	// main.c:692-694), so the editor offers 0..255; the configurator's
	// thresholds stop at 250.
	F_SCALED("SERVO", "LOW THRESHOLD",    0x20, 0, 255, 1, 2, 1, 750,  "US", 0),
	F_SCALED("SERVO", "HIGH THRESHOLD",   0x21, 0, 255, 1, 2, 1, 1750, "US", 0),
	F_SCALED("SERVO", "NEUTRAL",          0x22, 0, 255, 1, 1, 1, 1374, "US", 0),
	// In AM32's input units, not microseconds: the configurator gives no unit.
	F_RAW   ("SERVO", "DEAD BAND",        0x23, 0, 100, 1, "", 0),

	// ---- Layout revision 3 and later only --------------------------------
	// Ramp in 0.1 % duty per ms and minimum duty in 0.5 %, as the
	// configurator shows them over its ranges, 0.1..20 and 0..25.
	F_SCALED("ADVANCED", "MAX RAMP",      0x05, 1, 200, 1, 1, 10, 0, "%/MS", 3),
	F_SCALED("ADVANCED", "MIN DUTY",      0x06, 0, 50, 1, 1, 2, 0, "%", 3),
	F_BOOL  ("ADVANCED", "NO STICK CAL",  0x07, 3),
	// The absolute cutoff in 0.5 V steps from 2.21 on
	// (`absolute_voltage_cutoff * 50` against the battery voltage in 10 mV),
	// 0.5..50 V. 2.19 and 2.20 compare the raw byte with the 10 mV figure,
	// so below 2.21 it reads as the plain byte.
	{"ADVANCED", "ABS VOLT CUT", 0x08, A32_SCALED, 1, 100, 1, 3, 0, 1, 2, 0, "V", nullptr, 0,
	 0, 0, 0, 0, 0, 0, 221, am32ReadFw(0x08), 0, 0},
	F_RAW   ("ADVANCED", "CURRENT P",     0x09, 0, 255, 1, "", 3),
	F_RAW   ("ADVANCED", "CURRENT I",     0x0A, 0, 255, 1, "", 3),
	F_RAW   ("ADVANCED", "CURRENT D",     0x0B, 0, 255, 1, "", 3),
	// Active brake duty, used when BRAKE ON STOP is ACTIVE: one unit is
	// 0.5 % of the PWM period (`active_brake_power * tim1_arr / 2000 * 10`,
	// v2.19 to v2.22), so the configurator's 0..5 is 0..2.5 %.
	F_SCALED("ADVANCED", "ACTIVE BRAKE",  0x0C, 0, 5, 1, 1, 2, 0, "%", 3),
};

const uint16_t AM32_FIELD_COUNT = (uint16_t)(sizeof(AM32_FIELDS) / sizeof(AM32_FIELDS[0]));

bool am32FieldApplies(const Am32Field *f, uint8_t layoutRev) {
	if (f->minVer && layoutRev < f->minVer) return false;
	if (f->maxVer && layoutRev > f->maxVer) return false;
	return true;
}

void am32FormatValue(const Am32Field *f, const uint8_t *eeprom, char *out, int outLen) {
	uint8_t raw = eeprom[f->offset];

	if (am32ValueOff(f, eeprom)) {
		snprintf(out, outLen, "OFF");
		return;
	}
	// A number outside what the editor offers on this firmware reads as the
	// byte, not as a reading. For most fields AM32 puts its own default in
	// its place; CELL CUTOFF, POLES and DEAD BAND it takes as stored.
	if ((f->type == A32_RAW || f->type == A32_SCALED) &&
	    (raw < f->rawMin || raw > am32FieldMax(f, eeprom))) {
		snprintf(out, outLen, "? (%u)", raw);
		return;
	}

	switch (f->type) {
		case A32_BOOL:
			// 2 or more is not a value the configurator writes, and AM32
			// before 2.18 acts on several of these bytes when they are 1
			// alone: the byte, rather than ON.
			if (raw > 1) snprintf(out, outLen, "? (%u)", raw);
			else         snprintf(out, outLen, "%s", raw ? "ON" : "OFF");
			return;

		case A32_ENUM: {
			// A value this firmware does not accept, a gated top value among
			// them, reads as the byte. A name past the editor's top (PROTOCOL's
			// DRONECAN) still reads by name.
			bool locked = raw > am32FieldMax(f, eeprom) && raw <= f->rawMax;
			if (f->names && raw < f->nameCount && !locked)
				snprintf(out, outLen, "%s", f->names[raw]);
			else
				snprintf(out, outLen, "? (%u)", raw);
			return;
		}

		case A32_RAW:
			if (f->unit && f->unit[0]) snprintf(out, outLen, "%u%s", raw, f->unit);
			else                       snprintf(out, outLen, "%u", raw);
			return;

		case A32_ADVANCE: {
			int16_t d10 = am32AdvanceDeciDeg(raw);
			// Before 2.19 AM32 reads 0..3 alone and takes any other byte as 2.
			if (raw > 3 && am32Firmware(eeprom) < AM32_ADVANCE_NEW_FW) d10 = -1;
			if (d10 < 0) {
				// Neither encoding. Say the byte rather than an angle: AM32
				// substitutes its own default for this, so the ESC is not
				// running at whatever we would have printed.
				snprintf(out, outLen, "? (%u)", raw);
				return;
			}
			snprintf(out, outLen, "%d.%d%s", d10 / 10, d10 % 10, f->unit);
			return;
		}

		case A32_SCALED: {
			if (f->scaleFw && am32Firmware(eeprom) < f->scaleFw) {
				// Not a value in the field's unit on this firmware.
				snprintf(out, outLen, "%u", raw);
				return;
			}
			// (raw * mul + add) / div. The offset is applied *before* the
			// divide: the cell cutoff is (raw + 250) / 100 volts, not
			// raw / 100 + 250.
			int32_t num = (int32_t)raw * f->mul + f->add;
			int32_t div = f->div ? f->div : 1;
			if (div == 1) {
				snprintf(out, outLen, "%ld%s", (long)num, f->unit);
			} else {
				// One decimal place, without dragging in floating point.
				int32_t scaled = num * 10 / div;
				int32_t frac = scaled % 10;
				snprintf(out, outLen, "%ld.%ld%s",
				         (long)(scaled / 10), (long)(frac < 0 ? -frac : frac), f->unit);
			}
			return;
		}
	}
	snprintf(out, outLen, "%u", raw);
}

void am32DeviceName(const uint8_t *eeprom, char *out) {
	if (eeprom[AM32_OFF_LAYOUT_REVISION] >= 3) {
		// Those bytes are settings in this revision, not text.
		out[0] = '\0';
		return;
	}
	int n = 0;
	for (int i = 0; i < AM32_NAME_LEN; i++) {
		uint8_t c = eeprom[AM32_OFF_NAME + i];
		if (c < 0x20 || c > 0x7E) break;
		out[n++] = (char)c;
	}
	while (n > 0 && out[n - 1] == ' ') n--;   // trim the padding
	out[n] = '\0';
}

void am32Adjust(const Am32Field *f, uint8_t *eeprom, int dir) {
	int32_t v = (int32_t)eeprom[f->offset];
	int32_t step = f->step ? f->step : 1;

	if (f->type == A32_ADVANCE && am32Firmware(eeprom) < AM32_ADVANCE_NEW_FW) {
		// The old encoding alone, 0..3 in 7.5 degree steps: a byte above it
		// lands on 3, the nearer end, as the generic rule does.
		if (v > 3) v = 3;
		else v += (dir > 0 ? 1 : -1);
		if (v < 0) v = 0;
		if (v > 3) v = 3;
		eeprom[f->offset] = (uint8_t)v;
		return;
	}
	if (f->type == A32_ADVANCE) {
		// Convert to the modern encoding before stepping, rather than clamping
		// into it. Clamping is what the generic path below does, and on this
		// field it threw the setting away: an ESC holding the old value 2
		// (15 degrees) jumped to 10 (0 degrees) on the first press, because 2
		// is below the modern minimum. Converting keeps the angle and changes
		// only how it is written down.
		int32_t stepIdx = (int32_t)am32AdvanceStep((uint8_t)v);
		stepIdx += (dir > 0 ? 1 : -1);
		if (stepIdx < 0)  stepIdx = 0;
		if (stepIdx > 32) stepIdx = 32;
		eeprom[f->offset] = am32AdvanceRaw((uint8_t)stepIdx);
		return;
	}

	// A value already outside the field's range would otherwise need several
	// presses before it re-entered it; snap to the nearest end instead.
	int32_t hi = am32FieldMax(f, eeprom);
	if (v < f->rawMin) v = f->rawMin;
	else if (v > hi) v = hi;
	else v += (dir > 0 ? step : -step);

	if (v < f->rawMin) v = f->rawMin;
	if (v > hi) v = hi;
	eeprom[f->offset] = (uint8_t)v;
}

uint16_t am32Firmware(const uint8_t *eeprom) {
	uint8_t major = eeprom[AM32_OFF_MAIN_REVISION];
	uint8_t minor = eeprom[AM32_OFF_SUB_REVISION];
	if (major < 1 || major > 9 || minor > 99) return 0;
	return (uint16_t)(major * 100u + minor);
}

uint8_t am32FieldMin(const Am32Field *f, const uint8_t *eeprom) {
	// ADVANCE's old encoding starts at 0, below the modern 10.
	if (f->type == A32_ADVANCE && am32Firmware(eeprom) < AM32_ADVANCE_NEW_FW) return 0;
	return f->rawMin;
}

uint8_t am32FieldMax(const Am32Field *f, const uint8_t *eeprom) {
	if (f->topFw && am32Firmware(eeprom) < f->topFw) return f->oldMax;
	return f->rawMax;
}

bool am32ValueOff(const Am32Field *f, const uint8_t *eeprom) {
	if (!f->onMin && !f->onMax) return false;
	uint8_t raw = eeprom[f->offset];
	uint16_t fw = am32Firmware(eeprom);
	uint8_t hi = (f->onFw && fw < f->onFw) ? f->oldOnMax : f->onMax;
	uint8_t lo = (f->onMinFw && fw < f->onMinFw) ? f->oldOnMin : f->onMin;
	return raw < lo || raw > hi;
}

bool am32ProtectionOff(const Am32Field *f, const uint8_t *eeprom) {
	if (f->offset == AM32_OFF_LOW_VOLT_CUTOFF)
		return eeprom[f->offset] == 0 || eeprom[f->offset] > am32FieldMax(f, eeprom);
	return am32ValueOff(f, eeprom);
}

bool am32Is3d(const uint8_t *eeprom) {
	return eeprom[AM32_OFF_BI_DIRECTION] != 0;
}

bool am32InputNotDshot(const uint8_t *eeprom) {
	uint8_t v = eeprom[AM32_OFF_INPUT_TYPE];
	// loadEEpromSettings(): 0 and 1 set EDT_ARMED, 10 and above are AUTO.
	// SERIAL and 5 to 9 match no case that sets it. EDT ARM sets it only on
	// the EDT enable, and dshot.c clears it on every zero frame; the tester
	// sends zero frames after each enable, so the throttle is dropped too.
	return v < 10 && v != AM32_INPUT_AUTO && v != AM32_INPUT_DSHOT;
}

bool am32Plausible(const uint8_t *eeprom) {
	// An erased or absent settings page reads as all 0xFF or all 0x00.
	if (eeprom[AM32_OFF_BOOT_BYTE] == 0x00 || eeprom[AM32_OFF_BOOT_BYTE] == 0xFF) return false;
	uint8_t rev = eeprom[AM32_OFF_LAYOUT_REVISION];
	if (rev == 0 || rev > 8) return false;
	if (eeprom[0x1B] == 0) return false;   // a motor with zero poles is nonsense
	return true;
}

bool am32LayoutKnown(const uint8_t *eeprom) {
	uint8_t rev = eeprom[AM32_OFF_LAYOUT_REVISION];
	return rev >= AM32_LAYOUT_MIN_KNOWN && rev <= AM32_LAYOUT_MAX_KNOWN;
}

uint8_t am32ViewLayout(const uint8_t *eeprom) {
	uint8_t rev = eeprom[AM32_OFF_LAYOUT_REVISION];
	if (rev < AM32_LAYOUT_MIN_KNOWN) return 0;
	return rev > AM32_LAYOUT_MAX_KNOWN ? (uint8_t)AM32_LAYOUT_MAX_KNOWN : rev;
}

bool am32FieldShown(const Am32Field *f, const uint8_t *eeprom) {
	uint8_t view = am32ViewLayout(eeprom);
	if (!view || !am32FieldApplies(f, view)) return false;
	return !f->readFw || am32Firmware(eeprom) >= f->readFw;
}
