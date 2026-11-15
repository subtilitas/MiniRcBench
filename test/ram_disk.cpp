/**
 * @file ram_disk.cpp
 * @brief The RAM disk under FatFs in the `make lognum` programs.
 *        @see ram_disk.h
 */

#include "ram_disk.h"

#include "diskio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RamDisk g_disk = {};

DSTATUS disk_initialize(BYTE) { g_disk.noInit = false; return g_disk.protect ? STA_PROTECT : 0; }
DSTATUS disk_status(BYTE) {
	return (DSTATUS)((g_disk.noInit ? STA_NOINIT : 0) | (g_disk.protect ? STA_PROTECT : 0));
}

DRESULT disk_read(BYTE, BYTE *buf, LBA_t sector, UINT count) {
	RamImage &d = g_disk.images[g_disk.card];
	for (UINT i = 0; i < count; i++) {
		auto it = d.find(sector + i);
		if (it == d.end()) memset(buf + i * 512u, 0, 512);
		else memcpy(buf + i * 512u, it->second.data(), 512);
	}
	return RES_OK;
}

DRESULT disk_write(BYTE, const BYTE *buf, LBA_t sector, UINT count) {
	if (g_disk.protect) return RES_WRPRT;
	if (g_disk.failWrites) return RES_ERROR;
	if (g_disk.failPrefix) {
		size_t n = strlen(g_disk.failPrefix);
		for (UINT i = 0; i < count; i++)
			if (memcmp(buf + i * 512u, g_disk.failPrefix, n) == 0) return RES_ERROR;
	}
	RamImage &d = g_disk.images[g_disk.card];
	for (UINT i = 0; i < count; i++) {
		const BYTE *p = buf + i * 512u;
		bool zero = true;
		for (int j = 0; j < 512 && zero; j++) zero = p[j] == 0;
		if (zero) d.erase(sector + i);
		else d[sector + i].assign(p, p + 512);
	}
	return RES_OK;
}

DRESULT disk_ioctl(BYTE, BYTE cmd, void *buf) {
	switch (cmd) {
		case CTRL_SYNC:        return RES_OK;
		case GET_SECTOR_COUNT: *(LBA_t *)buf = g_disk.sectors; return RES_OK;
		case GET_SECTOR_SIZE:  *(WORD *)buf = 512; return RES_OK;
		case GET_BLOCK_SIZE:   *(DWORD *)buf = 1; return RES_OK;
		default:               return RES_PARERR;
	}
}

DWORD get_fattime() { return ((DWORD)(2026 - 1980) << 25) | (1u << 21) | (1u << 16); }

void ramDiskFormat(FATFS *fs, BYTE fmt, LBA_t sectors, DWORD au) {
	g_disk.sectors = sectors;
	g_disk.noInit = g_disk.protect = g_disk.failWrites = false;
	g_disk.failPrefix = nullptr;
	static BYTE work[FF_MAX_SS * 8];
	MKFS_PARM opt = { fmt, 2, 0, 0, au };
	for (int c = 1; c >= 0; c--) {
		g_disk.card = c;
		g_disk.images[c].clear();
		if (f_mkfs("", &opt, work, sizeof(work)) != FR_OK) { fprintf(stderr, "mkfs failed\n"); exit(2); }
	}
	if (f_mount(fs, "", 1) != FR_OK) { fprintf(stderr, "mount failed\n"); exit(2); }
}
