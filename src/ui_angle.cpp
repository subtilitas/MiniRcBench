/**
 * @file ui_angle.cpp
 * @brief ANGLE METER, its SETUP and its FLIP CAL. @see ui_angle.h
 *
 * Each view is a set of regions. A region's key hashes everything it draws;
 * a frame repaints a region only when its key changed, so a new angle
 * repaints the angle's band and not the panel. Changing the view repaints
 * everything.
 */

#include "ui_angle.h"
#include "ui.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_widgets.h"
#include "settings.h"
#include "qmi8658.h"
#include "gfx.h"
#include "touch.h"
#include "plat.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/** @brief gfxTextW()'s rule at compile time: (6 x characters - 1) x scale. */
static constexpr int textW(int chars, int scale) { return chars > 0 ? (6 * chars - 1) * scale : 0; }
/** @brief Characters of a literal. */
#define LEN(s) ((int)sizeof(s) - 1)
/** @brief One scale-1 line's height, a row more in high contrast (themeBold()). */
#define LINE_H 8

/**
 * @defgroup ui_angle_main_layout The main view's layout
 * @{
 */
#define ANG_R       231  /**< The angle, the travel and MAX end here. */
#define ANG_Y       66   /**< The angle, scale 5: y 66 to 100. */
#define ANG_SCALE   5
#define MSG_Y       80   /**< A line in the angle's band while the IMU is unavailable. */
#define CAP_Y       108  /**< FROM ZERO, or the too-steep line. */
#define MM_Y        122  /**< The travel, scale 3: y 122 to 142. */
#define MM_SCALE    3
#define LEN_Y       148  /**< The length line. */
#define MAX_Y       166  /**< MAX's values, scale 2: y 166 to 179. */
#define MAX_CAP_Y   170  /**< MAX's caption, scale 1, beside them. */
#define MAXMM_Y     186  /**< MAX's travel. */
#define TEMP_Y      204  /**< The IMU's temperature. */
#define HINT_Y      252  /**< ZERO's hint, or SETUP's failed save. */
#define BTN_W       72   /**< Each footer button. */
#define ZERO_X      UI_EDGE
#define RMAX_X      84
#define SETUP_X     162
/** @} */

static_assert(UI_STRIP_Y + UI_STRIP_H + 4 <= ANG_Y, "the angle runs into the strip");
static_assert(ANG_Y + 7 * ANG_SCALE + 1 + 4 <= CAP_Y, "the angle runs into its caption");
static_assert(MSG_Y >= ANG_Y && MSG_Y + LINE_H <= ANG_Y + 7 * ANG_SCALE, "the IMU line leaves the angle's band");
static_assert(CAP_Y + LINE_H + 4 <= MM_Y, "the caption runs into the travel");
static_assert(MM_Y + 7 * MM_SCALE + 1 + 4 <= LEN_Y, "the travel runs into the length line");
static_assert(LEN_Y + LINE_H + 8 <= MAX_Y, "the length line runs into MAX");
static_assert(MAX_CAP_Y >= MAX_Y && MAX_CAP_Y + LINE_H <= MAX_Y + 15, "MAX's caption leaves its band");
static_assert(MAX_Y + 15 + 4 <= MAXMM_Y, "MAX runs into its travel");
static_assert(MAXMM_Y + LINE_H + 8 <= TEMP_Y, "MAX's travel runs into the temperature");
static_assert(TEMP_Y + LINE_H + 8 <= HINT_Y, "the temperature runs into the hint");
static_assert(HINT_Y + LINE_H + 4 <= UI_FOOT_Y, "the hint runs into the footer");
static_assert(ZERO_X + BTN_W + 6 == RMAX_X && RMAX_X + BTN_W + 6 == SETUP_X &&
              SETUP_X + BTN_W == GFX_W - UI_EDGE, "the footer is not 6, 84 and 162, 72 px each");
static_assert(BTN_W >= UI_LIST_TAP_MIN && UI_FOOT_H >= UI_LIST_TAP_MIN, "a footer button is below the floor");

// --- the main view's texts, widest first ---
static_assert(UI_MARGIN + textW(LEN(UI_ANGLE_TITLE), 2) + 4 <= UI_BACK_X, "the title runs into BACK");
static_assert(UI_MARGIN + textW(LEN(UI_ANGLE_SETUP_TITLE), 2) + 4 <= UI_BACK_X, "SETUP's title runs into BACK");
static_assert(UI_MARGIN + textW(LEN(UI_ANGLE_FLIP_TITLE), 2) + 4 <= UI_BACK_X, "FLIP CAL's title runs into BACK");
/** @brief The strip's widest caption: `SHORT EDGE  AXIS TILT 90°`, 25 characters, 149 px. */
#define STRIP_WIDEST "SHORT EDGE  AXIS TILT 90`"
/** @brief The widest chip, `TOO STEEP`: 53 px of text and 6 px each side, from x 167. */
#define CHIP_WIDEST  "TOO STEEP"
static_assert(UI_MARGIN + textW(LEN(STRIP_WIDEST), 1) + 10 <= 232 - (textW(LEN(CHIP_WIDEST), 1) + 12),
              "the strip's caption comes within 10 px of the chip");
