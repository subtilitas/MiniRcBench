/**
 * @file ui_hitec.cpp
 * @brief The HITEC SERVO screen. @see ui_hitec.h
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * The screen draws and reads the touch; the session runs on core1
 * (hitec_job.h), what a parameter shows and how it steps is hitec_view.h,
 * and the card is hitec_files.h. A view is drawn whole when a hash of what it
 * shows changes, so a still screen costs no SPI (Serial Peripheral Interface)
 * traffic.
 */

#include "ui_hitec.h"
#include "card_cache.h"
#include "config.h"
#include "core1_job.h"
#include "esc_task.h"
#include "hitec_files.h"
#include "hitec_job.h"
#include "hitec_view.h"
#include "kst_job.h"
#include "kst_seek.h"
#include "kst_view.h"
#include "pin_claim.h"
#include "sd_log.h"
#include "servo_supply.h"
#include "settings.h"
#include "touch.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_list.h"
#include "ui_screen.h"
#include "ui_widgets.h"
#include "usb_msc.h"
#include "sdio/sd_host.h"
#include "gfx.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------

/**
 * @defgroup hitec_layout HITEC screen layout
 * @brief The KST screen's: the field list, its editor bar and footer, the
 *        file list, and the one wide button of the other views.
 * @{
 */
#define HL_ROW_H     UI_LIST_TAP_MIN  /**< A list row, 40 px. */
#define HL_ROW_PITCH 42               /**< Row top to row top. */
#define HL_LIST_Y1   215              /**< Last row of the field list. */
#define HL_EDIT_Y    218              /**< The editor bar. */
#define HL_EDIT_H    44
#define HL_STEP_W    44               /**< - and +. */
#define HL_FOOT_Y    268              /**< HOLD TO WRITE, REVERT, FILES; the wide button. */
#define HL_FOOT_H    46
#define HF_LIST_Y1   229              /**< Last row of the file list. */
#define HF_SAVE_Y    234              /**< SAVE AS, or RETRY MOUNT. */
#define HF_BTN_H     40
#define HF_LOAD_Y    278              /**< LOAD and HOLD TO RESTORE. */
/** @brief Files the FILES page lists at most. */
#define HITEC_UI_FILES_MAX 64
/** @brief Changes the write page lists before AND n MORE. */
#define HITEC_UI_CHANGES_MAX 6
/** @} */

static const UiRect FIELD_VIEW  = { 0, UI_BODY_Y, GFX_W, HL_LIST_Y1 - UI_BODY_Y + 1 };
static const UiRect FILE_VIEW   = { 0, UI_BODY_Y, GFX_W, HF_LIST_Y1 - UI_BODY_Y + 1 };
static const UiRect BTN_CLASSIC = { 6, 80, 228, 46 };
static const UiRect BTN_DSERIES = { 6, 168, 228, 46 };
static const UiRect BTN_MINUS   = { 6, HL_EDIT_Y, HL_STEP_W, HL_EDIT_H };
static const UiRect BTN_PLUS    = { GFX_W - 6 - HL_STEP_W, HL_EDIT_Y, HL_STEP_W, HL_EDIT_H };
static const UiRect BTN_REPAIR  = { 6, HL_EDIT_Y, 70, HL_EDIT_H };
static const UiRect BTN_WRITE   = { 6, HL_FOOT_Y, 110, HL_FOOT_H };
static const UiRect BTN_REVERT  = { 122, HL_FOOT_Y, 52, HL_FOOT_H };
static const UiRect BTN_FILES   = { 180, HL_FOOT_Y, 54, HL_FOOT_H };
static const UiRect BTN_WIDE    = { 6, HL_FOOT_Y, 228, HL_FOOT_H };
static const UiRect BTN_SAVE    = { 6, HF_SAVE_Y, 228, HF_BTN_H };
static const UiRect BTN_LOAD    = { 6, HF_LOAD_Y, 110, HF_BTN_H };
static const UiRect BTN_RESTORE = { 122, HF_LOAD_Y, 112, HF_BTN_H };
static const UiRect BTN_RESTART = { 40, 200, 160, 40 };
static const UiRect BTN_LREAD   = { 6, 214, 228, 46 };

static_assert(HL_LIST_Y1 < HL_EDIT_Y, "the field list overlaps the editor bar");
static_assert(HL_EDIT_Y + HL_EDIT_H + UI_HOLD_BAR_H < HL_FOOT_Y,
              "the editor bar overlaps the footer's hold bar");
static_assert(HL_FOOT_Y + HL_FOOT_H <= GFX_H, "the footer runs off the panel");
static_assert(HF_LIST_Y1 < HF_SAVE_Y && HF_SAVE_Y + HF_BTN_H + UI_HOLD_BAR_H < HF_LOAD_Y,
              "the file page's rows overlap");
static_assert(HF_LOAD_Y + HF_BTN_H <= GFX_H, "the file page's buttons run off the panel");
static_assert(HL_ROW_H >= UI_LIST_TAP_MIN && HL_ROW_PITCH > HL_ROW_H,
              "a Hitec row is lower than the list floor, or has no gap");
static_assert(HL_STEP_W >= UI_TAP_MIN && HL_FOOT_H >= UI_TAP_MIN && HF_BTN_H >= UI_TAP_MIN,
              "a Hitec button is smaller than a fingertip");

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static HitecView    s_view = HitecView::Family;
static bool         s_classicMeasured = HITEC_CLASSIC_MEASURED != 0;
static uint8_t      s_family = HITEC_D;     /**< The family chosen. */
static bool         s_claimed = false;      /**< pinClaimBegin() took the claim. */
static ClaimRefusal s_refusal = ClaimRefusal::None;
static HitecStatus  s_st;                   /**< This frame's status from core1. */
static const TouchState *s_t = nullptr;

/**
 * @defgroup hitec_ui_session The session as the screen keeps it
 * @{
 */
static uint16_t      s_seq = 0;             /**< Seq of the operation sent last. */
static HitecOp       s_op = HitecOp::ReadAll;
static bool          s_opOpen = false;      /**< An operation was sent and has not ended. */
static bool          s_enterSent = false;
static bool          s_readFailed = false;  /**< The last read did not complete. */
static bool          s_haveDraft = false;
static hitec_image_t s_draft;               /**< The servo with the pending changes. */
static hitec_image_t s_base;                /**< The servo the draft's edits are made against. */
static uint8_t       s_layout = HITEC_LAYOUT_D;
static hitec_image_t s_pageFrom;            /**< The servo when the write page opened. */
static hitec_image_t s_pageTo;              /**< What the write page writes. */
static bool          s_firstDone = false;   /**< The session's first write put its files on the card. */
static bool          s_wroteOnce = false;   /**< The session has written: no more horn line unless travel moves. */
static HitecWrite    s_write;               /**< What the last write put on the card. */
static uint32_t      s_firstVolume = 0;     /**< The serial number of the volume the backup went to. */
static bool          s_firstOnCard = false; /**< The backup went to a card: the swap check is armed. */
static bool          s_firstVolumeRead = false;
static bool          s_recordFound = false; /**< HITEC/HITEC.TXT on the card. */
static bool          s_lastFailed = false;  /**< The last write did not end verified. */
static char          s_otherModel[24];      /**< A loaded file of another model: its model; empty for none. */
/** @} */

/**
 * @defgroup hitec_ui_views View state
 * @{
 */
static UiList      s_list;
static HitecRow    s_rows[HITEC_ROWS_MAX];
static int         s_rowCount = 0;
static int         s_sel = -1;
static Repeat      s_repeat;
static UiHold      s_hold;
static bool        s_writing = false;
static bool        s_flushing = false;
static bool        s_restoring = false;
static bool        s_okPress = false;
static bool        s_result = false;
static uint8_t     s_resultSes = 0;
static HitecRefusal s_resultRefused = HitecRefusal::None;
static const char *s_cardNote = nullptr;
static bool        s_leaveQueued = false;
static char        s_preset[HITEC_FILES_NAME_MAX];
static bool        s_saveFull = false;
static char        s_backupNext[BACKUP_NAME_MAX];
static bool        s_presetCard = false;

static UiList         s_fileList;
static HitecFileName  s_files[HITEC_UI_FILES_MAX];
static BackupFileName s_backups[BACKUP_RING_NAMES];
static int            s_backupCount = 0;
static int            s_fileCount = 0;
static int            s_fileSel = -1;
static char           s_fileNote[64];
static uint16_t       s_fileNoteCol = 0;
/** @} */

static uint32_t s_drawnKey = 0;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static bool pressing(const UiRect &r) { return uiPressing(r, s_t); }
static bool tapped(const UiRect &r) { return uiTapped(r, s_t); }

static uint8_t sigGpio() {
	uint8_t g = pinClaimGpio(PIN_SIG);
	return g != PIN_CLAIM_NO_GPIO ? g : escTaskDshotPin();
}

/** @brief The claim holds SIG and the job is bound. */
static bool running() {
	return s_claimed && pinClaimState() == ClaimState::Running && pinClaimJobError() == 0;
}

/** @brief The seq of the last command sent, an operation or a card phase's. */
static uint16_t s_sentSeq = 0;

static bool taken(uint16_t ackSeq, uint16_t seq) { return (int16_t)(ackSeq - seq) >= 0; }

/** @brief The refusal of the operation sent last, latched while the status answers its command. */
static HitecRefusal s_opRefused = HitecRefusal::None;

/** @brief Latch the operation's refusal from @p st when it answers the operation's command. */
static void latchRefusal(const HitecStatus &st) {
	if (st.ackSeq == s_seq) s_opRefused = st.refused;
}

static void sendOp(HitecOp op, const hitec_image_t *target = nullptr) {
	s_opRefused = HitecRefusal::None;
	HitecCmd c;
	memset(&c, 0, sizeof(c));
	c.op = op;
	if (target) c.target = *target;
	s_seq = hitecJobCommand(&c);
	s_sentSeq = s_seq;
	s_op = op;
	s_opOpen = op == HitecOp::ReadAll || op == HitecOp::Write;
}

/** @brief The operation sent last has ended: taken, and no longer running. */
static bool opEnded() {
	return s_opOpen && taken(s_st.ackSeq, s_seq) && s_st.job != HitecJobState::Busy;
}

