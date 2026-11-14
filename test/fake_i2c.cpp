/**
 * @file fake_i2c.cpp
 * @brief The host suite's I2C1 bus and the parts on it. @see fake_i2c.h
 */

#include "fake_i2c.h"
#include "fakes.h"
#include "board_desc.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"
#include "pico/stdlib.h"

#include <math.h>
#include <string.h>

i2c_inst_t *i2c0 = nullptr;
i2c_inst_t *i2c1 = nullptr;

namespace {

enum class Kind : uint8_t { None, Ina219, Cst328 };

struct Part {
	Kind       kind;
	uint8_t    addr;
	FakeIna219 ina;
	uint16_t   ptr16;   /**< CST328: the 16-bit register pointer. */
};

#define PARTS 4
Part         g_parts[PARTS];
FakeI2cStats g_stats;
bool         g_open = false;        /**< A transfer with nostop left the transaction open. */
int          g_failN = 0;
int          g_failSkip = 0;
int          g_failCode = 0;
bool         g_holdSda = false, g_holdScl = false;
int          g_heldCode = PICO_ERROR_TIMEOUT;  /**< What a transfer returns while SDA alone is held. */
int          g_shortN = 0;          /**< Transfers left that return one byte short. */
int          g_stopReads = 0;       /**< SDA reads left in the block's STOP. */
bool         g_sdaStart = false;    /**< A recovery pulled SDA low while SCL was high. */
int          g_sdaReleaseAfter = 0;
bool         g_finger = false;
int          g_fx = 0, g_fy = 0;
uint32_t     g_cstReads = 0;
bool         g_cstWrongId = false;  /**< The CST328 answers its ID read with 0000h. */
uint32_t     g_toAddr[128];         /**< Transactions to each address, a part on it or not. */

Part *find(uint8_t addr) {
	for (Part &p : g_parts)
		if (p.kind != Kind::None && p.addr == addr) return &p;
	return nullptr;
}

Part *slot() {
	for (Part &p : g_parts)
		if (p.kind == Kind::None) return &p;
	return nullptr;
}

/** @brief One transfer's clocks: the address and @p bytes, a start and stop or a repeated start. */
void countClocks(size_t bytes, bool nostop) {
	g_stats.clocks += (uint32_t)(9u * (1u + bytes) + (g_open ? 1u : 2u));
	g_open = nostop;
}

/** @brief A pin a recovery drives low: SIO, an output, at 0. */
bool drivenLow(uint32_t gpio) {
	FakePin p = fakePin(gpio);
	return p.function == GPIO_FUNC_SIO && p.output && !p.high;
}

bool sdaLow() { return g_holdSda || drivenLow(g_board->sda); }
bool sclLow() { return g_holdScl || drivenLow(g_board->scl); }

/** @brief ADC conversion time of a BADC or SADC setting, µs (SBOS448G, "ADC Settings"). */
uint32_t adcUs(unsigned v) {
	static const uint32_t avg[8] = { 532, 1060, 2130, 4260, 8510, 17020, 34050, 68100 };
	static const uint32_t bits[4] = { 84, 148, 276, 532 };
	return (v & 8u) ? avg[v & 7u] : bits[v & 3u];
}

/** @brief The shunt and bus conversions of @p p from its inputs now. */
void convert(FakeIna219 *p) {
	unsigned pg = (p->reg[0] >> 11) & 3u;
	long limit = 4000L << pg;
	long code = lround(p->currentA * p->shuntOhm / 10e-6);
	if (code > limit) code = limit;
	if (code < -limit) code = -limit;
	p->shuntCode = (int16_t)code;
	long bus = lround((p->voutV - p->currentA * p->shuntOhm) / 0.004);
	long busMax = (p->reg[0] & 0x2000u) ? 8000 : 4000;
	if (bus < 0) bus = 0;
	if (bus > busMax) bus = busMax;
	p->busCode = (uint16_t)bus;
}

/** @brief Bring @p p's conversions up to now. */
void catchUp(FakeIna219 *p) {
	if (!p->timed) {
		convert(p);
		return;
	}
	uint64_t now = time_us_64();
	uint64_t cycle = fakeIna219CycleUs(p);
	if (now - p->cycleUs >= cycle) {
		convert(p);
		p->cycleUs += (now - p->cycleUs) / cycle * cycle;
	}
}

void inaWrite(FakeIna219 *p, const uint8_t *src, size_t len) {
	if (len < 1) return;
	p->ptr = src[0] < 6 ? src[0] : 0;
	if (len < 3) return;
	uint16_t v = (uint16_t)((src[1] << 8) | src[2]);
	if (p->ptr == 0) {
		p->configWrites++;
		if (v & 0x8000u) { fakeIna219Por(p); return; }
		if (p->deaf) return;
		p->reg[0] = v;
		p->cycleUs = time_us_64();
	} else if (p->ptr == 5) {
		p->reg[5] = v;
	}
}

void inaRead(FakeIna219 *p, uint8_t *dst, size_t len) {
	uint16_t v = p->reg[p->ptr];
	if (p->ptr == 1 || p->ptr == 2) catchUp(p);
	if (p->ptr == 1) v = (uint16_t)p->shuntCode;
	if (p->ptr == 2) v = (uint16_t)((p->busCode << 3) | 0x2u);
	for (size_t i = 0; i < len; i++) dst[i] = i == 0 ? (uint8_t)(v >> 8) : i == 1 ? (uint8_t)v : 0;
}

void cstWrite(Part *p, const uint8_t *src, size_t len) {
	if (len >= 2) p->ptr16 = (uint16_t)((src[0] << 8) | src[1]);
}

void cstRead(Part *p, uint8_t *dst, size_t len) {
	memset(dst, 0, len);
	if (p->ptr16 == 0xD1FC && len >= 4) {
		dst[2] = g_cstWrongId ? 0x00 : 0xCA;
		dst[3] = g_cstWrongId ? 0x00 : 0xCA;
	} else if (p->ptr16 == 0xD000 && len >= 5) {
		g_cstReads++;
		if (g_finger) {
			dst[0] = 0x06;
			dst[1] = (uint8_t)(g_fx >> 4);
			dst[2] = (uint8_t)(g_fy >> 4);
			dst[3] = (uint8_t)(((g_fx & 15) << 4) | (g_fy & 15));
		}
	}
}

/** @brief A transfer that waits out its whole timeout: counted, and the clock moved by it. */
int timedOut(unsigned timeoutUs) {
	g_open = false;
	g_stats.timeouts++;
	g_stats.timeoutUs += timeoutUs;
	sleep_us(timeoutUs);
	return PICO_ERROR_TIMEOUT;
}

/** @brief A transfer: the fault, the held line or the NACK first, then the part. */
int transfer(uint8_t addr, uint8_t *dst, const uint8_t *src, size_t len, bool nostop,
             unsigned timeoutUs) {
	g_stats.transfers++;
	g_stopReads = 0;
	if (g_failN > 0 && g_failSkip > 0) {
		g_failSkip--;
	} else if (g_failN > 0) {
		g_failN--;
		if (g_failCode == PICO_ERROR_TIMEOUT) return timedOut(timeoutUs);
		countClocks(0, false);
		if (dst) g_stopReads = FAKE_I2C_STOP_READS;
		return g_failCode;
	}
	if (sclLow() || (sdaLow() && g_heldCode == PICO_ERROR_TIMEOUT)) {
		// The block cannot finish a START or a clock: it waits out its time.
		return timedOut(timeoutUs);
	}
	if (sdaLow()) {
		// SCL free, SDA held: the block loses arbitration on the first 1 of
		// the address and aborts, PICO_ERROR_GENERIC.
		g_open = false;
		g_stats.clocks += 2;
		return g_heldCode;
	}
	Part *p = find(addr);
	if (!p) {
		countClocks(0, false);
		if (dst) g_stopReads = FAKE_I2C_STOP_READS;
		return PICO_ERROR_GENERIC;
	}
	if (g_shortN > 0 && len > 1) {
		// A data byte not acknowledged: the SDK returns the bytes sent.
		g_shortN--;
		countClocks(len - 1, false);
		if (!dst) {
			if (p->kind == Kind::Ina219 && len >= 1) p->ina.ptr = src[0] < 6 ? src[0] : 0;
		}
		return (int)len - 1;
	}
	countClocks(len, nostop);
	if (p->kind == Kind::Ina219) {
		if (dst) inaRead(&p->ina, dst, len); else inaWrite(&p->ina, src, len);
	} else {
		if (dst) cstRead(p, dst, len); else cstWrite(p, src, len);
	}
	return (int)len;
}

}  // namespace

