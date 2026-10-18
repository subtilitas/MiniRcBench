/**
 * @file ui.cpp
 * @brief Screen layout, touch handling and the arm/throttle state machines.
 *
 * Each screen region caches the values it last displayed and skips drawing
 * entirely when nothing changed, so a steady screen costs almost no SPI
 * traffic. Any state that affects what a region renders must therefore appear
 * in that region's cache key, or the region will not repaint when it changes.
 */

#include "ui.h"
#include "ui_am32.h"
#include "ui_kst.h"
#include "ui_servo.h"
#include "ui_menu.h"
#include "ui_sdlog.h"
#include "ui_settings.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "ui_screen.h"
#include "settings.h"
#include "gfx.h"
#include "touch.h"
#include "st7789.h"
#include "esc_task.h"
#include "esc_merge.h"
#include "rpm_filter.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "board_desc.h"
#include "config.h"

#include "plat.h"
#include <hardware/adc.h>
#include <stdio.h>
#include <string.h>

// The screen layout is portrait-only: every region below is a fixed pixel row
// in a 240x320 frame. Setting LCD_ROTATION to 1 or 3 gives a 320x240 frame, in
// which the buttons fall off the bottom. That used to compile happily and fail
// silently on the panel; now it does not compile.
static_assert(GFX_W == 240 && GFX_H == 320,
              "the UI layout assumes portrait; LCD_ROTATION must be 0 or 2");

/**
 * @defgroup ui_layout Layout (240x320 portrait)
 * @brief Vertical extents of each screen region, in framebuffer rows.
 * @{
 */
// The regions tile the panel exactly: each ends where the next begins. Gaps
// here are rows no region ever repaints, so whatever the previous screen left
// in them survives. The screen-change clears already keep those rows blank in
// practice; the asserts make the property structural rather than coincidental,
// so the next layout edit cannot quietly reopen a one-row leak.
#define Z_STATUS_Y0   0
#define Z_STATUS_Y1   26
#define Z_RPM_Y0      27
#define Z_RPM_Y1      127
#define Z_TELE_Y0     128
#define Z_TELE_Y1     233
#define Z_THR_Y0      234
#define Z_THR_Y1      281
#define Z_BTN_Y0      282
#define Z_BTN_Y1      319

static_assert(Z_STATUS_Y0 == 0, "regions must start at the top of the panel");
static_assert(Z_RPM_Y0  == Z_STATUS_Y1 + 1, "gap between status and RPM");
static_assert(Z_TELE_Y0 == Z_RPM_Y1 + 1,    "gap between RPM and telemetry");
static_assert(Z_THR_Y0  == Z_TELE_Y1 + 1,   "gap between telemetry and throttle");
static_assert(Z_BTN_Y0  == Z_THR_Y1 + 1,    "gap between throttle and buttons");
static_assert(Z_BTN_Y1  == GFX_H - 1,       "regions must reach the bottom");

#define THR_TRACK_X   8
#define THR_TRACK_Y   248
#define THR_TRACK_W   224
/**
 * @brief Track height.
 *
 * 26 px until a report that a powerful setup with the brake on gets a real jolt
 * when the slider is knocked back to zero, and that a taller bar is both harder
 * to miss and harder to fall off the bottom of into DISARM.
 *
 * The 8 px came out of the padding above the button row -- the buttons are
 * still at @ref BTN_ROW_Y, and the RPM digits and telemetry rows above are
 * untouched. What it costs is the slop below the track: see
 * @ref THR_TOUCH_PAD_BOT.
 */
#define THR_TRACK_H   34
/**
 * @brief Vertical grab slop above the track.
 *
 * Slop is what lets a finger that lands just off the bar still drive it. Above
 * the track there is room for a fingertip's worth of it: the row above holds
 * the caption, and the throttle pad's own region (PAD_Y1) stops short of it to
 * make space.
 */
#define THR_TOUCH_PAD_TOP 8
/**
 * @brief Vertical grab slop below the track, which is not the same number.
 *
 * The bar ends three pixels above the button row, so there is no room for a
 * matching margin here: slop that reached into DISARM would make the top of
 * that button drive the throttle instead of pressing it -- the accident issue
 * #24 asks for protection from, arriving by the other road. The static_assert
 * beside @ref BTN_ROW_Y is what keeps this true if either number moves again.
 */
#define THR_TOUCH_PAD_BOT 1

/**
 * @brief Track columns at each end that count as the rail rather than as a
 *        position on it.
 *
 * The last few columns of the panel are not reachable with a finger: the case
 * rim overhangs the glass, and the fingertip stops short of the edge even when
 * the touch controller would happily report it. A slider whose ends exist only
 * in theory cannot be set to zero or to the ceiling, which is what a 2" board
 * reported as a 20 % ceiling that would only reach 19 %.
 *
 * So the ends saturate: @ref THR_USABLE_W is what maps onto 0..ceiling, and
 * everything outside it is the rail it is nearest. The drawn track keeps its
 * full width, so the bar still fills to its visible end -- it just gets there
 * while the finger is still clear of the rim.
 *
 * 16 px is a little over 7 % of the track, and comfortably more than the ~11 px
 * that report was short by.
 */
#define THR_EDGE_SAT  16
/** @brief How far the HOLD-mode grab handle stands above the track. */
#define THR_HANDLE_OVER 3
static_assert(THR_TRACK_Y - THR_HANDLE_OVER >= Z_THR_Y0,
              "the HOLD handle is drawn above the throttle band");
/**
 * @brief The part of the track that is a position rather than a rail.
 *
 * THR_TRACK_W less @ref THR_EDGE_SAT at each end. This is what maps onto
 * 0..ceiling; the drawn track stays its full width.
 */
#define THR_USABLE_W  (THR_TRACK_W - 2 * THR_EDGE_SAT)
static_assert(THR_USABLE_W > 0, "the saturation margins have eaten the track");
static_assert(THR_TRACK_Y + THR_TRACK_H - 1 <= Z_THR_Y1,
              "the track overflows the throttle band");
static_assert(THR_EDGE_SAT * 2 < THR_TRACK_W / 2,
              "saturating half the track would make it a two-position switch");

