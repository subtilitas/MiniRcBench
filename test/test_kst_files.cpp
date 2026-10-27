/**
 * @file test_kst_files.cpp
 * @brief The KST files over FatFs, against the in-memory volume of
 *        fake_fatfs.h.
 *
 * kst_files.cpp and rcbench's kst_store.c and kst_file.c run as they ship.
 * The card's mount state comes from the fake logger and the fake card
 * reader in fakes.cpp. After every call the volume holds no open file, except
 * after a close that failed, which FatFs keeps open. The fake's errors are
 * one-time: a write error that persists, and FatFs's window that keeps the
 * sector it could not write, are not modelled (kst_files.h).
 */

#include "check.h"
#include "fakes.h"
#include "fake_fatfs.h"
#include "kst_files.h"
#include "kst_file.h"
#include "rcbench/kst_sim.h"
#include "sd_log.h"
#include "usb_msc.h"

#include <stdlib.h>
#include <string.h>

/** @brief A fresh volume, the logger Idle, the card reader off. */
static void card(uint32_t capacity = 1u << 20) {
	fakeFatReset(capacity);
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
}

static void logger(SdLogState s) {
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = s;
	fakeSdLogSet(&st);
}

/** @brief The image a file on the volume holds; false when it does not parse. */
static bool imageOf(const char *path, kst_image_t *out) {
	static char buf[KST_FILE_MAX];
	int32_t n = fakeFatGet(path, buf, sizeof(buf));
	if (n < 0) return false;
	kst_file_t f;
	if (kst_file_parse(buf, (size_t)n, &f, nullptr) != KST_FILE_OK) return false;
	*out = f.image;
	return true;
}

static kst_image_t bench() { return sim_bench_image(); }

static kst_image_t changed() {
	kst_image_t img = sim_bench_image();
	img.r[3] = 0x13;
	return img;
}

