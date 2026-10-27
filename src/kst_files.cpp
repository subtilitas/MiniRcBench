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

/** @brief Room for `/KST/` and a file name. */
#define KST_FILES_PATH_MAX (sizeof(KST_STORE_DIR) + 1 + KST_STORE_NAME_MAX)

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

/** @brief The `KST` directory from the root, whatever the current directory. */
#define KST_DIR_ABS "/" KST_STORE_DIR

/**
 * @brief Whether FatFs's current directory is `KST`, and how deep the
 *        guards nest.
 */
static bool     s_inKst;
static unsigned s_depth;

/**
 * @brief FatFs's current directory set to `KST` for the guard's scope, and
 *        back to the root at its end, so the blackbox logger's paths from
 *        the root are unchanged.
 *
 * A path with `KST/` looks `KST` up in the root on every call, and a root
 * that holds many blackbox logs makes that lookup the larger part of a card
 * phase. One f_chdir() per scope looks it up once. Without `KST` the guard
 * stays in the root and paths name `KST/` as before. The guards nest; only
 * the outermost one changes directory.
 */
static void enterKst() {
	if (s_depth++ == 0) s_inKst = f_chdir(KST_DIR_ABS) == FR_OK;
}

static void leaveKst() {
	if (--s_depth == 0 && s_inKst) {
		f_chdir("/");
		s_inKst = false;
	}
}

struct KstDirScope {
	KstDirScope() { enterKst(); }
	~KstDirScope() { leaveKst(); }
	KstDirScope(const KstDirScope &) = delete;
	KstDirScope &operator=(const KstDirScope &) = delete;
};

KstFilesScope::KstFilesScope() : m_on(kstFilesMounted()) {
	if (m_on) enterKst();
}

KstFilesScope::~KstFilesScope() {
	if (m_on) leaveKst();
}

/** @brief The path of @p name in `KST` into @p out: the name alone while in `KST`. */
static void pathOf(const char *name, char *out) {
	if (s_inKst) snprintf(out, KST_FILES_PATH_MAX, "%s", name);
	else snprintf(out, KST_FILES_PATH_MAX, "%s/%s", KST_DIR_ABS, name);
}

// ---------------------------------------------------------------------------
// kst_store_io_t
// ---------------------------------------------------------------------------

static bool ioMounted(void *) { return kstFilesMounted(); }

static bool ioWalk(void *, kst_store_visit_fn visit, void *visitCtx) {
	KstDirScope in;
	DIR dir;
	FRESULT fr = f_opendir(&dir, s_inKst ? "" : KST_DIR_ABS);
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
	KstDirScope in;
	char path[KST_FILES_PATH_MAX];
	pathOf(name, path);
	if (f_stat(path, &s_fno) != FR_OK || (s_fno.fattrib & AM_DIR)) return -1;
	return s_fno.fsize > INT32_MAX ? INT32_MAX : (int32_t)s_fno.fsize;
}

static int32_t ioRead(void *, const char *name, char *buf, size_t cap) {
	KstDirScope in;
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
	// Inside `KST`: it is there.
	if (s_inKst) return KST_STORE_IO_OK;
	FRESULT fr = f_mkdir(KST_DIR_ABS);
	if (fr == FR_OK) {
		*made = true;
		return KST_STORE_IO_OK;
	}
	if (fr != FR_EXIST) return createError(fr, KST_DIR_ABS);
	if (f_stat(KST_DIR_ABS, &s_fno) != FR_OK) return KST_STORE_IO_ERROR;
	return (s_fno.fattrib & AM_DIR) ? KST_STORE_IO_OK : KST_STORE_IO_DENIED;
}

static kst_store_io_result_t ioCreate(void *, const char *name, const char *data,
                                      size_t len) {
	KstDirScope in;
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
	if (r != KST_STORE_IO_OK && r != KST_STORE_IO_EXISTS && made) f_unlink(KST_DIR_ABS);
	return r;
}

static const kst_store_io_t IO = {
	nullptr, ioMounted, ioWalk, ioSize, ioRead, ioCreate,
};

