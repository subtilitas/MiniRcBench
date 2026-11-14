/**
 * @file sd_log_next.h
 * @brief The next blackbox log's file: `LOGS/`, the scan for the highest
 *        `LOGnnnnn.BFL`, the number kept between starts, and the open.
 *
 * Over FatFs only, with no card driver, so the host runs it against FatFs
 * R0.15 itself on a RAM disk (`make -C test lognum`, test/log_next_fatfs.cpp).
 * sd_log.cpp calls it from sdLogStart() on core0.
 *
 * The number is kept for one FatFs mount. FatFs gives every mount a new
 * `FATFS::id`; the board has no card-detect line, and after a card fault the
 * driver reports the card uninitialised, so FatFs mounts again on its next
 * call, without sdLogBegin(). A card swapped or edited in a card reader in
 * between is then read again: the number is read again whenever the id
 * differs from the one it was read under, and after any start that fails.
 */

#pragma once

#include <stdint.h>

#include "ff.h"
#include "sd_log.h"

/** @brief The next log's number, as read from `LOGS/` on one mount. */
struct SdLogNextCache {
	uint32_t next;   /**< The number the next log takes; 0 for none free. */
	WORD     fsId;   /**< `FATFS::id` of the mount the number was read on. */
	bool     known;  /**< `next` is valid on the mount `fsId` names. */
};

/**
 * @brief Forget the number: the next start reads `LOGS/` again.
 * @param c The cache.
 */
static inline void sdLogNextForget(SdLogNextCache *c) { c->known = false; }

/**
 * @brief Read @p dir once for the next log's number.
 *
 * One f_opendir() and one f_readdir() per entry: every directory entry is
 * read once, 32 bytes each, 16 to a 512-byte sector, so N logs with short
 * names cost about N / 16 sector reads (a long name adds an entry per 13
 * characters). Both of an entry's names count: the long one FatFs reports,
 * and its 8.3 alias when it has one (sdLogNoteName()).
 *
 * @param dir The directory, `SD_LOG_DIR_PATH` on the board.
 * @param[out] next The number, 0 for none free (sdLogNextNumber()). Set only
 *        on FR_OK.
 * @return The FatFs result; anything but FR_OK is a scan that failed.
 */
FRESULT sdLogScanNext(const char *dir, uint32_t *next);

/**
 * @brief Make @p dir if it is missing, find the next number and create its
 *        file, never over an existing one.
 *
 * The number comes from @p c while it is known for the mount @p fs is now
 * on (`FATFS::id`, read after f_mkdir(), which mounts again if the driver
 * lost the card); otherwise from sdLogScanNext(). A failed f_mkdir(), scan
 * or open forgets it.
 *
 * @param c The cache, kept by the caller across starts.
 * @param fs The volume object the card is mounted with.
 * @param dir The directory, `SD_LOG_DIR_PATH` on the board.
 * @param[out] file Open for writing on success.
 * @param[out] n The log's number, set on success.
 * @return SdLogEnd::None with @p file open; SdLogEnd::OpenFailed for a
 *         directory not made or read or a file not created;
 *         SdLogEnd::NoFreeNumber after `LOG99999.BFL`.
 */
SdLogEnd sdLogOpenNext(SdLogNextCache *c, const FATFS *fs, const char *dir, FIL *file,
                       uint32_t *n);
