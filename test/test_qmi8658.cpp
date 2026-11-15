/**
 * @file test_qmi8658.cpp
 * @brief The QMI8658 driver on the shared I2C1 bus, against the register
 *        model of fake_i2c.h: the codec, the probe's three outcomes, the
 *        reset and its two waits, the configuration, the burst and its time
 *        stamp check, start and stop, the faults through the shared bus
 *        record, touch and the INA219 on the same bus, and the bus time of a
 *        frame.
 *
 * The touch driver, src/cst328.cpp, is included in a namespace of its own, as
 * test_sense.cpp includes it, to run touch beside the sensor and the IMU.
 */

#include "check.h"
#include "fakes.h"
#include "fake_i2c.h"
#include "qmi8658.h"
#include "sense.h"
#include "touch.h"
#include "board_desc.h"
#include "board_pins.h"
#include "plat.h"
#include "ui.h"
#include "core1_job.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include <stdio.h>
#include <string.h>

namespace cst_imu {
#include "../src/cst328.cpp"
}

#define IMU QMI8658_I2C_ADDR

/** @brief One UI frame of the IMU: @p ms on, then its tick. */
static void frame(uint32_t ms = 25) {
	fakeAdvance(ms);
	qmi8658Tick(millis());
}

static ImuState state() { return qmi8658State(); }

/**
 * @brief The driver closed, an empty bus, the sensor at NONE, the bus record
 *        free, and 2000 ms on, so no earlier reset holds the next back.
 */
static void freshBus() {
	qmi8658Stop();
	senseConfigure(0);
	fakeI2cReset();
	fakePwmReset();
	senseNote(1, 1);
	fakeAdvance(QMI8658_RESET_GAP_MS);
}

/** @brief A QMI8658 on a fresh bus, the driver opened and the probe and reset run. */
static FakeQmi8658 *started() {
	freshBus();
	FakeQmi8658 *q = fakeI2cAddQmi8658(IMU);
	qmi8658Start(millis());
	frame(0);
	return q;
}

/** @brief A QMI8658 online: reset, configured at 15 ms, the first burst 100 ms later. */
static FakeQmi8658 *online() {
	FakeQmi8658 *q = started();
	frame(QMI8658_RESET_A_MS);
	frame(QMI8658_WARMUP_MS);
	return q;
}

/** @brief Writes of data logged from @p from on. */
static uint32_t writesFrom(const FakeQmi8658 *q, uint32_t from = 0) {
	uint32_t n = 0;
	for (uint32_t i = from; i < q->logN; i++)
		if (q->log[i].write) n++;
	return n;
}

/** @brief The index of the first log entry from @p from that matches; logN for none. */
static uint32_t firstOp(const FakeQmi8658 *q, bool write, uint8_t reg, uint32_t from = 0) {
	for (uint32_t i = from; i < q->logN; i++)
		if (q->log[i].write == write && q->log[i].reg == reg) return i;
	return q->logN;
}

// ---------------------------------------------------------------------------
// codec
// ---------------------------------------------------------------------------

static void testCodec() {
	section("IMU: the burst decoded little-endian");
	const uint8_t b[11] = { 0x56, 0x34, 0x12, 0x80, 0x1F, 0xC7, 0xCF, 0x02, 0x00, 0x00, 0xC0 };
	ImuSample s;
	memset(&s, 0, sizeof(s));
	qmi8658Decode(b, &s);
	checkInt("the time stamp, 24 bit", (long)s.stamp, 0x123456);
	checkInt("the temperature: 31.5 C, 8064", s.tempRaw, 31 * 256 + 128);
	checkInt("X: CFC7h is -12345", s.acc[0], -12345);
	checkInt("Y: 0002h is 2", s.acc[1], 2);
	checkInt("Z: C000h is -16384, -1 g", s.acc[2], -16384);
	const uint8_t top[3] = { 0xFF, 0xFF, 0xFF };
	checkInt("the stamp's top: FFFFFFh", (long)qmi8658Stamp(top), 0xFFFFFF);

	section("IMU: the board's axis map");
	checkTrue("BOARD_IMU_AXES names each axis once", qmi8658AxesValid(BOARD_IMU_AXES));
	checkTrue("signs turned: valid", qmi8658AxesValid(-2, 1, -3));
	checkTrue("an axis twice: not", !qmi8658AxesValid(1, -1, 3));
	checkTrue("Y twice: not", !qmi8658AxesValid(3, 2, -2));
	checkTrue("X along the normal and the long edge: not", !qmi8658AxesValid(1, 2, 1));
	checkTrue("0: not", !qmi8658AxesValid(0, 2, 3) && !qmi8658AxesValid(1, 0, 3) &&
	          !qmi8658AxesValid(1, 2, 0));
	checkTrue("4: not", !qmi8658AxesValid(4, 2, 3) && !qmi8658AxesValid(1, -4, 3) &&
	          !qmi8658AxesValid(1, 2, 4));
	checkTrue("-4: not", !qmi8658AxesValid(-4, 2, 3) && !qmi8658AxesValid(1, 2, -4));
}

// ---------------------------------------------------------------------------
// closed, probe
// ---------------------------------------------------------------------------

static void testClosed() {
	section("IMU: closed, nothing sent to 0x6B");
	freshBus();
	fakeI2cAddQmi8658(IMU);
	for (int i = 0; i < 80; i++) frame();
	checkInt("80 frames with the part present", fakeI2cTransactionsTo(IMU), 0);
	checkTrue("the state reads closed", state() == ImuState::Closed);
	qmi8658Stop();
	checkInt("a stop while closed sends nothing", fakeI2cTransactionsTo(IMU), 0);
}