/** @brief A first write that wrote the record @p text, for kstFilesRecordRemove(). */
static KstFirstWrite wroteRecord(const char *text = "x") {
	KstFirstWrite w;
	memset(&w, 0, sizeof(w));
	w.record = KST_STORE_OK;
	// FNV-1a, as kst_files.cpp hashes the record it writes.
	uint32_t h = 2166136261u;
	for (const char *c = text; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
	w.recordSum = h;
	return w;
}

static unsigned fatCalls() {
	unsigned n = 0;
	for (unsigned c = 0; c < (unsigned)FakeFatCall::Count; c++)
		n += fakeFatCalls((FakeFatCall)c);
	return n;
}

static void testMounted() {
	section("KST files: when the card counts as mounted");
	card();
	checkTrue("logger Idle, reader off: mounted", kstFilesMounted() &&
	                                             kstFilesIo()->mounted(nullptr));
	logger(SdLogState::Logging);
	checkTrue("a blackbox log running: mounted", kstFilesMounted());
	logger(SdLogState::NoCard);
	checkTrue("no card: not mounted", !kstFilesMounted() && !kstFilesIo()->mounted(nullptr));
	logger(SdLogState::Error);
	checkTrue("the logger in error: not mounted", !kstFilesMounted());
	logger(SdLogState::Idle);
	static const MscState HELD[] = { MscState::Handover, MscState::Serving, MscState::Reclaim };
	static const char *const NAMES[] = { "handing over", "serving a USB host", "taking it back" };
	for (int i = 0; i < 3; i++) {
		fakeMscSet(HELD[i], 0);
		char what[64];
		snprintf(what, sizeof(what), "the card reader %s: not mounted", NAMES[i]);
		checkTrue(what, !kstFilesMounted());
	}
	fakeMscSet(MscState::Idle, 0);
}

static void testNoCard() {
	section("KST files: no card, and a card held by USB");
	for (int held = 0; held < 2; held++) {
		card();
		if (held) fakeMscSet(MscState::Serving, 0);
		else      logger(SdLogState::NoCard);
		const char *how = held ? "held by USB" : "no card";
		kst_image_t b = bench();
		KstFirstWrite w;
		kstFilesFirstWrite(&b, &b, &w);
		char what[96];
		snprintf(what, sizeof(what), "%s: the first write saves nothing", how);
		checkTrue(what, w.backup == KST_STORE_NO_CARD && w.record == KST_STORE_NO_CARD &&
		                w.backupName[0] == '\0');
		snprintf(what, sizeof(what), "%s: the write runs, the backup in memory", how);
		checkTrue(what, kstFilesWriteAllowed(&w));
		snprintf(what, sizeof(what), "%s: no record found, none removed", how);
		KstFirstWrite wrote = wroteRecord();
		checkTrue(what, !kstFilesRecordPresent() && kstFilesRecordRemove(&wrote) == KST_STORE_NO_CARD);
		char buf[16];
		size_t len = 0;
		snprintf(what, sizeof(what), "%s: the record does not load", how);
		checkTrue(what, kstFilesRecordLoad(buf, sizeof(buf), &len) == KST_STORE_NO_CARD);
		snprintf(what, sizeof(what), "%s: the store sees no card", how);
		size_t count = 0;
		kst_store_entry_t e[2];
		checkTrue(what, kst_store_list(kstFilesIo(), e, 2, &count, nullptr, nullptr) ==
		                KST_STORE_NO_CARD);
		snprintf(what, sizeof(what), "%s: FatFs is not called", how);
		checkInt(what, fatCalls(), 0);
	}
}

static void testFirstWrite() {
	section("KST files: the first write of a session");
	card();
	kst_image_t b = bench(), before = changed();
	KstFirstWrite w;
	kstFilesFirstWrite(&b, &before, &w);
	checkTrue("the backup is saved as KST001.TXT",
	          w.backup == KST_STORE_OK && strcmp(w.backupName, "KST001.TXT") == 0);
	checkTrue("the record is written", w.record == KST_STORE_OK);
	checkTrue("no old record to keep", w.kept == KST_STORE_OK && w.keptName[0] == '\0');
	checkTrue("the KST directory was made", fakeFatExists("KST"));
	kst_image_t got;
	checkTrue("KST001.TXT holds the backup",
	          imageOf("KST/KST001.TXT", &got) && memcmp(got.r, b.r, KST_REG_COUNT) == 0);
	checkTrue("KST/KST.TXT holds the image before the write",
	          imageOf("KST/KST.TXT", &got) && memcmp(got.r, before.r, KST_REG_COUNT) == 0);
	checkTrue("the write runs", kstFilesWriteAllowed(&w));
	checkInt("no file left open", fakeFatOpenFiles(), 0);

	checkTrue("found at the next entry", kstFilesRecordPresent());
	size_t count = 0, total = 0;
	kst_store_entry_t e[4];
	kst_store_list(kstFilesIo(), e, 4, &count, &total, nullptr);
	checkTrue("the list leaves the record out", count == 1 && total == 1 &&
	                                           strcmp(e[0].name, "KST001.TXT") == 0);

	checkInt("no directory left open", fakeFatOpenDirs(), 0);
	// A card swapped in without an unmount: the record holds other bytes.
	static char mine[KST_FILE_MAX], other[KST_FILE_MAX];
	int32_t mineLen = fakeFatGet("KST/KST.TXT", mine, sizeof(mine));
	size_t otherLen = kst_file_format(&b, other, sizeof(other));
	fakeFatPut("KST/KST.TXT", other, otherLen);
	checkInt("another card's record: nothing to do", kstFilesRecordRemove(&w), KST_STORE_OK);
	checkTrue("it stays", fakeFatExists("KST/KST.TXT"));
	fakeFatPut("KST/KST.TXT", mine, (size_t)mineLen - 1);
	checkInt("one byte short: nothing to do", kstFilesRecordRemove(&w), KST_STORE_OK);
	checkTrue("it stays too", fakeFatExists("KST/KST.TXT"));
	fakeFatPut("KST/KST.TXT", mine, (size_t)mineLen);
	checkInt("a verified write removes it", kstFilesRecordRemove(&w), KST_STORE_OK);
	checkTrue("gone", !fakeFatExists("KST/KST.TXT") && !kstFilesRecordPresent());
	checkInt("removing it again: nothing to do", kstFilesRecordRemove(&w), KST_STORE_OK);
	checkInt("nor without the session's result", kstFilesRecordRemove(nullptr), KST_STORE_OK);

	kstFilesFirstWrite(&b, &before, &w);
	checkTrue("the next session's backup is KST002.TXT",
	          w.backup == KST_STORE_OK && strcmp(w.backupName, "KST002.TXT") == 0);

	// A record an interrupted write left: kept under the next number first.
	char old[KST_FILE_MAX];
	size_t oldLen = (size_t)fakeFatGet("KST/KST.TXT", old, sizeof(old));
	kstFilesFirstWrite(&b, &b, &w);
	checkTrue("an old record is kept as KST003.TXT",
	          w.kept == KST_STORE_OK && strcmp(w.keptName, "KST003.TXT") == 0);
	char moved[KST_FILE_MAX];
	checkTrue("byte for byte", fakeFatGet("KST/KST003.TXT", moved, sizeof(moved)) ==
	                           (int32_t)oldLen && memcmp(moved, old, oldLen) == 0);
	checkTrue("the backup takes the number after it",
	          w.backup == KST_STORE_OK && strcmp(w.backupName, "KST004.TXT") == 0);
	checkTrue("and the new record is the new image",
	          w.record == KST_STORE_OK && imageOf("KST/KST.TXT", &got) &&
	          memcmp(got.r, b.r, KST_REG_COUNT) == 0);
	checkInt("no file left open", fakeFatOpenFiles(), 0);

	// The old record cannot be renamed: it stays, and no record replaces it.
	fakeFatFail(FakeFatCall::Rename, FR_DISK_ERR);
	kstFilesFirstWrite(&before, &before, &w);
	checkTrue("a rename that fails: the old record stays",
	          w.kept == KST_STORE_WRITE_ERROR && w.keptName[0] == '\0' &&
	          imageOf("KST/KST.TXT", &got) && memcmp(got.r, b.r, KST_REG_COUNT) == 0);
	checkTrue("no new record over it", w.record == KST_STORE_WRITE_ERROR);
	checkTrue("the backup is still saved, so the write runs",
	          w.backup == KST_STORE_OK && kstFilesWriteAllowed(&w));
	checkInt("the verified write removes nothing", kstFilesRecordRemove(&w), KST_STORE_OK);
	checkTrue("the old record is still there",
	          imageOf("KST/KST.TXT", &got) && memcmp(got.r, b.r, KST_REG_COUNT) == 0);

	// The card does not say whether a record is there: none is written.
	fakeFatFail(FakeFatCall::Stat, FR_DISK_ERR);
	kstFilesFirstWrite(&before, &before, &w);
	checkTrue("a lookup that fails: no record written, none removed",
	          w.kept == KST_STORE_READ_ERROR && w.record == KST_STORE_READ_ERROR &&
	          kstFilesRecordRemove(&w) == KST_STORE_OK && imageOf("KST/KST.TXT", &got) &&
	          memcmp(got.r, b.r, KST_REG_COUNT) == 0);
	checkTrue("the backup is saved", w.backup == KST_STORE_OK);

	fakeFatFail(FakeFatCall::Rename, FR_DENIED);
	kstFilesFirstWrite(&before, &before, &w);
	checkTrue("no room for the rename: card full, the old record stays",
	          w.kept == KST_STORE_CARD_FULL && w.record == KST_STORE_CARD_FULL &&
	          fakeFatExists("KST/KST.TXT"));
	fakeFatWriteProtect(true);
	kstFilesFirstWrite(&before, &before, &w);
	checkTrue("a write-protected volume refuses the rename",
	          w.kept == KST_STORE_DENIED && w.backup == KST_STORE_DENIED);
	fakeFatWriteProtect(false);
	fakeFatDirLimit(fakeFatEntries() - 1);
	kstFilesFirstWrite(&before, &before, &w);
	checkTrue("a full directory refuses the rename too: card full, the record stays",
	          w.kept == KST_STORE_CARD_FULL && fakeFatExists("KST/KST.TXT"));
	fakeFatDirLimit(0xFFFFFFFFu);

	// A rename that took effect though FatFs reports it failed.
	fakeFatRenameFailsLate(FR_DISK_ERR);
	kstFilesFirstWrite(&b, &before, &w);
	char kept[KST_STORE_NAME_MAX + 4];
	snprintf(kept, sizeof(kept), "KST/%s", w.keptName);
	checkTrue("a rename reported failed that took effect: kept, under its new name",
	          w.kept == KST_STORE_OK && w.keptName[0] != '\0' && fakeFatExists(kept));
	checkTrue("and the new record is written",
	          w.record == KST_STORE_OK && imageOf("KST/KST.TXT", &got) &&
	          memcmp(got.r, before.r, KST_REG_COUNT) == 0);

	// A rename that stopped with both names: not kept, no record over it.
	kst_image_t third = changed();
	third.r[4] ^= 0x01;
	fakeFatRenameFailsHalf(FR_DISK_ERR);
	kstFilesFirstWrite(&third, &third, &w);
	checkTrue("a rename that left both names: a write error, no new record",
	          w.kept == KST_STORE_WRITE_ERROR && w.keptName[0] == '\0' &&
	          w.record == KST_STORE_WRITE_ERROR && imageOf("KST/KST.TXT", &got) &&
	          memcmp(got.r, before.r, KST_REG_COUNT) == 0);
	checkInt("and the verified write removes nothing", kstFilesRecordRemove(&w), KST_STORE_OK);

	// The lookups after a late rename that fail: no claim the record moved.
	for (unsigned skip = 1; skip <= 2; skip++) {
		// The late rename moved the record away: put one back each time.
		char text[KST_FILE_MAX];
		fakeFatPut("KST/KST.TXT", text, kst_file_format(&third, text, sizeof(text)));
		fakeFatRenameFailsLate(FR_DISK_ERR);
		fakeFatFail(FakeFatCall::Stat, FR_DISK_ERR, skip);
		kstFilesFirstWrite(&third, &third, &w);
		char msg[96];
		snprintf(msg, sizeof(msg), "a late rename, lookup %u after it fails: a write error", skip);
		checkTrue(msg, w.kept == KST_STORE_WRITE_ERROR && w.keptName[0] == '\0' &&
		               w.record == KST_STORE_WRITE_ERROR);
	}
}

/** @brief A first write on a card that refuses; nothing may be left behind. */
static void refused(const char *what, kst_store_t want) {
	kst_image_t b = bench();
	KstFirstWrite w;
	kstFilesFirstWrite(&b, &b, &w);
	char msg[96];
	snprintf(msg, sizeof(msg), "%s: the backup refused", what);
	checkInt(msg, w.backup, want);
	snprintf(msg, sizeof(msg), "%s: the record refused", what);
	checkInt(msg, w.record, want);
	snprintf(msg, sizeof(msg), "%s: no file left behind", what);
	checkTrue(msg, !fakeFatExists("KST/KST001.TXT") && !fakeFatExists("KST/KST.TXT") &&
	               fakeFatOpenFiles() == 0);
	snprintf(msg, sizeof(msg), "%s: the card took neither, the write stops", what);
	checkTrue(msg, !kstFilesWriteAllowed(&w));
}

static void testRefusals() {
	section("KST files: a card that takes no file");
	card(100);
	refused("full", KST_STORE_CARD_FULL);
	card();
	// The disk layer of this board never reports STA_PROTECT: a card that
	// refuses writes fails as a disk error.
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	fakeFatFail(FakeFatCall::MkDir, FR_DISK_ERR, 1);
	refused("this board's write-protected card", KST_STORE_WRITE_ERROR);
	card();
	fakeFatWriteProtect(true);
	refused("a volume FatFs reports write-protected", KST_STORE_DENIED);
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", "x", 1);
	KstFirstWrite wrote = wroteRecord();
	checkInt("write-protected: the record is not removed", kstFilesRecordRemove(&wrote),
	         KST_STORE_DENIED);
	card();
	fakeFatMkdir("KST");
	fakeFatMkdir("KST/KST.TXT");
	kst_image_t b0 = bench();
	KstFirstWrite w0;
	kstFilesFirstWrite(&b0, &b0, &w0);
	checkTrue("a directory named KST.TXT: the record refused, not a full card",
	          w0.record == KST_STORE_DENIED && w0.backup == KST_STORE_OK);
	card();
	fakeFatMkdir("KST");
	fakeFatDirLimit(0);
	refused("a full directory", KST_STORE_CARD_FULL);
	card();
	fakeFatPut("KST", "x", 1);
	refused("a file named KST", KST_STORE_DENIED);
	card();
	fakeFatFail(FakeFatCall::MkDir, FR_DENIED);
	kst_image_t b = bench();
	KstFirstWrite w;
	kstFilesFirstWrite(&b, &b, &w);
	checkInt("no room for the directory: full", w.backup, KST_STORE_CARD_FULL);

	static const FakeFatCall FAILS[] = { FakeFatCall::Write, FakeFatCall::Sync };
	static const char *const FAIL_NAMES[] = { "write", "sync" };
	for (int i = 0; i < 2; i++) {
		card();
		// The marker writes before the backup and does not sync.
		fakeFatFail(FAILS[i], FR_DISK_ERR, FAILS[i] == FakeFatCall::Write ? 1 : 0);
		kstFilesFirstWrite(&b, &b, &w);
		char msg[96];
		snprintf(msg, sizeof(msg), "a one-time %s error: a write error, the file removed",
		         FAIL_NAMES[i]);
		checkTrue(msg, w.backup == KST_STORE_WRITE_ERROR && !fakeFatExists("KST/KST001.TXT") &&
		               fakeFatOpenFiles() == 0);
	}
	// A close that fails keeps the file open in FatFs: it cannot be removed.
	// The first close is the marker's.
	card();
	fakeFatFail(FakeFatCall::Close, FR_DISK_ERR, 1);
	kstFilesFirstWrite(&b, &b, &w);
	checkTrue("a close error: a write error, the file left open and locked",
	          w.backup == KST_STORE_WRITE_ERROR && fakeFatExists("KST/KST001.TXT") &&
	          fakeFatOpenFiles() == 1);
	checkTrue("the record after it is written", w.record == KST_STORE_OK);
	char lbuf[KST_FILE_MAX];
	size_t llen = 0;
	checkInt("the locked file does not load",
	         kst_store_load(kstFilesIo(), "KST001.TXT", lbuf, sizeof(lbuf), &llen),
	         KST_STORE_READ_ERROR);

	// A create that fails takes back the directory it made.
	card();
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	checkTrue("a failed create on a fresh card: an error",
	          kstFilesIo()->create(nullptr, "Wing.TXT", "x", 1) == KST_STORE_IO_ERROR);
	checkTrue("and no KST directory left", !fakeFatExists("KST"));
	card();
	fakeFatMkdir("KST");
	fakeFatPut("KST/Tail.TXT", "x", 1);
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	kstFilesIo()->create(nullptr, "Wing.TXT", "x", 1);
	checkTrue("a directory it did not make stays", fakeFatExists("KST") &&
	                                               fakeFatExists("KST/Tail.TXT"));

	// One of the two taken: the write runs. The writes are the marker's, the
	// backup's and the record's, in that order.
	card();
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR, 2);
	kstFilesFirstWrite(&b, &b, &w);
	checkTrue("the backup taken, the record not: the write runs",
	          w.backup == KST_STORE_OK && w.record == KST_STORE_WRITE_ERROR &&
	          kstFilesWriteAllowed(&w));
	card();
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR, 1);
	kstFilesFirstWrite(&b, &b, &w);
	checkTrue("the record taken, the backup not: the write runs",
	          w.backup == KST_STORE_WRITE_ERROR && w.record == KST_STORE_OK &&
	          kstFilesWriteAllowed(&w));

	card();
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", "x", 1);
	fakeFatFail(FakeFatCall::Unlink, FR_DISK_ERR);
	checkInt("a remove that fails: a write error", kstFilesRecordRemove(&wrote),
	         KST_STORE_WRITE_ERROR);
	checkTrue("the record stays", fakeFatExists("KST/KST.TXT"));
	fakeFatReadOnly("KST/KST.TXT");
	checkInt("a read-only record: refused", kstFilesRecordRemove(&wrote), KST_STORE_DENIED);
	card();
	checkInt("no KST directory: nothing to remove", kstFilesRecordRemove(&wrote), KST_STORE_OK);
}

