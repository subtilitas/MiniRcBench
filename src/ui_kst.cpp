/**
 * @file ui_kst.cpp
 * @brief The KST SERVO screen. @see ui_kst.h
 *
 * The screen draws and reads the touch; the session runs on core1
 * (kst_job.h), what a field shows and how it steps is kst_view.h, and the
 * card is kst_files.h. A view is drawn whole when a hash of what it shows
 * changes, so a still screen costs no SPI (Serial Peripheral Interface)
 * traffic.
 */

#include "ui_kst.h"
#include "kst_files.h"
#include "kst_job.h"
#include "kst_view.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "sd_log.h"
#include "settings.h"
#include "touch.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_list.h"
#include "ui_screen.h"
#include "ui_widgets.h"
#include "usb_msc.h"
#include "gfx.h"

#include "plat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------

/**
 * @defgroup kst_layout KST screen layout
 * @brief The field list, its editor bar and footer, the file list, and the
 *        one wide button of the other views.
 * @{
 */
#define KL_ROW_H     UI_LIST_TAP_MIN  /**< A list row, 40 px. */
#define KL_ROW_PITCH 42               /**< Row top to row top. */
#define KL_LIST_Y1   215              /**< Last row of the field list. */
#define KL_EDIT_Y    218              /**< The editor bar. */
#define KL_EDIT_H    44
#define KL_STEP_W    44               /**< - and +. */
#define KL_FOOT_Y    268              /**< HOLD TO WRITE, REVERT, FILES; the wide button. */
#define KL_FOOT_H    46
#define KF_LIST_Y1   229              /**< Last row of the file list. */
#define KF_SAVE_Y    234              /**< SAVE AS, or RETRY MOUNT. */
#define KF_BTN_H     40
#define KF_LOAD_Y    278              /**< LOAD and HOLD TO RESTORE. */
#define KS_SPIN_Y    236              /**< The search's spinner, its centre. */
#define KS_SPIN_R    20
/** @brief Files the FILES page lists at most; the strip names the rest. */
#define KST_UI_FILES_MAX 64
/** @} */

static const UiRect FIELD_VIEW  = { 0, UI_BODY_Y, GFX_W, KL_LIST_Y1 - UI_BODY_Y + 1 };
static const UiRect FILE_VIEW   = { 0, UI_BODY_Y, GFX_W, KF_LIST_Y1 - UI_BODY_Y + 1 };
static const UiRect BTN_MINUS   = { 6, KL_EDIT_Y, KL_STEP_W, KL_EDIT_H };
static const UiRect BTN_PLUS    = { GFX_W - 6 - KL_STEP_W, KL_EDIT_Y, KL_STEP_W, KL_EDIT_H };
static const UiRect BTN_WRITE   = { 6, KL_FOOT_Y, 110, KL_FOOT_H };
static const UiRect BTN_REVERT  = { 122, KL_FOOT_Y, 52, KL_FOOT_H };
static const UiRect BTN_FILES   = { 180, KL_FOOT_Y, 54, KL_FOOT_H };
static const UiRect BTN_WIDE    = { 6, KL_FOOT_Y, 228, KL_FOOT_H };
static const UiRect BTN_SAVE    = { 6, KF_SAVE_Y, 228, KF_BTN_H };
static const UiRect BTN_LOAD    = { 6, KF_LOAD_Y, 110, KF_BTN_H };
static const UiRect BTN_RESTORE = { 122, KF_LOAD_Y, 112, KF_BTN_H };
static const UiRect BTN_RESTART = { 40, 200, 160, 40 };

static_assert(KL_LIST_Y1 < KL_EDIT_Y, "the field list overlaps the editor bar");
static_assert(KL_EDIT_Y + KL_EDIT_H + UI_HOLD_BAR_H < KL_FOOT_Y,
              "the editor bar overlaps the footer's hold bar");
static_assert(KL_FOOT_Y + KL_FOOT_H <= GFX_H, "the footer runs off the panel");
static_assert(KF_LIST_Y1 < KF_SAVE_Y && KF_SAVE_Y + KF_BTN_H + UI_HOLD_BAR_H < KF_LOAD_Y,
              "the file page's rows overlap");
static_assert(KF_LOAD_Y + KF_BTN_H <= GFX_H, "the file page's buttons run off the panel");
static_assert(KL_ROW_H >= UI_LIST_TAP_MIN && KL_ROW_PITCH > KL_ROW_H,
              "a KST row is lower than the list floor, or has no gap");
static_assert(KL_STEP_W >= UI_TAP_MIN && KL_FOOT_H >= UI_TAP_MIN && KF_BTN_H >= UI_TAP_MIN,
              "a KST button is smaller than a fingertip");
static_assert(KS_SPIN_Y + KS_SPIN_R + 3 < KL_FOOT_Y, "the spinner runs into the button");

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static KstView      s_view = KstView::Main;
static bool         s_claimed = false;   /**< pinClaimBegin() took the claim. */
static ClaimRefusal s_refusal = ClaimRefusal::None;
static KstStatus    s_st;                /**< This frame's status from core1. */
static KstJobState  s_prevJob = KstJobState::Closed;
static const TouchState *s_t = nullptr;

/**
 * @defgroup kst_ui_session The session as the screen keeps it
 * @{
 */
static uint16_t     s_seq = 0;           /**< Seq of the operation sent last. */
static KstOp        s_op = KstOp::ReadAll;
static bool         s_opOpen = false;    /**< An operation was sent and has not ended. */
static bool         s_searchSent = false;
static bool         s_needRead = false;  /**< A servo was found: read it. */
static bool         s_everFound = false; /**< A servo was found this claim. */
static bool         s_readFailed = false;
static bool         s_haveTarget = false;
static kst_image_t  s_target;            /**< The servo with the pending changes. */
static kst_image_t  s_pageFrom;          /**< The servo when the write page opened. */
static kst_image_t  s_pageTo;            /**< What the write page writes: the target or the backup. */
static bool         s_firstDone = false; /**< The session's first-write card calls are made. */
static KstFirstWrite s_first;
static bool         s_recordFound = false; /**< KST/KST.TXT on the card at entry. */
/** @} */

/**
 * @defgroup kst_ui_views View state
 * @{
 */
static UiList       s_list;              /**< The fields. */
static int          s_sel = -1;          /**< Selected row of the field list. */
static Repeat       s_repeat;            /**< The editor bar's held repeat. */
static UiHold       s_hold;              /**< The hold of the view that is up. */
static bool         s_writing = false;   /**< The write page's operation runs. */
static bool         s_restoring = false; /**< The write page is a restore. */
static bool         s_confirm = false;   /**< An unchecked restore waits for a second hold. */
static bool         s_okPress = false;   /**< A press landed on the write page while OK showed. */
static bool         s_result = false;    /**< The write page shows a result. */
static uint8_t      s_resultSes = 0;
static uint8_t      s_resultPlan = 0;
static KstRefusal   s_resultRefused = KstRefusal::None; /**< The job refused the write. */
static KstRefusal   s_readRefused = KstRefusal::None;   /**< The job refused the read. */
static const char  *s_cardNote = nullptr; /**< The card refused the first write's files. */
static bool         s_leaveQueued = false;
static char         s_preset[KST_STORE_PRESET_MAX]; /**< The next KSTnnn, for the backup line. */
static bool         s_presetCard = false; /**< A card was mounted at the write page's last frame. */

static UiList       s_fileList;
static kst_store_entry_t s_files[KST_UI_FILES_MAX];
static int          s_fileCount = 0;     /**< Rows: the record, if any, then the files. */
static size_t       s_fileTotal = 0;
static int          s_fileSel = -1;
static char         s_fileNote[64];
static uint16_t     s_fileNoteCol = 0;
static char         s_text[KST_FILE_MAX]; /**< A file's text, saved or loaded. */
/** @} */

static uint32_t     s_drawnKey = 0;      /**< Hash of the frame drawn last; 0 repaints. */

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

