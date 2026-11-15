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

/**
 * @brief The second strip line of a read-only list begins with this, then
 *        the backup's fate (@ref EFM8_UI_RO_SAVED and the others).
 */
#define EFM8_UI_SHOWN_ONLY "SHOWN ONLY."
/** @brief The first strip line of a page that lost its tags. */
#define EFM8_UI_LOST_TAGS "ESC LOST ITS TAGS: READ-ONLY."
/** @brief Its second line begins with this, then the backup's fate. */
#define EFM8_UI_REFLASH "REFLASH ON A COMPUTER."
/** @brief ...then this and the backup's five characters, `BACKUP BJ018`: on the card. */
#define EFM8_UI_RO_SAVED " BACKUP "
/** @brief ...or this, after SHOWN ONLY.: no card, or the card held by USB. */
#define EFM8_UI_RO_NO_CARD " NO CARD: NO BACKUP."
/** @brief ...or this, after REFLASH ON A COMPUTER.: no card. */
#define EFM8_UI_RO_NO_CARD_TAGS " NO CARD."
/** @brief ...or this: the card did not take the backup. */
#define EFM8_UI_RO_FAILED " BACKUP FAILED."

/**
 * @defgroup efm8_write The write's texts
 * @brief The result lines of ESC WRITE at y 196 and 208, the strip of a
 *        half-written page and the guard's lines
 *        (docs/design/bluejay-blheli-s.md, "The list, the editor and the
 *        write page" and "Safety"). Each at most @ref EFM8_LINE_CHARS
 *        characters.
 * @{
 */
/** @brief Under a verified write: LEAVE starts the firmware, which reads the page. 33 characters. */
#define EFM8_UI_LEAVING_2 "LEAVING STARTS THE ESC WITH THEM."
/** @brief The re-read before the erase failed: 28 characters. */
#define EFM8_UI_READ_1 "READ FAILED: NOTHING ERASED."
/** @brief The next step after a failure that left the page as read: 20 characters. */
#define EFM8_UI_AGAIN_2 "TAP OK, WRITE AGAIN."
/** @brief The re-read found another page than the first read: 30 characters. */
#define EFM8_UI_CHANGED_1 "ESC PAGE CHANGED: NOT WRITTEN."
/** @brief Its next step: 19 characters. */
#define EFM8_UI_READ_AGAIN_2 "TAP OK, READ AGAIN."
/** @brief Three procedures that did not take, the page read back as read: 27 characters. */
#define EFM8_UI_FAILED_1 "WRITE FAILED: PAGE AS READ."
/** @brief The page read back as neither: 35 characters. */
#define EFM8_UI_TORN_1 "HALF WRITTEN: KEEP THE ESC POWERED."
/** @brief Its next step: 24 characters. */
#define EFM8_UI_TORN_2 "TAP OK, WRITE AGAIN NOW."
/** @brief The ESC stopped answering after the erase: 35 characters. */
#define EFM8_UI_LOST_1 "NO ANSWER IN THE WRITE: KEEP POWER."
/** @brief Its next step: 24 characters. */
#define EFM8_UI_LOST_2 "TAP OK, THEN READ AGAIN."
/** @brief A heartbeat lapse stopped the write before its erase: 34 characters. */
#define EFM8_UI_STOPPED_1 "STOPPED BEFORE THE ERASE: AS READ."
/** @brief The line driver failed before the erase: 35 characters. */
#define EFM8_UI_DRIVER_1 "LINE DRIVER FAILED: NOTHING ERASED."
/** @brief The session or its guard refused the draft before any frame: 35 characters. */
#define EFM8_UI_REFUSED_W "NOT WRITTEN: REFUSED, NOTHING SENT."
/** @brief Under a write whose session ended: OK greets the ESC again. 36 characters. */
#define EFM8_UI_ENDED_2 "THE SESSION ENDED. TAP OK TO SEARCH."
/** @brief The strip of a half-written list that needs a read first: 23 characters. */
#define EFM8_UI_DIRTY_READ "READ AGAIN, THEN WRITE."
/** @brief The strip of a half-written list that was read since: 33 characters. */
#define EFM8_UI_DIRTY_WRITE "WRITE AGAIN NOW. REVERT: AS READ."
/** @brief The guard's first line on a half-written page: 35 characters. */
#define EFM8_UI_GUARD_1 "HALF WRITTEN. BACK AGAIN LEAVES IT;"
/** @brief The guard's second line: 34 characters. */
#define EFM8_UI_GUARD_2 "A POWER CYCLE THEN LOSES ITS TAGS."
/** @brief ESC WRITE with no change line: the page as first read goes back. 32 characters. */
#define EFM8_UI_AS_READ "THE PAGE AS READ, TAGS INCLUDED."
/** @brief The list's bar with no row to edit selected, first line: 21 characters. */
#define EFM8_UI_BAR_EDIT "TAP A SETTING TO EDIT"
/** @brief The bar of a read-only list, first line: 31 characters. */
#define EFM8_UI_BAR_SHOWN "SHOWN ONLY: NOTHING IS WRITTEN."
/** @} */

