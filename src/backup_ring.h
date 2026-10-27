/**
 * @file backup_ring.h
 * @brief Device backups on the SD (Secure Digital) card: one directory,
 *        `BACKUP`, and one ring of 50 numbers for every device.
 *
 * A tool that writes settings into a device saves the device's settings, as
 * read before the session's first write, here first. A backup is
 * `BACKUP/<prefix><nnn>.TXT`: the device's prefix (`KST`, `AM`, `HT`) and a
 * ring number from 001 to 050 (`BACKUP_RING_NAMES`). The numbers are one
 * sequence for all devices. `BACKUP/LAST` holds the latest number as 3 digits
 * and a line feed.
 *
 * The next backup takes the number after the latest, 001 after 050:
 * 1. `BACKUP/LAST` is rewritten in place with the new number;
 * 2. a file of the same device under that number is written over in place; a
 *    file of another device under it is renamed to the new name first, which
 *    keeps its cluster, and then written over; further files under the
 *    number are removed;
 * 3. no file under the number: a new file.
 *
 * A save that stops part way leaves the number claimed, and the old file or
 * part of the new bytes under it; the latest backup is never the one written
 * over. A marker the card refuses stops the save with nothing written over.
 * Without a readable marker the latest number is guessed from the names
 * (backupGuess()). A directory with a ring name counts as taken, is passed
 * over and never written. Any other name is outside the ring: never written
 * over, never removed. The directory holds at most 50 backups and the
 * marker: 53 entries with `.` and `..`, 4 sectors on FAT32.
 *
 * Core0 only; every card call needs cardMounted() (card_dir.h).
 *
 * @see docs/design/backups.md
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief The directory of the backups, from the root. */
#define BACKUP_DIR "BACKUP"

/** @brief The ring's numbers, 001 to 050. */
#define BACKUP_RING_NAMES 50

/** @brief The marker's file name in `BACKUP_DIR`: no extension, so no list shows it. */
#define BACKUP_MARKER_NAME "LAST"

/** @brief The marker's bytes: 3 digits and a line feed. */
#define BACKUP_MARKER_LEN 4

/** @brief Room for a backup's file name, `KST050.TXT`, and its terminator. */
#define BACKUP_NAME_MAX 11

/** @brief One backup's file name. */
typedef char BackupFileName[BACKUP_NAME_MAX];

/** @brief The devices with backups. The numbers are stable. */
enum class BackupDevice : uint8_t {
	Kst = 0,    /**< KST and Chaservo servos: `KSTnnn.TXT`. */
	Am32 = 1,   /**< AM32 ESCs (electronic speed controllers): `AMnnn.TXT`. */
	Hitec = 2,  /**< Hitec servos: `HTnnn.TXT`, reserved; nothing writes one yet. */
	Count = 3,
};

/** @brief The result of a backup call. The numbers are stable. */
enum class BackupResult : uint8_t {
	Ok = 0,
	NoCard,      /**< No card mounted, or the card held by a USB host. */
	ListError,   /**< `BACKUP` does not read to its end. */
	Full,        /**< No space on the card, or a full directory. */
	Denied,      /**< Write protected, or a file or directory in the way. */
	WriteError,
	NotFound,
	TooLarge,    /**< The file is larger than the buffer. */
	ReadError,
};

/** @brief Which numbers the directory holds, by which devices, and the latest. */
struct BackupNames {
	uint64_t taken;    /**< Bit n - 1 for number n, a file or a directory, any device, any case. */
	uint64_t dirs;     /**< The same bit for a directory: never written. */
	uint8_t  devices[BACKUP_RING_NAMES]; /**< Per number, bit d for a file of BackupDevice d. */
	unsigned marked;   /**< The number in `BACKUP/LAST`; 0 when it is missing or unreadable. */
	unsigned latest;   /**< `marked`, or without it backupGuess(); 0 for none. */
};

/** @brief The prefix of @p d's names: `KST`, `AM` or `HT`. */
const char *backupPrefix(BackupDevice d);

/** @brief The number after @p n in the ring, 1 to 50: 1 after 50 and after 0. Pure. */
unsigned backupAfter(unsigned n);

/**
 * @brief The latest number of a ring without a marker, guessed from its
 *        names, 0 for none: the highest until 50 is taken, then the one
 *        before the lowest free number; with all 50 taken, 50. Pure.
 */
unsigned backupGuess(uint64_t taken);

/** @brief The number @p len bytes of a marker hold, 1 to 50; 0 for any other bytes. Pure. */
unsigned backupMarker(const char *text, size_t len);

/** @brief The number the next backup takes: after `latest`, past directories. Pure. */
unsigned backupNext(const BackupNames *k);

/**
 * @brief The number of a ring name and its device; 0 for any other name.
 *        Case does not count. Pure.
 * @param name      A file name, `KST017.TXT`.
 * @param[out] dev  The device; may be NULL.
 */
unsigned backupNumber(const char *name, BackupDevice *dev);

/** @brief `<prefix><nnn>.TXT` of @p d and number @p n into @p out, `BACKUP_NAME_MAX` bytes. */
void backupName(BackupDevice d, unsigned n, char *out);

/** @brief Which numbers `BACKUP` holds, and its latest: one walk and a read of the marker. */
BackupResult backupNames(BackupNames *out);

/**
 * @brief Claim number @p n: `BACKUP/LAST` names it. Nothing else is touched.
 *        A refusal leaves `latest` as it was.
 */
BackupResult backupClaim(BackupNames *k, unsigned n);

/**
 * @brief Put @p len bytes of @p text under number @p n as a backup of @p d,
 *        after backupClaim(): over a file of @p d there, over another
 *        device's file there once renamed, or as a new file. @p name
 *        receives the file name, `BACKUP_NAME_MAX` bytes, and stays
 *        untouched unless Ok.
 */
BackupResult backupPut(BackupNames *k, BackupDevice d, unsigned n, const char *text,
                       size_t len, char *name);

/**
 * @brief Move the file at @p fromPath, a path from the root, into the ring
 *        as number @p n of @p d, after backupClaim(): the files under @p n
 *        are removed, then the file is renamed. A rename that FatFs reports
 *        as failed but took effect counts as moved: the file is looked up
 *        under both names. @p name receives the file name,
 *        `BACKUP_NAME_MAX` bytes, and stays untouched unless Ok.
 */
BackupResult backupMoveIn(BackupNames *k, BackupDevice d, unsigned n, const char *fromPath,
                          char *name);

/**
 * @brief Save a backup of @p d: backupNames(), backupClaim() and backupPut()
 *        for the next number, in one scope of `BACKUP`.
 * @param d         The device.
 * @param text      The file's bytes.
 * @param len       Their count.
 * @param[out] name The file name saved; empty unless Ok.
 */
BackupResult backupSave(BackupDevice d, const char *text, size_t len, char *name);

/**
 * @brief The backups of @p d, latest first: from `latest` down to 1, then
 *        from 50 down.
 * @param k          From backupNames(); the devices per number.
 * @param d          The device.
 * @param[out] names Up to @p cap file names of `BACKUP_NAME_MAX` bytes each.
 * @param cap        Room in @p names.
 * @return The number written.
 */
size_t backupList(const BackupNames *k, BackupDevice d, BackupFileName *names, size_t cap);

/**
 * @brief Read the backup @p name into @p buf; @p len receives the count. No
 *        terminator is written.
 */
BackupResult backupLoad(const char *name, char *buf, size_t cap, size_t *len);
