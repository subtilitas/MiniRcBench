/**
 * @file settings.cpp
 * @brief Settings defaults, validation, and the load/save rules.
 *
 * No hardware here on purpose: the flash sequence lives in settings_flash.cpp
 * behind @ref settingsStorageRead() / @ref settingsStorageWrite(), so
 * everything in this file is compiled into the host suite and exercised
 * directly. The rules that keep a corrupted block from becoming a live throttle
 * ceiling are exactly the rules worth testing.
 */

#include "settings.h"
#include "servo_supply.h"
#include "sense.h"
#include "config.h"
#include "board_desc.h"
#include "pin_claim.h"

#include <stddef.h>
#include <string.h>

/**
 * @brief The working copy, seeded with the compiled defaults on first use.
 *
 * Seeded lazily rather than at static-init time: computing the defaults reads
 * `g_board`'s pointee, and the descriptor lives in another translation unit.
 * On the device it is constant-initialised, but the host build gives it a
 * dynamic initialiser, so a static `defaultsValue()` here would read it in
 * whatever order the linker chose. First-use seeding runs after *all* static
 * init, on either toolchain.
 *
 * The original goal is kept: nothing that calls settings() before
 * settingsLoad() ever sees a zero-filled Settings — a pole count of 0 and a
 * throttle ceiling of 0 — instead of a diagnosable default.
 */
static Settings s_live;
static Settings s_saved;
static bool     s_stored = false;
/** @brief The last load discarded a whole block of another version. @see settingsResetByUpdate() */
static bool     s_resetByUpdate = false;
/** @brief The last load carried an older block over. @see settingsCarriedOver() */
static bool     s_carriedOver = false;
/** @brief The ESC pin at the last load. @see settingsBootDshotPin() */
static uint8_t  s_bootPin = 0;
static bool     s_seeded = false;

static void ensureSeeded() {
	if (s_seeded) return;
	s_seeded = true;
	settingsDefaults(&s_live);
	s_bootPin = s_live.dshotPin;
	s_saved = s_live;
}

// ---------------------------------------------------------------------------
// pin rules
// ---------------------------------------------------------------------------

bool settingsPinFree(uint8_t pin) {
	if (pin > 29) return false;
	return (g_board->freeGpioMask >> pin) & 1u;
}

uint8_t settingsNextPin(uint8_t from, int dir) {
	if (dir == 0) return from;
	int step = dir > 0 ? 1 : -1;
	int p = (int)from;
	// 30 hops covers every GPIO once, so a board offering no other candidate
	// returns the pin it was given rather than spinning.
	for (int i = 0; i < 30; i++) {
		p += step;
		if (p > 29) p = 0;
		if (p < 0) p = 29;
		if (settingsPinFree((uint8_t)p)) return (uint8_t)p;
	}
	return from;
}

// ---------------------------------------------------------------------------
// defaults and validation
// ---------------------------------------------------------------------------

void settingsDefaults(Settings *out) {
	memset(out, 0, sizeof(*out));
	// Pin defaults come from the descriptor, which reads them from config.h.
	out->boardId      = BOARD_ID_LCD_2_8;
	out->dshotPin     = g_board->defaultDshotPin;
	out->dshotKbaud   = DSHOT_SPEED_KBAUD;
	out->kissEnable   = g_board->defaultKissEnable ? 1 : 0;
	out->kissPin      = g_board->defaultKissPin;
	out->poles        = DEFAULT_MOTOR_POLES;
	out->maxThrottle  = DEFAULT_MAX_THROTTLE;
	out->backlight    = LCD_BACKLIGHT_DEFAULT;
	out->highContrast = 0;
	out->servoSupply  = 0;
	out->currentSense = (uint8_t)SenseMode::None;
}

/** @brief The bitrates the DShot driver is built and tested for. */
static const uint16_t LEGAL_KBAUD[4] = { 150, 300, 600, 1200 };

