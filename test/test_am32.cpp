/**
 * @file test_am32.cpp
 * @brief Protocol and settings-codec tests, checked against a reference tool.
 *
 * The blobs and expectations here are not invented: they are the default
 * settings the AM32 configurator ships, and the semantics documented in its own
 * byte-by-byte comments. Checking against an implementation known to work on
 * real hardware is what caught the x2 current limit scaling, which looked
 * perfectly plausible. The protocol values, the limits' off values and 3D
 * mode follow AM32's own source (`Inc/common.h`, `loadEEpromSettings()` and
 * `setInput()` in `Src/main.c`), which the reference tool's comments
 * contradict for the protocol.
 */

#include "check.h"
#include "am32_eeprom.h"
#include "am32_bl.h"
#include "am32_backup.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief "Air" defaults, from the configurator's compiled `.data` section.
 * @note Layout revision 2, so bytes 0x05..0x10 are the device name.
 */
static const uint8_t AIR[AM32_SETTINGS_SIZE] = {
	0x01,0x02,0x01,0x01,0x23,0x4e,0x45,0x4f,0x45,0x53,0x43,0x20,0x66,0x30,0x35,0x31,
	0x20,0x00,0x01,0x01,0x01,0x01,0x00,0x03,0x18,0x68,0x30,0x0e,0x01,0x01,0x05,0x00,
	0x80,0x80,0x80,0x32,0x01,0x50,0x00,0x00,0x0f,0x0a,0x0a,0x65,0x14,0x05,0x00,0x31};

/** @brief "Crawler" defaults, from the same section. */
static const uint8_t CRAWLER[AM32_SETTINGS_SIZE] = {
	0x01,0x02,0x01,0x01,0x23,0x4e,0x45,0x4f,0x45,0x53,0x43,0x20,0x66,0x30,0x35,0x31,
	0x20,0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x18,0x64,0x37,0x0e,0x00,0x00,0x05,0x00,
	0x80,0x80,0x80,0x32,0x00,0x32,0x00,0x00,0x0f,0x0a,0x0a,0x8d,0x66,0x06,0x01,0x00};

/**
 * @brief The reference Python tool's AIR_START_EEPROM, whose byte comments
 *        document what each field means. The expectations below are lifted
 *        straight from those comments.
 */
static const uint8_t REF_PY[AM32_SETTINGS_SIZE] = {
	0x01,0x02,0x01,0x01,0x23,0x4E,0x45,0x4F,0x45,0x53,0x43,0x20,0x66,0x30,0x35,0x31,
	0x20,0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x18,0x64,0x37,0x0E,0x00,0x00,0x05,0x00,
	0x80,0x80,0x80,0x32,0x00,0x32,0x00,0x00,0x0F,0x0A,0x0A,0x8D,0x66,0x06,0x01,0x00};

static const Am32Field *field(const char *name) {
	for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
		if (strcmp(AM32_FIELDS[i].name, name) == 0) return &AM32_FIELDS[i];
	}
	return nullptr;
}

static void expect(const uint8_t *ee, const char *name, const char *want) {
	const Am32Field *f = field(name);
	if (!f) { printf("  %-24s MISSING FIELD\n", name); g_failures++; return; }
	char got[24];
	am32FormatValue(f, ee, got, sizeof(got));
	checkStr(name, got, want);
}

/**
 * @brief The range one press may land in, per field and AM32 firmware
 *        (major x 100 + minor, 0 for none), written out from AM32's source
 *        and the configurator rather than from the field table, so a wrong
 *        member there fails against it.
 * @return false for a field this table does not know.
 */
