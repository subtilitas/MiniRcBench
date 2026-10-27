/**
 * @file backup_ring.cpp
 * @brief The ring of device backups in `BACKUP`. @see backup_ring.h
 */

#include "backup_ring.h"
#include "card_dir.h"

#include "ff.h"
#include <stdio.h>
#include <string.h>

static const char *const PREFIXES[(unsigned)BackupDevice::Count] = { "KST", "AM", "HT" };

/** @brief The extension of every backup. */
static const char EXT[] = ".TXT";

static FILINFO s_fno;

static uint64_t bitOf(unsigned n) { return 1ull << (n - 1u); }

static bool isTaken(uint64_t taken, unsigned n) { return (taken & bitOf(n)) != 0; }

const char *backupPrefix(BackupDevice d) {
	return (unsigned)d < (unsigned)BackupDevice::Count ? PREFIXES[(unsigned)d] : "";
}

unsigned backupAfter(unsigned n) { return n % BACKUP_RING_NAMES + 1u; }

unsigned backupGuess(uint64_t taken) {
	if (!taken) return 0;
	unsigned highest = 0, free = 0;
	for (unsigned n = 1; n <= BACKUP_RING_NAMES; n++) {
		if (isTaken(taken, n)) highest = n;
		else if (!free) free = n;
	}
	if (highest < BACKUP_RING_NAMES || !free) return highest;
	return free == 1 ? BACKUP_RING_NAMES : free - 1u;
}

unsigned backupMarker(const char *text, size_t len) {
	if (len != BACKUP_MARKER_LEN || text[3] != '\n') return 0;
	unsigned n = 0;
	for (int i = 0; i < 3; i++) {
		if (text[i] < '0' || text[i] > '9') return 0;
		n = n * 10u + (unsigned)(text[i] - '0');
	}
	return n >= 1 && n <= BACKUP_RING_NAMES ? n : 0;
}

unsigned backupNext(const BackupNames *k) {
	unsigned n = backupAfter(k->latest);
	for (unsigned i = 0; i < BACKUP_RING_NAMES && (k->dirs & bitOf(n)); i++) n = backupAfter(n);
	return n;
}

static char upper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 32) : c; }

unsigned backupNumber(const char *name, BackupDevice *dev) {
	size_t len = strlen(name);
	for (unsigned d = 0; d < (unsigned)BackupDevice::Count; d++) {
		size_t p = strlen(PREFIXES[d]);
		if (len != p + 3 + sizeof(EXT) - 1) continue;
		bool match = true;
		for (size_t i = 0; i < p && match; i++) match = upper(name[i]) == PREFIXES[d][i];
		for (size_t i = 0; i < sizeof(EXT) - 1 && match; i++) match = upper(name[p + 3 + i]) == EXT[i];
		unsigned n = 0;
		for (size_t i = p; i < p + 3 && match; i++) {
			match = name[i] >= '0' && name[i] <= '9';
			n = n * 10u + (unsigned)(name[i] - '0');
		}
		if (!match || n < 1 || n > BACKUP_RING_NAMES) continue;
		if (dev) *dev = (BackupDevice)d;
		return n;
	}
	return 0;
}

void backupName(BackupDevice d, unsigned n, char *out) {
	snprintf(out, BACKUP_NAME_MAX, "%s%03u%s", backupPrefix(d), n, EXT);
}

static void markName(const char *name, bool isDir, void *ctx) {
	BackupNames *k = (BackupNames *)ctx;
	BackupDevice d;
	unsigned n = backupNumber(name, &d);
	if (!n) return;
	k->taken |= bitOf(n);
	if (isDir) k->dirs |= bitOf(n);
	else k->devices[n - 1] |= (uint8_t)(1u << (unsigned)d);
}

/** @brief The number in `BACKUP/LAST`, 0 when it is missing or holds anything else. */
static unsigned readMarker() {
	char text[BACKUP_MARKER_LEN + 1];
	int32_t got = cardRead(BACKUP_DIR, BACKUP_MARKER_NAME, text, sizeof(text));
	return got < 0 ? 0 : backupMarker(text, (size_t)got);
}

BackupResult backupNames(BackupNames *out) {
	memset(out, 0, sizeof(*out));
	if (!cardMounted()) return BackupResult::NoCard;
	CardDirScope in(BACKUP_DIR);
	BackupNames k;
	memset(&k, 0, sizeof(k));
	if (!cardWalk(BACKUP_DIR, markName, &k)) return BackupResult::ListError;
	k.marked = readMarker();
	k.latest = k.marked ? k.marked : backupGuess(k.taken);
	*out = k;
	return BackupResult::Ok;
}

static BackupResult resultOf(CardIo r) {
	switch (r) {
		case CardIo::Ok:     return BackupResult::Ok;
		case CardIo::Full:   return BackupResult::Full;
		case CardIo::Denied: return BackupResult::Denied;
		default:             return BackupResult::WriteError;
	}
}

BackupResult backupClaim(BackupNames *k, unsigned n) {
	char text[BACKUP_MARKER_LEN + 1];
	snprintf(text, sizeof(text), "%03u\n", n);
	CardIo r = cardSize(BACKUP_DIR, BACKUP_MARKER_NAME) >= 0
	               ? cardOverwrite(BACKUP_DIR, BACKUP_MARKER_NAME, text, BACKUP_MARKER_LEN)
	               : cardCreate(BACKUP_DIR, BACKUP_MARKER_NAME, text, BACKUP_MARKER_LEN);
	if (r != CardIo::Ok) return resultOf(r);
	k->latest = k->marked = n;
	return BackupResult::Ok;
}

