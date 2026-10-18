/**
 * @file fake_fatfs.h
 * @brief An in-memory FAT volume behind the FatFs calls of test/stubs/ff.h,
 *        for kst_files.cpp.
 *
 * Names compare without case, as FAT does. Paths are relative to the root and
 * use `/`. The volume models what kst_files.cpp meets on a card: a missing
 * directory or file, a name that exists, a write-protected card, a full
 * volume (a short write that returns FR_OK, as FatFs does), a full directory
 * (FR_DENIED on a create), and any call failing with a given FRESULT.
 * It does not model clusters, the FAT or a removed card.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ff.h"

/** @brief The FatFs calls the volume can fail on demand. */
enum class FakeFatCall : uint8_t {
	Open, Close, Read, Write, Sync, Stat, OpenDir, ReadDir, MkDir, Unlink, Rename, Count,
};

/** @brief An empty volume with room for @p capacity bytes of file data. */
void fakeFatReset(uint32_t capacity = 1u << 20);

/** @brief A write-protected card: every call that would change the volume fails. */
void fakeFatWriteProtect(bool on);

/** @brief A directory full at @p entries entries: a create beyond fails FR_DENIED. */
void fakeFatDirLimit(uint32_t entries);

/** @brief Fail the @p skip + 1'th next call of @p call with @p fr. */
void fakeFatFail(FakeFatCall call, FRESULT fr, unsigned skip = 0);

/** @brief Put a file with @p len bytes of @p data on the volume. */
void fakeFatPut(const char *path, const char *data, size_t len);

/** @brief Put a directory on the volume. */
void fakeFatMkdir(const char *path);

/** @brief Whether @p path exists, file or directory. */
bool fakeFatExists(const char *path);

/** @brief Copy a file's bytes into @p buf. @return Its size, -1 for none. */
int32_t fakeFatGet(const char *path, char *buf, size_t cap);

/** @brief Files and directories on the volume. */
unsigned fakeFatEntries();

/** @brief Files open now; 0 after every call that closes what it opened. */
int fakeFatOpenFiles();

/** @brief Calls of @p call since fakeFatReset(). */
unsigned fakeFatCalls(FakeFatCall call);
