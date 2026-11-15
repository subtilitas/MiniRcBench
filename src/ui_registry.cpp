/**
 * @file ui_registry.cpp
 * @brief The MENU rows and their fit check. @see ui_registry.h
 */

#include "ui_registry.h"
#include "ui_am32.h"
#include "ui_esc_cmd.h"
#include "ui_esc_config.h"
#include "ui_hitec.h"
#include "ui_kst.h"
#include "ui_menu.h"
#include "ui_sdlog.h"
#include "ui_servo.h"
#include "ui_servo_prog.h"
#include "ui_rx_signals.h"
#include "ui_sbus.h"
#include "ui_settings.h"
#include "ui_tools.h"
#include "ui_angle.h"
#include "servo_supply.h"
#include "gfx.h"

/**
 * @brief One line per row, in MENU order; the number is the widest status
 *        in characters, 0 for a row with none.
 */
constexpr UiMenuEntry UI_MENU[] = {
	{ UI_ESC_CMD_TITLE,    0, &UI_ESC_CMD    },   // the DShot commands, BEEP 1 to 5 among them
	{ UI_ESC_CFG_TITLE,    0, &UI_ESC_CONFIG },   // the chooser: ESC SETTINGS, USB BRIDGE
	{ SERVO_UI_TITLE,      0, &UI_SERVO      },
	{ "SERVO PROGRAMMER",  0, &UI_SERVO_PROG },   // 190 px: no room for a status
	{ "RX SIGNALS",        0, &UI_RX_SIGNALS },   // the chooser: MONITOR, OUTPUT
	{ UI_TOOLS_TITLE,      0, &UI_TOOLS      },   // the chooser: ANGLE METER
	{ "SD LOG",           10, &UI_SDLOG      },   // ui_sdlog.cpp: CARD ERROR
	{ "SETTINGS",          7, &UI_SETTINGS   },   // ui_settings.cpp: UNSAVED, UPDATED
};
constexpr uint8_t UI_MENU_COUNT = (uint8_t)(sizeof(UI_MENU) / sizeof(UI_MENU[0]));

/**
 * @brief PROGRAMMER's entries, top to bottom. HITEC CLASSIC drives SIG to
 *        the converter's 5 V and needs the switched supply: it is drawn
 *        disabled without it, and its two lines name the hardware first,
 *        then the setting. The job refuses a classic entry without the
 *        supply as the backstop (hitec_job.h).
 */
constexpr UiProgEntry UI_PROGRAMMERS[] = {
	{ "KST / CHASERVO", nullptr, &UI_KST, uiKstEnter, nullptr,
	  UI_PROG_POWER_AFTER, UI_PROG_SWITCHED_FMT, { nullptr, nullptr } },
	{ "HITEC D SERIES", "D485HW, D645MW AND THE OTHERS", &UI_HITEC, uiHitecEnterD, nullptr,
	  UI_PROG_POWER_AFTER, UI_PROG_SWITCHED_FMT, { nullptr, nullptr } },
	{ "HITEC CLASSIC", "HS-5XXX, HS-7XXX", &UI_HITEC, uiHitecEnterClassic, servoSupplyFitted,
	  nullptr, UI_PROG_SWITCHED_FMT,
	  { "NEEDS THE CONVERTER OF WIRING PART C", "AND SETTINGS > SERVO SUPPLY GP28/29." } },
};
constexpr uint8_t UI_PROGRAMMERS_COUNT =
	(uint8_t)(sizeof(UI_PROGRAMMERS) / sizeof(UI_PROGRAMMERS[0]));

/** @brief Characters of @p s. */
static constexpr int textLen(const char *s) {
	int n = 0;
	while (s[n]) n++;
	return n;
}

/** @brief gfxTextW()'s rule: (6 x characters - 1) x scale, 0 for none. */
static constexpr int textW(int chars, int scale) {
	return chars > 0 ? (6 * chars - 1) * scale : 0;
}

/**
 * @brief Every label, at scale 2 from @ref UI_MENU_LABEL_X, ends
 *        @ref UI_MENU_LABEL_GAP px before its widest status, at scale 1
 *        right-aligned to @ref UI_MENU_STATUS_R.
 */
