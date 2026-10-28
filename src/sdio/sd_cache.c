/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file sd_cache.c
 * @brief The write-back queue and the read cache. @see sd_cache.h
 */

#include "sd_cache.h"

#include <stddef.h>
#include <string.h>

/** @brief One queued block. */
typedef struct {
	uint32_t lba;
	uint8_t  data[SDP_BLOCK_BYTES];
} sdc_block_t;

/** @brief One read-cache entry. */
typedef struct {
	bool     used;
	uint32_t lba;
	uint32_t age;   /**< Larger is more recent. */
	uint8_t  data[SDP_BLOCK_BYTES];
} sdc_entry_t;

static const sdc_dev_t *s_dev;
static bool        s_on;
static sdc_state_t s_state;

static sdc_block_t s_queue[SDC_QUEUE_BLOCKS];
static unsigned    s_head;      /**< Index of the oldest block. */
static unsigned    s_count;     /**< Blocks queued, the one in flight among them. */
static bool        s_inFlight;  /**< The head block is sent and not yet programmed. */
static unsigned    s_attempt;   /**< Tries of the head block so far. */
static bool        s_batchChecked; /**< The swap check ran for this batch. */

static sdc_entry_t s_read[SDC_READ_BLOCKS];
static uint32_t    s_clock;

static uint32_t s_checkLba, s_checkOff;
static uint8_t  s_check[SDC_CHECK_BYTES];
static uint8_t  s_scratch[SDP_BLOCK_BYTES];

static bool retryable(sd_err_t e) {
	return e == SDE_CRC || e == SDE_FRAME || e == SDE_TIMEOUT || e == SDE_NO_RESPONSE;
}

static void dropAll(void) {
	s_head = s_count = 0;
	s_inFlight = false;
	s_attempt = 0;
	s_batchChecked = false;
	for (unsigned i = 0; i < SDC_READ_BLOCKS; i++) s_read[i].used = false;
}

static void fail(void) {
	dropAll();
	s_state = SDC_FAILED;
}

/** @brief The read-cache entry of @p lba, or NULL. */
static sdc_entry_t *cached(uint32_t lba) {
	for (unsigned i = 0; i < SDC_READ_BLOCKS; i++)
		if (s_read[i].used && s_read[i].lba == lba) return &s_read[i];
	return NULL;
}

/** @brief Put @p data into the read cache as @p lba, the least recent entry out. */
static void keep(uint32_t lba, const uint8_t *data) {
	sdc_entry_t *e = cached(lba);
	if (!e) {
		e = &s_read[0];
		for (unsigned i = 0; i < SDC_READ_BLOCKS; i++) {
			if (!s_read[i].used) { e = &s_read[i]; break; }
			if (s_read[i].age < e->age) e = &s_read[i];
		}
	}
	e->used = true;
	e->lba = lba;
	e->age = ++s_clock;
	memcpy(e->data, data, SDP_BLOCK_BYTES);
}

sd_err_t sdc_begin(const sdc_dev_t *dev, uint32_t check_lba, uint32_t check_off) {
	s_on = false;
	if (!dev || check_off + SDC_CHECK_BYTES > SDP_BLOCK_BYTES) return SDE_PARAM;
	sd_err_t e = dev->read(dev->ctx, check_lba, 1, s_scratch);
	if (e != SDE_OK) return e;
	s_dev = dev;
	s_checkLba = check_lba;
	s_checkOff = check_off;
	memcpy(s_check, s_scratch + check_off, SDC_CHECK_BYTES);
	dropAll();
	s_state = SDC_IDLE;
	s_on = true;
	return SDE_OK;
}

void sdc_end(void) {
	s_on = false;
	dropAll();
	s_state = SDC_IDLE;
}

bool sdc_on(void) { return s_on; }

/** @brief Whether the card under the cache is still the one sdc_begin() saw. */
static bool sameCard(void) {
	if (s_dev->read(s_dev->ctx, s_checkLba, 1, s_scratch) != SDE_OK) return false;
	return memcmp(s_scratch + s_checkOff, s_check, SDC_CHECK_BYTES) == 0;
}

