/**
 * @file ui_sbus_out.cpp
 * @brief S.BUS OUTPUT. @see ui_sbus_out.h
 *
 * Three views share one claim: the main view, with 16 sliders on two pages;
 * SET, with the throttle channel, CH17, CH18 and the two flags; and the leave
 * page (ui_leave.h). Every frame is core1's (sbus_out_job.h); every text is
 * sbus_out_view.h's; this file draws, reads the touch and sends commands.
 *
 * Each region of a view is drawn from a key of the values it shows and is
 * repainted only when its key changes.
 */

#include "ui_sbus_out.h"
#include "sbus_out_view.h"
#include "sbus_out_job.h"
#include "ui_am32.h"
#include "ui_screen.h"
#include "ui_input.h"
#include "ui_hold.h"
#include "ui_leave.h"
#include "ui_list.h"
#include "ui_slider.h"
#include "ui_widgets.h"
#include "ui.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "settings.h"
#include "config.h"
#include "gfx.h"
#include "touch.h"

#include "plat.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// layout checks
// ---------------------------------------------------------------------------

/** @brief gfxTextW()'s rule at scale 1: 6 px a character less one. */
#define SO_W1(chars) (6 * (chars) - 1)
/** @brief A string literal's width at scale 1. */
#define SO_TEXT_W(s) SO_W1((int)sizeof(s) - 1)
/** @brief A string literal fits from x 8 to the right margin at scale 1. */
#define SO_FITS(s) (UI_MARGIN + SO_TEXT_W(s) <= GFX_W - UI_MARGIN)
/** @brief Where the widest chip, RUNNING or STALLED, starts: x 179. */
#define SO_CHIP_X (GFX_W - UI_MARGIN - (SO_TEXT_W(SBUS_OUT_VIEW_RUNNING) + 12))

/** @brief HOLD TO START, or STOP while RUNNING. */
static constexpr UiRect BTN_START = { UI_EDGE, SBOUT_UI_BOT_Y, 110, SBOUT_UI_BOT_H };
/** @brief CH 9-16 or CH 1-8. */
static constexpr UiRect BTN_PAGE  = { 120, SBOUT_UI_BOT_Y, 55, SBOUT_UI_BOT_H };
/** @brief SET. */
static constexpr UiRect BTN_SET   = { 179, SBOUT_UI_BOT_Y, 55, SBOUT_UI_BOT_H };
/** @brief RETRY, the wide button, under a failed bind. */
static constexpr UiRect BTN_RETRY = { UI_EDGE, UI_FOOT_Y, GFX_W - 2 * UI_EDGE, UI_FOOT_H };

static_assert(UI_MARGIN + SO_W1((int)sizeof(SBOUT_UI_TITLE) - 1) * 2 + 4 <= UI_BACK_X,
              "S.BUS OUTPUT's title runs into BACK");
static_assert(UI_MARGIN + SO_W1((int)sizeof(SBOUT_UI_SET_TITLE) - 1) * 2 + 4 <= UI_BACK_X,
              "SET's title runs into BACK");
// The caption with a three-letter role in place of %s, 4 px or more before
// the widest chip.
static_assert(UI_MARGIN + SO_W1((int)sizeof(SBOUT_UI_CAPTION_FMT) - 1 - 2 + 3) + 4 <= SO_CHIP_X,
              "S.BUS OUTPUT's caption runs under the widest chip");
static_assert(SO_TEXT_W(SBUS_OUT_VIEW_STALLED) <= SO_TEXT_W(SBUS_OUT_VIEW_RUNNING) &&
              SO_TEXT_W(SBUS_OUT_VIEW_SAFE) <= SO_TEXT_W(SBUS_OUT_VIEW_RUNNING),
              "a chip is wider than RUNNING");
static_assert(SBOUT_UI_NOTE1_Y == UI_BODY_Y && SBOUT_UI_NOTE1_Y + 7 < SBOUT_UI_NOTE2_Y &&
              SBOUT_UI_NOTE2_Y + 7 + 3 <= SBOUT_UI_ROW_Y,
              "the notes overlap, or run into the first row");
// The widest note: 37 characters, the ceiling line at MAX THROTTLE 100 %.
static_assert(UI_MARGIN + SO_W1(37) <= GFX_W - UI_MARGIN, "a note runs past the margin");
static_assert(SO_FITS(SBUS_OUT_VIEW_WARN_1) && SO_FITS(SBUS_OUT_VIEW_WARN_2) &&
              SO_FITS(SBUS_OUT_VIEW_STALL) && SO_FITS(SBUS_OUT_VIEW_IDLE) &&
              SO_FITS(SBUS_OUT_VIEW_REFUSED),
              "a note runs past the margin");
static_assert(SBOUT_UI_ROW_PITCH > SBOUT_UI_ROW_H && SBOUT_UI_ROW_H >= UI_TAP_MIN,
              "the rows touch, or are lower than a fingertip");
// The knob stands UI_SLIDER_OVERHANG px proud of the drawn track, inside the row.
static_assert(SBOUT_UI_TRACK_DY - UI_SLIDER_OVERHANG >= 0 &&
              SBOUT_UI_TRACK_DY + SBOUT_UI_TRACK_H + UI_SLIDER_OVERHANG <= SBOUT_UI_ROW_H,
              "the knob leaves its row");
// The widest label, `CH16` or `CH3 THR`, 7 characters, 4 px or more before the track.
static_assert(UI_MARGIN + SO_W1(7) + 4 <= SBOUT_UI_TRACK_X, "a label runs into its track");
// The widest value, `2000`, 4 characters, right-aligned.
static_assert(SBOUT_UI_TRACK_X + SBOUT_UI_TRACK_W + 4 <= SBOUT_UI_VALUE_END - SO_W1(4),
              "a track runs into its value");
static_assert(SBOUT_UI_VALUE_END == GFX_W - UI_MARGIN, "the values do not end at the right margin");
static_assert(SBOUT_UI_ROW_Y + (SBOUT_UI_ROWS - 1) * SBOUT_UI_ROW_PITCH + SBOUT_UI_ROW_H + 4 <=
              SBOUT_UI_BAR_Y, "the last row runs into the hold bar");
static_assert(SBOUT_UI_BAR_Y + UI_HOLD_BAR_H <= SBOUT_UI_BOT_Y && SBOUT_UI_BOT_Y + SBOUT_UI_BOT_H <= GFX_H,
              "the hold bar overlaps the bottom row, or the row runs off the panel");