static void sendOp(KstOp op, const kst_image_t *target = nullptr, bool confirm = false) {
	KstCmd c;
	memset(&c, 0, sizeof(c));
	c.op = op;
	if (target) c.target = *target;
	c.confirmUnchecked = confirm;
	s_seq = kstJobCommand(&c);
	s_op = op;
	// A new read, or an abort of one, ends what the last refusal said.
	if (op == KstOp::ReadAll || op == KstOp::Abort) s_readRefused = KstRefusal::None;
	s_opOpen = op == KstOp::ReadAll || op == KstOp::Edit || op == KstOp::Restore ||
	           op == KstOp::Verify;
}

/** @brief The operation sent last has ended: taken, and no longer running. */
static bool opEnded() {
	return s_opOpen && s_st.ackSeq == s_seq && s_st.job != KstJobState::Busy;
}

static kst_field_set_t pending() {
	return s_haveTarget && s_st.hasImage ? kstChanged(&s_st.image, &s_target) : 0;
}

static int bits(kst_field_set_t s) {
	int n = 0;
	for (; s; s &= s - 1) n++;
	return n;
}

/** @brief In the mode with no operation open; there may be no image yet. */
static bool idleMode() {
	return running() && s_st.job == KstJobState::InMode && !s_opOpen;
}

/** @brief The servo is in the mode, read with its backup, and nothing runs. */
static bool idleInMode() { return idleMode() && s_st.hasImage && s_st.hasBackup; }

/** @brief The rules the change of @p f alone adds, against the backup. */
static kst_rules_t fieldRules(kst_field_id_t f) {
	kst_image_t without = s_target;
	kst_field_edit(&without, f, kst_field_get(&s_st.image, f));
	return kstSoftRules(&s_st.backup, &s_target) & ~kstSoftRules(&s_st.backup, &without);
}

static kst_rule_t firstRule(kst_rules_t r) {
	for (int i = 0; i < KST_RULE_COUNT; i++)
		if (r & KST_RULE_BIT(i)) return (kst_rule_t)i;
	return (kst_rule_t)KST_RULE_COUNT;
}

static bool writeAllowed() {
	return idleInMode() && pending() && !s_st.locked && s_st.fingerprintOk &&
	       kstHardRules(&s_st.backup, &s_target) == 0;
}

static bool restoreAllowed() {
	return idleInMode() && s_st.hasBackup &&
	       (s_st.locked || memcmp(s_st.image.r, s_st.backup.r, KST_REG_COUNT) != 0);
}

static bool cardHeldByUsb() { return mscHoldsCard(mscGetState()); }

/** @brief Go to @p v, with its hold and its press state fresh. */
static void go(KstView v) {
	s_view = v;
	uiHoldReset(&s_hold);
	s_repeat = Repeat{};
	s_drawnKey = 0;
}

/** @brief Forget the servo: a new search follows. */
static void forgetServo() {
	s_haveTarget = false;
	s_sel = -1;
	s_readFailed = false;
	s_writing = false;
	s_result = false;
	s_confirm = false;
}

/** @brief No first write yet: no backup or record of the session's own. */
static void forgetFirst() {
	memset(&s_first, 0, sizeof(s_first));
	// KST_STORE_OK is 0: a record at OK counts as the session's to remove.
	s_first.backup = s_first.record = s_first.kept = KST_STORE_NO_CARD;
}

/** @brief A new session: the claim's first frame, or after BACK on the unplug page. */
static void newSession() {
	s_opOpen = false;
	s_searchSent = false;
	s_needRead = false;
	s_everFound = false;
	s_firstDone = false;
	forgetFirst();
	s_leaveQueued = false;
	s_cardNote = nullptr;
	s_prevJob = KstJobState::Closed;
	forgetServo();
	s_readRefused = KstRefusal::None;
	s_resultRefused = KstRefusal::None;
	int rows;
	kstRows(&rows);
	uiListInit(&s_list, FIELD_VIEW, KL_ROW_PITCH, rows);
	uiListSetBody(&s_list, KL_ROW_H, UI_LIST_TAP_MIN);
}

static void claimPin() {
	ClaimRefusal r = pinClaimBegin(PinUser::Kst, PIN_SIG);
	s_claimed = r == ClaimRefusal::None;
	s_refusal = r;
}

static void readFiles();
static bool holdOk();

/** @brief The last write's result, or why the job refused it. */
static KstResultText resultText() {
	if (s_resultRefused == KstRefusal::NoUi)
		return { "THE SCREEN STALLED. NOTHING WAS WRITTEN.", KstTone::Nothing };
	return kstResultText((kst_ses_t)s_resultSes, s_resultPlan);
}

// ---------------------------------------------------------------------------
// the session's events
// ---------------------------------------------------------------------------

/** @brief The operation sent last has ended. */
static void opEnd() {
	s_opOpen = false;
	if (s_op == KstOp::ReadAll) {
		// A refused read leaves the image of an earlier servo in the status:
		// it is no read of this one.
		s_readRefused = s_st.refused;
		if (s_st.refused == KstRefusal::None && s_st.result == KST_SES_OK && s_st.hasImage &&
		    s_st.hasBackup) {
			if (!s_haveTarget) s_target = s_st.image;
			s_haveTarget = true;
			s_readFailed = false;
			if (s_view == KstView::Main) go(KstView::List);
		} else if (s_st.job == KstJobState::InMode) {
			s_readFailed = true;
		}
		return;
	}
	// Edit and Restore: the write page shows how it ended.
	s_writing = false;
	s_result = true;
	s_resultRefused = s_st.refused;
	s_resultSes = s_st.refused == KstRefusal::None ? s_st.result : (uint8_t)KST_SES_ERR_BUSY;
	// A refused write sent no frame. Its record stays: it holds the servo
	// as it stands, and the session's next write relies on it.
	s_resultPlan = s_st.planResult;
	if (s_op == KstOp::Restore && s_resultSes == KST_SES_ERR_UNCONFIRMED) {
		s_confirm = true;
		s_result = false;
	}
	if (s_resultSes == KST_SES_OK) {
		// The write is verified: the session's record goes.
		kstFilesRecordRemove(&s_first);
		s_recordFound = kstFilesMounted() && kstFilesRecordPresent();
		s_target = s_st.image;
	}
	if (s_leaveQueued) {
		s_leaveQueued = false;
		go(KstView::Leave);
	}
}

/** @brief The job's state, read each frame, moves the views. */
static void follow() {
	if (!running()) return;
	// A card that goes away takes the session's record with it: the card
	// mounted next may hold a record of another session.
	if (s_first.record == KST_STORE_OK && !kstFilesMounted()) s_first.record = KST_STORE_NO_CARD;
	if (s_st.job == KstJobState::Opening && !s_searchSent) {
		sendOp(KstOp::Search);
		s_searchSent = true;
	}
	if (s_prevJob == KstJobState::Searching && s_st.job == KstJobState::InMode) {
		s_needRead = true;
		s_everFound = true;
		// A servo found, the first or after a loss: the session reads a new
		// backup, so its first write saves it and writes its own record. A
		// record an earlier servo left stays, and that first write keeps it.
		s_firstDone = false;
		forgetFirst();
		s_recordFound = kstFilesRecordPresent();
		// The lookup is a card call on core0: the heartbeat is stamped after
		// it, so the read this frame sends finds it fresh.
		escHeartbeat();
	}
	// A loss: the search runs again, for whatever servo is plugged in next.
	// With the heartbeat lapsed the job ends that search at once
	// (SearchEnded), and Main offers RETRY. The operation the loss ended
	// shows no result: the session is gone with the servo.
	bool searchState = s_st.job == KstJobState::Searching || s_st.job == KstJobState::SearchEnded;
	// The leave page stays: it is where BACK from the search goes.
	if (searchState && s_view != KstView::Main && s_view != KstView::Unplug &&
	    s_view != KstView::Leave) {
		forgetServo();
		s_opOpen = false;
		s_leaveQueued = false;
		go(KstView::Main);
	}
	if (opEnded()) opEnd();
	if (s_needRead && s_st.job == KstJobState::InMode && !s_opOpen) {
		s_needRead = false;
		s_readFailed = false;
		sendOp(KstOp::ReadAll);
	}
}

