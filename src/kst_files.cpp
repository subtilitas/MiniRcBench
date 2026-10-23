/**
 * @file kst_files.cpp
 * @brief The KST files over FatFs. @see kst_files.h
 */

#include "kst_files.h"
#include "kst_file.h"
#include "sd_log.h"
#include "usb_msc.h"

#include "ff.h"
#include <stdio.h>
#include <string.h>

/** @brief Room for `KST/` and a file name. */
#define KST_FILES_PATH_MAX (sizeof(KST_STORE_DIR) + KST_STORE_NAME_MAX)

/** @brief A file's text, for the backup and the record. */
static char s_text[KST_FILE_MAX];

/**
 * @brief The one file open at a time, and the one directory entry looked at.
 *        A FIL holds a 512-byte sector buffer and a FILINFO a 256-byte name:
 *        static, off core0's 2 KB stack. Core0 alone calls in here, no file
 *        call nests in another, and no caller keeps s_fno across a call that
 *        uses it.
 */
static FIL     s_fil;
static FILINFO s_fno;

/** @brief FNV-1a (Fowler-Noll-Vo), 32 bits, of @p n bytes at @p p. */
static uint32_t fnv1a(const char *p, size_t n) {
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)p[i]) * 16777619u;
	return h;
}

bool kstFilesMounted() {
	SdLogStatus st;
	sdLogStatus(&st);
	bool mounted = st.state == SdLogState::Idle || st.state == SdLogState::Logging;
	return mounted && mscGetState() == MscState::Idle;
}

/** @brief `KST/<name>` into @p out. */
static void pathOf(const char *name, char *out) {
	snprintf(out, KST_FILES_PATH_MAX, "%s/%s", KST_STORE_DIR, name);
}

// ---------------------------------------------------------------------------
// kst_store_io_t
// ---------------------------------------------------------------------------

static bool ioMounted(void *) { return kstFilesMounted(); }

static bool ioWalk(void *, kst_store_visit_fn visit, void *visitCtx) {
	DIR dir;
	FRESULT fr = f_opendir(&dir, KST_STORE_DIR);
	// No directory yet: no entry, and the walk reads to its end.
	if (fr == FR_NO_PATH || fr == FR_NO_FILE) return true;
	if (fr != FR_OK) return false;
	bool ok = true;
	for (;;) {
		FILINFO &fno = s_fno;
		fr = f_readdir(&dir, &fno);
		if (fr != FR_OK) {
			ok = false;
			break;
		}
		if (fno.fname[0] == '\0') break;
		visit(fno.fname, (fno.fattrib & AM_DIR) != 0, visitCtx);
	}
	f_closedir(&dir);
	return ok;
}

static int32_t ioSize(void *, const char *name) {
	char path[KST_FILES_PATH_MAX];
	pathOf(name, path);
	if (f_stat(path, &s_fno) != FR_OK || (s_fno.fattrib & AM_DIR)) return -1;
	return s_fno.fsize > INT32_MAX ? INT32_MAX : (int32_t)s_fno.fsize;
}

static int32_t ioRead(void *, const char *name, char *buf, size_t cap) {
	char path[KST_FILES_PATH_MAX];
	pathOf(name, path);
	if (f_open(&s_fil, path, FA_READ) != FR_OK) return -1;
	UINT got = 0;
	FRESULT fr = f_read(&s_fil, buf, (UINT)cap, &got);
	f_close(&s_fil);
	return fr == FR_OK ? (int32_t)got : -1;
}

/**
 * @brief The `kst_store_io_result_t` of a FatFs error while creating @p path.
 *
 * FatFs answers a full volume or directory with FR_DENIED, and an existing
 * directory or read-only object of that name too: the name tells them apart.
 * FR_WRITE_PROTECTED needs the disk layer's STA_PROTECT, which this board's
 * card driver never sets; a card that refuses writes fails as FR_DISK_ERR.
 */
static kst_store_io_result_t createError(FRESULT fr, const char *path) {
	switch (fr) {
		case FR_WRITE_PROTECTED: return KST_STORE_IO_DENIED;
		case FR_DENIED:
			return f_stat(path, &s_fno) == FR_OK ? KST_STORE_IO_DENIED : KST_STORE_IO_FULL;
		default:                 return KST_STORE_IO_ERROR;
	}
}

/**
 * @brief Make `KST` when it is missing; a file of that name is DENIED.
 * @param[out] made True when this call made the directory.
 */