static_assert(BTN_START.x + BTN_START.w < BTN_PAGE.x && BTN_PAGE.x + BTN_PAGE.w < BTN_SET.x &&
              BTN_SET.x + BTN_SET.w == GFX_W - UI_EDGE, "the bottom row's buttons overlap");
static_assert(SO_TEXT_W("HOLD TO START") + 8 <= BTN_START.w && 2 * SO_TEXT_W("STOP") + 8 <= BTN_START.w &&
              SO_TEXT_W(SBOUT_UI_PAGE_2) + 8 <= BTN_PAGE.w && SO_TEXT_W(SBOUT_UI_PAGE_1) + 8 <= BTN_PAGE.w &&
              SO_TEXT_W("SET") + 8 <= BTN_SET.w, "a bottom-row label does not fit its button");
static_assert(SBOUT_UI_BOT_H >= UI_TAP_MIN, "a bottom-row button is lower than a fingertip");
static_assert(SBOUT_UI_MSG_Y + 2 * SBOUT_UI_MSG_H + 7 < SBOUT_UI_BAR_Y, "a message runs into the hold bar");
// The ESC line with a one-digit GPIO in place of %u.
static_assert(UI_MARGIN + SO_W1((int)sizeof(SBOUT_UI_ESC_FMT) - 1 - 2 + 1) <= GFX_W - UI_MARGIN &&
              SO_FITS(SBOUT_UI_ESC_2) && SO_FITS(SBOUT_UI_ESC_3) && SO_FITS(SBOUT_UI_KISS_1) &&
              SO_FITS(SBOUT_UI_KISS_2) && SO_FITS(SBOUT_UI_KISS_3) && SO_FITS(SBOUT_UI_BL_2) && SO_FITS(SBOUT_UI_BL_3) &&
              SO_FITS(SBOUT_UI_NO_GP0_1) && SO_FITS(SBOUT_UI_NO_GP0_2) && SO_FITS(SBOUT_UI_SET_STOP) &&
              SO_FITS(SBOUT_UI_SET_START),
              "a message line runs past the margin");
// The probe's two lines with a three-letter role in place of %s.
static_assert(UI_MARGIN + SO_W1((int)sizeof(SBOUT_UI_CHECKING_FMT) - 1 - 2 + 3) <= GFX_W - UI_MARGIN &&
              UI_MARGIN + SO_W1((int)sizeof(SBOUT_UI_BL_1_FMT) - 1 - 2 + 3) <= GFX_W - UI_MARGIN,
              "a probe line runs past the margin");
// `SIG GP0 COULD NOT BE TAKEN (CLOCK)`, 34 characters.
static_assert(UI_MARGIN + SO_W1(34) <= GFX_W - UI_MARGIN, "the bind failure runs past the margin");
static_assert(sizeof(SBOUT_UI_UNPLUG) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(SBOUT_UI_RESTART_FMT) - 1 - 2 + 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(SBOUT_UI_PLUG_ESC) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(AM32_UI_LEAVE_1) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(AM32_UI_LEAVE_1A) - 1 <= UI_LEAVE_STEP_CHARS &&
              sizeof(AM32_UI_LEAVE_1B) - 1 <= UI_LEAVE_STEP_CHARS,
              "a leave step runs past x 232");
static_assert(SO_TEXT_W(SBOUT_UI_BACK_TO) + 8 <= UI_LEAVE_LEFT.w, "BACK TO OUTPUT does not fit its button");

/**
 * @defgroup sbus_out_set_layout SET's layout: SERVO SET's
 * @{
 */
#define SET_ROW_H     UI_LIST_TAP_MIN  /**< A SET row, 40 px. */
#define SET_ROW_PITCH 42               /**< Row top to row top. */
#define SET_LIST_Y0   UI_BODY_Y
#define SET_LIST_Y1   215
#define SET_EDIT_Y    218
#define SET_EDIT_H    44
/** @} */

static constexpr UiRect SET_VIEW  = { 0, SET_LIST_Y0, GFX_W, SET_LIST_Y1 - SET_LIST_Y0 + 1 };
static constexpr UiRect BTN_MINUS = { UI_EDGE, SET_EDIT_Y, UI_EDIT_BTN_W, SET_EDIT_H };
static constexpr UiRect BTN_PLUS  = { GFX_W - UI_EDIT_BTN_W - UI_EDGE, SET_EDIT_Y, UI_EDIT_BTN_W, SET_EDIT_H };
/** @brief SET's right footer button, SERVO SET's: STOP while RUNNING. */
static constexpr UiRect BTN_STOP  = { 162, UI_FOOT_Y, 72, UI_FOOT_H };

static_assert(SET_LIST_Y1 < SET_EDIT_Y && SET_EDIT_Y + SET_EDIT_H <= UI_FOOT_Y,
              "SET's list, editor bar and footer overlap");
static_assert(2 * SO_TEXT_W("STOP") + 8 <= BTN_STOP.w, "STOP does not fit SET's footer button");
// The footer warning ends at x 168, 6 px past the right button's left edge:
// the button is not drawn while it shows.
static_assert(UI_MARGIN + SO_TEXT_W(SBUS_OUT_VIEW_WARN_2) <= GFX_W - UI_MARGIN &&
              SBOUT_UI_FOOT_TEXT_Y + 7 <= GFX_H, "SET's footer warning does not fit");

/** @brief One row of SET. */
struct SetRow {
	const char *group;  /**< THROTTLE, DIGITAL or FLAGS. */
	const char *name;   /**< The row's label. */
};

static constexpr SetRow ROWS[] = {
	{ "THROTTLE", "CHANNEL" },
	{ "DIGITAL",  "CH17" },
	{ "DIGITAL",  "CH18" },
	{ "FLAGS",    "FAILSAFE BIT" },
	{ "FLAGS",    "FRAME LOST BIT" },
};
#define SET_ROWS ((int)(sizeof(ROWS) / sizeof(ROWS[0])))
/** @brief The CHANNEL row; the others are the frame's four flags. */
#define ROW_CHANNEL 0

/** @brief Every SET label within 14 characters, 83 px. */
static constexpr bool setRowsFit() {
	for (const SetRow &r : ROWS) {
		int n = 0;
		while (r.name[n]) n++;
		if (n > 14) return false;
	}
	return true;
}
static_assert(setRowsFit(), "a SET label is longer than 14 characters");
// `CH1`, `OFF`: 3 characters at scale 2, 34 px.
static_assert(2 * SO_W1(3) <= UI_FIELD_VALUE_W2, "a SET value is wider than a scale-2 value");

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

