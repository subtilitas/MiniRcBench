/**
 * @file clock.cpp
 * @brief The PCF85063A on the shared I2C1 bus: the codec, the boot read,
 *        the kept time and get_fattime(). @see clock.h
 */

#include "clock.h"
#include "board_desc.h"
#include "board_pins.h"
#include "sense.h"

#include "plat.h"
#include <hardware/i2c.h>

#include <string.h>
#include <time.h>

// ---------------------------------------------------------------------------
// codec and calendar
// ---------------------------------------------------------------------------

bool clockBcdIn(uint8_t b, uint8_t *out) {
	uint8_t hi = (uint8_t)(b >> 4), lo = (uint8_t)(b & 0x0Fu);
	if (hi > 9 || lo > 9) return false;
	*out = (uint8_t)(hi * 10u + lo);
	return true;
}

uint8_t clockBcdOut(uint8_t v) { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }

uint8_t clockDaysIn(uint16_t year, uint8_t month) {
	if (month < 1 || month > 12) return 0;
	if (month == 2) return year % 4u == 0 ? 29 : 28;
	// 31 days in the odd months to July and the even ones from August.
	return (uint8_t)(30u + ((month + month / 8u) & 1u));
}

bool clockValid(const ClockTime *t) {
	// A month outside 1 to 12 has 0 days, so no day is in range.
	return t->year >= 2000 && t->year <= 2099 && t->day >= 1 &&
	       t->day <= clockDaysIn(t->year, t->month) && t->hour <= 23 && t->minute <= 59 &&
	       t->second <= 59;
}

ClockBoot clockDecode(const uint8_t *r, ClockTime *out) {
	if (r[CLOCK_REG_SECONDS] & CLOCK_SECONDS_OS) return ClockBoot::OscStopped;
	if (r[CLOCK_REG_CONTROL_1] & CLOCK_CTL1_STOP) return ClockBoot::Stopped;
	if (r[CLOCK_REG_CONTROL_1] & CLOCK_CTL1_EXT_TEST) return ClockBoot::TestMode;
	if (r[CLOCK_REG_CONTROL_1] & CLOCK_CTL1_12_24) return ClockBoot::Hour12;
	// Each register is masked to the bits its BCD range needs: minutes 6 to
	// 0, hours and days 5 to 0, months 4 to 0; the years use all 8, and the
	// seconds' bit 7, OS, is 0 here. Register 08h, the weekday, is not read.
	uint8_t s = 0, mi = 0, h = 0, d = 0, mo = 0, y = 0;
	if (!clockBcdIn(r[0x04], &s) || !clockBcdIn(r[0x05] & 0x7Fu, &mi) ||
	    !clockBcdIn(r[0x06] & 0x3Fu, &h) || !clockBcdIn(r[0x07] & 0x3Fu, &d) ||
	    !clockBcdIn(r[0x09] & 0x1Fu, &mo) || !clockBcdIn(r[0x0A], &y))
		return ClockBoot::BadTime;
	ClockTime t = { (uint16_t)(2000u + y), mo, d, h, mi, s };
	if (!clockValid(&t)) return ClockBoot::BadTime;
	*out = t;
	return ClockBoot::Running;
}

uint32_t clockToSecs(const ClockTime *t) {
	uint32_t days = 0;
	for (uint16_t y = 2000; y < t->year; y++) days += y % 4u == 0 ? 366u : 365u;
	for (uint8_t m = 1; m < t->month; m++) days += clockDaysIn(t->year, m);
	days += t->day - 1u;
	return ((days * 24u + t->hour) * 60u + t->minute) * 60u + t->second;
}

void clockFromSecs(uint32_t secs, ClockTime *out) {
	out->second = (uint8_t)(secs % 60u);
	out->minute = (uint8_t)(secs / 60u % 60u);
	out->hour   = (uint8_t)(secs / 3600u % 24u);
	uint32_t days = secs / 86400u;
	uint16_t y = 2000;
	for (;;) {
		uint32_t len = y % 4u == 0 ? 366u : 365u;
		if (days < len) break;
		days -= len;
		y++;
	}
	uint8_t m = 1;
	while (days >= clockDaysIn(y, m)) {
		days -= clockDaysIn(y, m);
		m++;
	}
	out->year  = y;
	out->month = m;
	out->day   = (uint8_t)(days + 1u);
}