static kst_store_io_result_t makeDir(bool *made) {
	*made = false;
	FRESULT fr = f_mkdir(KST_STORE_DIR);
	if (fr == FR_OK) {
		*made = true;
		return KST_STORE_IO_OK;
	}
	if (fr != FR_EXIST) return createError(fr, KST_STORE_DIR);
	if (f_stat(KST_STORE_DIR, &s_fno) != FR_OK) return KST_STORE_IO_ERROR;
	return (s_fno.fattrib & AM_DIR) ? KST_STORE_IO_OK : KST_STORE_IO_DENIED;
}

static kst_store_io_result_t ioCreate(void *, const char *name, const char *data,
                                      size_t len) {
	bool made;
	kst_store_io_result_t d = makeDir(&made);
	if (d != KST_STORE_IO_OK) return d;
	char path[KST_FILES_PATH_MAX];
	pathOf(name, path);
	FRESULT fr = f_open(&s_fil, path, FA_WRITE | FA_CREATE_NEW);
	kst_store_io_result_t r = KST_STORE_IO_OK;
	if (fr == FR_EXIST) {
		r = KST_STORE_IO_EXISTS;
	} else if (fr != FR_OK) {
		r = createError(fr, path);
	} else {
		UINT put = 0;
		fr = f_write(&s_fil, data, (UINT)len, &put);
		// FatFs reports a full volume as a short write with FR_OK.
		if (fr != FR_OK)     r = createError(fr, path);
		else if (put < len)  r = KST_STORE_IO_FULL;
		else if (f_sync(&s_fil) != FR_OK) r = KST_STORE_IO_ERROR;
		if (f_close(&s_fil) != FR_OK && r == KST_STORE_IO_OK) r = KST_STORE_IO_ERROR;
		// The contract: a failed create leaves no file behind. A file whose
		// close failed stays open in FatFs, which then refuses the remove
		// with FR_LOCKED: it stays on the card, and holds one of FatFs's 16
		// lock entries until the card is mounted again.
		if (r != KST_STORE_IO_OK) f_unlink(path);
	}
	// Nor the directory this call made; one that holds a file stays.
	if (r != KST_STORE_IO_OK && r != KST_STORE_IO_EXISTS && made) f_unlink(KST_STORE_DIR);
	return r;
}

static const kst_store_io_t IO = {
	nullptr, ioMounted, ioWalk, ioSize, ioRead, ioCreate,
};

const kst_store_io_t *kstFilesIo() { return &IO; }

// ---------------------------------------------------------------------------
// the record and the first write
// ---------------------------------------------------------------------------

/**
 * @brief The `kst_store_t` of a failed rename: FR_DENIED is a full
 *        directory there.
 */
static kst_store_t renameError(FRESULT fr) {
	switch (fr) {
		case FR_WRITE_PROTECTED: return KST_STORE_DENIED;
		case FR_DENIED:          return KST_STORE_CARD_FULL;
		default:                 return KST_STORE_WRITE_ERROR;
	}
}

/** @brief The `kst_store_t` of a create's result. */
static kst_store_t storeResult(kst_store_io_result_t r) {
	switch (r) {
		case KST_STORE_IO_OK:     return KST_STORE_OK;
		case KST_STORE_IO_EXISTS: return KST_STORE_EXISTS;
		case KST_STORE_IO_FULL:   return KST_STORE_CARD_FULL;
		case KST_STORE_IO_DENIED: return KST_STORE_DENIED;
		default:                  return KST_STORE_WRITE_ERROR;
	}
}

bool kstFilesRecordPresent() {
	return kstFilesMounted() && ioSize(nullptr, KST_FILES_RECORD_NAME) >= 0;
}

kst_store_t kstFilesRecordLoad(char *buf, size_t cap, size_t *len) {
	if (!buf || !len) return KST_STORE_ARG;
	*len = 0;
	if (!kstFilesMounted()) return KST_STORE_NO_CARD;
	int32_t size = ioSize(nullptr, KST_FILES_RECORD_NAME);
	if (size < 0) return KST_STORE_NOT_FOUND;
	if ((size_t)size > cap) return KST_STORE_TOO_LARGE;
	int32_t got = ioRead(nullptr, KST_FILES_RECORD_NAME, buf, cap);
	if (got != size) return KST_STORE_READ_ERROR;
	*len = (size_t)got;
	return KST_STORE_OK;
}

