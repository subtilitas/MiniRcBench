/**
 * @file main.cpp
 * @brief Top level: core0 and core1 entry points.
 */

/**
 * @page architecture Architecture
 *
 * README.md is the front page (`USE_MDFILE_AS_MAINPAGE`), so this is a
 * \@page rather than a second \@mainpage — Doxygen treats two main pages as a
 * conflict and silently drops one. Note the backslashes: naming those commands
 * unescaped in prose makes Doxygen execute them instead of printing them,
 * which is itself how this block first went wrong.
 *
 * A self-contained bidirectional DShot ESC tester for the Waveshare
 * RP2350-Touch-LCD-2.8. Drag the on-screen throttle and the board sends
 * bidirectional DShot to a single ESC while decoding the eRPM and Extended
 * DShot Telemetry that comes back on the same wire.
 *
 * The image is built for that one board. It does not identify the board it
 * runs on: from the first instruction of setup() it drives the 2.8" pin map,
 * starting with the power latch on GP26. See board_desc.h.
 *
 * @section arch Two cores
 *
 * Bidirectional DShot is half-duplex: the ESC only answers a frame that *you*
 * sent, and it disarms if frames stop arriving. So a telemetry display cannot
 * be a passive sniffer — it has to be the throttle source.
 *
 * Frame timing is the whole game, and the display does multi-millisecond SPI
 * DMA bursts that would wreck it if the two shared a core:
 *
 * | | core0 | core1 |
 * |---|---|---|
 * | Job | touch, rendering, battery sense | DShot frame pump and telemetry decode while ESC TESTER or ESC COMMANDS is open, else SIG parked low; the job of a pin claim |
 * | Rate | ~40 Hz | 1 kHz (@ref DSHOT_PERIOD_US) |
 * | Allowed to block | yes, freely | never |
 *
 * The two share one small critical-section-guarded struct. See @ref esc_task.h.
 *
 * @section modules Modules
 *
 * All of these live in `src/`:
 *
 * - @ref config.h — every tunable setting
 * - @ref board_pins.h — the board's pin map, from the schematic
 * - @ref esc_task.h — core1 DShot pump and EDT decode
 * - @ref pin_claim.h — a screen takes the two signal pins from DShot
 * - @ref core1_job.h — core1 work on the claimed pins
 * - @ref usb_bridge.h — the USB bridge's side of the serial port (core0)
 * - @ref servo_job.h — the servo tester's core1 job: PWM pulses, slew, sweep
 * - @ref servo_model.h — the servo tester's rules, pure
 * - @ref ui_servo.h — the servo tester screens
 * - @ref ui.h — screens, touch handling, arm and throttle state machines
 * - @ref gfx.h — RGB565 framebuffer, dirty bands, fonts
 * - @ref st7789.h — panel init and DMA blitter
 * - @ref touch.h — capacitive touch, CST328
 * - @ref sd_log.h — blackbox logging to the microSD slot (core0)
 * - @ref diag.h — the restart reason and the uptime, shown on SETUP
 *
 * @warning This drives a real motor. Read the safety section of README.md
 *          before connecting anything with a propeller on it.
 */

#include "plat.h"
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <stdio.h>

#include "config.h"
#include "board_desc.h"
#include "gfx.h"
#include "st7789.h"
#include "touch.h"
#include "sense.h"
#include "clock.h"
#include "esc_task.h"
#include "core1_job.h"
#include "usb_bridge.h"
#include "ui.h"
#include "sd_log.h"
#include "usb_dev.h"
#include "usb_msc.h"
#include "card_cache.h"
#include "servo_log.h"
#include "settings.h"
#include "servo_supply.h"
#include "diag.h"

#if BOOT_DIAG
#include "boot_diag.h"
/** @brief Blink step @p n on GP16 before it runs. @see boot_diag.h */
#define BOOT_STEP(n) bootDiagStep(n)
#else
/** @brief Nothing: the boot diagnostic is not built. @see boot_diag.h */
#define BOOT_STEP(n) ((void)0)
#endif

/** @brief UI frame interval in milliseconds (~40 fps). */
#define UI_PERIOD_MS 25
static_assert(TOUCH_LOST_MS == 4 * UI_PERIOD_MS,
              "TOUCH_LOST_MS is documented as four UI frames");

static uint32_t s_nextUiMs = 0;   /**< Deadline for the next UI frame. */
#if SERIAL_TELEMETRY
static uint32_t s_nextLogMs = 0;  /**< Deadline for the next serial dump. */
#endif

/**
 * @brief Core0 setup: bring up the display, touch and UI.
 */
