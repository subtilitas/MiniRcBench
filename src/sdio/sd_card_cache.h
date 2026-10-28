/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sd_card_cache.h
 * @brief The write cache of sd_cache.h on the board's SDIO card: the calls
 *        sd_card_glue.c offers above the SD library's sd_card_t.
 *
 * While the cache is on, the card's read_blocks and write_blocks callbacks go
 * through it, so FatFs writes return once queued. Each call but
 * sd_sdio_cache_state() takes the card's lock. Core0 only.
 */

#ifndef SD_CARD_CACHE_H
#define SD_CARD_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#include "sd_card.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief sd_sdio_cache_state() with the cache off; the other values are sdc_state_t's. */
#define SD_SDIO_CACHE_OFF (-1)

/**
 * @brief Turn the cache on for card @p c.
 * @param c         The card, initialised.
 * @param check_lba The sector of the volume's serial number.
 * @param check_off Its offset in that sector.
 * @return False with the card not initialised or the sector not read; the
 *         cache stays off.
 */
bool sd_sdio_cache_begin(sd_card_t *c, uint32_t check_lba, uint32_t check_off);

/** @brief One step of the flush; nothing with the cache off. */
void sd_sdio_cache_poll(sd_card_t *c);

/** @brief SD_SDIO_CACHE_OFF or an sdc_state_t; @p left, when not NULL, the blocks to go. */
int sd_sdio_cache_state(uint16_t *left);

/** @brief Flush until idle or failed, blocking; the resulting state. */
int sd_sdio_cache_flush(sd_card_t *c);

/**
 * @brief Drop the queue and the read cache after a failed flush, and mark
 *        the card not ready (STA_NOINIT), so the next mount initialises it.
 */
void sd_sdio_cache_drop(sd_card_t *c);

/**
 * @brief Read one block from card @p c past the cache's queue and read
 *        cache, whether the cache is on or off.
 * @return False with the card not initialised or the read failed.
 */
bool sd_sdio_read_past(sd_card_t *c, uint32_t lba, uint8_t *buf);

/** @brief Flush and turn the cache off. */
void sd_sdio_cache_end(sd_card_t *c);

#ifdef __cplusplus
}
#endif

#endif
