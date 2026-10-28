/**
 * @file am32_backup.h
 * @brief The backup of an AM32 ESC (electronic speed controller) before the
 *        first write of an AM32 CONFIG session: its settings block as text,
 *        saved into the ring of `BACKUP/` (backup_ring.h) as `AMnnn.TXT`.
 *
 * The file follows the layout of rcbench's KST files: a header with the
 * format number, one line per field, the bytes in hex, and a checksum line
 * over every line before it, a CRC-32 (cyclic redundancy check) of IEEE
 * 802.3 in 8 hex digits.
 *
 * ```
 * AM32 ESC settings, format 1
 * ESC: F051
 * Settings address: 7C00
 * Layout revision: 3
 * Firmware: 2.21
 * MOTOR POLES = 14
 * ...                               one line per field the layout has
 * bytes 48
 * 00: 01 03 01 02 15 ...            16 bytes a line, 3 lines
 * checksum 1A2B3C4D
 * ```
 *
 * The fields are those of `AM32_FIELDS` that the block's layout revision has,
 * group, name, and value with its unit, as AM32 CONFIG shows them. The byte
 * lines hold all 48 bytes (`AM32_SETTINGS_SIZE`), so a byte no field covers is
 * kept too. The melody and the CAN (Controller Area Network) block, which the
 * editor neither shows nor writes, are not saved. Nothing reads the file back
 * yet.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "backup_ring.h"

/** @brief The file's format number. */
#define AM32_BACKUP_FORMAT 1

/** @brief Room for a backup's text: 39 fields of up to 48 characters, the header and the bytes. */
#define AM32_BACKUP_MAX 2560

/**
 * @brief CRC-32 of IEEE 802.3, reflected, over @p n bytes at @p p, as
 *        rcbench's KST files carry it: 0xCBF43926 for "123456789". Pure.
 */
uint32_t am32BackupCrc32(const char *p, size_t n);

/**
 * @brief The backup's text of @p settings. Pure.
 * @param settings The first `AM32_SETTINGS_SIZE` bytes of the settings region, as read.
 * @param escType  The ESC's MCU (microcontroller) family, as am32BlEscTypeName() names it.
 * @param address  The settings address, am32BlEepromAddr().
 * @param out      Destination.
 * @param cap      Room in @p out.
 * @return The number of bytes, no terminator; 0 when @p cap is too small.
 */
size_t am32BackupFormat(const uint8_t *settings, const char *escType, uint16_t address,
                        char *out, size_t cap);

/**
 * @brief Save @p settings as an AM32 backup into the ring.
 * @param settings  As am32BackupFormat().
 * @param escType   As am32BackupFormat().
 * @param address   As am32BackupFormat().
 * @param[out] name The file name saved, `BACKUP_NAME_MAX` bytes; empty unless Ok.
 * @return backupSave()'s result.
 */
BackupResult am32BackupSave(const uint8_t *settings, const char *escType, uint16_t address,
                            char *name);