static void testProbe() {
	section("IMU: no part, NO IMU, probed every 1000 ms");
	online();
	freshBus();
	qmi8658Start(millis());
	checkTrue("opened: starting", state() == ImuState::Starting);
	checkTrue("WHO_AM_I and REVISION_ID 0 until read",
	          qmi8658WhoAmI() == 0 && qmi8658Revision() == 0);
	frame(0);
	checkTrue("WHO_AM_I not answered: absent", state() == ImuState::Absent);
	checkInt("one transaction, the address NACKed", fakeI2cTransactionsTo(IMU), 1);
	checkTrue("a NACK does not stick the bus", !senseBus()->stuck);
	frame(999);
	checkInt("999 ms on: no probe", fakeI2cTransactionsTo(IMU), 1);
	frame(1);
	checkInt("1000 ms: probed again", fakeI2cTransactionsTo(IMU), 2);
	FakeQmi8658 *q = fakeI2cAddQmi8658(IMU);
	frame(1000);
	checkTrue("a part put in: found, reset, starting", state() == ImuState::Starting &&
	          q->resets == 1);

	section("IMU: another part, NOT A QMI8658, nothing written");
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	q->reg[QMI8658_REG_WHO_AM_I] = 0x3C;
	qmi8658Start(millis());
	frame(0);
	checkTrue("WHO_AM_I 3Ch: another part", state() == ImuState::WrongId);
	checkInt("the ID kept for the screen", qmi8658WhoAmI(), 0x3C);
	checkInt("no write", writesFrom(q), 0);
	checkInt("one read, WHO_AM_I", (long)q->logN, 1);
	frame(999);
	checkInt("999 ms on: not read again", (long)q->logN, 1);
	frame(1);
	checkInt("1000 ms: read again, still nothing written",
	         (long)(q->logN * 10 + writesFrom(q)), 20);
	qmi8658Stop();
	checkInt("stopped: nothing written to another part", writesFrom(q), 0);

	section("IMU: a QMI8658: WHO_AM_I, REVISION_ID, the reset");
	q = started();
	checkTrue("starting", state() == ImuState::Starting);
	checkInt("WHO_AM_I 05h", qmi8658WhoAmI(), 0x05);
	checkInt("REVISION_ID read, 7Ch", qmi8658Revision(), 0x7C);
	checkTrue("in that order", q->logN == 3 && !q->log[0].write && q->log[0].reg == 0x00 &&
	          !q->log[1].write && q->log[1].reg == 0x01);
	checkTrue("then B0h to RESET, one byte", q->log[2].write && q->log[2].reg == 0x60 &&
	          q->log[2].val == 0xB0 && q->log[2].len == 1 && q->resets == 1);
	checkInt("counted", (long)qmi8658Counts()->resets, 1);

	section("IMU: the probe failing after WHO_AM_I");
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	qmi8658Start(millis());
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	frame(0);
	checkTrue("REVISION_ID not answered: NO IMU, no reset",
	          state() == ImuState::Absent && q->resets == 0 && writesFrom(q) == 0);
	checkInt("one failure counted", (long)qmi8658Counts()->fails, 1);
	frame(1000);
	checkTrue("1000 ms on: found and reset", q->resets == 1 && state() == ImuState::Starting);

	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	qmi8658Start(millis());
	uint32_t at = millis();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 4);
	frame(0);
	checkTrue("the reset's write not answered: NO IMU", state() == ImuState::Absent &&
	          q->resets == 0);
	frame(1000);
	checkInt("1000 ms on: no probe, the reset counts from its attempt", (long)q->logN, 2);
	frame(QMI8658_RESET_GAP_MS - 1001);
	checkInt("1999 ms: none", (long)q->logN, 2);
	frame(1);
	checkTrue("2000 ms: probed and reset", q->resets == 1 &&
	          q->log[q->logN - 1].ms - at == QMI8658_RESET_GAP_MS);
}

// ---------------------------------------------------------------------------
// reset and configuration
// ---------------------------------------------------------------------------

/** @brief The four configuration writes after log entry @p from: single, read back, in order. */
static bool configuredFrom(const FakeQmi8658 *q, uint32_t from) {
	static const uint8_t want[4][2] = { { 0x02, 0x40 }, { 0x03, 0x08 }, { 0x06, 0x07 }, { 0x08, 0x01 } };
	uint32_t i = from;
	for (const auto &w : want) {
		if (i + 1 >= q->logN) return false;
		const FakeQmiOp &a = q->log[i], &b = q->log[i + 1];
		if (!a.write || a.reg != w[0] || a.val != w[1] || a.len != 1) return false;
		if (b.write || b.reg != w[0] || b.len != 1) return false;
		i += 2;
	}
	return true;
}

