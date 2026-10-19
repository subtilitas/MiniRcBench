/**
 * @file ui_registry.cpp
 * @brief The MENU rows and their fit check. @see ui_registry.h
 */

#include "ui_registry.h"
#include "ui_am32.h"
#include "ui_kst.h"
#include "ui_menu.h"
#include "ui_sdlog.h"
#include "ui_servo.h"
#include "ui_settings.h"

/** @brief One line per row, in MENU order; the number is the widest status in characters. */
constexpr UiMenuEntry UI_MENU[] = {
	{ "BEEP THE ESC",  8, &UI_BEEP     },   // ui_menu.cpp: DISARMED, REFUSED
	{ "AM32 CONFIG",   9, &UI_AM32     },   // ui_am32.cpp: GP29, PINS BUSY
	{ "SERVO TESTER",  9, &UI_SERVO    },   // ui_servo.cpp: GP29+28, PINS BUSY
	{ "KST SERVO",     9, &UI_KST      },   // ui_kst.cpp: GP29, PINS BUSY
	{ "SD LOG",       10, &UI_SDLOG    },   // ui_sdlog.cpp: CARD ERROR
	{ "SETTINGS",      9, &UI_SETTINGS },   // ui_settings.cpp: PINS BUSY, UNSAVED
};
constexpr uint8_t UI_MENU_COUNT = (uint8_t)(sizeof(UI_MENU) / sizeof(UI_MENU[0]));

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
