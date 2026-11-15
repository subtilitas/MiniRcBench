/**
 * @file efm8_view.h
 * @brief What ESC SETTINGS shows of a Bluejay or BLHeli_S page: the list's
 *        rows and values, the strip's lines and the connect page's refusal.
 *
 * BLHeli_S and Bluejay are names of their owners' firmware, used here only
 * to say which ESCs this module works with.
 *
 * The rows are rcbench's fields (`ew_map.h`) for the page's firmware and
 * layout, the revision bytes 0x00 to 0x02 left to the strip, grouped as
 * AM32's rows are: MOTOR, STARTUP, BRAKE, LIMITS, SOUND, INPUT, OTHER, and
 * by offset inside a group. A value reads as rcbench's `ew_field_text()`
 * writes it, in capitals, but for DIRECTION's 3 and 4, which read `3D` and
 * `3D REVERSED` (docs/design/bluejay-blheli-s.md, "The fields" and "What
 * each page shows").
 *
 * Every line here is at most @ref EFM8_LINE_CHARS characters: a scale-1
 * line from x 8. Pure; host-tested.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ew_id.h"
#include "ew_map.h"

/** @brief Characters of a scale-1 line from x 8 to x 232. */
#define EFM8_LINE_CHARS 37
/** @brief Rows a page lists at most: Bluejay 208 has 18. */
#define EFM8_ROWS_MAX 24
/** @brief The byte of DIRECTION in both firmwares. */
#define EFM8_OFF_DIRECTION 0x0Bu

/** @brief The second strip line of a read-only list: no backup in this build. */
#define EFM8_UI_SHOWN_ONLY "SHOWN ONLY. NO BACKUP SAVED."
/** @brief The first strip line of a page that lost its tags. */
#define EFM8_UI_LOST_TAGS "ESC LOST ITS TAGS: READ-ONLY."
/** @brief Its second line. */
#define EFM8_UI_REFLASH "REFLASH ON A COMPUTER. NO BACKUP."
/** @brief The first strip line of a page whose marker is not 0x55 0xAA. */
#define EFM8_UI_MARKER "SETTINGS MARKER WRONG: READ-ONLY."
/** @brief POWER RATING selected, first line. */
#define EFM8_UI_RATING_1 "SETS HOW THE ESC READS ITS TEMP:"
/** @brief POWER RATING selected, second line. */
#define EFM8_UI_RATING_2 "A WRONG VALUE MISREADS THE LIMIT."
/** @brief The connect page under a refused page, first line. */
#define EFM8_UI_REFUSED_1 "NOT BLUEJAY OR BLHELI_S AS KNOWN:"
/** @brief The connect page under a refused page, second line. */
#define EFM8_UI_REFUSED_2 "NOTHING IS WRITTEN."
/** @brief The connect page under an empty page, first line. */
#define EFM8_UI_EMPTY_1 "PAGE EMPTY, AS AFTER A CUT WRITE."
/** @brief The connect page under an empty page, second line. */
#define EFM8_UI_EMPTY_2 "KEEP THE ESC POWERED: SEE MANUAL."

/** @brief One row of the list. */
struct Efm8Row {
	const ew_field_t *f;      /**< rcbench's field. */
	const char       *group;  /**< Its group, drawn above its label. */
};

/** @brief How the screen shows a page, from its identification. */
enum class Efm8View : uint8_t {
	List,      /**< A written layout, marker right: the list. */
	ReadOnly,  /**< Known, not written (an older layout, lost tags, the marker): the list, two amber lines. */
	Refused,   /**< Not taken: the connect page names why. */
};

/**
 * @brief The group a row of @p fw at byte @p off is drawn under.
 * @param fw  An `ew_fw_t`.
 * @param off The byte.
 * @return MOTOR, STARTUP, BRAKE, LIMITS, SOUND, INPUT or OTHER.
 */
const char *efm8Group(uint8_t fw, uint8_t off);

/**
 * @brief The rows of a page: the fields @p layout has, but the revision
 *        bytes, by group and offset.
 * @param fw     An `ew_fw_t`, EW_FW_S or EW_FW_BJ.
 * @param layout The page's layout, byte 0x02.
 * @param[out] out At most @p max rows.
 * @param max    Room in @p out.
 * @return Rows written; 0 for another firmware.
 */
int efm8Rows(uint8_t fw, uint8_t layout, Efm8Row *out, int max);

/**
 * @brief A byte as the row shows it: rcbench's text in capitals, DIRECTION's
 *        3 and 4 as `3D` and `3D REVERSED`, a value none of the field's as
 *        `RAW 17`.
 * @param f    The field.
 * @param raw  The byte.
 * @param[out] out Takes the text.
 * @param n    Bytes at @p out.
 * @return false for a value none of the field's: the row draws it amber.
 */
bool efm8ValueText(const ew_field_t *f, uint8_t raw, char *out, size_t n);

/** @brief DIRECTION at 3D or 3D REVERSED: the page's byte 0x0B is 3 or 4. */
bool efm8Is3d(const uint8_t *page);

/** @brief `BLUEJAY` or `BLHELI_S`; `EFM8` for another firmware. */
const char *efm8FwName(uint8_t fw);

/** @brief The view a page takes. */
Efm8View efm8ViewOf(const ew_ident_t *id);

/**
 * @brief The strip's caption: the firmware, main and sub revision as
 *        numbers, the layout and the signature, `BLUEJAY 0.21  LAYOUT 208
 *        E8B2`; at most @ref EFM8_LINE_CHARS characters.
 * @param id   The identification.
 * @param page The page as read.
 * @param sig  The signature, 0xE8B2.
 * @param[out] out Takes it; at least @ref EFM8_LINE_CHARS + 1 bytes.
 * @param n    Bytes at @p out.
 */
void efm8Caption(const ew_ident_t *id, const uint8_t *page, uint16_t sig, char *out, size_t n);

/**
 * @brief The two amber strip lines of a read-only list: why, and that
 *        nothing is saved.
 * @param id   The identification.
 * @param page The page as read.
 * @param[out] l1 Takes the first; @ref EFM8_LINE_CHARS + 1 bytes.
 * @param[out] l2 Points at the second.
 * @return false for a page that is not read-only.
 */
bool efm8ReadOnlyLines(const ew_ident_t *id, const uint8_t *page, char *l1, const char **l2);

/**
 * @brief Whether the first `EW_MAP_LEN_S` bytes, 112, of a page read 0xFF: a
 *        page erased and not programmed again.
 */
bool efm8PageEmpty(const uint8_t *page);

/**
 * @brief The connect page after a refused page: its status, `EFM8 E8B2:
 *        PAGE NOT KNOWN` and the like, and the two lines under it.
 * @param id   The identification.
 * @param c    The first contact's answer.
 * @param page The bytes identification read.
 * @param[out] status Takes the status; @ref EFM8_LINE_CHARS + 1 bytes.
 * @param[out] l1 Points at the first line.
 * @param[out] l2 Points at the second.
 */
void efm8RefusalLines(const ew_ident_t *id, const ew_contact_t *c, const uint8_t *page,
                      char *status, const char **l1, const char **l2);

/**
 * @brief The selected row's strip note: DIRECTION at 3D or 3D REVERSED,
 *        TEMPERATURE or LOW-SPEED PROTECTION at OFF, POWER RATING.
 * @param f   The selected field.
 * @param raw Its byte.
 * @param[out] l1 Points at the first line.
 * @param[out] l2 Points at the second.
 * @return false when the row has no note.
 */
bool efm8RowNote(const ew_field_t *f, uint8_t raw, const char **l1, const char **l2);