static void testResetA() {
	section("IMU: the A part, dQY_L 80h at 15 ms, configured at once");
	FakeQmi8658 *q = started();
	uint32_t n = q->logN;
	frame(QMI8658_RESET_A_MS - 1);
	checkInt("14 ms: nothing", (long)(q->logN - n), 0);
	frame(1);
	checkTrue("15 ms: dQY_L read", firstOp(q, false, QMI8658_REG_DQY_L, n) == n);
	checkTrue("then CTRL1, CTRL2, CTRL5 and CTRL7, each one byte, each read back",
	          configuredFrom(q, n + 1));
	checkTrue("in the same frame", q->log[q->logN - 1].ms == q->log[n].ms);
	checkTrue("dQY_L before CTRL7", firstOp(q, false, QMI8658_REG_DQY_L) <
	          firstOp(q, true, QMI8658_REG_CTRL7));
	checkInt("no burst write", (long)q->burstWrites, 0);
	checkTrue("the registers: 40h, 08h, 07h, 01h",
	          q->reg[0x02] == 0x40 && q->reg[0x03] == 0x08 && q->reg[0x06] == 0x07 &&
	          q->reg[0x08] == 0x01);
	checkTrue("still starting", state() == ImuState::Starting);
	n = q->logN;
	frame(QMI8658_WARMUP_MS - 1);
	checkInt("99 ms after CTRL7: nothing read", (long)(q->logN - n), 0);
	frame(1);
	checkTrue("100 ms: online, the burst and the time stamp",
	          state() == ImuState::Online && q->logN - n == 2 && q->log[n].reg == 0x30 &&
	          q->log[n].len == 11 && q->log[n + 1].reg == 0x30 && q->log[n + 1].len == 3);
	checkTrue("a sample taken", qmi8658Latest()->seq == 1 && qmi8658Counts()->samples == 1);
}

static void testResetC() {
	section("IMU: dQY_L another value, the wait to 1750 ms");
	FakeQmi8658 *q = fakeI2cAddQmi8658(IMU);
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	q->dqyAfterReset = 0x00;
	qmi8658Start(millis());
	frame(0);
	uint32_t resetMs = q->log[2].ms;
	frame(QMI8658_RESET_A_MS);
	checkTrue("15 ms: dQY_L reads 00h, nothing written",
	          q->log[q->logN - 1].reg == QMI8658_REG_DQY_L && writesFrom(q) == 1);
	frame(QMI8658_RESET_C_MS - QMI8658_RESET_A_MS - 1);
	checkInt("1749 ms: nothing more", (long)q->logN, 4);
	frame(1);
	checkTrue("1750 ms: WHO_AM_I read, then the configuration",
	          q->log[4].reg == 0x00 && !q->log[4].write && configuredFrom(q, 5) &&
	          q->log[5].ms - resetMs == QMI8658_RESET_C_MS);
	frame(QMI8658_WARMUP_MS);
	checkTrue("online 100 ms later", state() == ImuState::Online);

	section("IMU: the bus stuck when dQY_L is due");
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	qmi8658Start(millis());
	frame(0);
	senseNote(PICO_ERROR_TIMEOUT, 1);
	senseNote(PICO_ERROR_TIMEOUT, 1);
	frame(QMI8658_RESET_A_MS);
	checkInt("15 ms, stuck: dQY_L not read", (long)fakeQmiCount(q, false, QMI8658_REG_DQY_L), 0);
	senseNote(1, 1);
	frame();
	checkTrue("free: dQY_L read, configured, no wait for 1750 ms",
	          fakeQmiCount(q, false, QMI8658_REG_DQY_L) == 1 && configuredFrom(q, 4));

	section("IMU: the C part answering nothing for 1750 ms");
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	q->dqyAfterReset = 0x00;
	q->bootMs = QMI8658_RESET_C_MS;
	qmi8658Start(millis());
	frame(0);
	uint32_t t0 = fakeI2cTransactionsTo(IMU);
	frame(QMI8658_RESET_A_MS);
	checkTrue("15 ms: dQY_L not answered, the bus not stuck",
	          fakeI2cTransactionsTo(IMU) == t0 + 1 && !senseBus()->stuck &&
	          state() == ImuState::Starting);
	checkInt("counted as a failure", (long)qmi8658Counts()->fails, 1);
	for (int i = 0; i < 69; i++) frame();
	checkInt("up to 1740 ms: nothing sent", fakeI2cTransactionsTo(IMU), t0 + 1);
	frame(10);
	checkTrue("1750 ms: answers, configured", configuredFrom(q, 1 + firstOp(q, false, 0x00, 2)));
	frame(QMI8658_WARMUP_MS);
	checkTrue("online", state() == ImuState::Online);

	section("IMU: WHO_AM_I at 1750 ms");
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	q->dqyAfterReset = 0x00;
	qmi8658Start(millis());
	frame(0);
	frame(QMI8658_RESET_A_MS);
	q->whoAmI = 0x11;
	q->reg[0x00] = 0x11;
	frame(QMI8658_RESET_C_MS - QMI8658_RESET_A_MS);
	checkTrue("another value: not a QMI8658, nothing written after the reset",
	          state() == ImuState::WrongId && writesFrom(q) == 1 && qmi8658WhoAmI() == 0x11);
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	q->dqyAfterReset = 0x00;
	qmi8658Start(millis());
	frame(0);
	frame(QMI8658_RESET_A_MS);
	fakeI2cRemove(IMU);
	frame(QMI8658_RESET_C_MS - QMI8658_RESET_A_MS);
	checkTrue("no answer: NO IMU", state() == ImuState::Absent);
}