/** @brief Which view owns the panel. */
enum class View : uint8_t { Main, Set, Leave };

static View         s_view = View::Main;
static bool         s_claimed = false;     /**< pinClaimBegin() took this screen's claim. */
static ClaimRefusal s_refusal = ClaimRefusal::None;  /**< Why the last claim was refused. */
static uint8_t      s_roles = 0;           /**< sbusOutClaimRoles() of the wiring; 0: no role on GP0. */
static SbusOutStatus s_st;                 /**< This frame's status from core1. */
static SbusOutProbeState s_probe = SbusOutProbeState::Off;  /**< This frame's probe. */
static UiLeave      s_leave;               /**< The leave page. */
static bool         s_efm8Page = false;    /**< The leave page has decision 8's first step. */
static const TouchState *s_t = nullptr;    /**< This frame's touch. */
static bool         s_full = true;         /**< Repaint every region this frame. */
static uint8_t      s_body = 0;            /**< What the body showed last: 1 rows, 2 a message, 3 RETRY. */

static UiSlider     s_slider[SBUS_CHANNELS];  /**< One slider a channel, us. */
static uint16_t     s_us[SBUS_CHANNELS];      /**< Each channel as commanded, us, in 5 us steps. */
static bool         s_flag[4];                /**< CH17, CH18, failsafe, frame lost, as commanded. */
static sbus_frame_t s_sentFrame;              /**< The frame last sent as a Frame, or START's. */
static uint8_t      s_page = 0;               /**< 0: CH1 to CH8; 1: CH9 to CH16. */
static uint8_t      s_thrCh = SBUS_OUT_THR_DEFAULT;  /**< The throttle channel's index. */
static uint16_t     s_ceilRaw = SBUS_OUT_RAW_LO;     /**< The throttle ceiling, raw. */
static SbusOutStop  s_stop = SbusOutStop::None;      /**< The screen's own stop since START. */
static uint32_t     s_touchMs = 0;            /**< millis() of the last frame with a finger down. */
static UiHold       s_hold;                   /**< HOLD TO START. */

static int    s_sel = -1;    /**< SET's selected row, or -1. */
static UiList s_list;        /**< SET's rows. */
static Repeat s_repeat;      /**< The editor bar's held repeat. */

/** @brief Keys of the regions drawn last. */
static uint32_t s_keys[3 + SBOUT_UI_ROWS + 1];

/** @brief Region indices into s_keys; SET and the leave page use the first ones. */
enum : int { K_TOP = 0, K_NOTES = 1, K_MSG = 2, K_ROW0 = 3, K_BOT = 3 + SBOUT_UI_ROWS };

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/** @brief FNV-1a of a key's bytes. */
static uint32_t hashOf(const void *p, size_t n) {
	const uint8_t *b = (const uint8_t *)p;
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
	return h;
}

/** @brief A hash of a line of text for a key; 0 for none. */
static uint32_t textKey(const char *s) { return s ? hashOf(s, strlen(s)) | 1u : 0; }

/** @brief True when region @p i must repaint for key @p key. */
static bool changed(int i, const void *key, size_t n) {
	uint32_t h = hashOf(key, n);
	if (!s_full && s_keys[i] == h) return false;
	s_keys[i] = h;
	return true;
}

static bool pressing(const UiRect &r) { return uiPressing(r, s_t); }
static bool tapped(const UiRect &r) { return uiTapped(r, s_t); }

/** @brief The GPIO a role names, from the claim, or from the pump before it holds. */
static uint8_t gpioOf(uint8_t role) {
	uint8_t g = pinClaimGpio(role);
	if (g != PIN_CLAIM_NO_GPIO) return g;
	return role == PIN_SIG ? escTaskDshotPin() : escTaskKissPin();
}

/** @brief GP0's role: `SIG` in the default wiring, `AUX` when SIG is GP1. */
static const char *gp0Role() { return gpioOf(PIN_SIG) == SBUS_OUT_GPIO ? "SIG" : "AUX"; }

/**
 * @brief Whether BACK acts on this frame: everywhere but on the leave page
 *        (docs/design/menu-b.md, decision 5).
 */
static bool backActs() { return s_view != View::Leave; }

/** @brief The top of the page's row @p i. */
static int rowY(int i) { return SBOUT_UI_ROW_Y + i * SBOUT_UI_ROW_PITCH; }

/** @brief The page's row @p i as a press target: the track's x span, the row's height. */
static UiRect rowRect(int i) {
	return UiRect{ SBOUT_UI_TRACK_X, (int16_t)rowY(i), SBOUT_UI_TRACK_W, SBOUT_UI_ROW_H };
}

/** @brief The highest us channel @p ch may take: the throttle ceiling's on the throttle channel. */
static uint16_t topUs(unsigned ch) {
	return ch == s_thrCh ? sbus_to_us(s_ceilRaw) : (uint16_t)SBUS_OUT_VIEW_US_HI;
}

/** @brief The frame the sliders and SET's flags command, every channel held to its top. */
static sbus_frame_t wantFrame() {
	sbus_frame_t f;
	memset(&f, 0, sizeof(f));
	for (unsigned i = 0; i < SBUS_CHANNELS; i++)
		f.channel[i] = sbusOutRawFor(s_us[i], i == s_thrCh ? s_ceilRaw : (uint16_t)SBUS_OUT_RAW_HI);
	f.ch17 = s_flag[0];
	f.ch18 = s_flag[1];
	f.failsafe = s_flag[2];
	f.frame_lost = s_flag[3];
	return f;
}

/** @brief The sliders, SET's flags and the last frame sent at the START frame. */
static void resetFrame() {
	sbus_frame_t f;
	sbusOutStartFrame(&f, s_thrCh);
	for (unsigned i = 0; i < SBUS_CHANNELS; i++) {
		s_us[i] = sbus_to_us(f.channel[i]);
		uiSliderRelease(&s_slider[i]);
		uiSliderSet(&s_slider[i], s_us[i]);
	}
	memset(s_flag, 0, sizeof(s_flag));
	s_sentFrame = f;
}

/** @brief Send a command: the frame of wantFrame(), the throttle channel and the ceiling. */
static void send(SbusOutCmdKind kind) {
	SbusOutCmd c;
	memset(&c, 0, sizeof(c));
	c.kind = kind;
	sbus_frame_t f = wantFrame();
	memcpy(c.channel, f.channel, sizeof(c.channel));
	c.ch17 = f.ch17;
	c.ch18 = f.ch18;
	c.failsafe = f.failsafe;
	c.frameLost = f.frame_lost;
	c.thrCh = s_thrCh;
	c.ceilRaw = s_ceilRaw;
	sbusOutJobCommand(&c);
}

