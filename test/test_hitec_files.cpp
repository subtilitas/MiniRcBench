/**
 * @file test_hitec_files.cpp
 * @brief The Hitec files over FatFs, against the in-memory volume of
 *        fake_fatfs.h.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * hitec_files.cpp and rcbench's hitec_card.c run as they ship. The card's
 * mount state comes from the fake logger and the fake card reader in
 * fakes.cpp. The servo images are rcbench's bench reads (hitec_reg_fix.h).
 */

#include "check.h"
#include "fakes.h"
#include "fake_fatfs.h"
#include "hitec_files.h"
#include "sd_log.h"
#include "usb_msc.h"

// rcbench's fixture brings its test harness, whose reporting is not used here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "rcbench/hitec_reg_fix.h"
#pragma GCC diagnostic pop

#include <stdio.h>
#include <string.h>

/** @brief A fresh volume, the logger Idle, the card reader off. */
static void card() {
	fakeFatReset(1u << 20);
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
}

static unsigned fatCalls() {
	unsigned n = 0;
	for (unsigned c = 0; c < (unsigned)FakeFatCall::Count; c++)
		n += fakeFatCalls((FakeFatCall)c);
	return n;
}

static hitec_image_t dImage() {
	hitec_image_t img;
	d485(&img);
	return img;
}

static hitec_image_t cImage() {
	hitec_image_t img;
	hs5645(&img);
	return img;
}

/** @brief A record of a D series write of SPEED. */
static hitec_card_record_t dRecord(int speed = 30) {
	hitec_card_record_t r;
	memset(&r, 0, sizeof(r));
	r.series = HITEC_D;
	r.port = 0;
	r.pin = 29;
	r.before = dImage();
	r.target = r.before;
	hitec_param_set(&r.target, HITEC_LAYOUT_D, HITEC_P_D_SPEED, speed);
	return r;
}

/** @brief Whether @p a and @p b hold the same bytes at every address of a servo file. */
static bool sameFileAddrs(const hitec_image_t *a, const hitec_image_t *b) {
	uint8_t addrs[HITEC_CARD_FILE_ADDRS];
	unsigned n = hitec_card_file_addrs(a->series, addrs);
	for (unsigned i = 0; i < n; i++) {
		uint8_t x = addrs[i];
		if (a->mem[x] != b->mem[x]) return false;
		if (a->series == HITEC_D && a->mem[x + 1u] != b->mem[x + 1u]) return false;
	}
	return n > 0;
}

/** @brief The text of the file at @p path into @p buf; its length, -1 for none. */
static int32_t fileText(const char *path, char *buf, size_t cap) { return fakeFatGet(path, buf, cap); }

static void testNoCard() {
	section("Hitec files: no card");
	card();
	fakeMscSet(MscState::Serving, 0);
	unsigned calls = fatCalls();
	HitecFileName names[4];
	size_t count = 9;
	char name[HITEC_FILES_NAME_MAX];
	hitec_image_t img = dImage();
	HitecWrite w;
	hitecFilesFirstWrite(&img, nullptr, &w);
	checkTrue("a card held by USB: every call answers NoCard",
	          hitecFilesList(names, 4, &count) == HitecFiles::NoCard && count == 0 &&
	          hitecFilesSaveAs(&img, name) == HitecFiles::NoCard && name[0] == '\0' &&
	          hitecFilesLoad("HT001.TXT", HITEC_D, &img) == HitecFiles::NoCard &&
	          !hitecFilesRecordPresent() && w.backup == HitecFiles::NoCard &&
	          w.record == HitecFiles::NoCard && hitecFilesWriteAllowed(&w));
	checkTrue("and makes no FatFs call", fatCalls() == calls);
}