/** @brief How long a card phase waits for the job to take its command, ms. */
#define HITEC_CARD_BEGIN_WAIT_MS 50u

static uint8_t s_cardDepth = 0;
static bool    s_cardSent = false;

/**
 * @brief A card phase around the card calls of a scope, with the switched
 *        supply, as KST's KstCardPhase: the job counts the SDIO driver's
 *        waits as the heartbeat, for at most HITEC_CARD_PHASE_MAX_MS. A
 *        command still held gets no phase, so CardBegin never replaces it.
 */
struct HitecCardPhase {
	HitecCardPhase() {
		if (s_cardDepth++ || !running() || !s_st.supplyFitted) return;
		HitecStatus st;
		uint32_t t0 = millis();
		for (hitecJobStatus(&st); !taken(st.ackSeq, s_sentSeq) && millis() - t0 < HITEC_CARD_BEGIN_WAIT_MS;
		     hitecJobStatus(&st))
			kstCardWaitPoll();
		if (!taken(st.ackSeq, s_sentSeq)) return;
		latchRefusal(st);
		HitecCmd c;
		memset(&c, 0, sizeof(c));
		c.op = HitecOp::CardBegin;
		uint16_t seq = hitecJobCommand(&c);
		s_sentSeq = seq;
		s_cardSent = true;
		t0 = millis();
		for (hitecJobStatus(&st); !taken(st.ackSeq, seq) && millis() - t0 < HITEC_CARD_BEGIN_WAIT_MS;
		     hitecJobStatus(&st))
			kstCardWaitPoll();
		if (!taken(st.ackSeq, seq) || !st.cardPhase) return;
		hitecCardStamp();
		sdh_wait_hook = hitecCardStamp;
	}
	~HitecCardPhase() {
		if (--s_cardDepth || !s_cardSent) return;
		escHeartbeat();
		HitecCmd c;
		memset(&c, 0, sizeof(c));
		c.op = HitecOp::CardEnd;
		s_sentSeq = hitecJobCommand(&c);
		sdh_wait_hook = nullptr;
		s_cardSent = false;
	}
	HitecCardPhase(const HitecCardPhase &) = delete;
	HitecCardPhase &operator=(const HitecCardPhase &) = delete;
};

/** @brief The pending changes: editable parameters the draft holds other than the servo. */
static int pending() {
	return s_haveDraft && s_st.hasImage ? hitecChangedCount(&s_st.image, &s_draft, s_layout) : 0;
}

/** @brief In the mode with no operation open. */
static bool idleMode() {
	return running() && s_st.job == HitecJobState::InMode && !s_opOpen;
}

/** @brief In the mode, read with its backup, nothing running. */
static bool idleRead() { return idleMode() && s_st.hasImage && s_st.hasBackup; }

static bool writeAllowed() { return idleRead() && !s_st.unknown && pending() > 0; }

static bool restoreAllowed() {
	return idleRead() && !s_st.unknown && hitecChangedCount(&s_st.image, &s_st.backup, s_layout) > 0;
}

static bool cardHeldByUsb() { return mscHoldsCard(mscGetState()); }

static bool classicAllowed() { return s_classicMeasured && servoSupplyFitted(); }

static void go(HitecView v) {
	if (v != HitecView::Write) s_flushing = false;
	s_view = v;
	uiHoldReset(&s_hold);
	s_repeat = Repeat{};
	s_drawnKey = 0;
}

/** @brief No first write yet: no backup or record of the session's own. */
static void forgetFirst() {
	memset(&s_write, 0, sizeof(s_write));
	s_write.backup = s_write.record = s_write.kept = HitecFiles::NoCard;
	s_firstDone = false;
	s_firstOnCard = false;
	s_firstVolumeRead = false;
	s_wroteOnce = false;
}

/** @brief Forget the servo: an entry follows. */
static void forgetServo() {
	s_haveDraft = false;
	s_sel = -1;
	s_readFailed = false;
	s_writing = false;
	s_result = false;
	s_lastFailed = false;
	s_otherModel[0] = '\0';
}

/** @brief A new session: the claim's first frame, or after BACK on the unplug page. */
static void newSession() {
	HitecCardPhase phase;
	s_opOpen = false;
	s_enterSent = false;
	forgetFirst();
	forgetServo();
	s_leaveQueued = false;
	s_cardNote = nullptr;
	s_recordFound = hitecFilesRecordPresent();
	escHeartbeat();
	s_rowCount = 0;
	uiListInit(&s_list, FIELD_VIEW, HL_ROW_PITCH, 0);
	uiListSetBody(&s_list, HL_ROW_H, UI_LIST_TAP_MIN);
}

/** @brief The rows of the servo that was read, and the list on them. */
static void layoutRows() {
	s_layout = hitecLayoutOf(&s_st.image);
	s_rowCount = hitecRows(s_layout, s_rows);
	uiListInit(&s_list, FIELD_VIEW, HL_ROW_PITCH, s_rowCount);
	uiListSetBody(&s_list, HL_ROW_H, UI_LIST_TAP_MIN);
	s_sel = -1;
}

static void claimPin() {
	ClaimRefusal r = pinClaimBegin(PinUser::Hitec, PIN_SIG);
	s_claimed = r == ClaimRefusal::None;
	s_refusal = r;
	if (s_claimed) cardCacheBegin();
}

static void readFiles();

// ---------------------------------------------------------------------------
// the session's events
// ---------------------------------------------------------------------------

/**
 * @brief A read completed: the draft is the servo, the list on its layout.
 *        A read with a draft keeps its pending edits: the writable
 *        addresses they changed go onto the servo as read.
 */
static void readDone() {
	bool fresh = !s_haveDraft || hitecLayoutOf(&s_st.image) != s_layout;
	hitec_image_t keep = s_draft;
	if (fresh) layoutRows();
	s_draft = s_st.image;
	if (!fresh) {
		// The writable addresses the edits changed, as they stand in the
		// draft: a side effect of one edit, such as DIRECTION's mirrored end
		// points, comes with it, and no edit runs twice.
		bool any = false;
		for (unsigned a = 0; a < HITEC_MEM_SIZE; a++) {
			uint8_t x = (uint8_t)a;
			if (!hitec_writable(s_family, x) || !hitec_image_known(&keep, x) || !hitec_image_known(&s_base, x))
				continue;
			if (s_family == HITEC_D) {
				if (hitec_image_reg(&keep, x) == hitec_image_reg(&s_base, x)) continue;
				hitec_image_put_reg(&s_draft, x, hitec_image_reg(&keep, x));
			} else {
				if (hitec_image_cell(&keep, x) == hitec_image_cell(&s_base, x)) continue;
				hitec_image_put_cell(&s_draft, x, hitec_image_cell(&keep, x));
			}
			any = true;
		}
		if (any && s_family == HITEC_CLASSIC) hitec_c_rebalance(&s_draft);
	}
	s_base = s_st.image;
	s_haveDraft = true;
	s_readFailed = false;
	s_otherModel[0] = '\0';
	if (s_view == HitecView::Main) go(HitecView::List);
}

static void opEnd() {
	s_opOpen = false;
	// The status's refusal is that of the last command taken; a card phase's
	// command after the operation has its own, so the operation's is latched.
	latchRefusal(s_st);
	HitecRefusal refused = s_opRefused;
	if (s_op == HitecOp::ReadAll) {
		if (refused == HitecRefusal::None && s_st.result == HITEC_SES_OK && s_st.hasImage) readDone();
		else s_readFailed = true;
		return;
	}
	s_writing = false;
	s_result = true;
	s_resultRefused = refused;
	s_resultSes = refused == HitecRefusal::None ? s_st.result : (uint8_t)HITEC_SES_ERR_BUSY;
	s_lastFailed = s_resultSes != HITEC_SES_OK;
	if (s_resultSes == HITEC_SES_OK) {
		HitecCardPhase phase;
		hitecFilesRecordRemove(&s_write);
		s_recordFound = hitecFilesMounted() && hitecFilesRecordPresent();
		s_draft = s_st.image;
		s_base = s_st.image;
		s_otherModel[0] = '\0';
	}
	if (s_leaveQueued) {
		s_leaveQueued = false;
		go(HitecView::Leave);
	}
}

static void follow() {
	if (!running()) return;
	latchRefusal(s_st);
	if (s_write.record == HitecFiles::Ok && !hitecFilesMounted()) s_write.record = HitecFiles::NoCard;
	if (s_st.job == HitecJobState::Opening && !s_enterSent) {
		sendOp(HitecOp::Enter);
		s_enterSent = true;
	}
	// The entry's own full read: the first image of the session.
	if (s_st.job == HitecJobState::InMode && !s_opOpen && !s_haveDraft) {
		if (s_st.hasImage) readDone();
		else if (s_st.result != HITEC_SES_OK && !s_readFailed) s_readFailed = true;
	}
	// A stall or a failed entry: the session is gone with the supply.
	if (s_st.job == HitecJobState::EntryEnded && s_haveDraft) {
		forgetServo();
		forgetFirst();
		s_opOpen = false;
		s_leaveQueued = false;
		if (s_view != HitecView::Leave && s_view != HitecView::Unplug) go(HitecView::Main);
	}
	if (opEnded()) opEnd();
}

// ---------------------------------------------------------------------------
// the main view: the claim, the entry and the read
// ---------------------------------------------------------------------------

/** @brief What the main view shows. */
struct MainText {
	char        strip[40];
	const char *big;
	const char *line[8];
	uint16_t    lineCol[8];
	const char *button;
	HitecCtl    ctl;
	bool        enabled;
};

static void addLine(MainText *m, const char *s, uint16_t c) {
	for (int i = 0; i < 8; i++)
		if (!m->line[i]) { m->line[i] = s; m->lineCol[i] = c; return; }
}

static void addWrapped(MainText *m, const char *text, uint16_t c, int lines = 2) {
	static char wl[4][3][48];
	static int next = 0;
	char (*buf)[48] = wl[next];
	next = (next + 1) % 4;
	int n = kstWrap(text, 37, buf, lines > 3 ? 3 : lines);
	for (int i = 0; i < n; i++) addLine(m, buf[i], c);
}

