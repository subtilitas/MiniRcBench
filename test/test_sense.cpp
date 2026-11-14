/**
 * @file test_sense.cpp
 * @brief The INA219 driver on the shared I2C1 bus, against the bus and the
 *        parts of fake_i2c.h: the codec at the range ends, the probe, the
 *        readings, the faults rcbench's sense_bus.c decides on, the sample
 *        and its age, the readout's text, and touch read every frame beside
 *        the sensor.
 *
 * The touch driver, src/cst328.cpp, is included here in a namespace of its
 * own: the rest of the suite links the scripted touch of fakes.cpp, and this
 * file runs the real driver on the same bus as the sensor.
 */

#include "check.h"
#include "fakes.h"
#include "fake_i2c.h"
#include "sense.h"
#include "settings.h"
#include "touch.h"
#include "board_desc.h"
#include "board_pins.h"
#include "config.h"
#include "gfx.h"
#include "plat.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include <stdio.h>
#include <string.h>

namespace cst {
#include "../src/cst328.cpp"
}

/** @brief Whether CURRENT SENSE's caption is @p want; false for none. */
static bool captionIs(const char *want) {
	const char *c = senseCaption();
	return c && strcmp(c, want) == 0;
}

/** @brief Whether SDA reads low. */
static bool sdaHeld() { return !gpio_get(g_board->sda); }

/** @brief One UI frame of the sensor: 25 ms on, then the read. */
static void tick(uint32_t ms = 25) {
	fakeAdvance(ms);
	senseTick(millis());
}

static SenseSample sample() {
	SenseSample s;
	senseSample(&s);
	return s;
}

static SenseState state() { return (SenseState)sample().state; }

/** @brief An empty bus, the sensor at NONE, then declared. */
static void declared() {
	senseConfigure(0);
	fakeI2cReset();
	fakePwmReset();
	senseConfigure(1);
}

/** @brief The INA219 at 0x40 and the sensor declared and probed. */
static FakeIna219 *online() {
	declared();
	FakeIna219 *p = fakeI2cAddIna219(INA219_ADDR);
	tick();
	return p;
}

// ---------------------------------------------------------------------------
// codec
// ---------------------------------------------------------------------------

static void testCodec() {
	section("Sense: the INA219 codec");
	checkTrue("19EFh: 16 V, /8, bus 12 bit, shunt 32 samples, continuous",
	          INA219_CONFIG_BENCH == 0x19EFu && ina219Config(0, 3, 3, 13, 7) == 0x19EFu);
	checkInt("BRNG is bit 13", ina219Config(1, 0, 0, 0, 0), 0x2000);
	checkInt("PG bits 12 and 11", ina219Config(0, 3, 0, 0, 0), 0x1800);
	checkInt("BADC bits 10 to 7", ina219Config(0, 0, 15, 0, 0), 0x0780);
	checkInt("SADC bits 6 to 3", ina219Config(0, 0, 0, 15, 0), 0x0078);
	checkInt("MODE bits 2 to 0", ina219Config(0, 0, 0, 0, 7), 0x0007);
	checkInt("the power-on value is /8, 32 V, 12 bit", ina219Config(1, 3, 3, 3, 7), 0x399F);

	sense_value_t v = ina219CurrentUa(0, 100);
	checkTrue("code 0: 0 uA", v.value == 0 && v.clip == SENSE_CLIP_NONE);
	v = ina219CurrentUa(1, 100);
	checkInt("code 1 on 0.1 ohm: 100 uA", v.value, 100);
	v = ina219CurrentUa(0xFFFF, 100);
	checkInt("code -1: -100 uA", v.value, -100);
	v = ina219CurrentUa(31999, 100);
	checkTrue("code 31999: 3,199,900 uA, a value", v.value == 3199900 && v.clip == SENSE_CLIP_NONE);
	v = ina219CurrentUa((uint16_t)-31999, 100);
	checkTrue("code -31999: -3,199,900 uA", v.value == -3199900 && v.clip == SENSE_CLIP_NONE);
	v = ina219CurrentUa(32000, 100);
	checkTrue("code 32000: clipped high, no value", v.clip == SENSE_CLIP_HIGH && v.value == 0);
	v = ina219CurrentUa((uint16_t)-32000, 100);
	checkTrue("code -32000: clipped low", v.clip == SENSE_CLIP_LOW && v.value == 0);
	v = ina219CurrentUa(0x7FFF, 100);
	checkTrue("beyond the end: clipped high", v.clip == SENSE_CLIP_HIGH);
	v = ina219CurrentUa(0x8000, 100);
	checkTrue("the most negative code: clipped low", v.clip == SENSE_CLIP_LOW);
	v = ina219CurrentUa(4200, 100);
	checkInt("4200: 0.42 A", v.value, 420000);
	v = ina219CurrentUa(4200, 50);
	checkInt("on 0.05 ohm: 0.84 A", v.value, 840000);
	checkInt("the end on 0.1 ohm: 3.2 A", senseEndUa(), 3200000);

	checkInt("bus code 0: 0 mV", ina219BusMv(0), 0);
	checkInt("bus code 1: 4 mV", ina219BusMv(1u << 3), 4);
	checkInt("bus code 1250: 5000 mV", ina219BusMv(1250u << 3), 5000);
	checkInt("CNVR and OVF ignored", ina219BusMv((1250u << 3) | 3u), 5000);
	checkInt("the top code: 32,764 mV", ina219BusMv(0xFFF8u), 32764);

	checkInt("SDK: the byte count is OK", senseErrOf(2, 2), SENSE_OK);
	checkInt("SDK: a short count is a NACK", senseErrOf(1, 2), SENSE_NACK);
	checkInt("SDK: PICO_ERROR_GENERIC is a NACK", senseErrOf(PICO_ERROR_GENERIC, 2), SENSE_NACK);
	checkInt("SDK: PICO_ERROR_TIMEOUT is a timeout", senseErrOf(PICO_ERROR_TIMEOUT, 2), SENSE_TIMEOUT);
}