static void testRecordLoad() {
	section("KST files: the record of an interrupted write");
	card();
	checkTrue("an empty card: no record", !kstFilesRecordPresent());
	char buf[KST_FILE_MAX];
	size_t len = 0;
	checkInt("and none loads", kstFilesRecordLoad(buf, sizeof(buf), &len), KST_STORE_NOT_FOUND);
	fakeFatMkdir("KST");
	fakeFatMkdir("KST/KST.TXT");
	checkTrue("a directory of that name is no record", !kstFilesRecordPresent());
	card();
	kst_image_t before = changed();
	char text[KST_FILE_MAX];
	size_t n = kst_file_format(&before, text, sizeof(text));
	fakeFatMkdir("KST");
	fakeFatPut("KST/KST.TXT", text, n);
	checkTrue("found", kstFilesRecordPresent());
	checkTrue("it loads byte for byte", kstFilesRecordLoad(buf, sizeof(buf), &len) == KST_STORE_OK &&
	                                    len == n && memcmp(buf, text, n) == 0);
	checkInt("into a buffer too small: too large",
	         kstFilesRecordLoad(buf, n - 1, &len), KST_STORE_TOO_LARGE);
	fakeFatFail(FakeFatCall::Read, FR_DISK_ERR);
	checkInt("a read error", kstFilesRecordLoad(buf, sizeof(buf), &len), KST_STORE_READ_ERROR);
	checkInt("no file left open", fakeFatOpenFiles(), 0);
	checkInt("a NULL buffer", kstFilesRecordLoad(nullptr, 1, &len), KST_STORE_ARG);
	len = 7;
	kstFilesRecordLoad(buf, n - 1, &len);
	checkInt("a failed load says 0 bytes", (long)len, 0);
	size_t count = 0, total = 0;
	kst_store_entry_t e[2];
	kst_store_list(kstFilesIo(), e, 2, &count, &total, nullptr);
	checkTrue("the store's list leaves it out", count == 0 && total == 0);
	checkInt("and the store refuses to load it",
	         kst_store_load(kstFilesIo(), "KST.TXT", buf, sizeof(buf), &len), KST_STORE_BAD_NAME);
}

