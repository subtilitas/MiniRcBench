/**
 * @file test_clock.cpp
 * @brief The board's clock, src/clock.cpp, against the PCF85063A model of
 *        fake_i2c.h on the shared bus: BCD in and out, the calendar, the
 *        boot read of a set clock and of each cause of a clock not set, the
 *        software reset only with OS set, a bus error at boot, the kept time
 *        across a minute, a day and a year, and get_fattime()'s packing.
 */

#include "check.h"
#include "fakes.h"
#include "fake_i2c.h"
#include "clock.h"
#include "sense.h"
#include "board_pins.h"

#include "hardware/i2c.h"

#include <stdio.h>
#include <string.h>

/** @brief An empty bus with the clock on it, in its reset state. */
static FakePcf85063 *rtcOnBus() {
	fakeI2cReset();
	return fakeI2cAddPcf85063(PCF85063_I2C_ADDR);
}

/** @brief The boot as setup() runs it: the bus record started, then the clock read. */
static void boot() {
	senseInit();
	clockInit();
}

static ClockReading now() {
	ClockReading c;
	clockNow(&c);
	return c;
}

/** @brief The kept time as text, or `not set`. */
static const char *nowText(char *b, size_t n) {
	ClockReading c = now();
	if (!c.set) snprintf(b, n, "not set");
	else        clockText(&c.t, b);
	return b;
}

static void checkNow(const char *what, const char *want) {
	char b[CLOCK_TEXT_BYTES + 8];
	checkStr(what, nowText(b, sizeof(b)), want);
}

static void checkBoot(const char *what, ClockBoot want) {
	checkInt(what, (long)clockBoot(), (long)want);
}