// ---------------------------------------------------------------------------
// probe and readings
// ---------------------------------------------------------------------------

static void testProbe() {
	section("Sense: the probe reads 00h, then writes RST and 19EFh");
	declared();
	checkTrue("declared, not probed yet: no caption", captionIs(""));
	FakeIna219 *p = fakeI2cAddIna219(INA219_ADDR);
	FakeI2cStats s0 = fakeI2cStats();
	tick();
	FakeI2cStats s1 = fakeI2cStats();
	checkInt("399Fh read: RST, then 19EFh, two writes", p->configWrites, 2);
	checkInt("the configuration is the bench's", p->reg[0], 0x19EF);
	checkInt("calibration left at 0", p->reg[5], 0);
	checkTrue("online", state() == SenseState::Online && captionIs("FOUND"));
	checkInt("the probe: 220 clocks, three reads and two writes", (long)(s1.clocks - s0.clocks), 220);
	checkTrue("identity: 399Fh and 19EFh only",
	          ina219Identity(0x399F) && ina219Identity(0x19EF) && !ina219Identity(0x4127) &&
	          !ina219Identity(0x7127) && !ina219Identity(0x0000) && !ina219Identity(0x1234));
	declared();
	p = fakeI2cAddIna219(INA219_ADDR);
	p->reg[0] = 0x19EF;   // the board reset, the INA219 kept its configuration
	tick();
	checkTrue("19EFh read: an INA219, reset and configured again",
	          state() == SenseState::Online && p->configWrites == 2);

	section("Sense: another chip at 0x40 is read, only the pointer written");
	const uint16_t others[3] = { 0x4127, 0x7127, 0x1234 };
	for (uint16_t id : others) {
		declared();
		p = fakeI2cAddIna219(INA219_ADDR, id);
		uint32_t t0 = fakeI2cTransactionsTo(INA219_ADDR);
		tick();
		char what[48];
		snprintf(what, sizeof(what), "%04Xh: OTHER PART", id);
		checkTrue(what, state() == SenseState::Absent && captionIs("OTHER PART") &&
		          sensePart()->id_device == id);
		for (int i = 0; i < 120; i++) tick();
		snprintf(what, sizeof(what), "%04Xh: no register written in 3000 ms", id);
		checkInt(what, p->configWrites, 0);
		snprintf(what, sizeof(what), "%04Xh: read again once a second", id);
		checkInt(what, (long)(fakeI2cTransactionsTo(INA219_ADDR) - t0), 4);
	}
	// The other chip taken off and an INA219 put in its place: found at the
	// next probe, without CURRENT SENSE touched.
	fakeI2cRemove(INA219_ADDR);
	p = fakeI2cAddIna219(INA219_ADDR);
	for (int i = 0; i < 40 && state() != SenseState::Online; i++) tick();
	checkTrue("an INA219 put in its place later is found", state() == SenseState::Online &&
	          p->reg[0] == 0x19EF);
	declared();
	p = fakeI2cAddIna219(INA219_ADDR, 0xFFFF);
	tick();
	checkTrue("FFFFh: no part, NOT FOUND",
	          state() == SenseState::Absent && captionIs("NOT FOUND"));
	declared();
	p = fakeI2cAddIna219(INA219_ADDR);
	p->deaf = true;
	tick();
	checkTrue("19EFh not held: refused, NOT FOUND, not used",
	          state() == SenseState::Absent && captionIs("NOT FOUND") &&
	          sensePart()->state == SENSE_PART_REFUSED);
	declared();
	p = fakeI2cAddIna219(INA219_ADDR);
	// The identity read, RST and its read go out; the write of 19EFh is
	// not answered.
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 5);
	tick();
	checkTrue("the set-up write NACKed: absent", sensePart()->state == SENSE_PART_ABSENT);
	tick(1000);
	checkTrue("1000 ms on: online", state() == SenseState::Online);

	section("Sense: probed at boot");
	senseConfigure(0);
	fakeI2cReset();
	p = fakeI2cAddIna219(INA219_ADDR);
	settings()->currentSense = 1;
	senseInit();
	checkTrue("senseInit() with INA219: probed, online", state() == SenseState::Online &&
	          p->reg[0] == 0x19EF);
	settings()->currentSense = 0;
	senseInit();
	checkTrue("with NONE: NONE", state() == SenseState::None);

	section("Sense: absent, probed again 1000 ms on and not 999 ms");
	declared();
	tick();
	uint32_t n = fakeI2cTransactionsTo(INA219_ADDR);
	checkInt("one transaction: the identity read, NACKed", n, 1);
	checkTrue("absent: NOT FOUND",
	          state() == SenseState::Absent && captionIs("NOT FOUND"));
	tick(999);
	checkInt("999 ms on: nothing sent", fakeI2cTransactionsTo(INA219_ADDR), 1);
	tick(1);
	checkInt("1000 ms: probed again", fakeI2cTransactionsTo(INA219_ADDR), 2);
	fakeI2cAddIna219(INA219_ADDR);
	tick(999);
	checkTrue("plugged in: not yet", state() == SenseState::Absent);
	tick(1);
	checkTrue("found at the next probe", state() == SenseState::Online);

	section("Sense: NONE sends nothing");
	senseConfigure(0);
	fakeI2cReset();
	fakeI2cAddIna219(INA219_ADDR);
	for (int i = 0; i < 100; i++) tick();
	checkInt("100 frames: nothing to 0x40", fakeI2cTransactionsTo(INA219_ADDR), 0);
	checkTrue("state NONE, no caption", state() == SenseState::None && senseCaption() == nullptr &&
	          !senseDeclared());
	char strip[32] = "GP0  SUPPLY ON";
	senseAppendCurrent(strip, sizeof(strip));
	checkStr("the strip unchanged", strip, "GP0  SUPPLY ON");
}

