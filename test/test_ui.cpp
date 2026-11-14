/**
 * @file test_ui.cpp
 * @brief UI, navigation and gesture tests driven through the real screen code.
 *
 * ui_am32.cpp is included rather than linked so the tests can read its
 * file-static state directly. That keeps the production source free of
 * test-only accessors, at the cost of this translation unit owning it -- the
 * Makefile therefore must not compile ui_am32.cpp separately.
 */

#include "check.h"
#include "fakes.h"
#include "diag.h"
#include "settings.h"
#include "servo_supply.h"
#include "sense.h"
#include "fake_i2c.h"
#include "hardware/gpio.h"
#include "ui_input.h"
#include "board_desc.h"
#include "gfx.h"
#include "ui.h"
#include "ui_settings.h"
#include "ui_menu.h"
#include "ui_sdlog.h"
#include "ui_screen.h"
#include "ui_servo.h"
#include "ui_hitec.h"
#include "ui_kst.h"
#include "touch.h"
#include "esc_task.h"
#include "esc_merge.h"
#include "config.h"
#include "kiss_telem.h"
#include "sd_log.h"
#include "pin_claim.h"
#include "core1_job.h"
#include "plat.h"

#include "card_cache.h"
#include "fake_fatfs.h"
#include "usb_msc.h"
#include "version.h"
#include "../src/ui_am32.cpp"   // NOLINT -- see the note above

// --- probes into the included translation unit ---
static uint8_t uiByte(int o)     { return s_eeprom[o]; }
static int     uiSelected()      { return s_selected; }
static int     uiScroll()        { return s_list.scrollPx; }
static int     uiVisibleCount()  { return (int)s_visibleCount; }

/** @brief Index in the AM32 list of the field called @p name, or -1. */
static int uiRowOf(const char *name) {
	for (uint16_t v = 0; v < s_visibleCount; v++)
		if (strcmp(AM32_FIELDS[s_visible[v]].name, name) == 0) return (int)v;
	return -1;
}

// --- frame driving ---
#define UI_FRAME_MS 25

static void frames(int n) {
	// Core1's pump runs between UI frames.
	for (int i = 0; i < n; i++) { fakeAdvance(UI_FRAME_MS); fakePumpPoll(); uiTick(); }
}

// The helpers that address a control by id. They run frames() between touches.
#define UI_IDS_FRAMES(n) frames(n)
// Not every helper is used in this file.
#include "ui_ids.h"

/**
 * @brief A tap at a computed point. A tap on a control goes through tapId();
 *        this is for the points no control owns, such as the gap between two
 *        buttons or a pixel just outside one.
 */
static void tap(int x, int y) {
	fakePress(x, y); frames(2); fakeRelease(); frames(1);
}
/** @brief A frame @p ms after the last, for holds timed to the millisecond. */
static void frameAfter(uint32_t ms) { fakeAdvance(ms); fakePumpPoll(); uiTick(); }
static void swipe(int x0, int x1, int y, int step) {
	fakePress(x0, y); frames(1);
	for (int x = x0; (step > 0) ? (x <= x1) : (x >= x1); x += step) {
		fakeHold(x, y); frames(1);
	}
	fakeRelease(); frames(1);
}

/**
 * @brief Scroll a named field into view and return a y inside its row.
 *
 * The scroll offset is in pixels now, so the row the field lands on has to be
 * computed from the offset that actually took effect: asking to put a field at
 * the top when there is not a viewport's worth of list below it leaves the
 * scroll clamped, and the row ends up further down than requested. The row's
 * rectangle comes from the control table after the scroll, so it is where the
 * list put it.
 */
static int uiRowY(const char *name) {
	int v = uiRowOf(name);
	if (v < 0) return -1;
	uiListScrollTo(&s_list, v * ROW_PITCH);
	UiRect r = ctlRect(UI_ROW(v, Row));
	return r.y + r.h / 2;
}

/** @brief The AM32 list's viewport, as the union of its row rectangles. */
static UiRect listView() {
	UiControl t[UI_CONTROLS_MAX];
	uint8_t n = uiControls(t, UI_CONTROLS_MAX);
	int x0 = 1000, y0 = 1000, x1 = -1, y1 = -1;
	for (uint8_t i = 0; i < n; i++) {
		if (t[i].id.row == UI_FIXED) continue;
		const UiRect &r = t[i].rect;
		if (r.x < x0) x0 = r.x;
		if (r.y < y0) y0 = r.y;
		if (r.x + r.w > x1) x1 = r.x + r.w;
		if (r.y + r.h > y1) y1 = r.y + r.h;
	}
	if (x1 < 0) return UiRect{ 0, 0, 0, 0 };
	return UiRect{ (int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0) };
}

/** @brief A y on the tester's throttle track, on its centre line. */
static int thrY() {
	UiRect r = ctlRect(UI_FIX(TesterCtl::Throttle));
	return r.y + r.h / 2;
}

/** @brief THR_EDGE_SAT in ui.cpp: a mapping constant, not a coordinate. Must match. */
#define THR_EDGE_SAT_T 16

/**
 * @brief The standard "ESC is talking" fixture, stamped as having just arrived.
 *
 * Call it again to keep the data alive. A real ESC sends continuously, but the
 * fake serves one frozen snapshot, so without re-stamping the readings expire
 * mid-test exactly as they are meant to -- which is correct behaviour and a
 * useless screenshot.
 */
static void feedLiveTelemetry() {
	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.erpm = 84210; tel.rpm = 12030;
	tel.volts = 15.8f; tel.edtVoltsMs  = millis();
	tel.amps = 23.4f;  tel.edtAmpsMs   = millis();
	tel.tempC = 46;    tel.edtTempMs   = millis();
	tel.stress = 12;   tel.edtStressMs = millis();
	tel.edtStatusMs = millis();
	tel.lastRpmMs = millis();
	tel.packetRate = 998;
	fakeSetTelemetry(&tel);
}

/**
 * @brief Hold the ARM button long enough to actually arm.
 * @param want Whether the button is expected to act; Disabled for a hold
 *        that is meant to be refused.
 */
static void holdToArm(IdExpect want = IdExpect::Enabled) {
	pressId(UI_FIX(TesterCtl::Arm), want);
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); frames(2);
}

/**
 * @brief Navigate main -> MENU -> SD LOG.
 *
 * Assumes the main screen is showing and the ESC is disarmed.
 */
static void enterLogScreen() {
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(menuRow(&UI_SDLOG));
	checkTrue("SD LOG opens from MENU", uiCurrentScreen() == UiScreenId::SdLog);
}

/**
 * @brief The logging screen's buttons, checked through the fake logger.
 *
 * No probe into ui.cpp is needed: the fake *is* the observable. Tapping START
 * has to reach sdLogStart() for the fake's state to change, so this exercises
 * the real navigation, hit-testing and dispatch rather than asserting on
 * pixels.
 */
static void testLogScreen() {
	section("SD logging screen");

	// A card that mounted but is not recording. The card details matter even
	// though no assertion reads them: this frame is published, and a screen
	// reading "MOUNT 0 OK" beside "CARD NONE" contradicts itself.
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	st.cardType = 3;             // SDHC/XC
	st.cardSizeMB = 30500;       // a 32 GB card, as the card reports itself
	fakeSdLogSet(&st);

	enterLogScreen();
	frames(2);
	checkTrue("not recording on arrival", !sdLogActive());
	checkControlTable("SD LOG, card idle");
	fakeDumpFrame("shot_log_ready.ppm");
	{
		// Command buttons carry the neutral text colour (menu-b.md, decision 11).
		UiRect t = ctlRect(UI_FIX(SdLogCtl::Toggle)), u = ctlRect(UI_FIX(SdLogCtl::Usb));
		checkTrue("START's label in the text colour, scale 2",
		          fakeTextIs(t.x + (t.w - gfxTextW("START", 2)) / 2, t.y + (t.h - 14) / 2,
		                     "START", C_TEXT, C_PANEL, 2));
		checkTrue("USB DRIVE's label in the text colour",
		          fakeTextIs(u.x + (u.w - gfxTextW("USB DRIVE", 1)) / 2, u.y + (u.h - 7) / 2,
		                     "USB DRIVE", C_TEXT, C_PANEL, 1));
	}

	tapId(UI_FIX(SdLogCtl::Toggle));
	checkTrue("START begins a log", sdLogActive());

	sdLogStatus(&st);
	checkTrue("a file number is assigned", st.fileNumber != 0);

	tapId(UI_FIX(SdLogCtl::Toggle));
	checkTrue("STOP ends it", !sdLogActive());

	// Counters worth showing: a full buffer and lost frames are the two things
	// the screen exists to make visible.
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Logging;
	st.cardType = 3;
	st.cardSizeMB = 30500;
	st.fileNumber = 42;
	st.framesLogged = 12345;
	st.bytesWritten = 178000;
	st.dropEvents = 3;
	st.peakBuffer = 7000;
	st.worstFlushMs = 64;
	fakeSdLogSet(&st);
	frames(2);
	fakeDumpFrame("shot_log_screen.ppm");

	// With no card, START must not pretend to have started.
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	st.mountResult = 3;          // FR_NOT_READY: the card did not initialise
	st.cardInit = 1;             // SD_SDIO_INIT_SILENT
	fakeSdLogSet(&st);
	frames(2);
	// The first thing anyone sees, since the card slot is empty by default.
	fakeDumpFrame("shot_log_nocard.ppm");
	checkControlTable("SD LOG, no card");
	static const char *const HINT = "INSERT A CARD, THEN RETRY MOUNT";
	const int noteY = 228;
	checkTrue("3 NOT READY, no size: the note says to insert a card",
	          fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	checkTrue("the hint fits the panel", gfxTextW(HINT, 1) <= GFX_W - 2 * UI_MARGIN);
	{
		UiRect r = ctlRect(UI_FIX(SdLogCtl::Retry));
		checkTrue("the button reads RETRY MOUNT, inside its 104 px",
		          gfxTextW("RETRY MOUNT", 1) + 8 <= r.w &&
		          fakeTextIs(r.x + (r.w - gfxTextW("RETRY MOUNT", 1)) / 2, r.y + (r.h - 7) / 2,
		                     "RETRY MOUNT", C_TEXT, C_PANEL, 1));
	}
	// A card that answers and fails otherwise needs another fix.
	st.mountResult = 13;         // FR_NO_FILESYSTEM
	st.cardSizeMB = 30500;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("13 NO FILESYSTEM: no insert hint", !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	st.mountResult = 3;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("3 NOT READY with a size read: no insert hint",
	          !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	st.mountResult = 1;          // FR_DISK_ERR
	st.cardSizeMB = 0;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("1 DISK ERR: no insert hint", !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	st.state = SdLogState::Error;
	st.mountResult = 3;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("CARD ERROR: no insert hint", !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("NO CARD and 3 NOT READY again: the hint", fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	// A card that answered and failed its initialisation: not an empty slot.
	static const char *const BAD = "CARD NOT USABLE: RESEAT OR REPLACE";
	st.cardInit = 2;             // SD_SDIO_INIT_UNUSABLE
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("a card that answered and failed: CARD NOT USABLE, amber",
	          fakeTextCentredIs(noteY, BAD, C_AMBER, C_BG, 1) &&
	          !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1));
	checkTrue("it fits the panel", gfxTextW(BAD, 1) <= GFX_W - 2 * UI_MARGIN);
	st.cardInit = 3;             // SD_SDIO_INIT_NO_BUS
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("the bus not claimed: no hint", !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1) &&
	          !fakeTextCentredIs(noteY, BAD, C_AMBER, C_BG, 1));
	st.cardInit = 0;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("initialised: no hint", !fakeTextCentredIs(noteY, HINT, C_DIM, C_BG, 1) &&
	          !fakeTextCentredIs(noteY, BAD, C_AMBER, C_BG, 1));
	st.cardInit = 1;
	fakeSdLogSet(&st);
	frames(2);
	tapId(UI_FIX(SdLogCtl::Toggle), IdExpect::Disabled);
	checkTrue("START does nothing without a card", !sdLogActive());

	// A card inserted after boot. sdLogBegin() only runs once, so without this
	// button the card would stay invisible until a power cycle -- which looks
	// exactly like a card the firmware cannot read.
	tapId(UI_FIX(SdLogCtl::Retry));
	frames(2);
	sdLogStatus(&st);
	checkTrue("RETRY finds a card inserted later",
	          st.state == SdLogState::Idle);
	checkInt("and reports it mounted cleanly", st.mountResult, 0);
	checkInt("with its type", st.cardType, 3);
	checkTrue("and its size", st.cardSizeMB > 100000);
	fakeDumpFrame("shot_log_mounted.ppm");

	// BACK leaves SD LOG for MENU, and MENU's BACK reaches the tester.
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SD LOG reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/** @brief Whether SD LOG's FILE row reads @p text, right-aligned. */
static bool logFileRowIs(const char *text) {
	// Row 1 of 19 px rows from y 44; scale 1 text centred in it.
	const int y = UI_HDR_H + 8 + 19 + (19 - 7) / 2;
	return fakeTextIs(GFX_W - UI_MARGIN - gfxTextW(text, 1), y, text, C_TEXT, C_BG, 1);
}

/**
 * @brief A new log's number: the highest `LOGnnnnn.BFL` plus 1, the name
 *        rule the directory scan applies, and the refusal after
 *        `LOG99999.BFL`.
 *
 * The pure rule in sd_log.h and SD LOG against the fake logger. The scan
 * itself, the kept number and its reading again after a remount run over
 * FatFs in test/log_next_fatfs.cpp (`make lognum`).
 */
static void testLogNumbering() {
	section("SD LOG: a new log is numbered after the highest file");

	struct NameCase { const char *name; bool match; uint32_t n; };
	static const NameCase NAMES[] = {
		{ "LOG00001.BFL", true, 1 },
		{ "LOG00042.BFL", true, 42 },
		{ "LOG00000.BFL", true, 0 },
		{ "LOG99999.BFL", true, 99999 },
		{ "LOG12345.BFL", true, 12345 },
		// FAT compares names without case: these are the same files.
		{ "log00007.bfl", true, 7 },
		{ "Log00008.Bfl", true, 8 },
		{ "LOG00009.bfl", true, 9 },
		{ "lOG00010.BFl", true, 10 },
		{ "LOG0001.BFL",  false, 0 },    // four digits
		{ "LOG000001.BFL", false, 0 },   // six digits
		{ "LOG00001.TXT", false, 0 },
		{ "LOG00001.BF",  false, 0 },
		{ "LOG00001.BFLX", false, 0 },
		{ "LOG00001.BFL.bak", false, 0 },
		{ "LOG00001",     false, 0 },
		{ "LOG00001.",    false, 0 },
		{ "LOG00001BFL",  false, 0 },
		{ "LOG0000A.BFL", false, 0 },
		{ "LOG-0001.BFL", false, 0 },
		{ "LOG 0001.BFL", false, 0 },
		{ "LOG:0001.BFL", false, 0 },    // ':' follows '9' in ASCII
		{ "LOG/0001.BFL", false, 0 },    // '/' precedes '0'
		{ " LOG00001.BFL", false, 0 },
		{ "LOG00001.BFL ", false, 0 },
		{ "XLOG00001.BFL", false, 0 },
		{ "XOG00001.BFL", false, 0 },
		{ "LAG00001.BFL", false, 0 },
		{ "LOX00001.BFL", false, 0 },
		{ "LOG00001XBFL", false, 0 },
		{ "LOG00001.AFL", false, 0 },
		{ "LOG00001.BEL", false, 0 },
		{ "LOG00001.BFK", false, 0 },
		{ "Flight with the new prop LOG00099.BFL", false, 0 },   // a long name
		{ "LOG000~1.BFL", false, 0 },    // an 8.3 alias of a long name
		{ "LOG",          false, 0 },
		{ "",             false, 0 },
		{ ".",            false, 0 },
		{ "..",           false, 0 },
		{ "LOGS",         false, 0 },
	};
	for (const NameCase &c : NAMES) {
		uint32_t n = 0xDEADBEEFu;
		bool m = sdLogNameNumber(c.name, &n);
		char what[96];
		snprintf(what, sizeof(what), "\"%s\": %s", c.name, c.match ? "a log's name" : "not a log's name");
		checkTrue(what, m == c.match);
		if (c.match) {
			snprintf(what, sizeof(what), "\"%s\": number %lu", c.name, (unsigned long)c.n);
			checkTrue(what, n == c.n);
		} else {
			checkTrue("and the number is left alone", n == 0xDEADBEEFu);
		}
	}
	{
		uint32_t n = 5;
		checkTrue("no name: no log", !sdLogNameNumber(nullptr, &n) && n == 5);
	}

	// The scan's fold: the highest wins, whatever the order; others ignored.
	{
		uint32_t h = 0;
		static const char *DIR1[] = { "LOG00003.BFL", "LOG00017.BFL", "notes.txt", "LOG00009.BFL",
		                              "LOG000017.BFL", "LOG00200.TXT", "log00012.bfl" };
		for (const char *nm : DIR1) sdLogNoteName(&h, nm);
		checkInt("the highest of 3, 17, 9 and 12, the rest ignored", (long)h, 17);
		checkInt("so the next is 18", (long)sdLogNextNumber(h), 18);
		h = 0;
		sdLogNoteName(&h, "LOG00000.BFL");
		checkInt("LOG00000.BFL alone: highest 0", (long)h, 0);
		checkInt("next 1", (long)sdLogNextNumber(h), 1);
		h = 40;
		sdLogNoteName(&h, "LOG00005.BFL");
		checkInt("a lower number leaves the highest", (long)h, 40);
		sdLogNoteName(&h, "LOG00041.BFL");
		checkInt("a higher one takes it", (long)h, 41);
	}
	checkInt("an empty or new directory: 1", (long)sdLogNextNumber(0), 1);
	checkInt("after 1: 2", (long)sdLogNextNumber(1), 2);
	checkInt("after 99998: 99999", (long)sdLogNextNumber(99998), 99999);
	checkInt("after 99999: none free", (long)sdLogNextNumber(99999), 0);
	checkInt("never past it", (long)sdLogNextNumber(100000), 0);
	checkInt("nor wrapped", (long)sdLogNextNumber(0xFFFFFFFFu), 0);

	// On SD LOG, through the fake card, which uses the same rule: each start
	// takes the number after the highest, also after a log was deleted below
	// it, and LOG99999.BFL refuses the start with its two lines.
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	st.cardType = 3;
	st.cardSizeMB = 30500;
	fakeSdLogSet(&st);
	fakeSdLogSetHighest(0);
	enterLogScreen();
	frames(2);
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	checkTrue("an empty LOGS/: LOG00001.BFL", logFileRowIs("LOG00001.BFL"));
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	checkTrue("the next start: LOG00002.BFL", logFileRowIs("LOG00002.BFL"));
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	// A computer left LOG00041.BFL as the highest, the ones below deleted.
	fakeSdLogSetHighest(41);
	tapId(UI_FIX(SdLogCtl::Retry));
	frames(2);
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	checkTrue("after LOG00041.BFL: LOG00042.BFL", logFileRowIs("LOG00042.BFL"));
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	fakeSdLogSetHighest(99998);
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	checkTrue("after LOG99998.BFL: LOG99999.BFL", logFileRowIs("LOG99999.BFL"));
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	sdLogStatus(&st);
	checkTrue("after LOG99999.BFL: no log starts", !sdLogActive() && st.state == SdLogState::Error &&
	                                                st.end == SdLogEnd::NoFreeNumber);
	checkTrue("the note says which file", fakeTextCentredIs(217, "NO LOG STARTED: LOG99999.BFL EXISTS",
	                                                        C_AMBER, C_BG, 1));
	checkTrue("and the next step", fakeTextCentredIs(228, "MOVE LOGS TO A COMPUTER, RETRY MOUNT",
	                                                 C_AMBER, C_BG, 1));
	checkTrue("the FILE row keeps the last log", logFileRowIs("LOG99999.BFL (LAST)"));
	// A mount forgets the cause; with the logs moved off, the next is 1.
	fakeSdLogSetHighest(0);
	tapId(UI_FIX(SdLogCtl::Retry));
	frames(2);
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	checkTrue("LOGS/ emptied on a computer: LOG00001.BFL again", logFileRowIs("LOG00001.BFL"));
	tapId(UI_FIX(SdLogCtl::Toggle));
	frames(2);
	fakeSdLogSetHighest(0);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
}

/**
 * @brief How the last log ended, on SD LOG's two note lines, and the FILE
 *        row's last log.
 */
static void testLogEndNotes() {
	section("SD LOG: how the last log ended; the last log's file");

	// The write's own rule: FatFs ends a write that finds no cluster with
	// FR_OK and fewer bytes; any error is a failed write.
	checkTrue("FR_OK, 100 of 512 bytes: the card is full",
	          sdLogWriteEnd(0, 100, 512) == SdLogEnd::CardFull);
	checkTrue("FR_OK, 511 of 512: full", sdLogWriteEnd(0, 511, 512) == SdLogEnd::CardFull);
	checkTrue("FR_OK, nothing written: full", sdLogWriteEnd(0, 0, 512) == SdLogEnd::CardFull);
	checkTrue("1 DISK ERR: the write failed", sdLogWriteEnd(1, 0, 512) == SdLogEnd::WriteFailed);
	checkTrue("9 INVALID OBJECT, part written: failed",
	          sdLogWriteEnd(9, 100, 512) == SdLogEnd::WriteFailed);
	checkTrue("an error with every byte reported: failed",
	          sdLogWriteEnd(1, 512, 512) == SdLogEnd::WriteFailed);
	// A mount forgets a card's fault and keeps a tool's stop.
	{
		static const SdLogEnd ALL[] = { SdLogEnd::None, SdLogEnd::CardFull, SdLogEnd::WriteFailed,
		                                SdLogEnd::OpenFailed, SdLogEnd::ByKst, SdLogEnd::ByHitec,
		                                SdLogEnd::ByAm32, SdLogEnd::NoFreeNumber };
		bool ok = true;
		for (SdLogEnd e : ALL) {
			SdLogEnd m = e;
			sdLogMountForget(&m);
			bool tool = e == SdLogEnd::ByKst || e == SdLogEnd::ByHitec || e == SdLogEnd::ByAm32;
			ok &= tool ? m == e : m == SdLogEnd::None;
		}
		checkTrue("a mount: the faults forgotten, the three tools' stops kept", ok);
	}

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	st.cardType = 3;
	st.cardSizeMB = 30500;
	fakeSdLogSet(&st);
	fakeSdLogSetHighest(0);                                // an empty LOGS/
	enterLogScreen();
	frames(2);
	checkTrue("before the first log: FILE --", logFileRowIs("--"));
	int x0 = 0, x1 = 0;
	checkTrue("and both note lines blank", !fakeInkExtent(217, 7, C_BG, &x0, &x1) &&
	                                       !fakeInkExtent(228, 7, C_BG, &x0, &x1));

	tapId(UI_FIX(SdLogCtl::Toggle));                       // START
	frames(2);
	checkTrue("recording: the open log's name", logFileRowIs("LOG00001.BFL"));
	tapId(UI_FIX(SdLogCtl::Toggle));                       // STOP
	frames(2);
	checkTrue("stopped: the same name, marked (LAST)", logFileRowIs("LOG00001.BFL (LAST)"));
	checkTrue("it keeps clear of the label FILE",
	          UI_MARGIN + gfxTextW("FILE", 1) + 6 <=
	              GFX_W - UI_MARGIN - gfxTextW("LOG09999.BFL (LAST)", 1));
	checkTrue("a STOP leaves no note", !fakeInkExtent(217, 7, C_BG, &x0, &x1));
	fakeDumpFrame("shot_log_last.ppm");
	// A mount: the card may be another one, and the file not on it.
	{
		SdLogStatus cur;
		sdLogStatus(&cur);
		cur.framesLogged = 5000;
		cur.bytesWritten = 72000;
		cur.dropEvents = 2;
		cur.peakBuffer = 900;
		cur.worstFlushMs = 30;
		fakeSdLogSet(&cur);
		tapId(UI_FIX(SdLogCtl::Retry));
		frames(2);
		sdLogStatus(&cur);
		checkTrue("RETRY MOUNT: FILE -- again", logFileRowIs("--"));
		checkTrue("and the counters beside it at zero",
		          cur.fileNumber == 0 && cur.framesLogged == 0 && cur.bytesWritten == 0 &&
		          cur.dropEvents == 0 && cur.peakBuffer == 0 && cur.worstFlushMs == 0);
	}

	// Each end, on its two lines, amber.
	static const struct {
		SdLogState state;
		SdLogEnd end;
		const char *l1, *l2;
	} ENDS[] = {
		{ SdLogState::Error, SdLogEnd::CardFull,
		  "RECORDING STOPPED: CARD FULL", "FREE SPACE ON A COMPUTER, RETRY MOUNT" },
		{ SdLogState::Error, SdLogEnd::WriteFailed,
		  "RECORDING STOPPED: CARD WRITE FAILED", "REMOVED? REINSERT, THEN RETRY MOUNT" },
		{ SdLogState::Error, SdLogEnd::OpenFailed,
		  "NO LOG STARTED: THE FILE DID NOT OPEN", "CHECK THE CARD, THEN RETRY MOUNT" },
		{ SdLogState::Error, SdLogEnd::NoFreeNumber,
		  "NO LOG STARTED: LOG99999.BFL EXISTS", "MOVE LOGS TO A COMPUTER, RETRY MOUNT" },
		{ SdLogState::Idle, SdLogEnd::ByKst,   "RECORDING STOPPED BY KST SERVO", "" },
		{ SdLogState::Idle, SdLogEnd::ByHitec, "RECORDING STOPPED BY HITEC SERVO", "" },
		{ SdLogState::Idle, SdLogEnd::ByAm32,  "RECORDING STOPPED BY AM32 CONFIG", "" },
	};
	for (const auto &e : ENDS) {
		st.state = e.state;
		st.end = e.end;
		st.fileNumber = 12;
		// About 12 minutes at 7.3 kB/s before the card filled.
		st.framesLogged = 361500;
		st.bytesWritten = 5242880;
		st.peakBuffer = 3100;
		st.worstFlushMs = 38;
		fakeSdLogSet(&st);
		frames(2);
		char what[96];
		snprintf(what, sizeof(what), "%s: line 1 at y 217, amber", e.l1);
		checkTrue(what, fakeTextCentredIs(217, e.l1, C_AMBER, C_BG, 1));
		if (e.l2[0]) {
			snprintf(what, sizeof(what), "%s: line 2 at y 228, amber", e.l2);
			checkTrue(what, fakeTextCentredIs(228, e.l2, C_AMBER, C_BG, 1));
		} else {
			checkTrue("a tool's stop: line 2 blank", !fakeInkExtent(228, 7, C_BG, &x0, &x1));
		}
		checkTrue("both lines inside the margins",
		          gfxTextW(e.l1, 1) <= GFX_W - 2 * UI_MARGIN &&
		          gfxTextW(e.l2, 1) <= GFX_W - 2 * UI_MARGIN);
		checkTrue("the FILE row names the last log", logFileRowIs("LOG00012.BFL (LAST)"));
		if (e.end == SdLogEnd::CardFull) fakeDumpFrame("shot_log_card_full.ppm");
	}

	// A refusal of USB DRIVE outranks the note for its 4000 ms.
	st.state = SdLogState::Logging;
	st.end = SdLogEnd::None;
	fakeSdLogSet(&st);
	tapId(UI_FIX(SdLogCtl::Usb));
	st.state = SdLogState::Error;
	st.end = SdLogEnd::WriteFailed;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("refused, then the write failed: the refusal alone",
	          fakeTextCentredIs(228, "STOP RECORDING FIRST", C_AMBER, C_BG, 1) &&
	          !fakeInkExtent(217, 7, C_BG, &x0, &x1));
	frames(170);
	checkTrue("the refusal over: the note",
	          fakeTextCentredIs(217, "RECORDING STOPPED: CARD WRITE FAILED", C_AMBER, C_BG, 1));

	// A tool's stop through the real claim: AM32 CONFIG on a running log.
	tapId(UI_FIX(SdLogCtl::Retry));                        // a mount: the note goes
	frames(2);
	checkTrue("RETRY MOUNT clears the note", !fakeInkExtent(217, 7, C_BG, &x0, &x1));
	tapId(UI_FIX(SdLogCtl::Toggle));                       // START
	checkTrue("a log runs", sdLogActive());
	tapId(UI_FIX(UI_BACK));
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	tapId(UI_FIX(TesterCtl::Menu));
	frames(2);
	tapId(menuRow(&UI_AM32));
	frames(20);
	checkTrue("AM32 CONFIG's claim stopped the log", !sdLogActive());
	tapId(UI_FIX(UI_BACK));
	frames(2);
	tapId(menuRow(&UI_SDLOG));
	frames(2);
	checkTrue("SD LOG names the tool",
	          fakeTextCentredIs(217, "RECORDING STOPPED BY AM32 CONFIG", C_AMBER, C_BG, 1));
	tapId(UI_FIX(SdLogCtl::Retry));
	frames(2);
	checkTrue("a mount keeps a tool's stop: it stays true of the last log",
	          fakeTextCentredIs(217, "RECORDING STOPPED BY AM32 CONFIG", C_AMBER, C_BG, 1));
	{
		// The card gone: the mount hint takes the second line.
		SdLogStatus gone;
		sdLogStatus(&gone);
		gone.state = SdLogState::NoCard;
		gone.mountResult = 3;
		gone.cardInit = 1;
		gone.cardSizeMB = 0;
		fakeSdLogSet(&gone);
		frames(2);
		checkTrue("no card: the tool's stop, and the insert hint under it",
		          fakeTextCentredIs(217, "RECORDING STOPPED BY AM32 CONFIG", C_AMBER, C_BG, 1) &&
		          fakeTextCentredIs(228, "INSERT A CARD, THEN RETRY MOUNT", C_DIM, C_BG, 1));
		tapId(UI_FIX(SdLogCtl::Retry));            // the fake finds a card
		frames(2);
	}
	tapId(UI_FIX(SdLogCtl::Toggle));                       // START
	frames(2);
	checkTrue("the next start clears it", !fakeInkExtent(217, 7, C_BG, &x0, &x1));
	tapId(UI_FIX(SdLogCtl::Toggle));                       // STOP
	frames(2);

	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("back on the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/**
 * @brief The tester's SDERR: an amber chip, armed or not, clear of the arm
 *        badge, the RPM and the throttle.
 */
static void testTesterSdErr() {
	section("Tester: SDERR is an amber chip");
	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("a card idle: SD, dim text", fakeTextIs(104, 14, "SD", C_DIM, C_PANEL, 1));
	holdToArm();
	checkTrue("armed", uiArmed());
	st.state = SdLogState::Error;
	st.end = SdLogEnd::WriteFailed;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("the log failed while armed: SDERR on an amber chip",
	          fakeTextIs(104, 14, "SDERR", C_PAPER, C_AMBER, 1));
	const uint16_t *fb = gfxBuffer();
	checkTrue("the chip's fill from x 101 to 134, y 12 to 22",
	          fb[12 * GFX_W + 102] == C_AMBER && fb[22 * GFX_W + 133] == C_AMBER &&
	          fb[17 * GFX_W + 100] == C_PANEL && fb[17 * GFX_W + 135] == C_PANEL &&
	          fb[11 * GFX_W + 110] == C_PANEL && fb[23 * GFX_W + 110] == C_PANEL);
	checkTrue("the arm badge still reads ARMED",
	          fakeTextIs(4 + (92 - gfxTextW("ARMED", 2)) / 2, 7, "ARMED", C_ONACCENT, C_RED, 2));
	fakeDumpFrame("shot_tester_sderr.ppm");
	tapId(UI_FIX(TesterCtl::Arm));                         // DISARM
	frames(2);
	checkTrue("disarmed", !uiArmed());
	checkTrue("disarmed: the chip stays", fakeTextIs(104, 14, "SDERR", C_PAPER, C_AMBER, 1));
	st.state = SdLogState::Idle;
	st.end = SdLogEnd::None;
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("mounted again: SD, the chip gone",
	          fakeTextIs(104, 14, "SD", C_DIM, C_PANEL, 1) && fb[12 * GFX_W + 102] == C_PANEL);
}

/**
 * @brief The USB drive button: what it refuses, and what the screen becomes.
 *
 * The rules themselves are asserted in test_usb_msc.cpp against the real
 * header. This is the other half -- that the screen actually asks, actually
 * shows the reason, and actually stops offering the logger's controls once the
 * card is gone.
 */
static void testUsbDriveScreen() {
	section("USB drive: the button and the screen");

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	st.cardType = 3;
	st.cardSizeMB = 30500;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);

	enterLogScreen();
	frames(2);

	// Refused while recording, and the screen says why rather than doing
	// nothing -- a button that silently declines is indistinguishable from one
	// that is broken.
	tapId(UI_FIX(SdLogCtl::Toggle));                       // START
	checkTrue("recording", sdLogActive());
	// The note line above START (y 228) is drawn text, not a control; the probe
	// takes the 14 px band around it.
	uint32_t quiet = fakeRegionHash(0, 224, GFX_W, 14);
	tapId(UI_FIX(SdLogCtl::Usb));
	frames(2);
	checkTrue("USB DRIVE is refused while recording",
	          mscGetState() == MscState::Idle);
	checkTrue("and the note line says so",
	          fakeRegionHash(0, 224, GFX_W, 14) != quiet);
	fakeDumpFrame("shot_log_usb_refused.ppm");

	// The note lasts 4000 ms and does not come back 2^31 ms after it ended.
	frames(170);
	checkTrue("4250 ms later the note is gone", fakeRegionHash(0, 224, GFX_W, 14) == quiet);
	fakeAdvance(0x80000000u);
	frames(2);
	checkTrue("2^31 ms after its end the note is not back",
	          fakeRegionHash(0, 224, GFX_W, 14) == quiet);
	// A refusal outranks the mount hint: refused while recording, then the
	// card gone with nothing answering, the refusal stays for its 4000 ms.
	{
		SdLogStatus keep;
		sdLogStatus(&keep);
		tapId(UI_FIX(SdLogCtl::Usb));
		SdLogStatus gone;
		memset(&gone, 0, sizeof(gone));
		gone.state = SdLogState::NoCard;
		gone.mountResult = 3;
		gone.cardInit = 1;
		fakeSdLogSet(&gone);
		frames(2);
		checkTrue("refusal and no card at once: the refusal shows",
		          fakeTextCentredIs(228, "STOP RECORDING FIRST", C_AMBER, C_BG, 1) &&
		          !fakeTextCentredIs(228, "INSERT A CARD, THEN RETRY MOUNT", C_DIM, C_BG, 1));
		frames(170);
		checkTrue("the refusal over: the mount hint",
		          fakeTextCentredIs(228, "INSERT A CARD, THEN RETRY MOUNT", C_DIM, C_BG, 1));
		fakeSdLogSet(&keep);
		frames(2);
	}
	// Nor a note that leaving the screen cut short, past 2^31 ms.
	tapId(UI_FIX(SdLogCtl::Usb));
	frames(2);
	checkTrue("refused again: the note shows", fakeRegionHash(0, 224, GFX_W, 14) != quiet);
	tapId(UI_FIX(UI_BACK));
	fakeAdvance(0x80000000u + 5000u);
	frames(1);
	tapId(menuRow(&UI_SDLOG));
	frames(2);
	checkTrue("a note cut short by BACK is not back past 2^31 ms",
	          uiCurrentScreen() == UiScreenId::SdLog &&
	          fakeRegionHash(0, 224, GFX_W, 14) == quiet);

	// Stop the log and it goes through.
	tapId(UI_FIX(SdLogCtl::Toggle));                       // STOP
	checkTrue("stopped", !sdLogActive());
	tapId(UI_FIX(SdLogCtl::Usb));
	frames(2);
	checkTrue("with the log stopped, the card is handed over",
	          mscGetState() == MscState::Serving);

	// The screen is now about the handover and nothing else.
	fakeMscSet(MscState::Serving, 3 * 1024 * 2);  // 3 MB read
	frames(2);
	checkControlTable("SD LOG, card held");
	fakeDumpFrame("shot_log_usb_serving.ppm");
	checkTrue("the read counter in lime, a live reading, at scale 2",
	          fakeTextCentredIs(UI_HDR_H + 8 + 104, "3.0 MB READ", C_LIME, C_BG, 2));

	// START must not work while a host owns the card: the logger has let go.
	tapId(UI_FIX(SdLogCtl::Toggle));                       // now EJECT, not START
	frames(2);
	checkTrue("the big button is EJECT while serving, not START",
	          !sdLogActive());
	checkTrue("and it gives the card back", mscGetState() == MscState::Idle);
	{
		// The card came back by a mount: the computer may have deleted the
		// last log, so FILE names none and the counters start again.
		SdLogStatus back;
		sdLogStatus(&back);
		frames(2);
		checkTrue("EJECT: FILE -- and the counters at zero",
		          logFileRowIs("--") && back.fileNumber == 0 && back.framesLogged == 0 &&
		          back.bytesWritten == 0);
	}

	// Arming is refused while a host holds the card, from the tester screen --
	// which is reachable, because BACK does not cancel a copy in progress.
	// BACK leaves SD LOG for MENU while the host holds the card too, and
	// MENU's BACK then reaches the tester.
	fakeMscSet(MscState::Serving, 0);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SD LOG, card held: MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("leaving does not end the handover", mscGetState() == MscState::Serving);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	frames(2);
	holdToArm(IdExpect::Disabled);
	checkTrue("a host holding the card blocks arming", !uiArmed());

	fakeMscSet(MscState::Idle, 0);
	frames(2);
}

/**
 * @brief A hand-started log must survive arming and the disarm that follows.
 *
 * Reported from hardware, and a good bug: opening MENU force-disarms, and
 * the logging screen is reached *through* MENU. So
 * auto-stop-on-disarm cancelled exactly the log you walked over to check on,
 * and there was no way to observe a manual log without ending it.
 */
static void testManualLogSurvivesArming() {
	section("Manual logging vs auto-on-arm");

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;
	st.cardSizeMB = 61000;
	fakeSdLogSet(&st);

	enterLogScreen();
	tapId(UI_FIX(SdLogCtl::Toggle));
	checkTrue("manual START begins a log", sdLogActive());

	// Back to the main screen, through MENU, and arm.
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SD LOG reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	// A tap will not arm: it takes a one-second hold, and using tap() here is
	// how an earlier version of this test passed against the broken rule.
	holdToArm();
	checkTrue("actually armed", uiArmed());
	checkTrue("still logging while armed", sdLogActive());

	// Walking back to the logging screen goes through MENU, which
	// force-disarms. That must not end a log the operator started.
	enterLogScreen();
	frames(2);
	checkTrue("armed state cleared by opening MENU", !uiArmed());
	checkTrue("manual log survives the force-disarm", sdLogActive());

	// STOP is still the operator's to press.
	tapId(UI_FIX(SdLogCtl::Toggle));
	checkTrue("manual STOP ends it", !sdLogActive());
	tapId(UI_FIX(UI_BACK));               // to MENU
	frames(2);
	tapId(UI_FIX(UI_BACK));               // to the tester
	frames(2);
	checkTrue("back on the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/**
 * @brief The main screen with KISS telemetry live.
 *
 * The merge policy itself is unit-tested in test_kiss.cpp; what this covers is
 * that the UI actually goes through escMerge() rather than reading the EDT
 * fields directly, which is what it did before.
 */
static void testKissDisplay() {
	section("KISS on the main screen");

	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.erpm = 84210; tel.rpm = 12030;
	tel.volts = 15.75f; tel.edtVoltsMs  = millis();  // EDT: 0.25 V steps
	tel.amps = 23.0f;   tel.edtAmpsMs   = millis();  // EDT: whole amps
	tel.tempC = 46;     tel.edtTempMs   = millis();
	tel.stress = 12;    tel.edtStressMs = millis();
	tel.edtStatusMs = millis();
	tel.lastRpmMs = millis();
	tel.packetRate = 998;

	// Fresh KISS, with values deliberately distinguishable from the EDT ones so
	// a screenshot shows which source won.
	tel.haveKiss   = true;
	tel.kissLastMs = millis();
	tel.kissVolts  = 15.83f;
	tel.kissAmps   = 23.47f;
	tel.kissTempC  = 47;
	tel.kissMah    = 812;
	tel.kissErpm   = 84200;
	fakeSetTelemetry(&tel);
	frames(2);
	fakeDumpFrame("shot_tester_kiss.ppm");
	// A source tag is neutral: KISS in the text colour, 6 px in from the
	// tile's right edge at x 119, on the label row at y 133.
	checkTrue("the KISS tag in the text colour",
	          fakeTextIs(119 - 6 - gfxTextW("KISS", 1), 133, "KISS", C_TEXT, C_PANEL, 1));

	// --- what the screen shows, not what escMerge() returns ---
	//
	// This test used to call escMerge() itself and assert on the struct, which
	// is a pure-function test already covered in test_kiss.cpp, and is not what
	// the docstring above claims. It proved nothing about the screen: making
	// drawTelemetry() read the raw EDT field instead of the merged one passed
	// it, and would have put a coarse 0.25 V value on screen wearing a KISS tag.
	//
	// Tying it to pixels is what closes that. The two sources carry deliberately
	// different values, so the voltage tile must render differently depending on
	// which one won.
	// The *value* rows only, not the whole tile. The KISS/EDT tag sits on the
	// label row at the top, and it changes with the source all by itself -- so
	// hashing the whole tile passes even when the number below it came from the
	// wrong place, which is exactly the regression being guarded against.
	// drawLabelled() puts the label at y+4 and the value at y+15, scale 2.
	auto voltTile = []() { return fakeRegionHash(1, 143, 118, 18); };
	auto ampTile  = []() { return fakeRegionHash(121, 143, 118, 18); };

	uint32_t voltsFromKiss = voltTile();
	uint32_t ampsFromKiss  = ampTile();

	// Let KISS expire while EDT stays fresh. Same ESC, same tile, coarser
	// number -- and a different tag beside it.
	fakeAdvance(KISS_STALE_MS + 50);
	tel.edtVoltsMs = millis();
	tel.edtAmpsMs  = millis();
	tel.edtTempMs  = millis();
	tel.edtStressMs = millis();
	tel.edtStatusMs = millis();
	tel.lastRpmMs = millis();
	fakeSetTelemetry(&tel);
	frames(2);
	checkTrue("the voltage tile changes when KISS expires", voltTile() != voltsFromKiss);
	checkTrue("the EDT tag dim",
	          fakeTextIs(119 - 6 - gfxTextW("EDT", 1), 133, "EDT", C_DIM, C_PANEL, 1));
	checkTrue("and so does the current tile", ampTile() != ampsFromKiss);

	// Bring KISS back and the fine values must return -- the same pixels as
	// before, not merely different ones.
	tel.kissLastMs = millis();
	fakeSetTelemetry(&tel);
	frames(2);
	checkTrue("fresh KISS restores the fine reading exactly", voltTile() == voltsFromKiss);
	checkTrue("for current too", ampTile() == ampsFromKiss);

	// The merge policy itself, for completeness. These are the assertions the
	// test used to consist of.
	EscReading r;
	escMerge(&tel, tel.kissLastMs + KISS_STALE_MS, KISS_STALE_MS, EDT_STALE_MS, &r);
	checkTrue("stale KISS falls back to EDT", r.voltsFrom == EscSource::Edt);
	escMerge(&tel, tel.kissLastMs + 1, KISS_STALE_MS, EDT_STALE_MS, &r);
	checkTrue("fresh KISS is preferred", r.voltsFrom == EscSource::Kiss);
	checkInt("and carries 0.01 V resolution",
	         (long)(r.volts * 100.0f + 0.5f), 1583);
}

/**
 * @brief Telemetry blanks when the ESC stops answering.
 *
 * Reported as: readings stay on screen after the ESC is unplugged or swapped,
 * looking exactly like live data from hardware that is no longer there.
 *
 * The merge policy has its own unit tests. What this covers is the part those
 * cannot: that the screen actually goes through it, and that an expired
 * reading renders identically to one that never arrived. Anything less strict
 * -- "the region changed" -- would pass on a display that merely dimmed the
 * stale number while still showing it.
 */
static void testTelemetryExpires() {
	section("Telemetry expiry on screen");

	// The LINK tile is excluded from every comparison below. Packet rate is a
	// rolling count of the last second's frames, so on hardware it falls to
	// zero by itself -- but the fake serves one fixed snapshot forever, so it
	// stays pinned at 998 and would mask the tiles that do expire.
	// Everything else in the band is covered: both top rows, plus the status
	// tile in the bottom-left.
	auto tiles = []() {
		return fakeRegionHash(0, 128, 240, 70) ^ fakeRegionHash(0, 199, 120, 34);
	};

	// Baseline: an ESC that has never said anything.
	EscTelemetry none;
	memset(&none, 0, sizeof(none));
	fakeSetTelemetry(&none);
	frames(2);
	uint32_t blank = tiles();
	// A board that no ESC has ever answered: the packet rate and the error
	// rate read 0, which is what the manual's no-ESC picture shows.
	fakeDumpFrame("shot_tester_noesc.ppm");

	// An ESC that answers with the motor at rest, for the picture before the
	// first arm: telemetry alive, RPM 0, a small idle current.
	EscTelemetry idle;
	memset(&idle, 0, sizeof(idle));
	idle.volts = 15.80f; idle.edtVoltsMs  = millis();
	idle.amps  = 0.40f;  idle.edtAmpsMs   = millis();
	idle.tempC = 31;     idle.edtTempMs   = millis();
	idle.stress = 0;     idle.edtStressMs = millis();
	idle.edtStatusMs = millis();
	idle.lastRpmMs = millis();
	idle.packetRate = 998;
	fakeSetTelemetry(&idle);
	frames(2);
	fakeDumpFrame("shot_tester_idle.ppm");
	fakeSetTelemetry(&none);
	frames(2);

	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.erpm = 84210; tel.rpm = 12030;
	tel.volts = 15.75f; tel.edtVoltsMs  = millis();
	tel.amps  = 23.0f;  tel.edtAmpsMs   = millis();
	tel.tempC = 46;     tel.edtTempMs   = millis();
	tel.stress = 12;    tel.edtStressMs = millis();
	tel.warning = true; tel.edtStatusMs = millis();
	tel.lastRpmMs = millis();
	tel.packetRate = 998;
	fakeSetTelemetry(&tel);
	frames(2);
	uint32_t live = tiles();
	checkTrue("live telemetry differs from blank", live != blank);
	fakeDumpFrame("shot_tester_live.ppm");

	// Still inside the window. frames() advances the clock itself, so the
	// budget has to cover the ticks as well as the wait -- getting that wrong
	// is what made this test fail first time round.
	fakeAdvance(EDT_STALE_MS - 200);
	frames(2);
	checkTrue("still shown just inside the window", tiles() == live);

	// Past it: gone, and gone completely. "The region changed" would not be
	// enough -- that passes on a display that merely dims a stale number while
	// still showing it.
	fakeAdvance(250);
	frames(2);
	uint32_t stale = tiles();
	checkTrue("expired telemetry is cleared", stale != live);
	checkTrue("and is indistinguishable from never-seen", stale == blank);
	fakeDumpFrame("shot_tester_stale.ppm");

	// The two checks above compare rendered states of the same code path, so a
	// change that corrupts both identically satisfies them. It did: mapping an
	// expired status block to "OK" rather than "--" kept stale == blank and
	// live != blank, and made an ESC that had stopped talking read as all-clear
	// -- the one direction a warning indicator may not fail.
	//
	// Comparing against a *known-good* render instead is what closes that. An
	// ESC reporting OK and an ESC reporting nothing must not look the same.
	auto statusTile = []() { return fakeRegionHash(0, 199, 120, 34); };

	EscTelemetry ok;
	memset(&ok, 0, sizeof(ok));
	ok.erpm = 84210; ok.rpm = 12030;
	ok.lastRpmMs = millis();
	ok.packetRate = 998;
	ok.edtStatusMs = millis();     // a status block arrived, and it is all clear
	fakeSetTelemetry(&ok);
	frames(2);
	uint32_t statusOk = statusTile();

	fakeAdvance(EDT_STALE_MS + 100);
	frames(2);
	checkTrue("an expired status never renders as OK", statusTile() != statusOk);

	// And the same for a warning: expiring must not silently clear it either.
	EscTelemetry warn = ok;
	warn.warning = true;
	warn.edtStatusMs = millis();
	warn.lastRpmMs = millis();
	fakeSetTelemetry(&warn);
	frames(2);
	uint32_t statusWarn = statusTile();
	checkTrue("a warning looks different from all-clear", statusWarn != statusOk);
	fakeAdvance(EDT_STALE_MS + 100);
	frames(2);
	checkTrue("an expired warning does not become OK", statusTile() != statusOk);
}

/**
 * @brief MENU_STATUS_R in ui_menu.cpp: a MENU row's status ends at this x.
 *        A pixel probe of drawn text, not a control coordinate. Must match.
 */
#define MENU_STATUS_R_T 222

/**
 * @brief Whether MENU row @p r shows @p text in @p col on a @p fill fill.
 *
 * The fill is read 5 px in from the row's left edge and 8 px down, clear of
 * the label. The status is scale 1, right-aligned to MENU_STATUS_R_T, 16 px
 * below the row's top: the expected text is drawn over the one on screen and
 * the two hashes compared, so the check is on the rendered glyphs. The
 * pixels under the probe are put back afterwards.
 */
static bool menuStatusIs(const UiRect &r, const char *text, uint16_t col, uint16_t fill) {
	uint16_t *fb = gfxBuffer();
	if (fb[(r.y + 8) * GFX_W + r.x + 5] != fill) return false;
	const int w = gfxTextW(text, 1), x = MENU_STATUS_R_T - w, y = r.y + 16;
	uint16_t keep[7][GFX_W];
	for (int j = 0; j < 7; j++) memcpy(keep[j], fb + (y + j) * GFX_W + x, w * sizeof(uint16_t));
	uint32_t shown = fakeRegionHash(x, y, w, 7);
	gfxRect(x, y, w, 7, fill);
	gfxText(x, y, text, col, 1);
	bool same = fakeRegionHash(x, y, w, 7) == shown;
	for (int j = 0; j < 7; j++) memcpy(fb + (y + j) * GFX_W + x, keep[j], w * sizeof(uint16_t));
	return same;
}

/**
 * @brief Whether MENU's strip chip reads @p text on a @p fill pill: the chip
 *        is right-aligned to x 232, its text 6 px in and white.
 */
static bool menuChipIs(const char *text, uint16_t fill) {
	const int w = gfxTextW(text, 1) + 12, x = GFX_W - UI_MARGIN - w;
	return gfxBuffer()[(UI_STRIP_Y + UI_STRIP_H / 2) * GFX_W + x + 2] == fill &&
	       fakeTextIs(x + 6, UI_STRIP_Y + (UI_STRIP_H - 7) / 2, text, C_ONACCENT, fill, 1);
}

/**
 * @brief MENU's EDT chip tracks reception, and BEEP's row acknowledges a tap.
 *
 * BEEP was reported as showing no reaction. The command was going out -- the
 * ESC beeped -- but nothing on screen moved, because the command is over in
 * about 6 ms and the UI repaints at 40 Hz. The acknowledgement is on the row
 * now: for 350 ms after a tap its status reads SENT on an ink fill, or
 * REFUSED on an amber fill.
 *
 * The EDT chip on the strip is read-only. There is no enable button any more:
 * the firmware sends one to each ESC as it appears, so the control was for
 * something already handled. What remains is worth showing, because "green"
 * and "all four telemetry tiles read --" are the same fact.
 */
static void testMenuBeepRow() {
	section("MENU: the EDT chip and BEEP's row");

	feedLiveTelemetry();
	tapId(UI_FIX(TesterCtl::Menu));          // into MENU (this force-disarms)
	frames(2);
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);
	const UiRect beep = ctlRect(menuRow(&UI_BEEP));
	auto beepRow = [&]() { return fakeRegionHash(beep.x, beep.y, beep.w, beep.h); };
	uint32_t chipOn = fakeRegionHash(120, UI_STRIP_Y, 120, UI_STRIP_H);
	uint32_t idle   = beepRow();
	checkTrue("BEEP's row reads DISARMED on the panel fill",
	          menuStatusIs(beep, "DISARMED", C_DIM, C_PANEL));
	fakeDumpFrame("shot_menu_edt_on.ppm");

	checkTrue("EDT frames: the chip reads EDT ON, green", menuChipIs(UI_MENU_EDT_ON, C_GREEN));

	// Letting telemetry expire has to change the chip, or the colour is
	// decoration rather than a readout.
	EscTelemetry none;
	memset(&none, 0, sizeof(none));
	fakeSetTelemetry(&none);
	frames(2);
	checkTrue("EDT chip changes when telemetry stops",
	          fakeRegionHash(120, UI_STRIP_Y, 120, UI_STRIP_H) != chipOn);
	// No ESC is the normal state of a bench with nothing plugged in: no
	// alarm red, a dim NO ESC.
	checkTrue("no eRPM: the chip reads NO ESC, dim", menuChipIs(UI_MENU_NO_ESC, C_GRID));
	checkTrue("and not EDT OFF in red", !menuChipIs(UI_MENU_EDT_OFF, C_RED));
	fakeDumpFrame("shot_menu_no_esc.ppm");

	// An ESC that answers eRPM and sends no EDT: that is the red state.
	EscTelemetry rpmOnly;
	memset(&rpmOnly, 0, sizeof(rpmOnly));
	rpmOnly.erpm = 84210;
	rpmOnly.lastRpmMs = millis();
	rpmOnly.packetRate = 998;
	fakeSetTelemetry(&rpmOnly);
	frames(2);
	checkTrue("eRPM without EDT: the chip reads EDT OFF, red", menuChipIs(UI_MENU_EDT_OFF, C_RED));
	fakeDumpFrame("shot_menu_edt_off.ppm");
	// 500 ms (ESC_LINK_STALE_MS) after the last eRPM frame it is NO ESC again.
	rpmOnly.lastRpmMs = millis();
	fakeSetTelemetry(&rpmOnly);
	frameAfter(ESC_LINK_STALE_MS - 1);
	checkTrue("eRPM 499 ms old: still EDT OFF", menuChipIs(UI_MENU_EDT_OFF, C_RED));
	frameAfter(1);
	checkTrue("eRPM 500 ms old: NO ESC", menuChipIs(UI_MENU_NO_ESC, C_GRID));
	// The widest text the chip shows keeps clear of the strip's caption,
	// `SIG GP1  AUX GP0  DSHOT1200` at its widest on this board, whose
	// signal pins are GP0 and GP1: 27 characters from x 8.
	checkTrue("the widest chip clears the widest caption",
	          UI_MARGIN + gfxTextW("SIG GP1  AUX GP0  DSHOT1200", 1) + 4 <=
	              GFX_W - UI_MARGIN - gfxTextW(UI_MENU_EDT_OFF, 1) - 12);
	checkTrue("and the caption while DShot restarts",
	          UI_MARGIN + gfxTextW("DSHOT RESTARTING ON SIG GP1", 1) + 4 <=
	              GFX_W - UI_MARGIN - gfxTextW(UI_MENU_EDT_OFF, 1) - 12);
	feedLiveTelemetry();
	frames(2);
	checkTrue("and changes back when it returns",
	          fakeRegionHash(120, UI_STRIP_Y, 120, UI_STRIP_H) == chipOn);

	// The chip is not a button. Tapping it must not be mistaken for one. It
	// lives on the strip under the title, not in the header: the header's
	// right-hand end is BACK's on every screen. The chip is drawn on the
	// strip, not a control, so the tap is a point.
	int beepBefore = fakeBeepRequests();
	tap(200, UI_STRIP_Y + UI_STRIP_H / 2);
	checkInt("tapping the chip does nothing", fakeBeepRequests(), beepBefore);

	// BEEP is a row of the list: it fires on release.
	pressId(menuRow(&UI_BEEP)); fakeRelease(); frames(1);
	checkInt("tapping BEEP sends the command", fakeBeepRequests(), beepBefore + 1);
	checkTrue("an action row opens nothing", uiCurrentScreen() == UiScreenId::Menu);
	uint32_t lit = beepRow();
	checkTrue("and the row acknowledges it", lit != idle);
	checkTrue("SENT, on the ink fill", menuStatusIs(beep, "SENT", C_PAPER, C_INK));
	fakeDumpFrame("shot_menu_beep_flash.ppm");

	// 350 ms from the frame of the release: still lit at 349 ms, gone at 350.
	frameAfter(349);
	checkTrue("the flash holds for 349 ms", menuStatusIs(beep, "SENT", C_PAPER, C_INK));
	frameAfter(1);
	checkTrue("and clears itself at 350 ms", beepRow() == idle);

	// Its far edge, the row's last pixel column, x 231, still hits.
	fakePress(beep.x + beep.w - 1, beep.y + beep.h / 2); frames(1); fakeRelease(); frames(1);
	checkInt("BEEP's last pixel is BEEP", fakeBeepRequests(), beepBefore + 2);
	fakeAdvance(400); frames(2);

	// Refusal. Not reachable by hand -- opening MENU force-disarms and there
	// is no ARM control on that screen -- so this drives the arm state
	// directly. It is tested because the request can refuse, not because a
	// user can currently make it.
	escSetArmed(true);
	pressId(menuRow(&UI_BEEP)); fakeRelease(); frames(1);
	uint32_t refused = beepRow();
	checkTrue("a refused command looks different from an accepted one",
	          refused != lit && refused != idle);
	checkTrue("REFUSED, on the amber fill", menuStatusIs(beep, "REFUSED", C_PAPER, C_AMBER));
	checkInt("and nothing was sent", fakeBeepRequests(), beepBefore + 2);
	fakeDumpFrame("shot_menu_beep_refused.ppm");
	escSetArmed(false);
	fakeAdvance(400); frames(2);
	checkTrue("the refusal clears too", beepRow() == idle);

	tapId(UI_FIX(UI_BACK));               // BACK, in the header
	frames(2);
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}


// The note line on SETTINGS (NOTE_Y in ui_settings.cpp) is drawn text, not a
// control. The tests below probe its pixels, so its y stays a number.
#define SETTINGS_NOTE_Y 254

/** @brief Walk from the main screen into MENU, then into SETTINGS. */
static void enterSettings() {
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(menuRow(&UI_SETTINGS));
	frames(2);
	checkTrue("SETTINGS opens from MENU", uiCurrentScreen() == UiScreenId::Settings);
}

/**
 * @brief Scroll SETTINGS until the row of @p id is whole, then tap @p id.
 *
 * The viewport, y 40 to 239, shows the first rows only, so a row further
 * down is scrolled into view first; a row already whole is tapped at once.
 */
static void tapRowId(UiControlId id, IdExpect want = IdExpect::Enabled) {
	revealRow(id.row);
	tapId(id, want);
}

/**
 * @brief Whether SETTINGS row @p row, scrolled whole into view, reads
 *        @p label at y+18, its value @p value in @p col at scale 2 and, under
 *        the value, the connector line @p sub in C_DIM at y+27, both ending at
 *        x 132; with @p sub null, nothing under the value. The value is at
 *        y+8 on a row that has a connector line, OWN's empty one included,
 *        and centred, at y+15, on a row that has none (@p subRow false).
 */
static bool settingsRowShows(int row, const char *label, const char *value, uint16_t col,
                             const char *sub, bool subRow = true) {
	revealRow(row);
	UiRect r = ctlRect(UI_ROW(row, Row));
	if (r.h != 44) return false;
	if (!fakeTextIs(UI_MARGIN, r.y + 18, label, C_DIM, C_BG, 1)) return false;
	// Nothing more of the label: blank for two character cells after it, on
	// its rows under a raised value's (y+22 to y+24).
	const uint16_t *lb = gfxBuffer();
	for (int y = r.y + 22; y < r.y + 25; y++)
		for (int x = UI_MARGIN + gfxTextW(label, 1); x < UI_MARGIN + gfxTextW(label, 1) + 12; x++)
			if (lb[y * GFX_W + x] != C_BG) return false;
	if (!fakeTextIs(132 - gfxTextW(value, 2), r.y + (subRow ? 8 : 15), value, col, C_BG, 2)) return false;
	if (sub) return fakeTextIs(132 - gfxTextW(sub, 1), r.y + 27, sub, C_DIM, C_BG, 1);
	const uint16_t *fb = gfxBuffer();
	// Under the label's rows (to y+24) and the value's (to y+21, or y+28
	// centred), to the row's foot.
	for (int y = r.y + (subRow ? 25 : 29); y < r.y + 44; y++)
		for (int x = 60; x < 133; x++)
			if (fb[y * GFX_W + x] != C_BG) return false;
	return true;
}

/**
 * @brief Pin @p name ("GP0") of connector @p conn ("J5") in the board header's
 *        connector table, read as tools/make_wiring.py reads it; 0 for none.
 *        The suite runs in test/, so the header is ../src/.
 */
static int boardConnectorPin(const char *conn, const char *name) {
	FILE *f = fopen("../src/board_rp2350_touch_lcd_2_8.h", "r");
	if (!f) return 0;
	char line[256], cur[8] = "";
	int found = 0;
	while (!found && fgets(line, sizeof(line), f)) {
		const char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p != '*') { cur[0] = '\0'; continue; }
		p++;
		while (*p == ' ') p++;
		char c[8]; int count, used = 0;
		if (sscanf(p, "%7[J0-9] (%dP):%n", c, &count, &used) == 2 && used > 0 && c[0] == 'J') {
			snprintf(cur, sizeof(cur), "%s", c);
			p += used;
		} else if (!(cur[0] && p[0] >= '0' && p[0] <= '9' && strchr(p, '='))) {
			cur[0] = '\0';
			continue;
		}
		if (strcmp(cur, conn) != 0) continue;
		int num, n;
		char nm[16];
		while (sscanf(p, " %d=%15s%n", &num, nm, &n) == 2) {
			if (strcmp(nm, name) == 0) { found = num; break; }
			p += n;
		}
	}
	fclose(f);
	return found;
}

/**
 * @brief SIG PIN's and AUX PIN's J5 lines and SERVO SUPPLY's J4 line agree
 *        with the board header's connector table.
 */
static void testSettingsConnectorLines() {
	section("SETTINGS: the connector lines agree with the board header");
	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();
	for (uint8_t g = 0; g < 2; g++) {
		settings()->dshotPin = g;
		settings()->kissPin = (uint8_t)(1 - g);
		tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));
		tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));   // a repaint
		char gp[8], sub[16], what[64];
		snprintf(gp, sizeof(gp), "GP%u", (unsigned)g);
		int j5 = boardConnectorPin("J5", gp);
		snprintf(sub, sizeof(sub), "J5 PIN %d", j5);
		snprintf(what, sizeof(what), "SIG on %s: %s, as the header's J5 says", gp, sub);
		checkTrue(what, j5 > 0 && settingsRowShows((int)SettingsRowId::Sig, "SIG PIN", gp, C_TEXT, sub));
		char agp[8], asub[16];
		snprintf(agp, sizeof(agp), "GP%u", (unsigned)(1 - g));
		int aj5 = boardConnectorPin("J5", agp);
		snprintf(asub, sizeof(asub), "J5 PIN %d", aj5);
		snprintf(what, sizeof(what), "AUX on %s: %s", agp, asub);
		checkTrue(what, aj5 > 0 && settingsRowShows((int)SettingsRowId::Aux, "AUX PIN", agp, C_TEXT, asub));
	}
	for (uint8_t mode = 1; mode <= SERVO_SUPPLY_MODE_MAX; mode++) {
		settings()->servoSupply = mode;
		tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));
		tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));
		char gp[8], sub[16], what[64];
		snprintf(gp, sizeof(gp), "GP%u", (unsigned)servoSupplyPinOf(mode));
		int j4 = boardConnectorPin("J4", gp);
		snprintf(sub, sizeof(sub), "J4 PIN %d", j4);
		snprintf(what, sizeof(what), "SERVO SUPPLY %s: %s, as the header's J4 says", gp, sub);
		checkTrue(what, j4 > 0 &&
		          settingsRowShows((int)SettingsRowId::ServoSupply, "SERVO SUPPLY", gp, C_TEXT, sub));
	}
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	fakeFlashClear();
	settingsLoad();
}

