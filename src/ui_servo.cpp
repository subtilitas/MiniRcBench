/**
 * @file ui_servo.cpp
 * @brief The servo tester screens. @see ui_servo.h
 *
 * Three views share one claim: SERVO, where the servo is driven; SET, where
 * the profile, SPEED's sweep and the servo count are chosen; and the unplug
 * page, between BACK and DShot's restart. Every rule about pulses, limits and
 * steppers is in servo_model.h, and every pulse is core1's (servo_job.h); this
 * file draws, reads the touch and sends commands.
 *
 * Each region of a view is drawn from a key of the values it shows and is
 * repainted only when its key changes, so a still screen costs no SPI
 * (Serial Peripheral Interface) traffic.
 */

#include "ui_servo.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "servo_job.h"
#include "servo_model.h"
#include "touch.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_list.h"
#include "ui_slider.h"
#include "ui_widgets.h"
#include "ui_screen.h"
#include "gfx.h"
#include "config.h"

#include "plat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------

/**
 * @defgroup servo_layout SERVO screen layout
 * @brief Rows of the SERVO view, each region ending where the next begins.
 * @{
 */
#define SV_READ_Y    200   /**< Readout row: the pulse, the selector. */
#define SV_READ_H     24
#define SV_SPEED_Y   227   /**< SPEED's track. */
#define SV_SPEED_H    22
#define SV_BTN_Y     254   /**< MIN, CENTRE, MAX, SWEEP. */
#define SV_BTN_H      30
#define SV_BTN_W      53
#define SV_BAR_Y     284   /**< HOLD TO START's progress bar, 3 px. */
#define SV_BOT_Y     287   /**< HOLD TO START or STOP, and SET. */
#define SV_BOT_H      32
#define SV_HORN_R     84   /**< Horn length, px from the shaft. */
#define SV_SHAFT_R     8   /**< Shaft radius, px. */
/** @brief First row of the region below SPEED: the buttons and the bar. */
#define SV_LOW_Y     (SV_SPEED_Y + SV_SPEED_H + UI_SLIDER_OVERHANG)
/** @} */

static const UiRect BTN_MIN    = { 8,   SV_BTN_Y, SV_BTN_W, SV_BTN_H };
static const UiRect BTN_CENTRE = { 65,  SV_BTN_Y, SV_BTN_W, SV_BTN_H };
static const UiRect BTN_MAX    = { 122, SV_BTN_Y, SV_BTN_W, SV_BTN_H };
static const UiRect BTN_SWEEP  = { 179, SV_BTN_Y, SV_BTN_W, SV_BTN_H };
static const UiRect BTN_START  = { 6,   SV_BOT_Y, 150, SV_BOT_H };
static const UiRect BTN_SET    = { 162, SV_BOT_Y, 72,  SV_BOT_H };
static const UiRect BTN_SEL_A  = { 122, SV_READ_Y + 1, 34, 22 };
static const UiRect BTN_SEL_B  = { 160, SV_READ_Y + 1, 34, 22 };
static const UiRect BTN_SEL_AB = { 198, SV_READ_Y + 1, 34, 22 };
static const UiRect BTN_RETRY  = { 70,  150, 100, 30 };
static const UiRect BTN_RESTART = { 40, 200, 160, 40 };
/** @brief SPEED's track: between its label and its value. */
static const UiRect SPEED_TRACK = { 44, SV_SPEED_Y, 150, SV_SPEED_H };

static_assert(SERVO_DIAL_Y0 == UI_BODY_Y, "the dial band starts under the strip");
static_assert(SV_READ_Y == SERVO_DIAL_Y1 + 1, "gap between the dial and the readout");
static_assert(SV_SPEED_Y - UI_SLIDER_OVERHANG >= SV_READ_Y + SV_READ_H,
              "SPEED's thumb reaches into the readout");
static_assert(SV_LOW_Y <= SV_BTN_Y, "SPEED's thumb reaches into the button row");
static_assert(SV_SPEED_Y - UI_SLIDER_OVERHANG == SV_READ_Y + SV_READ_H,
              "a row between the readout and SPEED is repainted by neither");
static_assert(SV_BTN_Y + SV_BTN_H <= SV_BAR_Y, "the buttons overlap the progress bar");
static_assert(SV_BAR_Y + UI_HOLD_BAR_H <= SV_BOT_Y, "the progress bar overlaps the bottom row");
static_assert(SV_BOT_Y + SV_BOT_H <= GFX_H, "the bottom row runs off the panel");
static_assert(8 + 4 * SV_BTN_W + 3 * 4 == 232, "the button row does not fill the margins");
static_assert(SV_BTN_H >= UI_TAP_MIN && SV_BOT_H >= UI_TAP_MIN && 22 >= UI_TAP_MIN &&
              SV_SPEED_H >= UI_TAP_MIN, "a servo control is smaller than a fingertip");
static_assert(SERVO_DIAL_Y - SERVO_DIAL_ARC_R - 4 > 82,
              "the arc runs into the two note lines");

/**
 * @defgroup servo_set_layout SET screen layout
 * @brief The AM32 editor's list idiom: rows, an editor bar, a footer.
 * @{
 */
#define SET_ROW_H    UI_LIST_TAP_MIN  /**< A SET row, 40 px. */
#define SET_ROW_PITCH 42     /**< Row top to row top: a 2 px gap under each row. */
#define SET_LIST_Y0  UI_BODY_Y
#define SET_LIST_Y1  215
#define SET_LIST_H   (SET_LIST_Y1 - SET_LIST_Y0 + 1)
#define SET_EDIT_Y   218
#define SET_EDIT_H   44
#define SET_EDIT_W   58
#define SET_FOOT_Y   268
#define SET_FOOT_H   46
/** @} */

static const UiRect BTN_MINUS    = { 6, SET_EDIT_Y, SET_EDIT_W, SET_EDIT_H };
static const UiRect BTN_PLUS     = { GFX_W - SET_EDIT_W - 6, SET_EDIT_Y, SET_EDIT_W, SET_EDIT_H };
static const UiRect BTN_APPLY    = { 6, SET_FOOT_Y, 140, SET_FOOT_H };
static const UiRect BTN_DEFAULTS = { 152, SET_FOOT_Y, 82, SET_FOOT_H };
/** @brief The SET list's viewport. */
static const UiRect SET_VIEW     = { 0, SET_LIST_Y0, GFX_W, SET_LIST_H };

