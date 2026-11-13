/**
 * @file sense.cpp
 * @brief The INA219 on the shared I2C1 bus: codec, probe, reading, recovery
 *        and the sample for core1. @see sense.h
 */

#include "sense.h"
#include "board_desc.h"
#include "settings.h"
#include "touch.h"

#include "plat.h"
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <pico/critical_section.h>

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// codec
// ---------------------------------------------------------------------------

sense_value_t ina219CurrentUa(uint16_t raw, uint32_t shuntMohm) {
	sense_value_t v = { 0, SENSE_CLIP_NONE };
	// At /8 the register is the shunt code in two's complement, its sign
	// extended through bit 15 (SBOS448G, "Shunt Voltage Register", to
	// confirm), so 16 bits read as a signed number at the bench's gain.
	int32_t code = sense_sext32(raw, 16u);
	if (code >= INA219_SHUNT_END)  { v.clip = SENSE_CLIP_HIGH; return v; }
	if (code <= -INA219_SHUNT_END) { v.clip = SENSE_CLIP_LOW;  return v; }
	// µA = code × 10 µV / (mΩ / 1000).
	v.value = (int32_t)sense_muldiv(code, INA219_SHUNT_UV * 1000u, shuntMohm);
	return v;
}

uint16_t ina219BusMv(uint16_t raw) {
	// Bits 15 to 3 hold the code; bit 1 is CNVR and bit 0 OVF, neither read
	// here (SBOS448G, "Bus Voltage Register", to confirm).
	return (uint16_t)((raw >> 3) * INA219_BUS_MV);
}

sense_err_t senseErrOf(int result, size_t want) {
	if (result >= 0 && (size_t)result == want) return SENSE_OK;
	if (result == PICO_ERROR_TIMEOUT) return SENSE_TIMEOUT;
	// PICO_ERROR_GENERIC: an abort, an address or a byte not acknowledged,
	// or arbitration lost; a short count. The wire tells a lost arbitration
	// from a NACK by the lines (errAfter()).
	return SENSE_NACK;
}

int32_t senseEndUa() {
	return (int32_t)sense_muldiv(INA219_SHUNT_END, INA219_SHUNT_UV * 1000u, INA219_SHUNT_MOHM);
}

bool senseFresh(const SenseSample *s, uint32_t nowMs) {
	return s->seq != 0 && (uint32_t)(nowMs - s->takenMs) < SENSE_STALE_MS;
}

/**
 * @brief @p text into @p buf, cut to @p n - 1 characters. The texts are
 *        formatted into a buffer of their own first, so a caller's short
 *        buffer cuts the text rather than the format.
 */
static void put(char *buf, size_t n, const char *text) {
	if (n == 0) return;
	size_t len = strlen(text);
	if (len > n - 1) len = n - 1;
	memcpy(buf, text, len);
	buf[len] = '\0';
}

/** @brief @p v hundredths with its sign, rounded half away from zero, as `-0.42`. */
static void hundredths(char *buf, size_t n, int32_t v, int32_t perHundredth, const char *unit) {
	bool neg = v < 0;
	uint32_t a = (uint32_t)(neg ? -(int64_t)v : v);
	uint32_t h = (a + (uint32_t)perHundredth / 2u) / (uint32_t)perHundredth;
	if (h == 0) neg = false;
	char t[32];
	snprintf(t, sizeof(t), "%s%lu.%02lu %s", neg ? "-" : "", (unsigned long)(h / 100u),
	         (unsigned long)(h % 100u), unit);
	put(buf, n, t);
}

bool senseCurrentText(const SenseSample *s, uint32_t nowMs, char *buf, size_t n) {
	if (!senseFresh(s, nowMs)) {
		put(buf, n, "-- A");
		return false;
	}
	if (s->flags & SENSE_FLAG_CLIPPED) {
		// The range's end in tenths of an ampere, from the shunt: 3.2 A on
		// 0.1 ohm.
		uint32_t tenths = (uint32_t)(senseEndUa() / 100000);
		char t[32];
		snprintf(t, sizeof(t), "%s%lu.%lu A", s->currentUa < 0 ? "<-" : ">",
		         (unsigned long)(tenths / 10u), (unsigned long)(tenths % 10u));
		put(buf, n, t);
		return true;
	}
	hundredths(buf, n, s->currentUa, 10000, "A");
	return false;
}

void senseVoltText(const SenseSample *s, uint32_t nowMs, char *buf, size_t n) {
	if (!senseFresh(s, nowMs)) {
		put(buf, n, "-- V");
		return;
	}
	hundredths(buf, n, s->busMv, 10, "V");
}

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

