/**
 * @file ui_servo_hub.cpp
 * @brief SERVO, a hub. @see ui_servo_hub.h
 */

#include "ui_servo_hub.h"
#include "ui_widgets.h"
#include "esc_task.h"
#include "servo_supply.h"

#include <stdio.h>

static_assert(UI_MARGIN + 6 * (int)(sizeof(UI_SERVO_HUB_TITLE) - 1) * 2 - 2 + 4 <= UI_BACK_X,
              "SERVO's title runs into BACK");
// The strip at its widest, with one-digit signal pins and a two-digit supply pin.
static_assert(sizeof(UI_SERVO_HUB_STRIP_FMT) - 1 - 6 + 2 + 4 <= UI_HUB_LINE_CHARS,
              "SERVO's strip runs off the panel");

void uiServoHubStrip(char *buf, size_t n) {
	char supply[8];
	if (servoSupplyFitted()) snprintf(supply, sizeof(supply), "GP%u", (unsigned)servoSupplyPin());
	else snprintf(supply, sizeof(supply), "OWN");
	snprintf(buf, n, UI_SERVO_HUB_STRIP_FMT, (unsigned)escTaskDshotPin(), (unsigned)escTaskKissPin(), supply);
}

void uiServoHubEnter() { uiHubEnter(UI_SERVO_HUB); }

bool uiServoHubTick(const TouchState *t) { return uiHubTick(UI_SERVO_HUB, t); }

uint8_t uiServoHubControls(UiControl *out, uint8_t max) { return uiHubControls(UI_SERVO_HUB, out, max); }

const UiScreen UI_SERVO_HUB_SCREEN = {
	UiScreenId::ServoHub, UI_SERVO_HUB_TITLE, PinUser::None, nullptr,
	uiServoHubEnter, uiServoHubTick, nullptr, uiServoHubControls, nullptr, false,
};