// The number-display area doubles as a large relative throttle pad. It runs
// from the top of the RPM readout down to where the gauge's grab area starts,
// so the two regions abut exactly: no overlap, no dead strip between them.
#define PAD_Y0        Z_RPM_Y0
#define PAD_Y1        (THR_TRACK_Y - THR_TOUCH_PAD_TOP - 1)
#define PAD_H         (PAD_Y1 - PAD_Y0 + 1)

/** @} */

// The main screen is the root, so it has no header and no BACK: there is
// nowhere to go back to, and a disabled or missing control in the place the
// habit says to look is worse than the habit not applying here. MENU is its
// navigation, and it sits with the other two actions.
#define BTN_ROW_Y     283   /**< Top of the three main-screen buttons. */
#define BTN_ROW_H      33   /**< ...and their height. */
static const UiRect BTN_ARM  = { 6, BTN_ROW_Y, 104, BTN_ROW_H };
static const UiRect BTN_HOLD = { 116, BTN_ROW_Y, 54, BTN_ROW_H };
static const UiRect BTN_MENU = { 176, BTN_ROW_Y, 58, BTN_ROW_H };

// The grab area may not reach the buttons. Written where both numbers are in
// scope rather than as a comment on either one, because it is a relationship
// between them and a comment is not checked.
static_assert(THR_TRACK_Y + THR_TRACK_H + THR_TOUCH_PAD_BOT <= BTN_ROW_Y,
              "the throttle grab area reaches into the button row");
static_assert(BTN_ROW_Y + BTN_ROW_H - 1 <= Z_BTN_Y1,
              "the buttons run off the bottom of the panel");


// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------
static bool     s_armed = false;
static bool     s_hold = false;
/** @brief Which level of the navigation owns the panel. */
enum class Level : uint8_t {
	Tester,  /**< The tester, the root. */
	Menu,    /**< MENU. */
	Tool,    /**< A tool opened from MENU: `s_tool`. */
};
static Level           s_level = Level::Tester;
static const UiScreen *s_tool = nullptr;  /**< The open tool at Level::Tool. */
static uint16_t s_throttle = 0;

// Pole count and throttle ceiling live in the persisted settings rather than in
// file statics here, so the -/+ buttons and the HOLD TO SAVE on SETTINGS act
// on one value. These two read through to it.
#define s_maxThrottle (settings()->maxThrottle)
#define s_poles       (settings()->poles)

static bool     s_armPressing = false;
static uint32_t s_armPressStart = 0;
/**
 * @brief The press on ARM landed on it while arming was allowed: disarmed,
 *        and no USB host holding the card. Only such a press is timed, so the
 *        press that disarms, or one made while arming was refused, does not
 *        arm when it is kept on the button. Cleared on a lift and on arming.
 */
static bool     s_armLive = false;
static uint32_t s_zeroSince = 0;
static uint32_t s_lastTouchMs = 0;
static bool     s_dragging = false;
// anchor for HOLD-mode relative dragging on the gauge
static int16_t  s_dragAnchorX = 0;
static uint16_t s_dragAnchorThrottle = 0;

// relative throttle pad (the number-display area)

static bool     s_padDragging = false;
static bool     s_padEngaged = false;    // deadzone cleared
static int16_t  s_padTouchY = 0;         // where the finger landed
static int16_t  s_padAnchorPos = 0;      // negated y, so bigger = higher up
static uint16_t s_padAnchorThrottle = 0;


/**
 * @defgroup ui_blflash Arm feedback through the backlight
 *
 * Arming and disarming are the two most consequential things this UI does, and
 * both used to announce themselves only by a badge changing colour -- in a
 * corner of a screen you are not looking at, because you are looking at the
 * motor.
 *
 * Dipping the whole panel for a moment is a peripheral-vision event. It costs
 * one PWM write per frame, needs no extra hardware, and works
 * identically in both themes.
 * @{
 */
/** @brief How long the panel dims for on an arm-state change, in ms. */
#define BL_DIP_MS 120
/** @brief Fraction of the normal level to dip to, as a divisor. */
#define BL_DIP_DIV 4

static uint32_t s_blDipUntil = 0;

/** @brief Start a dip. Called on every arm and disarm, however it happened. */
static void backlightDip() { s_blDipUntil = millis() + BL_DIP_MS; }
/** @} */

static TouchState s_touch;
static EscTelemetry s_tel;

/**
 * @brief Smoothing for the two speed readouts. @see rpm_filter.h
 *
 * One filter, on eRPM, with RPM derived from its output rather than filtered
 * separately. Two filters would drift apart under acceleration and put a pole
 * count's worth of disagreement between two numbers sitting one above the
 * other, which is a thing people notice and reasonably report as a bug.
 */
static RpmFilter s_erpmFilter;
/** @brief Filtered eRPM, stepped once per uiTick() and read by drawRpm(). */
static uint32_t  s_erpmShown = 0;
static float s_batteryV = 0.0f;

/**
 * @brief Last-rendered values, so a region is only repainted when one changes.
 *
 * Several regions display overlapping state, so they keep separate copies —
 * `armed` and `btnArmed` for instance — because whichever region draws first
 * would otherwise consume the change and leave the others stale.
 */
static struct {
	int  armed, btnArmed, holdOn, armProgress;
	int  battMv;
	uint32_t rpm, erpm;
	int  linkAlive, rpmArmed;
	int  volts100, amps100, tempC, stress, status, rate, errPct, edt;
	int  kbaud, pumpErr;
	int  voltsSrc, ampsSrc, mah;
	int  throttleRaw, maxPct, thrArmed, thrHold;
	int  config, poles;
	int  cmdFlash, edtActive, dirty, btnPress, idleLeft, padLive;
	int  logState, logFile, logDrops;
	uint32_t logBytes, logFrames, logPeak, logWorstMs;
	int  mscState, mscNote;
	uint32_t mscBlocks;
} s_shown;

/** @brief Force every region to repaint on the next uiTick(). */
static void invalidateAll() { memset(&s_shown, 0xFF, sizeof(s_shown)); }

