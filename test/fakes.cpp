/**
 * @file fakes.cpp
 * @brief Fake platform, fake ESC and scripted touch for the host tests.
 *
 * Stands in for everything the firmware talks to that is not present on a PC:
 * the Arduino/Pico SDK surface, the display, the touch panel, the DShot task,
 * and an AM32 ESC that serves a real settings blob and accepts writes.
 *
 * Time is virtual. millis() advances only when the test says so, which makes
 * hold-to-arm, hold-to-write and gesture repeat deterministic rather than
 * dependent on how fast the machine runs.
 */

#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "hardware/structs/powman.h"
#include "hardware/structs/watchdog.h"
#include "pico/bootrom.h"
#include "gfx.h"
#include "theme.h"
#include "st7789.h"
#include "touch.h"
#include "esc_task.h"
#include "core1_job.h"
#include "settings.h"
#include "config.h"
#include "am32_bl.h"
#include "am32_eeprom.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "ui.h"
#include "fakes.h"
#include "fake_i2c.h"
#include "card_cache.h"
#include "plat.h"

#include <stdio.h>
#include <string.h>

#include <deque>

// ---------------------------------------------------------------------------
// virtual clock and the Pico SDK surface the firmware uses
// ---------------------------------------------------------------------------

// Time is virtual: it advances only when a test says so. plat.h's millis() and
// micros() are inline wrappers over these, so faking the SDK timebase fakes
// both without the firmware knowing. The count is in microseconds, so bytes
// 120 us apart can be stamped; millis() is it divided by 1000.
static uint64_t g_us = 0;

void fakeAdvance(uint32_t ms) { g_us += (uint64_t)ms * 1000u; }
void fakeAdvanceUs(uint32_t us) { g_us += us; }
uint64_t time_us_64() { return g_us; }
uint32_t time_us_32() { return (uint32_t)g_us; }
void sleep_ms(uint32_t d) { g_us += (uint64_t)d * 1000u; }
// Whole milliseconds only: the sensor's recovery waits of a few
// microseconds move no time, so the tests' millisecond steps stay exact.
void sleep_us(uint64_t us) { g_us += us / 1000u * 1000u; }

// --- ADC: a steady ~3.92 V pack through the board's 200k/100k divider ---
void adc_init() {}
void adc_gpio_init(unsigned) {}
void adc_select_input(unsigned) {}
/** @brief The fake ADC's reading: the board's battery at about 3.92 V. */
static uint16_t s_adcRaw = (uint16_t)(3.92f / 3.0f / 3.3f * 4095.0f);
uint16_t adc_read() { return s_adcRaw; }
void fakeAdcSet(uint16_t raw) { s_adcRaw = raw; }
uint16_t fakeAdcDefault() { return (uint16_t)(3.92f / 3.0f / 3.3f * 4095.0f); }

// --- PWM and GPIO: a record of the registers the servo PWM driver sets.
//     The slice and channel follow the RP2350's fold for GP0 to GP29; a test
//     can break the slice to make the driver's agreement check refuse.
//     Nothing here models a counter or an edge: a level is what the compare
//     register was last set to ---
static FakePin   g_pins[FAKE_GPIOS];
/** @brief Counts every pin write the order stamps of FakePin record; never reset. */
static uint32_t  g_pinSeq = 0;
static FakeSlice g_slices[FAKE_SLICES];
static bool      g_sliceMapBroken = false;

/** @brief One compare write: when, and the level. */
struct FakeLevelWrite { uint64_t us; uint16_t level; };
/** @brief Each pin's recent compare writes, oldest first. */
static FakeLevelWrite g_writes[FAKE_GPIOS][32];
static int            g_writeCount[FAKE_GPIOS];

static unsigned sliceOf(unsigned gpio) { return (gpio >> 1) & 7u; }

/** @brief Record a compare write, keeping the newest 32. */
static void logWrite(unsigned gpio, uint64_t us, uint16_t level) {
	int &n = g_writeCount[gpio];
	if (n == 32) {
		memmove(&g_writes[gpio][0], &g_writes[gpio][1], 31 * sizeof(FakeLevelWrite));
		n = 31;
	}
	g_writes[gpio][n++] = FakeLevelWrite{us, level};
}

/** @brief The level latched for the frame that started at @p frameUs. */
static uint16_t latchedFor(unsigned gpio, uint64_t frameUs) {
	uint16_t level = 0;
	for (int i = 0; i < g_writeCount[gpio]; i++)
		if (g_writes[gpio][i].us < frameUs) level = g_writes[gpio][i].level;
	return level;
}

uint16_t fakePwmLatched(unsigned gpio) {
	if (gpio >= FAKE_GPIOS) return 0;
	const FakeSlice &s = g_slices[sliceOf(gpio)];
	if (!s.enabled) return 0;
	uint64_t now = time_us_64();
	uint64_t period = (uint64_t)s.top + 1u;
	uint64_t frame = now - (now - s.startUs) % period;
	uint16_t level = latchedFor(gpio, frame);
	return level > s.top ? s.top : level;
}

/**
 * @brief The pin's output stops following its slice: a stop, a new
 *        pwm_init() or a mux change. A pulse under way is cut short; at a
 *        frame's first count it has begun and is cut too.
 */
static void cutCheck(unsigned gpio) {
	FakePin &p = g_pins[gpio];
	const FakeSlice &s = g_slices[sliceOf(gpio)];
	if (p.function != GPIO_FUNC_PWM || !s.enabled) return;
	uint64_t now = time_us_64();
	uint64_t into = (now - s.startUs) % ((uint64_t)s.top + 1u);
	if (into < fakePwmLatched(gpio)) {
		p.cutPulses++;
		p.cutAtUs = (uint32_t)into;
	}
}

void fakePwmReset() {
	for (FakePin &p : g_pins) p = FakePin{};
	for (FakeSlice &s : g_slices) s = FakeSlice{};
	memset(g_writeCount, 0, sizeof(g_writeCount));
	g_sliceMapBroken = false;
}
void fakePwmBreakSliceMap(bool on) { g_sliceMapBroken = on; }
FakePin fakePin(unsigned gpio) { return gpio < FAKE_GPIOS ? g_pins[gpio] : FakePin{}; }
FakeSlice fakeSlice(unsigned slice) { return slice < FAKE_SLICES ? g_slices[slice] : FakeSlice{}; }

