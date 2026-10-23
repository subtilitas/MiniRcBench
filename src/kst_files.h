/**
 * @file kst_files.h
 * @brief The KST files on the SD (Secure Digital) card: rcbench's file store
 *        (`kst_store.h`) over FatFs, and the record of a write under way.
 *
 * Every function runs on core0, the only core that touches the card. Paths
 * are relative to the card's root, as the blackbox logs' are: the files are
 * `KST/<name>.TXT`.
 *
 * The card counts as mounted while the logger has it mounted (SdLogState
 * Idle or Logging) and the USB (Universal Serial Bus) card reader is off
 * (MscState::Idle). While a USB host holds the card, or the reader hands it
 * over or takes it back, every call answers as if no card were fitted.
 *
 * | Situation | Call | Effect |
 * |---|---|---|
 * | SAVE AS, LOAD, the FILES list | kstFilesIo() with `kst_store.h` | the store's own rules: no file is replaced, `KST.TXT` is refused |
 * | First write of a session | kstFilesFirstWrite() | the backup as the next KSTnnn.TXT, then the image before the write as `KST/KST.TXT` |
 * | Write verified, or LEAVE from a servo in a known state | kstFilesRecordRemove() | `KST/KST.TXT` removed, when this session wrote it |
 * | The screen opens | kstFilesRecordPresent(), kstFilesRecordLoad() | an interrupted write's record found and read |
 *
 * A record left by an interrupted write is renamed to the next KSTnnn.TXT
 * before a new one is written, so no record is lost; one that stays in place
 * is never written over or removed.
 *
 * Current limitations, from FatFs on this board:
 * - A write-protected card reports `KST_STORE_WRITE_ERROR`, not
 *   `KST_STORE_DENIED`: FatFs returns FR_WRITE_PROTECTED only when the disk
 *   layer sets STA_PROTECT, which this board's card driver never does.
 * - A write error that persists, as a card that refuses writes gives:
 *   FatFs keeps the sector it could not write in its window, the create's
 *   remove fails with FR_DISK_ERR, and every later call that moves the window
 *   fails with FR_DISK_ERR until the card is mounted again (SD LOG's RETRY,
 *   the USB card reader's hand-back or a power cycle), the list, LOAD and
 *   the record lookup among them. The new file's 0-byte entry stays in
 *   that sector and reaches the card if writes work again before a remount.
 * - A rename that stops between registering the new name and removing the
 *   old one leaves both on the card, two entries sharing one cluster chain.
 *   The first write reports it as not kept and writes no record; removing
 *   one of the two names on a computer frees the other's data, and later
 *   sessions keep the second name as one more KSTnnn.TXT.
 * - A create whose close fails with a clean window, a data sector that does
 *   not take its write, leaves its file on the card, open in FatFs, which
 *   refuses to remove it (FR_LOCKED) or to read it. It holds one of FatFs's
 *   16 lock entries (`FF_FS_LOCK`) until the card is mounted again.
 *
 * @see docs/design/kst-programming.md, "Storage".
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kst_reg.h"
#include "kst_store.h"

/** @brief The record's file name in `KST_STORE_DIR`. */
#define KST_FILES_RECORD_NAME KST_STORE_RECORD KST_STORE_EXT

/** @brief What the first write of a session put on the card. */
struct KstFirstWrite {
	uint8_t backup;                          /**< `kst_store_t` of the backup's save. */
	char    backupName[KST_STORE_NAME_MAX];  /**< Its file name; empty unless saved. */
	uint8_t record;                          /**< `kst_store_t` of the record. */
	uint8_t kept;                            /**< `kst_store_t` of keeping an old record; OK when there was none. */
	char    keptName[KST_STORE_NAME_MAX];    /**< The old record's new file name; empty for none. */
};

/** @brief Whether a card is mounted and not held by a USB host. */
bool kstFilesMounted();

/** @brief The card for `kst_store.h`: KSTnnn names, SAVE AS, LOAD, the list. */
const kst_store_io_t *kstFilesIo();

/**
 * @brief The card calls before the first write of a session.
 *
 * Saves @p backup as the next KSTnnn.TXT (kst_store_preset() and
 * kst_store_save()), then writes @p before as `KST/KST.TXT`. A record already
 * there is renamed to the next KSTnnn.TXT first; a record that cannot be
 * renamed is left as it is, and no new one is written. A rename FatFs
 * reports as failed that still took effect counts as kept: the record is
 * looked up under both names. A card that does not say whether a record is
 * there (an error other than "no file") writes no record either: `kept` is
 * `KST_STORE_READ_ERROR`.
 *
 * @param backup  The session's backup.
 * @param before  The image the write starts from.
 * @param[out] out What was written.
 */
void kstFilesFirstWrite(const kst_image_t *backup, const kst_image_t *before,
                        KstFirstWrite *out);

/**
 * @brief Whether the write may run after kstFilesFirstWrite(): with no card,
 *        or when the card took the backup or the record. A card that took
 *        neither stops it. Pure.
 */
bool kstFilesWriteAllowed(const KstFirstWrite *w);

/**
 * @brief Remove `KST/KST.TXT` after a verified write, or at LEAVE from a
 *        servo in a known state, when this session's kstFilesFirstWrite()
 *        wrote it.
 * @param w The session's kstFilesFirstWrite() result; a record it did not
 *          write, or NULL, removes nothing.
 * @return `KST_STORE_OK`, also when nothing is removed; `KST_STORE_NO_CARD`;
 *         `KST_STORE_DENIED` for a read-only file; `KST_STORE_WRITE_ERROR`.
 */
kst_store_t kstFilesRecordRemove(const KstFirstWrite *w);

/** @brief Whether `KST/KST.TXT` is on a mounted card. */
bool kstFilesRecordPresent();

/**
 * @brief Read `KST/KST.TXT` into @p buf, as kst_store_load() reads a file.
 * @param[out] buf The file's bytes.
 * @param cap      Room in @p buf.
 * @param[out] len The number of bytes, 0 unless `KST_STORE_OK`; untouched for
 *                 `KST_STORE_ARG`. No terminator is written.
 * @return `KST_STORE_OK`, `KST_STORE_ARG` for a NULL @p buf or @p len,
 *         `KST_STORE_NO_CARD`, `KST_STORE_NOT_FOUND`,
 *         `KST_STORE_TOO_LARGE` or `KST_STORE_READ_ERROR`.
 */
kst_store_t kstFilesRecordLoad(char *buf, size_t cap, size_t *len);