const kst_store_io_t *kstFilesIo() { return &IO; }

// ---------------------------------------------------------------------------
// the ring of names
// ---------------------------------------------------------------------------

static uint64_t bitOf(unsigned n) { return 1ull << (n - 1u); }

static bool isTaken(uint64_t taken, unsigned n) { return (taken & bitOf(n)) != 0; }

unsigned kstNamesAfter(unsigned n) { return n % KST_FILES_NAMES + 1u; }

unsigned kstNamesGuess(uint64_t taken) {
	if (!taken) return 0;
	unsigned highest = 0, free = 0;
	for (unsigned n = 1; n <= KST_FILES_NAMES; n++) {
		if (isTaken(taken, n)) highest = n;
		else if (!free) free = n;
	}
	if (highest < KST_FILES_NAMES || !free) return highest;
	return free == 1 ? KST_FILES_NAMES : free - 1u;
}

unsigned kstNamesMarker(const char *text, size_t len) {
	if (len != KST_FILES_MARKER_LEN || text[3] != '\n') return 0;
	unsigned n = 0;
	for (int i = 0; i < 3; i++) {
		if (text[i] < '0' || text[i] > '9') return 0;
		n = n * 10u + (unsigned)(text[i] - '0');
	}
	return n >= 1 && n <= KST_FILES_NAMES ? n : 0;
}

void kstNamesBase(unsigned n, char *out) {
	snprintf(out, KST_STORE_PRESET_MAX, "KST%03u", n);
}

/** @brief The number of a ring name, `KSTnnn.TXT` in any case; 0 for any other name. */
static unsigned ringNumber(const char *name) {
	static const char EXT[] = KST_STORE_EXT;
	if (strlen(name) != 6 + sizeof(EXT) - 1) return 0;
	if ((name[0] | 0x20) != 'k' || (name[1] | 0x20) != 's' || (name[2] | 0x20) != 't') return 0;
	for (size_t i = 0; i < sizeof(EXT) - 1; i++)
		if ((name[6 + i] | 0x20) != (EXT[i] | 0x20)) return 0;
	unsigned n = 0;
	for (int i = 3; i < 6; i++) {
		if (name[i] < '0' || name[i] > '9') return 0;
		n = n * 10u + (unsigned)(name[i] - '0');
	}
	return n >= 1 && n <= KST_FILES_NAMES ? n : 0;
}

static void markName(const char *name, bool isDir, void *ctx) {
	KstNames *k = (KstNames *)ctx;
	unsigned n = ringNumber(name);
	if (!n) return;
	k->taken |= bitOf(n);
	if (isDir) k->dirs |= bitOf(n);
}

/** @brief The number in `KST/LAST`, 0 when it is missing or holds anything else. */
static unsigned readMarker() {
	char text[KST_FILES_MARKER_LEN + 1];
	int32_t got = ioRead(nullptr, KST_FILES_MARKER_NAME, text, sizeof(text));
	return got < 0 ? 0 : kstNamesMarker(text, (size_t)got);
}

kst_store_t kstFilesNames(KstNames *out) {
	memset(out, 0, sizeof(*out));
	if (!kstFilesMounted()) return KST_STORE_NO_CARD;
	KstDirScope in;
	KstNames k;
	memset(&k, 0, sizeof(k));
	if (!ioWalk(nullptr, markName, &k)) return KST_STORE_LIST_ERROR;
	k.marked = readMarker();
	k.latest = k.marked ? k.marked : kstNamesGuess(k.taken);
	*out = k;
	return KST_STORE_OK;
}

/** @brief The number after @p n that no directory holds. */
static unsigned nextAfter(const KstNames *k, unsigned n) {
	n = kstNamesAfter(n);
	for (unsigned i = 0; i < KST_FILES_NAMES && (k->dirs & bitOf(n)); i++) n = kstNamesAfter(n);
	return n;
}

unsigned kstNamesNext(const KstNames *k) { return nextAfter(k, k->latest); }