unsigned pwm_gpio_to_slice_num(unsigned gpio) {
	unsigned s = (gpio >> 1) & 7u;
	return g_sliceMapBroken ? (s + 1u) & 7u : s;
}
unsigned pwm_gpio_to_channel(unsigned gpio) { return gpio & 1u; }
pwm_config pwm_get_default_config() { return pwm_config{0, 16, 0xFFFF}; }
// The divider in 1/16ths, as the register holds it.
void pwm_config_set_clkdiv(pwm_config *c, float div) { c->div = (uint32_t)(div * 16.0f); }
void pwm_config_set_wrap(pwm_config *c, uint16_t wrap) { c->top = wrap; }
void pwm_init(unsigned slice, pwm_config *c, bool start) {
	if (slice >= FAKE_SLICES) return;
	for (unsigned g = 0; g < FAKE_GPIOS; g++)
		if (sliceOf(g) == slice) cutCheck(g);
	FakeSlice &s = g_slices[slice];
	s.inits++;
	s.startUs = time_us_64();
	s.top = (uint16_t)c->top;
	s.div16 = c->div;
	s.enabled = start;
	// pwm_init() resets both compare registers to 0, at once.
	for (unsigned g = 0; g < FAKE_GPIOS; g++) {
		if (sliceOf(g) != slice) continue;
		g_pins[g].level = 0;
		g_writeCount[g] = 0;
	}
}
void pwm_set_enabled(unsigned slice, bool on) {
	if (slice >= FAKE_SLICES) return;
	if (!on)
		for (unsigned g = 0; g < FAKE_GPIOS; g++)
			if (sliceOf(g) == slice) cutCheck(g);
	g_slices[slice].enabled = on;
}
void pwm_set_gpio_level(unsigned gpio, uint16_t level) {
	if (gpio >= FAKE_GPIOS) return;
	g_pins[gpio].levelAt = ++g_pinSeq;
	g_pins[gpio].level = level;
	g_pins[gpio].levelWrites++;
	logWrite(gpio, time_us_64(), level);
}
extern "C" void gpio_set_function(uint32_t gpio, gpio_function fn) {
	if (gpio >= FAKE_GPIOS) return;
	if (fn != GPIO_FUNC_PWM) cutCheck(gpio);
	g_pins[gpio].functionAt = ++g_pinSeq;
	g_pins[gpio].function = (int)fn;
	// As the SDK does: selecting a function turns the pad's input on, and
	// writes the whole control register, which zeroes the overrides.
	g_pins[gpio].inputOff = false;
	g_pins[gpio].inover = GPIO_OVERRIDE_NORMAL;
}
extern "C" void gpio_set_inover(unsigned gpio, unsigned value) {
	if (gpio >= FAKE_GPIOS) return;
	g_pins[gpio].inover = (uint8_t)value;
	g_pins[gpio].inoverWrites++;
}
extern "C" void gpio_set_dir(uint32_t gpio, int out) {
	if (gpio >= FAKE_GPIOS) return;
	if (out && !g_pins[gpio].output && g_pins[gpio].high) g_pins[gpio].highEnables++;
	g_pins[gpio].dirAt = ++g_pinSeq;
	g_pins[gpio].output = out != 0;
	fakeI2cPinDir(gpio, out != 0);
}
extern "C" void gpio_disable_pulls(uint32_t gpio) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].pull = FakePull::None;
}
extern "C" void gpio_pull_down(uint32_t gpio) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].pull = FakePull::Down;
}
extern "C" void gpio_pull_up(uint32_t gpio) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].pull = FakePull::Up;
}
extern "C" void gpio_put(uint32_t gpio, int value) {
	if (gpio >= FAKE_GPIOS) return;
	g_pins[gpio].high = value != 0;
	g_pins[gpio].puts++;
	g_pins[gpio].putAt = ++g_pinSeq;
}
extern "C" void gpio_set_input_enabled(uint32_t gpio, bool on) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].inputOff = !on;
}

/** @brief clk_sys and clk_peri: 150 MHz, as SDK 2.1.1 sets them. */
static uint32_t g_clockHz[2] = { 150000000u, 150000000u };
uint32_t clock_get_hz(enum clock_index i) { return g_clockHz[i == clk_peri]; }
void fakeClockSet(enum clock_index i, uint32_t hz) { g_clockHz[i == clk_peri] = hz; }

// --- Critical sections: one thread, so there is nothing to exclude. The
//     fake records initialisation: init stores a magic value in the section,
//     and enter or exit on a section without it is counted. test_main.cpp
//     fails the run when the count is not 0 ---
static const int CS_MAGIC = 0x43530001;
static unsigned g_csMisuse = 0;
extern "C" void critical_section_init(critical_section_t *cs) { cs->lock = CS_MAGIC; }
extern "C" void critical_section_enter_blocking(critical_section_t *cs) {
	if (cs->lock != CS_MAGIC) g_csMisuse++;
}
extern "C" void critical_section_exit(critical_section_t *cs) {
	if (cs->lock != CS_MAGIC) g_csMisuse++;
}
unsigned fakeCriticalSectionMisuse() { return g_csMisuse; }

// --- I2C: fake_i2c.cpp is the bus. The scripted touchPoll() below drives
//     the UI tests; the real driver runs on that bus in test_sense.cpp ---

// The board descriptor names an SPI instance. On the host it is never dialled
// -- the panel is faked -- but the descriptor is a real object whose fields
// have to resolve at link time.
spi_inst_t *spi0 = nullptr;
spi_inst_t *spi1 = nullptr;

// --- UART0: a PL011 receiver as the S.BUS job sees it. The line is a queue
//     of UARTDR words as an 8E2 receiver would load them, each due when its
//     character, 11 bit times from its start bit, is in. A word due while
//     the UART is on and GP1 is on the UART function with its input inverted
//     enters the 32-entry receive FIFO, and is dropped otherwise. A word due
//     while the FIFO is full is lost, and the next word that enters carries
//     OE. The receiver reads the word in its own format: at 8N1 the parity
//     bit is taken as the stop bit, so a parity bit of 0 is FE; at odd parity
//     PE is the other way round. A character whose start bit came before the
//     receiver was on is caught in its middle: it loads as FE with its data
//     shifted. uart_set_format() on a running UART takes the SDK's 15 bit
//     times with the receiver off, as uart_disable_before_lcr_write() waits,
//     and only the character in progress at its start completes, in the old
//     format. Not modelled: the bits on the wire, what a line that is not
//     inverted would read, the transmitter ---

/** @brief The two UART instances; only their addresses are used. */
struct uart_inst { int id; };
static uart_inst g_uartInst[2] = { {0}, {1} };
uart_inst_t *uart0 = &g_uartInst[0];
uart_inst_t *uart1 = &g_uartInst[1];

/** @brief A word on the line and when its last bit is in. */
struct FakeLineWord { uint64_t dueUs; uint16_t word; };
static std::deque<FakeLineWord> g_line;
static uint16_t g_rxFifo[FAKE_UART_FIFO];
static unsigned g_rxHead, g_rxCount;
static bool     g_rxOverrunPending;
static uint64_t g_rxOnUs;         /**< When the receiver last came on. */
static uint32_t g_initTakesUs;    /**< Time uart_init() leaves the receiver on before it returns. */
static FakeUart g_uart;
static uart_hw_t g_uartHw;

/** @brief One character's time, us, from its start bit to its load: 11 bits. */
static uint64_t uartCharUs() { return g_uart.baud ? 11000000u / g_uart.baud : 0; }