kst_store_t kstFilesRecordRemove(const KstFirstWrite *w) {
	// Only the record this session wrote: one left in place by an earlier,
	// interrupted write is the servo before that write.
	if (!w || w->record != KST_STORE_OK) return KST_STORE_OK;
	if (!kstFilesMounted()) return KST_STORE_NO_CARD;
	// And only while the file holds its bytes: another card's record stays.
	size_t len = 0;
	kst_store_t r = kstFilesRecordLoad(s_text, sizeof(s_text), &len);
	if (r == KST_STORE_NOT_FOUND || r == KST_STORE_TOO_LARGE) return KST_STORE_OK;
	if (r != KST_STORE_OK) return r;
	if (fnv1a(s_text, len) != w->recordSum) return KST_STORE_OK;
	char path[KST_FILES_PATH_MAX];
	pathOf(KST_FILES_RECORD_NAME, path);
	FRESULT fr = f_unlink(path);
	switch (fr) {
		case FR_OK:
		case FR_NO_FILE:
		case FR_NO_PATH:         return KST_STORE_OK;
		// A read-only file: the card keeps it.
		case FR_DENIED:
		case FR_WRITE_PROTECTED: return KST_STORE_DENIED;
		default:                 return KST_STORE_WRITE_ERROR;
	}
}

/**
 * @brief Rename an old record to the next KSTnnn.TXT.
 * @return `KST_STORE_OK`, also with no record to keep; `KST_STORE_READ_ERROR`
 *         when the card does not say whether there is one.
 */
static kst_store_t keepOldRecord(KstFirstWrite *out) {
	char from[KST_FILES_PATH_MAX], to[KST_FILES_PATH_MAX];
	pathOf(KST_FILES_RECORD_NAME, from);
	FRESULT st = f_stat(from, &s_fno);
	if (st == FR_NO_FILE || st == FR_NO_PATH) return KST_STORE_OK;
	if (st != FR_OK) return KST_STORE_READ_ERROR;
	// A directory of that name is no record; the create then refuses it.
	if (s_fno.fattrib & AM_DIR) return KST_STORE_OK;
	char base[KST_STORE_PRESET_MAX];
	kst_store_t r = kst_store_preset(&IO, base, sizeof(base));
	if (r != KST_STORE_OK) return r;
	char name[KST_STORE_NAME_MAX];
	kst_store_file_name(base, name, sizeof(name));
	pathOf(name, to);
	FRESULT fr = f_rename(from, to);
	if (fr != FR_OK) {
		// FatFs reports a rename whose last sync failed, and its sector
		// window still holds the change, which the next call writes: ask the
		// card where the record is.
		if (f_stat(from, &s_fno) != FR_NO_FILE || f_stat(to, &s_fno) != FR_OK)
			return renameError(fr);
	}
	snprintf(out->keptName, sizeof(out->keptName), "%s", name);
	return KST_STORE_OK;
}

void kstFilesFirstWrite(const kst_image_t *backup, const kst_image_t *before,
                        KstFirstWrite *out) {
	memset(out, 0, sizeof(*out));
	if (!kstFilesMounted()) {
		out->backup = out->record = out->kept = KST_STORE_NO_CARD;
		return;
	}
	out->kept = (uint8_t)keepOldRecord(out);

	char base[KST_STORE_PRESET_MAX];
	kst_store_t r = kst_store_preset(&IO, base, sizeof(base));
	if (r == KST_STORE_OK) {
		size_t n = kst_file_format(backup, s_text, sizeof(s_text));
		r = kst_store_save(&IO, base, s_text, n, nullptr);
		if (r == KST_STORE_OK) kst_store_file_name(base, out->backupName, sizeof(out->backupName));
	}
	out->backup = (uint8_t)r;

	// A record that stayed in place is not replaced: it is the only copy of
	// the servo before the interrupted write.
	if (out->kept != KST_STORE_OK) {
		out->record = out->kept;
		return;
	}
	size_t n = kst_file_format(before, s_text, sizeof(s_text));
	out->record = (uint8_t)storeResult(ioCreate(nullptr, KST_FILES_RECORD_NAME, s_text, n));
	out->recordSum = fnv1a(s_text, n);
}

bool kstFilesWriteAllowed(const KstFirstWrite *w) {
	if (w->backup == KST_STORE_NO_CARD && w->record == KST_STORE_NO_CARD) return true;
	return w->backup == KST_STORE_OK || w->record == KST_STORE_OK;
}
