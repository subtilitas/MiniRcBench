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
#include "gfx.h"
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
#include "card_cache.h"
#include "plat.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// virtual clock and the Pico SDK surface the firmware uses
// ---------------------------------------------------------------------------

// Time is virtual: it advances only when a test says so. plat.h's millis() and
// micros() are inline wrappers over these, so faking the SDK timebase fakes
// both without the firmware knowing.
static uint32_t g_ms = 0;

void fakeAdvance(uint32_t ms) { g_ms += ms; }
uint64_t time_us_64() { return (uint64_t)g_ms * 1000u; }
uint32_t time_us_32() { return g_ms * 1000u; }
void sleep_ms(uint32_t d) { g_ms += d; }
void sleep_us(uint64_t us) { g_ms += (uint32_t)(us / 1000u); }

// --- ADC: a steady ~3.92 V pack through the board's 200k/100k divider ---
void adc_init() {}
void adc_gpio_init(unsigned) {}
void adc_select_input(unsigned) {}
uint16_t adc_read() { return (uint16_t)(3.92f / 3.0f / 3.3f * 4095.0f); }

// --- PWM and GPIO: a record of the registers the servo PWM driver sets.
//     The slice and channel follow the RP2350's fold for GP0 to GP29; a test
//     can break the slice to make the driver's agreement check refuse.
//     Nothing here models a counter or an edge: a level is what the compare
//     register was last set to ---
static FakePin   g_pins[FAKE_GPIOS];
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
	g_pins[gpio].level = level;
	g_pins[gpio].levelWrites++;
	logWrite(gpio, time_us_64(), level);
}
extern "C" void gpio_set_function(uint32_t gpio, gpio_function fn) {
	if (gpio >= FAKE_GPIOS) return;
	if (fn != GPIO_FUNC_PWM) cutCheck(gpio);
	g_pins[gpio].function = (int)fn;
}
extern "C" void gpio_set_dir(uint32_t gpio, int out) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].output = out != 0;
}
extern "C" void gpio_disable_pulls(uint32_t gpio) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].pull = FakePull::None;
}
extern "C" void gpio_pull_down(uint32_t gpio) {
	if (gpio < FAKE_GPIOS) g_pins[gpio].pull = FakePull::Down;
}

uint32_t clock_get_hz(enum clock_index) { return 150000000u; }

// --- Critical sections: one thread, so there is nothing to exclude ---
extern "C" void critical_section_init(critical_section_t *) {}
extern "C" void critical_section_enter_blocking(critical_section_t *) {}
extern "C" void critical_section_exit(critical_section_t *) {}

// --- I2C: every transfer fails, which is what an absent touch controller
//     does. touchPoll() then reports no contact and the fake touch injector
//     in this file drives the UI tests instead ---
i2c_inst_t *i2c0 = nullptr;
i2c_inst_t *i2c1 = nullptr;

// The board descriptor names an SPI instance. On the host it is never dialled
// -- the panel is faked -- but the descriptor is a real object whose fields
// have to resolve at link time.
spi_inst_t *spi0 = nullptr;
spi_inst_t *spi1 = nullptr;
void i2c_init(i2c_inst_t *, unsigned) {}
int i2c_write_timeout_us(i2c_inst_t *, uint8_t, const uint8_t *, size_t, bool, unsigned) { return -1; }
int i2c_read_timeout_us(i2c_inst_t *, uint8_t, uint8_t *, size_t, bool, unsigned) { return -1; }

// --- UART: never readable. No ESC here to answer a telemetry request ---
uart_inst_t *uart0 = nullptr;
uart_inst_t *uart1 = nullptr;
unsigned uart_init(uart_inst_t *, unsigned baud) { return baud; }
void uart_deinit(uart_inst_t *) {}
bool uart_is_readable(uart_inst_t *) { return false; }
uint8_t uart_getc(uart_inst_t *) { return 0; }
void uart_set_format(uart_inst_t *, unsigned, unsigned, enum uart_parity_t) {}
void uart_set_fifo_enabled(uart_inst_t *, bool) {}

// ---------------------------------------------------------------------------
// display: render into the real framebuffer, never push pixels anywhere
// ---------------------------------------------------------------------------
void st7789FlushDirty() { gfxClearDirty(); }
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

bool touchInit() { return true; }

