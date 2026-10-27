/**
 * @file am32_backup.cpp
 * @brief The AM32 backup's text and its save. @see am32_backup.h
 */

#include "am32_backup.h"
#include "am32_eeprom.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** @brief Bytes a line of the byte block. */
#define BYTES_PER_LINE 16

/** @brief Text appended into a buffer, with a flag for overflow. */
struct Buf {
	char  *out;
	size_t cap;
	size_t n;
	bool   over;
};

static void put(Buf *b, const char *fmt, ...) {
	if (b->over) return;
	va_list ap;
	va_start(ap, fmt);
	int k = vsnprintf(b->out + b->n, b->cap - b->n, fmt, ap);
	va_end(ap);
	if (k < 0 || (size_t)k >= b->cap - b->n) {
		b->over = true;
		return;
	}
	b->n += (size_t)k;
}

uint32_t am32BackupCrc32(const char *p, size_t n) {
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < n; i++) {
		crc ^= (uint8_t)p[i];
		for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

size_t am32BackupFormat(const uint8_t *settings, const char *escType, uint16_t address,
                        char *out, size_t cap) {
	if (!settings || !out || cap == 0) return 0;
	Buf b = { out, cap, 0, false };
	uint8_t layout = settings[AM32_OFF_LAYOUT_REVISION];
	put(&b, "AM32 ESC settings, format %u\n", (unsigned)AM32_BACKUP_FORMAT);
	put(&b, "ESC: %s\n", escType ? escType : "");
	put(&b, "Settings address: %04X\n", (unsigned)address);
	put(&b, "Layout revision: %u\n", (unsigned)layout);
	put(&b, "Firmware: %u.%u\n", (unsigned)settings[AM32_OFF_MAIN_REVISION],
	    (unsigned)settings[AM32_OFF_SUB_REVISION]);
	char value[32];
	for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
		const Am32Field *f = &AM32_FIELDS[i];
		if (!am32FieldApplies(f, layout)) continue;
		am32FormatValue(f, settings, value, (int)sizeof(value));
		put(&b, "%s %s = %s\n", f->group, f->name, value);
	}
	put(&b, "bytes %u\n", (unsigned)AM32_SETTINGS_SIZE);
	for (unsigned row = 0; row < AM32_SETTINGS_SIZE; row += BYTES_PER_LINE) {
		put(&b, "%02X:", row);
		for (unsigned i = row; i < row + BYTES_PER_LINE && i < AM32_SETTINGS_SIZE; i++)
			put(&b, " %02X", (unsigned)settings[i]);
		put(&b, "\n");
	}
	if (b.over) return 0;
	put(&b, "checksum %08lX\n", (unsigned long)am32BackupCrc32(out, b.n));
	return b.over ? 0 : b.n;
}

BackupResult am32BackupSave(const uint8_t *settings, const char *escType, uint16_t address,
                            char *name) {
	static char text[AM32_BACKUP_MAX];
	name[0] = '\0';
	size_t n = am32BackupFormat(settings, escType, address, text, sizeof(text));
	if (n == 0) return BackupResult::WriteError;
	return backupSave(BackupDevice::Am32, text, n, name);
}