static void testStoreIo() {
	section("KST files: the store over FatFs");
	card();
	const kst_store_io_t *io = kstFilesIo();
	size_t count = 9;
	kst_store_entry_t e[4];
	checkTrue("no KST directory: an empty list",
	          kst_store_list(io, e, 4, &count, nullptr, nullptr) == KST_STORE_OK && count == 0);
	char base[KST_STORE_PRESET_MAX];
	checkTrue("and KST001 proposed", kst_store_preset(io, base, sizeof(base)) == KST_STORE_OK &&
	                                 strcmp(base, "KST001") == 0);
	kst_image_t b = bench();
	char text[KST_FILE_MAX];
	size_t n = kst_file_format(&b, text, sizeof(text));
	checkInt("SAVE AS Wing", kst_store_save(io, "Wing", text, n, nullptr), KST_STORE_OK);
	checkInt("Wing again, in another case: exists",
	         kst_store_save(io, "WING", text, n, nullptr), KST_STORE_EXISTS);
	checkTrue("the create itself refuses a name that exists",
	          io->create(nullptr, "wing.txt", "x", 1) == KST_STORE_IO_EXISTS);
	fakeFatMkdir("KST/Folder.TXT");
	kst_store_list(io, e, 4, &count, nullptr, nullptr);
	checkTrue("a directory is not listed", count == 1 && strcmp(e[0].name, "Wing.TXT") == 0);
	char buf[KST_FILE_MAX];
	size_t len = 0;
	checkTrue("LOAD reads it back",
	          kst_store_load(io, "Wing.TXT", buf, sizeof(buf), &len) == KST_STORE_OK &&
	          len == n && memcmp(buf, text, n) == 0);
	checkInt("a missing file: not found",
	         kst_store_load(io, "Tail.TXT", buf, sizeof(buf), &len), KST_STORE_NOT_FOUND);
	checkTrue("a directory has no size", io->size(nullptr, "Folder.TXT") < 0);
	fakeFatFail(FakeFatCall::Open, FR_DISK_ERR);
	checkInt("an open that fails: a read error",
	         kst_store_load(io, "Wing.TXT", buf, sizeof(buf), &len), KST_STORE_READ_ERROR);
	fakeFatFail(FakeFatCall::Read, FR_DISK_ERR);
	checkInt("a read that fails: a read error",
	         kst_store_load(io, "Wing.TXT", buf, sizeof(buf), &len), KST_STORE_READ_ERROR);
	fakeFatFail(FakeFatCall::ReadDir, FR_DISK_ERR, 1);
	checkInt("a directory that does not read to its end",
	         kst_store_list(io, e, 4, &count, nullptr, nullptr), KST_STORE_LIST_ERROR);
	fakeFatFail(FakeFatCall::OpenDir, FR_DISK_ERR);
	checkInt("a directory that does not open",
	         kst_store_list(io, e, 4, &count, nullptr, nullptr), KST_STORE_LIST_ERROR);
	// A change into KST that fails leaves the call in the root, where the
	// create makes sure of the directory.
	fakeFatFail(FakeFatCall::ChDir, FR_DISK_ERR);
	fakeFatFail(FakeFatCall::Stat, FR_DISK_ERR);
	fakeFatFail(FakeFatCall::MkDir, FR_EXIST);
	checkTrue("KST exists but cannot be looked at: a write error",
	          io->create(nullptr, "Tail.TXT", "x", 1) == KST_STORE_IO_ERROR);
	fakeFatFail(FakeFatCall::Open, FR_DISK_ERR);
	checkTrue("an open that fails on create: an error",
	          io->create(nullptr, "Tail.TXT", "x", 1) == KST_STORE_IO_ERROR);
	checkTrue("and leaves no file", !fakeFatExists("KST/Tail.TXT"));
	checkInt("no file left open", fakeFatOpenFiles(), 0);
	checkInt("no directory left open, after a walk that failed too", fakeFatOpenDirs(), 0);
}

