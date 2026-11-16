/**
 * @file ui_tools.cpp
 * @brief TOOLS, a hub. @see ui_tools.h
 */

#include "ui_tools.h"
#include "ui_widgets.h"
#include "esc_task.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_TOOLS_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "TOOLS' title runs into BACK");
// A one-digit GPIO: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_TOOLS_STRIP_FMT) - 1 - 2 + 1 <= UI_HUB_LINE_CHARS,
              "TOOLS' strip runs off the panel");

void uiToolsStrip(char *buf, size_t n) {
	snprintf(buf, n, UI_TOOLS_STRIP_FMT, (unsigned)escTaskDshotPin());
}

void uiToolsEnter() { uiHubEnter(); }

bool uiToolsTick(const TouchState *t) { return uiHubTick(UI_TOOLS_HUB, t); }

uint8_t uiToolsControls(UiControl *out, uint8_t max) { return uiHubControls(UI_TOOLS_HUB, out, max); }

const UiScreen UI_TOOLS = {
	UiScreenId::Tools, UI_TOOLS_TITLE, PinUser::None, nullptr,
	uiToolsEnter, uiToolsTick, nullptr, uiToolsControls, nullptr, false,
};
