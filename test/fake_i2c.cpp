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

enum class Kind : uint8_t { None, Ina219, Cst328, Qmi8658, Pcf85063 };

struct Part {
	Kind         kind;
	uint8_t      addr;
	FakeIna219   ina;
	FakeQmi8658  qmi;
	FakePcf85063 rtc;
	uint16_t     ptr16;   /**< CST328: the 16-bit register pointer. */
};

#define PARTS 4
Part         g_parts[PARTS];
FakeI2cStats g_stats;
bool         g_open = false;        /**< A transfer with nostop left the transaction open. */
int          g_failN = 0;
int          g_failSkip = 0;
int          g_failCode = 0;
bool         g_failReach = false;   /**< A failed transfer reaches the part before it fails. */
bool         g_holdSda = false, g_holdScl = false;
int          g_holdSdaAfter = 0;    /**< Transfers left before SDA is held; 0 for none. */
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

/** @brief The shunt and bus conversions of @p p from a current of @p amps and its output now. */
void convert(FakeIna219 *p, double amps) {
	unsigned pg = (p->reg[0] >> 11) & 3u;
	long limit = 4000L << pg;
	long code = lround(amps * p->shuntOhm / 10e-6);
	if (code > limit) code = limit;
	if (code < -limit) code = -limit;
	p->shuntCode = (int16_t)code;
	long bus = lround((p->voutV - amps * p->shuntOhm) / 0.004);
	long busMax = (p->reg[0] & 0x2000u) ? 8000 : 4000;
	if (bus < 0) bus = 0;
	if (bus > busMax) bus = busMax;
	p->busCode = (uint16_t)bus;
}