// drawBtn(), hit(), pressing() and tapped() used to live here, and near-copies
// of all four lived in ui_settings.cpp and ui_am32.cpp. They are uiButton(),
// uiPressing() and uiTapped() now. @see ui_widgets.h
//
// These two adapt the shared helpers to this file's habit of passing the
// TouchState by reference, which is worth keeping: every call site here already
// has s_touch in scope and none of them can pass null.
/** @brief True while @p b is touched by a press that began inside it. */
static bool pressing(const UiRect &b, const TouchState &t) {
	return uiPressing(b, &t);
}

/** @brief True on the frame a tap of @p b completes. */
static bool tapped(const UiRect &b, const TouchState &t) {
	return uiTapped(b, &t);
}

/** @brief Draw one telemetry tile: dim caption above a larger value. */
static void drawLabelled(int x, int y, int w, int h, const char *label,
                         const char *value, uint16_t vcol) {
	gfxRect(x, y, w, h, C_PANEL);
	gfxText(x + 6, y + 4, label, C_DIM, 1);
	gfxText(x + 6, y + 15, value, vcol, 2);
}

/**
 * @brief Right-aligned "KISS" / "EDT" tag on a tile's label row.
 *
 * Cyan for KISS, dim for EDT: the point is to make the *fine* source obvious at
 * a glance, so that a readout quietly dropping back to 0.25 V steps is visible
 * rather than something you notice later in a log.
 *
 * @param xRight Right edge of the tile.
 * @param y      Tile top.
 * @param src    Which source supplied the value.
 */
static void drawSourceTag(int xRight, int y, EscSource src) {
	if (src == EscSource::None) return;
	const char *s = escSourceLabel(src);
	gfxText(xRight - 6 - gfxTextW(s, 1), y + 4, s,
	        src == EscSource::Kiss ? C_CYAN : C_DIM, 1);
}

/**
 * @brief Move the throttle by how far the finger has travelled from its anchor.
 *
 * Re-anchoring at the rails is what stops the classic relative-drag failure:
 * swipe hard past an end and, without it, the anchor keeps accumulating travel
 * the throttle cannot follow, so reversing direction does nothing until all of
 * that phantom travel has been unwound.
 *
 * @param         nowPos    Current finger position along the control's axis.
 *                          Negate screen Y so that larger means further up.
 * @param[in,out] anchorPos Position the drag is measured from. Reset at a rail.
 * @param[in,out] anchorThr Throttle the drag is measured from. Reset at a rail.
 * @param         spanPx    Travel covering the whole ceiling at 100 % sensitivity.
 * @param         sensPct   Sensitivity percentage.
 */
static void relativeThrottle(int nowPos, int16_t *anchorPos,
                             uint16_t *anchorThr, int spanPx, int sensPct) {
	int32_t delta = (int32_t)(nowPos - *anchorPos) * (int32_t)s_maxThrottle / spanPx;
	delta = delta * sensPct / 100;
	int32_t t = (int32_t)(*anchorThr) + delta;

	if (t <= 0) {
		t = 0;
		*anchorPos = (int16_t)nowPos;
		*anchorThr = 0;
	} else if (t >= (int32_t)s_maxThrottle) {
		t = s_maxThrottle;
		*anchorPos = (int16_t)nowPos;
		*anchorThr = s_maxThrottle;
	}
	s_throttle = (uint16_t)t;
}

/** @brief True if an eRPM frame has arrived recently. @see ESC_LINK_STALE_MS */
static bool telemetryAlive() {
	return escFieldFresh(s_tel.lastRpmMs, millis(), ESC_LINK_STALE_MS);
}

/**
 * @defgroup ui_regions Screen regions
 * @brief Each returns immediately if its cached values are unchanged.
 * @{
 */

/** @brief Arm badge, DShot rate, EDT indicator and pack voltage. */
static void drawStatusBar() {
	int progress = 0;
	if (s_armPressing) {
		uint32_t held = millis() - s_armPressStart;
		progress = (int)(held * 100 / ARM_HOLD_MS);
		if (progress > 100) progress = 100;
	}
	int battMv = (int)(s_batteryV * 100.0f);

	SdLogStatus log;
	sdLogStatus(&log);

	// Seconds left before the idle interlock disarms, or -1 when it is not
	// imminent. Counting down out loud matters: a motor stopping for no visible
	// reason is indistinguishable from a fault, and this interlock has no other
	// symptom at all.
	int idleLeft = -1;
#if IDLE_DISARM_MS > 0
	if (s_armed) {
		int32_t left = (int32_t)IDLE_DISARM_MS - (int32_t)(millis() - s_lastTouchMs);
		if (left < 0) left = 0;
		if (left <= 5000) idleLeft = (int)((left + 999) / 1000);
	}
#endif

	// The bitrate the pump is running at, not the compiled default: SETTINGS
	// changes it, and the label follows.
	int kbaud = (int)escTaskDshotKbaud();
	int pumpErr = s_tel.initError ? 1 : 0;

	if (s_shown.armed == (int)s_armed && s_shown.armProgress == progress &&
	    s_shown.battMv == battMv && s_shown.edt == (int)escEdtRequested() &&
	    s_shown.logState == (int)log.state && s_shown.idleLeft == idleLeft &&
	    s_shown.kbaud == kbaud && s_shown.pumpErr == pumpErr)
		return;

	s_shown.kbaud = kbaud;
	s_shown.pumpErr = pumpErr;
	s_shown.idleLeft = idleLeft;
	s_shown.armed = s_armed;
	s_shown.armProgress = progress;
	s_shown.battMv = battMv;
	s_shown.edt = escEdtRequested();
	s_shown.logState = (int)log.state;

	gfxRect(0, Z_STATUS_Y0, GFX_W, Z_STATUS_Y1 - Z_STATUS_Y0 + 1, C_PANEL);

	uint16_t badge = s_armed ? C_RED : C_GREEN;
	gfxRoundRect(4, 3, 92, 20, 4, badge);
	const char *txt = s_armed ? "ARMED" : "SAFE";
	gfxText(4 + (92 - gfxTextW(txt, 2)) / 2, 7, txt, C_ONACCENT, 2);

	if (!s_armed && progress > 0) {
		gfxRect(4, 21, 92 * progress / 100, 2, C_LIME);
	}

	// DSERR when the driver refused the pin: no PIO block could take it, so
	// nothing is on the wire at any rate. @see EscTelemetry::initError
	char buf[24];
	if (pumpErr) {
		gfxText(104, 3, "DSERR", C_AMBER, 1);
	} else {
		snprintf(buf, sizeof(buf), "DS%d", kbaud);
		gfxText(104, 3, buf, escEdtRequested() ? C_CYAN : C_DIM, 1);
	}

	// Recording state, under the DShot rate. Only worth a glance -- the detail
	// lives on the LOG screen -- but "am I actually recording?" is the one
	// question you want answered without navigating anywhere.
	const char *lg;
	uint16_t lgCol;
	switch (log.state) {
		case SdLogState::Logging: lg = "REC";   lgCol = C_RED;   break;
		case SdLogState::Idle:    lg = "SD";    lgCol = C_DIM;   break;
		case SdLogState::Error:   lg = "SDERR"; lgCol = C_AMBER; break;
		default:                  lg = "NOSD";  lgCol = C_GRID;  break;
	}
	gfxText(104, 14, lg, lgCol, 1);

	if (idleLeft >= 0) {
		// Displaces the pack voltage rather than squeezing in beside it: for the
		// five seconds this is up it is the more urgent of the two, and there is
		// not room for both at a legible size.
		snprintf(buf, sizeof(buf), "IDLE %ds", idleLeft);
		gfxText(GFX_W - 4 - gfxTextW(buf, 2), 7, buf, C_AMBER, 2);
	} else {
		snprintf(buf, sizeof(buf), "%d.%02dV", (int)s_batteryV,
		         (int)((s_batteryV - (int)s_batteryV) * 100.0f + 0.5f));
		gfxText(GFX_W - 4 - gfxTextW(buf, 2), 7, buf, C_TEXT, 2);
	}
}

