/**
 * @file test_settings.cpp
 * @brief Persistence: defaults, validation, and what a bad block does.
 *
 * The rules under test here are the ones that decide what a board does with a
 * flash sector it cannot trust. Every one of them is written to fail towards
 * the compiled defaults, and each check below is the assertion that it does —
 * because the field this most matters for is the throttle ceiling, and the
 * failure mode of getting it wrong is a motor at full power.
 */

#include "check.h"
#include "fakes.h"

#include "settings.h"
#include "config.h"
#include "board_desc.h"

#include <string.h>
#include <stddef.h>

/** @brief Write @p s to the fake flash as a valid, correctly-CRC'd block. */
static void storeValid(const Settings *s) {
	SettingsBlock blk;
	memset(&blk, 0, sizeof(blk));
	blk.magic   = SETTINGS_MAGIC;
	blk.version = SETTINGS_VERSION;
	blk.size    = (uint16_t)sizeof(Settings);
	blk.s       = *s;
	blk.crc32   = settingsCrc32(&blk, (uint32_t)(sizeof(blk) - sizeof(uint32_t)));
	memcpy(fakeFlashBytes(), &blk, sizeof(blk));
}

/** @brief The block currently in the fake flash. */
static SettingsBlock stored() {
	SettingsBlock blk;
	memcpy(&blk, fakeFlashBytes(), sizeof(blk));
	return blk;
}

static void testBlankFlash() {
	section("Settings: a blank part");

	fakeFlashClear();
	settingsLoad();

	Settings def;
	settingsDefaults(&def);

	checkTrue("no stored block reported", !settingsStored());
	checkInt("ESC pin is the compiled default", settings()->dshotPin, def.dshotPin);
	checkInt("ceiling is the compiled default", settings()->maxThrottle, def.maxThrottle);
	checkTrue("nothing to save", !settingsDirty());
}

static void testRoundTrip() {
	section("Settings: save and load back");

	fakeFlashClear();
	settingsLoad();

	settings()->poles = 12;
	settings()->maxThrottle = 600;
	settings()->highContrast = 1;
	checkTrue("editing marks it dirty", settingsDirty());

	checkTrue("save reports success", settingsSave());
	checkTrue("saving clears dirty", !settingsDirty());
	checkTrue("now reports a stored block", settingsStored());

	// Reload from scratch, as a power cycle would.
	settings()->poles = 4;
	settingsLoad();
	checkInt("poles survived", settings()->poles, 12);
	checkInt("ceiling survived", settings()->maxThrottle, 600);
	checkInt("contrast survived", settings()->highContrast, 1);
	checkTrue("stored block found", settingsStored());
}

static void testRejectsCorruption() {
	section("Settings: a block that does not validate is discarded whole");

	Settings def;
	settingsDefaults(&def);

	// A ceiling nobody would choose, stored correctly, so the only thing under
	// test below is whether the *block* is trusted.
	Settings evil = def;
	evil.maxThrottle = MAX_THROTTLE_CEILING;
	evil.poles = 22;

	// --- one flipped payload byte ---
	fakeFlashClear();
	storeValid(&evil);
	fakeFlashBytes()[offsetof(SettingsBlock, s)] ^= 0x01;
	settingsLoad();
	checkTrue("bad CRC: not treated as stored", !settingsStored());
	checkTrue("bad CRC, this version: not reset by an update", !settingsResetByUpdate());
	checkInt("bad CRC: ceiling falls back", settings()->maxThrottle, def.maxThrottle);
	checkInt("bad CRC: poles fall back", settings()->poles, def.poles);

	// --- a future version, otherwise perfect ---
	fakeFlashClear();
	storeValid(&evil);
	{
		SettingsBlock blk = stored();
		blk.version = SETTINGS_VERSION + 1;
		blk.crc32 = settingsCrc32(&blk, (uint32_t)(sizeof(blk) - sizeof(uint32_t)));
		memcpy(fakeFlashBytes(), &blk, sizeof(blk));
	}
	settingsLoad();
	// This is the one that matters. A migration that guessed at an old layout
	// could hand back a plausible 2000 as easily as a 400, and there is no
	// version of that trade worth taking: losing a pole count costs two taps.
	checkTrue("version bump: not treated as stored", !settingsStored());
	checkTrue("version bump: reset by an update", settingsResetByUpdate());
	checkInt("version bump: ceiling falls back, never carried over",
	         settings()->maxThrottle, def.maxThrottle);

	// --- wrong struct size, right version ---
	fakeFlashClear();
	storeValid(&evil);
	{
		SettingsBlock blk = stored();
		blk.size = (uint16_t)(sizeof(Settings) + 1);
		blk.crc32 = settingsCrc32(&blk, (uint32_t)(sizeof(blk) - sizeof(uint32_t)));
		memcpy(fakeFlashBytes(), &blk, sizeof(blk));
	}
	settingsLoad();
	checkTrue("size mismatch: not treated as stored", !settingsStored());
	checkTrue("size mismatch, this version: not reset by an update", !settingsResetByUpdate());
	checkInt("size mismatch: ceiling falls back",
	         settings()->maxThrottle, def.maxThrottle);

	// --- wrong magic ---
	fakeFlashClear();
	storeValid(&evil);
	fakeFlashBytes()[0] ^= 0xFF;
	settingsLoad();
	checkTrue("bad magic: not treated as stored", !settingsStored());
	checkInt("bad magic: ceiling falls back",
	         settings()->maxThrottle, def.maxThrottle);
}

