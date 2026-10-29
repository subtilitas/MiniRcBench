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
	section("Settings: KISS is switched off, never quietly moved");

	Settings s;
	settingsDefaults(&s);
	s.kissEnable = 1;
	s.kissPin = s.dshotPin;
	checkTrue("sharing a pin with the ESC is repaired", !settingsValidate(&s));
	// Moving it would start listening on a pin the user never connected, and a
	// telemetry row reading ON against the wrong pin is worse than one reading
	// OFF.
	checkInt("KISS switched off rather than relocated", s.kissEnable, 0);

	settingsDefaults(&s);
	s.kissEnable = 1;
	s.kissPin = 18;              // the LCD's CTS/peripheral territory on both
	settingsValidate(&s);
	checkInt("a pin the board is using turns KISS off", s.kissEnable, 0);

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
	section("Settings: the version 3 block layout");

	checkTrue("magic \"DSHT\"", SETTINGS_MAGIC == 0x44534854u);
	checkInt("version", SETTINGS_VERSION, 4);
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
	checkInt("block size", (long)sizeof(SettingsBlock), 28);

	// A block with every field off its default, the switch fitted on GP29,
	// loads as it was stored.
	Settings v3;
	settingsDefaults(&v3);
	v3.dshotPin     = 1;
	v3.kissEnable   = 1;
	v3.kissPin      = 0;
	v3.dshotKbaud   = 300;
	v3.poles        = 12;
	v3.maxThrottle  = 800;
	v3.backlight    = 96;
	v3.highContrast = 1;
	v3.servoSupply  = 2;
	fakeFlashClear();
	storeValid(&v3);
	settingsLoad();
	checkTrue("the block is taken", settingsStored());
	checkTrue("every field as stored", memcmp(settings(), &v3, sizeof(v3)) == 0);
	checkTrue("with nothing to save", !settingsDirty());

	// A version 3 block, its ESC pin on GP29 and its switch on GP0: the same
	// layout and a valid CRC, discarded whole for its version.
	SettingsBlock old;
	memset(&old, 0, sizeof(old));
	old.magic = SETTINGS_MAGIC;
	old.version = 3;
	old.size = sizeof(Settings);
	old.s = v3;
	old.s.dshotPin = 29;
	old.s.kissPin = 28;
	old.s.servoSupply = 1;
	old.crc32 = settingsCrc32(&old, offsetof(SettingsBlock, crc32));
	fakeFlashClear();
	memcpy(fakeFlashBytes(), &old, sizeof(old));
	settingsLoad();
	Settings fresh;
	settingsDefaults(&fresh);
	checkTrue("a version 3 block is discarded", !settingsStored());
	checkTrue("and the defaults load: the ESC on the board's default, no switch",
	          memcmp(settings(), &fresh, sizeof(fresh)) == 0 &&
	          settings()->dshotPin == g_board->defaultDshotPin && settings()->servoSupply == 0);

	// A version 2 block, as DshotDisplay 2.x and the builds before the
	// switch wrote it: 12 bytes of payload, its CRC right. Discarded whole.
	uint8_t v2[24];
	memset(v2, 0, sizeof(v2));
	const uint32_t magic = SETTINGS_MAGIC;
	const uint16_t version = 2, size = 12;
	memcpy(v2, &magic, 4);
	memcpy(v2 + 4, &version, 2);
	memcpy(v2 + 6, &size, 2);
	memcpy(v2 + 8, &v3, 12);
	const uint32_t crc = settingsCrc32(v2, 20);
	memcpy(v2 + 20, &crc, 4);
	fakeFlashClear();
	memcpy(fakeFlashBytes(), v2, sizeof(v2));
	settingsLoad();
	Settings d;
	settingsDefaults(&d);
	checkTrue("a version 2 block is discarded", !settingsStored());
	checkTrue("and the compiled defaults load, the ESC pin among them",
	          memcmp(settings(), &d, sizeof(d)) == 0);
	checkInt("the switch: none", settings()->servoSupply, 0);
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
	testServoSupplyClamp();

	// Leave the module in a known state: test_ui.cpp runs after this and reads
	// settings() for the pole count and throttle ceiling.
	fakeFlashClear();
	settingsLoad();
}
