/**
 * @file fakes.h
 * @brief Controls for the host-test fakes. @see fakes.cpp
 */

#pragma once

#include "usb_msc.h"

#include <stdint.h>

struct EscTelemetry;

/** @brief Advance the virtual clock. Time only moves when a test says so. */
void fakeAdvance(uint32_t ms);

struct SdLogStatus;

/**
 * @brief Force the fake logger into a given state. @param st What to report.
 */
void fakeSdLogSet(const SdLogStatus *st);

enum class CardCache : uint8_t;

/** @brief Calls the screens made of the fake write cache. */
struct FakeCacheCalls {
	unsigned begins, ends, flushes, recovers;
};

/**
 * @brief What one step of kstCardWaitPoll() runs: @p pass, or by default a
 *        millisecond and one core1JobPoll().
 */
void fakeCardWaitPass(void (*pass)());

/** @brief The fake cache off, Idle, no calls counted. */
void fakeCacheReset();

/** @brief Put the fake cache, while on, into @p st with @p left blocks to go. */
void fakeCacheSet(CardCache st, uint16_t left);

/** @brief What cardCacheFlush() ends in: Idle, or Failed. */
void fakeCacheFlushTo(CardCache st);

/** @brief The calls counted since fakeCacheReset(). */
FakeCacheCalls fakeCacheCalls();

/**
 * @brief What cardVolumeSerial() reads while a card is mounted: @p serial, or
 *        no answer when @p answers is false. fakeCacheReset() sets 1, answering.
 */
void fakeVolumeSet(uint32_t serial, bool answers = true);

/** @brief The next @p n reads of cardVolumeSerial() fail, whatever fakeVolumeSet() said. */
void fakeVolumeFailNext(unsigned n);

/** @brief Begin a touch at (@p x, @p y); sets the one-poll `pressed` edge. */
void fakePress(int x, int y);

/** @brief Continue a touch at (@p x, @p y) without a new `pressed` edge. */
void fakeHold(int x, int y);

/** @brief End the current touch; sets the one-poll `released` edge. */
void fakeRelease();

/** @brief Read a byte of the fake ESC's settings page, as actually stored. */
uint8_t fakeEscByte(int offset);

/** @brief Throttle most recently commanded to the fake DShot task, 0..2000. */
uint16_t fakeThrottle();

/** @brief Arm state of the fake DShot task. */
bool fakeArmed();

/**
 * @brief True while no suspend is requested: DShot has the pin or rebuilds
 *        on the next pass.
 */
bool fakePinReturned();

/** @brief The wiring escTaskConfigure() last set: what a rebuild binds. */
struct FakeWiring {
	uint8_t  dshotPin;    /**< ESC signal GPIO. */
	uint16_t dshotKbaud;  /**< DShot bitrate. */
	bool     kissEnable;  /**< KISS wire expected. */
	uint8_t  kissPin;     /**< KISS GPIO. */
};

/** @brief The fake pump's wiring at the time of the call. */
FakeWiring fakeWiring();

/**
 * @brief One pass of the fake DShot pump on core1: escTaskPoll()'s handover
 *        and one frame decision on the shipped rules.
 *
 * Not modelled: the PIO, the reply fold, the frame timing, the heartbeat
 * (uiAlive is true), the beacon request and the KISS receiver.
 */
void fakePumpPoll();

/** @brief EscPump::lastThrottleMs of the fake pump. */
uint32_t fakeLastThrottleMs();

/** @brief How many times escTaskResume() has been called. */
int fakeResumeCount();

/** @brief How many times the fake pump has bound. */
int fakeRebuildCount();

/** @brief The wiring the fake pump's last bind used. */
FakeWiring fakeResumeWiring();

/** @brief Whether the core1 job was idle at the last escTaskResume(). */
bool fakeResumeJobIdle();

/** @brief GPIO the AM32 transport was last handed by am32BlBegin(). */
uint8_t fakeAm32Pin();
/** @brief Forget the last am32BlBegin(): fakeAm32Pin() reads 255 until the next one. */
void fakeAm32Reset();

/** @brief Whether a suspend was still requested at the last am32BlEnd(). */
bool fakeAm32EndBeforeResume();

/** @brief Set the telemetry the fake ESC reports. */
void fakeSetTelemetry(const EscTelemetry *t);

/**
 * @brief FNV-1a over a rectangle of the framebuffer.
 *
 * For asserting that a region changed, or that two renders match, without
 * depending on where individual glyphs land.
 */
uint32_t fakeRegionHash(int x, int y, int w, int h);

/** @brief How many times the UI has asked for a beep. */
int fakeBeepRequests();