static void testReadings() {
	section("Sense: readings, mA and mV at the range ends");
	FakeIna219 *p = online();
	p->currentA = 0.42;
	p->voutV = 5.0;
	uint32_t seq0 = sample().seq;
	FakeI2cStats s0 = fakeI2cStats();
	tick();
	FakeI2cStats s1 = fakeI2cStats();
	SenseSample s = sample();
	checkInt("one reading: seq + 1", (long)(s.seq - seq0), 1);
	checkInt("0.42 A: 420,000 uA", s.currentUa, 420000);
	checkInt("5.0 V less 42 mV at IN-: 4960 mV", s.busMv, 4960);
	checkTrue("no flags, taken now", s.flags == 0 && s.takenMs == millis());
	checkInt("the frame: 96 clocks, two register reads", (long)(s1.clocks - s0.clocks), 96);

	p->currentA = 0.0;
	tick();
	checkTrue("0 A", sample().currentUa == 0 && sample().busMv == 5000);
	p->currentA = 3.1999;
	tick();
	checkTrue("3.1999 A: 3,199,900 uA, not clipped",
	          sample().currentUa == 3199900 && !(sample().flags & SENSE_FLAG_CLIPPED));
	p->currentA = 3.2;
	tick();
	checkTrue("3.2 A: the end code, clipped, 3.2 A",
	          (sample().flags & SENSE_FLAG_CLIPPED) && sample().currentUa == 3200000);
	p->currentA = 5.0;
	tick();
	checkTrue("5 A: held at the end, clipped",
	          (sample().flags & SENSE_FLAG_CLIPPED) && sample().currentUa == 3200000);
	p->currentA = -0.05;
	tick();
	checkTrue("reversed, -0.05 A: negative", sample().currentUa == -50000 && sample().flags == 0);
	p->currentA = -4.0;
	tick();
	checkTrue("-4 A: clipped low",
	          (sample().flags & SENSE_FLAG_CLIPPED) && sample().currentUa == -3200000);
	p->currentA = 0.0;
	p->voutV = 16.0;
	tick();
	checkInt("16.0 V: the 16 V range's top", sample().busMv, 16000);

	section("Sense: a conversion shows once its cycle ends");
	p = online();
	p->timed = true;
	checkInt("19EFh's cycle: 17.02 ms and 0.532 ms", fakeIna219CycleUs(p), 17552);
	p->currentA = 1.0;
	tick(17);
	checkInt("17 ms after the write: the old block, 0 A", sample().currentUa, 0);
	tick(1);
	checkInt("18 ms: the new block, 1 A", sample().currentUa, 1000000);
	senseConfigure(0);
}

// ---------------------------------------------------------------------------
// faults through sense_bus
// ---------------------------------------------------------------------------

