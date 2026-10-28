/**
 * @file hitec_files.cpp
 * @brief The Hitec files over FatFs. @see hitec_files.h
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 */

#include "hitec_files.h"

#include "ff.h"
#include <stdio.h>
#include <string.h>

/** @brief A file's text: the servo file is the longer of the two. */
static char s_text[HITEC_CARD_FILE_MAX];

static FILINFO s_fno;

/** @brief The stem of SAVE AS names: `HT`. */
static const char STEM[] = "HT";

/** @brief The highest SAVE AS number, `HT999`. */
static const unsigned NUMBER_MAX = 999;

/** @brief FNV-1a (Fowler-Noll-Vo), 32 bits, of @p n bytes at @p p. */
static uint32_t fnv1a(const char *p, size_t n) {
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)p[i]) * 16777619u;
	return h;
}

bool hitecFilesMounted() { return cardMounted(); }

/** @brief The `HitecFiles` of a card call's result. */
static HitecFiles ioResult(CardIo r) {
	switch (r) {
		case CardIo::Ok:     return HitecFiles::Ok;
		case CardIo::Exists: return HitecFiles::Exists;
		case CardIo::Full:   return HitecFiles::Full;
		case CardIo::Denied: return HitecFiles::Denied;
		default:             return HitecFiles::WriteError;
	}
}

/** @brief The `HitecFiles` of a backup call's result. */
static HitecFiles backupResult(BackupResult r) {
	switch (r) {
		case BackupResult::Ok:        return HitecFiles::Ok;
		case BackupResult::NoCard:    return HitecFiles::NoCard;
		case BackupResult::ListError: return HitecFiles::ListError;
		case BackupResult::Full:      return HitecFiles::Full;
		case BackupResult::Denied:    return HitecFiles::Denied;
		case BackupResult::NotFound:  return HitecFiles::NotFound;
		case BackupResult::TooLarge:  return HitecFiles::TooLarge;
		case BackupResult::ReadError: return HitecFiles::ReadError;
		default:                      return HitecFiles::WriteError;
	}
}

// ---------------------------------------------------------------------------
// the files of HITEC/
// ---------------------------------------------------------------------------

/** @brief The number of a SAVE AS name, `HTnnn.TXT`, 1 to 999; 0 for any other name. */
static unsigned saveNumber(const char *name) {
	if (strlen(name) != 9) return 0;
	if ((name[0] | 0x20) != 'h' || (name[1] | 0x20) != 't') return 0;
	unsigned n = 0;
	for (int i = 2; i < 5; i++) {
		if (name[i] < '0' || name[i] > '9') return 0;
		n = n * 10u + (unsigned)(name[i] - '0');
	}
	if (name[5] != '.' || (name[6] | 0x20) != 't' || (name[7] | 0x20) != 'x' ||
	    (name[8] | 0x20) != 't')
		return 0;
	return n;
}

/** @brief What a walk of `HITEC/` collects. */
struct Listing {
	HitecFileName *names;  /**< Where names go; may be NULL. */
	size_t        cap;     /**< Room in @c names. */
	size_t        count;   /**< SAVE AS files seen. */
	unsigned      highest; /**< Their highest number. */
};

static void visitFile(const char *name, bool isDir, void *ctx) {
	Listing *l = (Listing *)ctx;
	unsigned n = isDir ? 0 : saveNumber(name);
	if (!n) return;
	if (l->names && l->count < l->cap) {
		// In order of the names: insert, the arrays are at most 50 long.
		size_t at = l->count;
		while (at > 0 && strcmp(l->names[at - 1], name) > 0) {
			memcpy(l->names[at], l->names[at - 1], sizeof(HitecFileName));
			at--;
		}
		snprintf(l->names[at], sizeof(HitecFileName), "%s", name);
	}
	l->count++;
	if (n > l->highest) l->highest = n;
}

/** @brief Walk `HITEC/`. */
static HitecFiles listing(Listing *l) {
	if (!cardMounted()) return HitecFiles::NoCard;
	CardDirScope in(HITEC_FILES_DIR);
	return cardWalk(HITEC_FILES_DIR, visitFile, l) ? HitecFiles::Ok : HitecFiles::ListError;
}

HitecFiles hitecFilesList(HitecFileName *names, size_t cap, size_t *count) {
	*count = 0;
	Listing l = { names, cap, 0, 0 };
	HitecFiles r = listing(&l);
	if (r == HitecFiles::Ok) *count = l.count < cap ? l.count : cap;
	return r;
}

HitecFiles hitecFilesSaveName(char *name) {
	name[0] = '\0';
	Listing l = { nullptr, 0, 0, 0 };
	HitecFiles r = listing(&l);
	if (r != HitecFiles::Ok) return r;
	if (l.count >= HITEC_FILES_SAVE_MAX || l.highest >= NUMBER_MAX) return HitecFiles::NoNumber;
	snprintf(name, HITEC_FILES_NAME_MAX, "%s%03u.TXT", STEM, l.highest + 1u);
	return HitecFiles::Ok;
}

