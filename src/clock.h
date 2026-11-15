/**
 * @file clock.h
 * @brief The board's real-time clock (RTC), a PCF85063A at
 *        @ref PCF85063_I2C_ADDR on the shared I2C1 (Inter-Integrated Circuit)
 *        bus, and the time stamps FatFs puts on files (get_fattime()).
 *
 * The clock is read once, at boot, after touchInit() and senseInit() have
 * set the bus up: one transaction of 14 bytes on the wire, registers 00h to
 * 0Ah. From then on core0 keeps the time from that reading plus the 64-bit
 * microsecond timer, so FatFs and the run summary never wait on the bus.
 *
 * The clock is set when the reading shows a running clock: OS, STOP,
 * EXT_TEST and 12_24 all 0 and every time register in range. Anything else,
 * or no answer, leaves it not set for the power-up. With OS set, the
 * oscillator has stopped since the time was written, as at every power-up of
 * a board with no cell on J2: the firmware then sends the software reset,
 * 58h to Control_1, and turns CLKOUT off, 07h to Control_2. With the other
 * causes it sends nothing more.
 *
 * Its transfers go to the current sensor's bus record through senseNote(),
 * as the touch and IMU drivers' do: a timeout counts towards a stuck bus.
 * As the IMU driver, and unlike touch, the clock sends nothing while the
 * record shows the bus stuck.
 *
 * Setting the clock is slice 5 of docs/design/run-summary.md and not built:
 * this file reads the clock and keeps it.
 *
 * @see docs/design/run-summary.md, "The clock"
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup clock_regs PCF85063A registers
 * @brief NXP's PCF85063A data sheet, Rev. 7.3, the parts the driver uses.
 * @{
 */

#define CLOCK_REG_CONTROL_1  0x00u  /**< Control_1. */
#define CLOCK_REG_CONTROL_2  0x01u  /**< Control_2. */
#define CLOCK_REG_SECONDS    0x04u  /**< Seconds; the time runs from here to 0Ah. */

/** @brief Bytes of the boot read: 00h to 0Ah. */
#define CLOCK_READ_BYTES     11u

#define CLOCK_CTL1_EXT_TEST  0x80u  /**< Control_1 bit 7: external clock test mode. */
#define CLOCK_CTL1_STOP      0x20u  /**< Control_1 bit 5: the time does not count. */
#define CLOCK_CTL1_12_24     0x02u  /**< Control_1 bit 1: 12-hour mode. */
#define CLOCK_SECONDS_OS     0x80u  /**< Seconds bit 7: the oscillator has stopped. */

/** @brief Written to Control_1: the software reset, every register to its reset value. */
#define CLOCK_SOFT_RESET     0x58u
/** @brief Written to Control_2: COF = 111, CLKOUT off, no interrupts. */
#define CLOCK_CTL2_QUIET     0x07u

/** @} */

/** @brief A date and time, the local time the clock holds. */
struct ClockTime {
	uint16_t year;    /**< 2000 to 2099 from the part; the kept time can run on past it. */
	uint8_t  month;   /**< 1 to 12. */
	uint8_t  day;     /**< 1 to the month's last day. */
	uint8_t  hour;    /**< 0 to 23. */
	uint8_t  minute;  /**< 0 to 59. */
	uint8_t  second;  /**< 0 to 59. */
};

/** @brief What the boot read found. */
enum class ClockBoot : uint8_t {
	NotRead = 0,  /**< clockInit() has not run. */
	Running,      /**< A running clock: the clock is set. */
	NoAnswer,     /**< No answer at the address, a failed transfer, or the bus stuck. */
	OscStopped,   /**< OS = 1: the time is lost; the software reset was sent. */
	Stopped,      /**< STOP = 1: the time does not count. */
	TestMode,     /**< EXT_TEST = 1. */
	Hour12,       /**< 12_24 = 1: the hours register holds another format. */
	BadTime,      /**< A time register out of range, or a digit above 9. */
};

/** @brief The clock as core0 keeps it, at one moment. */
struct ClockReading {
	bool      set;   /**< The clock is set in this power-up. */
	uint32_t  sets;  /**< Times it became set in this power-up; the boot read counts as one. */
	ClockTime t;     /**< The time; all 0 while not set. */
};

/**
 * @defgroup clock_pure Codec and calendar
 * @brief Pure; host-tested.
 * @{
 */

