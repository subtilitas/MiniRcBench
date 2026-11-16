/**
 * @file ui_esc_config.cpp
 * @brief ESC CONFIG, a hub. @see ui_esc_config.h
 */

#include "ui_esc_config.h"
#include "ui_widgets.h"
#include "esc_task.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_ESC_CFG_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "ESC CONFIG's title runs into BACK");
// A one-digit GPIO: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_ESC_CFG_STRIP_FMT) - 1 - 2 + 1 <= UI_HUB_LINE_CHARS,
              "ESC CONFIG's strip runs off the panel");

void uiEscConfigStrip(char *buf, size_t n) {
	snprintf(buf, n, UI_ESC_CFG_STRIP_FMT, (unsigned)escTaskDshotPin());
}

void uiEscConfigEnter() { uiHubEnter(); }

bool uiEscConfigTick(const TouchState *t) { return uiHubTick(UI_ESC_CFG_HUB, t); }

uint8_t uiEscConfigControls(UiControl *out, uint8_t max) { return uiHubControls(UI_ESC_CFG_HUB, out, max); }

const UiScreen UI_ESC_CONFIG = {
	UiScreenId::EscConfig, UI_ESC_CFG_TITLE, PinUser::None, nullptr,
	uiEscConfigEnter, uiEscConfigTick, nullptr, uiEscConfigControls, nullptr, true,
};
