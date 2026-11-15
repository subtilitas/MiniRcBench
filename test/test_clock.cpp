/**
 * @file test_clock.cpp
 * @brief The board's clock, src/clock.cpp, against the PCF85063A model of
 *        fake_i2c.h on the shared bus: BCD in and out, the calendar, the
 *        boot read of a set clock and of each cause of a clock not set, the
 *        software reset only with OS set, a bus error at boot, the kept time
 *        across a minute, a day and a year, and get_fattime()'s packing;
 *        the set's transfers and read-back, and the line command of
 *        clock_line.h that carries it on the USB serial port.
 */

#include "check.h"
#include "fakes.h"
#include "fake_i2c.h"
#include "clock.h"
#include "clock_line.h"
#include "run_summary.h"
#include "sd_log.h"
#include "sense.h"
#include "board_pins.h"

#include "hardware/i2c.h"
#include "pico/time.h"

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
	senseNote(PICO_ERROR_TIMEOUT, 2);
	senseNote(PICO_ERROR_TIMEOUT, 2);
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

// ---------------------------------------------------------------------------
// the set
// ---------------------------------------------------------------------------

static void testWeekday() {
	section("Clock: the weekday register");
	ClockTime t = ct(2000, 1, 1, 0, 0, 0);
	checkInt("2000-01-01, a Saturday: 6", clockWeekday(&t), 6);
	t = ct(2000, 1, 2, 23, 59, 59);
	checkInt("2000-01-02, a Sunday: 0", clockWeekday(&t), 0);
	t = ct(2026, 11, 14, 15, 42, 7);
	checkInt("2026-11-14, a Saturday: 6", clockWeekday(&t), 6);
	t = ct(2026, 11, 16, 0, 0, 0);
	checkInt("2026-11-16, a Monday: 1", clockWeekday(&t), 1);
	t = ct(2024, 2, 29, 12, 0, 0);
	checkInt("2024-02-29, a Thursday: 4", clockWeekday(&t), 4);
	t = ct(2099, 12, 31, 23, 59, 59);
	checkInt("2099-12-31, a Thursday: 4", clockWeekday(&t), 4);
}

/** @brief Whether write @p i of @p p's log is @p n bytes equal to @p want. */
static bool wrote(const FakePcf85063 *p, uint32_t i, const uint8_t *want, uint8_t n) {
	return i < p->logN && p->log[i].len == n && memcmp(p->log[i].b, want, n) == 0;
}