/** @brief The amber line of every entry view with the switch. */
static void escLines(MainText *m) {
	if (!s_st.supplyFitted) return;
	addLine(m, "AN ESC ON THIS LEAD GETS THE", C_AMBER);
	addLine(m, "CONVERTER'S OUTPUT ON ITS BEC WIRE.", C_AMBER);
}

static void mainText(MainText *m) {
	static char a[48], b[48];
	memset(m, 0, sizeof(*m));
	uint8_t sig = sigGpio();
	if (!s_claimed) {
		switch (s_refusal) {
			case ClaimRefusal::EscAnswering:
				snprintf(a, sizeof(a), "AN ESC ANSWERS ON GP%u.", sig);
				addLine(m, a, C_AMBER);
				addLine(m, "UNPLUG IT: A HITEC ENTRY SENDS", C_AMBER);
				addLine(m, "FRAMES INTO WHAT IS ON THE WIRE.", C_AMBER);
				snprintf(m->strip, sizeof(m->strip), "CHECKING GP%u FOR AN ESC", sig);
				return;
			case ClaimRefusal::Armed: addLine(m, "DISARM FIRST", C_AMBER); return;
			default:                  addLine(m, "PINS BUSY", C_AMBER);    return;
		}
	}
	ClaimState cs = pinClaimState();
	if (cs == ClaimState::Stopping) {
		snprintf(m->strip, sizeof(m->strip), "RELEASING GP%u", sig);
		return;
	}
	uint8_t err = pinClaimJobError();
	if (err) {
		static const char *const WHY[] = {"", "REFUSED", "PWM", "PIO", "DMA", "CLOCK"};
		snprintf(a, sizeof(a), "GP%u COULD NOT BE TAKEN (%s)", sig, WHY[err < 6 ? err : 1]);
		addLine(m, a, C_AMBER);
		m->button = "RETRY"; m->ctl = HitecCtl::Retry; m->enabled = true;
		return;
	}
	if (cs != ClaimState::Running) {
		snprintf(m->strip, sizeof(m->strip), "TAKING GP%u", sig);
		return;
	}
	bool classic = s_family == HITEC_CLASSIC;
	switch (s_st.job) {
		case HitecJobState::Entering:
			m->button = "STOP"; m->ctl = HitecCtl::Stop; m->enabled = true;
			if (s_st.power == HitecPower::Off) {
				snprintf(m->strip, sizeof(m->strip), "GP%u  SUPPLY OFF %u.%u S", sig,
				         s_st.supplyLeftDs / 10, s_st.supplyLeftDs % 10);
				m->big = "SUPPLY OFF";
				addLine(m, classic ? "THEN ON: THE SERVO ENTERS SERIAL" : "THEN ON: THE SERVO STARTS, AND",
				        C_DIM);
				addLine(m, classic ? "MODE AT ITS POWER-UP." : "THE BENCH READS ITS MODEL.", C_DIM);
			} else if (classic) {
				snprintf(m->strip, sizeof(m->strip), "GP%u  SUPPLY ON", sig);
				m->big = "LISTENING";
				addLine(m, "FOR THE SERVO'S + AFTER ITS", C_DIM);
				addLine(m, "POWER-UP, THEN THE FIRST COMMAND.", C_DIM);
			} else {
				snprintf(m->strip, sizeof(m->strip), "GP%u  %s", sig, s_st.supplyFitted ? "SUPPLY ON" : "");
				m->big = s_st.supplyFitted ? "SERVO STARTING" : "READING THE MODEL";
				addLine(m, "THEN THE BENCH READS ITS MODEL.", C_DIM);
			}
			escLines(m);
			return;
		case HitecJobState::EntryEnded:
			m->button = "RETRY"; m->ctl = HitecCtl::Retry; m->enabled = true;
			if (s_st.stalled) {
				snprintf(m->strip, sizeof(m->strip), "GP%u  %s", sig, s_st.supplyFitted ? "SUPPLY OFF" : "STOPPED");
				if (s_st.supplyFitted) {
					addWrapped(m, "THE SCREEN STALLED, SO THE BENCH SWITCHED THE SERVO'S SUPPLY OFF.", C_AMBER);
					addLine(m, "RETRY POWERS IT UP AND READS IT AGAIN.", C_AMBER);
				} else {
					addWrapped(m, "THE SCREEN STALLED. RETRY ENTERS AGAIN.", C_AMBER);
				}
				if (!hitecFilesMounted() && s_st.stallWrote)
					addLine(m, "NO CARD: THE BACKUP IN MEMORY IS LOST.", C_AMBER);
				return;
			}
			snprintf(m->strip, sizeof(m->strip), "%s", classic ? "NO SERVO IN SERIAL MODE" : "NO SERVO ANSWERS");
			if (s_st.result == HITEC_SES_ERR_ABORTED) {
				addLine(m, "THE ENTRY WAS STOPPED.", C_TEXT);
			} else {
				snprintf(a, sizeof(a), "REPLY: %s", s_st.lastRx ? hitecRxWord(s_st.lastRx) : "NONE");
				addLine(m, a, C_AMBER);
				addWrapped(m, classic ? "CHECK THE CONVERTER ON GP0, THE SIGNAL AND GROUND."
				                      : "CHECK THE SERVO'S SUPPLY, THE SIGNAL AND GROUND.", C_TEXT);
			}
			if (s_st.supplyFitted) addLine(m, "THE SUPPLY IS OFF.", C_DIM);
			return;
		case HitecJobState::Busy:
		case HitecJobState::InMode:
			snprintf(m->strip, sizeof(m->strip), "GP%u  IN SERIAL MODE", sig);
			if (s_readFailed && s_st.job == HitecJobState::InMode) {
				if (s_st.result == HITEC_SES_ERR_ABORTED) {
					addLine(m, "THE READ WAS STOPPED.", C_AMBER);
				} else {
					snprintf(a, sizeof(a), "READ FAILED AT 0x%02X", s_st.failAddr);
					addLine(m, a, C_AMBER);
					snprintf(b, sizeof(b), "REPLY: %s", hitecRxWord(s_st.lastRx));
					addLine(m, b, C_AMBER);
				}
				m->button = "READ AGAIN"; m->ctl = HitecCtl::ReadAgain; m->enabled = idleMode();
				return;
			}
			snprintf(a, sizeof(a), "READING %u %s", (unsigned)s_st.total,
			         classic ? "CELLS" : "REGISTERS");
			addLine(m, a, C_TEXT);
			snprintf(b, sizeof(b), "%u OF %u", (unsigned)s_st.done, (unsigned)s_st.total);
			addLine(m, b, C_DIM);
			m->button = "STOP"; m->ctl = HitecCtl::Stop; m->enabled = s_st.job == HitecJobState::Busy;
			return;
		default:
			snprintf(m->strip, sizeof(m->strip), "STARTING THE ENTRY ON GP%u", sig);
			return;
	}
}

static void drawMain() {
	static MainText m;
	mainText(&m);
	uiHeader("HITEC SERVO", s_t);
	uiStripClear();
	if (m.strip[0]) uiStripText(m.strip, C_DIM);
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	int y = 80;
	if (m.big) { gfxTextCenter(70, m.big, C_LIME, 2); y = 100; }
	for (int i = 0; i < 8 && m.line[i]; i++) {
		gfxText(UI_MARGIN, y, m.line[i], m.lineCol[i], 1);
		y += 14;
	}
	if (running() && s_recordFound) {
		gfxText(UI_MARGIN, 230, "A WRITE WAS INTERRUPTED. HITEC/", C_AMBER, 1);
		gfxText(UI_MARGIN, 242, "HITEC.TXT HOLDS THE SERVO AS IT WAS.", C_AMBER, 1);
	}
	if (m.button)
		uiButton(BTN_WIDE, m.button, C_PANEL, C_TEXT, 2, m.enabled && pressing(BTN_WIDE), m.enabled);
}

// ---------------------------------------------------------------------------
// the family view
// ---------------------------------------------------------------------------

static void drawFamily() {
	uiHeader("HITEC SERVO", s_t);
	uiStripClear();
	uiStripText("CHOOSE THE SERVO'S FAMILY", C_DIM);
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	bool c = classicAllowed();
	uiButton(BTN_CLASSIC, "CLASSIC", C_PANEL, C_TEXT, 2, c && pressing(BTN_CLASSIC), c);
	gfxText(UI_MARGIN, BTN_CLASSIC.y + BTN_CLASSIC.h + 4, "HS-5XXX, HS-7XXX", C_DIM, 1);
	const char *why = !s_classicMeasured ? "LINE LEVEL NOT MEASURED"
	                  : !servoSupplyFitted() ? "NEEDS SERVO SUPPLY GP0" : "";
	if (why[0]) gfxText(UI_MARGIN, BTN_CLASSIC.y + BTN_CLASSIC.h + 18, why, C_AMBER, 1);
	uiButton(BTN_DSERIES, "D SERIES", C_PANEL, C_TEXT, 2, pressing(BTN_DSERIES));
	gfxText(UI_MARGIN, BTN_DSERIES.y + BTN_DSERIES.h + 4, "D485HW, D645MW AND THE OTHERS", C_DIM, 1);
	gfxText(UI_MARGIN, BTN_DSERIES.y + BTN_DSERIES.h + 18,
	        servoSupplyFitted() ? "SUPPLY SWITCHED ON GP0" : "ON THE SERVO'S OWN SUPPLY", C_DIM, 1);
}

static void familyTouch() {
	uint8_t f = 0xFF;
	if (classicAllowed() && tapped(BTN_CLASSIC)) f = HITEC_CLASSIC;
	else if (tapped(BTN_DSERIES)) f = HITEC_D;
	if (f == 0xFF) return;
	s_family = f;
	hitecJobSetFamily(f);
	newSession();
	go(HitecView::Main);
	claimPin();
}

// ---------------------------------------------------------------------------
// the field list
// ---------------------------------------------------------------------------

/** @brief The selected row is the classic CHECKSUM reading BAD: REPAIR in place of - and +. */
static bool repairRow() {
	if (s_sel < 0 || s_rows[s_sel].param != HITEC_P_C_CHECKSUM) return false;
	hitec_value_t v;
	return hitec_param_get(&s_draft, s_layout, HITEC_P_C_CHECKSUM, &v) == HITEC_GET_OK &&
	       v.value == HITEC_SUM_BAD;
}

