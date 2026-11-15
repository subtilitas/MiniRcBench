/**
 * @file test_sd_cache.cpp
 * @brief The SD write-back queue and read cache (src/sdio/sd_cache.c) over a
 *        fake card: sectors in memory, a busy time per block counted in
 *        polls, a log of every block in the order the card took it, and
 *        errors on demand.
 */

#include "check.h"
#include "../src/sdio/sd_cache.h"
#include "../src/sdio/sd_host.h"
#include "fakes.h"
#include "kst_job.h"
#include "plat.h"

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
	bool     failReadStart = false;  ///< The swap check's read is refused at its start.
	sd_err_t readEndsWith = SDE_OK;  ///< What the swap check's read ends with, when not SDE_OK.
	unsigned readPolls = 0;          ///< Polls the swap check's read stays under way.
	unsigned readLeft = 0;
	bool     reading = false;        ///< The swap check's read is under way.
	uint32_t readLba = 0;
	uint8_t *readBuf = nullptr;
	unsigned startWhileBusy = 0;     ///< A command sent while a block was in flight.
	unsigned readWhileBusy = 0;
	unsigned cmdWhileReading = 0;    ///< A command sent while the check's read was under way.
	unsigned readStarts = 0;         ///< Swap check reads started.
	unsigned readStartTries = 0;     ///< Swap check reads asked for, refused ones included.
	unsigned readFailsLeft = 0;      ///< The next n check reads end with readEndsWith; 0 for all of them.
	unsigned readPollCalls = 0;      ///< Looks at the check's read.
	unsigned writePollCalls = 0;     ///< Looks at a block in flight.
	bool     clockPerLook = false;   ///< Each look at the check's read takes 1 ms of the fake clock.

	std::vector<uint8_t> &sector(uint32_t lba) {
		auto &s = sectors[lba];
		if (s.empty()) s.assign(SDP_BLOCK_BYTES, 0);
		return s;
	}
};

FakeCard g_card;

sd_err_t fakeRead(void *, uint32_t lba, uint32_t n, uint8_t *buf) {
	if (g_card.inFlight) g_card.readWhileBusy++;
	if (g_card.reading) g_card.cmdWhileReading++;
	if (g_card.failRead) return SDE_CRC;
	for (uint32_t i = 0; i < n; i++) {
		memcpy(buf + i * SDP_BLOCK_BYTES, g_card.sector(lba + i).data(), SDP_BLOCK_BYTES);
		g_card.reads++;
	}
	return SDE_OK;
}

sd_err_t fakeStart(void *, uint32_t lba, const uint8_t *buf) {
	if (g_card.inFlight) g_card.startWhileBusy++;
	if (g_card.reading) g_card.cmdWhileReading++;
	g_card.inFlight = true;
	g_card.flightLba = lba;
	g_card.flightData.assign(buf, buf + SDP_BLOCK_BYTES);
	g_card.busyLeft = g_card.busyPolls;
	return SDE_OK;
}

