/**
 * @file settings.h
 * @brief Settings the user can change on the board, persisted to flash.
 *
 * Everything here used to be a `#define` in @ref config.h, changeable only by
 * recompiling. Those values are still there and are still the defaults — what
 * changed is that they are now the *starting point* rather than the answer.
 *
 * @section settings_why Why these, and not the rest of config.h
 *
 * A setting earns a place here if getting it wrong is quiet. The ESC pin is the
 * canonical case: point it at a GPIO no wire is on and nothing errors, nothing
 * warns, and the ESC simply never hears a frame. That is not a value to require
 * a toolchain to change.
 *
 * Timings, buffer sizes and protocol constants stay in config.h. They fail
 * loudly, they are not things a user on a bench needs mid-session, and every
 * one of them added here is another field that has to survive a version bump.
 *
 * @section settings_storage Storage
 *
 * One dedicated flash sector, at the top of the address space the build
 * declares — see settings_flash.cpp. The block is versioned and CRC'd, and
 * anything that fails to validate falls back to compiled defaults *whole*,
 * never field by field. A whole block of versions 2 to 4 is carried over
 * field by field from its own layout. @see settingsValidate(),
 * settingsLoad()
 *
 * Writing is deliberately manual. A flash erase parks core1 for tens of
 * milliseconds, which stops the DShot pump; that is a thing to do on purpose
 * with a button, not a side effect of turning a dial.
 */

#pragma once

#include <stdint.h>

#include "board_desc.h"

/** @brief Identifies a settings block. ASCII "DSHT". */
#define SETTINGS_MAGIC   0x44534854u

/**
 * @brief Layout version.
 *
 * Bump on any change to @ref Settings, and describe the layout it replaces
 * in settings.cpp's table of older layouts, so settingsLoad() carries a
 * block of it over. A block of a version the table does not hold, below 2
 * or above this one, is discarded whole.
 *
 * Version 2 is the 12-byte layout up to @ref Settings::highContrast.
 * Version 3 adds @ref Settings::servoSupply, then 0 none or 1 an enable on
 * GP0. Version 4 moves the signal pins to GP0 and GP1 and the supply's
 * enable to GP28 (1) or GP29 (2). Version 5 adds @ref Settings::currentSense
 * in the byte that was padding. The host suite pins every layout.
 *
 * DshotDisplay writes the same magic at the same place, version 2 with the
 * layout above (its `src/settings.h` at the tags v2.0 and v2.3). A block it
 * wrote is carried over as version 2; so a DshotDisplay that moves on must
 * not use versions 3 to 5 for another layout, nor this firmware a version
 * DshotDisplay uses, or one firmware reads the other's block as its own.
 */
#define SETTINGS_VERSION 5

/** @brief Lowest backlight level: below it the panel looks dead. */
#define BACKLIGHT_MIN 16

/** @brief The persisted settings. */
struct Settings {
	/**
	 * @brief Which board this block was written on: @ref BOARD_ID_LCD_2_8.
	 *
	 * A record, never an input. @ref settingsValidate() checks it against
	 * @ref BOARD_ID_LCD_2_8 and overwrites any other value, and re-judges every
	 * pin in the block against this board's free GPIOs. Nothing in the UI
	 * offers to change it.
	 */
	uint8_t  boardId;
	uint8_t  dshotPin;     /**< SIG: GPIO carrying the ESC signal wire. */
	uint16_t dshotKbaud;   /**< DShot bitrate: 150, 300, 600 or 1200. */
	uint8_t  kissEnable;   /**< Non-zero to claim a UART for KISS telemetry. */
	/** @brief AUX: GPIO the ESC's telemetry pad connects to, and servo B's.
	 *         Any free one but SIG's. */
	uint8_t  kissPin;
	uint8_t  poles;        /**< Motor pole count, for eRPM to RPM. */
	uint16_t maxThrottle;  /**< Throttle ceiling, out of 0..2000. */
	uint8_t  backlight;    /**< Preferred backlight level, @ref BACKLIGHT_MIN to 255. */
	uint8_t  highContrast; /**< Non-zero selects @ref Theme::HighContrast. */
	/** @brief The switched servo supply: 0 none (OWN on the screen: the
	 *         servo has its own supply), 1 an enable on GP28, 2 on GP29.
	 *         @see servo_supply.h */
	uint8_t  servoSupply;
	uint8_t  currentSense; /**< The current sensor: 0 none, 1 an INA219 on J3. @see sense.h */
};

