/**
 * @file ui_sdlog.cpp
 * @brief The SD LOG screen: card state, counters, START and STOP, RETRY and
 *        USB DRIVE. @see ui_sdlog.h
 */

#include "ui_sdlog.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "gfx.h"
#include "touch.h"
#include "config.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

/**
 * @defgroup ui_log_layout Logging screen layout
 * @{
 */
#define LOG_ROW0_Y     (UI_HDR_H + 8)   /**< First status row. */
#define LOG_ROW_H      20
#define LOG_ROWS        9   /**< Through MOUNT; CARD and MOUNT are diagnostics. */
#define LOG_NOTE_Y    228   /**< One line: why a button just refused. */
#define LOG_TOGGLE_Y  240   /**< START / STOP, or EJECT while a host has the card. */
#define LOG_TOGGLE_H   44
#define LOG_BOT_Y     288   /**< RETRY MOUNT and USB DRIVE, side by side. */
#define LOG_BOT_H      30
/** @} */

static const UiRect BTN_LOG_TOGGLE = { 14, LOG_TOGGLE_Y, 212, LOG_TOGGLE_H };
// The bottom row splits rather than the screen growing: 2 x 104 with a 4 px gap
// is the 212 px between the margins, the same arithmetic the settings nav row
// uses. Both halves clear UI_TAP_MIN, which is the point of splitting rather
// than stacking.
#define LOG_BTN_W    104
#define LOG_BTN_GAP    4
static const UiRect BTN_LOG_RETRY  = { 14, LOG_BOT_Y, LOG_BTN_W, LOG_BOT_H };
static const UiRect BTN_LOG_USB    = { 14 + LOG_BTN_W + LOG_BTN_GAP, LOG_BOT_Y,
                                       LOG_BTN_W, LOG_BOT_H };

static_assert(LOG_ROW0_Y >= UI_HDR_H, "logging rows overlap the header");
static_assert(LOG_ROW0_Y + LOG_ROWS * LOG_ROW_H <= LOG_NOTE_Y,
              "logging rows overlap the note line");
static_assert(LOG_NOTE_Y + 7 <= LOG_TOGGLE_Y,
              "the note line overlaps the START/STOP button");
static_assert(LOG_TOGGLE_Y + LOG_TOGGLE_H <= LOG_BOT_Y,
              "START/STOP button overlaps the bottom row");
static_assert(LOG_BOT_Y + LOG_BOT_H <= GFX_H,
              "the bottom row runs off the panel");
static_assert(LOG_TOGGLE_H >= UI_TAP_MIN && LOG_BOT_H >= UI_TAP_MIN,
              "a logging button is smaller than a fingertip");
// Against the constants rather than the structs: a UiRect is const, not
// constexpr, so its members are not usable in a constant expression.
static_assert(14 + 2 * LOG_BTN_W + LOG_BTN_GAP <= 226,
              "RETRY and USB DRIVE do not fit between the margins");

/** @brief This frame's touch. */
static TouchState s_touch;

/** @brief What the last frame drew, so an unchanged screen is not repainted. */
static struct {
	int      config, logState, logFile, logDrops, mscState, mscNote;
	uint32_t logBytes, logFrames, logPeak, logWorstMs, mscBlocks;
} s_shown;

/** @brief True while @p b is touched by a press that began inside it. */
static bool pressing(const UiRect &b, const TouchState &t) { return uiPressing(b, &t); }
/** @brief True on the frame a tap of @p b completes. */
static bool tapped(const UiRect &b, const TouchState &t) { return uiTapped(b, &t); }

/** @brief One label/value row on the logging screen. */
static void drawLogRow(int row, const char *label, const char *value,
                       uint16_t vcol) {
	uiRow(LOG_ROW0_Y + row * LOG_ROW_H, LOG_ROW_H, label, value, vcol, 1);
}