// ---------------------------------------------------------------------------
// the main view: the claim, the search and the read
// ---------------------------------------------------------------------------

/** @brief What the main view shows. */
struct MainText {
	char        strip[40];
	uint16_t    stripCol;
	const char *big[2];       /**< Scale 2, C_LIME. */
	const char *line[6];      /**< Scale 1. */
	uint16_t    lineCol[6];
	char        rx[48];       /**< A refused reply, C_AMBER. */
	bool        spinner;
	const char *button;       /**< The wide button's label, or null. */
	KstCtl      ctl;
	bool        enabled;
};

static void addLine(MainText *m, const char *s, uint16_t c) {
	for (int i = 0; i < 6; i++)
		if (!m->line[i]) { m->line[i] = s; m->lineCol[i] = c; return; }
}

/** @brief @p text as at most two lines of the main view, broken at spaces. */
static void addWrapped(MainText *m, const char *text, uint16_t c) {
	static char wl[2][48];
	int n = kstWrap(text, 37, wl, 2);
	for (int i = 0; i < n; i++) addLine(m, wl[i], c);
}

static void mainBody(MainText *m) {
	static char a[48], b[48];
	memset(m, 0, sizeof(*m));
	m->stripCol = C_DIM;
	uint8_t sig = sigGpio();
	if (!s_claimed) {
		switch (s_refusal) {
			case ClaimRefusal::EscAnswering:
				snprintf(a, sizeof(a), "AN ESC ANSWERS ON GP%u.", sig);
				addLine(m, a, C_AMBER);
				addLine(m, "UNPLUG IT: KST FRAMES MAY READ", C_AMBER);
				addLine(m, "AS THROTTLE.", C_AMBER);
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
		m->button = "RETRY"; m->ctl = KstCtl::Retry; m->enabled = true;
		return;
	}
	if (cs != ClaimState::Running) {
		snprintf(m->strip, sizeof(m->strip), "TAKING GP%u", sig);
		return;
	}
	switch (s_st.job) {
		case KstJobState::Searching: {
			unsigned ds = s_st.searchMs / 100;
			snprintf(m->strip, sizeof(m->strip), "GP%u  TRY %lu  %u.%u S", sig,
			         (unsigned long)s_st.attempts, ds / 10, ds % 10);
			m->big[0] = "POWER UP";
			m->big[1] = "THE SERVO NOW";
			addLine(m, "PLUG IT IN OR SWITCH ITS SUPPLY ON.", C_DIM);
			addLine(m, "IT ANSWERS ONLY RIGHT AFTER POWER-ON.", C_DIM);
			addLine(m, "A POWERED SERVO ON THIS PIN GETS NO", C_DIM);
			addLine(m, "PULSES AND MAY GO TO ITS FAILSAFE.", C_DIM);
			if (s_st.seekRx != KST_RX_NO_REPLY)
				snprintf(m->rx, sizeof(m->rx), "REPLY NOT VALID: %s", kstRxWord(s_st.seekRx));
			m->spinner = true;
			m->button = "STOP SEARCH"; m->ctl = KstCtl::StopSearch; m->enabled = true;
			return;
		}
		case KstJobState::SearchEnded:
			m->button = "RETRY"; m->ctl = KstCtl::Retry; m->enabled = true;
			switch (s_st.seek) {
				case KstSeek::TimedOut:
					snprintf(m->strip, sizeof(m->strip), "NO SERVO IN 30 S");
					addLine(m, "CHECK SIGNAL AND GROUND, THEN POWER", C_TEXT);
					addLine(m, "THE SERVO UP AGAIN.", C_TEXT);
					return;
				case KstSeek::NoUi:
					snprintf(m->strip, sizeof(m->strip), "SEARCH STOPPED");
					addLine(m, "THE SCREEN STALLED, SO THE SEARCH", C_AMBER);
					addLine(m, "STOPPED. RETRY SEARCHES AGAIN.", C_AMBER);
					return;
				case KstSeek::Stopped:
					snprintf(m->strip, sizeof(m->strip), "SEARCH STOPPED");
					addLine(m, "RETRY SEARCHES AGAIN.", C_TEXT);
					return;
				default: {
					snprintf(m->strip, sizeof(m->strip), "SEARCH FAILED");
					KstResultText r = kstResultText((kst_ses_t)s_st.result, s_st.planResult);
					addWrapped(m, r.text, C_AMBER);
					return;
				}
			}
		case KstJobState::InMode:
		case KstJobState::Busy:
			snprintf(m->strip, sizeof(m->strip), "IN PROGRAMMING MODE");
			if (s_readFailed) {
				// A refused read, or one that ended before its abort was
				// taken, leaves a result that is not the read's.
				// A refused read leaves an earlier read's registers: they are
				// listed only under the read's own result.
				if (s_readRefused == KstRefusal::NoUi) {
					addWrapped(m, "THE SCREEN STALLED. READ THE SERVO AGAIN.", C_AMBER);
				} else if (s_readRefused != KstRefusal::None || s_st.result == KST_SES_OK) {
					addWrapped(m, "THE READ WAS STOPPED. READ THE SERVO AGAIN.", C_AMBER);
				} else {
					addWrapped(m, kstResultText((kst_ses_t)s_st.result, s_st.planResult).text,
					           C_AMBER);
					static char more[48];
					kstBadRegsText(s_st.badRegs, b, more, sizeof(b));
					if (b[0]) addLine(m, b, C_AMBER);
					if (more[0]) addLine(m, more, C_AMBER);
				}
				m->button = "READ AGAIN"; m->ctl = KstCtl::ReadAgain; m->enabled = idleMode();
			} else {
				addLine(m, "READING 32 REGISTERS", C_TEXT);
			}
			return;
		default:
			snprintf(m->strip, sizeof(m->strip), "STARTING THE SEARCH ON GP%u", sig);
			return;
	}
}

static void mainText(MainText *m) {
	mainBody(m);
	if (running() && s_recordFound) {
		addLine(m, "A WRITE WAS INTERRUPTED. KST/KST.TXT", C_AMBER);
		addLine(m, "HOLDS THE SERVO AS IT WAS.", C_AMBER);
	}
}

static void drawMain() {
	static MainText m;
	mainText(&m);
	uiHeader("KST SERVO", s_t);
	uiStripClear();
	if (m.strip[0]) uiStripText(m.strip, m.stripCol);
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	int y = 70;
	for (int i = 0; i < 2; i++)
		if (m.big[i]) { gfxTextCenter(y, m.big[i], C_LIME, 2); y += 18; }
	y = m.big[0] ? 112 : 80;
	for (int i = 0; i < 6 && m.line[i]; i++) {
		gfxText(UI_MARGIN, y, m.line[i], m.lineCol[i], 1);
		y += (i % 2) ? 18 : 14;
	}
	// Below the sixth body line, which ends at y 197.
	if (m.rx[0]) gfxText(UI_MARGIN, 202, m.rx, C_AMBER, 1);
	if (m.spinner) {
		int phase = (int)((millis() / 150) % 12);
		for (int i = 0; i < 12; i++) {
			float a = i * 30 * 3.14159f / 180.0f;
			int dx = (int)(KS_SPIN_R * cosf(a)), dy = (int)(KS_SPIN_R * sinf(a));
			gfxRect(GFX_W / 2 + dx - 2, KS_SPIN_Y + dy - 2, 5, 5, i == phase ? C_LIME : C_PANEL);
		}
	}
	if (m.button)
		uiButton(BTN_WIDE, m.button, C_PANEL, C_TEXT, 2, m.enabled && pressing(BTN_WIDE), m.enabled);
}

// ---------------------------------------------------------------------------
// the field list
// ---------------------------------------------------------------------------

static const KstRow &rowAt(int i) {
	int n;
	return kstRows(&n)[i];
}

static void drawFieldRow(int i, int y, void *) {
	const KstRow &r = rowAt(i);
	bool editable = kstRowEditable(i);
	bool sel = i == s_sel;
	uint16_t now = kst_field_get(&s_st.image, r.field);
	uint16_t want = kst_field_get(&s_target, r.field);
	bool changed = now != want;
	gfxRect(0, y, GFX_W, KL_ROW_H - 1, sel ? C_PANEL : C_BG);
	gfxText(UI_MARGIN, y + 11, r.group, C_GRID, 1);
	gfxText(UI_MARGIN, y + 21, r.label, editable ? (sel ? C_TEXT : C_DIM) : C_GRID, 1);
	char v[24];
	kstValueText(r.field, want, v, sizeof(v));
	int sc = gfxTextW(v, 2) <= 104 ? 2 : 1;
	uint16_t col = !editable ? C_GRID : (changed ? C_LIME : C_TEXT);
	gfxText(GFX_W - UI_MARGIN - gfxTextW(v, sc), y + (KL_ROW_H - 1 - 7 * sc + 1) / 2, v, col, sc);
	// A change that breaks a soft rule: an amber mark at the row's edge.
	if (changed && fieldRules(r.field)) gfxRect(0, y, 3, KL_ROW_H - 1, C_AMBER);
	gfxHLine(0, y + KL_ROW_H - 1, GFX_W, C_PANEL);
}

/** @brief kstCanStep() for one direction, kept until its inputs change. */
struct StepCache {
	bool        valid;
	int         sel;
	kst_image_t target;
	kst_image_t backup;
	bool        ok;
};

static bool stepOk(int dir) {
	if (s_sel < 0 || !kstRowEditable(s_sel) || !idleInMode()) return false;
	// kstCanStep() walks the field's raw range against the limits, and the
	// controls ask it on every frame: the answer is kept per direction.
	static StepCache cache[2];
	StepCache &c = cache[dir > 0 ? 1 : 0];
	if (!c.valid || c.sel != s_sel || memcmp(c.target.r, s_target.r, KST_REG_COUNT) != 0 ||
	    memcmp(c.backup.r, s_st.backup.r, KST_REG_COUNT) != 0) {
		c.valid = true;
		c.sel = s_sel;
		c.target = s_target;
		c.backup = s_st.backup;
		c.ok = kstCanStep(&s_st.backup, &s_target, rowAt(s_sel).field, dir);
	}
	return c.ok;
}

static void drawList() {
	uiHeader("KST SERVO", s_t);
	uiStripClear();
	char strip[48];
	const char *type = kst_type_name(kst_type(&s_st.image));
	if (s_first.backupName[0])
		snprintf(strip, sizeof(strip), "%s  GP%u  BACKUP %.6s", type, sigGpio(), s_first.backupName);
	else
		snprintf(strip, sizeof(strip), "%s  GP%u  BACKUP IN MEMORY", type, sigGpio());
	uiStripText(strip, C_DIM);
	if (s_st.locked) uiChip("IN DOUBT", C_AMBER);
	else             uiChip("MODE", C_GREEN);
	uiListDraw(&s_list, drawFieldRow, nullptr);

	gfxRect(0, KL_LIST_Y1 + 1, GFX_W, KL_FOOT_Y - KL_LIST_Y1 - 1, C_BG);
	if (s_sel < 0 && !s_st.fingerprintOk) {
		gfxTextCenter(KL_EDIT_Y + 12, "READ ONLY: UNKNOWN LAYOUT", C_AMBER, 1);
		gfxTextCenter(KL_EDIT_Y + 26, "NOTHING IS WRITTEN TO THIS SERVO", C_GRID, 1);
	} else if (s_sel < 0) {
		gfxTextCenter(KL_EDIT_Y + 12, "TAP A ROW TO EDIT IT", C_GRID, 1);
		gfxTextCenter(KL_EDIT_Y + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_GRID, 1);
	} else {
		const KstRow &r = rowAt(s_sel);
		bool editable = kstRowEditable(s_sel);
		int cx = BTN_MINUS.x + BTN_MINUS.w + 2, cw = BTN_PLUS.x - 2 - cx;
		if (editable) {
			bool m = stepOk(-1), p = stepOk(+1);
			uiButton(BTN_MINUS, "-", C_GRID, C_ONACCENT, 2, m && pressing(BTN_MINUS), m);
			uiButton(BTN_PLUS, "+", C_GRID, C_ONACCENT, 2, p && pressing(BTN_PLUS), p);
		}
		char now[24], want[24], top[48];
		uint16_t rawNow = kst_field_get(&s_st.image, r.field);
		uint16_t rawWant = kst_field_get(&s_target, r.field);
		kstValuePair(r.field, rawNow, rawWant, now, want, sizeof(now));
		bool changed = rawNow != rawWant;
		kst_rules_t rules = editable ? fieldRules(r.field) : 0;
		if (rules) {
			snprintf(top, sizeof(top), "%s %s", r.label, want);
			if (gfxTextW(top, 1) > cw) snprintf(top, sizeof(top), "%s", want);
			gfxText(cx + (cw - gfxTextW(top, 1)) / 2, KL_EDIT_Y + 2, top, C_LIME, 1);
			char lines[3][48];
			int n = kstWrap(kstRuleText(firstRule(rules)), cw / 6, lines, 3);
			for (int i = 0; i < n; i++)
				gfxText(cx + (cw - gfxTextW(lines[i], 1)) / 2, KL_EDIT_Y + 14 + 10 * i, lines[i],
				        C_AMBER, 1);
		} else {
			if (changed) snprintf(top, sizeof(top), "%s  %s ->", r.label, now);
			else         snprintf(top, sizeof(top), "%s%s", r.label, editable ? "" : ": READ ONLY");
			// The value below names the field's new state; the label goes first.
			if (gfxTextW(top, 1) > cw) {
				if (changed) snprintf(top, sizeof(top), "%s ->", now);
				else         snprintf(top, sizeof(top), "%s", editable ? "" : "READ ONLY");
			}
			gfxText(cx + (cw - gfxTextW(top, 1)) / 2, KL_EDIT_Y + 2, top, C_DIM, 1);
			int sc = gfxTextW(want, 2) <= cw ? 2 : 1;
			gfxText(cx + (cw - gfxTextW(want, sc)) / 2, KL_EDIT_Y + 16, want,
			        editable ? C_LIME : C_GRID, sc);
		}
	}

	gfxRect(0, KL_FOOT_Y - 4, GFX_W, GFX_H - KL_FOOT_Y + 4, C_BG);
	bool w = writeAllowed();
	uiHoldDrawBar(&s_hold, BTN_WRITE, KL_FOOT_Y - 4, millis());
	const char *wl = !s_st.fingerprintOk ? "READ ONLY" : (s_st.locked ? "IN DOUBT" : "HOLD TO WRITE");
	uiButton(BTN_WRITE, wl, s_hold.holding ? C_RED : C_PANEL, s_hold.holding ? C_ONACCENT : C_TEXT,
	         1, s_hold.holding, w);
	bool rv = pending() != 0;
	uiButton(BTN_REVERT, "REVERT", C_PANEL, C_TEXT, 1, rv && pressing(BTN_REVERT), rv);
	uiButton(BTN_FILES, "FILES", C_PANEL, C_TEXT, 1, pressing(BTN_FILES));
}

/** @brief Step the selected row by @p steps coarse steps of @p factor raw steps. */
static void stepSel(int steps, int factor) {
	if (s_sel < 0 || !kstRowEditable(s_sel) || !idleInMode()) return;
	kstStep(&s_st.backup, &s_target, rowAt(s_sel).field, steps * factor);
}

static void listTouch() {
	if (uiHoldUpdate(&s_hold, BTN_WRITE, s_t, writeAllowed(), millis())) {
		s_result = false;
		s_confirm = false;
		s_restoring = false;
		s_cardNote = nullptr;
		s_preset[0] = '\0';
		s_presetCard = kstFilesMounted();
		if (!s_firstDone && s_presetCard)
			kst_store_preset(kstFilesIo(), s_preset, sizeof(s_preset));
		s_pageFrom = s_st.image;
		s_pageTo = s_target;
		go(KstView::Write);
		return;
	}
	if (pending() && tapped(BTN_REVERT)) s_target = s_st.image;
	if (tapped(BTN_FILES)) {
		s_fileSel = -1;
		s_fileNote[0] = '\0';
		go(KstView::Files);
		readFiles();
		return;
	}
	int dir = 0;
	if (pressing(BTN_MINUS) && stepOk(-1)) dir = -1;
	else if (pressing(BTN_PLUS) && stepOk(+1)) dir = +1;
	if (dir && repeatFires(&s_repeat, dir, millis())) stepSel(dir, 1);
	if (!dir) repeatFires(&s_repeat, 0, millis());
	UiListEvent e = uiListTouch(&s_list, s_t);
	if (e.kind == UiListEv::Tap || e.kind == UiListEv::Swipe) {
		// A LOCKED row is not selectable.
		if (kstRowEditable(e.row)) s_sel = e.row;
		if (e.steps && kstRowEditable(e.row)) stepSel(e.steps, kstCoarse(rowAt(e.row).field));
	}
}

// ---------------------------------------------------------------------------
// the write page
// ---------------------------------------------------------------------------

/** @brief The first write's card calls; false when the card took neither file. */
static bool firstWriteFiles() {
	if (s_firstDone) return true;
	kstFilesFirstWrite(&s_st.backup, &s_st.image, &s_first);
	if (!kstFilesWriteAllowed(&s_first)) {
		forgetFirst();
		return false;
	}
	// Without a card nothing is on it yet: a later write with a card
	// mounted saves the backup and writes the record then.
	s_firstDone = !(s_first.backup == KST_STORE_NO_CARD && s_first.record == KST_STORE_NO_CARD);
	s_recordFound = s_firstDone && kstFilesRecordPresent();
	return true;
}

static void startWrite() {
	s_cardNote = nullptr;
	s_result = false;
	if (!firstWriteFiles()) {
		s_cardNote = "THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.";
		return;
	}
	// The card calls block core0, up to seconds on a slow card; the job
	// takes no write while the heartbeat has lapsed.
	escHeartbeat();
	s_writing = true;
	if (s_restoring) sendOp(KstOp::Restore, nullptr, s_confirm);
	else             sendOp(KstOp::Edit, &s_target);
	s_confirm = false;
}

static void drawWrite() {
	uiHeader(s_restoring ? "KST RESTORE" : "KST WRITE", s_t);
	uiStripClear();
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	char buf[64];
	int y = UI_BODY_Y;
	if (s_restoring) {
		uiStripText("BACK TO THE SESSION'S BACKUP", C_DIM);
		// Registers, not fields: a session in doubt may differ only in a
		// LOCKED or unnamed register, which the restore writes too.
		int regs = 0;
		for (int i = 0; i < (int)KST_REG_COUNT; i++)
			if (s_pageFrom.r[i] != s_pageTo.r[i]) regs++;
		snprintf(buf, sizeof(buf), "%d REGISTER%s DIFFER%s FROM THE BACKUP", regs,
		         regs == 1 ? "" : "S", regs == 1 ? "S" : "");
		gfxText(UI_MARGIN, y, buf, C_TEXT, 1);
		y += 20;
	} else {
		// The page lists what it writes from where the servo stood when it
		// opened, so the result still names the fields.
		kst_field_set_t ch = kstChanged(&s_pageFrom, &s_pageTo);
		snprintf(buf, sizeof(buf), "%d FIELD%s", bits(ch), bits(ch) == 1 ? "" : "S");
		uiStripText(buf, C_DIM);
		int shown = 0, rows;
		const KstRow *all = kstRows(&rows);
		for (int i = 0; i < rows; i++) {
			if (!(ch & KST_FIELD_BIT(all[i].field))) continue;
			if (shown == 4) {
				snprintf(buf, sizeof(buf), "AND %d MORE", bits(ch) - 4);
				gfxText(UI_MARGIN, y, buf, C_DIM, 1);
				y += 20;
				break;
			}
			char a[24], b[24];
			kstValuePair(all[i].field, kst_field_get(&s_pageFrom, all[i].field),
			             kst_field_get(&s_pageTo, all[i].field), a, b, sizeof(a));
			gfxText(UI_MARGIN, y, all[i].label, C_TEXT, 1);
			snprintf(buf, sizeof(buf), "%s -> %s", a, b);
			// Values that would meet the label go to the line below it.
			int vy = UI_MARGIN + gfxTextW(all[i].label, 1) + 6 > GFX_W - UI_MARGIN - gfxTextW(buf, 1)
			             ? y + 10 : y;
			gfxText(GFX_W - UI_MARGIN - gfxTextW(buf, 1), vy, buf, C_LIME, 1);
			y += 20;
			shown++;
		}
		kst_rules_t soft = kstSoftRules(&s_st.backup, &s_pageTo);
		int lines = 0;
		// Rule lines end above the state lines at y 196.
		for (int i = 0; i < KST_RULE_COUNT && lines < 6 && y <= 180; i++) {
			if (!(soft & KST_RULE_BIT(i))) continue;
			char wl[3][48];
			int n = kstWrap(kstRuleText((kst_rule_t)i), 36, wl, 3);
			for (int k = 0; k < n && lines < 6 && y <= 180; k++, lines++) {
				gfxText(UI_MARGIN, y, k == 0 ? "*" : " ", C_AMBER, 1);
				gfxText(UI_MARGIN + 12, y, wl[k], C_AMBER, 1);
				y += 12;
			}
		}
	}

	if (s_first.backupName[0]) snprintf(buf, sizeof(buf), "BACKUP: KST/%s", s_first.backupName);
	else if (s_firstDone)
		snprintf(buf, sizeof(buf), "NO BACKUP ON THE CARD: IN MEMORY");
	else if (!kstFilesMounted())
		snprintf(buf, sizeof(buf), "%s", cardHeldByUsb() ? "CARD IN USE BY USB: BACKUP IN MEMORY"
		                                                : "NO CARD: BACKUP IN MEMORY");
	else if (s_preset[0]) snprintf(buf, sizeof(buf), "BACKUP FIRST: KST/%s.TXT", s_preset);
	else snprintf(buf, sizeof(buf), "BACKUP FIRST: TO THE CARD");
	gfxText(UI_MARGIN, 236, buf, C_DIM, 1);
	if (s_writing) {
		gfxText(UI_MARGIN, 196, "WRITING: DO NOT UNPLUG.", C_AMBER, 1);
		if (s_st.planning) snprintf(buf, sizeof(buf), "PLANNING THE ORDER OF THE WRITES");
		else               snprintf(buf, sizeof(buf), "FRAMES SENT: %lu", (unsigned long)s_st.frames);
		gfxText(UI_MARGIN, 210, buf, C_TEXT, 1);
		if (s_leaveQueued) gfxText(UI_MARGIN, 252, "LEAVING AFTER THE WRITE ENDS.", C_AMBER, 1);
		return;
	}
	if (s_result || s_cardNote) {
		KstResultText r = s_cardNote ? KstResultText{ s_cardNote, KstTone::Nothing } : resultText();
		uint16_t c = r.tone == KstTone::Done ? C_GREEN : (r.tone == KstTone::Doubt ? C_RED : C_AMBER);
		char wl[3][48];
		int n = kstWrap(r.text, 38, wl, 3);
		for (int k = 0; k < n; k++) gfxText(UI_MARGIN, 196 + 12 * k, wl[k], c, 1);
		uiButton(BTN_WIDE, "OK", C_PANEL, C_TEXT, 2, pressing(BTN_WIDE));
		return;
	} else if (s_confirm) {
		gfxText(UI_MARGIN, 196, "THE PLANNER DID NOT CHECK THIS", C_AMBER, 1);
		gfxText(UI_MARGIN, 208, "RESTORE. HOLD AGAIN TO RUN IT.", C_AMBER, 1);
	}

	bool ok = holdOk();
	uiHoldDrawBar(&s_hold, BTN_WIDE, KL_FOOT_Y - 4, millis());
	uiButton(BTN_WIDE, s_restoring ? "HOLD TO RESTORE" : "HOLD TO WRITE",
	         s_hold.holding ? C_RED : C_PANEL, s_hold.holding ? C_ONACCENT : C_TEXT, 2,
	         s_hold.holding, ok);
}

/** @brief The write page's hold may run: a write still wanted, or a restore still possible. */
static bool holdOk() { return s_restoring ? restoreAllowed() : writeAllowed(); }

static void writeTouch() {
	// OK takes a press that landed while it showed: the finger that ended
	// the hold lifts on the same rectangle.
	if (!(s_result || s_cardNote)) s_okPress = false;
	else if (s_t && s_t->pressed) s_okPress = true;
	if (s_writing) return;
	// A card mounted while the page is up: the backup line names the
	// KSTnnn it saves. The lookup is a card call on core0.
	bool card = kstFilesMounted();
	if (card && !s_presetCard && !s_firstDone) {
		kst_store_preset(kstFilesIo(), s_preset, sizeof(s_preset));
		escHeartbeat();
	}
	s_presetCard = card;
	if (s_result || s_cardNote) {
		if (s_okPress && tapped(BTN_WIDE)) {
			s_result = false;
			s_cardNote = nullptr;
			go(KstView::List);
		}
		return;
	}
	if (uiHoldUpdate(&s_hold, BTN_WIDE, s_t, holdOk(), millis())) startWrite();
}

// ---------------------------------------------------------------------------
// FILES
// ---------------------------------------------------------------------------

/** @brief Read the card's list into the page. */
static void readFiles() {
	s_fileCount = 0;
	s_fileTotal = 0;
	s_recordFound = kstFilesRecordPresent();
	if (kstFilesMounted()) {
		size_t count = 0, total = 0;
		kst_store_t r = kst_store_list(kstFilesIo(), s_files, KST_UI_FILES_MAX, &count, &total, nullptr);
		if (r != KST_STORE_OK && !s_fileNote[0]) {
			snprintf(s_fileNote, sizeof(s_fileNote), "THE CARD'S LIST DID NOT READ");
			s_fileNoteCol = C_AMBER;
		}
		s_fileCount = (int)count;
		s_fileTotal = total;
	}
	int rows = s_fileCount + (s_recordFound ? 1 : 0);
	uiListInit(&s_fileList, FILE_VIEW, KL_ROW_PITCH, rows);
	uiListSetBody(&s_fileList, KL_ROW_H, UI_LIST_TAP_MIN);
	if (s_fileSel >= rows) s_fileSel = -1;
	s_preset[0] = '\0';
	if (kstFilesMounted()) kst_store_preset(kstFilesIo(), s_preset, sizeof(s_preset));
}

/** @brief The file of list row @p i: null for the record. */
static const char *fileOf(int i) {
	if (s_recordFound) {
		if (i == 0) return nullptr;
		i--;
	}
	return i >= 0 && i < s_fileCount ? s_files[i].name : "";
}

static void drawFileRow(int i, int y, void *) {
	bool sel = i == s_fileSel;
	gfxRect(0, y, GFX_W, KL_ROW_H - 1, sel ? C_PANEL : C_BG);
	const char *f = fileOf(i);
	char name[KST_STORE_NAME_MAX];
	if (!f) snprintf(name, sizeof(name), "RECORD");
	else    snprintf(name, sizeof(name), "%.*s", (int)(strlen(f) > 4 ? strlen(f) - 4 : strlen(f)), f);
	gfxText(UI_MARGIN, y + 16, name, sel ? C_TEXT : C_DIM, 1);
	const char *tag = !f ? "(KST.TXT)" : (strcmp(f, s_first.backupName) == 0 ? "(BACKUP)" : "");
	gfxText(GFX_W - UI_MARGIN - gfxTextW(tag, 1), y + 16, tag, !f ? C_AMBER : C_GRID, 1);
	gfxHLine(0, y + KL_ROW_H - 1, GFX_W, C_PANEL);
}

static bool loadOk() { return s_fileSel >= 0 && kstFilesMounted() && idleInMode(); }

static void drawFiles() {
	uiHeader("KST FILES", s_t);
	uiStripClear();
	char strip[48];
	bool mounted = kstFilesMounted();
	if (s_fileNote[0])        uiStripText(s_fileNote, s_fileNoteCol);
	else if (cardHeldByUsb()) uiStripText("CARD IN USE BY USB", C_AMBER);
	else if (!mounted)        uiStripText("NO CARD", C_AMBER);
	else {
		if (s_fileTotal > (size_t)s_fileCount)
			snprintf(strip, sizeof(strip), "CARD  FIRST %d OF %u FILES", s_fileCount, (unsigned)s_fileTotal);
		else
			snprintf(strip, sizeof(strip), "CARD  %u FILE%s", (unsigned)s_fileTotal,
			         s_fileTotal == 1 ? "" : "S");
		uiStripText(strip, C_DIM);
	}
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	uiListDraw(&s_fileList, drawFileRow, nullptr);
	if (!mounted) {
		gfxTextCenter(120, "READ, EDIT AND WRITE STILL WORK.", C_DIM, 1);
		bool retry = !cardHeldByUsb();
		uiButton(BTN_SAVE, "RETRY MOUNT", C_PANEL, C_TEXT, 1, retry && pressing(BTN_SAVE), retry);
	} else {
		char save[32];
		snprintf(save, sizeof(save), "SAVE AS %s", s_preset[0] ? s_preset : "KST???");
		bool ok = s_preset[0] && idleInMode();
		uiButton(BTN_SAVE, save, C_PANEL, C_TEXT, 1, ok && pressing(BTN_SAVE), ok);
	}
	bool l = loadOk();
	uiButton(BTN_LOAD, "LOAD", C_PANEL, C_TEXT, 1, l && pressing(BTN_LOAD), l);
	bool r = restoreAllowed();
	uiHoldDrawBar(&s_hold, BTN_RESTORE, KF_LOAD_Y - 4, millis());
	uiButton(BTN_RESTORE, "HOLD TO RESTORE", s_hold.holding ? C_RED : C_PANEL,
	         s_hold.holding ? C_ONACCENT : C_TEXT, 1, s_hold.holding, r);
}

/** @brief Characters of the strip, x 8 to 232. */
#define KST_STRIP_CHARS 37

/**
 * @brief "LINE n: " and @p what into @p out, without the line number when
 *        the whole would not fit the strip.
 */
static void fitNote(char *out, size_t n, unsigned line, const char *what) {
	// 40 characters of what bound the first form; one longer than the strip
	// takes the second anyway.
	snprintf(out, n, "LINE %u: %.40s", line, what);
	if (strlen(out) > KST_STRIP_CHARS) snprintf(out, n, "%s", what);
}

/** @brief The FILES strip's note: @p text, in @p col. */
static void fileNote(uint16_t col, const char *text) {
	snprintf(s_fileNote, sizeof(s_fileNote), "%s", text);
	s_fileNoteCol = col;
}

/** @brief LOAD: the selected file, held against the servo, becomes the pending changes. */
static void loadSelected() {
	size_t len = 0;
	const char *f = fileOf(s_fileSel);
	kst_store_t r = f ? kst_store_load(kstFilesIo(), f, s_text, sizeof(s_text), &len)
	                  : kstFilesRecordLoad(s_text, sizeof(s_text), &len);
	if (r != KST_STORE_OK) {
		fileNote(C_AMBER, "THE FILE DID NOT READ");
		return;
	}
	static kst_file_t file;
	char note[64];
	unsigned line = 0;
	kst_file_err_t err = kst_file_parse(s_text, len, &file, &line);
	if (err != KST_FILE_OK) {
		char name[32], what[48];
		kstFileLineName(s_text, len, line, name, sizeof(name));
		if (err == KST_FILE_ERR_VALUE && name[0]) {
			snprintf(what, sizeof(what), "NOT A VALUE OF %s", name);
			fitNote(note, sizeof(note), line, what);
		} else if (line) {
			fitNote(note, sizeof(note), line, kstFileErrText(err));
		} else {
			snprintf(note, sizeof(note), "%s", kstFileErrText(err));
		}
		fileNote(C_AMBER, note);
		return;
	}
	static kst_file_load_t load;
	kst_file_verdict_t v = kst_file_load(&file, &s_st.image, &s_st.backup, &load);
	if (v == KST_FILE_LOAD_TYPE_MISMATCH) {
		snprintf(note, sizeof(note), "THE FILE IS FOR A %s SERVO", kst_type_name(load.file_type));
		fileNote(C_AMBER, note);
		return;
	}
	if (v == KST_FILE_LOAD_HARD_LIMIT) {
		int row = kstRowOf(load.hard_field);
		char what[48];
		if (row >= 0) snprintf(what, sizeof(what), "%s BREAKS A LIMIT", rowAt(row).label);
		else          snprintf(what, sizeof(what), "THE FILE BREAKS A LIMIT");
		if (row >= 0 && load.hard_line) fitNote(note, sizeof(note), load.hard_line, what);
		else                            snprintf(note, sizeof(note), "%s", what);
		fileNote(C_AMBER, note);
		return;
	}
	if (v != KST_FILE_LOAD_OK) {
		fileNote(C_AMBER, "THE FILE DID NOT LOAD");
		return;
	}
	s_target = load.target;
	s_haveTarget = true;
	go(KstView::List);
}

static void saveAs() {
	size_t n = kst_file_format(&s_st.image, s_text, sizeof(s_text));
	kst_store_t r = kst_store_save(kstFilesIo(), s_preset, s_text, n, nullptr);
	char note[32];
	snprintf(note, sizeof(note), "SAVED AS %s", s_preset);
	if (r == KST_STORE_OK) fileNote(C_GREEN, note);
	else if (r == KST_STORE_CARD_FULL) fileNote(C_AMBER, "THE CARD IS FULL");
	else fileNote(C_AMBER, "THE CARD TOOK NO FILE");
	// The new file sorts in among the others: the selection follows its name.
	char sel[KST_STORE_NAME_MAX] = "";
	bool selRecord = s_fileSel >= 0 && !fileOf(s_fileSel);
	if (s_fileSel >= 0 && fileOf(s_fileSel)) snprintf(sel, sizeof(sel), "%s", fileOf(s_fileSel));
	readFiles();
	s_fileSel = -1;
	int rows = s_fileCount + (s_recordFound ? 1 : 0);
	for (int i = 0; i < rows; i++) {
		const char *f = fileOf(i);
		if (selRecord ? !f : (sel[0] && f && strcmp(f, sel) == 0)) { s_fileSel = i; break; }
	}
}

static void filesTouch() {
	if (uiHoldUpdate(&s_hold, BTN_RESTORE, s_t, restoreAllowed(), millis())) {
		s_restoring = true;
		s_result = false;
		s_confirm = false;
		s_cardNote = nullptr;
		s_preset[0] = '\0';
		s_presetCard = kstFilesMounted();
		if (!s_firstDone && s_presetCard)
			kst_store_preset(kstFilesIo(), s_preset, sizeof(s_preset));
		s_pageFrom = s_st.image;
		s_pageTo = s_st.backup;
		go(KstView::Write);
		startWrite();
		return;
	}
	if (!kstFilesMounted()) {
		if (!cardHeldByUsb() && tapped(BTN_SAVE)) {
			s_fileNote[0] = '\0';
			sdLogRemount();
			readFiles();
		}
	} else if (s_preset[0] && idleInMode() && tapped(BTN_SAVE)) {
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
	uiHeader("KST SERVO", s_t);
	uiStripClear();
	char strip[32];
	// The type is the servo's that was read; after a loss there is none.
	if (s_haveTarget)
		snprintf(strip, sizeof(strip), "%s  GP%u", kst_type_name(kst_type(&s_st.image)), sigGpio());
	else
		snprintf(strip, sizeof(strip), "GP%u", sigGpio());
	uiStripText(strip, C_DIM);
	gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
	gfxText(UI_MARGIN, 76, "POWER THE SERVO OFF.", C_AMBER, 1);
	gfxText(UI_MARGIN, 92, "IT STAYS IN PROGRAMMING MODE UNTIL", C_TEXT, 1);
	gfxText(UI_MARGIN, 104, "ITS SUPPLY IS OFF.", C_TEXT, 1);
	int y = 124;
	char buf[48];
	int n = bits(pending());
	if (n) {
		snprintf(buf, sizeof(buf), "%d CHANGE%s NOT WRITTEN.", n, n == 1 ? "" : "S");
		gfxText(UI_MARGIN, y, buf, C_TEXT, 1);
		y += 14;
	}
	if (s_st.locked) {
		gfxText(UI_MARGIN, y, "THE SERVO IS IN DOUBT: RESTORE FIRST.", C_AMBER, 1);
		y += 14;
	}
	if (s_result) {
		KstResultText r = resultText();
		char wl[2][48];
		int k = kstWrap(r.text, 38, wl, 2);
		for (int i = 0; i < k; i++, y += 12) gfxText(UI_MARGIN, y, wl[i], C_DIM, 1);
	}
	if (s_leaveQueued && s_writing) gfxText(UI_MARGIN, y, "LEAVING AFTER THE WRITE ENDS.", C_AMBER, 1);
	bool ok = !s_writing;
	uiButton(BTN_WIDE, "LEAVE", C_PANEL, C_TEXT, 2, ok && pressing(BTN_WIDE), ok);
}

static void drawUnplug() {
	bool due = pinClaimUnplugDue();
	uiHeader("KST SERVO", s_t);
	gfxRect(0, UI_STRIP_Y, GFX_W, GFX_H - UI_STRIP_Y, C_BG);
	if (!due) { gfxTextCenter(120, "RELEASING THE PIN", C_DIM, 1); return; }
	gfxTextCenter(100, "UNPLUG THE SERVO.", C_AMBER, 2);
	char b[48];
	snprintf(b, sizeof(b), "DSHOT RESTARTS ON GP%u AND READS AS", sigGpio());
	gfxTextCenter(140, b, C_TEXT, 1);
	gfxTextCenter(154, "A 970 US PULSE AT 1000 HZ.", C_TEXT, 1);
	uiButton(BTN_RESTART, "RESTART DSHOT", C_PANEL, C_TEXT, 1, pressing(BTN_RESTART));
}

/** @brief End the claim; the unplug page follows. @return false when the screen left at once. */
static bool endClaim() {
	if (!s_claimed) return false;
	pinClaimEnd(PinUser::Kst);
	ClaimState cs = pinClaimState();
	if (cs == ClaimState::Free || cs == ClaimState::Restoring) return false;
	go(KstView::Unplug);
	return true;
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

bool uiKstAvailable() { return true; }

KstView uiKstView() { return s_view; }

uint8_t uiKstControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	switch (s_view) {
		case KstView::Unplug: {
			bool due = pinClaimUnplugDue();
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), due);
			if (due) uiControlAdd(out, &n, max, UI_FIX(KstCtl::Restart), BTN_RESTART, true);
			return n;
		}
		case KstView::Main: {
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			static MainText m;
			mainText(&m);
			if (m.button) uiControlAdd(out, &n, max, UI_FIX(m.ctl), BTN_WIDE, m.enabled);
			return n;
		}
		case KstView::List:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiListAddRows(&s_list, out, &n, max, true);
			for (uint8_t i = 1; i < n; i++)
				if (!kstRowEditable(out[i].id.row)) out[i].enabled = false;
			if (s_sel >= 0 && kstRowEditable(s_sel)) {
				uiControlAdd(out, &n, max, UI_FIX(KstCtl::EditMinus), BTN_MINUS, stepOk(-1));
				uiControlAdd(out, &n, max, UI_FIX(KstCtl::EditPlus), BTN_PLUS, stepOk(+1));
			}
			uiControlAdd(out, &n, max, UI_FIX(KstCtl::Write), BTN_WRITE, writeAllowed());
			uiControlAdd(out, &n, max, UI_FIX(KstCtl::Revert), BTN_REVERT, pending() != 0);
			uiControlAdd(out, &n, max, UI_FIX(KstCtl::Files), BTN_FILES, true);
			return n;
		case KstView::Write:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			if (s_writing) return n;
			if (s_result || s_cardNote) uiControlAdd(out, &n, max, UI_FIX(KstCtl::WriteOk), BTN_WIDE, true);
			else uiControlAdd(out, &n, max, UI_FIX(KstCtl::WriteHold), BTN_WIDE, holdOk());
			return n;
		case KstView::Files: {
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiListAddRows(&s_fileList, out, &n, max, true);
			if (!kstFilesMounted())
				uiControlAdd(out, &n, max, UI_FIX(KstCtl::RetryMount), BTN_SAVE, !cardHeldByUsb());
			else
				uiControlAdd(out, &n, max, UI_FIX(KstCtl::SaveAs), BTN_SAVE, s_preset[0] && idleInMode());
			uiControlAdd(out, &n, max, UI_FIX(KstCtl::Load), BTN_LOAD, loadOk());
			uiControlAdd(out, &n, max, UI_FIX(KstCtl::Restore), BTN_RESTORE, restoreAllowed());
			return n;
		}
		case KstView::Leave:
			uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
			uiControlAdd(out, &n, max, UI_FIX(KstCtl::Leave), BTN_WIDE, !s_writing);
			return n;
	}
	return n;
}

