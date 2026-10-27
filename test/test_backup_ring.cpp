/**
 * @file test_backup_ring.cpp
 * @brief The ring of device backups in `BACKUP/` (backup_ring.h) and the
 *        card calls under it (card_dir.h), against the in-memory volume of
 *        fake_fatfs.h.
 */

#include "check.h"
#include "fakes.h"
#include "fake_fatfs.h"
#include "backup_ring.h"
#include "card_dir.h"
#include "sd_log.h"
#include "usb_msc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief A fresh volume, the logger Idle, the card reader off. */
static void card(uint32_t capacity = 1u << 22) {
	fakeFatReset(capacity);
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
}

/** @brief The marker's number on the volume, 0 when missing or unreadable. */
static unsigned marker() {
	char buf[8];
	int32_t n = fakeFatGet("BACKUP/" BACKUP_MARKER_NAME, buf, sizeof(buf));
	return n < 0 ? 0 : backupMarker(buf, (size_t)n);
}

/** @brief The first byte of BACKUP/<prefix><nnn>.TXT, or '\0' when it is missing. */
static char firstByte(BackupDevice d, unsigned n) {
	char name[BACKUP_NAME_MAX], path[32], buf[4];
	backupName(d, n, name);
	snprintf(path, sizeof(path), "BACKUP/%s", name);
	return fakeFatGet(path, buf, sizeof(buf)) > 0 ? buf[0] : '\0';
}

static void countEntry(const char *, bool, void *ctx) { ++*(unsigned *)ctx; }

/** @brief The entries of BACKUP/, files and directories. */
static unsigned entries() {
	unsigned n = 0;
	cardWalk(BACKUP_DIR, countEntry, &n);
	return n;
}

/** @brief A backup of @p d with @p tag as its one byte; the number saved, 0 on a refusal. */
static unsigned save(BackupDevice d, char tag) {
	char name[BACKUP_NAME_MAX];
	if (backupSave(d, &tag, 1, name) != BackupResult::Ok) return 0;
	return backupNumber(name, nullptr);
}

static uint64_t ring(unsigned from, unsigned to) {
	uint64_t t = 0;
	for (unsigned n = from; n <= to; n++) t |= 1ull << (n - 1u);
	return t;
}

static void testRules() {
	section("Backups: the ring's rules");
	checkInt("after none: 1", backupAfter(0), 1);
	checkInt("after 49: 50", backupAfter(49), 50);
	checkInt("after 50: 1", backupAfter(50), 1);
	checkInt("no marker, no file: none", backupGuess(0), 0);
	checkInt("no marker, 1 to 10: 10", backupGuess(ring(1, 10)), 10);
	checkInt("a gap below the highest does not count", backupGuess(ring(1, 10) & ~ring(3, 3)), 10);
	checkInt("2 to 50: 50", backupGuess(ring(2, 50)), 50);
	checkInt("3 to 50 and 1: 1", backupGuess(ring(3, 50) | ring(1, 1)), 1);
	checkInt("all 50: 50", backupGuess(ring(1, 50)), 50);
	checkInt("a marker of 30", backupMarker("030\n", 4), 30);
	checkInt("50 is the last", backupMarker("050\n", 4), 50);
	checkInt("0 is no number", backupMarker("000\n", 4), 0);
	checkInt("51 is outside the ring", backupMarker("051\n", 4), 0);
	checkInt("2 digits: no number", backupMarker("30\n", 3), 0);
	checkInt("no line feed: no number", backupMarker("0300", 4), 0);
	checkInt("a letter: no number", backupMarker("03a\n", 4), 0);
	BackupDevice d = BackupDevice::Count;
	checkTrue("KST017.TXT: 17, KST", backupNumber("KST017.TXT", &d) == 17 && d == BackupDevice::Kst);
	checkTrue("am018.txt: 18, AM32, in any case",
	          backupNumber("am018.txt", &d) == 18 && d == BackupDevice::Am32);
	checkInt("HT050.TXT: 50", backupNumber("HT050.TXT", nullptr), 50);
	checkInt("KST051.TXT is outside the ring", backupNumber("KST051.TXT", nullptr), 0);
	checkInt("KST000.TXT is outside the ring", backupNumber("KST000.TXT", nullptr), 0);
	checkInt("XY017.TXT: no device", backupNumber("XY017.TXT", nullptr), 0);
	checkInt("KST17.TXT: 2 digits", backupNumber("KST17.TXT", nullptr), 0);
	checkInt("KST017.BIN: another extension", backupNumber("KST017.BIN", nullptr), 0);
	char name[BACKUP_NAME_MAX];
	backupName(BackupDevice::Am32, 7, name);
	checkTrue("the name of AM32's 7: AM007.TXT", strcmp(name, "AM007.TXT") == 0);
	BackupNames k;
	memset(&k, 0, sizeof(k));
	k.latest = 4;
	k.dirs = k.taken = ring(5, 6);
	checkInt("directories are passed over", backupNext(&k), 7);
	k.latest = 50;
	k.dirs = k.taken = ring(1, 50);
	checkInt("all directories: the next number all the same", backupNext(&k), 1);
}

