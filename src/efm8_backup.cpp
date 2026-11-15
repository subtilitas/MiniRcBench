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

BackupResult efm8BackupSave(const ew_contact_t *c, const ew_ident_t *id, const uint8_t *page, char *name) {
	static char text[EW_FILE_MAX];
	name[0] = '\0';
	size_t n = efm8BackupFormat(c, id, page, text, sizeof(text));
	if (n == 0) return BackupResult::WriteError;
	return backupSave(efm8BackupDevice(id->fw), text, n, name);
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