static_assert(SET_LIST_Y1 < SET_EDIT_Y, "the list overlaps the editor bar");
static_assert(SET_EDIT_Y + SET_EDIT_H <= SET_FOOT_Y, "the editor bar overlaps the footer");
static_assert(SET_FOOT_Y + SET_FOOT_H <= GFX_H, "the footer runs off the panel");
static_assert(SET_ROW_H >= UI_LIST_TAP_MIN && SET_ROW_PITCH > SET_ROW_H,
              "a SET row is lower than the list floor, or has no gap");
static_assert(SET_EDIT_H >= UI_TAP_MIN && SET_FOOT_H >= UI_TAP_MIN,
              "a SET button is smaller than a fingertip");

/**
 * @brief Hold that commits HOLD TO START and HOLD TO APPLY: one length on the
 *        device.
 */
#define SV_HOLD_MS UI_HOLD_MS

// ---------------------------------------------------------------------------
// the SET rows
// ---------------------------------------------------------------------------

/** @brief One row of the SET list. */
struct SetRow {
	const char *group;  /**< PULSE or SWEEP. */
	const char *name;   /**< The row's name. */
	bool        pulse;  /**< A PULSE row: ServoField, changes only while SAFE. */
	uint8_t     field;  /**< ServoField or SweepField. */
};

static const SetRow ROWS[] = {
	{ "PULSE", "SERVOS",    true,  (uint8_t)ServoField::Servos },
	{ "PULSE", "TYPE",      true,  (uint8_t)ServoField::Type },
	{ "PULSE", "RATE",      true,  (uint8_t)ServoField::Rate },
	{ "PULSE", "MIN",       true,  (uint8_t)ServoField::Min },
	{ "PULSE", "CENTRE",    true,  (uint8_t)ServoField::Centre },
	{ "PULSE", "MAX",       true,  (uint8_t)ServoField::Max },
	{ "PULSE", "REVERSE B", true,  (uint8_t)ServoField::ReverseB },
	{ "SWEEP", "SHAPE",     false, (uint8_t)SweepField::Shape },
	{ "SWEEP", "RATE",      false, (uint8_t)SweepField::Rate },
	{ "SWEEP", "AMPLITUDE", false, (uint8_t)SweepField::Amplitude },
	{ "SWEEP", "DWELL",     false, (uint8_t)SweepField::Dwell },
	{ "SWEEP", "MOVES",     false, (uint8_t)SweepField::Moves },
};
#define SET_ROWS ((int)(sizeof(ROWS) / sizeof(ROWS[0])))
/** @brief The SERVOS row. */
#define ROW_SERVOS 0

/**
 * @brief Steps a coarse step of a swipe, @ref UI_SWIPE_STEP_PX, applies:
 *        numbers ten, choices one.
 */
static int coarseOf(const SetRow &r) {
	if (!r.pulse) return r.field == (uint8_t)SweepField::Shape ? 1 : 10;
	ServoField f = (ServoField)r.field;
	return (f == ServoField::Min || f == ServoField::Centre || f == ServoField::Max) ? 10 : 1;
}

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

/** @brief Which view owns the panel. */
enum class View : uint8_t { Main, Set, Unplug };

static View         s_view = View::Main;
static bool         s_claimed = false;     /**< pinClaimBegin() took this screen's claim. */
static ClaimRefusal s_refusal = ClaimRefusal::None;     /**< Why the last claim was refused. */
static ClaimRefusal s_auxRefusal = ClaimRefusal::None;  /**< Why AUX was refused: TWO is not offered. */
static ServoProfile s_profile;             /**< The profile handed to the job. */
static ServoProfile s_edit;                /**< What SET shows; s_profile but for a pending change. */
static bool         s_pending = false;     /**< s_edit waits for HOLD TO APPLY. */
static bool         s_owed = false;        /**< s_profile is not yet accepted by the job. */
static sweep_cfg_t  s_sweep;               /**< The curve the next SWEEP runs. */
static UiSlider     s_speed;               /**< SPEED, percent. */
static uint8_t      s_mask = SERVO_MASK_A; /**< What the dial and the buttons drive. */
static bool         s_dragging = false;    /**< A press on the dial drives the servo. */
static uint16_t     s_dragCmd = 0;         /**< The last command the drag sent. */
static bool         s_wasRunning = false;  /**< RUNNING on the last frame. */
/**
 * @brief HOLD TO START or HOLD TO APPLY: one timer. A change of view comes on
 *        a release, which ends any hold.
 */
static UiHold       s_hold;
static bool         s_needLift = false;    /**< A STOP press is down; it starts and taps nothing. */
static ServoStatus  s_st;                  /**< This frame's status from core1. */
static const TouchState *s_t = nullptr;    /**< This frame's touch. */
static bool         s_full = true;         /**< Repaint every region this frame. */

/**
 * @defgroup servo_set_state SET list state
 * @{
 */
static int    s_sel = -1;        /**< Selected row, or -1. */
static UiList s_list;            /**< The rows, scrolled in pixels. */
static Repeat s_repeat;          /**< The editor bar's held repeat. */
/** @} */

/**
 * @brief Keys of the regions drawn last, so an unchanged region is not
 *        repainted.
 */
static uint32_t s_keys[8];

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

/**
 * @brief The GPIO a role names, from the claim, or from the pump before it
 *        holds.
 */
static uint8_t gpioOf(uint8_t role) {
	uint8_t g = pinClaimGpio(role);
	if (g != PIN_CLAIM_NO_GPIO) return g;
	return role == PIN_SIG ? escTaskDshotPin() : escTaskKissPin();
}

/** @brief The claim holds the pins and the job is bound. */
static bool live() {
	return s_claimed && pinClaimState() == ClaimState::Running && s_st.bindError == 0;
}

/** @brief Two servos, with AUX bound. */
static bool twoServos() { return s_profile.servos == 2; }

/** @brief Send a command with this frame's SPEED and sweep. */
static void send(ServoCmdKind kind, uint16_t command = SERVO_CMD_CENTRE) {
	ServoCmd c;
	memset(&c, 0, sizeof(c));
	c.kind = kind;
	c.mask = twoServos() ? s_mask : SERVO_MASK_A;
	c.command = command;
	c.slewPerS = servoSlewOf((int)(s_speed.value + 0.5f));
	c.sweep = s_sweep;
	servoJobCommand(&c);
}

/** @brief Hand s_profile to the job, at once or on a later frame. */
static void applyProfile() {
	s_owed = !servoJobProfile(&s_profile);
}