static bool selectable(int i) {
	if (i < 0 || i >= s_rowCount) return false;
	if (hitecEditable(s_layout, s_rows[i].param)) return true;
	if (s_rows[i].param != HITEC_P_C_CHECKSUM) return false;
	hitec_value_t v;
	return hitec_param_get(&s_st.image, s_layout, HITEC_P_C_CHECKSUM, &v) == HITEC_GET_OK &&
	       v.value == HITEC_SUM_BAD;
}

/**
 * @brief Whether - (@p dir -1) or + steps the selected row. The answer is
 *        kept while the draft and the selection stay: at the D series order's
 *        bound one answer tries the whole range.
 */
static bool stepOk(int dir) {
	if (s_sel < 0 || !hitecEditable(s_layout, s_rows[s_sel].param) || !idleRead()) return false;
	static hitec_image_t s_memoDraft;
	static int s_memoSel = -1;
	static bool s_memo[2];
	if (s_memoSel != s_sel || memcmp(&s_memoDraft, &s_draft, sizeof(s_draft)) != 0) {
		s_memoDraft = s_draft;
		s_memoSel = s_sel;
		s_memo[0] = hitecCanStep(&s_draft, s_layout, s_rows[s_sel].param, -1);
		s_memo[1] = hitecCanStep(&s_draft, s_layout, s_rows[s_sel].param, +1);
	}
	return s_memo[dir > 0];
}

static void drawFieldRow(int i, int y, void *) {
	const HitecRow &r = s_rows[i];
	bool edit = hitecEditable(s_layout, r.param);
	bool sel = i == s_sel;
	bool changed = hitecWritten(s_layout, r.param) && hitecDiffers(&s_st.image, &s_draft, s_layout, r.param);
	gfxRect(0, y, GFX_W, HL_ROW_H - 1, sel ? C_PANEL : C_BG);
	gfxText(UI_MARGIN, y + 11, r.group, C_GRID, 1);
	char label[24];
	snprintf(label, sizeof(label), "%s%s", changed ? "*" : "", r.label);
	gfxText(UI_MARGIN, y + 21, label, edit || selectable(i) ? (sel ? C_TEXT : C_DIM) : C_GRID, 1);
	char v[32];
	bool raw = false;
	hitecValueText(&s_draft, s_layout, r.param, v, sizeof(v), &raw);
	int sc = gfxTextW(v, 2) <= 104 ? 2 : 1;
	uint16_t col = raw ? C_AMBER : !edit ? (selectable(i) ? C_AMBER : C_GRID) : (changed ? C_LIME : C_TEXT);
	gfxText(GFX_W - UI_MARGIN - gfxTextW(v, sc), y + (HL_ROW_H - 1 - 7 * sc + 1) / 2, v, col, sc);
	if (changed && hitecSoftRules(&s_st.image, &s_draft, s_layout, r.param))
		gfxRect(0, y, 3, HL_ROW_H - 1, C_AMBER);
	gfxHLine(0, y + HL_ROW_H - 1, GFX_W, C_PANEL);
}

/** @brief The first rule of @p rules. */
static HitecRule firstRule(uint32_t rules) {
	for (int i = 0; i < (int)HitecRule::Count; i++)
		if (rules & HITEC_RULE_BIT(i)) return (HitecRule)i;
	return HitecRule::Count;
}

/** @brief The write button: READ AGAIN after a write that left the image not known. */
static bool readFirst() { return s_st.unknown; }

static void drawList() {
	uiHeader("HITEC SERVO", s_t);
	uiStripClear();
	char model[24], strip[48], backup[24], gp[8];
	hitecModelText(&s_st.image, model, sizeof(model));
	char ver[8] = "";
	if (s_family == HITEC_CLASSIC) snprintf(ver, sizeof(ver), " V%02X", s_st.image.version);
	snprintf(gp, sizeof(gp), " GP%u", sigGpio());
	if (s_write.backupName[0]) snprintf(backup, sizeof(backup), " BACKUP %.5s", s_write.backupName);
	else                       snprintf(backup, sizeof(backup), " BACKUP IN MEMORY");
	// The longest of these that ends before the MODE chip, 4 px clear of it.
	const int room = GFX_W - UI_MARGIN - (gfxTextW("MODE", 1) + 12) - 4 - UI_MARGIN;
	snprintf(strip, sizeof(strip), "%.12s%s%s%s", model, ver, gp, backup);
	if (gfxTextW(strip, 1) > room) snprintf(strip, sizeof(strip), "%.12s%s%s", model, ver, backup);
	if (gfxTextW(strip, 1) > room) snprintf(strip, sizeof(strip), "%.12s%s%s", model, ver, gp);
	uiStripText(strip, C_DIM);
	uiChip("MODE", C_GREEN);
	uiListDraw(&s_list, drawFieldRow, nullptr);

	gfxRect(0, HL_LIST_Y1 + 1, GFX_W, HL_FOOT_Y - HL_LIST_Y1 - 1, C_BG);
	if (s_sel < 0) {
		gfxTextCenter(HL_EDIT_Y + 12, "TAP A ROW TO EDIT IT", C_GRID, 1);
		gfxTextCenter(HL_EDIT_Y + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_GRID, 1);
	} else if (repairRow()) {
		uiButton(BTN_REPAIR, "REPAIR", C_GRID, C_ONACCENT, 1, idleRead() && pressing(BTN_REPAIR), idleRead());
		char lines[3][48];
		int x = BTN_REPAIR.x + BTN_REPAIR.w + 4;
		int n = kstWrap("CHECKSUM BAD. THE SERVO MAY IGNORE PWM AND DRAW HIGH CURRENT.", (GFX_W - x - 6) / 6, lines, 3);
		for (int i = 0; i < n; i++) gfxText(x, HL_EDIT_Y + 4 + 12 * i, lines[i], C_AMBER, 1);
	} else {
		const HitecRow &r = s_rows[s_sel];
		bool edit = hitecEditable(s_layout, r.param);
		int cx = BTN_MINUS.x + BTN_MINUS.w + 2, cw = BTN_PLUS.x - 2 - cx;
		if (edit) {
			bool m = stepOk(-1), p = stepOk(+1);
			uiButton(BTN_MINUS, "-", C_GRID, C_ONACCENT, 2, m && pressing(BTN_MINUS), m);
			uiButton(BTN_PLUS, "+", C_GRID, C_ONACCENT, 2, p && pressing(BTN_PLUS), p);
		}
		char now[32], want[32], top[64];
		hitecValueText(&s_st.image, s_layout, r.param, now, sizeof(now), nullptr);
		hitecValueText(&s_draft, s_layout, r.param, want, sizeof(want), nullptr);
		uint32_t rules = edit ? hitecSoftRules(&s_st.image, &s_draft, s_layout, r.param) : 0;
		const char *note = rules ? hitecRuleText(firstRule(rules)) : nullptr;
		hitec_value_t id;
		if (r.param == HITEC_P_D_ID && hitec_param_get(&s_draft, s_layout, HITEC_P_D_ID, &id) == HITEC_GET_OK &&
		    id.value != 0)
			note = "THE BENCH ADDRESSES ID 0. ANOTHER ID IS NOT TESTED.";
		if (note) {
			snprintf(top, sizeof(top), "%s %s", r.label, want);
			if (gfxTextW(top, 1) > cw) snprintf(top, sizeof(top), "%s", want);
			gfxText(cx + (cw - gfxTextW(top, 1)) / 2, HL_EDIT_Y + 2, top, C_LIME, 1);
			char lines[3][48];
			int n = kstWrap(note, cw / 6, lines, 3);
			for (int i = 0; i < n; i++)
				gfxText(cx + (cw - gfxTextW(lines[i], 1)) / 2, HL_EDIT_Y + 14 + 10 * i, lines[i], C_AMBER, 1);
		} else {
			bool changed = strcmp(now, want) != 0;
			if (changed) snprintf(top, sizeof(top), "%s  %s ->", r.label, now);
			else         snprintf(top, sizeof(top), "%s%s", r.label, edit ? "" : ": READ ONLY");
			if (gfxTextW(top, 1) > cw) snprintf(top, sizeof(top), "%s", changed ? "->" : "");
			gfxText(cx + (cw - gfxTextW(top, 1)) / 2, HL_EDIT_Y + 2, top, C_DIM, 1);
			int sc = gfxTextW(want, 2) <= cw ? 2 : 1;
			gfxText(cx + (cw - gfxTextW(want, sc)) / 2, HL_EDIT_Y + 16, want, edit ? C_LIME : C_GRID, sc);
		}
	}

	gfxRect(0, HL_FOOT_Y - 4, GFX_W, GFX_H - HL_FOOT_Y + 4, C_BG);
	if (readFirst()) {
		bool ok = idleMode();
		uiButton(BTN_WRITE, "READ AGAIN", C_PANEL, C_TEXT, 1, ok && pressing(BTN_WRITE), ok);
	} else {
		bool w = writeAllowed();
		uiHoldDrawBar(&s_hold, BTN_WRITE, HL_FOOT_Y - 4, millis());
		uiButton(BTN_WRITE, "HOLD TO WRITE", s_hold.holding ? C_RED : C_PANEL,
		         s_hold.holding ? C_ONACCENT : C_TEXT, 1, s_hold.holding, w);
	}
	bool rv = pending() > 0;
	uiButton(BTN_REVERT, "REVERT", C_PANEL, C_TEXT, 1, rv && pressing(BTN_REVERT), rv);
	uiButton(BTN_FILES, "FILES", C_PANEL, C_TEXT, 1, pressing(BTN_FILES));
}

/** @brief Open the write page for @p to, from the servo as it is. */
static void openWrite(const hitec_image_t *to, bool restoring) {
	s_restoring = restoring;
	s_result = false;
	s_cardNote = nullptr;
	s_backupNext[0] = '\0';
	s_presetCard = hitecFilesMounted();
	if (!s_firstDone && s_presetCard) {
		HitecCardPhase phase;
		hitecFilesNextBackup(s_backupNext);
	}
	s_pageFrom = s_st.image;
	s_pageTo = *to;
	go(HitecView::Write);
}