static void testMismatch() {
	section("IMU: a read-back mismatch, no second reset before 2000 ms");
	FakeQmi8658 *q = started();
	q->refuseReg = QMI8658_REG_CTRL2;
	uint32_t resetMs = q->log[2].ms;
	frame(QMI8658_RESET_A_MS);
	checkTrue("CTRL2 reads 00h: NO IMU", state() == ImuState::Absent);
	checkInt("CTRL5 and CTRL7 not written", (long)(fakeQmiCount(q, true, 0x06) +
	         fakeQmiCount(q, true, 0x08)), 0);
	for (int i = 0; i < 79; i++) frame();
	checkTrue("1990 ms: no new probe, one reset", q->resets == 1 && state() == ImuState::Absent);
	frame(9);
	checkInt("1999 ms: one reset", (long)q->resets, 1);
	q->refuseReg = 0;
	frame(1);
	checkInt("2000 ms: the second", (long)q->resets, 2);
	uint32_t second = q->log[firstOp(q, true, 0x60, firstOp(q, true, 0x60) + 1)].ms;
	checkInt("2000 ms after the first", (long)(second - resetMs), 2000);
	frame(QMI8658_RESET_A_MS);
	frame(QMI8658_WARMUP_MS);
	checkTrue("then online", state() == ImuState::Online);

	section("IMU: a read-back not answered");
	q = started();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 3);
	frame(QMI8658_RESET_A_MS);
	checkTrue("CTRL1's read-back NACKed: NO IMU, CTRL2 not written",
	          state() == ImuState::Absent && fakeQmiCount(q, true, 0x03) == 0);
	checkInt("one failure counted", (long)qmi8658Counts()->fails, 1);

	section("IMU: a configuration write not answered");
	q = started();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	frame(QMI8658_RESET_A_MS);
	checkTrue("CTRL1's write NACKed: NO IMU, not read back",
	          state() == ImuState::Absent && fakeQmiCount(q, false, 0x02) == 0);
	checkInt("one failure counted", (long)qmi8658Counts()->fails, 1);

	section("IMU: closed and opened again, the reset still waits for 2000 ms");
	q = started();
	resetMs = q->log[2].ms;
	frame(QMI8658_RESET_A_MS);
	qmi8658Stop();
	frame(85);
	qmi8658Start(millis());
	frame(0);
	checkInt("opened at 100 ms: no probe yet", (long)q->resets, 1);
	for (int i = 0; i < 75; i++) frame();
	checkInt("1975 ms: still one reset", (long)q->resets, 1);
	frame(25);
	checkInt("2000 ms: the second", (long)q->resets, 2);
	checkInt("2000 ms after the first",
	         (long)(q->log[firstOp(q, true, 0x60, 3)].ms - resetMs), 2000);

	section("IMU: opened again 1999 ms after the reset");
	q = started();
	resetMs = q->log[2].ms;
	qmi8658Stop();
	frame(QMI8658_RESET_GAP_MS - 1);
	qmi8658Start(millis());
	frame(0);
	checkInt("1999 ms: no probe", (long)q->logN, 3);
	frame(1);
	checkTrue("2000 ms: probed and reset", q->resets == 2 &&
	          q->log[q->logN - 1].ms - resetMs == QMI8658_RESET_GAP_MS);
}

// ---------------------------------------------------------------------------
// samples
// ---------------------------------------------------------------------------

static void testSamples() {
	section("IMU: samples decoded, a time stamp repeated not counted");
	FakeQmi8658 *q = online();
	checkInt("CTRL1 40h: little-endian, auto-increment", q->reg[0x02], 0x40);
	q->acc[0] = -12345;
	q->acc[1] = 2;
	q->acc[2] = -16384;
	q->temp = 31 * 256 + 128;
	uint32_t s0 = q->stamp;
	for (int i = 0; i < 2; i++) frame();
	const ImuSample *s = qmi8658Latest();
	checkTrue("X, Y, Z: -12345, 2, -16384", s->acc[0] == -12345 && s->acc[1] == 2 &&
	          s->acc[2] == -16384);
	checkInt("the temperature, 256 a degree", s->tempRaw, 31 * 256 + 128);
	checkTrue("the time stamp the part's", s->stamp == q->stamp && s->stamp > s0);
	checkInt("taken now", (long)s->takenMs, (long)millis());

	ImuCounts c0 = *qmi8658Counts();
	uint32_t st0 = q->stamp;
	for (int i = 0; i < 32; i++) frame();
	const ImuCounts *c = qmi8658Counts();
	checkInt("32 frames of 25 ms: 25 samples of 32 ms", (long)(c->samples - c0.samples), 25);
	checkInt("and each sample once", (long)(c->samples - c0.samples), (long)(q->stamp - st0));
	checkInt("7 frames repeated a time stamp", (long)(c->repeats - c0.repeats), 7);
	checkInt("none torn", (long)(c->torn - c0.torn), 0);
	checkInt("seq counts the samples", (long)qmi8658Latest()->seq, (long)c->samples);

	section("IMU: a sample changing during the burst, dropped");
	uint32_t seq = qmi8658Latest()->seq;
	int16_t x = qmi8658Latest()->acc[0];
	q->acc[0] = 999;
	q->tearNext = true;
	frame();
	checkInt("the burst dropped", (long)(qmi8658Counts()->torn - c0.torn), 1);
	checkTrue("its bytes not taken", qmi8658Latest()->seq == seq && qmi8658Latest()->acc[0] == x);
	frame();
	checkTrue("the next frame takes the new sample", qmi8658Latest()->seq == seq + 1 &&
	          qmi8658Latest()->acc[0] == 999 && qmi8658Latest()->stamp == q->stamp);

	section("IMU: the time stamp across its 24-bit wrap");
	q->stamp = 0xFFFFFE;
	uint32_t taken = qmi8658Counts()->samples;
	frame(32);
	checkTrue("FFFFFFh taken", qmi8658Latest()->stamp == 0xFFFFFF);
	frame(32);
	checkTrue("then 0 taken as a new sample",
	          qmi8658Latest()->stamp == 0 && qmi8658Counts()->samples == taken + 2);
}