/** @brief Big seven-segment RPM readout, eRPM line and the pad affordance. */
static void drawRpm() {
	bool alive = telemetryAlive();
	// Both from the one filtered figure, so the big number and the eRPM line
	// under it can never disagree. The pole conversion is the same one core1
	// does; doing it here rather than reading s_tel.rpm is what keeps them tied.
	uint32_t erpm = alive ? s_erpmShown : 0;
	uint32_t rpm  = (alive && s_poles >= 2) ? erpm / (s_poles / 2) : 0;
	if (rpm > 99999) rpm = 99999;

	if (s_shown.rpm == rpm && s_shown.erpm == erpm &&
	    s_shown.linkAlive == (int)alive && s_shown.rpmArmed == (int)s_armed &&
	    s_shown.padLive == (int)s_padEngaged)
		return;
	s_shown.padLive = s_padEngaged;
	s_shown.rpm = rpm;
	s_shown.erpm = erpm;
	s_shown.linkAlive = alive;
	s_shown.rpmArmed = s_armed;

	gfxRect(0, Z_RPM_Y0, GFX_W, Z_RPM_Y1 - Z_RPM_Y0 + 1, C_BG);

	uint16_t on = alive ? C_LIME : C_REDDARK;
	gfxSegNumber(226, 32, 36, 62, themeSegStroke(8), 6, rpm, 5, on, C_GHOST);

	gfxText(226 - gfxTextW("RPM", 2), 100, "RPM", C_DIM, 2);

	char buf[32];
	if (alive) snprintf(buf, sizeof(buf), "ERPM %lu", (unsigned long)erpm);
	else       snprintf(buf, sizeof(buf), "NO TELEMETRY");
	gfxText(10, 102, buf, alive ? C_DIM : C_AMBER, 1);

	snprintf(buf, sizeof(buf), "%dP", s_poles);
	gfxText(10, 114, buf, C_DIM, 1);

	// Tell the user this whole region is a throttle pad -- a relative control
	// with no visible handle is invisible otherwise.
	// Three states, not two. The pad has a deadzone, so there is a moment
	// between touching it and it taking effect, and the only other evidence a
	// drag engaged is the throttle number moving -- which is exactly what you
	// are not watching while a motor spins up.
	const char *hint = s_padEngaged ? "SWIPE ACTIVE" : "SWIPE = THROTTLE";
	gfxText(226 - gfxTextW(hint, 1), 114, hint,
	        s_padEngaged ? C_LIME : s_armed ? C_CYAN : C_GRID, 1);
}