static void listTouch() {
	if (readFirst()) {
		if (idleMode() && tapped(BTN_WRITE)) {
			s_readFailed = false;
			sendOp(HitecOp::ReadAll);
		}
	} else if (uiHoldUpdate(&s_hold, BTN_WRITE, s_t, writeAllowed(), millis())) {
		openWrite(&s_draft, false);
		return;
	}
	if (pending() && tapped(BTN_REVERT)) {
		s_draft = s_st.image;
		s_otherModel[0] = '\0';
	}
	if (tapped(BTN_FILES)) {
		s_fileSel = -1;
		s_fileNote[0] = '\0';
		go(HitecView::Files);
		readFiles();
		return;
	}
	if (repairRow()) {
		if (idleRead() && tapped(BTN_REPAIR)) hitec_c_rebalance(&s_draft);
	} else {
		int dir = 0;
		if (pressing(BTN_MINUS) && stepOk(-1)) dir = -1;
		else if (pressing(BTN_PLUS) && stepOk(+1)) dir = +1;
		if (dir && repeatFires(&s_repeat, dir, millis()))
			hitecStep(&s_draft, s_layout, s_rows[s_sel].param, dir);
		if (!dir) repeatFires(&s_repeat, 0, millis());
	}
	UiListEvent e = uiListTouch(&s_list, s_t);
	if (e.kind == UiListEv::Tap || e.kind == UiListEv::Swipe) {
		if (selectable(e.row)) s_sel = e.row;
		if (e.steps && idleRead() && hitecEditable(s_layout, s_rows[e.row].param))
			hitecStep(&s_draft, s_layout, s_rows[e.row].param,
			          e.steps * hitecCoarse(s_layout, s_rows[e.row].param));
	}
}

// ---------------------------------------------------------------------------
// the write page
// ---------------------------------------------------------------------------

/** @brief This write's record: the servo before it and the target. */
static hitec_card_record_t recordOf() {
	hitec_card_record_t r;
	memset(&r, 0, sizeof(r));
	r.series = s_family;
	r.port = 0;
	r.pin = sigGpio();
	r.before = s_st.image;
	r.target = s_pageTo;
	return r;
}

/** @brief The card calls before a write; false when the card took no file. */
static bool writeFiles() {
	HitecCardPhase phase;
	hitec_card_record_t rec = recordOf();
	if (!s_firstDone) {
		hitecFilesFirstWrite(&s_st.backup, &rec, &s_write);
		if (!hitecFilesWriteAllowed(&s_write)) {
			forgetFirst();
			return false;
		}
		s_firstDone = !(s_write.backup == HitecFiles::NoCard && s_write.record == HitecFiles::NoCard);
		s_firstOnCard = s_firstDone;
		s_firstVolumeRead = s_firstOnCard && cardVolumeSerial(&s_firstVolume);
	} else {
		HitecWrite w;
		hitecFilesRecordWrite(&s_write, &rec, &w);
		// A later write keeps the session's backup name.
		memcpy(w.backupName, s_write.backupName, sizeof(w.backupName));
		s_write = w;
	}
	s_recordFound = hitecFilesMounted() && hitecFilesRecordPresent();
	return true;
}

static void sendWrite() {
	escHeartbeat();
	s_writing = true;
	sendOp(HitecOp::Write, &s_pageTo);
}

static void recoverFlush() {
	HitecCardPhase phase;
	cardCacheRecover();
	forgetFirst();
}

static bool cardSwapped() {
	HitecCardPhase phase;
	uint32_t v = 0;
	return s_firstOnCard && cardMounted() &&
	       (!s_firstVolumeRead || !cardVolumeSerial(&v) || v != s_firstVolume);
}

static void restartOnNewCard() {
	HitecCardPhase phase;
	recoverFlush();
	s_backupNext[0] = '\0';
	if (hitecFilesMounted()) hitecFilesNextBackup(s_backupNext);
}

/** @brief What a flush that runs holds up: the files of this write, or the write itself. */
enum class FlushWait : uint8_t { None, BeforeFiles, BeforeWrite };
static FlushWait s_flushWait = FlushWait::None;
/** @brief The batch the write waits for holds the session's backup: its first write. */
static bool s_flushFirst = false;

static void filesThenWrite();

static const char *const FLUSH_FAILED = "THE CARD DID NOT TAKE THE BACKUP. NOTHING WAS WRITTEN TO THE SERVO.";

/**
 * @brief The flush a write waits for has ended: with an earlier batch on the
 *        card, this write's files follow; with this write's own batch, the
 *        write goes out. A failed flush stops it.
 */
static void flushStep() {
	if (!s_flushing) return;
	CardCache c = cardCacheState();
	if (c == CardCache::Flushing) return;
	s_flushing = false;
	FlushWait w = s_flushWait;
	s_flushWait = FlushWait::None;
	if (c == CardCache::Failed) {
		recoverFlush();
		s_cardNote = w == FlushWait::BeforeFiles || !s_flushFirst
		                 ? "THE CARD DID NOT TAKE A FILE. NOTHING WAS WRITTEN TO THE SERVO."
		                 : FLUSH_FAILED;
		return;
	}
	if (w == FlushWait::BeforeFiles) filesThenWrite();
	else sendWrite();
}

/** @brief Wait for the flush that runs; @p w says what follows it. */
static void waitFlush(FlushWait w) {
	s_flushing = true;
	s_flushWait = w;
	flushStep();
}

/**
 * @brief The swap check, this write's files, the flush barrier, then the
 *        write: every write puts its record on the card before its first frame.
 */
static void filesThenWrite() {
	if (cardSwapped()) restartOnNewCard();
	s_flushFirst = !s_firstDone;
	if (!writeFiles()) {
		s_cardNote = "THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.";
		return;
	}
	if (cardCacheState() != CardCache::Off) {
		waitFlush(FlushWait::BeforeWrite);
		return;
	}
	sendWrite();
}

static void startWrite() {
	s_cardNote = nullptr;
	s_result = false;
	if (cardCacheState() == CardCache::Failed) recoverFlush();
	// With the swap check armed, an earlier batch still flushing ends before
	// this write's files go in, so the check reads the volume they land on.
	if (s_firstOnCard && cardCacheState() == CardCache::Flushing) {
		waitFlush(FlushWait::BeforeFiles);
		return;
	}
	filesThenWrite();
}

/** @brief Whether the write moves the travel: the horn line shows. */
static bool travelMoves() {
	static const uint8_t TRAVEL[] = {
		HITEC_P_D_DIRECTION, HITEC_P_D_NEUTRAL, HITEC_P_D_LEFT_END, HITEC_P_D_RIGHT_END,
		HITEC_P_C_DIRECTION, HITEC_P_C_CENTRE, HITEC_P_C_LEFT_END, HITEC_P_C_RIGHT_END,
	};
	for (uint8_t p : TRAVEL)
		if (hitec_param(s_layout, p) && hitecDiffers(&s_pageFrom, &s_pageTo, s_layout, p)) return true;
	return false;
}

static bool holdOk() {
	if (s_restoring) return restoreAllowed();
	return idleRead() && !s_st.unknown && hitecChangedCount(&s_st.image, &s_pageTo, s_layout) > 0;
}