/** @brief The marker's number on the volume, 0 when missing or unreadable. */
static unsigned marker() {
	char buf[8];
	int32_t n = fakeFatGet("KST/" KST_FILES_MARKER_NAME, buf, sizeof(buf));
	return n < 0 ? 0 : kstNamesMarker(buf, (size_t)n);
}

/** @brief The first byte of KST/KSTnnn.TXT, or '\0' when it is missing. */
static char firstByte(unsigned n) {
	char path[24], buf[4];
	snprintf(path, sizeof(path), "KST/KST%03u.TXT", n);
	return fakeFatGet(path, buf, sizeof(buf)) > 0 ? buf[0] : '\0';
}

static void countEntry(const char *, bool, void *ctx) { ++*(unsigned *)ctx; }

/** @brief The entries of KST, files and directories. */
static unsigned kstEntries() {
	unsigned n = 0;
	kstFilesIo()->walk(nullptr, countEntry, &n);
	return n;
}

/** @brief SAVE AS with @p tag as the file's one byte; the number saved, 0 on a refusal. */
static unsigned saveTag(char tag) {
	char name[KST_STORE_NAME_MAX];
	if (kstFilesSaveNext(&tag, 1, name) != KST_STORE_OK) return 0;
	return (unsigned)atoi(name + 3);
}