/**
 * @brief Drag SETTINGS' list by @p dy px: a press at x 60, on a label or a
 *        caption, which is no control, so the list scrolls.
 */
static void settingsDrag(int dy) {
	const int x = 60, y0 = dy < 0 ? 230 : 50;
	fakePress(x, y0); frames(1);
	for (int k = 1; k <= 8; k++) { fakeHold(x, y0 + dy * k / 8); frames(1); }
	fakeRelease(); frames(1);
}

/**
 * @brief The SETTINGS screen: wiring changes reach the pump, and only legal
 *        values can be selected.
 *
 * The last part is the one worth having. A pin picker whose buttons can land on
 * an occupied GPIO needs an error state, a message, and a rule about what
 * happens to a pin you cannot use; one that steps through the board's free mask
 * needs none of those, because the invalid selection cannot be made. Asserting
 * it here is what keeps that true when the mask changes.
 */
static void testSettingsWiring() {
	section("SETTINGS: wiring");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	uint8_t start = settings()->dshotPin;
	checkTrue("entering SETTINGS does not disturb the pin",
	          escTaskDshotPin() == start);
	checkControlTable("SETTINGS");

	// One step first: the board offers exactly two free pins, so an even
	// number of steps lands back where it started and nothing would be dirty.
	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	checkTrue("one step moves the pin", settings()->dshotPin != start);
	checkTrue("changing the pin is unsaved", settingsDirty());
	checkTrue("the change reached the DShot pump",
	          escTaskDshotPin() == settings()->dshotPin);

	// Then step all the way round and check every stop is legal.
	bool allFree = true;
	for (int i = 0; i < 6; i++) {
		tapId(UI_ROW(SettingsRowId::Sig, Plus));
		if (!settingsPinFree(settings()->dshotPin)) allFree = false;
		tapId(UI_ROW(SettingsRowId::Sig, Minus));
		if (!settingsPinFree(settings()->dshotPin)) allFree = false;
	}
	checkTrue("every step lands on a free GPIO", allFree);

	// The frame for the docs, at 0 px: the first rows and an unsaved change,
	// with the ESC pin back on the boot pin (test_menu.cpp takes the frame
	// with the pin's warning).
	tapId(UI_ROW(SettingsRowId::Sig, Minus));
	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	fakeDumpFrame("shot_settings.ppm");
	tapId(UI_ROW(SettingsRowId::Poles, Minus));
	tapId(UI_ROW(SettingsRowId::Sig, Plus));

	// Bitrate halves and doubles through the four legal values, and wraps.
	// DSHOT's row is cut by the viewport's bottom at 0 px, 10 px of it
	// showing, so it is scrolled into view first.
	settings()->dshotKbaud = 600;
	tapRowId(UI_ROW(SettingsRowId::Speed, Minus));
	checkInt("minus steps the bitrate down", settings()->dshotKbaud, 300);
	tapRowId(UI_ROW(SettingsRowId::Speed, Plus));
	tapRowId(UI_ROW(SettingsRowId::Speed, Plus));
	checkInt("plus steps it up", settings()->dshotKbaud, 1200);
	tapRowId(UI_ROW(SettingsRowId::Speed, Plus));
	checkInt("and wraps rather than sticking", settings()->dshotKbaud, 150);
	checkInt("the pump was told", fakeDshotKbaud(), 150);
}

/**
 * @brief Whether the status bar's DShot label reads @p text in @p colour.
 *
 * Draws the expected label over the one on screen and compares the two
 * hashes, so the check is on the rendered glyphs rather than on a string the
 * UI meant to draw. The rectangle is the widest label, `DSHOT1200`, at scale 1.
 */
static bool statusLabelIs(const char *text, uint16_t colour) {
	const int x = 104, y = 3, w = 53, h = 7;
	uint32_t shown = fakeRegionHash(x, y, w, h);
	gfxRect(x, y, w, h, C_PANEL);
	gfxText(x, y, text, colour, 1);
	return fakeRegionHash(x, y, w, h) == shown;
}

/**
 * @brief The status bar names the bitrate the pump runs at.
 *
 * SETTINGS changes the bitrate at run time, so the label follows the pump, not
 * the compiled default. A pin the driver refused shows DSERR instead: nothing
 * is on the wire at any rate.
 */
static void testStatusBarBitrate() {
	section("Status bar: the DShot label follows the pump");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	fakeSetEdtRequested(false);
	feedLiveTelemetry();
	uint8_t pin = escTaskDshotPin();
	escTaskConfigure(pin, 600, false, 0);
	frames(2);
	checkTrue("DSHOT600 at 600 kbit/s", statusLabelIs("DSHOT600", C_DIM));

	escTaskConfigure(pin, 150, false, 0);
	frames(1);
	checkTrue("DSHOT150 once the pump runs at 150", statusLabelIs("DSHOT150", C_DIM));
	escTaskConfigure(pin, 300, false, 0);
	frames(1);
	checkTrue("DSHOT300 at 300", statusLabelIs("DSHOT300", C_DIM));
	escTaskConfigure(pin, 1200, false, 0);
	frames(1);
	checkTrue("DSHOT1200 at 1200", statusLabelIs("DSHOT1200", C_DIM));

	fakeSetEdtRequested(true);
	frames(1);
	checkTrue("the text colour once EDT is requested", statusLabelIs("DSHOT1200", C_TEXT));

	EscTelemetry tel;
	memset(&tel, 0, sizeof(tel));
	tel.initError = true;
	fakeSetTelemetry(&tel);
	frames(1);
	checkTrue("DSERR when the bind failed", statusLabelIs("DSERR", C_AMBER));

	// Back to the defaults, so later screenshots show what a fresh board does.
	feedLiveTelemetry();
	escTaskConfigure(pin, DSHOT_SPEED_KBAUD, false, 0);
	frames(1);
	checkTrue("and the bitrate again when it clears", statusLabelIs("DSHOT600", C_TEXT));
}

/**
 * @brief SIG PIN and AUX PIN: both live whatever KISS is, and a step of
 *        either swaps the two, so they never share a GPIO; KISS TELEM
 *        switches KISS on and off on AUX without moving a pin.
 *
 * The 2.8" board frees two pins, so every step of either row is a swap.
 * Each step is checked against the pins it started from, at once: a step up
 * and one down checked only at the end pass whether or not the row acts.
 */
static void testSettingsKiss() {
	section("SETTINGS: SIG PIN and AUX PIN swap, live whatever KISS is");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	settings()->kissEnable = 0;
	settings()->dshotPin = 0;
	settings()->kissPin = 1;
	tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));
	tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));   // a repaint, KISS off again
	checkTrue("SIG PIN: GP0, J5 PIN 2 under it",
	          settingsRowShows((int)SettingsRowId::Sig, "SIG PIN", "GP0", C_TEXT, "J5 PIN 2"));
	checkTrue("AUX PIN: GP1, J5 PIN 1 under it",
	          settingsRowShows((int)SettingsRowId::Aux, "AUX PIN", "GP1", C_TEXT, "J5 PIN 1"));
	checkTrue("DSHOT: the rate alone, nothing under it",
	          settingsRowShows((int)SettingsRowId::Speed, "DSHOT", "600", C_TEXT, nullptr, false));
	for (int i = 0; i < 2; i++) {
		uint8_t sig = settings()->dshotPin, aux = settings()->kissPin;
		tapRowId(UI_ROW(SettingsRowId::Aux, Plus));
		checkTrue("KISS off: AUX PIN + acts and swaps SIG and AUX",
		          settings()->kissPin == sig && settings()->dshotPin == aux);
		checkTrue("and DShot follows SIG", fakeWiring().dshotPin == settings()->dshotPin);
	}
	{
		uint8_t sig = settings()->dshotPin, aux = settings()->kissPin;
		tapRowId(UI_ROW(SettingsRowId::Aux, Minus));
		checkTrue("AUX PIN - swaps them too",
		          settings()->kissPin == sig && settings()->dshotPin == aux);
	}
	checkTrue("SIG on GP1: J5 PIN 1 under it",
	          settings()->dshotPin == 1 &&
	          settingsRowShows((int)SettingsRowId::Sig, "SIG PIN", "GP1", C_TEXT, "J5 PIN 1"));
	checkTrue("AUX on GP0: J5 PIN 2 under it",
	          settingsRowShows((int)SettingsRowId::Aux, "AUX PIN", "GP0", C_TEXT, "J5 PIN 2"));
	checkInt("and KISS stays off", settings()->kissEnable, 0);

	uint8_t sig0 = settings()->dshotPin, aux0 = settings()->kissPin;
	tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));
	checkInt("KISS TELEM switches KISS on", settings()->kissEnable, 1);
	checkTrue("on AUX, moving neither pin",
	          settings()->dshotPin == sig0 && settings()->kissPin == aux0);
	for (int i = 0; i < 2; i++) {
		uint8_t sig = settings()->dshotPin, aux = settings()->kissPin;
		tapRowId(UI_ROW(SettingsRowId::Sig, Minus));
		checkTrue("KISS on: SIG PIN - swaps SIG and AUX",
		          settings()->kissPin == sig && settings()->dshotPin == aux);
		checkTrue("and the wiring follows both",
		          fakeWiring().dshotPin == settings()->dshotPin &&
		          fakeWiring().kissPin == settings()->kissPin && fakeWiring().kissEnable);
		// Off the boot pin with KISS on, the line over LINK names the swap
		// of the ESC's wires, not a servo.
		bool off = settings()->dshotPin != settingsBootDshotPin();
		bool swapShown = fakeTextIs(UI_MARGIN, 230, "SWAP THE ESC'S SIGNAL AND KISS WIRES",
		                            C_AMBER, C_BG, 1);
		checkTrue(off ? "SIG off the boot pin, KISS on: SWAP THE ESC'S SIGNAL AND KISS WIRES"
		              : "SIG back on the boot pin: no swap line",
		          off == swapShown);
	}
	checkTrue("no step left a pin outside the free set",
	          settingsPinFree(settings()->dshotPin) && settingsPinFree(settings()->kissPin));
	sig0 = settings()->dshotPin;
	aux0 = settings()->kissPin;
	tapRowId(UI_ROW(SettingsRowId::Kiss, Toggle));
	checkInt("toggling again switches it off", settings()->kissEnable, 0);
	checkTrue("and moves neither pin",
	          settings()->dshotPin == sig0 && settings()->kissPin == aux0);
}

/** @brief Hold HOLD DEFAULTS until it fires, then lift. */
static void holdReset() {
	pressId(UI_FIX(SettingsCtl::Defaults));
	holdId(UI_FIX(SettingsCtl::Defaults), 60);
	fakeRelease(); frames(1);
}

/**
 * @brief SERVO SUPPLY: a core section, stepped OWN, GP28, GP29 with the J4
 *        pin under the value, applied at once, saved and reset.
 */