static critical_section_t s_cs;
static bool         s_csReady = false;
static SenseSample  s_pub;        /**< What senseSample() hands out. */
static sense_bus_t  s_bus;
static sense_part_t s_part;
static uint8_t      s_mode = 0;
static uint32_t     s_seq = 0;
static uint32_t     s_readbackAt = 0;  /**< When the configuration is read back next. */
static uint8_t      s_misses = 0;      /**< Read-backs in a row that found another value. */
static bool         s_resetSeen = false;  /**< The next reading carries SENSE_FLAG_RESET. */
static int32_t      s_lastUa = 0;
static uint16_t     s_lastMv = 0;
static uint8_t      s_lastFlags = 0;
static uint32_t     s_lastMs = 0;

static i2c_inst_t *bus() { return (i2c_inst_t *)g_board->i2c; }

/** @brief Either line low before a transaction: nothing goes out. */
static bool linesLow() {
	return !gpio_get(g_board->sda) || !gpio_get(g_board->scl);
}

/**
 * @brief What a transfer came to, with a lost arbitration told from a NACK.
 *
 * The SDK returns PICO_ERROR_GENERIC for every abort of the I2C block,
 * a NACK and a lost arbitration alike. A part holding SDA low makes the
 * block lose arbitration on the first 1 it sends (DW_apb_i2c, the RP2350's
 * I2C block, as its abort source register describes it; to be confirmed on
 * hardware, bench test B3). A NACK leaves both lines high, so a line read
 * low after the abort makes it SENSE_BUS_LOW, which marks the bus stuck,
 * where a NACK would clear it.
 *
 * The SDK's read returns on the abort without waiting for the STOP the
 * block sends after it, so the block's own STOP can still hold a line low.
 * The lines are therefore polled for up to @ref SENSE_SETTLE_US, and only a
 * line low for all of it counts.
 *
 * @param result The SDK's return.
 * @param want   The byte count asked.
 * @return As senseErrOf(), with SENSE_NACK as SENSE_BUS_LOW while a line is low.
 */
static sense_err_t errAfter(int result, size_t want) {
	sense_err_t e = senseErrOf(result, want);
	if (e != SENSE_NACK) return e;
	for (uint32_t us = 0; linesLow(); us++) {
		if (us >= SENSE_SETTLE_US) return SENSE_BUS_LOW;
		delayMicroseconds(1);
	}
	return e;
}

/** @brief The wire's read: the pointer, a repeated start, @p n bytes. */
static sense_err_t wireRead(void *, uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
	if (linesLow()) return SENSE_BUS_LOW;
	sense_err_t e = errAfter(i2c_write_timeout_us(bus(), addr, &reg, 1, true, SENSE_TIMEOUT_US), 1);
	if (e != SENSE_OK) return e;
	return errAfter(i2c_read_timeout_us(bus(), addr, buf, n, false, SENSE_TIMEOUT_US), n);
}

/** @brief The wire's write: the pointer and @p n bytes in one transfer. */
static sense_err_t wireWrite(void *, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t n) {
	if (linesLow()) return SENSE_BUS_LOW;
	uint8_t b[3] = { reg, 0, 0 };
	if (n > 2) n = 2;
	memcpy(b + 1, buf, n);
	return errAfter(i2c_write_timeout_us(bus(), addr, b, n + 1, false, SENSE_TIMEOUT_US), n + 1);
}

static const sense_i2c_t WIRE = { wireRead, wireWrite, nullptr };

/** @brief The state a consumer sees now. */
static SenseState stateNow() {
	if (s_mode != (uint8_t)SenseMode::Ina219) return SenseState::None;
	if (s_bus.stuck) return SenseState::Stuck;
	switch (s_part.state) {
		case SENSE_PART_ONLINE:  return SenseState::Online;
		case SENSE_PART_OFFLINE: return SenseState::Offline;
		default:                 return SenseState::Absent;
	}
}

static void lockedPublish() {
	if (s_csReady) critical_section_enter_blocking(&s_cs);
	s_pub.seq       = s_seq;
	s_pub.takenMs   = s_lastMs;
	s_pub.currentUa = s_lastUa;
	s_pub.busMv     = s_lastMv;
	s_pub.flags     = s_lastFlags;
	s_pub.state     = (uint8_t)stateNow();
	if (s_csReady) critical_section_exit(&s_cs);
}

void senseSample(SenseSample *out) {
	if (s_csReady) critical_section_enter_blocking(&s_cs);
	*out = s_pub;
	if (s_csReady) critical_section_exit(&s_cs);
}

// ---------------------------------------------------------------------------
// the bus
// ---------------------------------------------------------------------------

/**
 * @brief Free a bus a part holds: SCL, switched to SIO (single-cycle I/O),
 *        pulled low and released @ref SENSE_RECOVER_CLOCKS times at about
 *        100 kHz, a START and a STOP, then both lines back to I2C with the block
 *        initialised again. The pads' pull-ups stay on: a released line is an
 *        input, a low one an output at 0.
 */