/** @brief `KSTnnn.TXT` of ring number @p n. */
static void ringName(unsigned n, char *name) {
	char base[KST_STORE_PRESET_MAX];
	kstNamesBase(n, base);
	kst_store_file_name(base, name, KST_STORE_NAME_MAX);
}

kst_store_t kstFilesNextName(char *base) {
	base[0] = '\0';
	KstNames k;
	kst_store_t r = kstFilesNames(&k);
	if (r == KST_STORE_OK) kstNamesBase(kstNamesNext(&k), base);
	return r;
}

/**
 * @brief Write @p n into `KST/LAST`, in place: the file keeps its cluster,
 *        so a rewrite costs its data sector and its directory entry.
 */
static kst_store_io_result_t writeMarker(unsigned n) {
	bool made;
	kst_store_io_result_t d = makeDir(&made);
	if (d != KST_STORE_IO_OK) return d;
	char path[KST_FILES_PATH_MAX];
	pathOf(KST_FILES_MARKER_NAME, path);
	FRESULT fr = f_open(&s_fil, path, FA_WRITE | FA_OPEN_ALWAYS);
	if (fr != FR_OK) return createError(fr, path);
	char text[KST_FILES_MARKER_LEN + 1];
	snprintf(text, sizeof(text), "%03u\n", n);
	UINT put = 0;
	kst_store_io_result_t r = KST_STORE_IO_OK;
	fr = f_write(&s_fil, text, KST_FILES_MARKER_LEN, &put);
	if (fr != FR_OK)                          r = createError(fr, path);
	else if (put < KST_FILES_MARKER_LEN)      r = KST_STORE_IO_FULL;
	else if (f_truncate(&s_fil) != FR_OK)     r = KST_STORE_IO_ERROR;
	if (f_close(&s_fil) != FR_OK && r == KST_STORE_IO_OK) r = KST_STORE_IO_ERROR;
	return r;
}

static kst_store_t storeResult(kst_store_io_result_t r);

/** @brief Name @p n the latest in `KST/LAST`; the marker's result as a create's. */
static kst_store_t markLatest(KstNames *k, unsigned n) {
	kst_store_t m = storeResult(writeMarker(n));
	if (m == KST_STORE_OK) k->latest = k->marked = n;
	return m;
}

/**
 * @brief Write @p len bytes over the file @p name, in place: the file keeps
 *        its first cluster, so a file of one cluster costs its data sectors
 *        and its directory entry, and no FAT (File Allocation Table) sector.
 *        A write that fails leaves the file with part of the new bytes,
 *        which its checksum line then refuses at LOAD.
 */
static kst_store_io_result_t ioOverwrite(const char *name, const char *data, size_t len) {
	char path[KST_FILES_PATH_MAX];
	pathOf(name, path);
	FRESULT fr = f_open(&s_fil, path, FA_WRITE | FA_OPEN_EXISTING);
	if (fr != FR_OK) return createError(fr, path);
	UINT put = 0;
	kst_store_io_result_t r = KST_STORE_IO_OK;
	fr = f_write(&s_fil, data, (UINT)len, &put);
	if (fr != FR_OK)                     r = createError(fr, path);
	else if (put < len)                  r = KST_STORE_IO_FULL;
	else if (f_truncate(&s_fil) != FR_OK || f_sync(&s_fil) != FR_OK) r = KST_STORE_IO_ERROR;
	if (f_close(&s_fil) != FR_OK && r == KST_STORE_IO_OK) r = KST_STORE_IO_ERROR;
	return r;
}

/**
 * @brief Put @p text under ring number @p n: over the file there, the
 *        oldest of a full ring, or as a new file. @p name receives
 *        `KSTnnn.TXT`, and stays untouched unless `KST_STORE_OK`.
 */