// ---------------------------------------------------------------------------
// the SDK surface
// ---------------------------------------------------------------------------

void i2c_init(i2c_inst_t *, unsigned) {
	g_stats.inits++;
	g_open = false;
	g_stopReads = 0;
}
void i2c_deinit(i2c_inst_t *) {}

int i2c_write_timeout_us(i2c_inst_t *, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
                         unsigned timeoutUs) {
	if (!g_open) g_toAddr[addr & 127]++;
	return transfer(addr, nullptr, src, len, nostop, timeoutUs);
}

int i2c_read_timeout_us(i2c_inst_t *, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                        unsigned timeoutUs) {
	if (!g_open) g_toAddr[addr & 127]++;
	return transfer(addr, dst, nullptr, len, nostop, timeoutUs);
}

extern "C" bool gpio_get(uint32_t gpio) {
	if (gpio == g_board->sda) {
		if (g_stopReads > 0) {
			g_stopReads--;
			return false;
		}
		return !sdaLow();
	}
	if (gpio == g_board->scl) return !sclLow();
	return fakePin(gpio).high;
}

extern "C" void gpio_init(uint32_t gpio) {
	gpio_set_dir(gpio, GPIO_IN);
	gpio_set_function(gpio, GPIO_FUNC_SIO);
}

// ---------------------------------------------------------------------------
// controls
// ---------------------------------------------------------------------------

