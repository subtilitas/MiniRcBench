/**
 * @file am32_eeprom.h
 * @brief AM32 ESC settings block: layout, decoding and presentation.
 *
 * The byte layout matches the AM32 configurator's `EepromLayout`. Offsets are
 * absolute within the settings region, which starts at the address
 * am32BlEepromAddr() reports for the connected ESC's MCU family.
 *
 * Region map:
 *
 * | Range      | Size | Contents                          |
 * |------------|------|-----------------------------------|
 * | 0x00..0x04 | 5    | boot byte, layout and FW revisions |
 * | 0x05..0x10 | 12   | device name (layout revision < 3)  |
 * | 0x05..0x0C | 8    | extra settings (layout revision 3+)|
 * | 0x11..0x2F | 31   | the settings proper                |
 * | 0x30..0xAF | 128  | startup melody                     |
 * | 0xB0..0xBF | 16   | CAN settings                       |
 *
 * Bytes 0x05..0x0C are overloaded: in layout revision 3 and later they hold
 * ramp/current-PID settings, and before that they are part of the device name
 * string. Fields carry a version gate so the wrong set is never shown.
 */

#pragma once

#include <stdint.h>

/** @brief Bytes we read back: settings + melody + CAN block. */
#define AM32_EEPROM_SIZE   0xC0

/** @brief Bytes covering just the settings, excluding melody and CAN. */
#define AM32_SETTINGS_SIZE 0x30

/** @brief Offsets of the fixed header fields. */
enum {
	AM32_OFF_BOOT_BYTE       = 0x00, /**< Non-zero once the ESC has valid settings. */
	AM32_OFF_LAYOUT_REVISION = 0x01, /**< Governs which optional fields exist. */
	AM32_OFF_BOOTLOADER_REV  = 0x02,
	AM32_OFF_MAIN_REVISION   = 0x03,
	AM32_OFF_SUB_REVISION    = 0x04,
	AM32_OFF_NAME            = 0x05, /**< 12 bytes, layout revision < 3 only. */
	AM32_OFF_BI_DIRECTION    = 0x12, /**< AM32's `bi_direction`: 3D (reversible) mode. */
	AM32_OFF_ADVANCE         = 0x17, /**< Timing advance; two encodings. */
	AM32_OFF_LOW_VOLT_CUTOFF = 0x24, /**< 0 off, 1 per cell, 2 absolute. */
	AM32_OFF_INPUT_TYPE      = 0x2E, /**< AM32's `input_type`. @see Am32Input */
	AM32_NAME_LEN            = 12,
	AM32_OFF_MELODY          = 0x30,
	AM32_MELODY_LEN          = 128,
	AM32_OFF_CAN             = 0xB0,
	AM32_CAN_LEN             = 16,
};

/**
 * @brief Values of AM32's `input_type`, byte 0x2E: `enum inputType` in
 *        AM32's `Inc/common.h`, zero-based (AUTO_IN = 0).
 */
enum Am32Input : uint8_t {
	AM32_INPUT_AUTO     = 0, /**< Detects DShot or servo PWM. */
	AM32_INPUT_DSHOT    = 1,
	AM32_INPUT_SERVO    = 2, /**< Servo PWM only: DShot is not detected. */
	AM32_INPUT_SERIAL   = 3, /**< DShot is detected, but every throttle dropped: `EDT_ARMED` stays 0. */
	AM32_INPUT_EDT_ARM  = 4, /**< DShot; arms on the EDT enable, disarms on a zero frame. */
	AM32_INPUT_DRONECAN = 5, /**< DroneCAN builds; elsewhere as SERIAL. Not offered by the editor. */
};

/** @brief How a raw byte is turned into something a human can read. */
enum Am32Type : uint8_t {
	A32_BOOL,   /**< Off / On. */
	A32_ENUM,   /**< Index into Am32Field::names. */
	A32_RAW,    /**< Plain integer, shown with Am32Field::unit. */
	A32_SCALED, /**< `(raw * mul) / div + add`, shown with one decimal if div > 1. */
	A32_ADVANCE,/**< Timing advance. Two encodings by value. @see am32AdvanceDeciDeg() */
};

/**
 * @brief Timing advance in tenths of a degree, from the raw EEPROM byte.
 *
 * This field has two encodings and the byte itself is what says which, exactly
 * as AM32's own start-up code decides it:
 *
 * - **0..3** — the pre-1.90 format. Four steps of 7.5 degrees, 0 to 22.5.
 * - **10..42** — 1.90 and later. `raw - 10` steps of 0.9375 degrees (60/64,
 *   which is the resolution the commutation timer actually works in), 0 to 30.
 * - anything else is not a value AM32 wrote. It substitutes its own default;
 *   this returns -1 so the screen can say so rather than inventing an angle.
 *
 * Treating the field as the old encoding alone is what made a modern ESC read
 * back nonsense until the first `-` or `+` press, which clamped the byte into
 * 0..4 and made the number plausible again. A display that becomes correct
 * *because you edited it* is worse than one that is obviously wrong, because
 * the edit is what you then write back.
 *
 * @param raw Byte at offset 0x17, `AM32_OFF_ADVANCE`.
 * @return Tenths of a degree, or -1 if the byte is in neither encoding.
 */