/** @brief Six EDT tiles: voltage, current, temperature, stress, status, link. */
static void drawTelemetry() {
	// Merged view: KISS where it is fresh, EDT otherwise, decided per field.
	EscReading r;
	escMerge(&s_tel, millis(), KISS_STALE_MS, EDT_STALE_MS, &r);

	// Hundredths, not tenths. KISS resolves 0.01 V and 0.01 A, and rounding the
	// display to 0.1 would throw away exactly the precision the extra wire was
	// run for.
	int volts100 = (int)(r.volts * 100.0f + 0.5f);
	int amps100  = (int)(r.amps * 100.0f + 0.5f);
	// -1 is "no status block", not "OK". An expired status must not read as
	// all-clear -- that is the one direction a warning indicator may not fail.
	int status   = (r.statusFrom == EscSource::None) ? -1
	             : r.alert ? 3 : r.error ? 2 : r.warning ? 1 : 0;

	if (s_shown.volts100 == volts100 && s_shown.amps100 == amps100 &&
	    s_shown.tempC == r.tempC && s_shown.stress == (int)r.stress &&
	    s_shown.status == status && s_shown.rate == s_tel.packetRate &&
	    s_shown.errPct == s_tel.errPercent &&
	    s_shown.voltsSrc == (int)r.voltsFrom && s_shown.ampsSrc == (int)r.ampsFrom &&
	    s_shown.mah == (int)r.mah)
		return;

	s_shown.volts100 = volts100;
	s_shown.amps100  = amps100;
	s_shown.tempC    = r.tempC;
	s_shown.stress   = r.stress;
	s_shown.status   = status;
	s_shown.rate     = s_tel.packetRate;
	s_shown.errPct   = s_tel.errPercent;
	s_shown.voltsSrc = (int)r.voltsFrom;
	s_shown.ampsSrc  = (int)r.ampsFrom;
	s_shown.mah      = r.mah;

	gfxRect(0, Z_TELE_Y0, GFX_W, Z_TELE_Y1 - Z_TELE_Y0 + 1, C_BG);

	const int cw = 118, ch = 33;
	const int cx[2] = {1, 121};
	const int cy[3] = {129, 164, 199};
	char buf[24];

	// A number whose provenance changes silently is worse than no number:
	// 12.25 V from EDT means "somewhere in a 0.25 V bucket", the same reading
	// from KISS means "within 0.01 V". The tag says which claim is being made.
	if (r.voltsFrom != EscSource::None)
		snprintf(buf, sizeof(buf), "%d.%02dV", volts100 / 100, volts100 % 100);
	else
		snprintf(buf, sizeof(buf), "--");
	drawLabelled(cx[0], cy[0], cw, ch, "VOLTAGE", buf, C_TEXT);
	drawSourceTag(cx[0] + cw, cy[0], r.voltsFrom);

	if (r.ampsFrom != EscSource::None)
		snprintf(buf, sizeof(buf), "%d.%02dA", amps100 / 100, amps100 % 100);
	else
		snprintf(buf, sizeof(buf), "--");
	drawLabelled(cx[1], cy[0], cw, ch, "CURRENT", buf, C_TEXT);
	drawSourceTag(cx[1] + cw, cy[0], r.ampsFrom);

	if (r.tempFrom != EscSource::None) snprintf(buf, sizeof(buf), "%d`C", r.tempC);
	else                               snprintf(buf, sizeof(buf), "--");
	// backtick renders as a degree sign in the 5x7 font
	drawLabelled(cx[0], cy[1], cw, ch, "ESC TEMP", buf,
	             (r.tempFrom != EscSource::None && r.tempC >= 90) ? C_RED : C_TEXT);
	// Consumption has no EDT equivalent, so it only ever appears with KISS.
	if (r.mahFrom == EscSource::Kiss) {
		snprintf(buf, sizeof(buf), "%umAh", (unsigned)r.mah);
		gfxText(cx[0] + cw - 6 - gfxTextW(buf, 1), cy[1] + 4, buf, C_DIM, 1);
	}

	if (r.stressFrom != EscSource::None) snprintf(buf, sizeof(buf), "%d", r.stress);
	else                                 snprintf(buf, sizeof(buf), "--");
	drawLabelled(cx[1], cy[1], cw, ch, "STRESS", buf,
	             (r.stressFrom != EscSource::None && r.stress > 200) ? C_AMBER : C_TEXT);

	static const char *STATUS_TXT[4] = {"OK", "WARN", "ERROR", "ALERT"};
	// Not static: the colours come from the theme in force at this draw.
	const uint16_t STATUS_COL[4] = {C_LIME, C_AMBER, C_RED, C_MAGENTA};
	drawLabelled(cx[0], cy[2], cw, ch, "ESC STATUS",
	             status < 0 ? "--" : STATUS_TXT[status],
	             status < 0 ? C_DIM : STATUS_COL[status]);

	snprintf(buf, sizeof(buf), "%d/S", s_tel.packetRate);
	uint16_t linkCol = s_tel.errPercent > 5 ? C_AMBER : C_TEXT;
	drawLabelled(cx[1], cy[2], cw, ch, "LINK", buf, linkCol);
	// error rate rides on the label row so it cannot collide with the value
	snprintf(buf, sizeof(buf), "%d%% ERR", s_tel.errPercent);
	gfxText(cx[1] + cw - 6 - gfxTextW(buf, 1), cy[2] + 4, buf,
	        s_tel.errPercent > 5 ? C_AMBER : C_DIM, 1);
}

/** @brief Throttle gauge, ceiling caption and the HOLD-mode grab handle. */
static void drawThrottle() {
	int pct = (int)((uint32_t)s_throttle * 100 / 2000);
	int maxPct = (int)((uint32_t)s_maxThrottle * 100 / 2000);

	if (s_shown.throttleRaw == (int)s_throttle && s_shown.maxPct == maxPct &&
	    s_shown.thrArmed == (int)s_armed && s_shown.thrHold == (int)s_hold)
		return;
	s_shown.throttleRaw = s_throttle;
	s_shown.maxPct = maxPct;
	s_shown.thrArmed = s_armed;
	s_shown.thrHold = s_hold;

	gfxRect(0, Z_THR_Y0, GFX_W, Z_THR_Y1 - Z_THR_Y0 + 1, C_BG);

	char buf[24];
	gfxText(THR_TRACK_X, 236, s_hold ? "THROTTLE REL" : "THROTTLE", C_DIM, 1);
	snprintf(buf, sizeof(buf), "MAX %d%%", maxPct);
	gfxText(THR_TRACK_X + 80, 236, buf, C_AMBER, 1);

	// The full width of the track *is* the ceiling, so where you put your
	// finger is where the bar ends. The percentage is still of the real
	// 0..2000 DShot range so the number never lies about what the ESC sees.
	snprintf(buf, sizeof(buf), "%d%%", pct);
	gfxText(GFX_W - THR_TRACK_X - gfxTextW(buf, 2), 234, buf,
	        pct > 0 ? C_LIME : C_DIM, 2);

	gfxRoundRect(THR_TRACK_X, THR_TRACK_Y, THR_TRACK_W, THR_TRACK_H, 5, C_PANEL);
	int fillW = s_maxThrottle
	                ? (int)((uint32_t)s_throttle * THR_TRACK_W / s_maxThrottle)
	                : 0;
	if (fillW > THR_TRACK_W) fillW = THR_TRACK_W;
	if (fillW > 0) {
		gfxRoundRect(THR_TRACK_X, THR_TRACK_Y, fillW, THR_TRACK_H, 5,
		             s_armed ? C_LIME : C_GRID);
	}
	// In HOLD mode the bar is a relative control, so give it a grab handle to
	// say so -- it is a thing you push, not a place you tap.
	if (s_armed && s_hold) {
		int hx = THR_TRACK_X + fillW - 3;
		if (hx < THR_TRACK_X) hx = THR_TRACK_X;
		if (hx > THR_TRACK_X + THR_TRACK_W - 6) hx = THR_TRACK_X + THR_TRACK_W - 6;
		// Overhangs the top only. It used to overhang both ends by 3 px, which
		// the taller track has no room for: the bar now ends on the last row of
		// the band, and three more would be drawn into the button row and wiped
		// the next time the buttons repainted.
		gfxRect(hx, THR_TRACK_Y - THR_HANDLE_OVER, 6,
		        THR_TRACK_H + THR_HANDLE_OVER, C_INK);
	}

	gfxRoundFrame(THR_TRACK_X, THR_TRACK_Y, THR_TRACK_W, THR_TRACK_H, 5, C_GRID);
	if (!s_armed) {
		gfxText(THR_TRACK_X + 8, THR_TRACK_Y + (THR_TRACK_H - 7) / 2,
		        "ARM TO ENABLE", C_DIM, 1);
	}
}

