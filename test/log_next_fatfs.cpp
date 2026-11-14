/**
 * @file log_next_fatfs.cpp
 * @brief The next log's file (`src/sd_log_next.cpp`) over FatFs R0.15 itself.
 *
 * Built by `make -C test lognum`, against FatFs from the SD library at the
 * commit `CMakeLists.txt` pins, with the library's `ffconf.h` (long names,
 * exFAT), as `make kstcount` builds it. The card is a RAM disk; the test can
 * swap it for another card image, report it uninitialised as the SDIO glue
 * does after a card fault, or report it write-protected.
 *
 * Covers the scan (gaps, the full range, foreign names, letter case, long
 * names), the number kept between starts, a file the scan did not count,
 * a failed f_mkdir() and a failed scan forgetting the number, and FatFs
 * mounting again, by f_mount() or on its own after a card fault, reading
 * `LOGS/` again: on FAT and on exFAT.
 */

#include "sd_log_next.h"
#include "check.h"

#include "diskio.h"
#include "ff.h"

#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

int g_failures = 0;

// ---------------------------------------------------------------------------
// the card: sparse sectors; two images to swap between
// ---------------------------------------------------------------------------

typedef std::map<LBA_t, std::vector<BYTE>> Image;

static Image  s_cards[2];
static int    s_card = 0;
static LBA_t  s_sectors;
/** @brief Reported by disk_status() until the next disk_initialize(). */
static bool   s_noInit = false;
static bool   s_protect = false;

DSTATUS disk_initialize(BYTE) { s_noInit = false; return s_protect ? STA_PROTECT : 0; }
DSTATUS disk_status(BYTE) {
	return (DSTATUS)((s_noInit ? STA_NOINIT : 0) | (s_protect ? STA_PROTECT : 0));
}

DRESULT disk_read(BYTE, BYTE *buf, LBA_t sector, UINT count) {
	Image &d = s_cards[s_card];
	for (UINT i = 0; i < count; i++) {
		auto it = d.find(sector + i);
		if (it == d.end()) memset(buf + i * 512u, 0, 512);
		else memcpy(buf + i * 512u, it->second.data(), 512);
	}
	return RES_OK;
}

DRESULT disk_write(BYTE, const BYTE *buf, LBA_t sector, UINT count) {
	if (s_protect) return RES_WRPRT;
	Image &d = s_cards[s_card];
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
		case GET_SECTOR_COUNT: *(LBA_t *)buf = s_sectors; return RES_OK;
		case GET_SECTOR_SIZE:  *(WORD *)buf = 512; return RES_OK;
		case GET_BLOCK_SIZE:   *(DWORD *)buf = 1; return RES_OK;
		default:               return RES_PARERR;
	}
}

DWORD get_fattime() { return ((DWORD)(2026 - 1980) << 25) | (1u << 21) | (1u << 16); }

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static FATFS s_fs;
static const char *const DIR_PATH = "/LOGS";

/** @brief Format both card images with @p fmt and mount card 0. */
static void format(BYTE fmt, LBA_t sectors, DWORD au) {
	s_sectors = sectors;
	s_noInit = s_protect = false;
	static BYTE work[FF_MAX_SS * 8];
	MKFS_PARM opt = { fmt, 2, 0, 0, au };
	for (int c = 1; c >= 0; c--) {
		s_card = c;
		s_cards[c].clear();
		if (f_mkfs("", &opt, work, sizeof(work)) != FR_OK) { fprintf(stderr, "mkfs failed\n"); exit(2); }
	}
	if (f_mount(&s_fs, "", 1) != FR_OK) { fprintf(stderr, "mount failed\n"); exit(2); }
}

/** @brief Put an empty file at @p path, as a computer would. */
static void put(const char *path) {
	FIL f;
	if (f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) { fprintf(stderr, "put %s\n", path); exit(2); }
	f_close(&f);
}

/** @brief One start: the number opened, 0 for none, and why not. */
static uint32_t start(SdLogNextCache *c, SdLogEnd *why = nullptr) {
	FIL f;
	uint32_t n = 0;
	SdLogEnd e = sdLogOpenNext(c, &s_fs, DIR_PATH, &f, &n);
	if (why) *why = e;
	if (e != SdLogEnd::None) return 0;
	UINT put = 0;
	f_write(&f, "H", 1, &put);
	f_close(&f);
	return n;
}

