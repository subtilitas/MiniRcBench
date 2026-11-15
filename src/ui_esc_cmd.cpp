/**
 * @file ui_esc_cmd.cpp
 * @brief ESC COMMANDS. @see ui_esc_cmd.h
 */

#include "ui_esc_cmd.h"
#include "ui_hold.h"
#include "ui_input.h"
#include "ui_menu.h"
#include "ui_widgets.h"
#include "esc_3d.h"
#include "esc_task.h"
#include "esc_merge.h"
#include "config.h"
#include "gfx.h"
#include "touch.h"
#include "dshot/dshot.h"

#include "plat.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** @brief gfxTextW()'s rule at scale 1: 6 px a character, less one. */
#define ECW(s) (6 * (int)(sizeof(s) - 1) - 1)

static_assert(UI_MARGIN + 2 * (6 * (int)(sizeof(UI_ESC_CMD_TITLE) - 1) - 1) + 20 <= UI_BACK_X,
              "ESC COMMANDS' title is not 20 px clear of BACK");
// The widest caption, SIG GP1 at 1200 kbit/s, ends 4 px or more before the
// widest chip, EDT OFF, from x 179.
static_assert(UI_MARGIN + ECW("SIG GP1  DSHOT1200") + 4 <=
                  GFX_W - UI_MARGIN - (ECW(UI_MENU_EDT_OFF) + 12),
              "ESC COMMANDS' caption runs under the EDT chip");
static_assert(UI_EDGE + 4 * UI_ESC_CMD_BEEP_PITCH + UI_ESC_CMD_BEEP_SIZE == GFX_W - UI_EDGE,
              "BEEP 1 to 5 do not span x 6 to 233");
static_assert(UI_ESC_CMD_BEEP_SIZE >= UI_LIST_TAP_MIN && UI_ESC_CMD_HOLD_H >= UI_LIST_TAP_MIN,
              "an ESC COMMANDS control is lower than 40 px");
static_assert(UI_ESC_CMD_BEEP_CAP_Y >= UI_BODY_Y && UI_ESC_CMD_BEEP_CAP_Y + 8 <= UI_ESC_CMD_BEEP_Y &&
              UI_ESC_CMD_BEEP_Y + UI_ESC_CMD_BEEP_SIZE + 8 <= UI_ESC_CMD_DIR_CAP_Y &&
              UI_ESC_CMD_DIR_CAP_Y + 8 <= UI_ESC_CMD_DIR_Y &&
              UI_ESC_CMD_DIR_Y + UI_ESC_CMD_HOLD_H + UI_HOLD_BAR_H <= UI_ESC_CMD_3D_CAP_Y &&
              UI_ESC_CMD_3D_CAP_Y + 8 <= UI_ESC_CMD_3D_Y &&
              UI_ESC_CMD_3D_Y + UI_ESC_CMD_HOLD_H + UI_HOLD_BAR_H <= UI_ESC_CMD_LINE_Y,
              "ESC COMMANDS' rows overlap");
static_assert(UI_EDGE + UI_ESC_CMD_HOLD_W < UI_ESC_CMD_HOLD_X2 &&
              UI_ESC_CMD_HOLD_X2 + UI_ESC_CMD_HOLD_W == GFX_W - UI_EDGE,
              "the holds overlap or do not end at x 233");
static_assert(UI_ESC_CMD_LINE_Y + (UI_ESC_CMD_LINES - 1) * UI_ESC_CMD_LINE_H + 7 < GFX_H,
              "ESC COMMANDS' last line runs off the panel");
static_assert(UI_MARGIN + 6 * UI_ESC_CMD_LINE_CHARS - 1 <= GFX_W - UI_MARGIN,
              "a message line runs past x 232");
// The texts that say how long: the hold and the settle.
static_assert(UI_HOLD_MS == 1000, "UI_ESC_CMD_IDLE_1 says HOLD 1 S");
static_assert(ESC_CMD_SETTLE_MS == 1500, "UI_ESC_CMD_SETTLING says WAIT 1.5 S");

/** @brief Characters of @p s. */
static constexpr int textLen(const char *s) {
	int n = 0;
	while (s[n]) n++;
	return n;
}

/**
 * @brief Every message line fits @ref UI_ESC_CMD_LINE_CHARS, each format at
 *        its widest: a command and its name, a one-digit beacon, a two-digit
 *        GPIO.
 */