// ---------------------------------------------------------------------------
// faults
// ---------------------------------------------------------------------------

static void testFaults() {
	section("IMU: three failed frames take it offline");
	FakeQmi8658 *q = online();
	// 2000 ms online first, so the reset's gap holds no probe back.
	for (int i = 0; i < 80; i++) frame();
	fakeI2cRemove(IMU);
	frame();
	frame();
	checkTrue("two NACKed bursts: still online", state() == ImuState::Online);
	frame();
	checkTrue("the third: offline", state() == ImuState::Offline);
	checkTrue("NACKs do not stick the bus", !senseBus()->stuck);
	checkInt("three failures counted", (long)qmi8658Counts()->fails, 3);
	uint32_t n = fakeI2cTransactionsTo(IMU);
	frame(999);
	checkInt("999 ms on: no probe", fakeI2cTransactionsTo(IMU), n);
	q = fakeI2cAddQmi8658(IMU);
	frame(1);
	checkTrue("1000 ms: probed, reset again", q->resets == 1 && state() == ImuState::Starting);
	frame(QMI8658_RESET_A_MS);
	frame(QMI8658_WARMUP_MS);
	checkTrue("and online", state() == ImuState::Online);

	section("IMU: back from offline, the first sample taken whatever its stamp");
	q = online();
	for (int i = 0; i < 80; i++) frame();
	q->stamp = 2;
	frame(32);
	checkInt("a sample with time stamp 3", (long)qmi8658Latest()->stamp, 3);
	fakeI2cRemove(IMU);
	for (int i = 0; i < 3; i++) frame();
	q = fakeI2cAddQmi8658(IMU);
	frame(1000);
	frame(QMI8658_RESET_A_MS);
	uint32_t seq0 = qmi8658Latest()->seq;
	frame(QMI8658_WARMUP_MS);
	checkTrue("a new part's third sample, time stamp 3 again, taken",
	          state() == ImuState::Online && q->stamp == 3 && qmi8658Latest()->seq == seq0 + 1);

	section("IMU: back from offline, the count starts at 0");
	for (int i = 0; i < 80; i++) frame();
	fakeI2cRemove(IMU);
	for (int i = 0; i < 3; i++) frame();
	checkTrue("offline again", state() == ImuState::Offline);
	q = fakeI2cAddQmi8658(IMU);
	frame(1000);
	frame(QMI8658_RESET_A_MS);
	fakeI2cFailNext(1, PICO_ERROR_GENERIC);
	frame(QMI8658_WARMUP_MS);
	checkTrue("the first burst NACKed: not offline", state() == ImuState::Starting);
	frame();
	checkTrue("the next frame: online", state() == ImuState::Online);

	section("IMU: a failure and a good frame start the count again");
	q = online();
	fakeI2cFailNext(2, PICO_ERROR_GENERIC);
	frame();
	frame();
	frame();
	fakeI2cFailNext(2, PICO_ERROR_GENERIC);
	frame();
	frame();
	checkTrue("two, a good frame, two: online", state() == ImuState::Online);
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	frame();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	frame();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	frame();
	checkTrue("the time stamp read failing three frames: offline", state() == ImuState::Offline);

	section("IMU: a data byte not acknowledged");
	q = online();
	uint32_t seq = qmi8658Latest()->seq;
	fakeI2cShortNext(1);
	frame(32);
	checkTrue("the burst one byte short: no sample, one failure, the bus not stuck",
	          qmi8658Latest()->seq == seq && qmi8658Counts()->fails == 1 && !senseBus()->stuck);

	section("IMU: two timeouts stick the shared bus, the IMU then sends nothing");
	q = online();
	fakeI2cFailNext(100, PICO_ERROR_TIMEOUT);
	FakeI2cStats a = fakeI2cStats();
	frame();
	checkTrue("one timeout: noted, not stuck", senseBus()->timeouts == 1 && !senseBus()->stuck);
	checkInt("the frame ends at it", (long)(fakeI2cStats().transfers - a.transfers), 1);
	frame();
	checkTrue("two: the bus stuck", senseBus()->stuck);
	a = fakeI2cStats();
	frame();
	frame();
	checkInt("stuck: nothing sent", (long)(fakeI2cStats().transfers - a.transfers), 0);
	checkTrue("not offline by the stuck bus", state() == ImuState::Online);
	fakeI2cFailNext(0, 0);
	uint32_t r0 = senseBus()->recoveries;
	senseTick(millis());
	checkTrue("the sensor's tick recovers it at NONE", senseBus()->recoveries == r0 + 1);
	frame();
	checkInt("the IMU does not test the bus", (long)(fakeI2cStats().transfers - a.transfers), 0);
	senseNote(5, 5);
	checkTrue("a touch read that answers frees it", !senseBus()->stuck);
	frame();
	frame(32);
	checkTrue("then the IMU reads, its count started again",
	          qmi8658Counts()->samples > 0 && state() == ImuState::Online);

	section("IMU: SDA held, arbitration lost, a line low");
	q = online();
	fakeI2cHeldSdaCode(PICO_ERROR_GENERIC);
	fakeI2cSdaReleaseAfter(5);
	a = fakeI2cStats();
	frame();
	checkTrue("one transfer, the bus stuck at once", senseBus()->stuck &&
	          fakeI2cStats().transfers - a.transfers == 1);
	checkInt("one failure counted", (long)qmi8658Counts()->fails, 1);
	senseTick(millis());
	checkTrue("the recovery's clocks free SDA", gpio_get(g_board->sda));
	senseNote(5, 5);
	frame();
	frame(32);
	checkTrue("reading again", !senseBus()->stuck && state() == ImuState::Online &&
	          qmi8658Counts()->samples > 1);
	fakeI2cHeldSdaCode(PICO_ERROR_TIMEOUT);

	section("IMU: a start held while the bus is stuck");
	freshBus();
	q = fakeI2cAddQmi8658(IMU);
	senseNote(PICO_ERROR_TIMEOUT, 1);
	senseNote(PICO_ERROR_TIMEOUT, 1);
	qmi8658Start(millis());
	frame(0);
	frame();
	checkInt("stuck: no probe", fakeI2cTransactionsTo(IMU), 0);
	senseNote(1, 1);
	frame();
	checkTrue("free: probed and reset", q->resets == 1 && state() == ImuState::Starting);
}