static_assert(ANG_R - textW(LEN("-180.0`"), ANG_SCALE) >= UI_MARGIN, "the angle runs off the left edge");
static_assert(ANG_R - textW(LEN("-500.0 MM"), MM_SCALE) >= UI_MARGIN, "the travel runs off the left edge");
static_assert(UI_MARGIN + textW(LEN(UI_ANGLE_NOT_ZEROED), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN(UI_ANGLE_STEEP_LINE), 1) <= ANG_R, "the caption runs off the panel");
static_assert(UI_MARGIN + textW(LEN("L 500 MM, HINGE TO TRAILING EDGE"), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN(UI_ANGLE_LEN_OFF), 1) <= ANG_R, "the length line runs off the panel");
static_assert(UI_MARGIN + textW(LEN("MAX"), 1) + 8 <= ANG_R - textW(LEN("-180.0`  +180.0`"), 2),
              "MAX's caption runs into its values");
static_assert(ANG_R - textW(LEN("-500.0 MM  +500.0 MM"), 1) >= UI_MARGIN, "MAX's travel runs off the panel");
// The temperature registers' range, -128.0 °C to 128.0 °C, and a change of
// twice that since the zero.
static_assert(UI_MARGIN + textW(LEN("IMU -128.0`C, -256.0`C SINCE ZERO"), 1) <= ANG_R,
              "the temperature line runs off the panel");
static_assert(UI_MARGIN + textW(LEN(UI_ANGLE_NO_ANSWER), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN("NOT A QMI8658: ID 0XFF."), 1) <= ANG_R, "an IMU line runs off the panel");
static_assert(UI_MARGIN + textW(LEN(UI_ANGLE_HINT_ARMED), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN(UI_ANGLE_HINT_TIMEOUT), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN(UI_ANGLE_HINT_UNSAVED), 1) <= ANG_R, "a hint runs off the panel");
static_assert(textW(LEN("ZERO"), 2) + 8 <= BTN_W && textW(LEN("RESET MAX"), 1) + 8 <= BTN_W &&
              textW(LEN("SETUP"), 2) + 8 <= BTN_W, "a footer label does not fit its button");

/**
 * @defgroup ui_angle_setup_layout SETUP's layout: SETTINGS' rows
 *           (ui_settings.cpp)
 * @{
 */
#define ROW_Y0       UI_STRIP_Y  /**< The first row, y 40: SETUP has no strip. */
#define ROW_H        44
#define ROW_PITCH    50
#define VAL_R        132  /**< LENGTH's value ends here. */
#define ROW_BTN_W    44   /**< `-` and `+`. */
#define BTN_M_X      138
#define BTN_P_X      188
#define TOGGLE_W     94   /**< From BTN_M_X to x 231. */
#define CAP_LABEL_DY 12   /**< A captioned row's label, from the row's top. */
#define CAP_TEXT_DY  25   /**< Its caption. */
#define VAL_SUB_DY   8    /**< A value over its connector line. */
#define SUB_DY       27   /**< The line under LENGTH's value. */
#define ROWS         4    /**< ALONG AXIS, LENGTH, SIGN, CALIBRATION. */
/** @} */

static_assert(ROW_Y0 + (ROWS - 1) * ROW_PITCH + ROW_H <= UI_FOOT_Y, "SETUP's rows run off the view");
static_assert(BTN_M_X + ROW_BTN_W < BTN_P_X && BTN_P_X + ROW_BTN_W <= GFX_W - 4 &&
              BTN_M_X + TOGGLE_W <= GFX_W - 4, "SETUP's buttons overlap or leave the panel");
static_assert(ROW_H >= UI_LIST_TAP_MIN && ROW_BTN_W >= UI_LIST_TAP_MIN && TOGGLE_W >= UI_LIST_TAP_MIN,
              "a SETUP control is below the floor");
static_assert(UI_MARGIN + textW(LEN("CALIBRATION"), 1) + 4 <= BTN_M_X &&
              UI_MARGIN + textW(LEN("ALONG AXIS"), 1) + 4 <= BTN_M_X &&
              UI_MARGIN + textW(LEN("SIGN"), 1) + 4 <= BTN_M_X, "a toggle's label runs into it");
static_assert(UI_MARGIN + textW(LEN("LENGTH, MM"), 1) + 4 <= VAL_R - textW(LEN("500"), 2),
              "LENGTH's label runs into its value");
static_assert(UI_MARGIN + textW(LEN("SHORT EDGE ON AXIS"), 1) + 4 <= BTN_M_X &&
              UI_MARGIN + textW(LEN(UI_ANGLE_CAL_NO), 1) + 4 <= BTN_M_X, "a caption runs into its toggle");
static_assert(textW(LEN("NORMAL"), 2) <= TOGGLE_W - 8 && textW(LEN("INVERT"), 2) <= TOGGLE_W - 8 &&
              textW(LEN("SHORT"), 2) <= TOGGLE_W - 8 && textW(LEN("FLIP"), 2) <= TOGGLE_W - 8,
              "a toggle's text does not fit it");
static_assert(VAL_R - textW(LEN("HINGE TO EDGE"), 1) >= UI_MARGIN + textW(LEN("LENGTH, MM"), 1) + 4 ||
              SUB_DY >= (ROW_H - 7) / 2 + 7 + 2, "LENGTH's line meets its label");