/**
 * @brief HOLD TO START is live: the frames are off, the probe's gate holds,
 *        a cycle with no answer passed in this bind, and the touch
 *        controller answers (uiTouchLost()). This is the only gate to START.
 *        The probe's gate holds only while this claim's job is bound in its
 *        UART phase, so the claim runs with no bind error whenever it does.
 */
static bool startAllowed() {
	return !s_st.running && !uiTouchLost() && sbusOutProbePassed();
}

/**
 * @brief START: no stop of the screen's own, the sliders and the flags at
 *        the START frame, then the command with the throttle channel and
 *        MAX THROTTLE's ceiling. Called only from the hold, which
 *        startAllowed() gates; the hold's finger is the idle rule's touch.
 */
static void startFrames() {
	s_stop = SbusOutStop::None;
	resetFrame();
	send(SbusOutCmdKind::Start);
}

/** @brief Ask for GP0's role. No role on GP0 asks for nothing. */
static void claimPin() {
	s_roles = sbusOutClaimRoles(escTaskDshotPin(), escTaskKissPin());
	s_claimed = false;
	if (!s_roles) {
		s_refusal = ClaimRefusal::None;
		return;
	}
	s_refusal = pinClaimBegin(PinUser::SbusOut, s_roles);
	s_claimed = s_refusal == ClaimRefusal::None;
}

/**
 * @brief What the rows' area shows in place of the notes and rows, or null
 *        for them: the claim's refusal, its progress, a failed bind, or the
 *        bootloader probe.
 */
static void message(const char **l1, const char **l2, const char **l3, uint16_t *col, bool *retry) {
	static char a[40];
	*l1 = *l2 = *l3 = nullptr;
	*col = UI_COL_WARN;
	*retry = false;
	if (!s_roles) {
		*l1 = SBOUT_UI_NO_GP0_1; *l2 = SBOUT_UI_NO_GP0_2;
		return;
	}
	if (!s_claimed) {
		switch (s_refusal) {
			case ClaimRefusal::EscAnswering:
				snprintf(a, sizeof(a), SBOUT_UI_ESC_FMT, (unsigned)gpioOf(PIN_SIG));
				*l1 = a; *l2 = SBOUT_UI_ESC_2; *l3 = SBOUT_UI_ESC_3;
				return;
			case ClaimRefusal::KissLive:
				*l1 = SBOUT_UI_KISS_1; *l2 = SBOUT_UI_KISS_2; *l3 = SBOUT_UI_KISS_3;
				return;
			case ClaimRefusal::Armed: *l1 = "DISARM FIRST"; return;
			default:                  *l1 = UI_WAIT_RESTART_TEXT; return;
		}
	}
	ClaimState cs = pinClaimState();
	if (cs == ClaimState::Stopping) {
		snprintf(a, sizeof(a), "RELEASING %s GP0", gp0Role());
		*l1 = a; *col = C_DIM;
		return;
	}
	uint8_t err = pinClaimJobError();
	if (err == CORE1_JOB_NO_GP0) {
		*l1 = SBOUT_UI_NO_GP0_1; *l2 = SBOUT_UI_NO_GP0_2;
		return;
	}
	if (err) {
		static const char *const WHY[] = {"", "REFUSED", "PWM", "PIO", "DMA", "CLOCK"};
		snprintf(a, sizeof(a), "%s GP0 COULD NOT BE TAKEN (%s)", gp0Role(), WHY[err < 6 ? err : 1]);
		*l1 = a;
		*retry = true;
		return;
	}
	// The job not bound yet: the probe's state is Off until the bind.
	if (s_probe == SbusOutProbeState::Off) {
		snprintf(a, sizeof(a), "TAKING %s GP0", gp0Role());
		*l1 = a; *col = C_DIM;
		return;
	}
	if (s_probe == SbusOutProbeState::Checking) {
		snprintf(a, sizeof(a), SBOUT_UI_CHECKING_FMT, gp0Role());
		*l1 = a; *col = C_DIM;
		return;
	}
	if (s_probe == SbusOutProbeState::Answered) {
		snprintf(a, sizeof(a), SBOUT_UI_BL_1_FMT, gp0Role());
		*l1 = a; *l2 = SBOUT_UI_BL_2; *l3 = SBOUT_UI_BL_3;
	}
}

/** @brief The rows show: no message in their place. */
static bool rowsShown() {
	const char *l1, *l2, *l3;
	uint16_t col;
	bool retry;
	message(&l1, &l2, &l3, &col, &retry);
	return !l1;
}

/** @brief RETRY shows in place of the bottom row. */
static bool retryShown() {
	const char *l1, *l2, *l3;
	uint16_t col;
	bool retry;
	message(&l1, &l2, &l3, &col, &retry);
	return retry;
}

// ---------------------------------------------------------------------------
// the main view
// ---------------------------------------------------------------------------

/** @brief The header, the caption and, while the rows show, the chip. */
static void drawTop(const char *title, bool rows) {
	struct { bool back, backOk, rows; uint8_t chip, roles, sig; } k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.backOk = backActs();
	k.rows = rows;
	k.chip = (uint8_t)sbusOutChip(&s_st);
	k.roles = s_roles;
	k.sig = gpioOf(PIN_SIG);
	if (!changed(K_TOP, &k, sizeof(k))) return;
	uiHeader(title, s_t, k.backOk);
	uiStripClear();
	char cap[40];
	if (s_roles) snprintf(cap, sizeof(cap), SBOUT_UI_CAPTION_FMT, gp0Role());
	else snprintf(cap, sizeof(cap), "%s", SBOUT_UI_CAPTION_TAIL);
	uiStripText(cap, C_DIM);
	if (rows) {
		SbusOutChip c = sbusOutChip(&s_st);
		uiChip(sbusOutChipText(c), sbusOutChipFill(c));
	}
}

