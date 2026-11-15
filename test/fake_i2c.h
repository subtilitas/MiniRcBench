/**
 * @file fake_i2c.h
 * @brief The host suite's I2C1 bus: the parts on it by address, the clocks
 *        each transaction takes, faults on demand, and the levels of SDA and
 *        SCL.
 *
 * Replaces the stub that failed every transfer: with no part added, every
 * address is answered with a NACK (not acknowledged), PICO_ERROR_GENERIC, as
 * an absent touch controller was before.
 *
 * Clocks are counted as rcbench's sense_sched.h counts them: 9 per byte on
 * the wire, 2 for a start and a stop, 1 for a repeated start. A transfer
 * with nostop leaves the transaction open; the next transfer is its repeated
 * start.
 *
 * The INA219 model: the six registers and their power-on values; RST, which
 * restores them; the pointer kept between transactions; BRNG and PG; the
 * shunt code from a current and a shunt the test sets, 10 µV a code, held at
 * the range of the PG setting, ±32000 at /8; the bus code at IN-, the
 * converter's output less I × R, 4 mV a code. With a clock, the shunt and bus
 * registers change only at the end of each conversion cycle the
 * configuration sets.
 *
 * The QMI8658 model: a register file of 128 bytes with the power-on values
 * the design note gives (CTRL1 20h, big-endian), WHO_AM_I and REVISION_ID as
 * a test sets them, the pointer, address auto-increment by CTRL1 bit 6 and
 * the byte order of the data registers by CTRL1 bit 5, one byte taken from a
 * write of several, the soft reset by B0h in RESET (60h) with dQY_L (4Dh) set
 * after it and cleared by CTRL7 turning a sensor on, no answer for a boot
 * time after a reset, and with the accelerometer on a sample every 32 ms
 * (31.25 Hz) whatever CTRL2 says, each with a time stamp one higher. A test
 * can latch a sample in the middle of the next burst. The latched sample's
 * latch time is kept. Every register read and every write of data is logged
 * with its time.
 *
 * Time: a transfer that times out moves the fake clock on by its timeout, in
 * whole milliseconds as the clock counts them, and adds it to
 * FakeI2cStats::timeoutUs, so fakeI2cBusyUs() bounds how long core0 blocks.
 *
 * The STOP after a read's abort: the SDK's read returns on the abort without
 * waiting for the STOP the block sends after it. So after a read that aborts,
 * at its address or by an injected PICO_ERROR_GENERIC, the next
 * FAKE_I2C_STOP_READS reads of SDA with gpio_get() return low, as the block's
 * own STOP set-up would. The next transfer or i2c_init() ends the window. A
 * write waits for its STOP in the SDK and opens no window.
 *
 * Not modelled: the SDK's restart_on_next. After a write with nostop that
 * fails, the SDK starts the next transfer with a RESTART. On an abort the
 * block has already sent a STOP, and a RESTART on an idle bus is a START, so
 * the next transaction looks the same on the wire. On a timeout the block
 * sends no STOP and is left active; that case is not modelled and is left to
 * the recovery's i2c_init(). The fake therefore closes the transaction on
 * every failure.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief SDA reads that return low after a read's abort: the block's STOP. */
#define FAKE_I2C_STOP_READS 3

/** @brief One INA219 on the bus. Fields a test may set are marked. */
struct FakeIna219 {
	uint8_t  addr;
	uint16_t reg[6];      /**< 00h to 05h as last written; 01h and 02h are computed. */
	uint8_t  ptr;         /**< The register pointer. */
	uint16_t porConfig;   /**< Configuration after RST or POR: 399Fh, or 4127h for another part. Test. */
	double   currentA;    /**< Current through the shunt, IN+ to IN-. Test. */
	double   shuntOhm;    /**< The module's shunt. Test; 0.1 by default. */
	double   voutV;       /**< The converter's output at IN+. Test. */
	bool     timed;       /**< Conversions take their configured time. Test. */
	bool     deaf;        /**< Takes RST and no other configuration. Test. */
	uint64_t cycleUs;     /**< When the running conversion cycle started. */
	int16_t  shuntCode;   /**< The last completed shunt conversion. */
	uint16_t busCode;     /**< The last completed bus conversion, 13 bits. */
	uint32_t configWrites;  /**< Writes of 00h. */
};

/** @brief One register access to the QMI8658 model. */
struct FakeQmiOp {
	bool     write;  /**< A write of data; false for a read. */
	uint8_t  reg;    /**< The register pointer at its start. */
	uint8_t  len;    /**< Data bytes written or read. */
	uint8_t  val;    /**< The first byte written. */
	uint32_t ms;     /**< millis() at the access. */
};

/** @brief Accesses the QMI8658 model logs. */
#define FAKE_QMI_LOG 256

/** @brief One QMI8658 on the bus. Fields a test may set are marked. */
struct FakeQmi8658 {
	uint8_t   addr;
	uint8_t   reg[128];      /**< The register file. */
	uint8_t   ptr;           /**< The register pointer. */
	uint8_t   whoAmI;        /**< WHO_AM_I. Test; 05h by default. */
	uint8_t   revision;      /**< REVISION_ID. Test; 7Ch by default. */
	uint8_t   dqyAfterReset; /**< dQY_L after a reset. Test; 80h, the A part, by default. */
	uint32_t  bootMs;        /**< No answer for this long after a reset. Test; 15 by default. */
	uint8_t   refuseReg;     /**< Writes to this register are ignored; 0 for none. Test. */
	int16_t   acc[3];        /**< X, Y and Z the next sample latches, counts. Test. */
	int16_t   temp;          /**< Temperature the next sample latches, 256 a °C. Test. */
	bool      tearNext;      /**< The next read of 4 bytes or more latches a sample after its 3rd byte. Test. */
	uint64_t  resetUs;       /**< When the last reset was written; 0 for none. */
	bool      everReset;     /**< A reset was written. */
	uint64_t  enUs;          /**< When CTRL7 turned the accelerometer on. */
	uint32_t  timed;         /**< Samples latched by time since then. */
	uint32_t  stamp;         /**< The time stamp of the latched sample. */
	uint64_t  latchUs;       /**< When the latched sample was latched. */
	uint32_t  resets;        /**< Resets written. */
	uint32_t  burstWrites;   /**< Writes of more than one data byte. */
	FakeQmiOp log[FAKE_QMI_LOG];  /**< The accesses, oldest first. */
	uint32_t  logN;          /**< Accesses logged; the log stops at FAKE_QMI_LOG. */
};