sd_err_t fakePoll(void *, bool *done) {
	*done = false;
	g_card.writePollCalls++;
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

sd_err_t fakeReadStart(void *, uint32_t lba, uint8_t *buf) {
	if (g_card.inFlight) g_card.readWhileBusy++;
	if (g_card.reading) g_card.cmdWhileReading++;
	if (((uintptr_t)buf & 3u) != 0) return SDE_PARAM;   // the receiver's DMA needs it aligned
	g_card.readStartTries++;
	if (g_card.failReadStart) return SDE_NO_RESPONSE;
	g_card.readStarts++;
	g_card.reads++;
	g_card.reading = true;
	g_card.readLba = lba;
	g_card.readBuf = buf;
	g_card.readLeft = g_card.readPolls;
	return SDE_OK;
}

sd_err_t fakeReadPoll(void *, bool *done) {
	*done = false;
	g_card.readPollCalls++;
	if (g_card.clockPerLook) fakeAdvance(1);
	if (!g_card.reading) {
		*done = true;
		return SDE_PARAM;
	}
	if (g_card.readLeft) {
		g_card.readLeft--;
		return SDE_OK;
	}
	*done = true;
	g_card.reading = false;
	if (g_card.failRead) return SDE_CRC;
	if (g_card.readEndsWith != SDE_OK) {
		sd_err_t e = g_card.readEndsWith;
		if (g_card.readFailsLeft && --g_card.readFailsLeft == 0) g_card.readEndsWith = SDE_OK;
		return e;
	}
	memcpy(g_card.readBuf, g_card.sector(g_card.readLba).data(), SDP_BLOCK_BYTES);
	return SDE_OK;
}

const sdc_dev_t DEV = { nullptr, fakeRead, fakeStart, fakePoll, fakeReadStart, fakeReadPoll };

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
	checkTrue("the first poll starts the check's read and returns: nothing sent",
	          g_card.reads == reads + 1 && g_card.reading && !g_card.inFlight);
	sdc_poll();
	checkTrue("the second collects it: the same card, nothing sent yet",
	          !g_card.reading && !g_card.inFlight);
	sdc_poll();
	checkTrue("the third sends the first block", g_card.inFlight && g_card.flightLba == 100);
	sdc_poll();
	checkTrue("busy: no second block sent", g_card.startWhileBusy == 0 && g_card.written.empty());
	checkInt("idle after the flush", sdc_flush(), SDC_IDLE);
	checkTrue("the card took 100, 101, 7, 100, in that order",
	          g_card.written == std::vector<uint32_t>({ 100, 101, 7, 100 }));
	checkTrue("and holds the last copy of 100", g_card.sector(100)[0] == 0x44);
	checkInt("one check per batch", g_card.reads, reads + 1);
	checkTrue("never a command while busy or while the check reads",
	          g_card.startWhileBusy == 0 && g_card.readWhileBusy == 0 && g_card.cmdWhileReading == 0);
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
	sdc_poll();
	checkTrue("a block in flight", g_card.inFlight);
	sdc_read(61, 1, r);
	checkTrue("a miss waits for it: no read while busy",
	          g_card.readWhileBusy == 0 && !g_card.inFlight && g_card.written.back() == 60);

	sdc_write(62, 1, b);
	sdc_write(63, 1, b);
	sdc_poll();
	sdc_poll();
	sdc_poll();
	checkTrue("another block in flight", g_card.inFlight);
	sdc_settle();
	uint16_t left = 0;
	sdc_state(&left);
	checkTrue("settle: the block in flight ends, the next stays queued",
	          !g_card.inFlight && g_card.written.back() == 62 && left == 1);
	sdc_flush();
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
	checkInt("and reads: the card's copy is older than FatFs's", sdc_read(12, 1, b), SDE_NOT_READY);
	sdc_drop();
	checkInt("dropped: reads again", sdc_read(12, 1, b), SDE_OK);

	// A read that misses waits for the block in flight; that block fails.
	block(b, 0x05);
	sdc_write(20, 1, b);
	sdc_write(21, 1, b);
	sdc_poll();
	sdc_poll();
	sdc_poll();
	g_card.failWrites = 3;
	g_card.failWith = SDE_WRITE;
	uint8_t r2[2 * SDP_BLOCK_BYTES];
	checkInt("a miss whose wait ends the batch: refused", sdc_read(22, 2, r2), SDE_NOT_READY);
	g_card.failWrites = 0;
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


static void testCheckAsync() {
	section("SD cache: the swap check's read started in one poll, collected in a later one");
	freshCard();
	sdc_begin(&DEV, 0, 67);
	uint8_t b[SDP_BLOCK_BYTES];
	block(b, 0x09);
	g_card.readPolls = 5;
	sdc_write(30, 1, b);
	sdc_poll();
	checkTrue("the first poll starts the read and returns", g_card.reading && g_card.readStarts == 1);
	for (int i = 0; i < 5; i++) sdc_poll();
	checkTrue("5 polls: the read under way, each poll one look, nothing sent",
	          g_card.reading && !g_card.inFlight && g_card.readStarts == 1);
	uint16_t left = 0;
	checkTrue("flushing, the block queued", sdc_state(&left) == SDC_FLUSHING && left == 1);
	sdc_poll();
	checkTrue("the 7th: the read in, the same card, nothing sent yet", !g_card.reading && !g_card.inFlight);
	sdc_poll();
	checkTrue("the 8th sends the block", g_card.inFlight && g_card.flightLba == 30);
	checkInt("then idle", sdc_flush(), SDC_IDLE);

	// A hit while the check reads touches no card; a miss collects the
	// check's read first.
	sdc_write(31, 1, b);
	sdc_poll();
	uint8_t r[SDP_BLOCK_BYTES];
	unsigned reads = g_card.reads;
	sdc_read(31, 1, r);
	checkTrue("a queued sector while the check reads: no command", g_card.reading &&
	          g_card.reads == reads && g_card.cmdWhileReading == 0);
	sdc_read(90, 1, r);
	checkTrue("a miss: the check's read collected first, then the card read",
	          !g_card.reading && g_card.cmdWhileReading == 0 && g_card.reads == reads + 1);
	checkInt("the batch goes on", sdc_flush(), SDC_IDLE);
	checkTrue("block 31 on the card", g_card.written.back() == 31);

	// settle collects the read too.
	sdc_write(32, 1, b);
	sdc_poll();
	sdc_settle();
	checkTrue("settle: the check's read collected, nothing sent", !g_card.reading && !g_card.inFlight);
	sdc_flush();

	// A start the card refuses, a read that times out, a CRC error: tried
	// again, 3 reads in all, then the batch fails, nothing sent. Another
	// card's serial number fails it at once.
	struct { bool startFails; sd_err_t ends; bool swap; unsigned tries; const char *what; } fails[] = {
		{ true,  SDE_OK,      false, 3, "a start the card refuses, 3 times: failed" },
		{ false, SDE_TIMEOUT, false, 3, "a read that times out, 3 times: failed" },
		{ false, SDE_CRC,     false, 3, "a read with a CRC error, 3 times: failed" },
		{ false, SDE_OK,      true,  1, "another card's serial number: failed at once" },
	};
	for (auto &f : fails) {
		g_card.failReadStart = f.startFails;
		g_card.readEndsWith = f.ends;
		g_card.readFailsLeft = 0;
		if (f.swap) g_card.sector(0)[69] = 0x00;
		size_t before = g_card.written.size();
		unsigned tries = g_card.readStartTries;
		sdc_write(40, 1, b);
		sdc_poll();
		checkTrue("the first try: nothing sent", !g_card.inFlight);
		checkTrue(f.what, sdc_flush() == SDC_FAILED && sdc_state(&left) == SDC_FAILED && left == 0 &&
		          g_card.written.size() == before && !g_card.reading &&
		          g_card.readStartTries - tries == f.tries);
		g_card.failReadStart = false;
		g_card.readEndsWith = SDE_OK;
		g_card.sector(0)[69] = 0xBE;
		sdc_drop();
	}

	// Two failed reads, then a good one: the batch goes on.
	struct { sd_err_t ends; const char *what; } twice[] = {
		{ SDE_CRC,     "two CRC errors, then the read: the block sent" },
		{ SDE_TIMEOUT, "two timeouts, then the read: the block sent" },
	};
	for (auto &t : twice) {
		g_card.readEndsWith = t.ends;
		g_card.readFailsLeft = 2;
		unsigned tries = g_card.readStartTries;
		sdc_write(43, 1, b);
		checkTrue(t.what, sdc_flush() == SDC_IDLE && g_card.written.back() == 43 &&
		          g_card.readStartTries - tries == 3);
	}
	g_card.readEndsWith = SDE_OK;
	g_card.readFailsLeft = 0;
	// The count starts again with each batch.
	g_card.readEndsWith = SDE_CRC;
	g_card.readFailsLeft = 2;
	sdc_write(44, 1, b);
	sdc_flush();
	g_card.readEndsWith = SDE_CRC;
	g_card.readFailsLeft = 2;
	sdc_write(45, 1, b);
	checkTrue("two failed reads in the next batch: it goes on too", sdc_flush() == SDC_IDLE &&
	          g_card.written.back() == 45);
	g_card.readEndsWith = SDE_OK;
	g_card.readFailsLeft = 0;

	// drop and end collect a read under way.
	sdc_write(41, 1, b);
	sdc_poll();
	checkTrue("a read under way", g_card.reading);
	sdc_drop();
	checkTrue("drop: collected, idle", !g_card.reading && sdc_state(nullptr) == SDC_IDLE);
	sdc_write(42, 1, b);
	sdc_poll();
	sdc_end();
	checkTrue("end: collected, off", !g_card.reading && !sdc_on());
}

static unsigned s_hooks = 0;
static void hookCount() { s_hooks++; }

static uint32_t s_lastStampMs = 0, s_worstStampGapMs = 0;
/** @brief KST SERVO's hook in a card phase (ui_kst.cpp), with the gap between stamps kept. */
static void kstStampHook() {
	uint32_t now = millis();
	if (now - s_lastStampMs > s_worstStampGapMs) s_worstStampGapMs = now - s_lastStampMs;
	s_lastStampMs = now;
	kstCardStamp();
}

static void testWaitHook() {
	section("SD cache: every wait on the card calls the driver's wait hook");
	freshCard();
	sdc_begin(&DEV, 0, 67);
	uint8_t b[SDP_BLOCK_BYTES];
	block(b, 0x0A);
	sdh_wait_hook = hookCount;

	// A check's read that does not end for 40 looks, as one that times out.
	g_card.readPolls = 40;
	sdc_write(50, 1, b);
	s_hooks = 0;
	unsigned looks = g_card.readPollCalls, writeLooks = g_card.writePollCalls;
	sdc_flush();
	checkTrue("flush: a hook at every look at the check's read and at the block in flight",
	          s_hooks >= (g_card.readPollCalls - looks) + (g_card.writePollCalls - writeLooks) &&
	          g_card.readPollCalls - looks >= 41);

	// settle, while a block is in flight and while a check reads.
	sdc_write(51, 1, b);
	sdc_poll();
	s_hooks = 0;
	looks = g_card.readPollCalls;
	sdc_settle();
	checkTrue("settle on a check's read: a hook at every look",
	          s_hooks >= g_card.readPollCalls - looks && s_hooks >= 40);
	sdc_poll();
	checkTrue("a block in flight", g_card.inFlight);
	s_hooks = 0;
	writeLooks = g_card.writePollCalls;
	sdc_settle();
	checkTrue("settle on a block in flight: a hook at every look",
	          s_hooks >= g_card.writePollCalls - writeLooks && s_hooks >= 3);

	// drop and end collecting a read; a full queue waiting for room.
	sdc_write(52, 1, b);
	sdc_poll();
	s_hooks = 0;
	sdc_drop();
	checkTrue("drop collecting a check's read: a hook at every look", s_hooks >= 40);
	g_card.readPolls = 0;
	for (uint32_t i = 0; i < SDC_QUEUE_BLOCKS; i++) sdc_write(300 + i, 1, b);
	s_hooks = 0;
	sdc_write(400, 1, b);
	checkTrue("a full queue waiting for room: a hook at every look", s_hooks >= 3);
	g_card.readPolls = 40;
	sdc_write(53, 1, b);
	sdc_flush();
	sdc_write(54, 1, b);
	sdc_poll();
	s_hooks = 0;
	sdc_end();
	checkTrue("end collecting a check's read: a hook at every look", s_hooks >= 40);
	sdh_wait_hook = nullptr;

	// KST SERVO's card phase: a backup's flush whose check read times out
	// three times, 250 looks of 1 ms each: KST's card stamp, the hook, is
	// never more than a look old, so its job sees no lapse.
	sdc_begin(&DEV, 0, 67);
	g_card.readPolls = 250;
	g_card.readEndsWith = SDE_TIMEOUT;
	g_card.clockPerLook = true;
	sdc_write(56, 1, b);
	sdh_wait_hook = kstStampHook;
	s_lastStampMs = millis();
	s_worstStampGapMs = 0;
	uint32_t t0 = millis();
	sdc_state_t end = sdc_flush();
	sdh_wait_hook = nullptr;
	checkTrue("three reads timed out, 750 ms and more: failed", end == SDC_FAILED && millis() - t0 >= 750);
	checkTrue("KST's card stamp at every look: never more than 1 ms old", s_worstStampGapMs <= 1);
	g_card.clockPerLook = false;
	g_card.readEndsWith = SDE_OK;
	g_card.readPolls = 0;
	sdc_drop();
	sdc_end();

	// begin collects a read a cache left under way.
	freshCard();
	sdc_begin(&DEV, 0, 67);
	g_card.readPolls = 5;
	sdc_write(55, 1, b);
	sdc_poll();
	checkTrue("a read under way", g_card.reading);
	unsigned cmds = g_card.cmdWhileReading;
	sdc_begin(&DEV, 0, 67);
	checkTrue("begin again: the read collected first, no command over it",
	          !g_card.reading && g_card.cmdWhileReading == cmds && sdc_on());
	sdc_end();
}

void runSdCacheTests() {
	testQueue();
	testReadCache();
	testFullQueue();
	testErrors();
	testCheckAsync();
	testWaitHook();
}