static void testFaults() {
	section("Sense: three NACKs take the part offline");
	FakeIna219 *p = online();
	fakeI2cRemove(INA219_ADDR);
	tick();
	tick();
	checkTrue("two failed reads: still online", state() == SenseState::Online);
	tick();
	checkTrue("the third: offline, 75 ms", state() == SenseState::Offline &&
	          captionIs("NOT FOUND"));
	checkTrue("a NACK does not stick the bus", !senseBus()->stuck);
	uint32_t n = fakeI2cTransactionsTo(INA219_ADDR);
	tick(999);
	checkInt("offline: probed 1000 ms on, not 999 ms", fakeI2cTransactionsTo(INA219_ADDR), n);
	p = fakeI2cAddIna219(INA219_ADDR);
	tick(1);
	checkTrue("1000 ms: probed, online again", state() == SenseState::Online && p->reg[0] == 0x19EF);

	section("Sense: two timeouts stick the bus, a recovery frees it");
	p = online();
	FakeI2cStats s0 = fakeI2cStats();
	fakeI2cFailNext(100, PICO_ERROR_TIMEOUT);
	tick();
	checkTrue("one timeout: not stuck", !senseBus()->stuck && state() == SenseState::Online);
	checkInt("one transfer in the frame", (long)(fakeI2cStats().timeouts - s0.timeouts), 1);
	tick();
	checkTrue("two: stuck", senseBus()->stuck && state() == SenseState::Stuck);
	checkTrue("at most two timeouts in a frame, 1000 us each",
	          fakeI2cStats().timeouts - s0.timeouts <= 2 && SENSE_TIMEOUT_US == 1000u);
	fakeI2cFailNext(0, 0);
	FakeI2cStats s1 = fakeI2cStats();
	tick();
	FakeI2cStats s2 = fakeI2cStats();
	checkInt("a recovery at once: 9 clocks on SCL", (long)(s2.sclPulses - s1.sclPulses), 9);
	checkInt("and the block initialised again", (long)(s2.inits - s1.inits), 1);
	checkTrue("after the clocks, one START and one STOP on SDA, SCL high",
	          s2.starts - s1.starts == 1 && s2.stops - s1.stops == 1);
	checkTrue("SDA and SCL back to I2C",
	          fakePin(g_board->sda).function == GPIO_FUNC_I2C &&
	          fakePin(g_board->scl).function == GPIO_FUNC_I2C);
	checkTrue("the next transaction tested the bus: free, online",
	          !senseBus()->stuck && state() == SenseState::Online);

	section("Sense: SDA held low, a recovery every 100 ms");
	p = online();
	uint32_t r0 = senseBus()->recoveries;
	fakeI2cHoldSda(true);
	s0 = fakeI2cStats();
	tick();
	checkTrue("a line low: stuck, nothing sent", senseBus()->stuck &&
	          fakeI2cStats().transfers == s0.transfers);
	tick();
	checkInt("a recovery at once", (long)(senseBus()->recoveries - r0), 1);
	checkTrue("its test found SDA still low: stuck", senseBus()->stuck);
	for (int i = 0; i < 3; i++) tick();
	tick(24);
	checkInt("99 ms on: no second recovery", (long)(senseBus()->recoveries - r0), 1);
	tick(1);
	checkInt("100 ms: the second", (long)(senseBus()->recoveries - r0), 2);
	fakeI2cHoldSda(false);
	// The part went offline on its third failed read; the bus is tested by
	// the next transaction, the probe 1000 ms on, or a touch read.
	for (int i = 0; i < 45; i++) tick();
	checkTrue("SDA free: a recovery and the probe's test clear it, online",
	          !senseBus()->stuck && state() == SenseState::Online);

	section("Sense: a part holding SDA mid-byte, freed by the clocks");
	p = online();
	fakeI2cSdaReleaseAfter(5);
	tick();
	checkTrue("stuck", senseBus()->stuck);
	tick();
	checkTrue("the recovery's clocks free SDA, the test passes, online",
	          !senseBus()->stuck && state() == SenseState::Online);

	section("Sense: touch transfers noted");
	p = online();
	senseNoteTouch(PICO_ERROR_GENERIC, 2);
	senseNoteTouch(PICO_ERROR_GENERIC, 2);
	senseNoteTouch(PICO_ERROR_GENERIC, 2);
	checkTrue("touch NACKs: not stuck", !senseBus()->stuck);
	senseNoteTouch(PICO_ERROR_TIMEOUT, 2);
	checkTrue("one touch timeout: not stuck", !senseBus()->stuck);
	senseNoteTouch(PICO_ERROR_TIMEOUT, 5);
	checkTrue("two: stuck, seen between sensor reads", senseBus()->stuck && state() == SenseState::Online);
	uint32_t r1 = senseBus()->recoveries;
	senseTick(millis());
	checkTrue("the sensor's frame recovers it", !senseBus()->stuck && senseBus()->recoveries == r1 + 1);
	senseNoteTouch(PICO_ERROR_TIMEOUT, 2);
	senseNoteTouch(5, 5);
	senseNoteTouch(PICO_ERROR_TIMEOUT, 2);
	checkTrue("a good touch read between ends the run", !senseBus()->stuck);
	senseConfigure(0);
}

static void testReadback() {
	section("Sense: the read-back once a second, a reset in two");
	FakeIna219 *p = online();
	uint32_t w = p->configWrites;
	FakeI2cStats s0 = fakeI2cStats();
	for (int i = 0; i < 39; i++) tick();
	FakeI2cStats s1 = fakeI2cStats();
	checkInt("975 ms: no read-back, 39 frames of 96 clocks", (long)(s1.clocks - s0.clocks), 39 * 96);
	tick();
	checkInt("1000 ms: the read-back, 48 clocks more", (long)(fakeI2cStats().clocks - s1.clocks), 96 + 48);
	// One odd read-back is no reset.
	p->reg[0] = 0x399F;
	for (int i = 0; i < 40; i++) tick();
	p->reg[0] = 0x19EF;
	for (int i = 0; i < 40; i++) tick();
	checkTrue("one read-back of 399Fh: still online", state() == SenseState::Online &&
	          p->configWrites == w);

	fakeIna219Por(p);
	p->currentA = 0.5;
	int frames = 0;
	while (state() == SenseState::Online && frames < 200) { tick(); frames++; }
	checkTrue("a POR found within two read-backs, 2000 ms", frames <= 80 &&
	          state() == SenseState::Offline);
	checkTrue("readings on 399Fh are right in value", sample().currentUa == 500000);
	for (int i = 0; i < 40 && state() != SenseState::Online; i++) tick();
	checkTrue("probed and configured again", state() == SenseState::Online && p->reg[0] == 0x19EF);
	tick();
	checkTrue("the first reading carries the reset flag", (sample().flags & SENSE_FLAG_RESET) != 0);
	tick();
	checkTrue("the next does not", (sample().flags & SENSE_FLAG_RESET) == 0);

	section("Sense: a reading whose bus read fails");
	p = online();
	uint32_t seq0 = sample().seq;
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	tick();
	checkTrue("no reading, one fail counted as the run's", sample().seq == seq0 &&
	          sensePart()->fails == 1);
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	tick();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	tick();
	checkTrue("three samples whose second read fails: offline", state() == SenseState::Offline);

	section("Sense: a failed read-back is a failed transaction");
	p = online();
	for (int i = 0; i < 39; i++) tick();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC);
	uint32_t seq = sample().seq;
	tick();
	checkTrue("no reading that frame, no reset, one fail counted",
	          sample().seq == seq && state() == SenseState::Online && sensePart()->fails == 1);
	tick();
	checkTrue("the next frame reads", sample().seq == seq + 1 && sensePart()->fails == 0);
	senseConfigure(0);
}