static_assert(SUB_DY >= VAL_SUB_DY + 14 + 4 && SUB_DY + 7 <= ROW_H, "LENGTH's line meets its value or leaves the row");
static_assert(CAP_TEXT_DY >= CAP_LABEL_DY + 7 + 4 && CAP_TEXT_DY + 7 <= ROW_H, "a caption meets its label");

/**
 * @defgroup ui_angle_flip_layout FLIP CAL's layout
 * @{
 */
#define FLIP_LINE_Y   62   /**< The step's first line; the second 12 px under it. */
#define FLIP_LINE_H   12
#define FLIP_THETA_Y  92   /**< The live angle, scale 3: y 92 to 112. */
#define OUT_Y         252  /**< KEPT AND SAVED. */
#define KEEP_W        150  /**< HOLD TO KEEP, x 6 to 155; AGAIN at SETUP_X, 72 px. */
/** @} */

static_assert(FLIP_LINE_Y >= UI_BODY_Y && FLIP_LINE_Y + FLIP_LINE_H + LINE_H + 4 <= FLIP_THETA_Y,
              "FLIP CAL's lines meet the strip or the angle");
static_assert(FLIP_THETA_Y + 7 * MM_SCALE + 1 + 4 <= OUT_Y, "the angle runs into the outcome");
static_assert(OUT_Y + LINE_H + UI_HOLD_BAR_H <= UI_FOOT_Y - 4 + UI_HOLD_BAR_H &&
              OUT_Y + LINE_H <= UI_FOOT_Y - 4, "the outcome runs into HOLD TO KEEP's bar");
static_assert(UI_EDGE + KEEP_W + 6 <= SETUP_X, "HOLD TO KEEP meets AGAIN");
static_assert(textW(LEN("HOLD TO KEEP"), 2) + 8 <= KEEP_W && textW(LEN("AGAIN"), 2) + 8 <= BTN_W,
              "a FLIP CAL label does not fit its button");
static_assert(UI_MARGIN + textW(LEN("TURN IT ROUND, SCREEN UP, SAME SPOT."), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN("LAY THE BOARD ON A FLAT SURFACE,"), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN("OFFSET X -999  Y -999  Z -999 MG"), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN("THE POSES GAVE NO CALIBRATION"), 1) <= ANG_R &&
              UI_MARGIN + textW(LEN(UI_ANGLE_KEPT_UNSAVED), 1) <= ANG_R, "a FLIP CAL line runs off the panel");

static const UiRect BTN_ZERO  = { ZERO_X, UI_FOOT_Y, BTN_W, UI_FOOT_H };
static const UiRect BTN_RMAX  = { RMAX_X, UI_FOOT_Y, BTN_W, UI_FOOT_H };
static const UiRect BTN_SETUP = { SETUP_X, UI_FOOT_Y, BTN_W, UI_FOOT_H };
static const UiRect BTN_KEEP  = { UI_EDGE, UI_FOOT_Y, KEEP_W, UI_FOOT_H };
static const UiRect BTN_AGAIN = { SETUP_X, UI_FOOT_Y, BTN_W, UI_FOOT_H };

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static AngleModel        s_m;
static AngleReading      s_r;
static AngleView         s_view = AngleView::Main;
static const TouchState *s_t = nullptr;
/** @brief Repaint every region on the next frame. */
static bool              s_full = true;
/** @brief LENGTH's hold-to-repeat. */
static Repeat            s_rep;
/** @brief HOLD TO KEEP. */
static UiHold            s_keepHold;
/** @brief The flip's result was kept on this visit to FLIP CAL. */
static bool              s_kept = false;
/** @brief Its save landed. */
static bool              s_keptSaved = false;

/** @brief A region's last key per view; 0 repaints it. */
#define REGIONS 12
static uint32_t s_keys[REGIONS];

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static uint32_t mixText(uint32_t h, const char *s) {
	for (; *s; s++) h = mix(h, (uint8_t)*s);
	return mix(h, 0xFFu);
}

/**
 * @brief Whether region @p i must repaint for @p key; records it. A new view
 *        clears every key first, so all its regions repaint.
 */
static bool regionDue(int i, uint32_t key) {
	key |= 1u;
	if (s_keys[i] == key) return false;
	s_keys[i] = key;
	return true;
}

/** @brief Text @p s, right-aligned to @ref ANG_R at @p scale. */
static void textRight(int y, const char *s, uint16_t col, int scale) {
	gfxText(ANG_R - gfxTextW(s, scale), y, s, col, scale);
}

// ---------------------------------------------------------------------------
// settings
// ---------------------------------------------------------------------------

/** @brief The setup the settings hold, with the board's axis map. */
static void setupFromSettings(AngleSetup *s) {
	angleSetupDefaults(s);
	const Settings *st = settings();
	s->along = st->angleAlong ? AngleAlong::Short : AngleAlong::Long;
	s->invert = st->angleInvert != 0;
	s->lengthMm = st->angleLengthMm;
	s->cal.valid = st->angleCalValid != 0;
	memcpy(s->cal.offG, st->angleCalOffG, sizeof(s->cal.offG));
	memcpy(s->cal.gain, st->angleCalGain, sizeof(s->cal.gain));
}