static uint64_t ring(unsigned from, unsigned to) {
	uint64_t t = 0;
	for (unsigned n = from; n <= to; n++) t |= 1ull << (n - 1u);
	return t;
}

static void testRingRules() {
	section("KST files: the ring's rules");
	checkInt("after none: 1", kstNamesAfter(0), 1);
	checkInt("after 49: 50", kstNamesAfter(49), 50);
	checkInt("after 50: 1", kstNamesAfter(50), 1);
	checkInt("no marker, no file: none", kstNamesGuess(0), 0);
	checkInt("no marker, 1 to 10: 10", kstNamesGuess(ring(1, 10)), 10);
	checkInt("a gap below the highest does not count",
	         kstNamesGuess(ring(1, 10) & ~ring(3, 3)), 10);
	checkInt("2 to 50: 50", kstNamesGuess(ring(2, 50)), 50);
	checkInt("3 to 50 and 1: 1", kstNamesGuess(ring(3, 50) | ring(1, 1)), 1);
	checkInt("all 50: 50", kstNamesGuess(ring(1, 50)), 50);
	checkInt("a marker of 30", kstNamesMarker("030\n", 4), 30);
	checkInt("50 is the last", kstNamesMarker("050\n", 4), 50);
	checkInt("0 is no number", kstNamesMarker("000\n", 4), 0);
	checkInt("51 is outside the ring", kstNamesMarker("051\n", 4), 0);
	checkInt("2 digits: no number", kstNamesMarker("30\n", 3), 0);
	checkInt("no line feed: no number", kstNamesMarker("0300", 4), 0);
	checkInt("a letter: no number", kstNamesMarker("03a\n", 4), 0);
	KstNames k;
	memset(&k, 0, sizeof(k));
	k.latest = 4;
	k.dirs = k.taken = ring(5, 6);
	checkInt("directories are passed over", kstNamesNext(&k), 7);
	k.latest = 50;
	k.dirs = k.taken = ring(1, 50);
	checkInt("all directories: the next number all the same", kstNamesNext(&k), 1);
}

