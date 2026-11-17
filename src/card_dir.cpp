/**
 * @file card_dir.cpp
 * @brief The SD card's mount rule, current directory and file calls.
 *        @see card_dir.h
 */

#include "card_dir.h"
#include "sd_log.h"
#include "usb_msc.h"

#include "ff.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief The one file open at a time, and the one directory entry looked at.
 *        A FIL holds a 512-byte sector buffer and a FILINFO a 256-byte name:
 *        static, off core0's 2 KB stack. Core0 alone calls in here and no
 *        file call nests in another.
 */
static FIL     s_fil;
static FILINFO s_fno;

/** @brief The current directory, empty for the root, and how deep the scopes nest. */
static char     s_current[12];
static unsigned s_depth;
/** @brief Whether the outermost scope changed directory. */
static bool     s_changed;
/** @brief sdLogMountId() when the scope changed directory: a mount since then made the root current. */
static uint16_t s_mountId;

bool cardMounted() {
	SdLogStatus st;
	sdLogStatus(&st);
	bool mounted = st.state == SdLogState::Idle || st.state == SdLogState::Logging;
	return mounted && mscGetState() == MscState::Idle;
}

/** @brief `/<dir>` into @p out. */
static void absDir(const char *dir, char *out) { snprintf(out, CARD_PATH_MAX, "/%s", dir); }

CardDirScope::CardDirScope(const char *dir, bool on) : m_on(on) {
	if (!m_on) return;
	if (s_depth++ == 0) {
		char p[CARD_PATH_MAX];
		absDir(dir, p);
		s_changed = f_chdir(p) == FR_OK;
		if (s_changed) snprintf(s_current, sizeof(s_current), "%s", dir);
		s_mountId = sdLogMountId();
	}
}

CardDirScope::~CardDirScope() {
	if (!m_on) return;
	if (--s_depth == 0 && s_changed) {
		f_chdir("/");
		s_changed = false;
		s_current[0] = '\0';
	}
}

bool cardDirCurrent(const char *dir) {
	// A mount since the scope's f_chdir(), the remount after a failed close
	// or FatFs's own after a card fault, made the root current again: the
	// paths are absolute from then on.
	return s_current[0] && s_mountId == sdLogMountId() && strcmp(s_current, dir) == 0;
}

void cardPath(const char *dir, const char *name, char *out) {
	if (cardDirCurrent(dir)) snprintf(out, CARD_PATH_MAX, "%s", name);
	else snprintf(out, CARD_PATH_MAX, "/%s/%s", dir, name);
}

const char *cardDirPath(const char *dir) {
	static char p[CARD_PATH_MAX];
	if (cardDirCurrent(dir)) return "";
	absDir(dir, p);
	return p;
}

/**
 * @brief The CardIo of a FatFs error while creating @p path.
 *
 * FatFs answers a full volume or directory with FR_DENIED, and an existing
 * directory or read-only object of that name too: the name tells them apart.
 * FR_WRITE_PROTECTED needs the disk layer's STA_PROTECT, which this board's
 * card driver never sets; a card that refuses writes fails as FR_DISK_ERR.
 */
static CardIo createError(FRESULT fr, const char *path) {
	switch (fr) {
		case FR_WRITE_PROTECTED: return CardIo::Denied;
		case FR_DENIED:          return f_stat(path, &s_fno) == FR_OK ? CardIo::Denied : CardIo::Full;
		default:                 return CardIo::Error;
	}
}

CardIo cardMakeDir(const char *dir, bool *made) {
	*made = false;
	// The current directory: it is there.
	if (cardDirCurrent(dir)) return CardIo::Ok;
	char p[CARD_PATH_MAX];
	absDir(dir, p);
	FRESULT fr = f_mkdir(p);
	if (fr == FR_OK) {
		*made = true;
		return CardIo::Ok;
	}
	if (fr != FR_EXIST) return createError(fr, p);
	if (f_stat(p, &s_fno) != FR_OK) return CardIo::Error;
	return (s_fno.fattrib & AM_DIR) ? CardIo::Ok : CardIo::Denied;
}