/**
 * @brief Save the angle fields, never while armed: a flash erase parks
 *        core1. Unreachable armed, since MENU disarms; checked anyway.
 */
static bool saveAngle() { return !uiArmed() && settingsSaveAngle(); }

/** @brief LENGTH one step: `OFF` below 10 mm, 10 to 500 mm, held at the ends. */
static uint16_t lengthStep(uint16_t mm, int dir) {
	const uint16_t lo = (uint16_t)SERVO_RUDDER_LEN_MIN_MM, hi = (uint16_t)SERVO_RUDDER_LEN_MAX_MM;
	if (dir > 0) return mm < lo ? lo : (mm >= hi ? hi : (uint16_t)(mm + 1));
	if (dir < 0) return mm <= lo ? (uint16_t)ANGLE_LEN_OFF : (uint16_t)(mm - 1);
	return mm;
}

/** @brief LENGTH is a length the travel takes. */
static bool lengthSet(uint16_t mm) {
	return mm >= (uint16_t)SERVO_RUDDER_LEN_MIN_MM && mm <= (uint16_t)SERVO_RUDDER_LEN_MAX_MM;
}

// ---------------------------------------------------------------------------
// texts
// ---------------------------------------------------------------------------

/** @brief The chip's text and fill for the IMU and the reading. */
static const char *chipText(uint16_t *fill) {
	switch (qmi8658State()) {
		case ImuState::Absent:
		case ImuState::WrongId:
		case ImuState::Offline:
			*fill = UI_COL_WARN;
			return "NO IMU";
		case ImuState::Online:
			break;
		default:
			*fill = UI_COL_WARN;
			return "STARTING";
	}
	if (!s_r.valid) { *fill = UI_COL_WARN; return "STARTING"; }
	if (s_r.tooSteep) { *fill = UI_COL_WARN; return CHIP_WIDEST; }
	if (s_r.settled) { *fill = UI_COL_SAFE; return "SETTLED"; }
	*fill = C_GRID;
	return "MOVING";
}

/** @brief The IMU is not there to read: no part, another part, or offline. */
static bool imuMissing() {
	ImuState st = qmi8658State();
	return st == ImuState::Absent || st == ImuState::WrongId || st == ImuState::Offline;
}

/** @brief `+12.4°` from @p v, or `---`. */
static void degText(char *b, size_t n, bool has, float v) {
	if (!has) { snprintf(b, n, "---"); return; }
	char t[16];
	snprintf(b, n, "%s`", angleTenthsText(t, sizeof(t), v, true));
}

/** @brief `+8.6 MM` from @p v, or `--- MM`. */
static void mmText(char *b, size_t n, bool has, float v) {
	char t[16];
	snprintf(b, n, "%s MM", has ? angleTenthsText(t, sizeof(t), v, true) : "---");
}

// ---------------------------------------------------------------------------
// the main view
// ---------------------------------------------------------------------------

/** @brief The main view's regions. */
enum : int { R_HEAD, R_STRIP, R_ANGLE, R_CAP, R_MM, R_LEN, R_MAX, R_MAXMM, R_TEMP, R_HINT, R_FOOT };

static void drawStrip(const char *cap, uint16_t capCol, const char *chip, uint16_t chipCol) {
	uiStripClear();
	uiStripText(cap, capCol);
	uiChip(chip, chipCol);
}

