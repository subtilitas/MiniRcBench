/**
 * @file kst_files.cpp
 * @brief The KST files over FatFs. @see kst_files.h
 */

#include "kst_files.h"
#include "kst_file.h"

#include "ff.h"
#include <stdio.h>
#include <string.h>

/** @brief A file's text, for the backup and the record. */
static char s_text[KST_FILE_MAX];

static FILINFO s_fno;

/** @brief FNV-1a (Fowler-Noll-Vo), 32 bits, of @p n bytes at @p p. */
static uint32_t fnv1a(const char *p, size_t n) {
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)p[i]) * 16777619u;
	return h;
}

bool kstFilesMounted() { return cardMounted(); }

KstFilesScope::KstFilesScope() : m_dir(KST_STORE_DIR, cardMounted()) {}

// ---------------------------------------------------------------------------
// kst_store_io_t: the files of KST/
// ---------------------------------------------------------------------------

static bool ioMounted(void *) { return cardMounted(); }

static bool ioWalk(void *, kst_store_visit_fn visit, void *visitCtx) {
	CardDirScope in(KST_STORE_DIR);
	return cardWalk(KST_STORE_DIR, visit, visitCtx);
}

static int32_t ioSize(void *, const char *name) {
	CardDirScope in(KST_STORE_DIR);
	return cardSize(KST_STORE_DIR, name);
}

static int32_t ioRead(void *, const char *name, char *buf, size_t cap) {
	CardDirScope in(KST_STORE_DIR);
	return cardRead(KST_STORE_DIR, name, buf, cap);
}

static kst_store_io_result_t ioResult(CardIo r) {
	switch (r) {
		case CardIo::Ok:     return KST_STORE_IO_OK;
		case CardIo::Exists: return KST_STORE_IO_EXISTS;
		case CardIo::Full:   return KST_STORE_IO_FULL;
		case CardIo::Denied: return KST_STORE_IO_DENIED;
		default:             return KST_STORE_IO_ERROR;
	}
}

static kst_store_io_result_t ioCreate(void *, const char *name, const char *data, size_t len) {
	CardDirScope in(KST_STORE_DIR);
	return ioResult(cardCreate(KST_STORE_DIR, name, data, len));
}

static const kst_store_io_t IO = {
	nullptr, ioMounted, ioWalk, ioSize, ioRead, ioCreate,
};

const kst_store_io_t *kstFilesIo() { return &IO; }

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

/** @brief The `kst_store_t` of a backup call's result. */
static kst_store_t backupResult(BackupResult r) {
	switch (r) {
		case BackupResult::Ok:         return KST_STORE_OK;
		case BackupResult::NoCard:     return KST_STORE_NO_CARD;
		case BackupResult::ListError:  return KST_STORE_LIST_ERROR;
		case BackupResult::Full:       return KST_STORE_CARD_FULL;
		case BackupResult::Denied:     return KST_STORE_DENIED;
		case BackupResult::NotFound:   return KST_STORE_NOT_FOUND;
		case BackupResult::TooLarge:   return KST_STORE_TOO_LARGE;
		case BackupResult::ReadError:  return KST_STORE_READ_ERROR;
		default:                       return KST_STORE_WRITE_ERROR;
	}
}

// ---------------------------------------------------------------------------
// SAVE AS and the backups
// ---------------------------------------------------------------------------

kst_store_t kstFilesSaveName(char *base) {
	base[0] = '\0';
	KstFilesScope scope;
	size_t total = 0;
	kst_store_t r = kst_store_list(&IO, nullptr, 0, nullptr, &total, nullptr);
	if (r != KST_STORE_OK) return r;
	if (total >= KST_FILES_SAVE_MAX) return KST_STORE_NO_NUMBER;
	return kst_store_preset(&IO, base, KST_STORE_PRESET_MAX);
}

kst_store_t kstFilesSaveAs(const char *text, size_t len, char *name) {
	name[0] = '\0';
	KstFilesScope scope;
	char base[KST_STORE_PRESET_MAX];
	kst_store_t r = kstFilesSaveName(base);
	if (r != KST_STORE_OK) return r;
	r = kst_store_save(&IO, base, text, len, nullptr);
	if (r == KST_STORE_OK) kst_store_file_name(base, name, KST_STORE_NAME_MAX);
	return r;
}

kst_store_t kstFilesBackups(BackupFileName *names, size_t cap, size_t *count) {
	*count = 0;
	BackupNames k;
	kst_store_t r = backupResult(backupNames(&k));
	if (r == KST_STORE_OK) *count = backupList(&k, BackupDevice::Kst, names, cap);
	return r;
}