static void testSaveAs() {
	section("Hitec files: SAVE AS, the list and LOAD");
	card();
	hitec_image_t d = dImage();
	char name[HITEC_FILES_NAME_MAX];
	checkTrue("SAVE AS on an empty card: HT001.TXT in HITEC/",
	          hitecFilesSaveAs(&d, name) == HitecFiles::Ok && strcmp(name, "HT001.TXT") == 0 &&
	          fakeFatExists("HITEC/HT001.TXT"));
	char text[HITEC_CARD_FILE_MAX];
	int32_t n = fileText("HITEC/HT001.TXT", text, sizeof(text));
	checkTrue("the file is rcbench's servo file text",
	          n > 0 && strncmp(text, "RCBENCH HITEC 1\n", 16) == 0);
	hitec_image_t back;
	checkTrue("LOAD reads the image back",
	          hitecFilesLoad("HT001.TXT", HITEC_D, &back) == HitecFiles::Ok && sameFileAddrs(&d, &back));
	checkTrue("a D series file refused on a classic claim",
	          hitecFilesLoad("HT001.TXT", HITEC_CLASSIC, &back) == HitecFiles::OtherSeries);

	hitec_image_t c = cImage();
	checkTrue("the next name is HT002", hitecFilesSaveAs(&c, name) == HitecFiles::Ok &&
	          strcmp(name, "HT002.TXT") == 0);
	checkTrue("a classic file loads with its version",
	          hitecFilesLoad("HT002.TXT", HITEC_CLASSIC, &back) == HitecFiles::Ok &&
	          sameFileAddrs(&c, &back) && back.version == c.version);
	fakeFatPut("HITEC/HT010.TXT", text, (size_t)n);
	fakeFatPut("HITEC/NOTES.TXT", "x", 1);
	HitecFileName names[8];
	size_t count = 0;
	checkTrue("the list: the HTnnn files by name, nothing else",
	          hitecFilesList(names, 8, &count) == HitecFiles::Ok && count == 3 &&
	          strcmp(names[0], "HT001.TXT") == 0 && strcmp(names[1], "HT002.TXT") == 0 &&
	          strcmp(names[2], "HT010.TXT") == 0);
	checkTrue("SAVE AS takes one above the highest: HT011",
	          hitecFilesSaveName(name) == HitecFiles::Ok && strcmp(name, "HT011.TXT") == 0);

	// A text edited by hand does not load.
	char edited[HITEC_CARD_FILE_MAX];
	memcpy(edited, text, (size_t)n);
	edited[n - 3] = edited[n - 3] == '0' ? '1' : '0';
	fakeFatPut("HITEC/HT020.TXT", edited, (size_t)n);
	checkTrue("an edited file: BadText", hitecFilesLoad("HT020.TXT", HITEC_D, &back) == HitecFiles::BadText);
	checkTrue("no such file: NotFound", hitecFilesLoad("HT030.TXT", HITEC_D, &back) == HitecFiles::NotFound);
	static char big[HITEC_CARD_FILE_MAX + 10];
	memset(big, 'x', sizeof(big));
	fakeFatPut("HITEC/HT040.TXT", big, sizeof(big));
	checkTrue("a file longer than any servo file: TooLarge",
	          hitecFilesLoad("HT040.TXT", HITEC_D, &back) == HitecFiles::TooLarge);

	// 50 files: SAVE AS refuses.
	card();
	fakeFatMkdir("HITEC");
	for (int i = 1; i <= HITEC_FILES_SAVE_MAX; i++) {
		char path[32];
		snprintf(path, sizeof(path), "HITEC/HT%03d.TXT", i);
		fakeFatPut(path, text, (size_t)n);
	}
	checkTrue("50 files: SAVE AS refused, nothing written",
	          hitecFilesSaveAs(&d, name) == HitecFiles::NoNumber && name[0] == '\0' &&
	          !fakeFatExists("HITEC/HT051.TXT"));
	card();
	fakeFatMkdir("HITEC");
	fakeFatPut("HITEC/HT999.TXT", text, (size_t)n);
	checkTrue("HT999 taken: SAVE AS refused", hitecFilesSaveName(name) == HitecFiles::NoNumber);
}