/**
 * @brief A binary-coded decimal (BCD) byte as a number.
 * @param b   Two BCD digits.
 * @param out The number, 0 to 99.
 * @return false when a digit is above 9.
 */
bool clockBcdIn(uint8_t b, uint8_t *out);

/** @brief A number 0 to 99 as two BCD digits. @param v The number. @return The byte. */
uint8_t clockBcdOut(uint8_t v);

/**
 * @brief Days in @p month of @p year: a year divisible by 4 is a leap year,
 *        right for 2000 to 2099.
 * @param year  The year.
 * @param month 1 to 12.
 * @return The days; 0 for another month.
 */
uint8_t clockDaysIn(uint16_t year, uint8_t month);

/**
 * @brief Whether @p t is a time the part can hold: 2000 to 2099, a day the
 *        month has, 0:00:00 to 23:59:59.
 * @param t The time.
 * @return true when in range.
 */
bool clockValid(const ClockTime *t);

/**
 * @brief What registers 00h to 0Ah say, in the order the boot read checks
 *        them: OS, STOP, EXT_TEST, 12_24, then each time register. Unused
 *        bits are masked as the data sheet marks them; the weekday is not
 *        read.
 * @param r   Registers 00h to 0Ah, as read.
 * @param out The time, filled in for @ref ClockBoot::Running only.
 * @return @ref ClockBoot::Running for a running clock, else the first cause.
 */
ClockBoot clockDecode(const uint8_t *r, ClockTime *out);

/** @brief Seconds from 2000-01-01 00:00:00 to @p t. @param t A valid time. @return Seconds. */
uint32_t clockToSecs(const ClockTime *t);

/** @brief The time @p secs after 2000-01-01 00:00:00. @param secs Seconds. @param out The time. */
void clockFromSecs(uint32_t secs, ClockTime *out);

/**
 * @brief @p t packed as FatFs stamps a file: year − 1980 in bits 31 to 25,
 *        month in 24 to 21, day in 20 to 16, hours in 15 to 11, minutes in
 *        10 to 5, seconds / 2 in 4 to 0.
 * @param t The time.
 * @return The packed time; 0 for a year outside 1980 to 2107, which FAT
 *         cannot hold.
 */
uint32_t clockFatTime(const ClockTime *t);

/** @brief Bytes clockText() writes, the NUL included. */
#define CLOCK_TEXT_BYTES 20

/**
 * @brief @p t as `2026-11-14 15:42:07`, the run summary's form: each field
 *        its fixed width, its lowest digits.
 * @param t   The time.
 * @param out At least @ref CLOCK_TEXT_BYTES bytes.
 */
static inline void clockText(const ClockTime *t, char *out) {
	unsigned y = t->year;
	out[0] = (char)('0' + y / 1000u % 10u);
	out[1] = (char)('0' + y / 100u % 10u);
	out[2] = (char)('0' + y / 10u % 10u);
	out[3] = (char)('0' + y % 10u);
	const uint8_t f[5] = { t->month, t->day, t->hour, t->minute, t->second };
	const char sep[5] = { '-', '-', ' ', ':', ':' };
	for (int i = 0; i < 5; i++) {
		out[4 + 3 * i] = sep[i];
		out[5 + 3 * i] = (char)('0' + f[i] / 10u % 10u);
		out[6 + 3 * i] = (char)('0' + f[i] % 10u);
	}
	out[19] = '\0';
}

/** @} */

/**
 * @defgroup clock_core0 Core0
 * @{
 */

/**
 * @brief In `setup()` after senseInit(): the boot read, the software reset
 *        when OS is set, and the kept time from a running clock. Forgets an
 *        earlier reading.
 */
void clockInit();

/** @brief What the boot read found. @return The cause, @ref ClockBoot::Running when set. */
ClockBoot clockBoot();

/**
 * @brief The kept time now: the boot reading plus the whole seconds of
 *        time_us_64() since it. No bus transfer.
 * @param[out] out The reading; its time is all 0 while not set.
 */
void clockNow(ClockReading *out);

/** @} */

/**
 * @brief FatFs's time stamp, called from inside FatFs on core0: the kept
 *        time packed by clockFatTime() while the clock is set in this
 *        power-up, else 0, FAT date 0. No bus transfer.
 * @return The packed time.
 */
extern "C" uint32_t get_fattime(void);
