/**
 * @file touch.h
 * @brief Capacitive touch interface.
 *
 * Implemented by `cst328.cpp`, the driver for the CST328 controller of the
 * RP2350-Touch-LCD-2.8: I2C address 0x1A, 16-bit registers. The host test
 * suite links a scripted fake in its place.
 *
 * The panel reports coordinates in a native 240x320 portrait frame. The driver
 * rotates them to match @ref LCD_ROTATION, so UI code only ever sees
 * framebuffer coordinates.
 */

#pragma once

#include <stdint.h>

#include "config.h"

/**
 * @brief The shared I2C1 bus's clock, Hz: 400 kHz, fast mode. touchInit()
 *        starts the bus at it, and the current sensor's recovery (sense.h)
 *        restarts it at it.
 */
#define TOUCH_I2C_HZ (400u * 1000u)

/** @brief Native panel width, in portrait, independent of @ref LCD_ROTATION. */
#define TOUCH_NATIVE_W 240
/** @brief Native panel height, in portrait, independent of @ref LCD_ROTATION. */
#define TOUCH_NATIVE_H 320

/**
 * @brief Map native portrait coordinates into the active framebuffer frame.
 *
 * Lives here rather than in the driver so that the rotation is one inline
 * function the host suite can call without the driver.
 *
 * @param      nx,ny Native panel coordinates.
 * @param[out] ox,oy Framebuffer coordinates for the current @ref LCD_ROTATION.
 */
static inline void touchMapCoords(int16_t nx, int16_t ny, int16_t *ox, int16_t *oy) {
#if LCD_ROTATION == 1
	*ox = ny;
	*oy = (int16_t)(TOUCH_NATIVE_W - 1 - nx);
#elif LCD_ROTATION == 2
	*ox = (int16_t)(TOUCH_NATIVE_W - 1 - nx);
	*oy = (int16_t)(TOUCH_NATIVE_H - 1 - ny);
#elif LCD_ROTATION == 3
	*ox = (int16_t)(TOUCH_NATIVE_H - 1 - ny);
	*oy = nx;
#else
	*ox = nx;
	*oy = ny;
#endif
}

/**
 * @brief Snapshot of the touch panel for one poll.
 *
 * Edge flags are computed against the previous poll, so a caller that polls at
 * a fixed rate sees exactly one `pressed` and one `released` per contact.
 */
struct TouchState {
	bool     down;      /**< Finger currently on the glass. */
	bool     pressed;   /**< Rising edge: contact started this poll. */
	bool     released;  /**< Falling edge: contact ended this poll. */
	int16_t  x;         /**< Current X, framebuffer coordinates. */
	int16_t  y;         /**< Current Y, framebuffer coordinates. */
	int16_t  downX;     /**< X where the current press started. */
	int16_t  downY;     /**< Y where the current press started. */
	uint8_t  gesture;   /**< Controller gesture id, 0 if it reports none. */
};

/**
 * @brief Start the touch bus and configure the controller.
 *
 * Called after st7789Init(). The two chips have separate reset lines, and
 * this driver pulses its own, so the order is a convention rather than a
 * requirement.
 *
 * @return true if the controller answered the presence check with the
 *         0xCACA ID. Only that counts as an answer in touchLastAnswer()'s
 *         record; a probe that reads another ID records none.
 */
bool touchInit();

/**
 * @brief Whether the controller has answered since touchInit(), and when it
 *        last did.
 *
 * An answer is the presence check in touchInit() returning the 0xCACA ID,
 * or a finger read in touchPoll() that completes. The time is millis() at
 * that read. The splash reads it (uiSplashTick()).
 *
 * This is the touch controller's own record. The shared bus's record in
 * sense.h counts timeouts and held lines for the stuck decision and not a
 * NACK (not acknowledged), which is what a controller off the bus, a loose
 * flex for instance, returns; so it cannot say whether the controller
 * answers.
 *
 * @param[out] ms millis() at the last answer; untouched when there was none.
 * @return false while the controller has not answered once since
 *         touchInit().
 */
bool touchLastAnswer(uint32_t *ms);

/**
 * @brief Poll the controller and update @p t.
 *
 * Safe to call at any rate; ~60 Hz is plenty. A single failed I2C transaction
 * holds the previous contact state with the position unchanged — a one-poll
 * glitch mid-press must not synthesise a release, because taps fire on
 * release and a synthetic release *commits* whatever the finger is resting
 * on. Only consecutive failures report the finger up.
 *
 * @param[in,out] t State to update. Must persist between calls.
 */
void touchPoll(TouchState *t);