static void testFirstWrite() {
	section("Hitec files: the first write and later writes");
	card();
	hitec_image_t d = dImage();
	hitec_card_record_t rec = dRecord();
	char next[BACKUP_NAME_MAX];
	checkTrue("the write page's backup name: HT001",
	          hitecFilesNextBackup(next) == HitecFiles::Ok && strcmp(next, "HT001.TXT") == 0);
	HitecWrite w;
	hitecFilesFirstWrite(&d, &rec, &w);
	checkTrue("no record before: the backup HT001 in BACKUP/, the record in HITEC/",
	          w.backup == HitecFiles::Ok && strcmp(w.backupName, "HT001.TXT") == 0 &&
	          w.kept == HitecFiles::Ok && w.keptName[0] == '\0' && w.record == HitecFiles::Ok &&
	          fakeFatExists("BACKUP/HT001.TXT") && hitecFilesRecordPresent() &&
	          hitecFilesWriteAllowed(&w));
	hitec_card_record_t back;
	checkTrue("the record parses to the write's images",
	          hitecFilesRecordLoad(&back) == HitecFiles::Ok && back.series == HITEC_D &&
	          back.pin == 29 && sameFileAddrs(&back.before, &rec.before));
	hitec_image_t img;
	checkTrue("the backup loads", hitecFilesBackupLoad("HT001.TXT", HITEC_D, &img) == HitecFiles::Ok &&
	          sameFileAddrs(&img, &d));

	// A later write: its record over the session's own.
	hitec_card_record_t rec2 = dRecord(60);
	HitecWrite w2;
	hitecFilesRecordWrite(&w, &rec2, &w2);
	checkTrue("a later write: the record written over, no new backup",
	          w2.record == HitecFiles::Ok && w2.backupName[0] == '\0' && w2.recordSum != w.recordSum &&
	          !fakeFatExists("BACKUP/HT002.TXT"));
	checkTrue("the verified write removes it",
	          hitecFilesRecordRemove(&w2) == HitecFiles::Ok && !hitecFilesRecordPresent());
	HitecWrite w3;
	hitecFilesRecordWrite(&w2, &rec2, &w3);
	checkTrue("a later write with no record: a new one", w3.record == HitecFiles::Ok &&
	          hitecFilesRecordPresent());

	// The record of another session stays.
	card();
	fakeFatMkdir("HITEC");
	char text[HITEC_CARD_TEXT_MAX];
	hitec_card_record_t old = dRecord(80);
	size_t n = hitec_card_text(&old, text, sizeof(text));
	fakeFatPut("HITEC/HITEC.TXT", text, n);
	checkTrue("a record on the card: the backup name moves past it, HT002",
	          hitecFilesNextBackup(next) == HitecFiles::Ok && strcmp(next, "HT002.TXT") == 0);
	hitecFilesFirstWrite(&d, &rec, &w);
	checkTrue("an old record is moved into the ring as HT001, the backup HT002",
	          w.kept == HitecFiles::Ok && strcmp(w.keptName, "HT001.TXT") == 0 &&
	          strcmp(w.backupName, "HT002.TXT") == 0 && w.record == HitecFiles::Ok);
	checkTrue("the moved record loads as its image before the write",
	          hitecFilesBackupLoad("HT001.TXT", HITEC_D, &img) == HitecFiles::Ok &&
	          sameFileAddrs(&img, &old.before));
	BackupFileName names[4];
	size_t count = 0;
	checkTrue("the backups list, latest first",
	          hitecFilesBackups(names, 4, &count) == HitecFiles::Ok && count == 2 &&
	          strcmp(names[0], "HT002.TXT") == 0 && strcmp(names[1], "HT001.TXT") == 0);

	// A later write meets a record it did not write.
	card();
	fakeFatMkdir("HITEC");
	fakeFatPut("HITEC/HITEC.TXT", text, n);
	hitecFilesRecordWrite(&w, &rec2, &w2);
	checkTrue("a later write over another record: refused, the record kept",
	          w2.record == HitecFiles::Exists && fakeFatExists("HITEC/HITEC.TXT") &&
	          hitecFilesRecordRemove(&w2) == HitecFiles::Ok && hitecFilesRecordPresent());
	HitecWrite fake = w;
	fake.recordSum ^= 1u;
	checkTrue("a record with other bytes is not removed",
	          hitecFilesRecordRemove(&fake) == HitecFiles::Ok && hitecFilesRecordPresent());

	// A record that cannot be moved: no backup claim lost, no new record.
	card();
	fakeFatMkdir("HITEC");
	fakeFatPut("HITEC/HITEC.TXT", text, n);
	fakeFatFail(FakeFatCall::Rename, FR_DENIED);
	hitecFilesFirstWrite(&d, &rec, &w);
	checkTrue("an old record that does not move: left, no new record written",
	          w.kept != HitecFiles::Ok && w.record == w.kept && hitecFilesWriteAllowed(&w) &&
	          w.backup == HitecFiles::Ok);
	char now[HITEC_CARD_TEXT_MAX];
	int32_t got = fileText("HITEC/HITEC.TXT", now, sizeof(now));
	checkTrue("the old record holds its bytes", got == (int32_t)n && memcmp(now, text, n) == 0);

	// A card that takes neither file stops the write.
	HitecWrite none;
	memset(&none, 0, sizeof(none));
	none.backup = HitecFiles::WriteError;
	none.record = HitecFiles::WriteError;
	checkTrue("a card that took neither: the write stops", !hitecFilesWriteAllowed(&none));
}

void runHitecFilesTests() {
	testNoCard();
	testSaveAs();
	testFirstWrite();
}