void sdc_poll(void) {
	if (!s_on || s_state == SDC_FAILED || s_count == 0) return;
	sdc_block_t *b = &s_queue[s_head];
	if (!s_inFlight) {
		if (!s_batchChecked) {
			if (!sameCard()) {
				fail();
				return;
			}
			s_batchChecked = true;
		}
		sd_err_t e = s_dev->write_start(s_dev->ctx, b->lba, b->data);
		if (e != SDE_OK) {
			if (retryable(e) && ++s_attempt < SDC_ATTEMPTS) return;
			fail();
			return;
		}
		s_inFlight = true;
		return;
	}
	bool done = false;
	sd_err_t e = s_dev->write_poll(s_dev->ctx, &done);
	if (!done) return;
	s_inFlight = false;
	if (e != SDE_OK) {
		if (retryable(e) && ++s_attempt < SDC_ATTEMPTS) return;
		fail();
		return;
	}
	s_attempt = 0;
	s_head = (s_head + 1u) % SDC_QUEUE_BLOCKS;
	if (--s_count == 0) {
		s_batchChecked = false;
		s_state = SDC_IDLE;
	}
}

sdc_state_t sdc_state(uint16_t *left) {
	if (left) *left = (uint16_t)s_count;
	return s_state;
}

sdc_state_t sdc_flush(void) {
	while (s_on && s_state == SDC_FLUSHING) sdc_poll();
	return s_state;
}

void sdc_drop(void) {
	dropAll();
	s_state = SDC_IDLE;
}

/** @brief The newest queued copy of @p lba, or NULL. */
static const sdc_block_t *queued(uint32_t lba) {
	for (unsigned k = s_count; k > 0; k--) {
		const sdc_block_t *b = &s_queue[(s_head + k - 1u) % SDC_QUEUE_BLOCKS];
		if (b->lba == lba) return b;
	}
	return NULL;
}

sd_err_t sdc_read(uint32_t lba, uint32_t n, uint8_t *buf) {
	if (!s_on) return SDE_NOT_READY;
	for (uint32_t i = 0; i < n; i++) {
		// A failed batch dropped blocks newer than the card's: until
		// sdc_drop() the card's copy is not what FatFs wrote.
		if (s_state == SDC_FAILED) return SDE_NOT_READY;
		uint8_t *dst = buf + i * SDP_BLOCK_BYTES;
		const sdc_block_t *q = queued(lba + i);
		if (q) {
			memcpy(dst, q->data, SDP_BLOCK_BYTES);
			continue;
		}
		sdc_entry_t *c = cached(lba + i);
		if (c) {
			c->age = ++s_clock;
			memcpy(dst, c->data, SDP_BLOCK_BYTES);
			continue;
		}
		// A card programming a block takes no read command: the block in
		// flight ends first.
		while (s_inFlight) sdc_poll();
		if (s_state == SDC_FAILED) return SDE_NOT_READY;
		sd_err_t e = s_dev->read(s_dev->ctx, lba + i, 1, dst);
		if (e != SDE_OK) return e;
		keep(lba + i, dst);
	}
	return SDE_OK;
}

sd_err_t sdc_write(uint32_t lba, uint32_t n, const uint8_t *buf) {
	if (!s_on) return SDE_NOT_READY;
	for (uint32_t i = 0; i < n; i++) {
		if (s_state == SDC_FAILED) return SDE_WRITE;
		while (s_count == SDC_QUEUE_BLOCKS && s_state != SDC_FAILED) sdc_poll();
		if (s_state == SDC_FAILED) return SDE_WRITE;
		const uint8_t *src = buf + i * SDP_BLOCK_BYTES;
		sdc_block_t *b = &s_queue[(s_head + s_count) % SDC_QUEUE_BLOCKS];
		b->lba = lba + i;
		memcpy(b->data, src, SDP_BLOCK_BYTES);
		s_count++;
		s_state = SDC_FLUSHING;
		if (cached(lba + i)) keep(lba + i, src);
	}
	return SDE_OK;
}