static void testValidateRepairs() {
	section("Settings: validation clamps everything downstream assumes");

	Settings s;
	settingsDefaults(&s);

	s.poles = 0;
	checkTrue("a zero pole count is repaired", !settingsValidate(&s));
	checkInt("clamped up to the minimum", s.poles, MIN_MOTOR_POLES);

	settingsDefaults(&s);
	s.poles = 13;
	settingsValidate(&s);
	checkInt("odd pole counts are made even", s.poles, 12);

	settingsDefaults(&s);
	s.poles = 200;
	settingsValidate(&s);
	checkInt("pole count clamped down", s.poles, MAX_MOTOR_POLES);

	settingsDefaults(&s);
	s.maxThrottle = 60000;
	settingsValidate(&s);
	checkInt("ceiling clamped to the compiled maximum",
	         s.maxThrottle, MAX_THROTTLE_CEILING);

	settingsDefaults(&s);
	s.maxThrottle = 0;
	settingsValidate(&s);
	checkInt("a zero ceiling becomes one step", s.maxThrottle, MAX_THROTTLE_STEP);

	settingsDefaults(&s);
	s.maxThrottle = 455;
	settingsValidate(&s);
	checkInt("ceiling snapped to a whole step", s.maxThrottle, 400);

	settingsDefaults(&s);
	s.dshotKbaud = 999;
	settingsValidate(&s);
	checkInt("an illegal bitrate falls back", s.dshotKbaud, DSHOT_SPEED_KBAUD);

	settingsDefaults(&s);
	s.backlight = 0;
	settingsValidate(&s);
	checkTrue("backlight never left looking like a dead panel", s.backlight >= 16);
}

/**
 * @brief The pin defaults are the macros'.
 *
 * Tautological at face value, which is the point: CI runs this suite again
 * with the macros overridden, and a descriptor that carried literals instead
 * of reading them passes at the defaults and fails there. A pin option that
 * configures, prints a status line claiming it took, and leaves the image on
 * the literal is the failure this catches. @see cfg_pin_defaults
 */
static void testPinDefaults() {
	section("Settings: the pin defaults are the board macros'");

	checkInt("ESC pin", BOARD_DESC_LCD_2_8.defaultDshotPin, DSHOT_PIN_LCD_2_8);
	checkInt("KISS pin", BOARD_DESC_LCD_2_8.defaultKissPin, KISS_PIN_LCD_2_8);
	checkInt("KISS default",
	         BOARD_DESC_LCD_2_8.defaultKissEnable ? 1 : 0, KISS_ENABLE_LCD_2_8 ? 1 : 0);

	// The board frees two pins, one wire each, and a default that put both on
	// GP0 would describe a wiring nobody can solder.
	checkTrue("the two wires are on different pins",
	          BOARD_DESC_LCD_2_8.defaultDshotPin != BOARD_DESC_LCD_2_8.defaultKissPin);

	// The descriptor static_asserts these too, so a bad override never links --
	// this is the version that says so in the output rather than in a compiler
	// error.
	checkTrue("the ESC default is free on the board",
	          (g_board->freeGpioMask >> g_board->defaultDshotPin) & 1u);
	checkTrue("the KISS default is free on the board",
	          (g_board->freeGpioMask >> g_board->defaultKissPin) & 1u);

	Settings d;
	settingsDefaults(&d);
	checkInt("settingsDefaults() takes the ESC pin from the descriptor",
	         d.dshotPin, g_board->defaultDshotPin);
	checkInt("and the KISS pin", d.kissPin, g_board->defaultKissPin);
	checkInt("and records the board", d.boardId, BOARD_ID_LCD_2_8);
}

static void testPinRules() {
	section("Settings: only pins this board actually offers");

	checkTrue("the default ESC pin is free on this board",
	          settingsPinFree(g_board->defaultDshotPin));
	checkTrue("GP30 is not a pin", !settingsPinFree(30));
	// The signal wires go on GP0 and GP1, the 5 V tolerant pins, and nothing
	// else: GP28 and GP29 are ADC pins that carry only the supply's enable.
	int freeCount = 0;
	for (int p = 0; p <= 29; p++) if (settingsPinFree((uint8_t)p)) freeCount++;
	checkInt("two free pins", freeCount, 2);
	checkTrue("GP0 and GP1", settingsPinFree(0) && settingsPinFree(1));
	checkTrue("not GP28 or GP29", !settingsPinFree(28) && !settingsPinFree(29));

	// Stepping only ever lands somewhere legal, in both directions, for every
	// GPIO -- which is what removes the invalid-selection state entirely.
	//
	// There used to be a second flavour of this, stepping only through the
	// eight GPIOs where n % 4 == 1, because those are the only ones the two
	// hardware UARTs can receive on and the KISS pin had to be one of them.
	// The receiver is a PIO state machine now and samples whatever pin it is
	// given, so the telemetry wire steps through the same free set as the ESC
	// wire and there is one rule here instead of two. @see pio_uart_rx.h
	bool allFree = true;
	for (int p = 0; p <= 29; p++) {
		uint8_t up = settingsNextPin((uint8_t)p, +1);
		uint8_t dn = settingsNextPin((uint8_t)p, -1);
		if (!settingsPinFree(up) || !settingsPinFree(dn)) allFree = false;
	}
	checkTrue("stepping always lands on a free pin", allFree);
	checkInt("and stepping nowhere stays put",
	         settingsNextPin(g_board->defaultDshotPin, 0),
	         g_board->defaultDshotPin);
}

