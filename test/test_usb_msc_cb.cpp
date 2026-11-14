/**
 * @file test_usb_msc_cb.cpp
 * @brief usb_msc.cpp itself, linked against stubs of the card, the logger
 *        and TinyUSB: the TinyUSB callbacks drive the shipped state machine.
 *
 * A program of its own: the main suite links a fake of usb_msc.cpp's
 * runtime half (fakes.cpp) for the screens. This one links the real file
 * and nothing of the UI, and passes by its exit status.
 */

#include "usb_msc.h"
#include "sd_log.h"
#include "ui.h"

#include "tusb.h"
#include "hw_config.h"
#include "sd_card.h"

#include <stdio.h>
#include <string.h>

// Defined in usb_msc.cpp; TinyUSB's own headers declare them on the board.
bool tud_msc_test_unit_ready_cb(uint8_t lun);
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject);

static int g_failed = 0;

static void check(const char *what, bool ok) {
	printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
	if (!ok) g_failed++;
}

// --- the logger, as far as usb_msc.cpp asks it ---
static bool g_released = false;
static int  g_reacquires = 0;

void sdLogStatus(SdLogStatus *out) {
	memset(out, 0, sizeof(*out));
	out->state = g_released ? SdLogState::NoCard : SdLogState::Idle;
}
bool sdLogRelease() { g_released = true; return true; }
bool sdLogReacquire() { g_released = false; g_reacquires++; return true; }

bool uiArmed() { return false; }

// --- the card: 1000 sectors, every read answered ---
static block_dev_err_t readBlocks(sd_card_t *, uint8_t *, uint32_t, uint32_t) {
	return SD_BLOCK_DEVICE_ERROR_NONE;
}
static sd_card_t g_card = {};
sd_card_t *sd_get_by_num(size_t num) { return num == 0 ? &g_card : nullptr; }

bool tud_msc_set_sense(uint8_t, uint8_t, uint8_t, uint8_t) { return true; }

/** @brief USB DRIVE pressed and the handover run: the host has the card. */
static void serve() {
	mscInit();
	g_released = false;
	check("USB DRIVE accepted", mscRequest() == MscRefusal::None);
	mscTick();
	check("the card handed over: serving", mscGetState() == MscState::Serving && g_released);
	check("the medium reads present", tud_msc_test_unit_ready_cb(0));
}

int main() {
	g_card.state.sectors = 1000;
	g_card.read_blocks = readBlocks;

	printf("USB drive: the host unconfigures or unplugs while it holds the card\n");
	serve();
	int before = g_reacquires;
	tud_umount_cb();
	check("unmounted: the card is reclaimed, as on an eject", mscGetState() == MscState::Reclaim);
	check("the host no longer holds it, arming is not blocked",
	      !mscHoldsCard(mscGetState()));
	check("the medium reads absent at once", !tud_msc_test_unit_ready_cb(0));
	mscTick();
	check("the next tick mounts it for the logger", mscGetState() == MscState::Idle &&
	      g_reacquires == before + 1 && !g_released);

	printf("USB drive: an eject from the host does the same\n");
	serve();
	before = g_reacquires;
	tud_msc_start_stop_cb(0, 0, false, true);
	mscTick();
	check("ejected: back with the logger", mscGetState() == MscState::Idle &&
	      g_reacquires == before + 1);

	printf("USB drive: an unmount with nothing held changes nothing\n");
	mscInit();
	tud_umount_cb();
	check("idle stays idle", mscGetState() == MscState::Idle);

	printf("%s\n", g_failed ? "usb_msc callbacks: FAILED" : "usb_msc callbacks: all checks passed");
	return g_failed ? 1 : 0;
}