/** @brief The two notes under the strip. */
static void drawNotes() {
	char n1[48], n2[48];
	uint16_t c1, c2;
	sbusOutNotes(&s_st, s_stop, s_thrCh, s_ceilRaw, settings()->maxThrottle, n1, n2, sizeof(n1), &c1, &c2);
	struct { uint32_t n1, n2; uint16_t c1, c2; } k;
	memset(&k, 0, sizeof(k));
	k.n1 = textKey(n1); k.n2 = textKey(n2); k.c1 = c1; k.c2 = c2;
	if (!changed(K_NOTES, &k, sizeof(k))) return;
	gfxRect(0, SBOUT_UI_NOTE1_Y, GFX_W, SBOUT_UI_ROW_Y - SBOUT_UI_NOTE1_Y, C_BG);
	gfxText(UI_MARGIN, SBOUT_UI_NOTE1_Y, n1, c1, 1);
	gfxText(UI_MARGIN, SBOUT_UI_NOTE2_Y, n2, c2, 1);
}

/** @brief The x where the knob of a track at @p us starts: ui_slider.cpp's rule, 14 px wide. */
static int knobX(uint16_t us) {
	const int w = SBOUT_UI_TRACK_W, half = 7;
	float frac = (float)(us - SBUS_OUT_VIEW_US_LO) / (float)(SBUS_OUT_VIEW_US_HI - SBUS_OUT_VIEW_US_LO);
	int hx = SBOUT_UI_TRACK_X + (int)(frac * (float)w + 0.5f);
	if (hx < SBOUT_UI_TRACK_X + half) hx = SBOUT_UI_TRACK_X + half;
	if (hx > SBOUT_UI_TRACK_X + w - half) hx = SBOUT_UI_TRACK_X + w - half;
	return hx - half;
}

/** @brief The page's row @p i: label, slider, the ceiling's mark on the throttle row, value. */
static void drawRow(int i) {
	const unsigned ch = s_page * SBOUT_UI_ROWS + (unsigned)i;
	char lab[12], val[8];
	bool thr = sbusOutLabel(lab, sizeof(lab), ch, s_thrCh);
	sbusOutValueText(val, sizeof(val), &s_st, ch, s_thrCh);
	struct { uint8_t ch; bool thr, run; uint16_t us, ceilUs; uint32_t lab, val; } k;
	memset(&k, 0, sizeof(k));
	k.ch = (uint8_t)ch; k.thr = thr; k.run = s_st.running; k.us = s_us[ch];
	k.ceilUs = thr ? topUs(ch) : 0;
	k.lab = textKey(lab); k.val = textKey(val);
	if (!changed(K_ROW0 + i, &k, sizeof(k))) return;
	const int y = rowY(i);
	gfxRect(0, y, GFX_W, SBOUT_UI_ROW_H, C_BG);
	gfxText(UI_MARGIN, y + SBOUT_UI_TEXT_DY, lab, thr ? UI_COL_WARN : C_DIM, 1);
	// The slider drawn on its 10 px track; it takes presses on the row's 22 px.
	UiSlider d = s_slider[ch];
	d.track = UiRect{ SBOUT_UI_TRACK_X, (int16_t)(y + SBOUT_UI_TRACK_DY), SBOUT_UI_TRACK_W, SBOUT_UI_TRACK_H };
	d.value = s_us[ch];
	d.presetCount = 0;
	d.dim = !s_st.running;
	uiSliderDraw(&d, C_GRID);
	if (thr) {
		// The ceiling: the track past it dim, a 2 px line in the fill's
		// colour along its middle, a fifth of the fill's 10 px, that reads
		// as no fill, the knob never past it, and a 1 px mark the row's
		// height, broken where the knob covers it.
		const int mx = sbusOutUsX(k.ceilUs, SBOUT_UI_TRACK_X, SBOUT_UI_TRACK_W);
		const int kx = knobX(s_us[ch]);
		int x0 = mx + 1 > kx + 14 ? mx + 1 : kx + 14;
		int x1 = SBOUT_UI_TRACK_X + SBOUT_UI_TRACK_W - 3;
		if (x1 > x0) gfxRect(x0, y + SBOUT_UI_TRACK_DY + SBOUT_UI_TRACK_H / 2 - 1, x1 - x0, 2, C_GRID);
		bool covered = mx >= kx && mx < kx + 14;
		gfxVLine(mx, y, UI_SLIDER_OVERHANG, UI_COL_WARN);
		gfxVLine(mx, y + SBOUT_UI_ROW_H - UI_SLIDER_OVERHANG, UI_SLIDER_OVERHANG, UI_COL_WARN);
		if (!covered)
			gfxVLine(mx, y + UI_SLIDER_OVERHANG, SBOUT_UI_ROW_H - 2 * UI_SLIDER_OVERHANG, UI_COL_WARN);
	}
	gfxText(SBOUT_UI_VALUE_END - gfxTextW(val, 1), y + SBOUT_UI_TEXT_DY, val,
	        s_st.running ? UI_COL_LIVE : C_DIM, 1);
}

/** @brief HOLD TO START or STOP, the page, SET; nothing under RETRY. */
static void drawBottom(bool retry) {
	bool startOk = startAllowed();
	struct { bool retry, running, startOk, holding, p0, p1, p2; int pct; uint8_t page; } k;
	memset(&k, 0, sizeof(k));
	k.retry = retry; k.running = s_st.running; k.startOk = startOk; k.holding = s_hold.holding;
	k.pct = uiHoldPercent(&s_hold, millis());
	k.p0 = pressing(BTN_START); k.p1 = pressing(BTN_PAGE); k.p2 = pressing(BTN_SET);
	k.page = s_page;
	if (!changed(K_BOT, &k, sizeof(k))) return;
	if (retry) return;
	gfxRect(0, SBOUT_UI_BAR_Y, GFX_W, GFX_H - SBOUT_UI_BAR_Y, C_BG);
	if (s_st.running) {
		uiButton(BTN_START, "STOP", UI_COL_ON, C_ONACCENT, 2, k.p0);
	} else {
		uiHoldDrawBar(&s_hold, BTN_START, SBOUT_UI_BAR_Y, millis());
		uiButton(BTN_START, "HOLD TO START", s_hold.holding ? UI_COL_ON : C_PANEL,
		         s_hold.holding ? C_ONACCENT : C_TEXT, 1, s_hold.holding, startOk);
	}
	uiButton(BTN_PAGE, s_page ? SBOUT_UI_PAGE_1 : SBOUT_UI_PAGE_2, C_PANEL, C_TEXT, 1, k.p1);
	uiButton(BTN_SET, "SET", C_PANEL, C_TEXT, 1, k.p2);
}

