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
 * | LOAD, the FILES list | kstFilesIo() with `kst_store.h` | the store's own rules: `KST.TXT` is refused |
 * | SAVE AS, the name it proposes | kstFilesSaveNext(), kstFilesNextName() | the ring's next name, `KST/LAST` updated, the ring's oldest file replaced |
 * | First write of a session | kstFilesFirstWrite() | the backup as the next name of the ring, then the image before the write as `KST/KST.TXT` |
 * | Write verified, or LEAVE from a servo in a known state | kstFilesRecordRemove() | `KST/KST.TXT` removed, when this session wrote it and it holds the bytes written |
 * | The screen opens | kstFilesRecordPresent(), kstFilesRecordLoad() | an interrupted write's record found and read |
 *
 * A record left by an interrupted write is renamed to the next KSTnnn.TXT
 * before a new one is written, so no record is lost; one that stays in place
 * is never written over or removed.
 *
 * The ring. Every numbered file this layer writes, a backup, a kept record
 * or a SAVE AS, takes a name from KST001 to KST050 (`KST_FILES_NAMES`), so
 * the `KST` directory holds at most 50 of them, the record and the marker:
 * 54 entries with `.` and `..`, 4 sectors of 512 bytes. The marker,
 * `KST/LAST`, holds the number of the latest file as 3 digits and a line
 * feed. The next file takes the number after it, KST001 after KST050; a
 * file already under that number, the oldest of a full ring, is removed
 * first. The marker is written before the removal and the file: a save that
 * stops between them leaves a name unused, never the latest file removed. A
 * marker that does not take the number stops the save with the marker's
 * result, read as a create's, and nothing removed. Without a readable marker the latest number is guessed from the
 * names (kstNamesGuess()). A directory with a KSTnnn name counts as taken,
 * is passed over and never removed. Names from KST051 up, and any other
 * name, are outside the ring: never proposed, never removed, and not
 * bounded.
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

/** @brief The numbered names of the ring, KST001 to KST050. */
#define KST_FILES_NAMES 50

/** @brief The marker's file name in `KST_STORE_DIR`: no extension, so no list shows it. */
#define KST_FILES_MARKER_NAME "LAST"

/** @brief The marker's bytes: 3 digits and a line feed. */
#define KST_FILES_MARKER_LEN 4

/** @brief Which names of the ring the `KST` directory holds, and the latest. */
struct KstNames {
	uint64_t taken;   /**< Bit n - 1 for KSTnnn.TXT, a file or a directory, in any case. */
	uint64_t dirs;    /**< The same bit for a directory: never removed. */
	unsigned marked;  /**< The number in `KST/LAST`; 0 when it is missing or unreadable. */
	unsigned latest;  /**< `marked`, or without it kstNamesGuess(); 0 for none. */
};

/** @brief The number after @p n in the ring, 1 to 50: KST001 after KST050 and after 0. Pure. */
unsigned kstNamesAfter(unsigned n);

/**
 * @brief The latest number of a ring without a marker, guessed from its
 *        names, 0 for none: the highest until KST050 is taken, then the one
 *        before the lowest free number; with all 50 taken, 50. Pure.
 * @param taken `KstNames::taken`.
 */
unsigned kstNamesGuess(uint64_t taken);

/** @brief The number the next file takes: after `latest`, past directories. Pure. */
unsigned kstNamesNext(const KstNames *k);

/** @brief The number @p len bytes of a marker hold, 1 to 50; 0 for any other bytes. Pure. */
unsigned kstNamesMarker(const char *text, size_t len);

/** @brief The base name of number @p n, `KSTnnn`, into @p out, `KST_STORE_PRESET_MAX` bytes. */
void kstNamesBase(unsigned n, char *out);

/**
 * @brief Which names of the ring the card holds, and its latest: one walk of
 *        `KST` and a read of `KST/LAST`.
 * @return `KST_STORE_OK`, `KST_STORE_NO_CARD` or `KST_STORE_LIST_ERROR`;
 *         @p out is empty unless OK.
 */
kst_store_t kstFilesNames(KstNames *out);