bool settingsValidate(Settings *s) {
	bool ok = true;

	// --- motor poles: even, and in range ---
	if (s->poles < MIN_MOTOR_POLES) { s->poles = MIN_MOTOR_POLES; ok = false; }
	if (s->poles > MAX_MOTOR_POLES) { s->poles = MAX_MOTOR_POLES; ok = false; }
	if (s->poles & 1u) { s->poles = (uint8_t)(s->poles - 1); ok = false; }

	// --- throttle ceiling: whole steps, never above the compiled ceiling ---
	if (s->maxThrottle > MAX_THROTTLE_CEILING) {
		s->maxThrottle = MAX_THROTTLE_CEILING;
		ok = false;
	}
	if (s->maxThrottle % MAX_THROTTLE_STEP) {
		s->maxThrottle = (uint16_t)(s->maxThrottle / MAX_THROTTLE_STEP * MAX_THROTTLE_STEP);
		ok = false;
	}
	if (s->maxThrottle < MAX_THROTTLE_STEP) {
		s->maxThrottle = MAX_THROTTLE_STEP;
		ok = false;
	}

	// --- DShot bitrate ---
	bool legal = false;
	for (int i = 0; i < 4; i++) if (s->dshotKbaud == LEGAL_KBAUD[i]) legal = true;
	if (!legal) { s->dshotKbaud = DSHOT_SPEED_KBAUD; ok = false; }

	// --- board ---
	// Not a setting. The field records the board a block was written on, and
	// this image drives one board, so any other value is overwritten rather
	// than honoured. A block written on other hardware, or by a build this one
	// is not, is recognisable by the id, and its pins are re-judged below
	// against this board's free GPIOs.
	if (s->boardId != BOARD_ID_LCD_2_8) {
		s->boardId = BOARD_ID_LCD_2_8;
		ok = false;
	}
	const BoardDesc *b = g_board;

	// --- ESC pin ---
	if (!settingsPinFree(s->dshotPin)) {
		s->dshotPin = b->defaultDshotPin;
		ok = false;
	}

	// --- KISS ---
	// Two rules left, now that the receiver is a PIO state machine and any free
	// GPIO can take it: the pin must be free on this board, and it must not be
	// the ESC's. The third — "and it must be one of the eight the hardware
	// UARTs can receive on" — is gone with the hardware UART. @see pio_uart_rx.h
	//
	// Switched off rather than moved, still. Moving it would silently start
	// listening on a pin the user never connected anything to, and a telemetry
	// wire that reads "ON" against the wrong pin is worse than one reading
	// "OFF". SETTINGS does move it, because there the pin change is the
	// thing being asked for. @see uiSettingsTick()
	if (s->kissEnable) {
		if (!settingsPinFree(s->kissPin) ||
		    s->kissPin == s->dshotPin) {
			s->kissEnable = 0;
			ok = false;
		}
	}
	if (s->kissEnable != 0 && s->kissEnable != 1) { s->kissEnable = 1; ok = false; }
	if (s->highContrast != 0 && s->highContrast != 1) { s->highContrast = 1; ok = false; }
	// No switch is the safe reading of a value this build does not know: no
	// enable pin is then driven.
	if (s->servoSupply > SERVO_SUPPLY_MODE_MAX) { s->servoSupply = 0; ok = false; }
	// Likewise no sensor: nothing is then sent to its address.
	if (s->currentSense > SENSE_MODE_MAX) { s->currentSense = 0; ok = false; }

	// --- backlight: never so dim the panel looks dead ---
	if (s->backlight < BACKLIGHT_MIN) { s->backlight = BACKLIGHT_MIN; ok = false; }

	return ok;
}

// ---------------------------------------------------------------------------
// CRC
// ---------------------------------------------------------------------------

uint32_t settingsCrc32(const void *data, uint32_t len) {
	const uint8_t *p = (const uint8_t *)data;
	uint32_t crc = 0xFFFFFFFFu;
	for (uint32_t i = 0; i < len; i++) {
		crc ^= p[i];
		for (int b = 0; b < 8; b++) {
			crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
		}
	}
	return ~crc;
}

/** @brief Bytes of a block that the CRC covers: everything before the CRC. */
static uint32_t crcSpan() {
	return (uint32_t)(sizeof(SettingsBlock) - sizeof(uint32_t));
}

// ---------------------------------------------------------------------------
// older layouts
// ---------------------------------------------------------------------------