void uiKstEnter() {
	s_view = KstView::Main;
	s_claimed = false;
	s_refusal = ClaimRefusal::None;
	newSession();
	uiHoldReset(&s_hold);
	s_repeat = Repeat{};
	s_recordFound = kstFilesRecordPresent();
	s_drawnKey = 0;
	claimPin();
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
	// The status without what changes faster than it is shown: the search
	// time is shown in tenths of a second, the reply timing not at all.
	KstStatus st = s_st;
	st.searchMs /= 100;
	st.lastRx = 0;
	memset(&st.stats, 0, sizeof(st.stats));
	h = mixBytes(h, &st, sizeof(st));
	h = mix(h, ((uint32_t)s_claimed << 8) | (uint32_t)s_refusal);
	h = mix(h, ((uint32_t)pinClaimState() << 8) | pinClaimJobError());
	h = mix(h, pinClaimUnplugDue());
	h = mixBytes(h, &s_target, sizeof(s_target));
	h = mix(h, ((uint32_t)(s_sel + 1) << 16) | (uint32_t)(s_fileSel + 1));
	h = mix(h, (uint32_t)s_list.scrollPx);
	h = mix(h, (uint32_t)s_fileList.scrollPx);
	h = mix(h, ((uint32_t)s_writing << 0) | ((uint32_t)s_result << 1) | ((uint32_t)s_confirm << 2) |
	           ((uint32_t)s_readFailed << 3) | ((uint32_t)s_leaveQueued << 4) |
	           ((uint32_t)s_recordFound << 5) | ((uint32_t)kstFilesMounted() << 6) |
	           ((uint32_t)cardHeldByUsb() << 7) | ((uint32_t)s_haveTarget << 8));
	h = mix(h, ((uint32_t)s_resultSes << 8) | s_resultPlan);
	h = mixBytes(h, s_fileNote, strlen(s_fileNote));
	h = mixBytes(h, s_preset, strlen(s_preset));
	h = mixBytes(h, s_first.backupName, strlen(s_first.backupName));
	h = mix(h, ((uint32_t)s_fileCount << 16) ^ (uint32_t)s_fileTotal);
	h = mix(h, (uint32_t)(uintptr_t)s_cardNote);
	h = mix(h, (uint32_t)uiHoldPercent(&s_hold, millis()));
	if (s_view == KstView::Main && s_st.job == KstJobState::Searching) h = mix(h, millis() / 150);
	// Static: 384 bytes off core0's stack, which the first write's FatFs
	// calls share with this frame.
	static UiControl c[UI_CONTROLS_MAX];
	uint8_t n = uiKstControls(c, UI_CONTROLS_MAX);
	for (uint8_t i = 0; i < n; i++) h = mix(h, uiPressing(c[i].rect, s_t));
	return h | 1u;
}

