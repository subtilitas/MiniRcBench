/**
 * @file ui_registry.cpp
 * @brief The MENU rows, the three hubs' entries and their fit checks. @see ui_registry.h
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
#include "ui_servo_hub.h"
#include "ui_rx_signals.h"
#include "ui_sbus.h"
#include "ui_sbus_out.h"
#include "ui_settings.h"
#include "ui_angle.h"
#include "ui.h"
#include "servo_supply.h"
#include "gfx.h"

/**
 * @brief One line per row, in MENU order; the number is the widest status
 *        in characters, 0 for a row with none. Six rows: ESC TESTER, then
 *        one row a device class, then the log and the settings
 *        (docs/design/menu-c.md owner decision 4). A row whose screen is a
 *        hub (uiHubOf()) carries `>` in place of a status.
 */
constexpr UiMenuEntry UI_MENU[] = {
	{ UI_TESTER_LABEL,     0, &UI_TESTER           },   // the tester; the first row
	{ UI_ESC_CFG_TITLE,    0, &UI_ESC_CONFIG       },   // the hub: ESC COMMANDS, ESC SETTINGS, USB BRIDGE
	{ UI_SERVO_HUB_TITLE,  0, &UI_SERVO_HUB_SCREEN },   // the hub: the tester, the angle meter, the programmers
	{ UI_RX_TITLE,         0, &UI_RX_SIGNALS       },   // the hub: MONITOR, OUTPUT
	{ "SD LOG",           10, &UI_SDLOG            },   // ui_sdlog.cpp: CARD ERROR
	{ "SETTINGS",          7, &UI_SETTINGS         },   // ui_settings.cpp: UNSAVED, UPDATED
};
constexpr uint8_t UI_MENU_COUNT = (uint8_t)(sizeof(UI_MENU) / sizeof(UI_MENU[0]));
static_assert(UI_MENU_COUNT == 6, "MENU does not have its six rows");

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

/**
 * @brief ESC CONFIG's entries, top to bottom (docs/design/menu-c.md, "The
 *        hub"; docs/design/bluejay-blheli-s.md decision 1 and the
 *        developer's call B; docs/design/usb-bridge.md decision 1). ESC
 *        COMMANDS sends beacons 1 to 5, the direction and the 3D mode as
 *        DShot commands (docs/design/esc-commands.md). ESC SETTINGS serves
 *        AM32, Bluejay and BLHeli_S; USB BRIDGE passes MSP, the 4-way
 *        interface and raw bytes through.
 */