static bool acceptedRange(const char *n, uint16_t fw, int *lo, int *hi) {
	struct R { const char *name; int lo, hi; };
	static const R fixed[] = {
		{"DIRECTION", 0, 1}, {"3D MODE", 0, 1}, {"POLES", 2, 36}, {"KV", 0, 255},
		{"HALL SENSORS", 0, 1}, {"AUTO ADVANCE", 0, 1},
		{"COMPLEMENTARY", 0, 1}, {"POWER", 50, 150}, {"SINE STARTUP", 0, 1},
		{"SINE RANGE", 5, 25}, {"SINE POWER", 1, 10}, {"STUCK ROTOR", 0, 1},
		{"STALL PROTECT", 0, 1}, {"STOPPED LEVEL", 1, 10}, {"RUNNING LEVEL", 1, 10},
		{"RC CAR REVERSE", 0, 1}, {"CELL CUTOFF", 0, 100}, {"TEMP LIMIT", 70, 141},
		{"CURRENT LIMIT", 0, 102}, {"PROTOCOL", 0, 4}, {"30MS TELEMETRY", 0, 1},
		{"BEEP VOLUME", 0, 11}, {"LOW THRESHOLD", 0, 255}, {"HIGH THRESHOLD", 0, 255},
		{"NEUTRAL", 0, 255}, {"DEAD BAND", 0, 100}, {"MAX RAMP", 1, 200},
		{"MIN DUTY", 0, 50}, {"NO STICK CAL", 0, 1}, {"ABS VOLT CUT", 1, 100},
		{"CURRENT P", 0, 255}, {"CURRENT I", 0, 255}, {"CURRENT D", 0, 255},
		{"ACTIVE BRAKE", 0, 5},
	};
	for (const R &r : fixed)
		if (strcmp(r.name, n) == 0) { *lo = r.lo; *hi = r.hi; return true; }
	// The firmware gates: `pwm_frequency < 49` before v2.19, `< 145` from it;
	// variable_pwm 2 from v2.18; brake_on_stop 2 from v2.19; an absolute
	// cutoff that trips from v2.21.
	*lo = 0;
	// advance_level: 0..3 alone before v2.19 (`< 4`, else 2); from v2.19 both
	// encodings, and a press writes the 10..42 one.
	if (strcmp(n, "ADVANCE") == 0)         { *lo = fw >= 219 ? 10 : 0; *hi = fw >= 219 ? 42 : 3; return true; }
	if (strcmp(n, "PWM FREQ") == 0)        { *lo = 8; *hi = fw >= 219 ? 144 : 48; return true; }
	if (strcmp(n, "PWM TYPE") == 0)        { *hi = fw >= 218 ? 2 : 1; return true; }
	if (strcmp(n, "BRAKE ON STOP") == 0)   { *hi = fw >= 219 ? 2 : 1; return true; }
	if (strcmp(n, "LOW VOLT CUTOFF") == 0) { *hi = fw >= 221 ? 2 : 1; return true; }
	return false;
}

/**
 * @brief Timing advance, which AM32 writes in two different encodings.
 *
 * Reported from the bench as "the initial value of timing is read without
 * conversion into the display; when you change it it gets corrected". Both
 * halves of that sentence were symptoms of the same thing: the field table
 * described the old encoding only, so a modern ESC's byte was scaled as
 * though it were an old one, and the first `-` or `+` press clamped it into the
 * old range -- which made the number plausible again by destroying it.
 *
 * The encodings, from AM32's own start-up code:
 *   0..3   the old encoding, read by every firmware, 7.5 degree steps
 *   10..42 read from firmware 2.19, (raw - 10) steps of 0.9375 degrees
 */
