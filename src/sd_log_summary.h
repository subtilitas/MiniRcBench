/**
 * @file sd_log_summary.h
 * @brief A log's run summary as `LOGS/LOGnnnnn.TXT`, beside its `.BFL`.
 *
 * Over FatFs only, with no card driver, so the host runs it against FatFs
 * R0.15 itself on a RAM disk (`make -C test lognum`,
 * test/sd_log_fatfs.cpp). sd_log.cpp calls it from sdLogStop() on core0,
 * after the `.BFL` has closed clean.
 *
 * @see docs/design/run-summary.md, "The summary file"
 */

#pragma once

#include <stdint.h>

#include "ff.h"
#include "run_summary.h"

/**
 * @brief What a failed write of the `.TXT` says about the card, as
 *        sdLogWriteEnd() reads a write of the log. Pure.
 * @param fr    The FRESULT, 0 for FR_OK.
 * @param wrote Bytes FatFs reports written.
 * @param asked Bytes handed to it.
 * @return RunSumFile::CardFull for a short write with FR_OK, else
 *         RunSumFile::WriteFailed.
 */
static inline RunSumFile sdLogSummaryWriteEnd(int fr, uint32_t wrote, uint32_t asked) {
	return sdLogWriteEnd(fr, wrote, asked) == SdLogEnd::CardFull ? RunSumFile::CardFull
	                                                             : RunSumFile::WriteFailed;
}

/**
 * @brief What a failed open of the `.TXT` says about the card. Pure.
 *
 * FatFs R0.15 refuses the create with FR_DENIED when the directory has no
 * free entry and no free cluster to grow by (dir_register()): a full card.
 * It also gives FR_DENIED for a read-only file or a directory of that
 * name, which only a computer makes; those read as a full card as well.
 *
 * @param fr The FRESULT of f_open(), not FR_OK.
 * @return RunSumFile::CardFull for FR_DENIED, else RunSumFile::WriteFailed.
 */
static inline RunSumFile sdLogSummaryOpenEnd(int fr) {
	return fr == FR_DENIED ? RunSumFile::CardFull : RunSumFile::WriteFailed;
}

/**
 * @brief Write @p text as `dir/LOGnnnnn.TXT`, over any file of that name.
 *
 * One f_open() with `FA_CREATE_ALWAYS`, one f_write(), one f_close(). A
 * write or close that fails, or writes fewer bytes than asked, leaves no cut
 * summary: the file is closed and removed (f_unlink()) when the card still
 * accepts that. A close that fails leaves the file open to FatFs, whose lock
 * refuses the removal until the next mount; the entry then on the card is
 * an empty `.TXT`, as its size is written only by a sync that succeeds. No
 * other file is touched.
 *
 * @param file A file object not open, used here and closed again; the
 *        logger passes its `.BFL`'s, closed by then.
 * @param dir  The directory, `SD_LOG_DIR_PATH` on the board.
 * @param n    The log's number.
 * @param text The text, runSumText()'s.
 * @param len  Its length in bytes.
 * @return RunSumFile::Saved, or why not: RunSumFile::CardFull or
 *         RunSumFile::WriteFailed.
 */
RunSumFile sdLogWriteSummary(FIL *file, const char *dir, uint32_t n,
                             const char *text, uint32_t len);