static void mainFrame() {
	char b[48], c[48];
	if (regionDue(R_HEAD, mix(1, uiPressing(uiBackRect(), s_t)))) {
		gfxRect(0, 0, GFX_W, UI_HDR_H, C_BG);
		uiHeader(UI_ANGLE_TITLE, s_t);
	}

	// --- the strip: the edge, the axis's tilt, the chip ---
	{
		const char *edge = s_m.set.along == AngleAlong::Short ? "SHORT EDGE" : "LONG EDGE";
		if (s_r.valid) snprintf(b, sizeof(b), "%s  AXIS TILT %d`", edge, (int)fabsf(s_r.tiltDeg));
		else snprintf(b, sizeof(b), "%s  AXIS TILT --", edge);
		uint16_t capCol = s_r.valid && s_r.steepWarn ? UI_COL_WARN : C_DIM;
		uint16_t chipCol;
		const char *chip = chipText(&chipCol);
		if (regionDue(R_STRIP, mix(mix(mixText(mixText(1, b), chip), capCol), chipCol)))
			drawStrip(b, capCol, chip, chipCol);
	}

	// --- the angle, or why there is none ---
	{
		uint16_t col;
		bool msg = imuMissing();
		if (qmi8658State() == ImuState::WrongId) {
			snprintf(b, sizeof(b), UI_ANGLE_WRONG_FMT, (unsigned)qmi8658WhoAmI());
			col = UI_COL_WARN;
		} else if (msg) {
			snprintf(b, sizeof(b), "%s", UI_ANGLE_NO_ANSWER);
			col = UI_COL_WARN;
		} else {
			degText(b, sizeof(b), s_r.hasDeg, s_r.deg);
			col = s_r.hasDeg ? UI_COL_LIVE : C_DIM;
		}
		if (regionDue(R_ANGLE, mix(mix(mixText(1, b), col), msg))) {
			gfxRect(0, ANG_Y, GFX_W, 7 * ANG_SCALE + 1, C_BG);
			if (msg) gfxText(UI_MARGIN, MSG_Y, b, col, 1);
			else textRight(ANG_Y, b, col, ANG_SCALE);
		}
	}

	// --- its caption ---
	{
		bool steep = s_r.valid && s_r.tooSteep && !imuMissing();
		const char *cap = steep ? UI_ANGLE_STEEP_LINE : s_r.zeroed ? UI_ANGLE_FROM_ZERO : UI_ANGLE_NOT_ZEROED;
		uint16_t col = steep ? UI_COL_WARN : C_DIM;
		if (regionDue(R_CAP, mix(mixText(1, cap), col))) {
			gfxRect(0, CAP_Y, GFX_W, LINE_H, C_BG);
			gfxText(UI_MARGIN, CAP_Y, cap, col, 1);
		}
	}

	// --- the travel, and the length it comes from ---
	bool len = lengthSet(s_m.set.lengthMm);
	{
		b[0] = '\0';
		if (len) mmText(b, sizeof(b), s_r.hasMm, s_r.mm);
		uint16_t col = s_r.hasMm ? UI_COL_LIVE : C_DIM;
		if (regionDue(R_MM, mix(mixText(1, b), col))) {
			gfxRect(0, MM_Y, GFX_W, 7 * MM_SCALE + 1, C_BG);
			if (b[0]) textRight(MM_Y, b, col, MM_SCALE);
		}
		if (len) snprintf(c, sizeof(c), UI_ANGLE_LEN_FMT, (unsigned)s_m.set.lengthMm);
		else snprintf(c, sizeof(c), "%s", UI_ANGLE_LEN_OFF);
		if (regionDue(R_LEN, mixText(1, c))) {
			gfxRect(0, LEN_Y, GFX_W, LINE_H, C_BG);
			gfxText(UI_MARGIN, LEN_Y, c, C_DIM, 1);
		}
	}

	// --- MAX each side, negative first, and its travel ---
	{
		char neg[16], pos[16];
		degText(neg, sizeof(neg), s_r.hasMax[0], s_r.maxDeg[0]);
		degText(pos, sizeof(pos), s_r.hasMax[1], s_r.maxDeg[1]);
		if (!s_r.hasMax[0]) snprintf(neg, sizeof(neg), "--");
		if (!s_r.hasMax[1]) snprintf(pos, sizeof(pos), "--");
		snprintf(b, sizeof(b), "%s  %s", neg, pos);
		if (regionDue(R_MAX, mixText(1, b))) {
			gfxRect(0, MAX_Y, GFX_W, 15, C_BG);
			gfxText(UI_MARGIN, MAX_CAP_Y, "MAX", C_DIM, 1);
			textRight(MAX_Y, b, C_TEXT, 2);
		}
		c[0] = '\0';
		if (len) {
			if (s_r.hasMaxMm[0]) mmText(neg, sizeof(neg), true, s_r.maxMm[0]);
			else snprintf(neg, sizeof(neg), "--");
			if (s_r.hasMaxMm[1]) mmText(pos, sizeof(pos), true, s_r.maxMm[1]);
			else snprintf(pos, sizeof(pos), "--");
			snprintf(c, sizeof(c), "%s  %s", neg, pos);
		}
		if (regionDue(R_MAXMM, mixText(1, c))) {
			gfxRect(0, MAXMM_Y, GFX_W, LINE_H, C_BG);
			if (c[0]) textRight(MAXMM_Y, c, C_DIM, 1);
		}
	}

	// --- the IMU's temperature, and its change since the zero ---
	{
		b[0] = '\0';
		if (s_r.hasTemp) {
			char t[16], d[16];
			angleTenthsText(t, sizeof(t), s_r.tempC, false);
			if (s_r.zeroed)
				snprintf(b, sizeof(b), "IMU %s`C, %s`C SINCE ZERO", t,
				         angleTenthsText(d, sizeof(d), s_r.tempSinceZero, true));
			else
				snprintf(b, sizeof(b), "IMU %s`C", t);
		}
		uint16_t col = s_r.tempWarn ? UI_COL_WARN : C_DIM;
		if (regionDue(R_TEMP, mix(mixText(1, b), col))) {
			gfxRect(0, TEMP_Y, GFX_W, LINE_H, C_BG);
			gfxText(UI_MARGIN, TEMP_Y, b, col, 1);
		}
	}

	// --- the hint ---
	{
		const char *h = s_r.zero == AngleZero::Armed    ? UI_ANGLE_HINT_ARMED
		              : s_r.zero == AngleZero::TimedOut ? UI_ANGLE_HINT_TIMEOUT
		              : settingsAngleDirty()            ? UI_ANGLE_HINT_UNSAVED
		                                                : "";
		if (regionDue(R_HINT, mixText(1, h))) {
			gfxRect(0, HINT_Y, GFX_W, LINE_H, C_BG);
			gfxText(UI_MARGIN, HINT_Y, h, UI_COL_WARN, 1);
		}
	}

	// --- the footer ---
	{
		bool zeroLive = qmi8658State() == ImuState::Online;
		uint32_t k = mix(mix(mix(mix(1, zeroLive), uiPressing(BTN_ZERO, s_t)), uiPressing(BTN_RMAX, s_t)),
		                 uiPressing(BTN_SETUP, s_t));
		if (regionDue(R_FOOT, k)) {
			gfxRect(0, UI_FOOT_Y, GFX_W, UI_FOOT_H, C_BG);
			uiButton(BTN_ZERO, "ZERO", C_PANEL, C_TEXT, 2, uiPressing(BTN_ZERO, s_t), zeroLive);
			uiButton(BTN_RMAX, "RESET MAX", C_PANEL, C_TEXT, 1, uiPressing(BTN_RMAX, s_t));
			uiButton(BTN_SETUP, "SETUP", C_PANEL, C_TEXT, 2, uiPressing(BTN_SETUP, s_t));
		}
	}
}