/**
 * @brief The base name SAVE AS and the first write's backup take next, in
 *        place of `kst_store_preset()`.
 * @param[out] base `KSTnnn`, `KST_STORE_PRESET_MAX` bytes; empty unless `KST_STORE_OK`.
 */
kst_store_t kstFilesNextName(char *base);

/**
 * @brief SAVE AS: save @p len bytes of @p text under the ring's next name:
 *        the marker, the removal of the file under that name, the create.
 * @param text      The file's bytes.
 * @param len       Their count.
 * @param[out] name The file name saved, `KSTnnn.TXT`, `KST_STORE_NAME_MAX`
 *                  bytes; empty unless `KST_STORE_OK`.
 * @return `KST_STORE_OK`, `KST_STORE_NO_CARD`, `KST_STORE_LIST_ERROR`,
 *         or, for the marker and then the file, the create's `KST_STORE_EXISTS`, `KST_STORE_CARD_FULL`,
 *         `KST_STORE_DENIED` or `KST_STORE_WRITE_ERROR`.
 */
kst_store_t kstFilesSaveNext(const char *text, size_t len, char *name);

/**
 * @brief Put the ring's latest file first in a list from `kst_store_list()`:
 *        the run of KSTnnn files, by falling number, is rotated so that it
 *        falls from @p latest. Pure.
 * @param entries The list, rotated in place.
 * @param count   Its rows.
 * @param latest  `KstNames::latest`.
 */
void kstFilesOrder(kst_store_entry_t *entries, size_t count, unsigned latest);

/** @brief What the first write of a session put on the card. */
struct KstFirstWrite {
	uint8_t backup;                          /**< `kst_store_t` of the backup's save. */
	char    backupName[KST_STORE_NAME_MAX];  /**< Its file name; empty unless saved. */
	uint8_t record;                          /**< `kst_store_t` of the record. */
	uint8_t kept;                            /**< `kst_store_t` of keeping an old record; OK when there was none. */
	char    keptName[KST_STORE_NAME_MAX];    /**< The old record's new file name; empty for none. */
	uint32_t recordSum;                      /**< FNV-1a (Fowler-Noll-Vo) hash of the record's bytes. */
};

/** @brief Whether a card is mounted and not held by a USB host. */
bool kstFilesMounted();

/**
 * @brief FatFs's current directory in `KST` for the object's life, and the
 *        root again at its end, so the blackbox logger's paths are unchanged.
 *
 * A path through `KST/` looks `KST` up in the root, and every blackbox log
 * in the root lengthens that lookup. Every function here opens a scope of
 * its own, so each looks `KST` up once; an outer scope around several calls,
 * a FILES page that lists, names and looks for the record, makes it once for
 * all of them. Scopes nest. Without a mounted card, or without `KST` on it,
 * the scope does nothing and the paths name `KST/` from the root. Core0 only;
 * no blackbox log call may run inside one.
 */
class KstFilesScope {
public:
	/** @brief Change into `KST` when a card is mounted and no outer scope has. */
	KstFilesScope();
	/** @brief Back to the root when this is the outermost scope that changed. */
	~KstFilesScope();
	KstFilesScope(const KstFilesScope &) = delete;
	KstFilesScope &operator=(const KstFilesScope &) = delete;
private:
	bool m_on;
};

/** @brief The card for `kst_store.h`: KSTnnn names, SAVE AS, LOAD, the list. */
const kst_store_io_t *kstFilesIo();

/**
 * @brief The card calls before the first write of a session.
 *
 * Saves @p backup under the ring's next name, then writes @p before as
 * `KST/KST.TXT`. A record already there is renamed to the ring's next name
 * first; each takes a name the way kstFilesSaveNext() does, and both names
 * come from one walk of the directory; a record that cannot be
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
 *        wrote it and the file still holds the bytes it wrote. The board has
 *        no card-detect line: a card swapped in its place is not seen as an
 *        unmount, and its record, with other bytes, stays.
 * @param w The session's kstFilesFirstWrite() result; a record it did not
 *          write, or NULL, removes nothing.
 * @return `KST_STORE_OK`, also when nothing is removed; `KST_STORE_NO_CARD`;
 *         `KST_STORE_READ_ERROR` when the file cannot be read back;
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