static void testKissNeedsItsOwnPin() {
	section("Settings: AUX moves off SIG; KISS on a pin the board uses is switched off");

	Settings s;
	settingsDefaults(&s);
	s.kissEnable = 1;
	s.kissPin = s.dshotPin;
	checkTrue("AUX sharing SIG's pin is repaired", !settingsValidate(&s));
	checkTrue("AUX moves to a free pin that is not SIG's",
	          s.kissPin != s.dshotPin && settingsPinFree(s.kissPin));
	checkInt("and KISS stays on, listening there", s.kissEnable, 1);

	settingsDefaults(&s);
	s.kissEnable = 0;
	s.kissPin = s.dshotPin;
	checkTrue("with KISS off too: the servo tester's servo B is on AUX", !settingsValidate(&s));
	checkTrue("AUX moves off SIG", s.kissPin != s.dshotPin && settingsPinFree(s.kissPin));
	checkInt("and KISS stays off", s.kissEnable, 0);

	// SIG on the board's default AUX pin: AUX takes the next free pin after
	// SIG, the default being SIG's.
	settingsDefaults(&s);
	s.dshotPin = g_board->defaultKissPin;
	s.kissPin = g_board->defaultKissPin;
	s.kissEnable = 1;
	settingsValidate(&s);
	checkInt("SIG on the default AUX pin: AUX the next free pin after SIG",
	         s.kissPin, settingsNextPin(s.dshotPin, +1));
	checkTrue("which is not SIG's", s.kissPin != s.dshotPin && s.kissEnable == 1);

	settingsDefaults(&s);
	s.kissEnable = 1;
	s.kissPin = 18;              // the LCD's CTS/peripheral territory on both
	settingsValidate(&s);
	checkInt("a pin the board is using turns KISS off", s.kissEnable, 0);
	checkInt("and AUX takes the board's default", s.kissPin, g_board->defaultKissPin);

	settingsDefaults(&s);
	s.kissEnable = 0;
	s.kissPin = 28;
	checkTrue("with KISS off, AUX on a pin the board uses is repaired too", !settingsValidate(&s));
	checkInt("to the board's default", s.kissPin, g_board->defaultKissPin);

	// The rule that used to sit between those two is gone: a KISS pin no longer
	// has to be one of the eight a hardware UART can receive on. That rule is
	// what made KISS impossible to enable on the 2.8" -- two free pins, only
	// one of them a UART RX, and the ESC starting on it -- so every free pin
	// that is not the ESC's is now a legal telemetry pin on every board.
	int legal = 0;
	for (int p = 0; p <= 29 && legal < 4; p++) {
		settingsDefaults(&s);
		if (!settingsPinFree((uint8_t)p) || (uint8_t)p == s.dshotPin) continue;
		s.kissEnable = 1;
		s.kissPin = (uint8_t)p;
		checkTrue("any free pin that is not the ESC's is a legal KISS pin",
		          settingsValidate(&s) && s.kissEnable == 1);
		legal++;
	}
	checkTrue("and this board offers at least one", legal > 0);
}

static void testSaveIsVerified() {
	section("Settings: a save that cannot land reports failure");

	fakeFlashClear();
	settingsLoad();
	settings()->poles = 8;

	fakeFlashSetWritable(false);
	checkTrue("refused write is reported, not swallowed", !settingsSave());
	checkTrue("still dirty afterwards", settingsDirty());
	checkTrue("still no stored block", !settingsStored());

	fakeFlashSetWritable(true);
	checkTrue("succeeds once flash accepts it", settingsSave());
	checkTrue("clean afterwards", !settingsDirty());
}

static void testSaveValidatesFirst() {
	section("Settings: a save can only ever store something loadable");

	fakeFlashClear();
	settingsLoad();
	settings()->poles = 13;               // odd
	settings()->maxThrottle = 60000;      // absurd
	checkTrue("save succeeds", settingsSave());

	settingsLoad();
	checkInt("stored pole count was made even", settings()->poles, 12);
	checkInt("stored ceiling was clamped",
	         settings()->maxThrottle, MAX_THROTTLE_CEILING);
}

static void testCrcKnownAnswer() {
	section("Settings: the CRC is CRC-32, not merely self-consistent");

	// The reflected-0xEDB88320 check value for "123456789". Every other test
	// here only proves the implementation agrees with itself; a wrong init or
	// final XOR would pass all of them and surface later as a forced format
	// break, because fixing the CRC would orphan every stored block in the
	// field.
	checkTrue("CRC-32(\"123456789\") == 0xCBF43926",
	          settingsCrc32("123456789", 9) == 0xCBF43926u);
	checkTrue("CRC-32 of nothing is 0", settingsCrc32("", 0) == 0u);
}

static void testForeignBoardBlock() {
	section("Settings: a valid block naming a board this image is not");

	// Magic, version, size and CRC all correct -- only the *content* is
	// foreign: a board id no build recognises, carrying a pin that every
	// board's mask refuses. The load-path validate call is the sole interlock
	// between this block and the pump being seeded with an occupied GPIO;
	// delete it and these checks are what fails.
	Settings evil;
	settingsDefaults(&evil);
	evil.boardId = 99;
	evil.dshotPin = 18;                    // LCD clock / touch INT territory
	fakeFlashClear();
	storeValid(&evil);
	settingsLoad();
	checkInt("the alien board id is repaired to the 2.8\"'s",
	         settings()->boardId, BOARD_ID_LCD_2_8);
	checkTrue("the ESC pin was re-judged against this board",
	          settingsPinFree(settings()->dshotPin));
	checkInt("and fell back to this board's default",
	         settings()->dshotPin, g_board->defaultDshotPin);
}

static void testNoOpSaveWritesNothing() {
	section("Settings: saving what is already stored writes nothing");

	fakeFlashClear();
	settingsLoad();
	settings()->poles = 10;
	checkTrue("the first save lands", settingsSave());

	// On the device every write attempt is an erase first: wear, core1 parked,
	// and a torn-write window in the only copy of the block. A save of exactly
	// what is stored must therefore not touch flash at all.
	int before = fakeFlashWrites();
	checkTrue("saving again unchanged still reports success", settingsSave());
	checkInt("but attempts no write", fakeFlashWrites() - before, 0);

	settings()->poles = 12;
	checkTrue("a real change saves again", settingsSave());
	checkInt("with exactly one write", fakeFlashWrites() - before, 1);
}