static void testClockSet() {
	section("Clock: the set");
	// A power-up with no cell: OS set, the reset at boot, not set.
	FakePcf85063 *p = rtcOnBus();
	boot();
	checkTrue("not set before", !now().set);
	uint32_t logBefore = p->logN, tr = fakeI2cTransactionsTo(PCF85063_I2C_ADDR);
	uint32_t resets = p->resets;
	FakeI2cStats before = fakeI2cStats();
	const ClockTime t = ct(2026, 11, 14, 15, 42, 7);
	checkInt("set", (long)clockSet(&t), (long)ClockSetResult::Set);
	FakeI2cStats after = fakeI2cStats();
	checkInt("four writes and the read-back: 5 transactions",
	         (long)(fakeI2cTransactionsTo(PCF85063_I2C_ADDR) - tr), 5);
	checkInt("308 clocks", (long)(after.clocks - before.clocks), 308);
	checkInt("770 us at 400 kHz", (long)fakeI2cBusyUs(after) - (long)fakeI2cBusyUs(before), 770);
	checkInt("four writes and the read-back's pointer logged", (long)(p->logN - logBefore), 5);
	const uint8_t w1[] = { 0x00, 0x58 };
	const uint8_t w2[] = { 0x00, 0x20, 0x07 };
	const uint8_t w3[] = { 0x04, 0x07, 0x42, 0x15, 0x14, 0x06, 0x11, 0x26 };
	const uint8_t w4[] = { 0x00, 0x00 };
	checkTrue("1: 58h to Control_1, the software reset", wrote(p, logBefore, w1, sizeof(w1)));
	checkTrue("2: 20h, 07h from 00h: STOP, CLKOUT off", wrote(p, logBefore + 1, w2, sizeof(w2)));
	checkTrue("3: 04h to 0Ah, OS clear, Saturday 6", wrote(p, logBefore + 2, w3, sizeof(w3)));
	checkTrue("4: 00h to Control_1, STOP released", wrote(p, logBefore + 3, w4, sizeof(w4)));
	const uint8_t w5[] = { 0x00 };
	checkTrue("then the read-back's pointer, 00h", wrote(p, logBefore + 4, w5, sizeof(w5)));
	checkInt("the reset taken", (long)(p->resets - resets), 1);
	checkInt("Control_2 07h", p->reg[0x01], 0x07);
	checkInt("one read back", (long)p->reads, 2);
	checkTrue("kept: set", now().set);
	checkInt("sets 1", (long)now().sets, 1);
	checkNow("the time sent", "2026-11-14 15:42:07");
	checkTrue("get_fattime() packs it", get_fattime() == clockFatTime(&t));
	checkInt("the boot's cause kept", (long)clockBoot(), (long)ClockBoot::OscStopped);

	// The part counts its first second 0.508 s after STOP's release; the
	// kept time counts whole seconds from the release.
	fakeAdvance(507);
	checkNow("0.507 s on, kept", "2026-11-14 15:42:07");
	checkInt("0.507 s on, the part's seconds", p->reg[0x04], 0x07);
	uint8_t reg = 0x04;
	uint8_t secs = 0;
	fakeAdvance(1);
	i2c_write_timeout_us((i2c_inst_t *)nullptr, PCF85063_I2C_ADDR, &reg, 1, true, 1000);
	i2c_read_timeout_us((i2c_inst_t *)nullptr, PCF85063_I2C_ADDR, &secs, 1, false, 1000);
	checkInt("0.508 s on, the part counts its first second", secs, 0x08);
	fakeAdvance(491);
	checkNow("0.999 s on, kept: the same second", "2026-11-14 15:42:07");
	fakeAdvance(1);
	checkNow("1 s on, kept: the next", "2026-11-14 15:42:08");
	// A boot now reads what the set wrote.
	fakeAdvance(2000);
	boot();
	checkBoot("the next boot reads it running", ClockBoot::Running);
	checkNow("at the time it counted to", "2026-11-14 15:42:10");

	// A set over a clock the boot read running: sets 2.
	const ClockTime u = ct(2027, 2, 28, 23, 59, 59);
	checkInt("a second set", (long)clockSet(&u), (long)ClockSetResult::Set);
	checkInt("sets 2", (long)now().sets, 2);
	checkNow("its time", "2027-02-28 23:59:59");
	fakeAdvance(1000);
	checkNow("1 s on: 1 March, 2027 is no leap year", "2027-03-01 00:00:00");

	// Times the part cannot hold: refused, nothing sent, nothing kept.
	const ClockTime bad[] = {
		ct(1999, 12, 31, 23, 59, 59), ct(2100, 1, 1, 0, 0, 0), ct(2027, 2, 29, 0, 0, 0),
		ct(2026, 11, 31, 0, 0, 0),    ct(2026, 0, 1, 0, 0, 0),  ct(2026, 13, 1, 0, 0, 0),
		ct(2026, 1, 0, 0, 0, 0),      ct(2026, 1, 1, 24, 0, 0), ct(2026, 1, 1, 0, 60, 0),
		ct(2026, 1, 1, 0, 0, 60),
	};
	tr = fakeI2cTransactionsTo(PCF85063_I2C_ADDR);
	int refused = 0;
	for (const ClockTime &b : bad) refused += clockSet(&b) == ClockSetResult::BadDate;
	checkInt("ten bad times: each BAD DATE", refused, 10);
	checkInt("nothing sent for them", (long)(fakeI2cTransactionsTo(PCF85063_I2C_ADDR) - tr), 0);
	checkInt("sets still 2", (long)now().sets, 2);
	checkNow("the kept time unchanged", "2027-03-01 00:00:00");
	const ClockTime edge[] = { ct(2000, 1, 1, 0, 0, 0), ct(2099, 12, 31, 23, 59, 59),
	                           ct(2028, 2, 29, 12, 0, 0) };
	int ok = 0;
	for (const ClockTime &e : edge) ok += clockSet(&e) == ClockSetResult::Set;
	checkInt("2000-01-01, 2099-12-31 23:59:59, 2028-02-29: set", ok, 3);
	checkNow("the last of them", "2028-02-29 12:00:00");
}