/**
 * @brief Byte offsets of the fields in a block of version 2, 3 or 4, from
 *        the start of the block, and their widths.
 *
 * Recovered from git: `struct Settings` and `struct SettingsBlock` of
 * src/settings.h at 8a5eebb (version 2), 5af4ddf (3) and 3cc8798 (4). Every
 * version has the same 8-byte header, magic (4 bytes), version (2) and
 * payload size (2), and the same first twelve payload bytes; the compiler
 * aligned the 16-bit fields to 2 bytes and the CRC to 4. Little-endian, as
 * the RP2350 and the host suite store them.
 *
 * | Block byte | Width | Field                     | Versions |
 * |-----------:|------:|---------------------------|----------|
 * |  8         | 1     | boardId                   | 2, 3, 4  |
 * |  9         | 1     | dshotPin                  | 2, 3, 4  |
 * | 10         | 2     | dshotKbaud                | 2, 3, 4  |
 * | 12         | 1     | kissEnable                | 2, 3, 4  |
 * | 13         | 1     | kissPin                   | 2, 3, 4  |
 * | 14         | 1     | poles                     | 2, 3, 4  |
 * | 15         | 1     | padding                   | 2, 3, 4  |
 * | 16         | 2     | maxThrottle               | 2, 3, 4  |
 * | 18         | 1     | backlight                 | 2, 3, 4  |
 * | 19         | 1     | highContrast              | 2, 3, 4  |
 * | 20         | 1     | servoSupply               | 3, 4     |
 * | 21         | 1     | padding                   | 3, 4     |
 * | 20 or 24   | 4     | CRC-32 of the bytes before it | 2; 3, 4 |
 */
namespace old_layout {
constexpr uint32_t HEADER        = 8;
constexpr uint32_t BOARD_ID      = HEADER + 0;
constexpr uint32_t DSHOT_PIN     = HEADER + 1;
constexpr uint32_t DSHOT_KBAUD   = HEADER + 2;   // 16 bits
constexpr uint32_t KISS_ENABLE   = HEADER + 4;
constexpr uint32_t KISS_PIN      = HEADER + 5;
constexpr uint32_t POLES         = HEADER + 6;
constexpr uint32_t MAX_THROTTLE  = HEADER + 8;   // 16 bits
constexpr uint32_t BACKLIGHT     = HEADER + 10;
constexpr uint32_t HIGH_CONTRAST = HEADER + 11;
constexpr uint32_t SERVO_SUPPLY  = HEADER + 12;  // versions 3 and 4
}  // namespace old_layout

/** @brief What tells one older layout from another. */
struct OldLayout {
	uint16_t version;      /**< SettingsBlock::version. */
	uint16_t payload;      /**< SettingsBlock::size: the payload's bytes. */
	uint16_t crcAt;        /**< The block byte the CRC-32 starts at. */
	bool     servoSupply;  /**< Has servoSupply, at old_layout::SERVO_SUPPLY. */
	/**
	 * @brief Its servoSupply values mean what version 5's do: 1 an enable on
	 *        GP28, 2 on GP29. Version 3's 1 named GP0.
	 */
	bool     supplyAsNow;
};

/** @brief The layouts settingsLoad() carries over. */
static constexpr OldLayout OLD_LAYOUTS[] = {
	{ 2, 12, 20, false, false },
	{ 3, 14, 24, true,  false },
	{ 4, 14, 24, true,  true  },
};

/** @brief The CRC sits at the first 4-byte boundary after the payload. */
static constexpr bool crcAfterPayload(const OldLayout &l) {
	return l.crcAt == ((old_layout::HEADER + l.payload + 3u) & ~3u);
}
static_assert(crcAfterPayload(OLD_LAYOUTS[0]) && OLD_LAYOUTS[0].crcAt == 20,
              "version 2: 12 bytes of payload, the CRC at byte 20");
static_assert(crcAfterPayload(OLD_LAYOUTS[1]) && OLD_LAYOUTS[1].crcAt == 24,
              "version 3: 14 bytes of payload, the CRC at byte 24");
static_assert(crcAfterPayload(OLD_LAYOUTS[2]) && OLD_LAYOUTS[2].crcAt == 24,
              "version 4: 14 bytes of payload, the CRC at byte 24");
static_assert(old_layout::SERVO_SUPPLY + 1 <= old_layout::HEADER + OLD_LAYOUTS[1].payload,
              "servoSupply inside the version 3 payload");
static_assert(old_layout::HIGH_CONTRAST + 1 == old_layout::HEADER + OLD_LAYOUTS[0].payload,
              "highContrast the last byte of the version 2 payload");
static_assert(OLD_LAYOUTS[sizeof(OLD_LAYOUTS) / sizeof(OLD_LAYOUTS[0]) - 1].version ==
                  SETTINGS_VERSION - 1,
              "a version bump describes the layout it replaces here");
// Version 5 kept version 4's offsets and added currentSense in its padding:
// today's struct is the check that the table matches the last of them.
static_assert(offsetof(SettingsBlock, s) == old_layout::HEADER, "the header is 8 bytes");
static_assert(sizeof(Settings) == 14 && offsetof(SettingsBlock, crc32) == 24,
              "version 5 has version 4's size and CRC offset");