bool uiKstTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	kstJobStatus(&s_st);
	if (s_claimed) pinClaimPoll();
	else if (s_view != KstView::Unplug) claimPin();
	if (s_view != KstView::Unplug) follow();
	s_prevJob = s_st.job;

	bool back = uiBackTapped(s_t);
	if (back && s_view == KstView::Unplug && !pinClaimUnplugDue()) back = false;
	if (back) {
		switch (s_view) {
			case KstView::Main:
				if (running() && (s_everFound || s_st.job == KstJobState::InMode ||
				                  s_st.job == KstJobState::Busy)) {
					// BACK during a read aborts it; the leave page follows.
					// The aborted read reports no end of its own, so Main offers
					// READ AGAIN after BACK on the leave page.
					if (s_opOpen && s_op == KstOp::ReadAll) {
						sendOp(KstOp::Abort);
						s_readFailed = true;
					}
					// BACK during a search after a loss stops it: no servo
					// enters the mode behind the leave page.
					if (s_st.job == KstJobState::Searching) sendOp(KstOp::StopSearch);
					go(KstView::Leave);
				} else if (!endClaim()) {
					return false;
				}
				break;
			case KstView::List:  go(KstView::Leave); break;
			case KstView::Files: go(KstView::List); break;
			case KstView::Write:
				if (s_writing) s_leaveQueued = true;
				else { s_result = false; s_cardNote = nullptr; go(KstView::List); }
				break;
			case KstView::Leave: go(s_haveTarget ? KstView::List : KstView::Main); break;
			case KstView::Unplug:
				pinClaimKeep(PinUser::Kst);
				newSession();
				go(KstView::Main);
				break;
		}
		s_drawnKey = 0;
	}

	switch (s_view) {
		case KstView::Main: {
			static MainText m;
			mainText(&m);
			if (m.button && m.enabled && tapped(BTN_WIDE)) {
				if (m.ctl == KstCtl::Retry && pinClaimJobError()) pinClaimRetry(PinUser::Kst);
				else if (m.ctl == KstCtl::Retry) sendOp(KstOp::Search);
				else if (m.ctl == KstCtl::StopSearch) sendOp(KstOp::StopSearch);
				else if (m.ctl == KstCtl::ReadAgain) { s_readFailed = false; sendOp(KstOp::ReadAll); }
			}
			break;
		}
		case KstView::List:  listTouch(); break;
		case KstView::Write: writeTouch(); break;
		case KstView::Files: filesTouch(); break;
		case KstView::Leave:
			if (!s_writing && tapped(BTN_WIDE)) {
				// The servo leaves in a known state: no write is open, so
				// the session's record goes.
				if (s_st.job == KstJobState::InMode && !s_st.locked)
					kstFilesRecordRemove(&s_first);
				if (!endClaim()) return false;
			}
			break;
		case KstView::Unplug:
			if (pinClaimUnplugDue() && tapped(BTN_RESTART)) {
				pinClaimRestart(PinUser::Kst);
				return false;
			}
			break;
	}
	uint32_t key = frameKey();
	if (key != s_drawnKey) {
		s_drawnKey = key;
		switch (s_view) {
			case KstView::Main:   drawMain(); break;
			case KstView::List:   drawList(); break;
			case KstView::Write:  drawWrite(); break;
			case KstView::Files:  drawFiles(); break;
			case KstView::Leave:  drawLeave(); break;
			case KstView::Unplug: drawUnplug(); break;
		}
	}
	return true;
}

/** @brief KST SERVO's MENU status: the signal pin it claims, from the settings. */
static void kstMenuStatus(char *buf, size_t n, uint16_t *col, uint16_t *) {
	snprintf(buf, n, "GP%u", (unsigned)settings()->dshotPin);
	*col = C_DIM;
}

const UiScreen UI_KST = {
	UiScreenId::Kst, "KST SERVO", PinUser::Kst, kstMenuStatus,
	uiKstEnter, uiKstTick, nullptr, uiKstControls, nullptr, false,
};
