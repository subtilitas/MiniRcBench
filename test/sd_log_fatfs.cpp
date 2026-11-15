/**
 * @file sd_log_fatfs.cpp
 * @brief The logger, `src/sd_log.cpp`, with its run summary, over FatFs
 *        R0.15 itself.
 *
 * Built by `make -C test lognum` beside log_next_fatfs.cpp, against the same
 * FatFs and the RAM disk of ram_disk.h. sd_log.cpp runs as on the board; the
 * test fakes what lies below it and beside it: the timer, escSnapshot(), the
 * settings and the card driver's lookups. The clock starts past 2^32 µs, so
 * micros() and time_us_64() differ.
 *
 * Covers the summary fed by every frame, ended by STOP, DISARM and each
 * tool, by a full card and a failed write during the log and by a failed
 * flush of the stop's drain and a failed sync at the stop; the `.TXT` beside its `.BFL` with the text
 * runSumText() builds; a stray `.TXT` replaced; the next log's number
 * unmoved by `.TXT` files; the `.TXT` itself not saved, on a full card, in
 * a full directory, by a failed write and by a failed close, with the
 * logger ready and the `.BFL` byte for byte as on a card with room; the
 * summary kept by a failed start and forgotten by a mount and a release; the
 * clock lines from the clock read once at the start and once at the stop,
 * with clockNow() faked. On FAT32 and exFAT; the failures and the clock on
 * FAT32.
 */

#include "check.h"
#include "ram_disk.h"

#include "clock.h"
#include "config.h"
#include "diag.h"
#include "esc_task.h"
#include "hw_config.h"
#include "run_summary.h"
#include "sd_log.h"
#include "sd_log_summary.h"
#include "sdio/sd_card_cache.h"
#include "settings.h"

#include "ff.h"
#include "pico/time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

int g_failures = 0;

// ---------------------------------------------------------------------------
// what lies below and beside the logger
// ---------------------------------------------------------------------------

/** @brief The virtual timer: 5e9 µs at the first test, 83 minutes. */
static uint64_t g_us = 5000000000ull;
uint64_t time_us_64() { return g_us; }
uint32_t time_us_32() { return (uint32_t)g_us; }
void sleep_ms(uint32_t ms) { g_us += (uint64_t)ms * 1000u; }
void sleep_us(uint64_t us) { g_us += us; }

static EscTelemetry g_tel;
void escSnapshot(EscTelemetry *out) { *out = g_tel; }

static Settings g_settings;
Settings *settings() { return &g_settings; }

static sd_card_t g_card;
sd_card_t *sd_get_by_num(size_t) { return &g_card; }
const char *sd_get_drive_prefix(sd_card_t *) { return ""; }
int sd_sdio_init_result(void) { return 0; }

/**
 * @brief The board's clock, faked: set or not, its count of sets, and a
 *        time that runs on with the timer in whole seconds from
 *        @ref g_clockUs, within one day.
 */
static ClockReading g_clock = {};
static uint64_t g_clockUs = 0;   /**< The timer when @ref g_clock's time was given. */
static int g_clockReads = 0;     /**< clockNow() calls. */

void clockNow(ClockReading *out) {
	g_clockReads++;
	*out = g_clock;
	if (!out->set) return;
	uint32_t s = (uint32_t)g_clock.t.hour * 3600u + g_clock.t.minute * 60u + g_clock.t.second +
	             (uint32_t)((g_us - g_clockUs) / 1000000u);
	out->t.hour = (uint8_t)(s / 3600u % 24u);
	out->t.minute = (uint8_t)(s / 60u % 60u);
	out->t.second = (uint8_t)(s % 60u);
}

/** @brief The clock set at @p t from now, as the @p sets th set of the power-up. */
static void clockSetTo(uint32_t sets, ClockTime t) {
	g_clock = { true, sets, t };
	g_clockUs = g_us;
}