static constexpr bool linesFit() {
	const char *plain[] = {
		UI_ESC_CMD_IDLE_1, UI_ESC_CMD_IDLE_2, UI_ESC_CMD_CHECK, UI_ESC_CMD_IDLE_5,
		UI_ESC_CMD_IDLE_6, UI_ESC_CMD_UNCONFIRMED, UI_ESC_CMD_3D_NOW, UI_ESC_CMD_3D_LOW,
		UI_ESC_CMD_3D_HIGH, UI_ESC_CMD_3D_RESTART, UI_ESC_CMD_3D_STAYS, UI_ESC_CMD_TURNING,
		UI_ESC_CMD_SETTLING, UI_ESC_CMD_NOT_SENT, UI_ESC_CMD_SAVE_LOST, UI_ESC_CMD_EXPIRED,
	};
	for (const char *p : plain)
		if (textLen(p) > UI_ESC_CMD_LINE_CHARS) return false;
	// "%u" and "%s" at their widest together, 9 characters: 8 and REVERSED,
	// against 10 and 3D ON, 7 characters.
	if (textLen(UI_ESC_CMD_SENDING_FMT) - 4 + 9 > UI_ESC_CMD_LINE_CHARS) return false;
	if (textLen(UI_ESC_CMD_SENT_FMT) - 4 + 9 > UI_ESC_CMD_LINE_CHARS) return false;
	if (textLen(UI_ESC_CMD_BEEP_SENDING_FMT) - 4 + 2 > UI_ESC_CMD_LINE_CHARS) return false;
	if (textLen(UI_ESC_CMD_NO_ANSWER_FMT) - 2 + 2 > UI_ESC_CMD_LINE_CHARS) return false;
	return true;
}
static_assert(linesFit(), "an ESC COMMANDS message line is wider than 37 characters");

/** @brief One of the four holds: its command, its label and its place. */
struct HoldDef {
	EscCmdCtl   id;
	uint8_t     cmd;    /**< Sent before 12. */
	const char *label;  /**< Scale 2, centred: at most 8 characters, 94 px. */
	const char *name;   /**< In the message lines. */
	int16_t     x, y;
};

static const HoldDef HOLDS[4] = {
	{ EscCmdCtl::Normal,   DSHOT_CMD_SPIN_DIRECTION_1, "NORMAL",   "NORMAL",   UI_EDGE,
	  UI_ESC_CMD_DIR_Y },
	{ EscCmdCtl::Reversed, DSHOT_CMD_SPIN_DIRECTION_2, "REVERSED", "REVERSED", UI_ESC_CMD_HOLD_X2,
	  UI_ESC_CMD_DIR_Y },
	{ EscCmdCtl::Off3d,    DSHOT_CMD_3D_MODE_OFF,      "3D OFF",   "3D OFF",   UI_EDGE,
	  UI_ESC_CMD_3D_Y },
	{ EscCmdCtl::On3d,     DSHOT_CMD_3D_MODE_ON,       "3D ON",    "3D ON",    UI_ESC_CMD_HOLD_X2,
	  UI_ESC_CMD_3D_Y },
};
// A hold's label at scale 2 leaves 8 px each side of the 111 px button.
static_assert(2 * (6 * 8 - 1) + 16 <= UI_ESC_CMD_HOLD_W, "REVERSED does not fit its hold");

static const TouchState *s_t = nullptr;
/** @brief Hash of what the last frame drew; 0 forces a repaint. */
static uint32_t s_drawnKey = 0;
static UiHold   s_hold[4];
/** @brief Each hold's bar as last drawn, percent; -1 forces a redraw. */
static int      s_barPct[4];
/** @brief The page's own request was taken: its outcome is the one shown. */
static bool     s_own = false;
/** @brief That request's command. */
static uint8_t  s_ownCmd = 0;

/** @brief BEEP @p i + 1's button. */
static UiRect beepRect(int i) {
	return UiRect{ (int16_t)(UI_EDGE + i * UI_ESC_CMD_BEEP_PITCH), UI_ESC_CMD_BEEP_Y,
	               UI_ESC_CMD_BEEP_SIZE, UI_ESC_CMD_BEEP_SIZE };
}

/** @brief Hold @p i's button. */
static UiRect holdRect(int i) {
	return UiRect{ HOLDS[i].x, HOLDS[i].y, UI_ESC_CMD_HOLD_W, UI_ESC_CMD_HOLD_H };
}