static void testSettingsServoSupply() {
	section("SETTINGS: SERVO SUPPLY");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	checkInt("none by default", settings()->servoSupply, 0);
	checkTrue("SERVO SUPPLY reads OWN, nothing under it",
	          settingsRowShows((int)SettingsRowId::ServoSupply, "SERVO SUPPLY", "OWN", C_TEXT, nullptr));
	tapRowId(UI_ROW(SettingsRowId::ServoSupply, Minus));
	checkInt("- at OWN stays", settings()->servoSupply, 0);
	tapRowId(UI_ROW(SettingsRowId::ServoSupply, Plus));
	checkTrue("GP28, J4 PIN 11 under it",
	          settingsRowShows((int)SettingsRowId::ServoSupply, "SERVO SUPPLY", "GP28", C_TEXT, "J4 PIN 11"));
	FakePin p = fakePin(28);
	checkTrue("+: GP28, applied at once",
	          settings()->servoSupply == 1 && servoSupplyFitted() && servoSupplyPin() == 28);
	checkTrue("and GP28 an undriven input: no claim holds",
	          p.function == GPIO_FUNC_SIO && !p.output && p.pull == FakePull::None && p.inputOff);
	tapRowId(UI_ROW(SettingsRowId::ServoSupply, Plus));
	p = fakePin(29);
	checkTrue("+ again: GP29", settings()->servoSupply == 2 && servoSupplyPin() == 29);
	checkTrue("GP29, J4 PIN 12 under it",
	          settingsRowShows((int)SettingsRowId::ServoSupply, "SERVO SUPPLY", "GP29", C_TEXT, "J4 PIN 12"));
	checkTrue("GP29 an undriven input", p.function == GPIO_FUNC_SIO && !p.output && p.inputOff);
	checkTrue("GP28 stays one", !fakePin(28).output && fakePin(28).inputOff);
	tapRowId(UI_ROW(SettingsRowId::ServoSupply, Plus));
	checkInt("+ at GP29 stays", settings()->servoSupply, 2);
	checkTrue("unsaved", settingsDirty());
	fakeDumpFrame("shot_settings_supply.ppm");
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 60);
	fakeRelease(); frames(2);
	settingsLoad();
	checkTrue("saved: a reload keeps GP29", settingsStored() && settings()->servoSupply == 2);

	holdReset();
	checkTrue("DEFAULTS: none, applied at once", settings()->servoSupply == 0 && !servoSupplyFitted());
	p = fakePin(29);
	checkTrue("GP29 left an undriven input", !p.output && p.inputOff);
	tapRowId(UI_ROW(SettingsRowId::ServoSupply, Plus));
	tapRowId(UI_ROW(SettingsRowId::ServoSupply, Minus));
	checkTrue("+ then -: none again", settings()->servoSupply == 0 && !servoSupplyFitted());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	fakeFlashClear();
	settingsLoad();
}

/** @brief Whether CURRENT SENSE's caption is @p want; false for none. */
static bool captionIs(const char *want) {
	const char *c = senseCaption();
	return c && strcmp(c, want) == 0;
}

/**
 * @brief CURRENT SENSE: in the SERVO SUPPLY section, below SERVO SUPPLY,
 *        toggled NONE and INA219, applied at once, its caption from the
 *        probe, saved and reset; nothing sent to 0x40 with NONE.
 */
static void testSettingsCurrentSense() {
	section("SETTINGS: CURRENT SENSE");

	fakeFlashClear();
	settingsLoad();
	senseConfigure(0);
	fakeI2cReset();
	uiInit();
	frames(2);
	enterSettings();

	checkInt("none by default", settings()->currentSense, 0);
	FakeIna219 *p = fakeI2cAddIna219(INA219_ADDR);
	frames(40);
	checkInt("NONE: 40 frames, nothing to 0x40", fakeI2cTransactionsTo(INA219_ADDR), 0);
	checkTrue("no caption", senseCaption() == nullptr);

	tapRowId(UI_ROW(SettingsRowId::CurrentSense, Toggle));
	checkTrue("INA219, applied at once", settings()->currentSense == 1 && senseDeclared());
	frames(1);
	checkTrue("probed: FOUND, the part on 19EFh",
	          captionIs("FOUND") && p->reg[0] == 0x19EF);
	fakeDumpFrame("shot_settings_sense.ppm");
	checkTrue("unsaved", settingsDirty());
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 60);
	fakeRelease(); frames(2);
	settingsLoad();
	checkTrue("saved: a reload keeps INA219", settingsStored() && settings()->currentSense == 1);

	fakeI2cRemove(INA219_ADDR);
	frames(5);
	checkTrue("the part gone: NOT FOUND", captionIs("NOT FOUND"));
	fakeI2cAddIna219(INA219_ADDR, 0x4127);
	frames(45);
	checkTrue("another part at 0x40: OTHER PART", captionIs("OTHER PART"));

	holdReset();
	checkTrue("DEFAULTS: NONE, applied at once", settings()->currentSense == 0 && !senseDeclared());
	uint32_t n = fakeI2cTransactionsTo(INA219_ADDR);
	frames(80);
	checkInt("and nothing sent to 0x40 after", fakeI2cTransactionsTo(INA219_ADDR), n);
	tapRowId(UI_ROW(SettingsRowId::CurrentSense, Toggle));
	tapRowId(UI_ROW(SettingsRowId::CurrentSense, Toggle));
	checkTrue("toggled twice: NONE again", settings()->currentSense == 0 && !senseDeclared());
	tapId(UI_FIX(UI_BACK));
	tapId(UI_FIX(UI_BACK));
	fakeFlashClear();
	settingsLoad();
	fakeI2cReset();
}

/** @brief Contrast and backlight take effect immediately and persist. */
static void testSettingsDisplay() {
	section("SETTINGS: high contrast");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	checkTrue("starts in the dark theme", themeGet() == Theme::Dark);
	uint16_t darkBg = C_BG;
	const uint16_t darkLime = C_LIME;

	tapRowId(UI_ROW(SettingsRowId::Contrast, Toggle));
	checkTrue("toggle switches the theme", themeGet() == Theme::HighContrast);
	checkTrue("the background inverted", C_BG != darkBg);
	checkTrue("text is now the darker of the two", C_TEXT < C_BG);
	checkTrue("strokes got heavier", themeStroke() > 1);
	checkTrue("text got heavier", themeBold());
	checkInt("backlight is forced to full", themeBacklight(100), 255);
	fakeDumpFrame("shot_settings_contrast.ppm");

	tapRowId(UI_ROW(SettingsRowId::Backlight, Minus));
	checkTrue("the preferred level still moves", settings()->backlight < LCD_BACKLIGHT_DEFAULT);
	checkInt("but full brightness still wins while high contrast is on",
	         themeBacklight(settings()->backlight), 255);

	// The screen worth checking in high contrast is not this one: it is the
	// tester, which is what you are looking at outdoors with a motor running.
	// Rendering it here is what puts it in the published preview.
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SETTINGS reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));               // BACK, out of MENU
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	// Live data in both halves of the published pair. A dark screen full of
	// numbers next to a light screen reading "--" compares two different things.
	feedLiveTelemetry();
	frames(3);
	fakeDumpFrame("shot_tester_contrast.ppm");
	{
		// Every colour on the tester follows the theme: no pixel keeps a
		// dark-theme colour that the high-contrast palette does not also
		// hold. A function-level static colour table, filled on the first
		// draw, would keep the ESC STATUS tile in dark lime here.
		const uint16_t *hc = g_theme;
		bool inHc = false;
		for (int i = 0; i < TC_COUNT; i++) inHc = inHc || hc[i] == darkLime;
		int stale = 0;
		const uint16_t *fb = gfxBuffer();
		for (int i = 0; i < GFX_W * GFX_H; i++) stale += fb[i] == darkLime;
		checkTrue("dark lime is not a high-contrast colour", !inHc);
		checkInt("no pixel keeps the dark theme's lime", stale, 0);
	}
	enterSettings();

	tapRowId(UI_ROW(SettingsRowId::Contrast, Toggle));
	checkTrue("toggling back restores the dark theme", themeGet() == Theme::Dark);
	checkInt("and the preferred backlight level",
	         themeBacklight(settings()->backlight), settings()->backlight);
	checkInt("dark theme draws single-pixel frames", themeStroke(), 1);
}

/**
 * @brief SAVE needs a full second held on the button, and nothing less.
 *
 * Same interlock as the AM32 write, for a weaker reason and a real one: a flash
 * erase parks core1, which stops the DShot pump. It is a thing to do on purpose.
 */
static void testSettingsSave() {
	section("SETTINGS: hold to save");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	// The progress bar sits 4 px above SAVE, as wide as the button. It is
	// drawn, not a control, so the probes read its pixels.
	const UiRect save = ctlRect(UI_FIX(SettingsCtl::Save));
	const int barX = save.x, barY = save.y - 4, barW = save.w;

	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	checkTrue("a change is pending", settingsDirty());

	// A tap is not a save.
	tapId(UI_FIX(SettingsCtl::Save));
	checkTrue("a tap does not commit", settingsDirty());
	checkTrue("and nothing was stored", !settingsStored());

	// Releasing early does not commit either.
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 20);
	fakeRelease(); frames(1);
	checkTrue("half a second does not commit", settingsDirty());

	// Moving off the button mid-hold cancels.
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 20);
	holdId(UI_FIX(SettingsCtl::Defaults), 40);
	fakeRelease(); frames(1);
	checkTrue("sliding off cancels the hold", settingsDirty());

	// The bar has to actually appear while holding, or the hold is invisible.
	uint32_t idle = fakeRegionHash(barX, barY, barW, 3);
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 20);
	checkTrue("the hold draws a progress bar",
	          fakeRegionHash(barX, barY, barW, 3) != idle);
	{
		const uint16_t *fb = gfxBuffer() + barY * GFX_W;
		checkTrue("500 ms: 75 px of the 150, from the button's left edge",
		          fb[barX] == C_LIME && fb[barX + 74] == C_LIME &&
		          fb[barX + 75] == C_BG && fb[barX - 1] == C_BG);
		checkTrue("and the button is red while held, as every hold",
		          gfxBuffer()[(save.y + save.h - 5) * GFX_W + barX + 4] == C_RED);
	}
	fakeRelease(); frames(2);
	checkTrue("and it clears on release",
	          fakeRegionHash(barX, barY, barW, 3) == idle);

	// A full second does, and not a millisecond less.
	uint8_t want = settings()->dshotPin;
	int sx, sy;
	ctlCentre(UI_FIX(SettingsCtl::Save), &sx, &sy);
	pressId(UI_FIX(SettingsCtl::Save));
	fakeHold(sx, sy); frameAfter(999);
	checkTrue("held 999 ms: not saved", settingsDirty() && !settingsStored());
	fakeHold(sx, sy); frameAfter(1);
	checkTrue("a one-second hold commits", !settingsDirty());
	checkTrue("and reports a stored block", settingsStored());

	// Keeping the finger down must not save again a second later: each save is
	// a flash erase, and a long press is not a request for several.
	settings()->poles = (uint8_t)(settings()->poles == 12 ? 14 : 12);
	holdId(UI_FIX(SettingsCtl::Save), 120);
	checkTrue("holding on does not save again", settingsDirty());
	fakeRelease(); frames(2);

	// Survives a reload, which is the only thing the user cares about.
	settings()->dshotPin = 0;
	settingsLoad();
	checkInt("the pin came back after a reload", settings()->dshotPin, want);

	// DEFAULTS restores compiled defaults into the live copy without touching flash.
	Settings def;
	settingsDefaults(&def);
	holdReset();
	checkInt("DEFAULTS restores the compiled ESC pin", settings()->dshotPin, def.dshotPin);
	checkTrue("DEFAULTS alone leaves flash as it was", settingsStored());

	// SETTINGS opened again under a press that is on HOLD TO SAVE: the press
	// is not timed, as the hold waits for a press that lands.
	checkTrue("DEFAULTS left a change pending", settingsDirty());
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 20);
	uiSettingsEnter();
	holdId(UI_FIX(SettingsCtl::Save), 60);
	checkTrue("SETTINGS entered mid-press: held 1500 ms after, not saved", settingsDirty());
	fakeRelease(); frames(2);

	// BACK returns to MENU, whose SETTINGS row reads UNSAVED.
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SETTINGS reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	// MENU opens at 0 px: the SETTINGS row, the sixth, is whole and its
	// status shows with no drag.
	checkInt("MENU's SETTINGS row is whole at 0 px, no drag",
	         ctlRect(UI_ROW(uiMenuRowOf(&UI_SETTINGS), Row)).h, 40);
	checkTrue("MENU's SETTINGS row reads UNSAVED in amber",
	          menuStatusIs(ctlRect(menuRow(&UI_SETTINGS)), "UNSAVED", C_AMBER, C_PANEL));
	fakeDumpFrame("shot_menu_unsaved.ppm");
}


/**
 * @brief Sliding a finger off a button before lifting cancels it.
 *
 * The escape hatch. Every screen but the ARM button used to commit on
 * touch-down, which means a mis-tap has already happened by the time you see
 * it -- and on glass there is no travel to warn you first.
 */
static void testTapCancels() {
	section("Touch: a tap can be cancelled by sliding off");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);

	// LATCH is observable through the throttle gauge: with it on, the bar latches
	// instead of springing back. Arm, prove it springs back, then try to turn
	// LATCH on by a press that slides away -- it must still spring back.
	holdToArm();
	swipe(20, 160, thrY(), 8);
	frames(2);
	checkInt("spring mode: released throttle returns to zero", fakeThrottle(), 0);

	int hx, hy, ax, ay;
	ctlCentre(UI_FIX(TesterCtl::Latch), &hx, &hy);
	ctlCentre(UI_FIX(TesterCtl::Arm), &ax, &ay);
	fakePress(hx, hy); frames(2);
	fakeHold(ax, ay);  frames(2);   // slide off, onto ARM
	fakeRelease();     frames(2);
	swipe(20, 160, thrY(), 8);
	frames(2);
	checkInt("sliding off LATCH never turned it on", fakeThrottle(), 0);

	// The same press, released in place, does toggle it.
	tapId(UI_FIX(TesterCtl::Latch));
	swipe(20, 160, thrY(), 8);
	frames(2);
	checkTrue("released in place, LATCH latched the throttle", fakeThrottle() > 0);
	tapId(UI_FIX(TesterCtl::Latch));       // back off, which zeroes it
	tapId(UI_FIX(TesterCtl::Menu));       // into MENU for the rest
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);

	// MENU: press BEEP's row, slide off it, release. Neither fires.
	int beeps = fakeBeepRequests();
	int bx, by;
	ctlCentre(menuRow(&UI_BEEP), &bx, &by);
	fakePress(bx, by);      frames(2);
	fakeHold(bx, by + 60);  frames(2);
	fakeRelease();          frames(2);
	checkInt("sliding off BEEP sent nothing", fakeBeepRequests(), beeps);

	// Still on MENU, which the next tap proves by working: the row the
	// finger slid onto is only hit-tested, so if the slide had activated
	// anything this fails.
	fakePress(bx, by);      frames(2);
	fakeRelease();          frames(2);
	checkInt("released in place, it fires", fakeBeepRequests(), beeps + 1);
	checkTrue("so the screen never left", uiCurrentScreen() == UiScreenId::Menu);
}

/** @brief A held stepper repeats and accelerates; a tap moves exactly one step. */
static void testStepperRepeat() {
	section("Touch: hold-to-repeat on the SETTINGS steppers");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	// One tap, one step. The delay before repeating is what makes this true.
	uint16_t start = settings()->maxThrottle;
	tapId(UI_ROW(SettingsRowId::MaxThrottle, Plus));
	checkInt("a tap moves exactly one step",
	         settings()->maxThrottle, start + MAX_THROTTLE_STEP);

	// Held: many steps, and the whole range is reachable without lifting.
	start = settings()->maxThrottle;
	pressId(UI_ROW(SettingsRowId::MaxThrottle, Plus));
	holdId(UI_ROW(SettingsRowId::MaxThrottle, Plus), 200);
	fakeRelease(); frames(1);
	checkTrue("holding walks the ceiling up", settings()->maxThrottle > start);
	checkInt("and reaches the top without lifting",
	         settings()->maxThrottle, MAX_THROTTLE_CEILING);
	checkTrue("never past it", settings()->maxThrottle <= MAX_THROTTLE_CEILING);

	// And back down, without underflowing past one step.
	pressId(UI_ROW(SettingsRowId::MaxThrottle, Minus));
	holdId(UI_ROW(SettingsRowId::MaxThrottle, Minus), 300);
	fakeRelease(); frames(1);
	checkInt("and back down to one step, never below",
	         settings()->maxThrottle, MAX_THROTTLE_STEP);

	// Poles stay even throughout, which is what the eRPM maths assumes.
	pressId(UI_ROW(SettingsRowId::Poles, Plus));
	bool even = true;
	for (int i = 0; i < 120; i++) {
		holdId(UI_ROW(SettingsRowId::Poles, Plus), 1);
		if (settings()->poles & 1) even = false;
	}
	fakeRelease(); frames(1);
	checkTrue("pole count stays even while repeating", even);
	checkTrue("and inside its range",
	          settings()->poles >= MIN_MOTOR_POLES && settings()->poles <= MAX_MOTOR_POLES);
}

/** @brief The repeat rule itself, away from any screen. */
static void testRepeatRule() {
	section("Touch: the repeat rule");

	Repeat r{};
	checkTrue("fires once immediately on press", repeatFires(&r, +1, 1000));
	checkTrue("then pauses", !repeatFires(&r, +1, 1100));
	checkTrue("still paused just before the delay",
	          !repeatFires(&r, +1, 1000 + REPEAT_DELAY_MS));
	checkTrue("repeats after the delay", repeatFires(&r, +1, 1000 + REPEAT_DELAY_MS + 130));

	// Accelerates: the gap between steps shrinks the longer it is held.
	checkInt("slow tier", repeatInterval(500), 120);
	checkInt("faster after 1.2 s", repeatInterval(1500), 60);
	checkInt("fastest after 2.5 s", repeatInterval(3000), 25);

	// Reversing restarts the acceleration rather than carrying it over.
	Repeat r2{};
	repeatFires(&r2, +1, 0);
	for (uint32_t t = 100; t < 4000; t += 20) repeatFires(&r2, +1, t);
	checkTrue("reversing fires at once", repeatFires(&r2, -1, 4000));
	checkTrue("and starts slow again", !repeatFires(&r2, -1, 4100));

	// Letting go resets.
	checkTrue("release is not a step", !repeatFires(&r2, 0, 4200));
	checkTrue("and the next press fires immediately", repeatFires(&r2, -1, 4300));
}

/** @brief Arming and disarming dip the backlight, so the panel announces them. */
static void testArmBacklightDip() {
	section("Touch: the panel announces arm and disarm");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(6);
	uint8_t resting = fakeBacklight();
	checkTrue("resting at the configured level", resting > 0);

	fakeBacklightResetMin();
	holdToArm();
	frames(1);
	checkTrue("arming dips the panel", fakeBacklightMin() < resting);

	frames(20);   // the dip is short
	checkInt("and comes back", fakeBacklight(), resting);

	fakeBacklightResetMin();
	tapId(UI_FIX(TesterCtl::Arm));          // DISARM fires on press
	frames(1);
	checkTrue("no longer armed", !uiArmed());
	checkTrue("disarming dips it too", fakeBacklightMin() < resting);
	frames(20);
	checkInt("and comes back again", fakeBacklight(), resting);

	fakeAdvance(0x80000000u);
	fakeBacklightResetMin();
	frames(2);
	checkInt("2^31 ms after the dip the panel is not dimmed", fakeBacklightMin(), resting);
}


/**
 * @brief The interlocks that stand between a tap and a spinning motor.
 *
 * Every check here corresponds to a mutation that used to survive the entire
 * suite: shortening ARM_HOLD_MS to a millisecond, forcing the zero-throttle
 * precondition true, dropping the check that the press began on the button, and
 * deleting the disarmed-throttle backstop outright.
 */
static void testArmInterlocks() {
	section("Safety: arming takes a deliberate hold");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);

	// A tap is not a hold. Nothing in the suite asserted this, which is how
	// ARM_HOLD_MS could be reduced to 1 ms without a single check noticing.
	tapId(UI_FIX(TesterCtl::Arm));
	checkTrue("a tap does not arm", !uiArmed());
	checkTrue("nor does the ESC think so", !fakeArmed());

	// Nor is most of a hold.
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 30);
	checkTrue("750 ms of holding does not arm", !uiArmed());
	fakeRelease(); frames(2);
	checkTrue("and releasing early does not either", !uiArmed());

	// Sliding onto the button from elsewhere must not arm, however long it is
	// then held: the press has to have begun on it.
	// The press begins 40 px left of MENU's centre and 60 px above ARM's,
	// which is no button.
	int cx, cy, ax, ay;
	ctlCentre(UI_FIX(TesterCtl::Menu), &cx, &cy);
	ctlCentre(UI_FIX(TesterCtl::Arm), &ax, &ay);
	fakePress(cx - 40, ay - 60); frames(1);
	holdId(UI_FIX(TesterCtl::Arm), 80);
	checkTrue("sliding onto ARM and holding does not arm", !uiArmed());
	fakeRelease(); frames(2);

	// The real thing does.
	holdToArm();
	checkTrue("a full hold arms", uiArmed());
	checkTrue("and the ESC is told", fakeArmed());
}

/**
 * @brief DISARM. Never once pressed by the suite before.
 *
 * Every path out of the armed state went through MENU, so the incidental disarm
 * was covered and the deliberate one was not: replacing the button's body with
 * nothing, or inverting it so that pressing DISARM re-armed, both passed.
 */
static void testDisarmButton() {
	section("Safety: the DISARM button");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();
	checkTrue("armed to begin with", uiArmed());

	// Put some throttle on and make it stay, so the zeroing is observable.
	// LATCH latches it; in spring mode the release at the end of a swipe would
	// have already returned it to zero and the check below would prove nothing.
	tapId(UI_FIX(TesterCtl::Latch));
	swipe(20, 120, thrY(), 8);
	frames(2);
	checkTrue("throttle is latched up", fakeThrottle() > 0);

	// Disarm fires on press, not release: this is the one control that must not
	// wait for a lift.
	pressId(UI_FIX(TesterCtl::Arm));
	checkTrue("disarms on press, without waiting for release", !uiArmed());
	fakeRelease(); frames(2);

	checkTrue("the ESC is told", !fakeArmed());
	checkInt("and the commanded throttle is zeroed", fakeThrottle(), 0);
	checkInt("the UI agrees", uiThrottle(), 0);

	// LATCH is dropped too, so re-arming cannot resume a latched throttle.
	holdToArm();
	swipe(20, 120, thrY(), 8);
	frames(2);
	checkInt("re-arming does not restore a latched throttle", fakeThrottle(), 0);
}

/**
 * @brief HOLD TO ARM times only a press that landed on it while arming was
 *        allowed.
 *
 * Two presses that began while arming was refused: the press that disarms,
 * kept on the button, and a press made while a USB host holds the card,
 * kept on the button after the host lets go. Neither arms, however long it
 * stays.
 */
static void testArmNeedsLivePress() {
	section("Safety: HOLD TO ARM times only a press that landed while it was live");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();
	checkTrue("armed to begin with", uiArmed());
	pressId(UI_FIX(TesterCtl::Arm));
	checkTrue("the press disarms", !uiArmed());
	holdId(UI_FIX(TesterCtl::Arm), 80);
	checkTrue("the press that disarmed, held 2000 ms, does not arm again", !uiArmed() && !fakeArmed());
	fakeRelease(); frames(2);
	holdToArm();
	checkTrue("a new press arms", uiArmed());
	pressId(UI_FIX(TesterCtl::Arm));
	fakeRelease(); frames(2);

	fakeMscSet(MscState::Serving, 0);
	pressId(UI_FIX(TesterCtl::Arm), IdExpect::Disabled);
	holdId(UI_FIX(TesterCtl::Arm), 10);
	fakeMscSet(MscState::Idle, 0);
	holdId(UI_FIX(TesterCtl::Arm), 80);
	checkTrue("a press made while a host held the card, held 2000 ms after, does not arm",
	          !uiArmed() && !fakeArmed());
	fakeRelease(); frames(2);
	holdToArm();
	checkTrue("a new press once the card is back arms", uiArmed());
	pressId(UI_FIX(TesterCtl::Arm));
	fakeRelease(); frames(2);
}

/**
 * @brief Both rails are reachable without a finger on the panel's rim.
 *
 * Reported from a 2" board: a ceiling set to 20 % that would only ever reach
 * 19 %, and 60 % that stopped at 58 %. The gauge mapped finger position
 * straight onto the drawn track, so full throttle lived in the last pixel
 * column -- under the case rim, where a fingertip does not go.
 *
 * The test therefore drives the furthest x a finger actually reaches, not the
 * furthest the hit-test would accept. Reverting @ref THR_EDGE_SAT to zero puts
 * the numbers back to 96 % and 4 % of the ceiling and fails both ends.
 */
static void testThrottleRailsAreReachable() {
	section("Throttle: both rails are reachable short of the rim");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();

	const int ceiling = (int)settings()->maxThrottle;

	// The track is drawn x 8..232. The report puts a fingertip's limit around
	// x=221; 224 is generous to the old behaviour and still clear of the end.
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	const int ty = thr.y + thr.h / 2;
	const int right = thr.x + thr.w - 8;   // 224
	fakePress(right, ty); frames(1);
	fakeHold(right, ty);  frames(1);
	checkInt("a finger short of the right rim reads the ceiling",
	         fakeThrottle(), ceiling);

	// The same at the other end, which had the fault mirrored: zero was only
	// available in the leftmost columns, so releasing was the only way to stop.
	fakeHold(thr.x + thr.w / 2, ty); frames(1);
	checkTrue("mid-track is neither rail",
	          fakeThrottle() > 0 && fakeThrottle() < ceiling);
	fakeHold(thr.x + 10, ty); frames(1);    // 18
	checkInt("a finger short of the left rim reads zero", fakeThrottle(), 0);

	// The span between the rails still scales, rather than the gauge having
	// become a three-position switch: the centre is half the ceiling.
	fakeHold(thr.x + THR_EDGE_SAT_T + (thr.w - 2 * THR_EDGE_SAT_T) / 2, ty);
	frames(1);
	checkTrue("and the centre of the usable span is half the ceiling",
	          fakeThrottle() > ceiling * 45 / 100 &&
	          fakeThrottle() < ceiling * 55 / 100);

	fakeRelease(); frames(2);
	checkInt("released, it springs back to zero", fakeThrottle(), 0);
}

/**
 * @brief The taller bar stops exactly where the button row starts.
 *
 * The gauge grew from 26 px to 34 by taking the padding above the buttons, and
 * the grab slop below it had to shrink to 1 px to pay for that. Slop reaching
 * into the button row would make the top of DISARM a throttle surface, which is
 * the accident #24 asks for protection from arriving by the other road.
 *
 * Both halves matter: that the last row of the band still drives the bar, and
 * that the first row below it does not.
 */
static void testThrottleBarStopsAtTheButtons() {
	section("Throttle: the taller bar stops where the buttons start");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();

	// The row above ARM's top is the last row of the throttle band: the grab
	// area runs one pixel past the drawn track.
	const UiRect arm = ctlRect(UI_FIX(TesterCtl::Arm));
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	checkInt("the track's grab area ends where ARM begins", thr.y + thr.h + 1, arm.y);
	int hx, hy;
	ctlCentre(UI_FIX(TesterCtl::Latch), &hx, &hy);
	fakePress(hx, arm.y - 1); frames(1);
	fakeHold(hx, arm.y - 1);  frames(1);
	checkTrue("the last row of the band still drives the bar", fakeThrottle() > 0);
	fakeRelease(); frames(2);
	checkInt("and it springs back", fakeThrottle(), 0);

	// ARM's top row belongs to the button row whole.
	checkTrue("still armed", fakeArmed());
	fakePress(arm.x + arm.w / 2, arm.y); frames(2);
	checkTrue("the row below it disarms instead of driving", !fakeArmed());
	fakeRelease(); frames(2);
}

/**
 * @brief The throttle ceiling binds in relative mode, not just absolute.
 *
 * Deleting the upper clamp in relativeThrottle() outright used to pass: only
 * the lower rail was tested, and only absolute mode's ceiling.
 */
static void testCeilingBindsInRelativeMode() {
	section("Safety: the ceiling binds however the throttle is driven");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();

	uint16_t ceiling = settings()->maxThrottle;

	// LATCH mode: the gauge is relative. Swipe far past the right-hand end,
	// several times, and it must sit exactly on the ceiling.
	tapId(UI_FIX(TesterCtl::Latch));
	for (int i = 0; i < 4; i++) swipe(10, 235, thrY(), 6);
	frames(2);
	checkTrue("LATCH mode never exceeds the ceiling", fakeThrottle() <= ceiling);
	checkInt("and reaches it", fakeThrottle(), ceiling);

	// The pad: always relative, in either mode. Swipe up the full height
	// repeatedly: from 20 px above its bottom edge to 200 px higher.
	const UiRect pad = ctlRect(UI_FIX(TesterCtl::Pad));
	const int padX = pad.x + pad.w / 2, padBot = pad.y + pad.h;
	for (int i = 0; i < 6; i++) {
		fakePress(padX, padBot - 20); frames(1);
		for (int y = padBot - 20; y >= padBot - 200; y -= 10) { fakeHold(padX, y); frames(1); }
		fakeRelease(); frames(1);
	}
	checkTrue("the pad never exceeds it either", fakeThrottle() <= ceiling);
	checkInt("and reaches it too", fakeThrottle(), ceiling);

	// Lowering the ceiling while a latched throttle is above it must not leave
	// the motor above the new limit.
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("opening MENU disarmed", !uiArmed());
	checkInt("which zeroed the throttle", fakeThrottle(), 0);
}

/** @brief The disarmed-throttle backstop, independent of every other rule. */
static void testDisarmedThrottleBackstop() {
	section("Safety: no throttle while disarmed, by any route");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);

	// Never armed: every throttle surface must be inert.
	swipe(20, 200, thrY(), 8);
	frames(2);
	checkInt("the gauge does nothing while disarmed", fakeThrottle(), 0);

	// The pad is disabled, so its rectangle is read and not tapped by id.
	const UiRect pad = ctlRect(UI_FIX(TesterCtl::Pad));
	const int padX = pad.x + pad.w / 2, padBot = pad.y + pad.h;
	fakePress(padX, padBot - 20); frames(1);
	for (int y = padBot - 20; y >= padBot - 200; y -= 10) { fakeHold(padX, y); frames(1); }
	fakeRelease(); frames(2);
	checkInt("nor does the pad", fakeThrottle(), 0);
	checkInt("nor is one commanded", uiThrottle(), 0);
}

/**
 * @brief What core1 puts on the wire, including the interlocks core0 cannot see.
 *
 * escFrameAction() is pure precisely so these can be reached: esc_task.cpp
 * cannot be linked here, and the heartbeat timeout in particular had no
 * coverage of any kind -- deleting escHeartbeat()'s call site passed the suite.
 */
static void testFrameAction() {
	section("Safety: core1's frame decision");

	// Armed, alive, throttle commanded: it goes out.
	EscFrame f = escFrameAction(true, true, 1500, 0, 0, false);
	checkTrue("armed and alive sends the throttle", !f.sendCommand);
	checkInt("at the commanded value", f.throttle, 1500);

	// The heartbeat backstop. This is the interlock that does not need core0 to
	// be well enough to act, and it was untested.
	f = escFrameAction(true, false, 1500, 0, 0, false);
	checkInt("a dead UI forces the throttle to zero", f.throttle, 0);
	checkTrue("still a throttle frame, not silence", !f.sendCommand);

	// Disarmed means zero whatever was last commanded.
	f = escFrameAction(false, true, 1500, 0, 0, false);
	checkInt("disarmed forces zero", f.throttle, 0);

	// Commands go out only while disarmed, and are not consumed while armed --
	// a queued command must not be eaten by a frame that cannot carry it.
	f = escFrameAction(false, true, 0, 42, 6, false);
	checkTrue("a queued command is sent while disarmed", f.sendCommand);
	checkInt("and it is the queued one", f.command, 42);

	f = escFrameAction(true, true, 800, 42, 6, false);
	checkTrue("armed, the command waits rather than going out", !f.sendCommand);
	checkInt("and the throttle goes instead", f.throttle, 800);

	// KISS requests never ride alongside a command.
	f = escFrameAction(false, true, 0, 0, 0, true);
	checkTrue("a due KISS request is made", f.requestKiss);
	f = escFrameAction(false, true, 0, 42, 6, true);
	checkTrue("but never during a command sequence", !f.requestKiss);
}

/** @brief The automatic EDT enable, at the boundaries the UI cares about. */
static void testEdtAutoRule() {
	section("Safety: the automatic EDT enable");

	const uint32_t RETRY = 1000;
	checkTrue("an ESC that just appeared is sent one",
	          edtAutoAction(true, false, true, false, 0, RETRY) == EdtAutoAction::Send);
	checkTrue("and asked again if it did not take",
	          edtAutoAction(true, false, true, true, RETRY, RETRY) == EdtAutoAction::Send);
	checkTrue("but left alone once EDT is actually arriving",
	          edtAutoAction(true, true, true, true, RETRY, RETRY) == EdtAutoAction::None);
	checkTrue("losing the link re-arms it for the next ESC",
	          edtAutoAction(false, false, true, true, 0, RETRY) == EdtAutoAction::Rearm);
	checkTrue("no link and nothing sent is nothing to do",
	          edtAutoAction(false, false, true, false, 0, RETRY) == EdtAutoAction::None);
	// Armed, mid-burst, or a prop still turning: no attempt is made and, just as
	// importantly, none is recorded -- so the retry clock is not started by a
	// frame the ESC was never going to act on.
	checkTrue("an ESC that cannot execute a command is not sent one",
	          edtAutoAction(true, false, false, false, 0, RETRY) == EdtAutoAction::None);
}

// --- a model of core1's frame pump, for the EDT enable at bind ---

/**
 * @brief An ESC as core1's pump hears it, cut down to what the EDT enable
 *        and the decode of its replies depend on.
 *
 * Command 13 takes effect on its sixth identical frame in a row, as in the
 * AM32 model of test_dshot_edt.cpp, and only while `ready` (the ESC has
 * armed itself) and stopped. With EDT on, four replies in five are EDT
 * frames, each of which reads as a non-zero eRPM if decoded as one.
 */
struct SimEsc {
	bool     present;   /**< Answers at all. */
	bool     canEdt;    /**< Firmware with EDT support. */
	bool     edt;       /**< EDT is on. */
	bool     ready;     /**< Acts on a command. */
	uint32_t periodUs;  /**< Electrical period; 0 is a stopped motor. */
	uint16_t lastCmd;   /**< Command being counted. */
	unsigned count;     /**< Identical commands in a row. */
	unsigned n;         /**< Replies since power-up. */
};

/** @brief An electrical period as an ESC encodes it: lowest exponent, so normalised. */
static uint16_t simErpmPayload(uint32_t periodUs) {
	if (periodUs == 0) return 0x0FFFu;
	unsigned e = 0;
	while ((periodUs >> e) > 0x1FFu) e++;
	return (uint16_t)((e << 9) | (periodUs >> e));
}

/** @brief The RPM a correct decode of @p periodUs gives at 14 poles. */
static uint32_t simRpm(uint32_t periodUs) {
	uint16_t p = simErpmPayload(periodUs);
	uint32_t q = (uint32_t)(p & 0x1FFu) << (p >> 9);
	return (60000000u / q) / 7u;
}

/** @brief One frame in, the 16-bit reply out. */
static uint16_t simEscFrame(SimEsc *esc, uint16_t value) {
	if (value >= 1 && value <= DSHOT_CMD_MAX) {
		if (value != esc->lastCmd) { esc->lastCmd = value; esc->count = 0; }
		if (++esc->count >= 6 && value == DSHOT_CMD_EDT_ENABLE && esc->canEdt &&
		    esc->ready && esc->periodUs == 0)
			esc->edt = true;
	} else {
		esc->lastCmd = 0;
		esc->count = 0;
	}
	esc->n++;
	if (esc->edt) {
		switch (esc->n % 5) {
			case 1: return dshot_telem_value(DSHOT_TELEM_VOLTAGE, 66);
			case 2: return dshot_telem_value(DSHOT_TELEM_TEMPERATURE, 46);
			case 3: return dshot_telem_value(DSHOT_TELEM_CURRENT, 3);
			case 4: return dshot_telem_value(DSHOT_TELEM_STATUS, 0x07);
			default: break;
		}
	}
	return dshot_telem_value(DSHOT_TELEM_ERPM, simErpmPayload(esc->periodUs));
}

/** @brief Slots kept in the trace: 30 s at 1 kHz. */
#define SIM_SLOTS 30000

/**
 * @brief Core1's per-slot decisions in escTaskPoll() order, without the I/O.
 *
 * The decisions are the firmware's own: escPumpBind(), escPumpDecodeEdt(),
 * escFoldReply(), escPumpSlot(), escEdtForget() and escPumpFrameSent(), the
 * functions escTaskPoll() calls. What is modelled here is only what the PIO
 * driver, core0 and the clock do: one frame per millisecond, its reply read
 * one slot later, and the beacon request core0 raises.
 */
struct SimPump {
	EscPump  pump;
	EscTelemetry tel;
	bool     armed;
	uint16_t throttle;
	uint8_t  beepReq;       /**< core0's s_beepReq. */
	bool     kissRx;        /**< A KISS receiver runs. */
	unsigned kissReqs;      /**< Frames sent with the KISS request bit. */
	uint32_t ms;            /**< millis() of the slot about to run. */
	bool     havePrev;      /**< A reply to the last frame is owed. */
	uint16_t prevReply;     /**< That reply, as the ESC sent it. */
	unsigned slot;          /**< Slots run, the trace index. */
	uint16_t sent[SIM_SLOTS]; /**< DShot value of each slot's frame. */
	uint32_t rpm[SIM_SLOTS];  /**< tel.rpm after each slot's reply. */
	bool     kiss[SIM_SLOTS]; /**< The slot's frame carried the KISS request bit. */
};