static void testBlankNamesTheBoard() {
	section("Settings: blank flash names the board");

	fakeFlashClear();
	settingsLoad();
	checkInt("the board id is the 2.8\"'s", settings()->boardId, BOARD_ID_LCD_2_8);
	checkTrue("and the default ESC pin is free on it",
	          settingsPinFree(settings()->dshotPin));
}

/**
 * @brief The block layout DshotDisplay 2.x writes, pinned.
 *
 * A block DshotDisplay saved on the same board must load here unchanged, so
 * the magic, the version and every field offset are part of the format. A
 * change to any of them belongs with a new @ref SETTINGS_VERSION, and the
 * DshotDisplay block then stops loading on purpose rather than by accident.
 */
static void testLayout() {
	section("Settings: the version 5 block layout");

	checkTrue("magic \"DSHT\"", SETTINGS_MAGIC == 0x44534854u);
	checkInt("version", SETTINGS_VERSION, 5);
	checkInt("payload size", (long)sizeof(Settings), 14);
	checkInt("boardId at 0", (long)offsetof(Settings, boardId), 0);
	checkInt("dshotPin at 1", (long)offsetof(Settings, dshotPin), 1);
	checkInt("dshotKbaud at 2", (long)offsetof(Settings, dshotKbaud), 2);
	checkInt("kissEnable at 4", (long)offsetof(Settings, kissEnable), 4);
	checkInt("kissPin at 5", (long)offsetof(Settings, kissPin), 5);
	checkInt("poles at 6", (long)offsetof(Settings, poles), 6);
	checkInt("maxThrottle at 8", (long)offsetof(Settings, maxThrottle), 8);
	checkInt("backlight at 10", (long)offsetof(Settings, backlight), 10);
	checkInt("highContrast at 11", (long)offsetof(Settings, highContrast), 11);
	checkInt("servoSupply at 12", (long)offsetof(Settings, servoSupply), 12);
	checkInt("currentSense at 13, the byte that was padding",
	         (long)offsetof(Settings, currentSense), 13);
	checkInt("block size", (long)sizeof(SettingsBlock), 28);

	// A block with every field off its default, the switch fitted on GP29 and
	// the sensor declared, loads as it was stored.
	Settings v4;
	settingsDefaults(&v4);
	v4.currentSense = 1;
	v4.dshotPin     = 1;
	v4.kissEnable   = 1;
	v4.kissPin      = 0;
	v4.dshotKbaud   = 300;
	v4.poles        = 12;
	v4.maxThrottle  = 800;
	v4.backlight    = 96;
	v4.highContrast = 1;
	v4.servoSupply  = 2;
	fakeFlashClear();
	storeValid(&v4);
	settingsLoad();
	checkTrue("the block is taken", settingsStored());
	checkTrue("every field as stored", memcmp(settings(), &v4, sizeof(v4)) == 0);
	checkTrue("with nothing to save", !settingsDirty());

	// A block of version 6, which this firmware has no layout for: the same
	// size and a valid CRC, discarded whole for its version.
	SettingsBlock old;
	memset(&old, 0, sizeof(old));
	old.magic = SETTINGS_MAGIC;
	old.version = SETTINGS_VERSION + 1;
	old.size = sizeof(Settings);
	old.s = v4;
	old.crc32 = settingsCrc32(&old, offsetof(SettingsBlock, crc32));
	fakeFlashClear();
	memcpy(fakeFlashBytes(), &old, sizeof(old));
	settingsLoad();
	Settings fresh;
	settingsDefaults(&fresh);
	checkTrue("a version 6 block is discarded", !settingsStored() && !settingsCarriedOver());
	checkTrue("as reset by an update", settingsResetByUpdate());
	checkTrue("and the defaults load, every setting",
	          memcmp(settings(), &fresh, sizeof(fresh)) == 0);
	// Version 1, before the board id: no layout here either.
	old.version = 1;
	old.crc32 = settingsCrc32(&old, offsetof(SettingsBlock, crc32));
	fakeFlashClear();
	memcpy(fakeFlashBytes(), &old, sizeof(old));
	settingsLoad();
	checkTrue("a version 1 block: reset by an update, not carried over",
	          !settingsStored() && settingsResetByUpdate() && !settingsCarriedOver() &&
	          memcmp(settings(), &fresh, sizeof(fresh)) == 0);
	old.version = SETTINGS_VERSION + 1;
	old.crc32 = settingsCrc32(&old, offsetof(SettingsBlock, crc32));

	// A block of version 2 with 13 bytes of payload, which no version 2
	// firmware wrote, its CRC right at byte 24: not carried over, a reset.
	uint8_t v2[28];
	memset(v2, 0, sizeof(v2));
	const uint32_t magic = SETTINGS_MAGIC;
	const uint16_t version = 2, size = 13;
	memcpy(v2, &magic, 4);
	memcpy(v2 + 4, &version, 2);
	memcpy(v2 + 6, &size, 2);
	memcpy(v2 + 8, &v4, 13);
	const uint32_t crc = settingsCrc32(v2, 24);
	memcpy(v2 + 24, &crc, 4);
	fakeFlashClear();
	memcpy(fakeFlashBytes(), v2, sizeof(v2));
	settingsLoad();
	Settings d;
	settingsDefaults(&d);
	checkTrue("version 2, 13 bytes: not carried over", !settingsStored() && !settingsCarriedOver());
	checkTrue("a reset by an update: its CRC over its 13 bytes", settingsResetByUpdate());
	// The same block with one payload byte flipped: damaged, not a reset.
	fakeFlashBytes()[9] ^= 0x01;
	settingsLoad();
	checkTrue("with a bad CRC: not reset by an update",
	          !settingsStored() && !settingsResetByUpdate());
	// A size past SETTINGS_OTHER_MAX is not judged at all.
	{
		// The CRC sits at the next 4-byte boundary after the payload.
		uint8_t big[8 + SETTINGS_OTHER_MAX + 4 + 4];
		memset(big, 0, sizeof(big));
		const uint16_t bigSize = SETTINGS_OTHER_MAX + 1;
		const uint32_t bigAt = (8u + bigSize + 3u) & ~3u;
		memcpy(big, &magic, 4);
		memcpy(big + 4, &version, 2);
		memcpy(big + 6, &bigSize, 2);
		const uint32_t bigCrc = settingsCrc32(big, bigAt);
		memcpy(big + bigAt, &bigCrc, 4);
		fakeFlashClear();
		memcpy(fakeFlashBytes(), big, bigAt + 4);
		settingsLoad();
		checkTrue("a payload of SETTINGS_OTHER_MAX + 1 bytes: not judged, not a reset",
		          !settingsStored() && !settingsResetByUpdate());
		const uint16_t maxSize = SETTINGS_OTHER_MAX;
		const uint32_t maxAt = (8u + maxSize + 3u) & ~3u;
		memset(big + 8, 0, sizeof(big) - 8);
		memcpy(big + 6, &maxSize, 2);
		const uint32_t maxCrc = settingsCrc32(big, maxAt);
		memcpy(big + maxAt, &maxCrc, 4);
		fakeFlashClear();
		memcpy(fakeFlashBytes(), big, maxAt + 4);
		settingsLoad();
		checkTrue("one of SETTINGS_OTHER_MAX bytes: a reset", settingsResetByUpdate());
		// A version 5 block of an odd payload size, its CRC 2 bytes after it.
		uint8_t odd[8 + 14 + 2 + 4];
		memset(odd, 0, sizeof(odd));
		const uint16_t v6 = SETTINGS_VERSION + 1, oddSize = 14;
		memcpy(odd, &magic, 4);
		memcpy(odd + 4, &v6, 2);
		memcpy(odd + 6, &oddSize, 2);
		const uint32_t oddCrc = settingsCrc32(odd, 24);
		memcpy(odd + 24, &oddCrc, 4);
		fakeFlashClear();
		memcpy(fakeFlashBytes(), odd, sizeof(odd));
		settingsLoad();
		checkTrue("14 bytes of payload: the CRC at byte 24, a reset", settingsResetByUpdate());
		memcpy(fakeFlashBytes() + 22, &oddCrc, 4);
		settingsLoad();
		checkTrue("the CRC right after the payload, at byte 22: not judged whole",
		          !settingsResetByUpdate());
	}
	// Blank flash: a board that never had settings.
	fakeFlashClear();
	settingsLoad();
	checkTrue("blank flash: not reset by an update", !settingsStored() && !settingsResetByUpdate());
	// A save that lands ends it; one the flash refuses does not.
	fakeFlashClear();
	memcpy(fakeFlashBytes(), &old, sizeof(old));
	settingsLoad();
	checkTrue("version 6 again: reset by an update", settingsResetByUpdate());
	fakeFlashSetWritable(false);
	checkTrue("a save the flash refuses fails", !settingsSave());
	checkTrue("and the reset still shows", settingsResetByUpdate());
	fakeFlashSetWritable(true);
	checkTrue("a save that lands", settingsSave());
	checkTrue("ends it", !settingsResetByUpdate());
	settingsLoad();
	checkTrue("and the next boot loads its own block", settingsStored() && !settingsResetByUpdate());
	checkTrue("and the compiled defaults load, the ESC pin among them",
	          memcmp(settings(), &d, sizeof(d)) == 0);
	checkInt("the switch: none", settings()->servoSupply, 0);
}