/** @brief What the bus has carried since fakeI2cReset(). */
struct FakeI2cStats {
	uint32_t clocks;        /**< SCL clocks of every transaction. */
	uint32_t transfers;     /**< i2c_write_timeout_us() and i2c_read_timeout_us() calls. */
	uint32_t timeouts;      /**< Transfers answered with PICO_ERROR_TIMEOUT. */
	uint32_t inits;         /**< i2c_init() calls. */
	uint32_t sclPulses;     /**< SCL driven low as a GPIO (general-purpose input/output), a recovery's clocks. */
	uint32_t timeoutUs;     /**< Time spent in transfers that timed out, µs. */
	uint32_t starts;        /**< SDA driven low as a GPIO while SCL was high: a START. */
	uint32_t stops;         /**< SDA released after such a START while SCL was high: a STOP. */
};

/** @brief The bus time of @p s, µs: 2.5 µs a clock at 400 kHz, and every timeout in full. */
inline uint32_t fakeI2cBusyUs(const FakeI2cStats &s) { return s.clocks * 5u / 2u + s.timeoutUs; }

/** @brief An empty bus: no parts, both lines free, no faults, counters at 0. */
void fakeI2cReset();

/**
 * @brief Put an INA219 at @p addr in its power-on state: 0.1 Ω, 0 A, 5.0 V.
 * @param addr      Its address.
 * @param porConfig 399Fh, or another part's value after its reset.
 * @return The model, valid until fakeI2cReset().
 */
FakeIna219 *fakeI2cAddIna219(uint8_t addr, uint16_t porConfig = 0x399Fu);

/** @brief A power-on reset of @p p: every register to its power-on value. */
void fakeIna219Por(FakeIna219 *p);

/** @brief Conversion cycle of @p p's configuration, µs: the shunt's time plus the bus's. */
uint32_t fakeIna219CycleUs(const FakeIna219 *p);

/**
 * @brief Put a QMI8658 at @p addr in its power-on state: WHO_AM_I 05h, the A
 *        part's dQY_L and boot time, at rest flat, 0, 0 and +1 g.
 * @param addr Its address.
 * @return The model, valid until fakeI2cReset().
 */
FakeQmi8658 *fakeI2cAddQmi8658(uint8_t addr);

/**
 * @brief The accesses to @p q logged from @p from on that match: a read or a
 *        write of register @p reg.
 * @param q     The model.
 * @param write Writes, or reads.
 * @param reg   The register.
 * @param from  The first log entry looked at.
 * @return Their count.
 */
uint32_t fakeQmiCount(const FakeQmi8658 *q, bool write, uint8_t reg, uint32_t from = 0);

/** @brief Put a CST328 touch controller at @p addr, no finger down. */
void fakeI2cAddCst328(uint8_t addr);

/** @brief The CST328's finger: down at native (@p x, @p y), or up. */
void fakeCst328Finger(bool down, int x, int y);

/** @brief Reads of the CST328's first contact, D000h, answered. */
uint32_t fakeCst328Reads();

/** @brief The CST328 answers its firmware-word read, D1FCh, with 0000h in place of CACAh. */
void fakeCst328WrongId(bool on);

/** @brief Take the part at @p addr off the bus: it NACKs from now on. */
void fakeI2cRemove(uint8_t addr);

/**
 * @brief After @p skip more transfers, the next @p n fail with @p code,
 *        PICO_ERROR_GENERIC or PICO_ERROR_TIMEOUT, whatever their address.
 *        With @p reach each reaches a part that answers before it fails: a
 *        write lands, a read moves the pointer.
 */
void fakeI2cFailNext(int n, int code, int skip = 0, bool reach = false);

/** @brief SDA held low by a part: every transfer times out, gpio_get() reads it low. */
void fakeI2cHoldSda(bool low);

/** @brief SCL held low: every transfer times out, gpio_get() reads it low. */
void fakeI2cHoldScl(bool low);

/**
 * @brief What a transfer returns while SDA alone is held: PICO_ERROR_TIMEOUT,
 *        the default, or PICO_ERROR_GENERIC, a lost arbitration, after 2
 *        clocks.
 */
void fakeI2cHeldSdaCode(int code);

/** @brief The next @p n transfers to a part, of 2 bytes or more, end one byte short: a data NACK. */
void fakeI2cShortNext(int n);

/**
 * @brief SDA held low until @p clocks more clocks on SCL driven as a GPIO,
 *        as a part stopped mid-byte releases it; 0 holds it for ever.
 */
void fakeI2cSdaReleaseAfter(int clocks);

/** @brief The counters. @return A copy. */
FakeI2cStats fakeI2cStats();

/** @brief Transactions addressed to @p addr, answered or not. */
uint32_t fakeI2cTransactionsTo(uint8_t addr);

/** @brief From fakes.cpp's gpio_set_dir(): counts SCL clocks of a recovery. */
void fakeI2cPinDir(uint32_t gpio, bool out);