/** @brief Remove the files of number @p n, all devices but @p keep; Count keeps none. */
static void removeAll(BackupNames *k, unsigned n, BackupDevice keep) {
	for (unsigned d = 0; d < (unsigned)BackupDevice::Count; d++) {
		if (d == (unsigned)keep || !(k->devices[n - 1] & (1u << d))) continue;
		char name[BACKUP_NAME_MAX], path[CARD_PATH_MAX];
		backupName((BackupDevice)d, n, name);
		cardPath(BACKUP_DIR, name, path);
		if (f_unlink(path) == FR_OK) k->devices[n - 1] &= (uint8_t)~(1u << d);
	}
	if (!k->devices[n - 1] && !(k->dirs & bitOf(n))) k->taken &= ~bitOf(n);
}

BackupResult backupPut(BackupNames *k, BackupDevice d, unsigned n, const char *text,
                       size_t len, char *name) {
	char file[BACKUP_NAME_MAX];
	backupName(d, n, file);
	uint8_t mine = (uint8_t)(1u << (unsigned)d);
	if (!(k->devices[n - 1] & mine) && k->devices[n - 1]) {
		// Another device's file: renamed to this device's name, which keeps
		// its cluster, then written over.
		unsigned other = 0;
		while (!(k->devices[n - 1] & (1u << other))) other++;
		char from[BACKUP_NAME_MAX], pf[CARD_PATH_MAX], pt[CARD_PATH_MAX];
		backupName((BackupDevice)other, n, from);
		cardPath(BACKUP_DIR, from, pf);
		cardPath(BACKUP_DIR, file, pt);
		if (f_rename(pf, pt) == FR_OK) {
			k->devices[n - 1] &= (uint8_t)~(1u << other);
			k->devices[n - 1] |= mine;
		}
	}
	removeAll(k, n, d);
	CardIo r = (k->devices[n - 1] & mine) ? cardOverwrite(BACKUP_DIR, file, text, len)
	                                      : cardCreate(BACKUP_DIR, file, text, len);
	if (r != CardIo::Ok) return resultOf(r);
	k->taken |= bitOf(n);
	k->devices[n - 1] |= mine;
	snprintf(name, BACKUP_NAME_MAX, "%s", file);
	return BackupResult::Ok;
}

BackupResult backupMoveIn(BackupNames *k, BackupDevice d, unsigned n, const char *fromPath,
                          char *name) {
	char file[BACKUP_NAME_MAX], to[CARD_PATH_MAX];
	backupName(d, n, file);
	removeAll(k, n, BackupDevice::Count);
	cardPath(BACKUP_DIR, file, to);
	FRESULT fr = f_rename(fromPath, to);
	if (fr != FR_OK) {
		// FatFs reports a rename whose last sync failed, and its sector
		// window still holds the change, which the next call writes: ask the
		// card where the file is.
		if (f_stat(fromPath, &s_fno) != FR_NO_FILE || f_stat(to, &s_fno) != FR_OK) {
			switch (fr) {
				case FR_WRITE_PROTECTED: return BackupResult::Denied;
				case FR_DENIED:          return BackupResult::Full;
				default:                 return BackupResult::WriteError;
			}
		}
	}
	k->taken |= bitOf(n);
	k->devices[n - 1] |= (uint8_t)(1u << (unsigned)d);
	snprintf(name, BACKUP_NAME_MAX, "%s", file);
	return BackupResult::Ok;
}

BackupResult backupSave(BackupDevice d, const char *text, size_t len, char *name) {
	name[0] = '\0';
	if (!cardMounted()) return BackupResult::NoCard;
	CardDirScope in(BACKUP_DIR);
	BackupNames k;
	BackupResult r = backupNames(&k);
	if (r != BackupResult::Ok) return r;
	unsigned n = backupNext(&k);
	r = backupClaim(&k, n);
	return r == BackupResult::Ok ? backupPut(&k, d, n, text, len, name) : r;
}

size_t backupList(const BackupNames *k, BackupDevice d, BackupFileName *names, size_t cap) {
	size_t count = 0;
	unsigned n = k->latest ? k->latest : BACKUP_RING_NAMES;
	uint8_t mine = (uint8_t)(1u << (unsigned)d);
	for (unsigned i = 0; i < BACKUP_RING_NAMES && count < cap; i++) {
		if (k->devices[n - 1] & mine) backupName(d, n, names[count++]);
		n = n == 1 ? BACKUP_RING_NAMES : n - 1u;
	}
	return count;
}

BackupResult backupLoad(const char *name, char *buf, size_t cap, size_t *len) {
	*len = 0;
	if (!cardMounted()) return BackupResult::NoCard;
	CardDirScope in(BACKUP_DIR);
	int32_t size = cardSize(BACKUP_DIR, name);
	if (size < 0) return BackupResult::NotFound;
	if ((size_t)size > cap) return BackupResult::TooLarge;
	int32_t got = cardRead(BACKUP_DIR, name, buf, cap);
	if (got != size) return BackupResult::ReadError;
	*len = (size_t)got;
	return BackupResult::Ok;
}