constexpr UiHubEntry UI_ESC_CFG_ENTRIES[] = {
	{ UI_ESC_CMD_TITLE, { "BEEP 1 TO 5, DIRECTION, 3D MODE.", "SENT AS DSHOT COMMANDS." }, &UI_ESC_CMD,
	  nullptr, nullptr, { nullptr, nullptr }, nullptr, nullptr },
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
static_assert(UI_ESC_CFG_ENTRIES_COUNT == 3 && UI_ESC_CFG_ENTRIES[0].screen == &UI_ESC_CMD &&
              UI_ESC_CFG_ENTRIES[1].screen == &UI_AM32 && UI_ESC_CFG_ENTRIES[2].screen == &UI_BRIDGE,
              "EscCfgCtl does not name ESC CONFIG's entries in their order");

/**
 * @brief SERVO's entries, top to bottom, in the order of use: drive,
 *        measure, program (docs/design/menu-c.md, "The hub"). The servo
 *        tester drives one servo on SIG or two on SIG and AUX
 *        (docs/design/servo-tester.md). HITEC CLASSIC drives SIG to the
 *        converter's 5 V and needs the switched supply: it is drawn
 *        disabled without it, and its two lines name the hardware first,
 *        then the setting. The job refuses a classic entry without the
 *        supply as the backstop (hitec_job.h).
 */
constexpr UiHubEntry UI_SERVO_ENTRIES[] = {
	{ SERVO_UI_TITLE, { "ONE SERVO ON SIG, OR TWO WITH AUX.", nullptr }, &UI_SERVO, nullptr, nullptr,
	  { nullptr, nullptr }, nullptr, nullptr },
	{ UI_ANGLE_TITLE, { "INCIDENCE, DECALAGE, DIHEDRAL, THROW", nullptr }, &UI_ANGLE, nullptr, nullptr,
	  { nullptr, nullptr }, nullptr, nullptr },
	{ "KST / CHASERVO", { nullptr, nullptr }, &UI_KST, uiKstEnter, nullptr, { nullptr, nullptr },
	  UI_PROG_POWER_AFTER, UI_PROG_SWITCHED_FMT },
	{ "HITEC D SERIES", { "D485HW, D645MW AND THE OTHERS", nullptr }, &UI_HITEC, uiHitecEnterD, nullptr,
	  { nullptr, nullptr }, UI_PROG_POWER_AFTER, UI_PROG_SWITCHED_FMT },
	{ "HITEC CLASSIC", { "HS-5XXX, HS-7XXX", nullptr }, &UI_HITEC, uiHitecEnterClassic, servoSupplyFitted,
	  { "NEEDS THE CONVERTER OF WIRING PART C", "AND SETTINGS > SERVO SUPPLY GP28/29." },
	  nullptr, UI_PROG_SWITCHED_FMT },
};
constexpr uint8_t UI_SERVO_ENTRIES_COUNT =
	(uint8_t)(sizeof(UI_SERVO_ENTRIES) / sizeof(UI_SERVO_ENTRIES[0]));
constexpr UiHub UI_SERVO_HUB = { UI_SERVO_HUB_TITLE, uiServoHubStrip, UI_SERVO_ENTRIES, UI_SERVO_ENTRIES_COUNT };
static_assert(hubEntriesFit(UI_SERVO_HUB), "a SERVO line is too wide, or runs into the next entry");
static_assert(hubHasStrip(UI_SERVO_HUB), "the hub has no strip, which uiHubTick calls");
static_assert(hubScreensGuardPins(UI_SERVO_HUB), "a hub entry's screen has no pinUser and no freePins");
/** @brief Each programmer, an entry with an entry point of its own, has a switched-supply power line. */
static constexpr bool programmersHaveSwitchedFmt() {
	for (uint8_t i = 0; i < UI_SERVO_ENTRIES_COUNT; i++)
		if (UI_SERVO_ENTRIES[i].enter && !UI_SERVO_ENTRIES[i].powerSwitchedFmt) return false;
	return true;
}
static_assert(programmersHaveSwitchedFmt(), "a programmer entry of SERVO has no switched-supply power line");
// Five entries: 404 px of content in the 258 px viewport, a range of 146 px.
static_assert(uiHubContentPx(UI_SERVO_ENTRIES_COUNT) - (GFX_H - UI_HUB_ENTRY_Y) == 146,
              "SERVO does not scroll its 146 px");

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

/** @brief The three hubs, each with the screen that shows it. */
static constexpr struct { const UiScreen *screen; const UiHub *hub; } UI_HUBS[] = {
	{ &UI_ESC_CONFIG,       &UI_ESC_CFG_HUB },
	{ &UI_SERVO_HUB_SCREEN, &UI_SERVO_HUB   },
	{ &UI_RX_SIGNALS,       &UI_RX_HUB      },
};

/** @brief uiHubOf(), at build time too. */
static constexpr const UiHub *hubOf(const UiScreen *s) {
	for (const auto &h : UI_HUBS)
		if (h.screen == s) return h.hub;
	return nullptr;
}

const UiHub *uiHubOf(const UiScreen *s) { return hubOf(s); }

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

static_assert(UI_MENU_MARK_X + textW(textLen(UI_MENU_MARK), 2) <= 8 + 224,
              "MENU's mark runs off its row, x 8 to 231");

/**
 * @brief Every label, at scale 2 from @ref UI_MENU_LABEL_X, ends
 *        @ref UI_MENU_LABEL_GAP px before what stands at the row's right:
 *        its widest status, at scale 1 right-aligned to
 *        @ref UI_MENU_STATUS_R, or, on a row that opens a hub, the mark
 *        `>` at @ref UI_MENU_MARK_X. A hub row has no status: the mark
 *        stands in its place.
 */
static constexpr bool menuRowsFit() {
	for (const UiMenuEntry &e : UI_MENU) {
		bool hub = hubOf(e.screen) != nullptr;
		if (hub && e.statusWidest) return false;
		int right = hub ? UI_MENU_MARK_X : UI_MENU_STATUS_R - textW(e.statusWidest, 1);
		if (UI_MENU_LABEL_X + textW(textLen(e.label), 2) + UI_MENU_LABEL_GAP > right) return false;
	}
	return true;
}
static_assert(menuRowsFit(), "a MENU label runs into its status or its mark");