void touchPoll(TouchState *t) {
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
// UI tests operate on values a real ESC would actually report.
// ---------------------------------------------------------------------------
static uint8_t g_esc[AM32_EEPROM_SIZE] = {
	0x01,0x02,0x01,0x01,0x23,0x4e,0x45,0x4f,0x45,0x53,0x43,0x20,0x66,0x30,0x35,0x31,
	0x20,0x00,0x01,0x01,0x01,0x01,0x00,0x03,0x18,0x68,0x30,0x0e,0x01,0x01,0x05,0x00,
	0x80,0x80,0x80,0x32,0x01,0x50,0x00,0x00,0x0f,0x0a,0x0a,0x65,0x14,0x05,0x00,0x31};

uint8_t fakeEscByte(int offset) { return g_esc[offset]; }

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
Am32Result am32BlHandshake(uint8_t *) { return AM32_OK; }
Am32Result am32BlRead(uint16_t, uint8_t *b, uint8_t n) { memcpy(b, g_esc, n); return AM32_OK; }
Am32Result am32BlWrite(uint16_t, const uint8_t *b, uint8_t n) { memcpy(g_esc, b, n); return AM32_OK; }
Am32Result am32BlErasePage(uint16_t) { return AM32_OK; }
Am32Result am32BlRun() { return AM32_OK; }
const char *am32ResultText(Am32Result r) { return r == AM32_OK ? "OK" : "ERR"; }
uint8_t am32BlLastRx(const uint8_t **b) { static uint8_t none[1] = {0}; *b = none; return 0; }
uint16_t am32BlEepromAddr() { return 0x7C00; }
uint8_t am32BlEscType() { return AM32_ESC_F051_1KB; }
const char *am32BlEscTypeName() { return "F051"; }
bool am32BlAddrDiv4() { return false; }

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
// Mirrors the two-line rule in esc_task.cpp: the command is refused while
// armed. esc_task.cpp cannot be linked here (PIO, UART), so this is a copy --
// but the thing under test is what the UI does with the answer, and that is
// the shipped code.
// The counter now increments only when the command is actually queued, as
// production does. Counting refused requests too meant a test asserting "the
// button sent the command" passed for a request the ESC never saw.
static int g_beepRequests = 0;
int  fakeBeepRequests() { return g_beepRequests; }
bool escRequestBeep(uint8_t) {
	if (g_armed) return false;
	g_beepRequests++;
	return true;
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
// beacon request and no KISS receiver. Core1 is not a thread here:
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
	             : escPumpSlot(&g_pump, &g_tel, g_armed, true, g_throttle, 0,
	                           false, ms);
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
void am32BlEnd() { g_am32EndBeforeResume = g_suspendReq; }
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
// platReboot() has no caller: every setting applies live. So this counts a
// thing that must never happen -- see testBoardRow().
// ---------------------------------------------------------------------------
static int g_reboots = 0;

int fakeRebootCount() { return g_reboots; }

extern "C" void watchdog_reboot(uint32_t, uint32_t, uint32_t) { g_reboots++; }

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

void fakeSdLogSet(const SdLogStatus *st) { g_log = *st; }

bool sdLogBegin() {
	g_log.state = SdLogState::Idle;
	return true;
}

static bool g_autoStarted = false;

bool sdLogStart() {
	if (g_log.state == SdLogState::NoCard) return false;
	g_log.state = SdLogState::Logging;
	g_autoStarted = false;
	if (!g_log.fileNumber) g_log.fileNumber = 1;
	return true;
}

void sdLogStop() {
	if (g_log.state == SdLogState::Logging) g_log.state = SdLogState::Idle;
	g_log.fileNumber = 0;
	g_autoStarted = false;
}

bool sdLogRemount() {
	// Models a card that appears on retry: NO CARD becomes READY. That is the
	// case the button exists for -- a card inserted after boot.
	if (g_log.state == SdLogState::NoCard) {
		g_log.state = SdLogState::Idle;
		g_log.mountResult = 0;
		g_log.cardType = 3;          // SDHC/XC
		g_log.cardSizeMB = 122000;   // a 128 GB card, as the card reports itself
		return true;
	}
	return g_log.state != SdLogState::Error;
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
void sdLogStatus(SdLogStatus *out) { *out = g_log; }

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

bool cardCacheBegin() {
	g_cacheCalls.begins++;
	if (sdLogActive()) sdLogStop();
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
	return MscRefusal::None;
}

void mscRelease() { g_msc = MscState::Idle; }
void mscTick() {}

void fakeMscSet(MscState s, uint32_t blocksRead) {
	g_msc = s;
	g_mscBlocks = blocksRead;
}