/** @brief run_summary.cpp's SD LOG rows use it; this program draws none. */
void diagUptimeText(char *buf, size_t n, uint32_t seconds) {
	snprintf(buf, n, "%lu", (unsigned long)seconds);
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static FATFS s_fs;
static uint32_t nowMs() { return (uint32_t)(g_us / 1000u); }

/** @brief Format the card, mount it for the logger. */
static void format(BYTE fmt, LBA_t sectors, DWORD au) {
	ramDiskFormat(&s_fs, fmt, sectors, au);
	g_card.state.sectors = (uint32_t)sectors;
	if (!sdLogBegin()) { fprintf(stderr, "sdLogBegin failed\n"); exit(2); }
}

/** @brief The logger's status. */
static SdLogStatus status() {
	SdLogStatus st;
	sdLogStatus(&st);
	return st;
}

/** @brief The throttle sequence's state, for frames(). */
static uint32_t g_frame = 0;

/**
 * @brief Log @p n frames, 2 ms apart, flushing after each unless @p flush
 *        is false. DShot eRPM 7000 in every frame; EDT 16.00 V, 12 A, 40 °C
 *        every 50th frame; a throttle that differs from frame to frame, so
 *        no two blocks of a log begin alike.
 */
static void frames(int n, bool flush = true) {
	for (int i = 0; i < n; i++) {
		g_us += 2000;
		uint32_t ms = nowMs();
		g_tel.erpm = 7000;
		g_tel.lastRpmMs = ms;
		if (i % 50 == 0) {
			g_tel.volts = 16.0f;
			g_tel.amps = 12.0f;
			g_tel.tempC = 40;
			g_tel.edtVoltsMs = g_tel.edtAmpsMs = g_tel.edtTempMs = ms;
		}
		g_frame = g_frame * 1103515245u + 12345u;   // a linear congruential sequence
		sdLogTick(time_us_32(), (uint16_t)((g_frame >> 16) % 2001u));
		if (flush) sdLogFlush();
	}
}

/** @brief A file's bytes; false when it does not open. */
static bool readFile(const char *path, std::string *out) {
	FIL f;
	out->clear();
	if (f_open(&f, path, FA_READ) != FR_OK) return false;
	char buf[512];
	UINT got = 0;
	while (f_read(&f, buf, sizeof(buf), &got) == FR_OK && got > 0) out->append(buf, got);
	f_close(&f);
	return true;
}

static bool exists(const char *path) {
	FILINFO fno;
	return f_stat(path, &fno) == FR_OK;
}

/** @brief Put a file at @p path holding @p text, as a computer would. */
static void put(const char *path, const char *text) {
	FIL f;
	UINT w = 0;
	if (f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK ||
	    f_write(&f, text, (UINT)strlen(text), &w) != FR_OK || f_close(&f) != FR_OK) {
		fprintf(stderr, "put %s\n", path);
		exit(2);
	}
}

/** @brief The text runSumText() builds from the status's summary. */
static std::string summaryText(const SdLogStatus &st) {
	char buf[1200];
	size_t len = runSumText(st.summary, &st, st.summaryPoles, buf, sizeof(buf));
	return std::string(buf, len < sizeof(buf) ? len : 0);
}

static bool has(const std::string &text, const char *line) {
	return text.find(line) != std::string::npos;
}

/** @brief Leave @p keep clusters free: a file over the rest, as a full card. */
static void fill(DWORD keep) {
	DWORD freeClst = 0;
	FATFS *fs = nullptr;
	FIL f;
	if (f_getfree("", &freeClst, &fs) != FR_OK || freeClst < keep ||
	    f_open(&f, "/FILL.BIN", FA_WRITE | FA_CREATE_NEW) != FR_OK ||
	    f_expand(&f, (FSIZE_t)(freeClst - keep) * fs->csize * 512u, 1) != FR_OK ||
	    f_close(&f) != FR_OK) {
		fprintf(stderr, "fill failed\n");
		exit(2);
	}
}

static DWORD freeClusters() {
	DWORD n = 0;
	FATFS *fs = nullptr;
	return f_getfree("", &n, &fs) == FR_OK ? n : 0;
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

static void testPure() {
	section("Run summary file: what a failure says about the card");
	checkTrue("a short write with FR_OK: card full",
	          sdLogSummaryWriteEnd(FR_OK, 10, 900) == RunSumFile::CardFull);
	checkTrue("a write error: write failed",
	          sdLogSummaryWriteEnd(FR_DISK_ERR, 0, 900) == RunSumFile::WriteFailed);
	checkTrue("no entry to create: card full", sdLogSummaryOpenEnd(FR_DENIED) == RunSumFile::CardFull);
	checkTrue("an open error: write failed", sdLogSummaryOpenEnd(FR_DISK_ERR) == RunSumFile::WriteFailed);
	checkTrue("a card not ready: write failed", sdLogSummaryOpenEnd(FR_NOT_READY) == RunSumFile::WriteFailed);
}

/** @brief A log started by hand, armed for part of it, stopped by STOP. */
static void testStop(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Logger over FatFs, %s: STOP writes the .TXT beside the .BFL", vol);
	section(t);
	checkTrue("no summary before the first log", status().summary == nullptr);
	g_settings.poles = 14;
	checkTrue("START by hand", sdLogStart());
	uint64_t start = g_us;
	checkTrue("no summary while the log runs", status().summary == nullptr);
	frames(100);
	sdLogSetArmed(true);          // the log runs: arming leaves it
	uint64_t armed = g_us;
	frames(300);
	sdLogSetArmed(false);         // a log started by hand: disarming leaves it
	uint64_t disarmed = g_us;
	checkTrue("still logging after the disarm", sdLogActive());
	frames(100);
	g_settings.poles = 12;        // MOTOR POLES changed during the log
	g_us += 700;                  // the stop 0.7 ms after the last frame
	uint64_t stop = g_us;
	sdLogStop();

	SdLogStatus st = status();
	checkTrue("ready, nothing to report", st.state == SdLogState::Idle && st.end == SdLogEnd::None);
	checkTrue("the summary is there", st.summary != nullptr);
	if (!st.summary) return;
	checkTrue("ended by STOP, saved",
	          st.summary->end == RunSumEnd::Stop && st.summary->file == RunSumFile::Saved);
	checkTrue("from the start to the stop",
	          st.summary->startUs == start && st.summary->stopUs == stop);
	checkTrue("armed from the arm to the disarm", st.summary->armedUs == disarmed - armed);
	checkInt("poles at the stop", st.summaryPoles, 12);
	checkInt("frames", (long)st.framesLogged, 500);

	std::string txt, bfl;
	checkTrue("LOG00001.TXT is there", readFile("/LOGS/LOG00001.TXT", &txt));
	checkTrue("its text is runSumText()'s", txt == summaryText(st));
	checkTrue("Log: LOG00001.BFL", has(txt, "\nLog: LOG00001.BFL\n"));
	checkTrue("Ended by: STOP", has(txt, "\nEnded by: STOP\n"));
	checkTrue("Duration: 1.0 s, Armed: 0.6 s", has(txt, "\nDuration: 1.0 s\nArmed: 0.6 s\n"));
	checkTrue("Max RPM from the frames, 12 poles",
	          has(txt, "\nMax RPM: 1166 RPM (DShot, eRPM 7000, 12 poles; raw 1166 RPM)\n"));
	checkTrue("Max current from EDT", has(txt, "\nMax current: 12 A (EDT, 1 A steps; raw 12 A)\n"));
	checkTrue("Min voltage while armed",
	          has(txt, "\nMin voltage: 16.00 V (EDT, 0.25 V steps, while armed; raw 16.00 V)\n"));
	checkTrue("Frames: 500", has(txt, "\nFrames: 500\n"));
	checkTrue("LOG00001.BFL is whole", readFile("/LOGS/LOG00001.BFL", &bfl) &&
	          bfl.size() == st.bytesWritten && bfl.compare(0, 12, "H Product:Bl") == 0);

	// The .TXT does not move the next log's number, kept or read again.
	checkTrue("the next START: LOG00002.BFL", sdLogStart() && status().fileNumber == 2);
	frames(10);
	sdLogStop();
	checkTrue("LOG00002.TXT beside it", exists("/LOGS/LOG00002.BFL") && exists("/LOGS/LOG00002.TXT"));
	checkTrue("a mount", sdLogRemount());
	checkTrue("forgets the summary", status().summary == nullptr);
	checkTrue("LOGS/ read again: LOG00003.BFL", sdLogStart() && status().fileNumber == 3);
	sdLogStop();
}

/** @brief A log arming started and disarming stopped, and the three tools. */
static void testEnds(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Logger over FatFs, %s: DISARM and the tools", vol);
	section(t);
	g_settings.poles = 14;
	sdLogSetArmed(true);
	checkTrue("arming starts a log", sdLogActive() && status().fileNumber == 1);
	frames(200);
	sdLogSetArmed(false);
	SdLogStatus st = status();
	checkTrue("disarming stops it", st.state == SdLogState::Idle && st.summary != nullptr);
	if (!st.summary) return;
	checkTrue("ended by DISARM, saved",
	          st.summary->end == RunSumEnd::Disarm && st.summary->file == RunSumFile::Saved);
	checkTrue("armed for the whole log",
	          st.summary->armedUs == st.summary->stopUs - st.summary->startUs);
	std::string txt;
	checkTrue("Ended by: DISARM in LOG00001.TXT",
	          readFile("/LOGS/LOG00001.TXT", &txt) && has(txt, "\nEnded by: DISARM\n") &&
	          has(txt, "\nDuration: 0.4 s\nArmed: 0.4 s\n"));

	struct { SdLogEnd tool; RunSumEnd how; const char *line; const char *what; } tools[] = {
		{ SdLogEnd::ByKst,   RunSumEnd::ByKst,   "\nEnded by: KST SERVO\n",   "KST SERVO's claim, saved" },
		{ SdLogEnd::ByHitec, RunSumEnd::ByHitec, "\nEnded by: HITEC SERVO\n", "HITEC SERVO's claim, saved" },
		{ SdLogEnd::ByAm32,  RunSumEnd::ByAm32,  "\nEnded by: AM32 CONFIG\n", "AM32 CONFIG's claim, saved" },
	};
	uint32_t n = 2;
	for (auto &k : tools) {
		sdLogStart();
		frames(20);
		sdLogStopFor(k.tool);
		st = status();
		char path[32];
		snprintf(path, sizeof(path), "/LOGS/LOG%05lu.TXT", (unsigned long)n++);
		checkTrue(k.what, st.end == k.tool && st.summary && st.summary->end == k.how &&
		          st.summary->file == RunSumFile::Saved && readFile(path, &txt) && has(txt, k.line));
	}
}

/** @brief A stray `.TXT` of the new log's number, and one past it. */
static void testStray(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Logger over FatFs, %s: stray .TXT files", vol);
	section(t);
	f_mkdir("/LOGS");
	put("/LOGS/LOG00004.BFL", "H");
	std::string junk(3000, 'x');
	put("/LOGS/LOG00005.TXT", junk.c_str());
	put("/LOGS/LOG00009.TXT", "a summary whose log a computer deleted");
	checkTrue("after LOG00004.BFL: LOG00005.BFL", sdLogStart() && status().fileNumber == 5);
	frames(50);
	sdLogStop();
	std::string txt;
	SdLogStatus st = status();
	checkTrue("LOG00005.TXT replaced by the summary, whole",
	          readFile("/LOGS/LOG00005.TXT", &txt) && st.summary && txt == summaryText(st));
	checkTrue("LOG00009.TXT left alone", readFile("/LOGS/LOG00009.TXT", &txt) &&
	          txt == "a summary whose log a computer deleted");
	checkTrue("the next START: LOG00006.BFL, not after LOG00009",
	          sdLogStart() && status().fileNumber == 6);
	sdLogStop();
	sdLogRemount();
	checkTrue("after a mount as well: LOG00007.BFL", sdLogStart() && status().fileNumber == 7);
	sdLogStop();
}

/** @brief A failed start keeps the last log's summary. */
static void testFailedStart(const char *vol) {
	char t[96];
	snprintf(t, sizeof(t), "Logger over FatFs, %s: a failed start keeps the summary", vol);
	section(t);
	sdLogStart();
	frames(20);
	sdLogStop();
	const RunSum *sum = status().summary;
	g_disk.protect = true;
	g_disk.noInit = true;
	checkTrue("START on a write-protected card fails", !sdLogStart());
	SdLogStatus st = status();
	checkTrue("open failed, LOG00001's number and summary kept",
	          st.end == SdLogEnd::OpenFailed && st.fileNumber == 1 && st.summary == sum &&
	          sum && sum->file == RunSumFile::Saved);
	g_disk.protect = false;
	sdLogRemount();
	checkTrue("a mount forgets it", status().summary == nullptr);

	sdLogStart();
	frames(20);
	sdLogStop();
	checkTrue("the next log's summary", status().summary != nullptr);
	checkTrue("USB DRIVE's release", sdLogRelease());
	st = status();
	checkTrue("forgets it: no card, no summary",
	          st.state == SdLogState::NoCard && st.summary == nullptr);
	checkTrue("the card taken back", sdLogReacquire());
}

/** @brief The log a fixed run writes, from a fixed clock: the same bytes each time. */
static const uint64_t kRunStartUs = 6000000000ull;
static const int kRunFrames = 1500;

static void fixedRun() {
	g_us = kRunStartUs;
	g_tel = EscTelemetry();
	g_frame = 0;
	sdLogStart();
	frames(kRunFrames);
	sdLogStop();
}

/**
 * @brief The `.TXT` cannot be saved: the card is full, its entry has no room,
 *        or its block write fails. The logger stays ready and the `.BFL` is
 *        the bytes of the same run on a card with room.
 */
static void testTxtNotSaved() {
	section("Logger over FatFs, FAT32: the .TXT not saved, the .BFL whole");
	const LBA_t sectors = 2u * 1024u * 1024u;   // 1 GiB, 4 KiB clusters
	format(FM_FAT32, sectors, 4096);
	fixedRun();
	std::string want, wantTxt, got, txt;
	readFile("/LOGS/LOG00001.BFL", &want);
	readFile("/LOGS/LOG00001.TXT", &wantTxt);
	checkTrue("a card with room: saved", status().summary &&
	          status().summary->file == RunSumFile::Saved && exists("/LOGS/LOG00001.TXT"));
	DWORD clusters = (DWORD)((want.size() + 4095u) / 4096u);

	// The card full once the log is closed: no cluster for the text.
	format(FM_FAT32, sectors, 4096);
	f_mkdir("/LOGS");
	fill(clusters);
	fixedRun();
	SdLogStatus st = status();
	checkTrue("card full: not saved, CARD FULL",
	          st.summary && st.summary->file == RunSumFile::CardFull);
	checkTrue("the logger ready, no card fault",
	          st.state == SdLogState::Idle && st.end == SdLogEnd::None);
	checkTrue("no LOG00001.TXT: the empty file removed", !exists("/LOGS/LOG00001.TXT"));
	checkTrue("LOG00001.BFL byte for byte", readFile("/LOGS/LOG00001.BFL", &got) && got == want);
	checkInt("no cluster free", (long)freeClusters(), 0);

	// LOGS/ has no free entry and the card no cluster to grow it by.
	format(FM_FAT32, sectors, 4096);
	f_mkdir("/LOGS");
	for (int i = 0; i < 125; i++) {       // ".", "..", 125 files, the log: 128
		char p[24];
		snprintf(p, sizeof(p), "/LOGS/F%03d.DAT", i);
		put(p, "");
	}
	fill(clusters);
	fixedRun();
	st = status();
	checkTrue("no entry for the .TXT: CARD FULL", st.summary &&
	          st.summary->file == RunSumFile::CardFull && st.state == SdLogState::Idle);
	checkTrue("no LOG00001.TXT", !exists("/LOGS/LOG00001.TXT"));
	FIL f;
	checkInt("the create itself refused: FR_DENIED",
	         f_open(&f, "/LOGS/LOG00001.TXT", FA_WRITE | FA_CREATE_ALWAYS), FR_DENIED);
	checkTrue("LOG00001.BFL byte for byte", readFile("/LOGS/LOG00001.BFL", &got) && got == want);

	// The text's first block write fails, in f_write(): the text is longer
	// than a sector.
	format(FM_FAT32, sectors, 4096);
	static const char head[] = "MiniRcBench run summary";
	g_disk.failPrefix = head;
	g_disk.failLen = sizeof(head) - 1;
	fixedRun();
	g_disk.failPrefix = nullptr;
	st = status();
	checkTrue("the write fails: WRITE FAILED", st.summary &&
	          st.summary->file == RunSumFile::WriteFailed);
	checkTrue("the logger ready, no card fault",
	          st.state == SdLogState::Idle && st.end == SdLogEnd::None);
	checkTrue("no LOG00001.TXT: the file removed", !exists("/LOGS/LOG00001.TXT"));
	checkTrue("LOG00001.BFL byte for byte", readFile("/LOGS/LOG00001.BFL", &got) && got == want);
	checkTrue("the next START works: LOG00002.BFL", sdLogStart() && status().fileNumber == 2);
	sdLogStop();
	checkTrue("and saves its summary", status().summary &&
	          status().summary->file == RunSumFile::Saved && exists("/LOGS/LOG00002.TXT"));

	// The log's last block, written by the stop's sync, fails: CARD ERROR,
	// and no .TXT is tried, though the card would take it.
	static std::string last;
	size_t lastAt = (want.size() - 1) / 512u * 512u;
	last = want.substr(lastAt, 64);
	bool unique = true;
	for (size_t at = 0; at < lastAt; at += 512u) unique = unique && want.compare(at, 64, last) != 0;
	checkTrue("the log's last block begins unlike the others", unique);
	format(FM_FAT32, sectors, 4096);
	g_disk.failPrefix = last.data();
	g_disk.failLen = last.size();
	fixedRun();
	g_disk.failPrefix = nullptr;
	st = status();
	checkTrue("the stop's sync fails: WRITE FAILED",
	          st.state == SdLogState::Error && st.end == SdLogEnd::WriteFailed);
	checkTrue("the summary up to the stop, not saved", st.summary &&
	          st.summary->end == RunSumEnd::Stop && st.summary->file == RunSumFile::WriteFailed);
	checkTrue("no LOG00001.TXT", !exists("/LOGS/LOG00001.TXT"));

	// The text's last block, written by the close, fails: the close fails.
	static std::string tail;
	tail = wantTxt.substr(512, 64);
	format(FM_FAT32, sectors, 4096);
	g_disk.failPrefix = tail.data();
	g_disk.failLen = tail.size();
	fixedRun();
	g_disk.failPrefix = nullptr;
	st = status();
	checkTrue("the close fails: WRITE FAILED", st.summary &&
	          st.summary->file == RunSumFile::WriteFailed);
	checkTrue("the logger ready, no card fault",
	          st.state == SdLogState::Idle && st.end == SdLogEnd::None);
	checkTrue("LOG00001.BFL byte for byte", readFile("/LOGS/LOG00001.BFL", &got) && got == want);
	// FatFs keeps the file open after a failed close, and its lock refuses
	// the removal until the next mount; the entry it commits is empty.
	checkInt("LOG00001.TXT still locked: FR_LOCKED",
	         f_open(&f, "/LOGS/LOG00001.TXT", FA_READ), FR_LOCKED);
	sdLogRemount();
	checkTrue("after a mount: LOG00001.TXT empty, no cut text",
	          readFile("/LOGS/LOG00001.TXT", &txt) && txt.empty());
	checkTrue("the next START: LOG00002.BFL", sdLogStart() && status().fileNumber == 2);
	sdLogStop();
	f_mount(nullptr, "", 0);
}

/** @brief The log itself fails: the summary ends there, no file is tried. */
static void testLogFails() {
	section("Logger over FatFs, FAT32: the log fails, the summary not saved");
	const LBA_t sectors = 2u * 1024u * 1024u;
	format(FM_FAT32, sectors, 4096);
	f_mkdir("/LOGS");
	fill(2);
	sdLogStart();
	uint64_t start = g_us;
	SdLogStatus st = status();
	for (int i = 0; i < 5000 && st.state == SdLogState::Logging; i++) {
		frames(1);
		st = status();
	}
	uint64_t failed = g_us;
	checkTrue("CARD FULL during the log", st.state == SdLogState::Error && st.end == SdLogEnd::CardFull);
	checkTrue("the summary ends there, not saved: CARD FULL",
	          st.summary && st.summary->end == RunSumEnd::CardFull &&
	          st.summary->file == RunSumFile::CardFull);
	checkTrue("up to the failure", st.summary && st.summary->startUs == start &&
	          st.summary->stopUs == failed);
	g_us += 1000000;
	sdLogStop();
	checkTrue("STOP after it changes nothing", status().summary &&
	          status().summary->stopUs == failed && status().state == SdLogState::Error);
	checkTrue("no LOG00001.TXT", !exists("/LOGS/LOG00001.TXT"));

	// A write that fails during the log.
	format(FM_FAT32, sectors, 4096);
	sdLogStart();
	frames(500);
	g_disk.failWrites = true;
	for (int i = 0; i < 1000 && status().state == SdLogState::Logging; i++) frames(1);
	uint64_t at = g_us;
	g_disk.failWrites = false;
	st = status();
	checkTrue("WRITE FAILED during the log", st.state == SdLogState::Error && st.end == SdLogEnd::WriteFailed);
	checkTrue("the summary ends there, not saved: WRITE FAILED",
	          st.summary && st.summary->end == RunSumEnd::WriteFailed &&
	          st.summary->file == RunSumFile::WriteFailed && st.summary->stopUs == at);
	checkTrue("no LOG00001.TXT", !exists("/LOGS/LOG00001.TXT"));

	// The stop's sync fails.
	format(FM_FAT32, sectors, 4096);
	sdLogStart();
	frames(500);
	g_disk.failWrites = true;
	uint64_t stop = g_us;
	sdLogStop();
	g_disk.failWrites = false;
	st = status();
	checkTrue("the stop fails: WRITE FAILED", st.state == SdLogState::Error && st.end == SdLogEnd::WriteFailed);
	checkTrue("the summary up to the stop, not saved",
	          st.summary && st.summary->end == RunSumEnd::Stop &&
	          st.summary->file == RunSumFile::WriteFailed && st.summary->stopUs == stop);
	checkTrue("no LOG00001.TXT", !exists("/LOGS/LOG00001.TXT"));

	// The drain's first flush fails: the ring holds more than a chunk, as
	// when the loop has not flushed for a while. Each block write takes
	// 1 ms, so a later end shows in the summary's stop time.
	format(FM_FAT32, sectors, 4096);
	sdLogStart();
	frames(100, false);
	st = status();
	checkTrue("the ring holds more than a chunk, nothing written",
	          st.peakBuffer > SD_LOG_CHUNK_BYTES && st.bytesWritten == 0 && st.bytesDropped == 0);
	g_disk.failWrites = true;
	g_disk.clockUs = &g_us;
	g_disk.writeUs = 1000;
	stop = g_us;
	sdLogStop();
	bool wrote = g_us > stop;
	g_disk.clockUs = nullptr;
	g_disk.failWrites = false;
	st = status();
	checkTrue("the drain fails: WRITE FAILED, a block write tried",
	          st.state == SdLogState::Error && st.end == SdLogEnd::WriteFailed && wrote);
	checkTrue("the summary ended by STOP as the stop began, not saved",
	          st.summary && st.summary->end == RunSumEnd::Stop &&
	          st.summary->file == RunSumFile::WriteFailed && st.summary->stopUs == stop);
	checkTrue("no LOG00001.TXT", !exists("/LOGS/LOG00001.TXT"));
	f_mount(nullptr, "", 0);
}

/** @brief The clock lines of the `.TXT`: the clock read at the start and at the stop. */
static void testClock() {
	section("Logger over FatFs, FAT32: the clock at the start and the stop");
	const LBA_t sectors = 2u * 1024u * 1024u;
	format(FM_FAT32, sectors, 4096);
	std::string txt;

	// Not set: both lines say so, one read at the start and one at the stop.
	g_clock = {};
	int reads = g_clockReads;
	sdLogStart();
	checkInt("one read at the start", g_clockReads - reads, 1);
	frames(10);
	checkInt("none while logging", g_clockReads - reads, 1);
	sdLogStop();
	checkInt("one at the stop", g_clockReads - reads, 2);
	checkTrue("LOG00001.TXT", readFile("/LOGS/LOG00001.TXT", &txt));
	checkTrue("not set at both", has(txt, "\nClock at start: not set\nClock at stop: not set\n"));

	// Set before the start: the start and the stop 2.5 s later.
	clockSetTo(1, { 2026, 11, 14, 15, 42, 7 });
	g_us += 300000;                 // 0.3 s into the second
	sdLogStart();
	frames(1000);                   // 2 s
	g_us += 500000;
	sdLogStop();
	checkTrue("LOG00002.TXT", readFile("/LOGS/LOG00002.TXT", &txt));
	checkTrue("the start's time", has(txt, "\nClock at start: 2026-11-14 15:42:07\n"));
	checkTrue("the stop's time", has(txt, "\nClock at stop: 2026-11-14 15:42:09\n"));
	checkTrue("the summary holds both",
	          status().summary && status().summary->clockStart.set &&
	          status().summary->clockStop.t.second == 9);

	// Set during the log: not set at the start, the stop says so.
	g_clock = {};
	sdLogStart();
	frames(100);
	clockSetTo(1, { 2026, 11, 14, 16, 0, 0 });
	frames(100);
	sdLogStop();
	checkTrue("LOG00003.TXT", readFile("/LOGS/LOG00003.TXT", &txt));
	checkTrue("start not set, stop set during the log",
	          has(txt, "\nClock at start: not set\n"
	                   "Clock at stop: 2026-11-14 16:00:00 (clock set during the log)\n"));

	// Set again during the log.
	sdLogStart();
	frames(100);
	clockSetTo(2, { 2026, 11, 14, 17, 30, 0 });
	sdLogStop();
	checkTrue("LOG00004.TXT", readFile("/LOGS/LOG00004.TXT", &txt));
	checkTrue("the start's time, the stop's after a second set",
	          has(txt, "\nClock at start: 2026-11-14 16:00:00\n"
	                   "Clock at stop: 2026-11-14 17:30:00 (clock set during the log)\n"));

	// A disarm's stop reads the clock as STOP's does.
	sdLogSetArmed(true);            // arming starts the log
	frames(100);
	g_us += 1000000;
	sdLogSetArmed(false);
	checkTrue("LOG00005.TXT", readFile("/LOGS/LOG00005.TXT", &txt));
	checkTrue("DISARM: the stop's time",
	          has(txt, "\nClock at stop: 2026-11-14 17:30:01\nEnded by: DISARM\n"));

	// The drain fails inside the stop: the clock stays as read when the
	// stop began, read once.
	sdLogStart();
	frames(100, false);
	g_disk.failWrites = true;
	reads = g_clockReads;
	g_disk.clockUs = &g_us;
	g_disk.writeUs = 1000000;       // each block write 1 s: a second read would differ
	sdLogStop();
	g_disk.clockUs = nullptr;
	g_disk.failWrites = false;
	SdLogStatus st = status();
	checkTrue("the stop fails", st.state == SdLogState::Error && st.summary);
	checkInt("the clock read once at the stop", g_clockReads - reads, 1);
	checkTrue("the stop's reading from the stop's start",
	          st.summary && st.summary->clockStop.t.second == 1 &&
	          st.summary->clockStop.t.minute == 30);
	g_clock = {};
	f_mount(nullptr, "", 0);
}

static void runVolume(const char *vol, BYTE fmt, LBA_t sectors, DWORD au) {
	format(fmt, sectors, au);
	testStop(vol);
	format(fmt, sectors, au);
	testEnds(vol);
	format(fmt, sectors, au);
	testStray(vol);
	format(fmt, sectors, au);
	testFailedStart(vol);
	f_mount(nullptr, "", 0);
}

int main() {
	testPure();
	runVolume("FAT32", FM_FAT32, 2u * 1024u * 1024u, 4096);      // 1 GiB, 4 KiB clusters
	runVolume("exFAT", FM_EXFAT, 2u * 1024u * 1024u, 32768);     // 1 GiB, 32 KiB clusters
	testTxtNotSaved();
	testLogFails();
	testClock();
	printf("\n=======================\n");
	if (g_failures) printf("%d CHECK(S) FAILED\n", g_failures);
	else printf("all checks passed\n");
	return g_failures ? 1 : 0;
}