static_assert(offsetof(Settings, boardId)      + old_layout::HEADER == old_layout::BOARD_ID &&
              offsetof(Settings, dshotPin)     + old_layout::HEADER == old_layout::DSHOT_PIN &&
              offsetof(Settings, dshotKbaud)   + old_layout::HEADER == old_layout::DSHOT_KBAUD &&
              offsetof(Settings, kissEnable)   + old_layout::HEADER == old_layout::KISS_ENABLE &&
              offsetof(Settings, kissPin)      + old_layout::HEADER == old_layout::KISS_PIN &&
              offsetof(Settings, poles)        + old_layout::HEADER == old_layout::POLES &&
              offsetof(Settings, maxThrottle)  + old_layout::HEADER == old_layout::MAX_THROTTLE &&
              offsetof(Settings, backlight)    + old_layout::HEADER == old_layout::BACKLIGHT &&
              offsetof(Settings, highContrast) + old_layout::HEADER == old_layout::HIGH_CONTRAST &&
              offsetof(Settings, servoSupply)  + old_layout::HEADER == old_layout::SERVO_SUPPLY,
              "version 5's offsets are version 4's");

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Carry a whole block of an older layout over into @p out.
 *
 * Whole: @ref SETTINGS_MAGIC, a version in `OLD_LAYOUTS`, that layout's
 * payload size and the CRC-32 at its offset right over every byte before
 * it. Then @p out takes the compiled defaults and every field the layout
 * has, unvalidated: settingsLoad() validates.
 *
 * @param raw The block's bytes, at least 28.
 * @param[out] out The carried settings; untouched on false.
 * @return False for anything but such a block.
 */
static bool carryOver(const uint8_t *raw, Settings *out) {
	if (le32(raw) != SETTINGS_MAGIC) return false;
	const uint16_t version = le16(raw + 4);
	const OldLayout *l = nullptr;
	for (const OldLayout &c : OLD_LAYOUTS)
		if (c.version == version) l = &c;
	if (!l || le16(raw + 6) != l->payload) return false;
	if (le32(raw + l->crcAt) != settingsCrc32(raw, l->crcAt)) return false;

	using namespace old_layout;
	settingsDefaults(out);
	out->boardId      = raw[BOARD_ID];
	out->dshotPin     = raw[DSHOT_PIN];
	out->dshotKbaud   = le16(raw + DSHOT_KBAUD);
	out->kissEnable   = raw[KISS_ENABLE];
	out->kissPin      = raw[KISS_PIN];
	out->poles        = raw[POLES];
	out->maxThrottle  = le16(raw + MAX_THROTTLE);
	out->backlight    = raw[BACKLIGHT];
	out->highContrast = raw[HIGH_CONTRAST];
	// Version 3's 1 was an enable on GP0, the ESC's signal pin since
	// version 4. No version 5 value names it: none, so no pin is driven.
	if (l->servoSupply && l->supplyAsNow) out->servoSupply = raw[SERVO_SUPPLY];
	return true;
}

// ---------------------------------------------------------------------------
// load / save
// ---------------------------------------------------------------------------

/** @brief The blank-flash starting point: the board's compiled defaults. */
static void freshDefaults() {
	s_seeded = true;
	settingsDefaults(&s_live);
	s_saved = s_live;
	s_bootPin = s_live.dshotPin;
}

/**
 * @brief Whether @p raw holds a whole block of another version: the magic, a
 *        version other than this one, a payload of at most
 *        @ref SETTINGS_OTHER_MAX bytes and the CRC-32 after it right. In
 *        every version, 2 to 5, the header is 8 bytes and the CRC is the
 *        next 4-byte-aligned word after the payload, over every byte before
 *        it (the compiler pads `SettingsBlock` to that alignment).
 */
static bool otherVersionBlock(const uint8_t *raw) {
	uint32_t magic;
	uint16_t version, size;
	memcpy(&magic, raw, 4);
	memcpy(&version, raw + 4, 2);
	memcpy(&size, raw + 6, 2);
	if (magic != SETTINGS_MAGIC || version == SETTINGS_VERSION || size > SETTINGS_OTHER_MAX)
		return false;
	uint32_t at = (8u + size + 3u) & ~3u;
	uint32_t crc;
	memcpy(&crc, raw + at, 4);
	return crc == settingsCrc32(raw, at);
}