// ---------------------------------------------------------------------------
// the sample, its age and the readout
// ---------------------------------------------------------------------------

static void testSampleAge() {
	section("Sense: 250 ms stale, not at 249 ms, across the millis() wrap");
	FakeIna219 *p = online();
	p->currentA = 0.08;
	uint32_t start = millis();
	// The reading 100 ms before the clock wraps, so taken + 250 ms wraps.
	fakeAdvance((0u - 100u) - start - 25u);
	tick();
	SenseSample s = sample();
	checkTrue("taken + 250 ms wraps", s.takenMs + 250u < s.takenMs);
	senseConfigure(0);  // no further reading: the sample ages
	checkTrue("fresh at once", senseFresh(&s, millis()));
	fakeAdvance(249);
	checkTrue("the clock wrapped", millis() < s.takenMs);
	checkTrue("249 ms: fresh", senseFresh(&s, millis()));
	char t[16];
	senseCurrentText(&s, millis(), t, sizeof(t));
	checkStr("and reads", t, "0.08 A");
	fakeAdvance(1);
	checkTrue("250 ms: stale", !senseFresh(&s, millis()));
	senseCurrentText(&s, millis(), t, sizeof(t));
	checkStr("and reads", t, "-- A");
	senseVoltText(&s, millis(), t, sizeof(t));
	checkStr("the voltage", t, "-- V");
	SenseSample none;
	memset(&none, 0, sizeof(none));
	none.takenMs = millis();
	checkTrue("seq 0: none, whatever its time", !senseFresh(&none, millis()));
	fakeAdvance(start - millis());
}

/** @brief The readout of @p ua, fresh. */
static const char *curText(int32_t ua, uint8_t flags = 0) {
	static char t[16];
	SenseSample s;
	memset(&s, 0, sizeof(s));
	s.seq = 1;
	s.takenMs = millis();
	s.currentUa = ua;
	s.flags = flags;
	senseCurrentText(&s, millis(), t, sizeof(t));
	return t;
}

static const char *voltText(uint16_t mv) {
	static char t[16];
	SenseSample s;
	memset(&s, 0, sizeof(s));
	s.seq = 1;
	s.takenMs = millis();
	s.busMv = mv;
	senseVoltText(&s, millis(), t, sizeof(t));
	return t;
}

static void testReadoutText() {
	section("Sense: the readout's text and its width");
	checkStr("0.42 A", curText(420000), "0.42 A");
	checkStr("rounded: 0.415 A", curText(415000), "0.42 A");
	checkStr("rounded down: 0.4149 A", curText(414900), "0.41 A");
	checkStr("0 A", curText(0), "0.00 A");
	checkStr("-4 mA reads 0", curText(-4000), "0.00 A");
	checkStr("reversed", curText(-50000), "-0.05 A");
	checkStr("3.1999 A", curText(3199900), "3.20 A");
	checkStr("-3.1999 A", curText(-3199900), "-3.20 A");
	checkStr("clipped high, amber", curText(3200000, SENSE_FLAG_CLIPPED), ">3.2 A");
	checkStr("clipped low", curText(-3200000, SENSE_FLAG_CLIPPED), "<-3.2 A");
	SenseSample s;
	memset(&s, 0, sizeof(s));
	s.seq = 1; s.flags = SENSE_FLAG_CLIPPED; s.takenMs = millis();
	char t[16];
	checkTrue("a clipped reading reports amber", senseCurrentText(&s, millis(), t, sizeof(t)));
	s.flags = 0;
	checkTrue("a value does not", !senseCurrentText(&s, millis(), t, sizeof(t)));
	checkStr("5.02 V", voltText(5020), "5.02 V");
	checkStr("5.015 V rounds up", voltText(5015), "5.02 V");
	checkStr("0 V", voltText(0), "0.00 V");
	checkStr("the top code", voltText(32764), "32.76 V");
	int widest = 0;
	const char *all[] = { curText(-3199900), curText(-3200000, SENSE_FLAG_CLIPPED),
	                      curText(3200000, SENSE_FLAG_CLIPPED), voltText(32764), "-- A" };
	for (const char *a : all) {
		int w = (int)strlen(a);
		if (w > widest) widest = w;
	}
	(void)all;
	checkTrue("the widest text fits SENSE_TEXT_MAX", widest <= SENSE_TEXT_MAX &&
	          (int)strlen(curText(-3199900)) <= SENSE_TEXT_MAX &&
	          (int)strlen(curText(-3200000, SENSE_FLAG_CLIPPED)) <= SENSE_TEXT_MAX &&
	          (int)strlen(voltText(32764)) <= SENSE_TEXT_MAX);
	checkTrue("the widest current at scale 2 fits right of the widest pulse",
	          8 + gfxTextW("NO PULSE", 2) + 4 <= GFX_W - 8 - gfxTextW("<-3.2 A", 2));

	section("Sense: the strip's current");
	FakeIna219 *p = online();
	p->currentA = 0.08;
	tick();
	char strip[40] = "GP0  SUPPLY ON";
	senseAppendCurrent(strip, sizeof(strip));
	checkStr("SUPPLY ON 0.08 A", strip, "GP0  SUPPLY ON 0.08 A");
	fakeI2cRemove(INA219_ADDR);
	for (int i = 0; i < 10; i++) tick();
	char strip2[40] = "GP0  SUPPLY ON";
	senseAppendCurrent(strip2, sizeof(strip2));
	checkStr("lost: -- A", strip2, "GP0  SUPPLY ON -- A");
	senseConfigure(0);
}

