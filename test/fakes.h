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

/** @brief Force the fake logger into a given state. @param st What to report. */
void fakeSdLogSet(const SdLogStatus *st);

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
 * the settings screen's nav buttons is distinctive, so more than a trace of it
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

/** @brief Put the fake USB card reader into a state. @see usb_msc.h */
void fakeMscSet(MscState s, uint32_t blocksRead);