static void testClockSetBus() {
	section("Clock: the set on a failing bus");
	// No part: the first write is not acknowledged.
	fakeI2cReset();
	boot();
	const ClockTime t = ct(2026, 11, 14, 15, 42, 7);
	checkInt("no part: no answer", (long)clockSet(&t), (long)ClockSetResult::NoAnswer);
	checkInt("the boot's read and one try", (long)fakeI2cTransactionsTo(PCF85063_I2C_ADDR), 2);
	checkTrue("not set", !now().set && now().sets == 0);

	// Each of the six transfers failing in turn: the set ends there, and the
	// kept time and the count stay as an earlier set left them. The
	// read-back's pointer write and read, transfers 5 and 6, are one
	// transaction with a repeated start.
	for (int k = 0; k < 6; k++) {
		FakePcf85063 *p = rtcOnBus();
		boot();
		const ClockTime first = ct(2026, 1, 2, 3, 4, 5);
		clockSet(&first);
		uint32_t tr = fakeI2cTransactionsTo(PCF85063_I2C_ADDR);
		fakeI2cFailNext(1, PICO_ERROR_GENERIC, k);
		char w[64];
		snprintf(w, sizeof(w), "transfer %d failing: no answer", k + 1);
		checkInt(w, (long)clockSet(&t), (long)ClockSetResult::NoAnswer);
		const int want = k < 4 ? k + 1 : 5;
		snprintf(w, sizeof(w), "transfer %d failing: %d transactions, none after", k + 1, want);
		checkInt(w, (long)(fakeI2cTransactionsTo(PCF85063_I2C_ADDR) - tr), want);
		snprintf(w, sizeof(w), "transfer %d failing: the earlier set kept", k + 1);
		checkNow(w, "2026-01-02 03:04:05");
		snprintf(w, sizeof(w), "transfer %d failing: sets 1", k + 1);
		checkInt(w, (long)now().sets, 1);
		(void)p;
	}

	// A timeout on the reset counts towards a stuck bus; two mark it stuck,
	// and a set then sends nothing.
	rtcOnBus();
	boot();
	fakeI2cFailNext(1, PICO_ERROR_TIMEOUT);
	checkInt("a timeout: no answer", (long)clockSet(&t), (long)ClockSetResult::NoAnswer);
	checkInt("noted to the bus record", senseBus()->timeouts, 1);
	senseNote(PICO_ERROR_TIMEOUT, 2);
	checkInt("stuck", senseBus()->stuck, 1);
	uint32_t tr = fakeI2cTransactionsTo(PCF85063_I2C_ADDR);
	checkInt("stuck: no answer", (long)clockSet(&t), (long)ClockSetResult::NoAnswer);
	checkInt("nothing sent", (long)(fakeI2cTransactionsTo(PCF85063_I2C_ADDR) - tr), 0);

	// A short write: the time's last byte not acknowledged.
	rtcOnBus();
	boot();
	fakeI2cShortNext(1);
	checkInt("the reset a byte short: no answer", (long)clockSet(&t), (long)ClockSetResult::NoAnswer);
	checkTrue("not set", !now().set);

	// The read-back differs: a register the part drops, so the time it
	// holds is not the time sent, or STOP stays set, or OS stays set.
	const struct { const char *what; uint8_t reg; } drops[] = {
		{ "the years dropped", 0x0A }, { "the minutes dropped", 0x05 },
		{ "the hours dropped", 0x06 }, { "the seconds dropped: OS stays set", 0x04 },
	};
	for (const auto &d : drops) {
		FakePcf85063 *p = rtcOnBus();
		boot();
		p->refuse = true;
		p->refuseReg = d.reg;
		char w[64];
		snprintf(w, sizeof(w), "%s: READ-BACK", d.what);
		checkInt(w, (long)clockSet(&t), (long)ClockSetResult::ReadBack);
		snprintf(w, sizeof(w), "%s: not set", d.what);
		checkTrue(w, !now().set && now().sets == 0 && get_fattime() == 0);
	}

	// The INA219 and the CST328 on the bus: untouched by a set.
	fakeI2cReset();
	FakePcf85063 *p = fakeI2cAddPcf85063(PCF85063_I2C_ADDR);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	fakeI2cAddCst328(CST328_I2C_ADDR);
	boot();
	checkInt("beside the INA219 and the CST328: set", (long)clockSet(&t), (long)ClockSetResult::Set);
	checkInt("nothing to the INA219's configuration", (long)ina->configWrites, 0);
	checkInt("nothing to the CST328", (long)fakeI2cTransactionsTo(CST328_I2C_ADDR), 0);
	(void)p;
}