/** @brief The even parity bit of @p b. */
static uint16_t uartEvenParity(uint8_t b) {
	uint16_t p = 0;
	for (; b; b &= (uint8_t)(b - 1u)) p ^= 1u;
	return p;
}

/** @brief Load one word from the line, if the receiver hears it, as its format reads it. */
static void uartTake(const FakeLineWord &lw) {
	uint16_t w = lw.word;
	if (!g_uart.enabled || g_pins[1].function != GPIO_FUNC_UART ||
	    g_pins[1].inover != GPIO_OVERRIDE_INVERT || g_uart.dataBits != 8) {
		g_uart.dropped++;
		return;
	}
	if (lw.dueUs < g_rxOnUs + uartCharUs()) {
		// Started before the receiver was on: a 0 bit inside it is taken
		// for the start bit.
		w = (uint16_t)(((w & 0xFFu) >> 1) | UART_UARTDR_FE_BITS);
		g_uart.caughtMid++;
	} else if (!(w & UART_UARTDR_BE_BITS)) {
		uint16_t pbit = uartEvenParity((uint8_t)w) ^ ((w & UART_UARTDR_PE_BITS) ? 1u : 0u);
		if (g_uart.parity == UART_PARITY_NONE)
			w = (uint16_t)((w & 0xFFu) | (pbit ? 0u : UART_UARTDR_FE_BITS));
		else if (g_uart.parity == UART_PARITY_ODD)
			w ^= UART_UARTDR_PE_BITS;
	}
	if (g_rxCount == FAKE_UART_FIFO) {
		g_rxOverrunPending = true;
		g_uart.lost++;
		return;
	}
	if (g_rxOverrunPending) w |= UART_UARTDR_OE_BITS;
	g_rxOverrunPending = false;
	g_rxFifo[(g_rxHead + g_rxCount++) % FAKE_UART_FIFO] = w;
}

/** @brief Move every word due by now off the line, into the FIFO or away. */
static void uartLine() {
	uint64_t now = time_us_64();
	while (!g_line.empty() && g_line.front().dueUs <= now) {
		FakeLineWord lw = g_line.front();
		g_line.pop_front();
		uartTake(lw);
	}
}

/** @brief The PL011 after a reset: off, FIFO empty, no overrun owed. */
static void uartClear() {
	g_rxHead = g_rxCount = 0;
	g_rxOverrunPending = false;
	g_uart.enabled = false;
}

void fakeUartReset() {
	g_line.clear();
	g_uart = FakeUart{};
	uartClear();
	g_initTakesUs = 0;
	g_clockHz[1] = 150000000u;
}
void fakeUartLine(uint64_t dueUs, uint16_t word) { g_line.push_back(FakeLineWord{dueUs, word}); }
void fakeUartInitTakes(uint32_t us) { g_initTakesUs = us; }
FakeUart fakeUart() {
	uartLine();
	FakeUart u = g_uart;
	u.fifo = g_rxCount;
	u.queued = (unsigned)g_line.size();
	return u;
}

fake_uart_fr::operator uint32_t() const {
	uartLine();
	return g_rxCount ? 0u : UART_UARTFR_RXFE_BITS;
}
fake_uart_dr::operator uint32_t() const {
	uartLine();
	g_uart.drReads++;
	if (!g_rxCount) return 0;
	uint16_t w = g_rxFifo[g_rxHead];
	g_rxHead = (g_rxHead + 1u) % FAKE_UART_FIFO;
	g_rxCount--;
	return w;
}
uart_hw_t *uart_get_hw(uart_inst_t *) { return &g_uartHw; }

unsigned uart_init(uart_inst_t *, unsigned baud) {
	// Words due before the UART was on were never received.
	uartLine();
	uartClear();
	g_uart.inits++;
	uint32_t hz = g_clockHz[1];
	if (hz == 0) return 0;
	// uart_set_baudrate() of SDK 2.1.1, hardware_uart/uart.c.
	uint32_t div = 8u * hz / baud + 1u;
	uint32_t ibrd = div >> 7, fbrd;
	if (ibrd == 0)           { ibrd = 1; fbrd = 0; }
	else if (ibrd >= 65535u) { ibrd = 65535; fbrd = 0; }
	else                     fbrd = (div & 0x7Fu) >> 1;
	g_uart.ibrd = ibrd;
	g_uart.fbrd = fbrd;
	g_uart.baud = (unsigned)((4ull * hz) / (64u * ibrd + fbrd));
	// uart_init() sets 8N1 and turns the UART on.
	g_uart.dataBits = 8;
	g_uart.stopBits = 1;
	g_uart.parity = UART_PARITY_NONE;
	g_uart.enabled = true;
	g_rxOnUs = time_us_64();
	// The receiver runs at 8N1 until the caller's next call.
	g_us += g_initTakesUs;
	return g_uart.baud;
}
void uart_deinit(uart_inst_t *) {
	uartLine();
	uartClear();
	g_uart.deinits++;
}
bool uart_is_readable(uart_inst_t *) { return !(g_uartHw.fr & UART_UARTFR_RXFE_BITS); }
uint8_t uart_getc(uart_inst_t *) { return (uint8_t)g_uartHw.dr; }
void uart_set_format(uart_inst_t *, unsigned bits, unsigned stop, enum uart_parity_t p) {
	uartLine();
	if (g_uart.enabled) {
		// uart_disable_before_lcr_write(), SDK 2.1.1: UARTEN off, then a
		// wait of about 15 bit times. The character under way completes.
		uint64_t now = time_us_64();
		uint32_t scaled = g_clockHz[1] / 3662u;
		uint64_t wait = scaled ? (((uint64_t)64u * g_uart.ibrd + g_uart.fbrd) << 10) / scaled : 0;
		while (!g_line.empty() && g_line.front().dueUs <= now + wait) {
			FakeLineWord lw = g_line.front();
			g_line.pop_front();
			if (lw.dueUs < now + uartCharUs()) uartTake(lw);
			else g_uart.dropped++;
		}
		g_us = now + wait;
		g_uart.formatWaitUs = (uint32_t)wait;
		g_rxOnUs = g_us;
	}
	g_uart.dataBits = bits;
	g_uart.stopBits = stop;
	g_uart.parity = (int)p;
}
void uart_set_fifo_enabled(uart_inst_t *, bool) {}

// ---------------------------------------------------------------------------
// display: render into the real framebuffer, never push pixels anywhere
// ---------------------------------------------------------------------------
static int g_lastFlushBands = 0;
int fakeLastFlushBands() { return g_lastFlushBands; }
void st7789FlushDirty() {
	g_lastFlushBands = gfxDirtyCount();
	gfxClearDirty();
}
// Recorded rather than discarded: the arm-feedback dip is only observable as a
// backlight level, so a fake that threw it away would make it untestable.
static uint8_t g_backlight = 0;
static uint8_t g_backlightMin = 255;
uint8_t fakeBacklight() { return g_backlight; }
uint8_t fakeBacklightMin() { return g_backlightMin; }
void fakeBacklightResetMin() { g_backlightMin = 255; }
void st7789SetBacklight(uint8_t level) {
	g_backlight = level;
	if (level < g_backlightMin) g_backlightMin = level;
}