/** @brief What a bind does to the pump, as escTaskPoll() does it. */
static void simBind(SimPump *p) {
	escPumpBind(&p->pump, &p->armed, &p->throttle);
	p->havePrev = false;    // a new binding has sent nothing to read back
}

/** @brief A pump from power-up: telemetry block cleared, then the first bind. */
static void simBoot(SimPump *p, uint32_t ms) {
	memset(p, 0, sizeof(*p));
	p->ms = ms;
	simBind(p);
}

/** @brief One frame slot. */
static void simSlot(SimPump *p, SimEsc *esc) {
	uint32_t ms = p->ms;

	// 1. The reply to the previous frame.
	dshot_telem_t t;
	memset(&t, 0, sizeof(t));
	dshot_reply_t r = DSHOT_REPLY_NONE;
	if (p->havePrev && esc->present)
		r = dshot_telem_from_value(p->prevReply, escPumpDecodeEdt(&p->pump), &t)
		        ? DSHOT_REPLY_GOOD : DSHOT_REPLY_BAD;
	escFoldReply(&p->tel, r, &t, 14, ms);

	// 2. The decision, and what core1 does with it.
	EscSlot s = escPumpSlot(&p->pump, &p->tel, p->armed, true, p->throttle,
	                        p->beepReq, p->kissRx, ms);
	if (s.forgetEdt) escEdtForget(&p->tel);
	if (s.beepTaken) p->beepReq = 0;
	const EscFrame &f = s.frame;
	uint16_t value = f.sendCommand
	               ? f.command
	               : (uint16_t)(kissBuildDshotPayload(f.throttle, f.requestKiss) >> 1);
	if (f.requestKiss) p->kissReqs++;
	escPumpFrameSent(&p->pump, &f, ms);

	if (p->slot < SIM_SLOTS) {
		p->sent[p->slot] = value;
		p->rpm[p->slot]  = p->tel.rpm;
		p->kiss[p->slot] = f.requestKiss;
	}
	p->slot++;
	p->havePrev = true;
	p->prevReply = simEscFrame(esc, value);
	p->ms++;
}

/** @brief @p n slots. */
static void simRun(SimPump *p, SimEsc *esc, unsigned n) {
	for (unsigned i = 0; i < n; i++) simSlot(p, esc);
}

/** @brief The 10 frames from trace slot @p from are the EDT enable, and the next is not. */
static bool simBindBurstAt(const SimPump *p, unsigned from) {
	for (unsigned i = 0; i < EDT_ENABLE_REPEATS; i++)
		if (p->sent[from + i] != DSHOT_CMD_EDT_ENABLE) return false;
	return p->sent[from + EDT_ENABLE_REPEATS] != DSHOT_CMD_EDT_ENABLE;
}

/** @brief Highest RPM in the trace between two slots. */
static uint32_t simMaxRpm(const SimPump *p, unsigned from, unsigned to) {
	uint32_t m = 0;
	for (unsigned i = from; i < to; i++) if (p->rpm[i] > m) m = p->rpm[i];
	return m;
}

/** @brief Slots in the trace that start a run of the EDT enable. */
static unsigned simBurstStarts(const SimPump *p, unsigned from, unsigned to,
                               unsigned *starts, unsigned max) {
	unsigned n = 0;
	for (unsigned i = from; i < to; i++) {
		bool is = p->sent[i] == DSHOT_CMD_EDT_ENABLE;
		bool was = i > 0 && p->sent[i - 1] == DSHOT_CMD_EDT_ENABLE;
		if (is && !was) {
			if (n < max) starts[n] = i;
			n++;
		}
	}
	return n;
}

static SimPump s_sim;   // 210 kB, most of it trace: static, not on the stack

/**
 * @brief An ESC that kept EDT on is read as EDT from the first reply after
 *        every bind, and never yields an RPM.
 *
 * The ESC here is stopped, so any RPM above 0 is an EDT frame read as eRPM:
 * the display's false RPM. Three binds: a board restart (telemetry block
 * cleared), a pump rebuild after a SETTINGS change, which is also the path an
 * AM32 settings session returns through, and a rebuild while armed.
 */
static void testEdtKeptAcrossBind() {
	section("Safety: an ESC that kept EDT on shows no false RPM");

	SimEsc esc;
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.canEdt = esc.edt = esc.ready = true;

	SimPump *p = &s_sim;
	simBoot(p, 5000);
	simRun(p, &esc, EDT_ENABLE_REPEATS);
	checkTrue("boot: EDT voltage read during the enable",
	          p->tel.edtVoltsMs != 0 && p->tel.volts > 16.4f && p->tel.volts < 16.6f);
	simRun(p, &esc, 3000 - EDT_ENABLE_REPEATS);
	checkTrue("boot: the enable is the first 10 frames", simBindBurstAt(p, 0));
	checkInt("boot: RPM in the first 300 ms", (long)simMaxRpm(p, 0, 300), 0);
	checkInt("boot: RPM in 3 s", (long)simMaxRpm(p, 0, 3000), 0);
	checkInt("boot: no reply counted damaged", (long)p->tel.badPackets, 0);
	unsigned starts[8];
	checkInt("boot: no scheduled enable, EDT is arriving",
	         (long)simBurstStarts(p, 0, 3000, starts, 8), 1);

	// A SETTINGS change: the pump rebinds and the telemetry block carries over.
	unsigned at = p->slot;
	simBind(p);
	simRun(p, &esc, 3000);
	checkTrue("rebuild: the enable is the first 10 frames", simBindBurstAt(p, at));
	checkInt("rebuild: RPM in the first 300 ms", (long)simMaxRpm(p, at, at + 300), 0);
	checkInt("rebuild: RPM in 3 s", (long)simMaxRpm(p, at, at + 3000), 0);

	// Rebuilt while armed: the bind disarms, and the arm state from before
	// it does not abandon the burst.
	p->armed = true;
	p->throttle = 300;
	esc.periodUs = 2000;
	simRun(p, &esc, 200);
	checkInt("armed: the true RPM is read", (long)p->tel.rpm, (long)simRpm(2000));
	esc.periodUs = 0;
	simRun(p, &esc, 50);
	at = p->slot;
	simBind(p);
	simRun(p, &esc, 1000);
	checkTrue("rebuild while armed: the enable is the first 10 frames",
	          simBindBurstAt(p, at));
	checkInt("rebuild while armed: RPM after it", (long)simMaxRpm(p, at, at + 1000), 0);

	// The contrast: the same replies decoded as eRPM, which is what reading
	// with EDT decoding off does, carry a speed.
	dshot_telem_t t;
	dshot_telem_from_value(dshot_telem_value(DSHOT_TELEM_VOLTAGE, 66), false, &t);
	checkTrue("the same voltage frame read as eRPM is a speed", t.erpm == 227272u);
}

/**
 * @brief An ESC without EDT reads correct RPM with EDT decoding on.
 *
 * Swept over the whole period range the frame carries, 20 us to 65,408 us,
 * disarmed and coasting, then armed: every slot's RPM is the period's.
 */
static void testNoEdtEscRpm() {
	section("Safety: an ESC without EDT reads correct RPM");

	SimEsc esc;
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	esc.periodUs = 20;

	SimPump *p = &s_sim;
	simBoot(p, 5000);
	simRun(p, &esc, 2);
	int wrong = 0;
	for (uint32_t period = 20; period <= 65408u; period += 7) {
		esc.periodUs = period;
		simRun(p, &esc, 2);     // the second slot reads this period's reply
		if (p->tel.rpm != simRpm(period)) wrong++;
		if (period == 30000) { p->armed = true; p->throttle = 200; }
	}
	checkTrue("the enable went out first", simBindBurstAt(p, 0));
	checkInt("slots with the wrong RPM", wrong, 0);
	checkInt("no EDT field stamped", (long)(p->tel.edtVoltsMs | p->tel.edtAmpsMs |
	                                        p->tel.edtTempMs | p->tel.edtStressMs |
	                                        p->tel.edtStatusMs), 0);
	checkInt("no reply counted damaged", (long)p->tel.badPackets, 0);
}

/**
 * @brief The bind burst comes first, and the scheduled enable keeps its rules.
 *
 * The bind burst is not an attempt: the first scheduled enable is
 * @ref EDT_SETTLE_MS after link-up and the next @ref EDT_RETRY_MS after the
 * last frame of the one before.
 */
static void testEdtBindBurstRules() {
	section("Safety: the EDT enable at bind and the retry after it");

	SimPump *p = &s_sim;
	SimEsc esc;
	unsigned starts[16];

	// An ESC without EDT, connected and stopped.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	simBoot(p, 5000);
	simRun(p, &esc, 4000);
	unsigned n = simBurstStarts(p, 0, 4000, starts, 16);
	checkTrue("the enable is the first 10 frames", simBindBurstAt(p, 0));
	checkInt("bind, then settle, then three retries", (long)n, 5);
	// The first reply is read in slot 1, so the link is up from 5001 ms.
	checkInt("first scheduled one 300 ms after link-up", (long)starts[1],
	         1 + EDT_SETTLE_MS);
	checkInt("the next one 1000 ms after the last frame of that",
	         (long)(starts[2] - starts[1]), EDT_RETRY_MS + EDT_ENABLE_REPEATS - 1);
	checkInt("and the next", (long)(starts[3] - starts[2]),
	         EDT_RETRY_MS + EDT_ENABLE_REPEATS - 1);
	checkTrue("the enable is reported as sent", p->pump.edt.sent);

	// Nothing connected: the bind burst, then nothing, and the request state
	// clears once it has gone out.
	memset(&esc, 0, sizeof(esc));
	simBoot(p, 5000);
	simRun(p, &esc, EDT_ENABLE_REPEATS);
	checkTrue("no ESC: reported as sent during the burst", p->pump.edt.sent);
	simRun(p, &esc, 3000);
	checkTrue("no ESC: the enable is the first 10 frames", simBindBurstAt(p, 0));
	checkInt("no ESC: nothing after it", (long)simBurstStarts(p, 0, p->slot, starts, 16), 1);
	checkTrue("no ESC: not reported as sent after it", !p->pump.edt.sent);

	// An ESC with EDT that is not ready for commands at the bind takes a
	// scheduled one, and the asking stops.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.canEdt = true;
	simBoot(p, 5000);
	simRun(p, &esc, 1000);
	checkTrue("not ready: EDT is not on yet", !esc.edt);
	esc.ready = true;
	simRun(p, &esc, 3000);
	checkTrue("ready: EDT comes on", esc.edt);
	n = simBurstStarts(p, 0, p->slot, starts, 16);
	checkInt("ready: bind, settle, and the retry that took", (long)n, 3);
	checkInt("ready: no RPM at any time", (long)simMaxRpm(p, 0, p->slot), 0);

	// Armed during the bind burst: the throttle goes out on that frame, the
	// rest of the burst is dropped, and no command goes out while armed.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	simBoot(p, 5000);
	simRun(p, &esc, 3);
	p->armed = true;
	p->throttle = 100;
	simRun(p, &esc, 2000);
	unsigned thr = kissBuildDshotPayload(100, false) >> 1;
	checkInt("armed: the throttle goes out on the next frame", p->sent[3], (long)thr);
	checkInt("armed: no command after it",
	         (long)simBurstStarts(p, 3, p->slot, starts, 16), 0);
	p->armed = false;
	p->throttle = 0;
	unsigned from = p->slot;
	simRun(p, &esc, 400);
	n = simBurstStarts(p, from, p->slot, starts, 16);
	checkInt("disarmed: the enable follows on the settled link", (long)n, 1);

	// A beacon asked for at the bind waits for the burst.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	simBoot(p, 5000);
	p->beepReq = 2;
	simRun(p, &esc, EDT_ENABLE_REPEATS);
	checkInt("beacon: the request waits for the enable", p->beepReq, 2);
	simRun(p, &esc, 10);
	checkInt("beacon: and is taken after it", p->beepReq, 0);
	checkTrue("beacon: the enable is the first 10 frames", simBindBurstAt(p, 0));
	checkInt("beacon: frame 11 is the beacon", p->sent[EDT_ENABLE_REPEATS],
	         DSHOT_CMD_BEEP2);

	// An ESC that kept EDT on is unplugged and plugged back in. The link
	// loss clears the request state and the EDT fields; the replies after
	// the reconnect read as EDT, with no enable sent in between.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.canEdt = esc.edt = esc.ready = true;
	simBoot(p, 5000);
	simRun(p, &esc, 1000);
	esc.present = false;
	simRun(p, &esc, ESC_LINK_STALE_MS + 10);
	checkTrue("unplugged: not reported as sent", !p->pump.edt.sent);
	checkInt("unplugged: EDT fields cleared",
	         (long)(p->tel.edtVoltsMs | p->tel.edtAmpsMs | p->tel.edtTempMs |
	                p->tel.edtStressMs | p->tel.edtStatusMs), 0);
	unsigned back = p->slot;
	esc.present = true;
	simRun(p, &esc, 3000);
	checkInt("plugged back: RPM at any time", (long)simMaxRpm(p, back, p->slot), 0);
	checkTrue("plugged back: EDT read again", p->tel.edtVoltsMs > 0);
	checkInt("plugged back: no enable needed",
	         (long)simBurstStarts(p, back, p->slot, starts, 16), 0);
}

/** @brief Slot @p from starts a run of exactly @p n frames of @p value. */
static bool simRunOf(const SimPump *p, unsigned from, unsigned n, uint16_t value) {
	if (from > 0 && p->sent[from - 1] == value) return false;
	for (unsigned i = 0; i < n; i++)
		if (p->sent[from + i] != value) return false;
	return p->sent[from + n] != value;
}

/** @brief The first slot from @p from whose frame carried the KISS request bit. */
static unsigned simNextKiss(const SimPump *p, unsigned from) {
	for (unsigned i = from; i < p->slot && i < SIM_SLOTS; i++)
		if (p->kiss[i]) return i;
	return SIM_SLOTS;
}

/**
 * @brief The KISS request cadence around commands and binds, and an ESC
 *        swapped for one without EDT.
 *
 * A request is due every @ref KISS_REQUEST_EVERY_N frames. A command frame
 * holds the count, so a request due on one goes out on the first free frame
 * after the command, and a bind starts the count again.
 */
static void testKissCadence() {
	section("Safety: KISS requests around commands, and an ESC swap");

	SimPump *p = &s_sim;
	SimEsc esc;
	memset(&esc, 0, sizeof(esc));   // nothing connected: no scheduled enable

	simBoot(p, 5000);
	p->kissRx = true;
	simRun(p, &esc, 200);
	checkInt("first request on the first free frame",
	         (long)simNextKiss(p, 0), EDT_ENABLE_REPEATS);
	unsigned r = simNextKiss(p, 100);
	checkInt("then every 20 frames", (long)(simNextKiss(p, r + 1) - r),
	         KISS_REQUEST_EVERY_N);

	// A beacon that starts on the slot a request falls due on.
	unsigned at = p->slot;
	unsigned last = r;
	while (simNextKiss(p, last + 1) < at) last = simNextKiss(p, last + 1);
	simRun(p, &esc, last + KISS_REQUEST_EVERY_N - at);
	p->beepReq = 1;
	simRun(p, &esc, 100);
	unsigned due = last + KISS_REQUEST_EVERY_N;
	checkTrue("beacon: it takes the due slot", p->sent[due] == DSHOT_CMD_BEEP1);
	checkInt("beacon: the request on the next free frame",
	         (long)simNextKiss(p, last + 1), (long)(due + BEEP_REPEATS));

	// An arm while a beacon is owed, and an arm while one waits for the
	// enable. Either way the arm drops it: the requests keep their cadence
	// while armed, and no beacon goes out after the disarm.
	for (int waiting = 0; waiting < 2; waiting++) {
		memset(&esc, 0, sizeof(esc));
		simBoot(p, 5000);
		p->kissRx = true;
		simRun(p, &esc, waiting ? 3 : 100);
		p->beepReq = 3;
		simRun(p, &esc, waiting ? 2 : 3);
		if (!waiting)
			checkTrue("arm mid-beacon: the beacon is going out, frames owed",
			          p->sent[p->slot - 1] == DSHOT_CMD_BEEP3 &&
			          p->pump.beepReps > 0);
		else
			checkInt("arm in the enable: the beacon waits", p->beepReq, 3);
		p->armed = true;
		p->throttle = 100;
		unsigned armedAt = p->slot;
		simRun(p, &esc, 200);
		checkInt(waiting ? "arm in the enable: the request is taken"
		                 : "arm mid-beacon: no request left", p->beepReq, 0);
		unsigned k = simNextKiss(p, armedAt);
		checkTrue(waiting ? "arm in the enable: a request within 20 frames"
		                  : "arm mid-beacon: a request within 20 frames",
		          k - armedAt < KISS_REQUEST_EVERY_N);
		checkInt(waiting ? "arm in the enable: then every 20 frames"
		                 : "arm mid-beacon: then every 20 frames",
		         (long)(simNextKiss(p, k + 1) - k), KISS_REQUEST_EVERY_N);
		p->armed = false;
		p->throttle = 0;
		unsigned off = p->slot;
		simRun(p, &esc, 100);
		bool beeped = false;
		for (unsigned i = off; i < p->slot; i++)
			beeped = beeped || p->sent[i] == DSHOT_CMD_BEEP3;
		checkTrue(waiting ? "arm in the enable: no beacon after disarm"
		                  : "arm mid-beacon: no beacon after disarm", !beeped);
	}

	// A bind on the slot after a request starts the count again.
	for (unsigned i = 0; i < KISS_REQUEST_EVERY_N && !p->kiss[p->slot - 1]; i++)
		simRun(p, &esc, 1);
	checkTrue("rebind: the last slot carried a request", p->kiss[p->slot - 1]);
	at = p->slot;
	simBind(p);
	simRun(p, &esc, 100);
	checkInt("rebind: first request on the first free frame",
	         (long)(simNextKiss(p, at) - at), EDT_ENABLE_REPEATS);

	// An ESC with EDT on is unplugged, and one without EDT goes in its place
	// within 1 s of the last EDT frame. None of the old ESC's EDT arrival
	// times holds back the new one's enable.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.canEdt = esc.edt = esc.ready = true;
	simBoot(p, 5000);
	simRun(p, &esc, 1000);
	esc.present = false;
	simRun(p, &esc, ESC_LINK_STALE_MS + 10);
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	unsigned back = p->slot;
	unsigned starts[4];
	simRun(p, &esc, 600);
	unsigned n = simBurstStarts(p, back, p->slot, starts, 4);
	checkInt("swap: one scheduled enable", (long)n, 1);
	// The pump keeps running, so the new ESC's first reply is read in the
	// first slot after the swap and the link is up from there.
	checkInt("swap: 300 ms after the new link-up",
	         n ? (long)(starts[0] - back) : -1L, EDT_SETTLE_MS);
}

/**
 * @brief A scheduled enable waits for a stopped motor, a disarm and the end
 *        of a beacon, and an arm drops it.
 *
 * DShot commands execute only with the motor stopped and disarmed, and a
 * burst broken by another command reaches no repeat count. A burst an arm
 * leaves owed would also hold back every KISS request while armed.
 */
static void testEdtStepRules() {
	section("Safety: when a scheduled EDT enable goes out");

	SimPump *p = &s_sim;
	SimEsc esc;
	unsigned starts[16];

	// A rebuild after a SETTINGS change disarms with the prop still coasting.
	// No enable goes out until the motor reports 0 eRPM.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	esc.periodUs = 2000;
	simBoot(p, 5000);
	simRun(p, &esc, 3000);
	checkInt("coasting: only the bind burst in 3 s",
	         (long)simBurstStarts(p, 0, p->slot, starts, 16), 1);
	unsigned stop = p->slot;
	esc.periodUs = 0;
	simRun(p, &esc, 500);
	unsigned n = simBurstStarts(p, stop, p->slot, starts, 16);
	checkInt("stopped: one scheduled enable", (long)n, 1);
	// The reply to the first stopped frame is read one slot later.
	checkInt("stopped: on the first slot that reads 0 eRPM",
	         n ? (long)(starts[0] - stop) : -1L, 1);

	// Armed in the middle of a scheduled burst, with a KISS receiver: the
	// rest of the burst is dropped, no enable goes out while armed, and every
	// KISS request goes out on time.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	simBoot(p, 5000);
	p->kissRx = true;
	simRun(p, &esc, 1 + EDT_SETTLE_MS + 3);
	checkTrue("arm mid-burst: the burst is under way",
	          p->sent[1 + EDT_SETTLE_MS] == DSHOT_CMD_EDT_ENABLE &&
	          p->sent[p->slot - 1] == DSHOT_CMD_EDT_ENABLE);
	unsigned armAt = p->slot;
	unsigned kissAt = p->kissReqs;
	p->armed = true;
	p->throttle = 100;
	simRun(p, &esc, 2000);
	checkInt("arm mid-burst: throttle on the next frame",
	         p->sent[armAt], (long)(kissBuildDshotPayload(100, false) >> 1));
	checkInt("arm mid-burst: no enable while armed",
	         (long)simBurstStarts(p, armAt, p->slot, starts, 16), 0);
	checkInt("arm mid-burst: no command owed while armed", p->pump.edt.reps, 0);
	checkInt("arm mid-burst: KISS requests in 2 s armed",
	         (long)(p->kissReqs - kissAt), 2000 / KISS_REQUEST_EVERY_N);
	p->armed = false;
	p->throttle = 0;
	unsigned disarmAt = p->slot;
	simRun(p, &esc, 100);
	n = simBurstStarts(p, disarmAt, p->slot, starts, 16);
	checkInt("disarmed: one enable", (long)n, 1);
	checkTrue("disarmed: on the first frame, 10 frames long",
	          n == 1 && starts[0] == disarmAt &&
	          simRunOf(p, disarmAt, EDT_ENABLE_REPEATS, DSHOT_CMD_EDT_ENABLE));

	// A beacon that is going out when the scheduled enable falls due keeps
	// the wire, and the enable follows it.
	memset(&esc, 0, sizeof(esc));
	esc.present = esc.ready = true;
	simBoot(p, 5000);
	const unsigned due = 1 + EDT_SETTLE_MS;
	simRun(p, &esc, due - 3);
	p->beepReq = 3;
	simRun(p, &esc, 100);
	checkTrue("beacon: all 6 frames, unbroken",
	          simRunOf(p, due - 3, BEEP_REPEATS, DSHOT_CMD_BEEP3));
	n = simBurstStarts(p, due - 3, p->slot, starts, 16);
	checkInt("beacon: one enable after it", (long)n, 1);
	checkTrue("beacon: on the next frame, 10 frames long",
	          n == 1 && starts[0] == due - 3 + BEEP_REPEATS &&
	          simRunOf(p, starts[0], EDT_ENABLE_REPEATS, DSHOT_CMD_EDT_ENABLE));
}

/** @brief The idle interlock actually fires, and says so first. */
static void testIdleAutoDisarm() {
	section("Safety: idle auto-disarm");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();
	checkTrue("armed", uiArmed());

	// Not yet: well inside the window, with no touches at all.
	fakeAdvance(IDLE_DISARM_MS / 2);
	frames(2);
	checkTrue("still armed halfway through the window", uiArmed());

	// The countdown appears before it acts. A motor stopping unannounced is
	// indistinguishable from a fault.
	auto statusBar = []() { return fakeRegionHash(120, 0, 120, 26); };
	uint32_t quiet = statusBar();
	fakeAdvance(IDLE_DISARM_MS / 2 - 3000);
	frames(2);
	checkTrue("the status bar warns before it disarms", statusBar() != quiet);
	checkTrue("and is still armed while warning", uiArmed());

	// Past the window: disarmed, and the ESC told.
	fakeAdvance(4000);
	frames(2);
	checkTrue("idle disarms", !uiArmed());
	checkTrue("and the ESC is told", !fakeArmed());
	checkInt("throttle zeroed", fakeThrottle(), 0);

	// Touching resets the timer, so an operator who is present is not disarmed.
	holdToArm();
	for (int i = 0; i < 3; i++) {
		fakeAdvance(IDLE_DISARM_MS - 2000);
		frames(2);
		tapId(UI_FIX(TesterCtl::Latch));      // any touch counts
		tapId(UI_FIX(TesterCtl::Latch));      // and back off
	}
	checkTrue("touching keeps it armed", uiArmed());
}

/**
 * @brief The BOARD voltage's colour: amber below 3.50 V, red below 3.30 V,
 *        back only 50 mV above each threshold.
 */
static void testBoardCellColour() {
	section("Tester: the BOARD voltage's colour");

	checkTrue("3.50 V: text colour", uiBoardCell(3.500f, BoardCell::Ok) == BoardCell::Ok);
	checkTrue("3.49 V: amber", uiBoardCell(3.490f, BoardCell::Ok) == BoardCell::Low);
	checkTrue("3.30 V: amber", uiBoardCell(3.300f, BoardCell::Ok) == BoardCell::Low);
	checkTrue("3.29 V: red", uiBoardCell(3.290f, BoardCell::Ok) == BoardCell::Critical);
	checkTrue("amber at 3.54 V stays amber", uiBoardCell(3.540f, BoardCell::Low) == BoardCell::Low);
	checkTrue("amber at 3.55 V goes back", uiBoardCell(3.550f, BoardCell::Low) == BoardCell::Ok);
	checkTrue("red at 3.34 V stays red", uiBoardCell(3.340f, BoardCell::Critical) == BoardCell::Critical);
	checkTrue("red at 3.35 V goes to amber", uiBoardCell(3.350f, BoardCell::Critical) == BoardCell::Low);
	// The band follows the text, rounded to centivolts.
	checkTrue("3.4951 V reads 3.50V: the text colour",
	          uiBoardCell(3.4951f, BoardCell::Ok) == BoardCell::Ok);
	checkTrue("3.4949 V reads 3.49V: amber",
	          uiBoardCell(3.4949f, BoardCell::Ok) == BoardCell::Low);
	checkTrue("3.2951 V reads 3.30V: amber, not red",
	          uiBoardCell(3.2951f, BoardCell::Ok) == BoardCell::Low);
	checkTrue("3.2949 V reads 3.29V: red",
	          uiBoardCell(3.2949f, BoardCell::Ok) == BoardCell::Critical);
	checkTrue("amber at 3.5451 V, read 3.55V, goes back",
	          uiBoardCell(3.5451f, BoardCell::Low) == BoardCell::Ok);
	// Below a working cell's range the value says nothing about a cell.
	checkTrue("2.50 V: red", uiBoardCell(2.500f, BoardCell::Ok) == BoardCell::Critical);
	checkTrue("2.49 V: no cell, the text colour",
	          uiBoardCell(2.490f, BoardCell::Ok) == BoardCell::NoCell);
	checkTrue("2.49 V after red: no cell",
	          uiBoardCell(2.490f, BoardCell::Critical) == BoardCell::NoCell);
	checkTrue("0 V: no cell", uiBoardCell(0.0f, BoardCell::Ok) == BoardCell::NoCell);
	checkTrue("no cell at 2.54 V stays", uiBoardCell(2.540f, BoardCell::NoCell) == BoardCell::NoCell);
	checkTrue("no cell at 2.55 V goes red",
	          uiBoardCell(2.550f, BoardCell::NoCell) == BoardCell::Critical);
	// Leaving no cell: the band a start-up gives at the same reading. The
	// first frame classes from Ok (uiInit() sets it).
	static const float JUMPS[] = { 3.20f, 3.32f, 3.52f, 3.55f };
	for (float v : JUMPS) {
		char what[64];
		snprintf(what, sizeof(what), "no cell to %.2f V: as at start-up", (double)v);
		checkTrue(what, uiBoardCell(v, BoardCell::NoCell) == uiBoardCell(v, BoardCell::Ok));
	}
	checkTrue("no cell to 3.32 V: amber", uiBoardCell(3.32f, BoardCell::NoCell) == BoardCell::Low);
	checkTrue("no cell to 3.52 V: the text colour",
	          uiBoardCell(3.52f, BoardCell::NoCell) == BoardCell::Ok);
	checkTrue("no cell to 3.20 V: red",
	          uiBoardCell(3.20f, BoardCell::NoCell) == BoardCell::Critical);
	checkTrue("red at 3.60 V goes to the text colour",
	          uiBoardCell(3.600f, BoardCell::Critical) == BoardCell::Ok);
	checkTrue("amber at 3.20 V goes red at once",
	          uiBoardCell(3.200f, BoardCell::Low) == BoardCell::Critical);

	// Drawn, through the ADC: raw * 3.3 / 4095 * 3 V. The first frame takes
	// the reading unsmoothed; later ones move a tenth of the way a frame.
	auto shows = [](const char *text, uint16_t col) {
		return fakeTextIs(GFX_W - 4 - gfxTextW(text, 2), 7, text, col, C_PANEL, 2);
	};
	struct { uint16_t raw; const char *text; uint16_t col; const char *what; } cases[] = {
		{ 1427, "3.45V", C_AMBER, "3.45 V: amber" },
		{ 1325, "3.20V", C_RED,   "3.20 V: red" },
		{ 1460, "3.53V", C_TEXT,  "3.53 V: the text colour" },
		{ 1029, "2.49V", C_TEXT,  "2.49 V: the text colour, no cell to judge" },
		{ 0,    "0.00V", C_TEXT,  "0 V: the text colour" },
	};
	for (const auto &c : cases) {
		fakeAdcSet(c.raw);
		uiInit();
		frames(2);
		checkTrue(c.what, shows(c.text, c.col));
	}
	// Hysteresis through the smoothing: from 3.45 V, amber, to 3.53 V stays
	// amber; to 3.58 V goes back.
	fakeAdcSet(1427);
	uiInit();
	frames(2);
	fakeAdcSet(1460);
	frames(120);
	checkTrue("amber, then 3.53 V: still amber", shows("3.53V", C_AMBER));
	fakeAdcSet(1481);
	frames(120);
	checkTrue("then 3.58 V: the text colour", shows("3.58V", C_TEXT));
	// With the SDERR chip: both in place, the chip clear of BOARD and the
	// voltage, each in its own colour.
	{
		SdLogStatus was, err;
		sdLogStatus(&was);
		memset(&err, 0, sizeof(err));
		err.state = SdLogState::Error;
		fakeSdLogSet(&err);
		const struct { uint16_t raw; const char *text; uint16_t col; } both[] = {
			{ 1427, "3.45V", C_AMBER }, { 1325, "3.20V", C_RED },
		};
		for (const auto &c : both) {
			fakeAdcSet(c.raw);
			uiInit();
			frames(2);
			char what[64];
			snprintf(what, sizeof(what), "SDERR chip and %s together", c.text);
			checkTrue(what, fakeTextIs(104, 14, "SDERR", C_PAPER, C_AMBER, 1) &&
			                fakeTextIs(140, 14, "BOARD", C_DIM, C_PANEL, 1) &&
			                shows(c.text, c.col));
		}
		fakeSdLogSet(&was);
	}

	// IDLE's countdown replaces the voltage in amber, whatever the voltage's
	// colour: armed at a red reading, so the two colours differ, and run into
	// the last 5 s.
	fakeAdcSet(1325);
	uiInit();
	frames(4);
	holdToArm();
	checkTrue("armed at 3.20 V: the voltage in red", uiArmed() && shows("3.20V", C_RED));
	fakeAdvance(IDLE_DISARM_MS - 3000);
	frames(1);
	checkTrue("the last 5 s: IDLE 3s, amber, in the voltage's place, from y 11",
	          fakeTextIs(GFX_W - 4 - gfxTextW("IDLE 3s", 2), 11, "IDLE 3s", C_AMBER, C_PANEL, 2));
	checkTrue("and no voltage", !shows("3.20V", C_RED) && !shows("3.20V", C_TEXT));
	// At 1200 kbit/s the rate, DSHOT1200, ends at x 156, right of where
	// IDLE 5s starts, x 154: the two share no pixel. Each reads alone in its
	// own rectangle, which do not meet, and the row between is blank.
	{
		uint8_t pin = escTaskDshotPin();
		escTaskConfigure(pin, 1200, false, 0);
		fakeAdvance(1000);
		frames(1);
		const char *idle = "IDLE 2s";
		uint16_t rateCol = escEdtRequested() ? C_TEXT : C_DIM;
		bool alone = fakeTextIs(104, 3, "DSHOT1200", rateCol, C_PANEL, 1) &&
		             fakeTextIs(GFX_W - 4 - gfxTextW(idle, 2), 11, idle, C_AMBER, C_PANEL, 2);
		bool gap = true;
		for (int x = 104; x < GFX_W - 4; x++)
			if (gfxBuffer()[10 * GFX_W + x] != C_PANEL) gap = false;
		checkTrue("DSHOT1200 and IDLE 2s share no pixel, y 10 blank between", alone && gap);
		escTaskConfigure(pin, DSHOT_SPEED_KBAUD, false, 0);
	}
	fakeAdvance(4000);
	frames(2);
	checkTrue("the idle interlock disarms", !uiArmed());
	checkTrue("the voltage is back, red", shows("3.20V", C_RED));
	fakeAdcSet(fakeAdcDefault());
	uiInit();
	frames(2);
}

/** @brief The splash with a touch controller that did not answer at boot. */
static void testSplashTouch() {
	section("Splash: TOUCH CONTROLLER NOT FOUND");

	fakeFlashClear();
	settingsLoad();
	fakeTouchAnswers(false);
	checkTrue("touchInit() reports the failure", !touchInit());
	uiDrawSplash();
	checkTrue("the splash says so, amber, y 64",
	          fakeTextCentredIs(64, UI_SPLASH_TOUCH_TEXT, C_AMBER, C_BG, 1));
	checkTrue("it fits with 8 px to each edge",
	          gfxTextW(UI_SPLASH_TOUCH_TEXT, 1) <= GFX_W - 2 * UI_MARGIN);
	fakeDumpFrame("shot_splash_no_touch.ppm");
	for (int i = 0; i < 10; i++) { fakeAdvance(UI_FRAME_MS); uiSplashTick(); }
	checkTrue("it stays while the controller does not answer",
	          fakeTextCentredIs(UI_SPLASH_TOUCH_Y, UI_SPLASH_TOUCH_TEXT, C_AMBER, C_BG, 1));

	// A controller that answers later clears the splash's line.
	fakeTouchAnswers(false);
	touchInit();
	uiDrawSplash();
	fakeTouchAnswers(true);
	fakeAdvance(UI_FRAME_MS);
	uiSplashTick();
	int x0 = 0, x1 = 0;
	checkTrue("an answer clears the line",
	          !fakeInkExtent(UI_SPLASH_TOUCH_Y, 8, C_BG, &x0, &x1));

	// A finger on the glass through the end of the splash: the splash's read
	// took its press edge, so on the tester it presses nothing until lifted.
	uiInit();
	uiDrawSplash();
	int ax = 0, ay = 0;
	ctlCentre(UI_FIX(TesterCtl::Arm), &ax, &ay);
	fakePress(ax, ay);
	fakeAdvance(UI_FRAME_MS);
	uiSplashTick();
	frames(60);
	checkTrue("a finger on ARM from the splash on, 1.5 s: not armed", !uiArmed());
	fakeRelease();
	frames(2);
	holdToArm();
	checkTrue("lifted and pressed again: HOLD TO ARM arms", uiArmed());
	tapId(UI_FIX(TesterCtl::Arm));
	checkTrue("disarmed for what follows", !uiArmed());

	// Its lift still completes a tap on a button that acts on release: the
	// press began on MENU.
	uiInit();
	uiDrawSplash();
	int mx = 0, my = 0;
	ctlCentre(UI_FIX(TesterCtl::Menu), &mx, &my);
	fakePress(mx, my);
	fakeAdvance(UI_FRAME_MS);
	uiSplashTick();
	frames(4);
	checkTrue("a finger on MENU from the splash on: the tester stays", uiCurrentScreen() == UiScreenId::Tester);
	fakeRelease();
	frames(2);
	checkTrue("its lift opens MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK to the tester", uiCurrentScreen() == UiScreenId::Tester);

	// Answering at boot: no line.
	touchInit();
	uiDrawSplash();
	checkTrue("a controller found at boot: no line",
	          !fakeInkExtent(UI_SPLASH_TOUCH_Y, 8, C_BG, &x0, &x1));
	uiInit();
	frames(2);
}

/** @brief The pole count is clamped at both ends, and reaches the ESC. */
static void testPoleCountReachesEsc() {
	section("Safety: pole count");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	enterSettings();

	checkInt("the ESC starts with the stored count", fakePoles(), settings()->poles);

	tapId(UI_ROW(SettingsRowId::Poles, Plus));
	checkInt("a step reaches the ESC", fakePoles(), settings()->poles);

	// All the way down: the lower clamp is the one nothing tested, and the
	// eRPM conversion divides by poles/2.
	pressId(UI_ROW(SettingsRowId::Poles, Minus));
	holdId(UI_ROW(SettingsRowId::Poles, Minus), 200);
	fakeRelease(); frames(1);
	checkInt("never below the minimum", settings()->poles, MIN_MOTOR_POLES);
	checkInt("and the ESC has that too", fakePoles(), MIN_MOTOR_POLES);

	pressId(UI_ROW(SettingsRowId::Poles, Plus));
	holdId(UI_ROW(SettingsRowId::Poles, Plus), 200);
	fakeRelease(); frames(1);
	checkInt("never above the maximum", settings()->poles, MAX_MOTOR_POLES);
	checkInt("and the ESC has that too", fakePoles(), MAX_MOTOR_POLES);
}

/**
 * @brief The RPM readout goes dead when the ESC stops answering.
 *
 * Deliberately its own test: testTelemetryExpires() excludes the RPM band from
 * every comparison, because the fake serves a fixed packet rate that would mask
 * the tiles. So nothing looked at the readout, and hardcoding telemetryAlive()
 * to true -- which removes NO TELEMETRY and the dead-digit colour entirely --
 * passed the suite.
 */