static void testAdvanceEncodings() {
	section("AM32: timing advance, both encodings");

	uint8_t ee[AM32_SETTINGS_SIZE];
	memcpy(ee, REF_PY, sizeof(ee));
	// Firmware 2.22: both encodings are read from 2.19 on.
	ee[AM32_OFF_MAIN_REVISION] = 2;
	ee[AM32_OFF_SUB_REVISION] = 22;

	// --- modern encoding, which is what the bug report was about ---
	ee[AM32_OFF_ADVANCE] = 26;              // AM32's own default
	expect(ee, "ADVANCE", "15.0DEG");
	ee[AM32_OFF_ADVANCE] = 10;
	expect(ee, "ADVANCE", "0.0DEG");
	ee[AM32_OFF_ADVANCE] = 42;
	expect(ee, "ADVANCE", "30.0DEG");

	// The number this used to show for a modern default: 26 scaled as if it
	// were an old-format byte. Nothing on the ESC was ever set to 195 degrees.
	checkTrue("a modern byte is no longer read as the old encoding",
	          am32AdvanceDeciDeg(26) != 26 * 75);

	// --- old encoding, which must keep working ---
	ee[AM32_OFF_ADVANCE] = 0;  expect(ee, "ADVANCE", "0.0DEG");
	ee[AM32_OFF_ADVANCE] = 2;  expect(ee, "ADVANCE", "15.0DEG");
	ee[AM32_OFF_ADVANCE] = 3;  expect(ee, "ADVANCE", "22.5DEG");

	// --- neither: say so rather than invent an angle ---
	// AM32 substitutes its own default for these, so the ESC is not running at
	// whatever we would otherwise have printed.
	checkInt("a byte in neither encoding has no angle",
	         am32AdvanceDeciDeg(7), -1);
	checkInt("nor does one above the modern maximum",
	         am32AdvanceDeciDeg(200), -1);
	ee[AM32_OFF_ADVANCE] = 7;
	expect(ee, "ADVANCE", "? (7)");

	// --- editing ---
	const Am32Field *f = field("ADVANCE");
	checkTrue("the field exists", f != nullptr);

	// Editing writes the modern encoding, whatever was read.
	ee[AM32_OFF_ADVANCE] = 26;
	am32Adjust(f, ee, +1);
	checkInt("one step up is one 0.9375 degree increment", ee[AM32_OFF_ADVANCE], 27);
	am32Adjust(f, ee, -1);
	checkInt("and one step back returns", ee[AM32_OFF_ADVANCE], 26);

	// This is the half that silently threw the setting away. An ESC holding the
	// old value 2 -- 15 degrees -- used to jump to the modern minimum on the
	// first press, because 2 is below it. It must convert, keeping the angle.
	ee[AM32_OFF_ADVANCE] = 2;
	am32Adjust(f, ee, +1);
	checkInt("an old-format value converts rather than clamping to zero",
	         am32AdvanceDeciDeg(ee[AM32_OFF_ADVANCE]), 150 + 9);
	ee[AM32_OFF_ADVANCE] = 2;
	am32Adjust(f, ee, -1);
	checkTrue("and a step down from it stays near 15 degrees",
	          am32AdvanceDeciDeg(ee[AM32_OFF_ADVANCE]) > 130);

	// The rails hold.
	ee[AM32_OFF_ADVANCE] = 10;
	am32Adjust(f, ee, -1);
	checkInt("it does not step below zero degrees", ee[AM32_OFF_ADVANCE], 10);
	ee[AM32_OFF_ADVANCE] = 42;
	am32Adjust(f, ee, +1);
	checkInt("nor above thirty", ee[AM32_OFF_ADVANCE], 42);

	// A byte in neither encoding becomes AM32's own default rather than 0.
	ee[AM32_OFF_ADVANCE] = 200;
	am32Adjust(f, ee, +1);
	checkTrue("an unreadable byte edits from AM32's default, not from zero",
	          ee[AM32_OFF_ADVANCE] >= 25 && ee[AM32_OFF_ADVANCE] <= 28);

	// --- before 2.19: the old encoding alone (v2.05 main.c:629-633,
	//     v2.18 main.c:591: any other byte is taken as 2) ---
	for (int sub = 18; sub <= 19; sub++) {
		ee[AM32_OFF_SUB_REVISION] = (uint8_t)sub;
		ee[AM32_OFF_ADVANCE] = 26;
		expect(ee, "ADVANCE", sub == 18 ? "? (26)" : "15.0DEG");
	}
	ee[AM32_OFF_SUB_REVISION] = 18;
	ee[AM32_OFF_ADVANCE] = 3;  expect(ee, "ADVANCE", "22.5DEG");
	ee[AM32_OFF_ADVANCE] = 2;
	am32Adjust(f, ee, +1);
	checkInt("2.18: a step up from 15 degrees is 3, 22.5 degrees", ee[AM32_OFF_ADVANCE], 3);
	am32Adjust(f, ee, +1);
	checkInt("2.18: and stops there", ee[AM32_OFF_ADVANCE], 3);
	ee[AM32_OFF_ADVANCE] = 0;
	am32Adjust(f, ee, -1);
	checkInt("2.18: nor below 0", ee[AM32_OFF_ADVANCE], 0);
	ee[AM32_OFF_ADVANCE] = 26;
	am32Adjust(f, ee, -1);
	checkInt("2.18: a modern byte lands on 3, the nearer end", ee[AM32_OFF_ADVANCE], 3);
	ee[AM32_OFF_MAIN_REVISION] = 0xFF; ee[AM32_OFF_SUB_REVISION] = 0xFF;
	ee[AM32_OFF_ADVANCE] = 1;
	am32Adjust(f, ee, +1);
	checkInt("unknown firmware: the old encoding too", ee[AM32_OFF_ADVANCE], 2);
}

/** @brief The backup's text: header, a line per field, the bytes, the checksum. */
static void testBackupText() {
	section("AM32 backup: the text of the settings block");
	checkTrue("CRC-32 of \"123456789\" is CBF43926", am32BackupCrc32("123456789", 9) == 0xCBF43926u);
	uint8_t blk[AM32_SETTINGS_SIZE];
	for (unsigned i = 0; i < sizeof(blk); i++) blk[i] = (uint8_t)(i * 7u);
	blk[AM32_OFF_BOOT_BYTE] = 1;
	blk[AM32_OFF_LAYOUT_REVISION] = 3;
	blk[AM32_OFF_MAIN_REVISION] = 2;
	blk[AM32_OFF_SUB_REVISION] = 21;
	blk[0x1B] = 14;  // POLES
	static char out[AM32_BACKUP_MAX];
	size_t n = am32BackupFormat(blk, "F051", 0x7C00, out, sizeof(out));
	out[n] = '\0';
	checkTrue("it has text", n > 0 && n < sizeof(out));
	static const char HEAD[] = "AM32 ESC settings, format 1\nESC: F051\nSettings address: 7C00\n"
	                           "Layout revision: 3\nFirmware: 2.21\n";
	checkTrue("the header names format 1", strncmp(out, HEAD, sizeof(HEAD) - 1) == 0);
	checkTrue("a field line with its group", strstr(out, "\nMOTOR POLES = 14\n") != nullptr);
	int fields = 0;
	for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) fields += am32FieldApplies(&AM32_FIELDS[i], 3);
	int lines = 0;
	for (size_t i = 0; i < n; i++) lines += out[i] == '\n';
	checkInt("5 header lines, the fields, bytes, 3 byte lines, the checksum", lines, 5 + fields + 1 + 3 + 1);
	checkTrue("the last byte line", strstr(out, "\n20: ") != nullptr &&
	                                 strstr(out, "\n30: ") == nullptr);
	const char *sum = strstr(out, "\nchecksum ");
	char want[24];
	snprintf(want, sizeof(want), "\nchecksum %08lX\n",
	         (unsigned long)am32BackupCrc32(out, (size_t)(sum - out) + 1));
	checkTrue("the checksum covers every line before it", sum && strcmp(sum, want) == 0);
	checkInt("a buffer too small: nothing", (long)am32BackupFormat(blk, "F051", 0x7C00, out, 64), 0);
	// Firmware 1.79 reads nothing at 0x2A to 0x2F: no line for those six
	// fields, their bytes in the hex lines.
	blk[AM32_OFF_MAIN_REVISION] = 1;
	blk[AM32_OFF_SUB_REVISION] = 79;
	n = am32BackupFormat(blk, "F051", 0x7C00, out, sizeof(out));
	out[n] = '\0';
	int lines79 = 0;
	for (size_t i = 0; i < n; i++) lines79 += out[i] == '\n';
	checkTrue("1.79: no TEMP LIMIT or PROTOCOL line, still POLES",
	          strstr(out, "TEMP LIMIT") == nullptr && strstr(out, "PROTOCOL") == nullptr &&
	          strstr(out, "\nMOTOR POLES = 14\n") != nullptr);
	checkInt("1.79: six field lines fewer", lines79, lines - 6);
}