// ---------------------------------------------------------------------------
// scripted touch
// ---------------------------------------------------------------------------
static TouchState g_touch;
static bool     s_touchAnswers = true;   /**< Reads complete. */
static bool     s_touchAnswered = true;  /**< A read has completed since touchInit(). */

static uint32_t s_touchAnsweredMs = 0;  /**< millis() of the last read that completed. */

bool touchInit() {
	s_touchAnswered = s_touchAnswers;
	if (s_touchAnswers) s_touchAnsweredMs = millis();
	return s_touchAnswers;
}

bool touchAnswered() { return s_touchAnswered; }

uint32_t touchAnsweredMs() { return s_touchAnsweredMs; }

void fakeTouchStamp() { s_touchAnsweredMs = millis(); }

void fakeTouchAnswers(bool on) { s_touchAnswers = on; }

void touchPoll(TouchState *t) {
	if (s_touchAnswers) {
		s_touchAnswered = true;
		s_touchAnsweredMs = millis();
	}
	*t = g_touch;
	// Edge flags last exactly one poll, as the real driver guarantees.
	g_touch.pressed = false;
	g_touch.released = false;
}

void fakePress(int x, int y) {
	g_touch.down = true;
	g_touch.pressed = true;
	g_touch.x = (int16_t)x;
	g_touch.y = (int16_t)y;
	g_touch.downX = (int16_t)x;
	g_touch.downY = (int16_t)y;
}

void fakeHold(int x, int y) {
	g_touch.down = true;
	g_touch.x = (int16_t)x;
	g_touch.y = (int16_t)y;
}

void fakeRelease() {
	g_touch.down = false;
	g_touch.released = true;
}

// ---------------------------------------------------------------------------
// fake AM32 ESC
//
// Seeded with the Air preset recovered from the reference configurator, so the
// UI tests operate on values a real ESC would actually report. The preset's
// firmware bytes read 1.35; these read 2.16, a release that writes layout 2,
// so the list shows every layout-2 field. @see AM32_FW_READS_2F
// ---------------------------------------------------------------------------
static uint8_t g_esc[AM32_EEPROM_SIZE] = {
	0x01,0x02,0x01,0x02,0x10,0x4e,0x45,0x4f,0x45,0x53,0x43,0x20,0x66,0x30,0x35,0x31,
	0x20,0x00,0x01,0x01,0x01,0x01,0x00,0x03,0x18,0x68,0x30,0x0e,0x01,0x01,0x05,0x00,
	0x80,0x80,0x80,0x32,0x01,0x50,0x00,0x00,0x0f,0x0a,0x0a,0x65,0x14,0x05,0x00,0x31};

uint8_t fakeEscByte(int offset) { return g_esc[offset]; }
void fakeEscSet(int offset, uint8_t v) { g_esc[offset] = v; }

uint16_t am32Crc16(const uint8_t *d, uint16_t n) {
	uint16_t c = 0;
	for (uint16_t i = 0; i < n; i++) {
		uint8_t b = d[i];
		for (int k = 0; k < 8; k++) {
			if ((b ^ (uint8_t)c) & 1) { c >>= 1; c ^= 0xA001; } else { c >>= 1; }
			b >>= 1;
		}
	}
	return c;
}

static uint8_t g_am32Pin = 255;
uint8_t fakeAm32Pin() { return g_am32Pin; }
void fakeAm32Reset() { g_am32Pin = 255; }
void am32BlBegin(uint8_t pin) { g_am32Pin = pin; }
// Defined with the DShot task below: it records whether DShot was still
// down when the transport let go of the pin.
void am32BlJumpToBootloader() {}
void am32BlSetBaud(uint32_t) {}
uint32_t am32BlBaud() { return 19200; }
// Failures the tests ask for: each counts down, one per call.
static int     g_am32WriteFails = 0;   /**< am32BlWrite() calls that fail, storing nothing. */
static int     g_am32ReadFails = 0;    /**< am32BlRead() calls that fail. */
static int     g_am32WriteSkews = 0;   /**< am32BlWrite() calls that store byte 0x1B wrong. */
static uint8_t g_am32Garble[16];       /**< A handshake reply other than an AM32 bootloader's. */
static uint8_t g_am32GarbleLen = 0;    /**< Its length; 0: the handshake succeeds. */
/** @brief The last handshake's answer, classified by the real blAnswerClassify(). */
static BlAnswer g_am32Answer = { BlAnswerKind::Silent, 0, 0 };
static bool    g_am32Blank = false;    /**< Reads return an erased page. */
static bool    g_am32Silent = false;   /**< Handshakes time out with no reply. */
void fakeAm32FailWrites(int n) { g_am32WriteFails = n; }
void fakeAm32FailReads(int n) { g_am32ReadFails = n; }
void fakeAm32SkewWrites(int n) { g_am32WriteSkews = n; }
void fakeAm32Garble(const uint8_t *b, uint8_t n) {
	g_am32GarbleLen = n > sizeof(g_am32Garble) ? (uint8_t)sizeof(g_am32Garble) : n;
	if (n) memcpy(g_am32Garble, b, g_am32GarbleLen);
}
void fakeAm32Blank(bool on) { g_am32Blank = on; }
void fakeAm32Silent(bool on) { g_am32Silent = on; }
static int g_am32Handshakes = 0;  /**< am32BlHandshake() calls: greetings sent. */
int fakeAm32Handshakes() { return g_am32Handshakes; }
Am32Result am32BlHandshake(uint8_t *) {
	g_am32Handshakes++;
	// Nine bytes with F051's family byte fifth from the end and the ACK
	// last, or the bytes a test set, through the classification the
	// transport uses. The other bytes of an AM32 answer are not modelled.
	static const uint8_t F051[9] = { '4', '7', '1', 0, AM32_ESC_F051_1KB, 0, 0, 0, 0x30 };
	if (g_am32Silent) g_am32Answer = blAnswerClassify(nullptr, 0);
	else if (g_am32GarbleLen) g_am32Answer = blAnswerClassify(g_am32Garble, g_am32GarbleLen);
	else g_am32Answer = blAnswerClassify(F051, sizeof(F051));
	switch (g_am32Answer.kind) {
		case BlAnswerKind::Silent: return AM32_ERR_TIMEOUT;
		case BlAnswerKind::Am32:   return AM32_OK;
		default:                   return AM32_ERR_NAK;
	}
}
BlAnswer am32BlAnswer() { return g_am32Answer; }
Am32Result am32BlRead(uint16_t, uint8_t *b, uint8_t n) {
	if (g_am32ReadFails > 0) { g_am32ReadFails--; return AM32_ERR_TIMEOUT; }
	if (g_am32Blank) memset(b, 0xFF, n);
	else memcpy(b, g_esc, n);
	return AM32_OK;
}
static int g_am32Writes = 0;  /**< am32BlWrite() calls. */
int fakeAm32Writes() { return g_am32Writes; }
Am32Result am32BlWrite(uint16_t, const uint8_t *b, uint8_t n) {
	g_am32Writes++;
	if (g_am32WriteFails > 0) { g_am32WriteFails--; return AM32_ERR_NAK; }
	memcpy(g_esc, b, n);
	if (g_am32WriteSkews > 0) { g_am32WriteSkews--; g_esc[0x1B] ^= 0x40; }
	return AM32_OK;
}
uint8_t am32BlLastRx(const uint8_t **b) { *b = g_am32Garble; return g_am32Silent ? 0 : g_am32GarbleLen; }
uint16_t am32BlEepromAddr() { return 0x7C00; }
const char *am32BlEscTypeName() { return "F051"; }