/**
 * @brief What a settings block looks like in flash.
 *
 * Size is recorded as well as version so a block written by a build with a
 * different struct layout is rejected even if someone forgets to bump
 * @ref SETTINGS_VERSION.
 */
struct SettingsBlock {
	uint32_t magic;    /**< @ref SETTINGS_MAGIC. */
	uint16_t version;  /**< @ref SETTINGS_VERSION. */
	uint16_t size;     /**< `sizeof(Settings)` when written. */
	Settings s;        /**< The payload. */
	uint32_t crc32;    /**< CRC-32 over every byte before this one. */
};

/**
 * @defgroup settings_pins Pin rules
 * @brief Which GPIOs a user is allowed to pick, and what they can be used for.
 *
 * Both are pure and host-tested, because the alternative to encoding them is a
 * picker that offers a pin the panel is already using.
 * @{
 */

/**
 * @brief True if @p pin is free on this board for an ESC or telemetry wire.
 *
 * Reads `BOARD_FREE_GPIO_MASK` from the board header. Everything not in that
 * mask drives an on-board peripheral, and assigning one is how you end up
 * sending DShot out of a backlight transistor.
 *
 * @param pin GPIO number.
 * @return true if the pin is unclaimed.
 */
bool settingsPinFree(uint8_t pin);

/**
 * @brief The next free GPIO after @p from, wrapping.
 *
 * Drives the `-`/`+` buttons on SETTINGS. Stepping through only legal
 * values is what removes the error state entirely: there is no invalid pin to
 * reject, because one cannot be selected.
 *
 * @param from Current pin.
 * @param dir  +1 or -1.
 * @return The next candidate, or @p from if the board offers no other.
 */
uint8_t settingsNextPin(uint8_t from, int dir);

/** @} */

/**
 * @defgroup settings_api Access
 * @{
 */

/**
 * @brief Fill @p out with the compiled defaults from @ref config.h.
 * @param[out] out Destination.
 */
void settingsDefaults(Settings *out);

/**
 * @brief Clamp @p s into the range every consumer assumes.
 *
 * Called on load and before every save, so no code downstream has to defend
 * itself against a pole count of zero or a pin the panel is using.
 *
 * Repairs, in order: pole count even and in range; throttle ceiling a whole
 * number of steps and within @ref MAX_THROTTLE_CEILING; DShot bitrate one of
 * the four legal values; board id @ref BOARD_ID_LCD_2_8; SIG, the ESC pin,
 * free; AUX, the KISS pin, free and not SIG's, whatever KISS is. An AUX pin
 * the board does not free takes the board's default and switches KISS off.
 * AUX on SIG's GPIO moves to the next free pin after SIG, and KISS keeps its
 * switch; only with no second free pin is KISS switched off. Servo supply
 * 0 to 2, any other value 0; current sensor 0 or 1, any other value 0.
 *
 * @param[in,out] s Settings to repair in place.
 * @return true if nothing needed changing.
 */
bool settingsValidate(Settings *s);

/**
 * @brief Load from storage, or fall back to defaults.
 *
 * A block that fails magic, size, version or CRC is discarded **whole**. Not
 * field by field: a partially-trusted block is how a corrupted byte becomes a
 * throttle ceiling of 100 %, and there is no version of that trade worth
 * taking. @ref settingsStored() reports which happened.
 *
 * A whole block of version 2, 3 or 4, its size and its CRC right for its
 * own layout, is carried over (settingsCarriedOver()): each field it has is
 * read from its offset in that layout, each newer field takes its default,
 * and settingsValidate() then judges the result as it judges a version 5
 * block. A version 3 SERVO SUPPLY of 1 named an enable on GP0, which the
 * signal wire now uses: it loads as 0, OWN. A KISS pin this board does not
 * free takes the board's default, as settingsValidate() repairs it. Flash is
 * not rewritten: the old block stays until a save lands, and every boot
 * until then carries it over again.
 */