/**
 * @brief Ask for the pins: SIG and AUX, or SIG alone when the claim refuses
 *        AUX, which then leaves TWO off the SET screen.
 */
static void claimPins() {
	ClaimRefusal r = pinClaimBegin(PinUser::Servo, PIN_SIG | PIN_AUX);
	if (r == ClaimRefusal::None) s_auxRefusal = ClaimRefusal::None;
	if (r == ClaimRefusal::KissLive || r == ClaimRefusal::SamePin) {
		s_auxRefusal = r;
		r = pinClaimBegin(PinUser::Servo, PIN_SIG);
	}
	s_claimed = r == ClaimRefusal::None;
	s_refusal = r;
	if (s_claimed && s_auxRefusal != ClaimRefusal::None && s_profile.servos == 2) {
		s_profile.servos = 1;
		s_edit.servos = 1;
		applyProfile();
	}
}

/** @brief Draw @p text centred at @p y, scale 1. */
static void line(int y, const char *text, uint16_t c) { gfxTextCenter(y, text, c, 1); }

// ---------------------------------------------------------------------------
// the SERVO view
// ---------------------------------------------------------------------------

/** @brief The header, the caption and the chip. */
static void drawTop(const char *title, bool caption) {
	struct { bool back; uint8_t chip; ServoProfile p; uint8_t sig, aux; bool cap; } k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t);
	k.chip = s_st.running ? 1 : (s_st.tripped ? 2 : 0);
	k.p = s_profile;
	k.sig = gpioOf(PIN_SIG);
	k.aux = gpioOf(PIN_AUX);
	k.cap = caption;
	if (!changed(0, &k, sizeof(k))) return;
	uiHeader(title, s_t);
	uiStripClear();
	if (!caption) return;
	char buf[40];
	if (twoServos())
		snprintf(buf, sizeof(buf), "GP%u+%u  %u HZ  %u-%u US", k.sig, k.aux,
		         s_profile.rateHz, s_profile.minUs, s_profile.maxUs);
	else
		snprintf(buf, sizeof(buf), "GP%u  %u HZ  %u-%u US", k.sig, s_profile.rateHz,
		         s_profile.minUs, s_profile.maxUs);
	uiStripText(buf, C_DIM);
	if (s_st.running)      uiChip("RUNNING", C_RED);
	else if (s_st.tripped) uiChip("STALLED", C_MAGENTA);
	else                   uiChip("SAFE", C_GREEN);
}

/**
 * @brief What the dial band shows instead of the dial, or null for the dial.
 */
static void claimMessage(const char **l1, const char **l2, const char **l3,
                         uint16_t *col, bool *retry) {
	static char a[40], b[40];
	*l1 = *l2 = *l3 = nullptr;
	*col = C_AMBER;
	*retry = false;
	uint8_t sig = gpioOf(PIN_SIG);
	if (!s_claimed) {
		switch (s_refusal) {
			case ClaimRefusal::EscAnswering:
				snprintf(a, sizeof(a), "AN ESC ANSWERS ON GP%u.", sig);
				*l1 = a; *l2 = "UNPLUG IT: SERVO PULSES"; *l3 = "READ AS THROTTLE.";
				return;
			case ClaimRefusal::Armed: *l1 = "DISARM FIRST"; return;
			default:                  *l1 = "PINS BUSY";    return;
		}
	}
	ClaimState cs = pinClaimState();
	if (cs == ClaimState::Stopping) { *l1 = "RELEASING DSHOT PIN"; *col = C_DIM; return; }
	uint8_t err = pinClaimJobError() ? pinClaimJobError() : s_st.bindError;
	if (err) {
		static const char *const WHY[] = {"", "REFUSED", "PWM", "PIO", "DMA", "CLOCK"};
		snprintf(b, sizeof(b), "GP%u COULD NOT BE TAKEN (%s)", sig, WHY[err < 6 ? err : 1]);
		*l1 = b;
		*retry = true;
		return;
	}
	if (cs != ClaimState::Running) {
		snprintf(a, sizeof(a), "TAKING GP%u", sig);
		*l1 = a; *col = C_DIM;
	}
}

/** @brief The horn of one servo at its pulse. */
static void drawHorn(uint16_t us, uint16_t col, bool selected) {
	float deg = servoUsToDeg(&s_profile, us);
	float rad = deg * 3.14159265f / 180.0f;
	int tx = SERVO_DIAL_X + (int)(SV_HORN_R * sinf(rad) + (sinf(rad) < 0 ? -0.5f : 0.5f));
	int ty = SERVO_DIAL_Y - (int)(SV_HORN_R * cosf(rad) + (cosf(rad) < 0 ? -0.5f : 0.5f));
	gfxLine(SERVO_DIAL_X, SERVO_DIAL_Y, tx, ty, selected ? 5 : 1, col);
	if (selected) gfxRoundRect(tx - 6, ty - 6, 12, 12, 6, col);
}

/**
 * @brief The dial band: the claim's message, or the arc, the notes and the
 *        horns.
 */