// ---------------------------------------------------------------------------
// DShot task
// ---------------------------------------------------------------------------
static uint16_t g_throttle = 0;
static bool     g_armed = false;
static bool     g_suspendReq = false;
static bool     g_rebuildReq = false;
static bool     g_suspended = false;
static EscTelemetry g_tel;

uint16_t fakeThrottle() { return g_throttle; }
bool fakeArmed() { return g_armed; }
bool fakePinReturned() { return !g_suspendReq; }
void fakeSetTelemetry(const EscTelemetry *t) { g_tel = *t; }

// These three mirror esc_task.cpp deliberately, clamp for clamp. They used not
// to: escSetArmed() only set a flag, so "disarming zeroes the commanded
// throttle" -- a rule production does enforce -- could not be observed by any
// test, and escSetThrottle() had no clamp, so nothing checked the one in
// production either.
void escSetThrottle(uint16_t t) { g_throttle = t > 2000 ? 2000 : t; }
void escSetArmed(bool a) {
	if (!a) g_throttle = 0;
	g_armed = a;
}
static uint8_t g_poles = 14;
uint8_t fakePoles() { return g_poles; }
void escSetPoles(uint8_t p) { g_poles = p < 2 ? 2 : p; }
static uint32_t g_heartbeatMs = 0;
void escHeartbeat() { g_heartbeatMs = millis(); }
uint32_t escHeartbeatMs() { return g_heartbeatMs; }
// Core0's side of the command slot. esc_task.cpp cannot be linked here (PIO,
// UART), so the request side is a copy -- but its rule is the shipped
// escCmdAccept(). The readiness core1 would publish is set by the test (fakeSetCmdReady()); the
// default, Ready, is an ESC that has answered for 1500 ms with the motor
// stopped. The counter increments only when the request is taken, as
// production queues it only then. Counting refused requests too meant a test
// asserting "the button sent the command" passed for a request the ESC never
// saw.
static int         g_beepRequests = 0;
static uint8_t     g_lastCmd = 0;
static bool        g_lastSave = false;
static int         g_cmdCancels = 0;
static EscCmdReady g_cmdReady = EscCmdReady::Ready;
// The state as production reports it: Waiting while a taken request is not
// yet read, else what core1 published, which the test sets.
static bool        g_cmdWaiting = false;
static EscCmdState g_cmdState = EscCmdState::Idle;
static uint8_t     g_cmdStateCmd = 0;
int  fakeBeepRequests() { return g_beepRequests; }
uint8_t fakeLastCommand(bool *save) {
	if (save) *save = g_lastSave;
	return g_lastCmd;
}
int  fakeCommandCancels() { return g_cmdCancels; }
void fakeSetCmdReady(EscCmdReady r) { g_cmdReady = r; }
bool escRequestCommand(uint8_t cmd, bool save) {
	if (!escCmdAccept(cmd, save, g_armed, g_cmdReady)) return false;
	g_beepRequests++;
	g_lastCmd = cmd;
	g_lastSave = save;
	g_cmdWaiting = true;
	return true;
}
void fakeSetCmdState(EscCmdState st, uint8_t cmd) {
	g_cmdWaiting = false;
	g_cmdState = st;
	g_cmdStateCmd = cmd;
}
void escCancelCommand() {
	g_cmdCancels++;
	g_cmdWaiting = false;
}
EscCmdReady escCommandReady() { return g_cmdReady; }
EscCmdState escCommandState(uint8_t *cmd) {
	if (cmd) *cmd = g_cmdWaiting ? g_lastCmd : g_cmdStateCmd;
	return g_cmdWaiting ? EscCmdState::Waiting : g_cmdState;
}
// Settable, because production only reports true once an ESC has answered and
// been sent an enable -- so a hardcoded true made the DS600 chip's two states
// indistinguishable and the settings screen's EDT indicator untestable.
static bool g_edtRequested = true;
void fakeSetEdtRequested(bool on) { g_edtRequested = on; }
bool escEdtRequested() { return g_edtRequested; }
// Verbatim, with no helpful stamping of arrival times. That stamping used to
// be here, and it meant every test saw permanently fresh telemetry -- so the
// bug where readings never expired could not have been caught by any of them.
// Tests that want live data now say when it arrived.
void escSnapshot(EscTelemetry *o) { *o = g_tel; }

// The pump's handover on the shipped rules: escHandoverStep() decides run,
// drain or teardown, escDrainSlot() or escPumpSlot() decides the frame,
// escPumpFrameSent() stamps it, and escPumpBind() starts every binding. The
// rest of escTaskPoll() is not modelled: no PIO, no reply is read or folded
// into the telemetry, there is no 1 ms frame timing (one pass sends one frame,
// however much time has passed), and escPumpSlot() is given uiAlive true, no
// command request, no reply and no KISS receiver. Core1 is not a thread here:
// fakePumpPoll() is one pass, and the tests call it between UI frames, so a
// suspend, a drain and a rebuild each take as many passes as a test gives them.
static EscPump    g_pump;
static bool       g_pumpUp = false;
static int        g_resumes = 0;
static int        g_rebuilds = 0;
static FakeWiring g_rebuildWiring = {};
static bool       g_resumeJobIdle = true;

void fakePumpPoll() {
	uint32_t ms = millis();
	const EscCmdRequest noRequest = {0, false, 0};
	EscHandover h = escHandoverStep(g_suspendReq, g_rebuildReq, g_pumpUp,
	                                g_pump.lastThrottleMs, ms);
	if (h == EscHandover::Teardown) {
		g_pumpUp = false;
		g_rebuildReq = false;
		if (g_suspendReq) {
			g_suspended = true;
			return;
		}
	}
	if (!g_suspendReq && !g_pumpUp) {
		escPumpBind(&g_pump, &g_armed, &g_throttle);
		g_pumpUp = true;
		g_suspended = false;
		g_rebuilds++;
		g_rebuildWiring = fakeWiring();
	}
	EscSlot slot = h == EscHandover::Drain
	             ? escDrainSlot()
	             : escPumpSlot(&g_pump, &g_tel, g_armed, true, g_throttle,
	                           &noRequest, DSHOT_REPLY_NONE, false, ms);
	escPumpFrameSent(&g_pump, &slot.frame, ms);
}