/**
 * @brief The fields of a version 2, 3 or 4 block, written byte by byte at
 *        the offsets those firmwares used. Recovered from git, not from
 *        settings.cpp's table: `struct Settings` in src/settings.h at
 *        8a5eebb (version 2, 12 bytes), 5af4ddf (3, 14 bytes) and 3cc8798
 *        (4, 14 bytes), each with the 8-byte header before it and the
 *        CRC-32 at the next 4-byte boundary after it: byte 20, 24 and 24.
 */
struct OldBlock {
	uint16_t version;
	uint8_t  boardId, dshotPin;
	uint16_t dshotKbaud;
	uint8_t  kissEnable, kissPin, poles;
	uint16_t maxThrottle;
	uint8_t  backlight, highContrast;
	uint8_t  servoSupply;  /**< Versions 3 and 4. */
	uint8_t  pad;          /**< Every padding byte: byte 15, and 21 to 23 in versions 3 and 4. */
};

/** @brief @p b as its firmware wrote it, into the fake flash. @return Its length. */
static uint32_t putOld(const OldBlock &b) {
	uint8_t raw[28];
	memset(raw, 0xEE, sizeof(raw));
	const uint32_t payload = b.version == 2 ? 12 : 14, crcAt = b.version == 2 ? 20 : 24;
	raw[0] = 0x54; raw[1] = 0x48; raw[2] = 0x53; raw[3] = 0x44;   // "DSHT", little-endian
	raw[4] = (uint8_t)b.version; raw[5] = (uint8_t)(b.version >> 8);
	raw[6] = (uint8_t)payload;   raw[7] = 0;
	raw[8]  = b.boardId;
	raw[9]  = b.dshotPin;
	raw[10] = (uint8_t)b.dshotKbaud; raw[11] = (uint8_t)(b.dshotKbaud >> 8);
	raw[12] = b.kissEnable;
	raw[13] = b.kissPin;
	raw[14] = b.poles;
	raw[15] = b.pad;
	raw[16] = (uint8_t)b.maxThrottle; raw[17] = (uint8_t)(b.maxThrottle >> 8);
	raw[18] = b.backlight;
	raw[19] = b.highContrast;
	if (b.version != 2) {
		raw[20] = b.servoSupply;
		raw[21] = b.pad;
		raw[22] = b.pad; raw[23] = b.pad;
	}
	const uint32_t crc = settingsCrc32(raw, crcAt);
	raw[crcAt] = (uint8_t)crc; raw[crcAt + 1] = (uint8_t)(crc >> 8);
	raw[crcAt + 2] = (uint8_t)(crc >> 16); raw[crcAt + 3] = (uint8_t)(crc >> 24);
	fakeFlashClear();
	memcpy(fakeFlashBytes(), raw, crcAt + 4);
	return crcAt + 4;
}

