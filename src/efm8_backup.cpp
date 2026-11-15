/**
 * @file efm8_backup.cpp
 * @brief The Bluejay and BLHeli_S backup's text and its save. @see efm8_backup.h
 */

#include "efm8_backup.h"
#include "card_dir.h"
#include "ew_file.h"

#include <string.h>

BackupDevice efm8BackupDevice(uint8_t fw) {
	if (fw == EW_FW_BJ) return BackupDevice::Bluejay;
	if (fw == EW_FW_S) return BackupDevice::BlheliS;
	return BackupDevice::Count;
}

size_t efm8BackupFormat(const ew_contact_t *c, const ew_ident_t *id, const uint8_t *page, char *out,
                        size_t cap) {
	// A file holds a page of 256 bytes and more: kept off core0's stack.
	static ew_file_t f;
	if (efm8BackupDevice(id->fw) == BackupDevice::Count) return 0;
	if (!ew_file_capture(&f, c, id, page)) return 0;
	return ew_file_format(&f, out, cap);
}

/** @brief Whether the ring's latest backup is @p d's and holds @p n bytes of @p text; @p name takes its name. */
static bool latestHolds(BackupDevice d, const char *text, size_t n, char *name) {
	static char prev[EW_FILE_MAX];
	BackupNames k;
	if (backupNames(&k) != BackupResult::Ok || !k.latest) return false;
	if (!(k.devices[k.latest - 1] & (1u << (unsigned)d))) return false;
	char file[BACKUP_NAME_MAX];
	backupName(d, k.latest, file);
	size_t got = 0;
	if (backupLoad(file, prev, sizeof(prev), &got) != BackupResult::Ok || got != n ||
	    memcmp(prev, text, n) != 0)
		return false;
	memcpy(name, file, BACKUP_NAME_MAX);
	return true;
}

BackupResult efm8BackupSave(const ew_contact_t *c, const ew_ident_t *id, const uint8_t *page, char *name,
                            bool skipSame) {
	static char text[EW_FILE_MAX];
	name[0] = '\0';
	size_t n = efm8BackupFormat(c, id, page, text, sizeof(text));
	if (n == 0) return BackupResult::WriteError;
	BackupDevice d = efm8BackupDevice(id->fw);
	if (skipSame && cardMounted() && latestHolds(d, text, n, name)) return BackupResult::Ok;
	return backupSave(d, text, n, name);
}

BackupResult efm8BackupNext(uint8_t fw, char *name) {
	name[0] = '\0';
	BackupDevice d = efm8BackupDevice(fw);
	if (d == BackupDevice::Count) return BackupResult::NotFound;
	if (!cardMounted()) return BackupResult::NoCard;
	CardDirScope in(BACKUP_DIR);
	BackupNames k;
	BackupResult r = backupNames(&k);
	if (r != BackupResult::Ok) return r;
	backupName(d, backupNext(&k), name);
	return BackupResult::Ok;
}