/**
 * @brief What the last USB DRIVE press was refused for, if it was.
 *
 * Latched rather than recomputed, because the reason stops being true the
 * moment it is acted on -- disarm and the "DISARM FIRST" that told you to would
 * vanish before you had read it.
 */
static MscRefusal s_mscNote = MscRefusal::None;
static uint32_t   s_mscNoteUntilMs = 0;

/** @brief True while a refusal is still worth showing. */
static bool mscNoteActive() {
	return s_mscNote != MscRefusal::None &&
	       (int32_t)(millis() - s_mscNoteUntilMs) < 0;
}

/**
 * @brief The body of the logging screen while a host owns the card.
 *
 * Replaces the counters rather than sitting beside them, because none of them
 * mean anything now: the logger has let go of the card and its numbers are
 * frozen at whatever they were. A screen showing live-looking counters that
 * cannot move is the same lie the telemetry tiles refuse to tell.
 */
static void drawUsbPanel(MscState st) {
	gfxRect(0, LOG_ROW0_Y, GFX_W, LOG_NOTE_Y - LOG_ROW0_Y, C_BG);

	bool serving = (st == MscState::Serving);
	gfxTextCenter(LOG_ROW0_Y + 24, mscStateText(st), serving ? C_LIME : C_AMBER, 3);

	if (serving) {
		gfxTextCenter(LOG_ROW0_Y + 62, "THE CARD IS ON YOUR COMPUTER", C_TEXT, 1);
		gfxTextCenter(LOG_ROW0_Y + 78, "READ-ONLY - NOTHING CAN BE WRITTEN", C_DIM, 1);

		// Progress, because full speed USB moves about a megabyte a second and
		// a big card takes minutes. Without a number that moves, a long copy
		// and a hung one look identical.
		char buf[32];
		uint32_t kb = mscBlocksRead() / 2u;
		if (kb >= 1024) snprintf(buf, sizeof(buf), "%lu.%lu MB READ",
		                         (unsigned long)(kb / 1024), (unsigned long)((kb % 1024) * 10 / 1024));
		else            snprintf(buf, sizeof(buf), "%lu kB READ", (unsigned long)kb);
		gfxTextCenter(LOG_ROW0_Y + 104, buf, C_CYAN, 2);

		gfxTextCenter(LOG_ROW0_Y + 138, "EJECT ON THE COMPUTER FIRST,", C_GRID, 1);
		gfxTextCenter(LOG_ROW0_Y + 152, "OR PRESS EJECT BELOW", C_GRID, 1);
	} else {
		// Handover and Reclaim are both brief. Saying which one it is beats a
		// spinner: they fail differently, and a screen stuck on one of them is
		// worth being able to name in a bug report.
		gfxTextCenter(LOG_ROW0_Y + 70, "ONE MOMENT", C_DIM, 1);
	}
}

/**
 * @brief Logging status screen: card state, counters and manual start/stop.
 *
 * The counters are not decoration. `SD_LOG_BUFFER_BYTES` is a guess until it has
 * been measured, and BUF PEAK and WORST FLUSH are the two numbers to size it
 * from — a peak that approaches the buffer size, or a flush that takes longer
 * than the buffer holds, is the warning that it is too small. DROPS reaching
 * anything but zero says it already was.
 */
