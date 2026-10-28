/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sd_cache.h
 * @brief A write-back queue and a read cache in front of the SD card, for the
 *        pin claims of the tools that save device backups.
 *
 * While the cache is on, a block write goes into a queue in SRAM (static
 * random-access memory) and returns at once. sdc_poll(), called on every
 * pass of core0's loop, sends the queue to the card one block at a time, in
 * the order the writes came, and looks at the card's busy line once per call
 * instead of waiting on it. A block read comes from the newest queued copy of
 * its sector, from the read cache, or from the card, after the block in
 * flight has been programmed.
 *
 * Order. FatFs orders its writes so that a power loss leaves a volume a check
 * repairs; the queue keeps that order exactly: a sector written twice is
 * queued twice, and nothing is merged.
 *
 * The swap check. The board has no card-detect line. Before the first block
 * of every batch, 4 bytes of one sector, the volume's serial number in its
 * boot sector, are read past the cache and compared with what sdc_begin()
 * read. A difference or a failed read drops the queue and the read cache and
 * sets SDC_FAILED: no block of one card's directory reaches another card.
 *
 * Errors. A block is tried up to 3 times on a CRC (cyclic redundancy check),
 * frame, timeout or no-response error. A block that still fails ends the batch
 * as SDC_FAILED: the queue is dropped, and the volume holds the blocks before
 * it, as a power loss at that block would leave it. Later writes are refused
 * until sdc_drop().
 *
 * Nothing here touches hardware or FatFs: the card is a table of three
 * functions (sdc_dev_t), so the host suite runs the cache against a fake.
 *
 * @see docs/design/sd-write-cache.md
 */

#ifndef SD_CACHE_H
#define SD_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#include "sd_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDC_QUEUE_BLOCKS 32u   /**< Blocks the write queue holds: 16,384 bytes. */
#define SDC_READ_BLOCKS  16u   /**< Blocks the read cache holds: 8,192 bytes. */
#define SDC_ATTEMPTS     3u    /**< Tries of one block. */
#define SDC_CHECK_BYTES  4u    /**< Bytes of the swap check: a volume serial number. */

/** @brief The card under the cache. */
typedef struct {
	void *ctx;  /**< Passed to every function. */
	/** Read @p n blocks; returns once they are in @p buf. */
	sd_err_t (*read)(void *ctx, uint32_t lba, uint32_t n, uint8_t *buf);
	/** Send one block; returns once it is out, not programmed (sdh_write_start()). */
	sd_err_t (*write_start)(void *ctx, uint32_t lba, const uint8_t *buf);
	/** One look at the block in flight (sdh_write_poll()). */
	sd_err_t (*write_poll)(void *ctx, bool *done);
} sdc_dev_t;

/** @brief Where the queue stands. */
typedef enum {
	SDC_IDLE = 0,     /**< Empty, nothing in flight, the last block clean. */
	SDC_FLUSHING = 1, /**< Blocks queued or one in flight. */
	SDC_FAILED = 2,   /**< A block failed or the card changed; writes refused until sdc_drop(). */
} sdc_state_t;

/**
 * @brief Turn the cache on: read the swap check's sector and keep its bytes.
 * @param dev        The card.
 * @param check_lba  The sector that holds the volume serial number.
 * @param check_off  Its offset in the sector: 67 on FAT32, 100 on exFAT, 39 on FAT12 and FAT16.
 * @return SDE_OK, or the read's error with the cache off.
 */
sd_err_t sdc_begin(const sdc_dev_t *dev, uint32_t check_lba, uint32_t check_off);

/** @brief Turn the cache off. Call it with the queue empty: sdc_flush() first. */
void sdc_end(void);

/** @brief Whether the cache is on. */
bool sdc_on(void);

/** @brief Read @p n blocks: the queue, the read cache, then the card. */
sd_err_t sdc_read(uint32_t lba, uint32_t n, uint8_t *buf);

/**
 * @brief Queue @p n blocks. A queue without room is flushed on the spot until
 *        the blocks fit, core0 blocking for that time.
 * @return SDE_OK; SDE_WRITE while SDC_FAILED, with nothing queued.
 */
sd_err_t sdc_write(uint32_t lba, uint32_t n, const uint8_t *buf);

/** @brief One step of the flush: start a block, or look at the one in flight. */
void sdc_poll(void);

/** @brief The queue's state; @p left, when not NULL, the blocks still to go. */
sdc_state_t sdc_state(uint16_t *left);

/** @brief Flush until SDC_IDLE or SDC_FAILED, core0 blocking. */
sdc_state_t sdc_flush(void);

/** @brief Drop the queue and the read cache and leave SDC_FAILED for SDC_IDLE. */
void sdc_drop(void);

#ifdef __cplusplus
}
#endif

#endif