/** @brief Whether a load carried a block over: stored, the notice set, no reset. */
static bool carried() {
	return settingsStored() && settingsCarriedOver() && !settingsResetByUpdate() && !settingsDirty();
}

/**
 * @brief Blocks of versions 2, 3 and 4 carry over: every field they have,
 *        validated; every newer field at its default.
 */
static void testCarryOver() {
	section("Settings: a version 2, 3 or 4 block carries over");
	Settings def;
	settingsDefaults(&def);

	// --- version 4: every field off its default, the padding byte where
	//     version 5 keeps CURRENT SENSE set ---
	OldBlock b4 = { 4, BOARD_ID_LCD_2_8, 1, 1200, 1, 0, 14, 1200, 200, 1, 2, 0x01 };
	checkInt("a version 4 block is 28 bytes", (long)putOld(b4), 28);
	settingsLoad();
	checkTrue("version 4: carried over, stored, no reset, nothing to save", carried());
	checkInt("ESC pin", settings()->dshotPin, 1);
	checkInt("DShot kbaud", settings()->dshotKbaud, 1200);
	checkInt("KISS on", settings()->kissEnable, 1);
	checkInt("KISS pin", settings()->kissPin, 0);
	checkInt("poles", settings()->poles, 14);
	checkInt("ceiling", settings()->maxThrottle, 1200);
	checkInt("backlight", settings()->backlight, 200);
	checkInt("high contrast", settings()->highContrast, 1);
	checkInt("servo supply GP29", settings()->servoSupply, 2);
	checkInt("board", settings()->boardId, BOARD_ID_LCD_2_8);
	checkInt("current sensor: its default, not the padding byte", settings()->currentSense, 0);
	checkInt("the boot pin is the carried one", settingsBootDshotPin(), 1);
	{
		OldBlock g = b4;
		g.servoSupply = 1;
		putOld(g);
		settingsLoad();
		checkTrue("version 4's servo supply 1, GP28, carries as 1", carried() && settings()->servoSupply == 1);
	}

	// --- version 3: the pins of its wiring, which this board does not free ---
	OldBlock b3 = { 3, BOARD_ID_LCD_2_8, 29, 300, 1, 28, 10, 600, 64, 1, 1, 0xA5 };
	checkInt("a version 3 block is 28 bytes", (long)putOld(b3), 28);
	settingsLoad();
	checkTrue("version 3: carried over", carried());
	checkInt("kbaud", settings()->dshotKbaud, 300);
	checkInt("poles", settings()->poles, 10);
	checkInt("ceiling", settings()->maxThrottle, 600);
	checkInt("backlight", settings()->backlight, 64);
	checkInt("contrast", settings()->highContrast, 1);
	checkInt("ESC pin GP29: not free here, the default", settings()->dshotPin, def.dshotPin);
	checkInt("KISS on GP28: off (KISS OFF)", settings()->kissEnable, 0);
	checkInt("and its pin the board's default, a free one", settings()->kissPin, def.kissPin);
	checkInt("servo supply 1, an enable on GP0: none", settings()->servoSupply, 0);
	checkInt("current sensor: the default", settings()->currentSense, 0);
	{
		OldBlock n = b3;
		n.servoSupply = 0;
		n.kissEnable = 0;
		putOld(n);
		settingsLoad();
		checkTrue("version 3, no supply and KISS off: none, off, the default KISS pin",
		          carried() && settings()->servoSupply == 0 && settings()->kissEnable == 0 &&
		          settings()->kissPin == def.kissPin);
	}

	// --- version 2: 12 bytes, no servo supply ---
	OldBlock b2 = { 2, BOARD_ID_LCD_2_8, 1, 600, 0, 0, 12, 800, 96, 1, 0x77, 0x5A };
	checkInt("a version 2 block is 24 bytes", (long)putOld(b2), 24);
	settingsLoad();
	checkTrue("version 2: carried over", carried());
	checkInt("ESC pin", settings()->dshotPin, 1);
	checkInt("kbaud", settings()->dshotKbaud, 600);
	checkInt("KISS off", settings()->kissEnable, 0);
	checkInt("KISS pin GP0, free: kept", settings()->kissPin, 0);
	checkInt("poles", settings()->poles, 12);
	checkInt("ceiling", settings()->maxThrottle, 800);
	checkInt("backlight", settings()->backlight, 96);
	checkInt("contrast", settings()->highContrast, 1);
	checkInt("servo supply: the default, none", settings()->servoSupply, def.servoSupply);
	checkInt("current sensor: the default", settings()->currentSense, def.currentSense);
	// Byte 20, where versions 3 and 4 keep the servo supply, is the low byte
	// of version 2's CRC. With it 1 or 2, a legal supply, the supply still
	// loads as none: version 2 has no such field.
	for (uint8_t want = 1; want <= 2; want++) {
		OldBlock o = b2;
		bool found = false;
		// The padding byte and the backlight vary the CRC.
		for (int i = 0; i < 256 * 64 && !found; i++) {
			o.pad = (uint8_t)i;
			o.backlight = (uint8_t)(96 + i / 256);
			putOld(o);
			found = fakeFlashBytes()[20] == want;
		}
		char what[80];
		snprintf(what, sizeof(what), "version 2, its CRC's low byte %u: found", (unsigned)want);
		checkTrue(what, found);
		settingsLoad();
		snprintf(what, sizeof(what), "byte 20 = %u: carried over, servo supply none", (unsigned)want);
		checkTrue(what, carried() && settings()->servoSupply == 0 && settings()->poles == 12);
	}
	{
		// The board of version 2's two-board image, 2 for the LCD-2: the
		// board-id rule, as for a version 5 block.
		OldBlock o = b2;
		o.boardId = 2;
		putOld(o);
		settingsLoad();
		checkTrue("version 2 naming another board: carried, the id this board's",
		          carried() && settings()->boardId == BOARD_ID_LCD_2_8 && settings()->poles == 12);
	}

	// --- validation, as for a version 5 block ---
	{
		// KISS on the board's KISS pin, free and never the ESC's default
		// (the build refuses both on one pin), whatever the pin macros say.
		OldBlock v = { 4, 99, 18, 999, 1, def.kissPin, 13, 60000, 3, 5, 7, 0 };
		putOld(v);
		settingsLoad();
		checkTrue("out of range: carried over", carried());
		checkInt("a ceiling of 60000: the compiled ceiling", settings()->maxThrottle, MAX_THROTTLE_CEILING);
		checkInt("13 poles: 12", settings()->poles, 12);
		checkInt("999 kbaud: the default", settings()->dshotKbaud, DSHOT_SPEED_KBAUD);
		checkInt("backlight 3: the floor", settings()->backlight, BACKLIGHT_MIN);
		checkInt("contrast 5: 1", settings()->highContrast, 1);
		checkInt("servo supply 7: none", settings()->servoSupply, 0);
		checkInt("board 99: this board", settings()->boardId, BOARD_ID_LCD_2_8);
		checkInt("ESC pin 18, the panel's: the default", settings()->dshotPin, def.dshotPin);
		checkInt("KISS on its default pin, free and not the ESC's: on", settings()->kissEnable, 1);
		v.maxThrottle = 455;
		v.poles = 0;
		putOld(v);
		settingsLoad();
		checkInt("455: a whole step, 400", settings()->maxThrottle, 400);
		checkInt("0 poles: the minimum", settings()->poles, MIN_MOTOR_POLES);
		OldBlock k = b4;
		k.dshotPin = 0;
		k.kissPin = 0;
		putOld(k);
		settingsLoad();
		checkTrue("KISS on the ESC's pin GP0: AUX moves to GP1, KISS on, the ESC on GP0",
		          carried() && settings()->kissEnable == 1 && settings()->dshotPin == 0 &&
		          settings()->kissPin == 1);
	}

	// --- a damaged block: discarded as before, no notice either way ---
	static const OldBlock ALL[] = { b2, b3, b4 };
	for (const OldBlock &o : ALL) {
		char what[80];
		const uint32_t len = putOld(o);
		fakeFlashBytes()[len - 1] ^= 0x01;
		settingsLoad();
		snprintf(what, sizeof(what), "version %u, its CRC damaged: discarded, the defaults, no notice",
		         (unsigned)o.version);
		checkTrue(what, !settingsStored() && !settingsCarriedOver() && !settingsResetByUpdate() &&
		                memcmp(settings(), &def, sizeof(def)) == 0);
		putOld(o);
		fakeFlashBytes()[16] ^= 0x04;
		settingsLoad();
		snprintf(what, sizeof(what), "version %u, a payload byte damaged: discarded", (unsigned)o.version);
		checkTrue(what, !settingsStored() && !settingsCarriedOver() && !settingsResetByUpdate());
		putOld(o);
		fakeFlashBytes()[0] ^= 0xFF;
		settingsLoad();
		snprintf(what, sizeof(what), "version %u, its magic damaged: discarded", (unsigned)o.version);
		checkTrue(what, !settingsStored() && !settingsCarriedOver() && !settingsResetByUpdate());
	}
	// A version 3 block that says 12 bytes, its CRC right at byte 20: not
	// version 3's layout. Whole, of another version: the reset path.
	{
		putOld(b2);
		fakeFlashBytes()[4] = 3;
		const uint32_t crc = settingsCrc32(fakeFlashBytes(), 20);
		memcpy(fakeFlashBytes() + 20, &crc, 4);
		settingsLoad();
		checkTrue("version 3 with 12 bytes: not carried over, reset by an update",
		          !settingsStored() && !settingsCarriedOver() && settingsResetByUpdate());
		putOld(b3);
		fakeFlashBytes()[4] = 2;
		const uint32_t crc2 = settingsCrc32(fakeFlashBytes(), 24);
		memcpy(fakeFlashBytes() + 24, &crc2, 4);
		settingsLoad();
		checkTrue("version 2 with 14 bytes: not carried over, reset by an update",
		          !settingsStored() && !settingsCarriedOver() && settingsResetByUpdate());
		// The size field alone wrong, the CRC right where the version's own
		// layout has it: only the size check tells these from the real ones.
		putOld(b4);
		fakeFlashBytes()[6] = 13;
		const uint32_t crc3 = settingsCrc32(fakeFlashBytes(), 24);
		memcpy(fakeFlashBytes() + 24, &crc3, 4);
		settingsLoad();
		checkTrue("version 4 saying 13 bytes, its CRC right at byte 24: not carried over, a reset",
		          !settingsStored() && !settingsCarriedOver() && settingsResetByUpdate());
		putOld(b2);
		fakeFlashBytes()[6] = 14;
		const uint32_t crc4 = settingsCrc32(fakeFlashBytes(), 20);
		memcpy(fakeFlashBytes() + 20, &crc4, 4);
		settingsLoad();
		checkTrue("version 2 saying 14 bytes, its CRC right at byte 20: not carried over",
		          !settingsStored() && !settingsCarriedOver());
		// Another magic, the CRC right over it: not a settings block at all.
		putOld(b4);
		fakeFlashBytes()[3] = 0x45;
		const uint32_t crc5 = settingsCrc32(fakeFlashBytes(), 24);
		memcpy(fakeFlashBytes() + 24, &crc5, 4);
		settingsLoad();
		checkTrue("version 4 under another magic, its CRC right: discarded, no notice",
		          !settingsStored() && !settingsCarriedOver() && !settingsResetByUpdate());
	}

	// --- flash is not rewritten; every boot carries the block over again ---
	putOld(b4);
	uint8_t before[28];
	memcpy(before, fakeFlashBytes(), sizeof(before));
	int writes = fakeFlashWrites();
	settingsLoad();
	settingsLoad();
	checkTrue("two boots: carried over each time, the same settings",
	          carried() && settings()->poles == 14 && settings()->servoSupply == 2);
	checkTrue("and flash untouched", fakeFlashWrites() == writes &&
	                                 memcmp(before, fakeFlashBytes(), sizeof(before)) == 0);

	// --- a change marks it dirty; stepping back clears it ---
	settings()->poles = 16;
	checkTrue("a change: dirty, still carried over", settingsDirty() && settingsCarriedOver());
	settings()->poles = 14;
	checkTrue("stepped back: clean", !settingsDirty());

	// --- a refused save keeps the notice ---
	fakeFlashSetWritable(false);
	checkTrue("a save the flash refuses fails", !settingsSave());
	checkTrue("and the carry-over still shows", settingsCarriedOver());
	fakeFlashSetWritable(true);

	// --- a save with nothing changed writes a version 5 block ---
	writes = fakeFlashWrites();
	checkTrue("a save with nothing changed lands", settingsSave());
	checkInt("with one write", fakeFlashWrites() - writes, 1);
	checkTrue("and ends the notice", !settingsCarriedOver() && settingsStored() && !settingsDirty());
	SettingsBlock nb = stored();
	checkTrue("flash: a version 5 block of 14 bytes, its CRC right",
	          nb.magic == SETTINGS_MAGIC && nb.version == SETTINGS_VERSION && nb.size == sizeof(Settings) &&
	          nb.crc32 == settingsCrc32(&nb, offsetof(SettingsBlock, crc32)));
	checkTrue("holding the carried settings", nb.s.poles == 14 && nb.s.maxThrottle == 1200 &&
	          nb.s.servoSupply == 2 && nb.s.currentSense == 0 && nb.s.dshotPin == 1);
	Settings live = *settings();
	writes = fakeFlashWrites();
	checkTrue("a second save writes nothing", settingsSave() && fakeFlashWrites() == writes);
	settingsLoad();
	checkTrue("the next boot: its own block, no notice",
	          settingsStored() && !settingsCarriedOver() && !settingsResetByUpdate() &&
	          memcmp(settings(), &live, sizeof(live)) == 0);

	// --- version 3 saved: the supply stays none, the pins this board's ---
	putOld(b3);
	settingsLoad();
	checkTrue("version 3 saved", settingsSave());
	settingsLoad();
	checkTrue("loads as saved: ESC and KISS pins free, no supply, no notice",
	          !settingsCarriedOver() && settingsPinFree(settings()->dshotPin) &&
	          settingsPinFree(settings()->kissPin) && settings()->servoSupply == 0 &&
	          settings()->poles == 10);
}