static void drawLogScreen() {
	SdLogStatus st;
	sdLogStatus(&st);

	MscState msc = mscGetState();
	int note = mscNoteActive() ? (int)s_mscNote + 1 : 0;
	// Blocks read is in the key so the progress figure actually advances, and
	// only while serving -- otherwise a stale counter would repaint the whole
	// screen forever at whatever the last read left it on.
	uint32_t blocks = (msc == MscState::Serving) ? mscBlocksRead() : 0;

	if (s_shown.config == 2 &&
	    s_shown.logState == (int)st.state && s_shown.logFile == st.fileNumber &&
	    s_shown.logBytes == st.bytesWritten && s_shown.logFrames == st.framesLogged &&
	    s_shown.logDrops == (int)st.dropEvents && s_shown.logPeak == st.peakBuffer &&
	    s_shown.logWorstMs == st.worstFlushMs &&
	    s_shown.mscState == (int)msc && s_shown.mscNote == note &&
	    s_shown.mscBlocks == blocks)
		return;

	s_shown.mscState   = (int)msc;
	s_shown.mscNote    = note;
	s_shown.mscBlocks  = blocks;
	s_shown.config     = 2;
	s_shown.logState   = (int)st.state;
	s_shown.logFile    = st.fileNumber;
	s_shown.logBytes   = st.bytesWritten;
	s_shown.logFrames  = st.framesLogged;
	s_shown.logDrops   = st.dropEvents;
	s_shown.logPeak    = st.peakBuffer;
	s_shown.logWorstMs = st.worstFlushMs;

	gfxFill(C_BG);
	uiHeader("SD LOG", &s_touch);

	if (msc != MscState::Idle) {
		drawUsbPanel(msc);
		// One button, and it is the way out. START, STOP and RETRY MOUNT all
		// need the card, and the card is not ours; drawing them disabled would
		// be four controls saying no where one control saying "give it back"
		// is the only thing anybody wants.
		uiButton(BTN_LOG_TOGGLE, "EJECT", C_AMBER, C_PAPER, 2,
		         pressing(BTN_LOG_TOGGLE, s_touch));
		gfxRect(0, LOG_BOT_Y, GFX_W, LOG_BOT_H, C_BG);
		return;
	}

	char buf[32];
	const char *stateTxt;
	uint16_t stateCol;
	switch (st.state) {
		case SdLogState::Logging: stateTxt = "RECORDING"; stateCol = C_RED;   break;
		case SdLogState::Idle:    stateTxt = "READY";     stateCol = C_LIME;  break;
		case SdLogState::Error:   stateTxt = "CARD ERROR";stateCol = C_AMBER; break;
		default:                  stateTxt = "NO CARD";   stateCol = C_DIM;   break;
	}
	drawLogRow(0, "STATUS", stateTxt, stateCol);

	if (st.fileNumber) snprintf(buf, sizeof(buf), "LOG%05u.BFL", (unsigned)st.fileNumber);
	else               snprintf(buf, sizeof(buf), "--");
	drawLogRow(1, "FILE", buf, C_TEXT);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long)st.framesLogged);
	drawLogRow(2, "FRAMES", buf, C_TEXT);

	// kB rather than bytes: at ~7 kB/s the byte count is unreadable within
	// seconds, and the useful question is "is it growing".
	snprintf(buf, sizeof(buf), "%lu kB", (unsigned long)(st.bytesWritten / 1024u));
	drawLogRow(3, "WRITTEN", buf, C_TEXT);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long)st.dropEvents);
	drawLogRow(4, "DROPPED FRAMES", buf, st.dropEvents ? C_RED : C_DIM);

	// Against the configured size, so "how close to the edge" needs no mental
	// arithmetic.
	snprintf(buf, sizeof(buf), "%lu / %u B", (unsigned long)st.peakBuffer,
	         (unsigned)SD_LOG_BUFFER_BYTES);
	uint16_t peakCol = C_TEXT;
	if (st.peakBuffer > SD_LOG_BUFFER_BYTES / 2) peakCol = C_AMBER;
	if (st.peakBuffer > (SD_LOG_BUFFER_BYTES * 3) / 4) peakCol = C_RED;
	drawLogRow(5, "BUF PEAK", buf, peakCol);

	snprintf(buf, sizeof(buf), "%lu ms", (unsigned long)st.worstFlushMs);
	drawLogRow(6, "WORST FLUSH", buf,
	           st.worstFlushMs > 50 ? C_AMBER : C_TEXT);

	// What the card itself reported. Non-zero here with a failed mount means the
	// card is on the bus and talking -- the fault is the filesystem, not the
	// wiring -- and that is the single most useful thing to know when a card
	// "is not detected".
	// Size, not type: card_type is an SPI-mode concept and the SDIO driver
	// leaves it at zero, so keying presence off it would report "NONE" for a
	// card that has just mounted.
	static const char *TYPE_TXT[5] = {"", "SDSC v1 ", "SDSC v2 ", "SDHC/XC ", ""};
	uint8_t ct = st.cardType < 4 ? st.cardType : 4;
	if (st.cardSizeMB >= 1024)
		snprintf(buf, sizeof(buf), "%s%lu.%luGB", TYPE_TXT[ct],
		         (unsigned long)(st.cardSizeMB / 1024),
		         (unsigned long)((st.cardSizeMB % 1024) * 10 / 1024));
	else if (st.cardSizeMB)
		snprintf(buf, sizeof(buf), "%s%luMB", TYPE_TXT[ct],
		         (unsigned long)st.cardSizeMB);
	else
		snprintf(buf, sizeof(buf), "NONE");
	drawLogRow(7, "CARD", buf, st.cardSizeMB ? C_TEXT : C_DIM);

	// FatFs FRESULT. 0 is FR_OK, 3 FR_NOT_READY (nothing answered), 13
	// FR_NO_FILESYSTEM (card fine, no filesystem FatFs can read).
	static const char *FR_TXT[] = {
		"OK", "DISK ERR", "INT ERR", "NOT READY", "NO FILE", "NO PATH",
		"BAD NAME", "DENIED", "EXIST", "BAD OBJ", "WRITE PROT",
		"BAD DRIVE", "NOT ENABLED", "NO FILESYSTEM",
	};
	if (st.mountResult < sizeof(FR_TXT) / sizeof(FR_TXT[0]))
		snprintf(buf, sizeof(buf), "%u %s", st.mountResult, FR_TXT[st.mountResult]);
	else
		snprintf(buf, sizeof(buf), "%u", st.mountResult);
	drawLogRow(8, "MOUNT", buf, st.mountResult ? C_AMBER : C_LIME);

	bool active = (st.state == SdLogState::Logging);
	bool usable = (st.state != SdLogState::NoCard);
	// Without a card there is nothing to start. It was drawn grey for that
	// reason already, and pressed it anyway. @see #23
	uiButton(BTN_LOG_TOGGLE, active ? "STOP" : "START",
	        active ? C_RED : C_PANEL, active ? C_ONACCENT : C_LIME, 2,
	        pressing(BTN_LOG_TOGGLE, s_touch), usable);

	// The card is only mounted once, at boot, so one inserted afterwards needs
	// this. Without it, "insert card, nothing happens" is indistinguishable
	// from a card the firmware cannot read.
	uiButton(BTN_LOG_RETRY, "RETRY", C_PANEL, C_CYAN, 1,
	        pressing(BTN_LOG_RETRY, s_touch));

	// Hands the card to a computer over the same USB cable that powers the
	// board. Cyan like the other navigation-ish controls; it is not
	// destructive, and it is refused rather than guarded by a hold.
	// Same again. With a card present it stays live even when it will refuse --
	// armed, or recording -- because those refusals say something the screen
	// does not already: "NO CARD TO SHARE" only repeats the CARD row above.
	uiButton(BTN_LOG_USB, "USB DRIVE", C_PANEL, C_CYAN, 1,
	        pressing(BTN_LOG_USB, s_touch), usable);

	// The note line says why the last press was refused. Blank the row even
	// when there is nothing to say, or a stale reason outlives its cause.
	gfxRect(0, LOG_NOTE_Y, GFX_W, 8, C_BG);
	if (mscNoteActive())
		gfxTextCenter(LOG_NOTE_Y, mscRefusalText(s_mscNote), C_AMBER, 1);
}