uint32_t fakeLastThrottleMs() { return g_pump.lastThrottleMs; }

void escTaskSuspend() {
	escSetArmed(false);
	g_suspendReq = true;
}
// The transport must let go of the pin before DShot is asked to take it back.
static bool g_am32EndBeforeResume = false;
bool fakeAm32EndBeforeResume() { return g_am32EndBeforeResume; }
static int g_am32Ends = 0;  /**< am32BlEnd() calls. */
int fakeAm32Ends() { return g_am32Ends; }
void am32BlEnd() {
	g_am32Ends++;
	g_am32EndBeforeResume = g_suspendReq;
}
bool escTaskSuspended() { return g_suspended; }
// Only the request, as in esc_task.cpp: the rebuild is the next
// fakePumpPoll().
void escTaskResume() {
	g_suspendReq = false;
	g_resumes++;
	g_resumeJobIdle = core1JobPhase() == Core1JobPhase::Idle;
}
int fakeResumeCount() { return g_resumes; }
int fakeRebuildCount() { return g_rebuilds; }
FakeWiring fakeResumeWiring() { return g_rebuildWiring; }
bool fakeResumeJobIdle() { return g_resumeJobIdle; }

// Mirrors esc_task.cpp: a wiring change disarms and zeroes the throttle,
// because the ESC on the old pin stops hearing frames the moment it is
// released. Tests assert on that, so the fake has to do it too.
//
// Seeded to a pin the board does not offer rather than to a default, and for
// the same reason the real escTaskInit() seeds from the stored settings: the
// ESC pin default lives in the descriptor, and reading it from a static
// initialiser here would depend on the link order of two other translation
// units. Every test enters through uiInit(), which configures this
// before anything reads it; 255 is what a test that forgot would see, and it is
// unmistakable. @see cfg_pin_defaults
static uint8_t  g_dshotPin = 255;
static uint16_t g_dshotKbaud = DSHOT_SPEED_KBAUD;
static bool     g_kissEnable = false;
static uint8_t  g_kissPin = 255;
static int      g_configures = 0;
int fakeConfigureCount() { return g_configures; }
uint16_t fakeDshotKbaud() { return g_dshotKbaud; }
void escTaskConfigure(uint8_t pin, uint16_t kbaud, bool kissEnable, uint8_t kissPin) {
	if (g_dshotPin == pin && g_dshotKbaud == kbaud &&
	    g_kissEnable == kissEnable && g_kissPin == kissPin)
		return;
	g_dshotPin = pin;
	g_dshotKbaud = kbaud;
	g_kissEnable = kissEnable;
	g_kissPin = kissPin;
	escSetArmed(false);
	g_rebuildReq = true;
	g_configures++;
}
uint8_t escTaskDshotPin() { return g_dshotPin; }
uint16_t escTaskDshotKbaud() { return g_dshotKbaud; }
uint8_t escTaskKissPin() { return g_kissPin; }
FakeWiring fakeWiring() {
	FakeWiring w = {g_dshotPin, g_dshotKbaud, g_kissEnable, g_kissPin};
	return w;
}

// ---------------------------------------------------------------------------
// Settings storage
//
// settings.cpp holds every rule -- defaults, validation, the CRC, and the
// discard-whole-block fallback -- and none of the flash sequence, so it links
// here against this RAM array instead of settings_flash.cpp. That split is what
// makes the rules testable at all: the interesting behaviour is "what happens
// to a block that does not validate", and answering it needs to be able to
// write a bad one.
// ---------------------------------------------------------------------------
static uint8_t g_flash[256];
static bool    g_flashWritable = true;
static int     g_flashWrites = 0;

void fakeFlashClear() { memset(g_flash, 0xFF, sizeof(g_flash)); }
void fakeFlashSetWritable(bool on) { g_flashWritable = on; }
uint8_t *fakeFlashBytes() { return g_flash; }
int fakeFlashWrites() { return g_flashWrites; }

bool settingsStorageRead(void *dst, uint32_t len) {
	if (len > sizeof(g_flash)) return false;
	memcpy(dst, g_flash, len);
	return true;
}

bool settingsStorageWrite(const void *src, uint32_t len) {
	// Counted even when refused: on the device the sector is erased before the
	// program, so a write *attempt* is already wear and already the torn
	// window. "How many attempts" is the number the no-op-save test cares
	// about.
	g_flashWrites++;
	if (!g_flashWritable) return false;
	if (len > sizeof(g_flash)) return false;
	memcpy(g_flash, src, len);
	return true;
}

// ---------------------------------------------------------------------------
// Reboot
//
// Nothing in the firmware calls watchdog_reboot(): every setting applies
// live. So this counts a thing that must never happen -- see testBoardRow().
// ---------------------------------------------------------------------------
static int g_reboots = 0;

int fakeRebootCount() { return g_reboots; }

extern "C" void watchdog_reboot(uint32_t, uint32_t, uint32_t) { g_reboots++; }

// ---------------------------------------------------------------------------
// Restart reason
//
// The power manager's CHIP_RESET register, the watchdog's REASON register,
// watchdog_enable_caused_reboot() and rom_get_last_boot_type(), which
// diagCaptureRestart() reads. They read 0, false and BOOT_TYPE_NORMAL until
// a test sets them.
// ---------------------------------------------------------------------------
powman_hw_t g_fakePowman = {};
watchdog_hw_t g_fakeWatchdog = {};
static bool g_wdEnableCaused = false;
static int g_bootType = BOOT_TYPE_NORMAL;

void fakeSetChipReset(uint32_t v) { g_fakePowman.chip_reset = v; }
void fakeSetWatchdog(uint32_t reason, bool enableCaused) {
	g_fakeWatchdog.reason = reason;
	g_wdEnableCaused = enableCaused;
}
void fakeSetBootType(int type) { g_bootType = type; }
int rom_get_last_boot_type(void) { return g_bootType; }
extern "C" bool watchdog_enable_caused_reboot(void) { return g_wdEnableCaused; }

// ---------------------------------------------------------------------------
// Region fingerprint
//
// Lets a test say "this part of the screen changed" -- or, more usefully,
// "this part of the screen is now pixel-identical to the no-data case" --
// without hard-coding glyph positions that every layout tweak would break.
// ---------------------------------------------------------------------------
uint32_t fakeRegionHash(int x, int y, int w, int h) {
	const uint16_t *fb = gfxBuffer();
	uint32_t hash = 2166136261u;            // FNV-1a
	for (int yy = y; yy < y + h; yy++) {
		for (int xx = x; xx < x + w; xx++) {
			if (xx < 0 || yy < 0 || xx >= GFX_W || yy >= GFX_H) continue;
			hash = (hash ^ fb[yy * GFX_W + xx]) * 16777619u;
		}
	}
	return hash;
}