static void testRing() {
	section("KST files: the ring of 50 names");
	card(1u << 22);
	char base[KST_STORE_PRESET_MAX];
	checkTrue("an empty card: KST001 next",
	          kstFilesNextName(base) == KST_STORE_OK && strcmp(base, "KST001") == 0);
	checkInt("SAVE AS takes KST001", saveTag('a'), 1);
	checkInt("and the marker names it", marker(), 1);
	unsigned last = 0;
	for (int i = 0; i < 49; i++) last = saveTag('a');
	checkInt("50 files: the last is KST050", last, 50);
	checkInt("the marker names it", marker(), 50);
	checkInt("50 files, the marker: 51 entries", kstEntries(), 51);
	checkTrue("KST001 next", kstFilesNextName(base) == KST_STORE_OK && strcmp(base, "KST001") == 0);
	checkInt("the 51st save takes KST001", saveTag('b'), 1);
	checkTrue("in place of the oldest", firstByte(1) == 'b' && firstByte(2) == 'a');
	checkInt("still 51 entries", kstEntries(), 51);
	checkInt("the next takes KST002", saveTag('b'), 2);

	// The list: the latest first.
	kst_store_entry_t e[64];
	size_t count = 0;
	kst_store_list(kstFilesIo(), e, 64, &count, nullptr, nullptr);
	KstNames k;
	kstFilesNames(&k);
	kstFilesOrder(e, count, k.latest);
	checkTrue("listed from the latest: KST002, KST001, KST050",
	          count == 50 && strcmp(e[0].name, "KST002.TXT") == 0 &&
	          strcmp(e[1].name, "KST001.TXT") == 0 && strcmp(e[2].name, "KST050.TXT") == 0 &&
	          strcmp(e[49].name, "KST003.TXT") == 0);

	// A file removed on a computer leaves a gap the marker passes over.
	f_unlink("KST/KST010.TXT");
	checkInt("a gap at KST010: the next is KST003 all the same", saveTag('c'), 3);
	checkTrue("KST010 stays free", firstByte(10) == '\0');

	// Without a marker the names give the latest.
	f_unlink("KST/" KST_FILES_MARKER_NAME);
	checkInt("no marker: the number before the lowest free", saveTag('d'), 10);
	// All 50 taken and no marker: the guess is KST050, so KST001 goes.
	fakeFatPut("KST/" KST_FILES_MARKER_NAME, "x", 1);
	checkInt("a marker that does not read, all 50 taken: KST001", saveTag('d'), 1);
	checkInt("and the marker is written again", marker(), 1);

	// A directory under a ring name: passed over and kept.
	f_unlink("KST/KST002.TXT");
	fakeFatMkdir("KST/KST002.TXT");
	checkInt("a directory at KST002: KST003 next", saveTag('e'), 3);
	checkTrue("the directory stays", fakeFatExists("KST/KST002.TXT"));

	// Names outside the ring stay through a whole turn of it.
	fakeFatPut("KST/KST051.TXT", "z", 1);
	fakeFatPut("KST/Wing.TXT", "z", 1);
	for (int i = 0; i < 60; i++) saveTag('f');
	checkTrue("KST051 and Wing stay", fakeFatExists("KST/KST051.TXT") &&
	                                  fakeFatExists("KST/Wing.TXT"));
	checkTrue("49 ring files, the directory, KST051, Wing and the marker: 53 entries",
	          kstEntries() == 53);

	// A marker the card refuses: nothing removed.
	unsigned next = kstNamesAfter(marker());
	fakeFatFail(FakeFatCall::Write, FR_DISK_ERR);
	char name[KST_STORE_NAME_MAX];
	checkInt("a marker that does not write: a write error",
	         kstFilesSaveNext("g", 1, name), KST_STORE_WRITE_ERROR);
	checkTrue("and the oldest file stays", firstByte(next) == 'f' && name[0] == '\0');
	checkInt("the marker names the old latest", kstNamesAfter(marker()), next);
	checkInt("no file left open", fakeFatOpenFiles(), 0);
	checkTrue("and the current directory is the root again", fakeFatCwd()[0] == '\0');

	// The oldest is written over in place; a read-only one is refused.
	char path[24];
	snprintf(path, sizeof(path), "KST/KST%03u.TXT", next);
	fakeFatReadOnly(path);
	checkInt("a read-only oldest file: refused", kstFilesSaveNext("h", 1, name), KST_STORE_DENIED);
	checkTrue("it keeps its bytes", firstByte(next) == 'f');
	checkInt("the marker names it all the same", marker(), next);
	checkInt("the next save takes the number after it", saveTag('i'), kstNamesAfter(next));
	// A change into KST that fails: the paths name KST/ from the root.
	fakeFatFail(FakeFatCall::ChDir, FR_DISK_ERR);
	unsigned got = saveTag('j');
	checkTrue("a change into KST that fails: saved all the same",
	          got != 0 && firstByte(got) == 'j' && fakeFatCwd()[0] == '\0');
	size_t count2 = 0;
	kst_store_list(kstFilesIo(), e, 64, &count2, nullptr, nullptr);
	checkTrue("a list through the store leaves the root current",
	          count2 > 0 && fakeFatCwd()[0] == '\0');
}