static void drawWrite() {
	uiHeader(s_restoring ? "HITEC RESTORE" : "HITEC WRITE", s_t);
	uiStripClear();
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	char buf[96];
	int y = UI_BODY_Y;
	int changed = hitecChangedCount(&s_pageFrom, &s_pageTo, s_layout);
	snprintf(buf, sizeof(buf), "%s%d VALUE%s", s_restoring ? "BACK TO THE BACKUP: " : "", changed,
	         changed == 1 ? "" : "S");
	uiStripText(buf, C_DIM);
	int shown = 0;
	for (int i = 0; i < s_rowCount; i++) {
		uint8_t p = s_rows[i].param;
		if (!hitecWritten(s_layout, p) || !hitecDiffers(&s_pageFrom, &s_pageTo, s_layout, p)) continue;
		if (shown == HITEC_UI_CHANGES_MAX) {
			snprintf(buf, sizeof(buf), "AND %d MORE", changed - shown);
			gfxText(UI_MARGIN, y, buf, C_DIM, 1);
			y += 12;
			break;
		}
		char a[32], b[32];
		hitecValueText(&s_pageFrom, s_layout, p, a, sizeof(a), nullptr);
		hitecValueText(&s_pageTo, s_layout, p, b, sizeof(b), nullptr);
		gfxText(UI_MARGIN, y, s_rows[i].label, C_TEXT, 1);
		snprintf(buf, sizeof(buf), "%s -> %s", a, b);
		int vy = UI_MARGIN + gfxTextW(s_rows[i].label, 1) + 6 > GFX_W - UI_MARGIN - gfxTextW(buf, 1) ? y + 10 : y;
		gfxText(GFX_W - UI_MARGIN - gfxTextW(buf, 1), vy, buf, C_LIME, 1);
		y = vy + 12;
		shown++;
	}
	// Notes above the state lines at y 196, 12 px a line.
	uint32_t soft = hitecSoftRules(&s_pageFrom, &s_pageTo, s_layout);
	char wl[3][48];
	auto note = [&](const char *text, bool star) {
		int n = kstWrap(text, 36, wl, 3);
		for (int k = 0; k < n && y <= 180; k++, y += 12) {
			if (star && k == 0) gfxText(UI_MARGIN, y, "*", C_AMBER, 1);
			gfxText(UI_MARGIN + 12, y, wl[k], C_AMBER, 1);
		}
	};
	for (int i = 0; i < (int)HitecRule::Count; i++)
		if (soft & HITEC_RULE_BIT(i)) note(hitecRuleText((HitecRule)i), true);
	if (!hitecModelMeasured(&s_st.image))
		note(s_family == HITEC_D ? "MODEL NOT MEASURED: WRITTEN IN THE D LAYOUT."
		                         : "MODEL NOT MEASURED: WRITTEN IN THE CLASSIC LAYOUT.", false);
	hitec_value_t id;
	if (s_family == HITEC_D && hitecDiffers(&s_pageFrom, &s_pageTo, s_layout, HITEC_P_D_ID) &&
	    hitec_param_get(&s_pageTo, s_layout, HITEC_P_D_ID, &id) == HITEC_GET_OK && id.value != 0)
		note("THE BENCH ADDRESSES ID 0. ANOTHER ID IS NOT TESTED.", false);
	if (s_otherModel[0] && !s_restoring) {
		snprintf(buf, sizeof(buf), "FILE OF ANOTHER MODEL: %s.", s_otherModel);
		note(buf, false);
	}
	if ((!s_wroteOnce || travelMoves()) && y <= 180) {
		gfxText(UI_MARGIN, y, "KEEP THE SERVO HORN CLEAR.", C_TEXT, 1);
		y += 12;
	}

	if (s_write.backupName[0]) snprintf(buf, sizeof(buf), "BACKUP: BACKUP/%s", s_write.backupName);
	else if (s_firstDone) snprintf(buf, sizeof(buf), "NO BACKUP ON THE CARD: IN MEMORY");
	else if (!hitecFilesMounted())
		snprintf(buf, sizeof(buf), "%s", cardHeldByUsb() ? "CARD IN USE BY USB: BACKUP IN MEMORY"
		                                                : "NO CARD: BACKUP IN MEMORY");
	else if (s_backupNext[0]) snprintf(buf, sizeof(buf), "BACKUP FIRST: BACKUP/%s", s_backupNext);
	else snprintf(buf, sizeof(buf), "BACKUP FIRST: TO THE CARD");
	gfxText(UI_MARGIN, 236, buf, C_DIM, 1);
	if (s_flushing) {
		uint16_t left = 0;
		cardCacheState(&left);
		gfxText(UI_MARGIN, 196, s_flushWait == FlushWait::BeforeWrite && s_flushFirst
		                            ? "SAVING THE BACKUP TO THE CARD." : "SAVING THE FILES TO THE CARD.",
		        C_AMBER, 1);
		snprintf(buf, sizeof(buf), "BLOCKS LEFT: %u", (unsigned)left);
		gfxText(UI_MARGIN, 210, buf, C_TEXT, 1);
		return;
	}
	if (s_writing) {
		gfxText(UI_MARGIN, 196, "WRITING: DO NOT UNPLUG.", C_AMBER, 1);
		if (s_st.planSteps && s_st.stepIndex >= s_st.planSteps) snprintf(buf, sizeof(buf), "READING BACK");
		else snprintf(buf, sizeof(buf), "WRITES SENT: %u OF %u", (unsigned)s_st.stepIndex, (unsigned)s_st.planSteps);
		gfxText(UI_MARGIN, 210, buf, C_TEXT, 1);
		if (s_leaveQueued) gfxText(UI_MARGIN, 252, "LEAVING AFTER THE WRITE ENDS.", C_AMBER, 1);
		return;
	}
	if (s_result || s_cardNote) {
		HitecResultText r;
		if (s_cardNote) {
			snprintf(r.text, sizeof(r.text), "%s", s_cardNote);
			r.tone = HitecTone::Nothing;
		} else if (s_resultRefused == HitecRefusal::NoUi) {
			snprintf(r.text, sizeof(r.text), "THE SCREEN STALLED. NOTHING WAS WRITTEN.");
			r.tone = HitecTone::Nothing;
		} else {
			r = hitecResultText(s_resultSes, changed, s_st.stepIndex, s_st.planSteps, s_st.diff,
			                    s_st.failAddr, s_st.planResult, s_family);
		}
		uint16_t c = r.tone == HitecTone::Done ? C_GREEN : (r.tone == HitecTone::Doubt ? C_RED : C_AMBER);
		char rl[3][48];
		int n = kstWrap(r.text, 38, rl, 3);
		for (int k = 0; k < n; k++) gfxText(UI_MARGIN, 196 + 12 * k, rl[k], c, 1);
		uiButton(BTN_WIDE, "OK", C_PANEL, C_TEXT, 2, pressing(BTN_WIDE));
		return;
	}
	bool ok = holdOk();
	uiHoldDrawBar(&s_hold, BTN_WIDE, HL_FOOT_Y - 4, millis());
	uiButton(BTN_WIDE, s_restoring ? "HOLD TO RESTORE" : "HOLD TO WRITE", s_hold.holding ? C_RED : C_PANEL,
	         s_hold.holding ? C_ONACCENT : C_TEXT, 2, s_hold.holding, ok);
}

static void writeTouch() {
	if (!(s_result || s_cardNote)) s_okPress = false;
	else if (s_t && s_t->pressed) s_okPress = true;
	flushStep();
	if (s_writing || s_flushing) return;
	bool card = hitecFilesMounted();
	if (card && !s_presetCard && !s_firstDone) {
		HitecCardPhase phase;
		hitecFilesNextBackup(s_backupNext);
		escHeartbeat();
	}
	s_presetCard = card;
	if (s_result || s_cardNote) {
		if (s_okPress && tapped(BTN_WIDE)) {
			s_result = false;
			s_cardNote = nullptr;
			go(HitecView::List);
		}
		return;
	}
	if (uiHoldUpdate(&s_hold, BTN_WIDE, s_t, holdOk(), millis())) {
		s_wroteOnce = true;
		startWrite();
	}
}

// ---------------------------------------------------------------------------
// FILES
// ---------------------------------------------------------------------------

/** @brief Whether the servo's factory block is a row: classic only. */
static bool factoryRow() { return s_family == HITEC_CLASSIC && s_st.hasImage; }

/** @brief Rows: the record, the backups, FACTORY VALUES, then the files. */
static int fileRows() { return (s_recordFound ? 1 : 0) + s_backupCount + (factoryRow() ? 1 : 0) + s_fileCount; }

enum class FileRow : uint8_t { Record, Backup, Factory, File };

static FileRow rowKind(int i, int *idx) {
	if (s_recordFound) {
		if (i == 0) return FileRow::Record;
		i--;
	}
	if (i < s_backupCount) { *idx = i; return FileRow::Backup; }
	i -= s_backupCount;
	if (factoryRow()) {
		if (i == 0) return FileRow::Factory;
		i--;
	}
	*idx = i;
	return FileRow::File;
}

static void readFiles() {
	HitecCardPhase phase;
	if (cardCacheState() == CardCache::Failed) recoverFlush();
	CardDirScope scope(HITEC_FILES_DIR, hitecFilesMounted());
	s_fileCount = 0;
	s_backupCount = 0;
	s_recordFound = hitecFilesRecordPresent();
	if (hitecFilesMounted()) {
		size_t count = 0;
		if (hitecFilesList(s_files, HITEC_UI_FILES_MAX, &count) != HitecFiles::Ok && !s_fileNote[0]) {
			snprintf(s_fileNote, sizeof(s_fileNote), "THE CARD'S LIST DID NOT READ");
			s_fileNoteCol = C_AMBER;
		}
		s_fileCount = (int)count;
		size_t nb = 0;
		if (hitecFilesBackups(s_backups, BACKUP_RING_NAMES, &nb) == HitecFiles::Ok) s_backupCount = (int)nb;
	}
	uiListInit(&s_fileList, FILE_VIEW, HL_ROW_PITCH, fileRows());
	uiListSetBody(&s_fileList, HL_ROW_H, UI_LIST_TAP_MIN);
	if (s_fileSel >= fileRows()) s_fileSel = -1;
	s_preset[0] = '\0';
	s_saveFull = false;
	if (hitecFilesMounted()) s_saveFull = hitecFilesSaveName(s_preset) == HitecFiles::NoNumber;
}

static void drawFileRow(int i, int y, void *) {
	bool sel = i == s_fileSel;
	gfxRect(0, y, GFX_W, HL_ROW_H - 1, sel ? C_PANEL : C_BG);
	int idx = 0;
	FileRow k = rowKind(i, &idx);
	char name[24];
	const char *tag = "";
	uint16_t tagCol = C_GRID;
	switch (k) {
		case FileRow::Record:  snprintf(name, sizeof(name), "RECORD"); tag = "(HITEC.TXT)"; tagCol = C_AMBER; break;
		case FileRow::Backup:  snprintf(name, sizeof(name), "%.5s", s_backups[idx]); tag = "(BACKUP)"; break;
		case FileRow::Factory: snprintf(name, sizeof(name), "FACTORY VALUES"); tag = "(SERVO)"; break;
		default:               snprintf(name, sizeof(name), "%.5s", s_files[idx]); break;
	}
	gfxText(UI_MARGIN, y + 16, name, sel ? C_TEXT : C_DIM, 1);
	gfxText(GFX_W - UI_MARGIN - gfxTextW(tag, 1), y + 16, tag, tagCol, 1);
	gfxHLine(0, y + HL_ROW_H - 1, GFX_W, C_PANEL);
}

static bool loadOk() {
	if (s_fileSel < 0 || !idleRead()) return false;
	int idx;
	return rowKind(s_fileSel, &idx) == FileRow::Factory || hitecFilesMounted();
}

static void drawFiles() {
	uiHeader("HITEC FILES", s_t);
	uiStripClear();
	char strip[48];
	bool mounted = hitecFilesMounted();
	if (s_fileNote[0])              uiStripText(s_fileNote, s_fileNoteCol);
	else if (mounted && s_saveFull) uiStripText("HITEC/ HOLDS 50 FILES. REMOVE SOME.", C_AMBER);
	else if (cardHeldByUsb())       uiStripText("CARD IN USE BY USB", C_AMBER);
	else if (!mounted)              uiStripText("NO CARD", C_AMBER);
	else {
		snprintf(strip, sizeof(strip), "CARD  %d FILE%s", s_fileCount, s_fileCount == 1 ? "" : "S");
		uiStripText(strip, C_DIM);
	}
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	uiListDraw(&s_fileList, drawFileRow, nullptr);
	if (!mounted) {
		gfxTextCenter(160, "READ, EDIT AND WRITE STILL WORK.", C_DIM, 1);
		bool retry = !cardHeldByUsb();
		uiButton(BTN_SAVE, "RETRY MOUNT", C_PANEL, C_TEXT, 1, retry && pressing(BTN_SAVE), retry);
	} else {
		char save[32];
		snprintf(save, sizeof(save), "SAVE AS %.5s", s_preset[0] ? s_preset : "HT???");
		bool ok = s_preset[0] && idleRead();
		uiButton(BTN_SAVE, save, C_PANEL, C_TEXT, 1, ok && pressing(BTN_SAVE), ok);
	}
	bool l = loadOk();
	uiButton(BTN_LOAD, "LOAD", C_PANEL, C_TEXT, 1, l && pressing(BTN_LOAD), l);
	bool r = restoreAllowed();
	uiHoldDrawBar(&s_hold, BTN_RESTORE, HF_LOAD_Y - 4, millis());
	uiButton(BTN_RESTORE, "HOLD TO RESTORE", s_hold.holding ? C_RED : C_PANEL,
	         s_hold.holding ? C_ONACCENT : C_TEXT, 1, s_hold.holding, r);
}