bool fakeTextIs(int x, int y, const char *text, uint16_t col, uint16_t bg, int scale) {
	uint16_t *fb = gfxBuffer();
	// High contrast draws each glyph one row taller (gfxChar()): the probe
	// covers, hashes and restores that row too.
	const int w = gfxTextW(text, scale), h = 7 * scale + (themeBold() ? 1 : 0);
	if (x < 0 || y < 0 || x + w > GFX_W || y + h > GFX_H) return false;
	static uint16_t keep[GFX_H][GFX_W];
	for (int j = 0; j < h; j++) memcpy(keep[j], fb + (y + j) * GFX_W + x, (size_t)w * sizeof(uint16_t));
	uint32_t shown = fakeRegionHash(x, y, w, h);
	gfxRect(x, y, w, h, bg);
	gfxText(x, y, text, col, scale);
	bool same = fakeRegionHash(x, y, w, h) == shown;
	for (int j = 0; j < h; j++) memcpy(fb + (y + j) * GFX_W + x, keep[j], (size_t)w * sizeof(uint16_t));
	return same;
}

bool fakeTextCentredIs(int y, const char *text, uint16_t col, uint16_t bg, int scale) {
	// gfxTextCenter()'s x, clamped at 0 as it clamps.
	int x = (GFX_W - gfxTextW(text, scale)) / 2;
	if (x < 0) x = 0;
	return fakeTextIs(x, y, text, col, bg, scale);
}

bool fakeChangeIs(int y, const char *old, const char *now, uint16_t bg) {
	char head[64], pair[128];
	snprintf(head, sizeof(head), "%s -> ", old);
	snprintf(pair, sizeof(pair), "%s%s", head, now);
	int x = GFX_W - 8 - gfxTextW(pair, 1);
	return fakeTextIs(x, y, head, C_DIM, bg, 1) &&
	       fakeTextIs(x + 6 * (int)strlen(head), y, now, C_CYAN, bg, 1);
}

bool fakeInkExtent(int y, int h, uint16_t bg, int *x0, int *x1) {
	const uint16_t *fb = gfxBuffer();
	int lo = GFX_W, hi = -1;
	for (int yy = y; yy < y + h && yy < GFX_H; yy++)
		for (int xx = 0; xx < GFX_W; xx++)
			if (fb[yy * GFX_W + xx] != bg) {
				if (xx < lo) lo = xx;
				if (xx > hi) hi = xx;
			}
	if (hi < 0) return false;
	*x0 = lo;
	*x1 = hi;
	return true;
}

/** @brief Count framebuffer pixels of an exact colour. @see fakes.h */
int fakeCountColour(uint16_t c) {
	const uint16_t *fb = gfxBuffer();
	int n = 0;
	for (int i = 0; i < GFX_W * GFX_H; i++) if (fb[i] == c) n++;
	return n;
}