// ---------------------------------------------------------------------------
// touch beside the sensor
// ---------------------------------------------------------------------------

/** @brief @p n frames of touch, then the sensor; true when touch read every one. */
static bool touchEveryFrame(int n, TouchState *ts) {
	bool all = true;
	for (int i = 0; i < n; i++) {
		uint32_t before = fakeI2cTransactionsTo(CST328_I2C_ADDR);
		fakeAdvance(25);
		cst::touchPoll(ts);
		senseTick(millis());
		if (fakeI2cTransactionsTo(CST328_I2C_ADDR) != before + 1) all = false;
	}
	return all;
}

static void testTouchBeside() {
	section("Sense: touch reads every frame beside the sensor");
	senseConfigure(0);
	fakeI2cReset();
	fakePwmReset();
	fakeI2cAddCst328(CST328_I2C_ADDR);
	checkTrue("the driver finds the CST328 on the fake bus", cst::touchInit());
	TouchState ts;
	memset(&ts, 0, sizeof(ts));

	FakeI2cStats s0 = fakeI2cStats();
	checkTrue("no sensor declared: every frame", touchEveryFrame(10, &ts));
	checkInt("touch: 84 clocks a frame", (long)(fakeI2cStats().clocks - s0.clocks), 10 * 84);

	senseConfigure(1);
	fakeI2cAddIna219(INA219_ADDR);
	senseTick(millis());
	checkTrue("present", state() == SenseState::Online);
	s0 = fakeI2cStats();
	fakeCst328Finger(true, 120, 160);
	checkTrue("present: every frame", touchEveryFrame(10, &ts));
	checkTrue("and a finger is seen", ts.down);
	checkInt("touch and sensor: 180 clocks a frame", (long)(fakeI2cStats().clocks - s0.clocks),
	         10 * (84 + 96));
	fakeCst328Finger(false, 0, 0);

	fakeI2cRemove(INA219_ADDR);
	checkTrue("absent: every frame", touchEveryFrame(80, &ts));
	checkTrue("the sensor offline", state() == SenseState::Absent || state() == SenseState::Offline);

	fakeI2cAddIna219(INA219_ADDR);
	touchEveryFrame(41, &ts);
	checkTrue("back", state() == SenseState::Online);
	uint32_t r0 = senseBus()->recoveries;
	fakeI2cFailNext(4, PICO_ERROR_TIMEOUT);
	bool every = touchEveryFrame(6, &ts);
	checkTrue("failing, the bus stuck and recovered: every frame",
	          every && senseBus()->recoveries > r0 && !senseBus()->stuck);
	fakeCst328Finger(true, 60, 60);
	touchEveryFrame(2, &ts);
	checkTrue("and touch answers after the recovery", ts.down);
	fakeCst328Finger(false, 0, 0);

	// The touch driver's own transfers reach the bus record.
	fakeI2cFailNext(1, PICO_ERROR_TIMEOUT);
	cst::touchPoll(&ts);
	checkTrue("one touch timeout: noted, not stuck", senseBus()->timeouts == 1 && !senseBus()->stuck);
	fakeI2cFailNext(1, PICO_ERROR_TIMEOUT);
	cst::touchPoll(&ts);
	checkTrue("two touch reads timed out: the bus stuck", senseBus()->stuck);
	cst::touchPoll(&ts);
	checkTrue("a touch read that answers frees it", !senseBus()->stuck);
	senseConfigure(0);
	fakeI2cReset();
}

/**
 * @brief The touch driver's answer record against the CST328 model: a probe
 *        that reads another ID records none, a finger read that completes
 *        records one, a NACKing controller records none, and touchInit()
 *        starts the record again.
 */
static void testTouchAnswerRecord() {
	section("Touch: the driver's answer record");
	senseConfigure(0);
	fakeI2cReset();
	fakeI2cAddCst328(CST328_I2C_ADDR);
	fakeCst328WrongId(true);
	checkTrue("another ID: touchInit() fails", !cst::touchInit());
	checkTrue("and records no answer", !cst::touchAnswered());
	TouchState ts;
	memset(&ts, 0, sizeof(ts));
	cst::touchPoll(&ts);
	checkTrue("a finger read that completes records one", cst::touchAnswered());
	fakeCst328WrongId(false);
	checkTrue("found: the probe reads CACAh", cst::touchInit());
	checkTrue("the probe records an answer", cst::touchAnswered());
	fakeI2cRemove(CST328_I2C_ADDR);
	checkTrue("no part: touchInit() fails", !cst::touchInit());
	checkTrue("and starts the record again: no answer", !cst::touchAnswered());
	cst::touchPoll(&ts);
	checkTrue("a NACKed read records none", !cst::touchAnswered());
	fakeI2cReset();
}