/** @brief The name of change @p cmd, 7 to 10, for the message lines. */
static const char *changeName(uint8_t cmd) {
	for (const HoldDef &h : HOLDS)
		if (h.cmd == cmd) return h.name;
	return "?";
}

/** @brief One message line. */
struct Line {
	char     text[UI_ESC_CMD_LINE_CHARS + 4];
	uint16_t col;
};

/** @brief What a frame of the page shows and gates on. */
struct View {
	EscCmdReady ready;
	EscCmdState state;   /**< escCommandState(), read while the page's own request runs. */
	bool        going;   /**< The page's request waits or its sequence goes out. */
	bool        live;    /**< The controls act: Ready or Busy, and nothing of the page going out. */
	Line        lines[UI_ESC_CMD_LINES];
};

/** @brief Set line @p i of @p v. */
static void setLine(View *v, int i, uint16_t col, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
static void setLine(View *v, int i, uint16_t col, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(v->lines[i].text, sizeof(v->lines[i].text), fmt, ap);
	va_end(ap);
	v->lines[i].col = col;
}

/**
 * @brief The amber line for a readiness that refuses a command, or false
 *        for Busy and Ready.
 */
static bool readinessLine(View *v, int i) {
	switch (v->ready) {
		case EscCmdReady::NoAnswer:
			setLine(v, i, UI_COL_WARN, UI_ESC_CMD_NO_ANSWER_FMT, (unsigned)escTaskDshotPin());
			return true;
		case EscCmdReady::Turning:  setLine(v, i, UI_COL_WARN, "%s", UI_ESC_CMD_TURNING);  return true;
		case EscCmdReady::Settling: setLine(v, i, UI_COL_WARN, "%s", UI_ESC_CMD_SETTLING); return true;
		default: return false;
	}
}

/**
 * @brief The lines of the page's own sequence, from its state: going out,
 *        sent, or how it ended.
 */
static void outcomeLines(View *v) {
	const uint8_t c = s_ownCmd;
	const bool beacon = c >= DSHOT_CMD_BEEP1 && c <= DSHOT_CMD_BEEP5;
	switch (v->state) {
		case EscCmdState::Waiting:
		case EscCmdState::Sending:
			if (beacon) setLine(v, 0, C_TEXT, UI_ESC_CMD_BEEP_SENDING_FMT, (unsigned)c, (unsigned)c);
			else setLine(v, 0, C_TEXT, UI_ESC_CMD_SENDING_FMT, (unsigned)c, changeName(c));
			break;
		case EscCmdState::Sent:
			if (beacon) {
				setLine(v, 0, C_TEXT, UI_ESC_CMD_BEEP_SENT_FMT, (unsigned)c, (unsigned)c);
				break;
			}
			setLine(v, 0, C_TEXT, UI_ESC_CMD_SENT_FMT, (unsigned)c, changeName(c));
			if (c == DSHOT_CMD_3D_MODE_ON) {
				setLine(v, 1, C_TEXT, "%s", UI_ESC_CMD_3D_NOW);
				setLine(v, 2, UI_COL_WARN, "%s", UI_ESC_CMD_3D_LOW);
				setLine(v, 3, UI_COL_WARN, "%s", UI_ESC_CMD_3D_HIGH);
				setLine(v, 4, UI_COL_WARN, "%s", UI_ESC_CMD_3D_RESTART);
			} else if (c == DSHOT_CMD_3D_MODE_OFF) {
				setLine(v, 1, C_TEXT, "%s", UI_ESC_CMD_3D_STAYS);
			} else {
				setLine(v, 1, C_TEXT, "%s", UI_ESC_CMD_UNCONFIRMED);
				setLine(v, 2, C_TEXT, "%s", UI_ESC_CMD_CHECK);
			}
			break;
		case EscCmdState::NotSent:
			setLine(v, 0, UI_COL_WARN, "%s", UI_ESC_CMD_NOT_SENT);
			break;
		case EscCmdState::SaveLost:
			setLine(v, 0, UI_COL_WARN, "%s", UI_ESC_CMD_SAVE_LOST);
			setLine(v, 1, UI_COL_WARN, "%s", UI_ESC_CMD_CHECK);
			break;
		case EscCmdState::Expired:
			setLine(v, 0, UI_COL_WARN, "%s", UI_ESC_CMD_EXPIRED);
			break;
		case EscCmdState::Idle:
			break;
	}
}

/**
 * @brief This frame's view. The page's own outcome takes lines 1 to 5, and a
 *        readiness that refuses a command the sixth, which no outcome uses.
 *        With no outcome of its own, such a readiness takes the place of the
 *        idle lines; at Busy and Ready the idle lines show.
 */
static void buildView(View *v) {
	memset(v, 0, sizeof(*v));
	v->ready = escCommandReady();
	uint8_t cmd = 0;
	v->state = s_own ? escCommandState(&cmd) : EscCmdState::Idle;
	v->going = s_own && (v->state == EscCmdState::Waiting || v->state == EscCmdState::Sending);
	// Busy is live too: a request taken then waits in the slot, and an EDT
	// enable's 11 ms of Busy would otherwise end a hold under way.
	v->live = (v->ready == EscCmdReady::Ready || v->ready == EscCmdReady::Busy) && !v->going;
	if (s_own && v->state != EscCmdState::Idle) {
		outcomeLines(v);
		readinessLine(v, UI_ESC_CMD_LINES - 1);
		return;
	}
	if (readinessLine(v, 0)) return;
	setLine(v, 0, C_DIM, "%s", UI_ESC_CMD_IDLE_1);
	setLine(v, 1, C_DIM, "%s", UI_ESC_CMD_IDLE_2);
	setLine(v, 2, C_DIM, "%s", UI_ESC_CMD_CHECK);
	setLine(v, 4, C_DIM, "%s", UI_ESC_CMD_IDLE_5);
	setLine(v, 5, C_DIM, "%s", UI_ESC_CMD_IDLE_6);
}

/** @brief What the strip's chip says, as MENU's: @ref UI_MENU_EDT_ON and the rest. */
enum class Chip : uint8_t { NoEsc, Off, On };

static Chip chipNow() {
	EscTelemetry tel;
	escSnapshot(&tel);
	EscReading rd;
	escMerge(&tel, millis(), KISS_STALE_MS, EDT_STALE_MS, &rd);
	return rd.edtFresh ? Chip::On
	     : escFieldFresh(tel.lastRpmMs, millis(), ESC_LINK_STALE_MS) ? Chip::Off
	                                                                 : Chip::NoEsc;
}

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/** @brief Everything a frame draws but the hold bars, hashed: a change repaints. */
static uint32_t frameKey(const View *v, Chip chip) {
	uint32_t h = 2166136261u;
	h = mix(h, (uint32_t)chip);
	h = mix(h, escTaskDshotPin());
	h = mix(h, escTaskDshotKbaud());
	h = mix(h, v->live);
	h = mix(h, v->going);
	for (const Line &l : v->lines) {
		for (const char *c = l.text; *c; c++) h = mix(h, (uint8_t)*c);
		h = mix(h, l.col);
	}
	for (int i = 0; i < 5; i++) h = mix(h, v->live && uiPressing(beepRect(i), s_t));
	for (int i = 0; i < 4; i++) h = mix(h, s_hold[i].holding);
	h = mix(h, !v->going && uiPressing(uiBackRect(), s_t));
	return h | 1u;
}

static void draw(const View *v, Chip chip) {
	gfxFill(C_BG);
	uiHeader(UI_ESC_CMD_TITLE, s_t, !v->going);
	uiStripClear();
	char cap[32];
	snprintf(cap, sizeof(cap), UI_ESC_CMD_STRIP_FMT, (unsigned)escTaskDshotPin(),
	         (unsigned)escTaskDshotKbaud());
	uiStripText(cap, C_DIM);
	switch (chip) {
		case Chip::On:    uiChip(UI_MENU_EDT_ON, C_GREEN); break;
		case Chip::Off:   uiChip(UI_MENU_EDT_OFF, C_RED);  break;
		case Chip::NoEsc: uiChip(UI_MENU_NO_ESC, C_GRID);  break;
	}

	gfxText(UI_MARGIN, UI_ESC_CMD_BEEP_CAP_Y, "BEEP", C_DIM, 1);
	for (int i = 0; i < 5; i++) {
		char n[2] = { (char)('1' + i), '\0' };
		UiRect r = beepRect(i);
		uiButton(r, n, C_PANEL, C_TEXT, 2, v->live && uiPressing(r, s_t), v->live);
	}
	gfxText(UI_MARGIN, UI_ESC_CMD_DIR_CAP_Y, "DIRECTION", C_DIM, 1);
	gfxText(UI_MARGIN, UI_ESC_CMD_3D_CAP_Y, "3D MODE", C_DIM, 1);
	for (int i = 0; i < 4; i++) {
		// Every hold fills C_RED with C_ONACCENT text while held.
		bool holding = s_hold[i].holding;
		uiButton(holdRect(i), HOLDS[i].label, holding ? C_RED : C_PANEL,
		         holding ? C_ONACCENT : C_TEXT, 2, holding, v->live);
		s_barPct[i] = -1;
	}
	for (int i = 0; i < UI_ESC_CMD_LINES; i++)
		if (v->lines[i].text[0])
			gfxText(UI_MARGIN, UI_ESC_CMD_LINE_Y + i * UI_ESC_CMD_LINE_H, v->lines[i].text,
			        v->lines[i].col, 1);
}

/** @brief Hand a request to core1; the page shows its outcome once taken. */
static void request(uint8_t cmd, bool save) {
	// Refused only if the readiness changed since core1 last published, or
	// armed, which the page cannot reach: the readiness line says why.
	s_own = escRequestCommand(cmd, save);
	s_ownCmd = s_own ? cmd : 0;
	for (UiHold &h : s_hold) uiHoldReset(&h);
}

void uiEscCmdEnter() {
	s_own = false;
	s_ownCmd = 0;
	for (int i = 0; i < 4; i++) {
		uiHoldReset(&s_hold[i]);
		s_barPct[i] = -1;
	}
	s_drawnKey = 0;
}

bool uiEscCmdTick(const TouchState *t) {
	static const TouchState none = {};
	s_t = t ? t : &none;
	View v;
	buildView(&v);
	// Leaving cancels a request that still waits, so it cannot fire later on
	// another screen; BACK is drawn disabled while one does.
	if (!v.going && uiBackTapped(s_t)) {
		escCancelCommand();
		return false;
	}
	uint32_t now = millis();
	for (int i = 0; i < 5; i++) {
		if (v.live && uiTapped(beepRect(i), s_t)) {
			request((uint8_t)(DSHOT_CMD_BEEP1 + i), false);
			buildView(&v);
		}
	}
	for (int i = 0; i < 4; i++) {
		if (uiHoldUpdate(&s_hold[i], holdRect(i), s_t, v.live, now)) {
			request(HOLDS[i].cmd, true);
			buildView(&v);
		}
	}
	// 3D ON may have reached the ESC once it is sent, or stopped after its
	// first block's frame 6: the tester's note stands from then. 3D OFF
	// leaves the note (decision 4). @see esc3dNote()
	if (s_own && s_ownCmd == DSHOT_CMD_3D_MODE_ON &&
	    (v.state == EscCmdState::Sent || v.state == EscCmdState::SaveLost))
		esc3dSetNote(true);

	Chip chip = chipNow();
	uint32_t key = frameKey(&v, chip);
	if (key != s_drawnKey) {
		s_drawnKey = key;
		draw(&v, chip);
	}
	for (int i = 0; i < 4; i++) {
		int pct = uiHoldPercent(&s_hold[i], now);
		if (pct == s_barPct[i]) continue;
		s_barPct[i] = pct;
		UiRect r = holdRect(i);
		uiHoldDrawBar(&s_hold[i], r, r.y + r.h, now);
	}
	return true;
}

uint8_t uiEscCmdControls(UiControl *out, uint8_t max) {
	View v;
	buildView(&v);
	uint8_t n = 0;
	uiControlAdd(out, &n, max, UI_FIX(UI_BACK), uiBackRect(), !v.going);
	for (int i = 0; i < 5; i++)
		uiControlAdd(out, &n, max, UI_FIX((uint8_t)EscCmdCtl::Beep1 + i), beepRect(i), v.live);
	for (int i = 0; i < 4; i++)
		uiControlAdd(out, &n, max, UI_FIX(HOLDS[i].id), holdRect(i), v.live);
	return n;
}

const UiScreen UI_ESC_CMD = {
	UiScreenId::EscCmd, UI_ESC_CMD_TITLE, PinUser::None, nullptr,
	uiEscCmdEnter, uiEscCmdTick, nullptr, uiEscCmdControls, nullptr, true,
};
