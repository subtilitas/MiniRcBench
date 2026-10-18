/**
 * @file ui_kst.cpp
 * @brief KST and Chaservo programming screen, not built yet. @see ui_kst.h
 *
 * A stub. The CFG entry is drawn disabled while uiKstAvailable() is false,
 * so nothing calls the other two.
 */

#include "ui_kst.h"

bool uiKstAvailable() { return false; }

void uiKstEnter() {}

bool uiKstTick(const TouchState *t) {
	(void)t;
	return false;
}