static ClockTime ct(int y, int mo, int d, int h, int mi, int s) {
	ClockTime t = { (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s };
	return t;
}

// ---------------------------------------------------------------------------
// pure
// ---------------------------------------------------------------------------

static void testBcd() {
	section("Clock: BCD in and out");
	uint8_t v = 0xEE;
	checkTrue("00h is 0", clockBcdIn(0x00, &v) && v == 0);
	checkTrue("59h is 59", clockBcdIn(0x59, &v) && v == 59);
	checkTrue("99h is 99", clockBcdIn(0x99, &v) && v == 99);
	checkTrue("0Ah: a digit above 9", !clockBcdIn(0x0A, &v));
	checkTrue("A0h: a digit above 9", !clockBcdIn(0xA0, &v));
	checkTrue("9Fh: a digit above 9", !clockBcdIn(0x9F, &v));
	checkInt("0 out", clockBcdOut(0), 0x00);
	checkInt("7 out", clockBcdOut(7), 0x07);
	checkInt("59 out", clockBcdOut(59), 0x59);
	checkInt("99 out", clockBcdOut(99), 0x99);
	int bad = 0;
	for (int i = 0; i < 100; i++)
		if (!clockBcdIn(clockBcdOut((uint8_t)i), &v) || v != i) bad++;
	checkInt("0 to 99 there and back", bad, 0);
}

static void testCalendar() {
	section("Clock: the calendar");
	checkInt("January", clockDaysIn(2026, 1), 31);
	checkInt("February 2026", clockDaysIn(2026, 2), 28);
	checkInt("February 2028", clockDaysIn(2028, 2), 29);
	checkInt("February 2000", clockDaysIn(2000, 2), 29);
	checkInt("April", clockDaysIn(2026, 4), 30);
	checkInt("December", clockDaysIn(2026, 12), 31);
	checkInt("month 0", clockDaysIn(2026, 0), 0);
	checkInt("month 13", clockDaysIn(2026, 13), 0);

	ClockTime t = ct(2000, 1, 1, 0, 0, 0);
	checkInt("2000-01-01 is second 0", (long)clockToSecs(&t), 0);
	t = ct(2000, 1, 2, 0, 0, 0);
	checkInt("a day", (long)clockToSecs(&t), 86400);
	t = ct(2001, 1, 1, 0, 0, 0);
	checkInt("2000 has 366 days", (long)clockToSecs(&t), 366L * 86400);
	t = ct(2002, 1, 1, 0, 0, 0);
	checkInt("2001 has 365", (long)clockToSecs(&t), 731L * 86400);
	t = ct(2026, 11, 14, 15, 42, 7);
	// 26 years with 7 leap days, then 317 days into 2026.
	checkTrue("2026-11-14 15:42:07", clockToSecs(&t) ==
	          ((26u * 365u + 7u + 317u) * 24u + 15u) * 3600u + 42u * 60u + 7u);

	// Every day of 2000 to 2099 at 23:59:59 and the second after it.
	int bad = 0;
	ClockTime d = ct(2000, 1, 1, 23, 59, 59);
	uint32_t expect = 86399;
	while (d.year <= 2099) {
		ClockTime back;
		if (clockToSecs(&d) != expect) bad++;
		clockFromSecs(expect, &back);
		if (memcmp(&back, &d, sizeof(d)) != 0) bad++;
		clockFromSecs(expect + 1, &back);
		if (back.hour != 0 || back.minute != 0 || back.second != 0) bad++;
		if (++d.day > clockDaysIn(d.year, d.month)) {
			d.day = 1;
			if (++d.month > 12) { d.month = 1; d.year++; }
		}
		if (d.year <= 2099 && (back.year != d.year || back.month != d.month || back.day != d.day))
			bad++;
		expect += 86400;
	}
	checkInt("36525 days there and back", bad, 0);

	t = ct(2026, 2, 28, 0, 0, 0);
	checkTrue("valid", clockValid(&t));
	const struct { const char *what; ClockTime t; } invalid[] = {
		{ "1999", ct(1999, 12, 31, 0, 0, 0) },
		{ "2100", ct(2100, 1, 1, 0, 0, 0) },
		{ "month 0", ct(2026, 0, 1, 0, 0, 0) },
		{ "month 13", ct(2026, 13, 1, 0, 0, 0) },
		{ "day 0", ct(2026, 1, 0, 0, 0, 0) },
		{ "2026-02-29", ct(2026, 2, 29, 0, 0, 0) },
		{ "hour 24", ct(2026, 1, 1, 24, 0, 0) },
		{ "minute 60", ct(2026, 1, 1, 0, 60, 0) },
		{ "second 60", ct(2026, 1, 1, 0, 0, 60) },
	};
	for (const auto &c : invalid) checkTrue(c.what, !clockValid(&c.t));
	const struct { const char *what; ClockTime t; } edges[] = {
		{ "2000-01-01 00:00:00", ct(2000, 1, 1, 0, 0, 0) },
		{ "2099-12-31 23:59:59", ct(2099, 12, 31, 23, 59, 59) },
		{ "2028-02-29", ct(2028, 2, 29, 0, 0, 0) },
	};
	for (const auto &c : edges) checkTrue(c.what, clockValid(&c.t));
}

/** @brief Registers 00h to 0Ah of a running clock at 2026-11-14 15:42:07. */
static void runningRegs(uint8_t *r) {
	const uint8_t v[CLOCK_READ_BYTES] = { 0x00, 0x07, 0x00, 0x00, 0x07, 0x42, 0x15,
	                                      0x14, 0x06, 0x11, 0x26 };
	memcpy(r, v, CLOCK_READ_BYTES);
}

static void testDecode() {
	section("Clock: decoding registers 00h to 0Ah");
	uint8_t r[CLOCK_READ_BYTES];
	ClockTime t = {};
	char b[CLOCK_TEXT_BYTES];
	runningRegs(r);
	checkInt("running", (long)clockDecode(r, &t), (long)ClockBoot::Running);
	clockText(&t, b);
	checkStr("its time", b, "2026-11-14 15:42:07");

	// The unused bits are masked; Control_1's CAP_SEL and CIE, Control_2,
	// the offset and the RAM byte do not matter.
	runningRegs(r);
	r[0x05] |= 0x80; r[0x06] |= 0xC0; r[0x07] |= 0xC0; r[0x09] |= 0xE0;
	r[0x00] = 0x05; r[0x01] = 0xFF; r[0x02] = 0xFF; r[0x03] = 0xFF; r[0x08] = 0xFF;
	t = {};
	checkInt("unused bits set: running", (long)clockDecode(r, &t), (long)ClockBoot::Running);
	clockText(&t, b);
	checkStr("the same time", b, "2026-11-14 15:42:07");

	const struct { const char *what; int reg; uint8_t v; ClockBoot want; } cases[] = {
		{ "OS set", 0x04, 0x87, ClockBoot::OscStopped },
		{ "STOP set", 0x00, 0x20, ClockBoot::Stopped },
		{ "EXT_TEST set", 0x00, 0x80, ClockBoot::TestMode },
		{ "12-hour mode", 0x00, 0x02, ClockBoot::Hour12 },
		{ "seconds 60", 0x04, 0x60, ClockBoot::BadTime },
		{ "seconds 5Ah", 0x04, 0x5A, ClockBoot::BadTime },
		{ "minutes 60", 0x05, 0x60, ClockBoot::BadTime },
		{ "minutes 4Fh", 0x05, 0x4F, ClockBoot::BadTime },
		{ "hours 24", 0x06, 0x24, ClockBoot::BadTime },
		{ "hours 1Ah", 0x06, 0x1A, ClockBoot::BadTime },
		{ "day 0", 0x07, 0x00, ClockBoot::BadTime },
		{ "day 31 in November", 0x07, 0x31, ClockBoot::BadTime },
		{ "day 2Ah", 0x07, 0x2A, ClockBoot::BadTime },
		{ "month 0", 0x09, 0x00, ClockBoot::BadTime },
		{ "month 13", 0x09, 0x13, ClockBoot::BadTime },
		{ "month 0Ah", 0x09, 0x0A, ClockBoot::BadTime },
		{ "year A0h", 0x0A, 0xA0, ClockBoot::BadTime },
	};
	for (const auto &c : cases) {
		runningRegs(r);
		r[c.reg] = c.v;
		ClockTime u = ct(1, 2, 3, 4, 5, 6);
		ClockBoot got = clockDecode(r, &u);
		checkInt(c.what, (long)got, (long)c.want);
		if (u.year != 1 || u.second != 6) checkTrue("  the time left alone", false);
	}
	// OS is checked first, so it decides with every other cause present.
	runningRegs(r);
	r[0x04] = 0xE0; r[0x00] = 0xA2;
	checkInt("OS with every other cause", (long)clockDecode(r, &t), (long)ClockBoot::OscStopped);
	runningRegs(r);
	r[0x00] = 0xA2;
	checkInt("STOP before EXT_TEST", (long)clockDecode(r, &t), (long)ClockBoot::Stopped);
	r[0x00] = 0x82;
	checkInt("EXT_TEST before 12_24", (long)clockDecode(r, &t), (long)ClockBoot::TestMode);
	r[0x00] = 0x02; r[0x04] = 0x60;
	checkInt("12_24 before the range", (long)clockDecode(r, &t), (long)ClockBoot::Hour12);
	// The leap day: 2028 has one, 2027 not.
	runningRegs(r);
	r[0x07] = 0x29; r[0x09] = 0x02; r[0x0A] = 0x28;
	checkInt("2028-02-29: running", (long)clockDecode(r, &t), (long)ClockBoot::Running);
	r[0x0A] = 0x27;
	checkInt("2027-02-29: out of range", (long)clockDecode(r, &t), (long)ClockBoot::BadTime);
	r[0x0A] = 0x00; r[0x09] = 0x01; r[0x07] = 0x01;
	r[0x04] = 0x00; r[0x05] = 0x00; r[0x06] = 0x00;
	checkInt("2000-01-01 00:00:00", (long)clockDecode(r, &t), (long)ClockBoot::Running);
	r[0x0A] = 0x99; r[0x09] = 0x12; r[0x07] = 0x31;
	r[0x04] = 0x59; r[0x05] = 0x59; r[0x06] = 0x23;
	checkInt("2099-12-31 23:59:59", (long)clockDecode(r, &t), (long)ClockBoot::Running);
	clockText(&t, b);
	checkStr("its time", b, "2099-12-31 23:59:59");
}

static void testFatTime() {
	section("Clock: the FAT time");
	ClockTime t = ct(2000, 1, 1, 0, 0, 0);
	checkTrue("2000-01-01 00:00:00", clockFatTime(&t) == (20u << 25 | 1u << 21 | 1u << 16));
	t = ct(2026, 11, 14, 15, 42, 7);
	checkTrue("2026-11-14 15:42:07, seconds / 2",
	          clockFatTime(&t) == (46u << 25 | 11u << 21 | 14u << 16 | 15u << 11 | 42u << 5 | 3u));
	t.second = 6;
	checkTrue("15:42:06 the same", clockFatTime(&t) ==
	          (46u << 25 | 11u << 21 | 14u << 16 | 15u << 11 | 42u << 5 | 3u));
	t = ct(2099, 12, 31, 23, 59, 59);
	checkTrue("2099-12-31 23:59:59",
	          clockFatTime(&t) == (119u << 25 | 12u << 21 | 31u << 16 | 23u << 11 | 59u << 5 | 29u));
	t = ct(1980, 1, 1, 0, 0, 0);
	checkTrue("1980, FAT's first year", clockFatTime(&t) == (1u << 21 | 1u << 16));
	t = ct(2107, 12, 31, 23, 59, 59);
	checkTrue("2107, FAT's last year", clockFatTime(&t) >> 25 == 127u);
	t = ct(2108, 1, 1, 0, 0, 0);
	checkTrue("2108: 0", clockFatTime(&t) == 0);
	t = ct(1979, 12, 31, 0, 0, 0);
	checkTrue("1979: 0", clockFatTime(&t) == 0);
}

// ---------------------------------------------------------------------------
// the part
// ---------------------------------------------------------------------------

static void testSet() {
	section("Clock: a set clock read at boot");
	FakePcf85063 *p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	fakeAdvance(400);                       // 0.4 s into the second
	senseInit();
	FakeI2cStats before = fakeI2cStats();
	clockInit();
	FakeI2cStats after = fakeI2cStats();
	checkBoot("running", ClockBoot::Running);
	checkNow("the time read", "2026-11-14 15:42:07");
	checkInt("one sets", (long)now().sets, 1);
	checkInt("one transaction to 0x51", (long)fakeI2cTransactionsTo(PCF85063_I2C_ADDR), 1);
	checkInt("14 bytes: 129 clocks", (long)(after.clocks - before.clocks), 129);
	checkInt("nothing written to the part", (long)p->writes, 0);
	checkInt("no reset", (long)p->resets, 0);

	// Kept by the timer: no transfer afterwards.
	uint32_t reads = p->reads;
	fakeAdvance(999);
	checkNow("0.999 s on: the same second", "2026-11-14 15:42:07");
	fakeAdvance(1);
	checkNow("1 s on", "2026-11-14 15:42:08");
	fakeAdvance(53000);
	checkNow("a minute's carry", "2026-11-14 15:43:01");
	ClockTime t = ct(2026, 11, 14, 15, 43, 1);
	checkTrue("get_fattime() packs it", get_fattime() == clockFatTime(&t));
	checkInt("no read of the part since", (long)(p->reads - reads), 0);

	// Across a year's end, and past 2^32 µs on the timer.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 12, 31, 23, 59, 58);
	boot();
	checkNow("2026-12-31 23:59:58", "2026-12-31 23:59:58");
	fakeAdvance(2000);
	checkNow("2 s on: the new year", "2027-01-01 00:00:00");
	fakeAdvance(4294968u);                  // 4294.968 s: beyond 2^32 µs since the read
	checkNow("71 min 34.968 s on", "2027-01-01 01:11:34");
	// The leap day.
	p = rtcOnBus();
	fakePcf85063Set(p, 2028, 2, 28, 23, 59, 59);
	boot();
	fakeAdvance(1000);
	checkNow("2028-02-29", "2028-02-29 00:00:00");
	t = ct(2028, 2, 29, 0, 0, 0);
	checkTrue("get_fattime() on the leap day", get_fattime() == clockFatTime(&t));

	// The model counts on its own: a second boot reads it on.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	fakeAdvance(3000);
	boot();
	checkNow("a reboot 3 s later reads 3 s on", "2026-11-14 15:42:10");
}

static void testNotSet() {
	section("Clock: not set at boot");
	// A board powered up with no cell: the part after its power-on, OS set.
	FakePcf85063 *p = rtcOnBus();
	fakeAdvance(1500);
	boot();
	checkBoot("power-up: oscillator stopped", ClockBoot::OscStopped);
	checkNow("not set", "not set");
	checkInt("sets 0", (long)now().sets, 0);
	checkTrue("get_fattime(): 0", get_fattime() == 0);
	checkInt("the software reset sent", (long)p->resets, 1);
	checkInt("Control_2 07h: CLKOUT off", p->reg[0x01], 0x07);
	checkInt("Control_1 00h", p->reg[0x00], 0x00);
	checkInt("the read and two writes", (long)fakeI2cTransactionsTo(PCF85063_I2C_ADDR), 3);
	ClockReading c = now();
	checkTrue("the reading's time all 0", c.t.year == 0 && c.t.month == 0 && c.t.day == 0 &&
	          c.t.hour == 0 && c.t.minute == 0 && c.t.second == 0);
	fakeAdvance(10000);
	checkTrue("get_fattime() still 0", get_fattime() == 0);

	// A clock read set, then a boot that finds it lost: the earlier reading
	// is forgotten.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	boot();
	checkTrue("set", now().set);
	p->reg[0x04] |= 0x80;
	boot();
	checkNow("OS found at the next boot: not set", "not set");
	checkInt("sets back to 0", (long)now().sets, 0);

	// The other causes: nothing written to the part.
	const struct { const char *what; int reg; uint8_t v; ClockBoot want; } cases[] = {
		{ "STOP set", 0x00, 0x20, ClockBoot::Stopped },
		{ "EXT_TEST set", 0x00, 0x80, ClockBoot::TestMode },
		{ "12-hour mode", 0x00, 0x02, ClockBoot::Hour12 },
		{ "hours 25", 0x06, 0x25, ClockBoot::BadTime },
	};
	for (const auto &k : cases) {
		p = rtcOnBus();
		fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
		p->reg[k.reg] = k.v;
		boot();
		char w[64];
		snprintf(w, sizeof(w), "%s: the cause", k.what);
		checkBoot(w, k.want);
		snprintf(w, sizeof(w), "%s: not set", k.what);
		checkTrue(w, !now().set && get_fattime() == 0);
		snprintf(w, sizeof(w), "%s: nothing written", k.what);
		checkTrue(w, p->writes == 0 && p->resets == 0);
	}
}

static void testBus() {
	section("Clock: the bus at boot");
	// No part at 0x51.
	fakeI2cReset();
	boot();
	checkBoot("no part: no answer", ClockBoot::NoAnswer);
	checkTrue("not set", !now().set && get_fattime() == 0);
	checkInt("one try", (long)fakeI2cTransactionsTo(PCF85063_I2C_ADDR), 1);
	checkInt("a NACK leaves the bus working", senseBus()->stuck, 0);

	// The pointer's write times out.
	FakePcf85063 *p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	senseInit();
	fakeI2cFailNext(1, PICO_ERROR_TIMEOUT);
	clockInit();
	checkBoot("a timeout: no answer", ClockBoot::NoAnswer);
	checkTrue("not set", !now().set);
	checkInt("noted to the bus record", senseBus()->timeouts, 1);
	checkInt("no read after it", (long)p->reads, 0);

	// The read aborts after the pointer went out.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	senseInit();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 1);
	clockInit();
	checkBoot("the read aborted: no answer", ClockBoot::NoAnswer);
	checkTrue("not set", !now().set);

	// A short read: 10 bytes of 11.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	senseInit();
	fakeI2cShortNext(2);
	clockInit();
	checkBoot("a byte short: no answer", ClockBoot::NoAnswer);

	// SDA held low: the bus record marks the bus stuck.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	senseInit();
	fakeI2cHoldSda(true);
	fakeI2cHeldSdaCode(PICO_ERROR_GENERIC);
	clockInit();
	checkBoot("SDA held: no answer", ClockBoot::NoAnswer);
	checkInt("the bus stuck", senseBus()->stuck, 1);
	fakeI2cHoldSda(false);

	// A bus the record shows stuck already: nothing sent.
	p = rtcOnBus();
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	senseInit();
	senseNoteTouch(PICO_ERROR_TIMEOUT, 2);
	senseNoteTouch(PICO_ERROR_TIMEOUT, 2);
	checkInt("two timeouts: stuck", senseBus()->stuck, 1);
	clockInit();
	checkBoot("stuck: no answer", ClockBoot::NoAnswer);
	checkInt("nothing sent to 0x51", (long)fakeI2cTransactionsTo(PCF85063_I2C_ADDR), 0);

	// OS set, and the reset's write fails: the clock stays not set, and the
	// second write still goes out.
	p = rtcOnBus();
	senseInit();
	fakeI2cFailNext(1, PICO_ERROR_GENERIC, 2);
	clockInit();
	checkBoot("OS set, the reset refused", ClockBoot::OscStopped);
	checkInt("no reset taken", (long)p->resets, 0);
	checkInt("Control_2 written", p->reg[0x01], 0x07);
	checkTrue("not set", !now().set);

	// OS set, and a part holds SDA from the end of the read: the reset's
	// write loses arbitration, the bus record marks the bus stuck, and the
	// second write is not sent.
	p = rtcOnBus();
	senseInit();
	fakeI2cHeldSdaCode(PICO_ERROR_GENERIC);
	fakeI2cHoldSdaAfter(2);
	clockInit();
	fakeI2cHoldSda(false);
	checkBoot("OS set, SDA held after the read", ClockBoot::OscStopped);
	checkInt("the bus stuck", senseBus()->stuck, 1);
	checkInt("the read and one write", (long)fakeI2cTransactionsTo(PCF85063_I2C_ADDR), 2);
	checkInt("nothing written to the part", (long)p->writes, 0);

	// The current sensor and the touch controller on the same bus.
	fakeI2cReset();
	p = fakeI2cAddPcf85063(PCF85063_I2C_ADDR);
	fakePcf85063Set(p, 2026, 11, 14, 15, 42, 7);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	fakeI2cAddCst328(CST328_I2C_ADDR);
	boot();
	checkBoot("beside the INA219 and the CST328", ClockBoot::Running);
	checkInt("nothing to the INA219's configuration", (long)ina->configWrites, 0);
	checkInt("nothing to the CST328", (long)fakeI2cTransactionsTo(CST328_I2C_ADDR), 0);
}

/** @brief The state a later suite finds: a clock not set. */
static void leaveNotSet() {
	fakeI2cReset();
	senseInit();
	clockInit();
}

void runClockTests() {
	testBcd();
	testCalendar();
	testDecode();
	testFatTime();
	testSet();
	testNotSet();
	testBus();
	leaveNotSet();
}