/** @brief ARM/DISARM, HOLD and MENU buttons. */
static void drawButtons() {
	// The pressed state is part of the key, or a button would light up under a
	// finger and stay lit until something else happened to invalidate the row.
	int pressKey = (pressing(BTN_ARM, s_touch) ? 1 : 0) |
	               (pressing(BTN_HOLD, s_touch) ? 2 : 0) |
	               (pressing(BTN_MENU, s_touch) ? 4 : 0);
	if (s_shown.holdOn == (int)s_hold && s_shown.btnArmed == (int)s_armed &&
	    s_shown.config == 0 && s_shown.btnPress == pressKey)
		return;
	s_shown.btnPress = pressKey;
	s_shown.holdOn = s_hold;
	s_shown.btnArmed = s_armed;
	s_shown.config = 0;

	// The regions tile exactly (asserted at their definitions), so this is the
	// whole button band and nothing but it. It used to start a row early to
	// cover a boundary row that belonged to neither region.
	gfxRect(0, Z_BTN_Y0, GFX_W, Z_BTN_Y1 - Z_BTN_Y0 + 1, C_BG);
	uiButton(BTN_ARM, s_armed ? "DISARM" : "HOLD TO ARM",
	        s_armed ? C_RED : C_PANEL, s_armed ? C_ONACCENT : C_TEXT,
	        s_armed ? 2 : 1, pressing(BTN_ARM, s_touch));
	uiButton(BTN_HOLD, "HOLD", s_hold ? C_BLUE : C_PANEL,
	        s_hold ? C_ONACCENT : C_DIM, 1, pressing(BTN_HOLD, s_touch));
	uiButton(BTN_MENU, "MENU", C_PANEL, C_DIM, 1, pressing(BTN_MENU, s_touch));
}

/** @} */

/**
 * @brief The one route from the tester to MENU, and so to every tool: it
 *        disarms, zeroes the throttle and turns HOLD off.
 */
static void uiOpenMenu() {
	bool wasArmed = s_armed;
	s_armed = false;
	s_throttle = 0;
	s_hold = false;
	escSetArmed(false);
	if (wasArmed) backlightDip();
	s_level = Level::Menu;
	uiMenuEnter();
	gfxFill(C_BG);
}

/**
 * @brief Main-screen input: throttle gauge, throttle pad and the buttons.
 *
 * Two throttle surfaces feed the same value. The gauge is absolute in spring
 * mode and relative in HOLD mode; the pad is always relative, because a large
 * region with no positional meaning must not move the motor on a bare tap.
 */