/** @brief Where a read-only list's backup went, as the view opened. */
enum class Efm8Backup : uint8_t {
	NoCard,  /**< No card, or the card held by a USB host: none saved. */
	Saved,   /**< On the card. */
	Failed,  /**< The card did not take it. */
};

/** @brief What ESC WRITE says of a write that ended. */
struct Efm8Result {
	const char *l1;   /**< The first line; null for a verified write, whose line names the backup. */
	const char *l2;   /**< The second line, or null. */
	bool verified;    /**< Read back equal to the draft: the first line green. */
	bool mustRead;    /**< READ AGAIN takes WRITE...'s place until a read succeeds. */
};
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
 * @brief The two amber strip lines of a read-only list: why, and where its
 *        backup went.
 *
 * The second line reads, for example, `SHOWN ONLY. BACKUP BJ018`, or for
 * lost tags `REFLASH ON A COMPUTER. BACKUP BH018`.
 * @param id   The identification.
 * @param page The page as read.
 * @param b    The backup's fate.
 * @param name The backup's file name, `BJ018.TXT`; its first five
 *             characters are shown when @p b is Saved.
 * @param[out] l1 Takes the first; @ref EFM8_LINE_CHARS + 1 bytes.
 * @param[out] l2 Takes the second; @ref EFM8_LINE_CHARS + 1 bytes.
 * @return false for a page that is not read-only.
 */
bool efm8ReadOnlyLines(const ew_ident_t *id, const uint8_t *page, Efm8Backup b, const char *name, char *l1,
                       char *l2);

/**
 * @brief Whether the list edits @p f: a field rcbench's map lets a write
 *        change, on a page of a layout that is written. Every other row is
 *        shown only.
 * @param id The identification.
 * @param f  The field.
 */
bool efm8RowEditable(const ew_ident_t *id, const ew_field_t *f);

/**
 * @brief One step of a field: to the next of its values above or below
 *        @p raw, `alt` among them; a value none of the field's steps to the
 *        nearest value in that direction.
 * @param f   The field.
 * @param[in,out] raw The byte.
 * @param dir -1 or +1.
 * @return false at the rail: nothing changed.
 */
bool efm8Step(const ew_field_t *f, uint8_t *raw, int dir);

/**
 * @brief Steps of @p f per coarse step of a swipe: its range over 11, rounded
 *        up, as on AM32: 24 for a field of 0 to 255, 1 for a short list.
 */
int efm8CoarseFactor(const ew_field_t *f);

/**
 * @brief The rows whose byte differs between @p from and @p to.
 * @param rows The rows.
 * @param n    Their count.
 * @param from A page.
 * @param to   A page.
 */
int efm8Changes(const Efm8Row *rows, int n, const uint8_t *from, const uint8_t *to);

/**
 * @brief What ESC WRITE says of a write that ended with @p result.
 * @param result An `ew_ses_t`.
 * @param dirty  The session holds the page half written.
 * @param inMode The session goes on: false when a stop, the driver or a
 *               frame's refusal ended it, and the second line then says so.
 */
Efm8Result efm8WriteResult(uint8_t result, bool dirty, bool inMode);

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