static void testRpmGoesDead() {
	section("Safety: the RPM readout reports a dead link");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);

	auto rpmBand = []() { return fakeRegionHash(0, 27, 240, 100); };

	feedLiveTelemetry();
	frames(2);
	uint32_t live = rpmBand();

	// Past ESC_LINK_STALE_MS with nothing new arriving.
	fakeAdvance(ESC_LINK_STALE_MS + 200);
	frames(2);
	checkTrue("a dead link changes the RPM readout", rpmBand() != live);

	// And it comes back.
	feedLiveTelemetry();
	frames(2);
	checkTrue("and a live one restores it exactly", rpmBand() == live);
}

/** @brief Arming starts a log by itself, which is the shipped default. */
static void testAutoLogOnArm() {
	section("Safety: logging follows the arm state");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);

	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::Idle;          // a card is present and mounted
	fakeSdLogSet(&st);
	frames(2);
	checkTrue("not logging before arming", !sdLogActive());

	holdToArm();
	frames(2);
	// The existing manual-log test always hand-starts a log first, so
	// SD_LOG_AUTO_ON_ARM -- the default -- had no positive test at all.
	checkTrue("arming starts a log by itself", sdLogActive());

	tapId(UI_FIX(TesterCtl::Arm));            // DISARM
	frames(2);
	checkTrue("and disarming stops it", !sdLogActive());
}

/** @brief Hit-testing is half-open: a control's far edge belongs to its neighbour. */
static void testHitBoundaries() {
	section("Safety: button edges");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	tapId(UI_FIX(TesterCtl::Menu));
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);

	// MENU's rows are 40 px tall at a 42 px pitch: BEEP ends where the gap
	// above AM32 CONFIG starts, and SETTINGS where the gap above SD LOG
	// starts. A tap in either gap must hit neither neighbour -- an inclusive
	// far edge would make adjacent rows overlap.
	const UiRect beep  = ctlRect(menuRow(&UI_BEEP));
	const UiRect settingsRow = ctlRect(menuRow(&UI_SETTINGS));
	int beeps = fakeBeepRequests();
	tap(beep.x + beep.w / 2, beep.y + beep.h);
	frames(2);
	checkInt("a tap in the BEEP/AM32 gap sends nothing", fakeBeepRequests(), beeps);
	checkTrue("and opens nothing", uiCurrentScreen() == UiScreenId::Menu);
	tap(beep.x + beep.w, beep.y + beep.h / 2);
	checkInt("one past BEEP's right edge sends nothing", fakeBeepRequests(), beeps);
	tap(settingsRow.x + settingsRow.w / 2, settingsRow.y + settingsRow.h);
	frames(2);
	checkTrue("a tap in the gap below SETTINGS left MENU up",
	          uiCurrentScreen() == UiScreenId::Menu);
	tap(beep.x + beep.w / 2, beep.y + beep.h - 1);
	checkInt("BEEP's last pixel row is BEEP", fakeBeepRequests(), beeps + 1);
	fakeAdvance(400); frames(2);

	// MOTOR POLES - on SETTINGS is a rectangle. One past each far edge is
	// outside: the gap before + on the right, the gap between rows below.
	tapId(menuRow(&UI_SETTINGS));
	frames(2);
	checkTrue("SETTINGS opens from MENU", uiCurrentScreen() == UiScreenId::Settings);
	const UiRect pm = ctlRect(UI_ROW(SettingsRowId::Poles, Minus));
	uint16_t polesBefore = settings()->poles;
	tap(pm.x + pm.w, pm.y + pm.h / 2);
	checkInt("one past the right edge misses", settings()->poles, polesBefore);
	tap(pm.x + pm.w / 2, pm.y + pm.h);
	checkInt("one past the bottom edge misses", settings()->poles, polesBefore);

	// And the last pixel inside still hits.
	tap(pm.x + pm.w - 1, pm.y + pm.h - 1);
	checkTrue("the last pixel inside still hits", settings()->poles != polesBefore);
}

/**
 * @brief MENU's rows: SERVO opens its screen, and a tool that claims the
 *        pins is refused while they are not Free.
 *
 * A tool row that claims pins is drawn disabled under its own status while
 * the claim is not Free, as for the time Restoring takes after RESTART
 * DSHOT, and the strip says why: its control is gated on the same value,
 * and a tap opens nothing and claims nothing.
 * SERVO is built: a tap opens it and claims the pins, and leaving goes
 * through its unplug page to MENU. test_servo.cpp tests the screen itself.
 */
static void testMenuDestinations() {
	section("MENU: SERVO opens its screen, a busy claim refuses");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	EscTelemetry quiet;
	memset(&quiet, 0, sizeof(quiet));
	fakeSetTelemetry(&quiet);
	frames(2);
	tapId(UI_FIX(TesterCtl::Menu));
	frames(2);
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);
	checkControlTable("MENU");

	// The registry, in MENU order.
	uint8_t rows = 0;
	uiMenuEntries(&rows);
	checkInt("MENU has seven rows", rows, 7);
	checkTrue("BEEP, AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO, SETTINGS, SD LOG, in that order",
	          uiMenuRowOf(&UI_BEEP) == 0 && uiMenuRowOf(&UI_AM32) == 1 &&
	          uiMenuRowOf(&UI_SERVO) == 2 && uiMenuRowOf(&UI_KST) == 3 &&
	          uiMenuRowOf(&UI_HITEC) == 4 && uiMenuRowOf(&UI_SETTINGS) == 5 &&
	          uiMenuRowOf(&UI_SDLOG) == 6);

	// A pixel inside each row, clear of its label: the panel fill of a row.
	// It is 5 px in from the left edge and 8 px down from the top.
	const uint16_t *fb = gfxBuffer();
	auto pixelIn = [&](const UiScreen *s) {
		UiRect r = ctlRect(menuRow(s));
		return fb[(r.y + 8) * GFX_W + (r.x + 5)];
	};
	checkTrue("AM32 is filled", pixelIn(&UI_AM32) == C_PANEL);
	checkTrue("SETTINGS is filled", pixelIn(&UI_SETTINGS) == C_PANEL);
	checkTrue("SERVO is filled", pixelIn(&UI_SERVO) == C_PANEL);
	char pins[16];
	snprintf(pins, sizeof(pins), "GP%u", (unsigned)escTaskDshotPin());
	checkTrue("AM32's row names the SIG pin",
	          menuStatusIs(ctlRect(menuRow(&UI_AM32)), pins, C_DIM, C_PANEL));

	// Another claim holds the pins, as Restoring does after RESTART DSHOT:
	// both tools that claim them are drawn disabled and do not act.
	checkTrue("another claim holds the pins",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	frames(2);
	// A refused row is drawn disabled, an outline with the background
	// inside, under its own status: no word for the claim on the row.
	char servoPins[16];
	snprintf(servoPins, sizeof(servoPins), "GP%u+%u", (unsigned)settings()->dshotPin,
	         (unsigned)settings()->kissPin);
	checkTrue("AM32 keeps its status, drawn disabled",
	          menuStatusIs(ctlRect(menuRow(&UI_AM32)), pins, C_DIM, C_BG));
	checkTrue("SERVO keeps its status, drawn disabled",
	          menuStatusIs(ctlRect(menuRow(&UI_SERVO)), servoPins, C_DIM, C_BG));
	char restarting[40];
	snprintf(restarting, sizeof(restarting), "DSHOT RESTARTING ON SIG GP%u",
	         (unsigned)settings()->dshotPin);
	checkTrue("the strip says why, in amber",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, restarting, C_AMBER, C_BG, 1));
	checkTrue("both rows are gated",
	          !ctlEnabled(menuRow(&UI_AM32)) && !ctlEnabled(menuRow(&UI_SERVO)));
	checkTrue("the rows that claim nothing still act",
	          ctlEnabled(menuRow(&UI_BEEP)) && ctlEnabled(menuRow(&UI_SDLOG)));
	checkTrue("SETTINGS, which changes the wiring a claim latched, does not",
	          !ctlEnabled(menuRow(&UI_SETTINGS)));
	checkControlTable("MENU, pins busy");
	{
		UiRect r = ctlRect(menuRow(&UI_SERVO));
		uint32_t rest = fakeRegionHash(r.x, r.y, r.w, r.h);
		pressId(menuRow(&UI_SERVO), IdExpect::Disabled);
		frames(1);
		checkTrue("a press on the busy SERVO row draws no pressed look",
		          fakeRegionHash(r.x, r.y, r.w, r.h) == rest);
		fakeRelease(); frames(1);
	}
	tapId(menuRow(&UI_SERVO), IdExpect::Disabled);
	frames(1);
	checkTrue("a tap on SERVO opens nothing", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("nor claims a pin", pinClaimHolder() == PinUser::Am32);
	tapId(menuRow(&UI_AM32), IdExpect::Disabled);
	frames(1);
	checkTrue("nor does a tap on AM32", uiCurrentScreen() == UiScreenId::Menu);
	pinClaimEnd(PinUser::Am32);
	frames(2);
	checkTrue("the claim ends: the pins are Free", pinClaimState() == ClaimState::Free);
	checkTrue("and SERVO acts again", ctlEnabled(menuRow(&UI_SERVO)));
	{
		char cap[40];
		snprintf(cap, sizeof(cap), "SIG GP%u  AUX GP%u  DSHOT%u", (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin, (unsigned)settings()->dshotKbaud);
		checkTrue("the strip names both pins and the rate again, dim",
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, cap, C_DIM, C_BG, 1));
	}

	uint32_t whole = fakeRegionHash(0, UI_HDR_H, GFX_W, GFX_H - UI_HDR_H);
	tapId(menuRow(&UI_SERVO));
	checkTrue("a tap on SERVO claims the pins for it", pinClaimHolder() == PinUser::Servo);
	frames(2);
	checkTrue("and opens it", fakeRegionHash(0, UI_HDR_H, GFX_W, GFX_H - UI_HDR_H) != whole);
	checkTrue("SERVO is the open screen", uiCurrentScreen() == UiScreenId::Servo);
	// Core1 binds the job, BACK asks it to let go, and core1 releases it.
	core1JobPoll();
	frames(1);
	tapId(UI_FIX(UI_BACK));
	core1JobPoll();
	frames(1);
	checkTrue("BACK: the unplug page", pinClaimUnplugDue());
	tapId(UI_FIX(ServoCtl::Restart));   // RESTART DSHOT
	frames(2);
	checkTrue("RESTART DSHOT: DShot back, MENU up",
	          pinClaimState() == ClaimState::Free &&
	          pixelIn(&UI_SERVO) == C_PANEL);
	checkTrue("RESTART DSHOT leaves to MENU", uiCurrentScreen() == UiScreenId::Menu);
	int beeps = fakeBeepRequests();
	tapId(menuRow(&UI_BEEP));
	checkInt("and MENU takes taps", fakeBeepRequests(), beeps + 1);
	fakeAdvance(400); frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/**
 * @brief Reversing a relative drag at a rail responds immediately.
 *
 * The classic relative-control failure: without re-anchoring, travel past an
 * end keeps accumulating and reversing does nothing until all of that phantom
 * distance has been unwound.
 */
static void testRailReAnchor() {
	section("Safety: relative drags re-anchor at the rails");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(4);
	holdToArm();
	tapId(UI_FIX(TesterCtl::Latch));           // LATCH: the gauge is relative

	uint16_t ceiling = settings()->maxThrottle;

	// The drags run from 3 px beyond the track's left end to 3 px beyond its
	// right end, which the gauge still takes.
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	const int ty = thr.y + thr.h / 2;
	const int lo = thr.x - 3, hi = thr.x + thr.w + 3;   // 5 and 235

	// Get to the ceiling first, and let go. One track width is exactly one
	// ceiling of travel at 100 % sensitivity, so this leaves the throttle
	// pinned with no overshoot yet.
	fakePress(lo, ty); frames(1);
	for (int x = lo; x <= hi; x += 5) { fakeHold(x, ty); frames(1); }
	fakeRelease(); frames(1);
	checkInt("pinned at the ceiling", fakeThrottle(), ceiling);

	// Now a second full-width drag from a throttle that is *already* at the
	// rail. That is what generates real overshoot: without re-anchoring, the
	// anchor keeps a whole ceiling of phantom travel, and reversing has to
	// unwind all of it before anything moves. Starting from below the rail --
	// which is what this test did at first -- overshoots by a single unit and
	// proves nothing.
	fakePress(lo, ty); frames(1);
	for (int x = lo; x <= hi; x += 5) { fakeHold(x, ty); frames(1); }
	checkInt("still exactly at the ceiling", fakeThrottle(), ceiling);
	for (int x = hi; x >= hi - 30; x -= 5) { fakeHold(x, ty); frames(1); }
	fakeRelease(); frames(1);
	checkTrue("a small reverse responds at once", fakeThrottle() < ceiling);
	checkTrue("and not by collapsing to zero", fakeThrottle() > 0);

	// Same at the bottom rail. The full width of the track, because at 100 %
	// sensitivity that is exactly one ceiling of travel -- a shorter drag from a
	// high starting throttle never reaches the rail at all, and would be testing
	// nothing.
	fakePress(hi, ty); frames(1);
	for (int x = hi; x >= lo; x -= 5) { fakeHold(x, ty); frames(1); }
	for (int i = 0; i < 4; i++) { fakeHold(lo, ty); frames(1); }
	checkInt("pinned at zero", fakeThrottle(), 0);
	for (int x = lo; x <= lo + 35; x += 5) { fakeHold(x, ty); frames(1); }
	fakeRelease(); frames(1);
	checkTrue("a small push off the bottom responds at once", fakeThrottle() > 0);
}


/**
 * @brief The board: named on the SETTINGS screen, and not offered as a choice.
 *
 * The image is built for one board. The row that names it, BUILT FOR under
 * BOARD at the end of the list, carries no control, so nothing on this
 * screen can change which pins the firmware drives.
 */
static void testBoardRow() {
	section("Board: the SETTINGS row");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);

	checkTrue("the descriptor is the 2.8\"'s", g_board == &BOARD_DESC_LCD_2_8);
	checkTrue("with its label",
	          g_board->label && strcmp(g_board->label, "RP2350-TOUCH-LCD-2.8") == 0);
	checkTrue("whose default ESC pin is free on it",
	          settingsPinFree(g_board->defaultDshotPin));

	// LAST RESTART shows in the screenshot at the end of the list: a power-on.
	fakeSetChipReset(DIAG_CR_HAD_POR);
	fakeSetWatchdog(0, false);
	diagCaptureRestart();
	enterSettings();
	uint8_t recorded = settings()->boardId;
	uint8_t pumpPin  = escTaskDshotPin();
	int configures   = fakeConfigureCount();

	// The board row has no control, so nothing in the table is on it. It is
	// the last row, so it is scrolled into view first. The presses go where
	// its toggle would be: the CONTRAST toggle's x, on BUILT FOR's row.
	revealRow((int)SettingsRowId::BuiltFor);
	checkTrue("BOARD has no control",
	          !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Toggle)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::BuiltFor, Plus)));
	checkTrue("and its row takes no press",
	          !ctlEnabled(UI_ROW(SettingsRowId::BuiltFor, Row)));
	int tx, ty, bx, boardY;
	ctlCentre(UI_ROW(SettingsRowId::Contrast, Toggle), &tx, &ty);
	ctlCentre(UI_ROW(SettingsRowId::BuiltFor, Row), &bx, &boardY);
	tap(tx, boardY);
	checkInt("tapping the board row changes no setting",
	         settings()->boardId, recorded);
	checkInt("nor the pump's pin", escTaskDshotPin(), pumpPin);
	checkInt("nor rebuilds the pump at all", fakeConfigureCount(), configures);

	// Held, too. The row carries no control at all, so neither the tap path nor
	// the held-repeat path has anything to reach.
	fakePress(tx, boardY); frames(1);
	for (int i = 0; i < 40; i++) { fakeHold(tx, boardY); frames(1); }
	fakeRelease(); frames(2);
	checkInt("holding it does nothing either", settings()->boardId, recorded);

	// The scroll range. 6 captions of 20 px and 14 rows at a 50 px pitch are
	// 820 px of content; the viewport, y 40 to 239, is 200 px, so the list
	// scrolls 0 px to 620 px. Dragged past its end, it stops at 620 px, with
	// VERSION's row, content y 650, at y 40 + 650 - 620 = 70, then the
	// DIAGNOSTICS caption, LAST RESTART's row at y 140 and UPTIME's, content
	// y 770, at y 190.
	const UiRect save0 = ctlRect(UI_FIX(SettingsCtl::Save));
	settingsDrag(-180);
	settingsDrag(-180);
	settingsDrag(-180);
	checkInt("at the end of the list, VERSION's row is at y 70",
	         ctlRect(UI_ROW(SettingsRowId::Version, Row)).y, 70);
	checkInt("LAST RESTART's at y 140",
	         ctlRect(UI_ROW(SettingsRowId::LastRestart, Row)).y, 140);
	checkInt("and UPTIME's, the last, at y 190",
	         ctlRect(UI_ROW(SettingsRowId::Uptime, Row)).y, 190);
	settingsDrag(-180);
	checkInt("and a drag past the end does not move it",
	         ctlRect(UI_ROW(SettingsRowId::Uptime, Row)).y, 190);
	// The rows' text: label at x 8, value right-aligned to x 232, both
	// 18 px down the 44 px row.
	checkTrue("VERSION's label", fakeTextIs(UI_MARGIN, 70 + 18, "VERSION", C_DIM, C_BG, 1));
	checkTrue("VERSION reads the firmware's version, " FW_VERSION_STRING,
	          fakeTextIs(232 - gfxTextW(FW_VERSION_STRING, 1), 70 + 18, FW_VERSION_STRING,
	                     C_GRID, C_BG, 1));
	checkTrue("VERSION has no control",
	          !ctlPresent(UI_ROW(SettingsRowId::Version, Minus)) &&
	          !ctlPresent(UI_ROW(SettingsRowId::Version, Toggle)) &&
	          !ctlEnabled(UI_ROW(SettingsRowId::Version, Row)));
	checkControlTable("SETTINGS, at the end");
	fakeDumpFrame("shot_settings_scrolled.ppm");
	// 120 px back: BUILT FOR's row, content y 600, at y 40 + 600 - 500 = 140.
	settingsDrag(120);
	checkInt("120 px back, BUILT FOR's row is at y 140",
	         ctlRect(UI_ROW(SettingsRowId::BuiltFor, Row)).y, 140);
	checkTrue("BUILT FOR names the board, right-aligned",
	          fakeTextIs(232 - gfxTextW(g_board->label, 1), 140 + 18, g_board->label,
	                     C_GRID, C_BG, 1));
	const UiRect save1 = ctlRect(UI_FIX(SettingsCtl::Save));
	checkTrue("HOLD TO SAVE does not move with the list, at y 268",
	          save1.y == 268 && save1.y == save0.y && save1.h == save0.h);
	// Back up: at 0 px, MOTOR POLES' row, content y 20, is at y 60.
	settingsDrag(180);
	settingsDrag(180);
	settingsDrag(180);
	checkInt("dragged back, MOTOR POLES' row is at y 60",
	         ctlRect(UI_ROW(SettingsRowId::Poles, Row)).y, 60);

	// And no save reboots: every setting applies live. This counter staying
	// still is what fails if a save that needs a restart is ever added.
	int reboots = fakeRebootCount();
	tapId(UI_ROW(SettingsRowId::Sig, Plus));
	pressId(UI_FIX(SettingsCtl::Save));
	holdId(UI_FIX(SettingsCtl::Save), 60);
	fakeRelease(); frames(2);
	checkTrue("an ordinary save lands", !settingsDirty());
	checkInt("and no save reboots", fakeRebootCount() - reboots, 0);
	checkInt("the saved block records the 2.8\"",
	         settings()->boardId, BOARD_ID_LCD_2_8);

	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SETTINGS reaches MENU", uiCurrentScreen() == UiScreenId::Menu);

	fakeFlashClear();
	settingsLoad();
}

/**
 * @brief The armed-save backstop: flash is never erased over a live motor.
 *
 * A save parks core1 for the erase, which stalls the DShot pump; a spinning
 * ESC would time out and cut. MENU force-disarms, so SETTINGS cannot be
 * reached armed through the UI today -- which is exactly why the backstop is
 * driven directly here: the day that routing changes is not a day anyone will
 * remember this rule, but this test will.
 */
static void testArmedSaveRefused() {
	section("SETTINGS: a save while armed is refused");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	holdToArm();
	checkTrue("armed to begin with", uiArmed());

	settings()->poles = (uint8_t)(settings()->poles == 12 ? 14 : 12);
	checkTrue("a change is pending", settingsDirty());

	uiSettingsEnter();
	int writes = fakeFlashWrites();
	int reboots = fakeRebootCount();

	// SETTINGS is entered here without the tester's help, so uiControls()
	// still reports the tester. SAVE's centre is read from SETTINGS' own table.
	int saveX = -1, saveY = -1;
	{
		UiControl tbl[UI_CONTROLS_MAX];
		uint8_t n = UI_SETTINGS.controls(tbl, UI_CONTROLS_MAX);
		for (uint8_t i = 0; i < n; i++) {
			if (tbl[i].id == UI_FIX(SettingsCtl::Save)) {
				saveX = tbl[i].rect.x + tbl[i].rect.w / 2;
				saveY = tbl[i].rect.y + tbl[i].rect.h / 2;
			}
		}
		checkTrue("SETTINGS' table has SAVE", saveX >= 0);
	}

	TouchState t;
	memset(&t, 0, sizeof(t));
	t.down = true; t.pressed = true;
	t.x = t.downX = saveX;
	t.y = t.downY = saveY;
	uiSettingsTick(&t);
	t.pressed = false;
	for (int i = 0; i < 60; i++) { fakeAdvance(UI_FRAME_MS); uiSettingsTick(&t); }

	checkTrue("still armed afterwards", uiArmed());
	checkTrue("the save was refused", settingsDirty());
	{
		// The note line says why, in red: NOT WHILE ARMED - DISARM TO SAVE.
		// It is drawn text, not a control, so its y is a probe.
		uint32_t shown = fakeRegionHash(0, SETTINGS_NOTE_Y, GFX_W, 8);
		gfxRect(0, SETTINGS_NOTE_Y, GFX_W, 8, C_BG);
		gfxText(8, SETTINGS_NOTE_Y, "NOT WHILE ARMED - DISARM TO SAVE", C_RED, 1);
		checkTrue("the note says NOT WHILE ARMED - DISARM TO SAVE",
		          fakeRegionHash(0, SETTINGS_NOTE_Y, GFX_W, 8) == shown);
	}
	checkTrue("nothing was stored", !settingsStored());
	checkInt("flash was never touched", fakeFlashWrites() - writes, 0);
	checkInt("and nothing rebooted", fakeRebootCount() - reboots, 0);

	t.down = false; t.released = true;
	uiSettingsTick(&t);

	// Disarm on the main screen and leave the module clean.
	pressId(UI_FIX(TesterCtl::Arm));
	fakeRelease(); frames(2);
	checkTrue("disarmed again", !uiArmed());
	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
}

/**
 * @brief A held control looks pressed, and stops looking pressed.
 *
 * The second half is the regression: the release frame used to change no
 * cached value, so nothing repainted and the pressed double-frame survived
 * until something else happened to invalidate the screen.
 */
static void testPressedLookClears() {
	section("Buttons: the pressed look appears, and clears on release");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	tapId(UI_FIX(TesterCtl::Menu));           // to MENU
	frames(2);
	checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);

	// A MENU row opens its tool on release, so the release frame is not
	// MENU's. The row is at rest when MENU comes back.
	const UiRect log = ctlRect(menuRow(&UI_SDLOG));
	uint32_t idle = fakeRegionHash(log.x, log.y, log.w, log.h);
	pressId(menuRow(&UI_SDLOG));
	checkTrue("a held MENU row draws its pressed state",
	          fakeRegionHash(log.x, log.y, log.w, log.h) != idle);
	fakeRelease(); frames(1);
	checkTrue("its release opens SD LOG", uiCurrentScreen() == UiScreenId::SdLog);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SD LOG reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	menuRow(&UI_SDLOG);                       // MENU opens at 0 px: drag to the row again
	checkTrue("and the row is at rest",
	          fakeRegionHash(log.x, log.y, log.w, log.h) == idle);

	// The pole stepper, on SETTINGS now. The pole *value* is drawn outside
	// MOTOR POLES -'s rectangle, so the hash of the rectangle isolates the
	// button's own look.
	tapId(menuRow(&UI_SETTINGS));
	frames(2);
	checkTrue("SETTINGS opens from MENU", uiCurrentScreen() == UiScreenId::Settings);
	const UiRect pm = ctlRect(UI_ROW(SettingsRowId::Poles, Minus));
	idle = fakeRegionHash(pm.x, pm.y, pm.w, pm.h);
	pressId(UI_ROW(SettingsRowId::Poles, Minus));
	checkTrue("a held stepper draws its pressed state",
	          fakeRegionHash(pm.x, pm.y, pm.w, pm.h) != idle);
	fakeRelease(); frames(1);
	checkTrue("and returns to rest on release",
	          fakeRegionHash(pm.x, pm.y, pm.w, pm.h) == idle);

	// The same rule on the wiring rows, which never had pressed states at all.
	const UiRect pin = ctlRect(UI_ROW(SettingsRowId::Sig, Minus));
	idle = fakeRegionHash(pin.x, pin.y, pin.w, pin.h);
	pressId(UI_ROW(SettingsRowId::Sig, Minus));
	checkTrue("SIG PIN's stepper draws its pressed state",
	          fakeRegionHash(pin.x, pin.y, pin.w, pin.h) != idle);
	fakeRelease(); frames(1);
	checkTrue("and returns to rest on release",
	          fakeRegionHash(pin.x, pin.y, pin.w, pin.h) == idle);

	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SETTINGS reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	// The old tap here was at (120, 309), the bottom BACK of an earlier layout,
	// and hit nothing. BACK is in the header now.
	tapId(UI_FIX(UI_BACK));              // BACK, from MENU to the tester screen
	frames(2);
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/**
 * @brief Screen changes leave nothing behind.
 *
 * The companion to the compile-time tiling asserts in ui.cpp: those prove
 * every row belongs to a region, this proves the transitions blank what the
 * incoming screen does not cover.
 */
static void testScreenChangeResidue() {
	section("Screen changes leave nothing behind");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);

	// The marker is amber in the footer band, y 260 to y 319: SETTINGS'
	// HOLD RESET label is amber there, and the tester's button row is not.
	// The tester may show amber elsewhere -- NO TELEMETRY, the IDLE
	// countdown -- so the assertion is against its *own* baseline: a round
	// trip through SETTINGS must not add any.
	auto footerAmber = []() {
		int n = 0;
		for (int y = 260; y < GFX_H; y++)
			for (int x = 0; x < GFX_W; x++) n += gfxBuffer()[y * GFX_W + x] == C_AMBER;
		return n;
	};
	int before = footerAmber();

	enterSettings();
	int onSettings = footerAmber();
	checkTrue("SETTINGS shows more amber in the footer band", onSettings > before);

	tapId(UI_FIX(UI_BACK));                 // BACK, to MENU
	frames(2);
	checkTrue("BACK from SETTINGS reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));                 // BACK, to the tester screen
	frames(3);
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	int after = footerAmber();
	checkTrue("no SETTINGS amber survives on the tester screen",
	          after <= before);
}


/**
 * @brief The tester's top right names whose voltage it shows, and HOLD TO ARM
 *        looks like what it does while a USB host holds the card.
 *
 * The large voltage is the board's own battery, not the ESC's pack: BOARD
 * under the DShot rate's column says so. While a host holds the card ARM
 * does not act (the control table and handleMainTouch() agree); it is drawn
 * disabled, and the throttle track says why.
 */
static void testTesterBoardAndCard() {
	section("Tester: BOARD names the voltage; ARM while a USB host holds the card");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	feedLiveTelemetry();
	frames(4);
	checkTrue("BOARD at x 140, y 14, dim on the status bar",
	          fakeTextIs(140, 14, "BOARD", C_DIM, C_PANEL, 1));
	// The voltage, scale 2 from y 7, is right-aligned to x 236. Between
	// BOARD's last column, x 168, and the voltage's first, at least 4 px of
	// panel: no glyph of either runs into the other.
	int vx = 169;
	while (vx < GFX_W) {
		bool ink = false;
		for (int y = 7; y < 21; y++) ink |= gfxBuffer()[y * GFX_W + vx] != C_PANEL;
		if (ink) break;
		vx++;
	}
	checkTrue("the voltage starts 4 px or more after BOARD", vx >= 169 + 4 && vx < GFX_W);
	checkTrue("BOARD keeps 4 px from 9.99V, the widest voltage text",
	          140 + gfxTextW("BOARD", 1) + 4 <= GFX_W - 4 - gfxTextW("9.99V", 2));

	// The text rounds once, to centivolts. Rounding the fraction alone gave
	// 3.996 V as "3.100V": a wrong value, six characters, over BOARD.
	char vt[12];
	uiBoardVoltText(3.994f, vt, sizeof(vt));
	checkStr("3.994 V reads 3.99V", vt, "3.99V");
	uiBoardVoltText(3.996f, vt, sizeof(vt));
	checkStr("3.996 V reads 4.00V", vt, "4.00V");
	uiBoardVoltText(9.996f, vt, sizeof(vt));
	checkStr("9.996 V, past what the divider can read, clamps to 9.99V", vt, "9.99V");
	uiBoardVoltText(0.0f, vt, sizeof(vt));
	checkStr("0 V reads 0.00V", vt, "0.00V");
	// Drawn: ADC readings either side of 3.995 V, and the ADC's top, 4095,
	// which the 200 kOhm/100 kOhm divider makes 9.90 V: the most this board
	// can show.
	struct { uint16_t raw; const char *want; } volts[] = {
		{ 1652, "3.99V" },   // 3.9938 V
		{ 1653, "4.00V" },   // 3.9963 V
		{ 4095, "9.90V" },
	};
	for (const auto &vc : volts) {
		fakeAdcSet(vc.raw);
		uiInit();
		frames(2);
		const int w = gfxTextW(vc.want, 2), x = GFX_W - 4 - w;
		char what[64];
		snprintf(what, sizeof(what), "ADC %u: %s, right-aligned at y 7", vc.raw, vc.want);
		checkTrue(what, fakeTextIs(x, 7, vc.want, C_TEXT, C_PANEL, 2));
		bool clear = true;
		for (int y = 3; y < 22; y++)
			for (int xx = 169; xx < x; xx++) clear = clear && gfxBuffer()[y * GFX_W + xx] == C_PANEL;
		snprintf(what, sizeof(what), "ADC %u: no ink between BOARD and the value", vc.raw);
		checkTrue(what, clear && fakeTextIs(140, 14, "BOARD", C_DIM, C_PANEL, 1));
	}
	fakeAdcSet(fakeAdcDefault());
	uiInit();
	feedLiveTelemetry();
	frames(4);

	const UiRect arm = ctlRect(UI_FIX(TesterCtl::Arm));
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	const int tx = thr.x + 8, ty = thr.y + (thr.h - 7) / 2;
	auto armFill = [&]() { return gfxBuffer()[(arm.y + arm.h / 2) * GFX_W + arm.x + 4]; };
	const int lx = arm.x + (arm.w - gfxTextW("HOLD TO ARM", 1)) / 2, ly = arm.y + (arm.h - 7) / 2;
	checkTrue("card free: HOLD TO ARM on the panel fill", armFill() == C_PANEL &&
	          fakeTextIs(lx, ly, "HOLD TO ARM", C_TEXT, C_PANEL, 1));
	checkTrue("and the track reads ARM TO ENABLE", fakeTextIs(tx, ty, "ARM TO ENABLE", C_DIM, C_PANEL, 1));

	fakeMscSet(MscState::Serving, 0);
	frames(2);
	checkTrue("card on USB: ARM does not act", !ctlEnabled(UI_FIX(TesterCtl::Arm)));
	checkTrue("and is drawn so: an outline with no fill", armFill() == C_BG);
	checkTrue("its label in the outline's grey", fakeTextIs(lx, ly, "HOLD TO ARM", C_GRID, C_BG, 1));
	checkTrue("the track says why: " UI_TESTER_CARD_TEXT,
	          fakeTextIs(tx, ty, UI_TESTER_CARD_TEXT, C_AMBER, C_PANEL, 1));
	checkInt("26 characters, 155 px", gfxTextW(UI_TESTER_CARD_TEXT, 1), 155);
	checkTrue("ending 8 px or more inside the track",
	          tx + gfxTextW(UI_TESTER_CARD_TEXT, 1) <= thr.x + thr.w - 8);
	fakeDumpFrame("shot_tester_card_held.ppm");
	// A press on it stays a press on nothing: no pressed look either.
	fakePress(arm.x + arm.w / 2, arm.y + arm.h / 2); frames(1);
	checkTrue("a press draws no pressed frame", armFill() == C_BG &&
	          gfxBuffer()[arm.y * GFX_W + arm.x + arm.w / 2] == C_GRID);
	fakeRelease(); frames(2);
	// The handover's first state holds the card too.
	fakeMscSet(MscState::Handover, 0);
	frames(2);
	checkTrue("handing over: still disabled", armFill() == C_BG &&
	          fakeTextIs(tx, ty, UI_TESTER_CARD_TEXT, C_AMBER, C_PANEL, 1));

	fakeMscSet(MscState::Idle, 0);
	frames(2);
	checkTrue("card back: HOLD TO ARM live again", armFill() == C_PANEL &&
	          fakeTextIs(lx, ly, "HOLD TO ARM", C_TEXT, C_PANEL, 1));
	checkTrue("and the track reads ARM TO ENABLE again",
	          fakeTextIs(tx, ty, "ARM TO ENABLE", C_DIM, C_PANEL, 1));
	holdToArm();
	checkTrue("and it arms", uiArmed());
	pressId(UI_FIX(TesterCtl::Arm));
	fakeRelease(); frames(2);
}

static bool tester3dShown();

/** @brief Every pixel of row @p y from x @p x0 to @p x1, inclusive, is @p col. */
static bool rowIs(int y, int x0, int x1, uint16_t col) {
	for (int x = x0; x <= x1; x++)
		if (gfxBuffer()[y * GFX_W + x] != col) return false;
	return true;
}

/** @brief The underline under @p text at (x, y): the text's width in C_TEXT, C_BG either side. */
static bool underlined(int x, int y, const char *text) {
	const int w = gfxTextW(text, 1);
	return rowIs(y, x, x + w - 1, C_TEXT) && rowIs(y, x - 1, x - 1, C_BG) &&
	       rowIs(y, x + w, x + w, C_BG);
}

/** @brief Row @p y from x @p x0 to @p x1 carries no C_TEXT pixel. */
static bool noTextOn(int y, int x0, int x1) {
	for (int x = x0; x <= x1; x++)
		if (gfxBuffer()[y * GFX_W + x] == C_TEXT) return false;
	return true;
}

/**
 * @brief The tester's readouts open SETTINGS, and BACK there returns to the
 *        tester (docs/design/menu-b.md, owner decision 4 and decisions 17
 *        and 20); LATCH names the throttle latch (decision 15 a).
 *
 * `14 POLES` and `MAX 20%` are underlined while a tap on them acts:
 * disarmed, and the pin claim Free, as MENU's SETTINGS row. The tap runs
 * the disarm MENU runs, so LATCH is off afterwards. Armed, the pad and the
 * track take the press and the readouts do nothing. The pole count gives
 * way to the 3D note, and so does its target.
 */