static void fileNote(uint16_t col, const char *text) {
	snprintf(s_fileNote, sizeof(s_fileNote), "%s", text);
	s_fileNoteCol = col;
}

/** @brief The writable values of @p from into the draft, as pending edits. */
static void takeWritable(const hitec_image_t *from) {
	s_draft = s_st.image;
	for (unsigned a = 0; a < HITEC_MEM_SIZE; a++) {
		if (!hitec_writable(s_family, (uint8_t)a) || !hitec_image_known(from, (uint8_t)a)) continue;
		if (s_family == HITEC_D) hitec_image_put_reg(&s_draft, (uint8_t)a, hitec_image_reg(from, (uint8_t)a));
		else                     hitec_image_put_cell(&s_draft, (uint8_t)a, hitec_image_cell(from, (uint8_t)a));
	}
	if (s_family == HITEC_CLASSIC) hitec_c_rebalance(&s_draft);
}

static void loadSelected() {
	HitecCardPhase phase;
	if (cardCacheState() == CardCache::Failed) {
		recoverFlush();
		s_fileSel = -1;
		s_fileNote[0] = '\0';
		readFiles();
		if (!s_fileNote[0] && hitecFilesMounted()) fileNote(C_AMBER, "CARD MOUNTED AGAIN: CHOOSE AGAIN");
		return;
	}
	int idx = 0;
	FileRow k = rowKind(s_fileSel, &idx);
	if (k == FileRow::Factory) {
		hitec_image_t f = s_st.image;
		if (!hitec_c_factory(&f)) {
			fileNote(C_AMBER, "THE FACTORY BLOCK WAS NOT READ");
			return;
		}
		takeWritable(&f);
		s_otherModel[0] = '\0';
		go(HitecView::List);
		return;
	}
	static hitec_image_t img;
	HitecFiles r;
	if (k == FileRow::Record) {
		static hitec_card_record_t rec;
		r = hitecFilesRecordLoad(&rec);
		if (r == HitecFiles::Ok && rec.series != s_family) r = HitecFiles::OtherSeries;
		if (r == HitecFiles::Ok) img = rec.before;
	} else if (k == FileRow::Backup) {
		r = hitecFilesBackupLoad(s_backups[idx], s_family, &img);
	} else {
		r = hitecFilesLoad(s_files[idx], s_family, &img);
	}
	if (r == HitecFiles::OtherSeries || (r == HitecFiles::Ok && hitecLayoutOf(&img) != s_layout)) {
		char note[48];
		snprintf(note, sizeof(note), "NOT A FILE FOR THIS SERVO: %s",
		         r == HitecFiles::OtherSeries ? (s_family == HITEC_D ? "CLASSIC" : "D SERIES") : "LAYOUT");
		fileNote(C_AMBER, note);
		return;
	}
	if (r == HitecFiles::BadText) { fileNote(C_AMBER, "THE FILE DID NOT PARSE"); return; }
	if (r != HitecFiles::Ok)      { fileNote(C_AMBER, "THE FILE DID NOT READ"); return; }
	takeWritable(&img);
	char mine[24], theirs[24];
	hitecModelText(&s_st.image, mine, sizeof(mine));
	hitecModelText(&img, theirs, sizeof(theirs));
	if (strcmp(mine, theirs) != 0) snprintf(s_otherModel, sizeof(s_otherModel), "%s", theirs);
	else                           s_otherModel[0] = '\0';
	go(HitecView::List);
}

static void saveAs() {
	HitecCardPhase phase;
	if (cardCacheState() == CardCache::Failed) recoverFlush();
	char saved[HITEC_FILES_NAME_MAX];
	HitecFiles r = hitecFilesSaveAs(&s_st.image, saved);
	if (r == HitecFiles::Ok && cardCacheFlush() == CardCache::Failed) r = HitecFiles::WriteError;
	if (cardCacheState() == CardCache::Failed) recoverFlush();
	char *dot = strrchr(saved, '.');
	if (dot) *dot = '\0';
	char note[32];
	snprintf(note, sizeof(note), "SAVED AS %s", saved);
	if (r == HitecFiles::Ok) fileNote(C_GREEN, note);
	else if (r == HitecFiles::Full) fileNote(C_AMBER, "THE CARD IS FULL");
	else if (r == HitecFiles::NoNumber) fileNote(C_AMBER, "HITEC/ HOLDS 50 FILES. REMOVE SOME.");
	else fileNote(C_AMBER, "THE CARD TOOK NO FILE");
	readFiles();
	s_fileSel = -1;
}

static void filesTouch() {
	if (uiHoldUpdate(&s_hold, BTN_RESTORE, s_t, restoreAllowed(), millis())) {
		openWrite(&s_st.backup, true);
		return;
	}
	if (!hitecFilesMounted()) {
		if (!cardHeldByUsb() && tapped(BTN_SAVE)) {
			HitecCardPhase phase;
			s_fileNote[0] = '\0';
			sdLogRemount();
			readFiles();
		}
	} else if (s_preset[0] && idleRead() && tapped(BTN_SAVE)) {
		saveAs();
	}
	if (loadOk() && tapped(BTN_LOAD)) {
		loadSelected();
		return;
	}
	UiListEvent e = uiListTouch(&s_fileList, s_t);
	if (e.kind == UiListEv::Tap) s_fileSel = e.row;
}

// ---------------------------------------------------------------------------
// the leave page and the unplug page
// ---------------------------------------------------------------------------

static void drawLeave() {
	uiHeader("HITEC SERVO", s_t);
	uiStripClear();
	char strip[48];
	if (s_haveDraft) {
		char model[24];
		hitecModelText(&s_st.image, model, sizeof(model));
		snprintf(strip, sizeof(strip), "%.16s  GP%u  %s", model, sigGpio(),
		         s_st.supplyFitted ? (s_st.supplyOn ? "SUPPLY ON" : "SUPPLY OFF") : "");
	} else {
		snprintf(strip, sizeof(strip), "GP%u", sigGpio());
	}
	uiStripText(strip, C_DIM);
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	gfxText(UI_MARGIN, 76, s_st.supplyFitted ? "LEAVE SWITCHES THE SERVO'S SUPPLY OFF."
	                                         : "LEAVE RESTARTS THE SERVO.", C_AMBER, 1);
	int y = 96;
	char buf[48];
	int n = pending();
	if (n) {
		snprintf(buf, sizeof(buf), "%d CHANGE%s NOT WRITTEN.", n, n == 1 ? "" : "S");
		gfxText(UI_MARGIN, y, buf, C_TEXT, 1);
		y += 14;
	}
	if (s_lastFailed) {
		gfxText(UI_MARGIN, y, "A WRITE DID NOT END VERIFIED.", C_AMBER, 1);
		y += 14;
	}
	if (s_leaveQueued && s_writing) gfxText(UI_MARGIN, y, "LEAVING AFTER THE WRITE ENDS.", C_AMBER, 1);
	bool rd = idleMode();
	uiButton(BTN_LREAD, "READ AGAIN", C_PANEL, C_TEXT, 2, rd && pressing(BTN_LREAD), rd);
	bool ok = !s_writing;
	uiButton(BTN_WIDE, "LEAVE", C_PANEL, C_TEXT, 2, ok && pressing(BTN_WIDE), ok);
}

static void drawUnplug() {
	bool due = pinClaimUnplugDue();
	uiHeader("HITEC SERVO", s_t);
	gfxRect(0, UI_STRIP_Y, GFX_W, GFX_H - UI_STRIP_Y, C_BG);
	if (!due) {
		if (s_st.supplyUsed && s_st.supplyLeftDs) {
			char w[48];
			snprintf(w, sizeof(w), "SUPPLY OFF FOR %u.%u S: %u.%u S LEFT.", KST_SUPPLY_OFF_MS / 1000u,
			         KST_SUPPLY_OFF_MS / 100u % 10u, s_st.supplyLeftDs / 10, s_st.supplyLeftDs % 10);
			gfxTextCenter(120, w, C_AMBER, 1);
		} else {
			gfxTextCenter(120, "RELEASING THE PIN", C_DIM, 1);
		}
		return;
	}
	gfxTextCenter(100, "UNPLUG THE SERVO.", C_AMBER, 2);
	char b[48];
	snprintf(b, sizeof(b), "DSHOT RESTARTS ON GP%u AND READS AS", sigGpio());
	gfxTextCenter(140, b, C_TEXT, 1);
	gfxTextCenter(154, "A 970 US PULSE AT 1000 HZ.", C_TEXT, 1);
	uiButton(BTN_RESTART, "RESTART DSHOT", C_PANEL, C_TEXT, 1, pressing(BTN_RESTART));
}