/**
 * @brief A part holding SDA, the I2C block losing arbitration: the touch
 *        driver's transfers return PICO_ERROR_GENERIC, which must count as a
 *        line held low and not as a NACK, or no recovery ever runs.
 */
static void testArbitrationLost() {
	section("Sense: SDA held, touch losing arbitration, the recovery still runs");
	senseConfigure(0);
	fakeI2cReset();
	fakePwmReset();
	fakeI2cAddCst328(CST328_I2C_ADDR);
	cst::touchInit();
	senseConfigure(1);
	fakeI2cAddIna219(INA219_ADDR);
	TouchState ts;
	memset(&ts, 0, sizeof(ts));
	touchEveryFrame(2, &ts);
	checkTrue("online", state() == SenseState::Online);
	uint32_t r0 = senseBus()->recoveries;
	uint32_t reads0 = fakeCst328Reads();
	fakeI2cHeldSdaCode(PICO_ERROR_GENERIC);
	fakeI2cSdaReleaseAfter(5);
	senseNoteTouch(PICO_ERROR_GENERIC, 2);
	checkTrue("a touch abort with SDA low: the bus stuck, not a NACK", senseBus()->stuck);
	int frames = 0;
	while (frames < 200 && senseBus()->recoveries == r0) {
		touchEveryFrame(1, &ts);
		frames++;
	}
	checkTrue("a recovery runs", senseBus()->recoveries > r0 && frames <= 2);
	touchEveryFrame(2, &ts);
	checkTrue("SDA freed by its clocks, the bus free, the part online",
	          !sdaHeld() && !senseBus()->stuck && state() == SenseState::Online);
	checkTrue("touch reads again", fakeCst328Reads() > reads0);
	senseConfigure(0);
	fakeI2cReset();
}

/**
 * @brief Bus time of @p clocks at 400 kHz, 2.5 µs a clock, rounded up, plus
 *        @p timeoutUs.
 */
static uint32_t busUs(uint32_t clocks, uint32_t timeoutUs) {
	return (clocks * 5u + 1u) / 2u + timeoutUs;
}

/**
 * @brief The clocks of one transfer of @p bytes after the address, with a
 *        start and a stop, or a repeated start.
 */
static uint32_t xferClocks(uint32_t bytes, bool restart) {
	return 9u * (1u + bytes) + (restart ? 1u : 2u);
}

/** @brief The worst bus time of one frame's touch read and sensor read, µs. */
struct FrameCost { uint32_t touchUs, senseUs; };

/** @brief The bus time from @p a to @p b, µs. */
static uint32_t busBetween(const FakeI2cStats &a, const FakeI2cStats &b) {
	return busUs(b.clocks - a.clocks, b.timeoutUs - a.timeoutUs);
}

/** @brief @p n frames of touch and sensor; the worst of each part. */
static FrameCost worstFrames(int n) {
	TouchState ts;
	memset(&ts, 0, sizeof(ts));
	FrameCost w = { 0, 0 };
	for (int i = 0; i < n; i++) {
		fakeAdvance(25);
		FakeI2cStats a = fakeI2cStats();
		cst::touchPoll(&ts);
		FakeI2cStats b = fakeI2cStats();
		senseTick(millis());
		FakeI2cStats c = fakeI2cStats();
		uint32_t t = busBetween(a, b), se = busBetween(b, c);
		if (t > w.touchUs) w.touchUs = t;
		if (se > w.senseUs) w.senseUs = se;
	}
	return w;
}

/** @brief The bus with the CST328 and an online INA219, as each failure mode starts from. */
static void healthyBus() {
	senseConfigure(0);
	fakeI2cReset();
	fakePwmReset();
	fakeI2cAddCst328(CST328_I2C_ADDR);
	cst::touchInit();
	senseConfigure(1);
	fakeI2cAddIna219(INA219_ADDR);
	senseTick(millis());
}

/**
 * @brief How long core0 blocks on the bus in one frame, for each failure
 *        mode, against the note's bound.
 *
 * A failed transfer ends a frame, so the worst frame is the longest run of
 * transfers that answer followed by one that times out. For the sensor that
 * is the probe up to its read-back of 19EFh, whose data read times out:
 * read, RST, read, 19EFh and the read-back's pointer, 192 clocks, 480 µs,
 * and 1000 µs; 1480 µs. For touch it is the pointer write of D000h, 29
 * clocks, 72.5 µs, and a data read of 2000 µs; 2072.5 µs. Every frame of
 * every mode is measured, and the worst of all must meet each bound exactly.
 */
