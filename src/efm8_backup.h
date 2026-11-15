/**
 * @file efm8_backup.h
 * @brief The backup of a Bluejay or BLHeli_S ESC (electronic speed
 *        controller): its settings page as first read in an ESC SETTINGS
 *        session, as rcbench's ESC settings text, saved into the ring of
 *        `BACKUP/` (backup_ring.h) as `BJnnn.TXT` or `BHnnn.TXT`.
 *
 * BLHeli_S and Bluejay are names of their owners' firmware, used here only
 * to say which ESCs this module works with.
 *
 * The text is rcbench's own, `ew_file_capture()` and `ew_file_format()`
 * (ew_file.h), format 1: the header, `Firmware:`, `Layout:`, `Processor:
 * EFM8`, `Signature: E8B2`, one line per field of the layout, the page's
 * bytes, 16 a line, and a CRC-32 (cyclic redundancy check) line over every
 * line before it. The byte lines hold the whole page, the three tags at
 * 0x40 to 0x6F among them, so a reflash's choice or a restore can name the
 * ESC's layout from the file (docs/design/bluejay-blheli-s.md, decisions 4
 * and 5).
 *
 * Core0 only; the save needs cardMounted() (card_dir.h).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "backup_ring.h"
#include "ew_id.h"

/**
 * @brief The ring's device of a firmware: Bluejay's `BJ`, BLHeli_S's `BH`.
 * @param fw An `ew_fw_t`.
 * @return `BackupDevice::Count` for any other firmware: no backup.
 */
BackupDevice efm8BackupDevice(uint8_t fw);

/**
 * @brief The backup's text of a page. Pure.
 * @param c    The first contact's answer: the signature.
 * @param id   What identification found: the firmware, layout and length.
 * @param page The page as read, `id->len` bytes.
 * @param out  Takes the text, no terminator.
 * @param cap  Room in @p out; `EW_FILE_MAX`, 5280, always fits.
 * @return The number of bytes; 0 for a page rcbench does not capture or a
 *         text that does not fit.
 */
size_t efm8BackupFormat(const ew_contact_t *c, const ew_ident_t *id, const uint8_t *page, char *out,
                        size_t cap);

/**
 * @brief Save a page's backup into the ring, as the next number.
 * @param c        As efm8BackupFormat().
 * @param id       As efm8BackupFormat().
 * @param page     As efm8BackupFormat().
 * @param[out] name The file name saved, `BACKUP_NAME_MAX` bytes; empty unless Ok.
 * @param skipSame When the ring's latest backup is this device's and holds
 *                 the same text byte for byte, save nothing and name it:
 *                 a read-only list opened again on the same ESC does not
 *                 push older backups out of the ring.
 * @return backupSave()'s result; WriteError for a page with no text.
 */
BackupResult efm8BackupSave(const ew_contact_t *c, const ew_ident_t *id, const uint8_t *page, char *name,
                            bool skipSame = false);

/**
 * @brief The name the next backup of @p fw takes, for the write page's
 *        backup line: the number backupSave() would claim now.
 * @param fw        An `ew_fw_t`.
 * @param[out] name `BACKUP_NAME_MAX` bytes; empty unless Ok.
 * @return NoCard without a mounted card; NotFound for a firmware with no
 *         prefix; a listing failure as backupNames().
 */
BackupResult efm8BackupNext(uint8_t fw, char *name);