/** @brief The rows' area in place of the notes and rows: the message and RETRY. */
static void drawMessage(const char *l1, const char *l2, const char *l3, uint16_t col, bool retry) {
	struct { uint32_t l1, l2, l3; uint16_t col; bool retry, rp; } k;
	memset(&k, 0, sizeof(k));
	k.l1 = textKey(l1); k.l2 = textKey(l2); k.l3 = textKey(l3);
	k.col = col; k.retry = retry; k.rp = retry && pressing(BTN_RETRY);
	if (!changed(K_MSG, &k, sizeof(k))) return;
	gfxRect(0, UI_BODY_Y, GFX_W, (retry ? GFX_H : SBOUT_UI_BAR_Y) - UI_BODY_Y, C_BG);
	if (l1) gfxText(UI_MARGIN, SBOUT_UI_MSG_Y, l1, col, 1);
	if (l2) gfxText(UI_MARGIN, SBOUT_UI_MSG_Y + SBOUT_UI_MSG_H, l2, col, 1);
	if (l3) gfxText(UI_MARGIN, SBOUT_UI_MSG_Y + 2 * SBOUT_UI_MSG_H, l3, col, 1);
	if (retry) uiButton(BTN_RETRY, "RETRY", C_PANEL, C_TEXT, 2, k.rp);
}

/** @brief Open SET with no row selected. */
static void openSet() {
	s_view = View::Set;
	s_sel = -1;
	uiListInit(&s_list, SET_VIEW, SET_ROW_PITCH, SET_ROWS);
	uiListSetBody(&s_list, SET_ROW_H, UI_LIST_TAP_MIN);
	s_full = true;
}

/** @brief Every touch of the main view. */
static void mainTouch() {
	uint32_t now = millis();
	// HOLD TO START, live only through startAllowed(); STOP on press. The
	// press that stops the frames landed while HOLD TO START was not live,
	// so ui_hold never times it.
	if (uiHoldUpdate(&s_hold, BTN_START, s_t, startAllowed(), now)) startFrames();
	if (s_st.running && s_t->pressed && pressing(BTN_START)) send(SbusOutCmdKind::Safe);

	// The sliders move only while frames go out; a press alone commands
	// nothing (delta drag, tap-to-set off).
	if (s_st.running) {
		for (int i = 0; i < SBOUT_UI_ROWS; i++) {
			unsigned ch = s_page * SBOUT_UI_ROWS + (unsigned)i;
			UiSlider *sl = &s_slider[ch];
			uiSliderEvent(sl, s_t);
			s_us[ch] = sbusOutStepUs(sl->value, topUs(ch));
			// Back on its step once let go, so the next drag starts at the knob.
			if (!sl->dragging) uiSliderSet(sl, s_us[ch]);
		}
	}
	// One finger: a tap on the page or SET comes after any drag let go.
	if (tapped(BTN_PAGE)) s_page ^= 1u;
	else if (tapped(BTN_SET)) openSet();
}

// ---------------------------------------------------------------------------
// SET
// ---------------------------------------------------------------------------

/** @brief Whether a step of SET row @p row in @p dir is allowed and changes something. */
static bool canStep(int row, int dir) {
	if (row == ROW_CHANNEL) {
		int c = (int)s_thrCh + dir;
		return !s_st.running && c >= 0 && c < 4;
	}
	bool on = s_flag[row - 1];
	return s_st.running && (dir > 0 ? !on : on);
}

/** @brief One step of SET row @p row, while canStep() allows it. */
static void stepRow(int row, int dir) {
	if (!canStep(row, dir)) return;
	if (row == ROW_CHANNEL) {
		s_thrCh = (uint8_t)((int)s_thrCh + dir);
		// The new channel's START values: SAFE shows what START will send.
		resetFrame();
	} else {
		s_flag[row - 1] = dir > 0;
	}
}

/** @brief SET row @p i's value. */
static void setValueText(int i, char *buf, size_t n) {
	if (i == ROW_CHANNEL) snprintf(buf, n, "CH%u", (unsigned)s_thrCh + 1u);
	else snprintf(buf, n, "%s", s_flag[i - 1] ? "ON" : "OFF");
}

/** @brief SET row @p i's state: ReadOnly where SET locks it, else Setting. */
static UiValue setValue(int i) {
	bool locked = i == ROW_CHANNEL ? s_st.running : !s_st.running;
	return locked ? UiValue::ReadOnly : UiValue::Setting;
}

/** @brief Draw SET row @p i with its top at @p y. */
static void drawSetRow(int i, int y, void *) {
	char v[8];
	setValueText(i, v, sizeof(v));
	UiField f = { ROWS[i].group, ROWS[i].name, v, setValue(i), UiEdge::None, i == s_sel };
	uiListDrawField(y, SET_ROW_H, f);
}

/** @brief SET: header, note, list, editor bar and footer. */
static void drawSet() {
	const char *note = s_st.running ? SBOUT_UI_SET_STOP : SBOUT_UI_SET_START;
	struct { bool back; uint32_t note; } kt;
	memset(&kt, 0, sizeof(kt));
	kt.back = uiPressing(uiBackRect(), s_t);
	kt.note = textKey(note);
	if (changed(K_TOP, &kt, sizeof(kt))) {
		uiHeader(SBOUT_UI_SET_TITLE, s_t, backActs());
		gfxRect(0, UI_STRIP_Y, GFX_W, SET_LIST_Y0 - UI_STRIP_Y, C_BG);
		gfxText(UI_MARGIN, UI_STRIP_Y + 2, note, UI_COL_WARN, 1);
	}

	struct { uint8_t thr; bool f[4]; int sel, scroll; bool run; } kl;
	memset(&kl, 0, sizeof(kl));
	kl.thr = s_thrCh; memcpy(kl.f, s_flag, sizeof(kl.f));
	kl.sel = s_sel; kl.scroll = s_list.scrollPx; kl.run = s_st.running;
	if (changed(K_NOTES, &kl, sizeof(kl))) uiListDraw(&s_list, drawSetRow, nullptr);

	struct { int sel; uint8_t thr; bool f[4], cm, cp, m, p; } ke;
	memset(&ke, 0, sizeof(ke));
	ke.sel = s_sel; ke.thr = s_thrCh; memcpy(ke.f, s_flag, sizeof(ke.f));
	if (s_sel >= 0) {
		ke.cm = canStep(s_sel, -1); ke.cp = canStep(s_sel, +1);
		ke.m = ke.cm && pressing(BTN_MINUS); ke.p = ke.cp && pressing(BTN_PLUS);
	}
	if (changed(K_MSG, &ke, sizeof(ke))) {
		gfxRect(0, SET_LIST_Y1 + 1, GFX_W, UI_FOOT_Y - SET_LIST_Y1 - 1, C_BG);
		if (s_sel < 0) {
			gfxTextCenter(SET_EDIT_Y + 19, "TAP A ROW TO EDIT IT", C_DIM, 1);
		} else {
			uiButton(BTN_MINUS, "-", C_PANEL, C_TEXT, 2, ke.m, ke.cm);
			uiButton(BTN_PLUS, "+", C_PANEL, C_TEXT, 2, ke.p, ke.cp);
			int cx = BTN_MINUS.x + BTN_MINUS.w, cw = BTN_PLUS.x - cx;
			const char *name = ROWS[s_sel].name;
			gfxText(cx + (cw - gfxTextW(name, 1)) / 2, SET_EDIT_Y + 6, name, C_DIM, 1);
			char v[8];
			setValueText(s_sel, v, sizeof(v));
			gfxText(cx + (cw - gfxTextW(v, 2)) / 2, SET_EDIT_Y + 22, v,
			        uiListValueColour(setValue(s_sel)), 2);
		}
	}

	// While RUNNING the right button is STOP, as on SERVO SET; while SAFE
	// the footer carries the second warning and no button.
	struct { bool run, p; } kf;
	memset(&kf, 0, sizeof(kf));
	kf.run = s_st.running;
	kf.p = s_st.running && pressing(BTN_STOP);
	if (changed(K_ROW0, &kf, sizeof(kf))) {
		gfxRect(0, UI_FOOT_Y - 4, GFX_W, GFX_H - UI_FOOT_Y + 4, C_BG);
		if (s_st.running) uiButton(BTN_STOP, "STOP", UI_COL_ON, C_ONACCENT, 2, kf.p);
		else gfxText(UI_MARGIN, SBOUT_UI_FOOT_TEXT_Y, SBUS_OUT_VIEW_WARN_2, UI_COL_WARN, 1);
	}
}