// ---------------------------------------------------------------------------
// stop
// ---------------------------------------------------------------------------

static void testStop() {
	section("IMU: BACK turns the accelerometer off");
	FakeQmi8658 *q = online();
	uint32_t n = q->logN;
	qmi8658Stop();
	checkTrue("one write: CTRL7 00h, one byte", q->logN == n + 1 && q->log[n].write &&
	          q->log[n].reg == 0x08 && q->log[n].val == 0x00 && q->log[n].len == 1);
	checkTrue("the accelerometer off, the driver closed",
	          q->reg[0x08] == 0x00 && state() == ImuState::Closed);
	uint32_t t = fakeI2cTransactionsTo(IMU);
	for (int i = 0; i < 80; i++) frame();
	qmi8658Stop();
	checkInt("closed: nothing sent for 80 frames, nor by a second stop",
	         fakeI2cTransactionsTo(IMU), t);
	fakeAdvance(QMI8658_RESET_GAP_MS);
	qmi8658Start(millis());
	frame(0);
	checkTrue("opened again: probed and reset", q->resets == 2 && state() == ImuState::Starting);
	checkTrue("the counts start again", qmi8658Counts()->samples == 0 &&
	          qmi8658Latest()->seq == 0);

	section("IMU: a stop with the accelerometer not on sends nothing");
	q = started();
	t = fakeI2cTransactionsTo(IMU);
	qmi8658Stop();
	checkInt("after the reset, before CTRL7", fakeI2cTransactionsTo(IMU), t);
	freshBus();
	qmi8658Start(millis());
	frame(0);
	t = fakeI2cTransactionsTo(IMU);
	qmi8658Stop();
	checkInt("no IMU", fakeI2cTransactionsTo(IMU), t);

	section("IMU: a second start while open changes nothing");
	q = online();
	uint32_t seq = qmi8658Latest()->seq;
	uint32_t resets = q->resets;
	qmi8658Start(millis());
	frame();
	frame(32);
	checkTrue("still online, the samples counted on, no new reset",
	          state() == ImuState::Online && qmi8658Latest()->seq > seq && q->resets == resets);

	section("IMU: a stop after a new reset, before CTRL7, sends nothing");
	q = online();
	for (int i = 0; i < 80; i++) frame();
	fakeI2cRemove(IMU);
	for (int i = 0; i < 3; i++) frame();
	q = fakeI2cAddQmi8658(IMU);
	frame(1000);
	checkTrue("offline, probed, reset again", q->resets == 1 && state() == ImuState::Starting);
	t = fakeI2cTransactionsTo(IMU);
	qmi8658Stop();
	checkInt("stopped before CTRL7: nothing sent", fakeI2cTransactionsTo(IMU), t);

	section("IMU: a stop, the part gone, a start and a stop");
	q = online();
	qmi8658Stop();
	fakeI2cRemove(IMU);
	fakeAdvance(QMI8658_RESET_GAP_MS);
	qmi8658Start(millis());
	frame(0);
	t = fakeI2cTransactionsTo(IMU);
	qmi8658Stop();
	checkTrue("no IMU: the second stop sends nothing", state() == ImuState::Closed &&
	          fakeI2cTransactionsTo(IMU) == t);

	section("IMU: a stop while the bus is stuck sends nothing");
	q = online();
	senseNote(PICO_ERROR_TIMEOUT, 1);
	senseNote(PICO_ERROR_TIMEOUT, 1);
	t = fakeI2cTransactionsTo(IMU);
	qmi8658Stop();
	checkTrue("nothing sent, closed", fakeI2cTransactionsTo(IMU) == t &&
	          state() == ImuState::Closed);
	senseNote(1, 1);

	section("IMU: a stop after CTRL7 failed to read back");
	q = started();
	q->refuseReg = QMI8658_REG_CTRL7;
	frame(QMI8658_RESET_A_MS);
	checkTrue("NO IMU", state() == ImuState::Absent);
	n = q->logN;
	qmi8658Stop();
	checkTrue("CTRL7 00h written all the same", q->logN == n + 1 && q->log[n].reg == 0x08 &&
	          q->log[n].val == 0x00);
}

