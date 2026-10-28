/**
 * @file test_sd_cache.cpp
 * @brief The SD write-back queue and read cache (src/sdio/sd_cache.c) over a
 *        fake card: sectors in memory, a busy time per block counted in
 *        polls, a log of every block in the order the card took it, and
 *        errors on demand.
 */

#include "check.h"
#include "../src/sdio/sd_cache.h"

#include <map>
#include <string.h>
#include <vector>

namespace {

struct FakeCard {
	std::map<uint32_t, std::vector<uint8_t>> sectors;
	std::vector<uint32_t> written;   ///< LBAs in the order they were programmed.
	unsigned reads = 0;
	unsigned busyPolls = 3;          ///< Polls a block stays busy.
	unsigned busyLeft = 0;
	bool     inFlight = false;
	uint32_t flightLba = 0;
	std::vector<uint8_t> flightData;
	unsigned failWrites = 0;         ///< The next n blocks end with this error.
	sd_err_t failWith = SDE_OK;
	bool     failRead = false;
	unsigned startWhileBusy = 0;     ///< A command sent while a block was in flight.
	unsigned readWhileBusy = 0;

	std::vector<uint8_t> &sector(uint32_t lba) {
		auto &s = sectors[lba];
		if (s.empty()) s.assign(SDP_BLOCK_BYTES, 0);
		return s;
	}
};

FakeCard g_card;

sd_err_t fakeRead(void *, uint32_t lba, uint32_t n, uint8_t *buf) {
	if (g_card.inFlight) g_card.readWhileBusy++;
	if (g_card.failRead) return SDE_CRC;
	for (uint32_t i = 0; i < n; i++) {
		memcpy(buf + i * SDP_BLOCK_BYTES, g_card.sector(lba + i).data(), SDP_BLOCK_BYTES);
		g_card.reads++;
	}
	return SDE_OK;
}

sd_err_t fakeStart(void *, uint32_t lba, const uint8_t *buf) {
	if (g_card.inFlight) g_card.startWhileBusy++;
	g_card.inFlight = true;
	g_card.flightLba = lba;
	g_card.flightData.assign(buf, buf + SDP_BLOCK_BYTES);
	g_card.busyLeft = g_card.busyPolls;
	return SDE_OK;
}

sd_err_t fakePoll(void *, bool *done) {
	*done = false;
	if (g_card.busyLeft) {
		g_card.busyLeft--;
		return SDE_OK;
	}
	*done = true;
	g_card.inFlight = false;
	if (g_card.failWrites) {
		g_card.failWrites--;
		return g_card.failWith;
	}
	g_card.sector(g_card.flightLba) = g_card.flightData;
	g_card.written.push_back(g_card.flightLba);
	return SDE_OK;
}

const sdc_dev_t DEV = { nullptr, fakeRead, fakeStart, fakePoll };

/** A fresh card: sector 0 holds the serial DE AD BE EF at offset 67. */
void freshCard() {
	g_card = FakeCard();
	auto &boot = g_card.sector(0);
	boot[67] = 0xDE; boot[68] = 0xAD; boot[69] = 0xBE; boot[70] = 0xEF;
}

void block(uint8_t *b, uint8_t tag) { memset(b, tag, SDP_BLOCK_BYTES); }

}  // namespace

static void testQueue() {
	section("SD cache: writes queue and flush in their order");
	freshCard();
	checkInt("on, the check sector read", sdc_begin(&DEV, 0, 67), SDE_OK);
	checkTrue("on", sdc_on());
	uint8_t b[SDP_BLOCK_BYTES * 2];
	block(b, 0x11);
	block(b + SDP_BLOCK_BYTES, 0x22);
	unsigned reads = g_card.reads;
	checkInt("a write of 2 blocks returns at once", sdc_write(100, 2, b), SDE_OK);
	checkTrue("nothing on the card yet, nothing in flight",
	          g_card.written.empty() && !g_card.inFlight && g_card.reads == reads);
	uint16_t left = 0;
	checkTrue("flushing, 2 blocks left", sdc_state(&left) == SDC_FLUSHING && left == 2);
	block(b, 0x33);
	sdc_write(7, 1, b);
	block(b, 0x44);
	sdc_write(100, 1, b);   // sector 100 again: queued twice, not merged
	sdc_state(&left);
	checkInt("4 blocks queued", left, 4);

	uint8_t r[SDP_BLOCK_BYTES];
	sdc_read(100, 1, r);
	checkTrue("a read of a queued sector: its newest copy", r[0] == 0x44 && g_card.reads == reads);

	sdc_poll();
	checkTrue("the first poll checks the card and sends the first block",
	          g_card.reads == reads + 1 && g_card.inFlight && g_card.flightLba == 100);
	sdc_poll();
	checkTrue("busy: no second block sent", g_card.startWhileBusy == 0 && g_card.written.empty());
	checkInt("idle after the flush", sdc_flush(), SDC_IDLE);
	checkTrue("the card took 100, 101, 7, 100, in that order",
	          g_card.written == std::vector<uint32_t>({ 100, 101, 7, 100 }));
	checkTrue("and holds the last copy of 100", g_card.sector(100)[0] == 0x44);
	checkInt("one check per batch", g_card.reads, reads + 1);
	checkTrue("never a command while busy", g_card.startWhileBusy == 0 && g_card.readWhileBusy == 0);
	checkTrue("idle, none left", sdc_state(&left) == SDC_IDLE && left == 0);
	sdc_end();
}