static void drawDial() {
	const char *l1, *l2, *l3;
	uint16_t col;
	bool retry;
	claimMessage(&l1, &l2, &l3, &col, &retry);
	struct { uint32_t l1, l2, l3; bool retry, rp; uint16_t us[2]; uint8_t mask, servos;
	         bool running, tripped; } k;
	memset(&k, 0, sizeof(k));
	k.l1 = textKey(l1); k.l2 = textKey(l2); k.l3 = textKey(l3);
	k.retry = retry; k.rp = retry && pressing(BTN_RETRY);
	k.us[0] = s_st.outUs[0]; k.us[1] = s_st.outUs[1];
	k.mask = s_mask; k.servos = s_profile.servos;
	k.running = s_st.running; k.tripped = s_st.tripped;
	if (!changed(1, &k, sizeof(k))) return;

	gfxRect(0, SERVO_DIAL_Y0, GFX_W, SERVO_DIAL_Y1 - SERVO_DIAL_Y0 + 1, C_BG);
	if (l1) {
		line(90, l1, col);
		if (l2) line(104, l2, col);
		if (l3) line(118, l3, col);
		if (retry) uiButton(BTN_RETRY, "RETRY", C_PANEL, C_TEXT, 1, k.rp);
		return;
	}
	if (s_st.tripped && !s_st.running) {
		line(64, "STOPPED: SCREEN STALLED", C_MAGENTA);
		line(76, "HOLD TO START AGAIN.", C_DIM);
	} else if (!s_st.running) {
		line(64, "THE SERVO MOVES TO CENTRE AT START.", C_DIM);
		char b[40];
		snprintf(b, sizeof(b), "PULSES ON GP%u ARE THROTTLE TO AN ESC.", gpioOf(PIN_SIG));
		line(76, b, C_DIM);
	}
	gfxArcDots(SERVO_DIAL_X, SERVO_DIAL_Y, SERVO_DIAL_ARC_R, -90, 90, 10, 3, C_GRID);
	gfxLine(SERVO_DIAL_X, SERVO_DIAL_Y - SERVO_DIAL_ARC_R - 4, SERVO_DIAL_X,
	        SERVO_DIAL_Y - SERVO_DIAL_ARC_R + 4, 1, C_DIM);
	gfxText(SERVO_DIAL_X - SERVO_DIAL_ARC_R - 10, SERVO_DIAL_Y + 6, "MIN", C_DIM, 1);
	gfxText(SERVO_DIAL_X + SERVO_DIAL_ARC_R - 7, SERVO_DIAL_Y + 6, "MAX", C_DIM, 1);
	// The unselected horn first, so the selected one is drawn over it.
	bool selA = !twoServos() || (s_mask & SERVO_MASK_A);
	bool selB = twoServos() && (s_mask & SERVO_MASK_B);
	if (twoServos() && s_st.outUs[1] && !selB) drawHorn(s_st.outUs[1], C_CYAN, false);
	if (s_st.outUs[0] && !selA) drawHorn(s_st.outUs[0], C_LIME, false);
	if (twoServos() && s_st.outUs[1] && selB) drawHorn(s_st.outUs[1], C_CYAN, true);
	if (s_st.outUs[0] && selA) drawHorn(s_st.outUs[0], C_LIME, true);
	gfxRoundRect(SERVO_DIAL_X - SV_SHAFT_R, SERVO_DIAL_Y - SV_SHAFT_R, 2 * SV_SHAFT_R,
	             2 * SV_SHAFT_R, SV_SHAFT_R, C_TEXT);
}

/** @brief The pulse in us, the sweep's count, and the A/B/AB selector. */
static void drawReadout() {
	struct { uint16_t us[2]; bool sweeping; uint32_t moves; uint16_t limit; uint8_t mask, servos;
	         bool pa, pb, pab; } k;
	memset(&k, 0, sizeof(k));
	k.us[0] = s_st.outUs[0]; k.us[1] = s_st.outUs[1];
	k.sweeping = s_st.sweeping; k.moves = s_st.sweepMoves; k.limit = s_sweep.moves;
	k.mask = s_mask; k.servos = s_profile.servos;
	k.pa = pressing(BTN_SEL_A); k.pb = pressing(BTN_SEL_B); k.pab = pressing(BTN_SEL_AB);
	if (!changed(2, &k, sizeof(k))) return;

	gfxRect(0, SV_READ_Y, GFX_W, SV_READ_H, C_BG);
	char sw[24] = "";
	if (s_st.sweeping) {
		if (s_sweep.moves) snprintf(sw, sizeof(sw), "SWEEP %lu/%u", (unsigned long)s_st.sweepMoves, s_sweep.moves);
		else               snprintf(sw, sizeof(sw), "SWEEP %lu", (unsigned long)s_st.sweepMoves);
	}
	char buf[32];
	if (!twoServos()) {
		if (s_st.outUs[0]) snprintf(buf, sizeof(buf), "%u US", s_st.outUs[0]);
		else               snprintf(buf, sizeof(buf), "NO PULSE");
		gfxText(UI_MARGIN, SV_READ_Y + 5, buf, s_st.outUs[0] ? C_LIME : C_DIM, 2);
		if (sw[0]) gfxText(GFX_W - UI_MARGIN - gfxTextW(sw, 1), SV_READ_Y + 9, sw, C_CYAN, 1);
		return;
	}
	char a[8], b[8];
	if (s_st.outUs[0]) snprintf(a, sizeof(a), "%u", s_st.outUs[0]); else snprintf(a, sizeof(a), "--");
	if (s_st.outUs[1]) snprintf(b, sizeof(b), "%u", s_st.outUs[1]); else snprintf(b, sizeof(b), "--");
	gfxText(UI_MARGIN, SV_READ_Y + 3, "A", C_LIME, 1);
	gfxText(UI_MARGIN + 10, SV_READ_Y + 3, a, C_TEXT, 1);
	gfxText(UI_MARGIN + 50, SV_READ_Y + 3, "B", C_CYAN, 1);
	gfxText(UI_MARGIN + 60, SV_READ_Y + 3, b, C_TEXT, 1);
	if (sw[0]) gfxText(UI_MARGIN, SV_READ_Y + 14, sw, C_CYAN, 1);
	uiButton(BTN_SEL_A, "A", s_mask == SERVO_MASK_A ? C_LIME : C_PANEL,
	         s_mask == SERVO_MASK_A ? C_ONACCENT : C_TEXT, 1, k.pa);
	uiButton(BTN_SEL_B, "B", s_mask == SERVO_MASK_B ? C_CYAN : C_PANEL,
	         s_mask == SERVO_MASK_B ? C_ONACCENT : C_TEXT, 1, k.pb);
	uiButton(BTN_SEL_AB, "AB", s_mask == SERVO_MASK_AB ? C_INK : C_PANEL,
	         s_mask == SERVO_MASK_AB ? C_PAPER : C_TEXT, 1, k.pab);
}

/** @brief SPEED: label, slider, percentage. */
static void drawSpeed() {
	struct { float v; bool drag; } k;
	memset(&k, 0, sizeof(k));
	k.v = s_speed.value; k.drag = s_speed.dragging;
	if (!changed(3, &k, sizeof(k))) return;
	UiRect pr = uiSliderPaintedRect(&s_speed);
	gfxRect(0, pr.y, GFX_W, pr.h, C_BG);
	gfxText(UI_MARGIN, SV_SPEED_Y + (SV_SPEED_H - 7) / 2, "SPEED", C_DIM, 1);
	uiSliderDraw(&s_speed, C_CYAN);
	char buf[8];
	snprintf(buf, sizeof(buf), "%d%%", (int)(s_speed.value + 0.5f));
	gfxText(GFX_W - UI_MARGIN - gfxTextW(buf, 1), SV_SPEED_Y + (SV_SPEED_H - 7) / 2, buf, C_TEXT, 1);
}