static void testRing() {
	section("Backups: one ring of 50 for every device");
	card();
	checkInt("an empty card: KST001 first", save(BackupDevice::Kst, 'a'), 1);
	checkInt("the marker names it", marker(), 1);
	checkInt("an ESC's backup takes the next number", save(BackupDevice::Am32, 'e'), 2);
	checkTrue("as AM002.TXT", firstByte(BackupDevice::Am32, 2) == 'e');
	unsigned last = 0;
	for (int i = 0; i < 48; i++) last = save(BackupDevice::Kst, 'a');
	checkInt("50 backups: the last is 50", last, 50);
	checkInt("50 backups and the marker: 51 entries", entries(), 51);
	checkInt("the 51st takes 1", save(BackupDevice::Kst, 'b'), 1);
	checkTrue("in place of the oldest", firstByte(BackupDevice::Kst, 1) == 'b');
	// Number 2 is the ESC's: the servo's backup takes its place.
	checkInt("the next takes 2", save(BackupDevice::Kst, 'c'), 2);
	checkTrue("the ESC's file is renamed and written over",
	          firstByte(BackupDevice::Kst, 2) == 'c' && firstByte(BackupDevice::Am32, 2) == '\0');
	checkInt("still 51 entries", entries(), 51);

	// The list of one device, latest first.
	BackupNames k;
	backupNames(&k);
	char names[BACKUP_RING_NAMES][BACKUP_NAME_MAX];
	size_t n = backupList(&k, BackupDevice::Kst, names, BACKUP_RING_NAMES);
	checkTrue("KST's list from the latest: KST002, KST001, KST050",
	          n == 50 && strcmp(names[0], "KST002.TXT") == 0 &&
	          strcmp(names[1], "KST001.TXT") == 0 && strcmp(names[2], "KST050.TXT") == 0 &&
	          strcmp(names[49], "KST003.TXT") == 0);
	checkInt("AM32's list: empty", backupList(&k, BackupDevice::Am32, names, BACKUP_RING_NAMES), 0);

	// Two files under one number, from a computer: one is written over, the
	// other removed.
	fakeFatPut("BACKUP/AM003.TXT", "z", 1);
	checkInt("two files under 3: the next is 3", save(BackupDevice::Am32, 'd'), 3);
	checkTrue("AM003 written over, KST003 removed",
	          firstByte(BackupDevice::Am32, 3) == 'd' && firstByte(BackupDevice::Kst, 3) == '\0');

	// A file removed on a computer leaves a gap the marker passes over.
	f_unlink("BACKUP/KST010.TXT");
	checkInt("a gap at 10: the next is 4 all the same", save(BackupDevice::Kst, 'c'), 4);
	checkTrue("10 stays free", firstByte(BackupDevice::Kst, 10) == '\0');

	// Without a marker the names give the latest.
	f_unlink("BACKUP/" BACKUP_MARKER_NAME);
	checkInt("no marker: the number before the lowest free", save(BackupDevice::Kst, 'd'), 10);
	fakeFatPut("BACKUP/" BACKUP_MARKER_NAME, "x", 1);
	checkInt("a marker that does not read, all 50 taken: 1", save(BackupDevice::Kst, 'd'), 1);
	checkInt("and the marker is written again", marker(), 1);

	// A directory under a ring name: passed over and kept.
	f_unlink("BACKUP/KST002.TXT");
	fakeFatMkdir("BACKUP/KST002.TXT");
	checkInt("a directory at 2: 3 next", save(BackupDevice::Kst, 'e'), 3);
	checkTrue("the directory stays", fakeFatExists("BACKUP/KST002.TXT"));

	// Names outside the ring stay through a whole turn of it.
	fakeFatPut("BACKUP/KST051.TXT", "z", 1);
	fakeFatPut("BACKUP/Wing.TXT", "z", 1);
	for (int i = 0; i < 60; i++) save(BackupDevice::Kst, 'f');
	checkTrue("KST051 and Wing stay", fakeFatExists("BACKUP/KST051.TXT") &&
	                                  fakeFatExists("BACKUP/Wing.TXT"));
	checkTrue("49 backups, the directory, KST051, Wing and the marker: 53 entries",
	          entries() == 53);

	// A marker the card refuses: nothing written over.
	unsigned next = backupAfter(marker());
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	char name[BACKUP_NAME_MAX];
	checkTrue("a marker that does not write: a write error",
	          backupSave(BackupDevice::Kst, "g", 1, name) == BackupResult::WriteError && !name[0]);
	checkTrue("and the oldest backup stays", firstByte(BackupDevice::Kst, next) == 'f');
	checkInt("the marker names the old latest", backupAfter(marker()), next);
	checkInt("no file left open", fakeFatOpenFiles(), 0);
	checkTrue("and the current directory is the root again", fakeFatCwd()[0] == '\0');

	// The oldest is written over in place; a read-only one is refused.
	char path[32];
	snprintf(path, sizeof(path), "BACKUP/KST%03u.TXT", next);
	fakeFatReadOnly(path);
	checkTrue("a read-only oldest file: refused",
	          backupSave(BackupDevice::Kst, "h", 1, name) == BackupResult::Denied);
	checkTrue("it keeps its bytes", firstByte(BackupDevice::Kst, next) == 'f');
	checkInt("the marker names it all the same", marker(), next);
	checkInt("the next save takes the number after it", save(BackupDevice::Kst, 'i'), backupAfter(next));

	// A change into BACKUP that fails: the paths name /BACKUP/ from the root.
	fakeFatFail(FakeFatCall::ChDir, FR_DISK_ERR);
	unsigned got = save(BackupDevice::Kst, 'j');
	checkTrue("a change into BACKUP that fails: saved all the same",
	          got != 0 && firstByte(BackupDevice::Kst, got) == 'j' && fakeFatCwd()[0] == '\0');

	// A backup reads back; another name does not.
	size_t len = 0;
	char buf[8];
	char saved[BACKUP_NAME_MAX];
	backupName(BackupDevice::Kst, got, saved);
	checkTrue("a backup loads", backupLoad(saved, buf, sizeof(buf), &len) == BackupResult::Ok &&
	                            len == 1 && buf[0] == 'j');
	checkTrue("a missing one: not found",
	          backupLoad("AM049.TXT", buf, sizeof(buf), &len) == BackupResult::NotFound);
}

static void testNoCard() {
	section("Backups: no card, and a card held by USB");
	card();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	unsigned before = 0;
	for (unsigned c = 0; c < (unsigned)FakeFatCall::Count; c++) before += fakeFatCalls((FakeFatCall)c);
	char name[BACKUP_NAME_MAX];
	checkTrue("no card: refused", backupSave(BackupDevice::Am32, "x", 1, name) == BackupResult::NoCard);
	card();
	fakeMscSet(MscState::Serving, 0);
	checkTrue("a card held by USB: refused",
	          backupSave(BackupDevice::Am32, "x", 1, name) == BackupResult::NoCard);
	unsigned after = 0;
	for (unsigned c = 0; c < (unsigned)FakeFatCall::Count; c++) after += fakeFatCalls((FakeFatCall)c);
	checkInt("and no FatFs call", (long)after, (long)before);
	card();
}

void runBackupTests() {
	testRules();
	testRing();
	testNoCard();
	card();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
}