static void recoverBus() {
	const uint8_t sda = g_board->sda, scl = g_board->scl;
	gpio_put(sda, 0);
	gpio_put(scl, 0);
	gpio_set_dir(sda, GPIO_IN);
	gpio_set_dir(scl, GPIO_IN);
	gpio_set_function(sda, GPIO_FUNC_SIO);
	gpio_set_function(scl, GPIO_FUNC_SIO);
	for (unsigned i = 0; i < SENSE_RECOVER_CLOCKS; i++) {
		gpio_set_dir(scl, GPIO_OUT);
		delayMicroseconds(SENSE_RECOVER_HALF_US);
		gpio_set_dir(scl, GPIO_IN);
		delayMicroseconds(SENSE_RECOVER_HALF_US);
	}
	// SCL is high: SDA falling is a START, SDA rising the STOP that ends
	// whatever transaction a part was in.
	gpio_set_dir(sda, GPIO_OUT);
	delayMicroseconds(SENSE_RECOVER_HALF_US);
	gpio_set_dir(sda, GPIO_IN);
	delayMicroseconds(SENSE_RECOVER_HALF_US);
	i2c_init(bus(), TOUCH_I2C_HZ);
	gpio_set_function(sda, GPIO_FUNC_I2C);
	gpio_set_function(scl, GPIO_FUNC_I2C);
}

// ---------------------------------------------------------------------------
// the part
// ---------------------------------------------------------------------------

/** @brief A register of the part, outside the online part's counting. */
static sense_err_t readRaw(uint8_t reg, uint16_t *v) {
	uint8_t b[2];
	sense_err_t e = sense_bus_read(&s_bus, INA219_ADDR, reg, b, 2);
	if (e == SENSE_OK) *v = sense_be16(b);
	return e;
}

/**
 * @brief Whether register 00h's value is one an INA219 holds on this bench:
 *        its power-on value, or the bench's own configuration, which an
 *        INA219 keeps across a reset of the board alone.
 *
 * The identity rule: nothing but the register pointer, 00h, is written to
 * a part at @ref INA219_ADDR before register 00h reads one of these two.
 * Another chip at the address, a PCA9685 whose register 00h is MODE1 among
 * them, gets only that pointer.
 */
bool ina219Identity(uint16_t config) {
	return config == INA219_CONFIG_POR || config == INA219_CONFIG_BENCH;
}

/**
 * @brief Register 00h read as the identity, then RST, the power-on value
 *        read again, the bench's configuration written and read back.
 * @param nowMs millis() now.
 * @return The state the probe reached.
 */
static sense_state_t probe(uint32_t nowMs) {
	uint16_t v = 0;
	if (readRaw(INA219_REG_CONFIG, &v) != SENSE_OK) return SENSE_PART_ABSENT;
	// FFFFh is the bus that no part drives.
	if (v == 0xFFFFu) return SENSE_PART_ABSENT;
	// The INA219's configuration after a power-on reset is 399Fh (SBOS448G,
	// "Register Maps", to confirm); an INA226 or an INA3221 reads 4127h or
	// 7127h after its reset. Anything else is another part: read, and
	// nothing written to it but the register pointer.
	s_part.id_device = v;
	if (!ina219Identity(v)) return SENSE_PART_WRONG_ID;
	if (sense_bus_write16(&s_bus, INA219_ADDR, INA219_REG_CONFIG, INA219_CONFIG_RST) != SENSE_OK)
		return SENSE_PART_ABSENT;
	// The time the part needs after RST before it answers is not stated
	// (SBOS448G, to confirm); a read that fails here retries at the next
	// probe, 1000 ms on.
	if (readRaw(INA219_REG_CONFIG, &v) != SENSE_OK) return SENSE_PART_ABSENT;
	if (v != INA219_CONFIG_POR) return SENSE_PART_REFUSED;
	if (sense_bus_write16(&s_bus, INA219_ADDR, INA219_REG_CONFIG, INA219_CONFIG_BENCH) != SENSE_OK)
		return SENSE_PART_ABSENT;
	if (readRaw(INA219_REG_CONFIG, &v) != SENSE_OK) return SENSE_PART_ABSENT;
	if (v != INA219_CONFIG_BENCH) return SENSE_PART_REFUSED;
	s_readbackAt = nowMs + SENSE_READBACK_MS;
	s_misses = 0;
	return SENSE_PART_ONLINE;
}

/** @brief A register of the online part, counted towards taking it offline. */
static sense_err_t readPart(uint8_t reg, uint16_t *v) {
	uint8_t b[2];
	sense_err_t e = sense_part_read(&s_part, reg, b, 2);
	if (e == SENSE_OK) *v = sense_be16(b);
	return e;
}