void setup() {
	// First thing, before anything that could take a millisecond. GP26 is the
	// latch that keeps VBAT connected once the power button is released; miss
	// it and the board dies mid-boot on battery.
	gpio_init(g_board->batEnPin);
	gpio_set_dir(g_board->batEnPin, GPIO_OUT);
	gpio_put(g_board->batEnPin, 1);

	// The restart reason, read once: three register reads, before anything
	// else touches the watchdog. SETUP shows it under DIAGNOSTICS. @see diag.h
	BOOT_STEP(1);
	diagCaptureRestart();

	// USB, ours rather than the SDK's. stdio_init_all() would have brought up
	// pico_stdio_usb, which owns the USB descriptors and cannot share the device
	// with a second interface -- and the card reader is a second interface.
	// usbDevInit() puts the serial port back on our own descriptors.
	// @see usb_dev.h
	BOOT_STEP(2);
	usbDevInit();

	// Before escTaskInit(), which seeds core1's wiring from the settings, and
	// before uiInit(), which reads the palette and backlight preference. A
	// board with blank flash gets the compiled defaults; a block written on
	// other hardware arrives with every pin in it re-judged against this
	// board. @see settingsValidate()
	BOOT_STEP(3);
	settingsLoad();
	// Directly after the settings and before core1 starts: with a switch
	// fitted, its enable pin becomes an undriven input, and the converter's external
	// pull-down holds the servo's supply off. @see servo_supply.h
	BOOT_STEP(4);
	servoSupplyInit();

	// Before anything can call escSnapshot(), and before core1 is launched.
	// It parks SIG low and AUX on its pull-down on this core.
	BOOT_STEP(5);
	escTaskInit();
	BOOT_STEP(6);
	core1JobInit();

	BOOT_STEP(7);
	gfxInit();
	BOOT_STEP(8);
	st7789Init();
	// Pulses the touch controller's own reset line. Its result starts the
	// driver's record of answers, which the splash and the tester read:
	// TOUCH CONTROLLER NOT FOUND on the splash, TOUCH NOT ANSWERING on the
	// tester. @see touchAnswered(), uiTouchLost()
	BOOT_STEP(9);
	touchInit();
	BOOT_STEP(10);
	senseInit();        // the current sensor, on the bus touchInit() set up
	// The real-time clock, read once on the same bus, after senseInit() has
	// started the bus record its transfers are noted to. @see clock.h
	BOOT_STEP(11);
	clockInit();
	BOOT_STEP(12);
	uiInit();

	// The splash. Core1 is not running yet: main() launches it after
	// setup() returns. escTaskInit() parked SIG low: no DShot frame leaves
	// the board until ESC TESTER or ESC COMMANDS opens
	// (docs/design/dshot-lifetime.md).
	// The touch line follows the controller while the splash is up: a
	// controller that answers late clears it.
	uiDrawSplash();
	st7789FlushDirty();
	for (uint32_t t0 = millis(); (uint32_t)(millis() - t0) < SPLASH_MS;) {
		delay(UI_PERIOD_MS);
		uiSplashTick();
		st7789FlushDirty();
	}
	gfxFill(C_BG);
	st7789FlushDirty();

	// Paint one real frame before touching the card. Mounting talks to hardware
	// that may not be there, and however well it behaves it is the last thing
	// that runs before the screen would otherwise sit blank. If it ever does
	// stall, a frozen UI says far more than a black panel does.
	uiTick();

#if SD_LOG_ENABLE
	// A missing card is the normal bench case, not a fault: logging is
	// simply unavailable and the UI says so.
	sdLogBegin();
	// After the mount, so the reader starts from a known card. It reports no
	// medium until asked, so nothing about this makes a drive appear yet.
	mscInit();
#endif

	s_nextUiMs = millis();
#if BOOT_DIAG
	bootDiagDone();
#endif
}

/**
 * @brief Core0 loop: UI, SD logging, and the optional serial dump.
 */