/** @brief Write @p len bytes into the open s_fil, truncate, sync and close it. */
static CardIo writeOpen(const char *path, const char *data, size_t len) {
	UINT put = 0;
	CardIo r = CardIo::Ok;
	FRESULT fr = f_write(&s_fil, data, (UINT)len, &put);
	// FatFs reports a full volume as a short write with FR_OK.
	if (fr != FR_OK)     r = createError(fr, path);
	else if (put < len)  r = CardIo::Full;
	else if (f_truncate(&s_fil) != FR_OK || f_sync(&s_fil) != FR_OK) r = CardIo::Error;
	if (f_close(&s_fil) != FR_OK && r == CardIo::Ok) r = CardIo::Error;
	return r;
}

CardIo cardCreate(const char *dir, const char *name, const char *data, size_t len) {
	bool made;
	CardIo d = cardMakeDir(dir, &made);
	if (d != CardIo::Ok) return d;
	char path[CARD_PATH_MAX];
	cardPath(dir, name, path);
	FRESULT fr = f_open(&s_fil, path, FA_WRITE | FA_CREATE_NEW);
	CardIo r;
	if (fr == FR_EXIST) {
		r = CardIo::Exists;
	} else if (fr != FR_OK) {
		r = createError(fr, path);
	} else {
		r = writeOpen(path, data, len);
		// A failed create leaves no file behind. A file whose close failed
		// stays open in FatFs (f_close() clears obj.fs only on success),
		// holding one of FatFs's 16 lock entries, and the remove would be
		// refused with FR_LOCKED: the card is mounted again first, which
		// drops the lock entries and the cached sectors. While a log runs
		// or USB DRIVE holds the card, no mount happens and the file stays.
		if (r != CardIo::Ok && s_fil.obj.fs) {
			sdLogRemount();
			cardPath(dir, name, path);  // absolute now: the mount made the root current
		}
		if (r != CardIo::Ok) f_unlink(path);
	}
	// Nor the directory this call made; one that holds a file stays.
	if (r != CardIo::Ok && r != CardIo::Exists && made) {
		char p[CARD_PATH_MAX];
		absDir(dir, p);
		f_unlink(p);
	}
	return r;
}

CardIo cardOverwrite(const char *dir, const char *name, const char *data, size_t len) {
	char path[CARD_PATH_MAX];
	cardPath(dir, name, path);
	FRESULT fr = f_open(&s_fil, path, FA_WRITE | FA_OPEN_EXISTING);
	if (fr != FR_OK) return createError(fr, path);
	CardIo r = writeOpen(path, data, len);
	// A close that failed holds the file's lock entry until a mount, as in
	// cardCreate().
	if (r != CardIo::Ok && s_fil.obj.fs) sdLogRemount();
	return r;
}

int32_t cardSize(const char *dir, const char *name) {
	char path[CARD_PATH_MAX];
	cardPath(dir, name, path);
	if (f_stat(path, &s_fno) != FR_OK || (s_fno.fattrib & AM_DIR)) return -1;
	return s_fno.fsize > INT32_MAX ? INT32_MAX : (int32_t)s_fno.fsize;
}

int32_t cardRead(const char *dir, const char *name, char *buf, size_t cap) {
	char path[CARD_PATH_MAX];
	cardPath(dir, name, path);
	if (f_open(&s_fil, path, FA_READ) != FR_OK) return -1;
	UINT got = 0;
	FRESULT fr = f_read(&s_fil, buf, (UINT)cap, &got);
	f_close(&s_fil);
	return fr == FR_OK ? (int32_t)got : -1;
}

bool cardWalk(const char *dir, CardVisit visit, void *ctx) {
	DIR d;
	FRESULT fr = f_opendir(&d, cardDirPath(dir));
	// No directory yet: no entry, and the walk reads to its end.
	if (fr == FR_NO_PATH || fr == FR_NO_FILE) return true;
	if (fr != FR_OK) return false;
	bool ok = true;
	for (;;) {
		fr = f_readdir(&d, &s_fno);
		if (fr != FR_OK) {
			ok = false;
			break;
		}
		if (s_fno.fname[0] == '\0') break;
		visit(s_fno.fname, (s_fno.fattrib & AM_DIR) != 0, ctx);
	}
	f_closedir(&d);
	return ok;
}