/** @brief A line as clockLineParse() reads it. */
static ClockCmd parse(const char *line, ClockTime *t = nullptr) {
	ClockTime v = {};
	size_t n = strlen(line);
	bool over = n > CLOCK_LINE_MAX;
	ClockCmd c = clockLineParse(line, over ? CLOCK_LINE_MAX : n, over, &v);
	if (t) *t = v;
	return c;
}

static void testLineParse() {
	section("Clock: the serial line's parser");
	ClockTime t = {};
	checkInt("a set", (long)parse("CLOCK 2026-11-14 15:42:07", &t), (long)ClockCmd::Set);
	checkTrue("its time", t.year == 2026 && t.month == 11 && t.day == 14 && t.hour == 15 &&
	          t.minute == 42 && t.second == 7);
	checkInt("2000-01-01 00:00:00", (long)parse("CLOCK 2000-01-01 00:00:00", &t), (long)ClockCmd::Set);
	checkTrue("its time", t.year == 2000 && t.month == 1 && t.day == 1 && t.hour == 0 &&
	          t.minute == 0 && t.second == 0);
	checkInt("2099-12-31 23:59:59", (long)parse("CLOCK 2099-12-31 23:59:59", &t), (long)ClockCmd::Set);
	checkTrue("its time", t.year == 2099 && t.month == 12 && t.day == 31 && t.hour == 23 &&
	          t.minute == 59 && t.second == 59);
	checkInt("the leap day 2028", (long)parse("CLOCK 2028-02-29 12:00:00"), (long)ClockCmd::Set);
	checkInt("a query", (long)parse("CLOCK?"), (long)ClockCmd::Query);

	const char *bad_dates[] = {
		"CLOCK 1999-12-31 23:59:59", "CLOCK 2100-01-01 00:00:00", "CLOCK 2027-02-29 00:00:00",
		"CLOCK 2026-11-31 00:00:00", "CLOCK 2026-00-10 00:00:00", "CLOCK 2026-13-10 00:00:00",
		"CLOCK 2026-11-00 00:00:00", "CLOCK 2026-11-14 24:00:00", "CLOCK 2026-11-14 15:60:00",
		"CLOCK 2026-11-14 15:42:60", "CLOCK 0000-00-00 00:00:00", "CLOCK 9999-99-99 99:99:99",
	};
	for (const char *l : bad_dates) {
		char w[64];
		snprintf(w, sizeof(w), "%s: bad date", l);
		checkInt(w, (long)parse(l, &t), (long)ClockCmd::BadDate);
	}
	const char *bad_lines[] = {
		"CLOCK", "CLOCK ", "CLOCK?X", "CLOCK ?", "CLOCK??", "CLOCKX",
		"CLOCK 2026-11-14 15:42",  "CLOCK 2026-11-14 15:42:7", "CLOCK 2026-11-14 15:42:077",
		"CLOCK 2026-11-14 15:42:07 ", "CLOCK  2026-11-14 15:42:07", "CLOCK 2026-11-14  15:42:07",
		"CLOCK 2026/11/14 15:42:07", "CLOCK 2026-11-14T15:42:07", "CLOCK 2026-11-14 15.42.07",
		"CLOCK 2026-1-14 15:42:07X", "CLOCK 2026-11-14 15:42:0x", "CLOCK +026-11-14 15:42:07",
		"CLOCK 2026-11-14 15:42:07Z", "CLOCK:2026-11-14 15:42:07", "CLOCK 2026-11-14 -5:42:07",
		"CLOCK 2026-11-14 15:42:07 and more than the line holds",
	};
	for (const char *l : bad_lines) {
		char w[80];
		snprintf(w, sizeof(w), "'%s': bad line", l);
		checkInt(w, (long)parse(l, &t), (long)ClockCmd::BadLine);
	}
	const char *ignored[] = {
		"", "C", "CLOC", "clock 2026-11-14 15:42:07", "clock?", " CLOCK?", "XCLOCK?",
		"arm=0 thr=   0 rpm=     0", "CLOCk?",
		"a line longer than the line holds, not starting with the word",
	};
	for (const char *l : ignored) {
		char w[80];
		snprintf(w, sizeof(w), "'%s': ignored", l);
		checkInt(w, (long)parse(l, &t), (long)ClockCmd::Ignore);
	}
	// A NUL inside a line is a byte like any other.
	const char nul[] = "CLOCK 2026-11-14 15:42:0\0";
	checkInt("a NUL for a digit: bad line", (long)clockLineParse(nul, 25, false, &t),
	         (long)ClockCmd::BadLine);
	// Exactly 32 characters: kept whole, and not the set's form.
	checkInt("32 characters starting with CLOCK: bad line",
	         (long)clockLineParse("CLOCK 2026-11-14 15:42:07 ABCDEF", 32, false, &t),
	         (long)ClockCmd::BadLine);
	// A set's 25 characters marked over: bad line.
	checkInt("the set's form, but longer than kept: bad line",
	         (long)clockLineParse("CLOCK 2026-11-14 15:42:07", 25, true, &t), (long)ClockCmd::BadLine);
	checkInt("the query, but longer than kept: bad line",
	         (long)clockLineParse("CLOCK?", 6, true, &t), (long)ClockCmd::BadLine);
}

