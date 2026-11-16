/**
 * @file ui_registry.cpp
 * @brief The MENU rows, the four hubs' entries and their fit checks. @see ui_registry.h
 */

#include "ui_registry.h"
#include "ui_am32.h"
#include "ui_esc_cmd.h"
#include "ui_esc_config.h"
#include "ui_hub.h"
#include "ui_bridge.h"
#include "ui_hitec.h"
#include "ui_kst.h"
#include "ui_menu.h"
#include "ui_sdlog.h"
#include "ui_servo.h"
#include "ui_servo_prog.h"
#include "ui_rx_signals.h"
#include "ui_sbus.h"
#include "ui_sbus_out.h"
#include "ui_settings.h"
#include "ui_tools.h"
#include "ui_angle.h"
#include "ui.h"
#include "servo_supply.h"
#include "gfx.h"

/**
 * @brief One line per row, in MENU order; the number is the widest status
 *        in characters, 0 for a row with none.
 */
constexpr UiMenuEntry UI_MENU[] = {
	{ UI_TESTER_LABEL,     0, &UI_TESTER     },   // the tester; the first row (menu-c.md owner decision 4)
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
constexpr UiHubEntry UI_PROGRAMMERS[] = {
	{ "KST / CHASERVO", { nullptr, nullptr }, &UI_KST, uiKstEnter, nullptr, { nullptr, nullptr },
	  UI_PROG_POWER_AFTER, UI_PROG_SWITCHED_FMT },
	{ "HITEC D SERIES", { "D485HW, D645MW AND THE OTHERS", nullptr }, &UI_HITEC, uiHitecEnterD, nullptr,
	  { nullptr, nullptr }, UI_PROG_POWER_AFTER, UI_PROG_SWITCHED_FMT },
	{ "HITEC CLASSIC", { "HS-5XXX, HS-7XXX", nullptr }, &UI_HITEC, uiHitecEnterClassic, servoSupplyFitted,
	  { "NEEDS THE CONVERTER OF WIRING PART C", "AND SETTINGS > SERVO SUPPLY GP28/29." },
	  nullptr, UI_PROG_SWITCHED_FMT },
};
constexpr uint8_t UI_PROGRAMMERS_COUNT =
	(uint8_t)(sizeof(UI_PROGRAMMERS) / sizeof(UI_PROGRAMMERS[0]));
/**
 * @brief Every entry's screen has a pinUser or opens only while the pins are
 *        free (freePins). The hub's gate and its amber strip read the pin
 *        claim for the entry; a screen with neither is never refused.
 */
static constexpr bool hubScreensGuardPins(const UiHub &h) {
	for (uint8_t i = 0; i < h.count; i++)
		if (h.entries[i].screen->pinUser == PinUser::None && !h.entries[i].screen->freePins) return false;
	return true;
}
/** @brief Whether @p h has a strip function, which uiHubTick calls unchecked. */
static constexpr bool hubHasStrip(const UiHub &h) {
	return h.strip != nullptr;
}
constexpr UiHub UI_PROG_HUB = { UI_PROG_TITLE, uiServoProgStrip, UI_PROGRAMMERS, UI_PROGRAMMERS_COUNT };
static_assert(hubEntriesFit(UI_PROG_HUB), "a PROGRAMMER line is too wide, or runs into the next entry");
static_assert(hubHasStrip(UI_PROG_HUB), "the hub has no strip, which uiHubTick calls");
static_assert(hubScreensGuardPins(UI_PROG_HUB), "a hub entry's screen has no pinUser and no freePins");
static constexpr bool programmersHaveSwitchedFmt() {
	for (uint8_t i = 0; i < UI_PROGRAMMERS_COUNT; i++)
		if (!UI_PROGRAMMERS[i].powerSwitchedFmt) return false;
	return true;
}
static_assert(programmersHaveSwitchedFmt(), "a PROGRAMMER entry has no switched-supply power line");

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

/**
 * @brief RX SIGNALS' entries, top to bottom (docs/design/rx-signals.md,
 *        decision 4). OUTPUT's two lines are the S.BUS output's warning
 *        (docs/design/sbus-output.md, "Warnings").
 */
constexpr UiHubEntry UI_RX_ENTRIES[] = {
	{ "MONITOR", { "S.BUS FROM A RECEIVER, ON GP1.", nullptr }, &UI_SBUS, nullptr, nullptr,
	  { nullptr, nullptr }, nullptr, nullptr },
	{ "OUTPUT", { "S.BUS TO A FLIGHT CONTROLLER.", "ON GP0, NEVER TO AN ESC." }, &UI_SBUS_OUT, nullptr, nullptr,
	  { nullptr, nullptr }, nullptr, nullptr },
};
constexpr uint8_t UI_RX_ENTRIES_COUNT = (uint8_t)(sizeof(UI_RX_ENTRIES) / sizeof(UI_RX_ENTRIES[0]));
constexpr UiHub UI_RX_HUB = { UI_RX_TITLE, uiRxSignalsStrip, UI_RX_ENTRIES, UI_RX_ENTRIES_COUNT };
static_assert(hubEntriesFit(UI_RX_HUB), "an RX SIGNALS line is too wide, or runs into the next entry");
static_assert(hubHasStrip(UI_RX_HUB), "the hub has no strip, which uiHubTick calls");
static_assert(hubScreensGuardPins(UI_RX_HUB), "a hub entry's screen has no pinUser and no freePins");

/**
 * @brief ESC CONFIG's entries, top to bottom (docs/design/bluejay-blheli-s.md
 *        decision 1 and the developer's call B; docs/design/usb-bridge.md
 *        decision 1). ESC SETTINGS serves AM32 until the Bluejay slices land;
 *        USB BRIDGE passes bytes through, slice 2 of usb-bridge.md, MSP and
 *        the 4-way interface being its slice 3.
 */
constexpr UiHubEntry UI_ESC_CFG_ENTRIES[] = {
	{ AM32_UI_TITLE, { "AM32 SETTINGS, READ AND WRITTEN.", "BLUEJAY, BLHELI_S: READ AND WRITTEN." }, &UI_AM32,
	  nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr },
	{ BRIDGE_UI_TITLE, { "A DESKTOP ESC TOOL OVER USB.", "MSP, 4-WAY AND PASSTHROUGH." }, &UI_BRIDGE,
	  nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr },
};
constexpr uint8_t UI_ESC_CFG_ENTRIES_COUNT =
	(uint8_t)(sizeof(UI_ESC_CFG_ENTRIES) / sizeof(UI_ESC_CFG_ENTRIES[0]));
constexpr UiHub UI_ESC_CFG_HUB = { UI_ESC_CFG_TITLE, uiEscConfigStrip, UI_ESC_CFG_ENTRIES,
                                   UI_ESC_CFG_ENTRIES_COUNT };
static_assert(hubEntriesFit(UI_ESC_CFG_HUB), "an ESC CONFIG line is too wide, or runs into the next entry");
static_assert(hubHasStrip(UI_ESC_CFG_HUB), "the hub has no strip, which uiHubTick calls");
static_assert(hubScreensGuardPins(UI_ESC_CFG_HUB), "a hub entry's screen has no pinUser and no freePins");

/**
 * @brief TOOLS' entries, top to bottom (docs/design/angle-meter.md,
 *        decision 1). TOOLS holds ANGLE METER.
 */
constexpr UiHubEntry UI_TOOLS_ENTRIES[] = {
	{ UI_ANGLE_TITLE, { "INCIDENCE, DECALAGE, DIHEDRAL, THROW", nullptr }, &UI_ANGLE, nullptr, nullptr,
	  { nullptr, nullptr }, nullptr, nullptr },
};
constexpr uint8_t UI_TOOLS_ENTRIES_COUNT =
	(uint8_t)(sizeof(UI_TOOLS_ENTRIES) / sizeof(UI_TOOLS_ENTRIES[0]));
constexpr UiHub UI_TOOLS_HUB = { UI_TOOLS_TITLE, uiToolsStrip, UI_TOOLS_ENTRIES, UI_TOOLS_ENTRIES_COUNT };
static_assert(hubEntriesFit(UI_TOOLS_HUB), "a TOOLS line is too wide, or runs into the next entry");
static_assert(hubHasStrip(UI_TOOLS_HUB), "the hub has no strip, which uiHubTick calls");
static_assert(hubScreensGuardPins(UI_TOOLS_HUB), "a hub entry's screen has no pinUser and no freePins");