static kst_store_t putNumbered(KstNames *k, unsigned n, const char *text, size_t len,
                               char *name) {
	char file[KST_STORE_NAME_MAX];
	ringName(n, file);
	kst_store_t r = storeResult(isTaken(k->taken, n) ? ioOverwrite(file, text, len)
	                                                 : ioCreate(nullptr, file, text, len));
	if (r != KST_STORE_OK) return r;
	k->taken |= bitOf(n);
	snprintf(name, KST_STORE_NAME_MAX, "%s", file);
	return KST_STORE_OK;
}

kst_store_t kstFilesSaveNext(const char *text, size_t len, char *name) {
	name[0] = '\0';
	if (!kstFilesMounted()) return KST_STORE_NO_CARD;
	KstDirScope in;
	KstNames k;
	kst_store_t r = kstFilesNames(&k);
	if (r != KST_STORE_OK) return r;
	unsigned n = kstNamesNext(&k);
	r = markLatest(&k, n);
	return r == KST_STORE_OK ? putNumbered(&k, n, text, len, name) : r;
}

void kstFilesOrder(kst_store_entry_t *e, size_t count, unsigned latest) {
	// The store lists the ring's names in one run by falling number: the
	// names above the latest move behind the rest of the run.
	size_t first = count, end = count;
	for (size_t i = 0; i < count; i++) {
		bool ring = ringNumber(e[i].name) != 0;
		if (ring && first == count) first = i;
		if (!ring && first < count) { end = i; break; }
	}
	if (first == count) return;
	size_t mid = first;
	while (mid < end && ringNumber(e[mid].name) > latest) mid++;
	for (size_t moved = 0; moved < mid - first; moved++) {
		kst_store_entry_t t = e[first];
		memmove(&e[first], &e[first + 1], (end - first - 1) * sizeof(e[0]));
		e[end - 1] = t;
	}
}

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
	KstDirScope in;
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
	KstDirScope in;
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
 * @brief Rename the record at @p from to ring number @p n, after removing
 *        the file there, the oldest of a full ring.
 */
static kst_store_t keepRecord(KstNames *k, unsigned n, const char *from, KstFirstWrite *out) {
	char name[KST_STORE_NAME_MAX], to[KST_FILES_PATH_MAX];
	ringName(n, name);
	pathOf(name, to);
	if (isTaken(k->taken, n) && f_unlink(to) == FR_OK) k->taken &= ~bitOf(n);
	FRESULT fr = f_rename(from, to);
	if (fr != FR_OK) {
		// FatFs reports a rename whose last sync failed, and its sector
		// window still holds the change, which the next call writes: ask the
		// card where the record is.
		if (f_stat(from, &s_fno) != FR_NO_FILE || f_stat(to, &s_fno) != FR_OK)
			return renameError(fr);
	}
	k->taken |= bitOf(n);
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
	KstDirScope in;
	// One walk names both files, and one marker write claims both: a record
	// to keep takes the next number, the backup the one after it.
	KstNames k;
	kst_store_t names = kstFilesNames(&k);
	char from[KST_FILES_PATH_MAX];
	pathOf(KST_FILES_RECORD_NAME, from);
	FRESULT st = f_stat(from, &s_fno);
	// A directory of that name is no record; the create then refuses it.
	bool record = st == FR_OK && !(s_fno.fattrib & AM_DIR);
	// A card that does not say whether a record is there keeps none.
	kst_store_t kept = (st == FR_OK || st == FR_NO_FILE || st == FR_NO_PATH)
	                       ? KST_STORE_OK : KST_STORE_READ_ERROR;
	unsigned nKept = 0, nBackup = 0;
	kst_store_t mark = names;
	if (names == KST_STORE_OK) {
		if (record) nKept = kstNamesNext(&k);
		nBackup = nKept ? nextAfter(&k, nKept) : kstNamesNext(&k);
		mark = markLatest(&k, nBackup);
	}
	if (record) kept = mark != KST_STORE_OK ? mark : keepRecord(&k, nKept, from, out);
	out->kept = (uint8_t)kept;

	kst_store_t r = mark;
	if (r == KST_STORE_OK) {
		size_t n = kst_file_format(backup, s_text, sizeof(s_text));
		r = putNumbered(&k, nBackup, s_text, n, out->backupName);
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