/**
 * @brief Count framebuffer pixels of an exact colour.
 *
 * Used to prove a screen left nothing of the previous one behind: the cyan of
 * the SETTINGS section captions is distinctive, so more than a trace of it
 * surviving on the tester screen is a leftover.
 */
int fakeCountColour(uint16_t c);

/** @brief Write the current framebuffer to a binary PPM, for eyeballing. */
void fakeDumpFrame(const char *name);

/** @brief Pole count most recently pushed to the fake pump. */
uint8_t fakePoles();

/** @brief Set what escEdtRequested() reports. */
void fakeSetEdtRequested(bool on);

/** @brief Backlight level most recently driven. */
uint8_t fakeBacklight();

/** @brief Lowest backlight level seen since fakeBacklightResetMin(). */
uint8_t fakeBacklightMin();

/** @brief Start watching for a new backlight minimum. */
void fakeBacklightResetMin();

/** @brief Erase the fake settings flash to 0xFF, as a blank part reads. */
void fakeFlashClear();

/** @brief Make the fake settings flash refuse writes, as a full image would. */
void fakeFlashSetWritable(bool on);

/** @brief Raw bytes of the fake settings flash, for corrupting on purpose. */
uint8_t *fakeFlashBytes();

/** @brief How many storage writes have been *attempted* since process start. */
int fakeFlashWrites();

/** @brief How many times platReboot() has fired. Asserted to stay put. */
int fakeRebootCount();

/** @brief How many times the UI has rebuilt the DShot pump's wiring. */
int fakeConfigureCount();

/** @brief DShot bitrate most recently pushed to the fake pump. */
uint16_t fakeDshotKbaud();

/** @brief GPIO most recently pushed to the fake pump. @see escTaskDshotPin() */
uint8_t escTaskDshotPin();

/** @brief GPIOs the PWM and GPIO fake records. */
#define FAKE_GPIOS  30
/** @brief Slices the PWM fake records, for GP0 to GP29. */
#define FAKE_SLICES 8

/** @brief Which pull a pin has, as the GPIO fake last set it. */
enum class FakePull : uint8_t { Untouched, None, Down };

/** @brief What the fake recorded for one GPIO. */
struct FakePin {
	int      function;     /**< gpio_function last set; 0 for none. */
	uint16_t level;        /**< Compare level last set, or 0 after pwm_init(). */
	int      levelWrites;  /**< pwm_set_gpio_level() calls. */
	bool     output;       /**< gpio_set_dir() last set an output. */
	FakePull pull;         /**< Pull last set. */
	bool     high;         /**< gpio_put() last drove it high. */
	bool     inputOff;     /**< gpio_set_input_enabled() last disabled its input. */
	int      puts;         /**< gpio_put() calls. */
	int      highEnables;  /**< gpio_set_dir() made it an output while gpio_put() held it high. */
	int      cutPulses;    /**< Pulses cut short or left high by a stop or a mux change. */
	uint32_t cutAtUs;      /**< How far into the pulse the last cut came, us. */
};

/** @brief What the fake recorded for one PWM slice. */
struct FakeSlice {
	bool     enabled;  /**< Counting. */
	uint16_t top;      /**< Wrap register. */
	uint32_t div16;    /**< Clock divider, in 1/16ths. */
	int      inits;    /**< pwm_init() calls. */
	uint64_t startUs;  /**< time_us_64() when pwm_init() started its counter. */
};

/**
 * @brief The compare level a pin's slice latched for the frame under way at
 *        the virtual time: the last level written before that frame began.
 *
 * The counter runs at 1 MHz from pwm_init(), one frame of top + 1 counts. A
 * level written during a frame takes effect at the wrap that ends it, and a
 * level written exactly at a frame's start takes effect a frame later. The
 * pin is high while the counter is below the latched level.
 *
 * @param gpio GPIO.
 * @return The latched level; 0 for a pin whose slice is not counting.
 */
uint16_t fakePwmLatched(unsigned gpio);

/** @brief Clear every pin and slice record and the broken map. */
void fakePwmReset();

/**
 * @brief Make pwm_gpio_to_slice_num() name the wrong slice, as an SDK whose
 *        mapping disagreed with out_pwm_map.h would.
 * @param on True to break it.
 */
void fakePwmBreakSliceMap(bool on);

/** @brief The record of one GPIO. @param gpio GPIO. @return The record. */
FakePin fakePin(unsigned gpio);

/** @brief The record of one slice. @param slice Slice. @return The record. */
FakeSlice fakeSlice(unsigned slice);

/** @brief Put the fake USB card reader into a state. @see usb_msc.h */
void fakeMscSet(MscState s, uint32_t blocksRead);