static bool endClaim() {
	HitecCardPhase phase;
	if (!s_claimed) return false;
	cardCacheEnd();
	pinClaimEnd(PinUser::Hitec);
	ClaimState cs = pinClaimState();
	if (cs == ClaimState::Free || cs == ClaimState::Restoring) return false;
	go(HitecView::Unplug);
	return true;
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

HitecView uiHitecView() { return s_view; }

bool uiHitecClassicMeasured() { return s_classicMeasured; }

void uiHitecSetClassicMeasured(bool measured) { s_classicMeasured = measured; }

uint8_t uiHitecControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	switch (s_view) {
		case HitecView::Family:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Classic), BTN_CLASSIC, classicAllowed());
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::DSeries), BTN_DSERIES, true);
			return n;
		case HitecView::Unplug: {
			bool due = pinClaimUnplugDue();
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), due);
			if (due) uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Restart), BTN_RESTART, true);
			return n;
		}
		case HitecView::Main: {
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			static MainText m;
			mainText(&m);
			if (m.button) uiControlAdd(out, &n, max, UI_FIX(m.ctl), BTN_WIDE, m.enabled);
			return n;
		}
		case HitecView::List:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiListAddRows(&s_list, out, &n, max, true);
			for (uint8_t i = 1; i < n; i++)
				if (!selectable(out[i].id.row)) out[i].enabled = false;
			if (repairRow()) {
				uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Repair), BTN_REPAIR, idleRead());
			} else if (s_sel >= 0 && hitecEditable(s_layout, s_rows[s_sel].param)) {
				uiControlAdd(out, &n, max, UI_FIX(HitecCtl::EditMinus), BTN_MINUS, stepOk(-1));
				uiControlAdd(out, &n, max, UI_FIX(HitecCtl::EditPlus), BTN_PLUS, stepOk(+1));
			}
			if (readFirst()) uiControlAdd(out, &n, max, UI_FIX(HitecCtl::ReadAgain), BTN_WRITE, idleMode());
			else             uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Write), BTN_WRITE, writeAllowed());
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Revert), BTN_REVERT, pending() > 0);
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Files), BTN_FILES, true);
			return n;
		case HitecView::Write:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			if (s_writing || s_flushing) return n;
			if (s_result || s_cardNote) uiControlAdd(out, &n, max, UI_FIX(HitecCtl::WriteOk), BTN_WIDE, true);
			else uiControlAdd(out, &n, max, UI_FIX(HitecCtl::WriteHold), BTN_WIDE, holdOk());
			return n;
		case HitecView::Files:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiListAddRows(&s_fileList, out, &n, max, true);
			if (!hitecFilesMounted())
				uiControlAdd(out, &n, max, UI_FIX(HitecCtl::RetryMount), BTN_SAVE, !cardHeldByUsb());
			else
				uiControlAdd(out, &n, max, UI_FIX(HitecCtl::SaveAs), BTN_SAVE, s_preset[0] && idleRead());
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Load), BTN_LOAD, loadOk());
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Restore), BTN_RESTORE, restoreAllowed());
			return n;
		case HitecView::Leave:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::LeaveRead), BTN_LREAD, idleMode());
			uiControlAdd(out, &n, max, UI_FIX(HitecCtl::Leave), BTN_WIDE, !s_writing);
			return n;
	}
	return n;
}

void uiHitecEnter() {
	s_view = HitecView::Family;
	s_claimed = false;
	s_refusal = ClaimRefusal::None;
	uiHoldReset(&s_hold);
	s_repeat = Repeat{};
	s_drawnKey = 0;
}

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static uint32_t mixBytes(uint32_t h, const void *p, size_t n) {
	const uint8_t *b = (const uint8_t *)p;
	for (size_t i = 0; i < n; i++) h = mix(h, b[i]);
	return h;
}

/** @brief Everything the frame draws, hashed: a change repaints the view. */
static uint32_t frameKey() {
	uint32_t h = 2166136261u;
	h = mix(h, (uint32_t)s_view);
	h = mixBytes(h, &s_st, sizeof(s_st));
	h = mix(h, ((uint32_t)s_claimed << 8) | (uint32_t)s_refusal);
	h = mix(h, ((uint32_t)pinClaimState() << 8) | pinClaimJobError());
	h = mix(h, pinClaimUnplugDue());
	h = mixBytes(h, &s_draft, sizeof(s_draft));
	h = mix(h, ((uint32_t)(s_sel + 1) << 16) | (uint32_t)(s_fileSel + 1));
	h = mix(h, (uint32_t)s_list.scrollPx);
	h = mix(h, (uint32_t)s_fileList.scrollPx);
	{
		uint16_t left = 0;
		CardCache cc = cardCacheState(&left);
		h = mix(h, ((uint32_t)s_flushing << 24) | ((uint32_t)cc << 16) | left);
	}
	h = mix(h, ((uint32_t)s_writing << 0) | ((uint32_t)s_result << 1) | ((uint32_t)s_readFailed << 3) |
	           ((uint32_t)s_leaveQueued << 4) | ((uint32_t)s_recordFound << 5) |
	           ((uint32_t)hitecFilesMounted() << 6) | ((uint32_t)cardHeldByUsb() << 7) |
	           ((uint32_t)s_haveDraft << 8) | ((uint32_t)s_lastFailed << 9) |
	           ((uint32_t)servoSupplyFitted() << 10) | ((uint32_t)s_classicMeasured << 11));
	h = mix(h, s_resultSes);
	h = mixBytes(h, s_fileNote, strlen(s_fileNote));
	h = mixBytes(h, s_preset, strlen(s_preset));
	h = mixBytes(h, s_backupNext, strlen(s_backupNext));
	h = mixBytes(h, s_otherModel, strlen(s_otherModel));
	h = mix(h, ((uint32_t)s_backupCount << 1) | (uint32_t)s_saveFull);
	h = mixBytes(h, s_write.backupName, strlen(s_write.backupName));
	h = mix(h, (uint32_t)s_fileCount);
	h = mix(h, (uint32_t)(uintptr_t)s_cardNote);
	h = mix(h, (uint32_t)uiHoldPercent(&s_hold, millis()));
	static UiControl c[UI_CONTROLS_MAX];
	uint8_t n = uiHitecControls(c, UI_CONTROLS_MAX);
	for (uint8_t i = 0; i < n; i++) h = mix(h, uiPressing(c[i].rect, s_t));
	return h | 1u;
}

bool uiHitecTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	hitecJobStatus(&s_st);
	if (s_view != HitecView::Family) {
		if (s_claimed) pinClaimPoll();
		else if (s_view != HitecView::Unplug) claimPin();
		if (s_view != HitecView::Unplug) follow();
	}

	bool back = uiBackTapped(s_t);
	if (back && s_view == HitecView::Unplug && !pinClaimUnplugDue()) back = false;
	if (back) {
		switch (s_view) {
			case HitecView::Family:
				return false;
			case HitecView::Main:
				if (running() && (s_st.job == HitecJobState::InMode || s_st.job == HitecJobState::Busy)) {
					// BACK during a read, the entry's own or READ AGAIN, stops it;
					// the leave page follows.
					if (s_st.job == HitecJobState::Busy && !(s_opOpen && s_op == HitecOp::Write)) {
						sendOp(HitecOp::Abort);
						s_readFailed = true;
					}
					go(HitecView::Leave);
				} else {
					if (running() && s_st.job == HitecJobState::Entering) sendOp(HitecOp::Abort);
					if (!endClaim()) return false;
				}
				break;
			case HitecView::List:  go(HitecView::Leave); break;
			case HitecView::Files: go(HitecView::List); break;
			case HitecView::Write:
				if (s_writing) s_leaveQueued = true;
				else { s_result = false; s_cardNote = nullptr; go(HitecView::List); }
				break;
			case HitecView::Leave: go(s_haveDraft ? HitecView::List : HitecView::Main); break;
			case HitecView::Unplug:
				pinClaimKeep(PinUser::Hitec);
				cardCacheBegin();
				newSession();
				go(HitecView::Main);
				break;
		}
		s_drawnKey = 0;
	}

	switch (s_view) {
		case HitecView::Family: familyTouch(); break;
		case HitecView::Main: {
			static MainText m;
			mainText(&m);
			if (m.button && m.enabled && tapped(BTN_WIDE)) {
				if (m.ctl == HitecCtl::Retry && pinClaimJobError()) {
					pinClaimRetry(PinUser::Hitec);
				} else if (m.ctl == HitecCtl::Retry) {
					forgetServo();
					forgetFirst();
					sendOp(HitecOp::Enter);
				} else if (m.ctl == HitecCtl::Stop) {
					sendOp(HitecOp::Abort);
				} else if (m.ctl == HitecCtl::ReadAgain) {
					s_readFailed = false;
					sendOp(HitecOp::ReadAll);
				}
			}
			break;
		}
		case HitecView::List:  listTouch(); break;
		case HitecView::Write: writeTouch(); break;
		case HitecView::Files: filesTouch(); break;
		case HitecView::Leave:
			if (idleMode() && tapped(BTN_LREAD)) {
				s_readFailed = false;
				sendOp(HitecOp::ReadAll);
				go(s_haveDraft ? HitecView::List : HitecView::Main);
				break;
			}
			if (!s_writing && tapped(BTN_WIDE)) {
				// The servo leaves after a verified write: the session's record goes.
				if (s_st.job == HitecJobState::InMode && !s_lastFailed && !s_st.unknown) {
					HitecCardPhase phase;
					hitecFilesRecordRemove(&s_write);
				}
				if (!endClaim()) return false;
			}
			break;
		case HitecView::Unplug:
			if (pinClaimUnplugDue() && tapped(BTN_RESTART)) {
				pinClaimRestart(PinUser::Hitec);
				return false;
			}
			break;
	}
	uint32_t key = frameKey();
	if (key != s_drawnKey) {
		s_drawnKey = key;
		switch (s_view) {
			case HitecView::Family: drawFamily(); break;
			case HitecView::Main:   drawMain(); break;
			case HitecView::List:   drawList(); break;
			case HitecView::Write:  drawWrite(); break;
			case HitecView::Files:  drawFiles(); break;
			case HitecView::Leave:  drawLeave(); break;
			case HitecView::Unplug: drawUnplug(); break;
		}
	}
	return true;
}

/** @brief HITEC SERVO's MENU status: the signal pin it claims, from the settings. */
static void hitecMenuStatus(char *buf, size_t n, uint16_t *col, uint16_t *) {
	snprintf(buf, n, "GP%u", (unsigned)settings()->dshotPin);
	*col = C_DIM;
}

const UiScreen UI_HITEC = {
	UiScreenId::Hitec, "HITEC SERVO", PinUser::Hitec, hitecMenuStatus,
	uiHitecEnter, uiHitecTick, nullptr, uiHitecControls, nullptr, false,
};