void runAm32Tests() {
	testAdvanceEncodings();
	testBackupText();
	section("AM32 init string (must be byte-exact)");
	{
		// Twelve zeros, 0x0D, "BLHeli", then the fixed signature F4 7D.
		static const uint8_t REF[21] = {
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x0D,'B','L','H','e','l','i',0xF4,0x7D};
		checkInt("init string length", sizeof(REF), 21);
		int zeros = 0;
		while (zeros < 21 && REF[zeros] == 0) zeros++;
		checkInt("leading zero count", zeros, 12);
		// F4 7D is a literal, not a checksum over the body.
		checkTrue("trailer is not a CRC of the body",
		          am32Crc16(REF, 19) != (uint16_t)(0x7DF4));
	}

	section("AM32 result texts");
	{
		// Run at run time too: the screens' static_asserts evaluate them at
		// build time, which no counter sees.
		volatile int r = AM32_ERR_BUSY;
		checkStr("OK", am32ResultText(AM32_OK), "OK");
		checkStr("timeout", am32ResultText(AM32_ERR_TIMEOUT), "NO REPLY");
		checkStr("NAK", am32ResultText(AM32_ERR_NAK), "REFUSED");
		checkStr("CRC", am32ResultText(AM32_ERR_CRC), "CRC FAIL");
		checkStr("busy", am32ResultText((Am32Result)r), "NOT READY");
		checkStr("a value out of range", am32ResultText((Am32Result)(r + 1)), "?");
		checkInt("the longest is NOT READY, 9 characters", am32ResultTextMax(), 9);
	}

	section("CRC-16/ARC (poly 0xA001, init 0)");
	{
		const uint8_t v[] = {'1','2','3','4','5','6','7','8','9'};
		checkInt("crc16(\"123456789\")", am32Crc16(v, 9), 0xBB3D);
	}

	section("MCU family to settings address");
	{
		checkInt("G071 type byte", AM32_ESC_G071_2KB, 0x2B);
		checkInt("F051 type byte", AM32_ESC_F051_1KB, 0x1F);
		checkInt("F3 type byte",   AM32_ESC_F3_2KB,   0x35);
	}

	section("Device identity");
	{
		char name[AM32_NAME_LEN + 1];
		am32DeviceName(AIR, name);
		checkStr("device name", name, "NEOESC f051");
		checkTrue("AIR blob recognised", am32Plausible(AIR));
		checkTrue("CRAWLER blob recognised", am32Plausible(CRAWLER));

		uint8_t erased[AM32_SETTINGS_SIZE];
		memset(erased, 0xFF, sizeof(erased));
		checkTrue("erased page rejected", !am32Plausible(erased));
	}

	section("AIR preset decodes to the tool's values");
	expect(AIR, "POLES", "14");
	expect(AIR, "PWM FREQ", "24KHZ");
	expect(AIR, "KV", "1940KV");                  // 48 x 40 + 20
	expect(AIR, "POWER", "104");
	expect(AIR, "ADVANCE", "22.5DEG");
	expect(AIR, "DIRECTION", "NORMAL");
	expect(AIR, "3D MODE", "ON");
	expect(AIR, "LOW VOLT CUTOFF", "CELL");
	expect(AIR, "CELL CUTOFF", "3.3V");
	expect(AIR, "TEMP LIMIT", "101C");
	expect(AIR, "CURRENT LIMIT", "40A");
	expect(AIR, "NEUTRAL", "1502US");
	expect(AIR, "LOW THRESHOLD", "1006US");
	expect(AIR, "HIGH THRESHOLD", "2006US");
	expect(AIR, "DEAD BAND", "50");
	expect(AIR, "BEEP VOLUME", "5");
	expect(AIR, "BRAKE ON STOP", "BRAKE");

	section("CRAWLER preset differences show through");
	expect(CRAWLER, "3D MODE", "OFF");
	expect(CRAWLER, "SINE STARTUP", "OFF");
	expect(CRAWLER, "ADVANCE", "15.0DEG");
	expect(CRAWLER, "POWER", "100");
	expect(CRAWLER, "KV", "2220KV");
	expect(CRAWLER, "LOW VOLT CUTOFF", "OFF");
	expect(CRAWLER, "CELL CUTOFF", "3.0V");
	expect(CRAWLER, "TEMP LIMIT", "OFF");         // 141: no limit
	expect(CRAWLER, "CURRENT LIMIT", "OFF");      // 102: no limit
	expect(CRAWLER, "STUCK ROTOR", "ON");
	expect(CRAWLER, "BRAKE ON STOP", "OFF");

	section("Field table vs the reference tool's documented semantics");
	// Each expectation is what constants.py's own byte comment states.
	expect(REF_PY, "ADVANCE", "15.0DEG");        // "2 = 15 degrees"
	expect(REF_PY, "PWM FREQ", "24KHZ");         // "default 24khz"
	expect(REF_PY, "POWER", "100");              // "default 100 percent"
	// "55 = 2200kv" in the tool; AM32 reads raw x 40 + 20.
	expect(REF_PY, "KV", "2220KV");
	expect(REF_PY, "POLES", "14");               // "default 14"
	expect(REF_PY, "LOW THRESHOLD", "1006US");   // "(value*2) + 750"
	expect(REF_PY, "HIGH THRESHOLD", "2006US");  // "(value*2) + 1750"
	expect(REF_PY, "NEUTRAL", "1502US");         // "128 = 1500 us"
	expect(REF_PY, "CELL CUTOFF", "3.0V");       // "default 50 3.0volts"
	expect(REF_PY, "SINE RANGE", "15");          // "default 15"
	expect(REF_PY, "TEMP LIMIT", "OFF");         // "default 141": AM32 takes it as none
	expect(REF_PY, "CURRENT LIMIT", "OFF");      // "(value x 2) ... default 102": none
	expect(REF_PY, "SINE POWER", "6");           // "default 6"
	// The tool's comment reads "1)Auto"; AM32's enum inputType has
	// AUTO_IN = 0, DSHOT_IN = 1, and the configurator offers 0 as Auto.
	expect(REF_PY, "PROTOCOL", "DSHOT");
	expect(AIR, "PROTOCOL", "AUTO");

	section("AM32: protocol values, zero-based as AM32's enum inputType");
	{
		static const char *const want[] = {"AUTO", "DSHOT", "SERVO", "SERIAL", "EDT ARM", "DRONECAN"};
		uint8_t e[AM32_SETTINGS_SIZE];
		memcpy(e, AIR, sizeof(e));
		for (uint8_t v = 0; v < 6; v++) {
			e[AM32_OFF_INPUT_TYPE] = v;
			expect(e, "PROTOCOL", want[v]);
		}
		// loadEEpromSettings(): AUTO and DSHOT set EDT_ARMED; 10 and above
		// fall to AUTO. Every other value leaves it at 0, and dshot.c drops
		// each throttle above 47; EDT ARM sets it on the EDT enable and a
		// zero frame clears it again.
		bool notes = true;
		for (int v = 0; v < 256; v++) {
			e[AM32_OFF_INPUT_TYPE] = (uint8_t)v;
			bool runs = v == 0 || v == 1 || v >= 10;
			if (am32InputNotDshot(e) == runs) notes = false;
		}
		checkTrue("the note for 2 to 9; none for 0, 1 and 10 to 255", notes);
		e[AM32_OFF_INPUT_TYPE] = 6;
		expect(e, "PROTOCOL", "? (6)");
		checkInt("the editor stops at EDT ARM, as the configurator does", field("PROTOCOL")->rawMax, 4);
		e[AM32_OFF_INPUT_TYPE] = 5;
		am32Adjust(field("PROTOCOL"), e, +1);
		checkInt("a press on DRONECAN lands on EDT ARM", e[AM32_OFF_INPUT_TYPE], 4);
	}

	section("AM32: the limits' off values, as AM32 reads them");
	{
		uint8_t e[AM32_SETTINGS_SIZE];
		memcpy(e, AIR, sizeof(e));
		const Am32Field *temp = field("TEMP LIMIT");
		const Am32Field *cur = field("CURRENT LIMIT");
		const Am32Field *lvc = field("LOW VOLT CUTOFF");
		const Am32Field *poles = field("POLES");
		// Temperature: 70..140 limits, anything else is 255, none.
		e[0x2B] = 69;  expect(e, "TEMP LIMIT", "OFF");
		e[0x2B] = 70;  expect(e, "TEMP LIMIT", "70C");
		checkTrue("70 C: on", !am32ProtectionOff(temp, e));
		e[0x2B] = 140; expect(e, "TEMP LIMIT", "140C");
		e[0x2B] = 141; expect(e, "TEMP LIMIT", "OFF");
		checkTrue("141: off", am32ProtectionOff(temp, e));
		// Current: 1..100 limits from 2.21, 1..99 before, in 2 A steps; 0 and
		// above are none.
		e[0x2C] = 0;   expect(e, "CURRENT LIMIT", "OFF");
		checkTrue("0: off", am32ProtectionOff(cur, e));
		// 1.80 (AlkaMotors 9909ea7) acts on 2..99, 1.81 (f917c01) on 1..99.
		e[AM32_OFF_MAIN_REVISION] = 1; e[AM32_OFF_SUB_REVISION] = 80;
		e[0x2C] = 1;   expect(e, "CURRENT LIMIT", "OFF");
		e[0x2C] = 2;   expect(e, "CURRENT LIMIT", "4A");
		e[AM32_OFF_SUB_REVISION] = 81;
		e[0x2C] = 1;   expect(e, "CURRENT LIMIT", "2A");
		e[0x2C] = 99;  expect(e, "CURRENT LIMIT", "198A");
		e[0x2C] = 100; expect(e, "CURRENT LIMIT", "OFF");
		checkTrue("100 on firmware 1.35: off", am32ProtectionOff(cur, e));
		e[AM32_OFF_MAIN_REVISION] = 2; e[AM32_OFF_SUB_REVISION] = 20;
		checkTrue("100 on firmware 2.20: off, `< 100`", am32ProtectionOff(cur, e));
		e[AM32_OFF_SUB_REVISION] = 21;
		expect(e, "CURRENT LIMIT", "200A");
		checkTrue("100 on firmware 2.21: on, `<= 100`", !am32ProtectionOff(cur, e));
		e[AM32_OFF_MAIN_REVISION] = AIR[AM32_OFF_MAIN_REVISION];
		e[AM32_OFF_SUB_REVISION] = AIR[AM32_OFF_SUB_REVISION];
		e[0x2C] = 101; expect(e, "CURRENT LIMIT", "OFF");
		e[0x2C] = 102; expect(e, "CURRENT LIMIT", "OFF");
		checkTrue("102: off", am32ProtectionOff(cur, e));
		// Low voltage cutoff: 0 off; 1 per cell and 2 absolute both act.
		e[AM32_OFF_LOW_VOLT_CUTOFF] = 0;
		checkTrue("cutoff 0: off", am32ProtectionOff(lvc, e));
		e[AM32_OFF_LOW_VOLT_CUTOFF] = 2;
		checkTrue("cutoff 2 on firmware 1.35: off, AM32 acts on 1 alone", am32ProtectionOff(lvc, e));
		e[AM32_OFF_MAIN_REVISION] = 2; e[AM32_OFF_SUB_REVISION] = 19;
		checkTrue("cutoff 2 on firmware 2.19: off, the raw byte against 10 mV", am32ProtectionOff(lvc, e));
		e[AM32_OFF_SUB_REVISION] = 20;
		checkTrue("cutoff 2 on firmware 2.20: off", am32ProtectionOff(lvc, e));
		e[AM32_OFF_SUB_REVISION] = 21;
		checkTrue("cutoff 2 on firmware 2.21: on, x 50", !am32ProtectionOff(lvc, e));
		e[AM32_OFF_MAIN_REVISION] = AIR[AM32_OFF_MAIN_REVISION];
		e[AM32_OFF_SUB_REVISION] = AIR[AM32_OFF_SUB_REVISION];
		e[0x1B] = 0;
		checkTrue("a field that is no protection is never off", !am32ProtectionOff(poles, e));
		// Stepping from OFF at the top comes back into the range.
		e[0x2B] = 141;
		am32Adjust(temp, e, -1);
		expect(e, "TEMP LIMIT", "140C");
	}

	section("AM32: three-value fields, the top value gated by firmware");
	{
		struct C { const char *name; uint8_t off; uint16_t fw; const char *n[3]; };
		static const C cs[] = {
			{ "LOW VOLT CUTOFF", 0x24, 221, { "OFF", "CELL", "ABSOLUTE" } },
			{ "PWM TYPE",        0x15, 218, { "FIXED", "VARIABLE", "BY RPM" } },
			{ "BRAKE ON STOP",   0x1C, 219, { "OFF", "BRAKE", "ACTIVE" } },
		};
		for (const C &c : cs) {
			const Am32Field *f = field(c.name);
			checkTrue(c.name, f && f->offset == c.off && f->type == A32_ENUM);
			if (!f) continue;
			uint8_t e[AM32_SETTINGS_SIZE];
			memcpy(e, AIR, sizeof(e));
			// New enough: all three, and the editor reaches 2 and stops.
			e[AM32_OFF_MAIN_REVISION] = (uint8_t)(c.fw / 100);
			e[AM32_OFF_SUB_REVISION] = (uint8_t)(c.fw % 100);
			for (uint8_t v = 0; v < 3; v++) { e[c.off] = v; expect(e, c.name, c.n[v]); }
			checkInt("  firmware at the gate: the editor reaches 2", am32FieldMax(f, e), 2);
			e[c.off] = 2;
			am32Adjust(f, e, +1);
			checkInt("  + at 2 stays at 2", e[c.off], 2);
			am32Adjust(f, e, -1);
			checkInt("  - at 2 is 1", e[c.off], 1);
			// One minor version older: 2 is not a value this firmware has.
			e[AM32_OFF_SUB_REVISION] = (uint8_t)(c.fw % 100 - 1);
			checkInt("  one version older: the editor stops at 1", am32FieldMax(f, e), 1);
			e[c.off] = 1;
			am32Adjust(f, e, +1);
			checkInt("  + at 1 stays at 1", e[c.off], 1);
			e[c.off] = 2;
			expect(e, c.name, "? (2)");
			e[c.off] = 3;
			e[AM32_OFF_SUB_REVISION] = (uint8_t)(c.fw % 100);
			expect(e, c.name, "? (3)");
		}
		checkInt("2.19 reads as 219", am32Firmware((const uint8_t[]){0, 0, 0, 2, 19}), 219);
	}

	section("AM32: field scales against AM32's source and the configurator");
	{
		uint8_t e[AM32_SETTINGS_SIZE];
		memcpy(e, AIR, sizeof(e));
		e[AM32_OFF_LAYOUT_REVISION] = 3;
		e[0x1A] = 0;   expect(e, "KV", "20KV");
		e[0x1A] = 255; expect(e, "KV", "10220KV");
		e[0x05] = 1;   expect(e, "MAX RAMP", "0.1%/MS");
		e[0x05] = 200; expect(e, "MAX RAMP", "20.0%/MS");
		e[0x06] = 50;  expect(e, "MIN DUTY", "25.0%");
		e[0x08] = 10;  expect(e, "ABS VOLT CUT", "10");   // 1.35: no volts
		e[AM32_OFF_MAIN_REVISION] = 2; e[AM32_OFF_SUB_REVISION] = 20;
		expect(e, "ABS VOLT CUT", "10");                   // 2.20: the raw byte
		e[AM32_OFF_SUB_REVISION] = 21;
		e[0x08] = 1;   expect(e, "ABS VOLT CUT", "0.5V");
		e[0x08] = 100; expect(e, "ABS VOLT CUT", "50.0V");
		e[0x0C] = 5;   expect(e, "ACTIVE BRAKE", "2.5%");  // 0.5 % a unit
		e[0x0C] = 1;   expect(e, "ACTIVE BRAKE", "0.5%");
		checkInt("ACTIVE BRAKE stops at 5 %", field("ACTIVE BRAKE")->rawMax, 5);
		checkInt("ABS VOLT CUT stops at 100", field("ABS VOLT CUT")->rawMax, 100);
	}

	section("AM32: 3D MODE is byte 0x12, bi_direction");
	{
		const Am32Field *f = field("3D MODE");
		checkTrue("the field exists", f != nullptr);
		checkInt("at 0x12", f ? f->offset : 0, 0x12);
		checkTrue("AIR: on", am32Is3d(AIR));
		checkTrue("CRAWLER: off", !am32Is3d(CRAWLER));
		checkTrue("no field is called BIDIRECTIONAL", field("BIDIRECTIONAL") == nullptr);
	}

	section("AM32: firmware version, layout, and booleans above 1");
	{
		uint8_t e[AM32_SETTINGS_SIZE];
		memcpy(e, AIR, sizeof(e));
		e[AM32_OFF_MAIN_REVISION] = 0xFF; e[AM32_OFF_SUB_REVISION] = 0xFF;
		checkInt("FF.FF is no version: 0, the oldest", am32Firmware(e), 0);
		e[AM32_OFF_LOW_VOLT_CUTOFF] = 2;
		expect(e, "LOW VOLT CUTOFF", "? (2)");
		e[AM32_OFF_MAIN_REVISION] = 2; e[AM32_OFF_SUB_REVISION] = 100;
		checkInt("minor 100 is no version", am32Firmware(e), 0);
		e[AM32_OFF_MAIN_REVISION] = 0; e[AM32_OFF_SUB_REVISION] = 22;
		checkInt("major 0 is no version", am32Firmware(e), 0);
		e[AM32_OFF_MAIN_REVISION] = 9; e[AM32_OFF_SUB_REVISION] = 99;
		checkInt("9.99 is the newest that reads", am32Firmware(e), 999);
		for (uint8_t rev = 0; rev < 10; rev++) {
			e[AM32_OFF_LAYOUT_REVISION] = rev;
			checkTrue(rev >= 2 && rev <= 4 ? "  layout known" : "  layout not known",
			          am32LayoutKnown(e) == (rev >= 2 && rev <= 4));
		}
		memcpy(e, AIR, sizeof(e));
		e[0x1D] = 2;  expect(e, "STALL PROTECT", "? (2)");
		e[0x1D] = 1;  expect(e, "STALL PROTECT", "ON");
	}

	section("AM32: every field, every byte, eight firmware versions");
	{
		// fw: none (FF.FF), 1.35, 2.16, 2.18, 2.19, 2.20, 2.21, 2.22.
		static const uint8_t fws[][2] = {
			{0xFF, 0xFF}, {1, 35}, {2, 16}, {2, 18}, {2, 19}, {2, 20}, {2, 21}, {2, 22}};
		bool kept = true, inRange = true, known = true, snaps = true;
		for (const auto &fw : fws) {
			// The version as AM32 numbers it, from the bytes, by hand.
			uint16_t ver = (fw[0] >= 1 && fw[0] <= 9 && fw[1] <= 99) ? fw[0] * 100 + fw[1] : 0;
			for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
				const Am32Field *f = &AM32_FIELDS[i];
				int lo, hi;
				if (!acceptedRange(f->name, ver, &lo, &hi)) { known = false; continue; }
				for (int v = 0; v < 256; v++) {
					for (int dir = -1; dir <= 1; dir += 2) {
						uint8_t e[AM32_SETTINGS_SIZE];
						// Every byte different, so a write to any of them shows.
						for (int k = 0; k < AM32_SETTINGS_SIZE; k++) e[k] = (uint8_t)(k * 37 + v + 11);
						e[AM32_OFF_LAYOUT_REVISION] = 4;
						e[AM32_OFF_MAIN_REVISION] = fw[0];
						e[AM32_OFF_SUB_REVISION] = fw[1];
						e[f->offset] = (uint8_t)v;
						uint8_t before[AM32_SETTINGS_SIZE];
						memcpy(before, e, sizeof(e));
						am32Adjust(f, e, dir);
						// The whole block but the pressed byte, as it was.
						for (int k = 0; k < AM32_SETTINGS_SIZE; k++)
							if (k != f->offset && e[k] != before[k]) kept = false;
						// One press lands inside the range that firmware accepts,
						// from the literal table, not the field's own members.
						if (e[f->offset] < lo || e[f->offset] > hi) inRange = false;
						// From outside it, a press lands on the nearer end: the
						// firmware's own top, not one the table made up.
						// (From 2.19 a byte AM32 does not read edits from its
						// default, 15 degrees, instead.)
						if ((f->type != A32_ADVANCE || ver < 219) &&
						    ((v > hi && e[f->offset] != hi) ||
						                               (v < lo && e[f->offset] != lo)))
							snaps = false;
					}
				}
			}
		}
		checkTrue("every field has a row in the literal table", known);
		checkTrue("a press changes no byte of the block but its own", kept);
		checkTrue("one press from any byte lands in the firmware's range", inRange);
		checkTrue("from outside the range, on the end the firmware accepts", snaps);
	}

	section("PWM frequency range");
	{
		const Am32Field *f = field("PWM FREQ");
		checkInt("max is 144 kHz, not the old 48", f->rawMax, 144);
		checkInt("min", f->rawMin, 8);
	}

	section("Layout-revision gating");
	{
		int n2 = 0, n3 = 0;
		for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
			if (am32FieldApplies(&AM32_FIELDS[i], 2)) n2++;
			if (am32FieldApplies(&AM32_FIELDS[i], 3)) n3++;
		}
		checkInt("revision 3 exposes 8 more fields", n3 - n2, 8);

		uint8_t v3[AM32_SETTINGS_SIZE];
		memcpy(v3, AIR, sizeof(v3));
		v3[AM32_OFF_LAYOUT_REVISION] = 3;
		char name[AM32_NAME_LEN + 1];
		am32DeviceName(v3, name);
		// Those bytes are settings in revision 3, so rendering them as text
		// would print binary garbage as a device name.
		checkTrue("name suppressed at revision 3", name[0] == '\0');
	}

	section("Editing clamps and steps");
	{
		uint8_t e[AM32_SETTINGS_SIZE];
		memcpy(e, AIR, sizeof(e));
		const Am32Field *poles = field("POLES");
		for (int i = 0; i < 40; i++) am32Adjust(poles, e, +1);
		checkInt("poles clamp at max", e[poles->offset], poles->rawMax);
		for (int i = 0; i < 80; i++) am32Adjust(poles, e, -1);
		checkInt("poles clamp at min", e[poles->offset], poles->rawMin);

		const Am32Field *pwm = field("PWM FREQ");
		for (int i = 0; i < 200; i++) am32Adjust(pwm, e, +1);
		checkInt("firmware 1.35: pwm stops at 48, `< 49`", e[pwm->offset], 48);
		e[AM32_OFF_MAIN_REVISION] = 2; e[AM32_OFF_SUB_REVISION] = 18;
		am32Adjust(pwm, e, +1);
		checkInt("2.18: still 48", e[pwm->offset], 48);
		e[pwm->offset] = 100;
		expect(e, "PWM FREQ", "? (100)");                 // 2.18 ignores it
		e[0x28] = 30;  expect(e, "SINE RANGE", "? (30)"); // outside 5..25
		e[0x23] = 101; expect(e, "DEAD BAND", "? (101)");
		e[AM32_OFF_SUB_REVISION] = 19;
		expect(e, "PWM FREQ", "100KHZ");                  // 2.19 takes it
		for (int i = 0; i < 200; i++) am32Adjust(pwm, e, +1);
		checkInt("2.19: pwm reaches 144, `< 145`", e[pwm->offset], 144);
	}
}