static void testTesterReadouts() {
	section("Tester: the readouts open SETTINGS; LATCH");

	fakeFlashClear();
	settingsLoad();
	uiInit();
	feedLiveTelemetry();
	frames(4);
	const uint16_t *fb = gfxBuffer();

	// --- the texts and their underlines ---
	checkTrue("14 POLES in the text colour at x 10, y 114",
	          fakeTextIs(10, 114, "14 POLES", C_TEXT, C_BG, 1));
	checkInt("14 POLES is 47 px", gfxTextW("14 POLES", 1), 47);
	checkTrue("underlined at y 122, x 10 to 56", underlined(10, 122, "14 POLES"));
	checkTrue("MAX 20% at x 88, y 236", fakeTextIs(88, 236, "MAX 20%", C_TEXT, C_BG, 1));
	checkTrue("underlined at y 244, x 88 to 128", underlined(88, 244, "MAX 20%"));
	checkTrue("the widest texts fit their targets: 28 POLES, MAX 100%",
	          10 + gfxTextW("28 POLES", 1) <= 4 + 72 && 88 + gfxTextW("MAX 100%", 1) <= 82 + 60);
	checkTrue("LATCH on the middle button, THROTTLE on the track's caption",
	          fakeTextIs(116 + (54 - gfxTextW("LATCH", 1)) / 2, 283 + (33 - 7) / 2, "LATCH",
	                     C_DIM, C_PANEL, 1) &&
	          fakeTextIs(8, 236, "THROTTLE", C_DIM, C_BG, 1));

	// --- the control table ---
	const UiRect poles = ctlRect(UI_FIX(TesterCtl::Poles));
	const UiRect max = ctlRect(UI_FIX(TesterCtl::Max));
	checkTrue("Poles at {4, 106, 72, 22}",
	          poles.x == 4 && poles.y == 106 && poles.w == 72 && poles.h == 22);
	checkTrue("Max at {82, 230, 60, 22}",
	          max.x == 82 && max.y == 230 && max.w == 60 && max.h == 22);
	checkTrue("both act while disarmed",
	          ctlEnabled(UI_FIX(TesterCtl::Poles)) && ctlEnabled(UI_FIX(TesterCtl::Max)));
	checkControlTable("tester, the readouts live");

	// --- a tap opens SETTINGS; BACK returns to the tester ---
	tapId(UI_FIX(TesterCtl::Poles));
	checkTrue("a tap on the pole count opens SETTINGS",
	          uiCurrentScreen() == UiScreenId::Settings);
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK from SETTINGS returns to the tester", uiCurrentScreen() == UiScreenId::Tester);
	checkTrue("which is drawn whole again", fakeTextIs(140, 14, "BOARD", C_DIM, C_PANEL, 1) &&
	          underlined(10, 122, "14 POLES") && underlined(88, 244, "MAX 20%"));
	tapId(UI_FIX(TesterCtl::Max));
	checkTrue("a tap on MAX opens SETTINGS", uiCurrentScreen() == UiScreenId::Settings);
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("BACK returns to the tester", uiCurrentScreen() == UiScreenId::Tester);

	// The origin goes with the open: SETTINGS from MENU returns to MENU.
	enterSettings();
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("SETTINGS opened from MENU returns to MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("and MENU to the tester", uiCurrentScreen() == UiScreenId::Tester);

	// A press that slides off the readout opens nothing.
	fakePress(poles.x + poles.w / 2, poles.y + poles.h / 2); frames(1);
	fakeHold(poles.x + poles.w / 2, poles.y - 20); frames(1);
	fakeRelease(); frames(1);
	checkTrue("a press that slides off opens nothing", uiCurrentScreen() == UiScreenId::Tester);

	// --- the tap runs MENU's disarm: LATCH goes off ---
	tapId(UI_FIX(TesterCtl::Latch));
	frames(1);
	checkTrue("LATCH on: the button fills blue, the caption reads " UI_TESTER_LATCHED_TEXT,
	          fb[(283 + 4) * GFX_W + 116 + 4] == C_BLUE &&
	          fakeTextIs(8, 236, UI_TESTER_LATCHED_TEXT, C_DIM, C_BG, 1));
	checkInt("LATCHED is 41 px", gfxTextW(UI_TESTER_LATCHED_TEXT, 1), 41);
	{
		const int mx = max.x + max.w / 2, my = max.y + max.h / 2;
		fakePress(mx, my); frames(2);
		fakeRelease(); frames(1);
		checkTrue("the tap opens SETTINGS", uiCurrentScreen() == UiScreenId::Settings);
		// The frame that opened SETTINGS paints none of the tester over it.
		bool clear = true;
		for (int y = 234; y < GFX_H; y++) clear = clear && rowIs(y, 0, GFX_W - 1, C_BG);
		checkTrue("and paints no tester row over the cleared frame", clear);
	}
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	checkTrue("back on the tester: LATCH is off",
	          fb[(283 + 4) * GFX_W + 116 + 4] == C_PANEL &&
	          fakeTextIs(8, 236, "THROTTLE", C_DIM, C_BG, 1));

	// --- armed: inert, no underline ---
	holdToArm();
	checkTrue("armed", uiArmed() && fakeArmed());
	checkTrue("armed: 14 POLES and MAX 20% stay, without the underline",
	          fakeTextIs(10, 114, "14 POLES", C_TEXT, C_BG, 1) && noTextOn(122, 4, 75) &&
	          fakeTextIs(88, 236, "MAX 20%", C_TEXT, C_BG, 1) && noTextOn(244, 82, 141));
	checkTrue("armed: neither acts",
	          !ctlEnabled(UI_FIX(TesterCtl::Poles)) && !ctlEnabled(UI_FIX(TesterCtl::Max)));
	checkControlTable("tester, armed, the readouts inert");
	tapId(UI_FIX(TesterCtl::Poles), IdExpect::Disabled);
	checkTrue("armed: a tap on the pole count opens nothing and keeps the arm",
	          uiCurrentScreen() == UiScreenId::Tester && uiArmed() && fakeArmed());
	tapId(UI_FIX(TesterCtl::Max), IdExpect::Disabled);
	frames(1);
	checkTrue("nor does a tap on MAX: the track took it, and sprang back",
	          uiCurrentScreen() == UiScreenId::Tester && uiArmed() && fakeThrottle() == 0);
	tapId(UI_FIX(TesterCtl::Arm));                     // DISARM
	frames(2);
	checkTrue("disarmed: the underlines are back",
	          !uiArmed() && underlined(10, 122, "14 POLES") && underlined(88, 244, "MAX 20%"));

	// --- another claim holds the pins: inert, no underline ---
	checkTrue("another claim holds the pins",
	          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	frames(2);
	checkTrue("claim busy: no underline", noTextOn(122, 4, 75) && noTextOn(244, 82, 141));
	checkTrue("claim busy: neither acts",
	          !ctlEnabled(UI_FIX(TesterCtl::Poles)) && !ctlEnabled(UI_FIX(TesterCtl::Max)));
	checkControlTable("tester, claim busy");
	tapId(UI_FIX(TesterCtl::Poles), IdExpect::Disabled);
	tapId(UI_FIX(TesterCtl::Max), IdExpect::Disabled);
	checkTrue("claim busy: a tap on either opens nothing", uiCurrentScreen() == UiScreenId::Tester);
	pinClaimEnd(PinUser::Am32);
	frames(2);
	checkTrue("the claim ends: both act and are underlined again",
	          ctlEnabled(UI_FIX(TesterCtl::Poles)) && underlined(10, 122, "14 POLES") &&
	          underlined(88, 244, "MAX 20%"));

	// --- the 3D note takes the pole count's row, and its target ---
	s_esc3d = true;
	uiInit();
	frames(2);
	checkTrue("3D note: no pole count, no Poles control",
	          tester3dShown() && !ctlPresent(UI_FIX(TesterCtl::Poles)));
	fakePress(poles.x + poles.w / 2, poles.y + poles.h / 2); frames(2);
	fakeRelease(); frames(1);
	checkTrue("a tap on the note opens nothing", uiCurrentScreen() == UiScreenId::Tester);
	tapId(UI_FIX(TesterCtl::Max));
	checkTrue("MAX still opens SETTINGS", uiCurrentScreen() == UiScreenId::Settings);
	frames(2);
	tapId(UI_FIX(UI_BACK));
	frames(2);
	s_esc3d = false;
	uiInit();
	frames(2);
}

/**
 * @brief The text probes of fakes.h in high contrast, where every glyph is
 *        one row taller: a blank region reads false and is left as it was;
 *        a drawn text reads true.
 */
static void testTextProbes() {
	section("Test helpers: the text probes in high contrast");

	themeSet(Theme::HighContrast);
	gfxFill(C_BG);
	const uint32_t blank = fakeRegionHash(0, 0, GFX_W, 80);
	checkTrue("a blank region is not the text", !fakeTextIs(20, 20, "ABC", C_TEXT, C_BG, 1));
	checkTrue("and the probe leaves it blank, its bold row included",
	          fakeRegionHash(0, 0, GFX_W, 80) == blank);
	checkTrue("a blank region is not centred text either",
	          !fakeTextCentredIs(40, "ABC", C_TEXT, C_BG, 2) && fakeRegionHash(0, 0, GFX_W, 80) == blank);
	gfxText(20, 20, "ABC", C_TEXT, 1);
	gfxTextCenter(40, "ABC", C_TEXT, 2);
	const uint32_t drawn = fakeRegionHash(0, 0, GFX_W, 80);
	checkTrue("drawn text is the text", fakeTextIs(20, 20, "ABC", C_TEXT, C_BG, 1) &&
	          fakeTextCentredIs(40, "ABC", C_TEXT, C_BG, 2));
	checkTrue("and the probe leaves it as drawn", fakeRegionHash(0, 0, GFX_W, 80) == drawn);
	checkTrue("another text is not", !fakeTextIs(20, 20, "ABD", C_TEXT, C_BG, 1));
	checkTrue("text wider than the panel is never centred on it",
	          !fakeTextCentredIs(60, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", C_TEXT, C_BG, 2));
	themeSet(Theme::Dark);
	gfxFill(C_BG);
}

/**
 * @brief The splash: the firmware's version under the board's name, and the
 *        ESC pin named before DShot starts on it.
 */
static void testSplash() {
	section("Splash: the version and the ESC pin");

	fakeFlashClear();
	settingsLoad();
	uiDrawSplash();
	checkTrue("the board's name keeps its line, y 248",
	          fakeTextCentredIs(248, g_board->label, C_GRID, C_BG, 1));
	checkTrue("VERSION " FW_VERSION_STRING " under it, y 260",
	          fakeTextCentredIs(260, "VERSION " FW_VERSION_STRING, C_GRID, C_BG, 1));
	checkTrue("the credit keeps its line", fakeTextCentredIs(216, "JuWi made", C_CYAN, C_BG, 2));
	char want[48];
	snprintf(want, sizeof(want), UI_SPLASH_PIN_FMT, (unsigned)settings()->dshotPin);
	checkInt("the defaults put the ESC on the board's default pin",
	         settings()->dshotPin, g_board->defaultDshotPin);
	if (g_board->defaultDshotPin == 0)
		checkStr("the warning names the ESC pin", want, "UNPLUG ANY SERVO FROM GP0 NOW");
	checkTrue("in amber, y 288", fakeTextCentredIs(288, want, C_AMBER, C_BG, 1));
	int x0 = 0, x1 = 0;
	checkTrue("the warning stays 8 px or more from both edges",
	          fakeInkExtent(288, 7, C_BG, &x0, &x1) && x0 >= UI_MARGIN && x1 < GFX_W - UI_MARGIN);
	checkTrue("and nothing else is drawn on its rows or the 20 px above",
	          fakeInkExtent(268, 20, C_BG, &x0, &x1) == false);
	fakeDumpFrame("shot_splash.ppm");

	// The pin follows the settings: the ESC on the other free GPIO.
	const uint8_t other = settingsNextPin(settings()->dshotPin, +1);
	checkTrue("the board offers another pin", other != settings()->dshotPin);
	settings()->dshotPin = other;
	uiDrawSplash();
	snprintf(want, sizeof(want), UI_SPLASH_PIN_FMT, (unsigned)other);
	checkTrue("SIG PIN moved: the warning names the new pin",
	          fakeTextCentredIs(288, want, C_AMBER, C_BG, 1));
	snprintf(want, sizeof(want), UI_SPLASH_PIN_FMT, 29u);
	checkTrue("a two-digit pin fits too: 30 characters, 179 px",
	          gfxTextW(want, 1) == 179 && gfxTextW(want, 1) <= GFX_W - 2 * UI_MARGIN);

	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
}

/** @brief Open AM32 CONFIG from the tester, and run @p n frames. */
static void enterAm32(int n) {
	tapId(UI_FIX(TesterCtl::Menu));
	frames(2);
	tapId(menuRow(&UI_AM32));
	checkTrue("AM32 CONFIG opens", uiCurrentScreen() == UiScreenId::Am32);
	frames(n);
}

/** @brief BACK from AM32 CONFIG to MENU, then from MENU to the tester. */
static void leaveAm32ToTester() {
	tapId(UI_FIX(UI_BACK));
	frames(2);
	tapId(UI_FIX(UI_BACK));
	checkTrue("back on the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/** @brief MENU's strip shows the run cue on its second line, in amber. */
static bool menuRunCueShown() {
	return fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_RUN_CUE, UI_COL_WARN, C_BG, 1);
}

/** @brief The tester shows @ref AM32_UI_TESTER_3D on the RPM area's bottom row. */
static bool tester3dShown() {
	return fakeTextIs(10, 114, AM32_UI_TESTER_3D, C_AMBER, C_BG, 1);
}

/** @brief The tester's bottom row as it is without the 3D note: the pole count. */
static bool testerPolesShown() {
	char p[16];
	snprintf(p, sizeof(p), "%d POLES", settings()->poles);
	return fakeTextIs(10, 114, p, C_TEXT, C_BG, 1);
}

/** @brief Select 3D MODE in the open list and step it to @p on. */
static void am32Set3d(bool on) {
	const int v = uiRowOf("3D MODE");
	uiRowY("3D MODE"); frames(1);
	tapId(UI_ROW(v, Row));
	tapId(UI_FIX(on ? Am32Ctl::EditPlus : Am32Ctl::EditMinus));
}

/** @brief Hold WRITE for 50 frames, then dismiss the result with OK. */
static void am32HoldWrite() {
	int wx, wy;
	ctlCentre(UI_FIX(Am32Ctl::Write), &wx, &wy);
	pressId(UI_FIX(Am32Ctl::Write));
	for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
	fakeRelease(); frames(3);
	tapId(UI_FIX(Am32Ctl::Ok));
}

/**
 * @brief The tester's 3D note: set by a list read with 3D MODE on and by a
 *        write of it on that the ESC accepted, verified or not; cleared by a
 *        list read or a verified write with it off; kept by an edit not
 *        written, a refused write, a failed read, a read that opens no list
 *        and the read-only view; shown armed and disarmed and beside NO
 *        TELEMETRY, with SWIPE ACTIVE in its place while the pad is engaged.
 */
static void testAm32TesterNote() {
	EscTelemetry quiet;
	memset(&quiet, 0, sizeof(quiet));
	fakeSetTelemetry(&quiet);
	uint8_t keep[AM32_SETTINGS_SIZE];
	for (int i = 0; i < AM32_SETTINGS_SIZE; i++) keep[i] = fakeEscByte(i);
	s_esc3d = false;

	section("Tester: the 3D note");
	{
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
		frames(2);
		checkTrue("no AM32 read yet: the pole count, no note",
		          testerPolesShown() && !tester3dShown());
		checkTrue("35 characters, 209 px, from x 10 to the RPM label's right edge, x 226",
		          gfxTextW(AM32_UI_TESTER_3D, 1) == 209 && 10 + gfxTextW(AM32_UI_TESTER_3D, 1) <= 226);

		enterAm32(6);
		checkTrue("a read with 3D MODE off: no note", s_state == S_LIST && !uiAm32Esc3d());
		am32Set3d(true);
		leaveAm32ToTester();
		frames(2);
		checkTrue("3D MODE edited on, not written: no note", !uiAm32Esc3d() && !tester3dShown());

		enterAm32(6);
		am32Set3d(true);
		fakeAm32FailWrites(1);
		am32HoldWrite();
		checkTrue("a write of 3D MODE on that the ESC refuses: no note",
		          !s_verified && fakeEscByte(AM32_OFF_BI_DIRECTION) == 0 && !uiAm32Esc3d());
		fakeAm32FailReads(3);
		am32HoldWrite();
		checkTrue("accepted, its read-back failed: the note is set, fail-safe",
		          !s_verified && fakeEscByte(AM32_OFF_BI_DIRECTION) == 1 && uiAm32Esc3d());
		am32HoldWrite();
		checkTrue("the write verifies with 3D MODE on: the note stands",
		          s_verified && fakeEscByte(AM32_OFF_BI_DIRECTION) == 1 && uiAm32Esc3d());
		leaveAm32ToTester();
		frames(2);
		checkTrue("the tester: " AM32_UI_TESTER_3D " in amber at x 10, y 114", tester3dShown());
		checkTrue("the pole count and the swipe hint give way",
		          !testerPolesShown() &&
		          !fakeTextIs(226 - gfxTextW("SWIPE = THROTTLE", 1), 114, "SWIPE = THROTTLE", C_GRID, C_BG, 1));
		checkTrue("NO TELEMETRY stays on its row, y 102",
		          fakeTextIs(10, 102, "NO TELEMETRY", C_AMBER, C_BG, 1));
		holdToArm();
		feedLiveTelemetry();
		frames(2);
		checkTrue("armed: the note stays", fakeArmed() && tester3dShown());
		const UiRect pad = ctlRect(UI_FIX(TesterCtl::Pad));
		const int px = pad.x + pad.w / 2, pb = pad.y + pad.h;
		const uint16_t before = fakeThrottle();
		fakePress(px, pb - 40); frames(1);
		for (int y = pb - 40; y >= pb - 70; y -= 6) { fakeHold(px, y); frames(1); }
		checkTrue("the pad engaged: SWIPE ACTIVE in green in place of the note",
		          !tester3dShown() && !testerPolesShown() &&
		          fakeTextIs(226 - gfxTextW("SWIPE ACTIVE", 1), 114, "SWIPE ACTIVE", C_LIME, C_BG, 1));
		checkTrue("and the swipe raised the throttle, as without the note",
		          fakeThrottle() > before);
		fakeRelease(); frames(2);
		checkTrue("released: the note returns, no SWIPE ACTIVE",
		          tester3dShown() &&
		          !fakeTextIs(226 - gfxTextW("SWIPE ACTIVE", 1), 114, "SWIPE ACTIVE", C_LIME, C_BG, 1));
		fakeDumpFrame("shot_tester_3d.ppm");
		tapId(UI_FIX(TesterCtl::Arm));                         // DISARM
		frames(2);
		checkTrue("disarmed: the note stays", !fakeArmed() && tester3dShown());
		fakeSetTelemetry(&quiet);

		// A read of the ESC as it is: 3D MODE on, the note kept; edited off
		// and not written, kept; written off and verified, cleared.
		enterAm32(6);
		checkTrue("a read with 3D MODE on: the note stands", uiAm32Esc3d());
		am32Set3d(false);
		checkTrue("edited off, not written: the note stands", uiAm32Esc3d());
		fakeAm32SkewWrites(1);
		am32HoldWrite();
		checkTrue("a write that reads back different: the note stands", !s_verified && uiAm32Esc3d());
		am32HoldWrite();
		checkTrue("written off and verified: the note goes",
		          s_verified && fakeEscByte(AM32_OFF_BI_DIRECTION) == 0 && !uiAm32Esc3d());
		leaveAm32ToTester();
		frames(2);
		checkTrue("the tester: the pole count again, no note", testerPolesShown() && !tester3dShown());

		// A write of 3D MODE on that the ESC accepts and that reads back
		// different sets the note too; a verified write of it off clears it.
		enterAm32(6);
		am32Set3d(true);
		fakeAm32SkewWrites(1);
		am32HoldWrite();
		checkTrue("3D MODE on, accepted, read back different: the note is set",
		          !s_verified && fakeEscByte(AM32_OFF_BI_DIRECTION) == 1 && uiAm32Esc3d());
		fakeAm32FailReads(1000);
		leaveAm32ToTester();
		enterAm32(6);
		checkTrue("a read that fails: the note stands",
		          s_state != S_LIST && strncmp(s_status, "READ FAILED", 11) == 0 && uiAm32Esc3d());
		fakeAm32FailReads(0);
		leaveAm32ToTester();
		enterAm32(6);
		am32Set3d(false);
		am32HoldWrite();
		checkTrue("written off and verified: the note goes again",
		          s_verified && fakeEscByte(AM32_OFF_BI_DIRECTION) == 0 && !uiAm32Esc3d());
		leaveAm32ToTester();

		// Another ESC, or one set elsewhere: the read decides.
		fakeEscSet(AM32_OFF_BI_DIRECTION, 1);
		enterAm32(6);
		checkTrue("an ESC read with 3D MODE on: the note is set", uiAm32Esc3d());
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
		enterAm32(6);
		checkTrue("one read with 3D MODE off: the note goes", !uiAm32Esc3d());
		// A read that does not open the list leaves it as it was.
		fakeEscSet(AM32_OFF_BI_DIRECTION, 1);
		leaveAm32ToTester();
		enterAm32(6);
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
		fakeAm32Blank(true);
		enterAm32(6);
		checkTrue("NO VALID SETTINGS: the note stands", s_state == S_CONNECT && uiAm32Esc3d());
		// Left while the page still finds no valid settings, so no retry
		// reads the ESC before BACK.
		leaveAm32ToTester();
		fakeAm32Blank(false);
		// The read-only view of a layout not known does not read byte 0x12
		// as 3D MODE: it neither clears nor sets the note.
		const uint8_t lay = fakeEscByte(AM32_OFF_LAYOUT_REVISION);
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 5);
		checkTrue("the note before the read-only view", uiAm32Esc3d());
		enterAm32(6);
		checkTrue("the read-only view, byte 0x12 at 0: the note stands", s_state == S_VIEW && uiAm32Esc3d());
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, lay);
		enterAm32(6);
		checkTrue("a list read with 3D MODE off: no note", s_state == S_LIST && !uiAm32Esc3d());
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 5);
		fakeEscSet(AM32_OFF_BI_DIRECTION, 1);
		enterAm32(6);
		checkTrue("the read-only view, byte 0x12 at 1: no note", s_state == S_VIEW && !uiAm32Esc3d());
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, lay);
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
	}

	// The skewed writes above flip byte 0x1B in the ESC: every byte back.
	for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, keep[i]);
	s_esc3d = false;
	frames(2);
}

/** @brief A card in the slot, idle, nothing on it. */
static void am32Card() {
	fakeFatReset(1u << 20);
	SdLogStatus idle;
	memset(&idle, 0, sizeof(idle));
	idle.state = SdLogState::Idle;
	fakeSdLogSet(&idle);
	fakeMscSet(MscState::Idle, 0);
}

/** @brief No card in the slot. */
static void am32NoCard() {
	SdLogStatus none;
	memset(&none, 0, sizeof(none));
	none.state = SdLogState::NoCard;
	fakeSdLogSet(&none);
}

/** @brief The strip's two lines read @p l1 and @p l2 in amber. */
static bool stripNoteIs(const char *l1, const char *l2) {
	return fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, l1, UI_COL_WARN, C_BG, 1) &&
	       fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, l2, UI_COL_WARN, C_BG, 1);
}

/** @brief The board's voltage at @p volts, held for enough frames to settle. */
static void boardVolts(float volts) {
	fakeAdcSet((uint16_t)(volts / 3.0f / 3.3f * 4095.0f + 0.5f));
	frames(80);
}

/** @brief Fields of AM32_FIELDS shown for the ESC's block with byte 0x01 at @p layout. */
static int shownAt(uint8_t layout) {
	uint8_t b[AM32_SETTINGS_SIZE];
	for (int i = 0; i < AM32_SETTINGS_SIZE; i++) b[i] = fakeEscByte(i);
	b[AM32_OFF_LAYOUT_REVISION] = layout;
	int n = 0;
	for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) n += am32FieldShown(&AM32_FIELDS[i], b);
	return n;
}

/** @brief The read-only view's hex line @p k reads the fake ESC's bytes, at x 6. */
static bool hexLineShown(int k) {
	char line[64];
	int n = snprintf(line, sizeof(line), "%02X ", k * 8);
	for (int i = 0; i < 8 && k * 8 + i < AM32_SETTINGS_SIZE; i++)
		n += snprintf(line + n, sizeof(line) - n, "%02X ", fakeEscByte(k * 8 + i));
	return fakeTextIs(UI_MARGIN, LIST_Y0 + k * HEX_ROW_H, line, C_DIM, C_BG, 1);
}

/** @brief A tap at the middle of @p r, a rectangle no control of this frame owns. */
static void tapRect(const UiRect &r) { tap(r.x + r.w / 2, r.y + r.h / 2); }

/**
 * @brief A plausible block in a layout the table does not know, 1 or 5 to 8,
 *        opens the read-only view: the bytes in hex, NOT VERIFIED, the backup
 *        on the card. No control leads to the write, and the write itself
 *        refuses the bytes.
 */
static void testAm32ReadOnly() {
	EscTelemetry quiet;
	memset(&quiet, 0, sizeof(quiet));
	fakeSetTelemetry(&quiet);
	uint8_t keep[AM32_SETTINGS_SIZE];
	for (int i = 0; i < AM32_SETTINGS_SIZE; i++) keep[i] = fakeEscByte(i);

	section("AM32 read-only: layout 5, the bytes, a backup, no write");
	{
		am32Card();
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 5);
		enterAm32(6);
		checkTrue("the read-only view opens", s_state == S_VIEW && s_hexView);
		checkInt("no field is shown in layout 5", shownAt(5), 0);
		checkInt("nor in 1, 6, 7 or 8", shownAt(1) + shownAt(6) + shownAt(7) + shownAt(8), 0);
		checkTrue("layouts 2 to 4 show fields", shownAt(2) > 0 && shownAt(3) > 0 && shownAt(4) > 0);
		checkInt("the list holds no field", s_visibleCount, 0);
		checkTrue("the strip: LAYOUT 5 NOT KNOWN: READ-ONLY. / NOT VERIFIED. BACKUP AM001",
		          stripNoteIs("LAYOUT 5 NOT KNOWN: READ-ONLY.", "NOT VERIFIED. BACKUP AM001"));
		bool hex = true;
		for (int k = 0; k < HEX_LINES; k++) hex = hex && hexLineShown(k);
		checkTrue("the 48 bytes in hex, 8 a line, from the text margin, x 8", hex);
		checkTrue(AM32_UI_RO_BYTES ", grey", fakeTextCentredIs(EDIT_Y + 12, AM32_UI_RO_BYTES, C_GRID, C_BG, 1));
		checkTrue(AM32_UI_RO_TABLE ", grey", fakeTextCentredIs(EDIT_Y + 26, AM32_UI_RO_TABLE, C_GRID, C_BG, 1));
		int x0 = 0, x1 = 0;
		checkTrue("the strip stays inside the panel's 8 px margins",
		          fakeInkExtent(UI_STRIP_LINE1_Y, 18, C_BG, &x0, &x1) && x0 >= UI_MARGIN &&
		          x1 < GFX_W - UI_MARGIN);
		static char want[AM32_BACKUP_MAX], got[AM32_BACKUP_MAX];
		size_t wantLen = am32BackupFormat(s_entry, am32BlEscTypeName(), am32BlEepromAddr(),
		                                  want, sizeof(want));
		int32_t gotLen = fakeFatGet("BACKUP/AM001.TXT", got, sizeof(got));
		checkTrue("the backup is on the card as the view opened",
		          gotLen == (int32_t)wantLen && wantLen > 0 && memcmp(got, want, wantLen) == 0);
		checkTrue("it says the fields are not read, and holds no field line",
		          strstr(want, "Layout not known: fields not read, not verified\n") != nullptr &&
		          strstr(want, " = ") == nullptr && strstr(want, "bytes 48\n") != nullptr);
		UiControl tbl[UI_CONTROLS_MAX];
		checkInt("BACK is the one control", uiControls(tbl, UI_CONTROLS_MAX), 1);
		checkTrue("and it is BACK", ctlPresent(UI_FIX(UI_BACK)) && !ctlPresent(UI_FIX(Am32Ctl::Write)) &&
		          !ctlPresent(UI_FIX(Am32Ctl::Revert)) && !ctlPresent(UI_FIX(Am32Ctl::Hex)));
		checkControlTable("AM32 read-only");
		fakeDumpFrame("shot_am32_readonly.ppm");

		// Every place a control of the list stands: nothing selects, steps,
		// toggles or writes.
		int writes = fakeAm32Writes();
		uint8_t before[AM32_SETTINGS_SIZE];
		memcpy(before, s_eeprom, sizeof(before));
		fakePress(BTN_WRITE.x + BTN_WRITE.w / 2, BTN_WRITE.y + BTN_WRITE.h / 2);
		frames(1);
		for (int i = 0; i < 60; i++) {
			fakeHold(BTN_WRITE.x + BTN_WRITE.w / 2, BTN_WRITE.y + BTN_WRITE.h / 2);
			frames(1);
		}
		fakeRelease();
		frames(2);
		tapRect(BTN_REVERT);
		tapRect(BTN_HEX);
		tapRect(BTN_MINUS);
		tapRect(BTN_PLUS);
		tap(GFX_W / 2, LIST_Y0 + 5);
		swipe(30, 220, LIST_Y0 + 20, 10);
		checkTrue("a hold where WRITE stands, taps on REVERT, HEX, - and +, a swipe: "
		          "the view as it was", s_state == S_VIEW && s_hexView && s_selected < 0 &&
		          memcmp(before, s_eeprom, sizeof(before)) == 0);
		checkInt("no write went to the ESC", fakeAm32Writes(), writes);

		// The write itself refuses the bytes, whatever reaches it: an edit
		// made behind the screen's back and the write state set directly.
		s_eeprom[0x1B] ^= 0x02;
		uint8_t esc = fakeEscByte(0x1B);
		s_state = S_WRITING;
		snprintf(s_status, sizeof(s_status), "WRITING...");
		frames(2);
		checkTrue("the write refuses a layout not known: nothing to the ESC",
		          fakeAm32Writes() == writes && fakeEscByte(0x1B) == esc &&
		          strcmp(s_status, AM32_UI_RO_REFUSED) == 0);
		checkTrue(AM32_UI_RO_REFUSED " on the result page",
		          fakeTextCentredIs(RESULT_Y, AM32_UI_RO_REFUSED, C_TEXT, C_BG, 1));
		checkTrue("no second backup", !fakeFatExists("BACKUP/AM002.TXT"));
		tapId(UI_FIX(Am32Ctl::Ok));
		checkTrue("OK: the read-only view again", s_state == S_VIEW);
		s_eeprom[0x1B] ^= 0x02;
		leaveAm32ToTester();
	}

	section("AM32 read-only: no card, a card that refuses");
	{
		am32NoCard();
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 8);
		enterAm32(6);
		checkTrue("layout 8, no card: read-only, no backup", s_state == S_VIEW &&
		          stripNoteIs("LAYOUT 8 NOT KNOWN: READ-ONLY.", "NOT VERIFIED. NO CARD: NO BACKUP."));
		int x0 = 0, x1 = 0;
		checkTrue("the longest second line stays inside the panel's 8 px margins",
		          fakeInkExtent(UI_STRIP_LINE2_Y, 8, C_BG, &x0, &x1) && x0 >= UI_MARGIN &&
		          x1 < GFX_W - UI_MARGIN);
		leaveAm32ToTester();
		am32Card();
		fakeFatWriteProtect(true);
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 6);
		enterAm32(6);
		checkTrue("a card that refuses: BACKUP FAILED.", s_state == S_VIEW &&
		          stripNoteIs("LAYOUT 6 NOT KNOWN: READ-ONLY.", "NOT VERIFIED. BACKUP FAILED."));
		fakeFatWriteProtect(false);
		leaveAm32ToTester();
	}

	section("AM32 read-only: layout 1");
	{
		am32Card();
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 1);
		enterAm32(6);
		checkTrue("layout 1 opens the read-only view", s_state == S_VIEW && s_hexView);
		checkTrue("the note", stripNoteIs("LAYOUT 1 NOT KNOWN: READ-ONLY.", "NOT VERIFIED. BACKUP AM001"));
		static char got[AM32_BACKUP_MAX];
		checkTrue("the backup holds no field line",
		          fakeFatGet("BACKUP/AM001.TXT", got, sizeof(got)) > 0 &&
		          strstr(got, "Layout not known: fields not read, not verified\n") != nullptr &&
		          strstr(got, " = ") == nullptr);
		leaveAm32ToTester();
	}

	section("AM32: layouts 2 to 4 open the list, a block that is not plausible does not open");
	{
		for (uint8_t lay = 2; lay <= 4; lay++) {
			am32Card();
			fakeEscSet(AM32_OFF_LAYOUT_REVISION, lay);
			enterAm32(6);
			char what[64];
			snprintf(what, sizeof(what), "layout %u: the list, WRITE, no backup yet", lay);
			checkTrue(what, s_state == S_LIST && !s_hexView && ctlPresent(UI_FIX(Am32Ctl::Write)) &&
			          !fakeFatExists("BACKUP/AM001.TXT"));
			leaveAm32ToTester();
		}
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 9);
		enterAm32(6);
		checkTrue("layout 9 is not plausible: the connect page explains",
		          s_state == S_CONNECT && strcmp(s_status, "NO VALID SETTINGS") == 0 &&
		          fakeTextCentredIs(196, AM32_UI_NO_SETTINGS_1, C_AMBER, C_BG, 1));
		leaveAm32ToTester();
	}
	am32NoCard();
	for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, keep[i]);
}

/**
 * @brief The board's voltage in the red band refuses an AM32 write: the
 *        result page says why and what to do. Amber and NoCell refuse
 *        nothing.
 */
static void testAm32BoardLow() {
	section("AM32 write: refused while the BOARD voltage is red");
	EscTelemetry quiet;
	memset(&quiet, 0, sizeof(quiet));
	fakeSetTelemetry(&quiet);
	am32NoCard();
	enterAm32(6);
	checkTrue("the list is open", s_state == S_LIST);
	boardVolts(3.20f);
	checkTrue("3.20 V on the AM32 screen: the band is red", uiBoardCellNow() == BoardCell::Critical);
	const int vp = uiRowOf("POLES");
	uiRowY("POLES"); frames(1);
	tapId(UI_ROW(vp, Row));
	tapId(UI_FIX(Am32Ctl::EditMinus));
	uint8_t want = uiByte(0x1B);
	checkTrue("a change to write", want != fakeEscByte(0x1B));
	int writes = fakeAm32Writes();
	pressId(UI_FIX(Am32Ctl::Write));
	holdId(UI_FIX(Am32Ctl::Write), 50);
	fakeRelease(); frames(3);
	checkTrue("held: nothing written", fakeEscByte(0x1B) != want && fakeAm32Writes() == writes &&
	          strcmp(s_status, UI_BOARD_LOW_1) == 0);
	checkTrue(UI_BOARD_LOW_1 ", y 140", fakeTextCentredIs(140, UI_BOARD_LOW_1, C_TEXT, C_BG, 1));
	checkTrue("then " UI_BOARD_LOW_2 " in amber, y 170",
	          fakeTextCentredIs(170, UI_BOARD_LOW_2, UI_COL_WARN, C_BG, 1));
	fakeDumpFrame("shot_am32_board_low.ppm");
	tapId(UI_FIX(Am32Ctl::Ok));
	checkTrue("OK: the list, the change kept", s_state == S_LIST && uiByte(0x1B) == want);
	boardVolts(3.40f);
	checkTrue("3.40 V: amber", uiBoardCellNow() == BoardCell::Low);
	pressId(UI_FIX(Am32Ctl::Write));
	holdId(UI_FIX(Am32Ctl::Write), 50);
	fakeRelease(); frames(3);
	checkTrue("amber writes", fakeEscByte(0x1B) == want && strcmp(s_status, "WRITE VERIFIED") == 0);
	tapId(UI_FIX(Am32Ctl::Ok));
	boardVolts(1.0f);
	checkTrue("1.00 V: no cell to judge", uiBoardCellNow() == BoardCell::NoCell);
	tapId(UI_FIX(Am32Ctl::EditPlus));
	want = uiByte(0x1B);
	pressId(UI_FIX(Am32Ctl::Write));
	holdId(UI_FIX(Am32Ctl::Write), 50);
	fakeRelease(); frames(3);
	checkTrue("NoCell writes", fakeEscByte(0x1B) == want && strcmp(s_status, "WRITE VERIFIED") == 0);
	tapId(UI_FIX(Am32Ctl::Ok));
	boardVolts(3.92f);
	checkTrue("3.92 V: the text colour", uiBoardCellNow() == BoardCell::Ok);
	tapId(UI_FIX(Am32Ctl::EditMinus));
	want = uiByte(0x1B);
	pressId(UI_FIX(Am32Ctl::Write));
	holdId(UI_FIX(Am32Ctl::Write), 50);
	fakeRelease(); frames(3);
	checkTrue("Ok writes", fakeEscByte(0x1B) == want && strcmp(s_status, "WRITE VERIFIED") == 0);
	tapId(UI_FIX(Am32Ctl::Ok));
	fakeAdcSet(fakeAdcDefault());
	frames(80);
	leaveAm32ToTester();
}

/**
 * @brief Decision 14 of docs/design/menu-b.md on AM32 CONFIG: the editor's
 *        44 px - and + at the button edge, the footer at y 268, and the
 *        result's OK as the wide button, 6/228 at y 268, which takes only a
 *        press that lands after the write ended: the finger that ended the
 *        hold lifts on it.
 */