void settingsLoad();

/**
 * @brief The live settings. Mutable; changes take effect immediately.
 * @return Pointer to the working copy.
 */
Settings *settings();

/**
 * @brief Write the live settings to flash.
 *
 * Validates first, so a save can only ever store something loadable.
 *
 * @warning Erasing flash parks core1 and stalls the DShot pump for tens of
 *          milliseconds. Callers must be disarmed. SETTINGS enforces
 *          that; this function cannot, because it has no way to ask.
 *
 * Refused, with nothing written, while a pin claim holds (pin_claim.h): a
 * core1 job would stall for as long as the pump does.
 *
 * @return true if the block was written and read back correctly.
 */
bool settingsSave();

/** @brief True if a valid block was found at boot. @return Load outcome. */
bool settingsStored();

/**
 * @brief True after settingsLoad() discarded a block of another version: a
 *        firmware with a @ref SETTINGS_VERSION this one cannot carry over
 *        wrote it, and every setting is back at its compiled default.
 *
 * The block must be whole to count: @ref SETTINGS_MAGIC, a version other
 * than this one, a size of at most @ref SETTINGS_OTHER_MAX bytes of payload
 * and a CRC-32 right over its own header and payload. Blank flash and a
 * damaged block read false; so does a block carried over
 * (settingsCarriedOver()). The splash and MENU's SETTINGS row show it; a
 * save that lands (settingsSave() true) ends it.
 * @return Whether the last load discarded such a block and no save has landed since.
 */
bool settingsResetByUpdate();

/**
 * @brief True after settingsLoad() carried a version 2, 3 or 4 block over
 *        into the live settings, until a save lands.
 *
 * The live settings are the carried ones and settingsDirty() compares
 * against them, so it reads false; flash still holds the old block, and
 * settingsSave() writes a version 5 block even with nothing changed. The
 * splash, MENU's SETTINGS row and SETTINGS' note line show it in place of
 * the reset by an update.
 * @return Whether the last load carried a block over and no save has landed since.
 */
bool settingsCarriedOver();

/**
 * @brief The ESC pin settingsLoad() put live: the pin the splash names, and
 *        the one DShot starts on at boot. The compiled default's before a
 *        load. SETTINGS warns while the live pin differs from it.
 * @return GPIO number.
 */
uint8_t settingsBootDshotPin();

/**
 * @brief The largest payload a block of another version may declare and
 *        still be judged whole by settingsResetByUpdate(). Version 2 had
 *        12 bytes, versions 3 to 5 have `sizeof(Settings)`.
 */
#define SETTINGS_OTHER_MAX 52

/**
 * @brief True if the live settings differ from what is in flash: from what
 *        the last load or save put live. After a carry-over that is the
 *        carried settings, not the older block flash still holds.
 * @return Whether the pilot changed anything since.
 */
bool settingsDirty();

/** @} */

/**
 * @defgroup settings_storage Storage back end
 * @brief The two calls that touch flash, kept behind a seam.
 *
 * settings.cpp holds the rules and is compiled into the host test suite;
 * settings_flash.cpp holds the RP2350 flash sequence and cannot be. The host
 * suite supplies its own RAM-backed pair, which is what lets the validation and
 * fallback rules above be tested at all.
 * @{
 */

/**
 * @brief Read the persisted block.
 * @param[out] dst Destination buffer.
 * @param len      Bytes to read.
 * @return true if @p dst was filled.
 */
bool settingsStorageRead(void *dst, uint32_t len);

/**
 * @brief Overwrite the persisted block.
 * @param src Source buffer.
 * @param len Bytes to write.
 * @return true if the write completed.
 */
bool settingsStorageWrite(const void *src, uint32_t len);

/** @} */

/**
 * @brief CRC-32, the common reflected polynomial (0xEDB88320).
 *
 * Exposed so the tests can corrupt a block deliberately and confirm the load
 * path rejects it, rather than asserting that some opaque number is unchanged.
 *
 * @param data Bytes to sum.
 * @param len  Number of bytes.
 * @return The checksum.
 */
uint32_t settingsCrc32(const void *data, uint32_t len);
