/**
 * @file ui_servo_prog.cpp
 * @brief PROGRAMMER, a hub. @see ui_servo_prog.h
 */

#include "ui_servo_prog.h"
#include "ui_widgets.h"
#include "esc_task.h"
#include "servo_supply.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_PROG_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "PROGRAMMER's title runs into BACK");
// The strip at its widest, with a one-digit SIG and a two-digit supply pin.
static_assert(sizeof(UI_PROG_STRIP_FMT) - 1 - 4 + 1 + 4 <= UI_HUB_LINE_CHARS,
              "PROGRAMMER's strip runs off the panel");

void uiServoProgStrip(char *buf, size_t n) {
	char supply[8];
	if (servoSupplyFitted()) snprintf(supply, sizeof(supply), "GP%u", (unsigned)servoSupplyPin());
	else snprintf(supply, sizeof(supply), "OWN");
	snprintf(buf, n, UI_PROG_STRIP_FMT, (unsigned)escTaskDshotPin(), supply);
}

void uiServoProgEnter() { uiHubEnter(); }

bool uiServoProgTick(const TouchState *t) { return uiHubTick(UI_PROG_HUB, t); }

uint8_t uiServoProgControls(UiControl *out, uint8_t max) { return uiHubControls(UI_PROG_HUB, out, max); }

const UiScreen UI_SERVO_PROG = {
	UiScreenId::ServoProg, UI_PROG_TITLE, PinUser::None, nullptr,
	uiServoProgEnter, uiServoProgTick, nullptr, uiServoProgControls, nullptr, false,
};