static inline int16_t am32AdvanceDeciDeg(uint8_t raw) {
	if (raw <= 3)                 return (int16_t)(raw * 75);
	if (raw >= 10 && raw <= 42)   return (int16_t)((raw - 10) * 75 / 8);
	return -1;
}

/**
 * @brief The raw byte for a given advance step, in the modern encoding.
 *
 * Editing always writes 1.90-format bytes, whatever was read. AM32 accepts them
 * on any version — its start-up code converts in that direction anyway — and
 * writing the format the ESC's own configurator writes is the one choice that
 * cannot surprise somebody who opens it there afterwards.
 *
 * @param step 0..32, in 0.9375 degree increments.
 * @return Byte to store.
 */
static inline uint8_t am32AdvanceRaw(uint8_t step) {
	if (step > 32) step = 32;
	return (uint8_t)(step + 10);
}

/**
 * @brief The 0..32 advance step a raw byte represents, in either encoding.
 *
 * Derived from the byte directly rather than by converting the angle back,
 * because tenths of a degree do not divide evenly by 0.9375: a round trip
 * through am32AdvanceDeciDeg() loses a step, and an editor built on it stepped
 * up by one and back down by two.
 *
 * The old-format conversion is `raw << 3`, which is exactly what AM32's own
 * start-up code does with it.
 *
 * @param raw Byte at offset 0x17, `AM32_OFF_ADVANCE`.
 * @return Step 0..32. A byte in neither encoding gives 16, AM32's own
 *         substitute, so editing from it starts where the ESC actually is.
 */
static inline uint8_t am32AdvanceStep(uint8_t raw) {
	if (raw <= 3)               return (uint8_t)(raw << 3);
	if (raw >= 10 && raw <= 42) return (uint8_t)(raw - 10);
	return 16;
}

/**
 * @brief One editable setting.
 *
 * @p minVer and @p maxVer gate the field against the EEPROM's layout revision
 * at offset 0x01; 0 means unbounded on that side.
 */
struct Am32Field {
	const char *group;      /**< Section heading this field sits under. */
	const char *name;       /**< Display label. */
	uint8_t     offset;     /**< Byte offset within the settings region. */
	Am32Type    type;       /**< Presentation. */
	uint8_t     rawMin;     /**< Lowest value the editor will produce. */
	uint8_t     rawMax;     /**< Highest value the editor will produce. */
	uint8_t     step;       /**< Editor increment. */
	uint8_t     minVer;     /**< Minimum layout revision, 0 = any. */
	uint8_t     maxVer;     /**< Maximum layout revision, 0 = any. */
	int16_t     mul;        /**< A32_SCALED numerator. */
	int16_t     div;        /**< A32_SCALED denominator. */
	int16_t     add;        /**< A32_SCALED offset, applied after scaling. */
	const char *unit;       /**< Suffix, or "" for none. */
	const char *const *names; /**< A32_ENUM labels. */
	uint8_t     nameCount;  /**< Number of A32_ENUM labels. */
	/**
	 * @brief Lowest raw value at which the limit acts; with @p onMax, a raw
	 *        value outside onMin..onMax reads `OFF`. Both 0: no such rule.
	 */
	uint8_t     onMin;
	uint8_t     onMax;      /**< Highest raw value at which the limit acts. */
	/**
	 * @brief AM32 firmware, major x 100 + minor, from which @p rawMax is
	 *        accepted: below it the editor stops at @p oldMax, and a stored
	 *        value above that reads "? (n)" on a field with names. 0: every
	 *        firmware. @see am32FieldMax()
	 */
	uint16_t    topFw;
	uint8_t     oldMax;     /**< The editor's top below @p topFw. */
	/** @brief AM32 firmware from which @p onMax acts; below it @p oldOnMax. 0: every firmware. */
	uint16_t    onFw;
	uint8_t     oldOnMax;   /**< Highest raw value at which the limit acts below @p onFw. */
	/**
	 * @brief AM32 firmware from which the A32_SCALED scale applies; below it
	 *        the value reads as the plain byte. 0: every firmware.
	 */
	uint16_t    scaleFw;
	/**
	 * @brief AM32 firmware from which loadEEpromSettings() reads the byte at
	 *        all; below it the field is not shown. 0: every firmware.
	 *        @see am32FieldShown()
	 */
	uint16_t    readFw;
};

/**
 * @brief The lowest layout revision the field table is checked against: 2,
 *        the oldest that am32-firmware/AM32 and AlkaMotors 1.78 and later
 *        write. Layout 1 (AlkaMotors before 1.78) reads nothing above 0x29.
 */
#define AM32_LAYOUT_MIN_KNOWN 2