/** @brief MIN, CENTRE, MAX, SWEEP; HOLD TO START or STOP; SET. */
static void drawButtons(bool startOk, int holdPct) {
	bool run = s_st.running && live();
	struct { bool run, sweeping, startOk, holding; int pct; bool p[6]; } k;
	memset(&k, 0, sizeof(k));
	k.run = run; k.sweeping = s_st.sweeping; k.startOk = startOk; k.holding = s_hold.holding;
	k.pct = holdPct;
	k.p[0] = pressing(BTN_MIN); k.p[1] = pressing(BTN_CENTRE); k.p[2] = pressing(BTN_MAX);
	k.p[3] = pressing(BTN_SWEEP); k.p[4] = pressing(BTN_START); k.p[5] = pressing(BTN_SET);
	if (!changed(4, &k, sizeof(k))) return;
	gfxRect(0, SV_LOW_Y, GFX_W, GFX_H - SV_LOW_Y, C_BG);
	uiButton(BTN_MIN, "MIN", C_PANEL, C_CYAN, 1, k.p[0], run);
	uiButton(BTN_CENTRE, "CENTRE", C_PANEL, C_CYAN, 1, k.p[1], run);
	uiButton(BTN_MAX, "MAX", C_PANEL, C_CYAN, 1, k.p[2], run);
	uiButton(BTN_SWEEP, s_st.sweeping ? "STOP SWEEP" : "SWEEP", C_PANEL, C_CYAN, 1, k.p[3], run);
	if (s_st.running) {
		uiButton(BTN_START, "STOP", C_RED, C_ONACCENT, 2, k.p[4]);
	} else {
		uiHoldDrawBar(&s_hold, BTN_START, SV_BAR_Y, millis());
		uiButton(BTN_START, "HOLD TO START", s_hold.holding ? C_RED : C_PANEL,
		         s_hold.holding ? C_ONACCENT : C_TEXT, 1, s_hold.holding, startOk);
	}
	uiButton(BTN_SET, "SET", C_PANEL, C_CYAN, 1, k.p[5]);
}

/** @brief Point the selection, or the dial, at a command. */
static void position(uint16_t cmd) { send(ServoCmdKind::Position, cmd); }

/** @brief Every touch of the SERVO view. */
static void mainTouch(bool startOk) {
	bool run = s_st.running && live();

	// A drag under way at START or STOP ends there; the next input is a new
	// press.
	if (s_st.running != s_wasRunning) s_dragging = false;
	s_wasRunning = s_st.running;

	// --- HOLD TO START, or STOP on press. ui_hold times only a press that
	//     landed on the live button, and not again once it fired: the press
	//     that started the servo does not start it again after a trip, and
	//     one made before the claim was live does not start it at all ---
	if (uiHoldUpdate(&s_hold, BTN_START, s_t, startOk && !s_needLift, millis()))
		send(ServoCmdKind::Start);
	if (s_st.running && s_t->pressed && pressing(BTN_START)) {
		send(ServoCmdKind::Safe);
		s_dragging = false;
		// The press that stopped the servo does not start it again.
		s_needLift = true;
		return;
	}
	if (!s_t->down) s_needLift = false;

	// --- the dial ---
	float deg;
	if (s_t->pressed) {
		s_dragging = run && servoOnDial(s_t->x, s_t->y, &deg);
		if (s_dragging) {
			s_dragCmd = servoDegToCmd(&s_profile, deg);
			position(s_dragCmd);
		}
	} else if (s_t->down && s_dragging) {
		uint16_t cmd = servoDegToCmd(&s_profile, servoDialAngle(s_t->x, s_t->y));
		if (cmd != s_dragCmd) {
			s_dragCmd = cmd;
			position(cmd);
		}
	}
	if (!s_t->down) s_dragging = false;

	// --- SPEED, which commands nothing by itself ---
	if (uiSliderEvent(&s_speed, s_t) && s_st.running) send(ServoCmdKind::Slew);

	// --- taps, on release ---
	if (run && tapped(BTN_MIN))         position(0);
	else if (run && tapped(BTN_CENTRE)) position(SERVO_CMD_CENTRE);
	else if (run && tapped(BTN_MAX))    position(SERVO_SPAN);
	else if (run && tapped(BTN_SWEEP))  send(s_st.sweeping ? ServoCmdKind::SweepStop : ServoCmdKind::Sweep);
	else if (twoServos() && tapped(BTN_SEL_A))  s_mask = SERVO_MASK_A;
	else if (twoServos() && tapped(BTN_SEL_B))  s_mask = SERVO_MASK_B;
	else if (twoServos() && tapped(BTN_SEL_AB)) s_mask = SERVO_MASK_AB;
	else if (tapped(BTN_SET)) {
		s_view = View::Set;
		s_edit = s_profile;
		s_pending = false;
		s_sel = -1;
		uiListInit(&s_list, SET_VIEW, SET_ROW_PITCH, SET_ROWS);
		uiListSetBody(&s_list, SET_ROW_H, UI_LIST_TAP_MIN);
		s_full = true;
	}
}

// ---------------------------------------------------------------------------
// the SET view
// ---------------------------------------------------------------------------

/** @brief A row's value as SET shows it. */
static void valueText(int row, char *buf, size_t n) {
	const SetRow &r = ROWS[row];
	if (r.pulse) {
		const ServoProfile &p = s_edit;
		switch ((ServoField)r.field) {
			case ServoField::Servos:   snprintf(buf, n, "%s", p.servos == 2 ? "TWO" : "ONE"); return;
			case ServoField::Type:     snprintf(buf, n, "%s", servoTypeInfo(p.type)->name); return;
			case ServoField::Rate:     snprintf(buf, n, "%u HZ", p.rateHz); return;
			case ServoField::Min:      snprintf(buf, n, "%u US", p.minUs); return;
			case ServoField::Centre:   snprintf(buf, n, "%u US", p.centreUs); return;
			case ServoField::Max:      snprintf(buf, n, "%u US", p.maxUs); return;
			case ServoField::ReverseB: snprintf(buf, n, "%s", p.reverseB ? "ON" : "OFF"); return;
		}
	}
	static const char *const SHAPES[] = {"OFF", "SQUARE", "SINE", "TRIANGLE"};
	switch ((SweepField)r.field) {
		case SweepField::Shape:     snprintf(buf, n, "%s", SHAPES[s_sweep.kind]); return;
		case SweepField::Rate:      snprintf(buf, n, "%u.%02u HZ", s_sweep.mhz / 1000, s_sweep.mhz % 1000 / 10); return;
		case SweepField::Amplitude: snprintf(buf, n, "%u %%", s_sweep.amplitude * 100u / SWEEP_AMPLITUDE_MAX); return;
		case SweepField::Dwell:     snprintf(buf, n, "%u MS", s_sweep.dwell_ms); return;
		case SweepField::Moves:
			if (s_sweep.moves) snprintf(buf, n, "%u", s_sweep.moves);
			else               snprintf(buf, n, "ENDLESS");
			return;
	}
}