/** @brief Every touch of SET. */
static void setTouch() {
	int dir = 0;
	if (s_sel >= 0 && pressing(BTN_MINUS)) dir = -1;
	else if (s_sel >= 0 && pressing(BTN_PLUS)) dir = +1;
	if (dir && repeatFires(&s_repeat, dir, millis())) stepRow(s_sel, dir);
	if (!dir) repeatFires(&s_repeat, 0, millis());
	if (s_st.running && s_t->pressed && pressing(BTN_STOP)) send(SbusOutCmdKind::Safe);
	UiListEvent e = uiListTouch(&s_list, s_t);
	if (e.kind == UiListEv::Tap || e.kind == UiListEv::Swipe) s_sel = e.row;
}

// ---------------------------------------------------------------------------
// the leave page
// ---------------------------------------------------------------------------

/**
 * @brief Draw the leave page.
 *
 * Opened releasing, its first step unplugs the FC. After an EFM8 answer it
 * opens before LEAVE with the first step of decision 8, and its last step
 * plugs the ESC back in.
 */
static void drawLeave() {
	uint32_t now = millis();
	struct {
		bool    back, left, wide, live, leavePress, restartPress, efm8;
		uint8_t phase, sig, aux;
	} k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.left = uiPressing(UI_LEAVE_LEFT, s_t);
	k.wide = uiPressing(UI_LEAVE_WIDE, s_t);
	k.live = uiLeaveRestartLive(&s_leave, now);
	k.leavePress = s_leave.leavePress;
	k.restartPress = s_leave.restartPress;
	k.efm8 = s_efm8Page;
	k.phase = (uint8_t)s_leave.phase;
	k.sig = gpioOf(PIN_SIG);
	k.aux = gpioOf(PIN_AUX);
	if (!changed(K_TOP, &k, sizeof(k))) return;
	uiHeader(SBOUT_UI_TITLE, s_t, backActs());
	uiStripClear();
	char strip[32];
	snprintf(strip, sizeof(strip), "SIG GP%u  AUX GP%u", (unsigned)k.sig, (unsigned)k.aux);
	uiStripText(strip, C_DIM);
	static char dshot[40];
	snprintf(dshot, sizeof(dshot), SBOUT_UI_RESTART_FMT, (unsigned)k.sig);
	UiLeaveText tx;
	memset(&tx, 0, sizeof(tx));
	int s = 0;
	if (s_efm8Page) {
		tx.step[0][0] = AM32_UI_LEAVE_1;
		tx.step[0][1] = AM32_UI_LEAVE_1A;
		tx.step[0][2] = AM32_UI_LEAVE_1B;
		s = 1;
	}
	tx.step[s][0] = SBOUT_UI_UNPLUG;
	tx.step[s][1] = dshot;
	tx.step[s + 1][0] = UI_LEAVE_RESTART;
	if (s_efm8Page) tx.step[s + 1][1] = SBOUT_UI_PLUG_ESC;
	tx.next = SBOUT_UI_BACK_TO;
	if (s_leave.phase == UiLeavePhase::Releasing) tx.note[0] = UI_LEAVE_RELEASING;
	uiLeaveDraw(&s_leave, tx, s_t, now);
}

/** @brief A new visit after BACK TO OUTPUT, or on entry: page 1, the START frame, no stop. */
static void newVisit() {
	s_view = View::Main;
	s_page = 0;
	s_stop = SbusOutStop::None;
	s_ceilRaw = sbusOutCeilRaw(settings()->maxThrottle);
	resetFrame();
	uiHoldReset(&s_hold);
	s_full = true;
}

/**
 * @brief One frame of the leave page.
 * @return false once RESTART DSHOT gave GP0 back to DShot.
 */