// ---------------------------------------------------------------------------
// the shared bus
// ---------------------------------------------------------------------------

/** @brief The bus time of @p clocks at 400 kHz, 2.5 µs a clock rounded up, plus @p timeoutUs. */
static uint32_t busUs(uint32_t clocks, uint32_t timeoutUs) {
	return (clocks * 5u + 1u) / 2u + timeoutUs;
}

/** @brief The clocks of one transfer of @p bytes after the address. */
static uint32_t xferClocks(uint32_t bytes, bool restart) {
	return 9u * (1u + bytes) + (restart ? 1u : 2u);
}

static uint32_t busBetween(const FakeI2cStats &a, const FakeI2cStats &b) {
	return busUs(b.clocks - a.clocks, b.timeoutUs - a.timeoutUs);
}

/** @brief Per client, the clocks of one frame. */
struct Clocks { uint32_t touch, sense, imu; bool touchRead; };

/** @brief One frame of touch, the sensor and the IMU in uiTick()'s order. */
static Clocks busFrame(TouchState *ts) {
	fakeAdvance(25);
	uint32_t reads = fakeI2cTransactionsTo(CST328_I2C_ADDR);
	FakeI2cStats a = fakeI2cStats();
	cst_imu::touchPoll(ts);
	FakeI2cStats b = fakeI2cStats();
	senseTick(millis());
	FakeI2cStats c = fakeI2cStats();
	qmi8658Tick(millis());
	FakeI2cStats d = fakeI2cStats();
	return { b.clocks - a.clocks, c.clocks - b.clocks, d.clocks - c.clocks,
	         fakeI2cTransactionsTo(CST328_I2C_ADDR) == reads + 1 };
}

static void testSharedBus() {
	section("IMU: touch, the INA219 and the IMU on one bus");
	freshBus();
	fakeI2cAddCst328(CST328_I2C_ADDR);
	checkTrue("touch found", cst_imu::touchInit());
	senseConfigure(1);
	fakeI2cAddIna219(INA219_ADDR);
	FakeQmi8658 *q = fakeI2cAddQmi8658(IMU);
	TouchState ts;
	memset(&ts, 0, sizeof(ts));
	busFrame(&ts);
	qmi8658Start(millis());
	for (int i = 0; i < 6; i++) busFrame(&ts);
	checkTrue("the three answer", state() == ImuState::Online &&
	          sensePart()->state == SENSE_PART_ONLINE);
	SenseSample s0;
	senseSample(&s0);
	uint32_t imu0 = qmi8658Counts()->samples;
	bool every = true, imuEach = true, touchEach = true;
	fakeCst328Finger(true, 100, 200);
	for (int i = 0; i < 40; i++) {
		Clocks k = busFrame(&ts);
		every = every && k.touchRead;
		touchEach = touchEach && k.touch == 84;
		imuEach = imuEach && k.imu == 186;
	}
	SenseSample s1;
	senseSample(&s1);
	checkTrue("touch read every frame, 84 clocks, the finger seen", every && touchEach && ts.down);
	checkInt("the sensor read every frame", (long)(s1.seq - s0.seq), 40);
	checkTrue("the IMU's frame: 186 clocks, burst and time stamp", imuEach);
	checkInt("the IMU's samples: 40 frames of 25 ms, a sample each 32 ms",
	         (long)(qmi8658Counts()->samples - imu0), 31, 1);
	fakeCst328Finger(false, 0, 0);

	section("IMU: the IMU failing, touch and the sensor go on");
	fakeI2cRemove(IMU);
	senseSample(&s0);
	every = true;
	for (int i = 0; i < 4; i++) every = busFrame(&ts).touchRead && every;
	senseSample(&s1);
	checkTrue("the IMU offline, touch every frame, the sensor every frame",
	          state() == ImuState::Offline && every && s1.seq - s0.seq == 4);
	q = fakeI2cAddQmi8658(IMU);
	for (int i = 0; i < 50; i++) busFrame(&ts);
	checkTrue("put back: online", state() == ImuState::Online && q->resets == 1);
	uint32_t r0 = senseBus()->recoveries;
	fakeI2cFailNext(2, PICO_ERROR_TIMEOUT, 7);
	every = true;
	for (int i = 0; i < 6; i++) every = busFrame(&ts).touchRead && every;
	checkTrue("the IMU's two timeouts stick the bus, recovered, touch every frame",
	          every && senseBus()->recoveries > r0 && !senseBus()->stuck);
	checkTrue("the IMU and the sensor online after it", state() == ImuState::Online &&
	          sensePart()->state == SENSE_PART_ONLINE);
	qmi8658Stop();
	checkTrue("BACK: CTRL7 00h", q->reg[0x08] == 0);
	uint32_t t = fakeI2cTransactionsTo(IMU);
	for (int i = 0; i < 10; i++) busFrame(&ts);
	checkInt("then nothing to 0x6B, touch and sensor going on", fakeI2cTransactionsTo(IMU), t);
	senseConfigure(0);
}

/**
 * @brief How long the IMU's tick holds the bus in one frame, for each failure
 *        mode, against its bound: the configuration's frame, dQY_L or
 *        WHO_AM_I and three registers written and read back, CTRL7 written,
 *        and its read-back's data read timing out: 292 clocks, 730 µs, and
 *        1000 µs.
 */