/** @brief The servo supply: 0 to 2 pass; anything else reads as none. */
static void testServoSupplyClamp() {
	section("Settings: the servo supply clamp");
	Settings s;
	settingsDefaults(&s);
	checkInt("default: none", s.servoSupply, 0);
	s.servoSupply = 1;
	checkTrue("GP28 passes", settingsValidate(&s) && s.servoSupply == 1);
	s.servoSupply = 2;
	checkTrue("GP29 passes", settingsValidate(&s) && s.servoSupply == 2);
	s.servoSupply = 3;
	checkTrue("3 is repaired", !settingsValidate(&s));
	checkInt("to none", s.servoSupply, 0);
	s.servoSupply = 255;
	settingsValidate(&s);
	checkInt("255 to none", s.servoSupply, 0);
}

/** @brief The current sensor: 0 and 1 pass; anything else reads as none. */
static void testCurrentSenseClamp() {
	section("Settings: the current sensor clamp");
	Settings s;
	settingsDefaults(&s);
	checkInt("default: none", s.currentSense, 0);
	s.currentSense = 1;
	checkTrue("INA219 passes", settingsValidate(&s) && s.currentSense == 1);
	s.currentSense = 2;
	checkTrue("2 is repaired", !settingsValidate(&s));
	checkInt("to none", s.currentSense, 0);
	s.currentSense = 255;
	settingsValidate(&s);
	checkInt("255 to none", s.currentSense, 0);
}

void runSettingsTests() {
	testBlankFlash();
	testRoundTrip();
	testRejectsCorruption();
	testValidateRepairs();
	testPinDefaults();
	testPinRules();
	testKissNeedsItsOwnPin();
	testSaveIsVerified();
	testSaveValidatesFirst();
	testCrcKnownAnswer();
	testForeignBoardBlock();
	testNoOpSaveWritesNothing();
	testBlankNamesTheBoard();
	testLayout();
	testCarryOver();
	testServoSupplyClamp();
	testCurrentSenseClamp();

	// Leave the module in a known state: test_ui.cpp runs after this and reads
	// settings() for the pole count and throttle ceiling.
	fakeFlashClear();
	settingsLoad();
}