static void testFrameTime() {
	section("Sense: bus time per frame, each failure mode");
	const uint32_t regRead = xferClocks(1, false) + xferClocks(2, true);
	const uint32_t regWrite = xferClocks(3, false);
	const uint32_t senseBound =
		busUs(regRead + regWrite + regRead + regWrite + xferClocks(1, false), SENSE_TIMEOUT_US);
	const uint32_t touchBound = busUs(xferClocks(2, false), 2000u);
	checkInt("the sensor's bound: 192 clocks and a timeout, 1480 us", senseBound, 1480);
	checkInt("touch's bound: 29 clocks and a timeout, 2073 us", touchBound, 2073);

	struct Mode { const char *name; void (*arm)(); };
	static const Mode modes[] = {
		{ "healthy", [] {} },
		{ "sensor absent", [] { fakeI2cRemove(INA219_ADDR); } },
		{ "every transfer NACKed", [] { fakeI2cFailNext(100000, PICO_ERROR_GENERIC); } },
		{ "every transfer timing out", [] { fakeI2cFailNext(100000, PICO_ERROR_TIMEOUT); } },
		{ "SDA held, transfers time out", [] { fakeI2cHoldSda(true); } },
		{ "SDA held, arbitration lost",
		  [] { fakeI2cHeldSdaCode(PICO_ERROR_GENERIC); fakeI2cHoldSda(true); } },
		{ "SCL held", [] { fakeI2cHoldScl(true); } },
	};
	FrameCost all = { 0, 0 };
	char what[96];
	for (const Mode &m : modes) {
		healthyBus();
		m.arm();
		FrameCost w = worstFrames(120);
		printf("  %-34s touch %5u us, sensor %5u us\n", m.name, (unsigned)w.touchUs,
		       (unsigned)w.senseUs);
		snprintf(what, sizeof(what), "%s: sensor within %u us", m.name, (unsigned)senseBound);
		checkTrue(what, w.senseUs <= senseBound);
		snprintf(what, sizeof(what), "%s: touch within %u us", m.name, (unsigned)touchBound);
		checkTrue(what, w.touchUs <= touchBound);
		if (w.senseUs > all.senseUs) all.senseUs = w.senseUs;
		if (w.touchUs > all.touchUs) all.touchUs = w.touchUs;
	}

	// One timeout after k transfers answer, from each start a frame has: the
	// probe due, a reading, and a reading with the read-back due. Touch's two
	// transfers come first in each frame.
	struct Start { const char *name; void (*arm)(); };
	static const Start starts[] = {
		{ "probe", [] { senseConfigure(0); senseConfigure(1); } },
		{ "reading", [] {} },
		{ "read-back", [] { fakeAdvance(SENSE_READBACK_MS); } },
	};
	for (const Start &st : starts) {
		FrameCost worst = { 0, 0 };
		int worstK = 0;
		for (int k = 0; k <= 12; k++) {
			healthyBus();
			st.arm();
			fakeI2cFailNext(1, PICO_ERROR_TIMEOUT, k);
			FrameCost w = worstFrames(80);
			snprintf(what, sizeof(what), "%s, a timeout after %d transfers: within the bounds",
			         st.name, k);
			checkTrue(what, w.senseUs <= senseBound && w.touchUs <= touchBound);
			if (w.senseUs > worst.senseUs) {
				worst.senseUs = w.senseUs;
				worstK = k;
			}
			if (w.touchUs > worst.touchUs) worst.touchUs = w.touchUs;
		}
		printf("  %-34s touch %5u us, sensor %5u us (after %d)\n", st.name,
		       (unsigned)worst.touchUs, (unsigned)worst.senseUs, worstK);
		if (worst.senseUs > all.senseUs) all.senseUs = worst.senseUs;
		if (worst.touchUs > all.touchUs) all.touchUs = worst.touchUs;
	}
	checkInt("the worst sensor frame meets its bound: the probe's last read timing out",
	         all.senseUs, senseBound);
	checkInt("the worst touch frame meets its bound: the data read timing out", all.touchUs,
	         touchBound);

	healthyBus();
	FrameCost h = worstFrames(80);
	checkInt("healthy: touch 84 clocks, 210 us", h.touchUs, 210);
	checkInt("healthy: sensor 96 clocks and the read-back, 360 us at most", h.senseUs, 360);
	senseConfigure(0);
	fakeI2cReset();
}

/**
 * @brief A read that aborts at its address: the SDK returns before the
 *        block's STOP, which still holds SDA low for a moment. It is a NACK,
 *        not a line held low.
 */
static void testReadAbortStop() {
	section("Sense: a read's abort, the block's own STOP on SDA, still a NACK");
	FakeIna219 *p = online();
	(void)p;
	tick();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 1);
	tick();
	checkTrue("the shunt's data read aborted: a NACK, one fail, the bus not stuck",
	          sensePart()->last_err == SENSE_NACK && sensePart()->fails == 1 &&
	          !senseBus()->stuck);
	checkTrue("and SDA reads high after it", !sdaHeld());
	tick();
	checkTrue("the next frame reads", sensePart()->fails == 0 && state() == SenseState::Online);

	fakeI2cAddCst328(CST328_I2C_ADDR);
	TouchState ts;
	memset(&ts, 0, sizeof(ts));
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 1);
	cst::touchPoll(&ts);
	checkTrue("a touch read aborted the same way: not stuck", !senseBus()->stuck);
	senseConfigure(0);
	fakeI2cReset();
}

/** @brief A data byte not acknowledged: the SDK's short count, through the wire. */
static void testShortCount() {
	section("Sense: a short count through the wire is a failed transaction");
	FakeIna219 *p = online();
	p->currentA = 0.1;
	tick();
	uint32_t seq = sample().seq;
	fakeI2cShortNext(1);
	tick();
	checkTrue("the shunt read one byte short: no reading, one fail, the bus not stuck",
	          sample().seq == seq && sensePart()->fails == 1 && !senseBus()->stuck &&
	          sensePart()->last_err == SENSE_NACK);
	tick();
	checkTrue("the next frame reads", sample().seq == seq + 1 && sensePart()->fails == 0);
	senseConfigure(0);
}

void runSenseTests() {
	testCodec();
	testProbe();
	testReadings();
	testFaults();
	testReadback();
	testSampleAge();
	testReadoutText();
	testTouchBeside();
	testTouchAnswerRecord();
	testArbitrationLost();
	testFrameTime();
	testReadAbortStop();
	testShortCount();
}