/** @brief The main view's taps. @return false once BACK has left. */
static bool mainTouch(uint32_t now) {
	if (uiBackTapped(s_t)) {
		// The meter's only exit: SETUP's and FLIP CAL's BACK stay in it.
		qmi8658Stop();
		return false;
	}
	if (qmi8658State() == ImuState::Online && uiTapped(BTN_ZERO, s_t)) {
		// On the lift: the model takes the zero from a settled reading at
		// least a second later, so the tap's own movement has passed.
		angleZeroArm(&s_m, now);
	}
	if (uiTapped(BTN_RMAX, s_t)) angleResetMax(&s_m);
	if (uiTapped(BTN_SETUP, s_t)) {
		memset(&s_rep, 0, sizeof(s_rep));
		s_view = AngleView::Setup;
		s_full = true;
	}
	return true;
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------

/** @brief SETUP's row @p i, its top. */
static int rowY(int i) { return ROW_Y0 + i * ROW_PITCH; }

static UiRect toggleRect(int i) { return UiRect{ BTN_M_X, (int16_t)rowY(i), TOGGLE_W, ROW_H }; }
static UiRect minusRect() { return UiRect{ BTN_M_X, (int16_t)rowY(1), ROW_BTN_W, ROW_H }; }
static UiRect plusRect() { return UiRect{ BTN_P_X, (int16_t)rowY(1), ROW_BTN_W, ROW_H }; }

/** @brief A toggle row: its label, its caption if any, its button. */
static void drawToggleRow(int i, const char *label, const char *cap, const char *value) {
	int y = rowY(i);
	gfxRect(0, y, GFX_W, ROW_H, C_BG);
	if (cap) {
		gfxText(UI_MARGIN, y + CAP_LABEL_DY, label, C_DIM, 1);
		gfxText(UI_MARGIN, y + CAP_TEXT_DY, cap, C_DIM, 1);
	} else {
		gfxText(UI_MARGIN, y + (ROW_H - 7) / 2, label, C_DIM, 1);
	}
	UiRect r = toggleRect(i);
	uiButton(r, value, C_PANEL, C_TEXT, 2, uiPressing(r, s_t));
}

static void setupFrame() {
	if (regionDue(0, mix(2, uiPressing(uiBackRect(), s_t)))) {
		gfxRect(0, 0, GFX_W, UI_HDR_H, C_BG);
		uiHeader(UI_ANGLE_SETUP_TITLE, s_t);
	}
	bool shortEdge = s_m.set.along == AngleAlong::Short;
	const char *along = shortEdge ? "SHORT" : "LONG";
	if (regionDue(1, mix(mixText(1, along), uiPressing(toggleRect(0), s_t))))
		drawToggleRow(0, "ALONG AXIS", shortEdge ? "SHORT EDGE ON AXIS" : "LONG EDGE ON AXIS", along);

	char v[8];
	if (lengthSet(s_m.set.lengthMm)) snprintf(v, sizeof(v), "%u", (unsigned)s_m.set.lengthMm);
	else snprintf(v, sizeof(v), "OFF");
	if (regionDue(2, mix(mix(mixText(1, v), uiPressing(minusRect(), s_t)), uiPressing(plusRect(), s_t)))) {
		int y = rowY(1);
		gfxRect(0, y, GFX_W, ROW_H, C_BG);
		gfxText(UI_MARGIN, y + (ROW_H - 7) / 2, "LENGTH, MM", C_DIM, 1);
		gfxText(VAL_R - gfxTextW(v, 2), y + VAL_SUB_DY, v, C_TEXT, 2);
		gfxText(VAL_R - gfxTextW("HINGE TO EDGE", 1), y + SUB_DY, "HINGE TO EDGE", C_DIM, 1);
		uiButton(minusRect(), "-", C_PANEL, C_TEXT, 2, uiPressing(minusRect(), s_t));
		uiButton(plusRect(), "+", C_PANEL, C_TEXT, 2, uiPressing(plusRect(), s_t));
	}

	const char *sign = s_m.set.invert ? "INVERT" : "NORMAL";
	if (regionDue(3, mix(mixText(1, sign), uiPressing(toggleRect(2), s_t))))
		drawToggleRow(2, "SIGN", nullptr, sign);

	bool cal = angleCalValid(&s_m.set.cal);
	if (regionDue(4, mix(mix(1, cal), uiPressing(toggleRect(3), s_t))))
		drawToggleRow(3, "CALIBRATION", cal ? UI_ANGLE_CAL_YES : UI_ANGLE_CAL_NO, "FLIP");
}

/** @brief SETUP's taps and steps. */
static void setupTouch(uint32_t now) {
	Settings *st = settings();
	if (uiBackTapped(s_t)) {
		// A save whenever the fields differ from flash: a change on this
		// visit, a calibration FLIP CAL could not save, or a save that
		// failed before. The hint shows while they differ.
		if (settingsAngleDirty()) saveAngle();
		s_view = AngleView::Main;
		s_full = true;
		return;
	}
	if (uiTapped(toggleRect(0), s_t)) {
		AngleAlong a = s_m.set.along == AngleAlong::Long ? AngleAlong::Short : AngleAlong::Long;
		angleSetAlong(&s_m, a);
		st->angleAlong = (uint8_t)a;
	}
	int dir = 0;
	if (uiPressing(minusRect(), s_t)) dir = -1;
	else if (uiPressing(plusRect(), s_t)) dir = +1;
	if (repeatFires(&s_rep, dir, now) && dir) {
		uint16_t mm = lengthStep(s_m.set.lengthMm, dir);
		angleSetLength(&s_m, mm);
		st->angleLengthMm = mm;
	}
	if (uiTapped(toggleRect(2), s_t)) {
		bool inv = !s_m.set.invert;
		angleSetInvert(&s_m, inv);
		st->angleInvert = inv ? 1 : 0;
	}
	if (uiTapped(toggleRect(3), s_t)) {
		angleFlipBegin(&s_m);
		uiHoldReset(&s_keepHold);
		s_kept = false;
		s_keptSaved = false;
		s_view = AngleView::Flip;
		s_full = true;
	}
}

// ---------------------------------------------------------------------------
// FLIP CAL
// ---------------------------------------------------------------------------

/** @brief The step's strip text and its two lines. */
static const char *flipTexts(char *l1, char *l2, size_t n) {
	const AngleFlip &f = s_m.flip;
	switch (f.step) {
		case FlipStep::Up:
			snprintf(l1, n, "LAY THE BOARD ON A FLAT SURFACE,");
			snprintf(l2, n, "SCREEN UP. HOLD STILL.");
			return "STEP 1 OF 3";
		case FlipStep::Turn:
			snprintf(l1, n, "TURN IT ROUND, SCREEN UP, SAME SPOT.");
			l2[0] = '\0';
			return "STEP 2 OF 3";
		case FlipStep::Down:
		case FlipStep::Back:
			snprintf(l1, n, "TURN IT SCREEN DOWN, WAIT 3 S,");
			snprintf(l2, n, "THEN SCREEN UP AGAIN.");
			return "STEP 3 OF 3";
		case FlipStep::Done: {
			const AngleCal &c = f.result;
			snprintf(l1, n, "OFFSET X %ld  Y %ld  Z %ld MG", lroundf(c.offG[0] * 1000.0f),
			         lroundf(c.offG[1] * 1000.0f), lroundf(c.offG[2] * 1000.0f));
			// The gain of the normal's chip axis, the one the flip finds.
			int n3 = s_m.set.axes[2] < 0 ? -s_m.set.axes[2] : s_m.set.axes[2];
			long g = lroundf(c.gain[n3 - 1] * 1000.0f);
			snprintf(l2, n, "GAIN %c %ld.%03ld", "XYZ"[n3 - 1], g / 1000, g % 1000);
			return "RESULT";
		}
		default:
			snprintf(l1, n, "THE POSES GAVE NO CALIBRATION");
			snprintf(l2, n, "WITHIN THE LIMITS. TAP AGAIN.");
			return "NO RESULT";
	}
}

/** @brief The board lies screen down: FLIP CAL's controls ignore touches and are drawn disabled. */
static bool flipDown() { return s_r.faceDown; }

static bool keepLive() { return s_m.flip.step == FlipStep::Done && !s_kept && !flipDown(); }

static void flipFrame(uint32_t now) {
	char l1[48], l2[48];
	const char *step = flipTexts(l1, l2, sizeof(l1));
	bool down = flipDown();
	if (regionDue(0, mix(mix(3, uiPressing(uiBackRect(), s_t)), down))) {
		gfxRect(0, 0, GFX_W, UI_HDR_H, C_BG);
		uiHeader(UI_ANGLE_FLIP_TITLE, s_t, !down);
	}
	uint16_t chipCol;
	const char *chip = chipText(&chipCol);
	if (regionDue(1, mix(mixText(mixText(1, step), chip), chipCol))) drawStrip(step, C_DIM, chip, chipCol);
	if (regionDue(2, mixText(mixText(1, l1), l2))) {
		gfxRect(0, FLIP_LINE_Y, GFX_W, FLIP_LINE_H + LINE_H, C_BG);
		gfxText(UI_MARGIN, FLIP_LINE_Y, l1, C_TEXT, 1);
		gfxText(UI_MARGIN, FLIP_LINE_Y + FLIP_LINE_H, l2, C_TEXT, 1);
	}
	char b[16];
	degText(b, sizeof(b), s_r.hasDeg, s_r.deg);
	uint16_t col = s_r.hasDeg ? UI_COL_LIVE : C_DIM;
	if (regionDue(3, mix(mixText(1, b), col))) {
		gfxRect(0, FLIP_THETA_Y, GFX_W, 7 * MM_SCALE + 1, C_BG);
		textRight(FLIP_THETA_Y, b, col, MM_SCALE);
	}
	const char *out = !s_kept ? "" : s_keptSaved ? UI_ANGLE_KEPT : UI_ANGLE_KEPT_UNSAVED;
	if (regionDue(4, mixText(1, out))) {
		gfxRect(0, OUT_Y, GFX_W, LINE_H, C_BG);
		gfxText(UI_MARGIN, OUT_Y, out, s_keptSaved ? UI_COL_SAFE : UI_COL_WARN, 1);
	}
	bool live = keepLive();
	bool holding = s_keepHold.holding;
	uint32_t k = mix(mix(mix(mix(mix(1, live), holding), uiHoldPercent(&s_keepHold, now)),
	                     uiPressing(BTN_AGAIN, s_t)), down);
	if (regionDue(5, k)) {
		gfxRect(0, UI_FOOT_Y, GFX_W, UI_FOOT_H, C_BG);
		uiHoldDrawBar(&s_keepHold, BTN_KEEP, UI_FOOT_Y - 4, now);
		// Every hold fills C_RED with C_ONACCENT text while held, as SETTINGS'.
		uiButton(BTN_KEEP, "HOLD TO KEEP", holding ? C_RED : C_PANEL, holding ? C_ONACCENT : C_TEXT, 2,
		         holding, live);
		uiButton(BTN_AGAIN, "AGAIN", C_PANEL, C_TEXT, 2, uiPressing(BTN_AGAIN, s_t), !down);
	}
}

/** @brief FLIP CAL's taps and its hold. */
static void flipTouch(uint32_t now) {
	if (uiHoldUpdate(&s_keepHold, BTN_KEEP, s_t, keepLive(), now)) {
		AngleCal cal = s_m.flip.result;
		angleSetCal(&s_m, &cal);
		Settings *st = settings();
		st->angleCalValid = 1;
		memcpy(st->angleCalOffG, cal.offG, sizeof(st->angleCalOffG));
		memcpy(st->angleCalGain, cal.gain, sizeof(st->angleCalGain));
		s_keptSaved = saveAngle();
		s_kept = true;
	}
	if (uiBackTapped(s_t)) {
		angleFlipEnd(&s_m);
		uiHoldReset(&s_keepHold);
		memset(&s_rep, 0, sizeof(s_rep));
		s_view = AngleView::Setup;
		s_full = true;
		return;
	}
	if (uiTapped(BTN_AGAIN, s_t)) {
		angleFlipBegin(&s_m);
		uiHoldReset(&s_keepHold);
		s_kept = false;
		s_keptSaved = false;
	}
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void uiAngleEnter() {
	AngleSetup set;
	setupFromSettings(&set);
	angleOpen(&s_m, &set);
	memset(&s_r, 0, sizeof(s_r));
	angleRead(&s_m, &s_r);
	s_view = AngleView::Main;
	s_full = true;
	s_kept = false;
	s_keptSaved = false;
	memset(&s_rep, 0, sizeof(s_rep));
	uiHoldReset(&s_keepHold);
	qmi8658Start(millis());
}

bool uiAngleTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	uint32_t now = millis();

	// The driver ticked this frame before the tool (uiTick()); the model
	// takes its newest sample once.
	angleSample(&s_m, qmi8658Latest());
	angleTick(&s_m, now);
	angleRead(&s_m, &s_r);

	if (s_view == AngleView::Main) {
		if (!mainTouch(now)) return false;
	} else if (s_view == AngleView::Setup) {
		setupTouch(now);
	} else {
		// Screen down the glass lies on the table: no touch reaches a control.
		if (flipDown()) s_t = &none;
		flipTouch(now);
	}
	// What the taps changed, ZERO armed or MAX reset among them, shows on
	// this frame.
	angleRead(&s_m, &s_r);

	if (s_full) {
		memset(s_keys, 0, sizeof(s_keys));
		gfxFill(C_BG);
	}
	if (s_view == AngleView::Main) mainFrame();
	else if (s_view == AngleView::Setup) setupFrame();
	else flipFrame(now);
	s_full = false;
	return true;
}

uint8_t uiAngleControls(UiControl *out, uint8_t max) {
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), s_view != AngleView::Flip || !flipDown());
	if (s_view == AngleView::Main) {
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Zero), BTN_ZERO, qmi8658State() == ImuState::Online);
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::ResetMax), BTN_RMAX, true);
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Setup), BTN_SETUP, true);
	} else if (s_view == AngleView::Setup) {
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Along), toggleRect(0), true);
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::LenMinus), minusRect(), true);
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::LenPlus), plusRect(), true);
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Sign), toggleRect(2), true);
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Flip), toggleRect(3), true);
	} else {
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Keep), BTN_KEEP, keepLive());
		uiControlAdd(out, &n, max, UI_FIX(AngleCtl::Again), BTN_AGAIN, !flipDown());
	}
	return n;
}

AngleView uiAngleView() { return s_view; }

const AngleModel *uiAngleModel() { return &s_m; }

const UiScreen UI_ANGLE = {
	UiScreenId::Angle, UI_ANGLE_TITLE, PinUser::None, nullptr,
	uiAngleEnter, uiAngleTick, nullptr, uiAngleControls, nullptr, true,
};
