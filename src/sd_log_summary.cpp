/**
 * @file sd_log_summary.cpp
 * @brief A log's run summary as its `.TXT`. @see sd_log_summary.h
 */

#include "sd_log_summary.h"

#include <stdio.h>

RunSumFile sdLogWriteSummary(FIL *file, const char *dir, uint32_t n,
                             const char *text, uint32_t len) {
	char name[RUN_SUM_NAME_BYTES];
	runSumTxtName(n, name);
	char path[40];
	int plen = snprintf(path, sizeof(path), "%s/%s", dir, name);
	if (plen < 0 || plen >= (int)sizeof(path)) return RunSumFile::WriteFailed;

	FRESULT fr = f_open(file, path, FA_WRITE | FA_CREATE_ALWAYS);
	if (fr != FR_OK) return sdLogSummaryOpenEnd((int)fr);

	UINT wrote = 0;
	fr = f_write(file, text, len, &wrote);
	RunSumFile result = RunSumFile::Saved;
	if (fr != FR_OK || wrote != len) result = sdLogSummaryWriteEnd((int)fr, (uint32_t)wrote, len);
	if (f_close(file) != FR_OK && result == RunSumFile::Saved) result = RunSumFile::WriteFailed;
	if (result != RunSumFile::Saved) f_unlink(path);
	return result;
}
