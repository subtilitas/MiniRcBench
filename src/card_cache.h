/**
 * @file card_cache.h
 * @brief The SD write cache (src/sdio/sd_cache.h) for the pin claims of the
 *        tools that save device backups: KST SERVO and AM32 CONFIG.
 *
 * cardCacheBegin() at the claim's start stops a running blackbox log and
 * turns the cache on; card writes then return once queued, and
 * cardCachePoll(), on every pass of core0's loop, sends them. A tool waits
 * for cardCacheState() to read Idle before it writes to its device, so the
 * backup is on the card first. cardCacheEnd() at the claim's end flushes and
 * turns the cache off. Without a mounted card the cache stays off and every
 * call answers Off.
 *
 * A flush that fails leaves the volume as a power loss at that block would:
 * cardCacheRecover() drops the queue and mounts the card again, so FatFs
 * drops the sector it believed written.
 *
 * Core0 only. The host suite links a fake of these calls (test/fakes.cpp).
 *
 * @see docs/design/sd-write-cache.md
 */

#pragma once

#include <stdint.h>

/** @brief Where the cache stands. */
enum class CardCache : uint8_t {
	Off,       /**< Not on: no card at the claim's start, or no claim. */
	Idle,      /**< On, nothing queued. */
	Flushing,  /**< Blocks queued or one in flight. */
	Failed,    /**< A block failed or the card changed; cardCacheRecover() next. */
};

/** @brief Stop a running log and turn the cache on. False with the cache off. */
bool cardCacheBegin();

/** @brief Flush, blocking, and turn the cache off; a failed flush is recovered. */
void cardCacheEnd();

/** @brief One step of the flush; nothing with the cache off. */
void cardCachePoll();

/** @brief The cache's state; @p left, when not NULL, the blocks still to go. */
CardCache cardCacheState(uint16_t *left = nullptr);

/** @brief Flush until Idle or Failed, blocking; Off with the cache off. */
CardCache cardCacheFlush();

/** @brief After Failed: drop the queue and mount the card again. */
void cardCacheRecover();