/** @brief Bring @p p's conversions up to now. */
void catchUp(FakeIna219 *p) {
	if (!p->timed) {
		convert(p, p->currentA);
		return;
	}
	uint64_t now = time_us_64();
	uint64_t cycle = fakeIna219CycleUs(p);
	if (now - p->cycleUs >= cycle) {
		p->cycleUs += (now - p->cycleUs) / cycle * cycle;
		double amps = p->currentA;
		if (p->currentAt) {
			// The newest completed cycle's shunt conversion: its first part.
			uint64_t from = p->cycleUs - cycle;
			uint32_t shunt = adcUs((p->reg[0] >> 3) & 15u);
			double sum = 0;
			unsigned n = 0;
			for (uint64_t t = from; t < from + shunt; t += 10, n++) sum += p->currentAt(t, p->currentCtx);
			amps = sum / n;
		}
		convert(p, amps);
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

// --- the QMI8658 ----------------------------------------------------------

/** @brief A sample period at 31.25 Hz, µs. */
#define QMI_PERIOD_US 32000u

/** @brief The test's values into the data registers, in CTRL1's byte order, latched at @p us. */
void qmiLatch(FakeQmi8658 *q, uint64_t us) {
	q->latchUs = us;
	q->stamp = (q->stamp + 1u) & 0xFFFFFFu;
	q->reg[0x30] = (uint8_t)q->stamp;
	q->reg[0x31] = (uint8_t)(q->stamp >> 8);
	q->reg[0x32] = (uint8_t)(q->stamp >> 16);
	bool be = (q->reg[0x02] & 0x20u) != 0;
	int16_t v[4] = { q->temp, q->acc[0], q->acc[1], q->acc[2] };
	for (int i = 0; i < 4; i++) {
		uint8_t lo = (uint8_t)(uint16_t)v[i], hi = (uint8_t)((uint16_t)v[i] >> 8);
		q->reg[0x33 + 2 * i] = be ? hi : lo;
		q->reg[0x34 + 2 * i] = be ? lo : hi;
	}
}

/** @brief The samples due by now latched, the last one's values in the registers. */
void qmiCatchUp(FakeQmi8658 *q) {
	if (!(q->reg[0x08] & 0x01u)) return;
	uint64_t due = (time_us_64() - q->enUs) / QMI_PERIOD_US;
	while (q->timed < due) {
		q->timed++;
		qmiLatch(q, q->enUs + (uint64_t)q->timed * QMI_PERIOD_US);
	}
}

/** @brief Every register to its power-on value. */
void qmiDefaults(FakeQmi8658 *q) {
	memset(q->reg, 0, sizeof(q->reg));
	q->reg[0x00] = q->whoAmI;
	q->reg[0x01] = q->revision;
	q->reg[0x02] = 0x20;   // big-endian, auto-increment off
	q->ptr = 0;
	q->timed = 0;
}

/** @brief Whether @p q is booting after a reset and answers nothing. */
bool qmiBooting(const FakeQmi8658 *q) {
	return q->everReset && time_us_64() - q->resetUs < (uint64_t)q->bootMs * 1000u;
}

void qmiLog(FakeQmi8658 *q, bool write, uint8_t reg, size_t len, uint8_t val) {
	if (q->logN >= FAKE_QMI_LOG) return;
	q->log[q->logN++] = { write, reg, (uint8_t)len, val, (uint32_t)(time_us_64() / 1000u) };
}

void qmiWrite(FakeQmi8658 *q, const uint8_t *src, size_t len) {
	if (len < 1) return;
	q->ptr = src[0] & 0x7Fu;
	if (len < 2) return;
	qmiLog(q, true, q->ptr, len - 1, src[1]);
	// The QMI8658A takes no burst write to its configuration registers: the
	// first byte lands, the rest are dropped.
	if (len > 2) q->burstWrites++;
	uint8_t r = q->ptr, v = src[1];
	if (r == 0x60) {
		if (v != 0xB0) return;
		q->resets++;
		q->everReset = true;
		q->resetUs = time_us_64();
		qmiDefaults(q);
		q->reg[0x4D] = q->dqyAfterReset;
		return;
	}
	if (r == 0x00 || r == 0x01 || (r >= 0x30 && r <= 0x3A)) return;   // read-only
	if (q->refuseReg != 0 && r == q->refuseReg) return;
	if (r == 0x08) {
		bool was = q->reg[0x08] & 0x01u;
		if ((v & 0x01u) && !was) {
			q->enUs = time_us_64();
			q->timed = 0;
		}
		// Turning a sensor on overwrites dQY_L (A datasheet, 7.4).
		if (v & 0x03u) q->reg[0x4D] = 0;
	}
	q->reg[r] = v;
}

void qmiRead(FakeQmi8658 *q, uint8_t *dst, size_t len) {
	qmiCatchUp(q);
	qmiLog(q, false, q->ptr, len, 0);
	bool ai = (q->reg[0x02] & 0x40u) != 0;
	for (size_t i = 0; i < len; i++) {
		if (i == 3 && q->tearNext) {
			q->tearNext = false;
			qmiLatch(q, time_us_64());
		}
		uint8_t r = (uint8_t)((ai ? q->ptr + i : q->ptr) & 0x7Fu);
		dst[i] = q->reg[r];
	}
}

// --- the PCF85063A -----------------------------------------------------------

/** @brief Two BCD digits as a number; -1 for a digit above 9. */
int rtcBcd(uint8_t b) {
	return (b >> 4) > 9 || (b & 15) > 9 ? -1 : (b >> 4) * 10 + (b & 15);
}

uint8_t rtcToBcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

int rtcDays(int year, int month) {
	static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	return month == 2 && year % 4 == 0 ? 29 : days[month - 1];
}

/**
 * @brief One second on @p p's time registers, with its carries; false, and
 *        nothing changed, when they hold no valid date.
 */
bool rtcSecond(FakePcf85063 *p) {
	uint8_t *r = p->reg;
	int s = rtcBcd(r[4] & 0x7F), mi = rtcBcd(r[5] & 0x7F), h = rtcBcd(r[6] & 0x3F);
	int d = rtcBcd(r[7] & 0x3F), mo = rtcBcd(r[9] & 0x1F), y = rtcBcd(r[10]);
	if (s < 0 || s > 59 || mi < 0 || mi > 59 || h < 0 || h > 23 || mo < 1 || mo > 12 || y < 0 ||
	    d < 1 || d > rtcDays(y, mo))
		return false;
	if (++s == 60) {
		s = 0;
		if (++mi == 60) {
			mi = 0;
			if (++h == 24) {
				h = 0;
				if (++d > rtcDays(y, mo)) {
					d = 1;
					if (++mo == 13) {
						mo = 1;
						y = (y + 1) % 100;
					}
				}
			}
		}
	}
	r[4] = (uint8_t)((r[4] & 0x80) | rtcToBcd(s));
	r[5] = rtcToBcd(mi);
	r[6] = rtcToBcd(h);
	r[7] = rtcToBcd(d);
	r[9] = rtcToBcd(mo);
	r[10] = rtcToBcd(y);
	return true;
}

/** @brief Bring @p p's time up to now: a second for each whole second since countUs, unless STOP is set. */
void rtcCatchUp(FakePcf85063 *p) {
	uint64_t now = time_us_64();
	if (p->reg[0] & 0x20) {
		p->countUs = now;
		return;
	}
	while (now - p->countUs >= 1000000u) {
		p->countUs += 1000000u;
		if (!rtcSecond(p)) {
			p->countUs = now;
			return;
		}
	}
}

void rtcWrite(FakePcf85063 *p, const uint8_t *src, size_t len) {
	if (len < 1) return;
	if (p->logN < FAKE_RTC_LOG) {
		FakeRtcWrite *w = &p->log[p->logN++];
		w->len = (uint8_t)len;
		memcpy(w->b, src, len < sizeof(w->b) ? len : sizeof(w->b));
	}
	rtcCatchUp(p);
	p->ptr = src[0] < 0x12 ? src[0] : 0;
	if (len < 2) return;
	p->writes++;
	for (size_t i = 1; i < len; i++) {
		uint8_t r = p->ptr, v = src[i];
		p->ptr = (uint8_t)((p->ptr + 1) % 0x12);
		if (p->refuse && r == p->refuseReg) continue;
		if (r == 0 && v == 0x58) {
			fakePcf85063Reset(p);
			p->resets++;
			return;
		}
		if (r == 0 && (p->reg[0] & 0x20) && !(v & 0x20)) {
			// STOP released: the first second 0.507813 s on.
			p->countUs = time_us_64() + 507813u - 1000000u;
		}
		p->reg[r] = v;
	}
}

void rtcRead(FakePcf85063 *p, uint8_t *dst, size_t len) {
	rtcCatchUp(p);
	p->reads++;
	for (size_t i = 0; i < len; i++) {
		dst[i] = p->reg[p->ptr];
		p->ptr = (uint8_t)((p->ptr + 1) % 0x12);
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

/** @brief A transfer's bytes to or from part @p p. */
void deliver(Part *p, uint8_t *dst, const uint8_t *src, size_t len) {
	if (p->kind == Kind::Ina219) {
		if (dst) inaRead(&p->ina, dst, len); else inaWrite(&p->ina, src, len);
	} else if (p->kind == Kind::Qmi8658) {
		if (dst) qmiRead(&p->qmi, dst, len); else qmiWrite(&p->qmi, src, len);
	} else if (p->kind == Kind::Pcf85063) {
		if (dst) rtcRead(&p->rtc, dst, len); else rtcWrite(&p->rtc, src, len);
	} else {
		if (dst) cstRead(p, dst, len); else cstWrite(p, src, len);
	}
}

/** @brief A transfer: the fault, the held line or the NACK first, then the part. */
int transferOnce(uint8_t addr, uint8_t *dst, const uint8_t *src, size_t len, bool nostop,
             unsigned timeoutUs) {
	g_stats.transfers++;
	g_stopReads = 0;
	if (g_failN > 0 && g_failSkip > 0) {
		g_failSkip--;
	} else if (g_failN > 0) {
		g_failN--;
		if (g_failReach) {
			Part *p = find(addr);
			if (p && !(p->kind == Kind::Qmi8658 && qmiBooting(&p->qmi))) deliver(p, dst, src, len);
		}
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
	if (p && p->kind == Kind::Qmi8658 && qmiBooting(&p->qmi)) p = nullptr;
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
	deliver(p, dst, src, len);
	return (int)len;
}

/** @brief A transfer, then SDA held when fakeI2cHoldSdaAfter()'s count runs out. */
int transfer(uint8_t addr, uint8_t *dst, const uint8_t *src, size_t len, bool nostop,
             unsigned timeoutUs) {
	int rc = transferOnce(addr, dst, src, len, nostop, timeoutUs);
	if (g_holdSdaAfter > 0 && --g_holdSdaAfter == 0) g_holdSda = true;
	return rc;
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
	g_failReach = false;
	g_holdSda = g_holdScl = false;
	g_holdSdaAfter = 0;
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

FakeQmi8658 *fakeI2cAddQmi8658(uint8_t addr) {
	Part *p = slot();
	if (!p) return nullptr;
	memset(p, 0, sizeof(*p));
	p->kind = Kind::Qmi8658;
	p->addr = addr;
	FakeQmi8658 *q = &p->qmi;
	q->addr = addr;
	q->whoAmI = 0x05;
	q->revision = 0x7C;
	q->dqyAfterReset = 0x80;
	q->bootMs = 15;
	q->acc[2] = 16384;
	qmiDefaults(q);
	return q;
}

uint32_t fakeQmiCount(const FakeQmi8658 *q, bool write, uint8_t reg, uint32_t from) {
	uint32_t n = 0;
	for (uint32_t i = from; i < q->logN; i++)
		if (q->log[i].write == write && q->log[i].reg == reg) n++;
	return n;
}

FakePcf85063 *fakeI2cAddPcf85063(uint8_t addr) {
	Part *p = slot();
	if (!p) return nullptr;
	memset(p, 0, sizeof(*p));
	p->kind = Kind::Pcf85063;
	p->addr = addr;
	p->rtc.addr = addr;
	fakePcf85063Reset(&p->rtc);
	return &p->rtc;
}

void fakePcf85063Reset(FakePcf85063 *p) {
	memset(p->reg, 0, sizeof(p->reg));
	p->reg[0x04] = 0x80;   // OS
	p->reg[0x07] = 0x01;   // day 1
	p->reg[0x08] = 0x06;   // Saturday, 1 January 2000; not read by the firmware
	p->reg[0x09] = 0x01;   // January
	p->ptr = 0;
	p->countUs = time_us_64();
}

void fakePcf85063Set(FakePcf85063 *p, int year, int month, int day, int hour, int minute,
                     int second) {
	p->reg[0x00] = 0x00;
	p->reg[0x04] = rtcToBcd(second);
	p->reg[0x05] = rtcToBcd(minute);
	p->reg[0x06] = rtcToBcd(hour);
	p->reg[0x07] = rtcToBcd(day);
	p->reg[0x09] = rtcToBcd(month);
	p->reg[0x0A] = rtcToBcd(year - 2000);
	p->countUs = time_us_64();
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

void fakeI2cFailNext(int n, int code, int skip, bool reach) {
	g_failN = n;
	g_failSkip = skip;
	g_failCode = code;
	g_failReach = reach;
}

void fakeI2cHoldSda(bool low) { g_holdSda = low; }
void fakeI2cHoldSdaAfter(int n) { g_holdSdaAfter = n; }
void fakeI2cHoldScl(bool low) { g_holdScl = low; }
void fakeI2cHeldSdaCode(int code) { g_heldCode = code; }
void fakeI2cShortNext(int n) { g_shortN = n; }

void fakeI2cSdaReleaseAfter(int clocks) {
	g_sdaReleaseAfter = clocks;
	g_holdSda = true;
}

FakeI2cStats fakeI2cStats() { return g_stats; }

uint32_t fakeI2cTransactionsTo(uint8_t addr) { return g_toAddr[addr & 127]; }