static bool leaveTick() {
	uint32_t now = millis();
	uiLeavePoll(&s_leave, pinClaimUnplugDue(), now);
	UiLeaveEv ev = uiLeaveTouch(&s_leave, s_t, now);
	if (ev == UiLeaveEv::Stay) {
		s_view = View::Main;
		s_full = true;
		return true;
	}
	if (ev == UiLeaveEv::Leave) {
		pinClaimEnd(PinUser::SbusOut);
		ClaimState cs = pinClaimState();
		if (cs == ClaimState::Free || cs == ClaimState::Restoring) return false;
		s_full = true;
	}
	// The page gives RESTART DSHOT and BACK TO OUTPUT only once GP0 is let
	// go (uiLeavePoll()).
	if (ev == UiLeaveEv::Restart) {
		pinClaimRestart(PinUser::SbusOut);
		// Only the page that asked for the ESC's battery to come off clears
		// the EFM8 mark and MENU's cue: a mark that landed after BACK keeps
		// them.
		if (s_efm8Page) sbusOutProbeRestart();
		return false;
	}
	if (ev == UiLeaveEv::Next) {
		pinClaimKeep(PinUser::SbusOut);
		newVisit();
		return true;
	}
	if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
	drawLeave();
	s_full = false;
	return true;
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

uint8_t uiSbusOutControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), backActs());
	if (s_view == View::Leave) {
		bool before = s_leave.phase == UiLeavePhase::Before;
		uiControlAdd(out, &n, max, UI_FIX(before ? SbusOutCtl::Stay : SbusOutCtl::BackToOutput),
		             UI_LEAVE_LEFT, before || s_leave.phase == UiLeavePhase::Released);
		uiControlAdd(out, &n, max, UI_FIX(before ? SbusOutCtl::Leave : SbusOutCtl::Restart),
		             UI_LEAVE_WIDE, before ? s_leave.leaveEnabled : uiLeaveRestartLive(&s_leave, millis()));
		return n;
	}
	if (s_view == View::Set) {
		uiListAddRows(&s_list, out, &n, max, true);
		if (s_sel >= 0) {
			uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::EditMinus), BTN_MINUS, canStep(s_sel, -1));
			uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::EditPlus), BTN_PLUS, canStep(s_sel, +1));
		}
		if (s_st.running) uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::Stop), BTN_STOP, true);
		return n;
	}
	if (retryShown()) {
		uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::Retry), BTN_RETRY, true);
		return n;
	}
	if (rowsShown())
		for (int i = 0; i < SBOUT_UI_ROWS; i++)
			uiControlAdd(out, &n, max, UI_FIX((uint8_t)SbusOutCtl::Row0 + i), rowRect(i), s_st.running);
	uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::Start), BTN_START, s_st.running || startAllowed());
	uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::Page), BTN_PAGE, true);
	uiControlAdd(out, &n, max, UI_FIX(SbusOutCtl::Set), BTN_SET, true);
	return n;
}

void uiSbusOutEnter() {
	s_claimed = false;
	s_refusal = ClaimRefusal::None;
	s_probe = SbusOutProbeState::Off;
	s_efm8Page = false;
	s_thrCh = SBUS_OUT_THR_DEFAULT;
	memset(&s_st, 0, sizeof(s_st));
	for (unsigned i = 0; i < SBUS_CHANNELS; i++) {
		uiSliderInit(&s_slider[i], rowRect((int)(i % SBOUT_UI_ROWS)), (float)SBUS_OUT_VIEW_US_LO,
		             (float)SBUS_OUT_VIEW_US_HI);
	}
	s_touchMs = millis();
	s_sel = -1;
	s_repeat = Repeat{};
	newVisit();
	claimPin();
}

bool uiSbusOutTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	uint32_t now = millis();
	if (s_t->down) s_touchMs = now;
	sbusOutJobStatus(&s_st);
	if (s_claimed) {
		pinClaimPoll();
		s_probe = sbusOutProbePoll();
	} else if (s_view != View::Leave) {
		claimPin();
	}

	// BACK: from SET to the main view; from the main view out, through the
	// leave page once a job has held GP0. After an EFM8 answer the page opens
	// before LEAVE with decision 8's first step, the frames stopped first.
	if (backActs() && uiBackTapped(s_t)) {
		s_full = true;
		if (s_view == View::Set) {
			s_view = View::Main;
		} else {
			for (UiSlider &sl : s_slider) uiSliderRelease(&sl);
			if (!s_claimed) return false;
			s_efm8Page = sbusOutProbeEfm8();
			s_view = View::Leave;
			if (s_efm8Page) {
				send(SbusOutCmdKind::Safe);
				uiLeaveOpen(&s_leave, UiLeavePhase::Before, UI_HOLD_MS);
			} else {
				pinClaimEnd(PinUser::SbusOut);
				ClaimState cs = pinClaimState();
				// Restoring: core1 suspended the pump between pinClaimPoll()
				// and pinClaimEnd().
				if (cs == ClaimState::Free || cs == ClaimState::Restoring) return false;
				// RESTART DSHOT waits 1000 ms after GP0 is let go.
				uiLeaveOpen(&s_leave, UiLeavePhase::Releasing, UI_HOLD_MS);
			}
		}
	}

	if (s_view == View::Leave) return leaveTick();

	const char *l1, *l2, *l3;
	uint16_t col;
	bool retry;
	message(&l1, &l2, &l3, &col, &retry);
	if (retry && tapped(BTN_RETRY)) pinClaimRetry(PinUser::SbusOut);

	// RETRY lies over the bottom row, which is not drawn under it.
	if (s_view == View::Set) setTouch();
	else if (!retry) mainTouch();

	// The stops of the screen's own, in either view: a touch that does not
	// answer, which cannot press STOP, sent every frame until the job reports
	// the frames off; then the idle rule.
	sbus_frame_t want = wantFrame();
	if (s_st.running && uiTouchLost()) {
		send(SbusOutCmdKind::Safe);
		s_stop = SbusOutStop::Touch;
	} else if (sbusOutIdleDue(s_st.running, &want, s_thrCh, now, s_touchMs)) {
		send(SbusOutCmdKind::Safe);
		s_stop = SbusOutStop::Idle;
	} else if (memcmp(&want, &s_sentFrame, sizeof(want)) != 0) {
		// The sliders and the flags move only while RUNNING.
		send(SbusOutCmdKind::Frame);
		s_sentFrame = want;
	}

	if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
	if (s_view == View::Set) {
		drawSet();
		s_full = false;
		return true;
	}
	message(&l1, &l2, &l3, &col, &retry);
	bool rows = !l1;
	// The rows, a message and RETRY cover different parts of the body: a
	// change between them starts from a clear body.
	uint8_t body = rows ? 1 : retry ? 3 : 2;
	if (body != s_body && !s_full) {
		gfxRect(0, UI_BODY_Y, GFX_W, GFX_H - UI_BODY_Y, C_BG);
		for (int i = K_NOTES; i <= K_BOT; i++) s_keys[i] = 0;
	}
	s_body = body;
	drawTop(SBOUT_UI_TITLE, rows);
	if (rows) {
		drawNotes();
		for (int i = 0; i < SBOUT_UI_ROWS; i++) drawRow(i);
	} else {
		drawMessage(l1, l2, l3, col, retry);
	}
	drawBottom(retry);
	s_full = false;
	return true;
}

