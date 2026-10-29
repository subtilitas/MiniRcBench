/**
 * @file kst_card_count.cpp
 * @brief The card phases of the KST and Hitec programmers and the AM32 backup
 *        in block reads and writes: `src/kst_files.cpp`, `src/hitec_files.cpp`,
 *        `src/backup_ring.cpp`, `src/am32_backup.cpp` and rcbench's
 *        `kst_store.c` and `hitec_card.c` over FatFs R0.15 itself, on a RAM
 *        disk that counts every block it moves.
 *
 * Built by `make -C test kstcount`, which fetches FatFs from the SD library
 * at the commit `CMakeLists.txt` pins, with the library's `ffconf.h`. Each
 * phase runs on a full card: a ring of `BACKUP/` that has turned, KST and
 * AM32 backups by turns, `BACKUP/LAST`, and 49 SAVE AS files in `KST/`. The
 * volumes are FAT32 of 32 GiB with 32 KiB clusters (an SDHC card as the SD
 * Association formats it) and exFAT of 64 GiB with 128 KiB clusters (SDXC).
 * Each holds no blackbox log, 1000 logs in `LOGS/`, or 1000 logs in the root,
 * as an earlier firmware wrote them.
 *
 * Each total is the reads times one read plus the writes times one write, at
 * three sets of card times: a typical card, 10 ms each; SDHC within the SD
 * specification, 105 ms a read and 261 ms a write; SDXC within it, 105 ms a
 * read and 511 ms a write (docs/design/kst-programming.md, "The cap").
 *
 * The Hitec phases run on the same card with 49 SAVE AS files in `HITEC/`
 * and Hitec backups in the ring too (docs/design/hitec-programming.md).
 *
 * `--print` prints both tables. `--write NOTE` puts the KST table between the
 * `kst-card-count` markers of NOTE and the Hitec table between its
 * `hitec-card-count` markers, whichever it has, and `--check NOTE` fails when
 * a table there differs. A total over the card-phase cap of 15,000 ms is
 * marked "over the cap".
 */

#include "am32_backup.h"
#include "am32_eeprom.h"
#include "hitec_files.h"
#include "kst_files.h"
#include "kst_file.h"
#include "kst_store.h"
#include "sd_log.h"
#include "usb_msc.h"

#include "diskio.h"
#include "ff.h"

#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

// rcbench's fixture brings its test harness, whose reporting is not used here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "rcbench/hitec_reg_fix.h"
#pragma GCC diagnostic pop

/** @brief The card-phase cap the design note sets, in ms. */
static const double CAP_MS = 15000.0;

// ---------------------------------------------------------------------------
// the card: sparse sectors, counted
// ---------------------------------------------------------------------------

static std::map<LBA_t, std::vector<BYTE>> s_disk;
static LBA_t    s_sectors;
static unsigned s_reads, s_writes;

DSTATUS disk_initialize(BYTE) { return 0; }
DSTATUS disk_status(BYTE) { return 0; }

DRESULT disk_read(BYTE, BYTE *buf, LBA_t sector, UINT count) {
	for (UINT i = 0; i < count; i++) {
		auto it = s_disk.find(sector + i);
		if (it == s_disk.end()) memset(buf + i * 512u, 0, 512);
		else memcpy(buf + i * 512u, it->second.data(), 512);
	}
	s_reads += count;
	return RES_OK;
}