/**
 * @brief Whether a step of @p row in @p dir is allowed and changes something.
 */
static bool canStep(int row, int dir) {
	const SetRow &r = ROWS[row];
	if (!r.pulse) return servoSweepCanStep(&s_sweep, (SweepField)r.field, dir);
	if (s_st.running) return false;
	if (row == ROW_SERVOS && dir > 0 && s_auxRefusal != ClaimRefusal::None) return false;
	return servoProfileCanStep(&s_edit, (ServoField)r.field, dir);
}

/**
 * @brief One step of a row, while canStep() allows it.
 * @param ctx The row, an int.
 * @param dir -1 or +1.
 * @return true when the step was taken.
 */
static bool rowStep(void *ctx, int dir) {
	int row = *(const int *)ctx;
	const SetRow &r = ROWS[row];
	if (!canStep(row, dir)) return false;
	if (r.pulse) servoProfileStep(&s_edit, (ServoField)r.field, dir);
	else         servoSweepStep(&s_sweep, (SweepField)r.field, dir);
	return true;
}

/**
 * @brief Step @p row by @p steps coarse steps of @p factor steps each, as
 *        uiSwipeApply() does: one step at a time while canStep() allows it.
 *        A PULSE change applies at once, unless it is a TYPE or RATE that
 *        needs the warning, which waits for HOLD TO APPLY.
 */
static void stepRow(int row, int steps, int factor) {
	const SetRow &r = ROWS[row];
	if (uiSwipeApply(steps, factor, rowStep, &row) == 0 || !r.pulse) return;
	bool risky = servoDangerous(s_edit.type, s_edit.rateHz) &&
	             (s_edit.type != s_profile.type || s_edit.rateHz != s_profile.rateHz);
	s_pending = risky;
	if (!risky) {
		s_profile = s_edit;
		applyProfile();
	}
}

/** @brief The note band under the header: up to two lines. */
static void setNote(const char **l1, const char **l2, uint16_t *col) {
	*l1 = *l2 = nullptr;
	*col = C_AMBER;
	static char a[40];
	if (s_pending) {
		if (servoTypeInfo(s_edit.type)->heli) { *l1 = "A HELI PROFILE CAN DESTROY"; *l2 = "A SERVO NOT MADE FOR IT."; }
		else                                   { *l1 = "ABOVE 60 HZ AN ANALOGUE"; *l2 = "SERVO OVERHEATS."; }
		return;
	}
	if (s_st.running) { *l1 = "STOP TO CHANGE THE PROFILE"; return; }
	bool aux = s_auxRefusal != ClaimRefusal::None;
	if (aux && s_sel == ROW_SERVOS) {
		if (s_auxRefusal == ClaimRefusal::KissLive) {
			snprintf(a, sizeof(a), "KISS WIRE ON GP%u IS LIVE.", gpioOf(PIN_AUX));
			*l1 = a; *l2 = "TWO SERVOS NEED IT UNPLUGGED.";
		} else {
			*l1 = "ESC AND KISS PIN ARE THE SAME GPIO."; *l2 = "MOVE THE ESC PIN IN SETUP.";
		}
		return;
	}
	if (sweep_slew_limited(&s_sweep, servoSlewOf((int)(s_speed.value + 0.5f)))) {
		*l1 = "SPEED LIMITS THE SWEEP"; *col = C_DIM;
		return;
	}
	if (aux) { *l1 = "TWO SERVOS ARE NOT OFFERED: SEE SERVOS"; *col = C_DIM; }
}

/**
 * @brief Draw SET row @p i with its top edge at @p y.
 * @param i Row of the list.
 * @param y Top of the row.
 */
static void drawSetRow(int i, int y, void *) {
	bool sel = i == s_sel;
	bool locked = ROWS[i].pulse && s_st.running;
	gfxRect(0, y, GFX_W, SET_ROW_H - 1, sel ? C_PANEL : C_BG);
	gfxText(UI_MARGIN, y + 11, ROWS[i].group, C_GRID, 1);
	gfxText(UI_MARGIN, y + 21, ROWS[i].name, sel ? C_TEXT : C_DIM, 1);
	char v[24];
	valueText(i, v, sizeof(v));
	int sc = gfxTextW(v, 2) <= GFX_W - 80 ? 2 : 1;
	gfxText(GFX_W - UI_MARGIN - gfxTextW(v, sc), y + (SET_ROW_H - 1 - 7 * sc + 1) / 2, v,
	        locked ? C_GRID : (sel ? C_LIME : C_TEXT), sc);
	gfxHLine(0, y + SET_ROW_H - 1, GFX_W, C_PANEL);
}

