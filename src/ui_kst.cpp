/**
 * @file ui_kst.cpp
 * @brief KST and Chaservo programming screen, not built yet. @see ui_kst.h
 *
 * A stub. The CFG entry is drawn disabled while uiKstAvailable() is false,
 * so nothing calls the other two.
 */

#include "ui_kst.h"
#include "ui_screen.h"

bool uiKstAvailable() { return false; }

void uiKstEnter() {}

bool uiKstTick(const TouchState *t) {
	(void)t;
	return false;
}

/** @brief No control: the screen is not built. */
static uint8_t kstControls(UiControl *, uint8_t) { return 0; }

const UiScreen UI_KST = {
	UiScreenId::Kst, "KST SERVO", PinUser::Kst, nullptr,
	uiKstEnter, uiKstTick, nullptr, kstControls, nullptr,
};
