/**
 * @file card_cache.cpp
 * @brief The SD write cache on the board's card. @see card_cache.h
 */

#include "card_cache.h"
#include "card_dir.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "sdio/sd_card_cache.h"
#include "sdio/sd_cache.h"

#include "hw_config.h"
#include "sd_card.h"

/** @brief The board's one card, as the SD library numbers it. */
static sd_card_t *card() { return sd_get_by_num(0); }

static CardCache stateOf(int st) {
	switch (st) {
		case SDC_IDLE:     return CardCache::Idle;
		case SDC_FLUSHING: return CardCache::Flushing;
		case SDC_FAILED:   return CardCache::Failed;
		default:           return CardCache::Off;
	}
}

bool cardCacheBegin() {
	// A log has nothing to record while a claim holds the pins, and its
	// writes would fill the queue behind the backup.
	if (sdLogActive()) sdLogStop();
	if (!cardMounted() || !card()) return false;
	uint32_t lba = 0, off = 0;
	if (!sdLogVolumeId(&lba, &off)) return false;
	return sd_sdio_cache_begin(card(), lba, off);
}

void cardCacheEnd() {
	if (!card()) return;
	if (sd_sdio_cache_flush(card()) == SDC_FAILED) cardCacheRecover();
	sd_sdio_cache_end(card());
}

void cardCachePoll() {
	if (card()) sd_sdio_cache_poll(card());
}

CardCache cardCacheState(uint16_t *left) { return stateOf(sd_sdio_cache_state(left)); }

CardCache cardCacheFlush() {
	return card() ? stateOf(sd_sdio_cache_flush(card())) : CardCache::Off;
}

void cardCacheRecover() {
	// USB DRIVE holds the card: a remount is refused, and a card marked for
	// initialisation would fail every read of the host.
	if (!card() || mscGetState() != MscState::Idle) return;
	bool was = sd_sdio_cache_state(nullptr) != SD_SDIO_CACHE_OFF;
	sd_sdio_cache_drop(card());
	sdLogRemount();
	if (!was) return;
	// The card mounted now may be another one: the swap check takes its
	// serial number. Without a mounted card the cache stays off.
	uint32_t lba = 0, off = 0;
	if (!sdLogVolumeId(&lba, &off) || !sd_sdio_cache_begin(card(), lba, off))
		sd_sdio_cache_end(card());
}

bool cardVolumeSerial(uint32_t *serial) {
	*serial = 0;
	uint32_t lba = 0, off = 0;
	if (!card() || !cardMounted() || !sdLogVolumeId(&lba, &off)) return false;
	static uint8_t s_sector[512];
	if (!sd_sdio_read_past(card(), lba, s_sector)) return false;
	const uint8_t *p = s_sector + off;
	*serial = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
	return true;
}