// ---------------------------------------------------------------------------
// PPM dump, so a failing layout test can be looked at
// ---------------------------------------------------------------------------
void fakeDumpFrame(const char *name) {
	const uint16_t *fb = gfxBuffer();
	FILE *f = fopen(name, "wb");
	if (!f) return;
	fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
	for (int i = 0; i < GFX_W * GFX_H; i++) {
		uint16_t c = fb[i];
		uint8_t px[3] = { (uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
		                  (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
		                  (uint8_t)((c & 0x1F) * 255 / 31) };
		fwrite(px, 1, 3, f);
	}
	fclose(f);
}

// ---------------------------------------------------------------------------
// SD logging: a fake card the tests drive directly
// ---------------------------------------------------------------------------
//
// sd_log.cpp itself cannot be linked here -- it pulls in FatFs and the SPI
// driver -- so the UI is tested against this instead. It is not a simulation of
// a card: it is a way to put the logger into a given state and check the screen
// renders it.

static SdLogStatus g_log = {};
/** @brief The summary a start begins and its stop leaves, and the poles kept at the stop. */
static const RunSum *g_logSum = nullptr;
static uint8_t g_logSumPoles = 0;

void fakeSdLogSet(const SdLogStatus *st) { g_log = *st; }

void fakeSdLogSetSummary(const RunSum *s, uint8_t poles) {
	g_logSum = s;
	g_logSumPoles = poles;
}

/** @brief A mount, as sd_log.cpp's sdLogBegin() does it to the status. */
static void fakeMountForget() {
	sdLogMountForget(&g_log.end);
	g_log.summary = nullptr;
	g_log.fileNumber = 0;
	g_log.framesLogged = 0;
	g_log.bytesWritten = 0;
	g_log.bytesDropped = 0;
	g_log.dropEvents = 0;
	g_log.peakBuffer = 0;
	g_log.worstFlushMs = 0;
}

bool sdLogBegin() {
	g_log.state = SdLogState::Idle;
	fakeMountForget();
	return true;
}

static bool g_autoStarted = false;
/** @brief The highest `LOGnnnnn.BFL` on the fake card, 0 for none. */
static uint32_t g_logHighest = 0;

void fakeSdLogSetHighest(uint32_t n) { g_logHighest = n; }

// As sd_log.cpp: the highest number on the card plus 1, through the shipped
// rule; after LOG99999.BFL the start is refused with its cause.
bool sdLogStart() {
	if (g_log.state == SdLogState::NoCard) return false;
	if (g_log.state == SdLogState::Logging) return true;
	g_log.end = SdLogEnd::None;
	uint32_t n = sdLogNextNumber(g_logHighest);
	if (n == 0) {
		g_log.state = SdLogState::Error;
		g_log.end = SdLogEnd::NoFreeNumber;
		return false;
	}
	g_log.state = SdLogState::Logging;
	g_autoStarted = false;
	// As sd_log.cpp: a start that opens its log begins the summary, which
	// the status shows only once no log runs; a failed start keeps the last.
	g_log.summary = g_logSum;
	g_log.summaryPoles = g_logSumPoles;
	g_log.fileNumber = n;
	g_logHighest = n;
	return true;
}

// As sd_log.cpp: the number stays, for the FILE row's (LAST), until the next
// start.
void sdLogStop() {
	if (g_log.state == SdLogState::Logging) g_log.state = SdLogState::Idle;
	g_autoStarted = false;
}

void sdLogStopFor(SdLogEnd tool) {
	if (g_log.state != SdLogState::Logging) return;
	sdLogStop();
	if (g_log.state == SdLogState::Idle) g_log.end = tool;
}

bool sdLogRemount() {
	// Models a card that appears on retry: NO CARD becomes READY. That is the
	// case the button exists for -- a card inserted after boot.
	// A mount, as sdLogBegin(), which sd_log.cpp skips while a log runs:
	// how the last log ended is forgotten.
	// From any other state the card in the slot mounts, as sdLogBegin() does
	// with a card that answers: READY, a card fault forgotten.
	if (g_log.state == SdLogState::Logging) return true;
	fakeMountForget();
	if (g_log.state == SdLogState::NoCard) {
		g_log.mountResult = 0;
		g_log.cardType = 3;          // SDHC/XC
		g_log.cardSizeMB = 122000;   // a 128 GB card, as the card reports itself
	}
	g_log.state = SdLogState::Idle;
	return true;
}

bool sdLogActive() { return g_log.state == SdLogState::Logging; }
void sdLogTick(uint32_t, uint16_t) {}
void sdLogFlush() {}
// Uses the shipped decision function, not a copy of it, so the UI tests
// actually exercise the policy the firmware runs.
static bool g_sdArmed = false;
void sdLogSetArmed(bool armed) {
	SdLogArmAction act = sdLogArmAction(armed, g_sdArmed, sdLogActive(),
	                                    g_autoStarted);
	g_sdArmed = armed;
	switch (act) {
		case SdLogArmAction::Start: g_autoStarted = sdLogStart(); break;
		case SdLogArmAction::Stop:  sdLogStop();                  break;
		case SdLogArmAction::None:                                break;
	}
}
void sdLogStatus(SdLogStatus *out) {
	*out = g_log;
	// As sd_log.cpp: no summary while a log runs.
	if (out->state == SdLogState::Logging) out->summary = nullptr;
}

// ---------------------------------------------------------------------------
// the SD write cache (card_cache.h)
// ---------------------------------------------------------------------------
// card_cache.cpp drives the SDIO driver and is not linked here; the cache's
// own rules are tested in test_sd_cache.cpp. This fake lets a test put the
// cache into a state and count the calls the screens make.

static bool      g_cacheOn = false;
static CardCache g_cacheState = CardCache::Idle;
static uint16_t  g_cacheLeft = 0;
static CardCache g_cacheFlushTo = CardCache::Idle;
static FakeCacheCalls g_cacheCalls = {};
static uint32_t  g_volumeSerial = 1;
static bool      g_volumeAnswers = true;
static unsigned  g_volumeFailNext = 0;

// ---------------------------------------------------------------------------
// the KST card phase's wait (kst_job.h)
// ---------------------------------------------------------------------------
// On the firmware core1 runs while core0 waits for the job to take a
// CardBegin; here the wait runs core1 itself, one millisecond a step.

static void (*g_cardWaitPass)() = nullptr;

void fakeCardWaitPass(void (*pass)()) { g_cardWaitPass = pass; }

void kstCardWaitPoll() {
	if (g_cardWaitPass) {
		g_cardWaitPass();
		return;
	}
	fakeAdvance(1);
	core1JobPoll();
}

void fakeCacheReset() {
	g_volumeSerial = 1;
	g_volumeFailNext = 0;
	g_volumeAnswers = true;
	g_cacheOn = false;
	g_cacheState = CardCache::Idle;
	g_cacheLeft = 0;
	g_cacheFlushTo = CardCache::Idle;
	g_cacheCalls = FakeCacheCalls{};
}

void fakeCacheSet(CardCache st, uint16_t left) {
	g_cacheState = st;
	g_cacheLeft = left;
}

void fakeCacheFlushTo(CardCache st) { g_cacheFlushTo = st; }

void fakeVolumeFailNext(unsigned n) { g_volumeFailNext = n; }

void fakeVolumeSet(uint32_t serial, bool answers) {
	g_volumeSerial = serial;
	g_volumeAnswers = answers;
}

FakeCacheCalls fakeCacheCalls() { return g_cacheCalls; }

bool cardCacheBegin(SdLogEnd tool) {
	g_cacheCalls.begins++;
	sdLogStopFor(tool);
	SdLogStatus st;
	sdLogStatus(&st);
	bool mounted = st.state == SdLogState::Idle || st.state == SdLogState::Logging;
	if (!mounted || mscGetState() != MscState::Idle) return false;
	g_cacheOn = true;
	g_cacheState = CardCache::Idle;
	g_cacheLeft = 0;
	return true;
}

void cardCacheEnd() {
	g_cacheCalls.ends++;
	g_cacheOn = false;
}

void cardCachePoll() {}

CardCache cardCacheState(uint16_t *left) {
	if (left) *left = g_cacheOn ? g_cacheLeft : 0;
	return g_cacheOn ? g_cacheState : CardCache::Off;
}

CardCache cardCacheFlush() {
	g_cacheCalls.flushes++;
	if (!g_cacheOn) return CardCache::Off;
	if (g_cacheState == CardCache::Flushing || g_cacheFlushTo == CardCache::Failed) {
		g_cacheState = g_cacheFlushTo;
		g_cacheLeft = 0;
	}
	return g_cacheState;
}

void cardCacheRecover() {
	g_cacheCalls.recovers++;
	g_cacheState = CardCache::Idle;
	g_cacheLeft = 0;
	// The remount reads the card in the slot: one that answers again.
	g_volumeAnswers = true;
}

bool cardVolumeSerial(uint32_t *serial) {
	*serial = 0;
	SdLogStatus st;
	sdLogStatus(&st);
	bool mounted = st.state == SdLogState::Idle || st.state == SdLogState::Logging;
	if (!mounted || mscGetState() != MscState::Idle || !g_volumeAnswers) return false;
	if (g_volumeFailNext) {
		g_volumeFailNext--;
		return false;
	}
	*serial = g_volumeSerial;
	return true;
}

// ---------------------------------------------------------------------------
// USB mass storage
// ---------------------------------------------------------------------------
// The pure rules are tested directly in test_usb_msc.cpp against the real
// header. This fake exists so ui.cpp -- which is the real file, linked into the
// suite -- can be driven through the screen states without a USB stack.

static MscState g_msc = MscState::Idle;
static uint32_t g_mscBlocks = 0;

void mscInit() { g_msc = MscState::Idle; g_mscBlocks = 0; }
MscState mscGetState() { return g_msc; }
uint32_t mscBlocksRead() { return g_mscBlocks; }

MscRefusal mscRequest() {
	SdLogStatus st;
	sdLogStatus(&st);
	MscRefusal r = mscRefusal(st.state != SdLogState::NoCard, uiArmed(),
	                          st.state == SdLogState::Logging);
	if (r != MscRefusal::None) return r;
	// Straight to Serving. The real one goes through Handover and needs an
	// mscTick() to get here; the tests that care about that ordering assert it
	// on mscNext() itself, where there is no card to unmount.
	g_msc = MscState::Serving;
	// As sdLogRelease(): the card handed over, no summary shows.
	g_log.summary = nullptr;
	return MscRefusal::None;
}

// As usb_msc.cpp with its mscTick() folded in, as mscRequest() folds the
// handover: the release reclaims the card, and the reclaim is a mount
// (sdLogReacquire(), sdLogBegin()) of a card that is still there.
void mscRelease() {
	if (g_msc == MscState::Idle) return;
	g_msc = MscState::Idle;
	sdLogBegin();
}
void mscTick() {
	if (g_msc != MscState::Reclaim) return;
	g_msc = MscState::Idle;
	sdLogBegin();
}

void fakeMscSet(MscState s, uint32_t blocksRead) {
	g_msc = s;
	g_mscBlocks = blocksRead;
}
