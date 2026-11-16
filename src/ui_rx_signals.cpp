/**
 * @file ui_rx_signals.cpp
 * @brief RX SIGNALS, a hub. @see ui_rx_signals.h
 */

#include "ui_rx_signals.h"
#include "ui_widgets.h"
#include "esc_task.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_RX_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "RX SIGNALS' title runs into BACK");
// One-digit GPIOs: this board's signal pins are GP0 and GP1.
static_assert(sizeof(UI_RX_STRIP_FMT) - 1 - 4 + 2 <= UI_HUB_LINE_CHARS,
              "RX SIGNALS' strip runs off the panel");

void uiRxSignalsStrip(char *buf, size_t n) {
	snprintf(buf, n, UI_RX_STRIP_FMT, (unsigned)escTaskDshotPin(), (unsigned)escTaskKissPin());
}

void uiRxSignalsEnter() { uiHubEnter(UI_RX_HUB); }

bool uiRxSignalsTick(const TouchState *t) { return uiHubTick(UI_RX_HUB, t); }

uint8_t uiRxSignalsControls(UiControl *out, uint8_t max) { return uiHubControls(UI_RX_HUB, out, max); }

const UiScreen UI_RX_SIGNALS = {
	UiScreenId::RxSignals, UI_RX_TITLE, PinUser::None, nullptr,
	uiRxSignalsEnter, uiRxSignalsTick, nullptr, uiRxSignalsControls, nullptr, false,
};