uint32_t clockFatTime(const ClockTime *t) {
	if (t->year < 1980 || t->year > 2107) return 0;
	return (uint32_t)(t->year - 1980u) << 25 | (uint32_t)t->month << 21 |
	       (uint32_t)t->day << 16 | (uint32_t)t->hour << 11 | (uint32_t)t->minute << 5 |
	       (uint32_t)(t->second / 2u);
}

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static ClockBoot s_boot = ClockBoot::NotRead;
static bool      s_set = false;
static uint32_t  s_sets = 0;
static uint32_t  s_baseSecs = 0;  /**< The reading, seconds from 2000-01-01. */
static uint64_t  s_baseUs = 0;    /**< time_us_64() when it was taken. */

/**
 * @brief The SD library's crash.c records this in its crash report. The
 *        library's my_rtc.c, which would define it, is not in the firmware
 *        build; nothing sets it, so it stays 0.
 */
extern "C" {
time_t epochtime = 0;
}

// ---------------------------------------------------------------------------
// the bus
// ---------------------------------------------------------------------------

static i2c_inst_t *bus() { return (i2c_inst_t *)g_board->i2c; }


/** @brief Registers 00h to 0Ah: the pointer, a repeated start, 11 bytes. */
static bool readRegs(uint8_t *r) {
	if (senseBus()->stuck) return false;
	uint8_t reg = CLOCK_REG_CONTROL_1;
	if (senseNote(i2c_write_timeout_us(bus(), PCF85063_I2C_ADDR, &reg, 1, true, SENSE_TIMEOUT_US),
	              1) != SENSE_OK)
		return false;
	return senseNote(i2c_read_timeout_us(bus(), PCF85063_I2C_ADDR, r, CLOCK_READ_BYTES, false,
	                                     SENSE_TIMEOUT_US),
	                 CLOCK_READ_BYTES) == SENSE_OK;
}

/** @brief One register written in a transfer of its own: a START and a STOP around it. */
static void writeReg(uint8_t reg, uint8_t v) {
	if (senseBus()->stuck) return;
	uint8_t b[2] = { reg, v };
	senseNote(i2c_write_timeout_us(bus(), PCF85063_I2C_ADDR, b, 2, false, SENSE_TIMEOUT_US), 2);
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void clockInit() {
	s_set = false;
	s_sets = 0;
	uint8_t r[CLOCK_READ_BYTES];
	if (!readRegs(r)) {
		s_boot = ClockBoot::NoAnswer;
		return;
	}
	ClockTime t = {};
	s_boot = clockDecode(r, &t);
	if (s_boot == ClockBoot::OscStopped) {
		// The time is lost. The reset puts right registers that a supply
		// not starting from 0 V may have left wrong; CLKOUT then stops.
		writeReg(CLOCK_REG_CONTROL_1, CLOCK_SOFT_RESET);
		writeReg(CLOCK_REG_CONTROL_2, CLOCK_CTL2_QUIET);
	}
	if (s_boot != ClockBoot::Running) return;
	s_baseSecs = clockToSecs(&t);
	s_baseUs = time_us_64();
	s_set = true;
	s_sets = 1;
}

ClockBoot clockBoot() { return s_boot; }

void clockNow(ClockReading *out) {
	memset(out, 0, sizeof(*out));
	out->sets = s_sets;
	if (!s_set) return;
	out->set = true;
	clockFromSecs(s_baseSecs + (uint32_t)((time_us_64() - s_baseUs) / 1000000u), &out->t);
}

extern "C" uint32_t get_fattime(void) {
	ClockReading c;
	clockNow(&c);
	return c.set ? clockFatTime(&c.t) : 0;
}