HitecFiles hitecFilesSaveAs(const hitec_image_t *img, char *name) {
	name[0] = '\0';
	CardDirScope in(HITEC_FILES_DIR, cardMounted());
	char next[HITEC_FILES_NAME_MAX];
	HitecFiles r = hitecFilesSaveName(next);
	if (r != HitecFiles::Ok) return r;
	size_t n = hitec_card_file_text(img, s_text, sizeof(s_text));
	if (!n) return HitecFiles::WriteError;
	r = ioResult(cardCreate(HITEC_FILES_DIR, next, s_text, n));
	if (r == HitecFiles::Ok) snprintf(name, HITEC_FILES_NAME_MAX, "%s", next);
	return r;
}

/** @brief Parse @p len bytes of a servo file or a record as the image to load. */
static HitecFiles parseImage(const char *text, size_t len, uint8_t series, hitec_image_t *out) {
	hitec_image_t img;
	if (hitec_card_file_parse(text, len, &img, nullptr, nullptr) != HITEC_CARD_OK) {
		hitec_card_record_t rec;
		if (hitec_card_parse(text, len, &rec) != HITEC_CARD_OK) return HitecFiles::BadText;
		img = rec.before;
	}
	if (img.series != series) return HitecFiles::OtherSeries;
	*out = img;
	return HitecFiles::Ok;
}

/** @brief Read the file @p name of @p dir whole into s_text. */
static HitecFiles readText(const char *dir, const char *name, size_t *len) {
	*len = 0;
	int32_t size = cardSize(dir, name);
	if (size < 0) return HitecFiles::NotFound;
	if ((size_t)size > sizeof(s_text)) return HitecFiles::TooLarge;
	int32_t got = cardRead(dir, name, s_text, sizeof(s_text));
	if (got != size) return HitecFiles::ReadError;
	*len = (size_t)got;
	return HitecFiles::Ok;
}

HitecFiles hitecFilesLoad(const char *name, uint8_t series, hitec_image_t *out) {
	if (!cardMounted()) return HitecFiles::NoCard;
	CardDirScope in(HITEC_FILES_DIR);
	size_t len;
	HitecFiles r = readText(HITEC_FILES_DIR, name, &len);
	if (r != HitecFiles::Ok) return r;
	return parseImage(s_text, len, series, out);
}

HitecFiles hitecFilesBackups(BackupFileName *names, size_t cap, size_t *count) {
	*count = 0;
	BackupNames k;
	HitecFiles r = backupResult(backupNames(&k));
	if (r == HitecFiles::Ok) *count = backupList(&k, BackupDevice::Hitec, names, cap);
	return r;
}

HitecFiles hitecFilesBackupLoad(const char *name, uint8_t series, hitec_image_t *out) {
	BackupDevice d;
	if (!backupNumber(name, &d) || d != BackupDevice::Hitec) return HitecFiles::NotFound;
	size_t len = 0;
	HitecFiles r = backupResult(backupLoad(name, s_text, sizeof(s_text), &len));
	if (r != HitecFiles::Ok) return r;
	return parseImage(s_text, len, series, out);
}

// ---------------------------------------------------------------------------
// the record and the writes
// ---------------------------------------------------------------------------

/** @brief The record's path into @p out. */
static void recordPath(char *out) { cardPath(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME, out); }

bool hitecFilesRecordPresent() {
	if (!cardMounted()) return false;
	CardDirScope in(HITEC_FILES_DIR);
	return cardSize(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME) >= 0;
}

HitecFiles hitecFilesRecordLoad(hitec_card_record_t *out) {
	if (!cardMounted()) return HitecFiles::NoCard;
	CardDirScope in(HITEC_FILES_DIR);
	size_t len;
	HitecFiles r = readText(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME, &len);
	if (r != HitecFiles::Ok) return r;
	hitec_card_record_t rec;
	if (hitec_card_parse(s_text, len, &rec) != HITEC_CARD_OK) return HitecFiles::BadText;
	*out = rec;
	return HitecFiles::Ok;
}

/** @brief The numbers a first write takes: a record to keep, then the backup. */
static void firstWriteNumbers(const BackupNames *k, bool record, unsigned *nKept,
                              unsigned *nBackup) {
	*nKept = record ? backupNext(k) : 0;
	BackupNames after = *k;
	if (*nKept) after.latest = *nKept;
	*nBackup = backupNext(&after);
}

HitecFiles hitecFilesNextBackup(char *name) {
	name[0] = '\0';
	if (!cardMounted()) return HitecFiles::NoCard;
	CardDirScope in(BACKUP_DIR);
	BackupNames k;
	HitecFiles r = backupResult(backupNames(&k));
	if (r != HitecFiles::Ok) return r;
	unsigned nKept, nBackup;
	firstWriteNumbers(&k, hitecFilesRecordPresent(), &nKept, &nBackup);
	backupName(BackupDevice::Hitec, nBackup, name);
	return HitecFiles::Ok;
}