void loop() {
	// First, and every pass. This is what moves USB data, so a host reading the
	// card depends on it being called often; TinyUSB does bounded work and
	// returns, so calling it from the top of the loop costs nothing when idle.
	usbDevTask();

	// The USB bridge's side of the port, right after TinyUSB has taken what
	// arrived: every pass, not once a UI frame, so a frame from the PC goes
	// to the wire 2 ms after its last byte. Nothing while the bridge does not
	// hold the claim. @see usb_bridge.h
	usbBridgeTask();

	uint32_t now = millis();
	if ((int32_t)(now - s_nextUiMs) >= 0) {
		s_nextUiMs = now + UI_PERIOD_MS;
		uiTick();
	}

#if SD_LOG_ENABLE
	// Drive the handover, which does the unmounting and remounting one step per
	// call rather than inside the touch handler that asked for it.
	mscTick();

	// Encoding is cheap and only fills a RAM ring, so it runs every pass to
	// keep the log cadence even. uiTick() reports arm changes itself.
	// Both of these are inert while a host owns the card: sd_log refuses every
	// entry point once released. @see sdLogRelease()
	sdLogTick(micros(), uiThrottle());

	// Writing is the expensive half -- a card can pause tens of milliseconds
	// for an internal erase -- so it is a separate call, made once per pass
	// after the UI has already had its turn. Never on core1: a stall there
	// would break DShot timing outright.
	sdLogFlush();

	// The write cache of a backup claim: one block started or one look at
	// the block in flight per pass, never a wait on the card's busy line.
	// Nothing while the cache is off. @see card_cache.h
	cardCachePoll();

	// The servo tester's log, after the poll: its one card call of a pass
	// is made only while nothing is queued or in flight, so a pass reads
	// the card at most once, for this call or for the poll's batch start.
	// Nothing while no servo log is open. @see servo_log.h
	servoLogPoll();
#endif

#if SERIAL_TELEMETRY 
	if ((int32_t)(millis() - s_nextLogMs) >= 0) {
		s_nextLogMs = millis() + 100;
		EscTelemetry t;
		escSnapshot(&t);
		// uint32_t is `unsigned long` on arm-none-eabi, so the packet counters
		// are cast rather than printed with %u -- the Arduino core's printf was
		// lenient about this and newlib's is not.
		printf("arm=%d thr=%4u rpm=%6lu erpm=%6lu %5.2fV %5.1fA %3dC "
		       "stress=%3u st=0x%02X ok=%lu bad=%lu none=%lu rate=%u err=%u%%\n",
		       uiArmed() ? 1 : 0, uiThrottle(),
		       (unsigned long)t.rpm, (unsigned long)t.erpm,
		       (double)t.volts, (double)t.amps, t.tempC, t.stress, t.statusRaw,
		       (unsigned long)t.goodPackets, (unsigned long)t.badPackets,
		       (unsigned long)t.noPackets,
		       t.packetRate, t.errPercent);
	}
#endif
}

/**
 * @brief Core1 setup: opt in to being parked, then start the frame pump's
 *        loop. The pump binds no pin here: escTaskInit() asked for Park, and
 *        the first bind waits for ESC TESTER or ESC COMMANDS
 *        (pinClaimDshot()).
 *
 * `multicore_lockout_victim_init()` is what lets core0 write flash at all.
 * Erasing a sector turns XIP off, and core1 executes from XIP, so core0 has to
 * be able to hold it still first -- without this the settings save would fault
 * core1 rather than fail, and it would do so only on the boards where somebody
 * had actually pressed the button.
 */
void setup1() {
	multicore_lockout_victim_init();
	escTaskBegin();
}

/**
 * @brief Core1 loop: the DShot frame pump, or the job on the claimed pins.
 *
 * escTaskPoll() returns at once while the pins are parked or a pin claim
 * holds them, and core1JobPoll() returns at once while no job runs, so at
 * most one of the two does the work on any pass. Deliberately kept free of
 * anything that can block, so frame spacing stays within tolerance
 * regardless of what core0 is doing.
 */
void loop1() {
	escTaskPoll();
	core1JobPoll();
}

/**
 * @brief Core1 entry point.
 *
 * The Arduino core used to call setup1()/loop1() for us. Doing it explicitly is
 * the whole point of the port: nothing runs on core1 that is not written here.
 */
static void core1Main() {
	setup1();
	for (;;) loop1();
}

/**
 * @brief Core1's stack, PICO_CORE1_STACK_SIZE bytes in main SRAM. With the
 *        default launch, core1's stack is the top of SCRATCH_X, directly
 *        under core0's 4 KB in SCRATCH_Y, and core0's deepest path, a Hitec
 *        card write under the frame, comes within about 300 bytes of it. Here
 *        core0's stack has all of SCRATCH_Y and SCRATCH_X, 8 KB, and an
 *        overflow of it no longer reaches core1's stack.
 */
static uint32_t s_core1Stack[PICO_CORE1_STACK_SIZE / sizeof(uint32_t)];

/**
 * @brief Core0 entry point.
 *
 * Order matters, in two directions.
 *
 * Core1 is launched only after core0 has brought up the display, because
 * core1 claims PIO state machines and st7789Init() claims a DMA
 * channel; starting them concurrently makes which one wins a matter of timing.
 *
 * But core0 must not *use* anything core1 initialises before core1 has run, and
 * it very nearly does: the first loop() calls uiTick(), which calls
 * escSnapshot(), which takes a critical section. So that critical section is
 * initialised by escTaskInit() on core0 inside setup(), not by escTaskBegin()
 * on core1. Getting this wrong does not race intermittently — core0 wins every
 * time, and the board shows the splash and then a black screen forever.
 *
 * @return Never returns.
 */
int main(void) {
	setup();
	multicore_launch_core1_with_stack(core1Main, s_core1Stack, sizeof(s_core1Stack));
	for (;;) loop();
}