/** @brief Header, note band, list, editor bar and footer of SET. */
static void drawSet(int holdPct) {
	const char *l1, *l2;
	uint16_t col;
	setNote(&l1, &l2, &col);
	struct { bool back; uint32_t l1, l2; } kt;
	memset(&kt, 0, sizeof(kt));
	kt.back = uiPressing(uiBackRect(), s_t); kt.l1 = textKey(l1); kt.l2 = textKey(l2);
	if (changed(0, &kt, sizeof(kt))) {
		uiHeader("SERVO SET", s_t);
		gfxRect(0, UI_STRIP_Y, GFX_W, SET_LIST_Y0 - UI_STRIP_Y, C_BG);
		if (l1) gfxText(UI_MARGIN, UI_STRIP_Y + 2, l1, col, 1);
		if (l2) gfxText(UI_MARGIN, UI_STRIP_Y + 12, l2, col, 1);
	}

	struct { ServoProfile p; sweep_cfg_t w; int sel, scroll; bool run; } kl;
	memset(&kl, 0, sizeof(kl));
	kl.p = s_edit; kl.w = s_sweep; kl.sel = s_sel; kl.scroll = s_list.scrollPx; kl.run = s_st.running;
	if (changed(1, &kl, sizeof(kl))) {
		uiListDraw(&s_list, drawSetRow, nullptr);
	}

	struct { int sel; ServoProfile p; sweep_cfg_t w; bool m, pl, cm, cp; } ke;
	memset(&ke, 0, sizeof(ke));
	ke.sel = s_sel; ke.p = s_edit; ke.w = s_sweep;
	if (s_sel >= 0) {
		ke.cm = canStep(s_sel, -1); ke.cp = canStep(s_sel, +1);
		ke.m = ke.cm && pressing(BTN_MINUS); ke.pl = ke.cp && pressing(BTN_PLUS);
	}
	if (changed(2, &ke, sizeof(ke))) {
		gfxRect(0, SET_LIST_Y1 + 1, GFX_W, SET_FOOT_Y - SET_LIST_Y1 - 1, C_BG);
		if (s_sel < 0) {
			gfxTextCenter(SET_EDIT_Y + 12, "TAP A ROW TO EDIT IT", C_GRID, 1);
			gfxTextCenter(SET_EDIT_Y + 26, "SWIPE IT SIDEWAYS FOR COARSE", C_GRID, 1);
		} else {
			uiButton(BTN_MINUS, "-", C_GRID, C_ONACCENT, 2, ke.m, ke.cm);
			uiButton(BTN_PLUS, "+", C_GRID, C_ONACCENT, 2, ke.pl, ke.cp);
			int cx = BTN_MINUS.x + BTN_MINUS.w, cw = BTN_PLUS.x - cx;
			const char *name = ROWS[s_sel].name;
			gfxText(cx + (cw - gfxTextW(name, 1)) / 2, SET_EDIT_Y + 2, name, C_DIM, 1);
			char v[24];
			valueText(s_sel, v, sizeof(v));
			int sc = gfxTextW(v, 2) <= cw ? 2 : 1;
			gfxText(cx + (cw - gfxTextW(v, sc)) / 2, SET_EDIT_Y + 16, v, C_LIME, sc);
			gfxText(cx + (cw - gfxTextW("SWIPE = COARSE", 1)) / 2, SET_EDIT_Y + 34,
			        "SWIPE = COARSE", C_GRID, 1);
		}
	}

	// While RUNNING the footer's right button is STOP, as on SERVO; DEFAULTS
	// would change the PULSE rows, which RUNNING locks.
	bool applyOk = s_pending && !s_st.running;
	struct { bool ok, run, holding, dp; int pct; } kf;
	memset(&kf, 0, sizeof(kf));
	kf.ok = applyOk; kf.run = s_st.running; kf.holding = s_hold.holding; kf.pct = holdPct;
	kf.dp = pressing(BTN_DEFAULTS);
	if (changed(3, &kf, sizeof(kf))) {
		gfxRect(0, SET_FOOT_Y - 4, GFX_W, GFX_H - SET_FOOT_Y + 4, C_BG);
		uiHoldDrawBar(&s_hold, BTN_APPLY, SET_FOOT_Y - 4, millis());
		uiButton(BTN_APPLY, "HOLD TO APPLY", s_hold.holding ? C_RED : C_PANEL,
		         s_hold.holding ? C_ONACCENT : C_TEXT, 1, s_hold.holding, applyOk);
		if (s_st.running) uiButton(BTN_DEFAULTS, "STOP", C_RED, C_ONACCENT, 2, kf.dp);
		else              uiButton(BTN_DEFAULTS, "DEFAULTS", C_PANEL, C_AMBER, 1, kf.dp);
	}
}

/**
 * @brief List gestures, on ui_list: a vertical drag scrolls, a sideways swipe
 *        selects the row it began on and steps it coarseOf() steps per
 *        16 px, a press that goes nowhere selects.
 */
static void setGesture() {
	UiListEvent e = uiListTouch(&s_list, s_t);
	if (e.kind != UiListEv::Tap && e.kind != UiListEv::Swipe) return;
	s_sel = e.row;
	if (e.steps) stepRow(s_sel, e.steps, coarseOf(ROWS[s_sel]));
}

/** @brief Every touch of the SET view. */
static void setTouch() {
	if (uiHoldUpdate(&s_hold, BTN_APPLY, s_t, s_pending && !s_st.running, millis())) {
		s_pending = false;
		s_profile = s_edit;
		applyProfile();
	}

	int dir = 0;
	if (s_sel >= 0 && pressing(BTN_MINUS)) dir = -1;
	else if (s_sel >= 0 && pressing(BTN_PLUS)) dir = +1;
	if (dir && repeatFires(&s_repeat, dir, millis())) stepRow(s_sel, dir, 1);
	if (!dir) repeatFires(&s_repeat, 0, millis());

	// STOP on press while RUNNING, as on SERVO. The press that stops the servo
	// is not also a tap on DEFAULTS when the button changes under it.
	if (s_st.running && s_t->pressed && pressing(BTN_DEFAULTS)) {
		send(ServoCmdKind::Safe);
		s_needLift = true;
	} else if (!s_st.running && !s_needLift && tapped(BTN_DEFAULTS)) {
		servoProfileDefault(&s_edit);
		s_profile = s_edit;
		s_pending = false;
		servoSweepDefault(&s_sweep);
		applyProfile();
	}
	if (!s_t->down) s_needLift = false;
	// The list's own gestures. A press on the editor bar or the footer lies
	// outside the list, so it names no row and selects nothing.
	setGesture();
}

// ---------------------------------------------------------------------------
// the unplug page
// ---------------------------------------------------------------------------