/** @brief The bytes clockLinePoll() reads in the tests. */
static const char *s_feed = "";
static size_t s_feedLen = 0, s_feedAt = 0;
static int feedGet() { return s_feedAt < s_feedLen ? (uint8_t)s_feed[s_feedAt++] : -1; }
static void feed(const char *bytes, size_t n) { s_feed = bytes; s_feedLen = n; s_feedAt = 0; }
static void feed(const char *bytes) { feed(bytes, strlen(bytes)); }

/** @brief One poll's answer, as text; "" for none. */
static const char *poll(ClockLineIn *in, char *out) {
	size_t n = clockLinePoll(in, feedGet, out);
	if (n == 0) out[0] = '\0';
	return out;
}

static void testLinePoll() {
	section("Clock: the serial line, byte by byte");
	FakePcf85063 *p = rtcOnBus();
	boot();
	ClockLineIn in = {};
	char a[CLOCK_ANSWER_BYTES];

	feed("CLOCK?\n");
	checkStr("a query before a set", poll(&in, a), "CLOCK NOT SET\r\n");
	checkInt("every byte read", (long)s_feedAt, 7);
	feed("CLOCK 2026-11-14 15:42:07\r\n");
	checkStr("a set, CR LF", poll(&in, a), "CLOCK SET 2026-11-14 15:42:07\r\n");
	checkInt("read to the CR, the LF left for the next pass", (long)s_feedAt, 26);
	checkStr("the LF alone: nothing", poll(&in, a), "");
	checkTrue("the clock set", now().set && now().sets == 1);
	fakeAdvance(2000);
	feed("CLOCK?\r");
	checkStr("a query, CR alone", poll(&in, a), "CLOCK 2026-11-14 15:42:09\r\n");

	// One line a pass: a second waits.
	feed("CLOCK?\nCLOCK 2026-11-14 16:00:00\n");
	checkStr("the first of two lines", poll(&in, a), "CLOCK 2026-11-14 15:42:09\r\n");
	checkInt("the second not read", (long)s_feedAt, 7);
	checkNow("nor carried out", "2026-11-14 15:42:09");
	size_t at = s_feedAt;
	feed(s_feed + at, s_feedLen - at);
	checkStr("the second at the next pass", poll(&in, a), "CLOCK SET 2026-11-14 16:00:00\r\n");

	// A line split across passes.
	feed("CLOCK 2026-11-");
	checkStr("half a line: nothing", poll(&in, a), "");
	feed("14 15:42:07\n");
	checkStr("the rest: the set", poll(&in, a), "CLOCK SET 2026-11-14 15:42:07\r\n");

	// Ignored lines: no answer, and the next line is read whole.
	feed("arm=0 thr=   0 rpm=     0 erpm=     0\n");
	checkStr("a line not starting with CLOCK: no answer", poll(&in, a), "");
	feed("\r\n\r\n\n");
	checkStr("blank lines: no answer", poll(&in, a), "");
	checkInt("all read", (long)s_feedAt, 5);
	feed("clock?\nCLOCK?\n");
	checkStr("lower case: ignored", poll(&in, a), "");
	checkStr("then the query", poll(&in, a), "CLOCK 2026-11-14 15:42:07\r\n");
	// A line shorter than CLOCK, over the query's bytes still in the buffer.
	feed("CLOC\n");
	checkStr("CLOC after CLOCK?: ignored", poll(&in, a), "");

	// The refusals.
	uint32_t tr = fakeI2cTransactionsTo(PCF85063_I2C_ADDR);
	feed("CLOCK 2026-02-30 10:00:00\n");
	checkStr("30 February", poll(&in, a), "CLOCK REFUSED: BAD DATE\r\n");
	feed("CLOCK 2026-11-14 15:42\n");
	checkStr("no seconds", poll(&in, a), "CLOCK REFUSED: BAD LINE\r\n");
	feed("CLOCKS\n");
	checkStr("CLOCKS", poll(&in, a), "CLOCK REFUSED: BAD LINE\r\n");
	checkInt("nothing sent for them", (long)(fakeI2cTransactionsTo(PCF85063_I2C_ADDR) - tr), 0);
	checkNow("the clock unchanged", "2026-11-14 15:42:07");

	// A line longer than kept: bad when it starts with CLOCK, then the next
	// line is read whole.
	char lng[200];
	snprintf(lng, sizeof(lng), "CLOCK 2026-11-14 15:42:07%0150d\nCLOCK?\n", 0);
	feed(lng);
	checkStr("a line of 175 characters", poll(&in, a), "CLOCK REFUSED: BAD LINE\r\n");
	checkStr("then the query", poll(&in, a), "CLOCK 2026-11-14 15:42:07\r\n");
	snprintf(lng, sizeof(lng), "%0150d\nCLOCK?\n", 0);
	feed(lng);
	checkStr("a long line not starting with CLOCK: nothing", poll(&in, a), "");
	checkStr("then the query", poll(&in, a), "CLOCK 2026-11-14 15:42:07\r\n");
	// A line of exactly 32 characters, then one of 33.
	feed("CLOCK 2026-11-14 15:42:07 ABCDEF\nCLOCK 2026-11-14 15:42:07 ABCDEFG\n");
	checkStr("32 characters: bad line", poll(&in, a), "CLOCK REFUSED: BAD LINE\r\n");
	checkStr("33 characters: bad line", poll(&in, a), "CLOCK REFUSED: BAD LINE\r\n");
	// The query form with the overflow flag cleared after a long line.
	feed("CLOCK 2026-11-14 15:43:00\n");
	checkStr("after them, a set", poll(&in, a), "CLOCK SET 2026-11-14 15:43:00\r\n");

	// No answer at 0x51.
	fakeI2cRemove(PCF85063_I2C_ADDR);
	feed("CLOCK 2026-11-14 15:44:00\n");
	checkStr("the part gone", poll(&in, a), "CLOCK REFUSED: RTC NOT ANSWERING\r\n");
	checkNow("the clock kept as it was", "2026-11-14 15:43:00");
	// The read-back differs.
	p = rtcOnBus();
	boot();
	p->refuse = true;
	p->refuseReg = 0x0A;
	feed("CLOCK 2026-11-14 15:44:00\n");
	checkStr("a register dropped", poll(&in, a), "CLOCK REFUSED: RTC READ-BACK DIFFERS\r\n");
	checkStr("and the query", (feed("CLOCK?\n"), poll(&in, a)), "CLOCK NOT SET\r\n");

	// The answers fit their buffer, CR LF and NUL included.
	ClockTime t = ct(2026, 11, 14, 15, 42, 7);
	char big[128];
	size_t most = 0;
	const ClockCmd cmds[] = { ClockCmd::BadLine, ClockCmd::BadDate, ClockCmd::Query, ClockCmd::Set };
	for (ClockCmd c : cmds) {
		size_t n = clockLineAnswer(c, &t, big);
		if (n > most) most = n;
	}
	checkTrue("the longest answer, READ-BACK DIFFERS, fits", strlen("CLOCK REFUSED: RTC READ-BACK DIFFERS\r\n") + 1 <= CLOCK_ANSWER_BYTES);
	checkTrue("every answer fits", most + 1 <= CLOCK_ANSWER_BYTES);
	checkInt("Ignore: no answer", (long)clockLineAnswer(ClockCmd::Ignore, &t, big), 0);
}

