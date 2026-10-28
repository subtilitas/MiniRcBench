/**
 * @file hitec_files.h
 * @brief The Hitec files on the SD (Secure Digital) card: SAVE AS files in
 *        `HITEC/`, the backup in the ring of `BACKUP/`, and the record of a
 *        write under way, in rcbench's texts (`hitec_card.h`) over FatFs.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * Every function runs on core0, the only core that touches the card. The
 * card counts as mounted under the rule of cardMounted() (card_dir.h).
 *
 * | Situation | Call | Effect |
 * |---|---|---|
 * | the FILES list | hitecFilesList(), hitecFilesBackups() | the `.TXT` files of `HITEC/` by name without case, and the Hitec backups of `BACKUP/`, latest first |
 * | LOAD | hitecFilesLoad(), hitecFilesBackupLoad() | a servo file, or a record's image before its write |
 * | SAVE AS | hitecFilesSaveName(), hitecFilesSaveAs() | the next free HTnnn of `HITEC/`, never written over; refused once `HITEC/` holds 50 `.TXT` files, the record left out |
 * | first write of a session | hitecFilesFirstWrite() | an old record moved into the ring, the backup into the ring, then this write's record as `HITEC/HITEC.TXT` |
 * | a later write | hitecFilesRecordWrite() | this write's record written over the session's own |
 * | write verified | hitecFilesRecordRemove() | `HITEC/HITEC.TXT` removed, when this session wrote it and it holds the bytes written |
 * | the screen opens | hitecFilesRecordPresent(), hitecFilesRecordLoad() | an interrupted write's record found and read |
 *
 * The servo file text is `hitec_card_file_text()`'s, the record's
 * `hitec_card_text()`'s, both with a CRC-16 check line. A file edited by hand
 * does not load. A record left by an interrupted write is moved into the ring
 * as a Hitec backup before a new one is written, so no record is lost.
 *
 * Current limitations: those of kst_files.h on a failing card hold here
 * unchanged. A later write's record goes over the session's own in place
 * (cardOverwrite()). A failure there removes the record, so that write has
 * no record; the session's backup stays in `BACKUP/`. A card that refuses
 * the removal as well keeps the file, which the next open reports as an
 * interrupted write. A file
 * of `HITEC/` with a name longer than 8.3 is not listed and cannot be loaded.
 *
 * @see docs/design/hitec-programming.md, "Storage".
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "backup_ring.h"
#include "card_dir.h"
#include "hitec_card.h"
#include "hitec_reg.h"

/** @brief The directory of the SAVE AS files and the record, from the root. */
#define HITEC_FILES_DIR "HITEC"

/** @brief The files of `HITEC/` past which SAVE AS refuses. */
#define HITEC_FILES_SAVE_MAX 50

/** @brief The record's file name in `HITEC_FILES_DIR`. */
#define HITEC_FILES_RECORD_NAME HITEC_CARD_RECORD ".TXT"

/** @brief Room for an 8.3 file name of `HITEC/`, `HITEC001.TXT`, and its terminator. */
#define HITEC_FILES_NAME_MAX 13

/** @brief One file name of `HITEC/`. */
typedef char HitecFileName[HITEC_FILES_NAME_MAX];

/** @brief What a file call reports. */
enum class HitecFiles : uint8_t {
	Ok,
	NoCard,      /**< No card mounted, or a USB host holds it. */
	ListError,   /**< A directory did not read to its end. */
	NoNumber,    /**< SAVE AS: 50 `.TXT` files, the record left out, or HT001 to HT999 taken. */
	Exists,      /**< The name is taken; nothing was written. */
	Full,        /**< No space on the card. */
	Denied,      /**< Write protected, or a file or directory in the way. */
	WriteError,  /**< Any other write failure. */
	NotFound,    /**< No such file. */
	TooLarge,    /**< Longer than its text can be. */
	ReadError,   /**< The file did not read whole. */
	BadText,     /**< The text does not parse: hitec_card_file_parse() or hitec_card_parse() refused it. */
	OtherSeries, /**< LOAD: a file of the other family. */
};

/** @brief What the first write of a session, or a later one, put on the card. */
struct HitecWrite {
	HitecFiles    backup;                       /**< The backup's save into `BACKUP/`; Ok and no name for a later write. */
	BackupFileName backupName;                  /**< Its file name; empty unless saved. */
	HitecFiles    kept;                         /**< Keeping an old record; Ok when there was none. */
	BackupFileName keptName;                    /**< The old record's file name in `BACKUP/`; empty for none. */
	HitecFiles    record;                       /**< This write's record. */
	uint32_t      recordSum;                    /**< FNV-1a (Fowler-Noll-Vo) hash of the record's bytes. */
};

/** @brief Whether a card is mounted and not held by a USB host: cardMounted(). */
bool hitecFilesMounted();