static void drawUnplug() {
	bool due = pinClaimUnplugDue();
	struct { bool back, due, p; } k;
	memset(&k, 0, sizeof(k));
	k.back = uiPressing(uiBackRect(), s_t); k.due = due; k.p = due && pressing(BTN_RESTART);
	if (!changed(0, &k, sizeof(k))) return;
	uiHeader("SERVO", s_t);
	gfxRect(0, UI_STRIP_Y, GFX_W, GFX_H - UI_STRIP_Y, C_BG);
	if (!due) { line(120, "RELEASING THE PINS", C_DIM); return; }
	gfxTextCenter(100, "UNPLUG THE SERVO.", C_AMBER, 2);
	char b[48];
	snprintf(b, sizeof(b), "DSHOT RESTARTS ON GP%u AND READS AS", gpioOf(PIN_SIG));
	line(140, b, C_TEXT);
	line(154, "A 970 US PULSE AT 1000 HZ.", C_TEXT);
	uiButton(BTN_RESTART, "RESTART DSHOT", C_PANEL, C_TEXT, 1, k.p);
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

bool uiServoAvailable() { return true; }

uint8_t uiServoControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	if (s_view == View::Unplug) {
		bool due = pinClaimUnplugDue();
		uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), due);
		if (due) uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Restart), BTN_RESTART, true);
		return n;
	}
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), true);
	if (s_view == View::Set) {
		uiListAddRows(&s_list, out, &n, max, true);
		if (s_sel >= 0) {
			uiControlAdd(out, &n, max, UI_FIX(ServoCtl::EditMinus), BTN_MINUS, canStep(s_sel, -1));
			uiControlAdd(out, &n, max, UI_FIX(ServoCtl::EditPlus), BTN_PLUS, canStep(s_sel, +1));
		}
		uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Apply), BTN_APPLY, s_pending && !s_st.running);
		uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Defaults), BTN_DEFAULTS, true);
		return n;
	}
	const char *l1, *l2, *l3;
	uint16_t col;
	bool retry;
	claimMessage(&l1, &l2, &l3, &col, &retry);
	bool run = s_st.running && live();
	// A message replaces the dial in its band (drawDial()), and RETRY sits
	// in that band, so the dial is a control only while no message shows.
	if (retry) uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Retry), BTN_RETRY, true);
	if (!l1)
		uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Dial),
		             UiRect{ 0, SERVO_DIAL_Y0, GFX_W, SERVO_DIAL_Y1 - SERVO_DIAL_Y0 + 1 }, run);
	if (twoServos()) {
		uiControlAdd(out, &n, max, UI_FIX(ServoCtl::SelA), BTN_SEL_A, true);
		uiControlAdd(out, &n, max, UI_FIX(ServoCtl::SelB), BTN_SEL_B, true);
		uiControlAdd(out, &n, max, UI_FIX(ServoCtl::SelAB), BTN_SEL_AB, true);
	}
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Speed), SPEED_TRACK, true);
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Min), BTN_MIN, run);
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Centre), BTN_CENTRE, run);
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Max), BTN_MAX, run);
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Sweep), BTN_SWEEP, run);
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Start), BTN_START,
	             s_st.running || live());
	uiControlAdd(out, &n, max, UI_FIX(ServoCtl::Set), BTN_SET, true);
	return n;
}

void uiServoEnter() {
	servoJobNewSession();
	s_view = View::Main;
	s_claimed = false;
	s_refusal = ClaimRefusal::None;
	s_auxRefusal = ClaimRefusal::None;
	servoProfileDefault(&s_profile);
	s_edit = s_profile;
	s_pending = false;
	servoSweepDefault(&s_sweep);
	uiSliderInit(&s_speed, SPEED_TRACK, SERVO_SPEED_MIN_PCT, SERVO_SPEED_MAX_PCT);
	uiSliderSetTapToSet(&s_speed, true);
	uiSliderSet(&s_speed, SERVO_SPEED_MAX_PCT);
	s_mask = SERVO_MASK_AB;
	s_dragging = false;
	s_wasRunning = false;
	uiHoldReset(&s_hold);
	s_needLift = false;
	s_sel = -1;
	uiListInit(&s_list, SET_VIEW, SET_ROW_PITCH, SET_ROWS);
	uiListSetBody(&s_list, SET_ROW_H, UI_LIST_TAP_MIN);
	s_repeat = Repeat{};
	s_full = true;
	applyProfile();
	claimPins();
}

bool uiServoTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	servoJobStatus(&s_st);
	if (s_owed) applyProfile();
	if (s_claimed) pinClaimPoll();
	else if (s_view != View::Unplug) claimPins();

	// BACK: from SET to SERVO, dropping a pending change; from the unplug
	// page back to SERVO, keeping the claim, once the job has let go (until
	// then pinClaimKeep() refuses, and BACK there does nothing); from SERVO
	// out, through the unplug page once a job has held the pins.
	bool back = uiBackTapped(s_t);
	if (back && s_view == View::Unplug && !pinClaimUnplugDue()) back = false;
	if (back) {
		s_full = true;
		if (s_view == View::Set) {
			s_view = View::Main;
			s_edit = s_profile;
			s_pending = false;
		} else if (s_view == View::Unplug) {
			pinClaimKeep(PinUser::Servo);
			s_view = View::Main;
		} else {
			uiSliderRelease(&s_speed);
			if (!s_claimed) return false;
			pinClaimEnd(PinUser::Servo);
			ClaimState cs = pinClaimState();
			if (cs == ClaimState::Free || cs == ClaimState::Restoring) return false;
			s_view = View::Unplug;
		}
	}

	if (s_view == View::Unplug) {
		if (pinClaimUnplugDue() && tapped(BTN_RESTART)) {
			pinClaimRestart(PinUser::Servo);
			return false;
		}
		if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
		drawUnplug();
		s_full = false;
		return true;
	}

	bool startOk = live() && !s_st.running;
	if (s_view == View::Main) {
		const char *l1, *l2, *l3;
		uint16_t col;
		bool retry;
		claimMessage(&l1, &l2, &l3, &col, &retry);
		if (retry && tapped(BTN_RETRY)) {
			if (pinClaimJobError()) pinClaimRetry(PinUser::Servo);
			else                    applyProfile();
		}
		mainTouch(startOk);
	} else {
		setTouch();
	}

	int pct = uiHoldPercent(&s_hold, millis());
	// A view's regions do not cover the rows between them, so a new view
	// starts from a clear panel.
	if (s_full) gfxRect(0, 0, GFX_W, GFX_H, C_BG);
	if (s_view == View::Main) {
		startOk = live() && !s_st.running;
		drawTop("SERVO", true);
		drawDial();
		drawReadout();
		drawSpeed();
		drawButtons(startOk, pct);
	} else {
		drawSet(pct);
	}
	s_full = false;
	return true;
}

/** @brief SERVO TESTER's MENU status: the signal pins it claims, SIG and AUX. */
static void servoMenuStatus(char *buf, size_t n, uint16_t *col, uint16_t *) {
	snprintf(buf, n, "GP%u+%u", (unsigned)escTaskDshotPin(), (unsigned)escTaskKissPin());
	*col = C_DIM;
}

const UiScreen UI_SERVO = {
	UiScreenId::Servo, "SERVO", PinUser::Servo, servoMenuStatus,
	uiServoEnter, uiServoTick, nullptr, uiServoControls, nullptr,
};