void settingsLoad() {
	freshDefaults();
	s_stored = false;
	s_resetByUpdate = false;
	s_carriedOver = false;

	// The header, the largest payload a block of another version may have,
	// and its CRC: enough to tell a discarded block from blank flash.
	uint8_t raw[((8 + SETTINGS_OTHER_MAX + 3) & ~3) + 4];
	static_assert(sizeof(SettingsBlock) <= sizeof(raw), "a block is larger than the read");
	static_assert(offsetof(SettingsBlock, crc32) == ((8 + sizeof(Settings) + 3) & ~3u),
	              "the CRC is not where otherVersionBlock() looks for it");
	if (!settingsStorageRead(raw, sizeof(raw))) {
		return;
	}
	static_assert(sizeof(raw) >= 28, "a version 3 or 4 block is larger than the read");

	// An older layout the table knows, whole: carried over, then judged by
	// the same validation as a block of this version. The KISS pin with
	// KISS off is not judged there; one this board does not free, the old
	// GP28 or GP29, takes the default, so turning KISS on finds a free pin.
	Settings carried;
	if (carryOver(raw, &carried)) {
		s_live = carried;
		settingsValidate(&s_live);
		if (!s_live.kissEnable && !settingsPinFree(s_live.kissPin))
			s_live.kissPin = g_board->defaultKissPin;
		s_saved = s_live;
		s_stored = true;
		s_carriedOver = true;
		s_bootPin = s_live.dshotPin;
		return;
	}
	s_resetByUpdate = otherVersionBlock(raw);
	SettingsBlock blk;
	memcpy(&blk, raw, sizeof(blk));

	// Four independent reasons to distrust the block, all fatal, none partial.
	//
	// A version this firmware has no layout for is rejected outright: a guess
	// at a layout can hand back a plausible-looking 2000 as easily as a 400.
	// The older layouts above are not guesses; each is recovered from the
	// commit that wrote it and pinned by the host suite.
	if (blk.magic != SETTINGS_MAGIC ||
	    blk.version != SETTINGS_VERSION ||
	    blk.size != (uint16_t)sizeof(Settings) ||
	    blk.crc32 != settingsCrc32(&blk, crcSpan())) {
		return;
	}

	s_live = blk.s;
	// Validation re-judges every pin against this board, so a block written on
	// other hardware loads as this board's settings rather than as a pin map
	// for hardware that is not here.
	settingsValidate(&s_live);
	s_saved = s_live;
	s_stored = true;
	s_bootPin = s_live.dshotPin;
}

Settings *settings() {
	ensureSeeded();
	return &s_live;
}

bool settingsSave() {
	// Never during a pin claim. Erasing flash parks core1 for tens of
	// milliseconds, past a KST transaction's 5 ms driver slack. SETTINGS, the
	// one caller, opens from MENU only while the claim is Free, so this is
	// the assertion under that, and it refuses rather than writes.
	if (pinClaimHolder() != PinUser::None) return false;

	ensureSeeded();
	settingsValidate(&s_live);

	// A save of exactly what is already stored is a no-op, reported as
	// success. Every real erase parks core1 and opens a torn-write window in
	// the only copy of the block — there is no A/B sector — so the write has
	// to buy something.
	// After a carry-over flash holds the older block, so the first save writes
	// even with nothing changed.
	if (s_stored && !s_carriedOver && memcmp(&s_live, &s_saved, sizeof(Settings)) == 0)
		return true;

	SettingsBlock blk;
	memset(&blk, 0, sizeof(blk));
	blk.magic   = SETTINGS_MAGIC;
	blk.version = SETTINGS_VERSION;
	blk.size    = (uint16_t)sizeof(Settings);
	blk.s       = s_live;
	blk.crc32   = settingsCrc32(&blk, crcSpan());

	if (!settingsStorageWrite(&blk, sizeof(blk))) return false;

	// Read back rather than trusting the write, for the same reason the AM32
	// settings write does: a save that reports success but did not land is
	// worse than one that fails, because the next power cycle is where you
	// find out.
	SettingsBlock check;
	memset(&check, 0, sizeof(check));
	if (!settingsStorageRead(&check, sizeof(check))) return false;
	if (memcmp(&check, &blk, sizeof(blk)) != 0) return false;

	s_saved  = s_live;
	s_stored = true;
	s_resetByUpdate = false;
	s_carriedOver = false;
	return true;
}

bool settingsStored() { return s_stored; }

bool settingsResetByUpdate() { return s_resetByUpdate; }

bool settingsCarriedOver() { return s_carriedOver; }

uint8_t settingsBootDshotPin() {
	ensureSeeded();
	return s_bootPin;
}

bool settingsDirty() {
	ensureSeeded();
	return memcmp(&s_live, &s_saved, sizeof(Settings)) != 0;
}