DRESULT disk_write(BYTE, const BYTE *buf, LBA_t sector, UINT count) {
	for (UINT i = 0; i < count; i++) {
		const BYTE *p = buf + i * 512u;
		bool zero = true;
		for (int j = 0; j < 512 && zero; j++) zero = p[j] == 0;
		if (zero) s_disk.erase(sector + i);
		else s_disk[sector + i].assign(p, p + 512);
	}
	s_writes += count;
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
// the board as kst_files.cpp sees it: a mounted card, no USB host
// ---------------------------------------------------------------------------

void sdLogStatus(SdLogStatus *out) {
	memset(out, 0, sizeof(*out));
	out->state = SdLogState::Idle;
}

MscState mscGetState() { return MscState::Idle; }

// ---------------------------------------------------------------------------
// scenarios
// ---------------------------------------------------------------------------

static FATFS s_fs;

/** @brief Where the volume's 1000 blackbox logs are. */
enum class Logs { None, InLogs, InRoot };

static void volume(BYTE fmt, LBA_t sectors, DWORD au, Logs logs) {
	s_disk.clear();
	s_sectors = sectors;
	static BYTE work[FF_MAX_SS * 8];
	MKFS_PARM opt = { fmt, 2, 0, 0, au };
	if (f_mkfs("", &opt, work, sizeof(work)) != FR_OK) { fprintf(stderr, "mkfs failed\n"); exit(2); }
	if (f_mount(&s_fs, "", 1) != FR_OK) { fprintf(stderr, "mount failed\n"); exit(2); }
	FIL f;
	char name[24];
	if (logs == Logs::InLogs) f_mkdir("/LOGS");
	for (unsigned i = 1; logs != Logs::None && i <= 1000; i++) {
		snprintf(name, sizeof(name), logs == Logs::InLogs ? "/LOGS/LOG%05u.BFL" : "/LOG%05u.BFL", i);
		if (f_open(&f, name, FA_WRITE | FA_CREATE_NEW) != FR_OK) { fprintf(stderr, "log failed\n"); exit(2); }
		UINT put;
		f_write(&f, "H", 1, &put);
		f_close(&f);
	}
}

/**
 * @brief A full ring that has turned, the marker at 10: KST and AM32
 *        backups by turns; and 49 SAVE AS files in KST/.
 */
static void fullCard() {
	static char text[KST_FILE_MAX];
	kst_image_t img;
	memset(&img, 0, sizeof(img));
	size_t n = kst_file_format(&img, text, sizeof(text));
	char name[KST_STORE_NAME_MAX];
	for (int i = 0; i < 60; i++) {
		BackupDevice d = (i & 1) ? BackupDevice::Am32 : BackupDevice::Kst;
		if (backupSave(d, text, n, name) != BackupResult::Ok) { fprintf(stderr, "backup failed\n"); exit(2); }
	}
	for (int i = 0; i < KST_FILES_SAVE_MAX - 1; i++) {
		if (kstFilesSaveAs(text, n, name) != KST_STORE_OK) { fprintf(stderr, "save failed\n"); exit(2); }
	}
}

/**
 * @brief A full ring that has turned, the marker at 10: KST, AM32 and Hitec
 *        backups by turns; and 49 SAVE AS files in HITEC/.
 */
static void fullHitecCard() {
	static char text[HITEC_CARD_FILE_MAX];
	hitec_image_t img;
	d485(&img);
	size_t n = hitec_card_file_text(&img, text, sizeof(text));
	char name[BACKUP_NAME_MAX];
	static const BackupDevice turn[] = { BackupDevice::Kst, BackupDevice::Am32, BackupDevice::Hitec };
	for (int i = 0; i < 60; i++) {
		if (backupSave(turn[i % 3], text, n, name) != BackupResult::Ok) { fprintf(stderr, "backup failed\n"); exit(2); }
	}
	char file[HITEC_FILES_NAME_MAX];
	for (int i = 0; i < HITEC_FILES_SAVE_MAX - 1; i++) {
		if (hitecFilesSaveAs(&img, file) != HitecFiles::Ok) { fprintf(stderr, "save failed\n"); exit(2); }
	}
}

/** @brief A record of a D series write, with SPEED @p speed. */
static hitec_card_record_t hitecRecord(int speed) {
	hitec_card_record_t r;
	memset(&r, 0, sizeof(r));
	r.series = HITEC_D;
	r.pin = 0;
	d485(&r.before);
	r.target = r.before;
	hitec_param_set(&r.target, HITEC_LAYOUT_D, HITEC_P_D_SPEED, speed);
	return r;
}

static void putRecord() {
	FIL f;
	if (f_open(&f, "KST/KST.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) exit(2);
	static char text[KST_FILE_MAX];
	kst_image_t img;
	memset(&img, 0xA5, sizeof(img));
	UINT put;
	f_write(&f, text, (UINT)kst_file_format(&img, text, sizeof(text)), &put);
	f_close(&f);
}

struct Row { const char *what; unsigned reads, writes; };

static void begin() { s_reads = s_writes = 0; }

static Row end(const char *what) { return Row{ what, s_reads, s_writes }; }

static std::vector<Row> scenarios() {
	std::vector<Row> rows;
	kst_image_t b, before;
	memset(&b, 0x11, sizeof(b));
	memset(&before, 0x22, sizeof(before));
	KstFirstWrite w;
	static char text[KST_FILE_MAX];
	size_t n = kst_file_format(&b, text, sizeof(text));
	char name[KST_STORE_NAME_MAX];

	fullCard();
	begin();
	kstFilesFirstWrite(&b, &before, &w);
	rows.push_back(end("KST first write, no old record"));
	if (w.backup != KST_STORE_OK || w.record != KST_STORE_OK) exit(2);

	begin();
	kstFilesRecordRemove(&w);
	rows.push_back(end("KST record removed after the verify"));

	putRecord();
	begin();
	kstFilesFirstWrite(&b, &before, &w);
	rows.push_back(end("KST first write, an old record kept"));
	if (w.kept != KST_STORE_OK || w.backup != KST_STORE_OK) exit(2);
	kstFilesRecordRemove(&w);

	begin();
	if (kstFilesSaveAs(text, n, name) != KST_STORE_OK) exit(2);
	rows.push_back(end("KST SAVE AS, the 50th file of KST/"));

	begin();
	{
		KstFilesScope scope;  // as the FILES page opens it
		static kst_store_entry_t e[64];
		static BackupFileName backups[BACKUP_RING_NAMES];
		size_t count = 0, nb = 0;
		kst_store_list(kstFilesIo(), e, 64, &count, nullptr, nullptr);
		kstFilesBackups(backups, BACKUP_RING_NAMES, &nb);
		kstFilesRecordPresent();
		char base[KST_STORE_PRESET_MAX];
		kstFilesSaveName(base);
		if (count != 50 || nb == 0) exit(2);
	}
	rows.push_back(end("KST FILES page"));

	uint8_t settings[AM32_SETTINGS_SIZE];
	memset(settings, 0, sizeof(settings));
	settings[AM32_OFF_LAYOUT_REVISION] = 3;
	char bname[BACKUP_NAME_MAX];
	begin();
	if (am32BackupSave(settings, "F051", 0x7C00, bname) != BackupResult::Ok) exit(2);
	rows.push_back(end("AM32 backup"));
	return rows;
}

static std::vector<Row> hitecScenarios() {
	std::vector<Row> rows;
	hitec_image_t backup;
	d485(&backup);
	hitec_card_record_t rec = hitecRecord(30);
	HitecWrite w, w2;

	fullHitecCard();
	begin();
	hitecFilesFirstWrite(&backup, &rec, &w);
	rows.push_back(end("Hitec first write, no old record"));
	if (w.backup != HitecFiles::Ok || w.record != HitecFiles::Ok) exit(2);

	hitec_card_record_t rec2 = hitecRecord(60);
	begin();
	hitecFilesRecordWrite(&w, &rec2, &w2);
	rows.push_back(end("Hitec record of a later write"));
	if (w2.record != HitecFiles::Ok) exit(2);

	begin();
	hitecFilesRecordRemove(&w2);
	rows.push_back(end("Hitec record removed after the verify"));

	hitecFilesRecordWrite(&w2, &rec, &w);  // a record left by an interrupted write
	begin();
	hitecFilesFirstWrite(&backup, &rec2, &w2);
	rows.push_back(end("Hitec first write, an old record kept"));
	if (w2.kept != HitecFiles::Ok || w2.backup != HitecFiles::Ok) exit(2);
	hitecFilesRecordRemove(&w2);

	char file[HITEC_FILES_NAME_MAX];
	begin();
	if (hitecFilesSaveAs(&backup, file) != HitecFiles::Ok) exit(2);
	rows.push_back(end("Hitec SAVE AS, the 50th file of HITEC/"));

	begin();
	{
		CardDirScope scope(HITEC_FILES_DIR);  // as the FILES page opens it
		static HitecFileName files[64];
		static BackupFileName backups[BACKUP_RING_NAMES];
		size_t count = 0, nb = 0;
		hitecFilesList(files, 64, &count);
		hitecFilesBackups(backups, BACKUP_RING_NAMES, &nb);
		hitecFilesRecordPresent();
		hitecFilesSaveName(file);
		if (count != 50 || nb == 0) exit(2);
	}
	rows.push_back(end("Hitec FILES page"));

	hitec_image_t img;
	begin();
	if (hitecFilesLoad("HT050.TXT", HITEC_D, &img) != HitecFiles::Ok) exit(2);
	rows.push_back(end("Hitec LOAD of a file"));
	return rows;
}

/** @brief The table between the markers, every scenario of @p list on every volume. */
static std::string table(std::vector<Row> (*list)()) {
	struct Vol { const char *name; BYTE fmt; LBA_t sectors; DWORD au; } vols[] = {
		{ "FAT32, 32 GiB, 32 KiB clusters", FM_FAT32, 67108864ull, 32768 },
		{ "exFAT, 64 GiB, 128 KiB clusters", FM_EXFAT, 134217728ull, 131072 },
	};
	struct LogsAt { Logs at; const char *text; } logsAt[] = {
		{ Logs::None, "none" },
		{ Logs::InLogs, "1000 in LOGS/" },
		{ Logs::InRoot, "1000 in the root, an earlier firmware's" },
	};
	std::string out =
		"| Volume | Blackbox logs | Card phase | Block reads | Block writes | Typical card | SDHC | SDXC |\n"
		"| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |\n";
	char line[320];
	for (const Vol &v : vols) {
		for (const LogsAt &logs : logsAt) {
			volume(v.fmt, v.sectors, v.au, logs.at);
			for (const Row &r : list()) {
				double typ = r.reads * 10.0 + r.writes * 10.0;
				double hc = r.reads * 105.0 + r.writes * 261.0;
				double xc = r.reads * 105.0 + r.writes * 511.0;
				snprintf(line, sizeof(line), "| %s | %s | %s | %u | %u | %.1f s | %.1f s | %.1f s%s |\n",
				         v.name, logs.text, r.what, r.reads, r.writes, typ / 1000, hc / 1000, xc / 1000,
				         xc > CAP_MS ? ", over the cap" : "");
				out += line;
			}
			f_mount(nullptr, "", 0);
		}
	}
	return out;
}

/** @brief The note with the table between the @p tag markers replaced by @p t; the note unchanged without them. */
static std::string splice(const std::string &note, const char *tag, const std::string &t,
                          bool *found) {
	std::string start = std::string("<!-- ") + tag + ":start -->\n";
	std::string endMark = std::string("<!-- ") + tag + ":end -->";
	size_t a = note.find(start), b = note.find(endMark);
	*found = a != std::string::npos && b != std::string::npos && b > a;
	if (!*found) return note;
	a += start.size();
	return note.substr(0, a) + t + "\n" + "Generated by `make -C test kstcount-write`. CI runs `make -C test kstcount` and fails on drift.\n" + note.substr(b);
}

static std::string readFile(const char *path) {
	std::string s;
	FILE *f = fopen(path, "rb");
	if (!f) return s;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
	fclose(f);
	return s;
}

int main(int argc, char **argv) {
	const char *mode = argc > 1 ? argv[1] : "--print";
	std::string t = table(scenarios), h = table(hitecScenarios);
	if (strcmp(mode, "--print") == 0) {
		fputs(t.c_str(), stdout);
		fputs(h.c_str(), stdout);
		return 0;
	}
	if (argc < 3) {
		fprintf(stderr, "usage: kst_card_count [--print | --check NOTE | --write NOTE]\n");
		return 2;
	}
	std::string note = readFile(argv[2]);
	bool kst = false, hitec = false;
	std::string want = splice(splice(note, "kst-card-count", t, &kst), "hitec-card-count", h, &hitec);
	if (!kst && !hitec) {
		fprintf(stderr, "%s: no kst-card-count or hitec-card-count markers\n", argv[2]);
		return 2;
	}
	if (strcmp(mode, "--write") == 0) {
		FILE *f = fopen(argv[2], "wb");
		if (!f || fwrite(want.data(), 1, want.size(), f) != want.size()) return 2;
		fclose(f);
		return 0;
	}
	if (want != note) {
		fputs((kst ? t : h).c_str(), stdout);
		fprintf(stderr, "%s: the card-phase table drifts from the counts above; run make -C test kstcount-write\n", argv[2]);
		return 1;
	}
	printf("%s: the card-phase table matches FatFs\n", argv[2]);
	return 0;
}