/**
 * @brief The `.TXT` files of `HITEC/` other than the record, the first @p cap
 *        by name without case. A name longer than 8.3 (12 characters) is
 *        counted in @p total and not listed.
 * @param[out] names Up to @p cap names.
 * @param cap        Room in @p names.
 * @param[out] count The number written.
 * @param[out] total The `.TXT` files of `HITEC/`, the record left out; may be NULL.
 * @return Ok, NoCard or ListError.
 */
HitecFiles hitecFilesList(HitecFileName *names, size_t cap, size_t *count, size_t *total = nullptr);

/**
 * @brief The name SAVE AS takes: one above the highest HTnnn of `HITEC/`,
 *        a directory of that form included.
 * @param[out] name `HITEC_FILES_NAME_MAX` bytes; empty unless Ok.
 * @return Ok, NoCard, ListError, or NoNumber once `HITEC/` holds
 *         `HITEC_FILES_SAVE_MAX` `.TXT` files, the record left out, or HT999
 *         is taken.
 */
HitecFiles hitecFilesSaveName(char *name);

/**
 * @brief SAVE AS: the servo file of @p img under hitecFilesSaveName()'s name.
 * @param img       The image of a full read.
 * @param[out] name The file name saved, `HITEC_FILES_NAME_MAX` bytes; empty
 *                  unless Ok.
 * @return hitecFilesSaveName()'s refusals, or the create's result.
 */
HitecFiles hitecFilesSaveAs(const hitec_image_t *img, char *name);

/**
 * @brief LOAD a file of `HITEC/`: a servo file, or a record's image before
 *        its write.
 * @param name     The file name.
 * @param series   The claim's `hitec_family_t`.
 * @param[out] out The image; untouched unless Ok.
 * @return Ok, NoCard, NotFound, TooLarge, ReadError, BadText or OtherSeries.
 */
HitecFiles hitecFilesLoad(const char *name, uint8_t series, hitec_image_t *out);

/**
 * @brief The Hitec backups of `BACKUP/`, latest first.
 * @return Ok, NoCard or ListError.
 */
HitecFiles hitecFilesBackups(BackupFileName *names, size_t cap, size_t *count);

/** @brief LOAD a Hitec backup of `BACKUP/`, as hitecFilesLoad() reads a file. */
HitecFiles hitecFilesBackupLoad(const char *name, uint8_t series, hitec_image_t *out);

/**
 * @brief The name the session's backup takes next in `BACKUP/`, for the
 *        write page: after a record to keep, the number after the record's.
 * @param[out] name `BACKUP_NAME_MAX` bytes; empty unless Ok.
 * @return Ok, NoCard or ListError.
 */
HitecFiles hitecFilesNextBackup(char *name);

/**
 * @brief The card calls before the first write of a session.
 * A record already on the card is moved into the ring first, the backup
 * @p backup saved after it, then @p rec written as `HITEC/HITEC.TXT`. A
 * record that cannot be moved, or a card that does not say whether one is
 * there, is left as it is, and no new record is written.
 * @param backup   The session's backup.
 * @param rec      The record of this write.
 * @param[out] out What was written.
 */
void hitecFilesFirstWrite(const hitec_image_t *backup, const hitec_card_record_t *rec,
                          HitecWrite *out);

/**
 * @brief The record of a later write of the session, over the session's own:
 *        when @p prev wrote it and it still holds the bytes written, in place;
 *        with no record on the card, a new file. A record of another session
 *        is left, and no record is written.
 * @param prev     The session's last HitecWrite.
 * @param rec      The record of this write.
 * @param[out] out What was written; backup Ok with no name.
 */
void hitecFilesRecordWrite(const HitecWrite *prev, const hitec_card_record_t *rec,
                           HitecWrite *out);

/**
 * @brief Whether the write may run: with no card, or when the card took the
 *        backup or the record. A card that took neither stops it. Pure.
 */
bool hitecFilesWriteAllowed(const HitecWrite *w);

/**
 * @brief Remove `HITEC/HITEC.TXT` after a verified write, when @p w wrote it
 *        and the file still holds the bytes it wrote.
 * @return Ok, also when nothing is removed; NoCard, ReadError, Denied or
 *         WriteError.
 */
HitecFiles hitecFilesRecordRemove(const HitecWrite *w);

/** @brief Whether `HITEC/HITEC.TXT` is on a mounted card. */
bool hitecFilesRecordPresent();

/**
 * @brief Read and parse `HITEC/HITEC.TXT`.
 * @param[out] out The record; untouched unless Ok.
 * @return Ok, NoCard, NotFound, TooLarge, ReadError or BadText.
 */
HitecFiles hitecFilesRecordLoad(hitec_card_record_t *out);