static void handleMainTouch() {
	int x = s_touch.x, y = s_touch.y;

	// --- throttle slider ---
	bool inTrack = (y >= THR_TRACK_Y - THR_TOUCH_PAD_TOP) &&
	               (y < THR_TRACK_Y + THR_TRACK_H + THR_TOUCH_PAD_BOT) &&
	               (x >= THR_TRACK_X - 8) && (x < THR_TRACK_X + THR_TRACK_W + 8);

	bool inPad = (y >= PAD_Y0) && (y <= PAD_Y1);

	if (s_touch.pressed && s_armed) {
		if (inTrack) {
			s_dragging = true;
			s_dragAnchorX = (int16_t)x;
			s_dragAnchorThrottle = s_throttle;
		} else if (inPad) {
			s_padDragging = true;
			s_padEngaged = false;
			s_padTouchY = (int16_t)y;
		}
	}
	if (!s_touch.down) {
		s_dragging = false;
		s_padDragging = false;
		s_padEngaged = false;
	}

	if (s_dragging && s_armed) {
		if (s_hold) {
			// RELATIVE. The throttle is latched in HOLD mode, so an absolute
			// slider would make the motor jump to wherever the finger landed.
			relativeThrottle(x, &s_dragAnchorX, &s_dragAnchorThrottle,
			                 THR_TRACK_W, HOLD_DRAG_SENSITIVITY_PCT);
		} else {
			// ABSOLUTE. Spring-loaded trigger: finger position is the throttle,
			// and it returns to zero the moment you let go. Measured from the
			// inner edge of the saturation margin, so both rails are reachable
			// without touching the rim. @see THR_EDGE_SAT
			int rel = x - THR_TRACK_X - THR_EDGE_SAT;
			if (rel < 0) rel = 0;
			if (rel > THR_USABLE_W) rel = THR_USABLE_W;
			s_throttle = (uint16_t)((uint32_t)rel * s_maxThrottle / THR_USABLE_W);
		}
	} else if (s_padDragging && s_armed) {
		// Always relative, in both modes: the number area has no positional
		// meaning, so a tap on it must never move the motor. Position is
		// negated so that swiping *up* the screen raises the throttle.
		if (!s_padEngaged) {
			int travel = y - s_padTouchY;
			if (travel < 0) travel = -travel;
			if (travel >= PAD_DEADZONE_PX) {
				s_padEngaged = true;
				s_padAnchorPos = (int16_t)(-y);
				s_padAnchorThrottle = s_throttle;
			}
		}
		if (s_padEngaged) {
			relativeThrottle(-y, &s_padAnchorPos, &s_padAnchorThrottle,
			                 PAD_H, PAD_DRAG_SENSITIVITY_PCT);
		}
	} else if (!s_hold) {
		s_throttle = 0;
	}
	// Unconditional backstop. Also unreachable today: every drag surface above
	// is gated on s_armed, and every path that clears it zeroes the throttle on
	// the way. It exists so that adding a surface which forgets to check cannot
	// leave a throttle standing.
	if (!s_armed) s_throttle = 0;

	// --- arm button: press and hold ---
	//
	// Refused outright while a host owns the card. BACK from the logging screen
	// leaves the handover running -- deliberately, because a copy in progress
	// should not be cancelled by navigating -- so the tester screen is reachable
	// with the card gone, and arming there would spin a motor whose telemetry
	// cannot be recorded and whose interlocks assume a logger that is not
	// listening. @see usb_msc.h
	// Only a press that lands on ARM while arming is allowed is timed.
	bool cardHeld = mscHoldsCard(mscGetState());
	if (!s_touch.down)
		s_armLive = false;
	else if (s_touch.pressed)
		s_armLive = !s_armed && pressing(BTN_ARM, s_touch);
	if (cardHeld) {
		s_armPressing = false;
		s_armLive = false;
	} else if (pressing(BTN_ARM, s_touch)) {
		if (s_armed) {
			// Disarm is instant, on press. The one control that deliberately
			// does not wait for release: everything else here can afford the
			// cancel gesture, and the button whose job is "stop the motor now"
			// cannot.
			if (s_touch.pressed) {
				s_armed = false;
				s_throttle = 0;
				s_hold = false;
				escSetArmed(false);
				backlightDip();
				invalidateAll();
			}
		} else if (!s_armLive) {
			s_armPressing = false;
		} else {
			if (!s_armPressing) {
				s_armPressing = true;
				s_armPressStart = millis();
			}
			// Belt and braces, and currently unreachable: arming already
			// requires ARM_HOLD_MS of holding, throughout which the throttle is
			// necessarily zero, and ARM_HOLD_MS (1000) is longer than
			// ARM_ZERO_THROTTLE_MS (250). It binds only if the hold is ever
			// shortened below the window. Kept deliberately -- a mutation test
			// confirms no test can reach it, which is a property of the
			// constants rather than a gap in coverage.
			bool zeroLongEnough =
			    (uint32_t)(millis() - s_zeroSince) >= ARM_ZERO_THROTTLE_MS;
			if (zeroLongEnough &&
			    (uint32_t)(millis() - s_armPressStart) >= ARM_HOLD_MS) {
				s_armed = true;
				s_throttle = 0;
				escSetArmed(true);
				s_armPressing = false;
				// Removing this clear fails no test: no disarm can fire while
				// the arming finger is still down. It keeps that finger from
				// arming again if a disarm path ever does.
				s_armLive = false;
				backlightDip();
				invalidateAll();
			}
		}
	} else {
		s_armPressing = false;
	}

	if (tapped(BTN_HOLD, s_touch)) {
		s_hold = !s_hold;
		if (!s_hold) s_throttle = 0;
	} else if (tapped(BTN_MENU, s_touch)) {
		uiOpenMenu();
	}
}

// ---------------------------------------------------------------------------
// control tables (ui_screen.h)
// ---------------------------------------------------------------------------

/**
 * @brief The tester's controls, from the rectangles handleMainTouch() tests.
 *        The throttle reports its drawn track, the coordinates its mapping
 *        is measured from; its grab margin is in @ref TesterCtl.
 */
static uint8_t testerControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	bool cardFree = !mscHoldsCard(mscGetState());
	uiControlAdd(out, &n, max, UI_FIX(TesterCtl::Arm), BTN_ARM, cardFree);
	uiControlAdd(out, &n, max, UI_FIX(TesterCtl::Hold), BTN_HOLD, true);
	uiControlAdd(out, &n, max, UI_FIX(TesterCtl::Menu), BTN_MENU, true);
	uiControlAdd(out, &n, max, UI_FIX(TesterCtl::Throttle),
	             UiRect{ THR_TRACK_X, THR_TRACK_Y, THR_TRACK_W, THR_TRACK_H }, s_armed);
	uiControlAdd(out, &n, max, UI_FIX(TesterCtl::Pad),
	             UiRect{ 0, PAD_Y0, GFX_W, PAD_H }, s_armed);
	return n;
}

const UiScreen UI_TESTER = {
	UiScreenId::Tester, nullptr, PinUser::None, nullptr,
	nullptr, nullptr, nullptr, testerControls, nullptr,
};

/** @brief Every screen by id, for uiControls(). */
static constexpr struct { UiScreenId id; const UiScreen *screen; } SCREENS[] = {
	{ UiScreenId::Tester,   &UI_TESTER },
	{ UiScreenId::Menu,     &UI_MENU_SCREEN },
	{ UiScreenId::Settings, &UI_SETTINGS },
	{ UiScreenId::SdLog,    &UI_SDLOG },
	{ UiScreenId::Am32,     &UI_AM32 },
	{ UiScreenId::Servo,    &UI_SERVO },
	{ UiScreenId::Kst,      &UI_KST },
	{ UiScreenId::Beep,     &UI_BEEP },
};
/** @brief Each @ref UiScreenId has exactly one entry in `SCREENS`. */
static constexpr bool screensComplete() {
	for (size_t id = 0; id <= (size_t)UiScreenId::Beep; id++) {
		int n = 0;
		for (const auto &e : SCREENS) n += (size_t)e.id == id;
		if (n != 1) return false;
	}
	return sizeof(SCREENS) / sizeof(SCREENS[0]) == (size_t)UiScreenId::Beep + 1;
}
static_assert(screensComplete(), "a screen has no entry in SCREENS, or two");

UiScreenId uiCurrentScreen() {
	if (s_level == Level::Tool && s_tool) return s_tool->id;
	if (s_level == Level::Menu) return UiScreenId::Menu;
	return UiScreenId::Tester;
}