static bool exists(const char *path) {
	FILINFO fno;
	return f_stat(path, &fno) == FR_OK;
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void testScan(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Log number over FatFs, %s: the scan", vol);
	section(t);
	uint32_t next = 77;
	checkInt("no LOGS/: the scan fails", sdLogScanNext(DIR_PATH, &next), FR_NO_PATH);
	checkInt("and leaves the number", (long)next, 77);
	f_mkdir(DIR_PATH);
	checkTrue("an empty LOGS/: 1", sdLogScanNext(DIR_PATH, &next) == FR_OK && next == 1);
	// Gaps, letter case, foreign names, a long name, a directory.
	put("/LOGS/LOG00003.BFL");
	put("/LOGS/LOG00017.BFL");
	put("/LOGS/log00009.bfl");
	put("/LOGS/notes.txt");
	put("/LOGS/LOG0099.BFL");
	put("/LOGS/LOG00500.TXT");
	put("/LOGS/LOG000600.BFL");
	put("/LOGS/Flight with the new prop LOG00700.BFL");
	put("/LOGS/LOG00800.BFL.bak");
	f_mkdir("/LOGS/OLD");
	put("/LOGS/OLD/LOG00900.BFL");
	put("/LOG00950.BFL");
	checkTrue("3, 17 and 9 with gaps: 18, foreign names ignored",
	          sdLogScanNext(DIR_PATH, &next) == FR_OK && next == 18);
	put("/LOGS/log00031.Bfl");
	checkTrue("a name in mixed case counts: 32", sdLogScanNext(DIR_PATH, &next) == FR_OK && next == 32);
	put("/LOGS/LOG99998.BFL");
	checkTrue("LOG99998.BFL: 99999", sdLogScanNext(DIR_PATH, &next) == FR_OK && next == 99999);
	put("/LOGS/LOG99999.BFL");
	checkTrue("LOG99999.BFL: none free", sdLogScanNext(DIR_PATH, &next) == FR_OK && next == 0);
}

static void testOpen(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Log number over FatFs, %s: starts on one mount", vol);
	section(t);
	SdLogNextCache c = {};
	checkInt("no LOGS/: made, LOG00001.BFL", (long)start(&c), 1);
	checkTrue("the file is there", exists("/LOGS/LOG00001.BFL"));
	checkInt("the next start: 2", (long)start(&c), 2);
	// A computer deleted the files below the highest, between mounts.
	put("/LOGS/LOG00041.BFL");
	f_unlink("/LOGS/LOG00001.BFL");
	f_unlink("/LOGS/LOG00002.BFL");
	sdLogNextForget(&c);
	checkInt("after LOG00041.BFL, the ones below gone: 42", (long)start(&c), 42);
	// A file the kept number did not count: never written over, read again.
	put("/LOGS/LOG00043.BFL");
	SdLogEnd why = SdLogEnd::None;
	checkInt("LOG00043.BFL already there: no start", (long)start(&c, &why), 0);
	checkTrue("the open failed, the number forgotten", why == SdLogEnd::OpenFailed && !c.known);
	checkInt("the next start reads LOGS/ again: 44", (long)start(&c), 44);
	// The full range.
	put("/LOGS/LOG99998.BFL");
	sdLogNextForget(&c);
	checkInt("after LOG99998.BFL: 99999", (long)start(&c), 99999);
	checkInt("after LOG99999.BFL: no start", (long)start(&c, &why), 0);
	checkTrue("none free", why == SdLogEnd::NoFreeNumber);
	checkTrue("no file past it", !exists("/LOGS/LOG100000.BFL") && !exists("/LOGS/LOG00000.BFL"));
}

static void testMounts(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Log number over FatFs, %s: another mount reads LOGS/ again", vol);
	section(t);
	SdLogNextCache c = {};
	f_mkdir(DIR_PATH);
	put("/LOGS/LOG00042.BFL");
	checkInt("card A, LOG00042.BFL: 43", (long)start(&c), 43);
	checkTrue("the number is kept", c.known && c.next == 44);

	// Card B in the slot, the old card's mount still in place; the glue
	// reports the card uninitialised after the fault the swap caused, so
	// FatFs mounts again on its next call, without f_mount().
	WORD idA = s_fs.id;
	s_card = 1;
	s_noInit = true;
	checkInt("card B, an empty card, after a fault: LOG00001.BFL", (long)start(&c), 1);
	checkTrue("FatFs mounted again", s_fs.id != idA);
	checkTrue("card B holds LOG00001.BFL and no LOG00044.BFL",
	          exists("/LOGS/LOG00001.BFL") && !exists("/LOGS/LOG00044.BFL"));
	checkInt("card B's next: 2", (long)start(&c), 2);

	// Card A back by f_mount(), as sdLogBegin() mounts.
	s_card = 0;
	f_mount(&s_fs, "", 1);
	checkInt("card A again, by f_mount(): 44", (long)start(&c), 44);

	// Card A edited in a card reader: emptied, then a fault and a remount.
	f_unlink("/LOGS/LOG00042.BFL");
	f_unlink("/LOGS/LOG00043.BFL");
	f_unlink("/LOGS/LOG00044.BFL");
	s_noInit = true;
	checkInt("card A emptied, after a fault: 1", (long)start(&c), 1);
}

static void testFailures(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Log number over FatFs, %s: a failure forgets the number", vol);
	section(t);
	SdLogNextCache c = {};
	checkInt("the first start: 1", (long)start(&c), 1);
	checkTrue("kept", c.known);
	// A write-protected card: the directory cannot be made. The number is
	// forgotten, so a card changed meanwhile is read again.
	f_unlink("/LOGS/LOG00001.BFL");
	f_unlink(DIR_PATH);
	s_protect = true;
	s_noInit = true;
	SdLogEnd why = SdLogEnd::None;
	checkInt("LOGS/ not made: no start", (long)start(&c, &why), 0);
	checkTrue("the open failed, the number forgotten", why == SdLogEnd::OpenFailed && !c.known);
	s_protect = false;
	s_noInit = true;
	f_mkdir(DIR_PATH);
	put("/LOGS/LOG00100.BFL");
	c.known = true;          // as if kept from before, on this very mount
	c.fsId = s_fs.id;
	c.next = 2;
	s_protect = true;
	checkInt("LOGS/ there, the card write-protected: no start", (long)start(&c, &why), 0);
	checkTrue("the open failed, the number forgotten", why == SdLogEnd::OpenFailed && !c.known);
	s_protect = false;
	checkInt("writable again, the same mount: LOGS/ read again, 101", (long)start(&c), 101);
	// LOGS/ a file, not a directory: f_mkdir() says it exists, the scan fails.
	f_unlink("/LOGS/LOG00100.BFL");
	f_unlink("/LOGS/LOG00101.BFL");
	f_unlink(DIR_PATH);
	put(DIR_PATH);
	c.known = true;
	c.fsId = s_fs.id;
	c.next = 5;
	s_noInit = true;         // mounted again: the kept number is not used
	checkInt("LOGS a file: the scan fails, no start", (long)start(&c, &why), 0);
	checkTrue("the open failed, the number forgotten", why == SdLogEnd::OpenFailed && !c.known);
	f_unlink(DIR_PATH);
}

static void runVolume(const char *vol, BYTE fmt, LBA_t sectors, DWORD au) {
	format(fmt, sectors, au);
	testScan(vol);
	format(fmt, sectors, au);
	testOpen(vol);
	format(fmt, sectors, au);
	testMounts(vol);
	format(fmt, sectors, au);
	testFailures(vol);
	f_mount(nullptr, "", 0);
}

int main() {
	runVolume("FAT32", FM_FAT32, 2u * 1024u * 1024u, 4096);      // 1 GiB, 4 KiB clusters
	runVolume("exFAT", FM_EXFAT, 2u * 1024u * 1024u, 32768);     // 1 GiB, 32 KiB clusters
	printf("\n=======================\n");
	if (g_failures) printf("%d CHECK(S) FAILED\n", g_failures);
	else printf("all checks passed\n");
	return g_failures ? 1 : 0;
}