static constexpr bool menuRowsFit() {
	for (const UiMenuEntry &e : UI_MENU) {
		if (UI_MENU_LABEL_X + textW(textLen(e.label), 2) + UI_MENU_LABEL_GAP >
		    UI_MENU_STATUS_R - textW(e.statusWidest, 1))
			return false;
	}
	return true;
}
static_assert(menuRowsFit(), "a MENU label runs into its status");

/** @brief Characters of @p s, 0 for null. */
static constexpr int textLenOrZero(const char *s) { return s ? textLen(s) : 0; }

/**
 * @brief Every PROGRAMMER entry fits: its label at scale 2 in the button,
 *        each line at scale 1 from x 8 within @ref UI_PROG_LINE_CHARS (the
 *        switched power line with a two-digit GPIO for its `%u`), and its
 *        lines, the models and the power line or the lines that say why,
 *        end 4 px or more above the next entry, the last one on the panel.
 *        A disabled entry with no line that says why fails: the chooser
 *        would draw it disabled without a reason.
 */
static constexpr bool progEntriesFit() {
	for (int i = 0; i < UI_PROGRAMMERS_COUNT; i++) {
		const UiProgEntry &e = UI_PROGRAMMERS[i];
		if (textLen(e.label) > UI_PROG_LABEL_CHARS) return false;
		if (textLenOrZero(e.models) > UI_PROG_LINE_CHARS) return false;
		if (textLenOrZero(e.powerOwn) > UI_PROG_LINE_CHARS) return false;
		if (!e.powerSwitchedFmt || textLen(e.powerSwitchedFmt) > UI_PROG_LINE_CHARS) return false;
		int why = 0;
		for (const char *w : e.why) {
			if (textLenOrZero(w) > UI_PROG_LINE_CHARS) return false;
			why += w != nullptr;
		}
		if (e.enabled && why == 0) return false;
		int under = (e.models ? 1 : 0) + (why > 1 ? why : 1);
		int top = UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH;
		int end = top + UI_PROG_LINE_DY + (under - 1) * UI_PROG_LINE_H + 7;
		int room = i + 1 < UI_PROGRAMMERS_COUNT ? top + UI_PROG_ENTRY_PITCH - 4 : GFX_H;
		if (end > room) return false;
	}
	return UI_PROGRAMMERS_COUNT > 0;
}
static_assert(progEntriesFit(), "a PROGRAMMER line is too wide, or runs into the next entry");

/**
 * @brief RX SIGNALS' entries, top to bottom (docs/design/rx-signals.md,
 *        decision 4). OUTPUT, the S.BUS output, is not designed: no screen,
 *        so the chooser draws it disabled under `NOT BUILT`.
 */
constexpr UiRxEntry UI_RX_ENTRIES[] = {
	{ "MONITOR", { "S.BUS FROM A RECEIVER, ON GP1.", nullptr }, &UI_SBUS },
	{ "OUTPUT", { "S.BUS TO A FLIGHT CONTROLLER.", UI_RX_NOT_BUILT }, nullptr },
};
constexpr uint8_t UI_RX_ENTRIES_COUNT = (uint8_t)(sizeof(UI_RX_ENTRIES) / sizeof(UI_RX_ENTRIES[0]));

/**
 * @brief Every RX SIGNALS entry fits as PROGRAMMER's do: its label at scale 2
 *        in the button, its lines within @ref UI_PROG_LINE_CHARS, ending 4 px
 *        or more above the next entry, the last one on the panel.
 */
static constexpr bool rxEntriesFit() {
	for (int i = 0; i < UI_RX_ENTRIES_COUNT; i++) {
		const UiRxEntry &e = UI_RX_ENTRIES[i];
		if (textLen(e.label) > UI_PROG_LABEL_CHARS) return false;
		int lines = 0;
		for (const char *l : e.lines) {
			if (!l) break;
			if (textLen(l) > UI_PROG_LINE_CHARS) return false;
			lines++;
		}
		int top = UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH;
		int end = top + UI_PROG_LINE_DY + (lines > 0 ? (lines - 1) * UI_PROG_LINE_H + 7 : 0);
		int room = i + 1 < UI_RX_ENTRIES_COUNT ? top + UI_PROG_ENTRY_PITCH - 4 : GFX_H;
		if (end > room) return false;
	}
	return UI_RX_ENTRIES_COUNT > 0;
}
static_assert(rxEntriesFit(), "an RX SIGNALS line is too wide, or runs into the next entry");