void fakeI2cPinDir(uint32_t gpio, bool out) {
	if (gpio == g_board->sda && fakePin(gpio).function == GPIO_FUNC_SIO && !sclLow()) {
		// SDA changing while SCL is high: falling a START, rising a STOP.
		if (out && !fakePin(gpio).high) {
			g_sdaStart = true;
			g_stats.starts++;
		} else if (!out && g_sdaStart) {
			g_sdaStart = false;
			g_stats.stops++;
		}
		return;
	}
	if (gpio != g_board->scl || !out || fakePin(gpio).function != GPIO_FUNC_SIO) return;
	g_sdaStart = false;
	if (fakePin(gpio).high) return;
	g_stats.sclPulses++;
	if (g_sdaReleaseAfter > 0 && --g_sdaReleaseAfter == 0) g_holdSda = false;
}

void fakeI2cReset() {
	memset(g_parts, 0, sizeof(g_parts));
	memset(&g_stats, 0, sizeof(g_stats));
	memset(g_toAddr, 0, sizeof(g_toAddr));
	g_open = false;
	g_failN = 0;
	g_failSkip = 0;
	g_holdSda = g_holdScl = false;
	g_heldCode = PICO_ERROR_TIMEOUT;
	g_shortN = 0;
	g_stopReads = 0;
	g_sdaStart = false;
	g_sdaReleaseAfter = 0;
	g_finger = false;
	g_cstReads = 0;
	g_cstWrongId = false;
}

FakeIna219 *fakeI2cAddIna219(uint8_t addr, uint16_t porConfig) {
	Part *p = slot();
	if (!p) return nullptr;
	memset(p, 0, sizeof(*p));
	p->kind = Kind::Ina219;
	p->addr = addr;
	p->ina.addr = addr;
	p->ina.porConfig = porConfig;
	p->ina.shuntOhm = 0.1;
	p->ina.voutV = 5.0;
	fakeIna219Por(&p->ina);
	return &p->ina;
}

void fakeIna219Por(FakeIna219 *p) {
	memset(p->reg, 0, sizeof(p->reg));
	p->reg[0] = p->porConfig;
	p->ptr = 0;
	p->shuntCode = 0;
	p->busCode = 0;
	p->cycleUs = time_us_64();
}

uint32_t fakeIna219CycleUs(const FakeIna219 *p) {
	unsigned badc = (p->reg[0] >> 7) & 15u, sadc = (p->reg[0] >> 3) & 15u;
	return adcUs(badc) + adcUs(sadc);
}

void fakeI2cAddCst328(uint8_t addr) {
	Part *p = slot();
	if (!p) return;
	memset(p, 0, sizeof(*p));
	p->kind = Kind::Cst328;
	p->addr = addr;
}

void fakeCst328Finger(bool down, int x, int y) {
	g_finger = down;
	g_fx = x;
	g_fy = y;
}

uint32_t fakeCst328Reads() { return g_cstReads; }

void fakeCst328WrongId(bool on) { g_cstWrongId = on; }

void fakeI2cRemove(uint8_t addr) {
	Part *p = find(addr);
	if (p) p->kind = Kind::None;
}

void fakeI2cFailNext(int n, int code, int skip) {
	g_failN = n;
	g_failSkip = skip;
	g_failCode = code;
}

void fakeI2cHoldSda(bool low) { g_holdSda = low; }
void fakeI2cHoldScl(bool low) { g_holdScl = low; }
void fakeI2cHeldSdaCode(int code) { g_heldCode = code; }
void fakeI2cShortNext(int n) { g_shortN = n; }

void fakeI2cSdaReleaseAfter(int clocks) {
	g_sdaReleaseAfter = clocks;
	g_holdSda = true;
}

FakeI2cStats fakeI2cStats() { return g_stats; }

uint32_t fakeI2cTransactionsTo(uint8_t addr) { return g_toAddr[addr & 127]; }