uint8_t uiControls(UiControl *out, uint8_t max) {
	UiScreenId id = uiCurrentScreen();
	for (const auto &e : SCREENS)
		if (e.id == id) return e.screen->controls ? e.screen->controls(out, max) : 0;
	return 0;
}

void uiDrawSplash() {
	gfxFill(C_BG);
	// The name takes two lines: as one, "MINIRCBENCH" is 260 px wide at
	// scale 4, and the panel is 240 px wide.
	gfxTextCenter(104, "MINIRC", C_LIME, 4);
	gfxTextCenter(144, "BENCH", C_TEXT, 4);
	gfxTextCenter(188, "BIDIRECTIONAL ESC TESTER", C_DIM, 1);
	gfxTextCenter(216, "JuWi made", C_CYAN, 2);
	// The board this image is built for.
	gfxTextCenter(248, g_board->label, C_GRID, 1);
}

uint16_t uiThrottle() { return s_throttle; }
bool uiArmed() { return s_armed; }

void uiInit() {
	memset(&s_touch, 0, sizeof(s_touch));
	memset(&s_tel, 0, sizeof(s_tel));

	// Which screen is showing is UI state too, and this function claims to reset
	// UI state. It did not, which on hardware is invisible -- it runs once, at
	// boot, from a cold start -- and in the test suite meant a case that left
	// MENU up handed it to whatever ran next, where taps landed
	// on a screen the test did not think it was looking at.
	s_level = Level::Tester;
	s_tool = nullptr;
	s_armed = false;
	s_hold = false;
	s_throttle = 0;
	s_armPressing = false;
	s_armLive = false;
	s_dragging = false;
	s_padDragging = false;
	s_padEngaged = false;
	s_blDipUntil = 0;
	rpmFilterReset(&s_erpmFilter);
	s_erpmShown = 0;

	invalidateAll();

	// 12-bit SAR, 3.3 V reference. adc_init() must precede the GPIO setup.
	adc_init();
	adc_gpio_init(g_board->batAdcPin);

	// The palette and backlight follow the stored preference before the first
	// frame, so a board saved in high contrast never flashes a dark screen on
	// the way up.
	themeSet(settings()->highContrast ? Theme::HighContrast : Theme::Dark);
	st7789SetBacklight(themeBacklight(settings()->backlight));

	escSetPoles(s_poles);
	escSetArmed(false);

	gfxFill(C_BG);
	s_zeroSince = millis();
	s_lastTouchMs = millis();
}

void uiTick() {
	escHeartbeat();
	escSnapshot(&s_tel);
	touchPoll(&s_touch);

	// battery, lightly smoothed
	adc_select_input(g_board->batAdcChan);
	int raw = (int)adc_read();
	float v = (raw * 3.3f / 4095.0f) * g_board->batDivider;
	s_batteryV = s_batteryV == 0.0f ? v : (s_batteryV * 0.9f + v * 0.1f);

	if (s_touch.down) s_lastTouchMs = millis();

	// Once per frame, before anything draws. Inside drawRpm() it would only run
	// when that region repaints -- and that region repaints *because* the value
	// changed, so the filter would only ever be stepped by its own output.
	s_erpmShown = rpmFilterStep(&s_erpmFilter, s_tel.erpm, telemetryAlive(),
	                            RPM_FILTER_SHIFT);

	// Backlight, every frame: the level is a function of the theme, the stored
	// preference and whether a dip is running, so recomputing it is simpler than
	// tracking who last changed which of the three.
	{
		uint8_t want = themeBacklight(settings()->backlight);
		if ((int32_t)(millis() - s_blDipUntil) < 0) want = (uint8_t)(want / BL_DIP_DIV);
		st7789SetBacklight(want);
	}

	// A tool owns the whole screen, and the pins it claims, while it runs.
	// SETTINGS can change the pin the pump drives and the palette everything
	// is rendered in, so leaving any tool invalidates every cached region: a
	// theme swap is invisible to caches that only remember values. A tool
	// returns false once it has left, and BACK takes every tool to MENU.
	if (s_level == Level::Tool) {
		if (!s_tool->tick(&s_touch)) {
			s_level = Level::Menu;
			s_tool = nullptr;
			uiMenuEnter();
			invalidateAll();
			gfxFill(C_BG);
		}
		st7789FlushDirty();
		return;
	}

	// s_zeroSince tracks the last moment throttle was non-zero, so
	// (millis() - s_zeroSince) is "how long we have been at idle".
	if (s_throttle != 0) s_zeroSince = millis();

	// Told here rather than from loop(): the UI owns the arm state, and putting
	// the coupling in main.cpp put it in the one file the host tests never run,
	// so a test covering it could pass against a broken rule.
	sdLogSetArmed(s_armed);

	// Each handler can navigate away, so each draw is conditional on still being
	// on that screen. Painting it anyway is not merely wasted work: the press
	// that leaves also calls gfxFill(), so the extra paint puts the old screen
	// back over a cleared frame, and whatever it leaves on a row the incoming
	// screen's regions do not cover stays there. The residue was invisible for
	// as long as the palette was dark on black; it is not in high contrast.
	if (s_level == Level::Menu) {
		const UiScreen *open = nullptr;
		UiMenuResult r = uiMenuTick(&s_touch, &open);
		if (r == UiMenuResult::Back) {
			s_level = Level::Tester;
			invalidateAll();
			gfxFill(C_BG);
		} else if (r == UiMenuResult::Open) {
			s_level = Level::Tool;
			s_tool = open;
			gfxFill(C_BG);
			open->enter();
		}
	} else {
		handleMainTouch();

		// idle auto-disarm
#if IDLE_DISARM_MS > 0
		if (s_armed && (uint32_t)(millis() - s_lastTouchMs) > IDLE_DISARM_MS) {
			s_armed = false;
			s_throttle = 0;
			s_hold = false;
			escSetArmed(false);
			backlightDip();
			invalidateAll();
		}
#endif

		escSetThrottle(s_throttle);

		drawStatusBar();
		drawRpm();
		drawTelemetry();
		drawThrottle();
		drawButtons();
	}

	st7789FlushDirty();
}