static void testRingFirstWrite() {
	section("KST files: the first write in a full ring");
	card(1u << 22);
	for (int i = 0; i < 50; i++) saveTag('a');
	kst_image_t b = bench(), before = changed();
	KstFirstWrite w;
	kstFilesFirstWrite(&b, &before, &w);
	checkTrue("the backup replaces the oldest, KST001",
	          w.backup == KST_STORE_OK && strcmp(w.backupName, "KST001.TXT") == 0 &&
	          firstByte(1) != 'a' && marker() == 1);
	// The next session finds the record its write left.
	kstFilesFirstWrite(&b, &before, &w);
	checkTrue("an old record is kept as KST002, the backup is KST003",
	          w.kept == KST_STORE_OK && strcmp(w.keptName, "KST002.TXT") == 0 &&
	          w.backup == KST_STORE_OK && strcmp(w.backupName, "KST003.TXT") == 0);
	checkTrue("KST004 stays, the marker names KST003",
	          firstByte(4) == 'a' && marker() == 3);
	checkInt("50 files, the record and the marker", kstEntries(), 52);
	checkTrue("the current directory is the root again", fakeFatCwd()[0] == '\0');
	checkInt("a verified write removes its record", kstFilesRecordRemove(&w), KST_STORE_OK);
	checkTrue("gone, and the root current", !fakeFatExists("KST/KST.TXT") && fakeFatCwd()[0] == '\0');
}

static void testWriteAllowed() {
	section("KST files: whether the write runs");
	struct { uint8_t backup, record; bool want; const char *what; } rows[] = {
		{ KST_STORE_NO_CARD,     KST_STORE_NO_CARD,     true,  "no card" },
		{ KST_STORE_OK,          KST_STORE_OK,          true,  "both taken" },
		{ KST_STORE_OK,          KST_STORE_CARD_FULL,   true,  "the backup only" },
		{ KST_STORE_CARD_FULL,   KST_STORE_OK,          true,  "the record only" },
		{ KST_STORE_CARD_FULL,   KST_STORE_CARD_FULL,   false, "neither, full" },
		{ KST_STORE_DENIED,      KST_STORE_DENIED,      false, "neither, refused" },
		{ KST_STORE_NO_NUMBER,   KST_STORE_WRITE_ERROR, false, "neither, no number and an error" },
		{ KST_STORE_NO_CARD,     KST_STORE_WRITE_ERROR, false, "a card that went, and an error" },
	};
	for (auto &r : rows) {
		KstFirstWrite w;
		memset(&w, 0, sizeof(w));
		w.backup = r.backup;
		w.record = r.record;
		checkTrue(r.what, kstFilesWriteAllowed(&w) == r.want);
	}
}

void runKstFilesTests() {
	testMounted();
	testNoCard();
	testFirstWrite();
	testRefusals();
	testRecordLoad();
	testStoreIo();
	testRingRules();
	testRing();
	testRingFirstWrite();
	testWriteAllowed();
	card();
	logger(SdLogState::NoCard);
}