static void testFrameTime() {
	section("IMU: bus time per frame, each failure mode");
	const uint32_t read1 = xferClocks(1, false) + xferClocks(1, true);
	const uint32_t write1 = xferClocks(2, false);
	const uint32_t bound =
		busUs(read1 + 3 * (write1 + read1) + write1 + xferClocks(1, false), SENSE_TIMEOUT_US);
	checkInt("the bound: 292 clocks and a timeout, 1730 us", bound, 1730);
	checkInt("a healthy frame: 129 and 57 clocks, 465 us",
	         busUs(xferClocks(1, false) + xferClocks(11, true) + xferClocks(1, false) +
	               xferClocks(3, true), 0), 465);

	struct Start { const char *name; void (*arm)(FakeQmi8658 *); };
	static const Start starts[] = {
		{ "the A part's start", [](FakeQmi8658 *) {} },
		{ "the C part's start", [](FakeQmi8658 *q) { q->dqyAfterReset = 0; } },
		{ "reading", [](FakeQmi8658 *) {
			frame(QMI8658_RESET_A_MS);
			frame(QMI8658_WARMUP_MS);
		} },
	};
	uint32_t all = 0;
	char what[96];
	for (const Start &st : starts) {
		uint32_t worst = 0;
		int worstK = 0;
		for (int k = 0; k <= 24; k++) {
			FakeQmi8658 *q = started();
			st.arm(q);
			fakeI2cFailNext(1, PICO_ERROR_TIMEOUT, k);
			uint32_t w = 0;
			for (int i = 0; i < 120; i++) {
				fakeAdvance(25);
				FakeI2cStats a = fakeI2cStats();
				qmi8658Tick(millis());
				uint32_t us = busBetween(a, fakeI2cStats());
				if (us > w) w = us;
			}
			snprintf(what, sizeof(what), "%s, a timeout after %d transfers: within", st.name, k);
			checkTrue(what, w <= bound);
			if (w > worst) {
				worst = w;
				worstK = k;
			}
		}
		printf("  %-34s %5u us (after %d)\n", st.name, (unsigned)worst, worstK);
		if (worst > all) all = worst;
	}
	checkInt("the worst frame meets the bound: CTRL7's read-back timing out", all, bound);

	struct Mode { const char *name; void (*arm)(); };
	static const Mode modes[] = {
		{ "healthy", [] {} },
		{ "the IMU absent", [] { fakeI2cRemove(IMU); } },
		{ "every transfer NACKed", [] { fakeI2cFailNext(100000, PICO_ERROR_GENERIC); } },
		{ "every transfer timing out", [] { fakeI2cFailNext(100000, PICO_ERROR_TIMEOUT); } },
		{ "SDA held, transfers time out", [] { fakeI2cHoldSda(true); } },
		{ "SDA held, arbitration lost",
		  [] { fakeI2cHeldSdaCode(PICO_ERROR_GENERIC); fakeI2cHoldSda(true); } },
		{ "SCL held", [] { fakeI2cHoldScl(true); } },
	};
	for (const Mode &m : modes) {
		online();
		m.arm();
		uint32_t w = 0;
		for (int i = 0; i < 120; i++) {
			fakeAdvance(25);
			senseTick(millis());
			FakeI2cStats a = fakeI2cStats();
			qmi8658Tick(millis());
			uint32_t us = busBetween(a, fakeI2cStats());
			if (us > w) w = us;
		}
		printf("  %-34s %5u us\n", m.name, (unsigned)w);
		snprintf(what, sizeof(what), "%s: within %u us", m.name, (unsigned)bound);
		checkTrue(what, w <= bound);
		fakeI2cHeldSdaCode(PICO_ERROR_TIMEOUT);
	}
	online();
	uint32_t h = 0;
	for (int i = 0; i < 40; i++) {
		fakeAdvance(25);
		FakeI2cStats a = fakeI2cStats();
		qmi8658Tick(millis());
		uint32_t us = busBetween(a, fakeI2cStats());
		if (us > h) h = us;
	}
	checkInt("healthy, reading: 465 us a frame", h, 465);
	freshBus();
}

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void uiFrames(int n) {
	for (int i = 0; i < n; i++) {
		fakeAdvance(25);
		fakePumpPoll();
		core1JobPoll();
		uiTick();
	}
}

/** @brief uiTick() ticks the IMU: nothing while closed, the reads once opened. */
static void testUiTick() {
	section("IMU: uiTick() runs the driver's tick");
	uiInit();
	freshBus();
	FakeQmi8658 *q = fakeI2cAddQmi8658(IMU);
	uiFrames(80);
	checkInt("closed: nothing to 0x6B in 80 frames", fakeI2cTransactionsTo(IMU), 0);
	qmi8658Start(millis());
	uiFrames(10);
	checkTrue("opened: reset, configured, online, samples",
	          q->resets == 1 && state() == ImuState::Online && qmi8658Counts()->samples > 0);
	qmi8658Stop();
	uint32_t t = fakeI2cTransactionsTo(IMU);
	uiFrames(20);
	checkTrue("closed again: CTRL7 00h, nothing after it",
	          q->reg[0x08] == 0 && fakeI2cTransactionsTo(IMU) == t);
}

void runQmi8658Tests() {
	testCodec();
	testClosed();
	testProbe();
	testResetA();
	testResetC();
	testMismatch();
	testSamples();
	testFaults();
	testStop();
	testSharedBus();
	testFrameTime();
	testUiTick();
	freshBus();
}