kst_store_t kstFilesBackupLoad(const char *name, char *buf, size_t cap, size_t *len) {
	*len = 0;
	BackupDevice d;
	if (!backupNumber(name, &d) || d != BackupDevice::Kst) return KST_STORE_NOT_FOUND;
	return backupResult(backupLoad(name, buf, cap, len));
}

// ---------------------------------------------------------------------------
// the record and the first write
// ---------------------------------------------------------------------------

/** @brief The record's path into @p out. */
static void recordPath(char *out) { cardPath(KST_STORE_DIR, KST_FILES_RECORD_NAME, out); }

bool kstFilesRecordPresent() {
	return cardMounted() && ioSize(nullptr, KST_FILES_RECORD_NAME) >= 0;
}

kst_store_t kstFilesRecordLoad(char *buf, size_t cap, size_t *len) {
	if (!buf || !len) return KST_STORE_ARG;
	*len = 0;
	if (!cardMounted()) return KST_STORE_NO_CARD;
	KstFilesScope scope;
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
	if (!cardMounted()) return KST_STORE_NO_CARD;
	KstFilesScope scope;
	// And only while the file holds its bytes: another card's record stays.
	size_t len = 0;
	kst_store_t r = kstFilesRecordLoad(s_text, sizeof(s_text), &len);
	if (r == KST_STORE_NOT_FOUND || r == KST_STORE_TOO_LARGE) return KST_STORE_OK;
	if (r != KST_STORE_OK) return r;
	if (fnv1a(s_text, len) != w->recordSum) return KST_STORE_OK;
	char path[CARD_PATH_MAX];
	recordPath(path);
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

/** @brief The numbers a first write takes: a record to keep, then the backup. */
static void firstWriteNumbers(const BackupNames *k, bool record, unsigned *nKept,
                              unsigned *nBackup) {
	*nKept = record ? backupNext(k) : 0;
	BackupNames after = *k;
	if (*nKept) after.latest = *nKept;
	*nBackup = backupNext(&after);
}

kst_store_t kstFilesNextBackup(char *name) {
	name[0] = '\0';
	if (!cardMounted()) return KST_STORE_NO_CARD;
	CardDirScope in(BACKUP_DIR);
	BackupNames k;
	kst_store_t r = backupResult(backupNames(&k));
	if (r != KST_STORE_OK) return r;
	unsigned nKept, nBackup;
	firstWriteNumbers(&k, kstFilesRecordPresent(), &nKept, &nBackup);
	backupName(BackupDevice::Kst, nBackup, name);
	return KST_STORE_OK;
}

void kstFilesFirstWrite(const kst_image_t *backup, const kst_image_t *before,
                        KstFirstWrite *out) {
	memset(out, 0, sizeof(*out));
	if (!cardMounted()) {
		out->backup = out->record = out->kept = KST_STORE_NO_CARD;
		return;
	}
	CardDirScope in(BACKUP_DIR);
	// One walk names both files, and one marker write claims both: a record
	// to keep takes the next number, the backup the one after it.
	BackupNames k;
	kst_store_t names = backupResult(backupNames(&k));
	char from[CARD_PATH_MAX];
	recordPath(from);
	FRESULT st = f_stat(from, &s_fno);
	// A directory of that name is no record; the create then refuses it.
	bool record = st == FR_OK && !(s_fno.fattrib & AM_DIR);
	// A card that does not say whether a record is there keeps none.
	kst_store_t kept = (st == FR_OK || st == FR_NO_FILE || st == FR_NO_PATH)
	                       ? KST_STORE_OK : KST_STORE_READ_ERROR;
	unsigned nKept = 0, nBackup = 0;
	kst_store_t mark = names;
	if (names == KST_STORE_OK) {
		firstWriteNumbers(&k, record, &nKept, &nBackup);
		mark = backupResult(backupClaim(&k, nBackup));
	}
	if (record)
		kept = mark != KST_STORE_OK
		           ? mark
		           : backupResult(backupMoveIn(&k, BackupDevice::Kst, nKept, from, out->keptName));
	out->kept = (uint8_t)kept;

	kst_store_t r = mark;
	if (r == KST_STORE_OK) {
		size_t n = kst_file_format(backup, s_text, sizeof(s_text));
		r = backupResult(backupPut(&k, BackupDevice::Kst, nBackup, s_text, n, out->backupName));
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