/** @brief Touch handling for the logging screen. All three fire on release. */
static bool handleLogTouch() {
	if (s_touch.down) s_shown.config = -1;   // keep the pressed look live

	// While a host owns the card, the only control that does anything is the
	// one that takes it back. BACK still works -- leaving the screen does not
	// end the handover, and should not: the copy is still running.
	if (mscGetState() != MscState::Idle) {
		if (tapped(BTN_LOG_TOGGLE, s_touch)) {
			mscRelease();
			s_shown.config = -1;
		} else if (uiBackTapped(&s_touch)) {
			return false;
		}
		return true;
	}

	// The same expression the buttons are drawn with: no card, no start and no
	// handover. RETRY is exempt and always live -- it is the control that gets
	// you *out* of the no-card state, and disabling it would be a screen with
	// nothing on it that does anything.
	SdLogStatus st;
	sdLogStatus(&st);
	const bool usable = (st.state != SdLogState::NoCard);

	if (usable && tapped(BTN_LOG_TOGGLE, s_touch)) {
		if (sdLogActive()) sdLogStop();
		else               sdLogStart();
		s_shown.config = -1;
	} else if (tapped(BTN_LOG_RETRY, s_touch)) {
		sdLogRemount();
		s_shown.config = -1;
	} else if (usable && tapped(BTN_LOG_USB, s_touch)) {
		MscRefusal r = mscRequest();
		if (r != MscRefusal::None) {
			s_mscNote = r;
			s_mscNoteUntilMs = millis() + MSC_NOTE_MS;
		}
		s_shown.config = -1;
	} else if (uiBackTapped(&s_touch)) {
		return false;
	}
	return true;
}
/** @brief The SD LOG controls, gated as handleLogTouch() gates them. */
static uint8_t sdLogControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	if (mscGetState() != MscState::Idle) {
		uiControlAdd(out, &n, max, UI_FIX(SdLogCtl::Toggle), BTN_LOG_TOGGLE, true);
		return n;
	}
	SdLogStatus st;
	sdLogStatus(&st);
	const bool usable = (st.state != SdLogState::NoCard);
	uiControlAdd(out, &n, max, UI_FIX(SdLogCtl::Toggle), BTN_LOG_TOGGLE, usable);
	uiControlAdd(out, &n, max, UI_FIX(SdLogCtl::Retry), BTN_LOG_RETRY, true);
	uiControlAdd(out, &n, max, UI_FIX(SdLogCtl::Usb), BTN_LOG_USB, usable);
	return n;
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void uiSdLogEnter() {
	memset(&s_shown, 0xFF, sizeof(s_shown));
	memset(&s_touch, 0, sizeof(s_touch));
}