/**
 * @brief ESC CONFIG's entries, top to bottom (docs/design/bluejay-blheli-s.md
 *        decision 1 and the developer's call B; docs/design/usb-bridge.md
 *        decision 1). ESC SETTINGS serves AM32 until the Bluejay slices land;
 *        USB BRIDGE is designed and not built: no screen, so the chooser
 *        draws it disabled under `NOT BUILT`.
 */
constexpr UiEscCfgEntry UI_ESC_CFG_ENTRIES[] = {
	{ AM32_UI_TITLE, { "AM32 SETTINGS, READ AND WRITTEN.", "BLUEJAY, BLHELI_S: READ, NOT WRITTEN." }, &UI_AM32 },
	{ "USB BRIDGE", { "A DESKTOP ESC TOOL OVER USB.", UI_ESC_CFG_NOT_BUILT }, nullptr },
};
constexpr uint8_t UI_ESC_CFG_ENTRIES_COUNT =
	(uint8_t)(sizeof(UI_ESC_CFG_ENTRIES) / sizeof(UI_ESC_CFG_ENTRIES[0]));

/**
 * @brief Every ESC CONFIG entry fits as RX SIGNALS' do: its label at scale 2
 *        in the button, its lines within @ref UI_PROG_LINE_CHARS, ending 4 px
 *        or more above the next entry, the last one on the panel.
 */
static constexpr bool escCfgEntriesFit() {
	for (int i = 0; i < UI_ESC_CFG_ENTRIES_COUNT; i++) {
		const UiEscCfgEntry &e = UI_ESC_CFG_ENTRIES[i];
		if (textLen(e.label) > UI_PROG_LABEL_CHARS) return false;
		int lines = 0;
		for (const char *l : e.lines) {
			if (!l) break;
			if (textLen(l) > UI_PROG_LINE_CHARS) return false;
			lines++;
		}
		int top = UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH;
		int end = top + UI_PROG_LINE_DY + (lines > 0 ? (lines - 1) * UI_PROG_LINE_H + 7 : 0);
		int room = i + 1 < UI_ESC_CFG_ENTRIES_COUNT ? top + UI_PROG_ENTRY_PITCH - 4 : GFX_H;
		if (end > room) return false;
	}
	return UI_ESC_CFG_ENTRIES_COUNT > 0;
}
static_assert(escCfgEntriesFit(), "an ESC CONFIG line is too wide, or runs into the next entry");

/**
 * @brief TOOLS' entries, top to bottom (docs/design/angle-meter.md,
 *        decision 1). TOOLS holds ANGLE METER.
 */
constexpr UiToolsEntry UI_TOOLS_ENTRIES[] = {
	{ UI_ANGLE_TITLE, { "INCIDENCE, DECALAGE, DIHEDRAL, THROW", nullptr }, &UI_ANGLE },
};
constexpr uint8_t UI_TOOLS_ENTRIES_COUNT =
	(uint8_t)(sizeof(UI_TOOLS_ENTRIES) / sizeof(UI_TOOLS_ENTRIES[0]));

/**
 * @brief Every TOOLS entry fits as ESC CONFIG's do: its label at scale 2 in
 *        the button, its lines within @ref UI_PROG_LINE_CHARS, ending 4 px or
 *        more above the next entry, the last one on the panel.
 */
static constexpr bool toolsEntriesFit() {
	for (int i = 0; i < UI_TOOLS_ENTRIES_COUNT; i++) {
		const UiToolsEntry &e = UI_TOOLS_ENTRIES[i];
		if (textLen(e.label) > UI_PROG_LABEL_CHARS || !e.screen) return false;
		int lines = 0;
		for (const char *l : e.lines) {
			if (!l) break;
			if (textLen(l) > UI_PROG_LINE_CHARS) return false;
			lines++;
		}
		int top = UI_PROG_ENTRY_Y + i * UI_PROG_ENTRY_PITCH;
		int end = top + UI_PROG_LINE_DY + (lines > 0 ? (lines - 1) * UI_PROG_LINE_H + 7 : 0);
		int room = i + 1 < UI_TOOLS_ENTRIES_COUNT ? top + UI_PROG_ENTRY_PITCH - 4 : GFX_H;
		if (end > room) return false;
	}
	return UI_TOOLS_ENTRIES_COUNT > 0;
}
static_assert(toolsEntriesFit(), "a TOOLS line is too wide, or runs into the next entry");
