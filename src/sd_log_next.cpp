/**
 * @file sd_log_next.cpp
 * @brief The next blackbox log's file. @see sd_log_next.h
 */

#include "sd_log_next.h"

#include <stdio.h>

FRESULT sdLogScanNext(const char *dir, uint32_t *next) {
	DIR d;
	FRESULT fr = f_opendir(&d, dir);
	if (fr != FR_OK) return fr;
	uint32_t highest = 0;
	FILINFO fno;
	for (;;) {
		fr = f_readdir(&d, &fno);
		if (fr != FR_OK || fno.fname[0] == '\0') break;
		sdLogNoteName(&highest, fno.fname);
#if FF_USE_LFN
		sdLogNoteName(&highest, fno.altname);
#endif
	}
	f_closedir(&d);
	if (fr == FR_OK) *next = sdLogNextNumber(highest);
	return fr;
}

SdLogEnd sdLogOpenNext(SdLogNextCache *c, const FATFS *fs, const char *dir, FIL *file,
                       uint32_t *n) {
	// The logs live in their own directory, so the root holds a few
	// directories and a lookup in it reads one block. FR_EXIST is the
	// directory already there. This is also the call that mounts again if
	// the driver lost the card, so the id is read after it.
	FRESULT md = f_mkdir(dir);
	if (md != FR_OK && md != FR_EXIST) {
		sdLogNextForget(c);
		return SdLogEnd::OpenFailed;
	}

	// A scan that fails is a file that cannot be opened: the same cause.
	if (!c->known || c->fsId != fs->id) {
		if (sdLogScanNext(dir, &c->next) != FR_OK) {
			sdLogNextForget(c);
			return SdLogEnd::OpenFailed;
		}
		c->fsId = fs->id;
		c->known = true;
	}
	// After LOG99999.BFL. Never wrapped to a low number a file may hold.
	if (c->next == 0) return SdLogEnd::NoFreeNumber;

	// Created new, never over a file: one the scan did not count fails here
	// as an open, and the next start reads the directory again.
	char name[40];
	int len = snprintf(name, sizeof(name), "%s/LOG%05lu.BFL", dir, (unsigned long)c->next);
	if (len < 0 || len >= (int)sizeof(name) ||
	    f_open(file, name, FA_WRITE | FA_CREATE_NEW) != FR_OK) {
		sdLogNextForget(c);
		return SdLogEnd::OpenFailed;
	}
	*n = c->next;
	c->next = sdLogNextNumber(*n);
	return SdLogEnd::None;
}