static void testReadCache() {
	section("SD cache: reads from the cache, misses after the block in flight");
	freshCard();
	sdc_begin(&DEV, 0, 67);
	g_card.sector(50)[0] = 0x50;
	uint8_t r[SDP_BLOCK_BYTES];
	unsigned reads = g_card.reads;
	sdc_read(50, 1, r);
	checkTrue("a miss reads the card", r[0] == 0x50 && g_card.reads == reads + 1);
	sdc_read(50, 1, r);
	checkInt("a hit does not", g_card.reads, reads + 1);
	for (uint32_t i = 0; i < SDC_READ_BLOCKS; i++) sdc_read(200 + i, 1, r);
	reads = g_card.reads;
	sdc_read(50, 1, r);
	checkInt("16 newer sectors push the oldest out", g_card.reads, reads + 1);

	uint8_t b[SDP_BLOCK_BYTES];
	block(b, 0x77);
	sdc_write(50, 1, b);
	sdc_flush();
	reads = g_card.reads;
	sdc_read(50, 1, r);
	checkTrue("a written sector's cache entry holds the new bytes",
	          r[0] == 0x77 && g_card.reads == reads);

	sdc_write(60, 1, b);
	sdc_poll();
	sdc_poll();
	checkTrue("a block in flight", g_card.inFlight);
	sdc_read(61, 1, r);
	checkTrue("a miss waits for it: no read while busy",
	          g_card.readWhileBusy == 0 && !g_card.inFlight && g_card.written.back() == 60);
	sdc_end();
}

static void testFullQueue() {
	section("SD cache: a full queue flushes until the write fits");
	freshCard();
	sdc_begin(&DEV, 0, 67);
	uint8_t b[SDP_BLOCK_BYTES];
	block(b, 0x01);
	for (uint32_t i = 0; i < SDC_QUEUE_BLOCKS; i++) sdc_write(300 + i, 1, b);
	checkTrue("32 queued, none on the card", g_card.written.empty());
	checkInt("the 33rd waits for room and fits", sdc_write(400, 1, b), SDE_OK);
	checkInt("one block programmed to make room", (long)g_card.written.size(), 1);
	uint16_t left = 0;
	sdc_state(&left);
	checkInt("32 left", left, 32);
	sdc_flush();
	sdc_end();
}

static void testErrors() {
	section("SD cache: retries, a failed block, a swapped card");
	freshCard();
	sdc_begin(&DEV, 0, 67);
	uint8_t b[SDP_BLOCK_BYTES];
	block(b, 0x05);
	g_card.failWrites = 2;
	g_card.failWith = SDE_CRC;
	sdc_write(10, 1, b);
	checkInt("two CRC errors: the third try lands", sdc_flush(), SDC_IDLE);
	checkTrue("once on the card", g_card.written == std::vector<uint32_t>({ 10 }));

	g_card.failWrites = 3;
	sdc_write(11, 1, b);
	sdc_write(12, 1, b);
	checkInt("three CRC errors: failed", sdc_flush(), SDC_FAILED);
	uint16_t left = 9;
	checkTrue("the queue dropped", sdc_state(&left) == SDC_FAILED && left == 0);
	checkTrue("block 12 never sent", g_card.written == std::vector<uint32_t>({ 10 }));
	checkInt("writes refused while failed", sdc_write(13, 1, b), SDE_WRITE);
	sdc_drop();
	checkInt("dropped: idle", sdc_state(nullptr), SDC_IDLE);

	g_card.failWrites = 1;
	g_card.failWith = SDE_WRITE;
	sdc_write(14, 1, b);
	checkInt("a write error token: failed at once", sdc_flush(), SDC_FAILED);
	sdc_drop();

	// Another card: its serial differs. Nothing of this card's queue reaches it.
	sdc_write(15, 1, b);
	g_card.sector(0)[68] = 0x00;
	size_t before = g_card.written.size();
	checkInt("another serial number: failed", sdc_flush(), SDC_FAILED);
	checkTrue("and no block written", g_card.written.size() == before);
	sdc_drop();
	g_card.sector(0)[68] = 0xAD;
	sdc_write(16, 1, b);
	g_card.failRead = true;
	checkInt("a check that does not read: failed", sdc_flush(), SDC_FAILED);
	g_card.failRead = false;
	sdc_end();
	checkTrue("off", !sdc_on());
	checkInt("off: reads refused", sdc_read(0, 1, b), SDE_NOT_READY);
	checkInt("off: writes refused", sdc_write(0, 1, b), SDE_NOT_READY);
	g_card.failRead = true;
	checkTrue("a check sector that does not read: the cache stays off",
	          sdc_begin(&DEV, 0, 67) == SDE_CRC && !sdc_on());
	checkInt("an offset past the sector: refused", sdc_begin(&DEV, 0, 510), SDE_PARAM);
}

void runSdCacheTests() {
	testQueue();
	testReadCache();
	testFullQueue();
	testErrors();
}