static void testAm32LayoutTable() {
	section("AM32 layout: the editor's - and +, the footer, OK the wide button");
	EscTelemetry quiet;
	memset(&quiet, 0, sizeof(quiet));
	fakeSetTelemetry(&quiet);
	am32NoCard();
	enterAm32(6);
	checkTrue("the list is open", s_state == S_LIST);
	const int vp = uiRowOf("POLES");
	uiRowY("POLES"); frames(1);
	tapId(UI_ROW(vp, Row));
	UiRect m = ctlRect(UI_FIX(Am32Ctl::EditMinus)), p = ctlRect(UI_FIX(Am32Ctl::EditPlus));
	checkTrue("- at x 6, 44 x 44 px, y 218", m.x == 6 && m.y == 218 && m.w == 44 && m.h == 44);
	checkTrue("+ ending at x 234, 44 x 44 px", p.x == 190 && p.y == 218 && p.w == 44 && p.h == 44);
	checkTrue("the hint centred in the 140 px between them",
	          fakeTextIs(50 + (140 - gfxTextW("SWIPE = COARSE", 1)) / 2, EDIT_Y + 33, "SWIPE = COARSE",
	                     C_GRID, C_BG, 1));
	UiRect w = ctlRect(UI_FIX(Am32Ctl::Write)), r = ctlRect(UI_FIX(Am32Ctl::Revert)),
	       h = ctlRect(UI_FIX(Am32Ctl::Hex));
	checkTrue("the footer 6/110, 122/52, 180/54 at y 268, 46 px",
	          w.x == 6 && w.w == 110 && r.x == 122 && r.w == 52 && h.x == 180 && h.w == 54 &&
	          w.y == 268 && r.y == 268 && h.y == 268 && w.h == 46 && r.h == 46 && h.h == 46);
	checkControlTable("AM32 list, the layout table");

	tapId(UI_FIX(Am32Ctl::EditMinus));
	uint8_t want = uiByte(0x1B);
	int wx, wy;
	ctlCentre(UI_FIX(Am32Ctl::Write), &wx, &wy);
	pressId(UI_FIX(Am32Ctl::Write));
	for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
	const int okX = 6 + (228 - gfxTextW("OK", 2)) / 2, okY = 268 + (46 - 14) / 2;
	checkTrue("the hold's finger still down on OK: OK drawn at rest, not pressed",
	          s_state == S_WRITING && s_verified && fakeTextIs(okX, okY, "OK", C_TEXT, C_PANEL, 2) &&
	          !fakeTextIs(okX + 1, okY + 1, "OK", C_TEXT, C_PANEL, 2));
	fakeRelease(); frames(3);
	checkTrue("written and verified", fakeEscByte(0x1B) == want && s_verified);
	UiRect ok = ctlRect(UI_FIX(Am32Ctl::Ok));
	checkTrue("OK: the wide button, 6/228 at y 268, 46 px",
	          ok.x == 6 && ok.y == 268 && ok.w == 228 && ok.h == 46);
	checkTrue("over HOLD TO WRITE: the hold's lift lands on OK", wx >= ok.x && wx < ok.x + ok.w &&
	          wy >= ok.y && wy < ok.y + ok.h);
	checkTrue("that lift leaves the result up", s_state == S_WRITING && ctlEnabled(UI_FIX(Am32Ctl::Ok)));
	checkTrue("OK at scale 2, centred, as KST's and Hitec's",
	          fakeTextIs(6 + (228 - gfxTextW("OK", 2)) / 2, 268 + (46 - 14) / 2, "OK", C_TEXT, C_PANEL, 2));
	checkTrue("the power-cycle line above it", fakeTextCentredIs(170, AM32_UI_POWER_CYCLE, C_TEXT, C_BG, 1));
	checkControlTable("AM32 result, the layout table");
	pressId(UI_FIX(Am32Ctl::Ok));
	checkTrue("a fresh press on OK draws it pressed, 1 px in",
	          fakeTextIs(okX + 1, okY + 1, "OK", C_TEXT, C_PANEL, 2));
	fakeRelease(); frames(1);
	checkTrue("a tap on OK: the list", s_state == S_LIST);

	// Put the fake ESC back as it was.
	tapId(UI_FIX(Am32Ctl::EditPlus));
	pressId(UI_FIX(Am32Ctl::Write));
	for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
	fakeRelease(); frames(3);
	tapId(UI_FIX(Am32Ctl::Ok));
	checkTrue("the ESC as it was", s_state == S_LIST && !anyDirty());
	leaveAm32ToTester();
}

/**
 * @brief AM32 CONFIG says what a value does and what to do next: the strip
 *        notes, the next step under a write that is not verified, the
 *        connect page's lines, and MENU's cue after a session that reached
 *        the bootloader.
 */
/**
 * @brief AM32 CONFIG's claim names SIG by role and number, and a refusal for
 *        another holder, which MENU never lets through, says to wait.
 */
static void testAm32ClaimWords() {
	section("AM32 CONFIG: RELEASING SIG GP0, and WAIT: DSHOT RESTARTING");
	fakeFlashClear();
	settingsLoad();
	uiInit();
	frames(2);
	checkTrue("another claim holds the pins",
	          pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	claimPins();
	checkTrue("refused: WAIT: DSHOT RESTARTING",
	          !s_claimed && strcmp(s_status, UI_WAIT_RESTART_TEXT) == 0);
	pinClaimEnd(PinUser::Servo);
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) frames(1);
	claimPins();
	char want[32];
	snprintf(want, sizeof(want), "RELEASING SIG GP%u", (unsigned)settings()->dshotPin);
	checkTrue("taken: RELEASING SIG GP0", s_claimed && strcmp(s_status, want) == 0);
	cardCacheEnd();
	pinClaimEnd(PinUser::Am32);
	s_claimed = false;
	for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) frames(1);
	checkTrue("and given back", pinClaimState() == ClaimState::Free);
}

static void testAm32Guidance() {
	// No eRPM for the rest of this test unless it stamps one.
	EscTelemetry quiet;
	memset(&quiet, 0, sizeof(quiet));
	fakeSetTelemetry(&quiet);
	// Forget the earlier sessions' handshake: MENU's cue starts clear.
	s_blReached = false;

	section("AM32 connect: a reply that is not a bootloader's");
	{
		static const uint8_t junk[3] = { 0x55, 0xAA, 0x01 };
		fakeAm32Garble(junk, 3);
		enterAm32(4);
		checkTrue("still connecting", s_state == S_CONNECT);
		checkTrue("the bytes once in the page, y 176",
		          fakeTextCentredIs(176, s_status, C_AMBER, C_BG, 1));
		checkTrue("REPLY NOT UNDERSTOOD:, y 196",
		          fakeTextCentredIs(196, "REPLY NOT UNDERSTOOD:", C_AMBER, C_BG, 1));
		checkTrue("then " AM32_UI_NOT_AM32 ", y 210",
		          fakeTextCentredIs(210, AM32_UI_NOT_AM32, C_AMBER, C_BG, 1));
		int gx0 = 0, gx1 = 0;
		checkTrue("and nothing at y 224", !fakeInkExtent(224, 8, C_BG, &gx0, &gx1) ||
		          (gx0 >= GFX_W / 2 - 18 && gx1 <= GFX_W / 2 + 18));
		checkTrue("no handshake: no cue for MENU", !s_blReached);
		fakeDumpFrame("shot_am32_connect_garbled.ppm");
		fakeAm32Garble(nullptr, 0);
		leaveAm32ToTester();
	}

	section("AM32 connect: silence shows the page alone");
	{
		fakeAm32Silent(true);
		enterAm32(4);
		checkTrue("still connecting", s_state == S_CONNECT);
		checkTrue("POWER-CYCLE THE ESC, y 84, amber: the current step",
		          fakeTextCentredIs(84, "POWER-CYCLE THE ESC", C_AMBER, C_BG, 2));
		int x0 = 0, x1 = 0;
		checkTrue("no note at y 196 to y 231", !fakeInkExtent(196, 36, C_BG, &x0, &x1) ||
		          (x0 >= GFX_W / 2 - 42 && x1 <= GFX_W / 2 + 42));
		fakeDumpFrame("shot_am32_connect.ppm");
		fakeAm32Silent(false);
		leaveAm32ToTester();
	}

	section("AM32 connect: settings that are not AM32's, and nothing written");
	{
		uint8_t esc[AM32_SETTINGS_SIZE];
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) esc[i] = fakeEscByte(i);
		fakeAm32Blank(true);
		enterAm32(6);
		checkTrue("the connect page stays", s_state == S_CONNECT &&
		          strcmp(s_status, "NO VALID SETTINGS") == 0);
		checkTrue(AM32_UI_NO_SETTINGS_1 ", y 196",
		          fakeTextCentredIs(196, AM32_UI_NO_SETTINGS_1, C_AMBER, C_BG, 1));
		checkTrue(AM32_UI_NO_SETTINGS_2 ", y 210",
		          fakeTextCentredIs(210, AM32_UI_NO_SETTINGS_2, C_AMBER, C_BG, 1));
		checkTrue("and no WRITE to hold", !ctlPresent(UI_FIX(Am32Ctl::Write)));
		bool same = true;
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) same = same && esc[i] == fakeEscByte(i);
		checkTrue("the ESC's settings are untouched", same);
		checkTrue("the handshake reached the bootloader: MENU's cue stands", s_blReached);
		fakeAm32Blank(false);
		frames(2);
		checkTrue("a good read opens the list", s_state == S_LIST);
		tapId(UI_FIX(UI_BACK));
		frames(2);
		checkTrue("MENU: " AM32_UI_RUN_CUE " in amber under the caption", menuRunCueShown());
		char cap[40];
		snprintf(cap, sizeof(cap), "SIG GP%u  AUX GP%u  DSHOT%u", (unsigned)settings()->dshotPin,
		         (unsigned)settings()->kissPin, (unsigned)settings()->dshotKbaud);
		checkTrue("the caption stays on the first line",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, cap, C_DIM, C_BG, 1));
		int x0 = 0, x1 = 0;
		checkTrue("the cue ends left of the NO ESC chip, x 185",
		          fakeInkExtent(UI_STRIP_LINE2_Y + 4, 3, C_BG, &x0, &x1) && x1 < 185);
		fakeDumpFrame("shot_menu_run_cue.ppm");
		// An eRPM frame older than the handshake, from before the power cycle
		// the handshake needed, leaves it.
		EscTelemetry old;
		memset(&old, 0, sizeof(old));
		old.lastRpmMs = s_blReachedMs - ESC_LINK_STALE_MS;
		fakeSetTelemetry(&old);
		frames(2);
		checkTrue("an older eRPM frame: the cue stays", menuRunCueShown());
		// One after it is the firmware running: the cue goes, also when that
		// frame is already stale and the chip stays NO ESC.
		fakeAdvance(1000);
		EscTelemetry after;
		memset(&after, 0, sizeof(after));
		after.lastRpmMs = s_blReachedMs + 1;
		fakeSetTelemetry(&after);
		frames(2);
		checkTrue("a stale eRPM frame after the handshake: the cue goes, NO ESC stays",
		          !menuRunCueShown() && !s_blReached);
		// Armed again, as by a later handshake: that frame is older now.
		s_blReached = true;
		s_blReachedMs = millis();
		frames(2);
		checkTrue("a later handshake: the cue again", menuRunCueShown());
		feedLiveTelemetry();
		frames(2);
		checkTrue("an eRPM frame after the handshake: the cue goes",
		          !menuRunCueShown() && !s_blReached);
		fakeSetTelemetry(&quiet);
		frames(2);
		checkTrue("and stays gone when eRPM stops again", !menuRunCueShown());
		tapId(UI_FIX(UI_BACK));
		checkTrue("back on the tester", uiCurrentScreen() == UiScreenId::Tester);
	}

	section("AM32 list on firmware 2.22, layout 4: the values only it accepts");
	{
		uint8_t keep[AM32_SETTINGS_SIZE];
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) keep[i] = fakeEscByte(i);
		fakeEscSet(AM32_OFF_LAYOUT_REVISION, 4);
		fakeEscSet(AM32_OFF_MAIN_REVISION, 2);
		fakeEscSet(AM32_OFF_SUB_REVISION, 22);
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
		fakeEscSet(0x2C, 99);                       // 198 A
		fakeEscSet(0x08, 20);                       // 10.0 V
		enterAm32(6);
		checkTrue("the list is open", s_state == S_LIST);
		char v[24];
		// PWM FREQ reaches 144 kHz.
		const int vp = uiRowOf("PWM FREQ");
		uiRowY("PWM FREQ"); frames(1);
		tapId(UI_ROW(vp, Row));
		for (int i = 0; i < 4; i++) {
			int y = uiRowY("PWM FREQ"); frames(1);
			const UiRect lv = listView();
			swipe(lv.x + 30, lv.x + 220, y, 10);
		}
		checkInt("2.22: PWM FREQ reaches 144 kHz", uiByte(0x18), 144);
		// CURRENT LIMIT: 198 A, + to 200 A, an active limit: no note.
		const int vc = uiRowOf("CURRENT LIMIT");
		uiRowY("CURRENT LIMIT"); frames(1);
		tapId(UI_ROW(vc, Row));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		am32FormatValue(&AM32_FIELDS[s_selected], s_eeprom, v, sizeof(v));
		checkTrue("2.22: CURRENT LIMIT 100 reads 200A", uiByte(0x2C) == 100 && strcmp(v, "200A") == 0);
		frames(1);
		checkTrue("  an active limit: no protection note",
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));
		checkTrue("  the editor reads 200A, changed: cyan", fakeTextCentredIs(EDIT_Y + 14, "200A", C_CYAN, C_BG, 2));
		// LOW VOLT CUTOFF: + from CELL is ABSOLUTE, and ABS VOLT CUT reads volts.
		const int vl = uiRowOf("LOW VOLT CUTOFF");
		uiRowY("LOW VOLT CUTOFF"); frames(1);
		tapId(UI_ROW(vl, Row));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("2.22: + from CELL is ABSOLUTE",
		          uiByte(AM32_OFF_LOW_VOLT_CUTOFF) == 2 &&
		          fakeTextCentredIs(EDIT_Y + 14, "ABSOLUTE", C_CYAN, C_BG, 2));
		checkTrue("  an active cutoff: no protection note",
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));
		const int va = uiRowOf("ABS VOLT CUT");
		checkTrue("layout 4 shows ABS VOLT CUT", va >= 0);
		uiRowY("ABS VOLT CUT"); frames(1);
		tapId(UI_ROW(va, Row));
		frames(1);
		checkTrue("2.22: ABS VOLT CUT reads 10.0V, as read: the text colour",
		          fakeTextCentredIs(EDIT_Y + 14, "10.0V", C_TEXT, C_BG, 2));
		fakeDumpFrame("shot_am32_fw222.ppm");
		tapId(UI_FIX(Am32Ctl::Revert));
		leaveAm32ToTester();
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, keep[i]);
	}

	section("AM32 list on firmware 1.35, 1.80, 1.94: the bytes each reads");
	{
		uint8_t keep[2] = { fakeEscByte(AM32_OFF_MAIN_REVISION), fakeEscByte(AM32_OFF_SUB_REVISION) };
		fakeEscSet(AM32_OFF_MAIN_REVISION, 1);
		fakeEscSet(AM32_OFF_SUB_REVISION, 35);
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
		enterAm32(6);
		checkTrue("the note on the strip",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_OLDFW_1, UI_COL_WARN, C_BG, 1) &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_OLDFW_2, UI_COL_WARN, C_BG, 1));
		checkTrue("TEMP LIMIT, CURRENT LIMIT, PROTOCOL, AUTO ADVANCE not listed",
		          uiRowOf("TEMP LIMIT") < 0 && uiRowOf("CURRENT LIMIT") < 0 &&
		          uiRowOf("PROTOCOL") < 0 && uiRowOf("AUTO ADVANCE") < 0 &&
		          uiRowOf("RUNNING LEVEL") < 0 && uiRowOf("SINE POWER") < 0);
		checkTrue("CELL CUTOFF, at 0x25, is", uiRowOf("CELL CUTOFF") >= 0);
		fakeDumpFrame("shot_am32_oldfw.ppm");
		leaveAm32ToTester();
		// 1.80 reads 0x2A to 0x2D, not PROTOCOL at 0x2E: a stored SERVO there
		// is no note, and the old-firmware note is gone.
		uint8_t input = fakeEscByte(AM32_OFF_INPUT_TYPE);
		fakeEscSet(AM32_OFF_SUB_REVISION, 80);
		fakeEscSet(AM32_OFF_INPUT_TYPE, AM32_INPUT_SERVO);
		enterAm32(6);
		checkTrue("1.80: TEMP LIMIT listed, PROTOCOL not",
		          uiRowOf("TEMP LIMIT") >= 0 && uiRowOf("PROTOCOL") < 0);
		checkTrue("1.80: no PROTOCOL note for a byte it does not read, no old-firmware note",
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROTO_1, UI_COL_WARN, C_BG, 1) &&
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_OLDFW_1, UI_COL_WARN, C_BG, 1));
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_SUB_REVISION, 94);
		enterAm32(6);
		checkTrue("1.94: PROTOCOL listed, and SERVO's note",
		          uiRowOf("PROTOCOL") >= 0 &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROTO_1, UI_COL_WARN, C_BG, 1));
		leaveAm32ToTester();
		fakeEscSet(AM32_OFF_INPUT_TYPE, input);
		fakeEscSet(AM32_OFF_MAIN_REVISION, keep[0]);
		fakeEscSet(AM32_OFF_SUB_REVISION, keep[1]);
	}

	section("AM32 list: the strip's notes");
	{
		// The Air preset as the configurator ships it: 3D MODE on.
		fakeEscSet(AM32_OFF_BI_DIRECTION, 1);
		fakeEscSet(0x1B, 14);   // the preset's poles; the writes above moved them
		enterAm32(6);
		checkTrue("the list is open", s_state == S_LIST);
		checkTrue("3D MODE on: two amber lines on the strip",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_3D_1, UI_COL_WARN, C_BG, 1) &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_3D_2, UI_COL_WARN, C_BG, 1));
		checkTrue("36 and 34 characters inside the panel",
		          UI_MARGIN + gfxTextW(AM32_UI_3D_1, 1) <= GFX_W - UI_MARGIN &&
		          UI_MARGIN + gfxTextW(AM32_UI_3D_2, 1) <= GFX_W - UI_MARGIN);
		fakeDumpFrame("shot_am32_3d.ppm");
		// Edited off: the device caption comes back.
		const int v3d = uiRowOf("3D MODE");
		uiRowY("3D MODE"); frames(1);
		tapId(UI_ROW(v3d, Row));
		tapId(UI_FIX(Am32Ctl::EditMinus));
		checkInt("3D MODE edited off", uiByte(AM32_OFF_BI_DIRECTION), 0);
		frames(1);
		char name[AM32_NAME_LEN + 1], cap[48];
		am32DeviceName(s_eeprom, name);
		snprintf(cap, sizeof(cap), "%s  %s  FW %u.%u", name, am32BlEscTypeName(),
		         s_eeprom[AM32_OFF_MAIN_REVISION], s_eeprom[AM32_OFF_SUB_REVISION]);
		checkTrue("edited off: the device caption is back",
		          fakeTextIs(UI_MARGIN, UI_STRIP_Y + (UI_STRIP_H - 7) / 2, cap, C_DIM, C_BG, 1));
		int sx0 = 0, sx1 = 0;
		checkTrue("and the note's second line is gone: y 51 to 61 blank",
		          !fakeInkExtent(51, 11, C_BG, &sx0, &sx1));
		// Edited on again: the note returns.
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("edited on: the note returns",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_3D_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditMinus));

		// TEMP LIMIT at the top of its range reads OFF, list and editor alike.
		const int vt = uiRowOf("TEMP LIMIT");
		uiRowY("TEMP LIMIT"); frames(1);
		tapId(UI_ROW(vt, Row));
		for (int i = 0; i < 50; i++) tapId(UI_FIX(Am32Ctl::EditPlus));
		checkInt("TEMP LIMIT at 141", uiByte(0x2B), 141);
		frames(1);
		const UiRect tr = ctlRect(UI_ROW(vt, Row));
		checkTrue("the row reads OFF",
		          fakeTextIs(GFX_W - 8 - gfxTextW("OFF", 2), tr.y + 13, "OFF", C_CYAN, C_PANEL, 2));
		checkTrue("the editor reads OFF", fakeTextCentredIs(EDIT_Y + 14, "OFF", C_CYAN, C_BG, 2));
		checkTrue("selected and off: the protection note",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1) &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_PROT_2, UI_COL_WARN, C_BG, 1));
		fakeDumpFrame("shot_am32_protection_off.ppm");
		tapId(UI_FIX(Am32Ctl::EditMinus));
		frames(1);
		checkTrue("140 C: no note", !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));

		// CURRENT LIMIT: the preset's 40 A stepped down to 0 reads OFF.
		const int vc = uiRowOf("CURRENT LIMIT");
		uiRowY("CURRENT LIMIT"); frames(1);
		tapId(UI_ROW(vc, Row));
		for (int i = 0; i < 25; i++) tapId(UI_FIX(Am32Ctl::EditMinus));
		checkInt("CURRENT LIMIT at 0", uiByte(0x2C), 0);
		frames(1);
		checkTrue("0: the protection note",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("2 A: no note", !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));

		// LOW VOLT CUTOFF: on in the preset; off is the note.
		const int vl = uiRowOf("LOW VOLT CUTOFF");
		uiRowY("LOW VOLT CUTOFF"); frames(1);
		tapId(UI_ROW(vl, Row));
		frames(1);
		checkTrue("cutoff on: no note", !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditMinus));
		frames(1);
		checkTrue("cutoff off: the protection note",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));

		// PROTOCOL: SERVO is no DShot; the note stands whatever is selected.
		const int vp = uiRowOf("PROTOCOL");
		uiRowY("PROTOCOL"); frames(1);
		tapId(UI_ROW(vp, Row));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("DSHOT: no note", uiByte(AM32_OFF_INPUT_TYPE) == AM32_INPUT_DSHOT &&
		          !fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROTO_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("SERVO: the DShot note",
		          uiByte(AM32_OFF_INPUT_TYPE) == AM32_INPUT_SERVO &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROTO_1, UI_COL_WARN, C_BG, 1) &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, AM32_UI_PROTO_2, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("SERIAL: the note too, AM32 drops its throttle there",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROTO_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		frames(1);
		checkTrue("EDT ARM: the note too, a zero frame disarms it",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROTO_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::EditMinus));
		tapId(UI_FIX(Am32Ctl::EditMinus));
		// The selected row's note comes first: LOW VOLT CUTOFF, still off.
		uiRowY("LOW VOLT CUTOFF"); frames(1);
		tapId(UI_ROW(vl, Row));
		frames(1);
		checkTrue("the selected row's note before PROTOCOL's",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::Revert));
		frames(1);
		checkTrue("REVERT: the preset again, 3D MODE's note",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_3D_1, UI_COL_WARN, C_BG, 1));
	}

	section("AM32 list: LOW VOLT CUTOFF's three values, written back as read");
	{
		// Every value name fits the editor bar between - and +.
		int room = BTN_PLUS_X - (BTN_MINUS_X + EDIT_BTN_W);
		bool fits = true;
		for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
			const Am32Field *f = &AM32_FIELDS[i];
			for (uint8_t k = 0; f->names && k < f->nameCount; k++)
				if (gfxTextW(f->names[k], 2) > room) fits = false;
		}
		checkTrue("every value name fits the editor's 140 px at scale 2", fits && room == 140);
		const uint8_t fw[2] = { fakeEscByte(AM32_OFF_MAIN_REVISION), fakeEscByte(AM32_OFF_SUB_REVISION) };
		int wx, wy;
		for (int old = 0; old < 2; old++) {
			// ABSOLUTE, stored by another tool, on 2.22; then the same byte
			// on 1.35, where AM32 cuts off on 1 alone.
			fakeEscSet(AM32_OFF_MAIN_REVISION, old ? 1 : 2);
			fakeEscSet(AM32_OFF_SUB_REVISION, old ? 35 : 22);
			fakeEscSet(AM32_OFF_LOW_VOLT_CUTOFF, 2);
			leaveAm32ToTester();
			enterAm32(6);
			const int vl = uiRowOf("LOW VOLT CUTOFF");
			uiRowY("LOW VOLT CUTOFF"); frames(1);
			tapId(UI_ROW(vl, Row));
			frames(1);
			const char *want = old ? "? (2)" : "ABSOLUTE";
			checkTrue(old ? "1.35: the editor reads ? (2)" : "2.22: the editor reads ABSOLUTE",
			          fakeTextCentredIs(EDIT_Y + 14, want, C_TEXT, C_PANEL, 2) ||
			          fakeTextCentredIs(EDIT_Y + 14, want, C_TEXT, C_BG, 2));
			checkTrue(old ? "1.35: AM32 does not cut off on 2: the protection note"
			              : "2.22: no protection note",
			          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, AM32_UI_PROT_1, UI_COL_WARN, C_BG, 1) == (old == 1));
			// Another field edited and written: the cutoff byte goes back as read.
			const int vPoles = uiRowOf("POLES");
			uiRowY("POLES"); frames(1);
			tapId(UI_ROW(vPoles, Row));
			tapId(UI_FIX(Am32Ctl::EditMinus));
			uint8_t poles = uiByte(0x1B);
			ctlCentre(UI_FIX(Am32Ctl::Write), &wx, &wy);
			pressId(UI_FIX(Am32Ctl::Write));
			for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
			fakeRelease(); frames(3);
			checkTrue("  written and verified, LOW VOLT CUTOFF still 2",
			          s_verified && fakeEscByte(0x1B) == poles && fakeEscByte(AM32_OFF_LOW_VOLT_CUTOFF) == 2);
			tapId(UI_FIX(Am32Ctl::Ok));
			// Put POLES back.
			tapId(UI_FIX(Am32Ctl::EditPlus));
			pressId(UI_FIX(Am32Ctl::Write));
			for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
			fakeRelease(); frames(3);
			tapId(UI_FIX(Am32Ctl::Ok));
		}
		// A press on 2 under 1.35 snaps to CELL: the pilot's own press.
		uiRowY("LOW VOLT CUTOFF"); frames(1);
		tapId(UI_ROW(uiRowOf("LOW VOLT CUTOFF"), Row));
		tapId(UI_FIX(Am32Ctl::EditMinus));
		checkInt("1.35: - on ? (2) is CELL, 1", uiByte(AM32_OFF_LOW_VOLT_CUTOFF), 1);
		tapId(UI_FIX(Am32Ctl::Revert));
		fakeEscSet(AM32_OFF_LOW_VOLT_CUTOFF, 1);
		fakeEscSet(AM32_OFF_MAIN_REVISION, fw[0]);
		fakeEscSet(AM32_OFF_SUB_REVISION, fw[1]);
		leaveAm32ToTester();
		enterAm32(6);
	}

	section("AM32 write: a result that is not verified names the next step");
	{
		const int vPoles = uiRowOf("POLES");
		uiRowY("POLES"); frames(1);
		tapId(UI_ROW(vPoles, Row));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		uint8_t want = uiByte(0x1B);
		int wx, wy;
		ctlCentre(UI_FIX(Am32Ctl::Write), &wx, &wy);
		struct Case { const char *what; void (*arm)(); const char *prefix; };
		static const Case cases[] = {
			{ "WRITE FAILED", [] { fakeAm32FailWrites(1); }, "WRITE FAILED: " },
			{ "VERIFY READ",  [] { fakeAm32FailReads(3); },  "VERIFY READ: " },
			{ "DIFF",         [] { fakeAm32SkewWrites(1); }, "DIFF @1B: " },
		};
		for (const Case &c : cases) {
			c.arm();
			pressId(UI_FIX(Am32Ctl::Write));
			for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
			fakeRelease(); frames(3);
			checkTrue(c.what, strncmp(s_status, c.prefix, strlen(c.prefix)) == 0);
			checkTrue("  then " AM32_UI_RETRY " in amber, y 170",
			          fakeTextCentredIs(170, AM32_UI_RETRY, UI_COL_WARN, C_BG, 1));
			checkTrue("  and no power-cycle line",
			          !fakeTextCentredIs(170, AM32_UI_POWER_CYCLE, C_TEXT, C_BG, 1));
			int nx0 = 0, nx1 = 0;
			checkTrue("  the caption alone on the strip: y 51 to 61 blank",
			          !fakeInkExtent(51, 11, C_BG, &nx0, &nx1));
			if (c.prefix[0] == 'W') fakeDumpFrame("shot_am32_write_failed.ppm");
			tapId(UI_FIX(Am32Ctl::Ok));
			checkTrue("  OK: the list, the edit kept", s_state == S_LIST && uiByte(0x1B) == want &&
			          anyDirty() && ctlEnabled(UI_FIX(Am32Ctl::Write)));
		}
		// The hold again: the write runs and verifies. The frame the hold
		// fires on draws WRITING... alone, without the last result's line.
		pressId(UI_FIX(Am32Ctl::Write));
		int held = 0;
		while (s_state != S_WRITING && held++ < 60) { fakeHold(wx, wy); frames(1); }
		checkTrue("WRITING...: the last result's next step is gone",
		          s_state == S_WRITING && strcmp(s_status, "WRITING...") == 0 &&
		          !fakeTextCentredIs(170, AM32_UI_RETRY, UI_COL_WARN, C_BG, 1));
		for (int i = 0; i < 10; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("holding again writes and verifies", fakeEscByte(0x1B) == want && s_verified &&
		          fakeTextCentredIs(170, AM32_UI_POWER_CYCLE, C_TEXT, C_BG, 1));
		checkInt("29 and 34 characters: 173 and 203 px",
		         gfxTextW(AM32_UI_POWER_CYCLE, 1) * 1000 + gfxTextW(AM32_UI_RETRY, 1), 173203);
		tapId(UI_FIX(Am32Ctl::Ok));
		// Put the fake ESC back as it was.
		tapId(UI_FIX(Am32Ctl::EditMinus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		tapId(UI_FIX(Am32Ctl::Ok));
		leaveAm32ToTester();
	}

	section("AM32 write: arbitrary settings go back as read but the pressed field");
	{
		uint8_t keep[AM32_SETTINGS_SIZE];
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) keep[i] = fakeEscByte(i);
		static const uint8_t fws[][2] = {
			{0xFF, 0xFF}, {1, 35}, {2, 16}, {2, 18}, {2, 19}, {2, 20}, {2, 21}, {2, 22}};
		bool same = true, verified = true, opened = true;
		uint32_t seed = 12345;
		for (int n = 0; n < 8; n++) {
			uint8_t block[AM32_SETTINGS_SIZE];
			for (int i = 0; i < AM32_SETTINGS_SIZE; i++) {
				seed = seed * 1103515245u + 12345u;
				block[i] = (uint8_t)(seed >> 16);
			}
			// The header the list needs; every other byte as it fell.
			block[AM32_OFF_BOOT_BYTE] = 1;
			block[AM32_OFF_LAYOUT_REVISION] = (uint8_t)(2 + n % 3);
			block[AM32_OFF_MAIN_REVISION] = fws[n][0];
			block[AM32_OFF_SUB_REVISION] = fws[n][1];
			block[0x1B] = 14;
			for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, block[i]);
			enterAm32(6);
			if (s_state != S_LIST) { opened = false; leaveAm32ToTester(); continue; }
			const int vPoles = uiRowOf("POLES");
			uiRowY("POLES"); frames(1);
			tapId(UI_ROW(vPoles, Row));
			tapId(UI_FIX(Am32Ctl::EditPlus));
			int wx, wy;
			ctlCentre(UI_FIX(Am32Ctl::Write), &wx, &wy);
			pressId(UI_FIX(Am32Ctl::Write));
			for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
			fakeRelease(); frames(3);
			if (!s_verified) verified = false;
			for (int i = 0; i < AM32_SETTINGS_SIZE; i++)
				if (i != 0x1B && fakeEscByte(i) != block[i]) same = false;
			if (fakeEscByte(0x1B) != 16) same = false;
			tapId(UI_FIX(Am32Ctl::Ok));
			leaveAm32ToTester();
		}
		checkTrue("eight blocks, layouts 2 to 4, eight firmware versions: each list opens", opened);
		checkTrue("  each write verifies", verified);
		checkTrue("  the ESC holds every byte as read, POLES as pressed", same);
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, keep[i]);
	}

	// From the review of this screen: every visible row, tapped and pressed,
	// changes its own byte and no other, and the list holds the rows the
	// firmware reads, from a table written by hand.
	section("AM32 list: each visible row edits its own byte, per firmware and layout");
	{
		uint8_t keep[AM32_SETTINGS_SIZE];
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) keep[i] = fakeEscByte(i);
		// Firmware bytes and the rows layout 2 shows: 31 with every byte read,
		// less 0x2A to 0x2F below 1.80, 0x2E and 0x2F below 1.94, 0x2F below
		// 2.16. Layouts 3 and 4 add the eight at 0x05 to 0x0C.
		struct Fw { uint8_t major, minor; int rows2; };
		static const Fw fws[] = {
			{0xFF, 0xFF, 25}, {0, 0, 25}, {1, 35, 25}, {1, 79, 25}, {1, 80, 29},
			{1, 93, 29}, {1, 94, 30}, {1, 99, 30}, {2, 0, 30}, {2, 15, 30},
			{2, 16, 31}, {2, 18, 31}, {2, 19, 31}, {2, 22, 31}};
		bool ok = true, countOk = true;
		for (int lay = 2; lay <= 4; lay++) for (const Fw &fw : fws) {
			for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, keep[i]);
			fakeEscSet(AM32_OFF_LAYOUT_REVISION, (uint8_t)lay);
			fakeEscSet(AM32_OFF_MAIN_REVISION, fw.major);
			fakeEscSet(AM32_OFF_SUB_REVISION, fw.minor);
			fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
			int ver = (fw.major >= 1 && fw.major <= 9 && fw.minor <= 99) ? fw.major * 100 + fw.minor : 0;
			// The offsets expected in list order, from the gates by hand.
			int exp[64], ne = 0;
			for (uint16_t k = 0; k < AM32_FIELD_COUNT; k++) {
				uint8_t off = AM32_FIELDS[k].offset;
				if (off >= 0x05 && off <= 0x0C && lay < 3) continue;
				if (off == 0x2F && ver < 216) continue;
				if (off == 0x2E && ver < 194) continue;
				if (off >= 0x2A && off <= 0x2D && ver < 180) continue;
				exp[ne++] = off;
			}
			int rows = fw.rows2 + (lay >= 3 ? 8 : 0);
			if (ne != rows) countOk = false;
			enterAm32(6);
			if (s_state != S_LIST) { ok = false; leaveAm32ToTester(); continue; }
			if (uiVisibleCount() != rows) {
				countOk = false;
				printf("  layout %d, firmware %u.%u: %d rows, want %d\n", lay, fw.major, fw.minor,
				       uiVisibleCount(), rows);
			}
			for (int v = 0; v < uiVisibleCount() && v < ne; v++) {
				uiListScrollTo(&s_list, v * ROW_PITCH); frames(1);
				tapId(UI_ROW(v, Row));
				uint8_t before[AM32_SETTINGS_SIZE];
				memcpy(before, s_eeprom, sizeof(before));
				tapId(UI_FIX(Am32Ctl::EditPlus));
				int changed = -1, nch = 0;
				for (int k = 0; k < AM32_SETTINGS_SIZE; k++) if (s_eeprom[k] != before[k]) { changed = k; nch++; }
				if (nch == 0) {
					tapId(UI_FIX(Am32Ctl::EditMinus));
					for (int k = 0; k < AM32_SETTINGS_SIZE; k++) if (s_eeprom[k] != before[k]) { changed = k; nch++; }
				}
				if (nch != 1 || changed != exp[v]) {
					ok = false;
					printf("  layout %d, firmware %u.%u, row %d: byte %d changed (%d bytes), want 0x%02X\n",
					       lay, fw.major, fw.minor, v, changed, nch, exp[v]);
				}
			}
			tapId(UI_FIX(Am32Ctl::Revert));
			leaveAm32ToTester();
		}
		checkTrue("layouts 2 to 4, 14 firmware versions: the rows the table expects", countOk);
		checkTrue("  each row edits its own byte and no other", ok);
		for (int i = 0; i < AM32_SETTINGS_SIZE; i++) fakeEscSet(i, keep[i]);
	}
	feedLiveTelemetry();
	frames(2);
}