/** @brief Write @p rec as a new `HITEC/HITEC.TXT`; its hash goes into @p out. */
static void createRecord(const hitec_card_record_t *rec, HitecWrite *out) {
	size_t n = hitec_card_text(rec, s_text, sizeof(s_text));
	if (!n) {
		out->record = HitecFiles::WriteError;
		return;
	}
	out->record = ioResult(cardCreate(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME, s_text, n));
	out->recordSum = fnv1a(s_text, n);
}

void hitecFilesFirstWrite(const hitec_image_t *backup, const hitec_card_record_t *rec,
                          HitecWrite *out) {
	memset(out, 0, sizeof(*out));
	if (!cardMounted()) {
		out->backup = out->record = out->kept = HitecFiles::NoCard;
		return;
	}
	CardDirScope in(BACKUP_DIR);
	// One walk names both files, and one marker write claims both: a record
	// to keep takes the next number, the backup the one after it.
	BackupNames k;
	HitecFiles names = backupResult(backupNames(&k));
	char from[CARD_PATH_MAX];
	recordPath(from);
	FRESULT st = f_stat(from, &s_fno);
	// A directory of that name is no record; the create then refuses it.
	bool record = st == FR_OK && !(s_fno.fattrib & AM_DIR);
	// A card that does not say whether a record is there keeps none.
	HitecFiles kept = (st == FR_OK || st == FR_NO_FILE || st == FR_NO_PATH)
	                      ? HitecFiles::Ok : HitecFiles::ReadError;
	unsigned nKept = 0, nBackup = 0;
	HitecFiles mark = names;
	if (names == HitecFiles::Ok) {
		firstWriteNumbers(&k, record, &nKept, &nBackup);
		mark = backupResult(backupClaim(&k, nBackup));
	}
	if (record)
		kept = mark != HitecFiles::Ok
		           ? mark
		           : backupResult(backupMoveIn(&k, BackupDevice::Hitec, nKept, from, out->keptName));
	out->kept = kept;

	HitecFiles r = mark;
	if (r == HitecFiles::Ok) {
		size_t n = hitec_card_file_text(backup, s_text, sizeof(s_text));
		r = n ? backupResult(backupPut(&k, BackupDevice::Hitec, nBackup, s_text, n, out->backupName))
		      : HitecFiles::WriteError;
	}
	out->backup = r;

	// A record that stayed in place is not replaced: it is the only copy of
	// the servo before the interrupted write.
	if (out->kept != HitecFiles::Ok) {
		out->record = out->kept;
		return;
	}
	createRecord(rec, out);
}

void hitecFilesRecordWrite(const HitecWrite *prev, const hitec_card_record_t *rec,
                           HitecWrite *out) {
	memset(out, 0, sizeof(*out));
	out->backup = HitecFiles::Ok;
	out->kept = HitecFiles::Ok;
	if (!cardMounted()) {
		out->record = HitecFiles::NoCard;
		return;
	}
	CardDirScope in(HITEC_FILES_DIR);
	size_t len = 0;
	HitecFiles r = readText(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME, &len);
	if (r == HitecFiles::NotFound) {
		createRecord(rec, out);
		return;
	}
	// Only the session's own record is written over: another one is the
	// servo before an interrupted write.
	bool own = r == HitecFiles::Ok && prev && prev->record == HitecFiles::Ok &&
	           fnv1a(s_text, len) == prev->recordSum;
	if (!own) {
		out->record = r == HitecFiles::Ok || r == HitecFiles::TooLarge ? HitecFiles::Exists : r;
		return;
	}
	size_t n = hitec_card_text(rec, s_text, sizeof(s_text));
	if (!n) {
		out->record = HitecFiles::WriteError;
		return;
	}
	out->record = ioResult(cardOverwrite(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME, s_text, n));
	out->recordSum = fnv1a(s_text, n);
}

bool hitecFilesWriteAllowed(const HitecWrite *w) {
	if (w->backup == HitecFiles::NoCard && w->record == HitecFiles::NoCard) return true;
	return w->backup == HitecFiles::Ok || w->record == HitecFiles::Ok;
}

HitecFiles hitecFilesRecordRemove(const HitecWrite *w) {
	// Only the record this session wrote.
	if (!w || w->record != HitecFiles::Ok) return HitecFiles::Ok;
	if (!cardMounted()) return HitecFiles::NoCard;
	CardDirScope in(HITEC_FILES_DIR);
	size_t len = 0;
	HitecFiles r = readText(HITEC_FILES_DIR, HITEC_FILES_RECORD_NAME, &len);
	if (r == HitecFiles::NotFound || r == HitecFiles::TooLarge) return HitecFiles::Ok;
	if (r != HitecFiles::Ok) return r;
	// And only while the file holds its bytes: another card's record stays.
	if (fnv1a(s_text, len) != w->recordSum) return HitecFiles::Ok;
	char path[CARD_PATH_MAX];
	recordPath(path);
	switch (f_unlink(path)) {
		case FR_OK:
		case FR_NO_FILE:
		case FR_NO_PATH:         return HitecFiles::Ok;
		case FR_DENIED:
		case FR_WRITE_PROTECTED: return HitecFiles::Denied;
		default:                 return HitecFiles::WriteError;
	}
}