/**
 * @brief The online part's frame: the read-back when due, the shunt and the
 *        bus registers, and the reading published.
 */
static void readPartFrame(uint32_t nowMs) {
	uint16_t v = 0;
	if ((int32_t)(nowMs - s_readbackAt) >= 0) {
		s_readbackAt = nowMs + SENSE_READBACK_MS;
		// A failed read is a failed transaction, not a reset.
		if (readPart(INA219_REG_CONFIG, &v) != SENSE_OK) return;
		if (v == INA219_CONFIG_BENCH) {
			s_misses = 0;
		} else if (++s_misses >= SENSE_READBACK_MISSES) {
			// 399Fh after a reset of its own, FFFFh with no part driving the
			// bus, or any other value: the part no longer holds 19EFh.
			s_misses = 0;
			s_resetSeen = true;
			sense_part_lost(&s_part);
			return;
		}
	}
	uint8_t before = s_part.fails;
	uint16_t shunt = 0, busRaw = 0;
	if (readPart(INA219_REG_SHUNT, &shunt) != SENSE_OK) return;
	if (readPart(INA219_REG_BUS, &busRaw) != SENSE_OK) {
		sense_part_sample_failed(&s_part, before);
		return;
	}
	sense_value_t i = ina219CurrentUa(shunt, INA219_SHUNT_MOHM);
	int32_t endUa = senseEndUa();
	s_lastUa    = i.clip == SENSE_CLIP_HIGH ? endUa : i.clip == SENSE_CLIP_LOW ? -endUa : i.value;
	s_lastMv    = ina219BusMv(busRaw);
	s_lastFlags = (uint8_t)((i.clip != SENSE_CLIP_NONE ? SENSE_FLAG_CLIPPED : 0u) |
	                        (s_resetSeen ? SENSE_FLAG_RESET : 0u));
	s_resetSeen = false;
	s_lastMs    = nowMs;
	if (++s_seq == 0) s_seq = 1;
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

void senseConfigure(uint8_t mode) {
	if (mode > SENSE_MODE_MAX) mode = (uint8_t)SenseMode::None;
	// The wire, once, for a caller ahead of senseInit(): the host suite.
	if (!s_bus.io.read) sense_bus_init(&s_bus, &WIRE);
	if (mode == s_mode) return;
	s_mode = mode;
	sense_part_init(&s_part, &s_bus, INA219_ADDR);
	if (mode != (uint8_t)SenseMode::Ina219) s_part.state = SENSE_PART_UNSET;
	s_misses = 0;
	s_resetSeen = false;
	lockedPublish();
}

void senseInit() {
	if (!s_csReady) {
		critical_section_init(&s_cs);
		s_csReady = true;
	}
	sense_bus_init(&s_bus, &WIRE);
	s_mode = 0xFFu;
	senseConfigure(settings()->currentSense);
	senseTick(millis());
}

void senseTick(uint32_t nowMs) {
	if (s_mode != (uint8_t)SenseMode::Ina219) return;
	if (sense_bus_recovery_due(&s_bus, nowMs)) {
		recoverBus();
		sense_bus_recovered(&s_bus, nowMs);
	}
	// Every tick: sense_bus.h stamps the part's time here.
	if (sense_part_probe_due(&s_part, nowMs)) {
		// A stuck bus sends nothing; the probe stays due until it is free.
		if (!s_bus.stuck || s_bus.trial) sense_part_probed(&s_part, probe(nowMs));
	} else if (s_part.state == SENSE_PART_ONLINE) {
		readPartFrame(nowMs);
	}
	lockedPublish();
}

void senseNoteTouch(int result, size_t want) {
	sense_bus_note(&s_bus, errAfter(result, want));
}

bool senseDeclared() { return s_mode == (uint8_t)SenseMode::Ina219; }

const char *senseCaption() {
	if (!senseDeclared()) return nullptr;
	if (s_part.state == SENSE_PART_ONLINE && !s_bus.stuck) return "FOUND";
	if (s_part.state == SENSE_PART_WRONG_ID) return "OTHER PART";
	// Not probed yet, the frame CURRENT SENSE is set: nothing to say.
	if (s_part.state == SENSE_PART_UNPROBED) return "";
	return "NOT FOUND";
}

void senseAppendCurrent(char *buf, size_t n) {
	if (!senseDeclared()) return;
	SenseSample s;
	senseSample(&s);
	char t[SENSE_TEXT_MAX + 1];
	senseCurrentText(&s, millis(), t, sizeof(t));
	size_t used = strlen(buf);
	if (used + 1 < n) {
		buf[used] = ' ';
		put(buf + used + 1, n - used - 1, t);
	}
}

const sense_bus_t *senseBus() { return &s_bus; }
const sense_part_t *sensePart() { return &s_part; }