/**
 * @brief The highest layout revision the field table is checked against:
 *        AM32's `EEPROM_VERSION` 4, from v2.21 to v2.22.
 */
#define AM32_LAYOUT_MAX_KNOWN 4

/**
 * @brief The ESC's AM32 firmware as major x 100 + minor, from bytes 0x03 and
 *        0x04: 2.19 is 219. A major outside 1 to 9 or a minor above 99 is
 *        not a version AM32 wrote, and reads as 0, the oldest: every gated
 *        value stays locked.
 */
uint16_t am32Firmware(const uint8_t *eeprom);

/**
 * @brief The highest value the editor produces for @p f on this ESC:
 *        Am32Field::rawMax, or Am32Field::oldMax on firmware older than
 *        Am32Field::topFw.
 */
uint8_t am32FieldMax(const Am32Field *f, const uint8_t *eeprom);

/** @brief The full field table. */
extern const Am32Field AM32_FIELDS[];

/** @brief Number of entries in @ref AM32_FIELDS. */
extern const uint16_t AM32_FIELD_COUNT;

/**
 * @brief Whether a field applies to a given EEPROM layout revision.
 * @param f          Field to test.
 * @param layoutRev  Value read from offset 0x01.
 */
bool am32FieldApplies(const Am32Field *f, uint8_t layoutRev);

/**
 * @brief Render a field's current value into @p out.
 *
 * Never writes more than @p outLen bytes and always NUL-terminates.
 *
 * @param f      Field to format.
 * @param eeprom The settings region as read from the ESC.
 * @param out    Destination buffer.
 * @param outLen Size of @p out.
 */
void am32FormatValue(const Am32Field *f, const uint8_t *eeprom, char *out, int outLen);

/**
 * @brief Copy the device name out, NUL-terminated.
 *
 * Layout revision 3 and later reuse those bytes for settings, so this yields an
 * empty string there rather than nonsense.
 *
 * @param eeprom The settings region.
 * @param out    Destination, at least AM32_NAME_LEN + 1 bytes.
 */
void am32DeviceName(const uint8_t *eeprom, char *out);

/**
 * @brief Adjust a field by @p dir steps, clamped to its range.
 * @param f      Field to change.
 * @param eeprom Settings region, modified in place.
 * @param dir    +1 or -1.
 */
void am32Adjust(const Am32Field *f, uint8_t *eeprom, int dir);

/**
 * @brief Whether the field's value turns its limit off: a raw value outside
 *        Am32Field::onMin..onMax. am32FormatValue() shows such a value as
 *        `OFF`.
 */
bool am32ValueOff(const Am32Field *f, const uint8_t *eeprom);

/**
 * @brief Whether @p f is one of the ESC's protections (LOW VOLT CUTOFF, TEMP
 *        LIMIT, CURRENT LIMIT) and its value turns it off. LOW VOLT CUTOFF
 *        is off at 0 and at any value above am32FieldMax(): AM32 before 2.19
 *        acts on 1 alone, and 2.19 and 2.20 never trip on 2; 2.21 and later
 *        act on 1 and 2.
 */
bool am32ProtectionOff(const Am32Field *f, const uint8_t *eeprom);

/** @brief 3D (reversible) mode is on: byte 0x12, AM32's `bi_direction`. */
bool am32Is3d(const uint8_t *eeprom);

/**
 * @brief The input type is one on which AM32 does not run on this tester's
 *        DShot: every value but AUTO (0), DSHOT (1) and 10 or above. SERVO
 *        detects servo PWM only; SERIAL, DRONECAN and 6 to 9 leave AM32's
 *        `EDT_ARMED` at 0, so dshot.c drops every throttle above 47. EDT ARM
 *        sets it on the EDT enable and clears it on every zero frame, and the
 *        tester sends zero frames after each enable and before any throttle.
 */
bool am32InputNotDshot(const uint8_t *eeprom);

/** @brief True if the block looks like initialised AM32 settings. */
bool am32Plausible(const uint8_t *eeprom);

/**
 * @brief True if the layout revision is one the field table is checked
 *        against: @ref AM32_LAYOUT_MIN_KNOWN to @ref AM32_LAYOUT_MAX_KNOWN.
 */
bool am32LayoutKnown(const uint8_t *eeprom);

/**
 * @brief Whether the list shows @p f for this ESC: its layout revision has
 *        the field (am32FieldApplies()) and its firmware reads the byte
 *        (Am32Field::readFw).
 */
bool am32FieldShown(const Am32Field *f, const uint8_t *eeprom);

/**
 * @brief The firmware below which bytes 0x2A to 0x2E are not shown: 2.00.
 *        AlkaMotors' tags read 0x2A to 0x2D from v1.80 and 0x2E from v1.94,
 *        but the version an ESC stores does not follow the tags (the v1.99
 *        tag writes 1.95), so below 2.00 which bytes a firmware reads is not
 *        verified.
 */
#define AM32_FW_VERIFIED 200