/**
 * @brief A set from the serial line during a log: the summary's stop line
 *        notes it, the durations from the timer do not move.
 */
/** @brief The line of @p text that starts with @p head, without its line feed. */
static const char *lineOf(const char *text, const char *head, char *out, size_t n) {
	const char *l = strstr(text, head);
	out[0] = '\0';
	if (!l) return out;
	size_t k = 0;
	while (l[k] && l[k] != '\n' && k + 1 < n) { out[k] = l[k]; k++; }
	out[k] = '\0';
	return out;
}

/** @brief One line through clockLinePoll(), as the USB pass hands it on. */
static void sendLine(const char *line) {
	static ClockLineIn in;
	char a[CLOCK_ANSWER_BYTES];
	feed(line);
	clockLinePoll(&in, feedGet, a);
}

static void testSetDuringLog() {
	section("Clock: a set during a log");
	SdLogStatus st = {};
	st.fileNumber = 12;
	char t[1200], l[96];
	// Not set at the start, set over the line during the log: the start
	// reads not set, the stop the set clock, with the note. The timer's
	// durations do not move.
	rtcOnBus();
	boot();
	RunSum s;
	runSumBegin(&s, time_us_64(), false);
	clockNow(&s.clockStart);
	fakeAdvance(30000);
	sendLine("CLOCK 2026-11-14 15:42:07\n");
	fakeAdvance(14000);
	clockNow(&s.clockStop);
	runSumEnd(&s, time_us_64(), RunSumEnd::Stop);
	runSumText(&s, &st, 14, t, sizeof(t));
	checkStr("set first during the log: the start", lineOf(t, "Clock at start:", l, sizeof(l)),
	         "Clock at start: not set");
	checkStr("and the stop, noted", lineOf(t, "Clock at stop:", l, sizeof(l)),
	         "Clock at stop: 2026-11-14 15:42:21 (clock set during the log)");
	checkStr("Duration from the timer", lineOf(t, "Duration:", l, sizeof(l)), "Duration: 44.0 s");

	// Set at the start, set again during the log.
	runSumBegin(&s, time_us_64(), false);
	clockNow(&s.clockStart);
	fakeAdvance(5000);
	sendLine("CLOCK 2026-11-14 16:00:00\n");
	fakeAdvance(5000);
	clockNow(&s.clockStop);
	runSumEnd(&s, time_us_64(), RunSumEnd::Stop);
	runSumText(&s, &st, 14, t, sizeof(t));
	checkStr("set again: the start", lineOf(t, "Clock at start:", l, sizeof(l)),
	         "Clock at start: 2026-11-14 15:42:21");
	checkStr("the stop, noted", lineOf(t, "Clock at stop:", l, sizeof(l)),
	         "Clock at stop: 2026-11-14 16:00:05 (clock set during the log)");
	checkStr("Duration 10.0 s", lineOf(t, "Duration:", l, sizeof(l)), "Duration: 10.0 s");

	// No set during the log: no note. A refused set is not one.
	runSumBegin(&s, time_us_64(), false);
	clockNow(&s.clockStart);
	sendLine("CLOCK 2026-02-30 16:00:00\n");
	sendLine("CLOCK?\n");
	fakeAdvance(3000);
	clockNow(&s.clockStop);
	runSumEnd(&s, time_us_64(), RunSumEnd::Stop);
	runSumText(&s, &st, 14, t, sizeof(t));
	checkStr("no set: no note", lineOf(t, "Clock at stop:", l, sizeof(l)),
	         "Clock at stop: 2026-11-14 16:00:08");
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
	testWeekday();
	testClockSet();
	testClockSetBus();
	testLineParse();
	testLinePoll();
	testSetDuringLog();
	leaveNotSet();
}