bool uiSdLogTick(const TouchState *t) {
	static const TouchState none = {};
	s_touch = t ? *t : none;
	if (!handleLogTouch()) return false;
	drawLogScreen();
	return true;
}

/** @brief SD LOG's MENU status: the card's state, or USB DRIVE while a host has it. */
static void sdLogMenuStatus(char *buf, size_t n, uint16_t *col, uint16_t *) {
	MscState msc = mscGetState();
	if (msc != MscState::Idle) {
		snprintf(buf, n, "USB DRIVE");
		*col = msc == MscState::Serving ? C_LIME : C_AMBER;
		return;
	}
	SdLogStatus st;
	sdLogStatus(&st);
	switch (st.state) {
		case SdLogState::Logging: snprintf(buf, n, "RECORDING");  *col = C_RED;   break;
		case SdLogState::Idle:    snprintf(buf, n, "READY");      *col = C_LIME;  break;
		case SdLogState::Error:   snprintf(buf, n, "CARD ERROR"); *col = C_AMBER; break;
		default:                  snprintf(buf, n, "NO CARD");    *col = C_DIM;   break;
	}
}

const UiScreen UI_SDLOG = {
	UiScreenId::SdLog, "SD LOG", PinUser::None, sdLogMenuStatus,
	uiSdLogEnter, uiSdLogTick, nullptr, sdLogControls, nullptr,
};