void runUiTests() {
	// Off zero before anything is stamped. Virtual time starts at 0, and 0 is
	// reserved for "this frame type has never arrived" -- so telemetry stamped
	// at t=0 is correctly treated as absent, and every screenshot below would
	// render an ESC that had said nothing.
	fakeAdvance(1000);

	feedLiveTelemetry();

	gfxInit();
	// Render the real splash, so the documentation screenshot cannot drift
	// away from what the board shows.
	uiDrawSplash();
	fakeDumpFrame("shot_splash.ppm");

	uiInit();
	frames(4);
	checkTrue("the tester is open after uiInit()", uiCurrentScreen() == UiScreenId::Tester);
	checkControlTable("tester, disarmed");
	fakeDumpFrame("shot_tester_disarmed.ppm");

	// The throttle track as drawn, and a y on its centre line.
	const UiRect thr = ctlRect(UI_FIX(TesterCtl::Throttle));
	const int ty = thr.y + thr.h / 2;

	section("Tester: the readouts and the hint take the neutral colours");
	{
		const int hx = 226 - gfxTextW("SWIPE = THROTTLE", 1);
		checkTrue("disarmed: SWIPE = THROTTLE grey",
		          fakeTextIs(hx, 114, "SWIPE = THROTTLE", C_GRID, C_BG, 1));
		checkTrue("MAX 20% in the text colour at x 88, y 236",
		          fakeTextIs(88, 236, "MAX 20%", C_TEXT, C_BG, 1));
		holdToArm();
		checkTrue("armed: SWIPE = THROTTLE in the text colour",
		          fakeArmed() && fakeTextIs(hx, 114, "SWIPE = THROTTLE", C_TEXT, C_BG, 1));
		checkTrue("armed: MAX 20% still the text colour",
		          fakeTextIs(88, 236, "MAX 20%", C_TEXT, C_BG, 1));
		tapId(UI_FIX(TesterCtl::Arm));                     // DISARM
		frames(2);
		checkTrue("disarmed again", !fakeArmed());
	}

	section("Throttle: spring mode is absolute");
	{
		holdToArm();
		checkTrue("armed only after a 1 s hold", fakeArmed());
		checkControlTable("tester, armed");

		// 70 % of the *usable* span, which is the track minus the margin at
		// each end that reads as a rail. Measuring from the drawn edge instead
		// would now be asking for 73 % and calling it 70. @see THR_EDGE_SAT
		int tx = thr.x + THR_EDGE_SAT_T + ((thr.w - 2 * THR_EDGE_SAT_T) * 70 / 100);
		fakePress(tx, ty); frames(1); fakeHold(tx, ty); frames(2);
		checkInt("drag to 70% of track", fakeThrottle(), 400 * 70 / 100, 6);
		feedLiveTelemetry(); frames(2);   // the arm hold outlasted the fixture
		fakeDumpFrame("shot_tester_armed.ppm");
		fakeRelease(); frames(2);
		checkInt("springs back to zero on release", fakeThrottle(), 0);
	}

	section("Throttle: LATCH mode is relative");
	{
		const int mid = thr.x + thr.w / 2;
		tapId(UI_FIX(TesterCtl::Latch));                       // LATCH on
		fakePress(mid, ty); frames(2);
		checkInt("touching the bar does not jump", fakeThrottle(), 0);
		for (int x = mid; x <= mid + 56; x += 8) { fakeHold(x, ty); frames(1); }
		checkInt("drag +56px raises by a quarter range", fakeThrottle(), 100, 6);
		fakeRelease(); frames(2);
		checkInt("latches after release", fakeThrottle(), 100, 6);

		// Overshoot a rail, then reverse: must respond at once, not unwind.
		swipe(thr.x + 192, thr.x, ty, -8);
		checkInt("overshoot clamps to zero", fakeThrottle(), 0);
		fakePress(thr.x, ty); frames(1);
		fakeHold(thr.x + 11, ty); frames(1);
		fakeHold(thr.x + 22, ty); frames(1);
		checkInt("reverse re-anchors, no windup", fakeThrottle(), 39, 6);
		fakeRelease(); frames(2);
		tapId(UI_FIX(TesterCtl::Latch));                       // LATCH off
		checkInt("turning LATCH off zeroes throttle", fakeThrottle(), 0);
	}

	section("Throttle pad: the number area is always relative");
	{
		// Points on the pad are measured up from its bottom edge.
		const UiRect pad = ctlRect(UI_FIX(TesterCtl::Pad));
		const int px = pad.x + pad.w / 2, pb = pad.y + pad.h;
		tapId(UI_FIX(TesterCtl::Latch));                       // LATCH on
		fakePress(px, pb - 60); frames(2); fakeRelease(); frames(2);
		checkInt("a tap on the pad does nothing", fakeThrottle(), 0);
		fakePress(px, pb - 60); frames(1);
		fakeHold(px, pb - 64); frames(1);
		fakeHold(px, pb - 63); frames(1);
		checkInt("sub-deadzone wiggle ignored", fakeThrottle(), 0);
		fakeRelease(); frames(2);
		// 106 px of travel less the 6 px deadzone, at 60 % over a 213 px pad.
		fakePress(px, pb - 40); frames(1);
		for (int y = pb - 40; y >= pb - 146; y -= 6) { fakeHold(px, y); frames(1); }
		checkInt("swipe up raises", fakeThrottle(), 400 * 100 / 213 * 60 / 100, 4);
		fakeRelease(); frames(2);
		tapId(UI_FIX(TesterCtl::Latch));                       // LATCH off
	}

	section("Navigation into AM32 config");
	{
		// The published list shows the common case: 3D MODE off, the ESC's
		// name on the strip. testAm32Guidance() turns it on for the note.
		fakeEscSet(AM32_OFF_BI_DIRECTION, 0);
		tapId(UI_FIX(TesterCtl::Menu));
		checkTrue("the tester's MENU opens MENU", uiCurrentScreen() == UiScreenId::Menu);
		checkTrue("opening MENU force-disarms", !fakeArmed());
		// One more frame before the dump. The tap that navigates is handled by
		// the *outgoing* screen's handler, so the incoming one is not painted
		// until the following tick -- and this screenshot, which docs/ publishes,
		// was of the tester screen for as long as it has existed.
		frames(2);
		checkControlTable("MENU");
		fakeDumpFrame("shot_menu.ppm");
		tapId(menuRow(&UI_AM32));
		checkTrue("AM32 opens from MENU", uiCurrentScreen() == UiScreenId::Am32);
		for (int i = 0; i < 8; i++) frames(1);
		checkTrue("settings read and decoded", uiVisibleCount() > 0);
		checkInt("poles decoded", uiByte(0x1B), 14);
		checkControlTable("AM32 list");
		fakeDumpFrame("shot_am32_list.ppm");
	}

	section("Editing: fine, coarse, and the write interlock");
	{
		// Swipes run along a row, from x offsets on the list's own width.
		const UiRect lv = listView();
		checkTrue("the list has a viewport", lv.w > 0 && lv.h > 0);
		const int vPwm = uiRowOf("PWM FREQ");
		int yPwm = uiRowY("PWM FREQ"); frames(1);
		tapId(UI_ROW(vPwm, Row));
		checkInt("row selects", uiByte(0x18), 24);
		checkControlTable("AM32 list, a field selected");

		{
			// As read and selected: the row's fill lights, the value stays in
			// the text colour, no bar (menu-b.md, decision 8).
			frames(1);
			const UiRect r = ctlRect(UI_ROW(vPwm, Row));
			const uint16_t *fb = gfxBuffer();
			checkTrue("selected, as read: 24KHZ in the text colour on the row's fill, no bar",
			          fakeTextIs(232 - gfxTextW("24KHZ", 2), r.y + 13, "24KHZ", C_TEXT, C_PANEL, 2) &&
			          fb[(r.y + 5) * GFX_W + 1] == C_PANEL);
			checkTrue("the editor bar's value in the text colour too",
			          fakeTextCentredIs(EDIT_Y + 14, "24KHZ", C_TEXT, C_BG, 2));
		}
		tapId(UI_FIX(Am32Ctl::EditPlus));
		checkInt("+ button is one fine step", uiByte(0x18), 25);
		{
			frames(1);
			const UiRect r = ctlRect(UI_ROW(vPwm, Row));
			const uint16_t *fb = gfxBuffer();
			checkTrue("changed: 25KHZ cyan, and a 3 px cyan bar at the row's left edge",
			          fakeTextIs(232 - gfxTextW("25KHZ", 2), r.y + 13, "25KHZ", C_CYAN, C_PANEL, 2) &&
			          fb[(r.y + 5) * GFX_W] == C_CYAN && fb[(r.y + 5) * GFX_W + 2] == C_CYAN &&
			          fb[(r.y + 5) * GFX_W + 3] == C_PANEL);
			checkTrue("the editor bar's value cyan",
			          fakeTextCentredIs(EDIT_Y + 14, "25KHZ", C_CYAN, C_BG, 2));
			const UiRect h = ctlRect(UI_FIX(Am32Ctl::Hex));
			checkTrue("HEX, a command button: its label in the text colour",
			          fakeTextIs(h.x + (h.w - gfxTextW("HEX", 1)) / 2, h.y + (h.h - 7) / 2,
			                     "HEX", C_TEXT, C_PANEL, 1));
			const UiRect rv = ctlRect(UI_FIX(Am32Ctl::Revert));
			checkTrue("REVERT's label amber",
			          fakeTextIs(rv.x + (rv.w - gfxTextW("REVERT", 1)) / 2, rv.y + (rv.h - 7) / 2,
			                     "REVERT", C_AMBER, C_PANEL, 1));
		}
		fakeDumpFrame("shot_am32_edit.ppm");
		tapId(UI_FIX(Am32Ctl::EditMinus));
		checkInt("- button steps back", uiByte(0x18), 24);

		// Pressed at x 30, locked at x 40 after 10 px, then 180 px: 11 coarse
		// steps of 13 kHz pass the rail, and the swipe stops on it: 48 kHz,
		// the top AM32 before 2.19 accepts, on this ESC's firmware 1.35.
		swipe(lv.x + 30, lv.x + 220, yPwm, 10);
		checkInt("swipe right is coarse, and stops at 48, firmware 1.35's top", uiByte(0x18), 48);
		swipe(lv.x + 220, lv.x + 30, yPwm, -10);
		checkInt("swipe left stops at 8", uiByte(0x18), 8);

		// Locked at x 47, then 98 px: 6 coarse steps of 2 x 2 poles from 14
		// would reach 38; the swipe stops at 36, the rail, on the field's step.
		int yPoles = uiRowY("POLES"); frames(1);
		swipe(lv.x + 40, lv.x + 150, yPoles, 7);
		checkInt("stepped fields stay on their step, at the rail", uiByte(0x1B), 36);

		// Axis lock: a vertical drag must scroll, never edit.
		int yKv = uiRowY("KV"); frames(1);
		uint8_t kvBefore = uiByte(0x1A);
		fakePress(lv.x + 120, yKv); frames(1);
		for (int y = yKv; y >= lv.y + 4; y -= 10) { fakeHold(lv.x + 120, y); frames(1); }
		fakeRelease(); frames(1);
		checkInt("vertical drag did not edit", uiByte(0x1A), kvBefore);
		checkTrue("vertical drag scrolled", uiScroll() > 0);

		// The editor bar must never fall through to the row behind it.
		int before = uiSelected();
		tapId(UI_FIX(Am32Ctl::EditPlus));
		checkInt("editor bar does not re-select a row", uiSelected(), before);
	}

	/*
	 * Reported as "scrolling first selects the item touched and scrolls then
	 * with delay". Both halves were real and they had different causes: the
	 * selection fired on touch-down, and the scroll needed 10 px of axis lock
	 * plus a whole 26 px row before anything moved. So this section asserts the
	 * order of events, not just the end state.
	 */
	section("AM32 list: scrolls first, selects only on a tap that stayed put");
	{
		uiListScrollTo(&s_list, 0);
		s_selected = -1;
		frames(2);
		// The viewport, as the rows report it. The gestures below are measured
		// from its top edge.
		const UiRect lv = listView();
		const int lx = lv.x + 60;

		// 1. A press on a row commits nothing.
		fakePress(lx, lv.y + 10); frames(1);
		checkInt("a press alone selects nothing", uiSelected(), -1);

		// 2. Dragging up scrolls, and still selects nothing.
		for (int y = lv.y + 10; y >= lv.y - 40; y -= 5) {
			fakeHold(lx, y); frames(1);
		}
		checkTrue("the drag scrolled", uiScroll() > 0);
		checkInt("a drag never selects", uiSelected(), -1);
		fakeRelease(); frames(1);
		checkInt("and releasing after a drag does not either", uiSelected(), -1);

		// 3. The list tracks the finger 1:1, including the pixels spent
		//    deciding this was a scroll rather than a tap.
		int mark = uiScroll();
		fakePress(lx, lv.y + 94); frames(1);
		fakeHold(lx, lv.y + 54); frames(1);
		checkInt("the list moves exactly as far as the finger", uiScroll(),
		         mark + 40);
		fakeRelease(); frames(2);

		// 4. Which means the offset is normally mid-row, so the top and bottom
		//    rows are partial -- the case gfxSetClip() exists for. Nothing the
		//    list draws may reach the header strip above it.
		checkTrue("the scroll offset is mid-row", uiScroll() % ROW_PITCH != 0);
		uint32_t above = fakeRegionHash(0, 0, GFX_W, lv.y);
		fakePress(lx, lv.y + 94); frames(1);
		fakeHold(lx, lv.y + 87); frames(1);          // 7 px, so still mid-row
		fakeRelease(); frames(2);
		checkTrue("a partial row paints nothing above the viewport",
		          fakeRegionHash(0, 0, GFX_W, lv.y) == above);
		fakeDumpFrame("shot_am32_scrolled.ppm");

		// 5. A tap that goes nowhere selects, and selects the row actually
		//    under the finger -- which with a mid-row offset is not the row a
		//    slot-index calculation would have named.
		int y = lv.y + 40;
		int want = (int)s_visible[(uiScroll() + (y - lv.y)) / ROW_PITCH];
		fakePress(lx, y); frames(2); fakeRelease(); frames(1);
		checkInt("a tap that stayed put selects the row under it",
		         uiSelected(), want);

		// 6. And a horizontal swipe still adjusts the row it started on, even
		//    though the press no longer selected it. Selection has to happen at
		//    the axis lock, before the value anchor is taken, or the first
		//    swipe writes the old selection's value into the new field.
		s_selected = -1;
		frames(2);
		// 80 px down, on a row's body: rows are 40 px at a 42 px pitch, and a
		// press on the 2 px gap under a row is no row's.
		int y2 = lv.y + 80;
		checkTrue("the swipe starts on a row's body",
		          (uiScroll() + (y2 - lv.y)) % ROW_PITCH < ROW_H);
		int want2 = (int)s_visible[(uiScroll() + (y2 - lv.y)) / ROW_PITCH];
		uint8_t raw2 = s_eeprom[AM32_FIELDS[want2].offset];
		fakePress(lv.x + 40, y2); frames(1);
		for (int x = lv.x + 40; x <= lv.x + 200; x += 10) { fakeHold(x, y2); frames(1); }
		checkInt("a coarse swipe selects the row it began on", uiSelected(),
		         want2);
		checkTrue("and it is that row's value that moved",
		          s_eeprom[AM32_FIELDS[want2].offset] != raw2);
		fakeRelease(); frames(1);
	}

	section("AM32 swipe: 16 px per coarse step, a factor per field");
	{
		// Every field, whatever the layout revision, from either rail: 11
		// coarse steps, 176 px, reach the other.
		bool reach = true, onStep = true, stops = true;
		for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
			const Am32Field *f = &AM32_FIELDS[i];
			uint8_t keep = s_eeprom[f->offset];
			int step = f->step ? f->step : 1;
			if ((f->rawMax - f->rawMin) % step) onStep = false;
			s_eeprom[f->offset] = am32FieldMin(f, s_eeprom);
			uiSwipeApply(SWIPE_SPAN_STEPS, coarseFactor(f), swipeStep, (void *)f);
			if (s_eeprom[f->offset] != am32FieldMax(f, s_eeprom)) reach = false;
			if (uiSwipeApply(1, coarseFactor(f), swipeStep, (void *)f) != 0) stops = false;
			uiSwipeApply(-SWIPE_SPAN_STEPS, coarseFactor(f), swipeStep, (void *)f);
			if (s_eeprom[f->offset] != am32FieldMin(f, s_eeprom)) reach = false;
			if (uiSwipeApply(-1, coarseFactor(f), swipeStep, (void *)f) != 0) stops = false;
			s_eeprom[f->offset] = keep;
		}
		checkTrue("every field: 11 coarse steps from a rail reach the other", reach);
		checkTrue("every range is whole steps, so each rail is on the step", onStep);
		checkTrue("at either rail, a coarse step outwards takes no step", stops);
		checkInt("11 coarse steps", SWIPE_SPAN_STEPS, 11);
		const Am32Field *kv = nullptr, *poles = nullptr, *pwm = nullptr, *adv = nullptr,
		                *dir = nullptr, *temp = nullptr;
		for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++) {
			const char *n = AM32_FIELDS[i].name;
			if (!strcmp(n, "KV")) kv = &AM32_FIELDS[i];
			if (!strcmp(n, "POLES")) poles = &AM32_FIELDS[i];
			if (!strcmp(n, "PWM FREQ")) pwm = &AM32_FIELDS[i];
			if (!strcmp(n, "ADVANCE")) adv = &AM32_FIELDS[i];
			if (!strcmp(n, "DIRECTION")) dir = &AM32_FIELDS[i];
			if (!strcmp(n, "TEMP LIMIT")) temp = &AM32_FIELDS[i];
		}
		checkInt("KV, 1 to 255: 24 per coarse step", coarseFactor(kv), 24);
		checkInt("POLES, 2 to 36 in 2s: 2 steps, 4 poles", coarseFactor(poles), 2);
		checkInt("PWM FREQ, 8 to 144 kHz: 13", coarseFactor(pwm), 13);
		checkInt("ADVANCE, 33 angles: 3", coarseFactor(adv), 3);
		checkInt("TEMP LIMIT, 70 to 141: 7", coarseFactor(temp), 7);
		checkInt("DIRECTION, a choice of two: 1", coarseFactor(dir), 1);
		const Am32Field *beep = nullptr;
		for (uint16_t i = 0; i < AM32_FIELD_COUNT; i++)
			if (!strcmp(AM32_FIELDS[i].name, "BEEP VOLUME")) beep = &AM32_FIELDS[i];
		checkInt("BEEP VOLUME, 0 to 11, a range of exactly 11 steps: 1", coarseFactor(beep), 1);
		// ADVANCE in the old encoding on firmware 2.19 or later: raw 2 is 15
		// degrees, step 16 of the modern 0.9375 degree steps. A coarse step is
		// three of - and +'s steps, which convert it, so the angle is kept:
		// step 19, raw 29.
		uint8_t advKeep = s_eeprom[adv->offset];
		uint8_t fwKeep[2] = { s_eeprom[AM32_OFF_MAIN_REVISION], s_eeprom[AM32_OFF_SUB_REVISION] };
		s_eeprom[AM32_OFF_MAIN_REVISION] = 2;
		s_eeprom[AM32_OFF_SUB_REVISION] = 22;
		s_eeprom[adv->offset] = 2;
		uiSwipeApply(1, coarseFactor(adv), swipeStep, (void *)adv);
		checkInt("2.22: ADVANCE raw 2, old encoding, +1 coarse: raw 29", s_eeprom[adv->offset], 29);
		// The ESC's own firmware, 2.16, reads the old encoding alone: a coarse
		// step from 15 degrees stops at 22.5, raw 3.
		s_eeprom[AM32_OFF_MAIN_REVISION] = fwKeep[0];
		s_eeprom[AM32_OFF_SUB_REVISION] = fwKeep[1];
		s_eeprom[adv->offset] = 2;
		uiSwipeApply(1, coarseFactor(adv), swipeStep, (void *)adv);
		checkInt("2.16: ADVANCE raw 2, +1 coarse: raw 3, 22.5 degrees", s_eeprom[adv->offset], 3);
		s_eeprom[adv->offset] = advKeep;

		// Through the screen, on KV from 100. The x values are along the row.
		const UiRect lv = listView();
		const int sx = lv.x;
		int yKv = uiRowY("KV"); frames(1);
		s_eeprom[0x1A] = 100;
		fakePress(sx + 40, yKv); frames(1);
		fakeHold(sx + 47, yKv); frames(1);              // the lock: the anchor
		fakeHold(sx + 62, yKv); frames(1);
		checkInt("15 px past the lock: unchanged", uiByte(0x1A), 100);
		fakeHold(sx + 63, yKv); frames(1);
		checkInt("16 px: one coarse step, +24", uiByte(0x1A), 124);
		fakeHold(sx + 100, yKv); frames(1);
		checkInt("37 px more: two steps, +48", uiByte(0x1A), 172);
		fakeHold(sx + 84, yKv); frames(1);
		checkInt("11 px back: none, the anchor moved by whole steps", uiByte(0x1A), 172);
		fakeHold(sx + 239, yKv); frames(1);
		checkInt("9 steps more: stops at the rail, 255", uiByte(0x1A), 255);
		fakeHold(sx + 224, yKv); frames(1);
		checkInt("15 px back: still 255", uiByte(0x1A), 255);
		fakeHold(sx + 223, yKv); frames(1);
		checkInt("16 px back: off the rail by one coarse step", uiByte(0x1A), 231);
		fakeRelease(); frames(1);
		s_eeprom[0x1A] = s_original[0x1A];
		frames(1);
	}

	section("AM32 list on ui_list: extents, and the 40 px press rule at the edges");
	{
		// The viewport, as the rows report it at 0 px: y 62 to 215, 154 px.
		// The drags are measured from its top edge.
		uiListScrollTo(&s_list, 0);
		s_selected = -1;
		s_redraw = true;
		frames(1);
		const UiRect lv = listView();
		checkTrue("the viewport is y 62 to 215", lv.y == 62 && lv.y + lv.h - 1 == 215);
		const int lx = lv.x + 60;

		// Scroll extent: the content is a pitch per row, 42 px, the 2 px gap
		// under the last row included, so the scroll stops at the content less
		// the 154 px viewport, and the last row's rule is 2 px above the
		// viewport's last pixel row.
		for (int i = 0; i < 12; i++) {
			fakePress(lx, lv.y + 148); frames(1);
			for (int y = lv.y + 148; y >= lv.y + 4; y -= 30) { fakeHold(lx, y); frames(1); }
			fakeRelease(); frames(1);
		}
		checkInt("dragged up: the scroll stops at the content less 154 px", uiScroll(),
		         uiVisibleCount() * ROW_PITCH - LIST_H);
		const int rule = LIST_Y1 - (ROW_PITCH - ROW_H);
		checkTrue("the last row's rule on y 213, then the 2 px gap to y 215",
		          rule == 213 &&
		          gfxBuffer()[rule * GFX_W + 230] == C_PANEL &&
		          gfxBuffer()[(rule - 1) * GFX_W + 230] == C_BG &&
		          gfxBuffer()[(rule + 1) * GFX_W + 230] == C_BG &&
		          gfxBuffer()[LIST_Y1 * GFX_W + 230] == C_BG);
		for (int i = 0; i < 12; i++) {
			fakePress(lx, lv.y + 4); frames(1);
			for (int y = lv.y + 4; y <= lv.y + 148; y += 30) { fakeHold(lx, y); frames(1); }
			fakeRelease(); frames(1);
		}
		checkInt("dragged down: the scroll stops at 0 px", uiScroll(), 0);

		// The press rule at the viewport's edges: a row cut by an edge takes a
		// press only while 40 px of it show, which for a 40 px row is all of
		// it. Rows are at content 0, 42, 84 and 126, so 1 px of scroll leaves
		// 39 px of the top row, and 11 px leaves 39 px of row 3 at the bottom
		// (content 126 to 164 in a viewport of content 11 to 164).
		auto scrollTo = [](int px) {
			uiListScrollTo(&s_list, px);
			s_selected = -1;
			s_redraw = true;
			frames(1);
		};
		scrollTo(1);
		const UiRect top39 = ctlRect(UI_ROW(0, Row));
		checkTrue("at 1 px, the top row shows 39 px and does not act",
		          top39.y == 62 && top39.h == 39 && !ctlEnabled(UI_ROW(0, Row)));
		tap(lx, top39.y);
		checkInt("y 62 on it: no row", uiSelected(), -1);
		scrollTo(0);
		tap(lx, lv.y);
		checkInt("at 0 px, all 40 px: y 62 is the top row", uiSelected(), (int)s_visible[0]);
		scrollTo(11);
		const UiRect bot39 = ctlRect(UI_ROW(3, Row));
		checkTrue("at 11 px, row 3 shows 39 px at the bottom and does not act",
		          bot39.y + bot39.h - 1 == 215 && bot39.h == 39 && !ctlEnabled(UI_ROW(3, Row)));
		tap(lx, 215);
		checkInt("y 215 on it: no row", uiSelected(), -1);
		scrollTo(12);
		tap(lx, 215);
		checkInt("at 12 px, all 40 px: y 215 is row 3", uiSelected(), (int)s_visible[3]);
		s_selected = -1;
		tap(lx, lv.y - 1);
		tap(lx, lv.y + lv.h);
		checkInt("y 61 and y 216, outside the viewport: no row", uiSelected(), -1);

		// The editor bar and the footer do not move with the list.
		tapId(UI_ROW(2, Row));
		frames(1);
		const int below0 = lv.y + lv.h;
		uint32_t below = fakeRegionHash(0, below0, GFX_W, GFX_H - below0);
		fakePress(lx, lv.y + 94); frames(1);
		fakeHold(lx, lv.y + 64); frames(1);
		fakeRelease(); frames(2);
		checkTrue("a scroll leaves the editor bar and the footer as they were",
		          fakeRegionHash(0, below0, GFX_W, GFX_H - below0) == below);

		// HEX: six lines in 154 px, so a drag does not scroll, and the dump
		// takes neither taps nor swipes.
		int sel = uiSelected();
		uint8_t before[AM32_EEPROM_SIZE];
		memcpy(before, s_eeprom, sizeof(before));
		tapId(UI_FIX(Am32Ctl::Hex));
		checkInt("HEX opens at 0 px", uiScroll(), 0);
		checkControlTable("AM32 hex view");
		checkTrue("hex: the dump lines are not controls", !ctlPresent(UI_ROW(0, Row)));
		fakePress(lx, lv.y + 94); frames(1);
		fakeHold(lx, lv.y + 44); frames(1);
		fakeRelease(); frames(1);
		checkInt("a drag on the dump does not scroll it", uiScroll(), 0);
		tap(lx, lv.y + 5);
		swipe(lv.x + 40, lv.x + 200, lv.y + 5, 10);
		checkTrue("a tap and a swipe on it change nothing",
		          uiSelected() == sel && memcmp(before, s_eeprom, sizeof(before)) == 0);
		tapId(UI_FIX(Am32Ctl::Hex));
		checkInt("FIELDS: back at 0 px", uiScroll(), 0);
	}

	section("Write: interlock, commit, verify, hand back");
	{
		const int vPoles = uiRowOf("POLES");
		uiRowY("POLES"); frames(1);
		tapId(UI_ROW(vPoles, Row));
		uint8_t want = uiByte(0x1B);
		tapId(UI_FIX(Am32Ctl::EditPlus));
		want = uiByte(0x1B);
		checkTrue("edit is local until written", fakeEscByte(0x1B) != want);

		// WRITE's rectangle: the hold bar sits 4 px above it, and the button
		// shows red near its bottom edge while held.
		const UiRect wr = ctlRect(UI_FIX(Am32Ctl::Write));
		int wx, wy;
		ctlCentre(UI_FIX(Am32Ctl::Write), &wx, &wy);

		pressId(UI_FIX(Am32Ctl::Write));
		fakeRelease(); frames(2);
		checkTrue("a tap on WRITE does not commit", fakeEscByte(0x1B) != want);

		pressId(UI_FIX(Am32Ctl::Write));
		fakeHold(wx, wy); frameAfter(999);
		checkTrue("held 999 ms: not written", fakeEscByte(0x1B) != want && s_state == S_LIST);
		checkTrue("the bar shows the hold",
		          gfxBuffer()[(wr.y - 4) * GFX_W + wr.x + 2] == C_LIME);
		checkTrue("and the button is red while held",
		          gfxBuffer()[(wr.y + wr.h - 5) * GFX_W + wr.x + 4] == C_RED);
		fakeHold(wx, wy); frameAfter(1);
		checkTrue("OK is drawn and does not act during WRITING...", s_state == S_WRITING &&
		          ctlPresent(UI_FIX(Am32Ctl::Ok)) && !ctlEnabled(UI_FIX(Am32Ctl::Ok)));
		for (int i = 0; i < 20; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkInt("1000 ms commits and verifies", fakeEscByte(0x1B), want);
		checkControlTable("AM32 write result");
		// No card: WRITE VERIFIED fits at scale 2, and the next step is named.
		checkTrue("no card: WRITE VERIFIED at scale 2, y 140, green",
		          fakeTextCentredIs(140, "WRITE VERIFIED", C_GREEN, C_BG, 2));
		checkTrue("then " AM32_UI_POWER_CYCLE ", y 170",
		          fakeTextCentredIs(170, AM32_UI_POWER_CYCLE, C_TEXT, C_BG, 1));
		checkInt("29 characters, 173 px", gfxTextW(AM32_UI_POWER_CYCLE, 1), 173);
		fakeDumpFrame("shot_am32_written.ppm");

		tapId(UI_FIX(Am32Ctl::Ok));                      // dismiss the result
		checkTrue("OK returns to the list", s_state == S_LIST);

		// With a card: the settings as read at entry go to BACKUP/ first.
		// The write above ran without one, so the session has no backup yet.
		fakeFatReset(1u << 20);
		SdLogStatus logIdle;
		memset(&logIdle, 0, sizeof(logIdle));
		logIdle.state = SdLogState::Idle;
		fakeSdLogSet(&logIdle);
		fakeMscSet(MscState::Idle, 0);
		static char wantText[AM32_BACKUP_MAX], gotText[AM32_BACKUP_MAX];
		size_t wantLen = am32BackupFormat(s_entry, am32BlEscTypeName(), am32BlEepromAddr(),
		                                  wantText, sizeof(wantText));
		checkTrue("the entry differs from the ESC now", memcmp(s_entry, s_original, AM32_SETTINGS_SIZE) != 0);
		fakeFatWriteProtect(true);
		// POLES stands at its top, 36: a step down.
		tapId(UI_FIX(Am32Ctl::EditMinus));
		uint8_t want2 = uiByte(0x1B);
		checkTrue("a change to write", want2 != fakeEscByte(0x1B));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("a card that refuses the backup: nothing written to the ESC",
		          fakeEscByte(0x1B) != want2 && strcmp(s_status, "NO BACKUP ON THE CARD: NOT WRITTEN") == 0);
		// 34 characters: 406 px at scale 2, so scale 1, and no power cycle
		// for a write that did not happen.
		int rx0 = 0, rx1 = 0;
		checkTrue("the refusal, at scale 1, inside the panel",
		          fakeTextCentredIs(140, "NO BACKUP ON THE CARD: NOT WRITTEN", C_TEXT, C_BG, 1) &&
		          fakeInkExtent(140, 40, C_BG, &rx0, &rx1) && rx0 >= UI_MARGIN && rx1 < GFX_W - UI_MARGIN);
		checkTrue("no power-cycle line: " AM32_UI_CHECK_CARD " in amber, y 170",
		          fakeTextCentredIs(170, AM32_UI_CHECK_CARD, UI_COL_WARN, C_BG, 1));
		tapId(UI_FIX(Am32Ctl::Ok));
		fakeFatWriteProtect(false);
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		int32_t gotLen = fakeFatGet("BACKUP/AM001.TXT", gotText, sizeof(gotText));
		checkTrue("the backup is BACKUP/AM001.TXT, the settings as read at entry",
		          gotLen == (int32_t)wantLen && memcmp(gotText, wantText, wantLen) == 0);
		checkInt("then the write", fakeEscByte(0x1B), want2);
		checkTrue("the result names the backup", strcmp(s_status, "WRITE VERIFIED. BACKUP AM001") == 0);
		// With the backup named the result is 28 characters: 334 px at
		// scale 2, which ran off both edges of the 240 px panel. Every lit
		// pixel of the result's band stays 8 px or more inside the edges.
		int ex0 = 0, ex1 = 0;
		checkTrue("the result's band has text", fakeInkExtent(140, 40, C_BG, &ex0, &ex1));
		checkTrue("and it stays inside the panel's 8 px margins",
		          ex0 >= UI_MARGIN && ex1 < GFX_W - UI_MARGIN);
		checkTrue("WRITE VERIFIED. BACKUP AM001 at scale 1, y 140: 167 px",
		          gfxTextW("WRITE VERIFIED. BACKUP AM001", 1) == 167 &&
		          fakeTextCentredIs(140, "WRITE VERIFIED. BACKUP AM001", C_GREEN, C_BG, 1));
		checkTrue("then " AM32_UI_POWER_CYCLE,
		          fakeTextCentredIs(170, AM32_UI_POWER_CYCLE, C_TEXT, C_BG, 1));
		fakeDumpFrame("shot_am32_written_backup.ppm");
		tapId(UI_FIX(Am32Ctl::Ok));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("a second write saves no second backup", !fakeFatExists("BACKUP/AM002.TXT"));
		tapId(UI_FIX(Am32Ctl::Ok));
		// Another card in the slot since the backup: its serial differs.
		fakeVolumeSet(2);
		unsigned rec = fakeCacheCalls().recovers;
		tapId(UI_FIX(Am32Ctl::EditMinus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("another card: mounted again, the backup saved to it, then the write",
		          fakeCacheCalls().recovers == rec + 1 && fakeFatExists("BACKUP/AM002.TXT") &&
		          fakeEscByte(0x1B) == want2 && strcmp(s_status, "WRITE VERIFIED. BACKUP AM002") == 0);
		tapId(UI_FIX(Am32Ctl::Ok));
		// A card that does not answer its serial read: mounted again too.
		fakeVolumeSet(2, false);
		tapId(UI_FIX(Am32Ctl::EditPlus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("a card that does not answer: mounted again, the backup saved again",
		          fakeCacheCalls().recovers == rec + 2 && fakeFatExists("BACKUP/AM003.TXT"));
		tapId(UI_FIX(Am32Ctl::Ok));
		// A serial number that does not read right after the backup: the next
		// WRITE reads one, sees it differ from none, and saves again.
		s_backupDone = s_backupOnCard = false;
		fakeVolumeFailNext(1);
		tapId(UI_FIX(Am32Ctl::EditMinus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("the serial unread after the backup", fakeFatExists("BACKUP/AM004.TXT") &&
		                                              s_backupOnCard && !s_backupVolumeRead);
		tapId(UI_FIX(Am32Ctl::Ok));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("the next WRITE checks and saves again", fakeFatExists("BACKUP/AM005.TXT") &&
		                                                 fakeCacheCalls().recovers == rec + 3);
		tapId(UI_FIX(Am32Ctl::Ok));
		// USB DRIVE holding the card: no serial check, no recovery.
		fakeMscSet(MscState::Serving, 0);
		rec = fakeCacheCalls().recovers;
		tapId(UI_FIX(Am32Ctl::EditMinus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("USB holds the card: no recovery", fakeCacheCalls().recovers == rec);
		tapId(UI_FIX(Am32Ctl::Ok));
		fakeMscSet(MscState::Idle, 0);
		// A flush that fails: the ESC is not written.
		s_backupDone = false;
		s_backupOnCard = false;
		fakeCacheReset();
		cardCacheBegin(SdLogEnd::ByAm32);
		fakeCacheFlushTo(CardCache::Failed);
		uint8_t before3 = fakeEscByte(0x1B);
		tapId(UI_FIX(Am32Ctl::EditMinus));
		pressId(UI_FIX(Am32Ctl::Write));
		for (int i = 0; i < 50; i++) { fakeHold(wx, wy); frames(1); }
		fakeRelease(); frames(3);
		checkTrue("a backup the card's flush lost: nothing written to the ESC",
		          fakeEscByte(0x1B) == before3 &&
		          strcmp(s_status, "NO BACKUP ON THE CARD: NOT WRITTEN") == 0 &&
		          fakeCacheCalls().recovers == 1 && fakeCacheCalls().flushes >= 1);
		tapId(UI_FIX(Am32Ctl::Ok));
		tapId(UI_FIX(Am32Ctl::EditPlus));
		fakeCacheReset();
		logIdle.state = SdLogState::NoCard;
		fakeSdLogSet(&logIdle);
		pressId(UI_FIX(Am32Ctl::Write), IdExpect::Disabled);
		holdId(UI_FIX(Am32Ctl::Write), 50);
		fakeRelease(); frames(1);
		checkTrue("held on NO CHANGES: no write", s_state == S_LIST);
		// A change made under the same press, which the UI cannot do with
		// one finger: the press landed on a disabled button, so it is not
		// timed.
		pressId(UI_FIX(Am32Ctl::Write), IdExpect::Disabled);
		s_eeprom[0x1B] = (uint8_t)(s_original[0x1B] + 2);
		holdId(UI_FIX(Am32Ctl::Write), 60);
		checkTrue("a press on NO CHANGES, changes made under it: held 1500 ms, no write",
		          s_state == S_LIST && anyDirty());
		fakeRelease(); frames(1);
		s_eeprom[0x1B] = s_original[0x1B];
		frames(1);
		tapId(UI_FIX(Am32Ctl::Hex));                     // HEX view
		frames(2);
		fakeDumpFrame("shot_am32_hex.ppm");
		tapId(UI_FIX(Am32Ctl::Hex));                     // back to fields
		tapId(UI_FIX(UI_BACK));                   // leave AM32, back to MENU
		checkTrue("BACK from AM32 reaches MENU", uiCurrentScreen() == UiScreenId::Menu);
		checkTrue("DShot pin handed back", fakePinReturned());
		// And back to the main screen, which is what the sections after this
		// one assume. It used to be left on the settings screen and get away
		// with it: enterLogScreen()'s first tap, at a coordinate then, fell
		// into dead space there and was harmlessly ignored.
		tapId(UI_FIX(UI_BACK));
		checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
	}

	section("Splash fits the panel");
	{
		checkTrue("\"MINIRC\" at scale 4 fits", gfxTextW("MINIRC", 4) <= GFX_W);
		checkTrue("\"BENCH\" at scale 4 fits", gfxTextW("BENCH", 4) <= GFX_W);
		checkTrue("subtitle fits", gfxTextW("BIDIRECTIONAL ESC TESTER", 1) <= GFX_W);
		checkTrue("credit line fits", gfxTextW("JuWi made", 2) <= GFX_W);
		// Why the name takes two lines: as one, at scale 4, it is wider than
		// the panel.
		checkTrue("the name on one line at scale 4 would overflow",
		          gfxTextW("MINIRCBENCH", 4) > GFX_W);
	}

	testLogScreen();
	testLogEndNotes();
	testLogNumbering();
	testTesterSdErr();
	testUsbDriveScreen();
	testManualLogSurvivesArming();
	testKissDisplay();
	testTelemetryExpires();
	testMenuBeepRow();

	testBoardRow();
	testArmInterlocks();
	testIdleAutoDisarm();
	testPoleCountReachesEsc();
	testRpmGoesDead();
	testAutoLogOnArm();
	testHitBoundaries();
	testMenuDestinations();
	testRailReAnchor();
	testDisarmButton();
	testArmNeedsLivePress();
	testThrottleRailsAreReachable();
	testThrottleBarStopsAtTheButtons();
	testCeilingBindsInRelativeMode();
	testDisarmedThrottleBackstop();
	testFrameAction();
	testEdtAutoRule();
	testEdtKeptAcrossBind();
	testNoEdtEscRpm();
	testEdtBindBurstRules();
	testEdtStepRules();
	testKissCadence();

	testTapCancels();
	testStepperRepeat();
	testRepeatRule();
	testArmBacklightDip();

	testSettingsWiring();
	testStatusBarBitrate();
	testSettingsKiss();
	testSettingsServoSupply();
	testSettingsConnectorLines();
	testSettingsCurrentSense();
	testSettingsDisplay();
	testSettingsSave();
	testArmedSaveRefused();
	testPressedLookClears();
	testScreenChangeResidue();
	testTesterBoardAndCard();
	testTesterReadouts();
	testBoardCellColour();
	testSplash();
	testSplashTouch();
	testTextProbes();
	testAm32ClaimWords();
	testAm32Guidance();
	testAm32TesterNote();
	testAm32ReadOnly();
	testAm32BoardLow();
	testAm32LayoutTable();

	// Back to a known state for anything that runs after this.
	fakeFlashClear();
	settingsLoad();
	themeSet(Theme::Dark);

}
