/**
 * @file ui.h
 * @brief Touchscreen UI for the DShot motor tester.
 *
 * Owns the arm/throttle state machine and all rendering. Runs entirely on
 * core0 and pushes commands to core1 through the @ref esc_core0 API.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Reset UI state, seed the ADC, and clear the screen.
 * @pre st7789Init() and touchInit() must already have run.
 */
void uiInit();

/**
 * @brief Draw the boot splash into the framebuffer.
 *
 * Separate from setup() so the host tests render the real thing rather than a
 * copy — the documentation screenshots and the width checks would otherwise
 * drift away from what the board actually shows.
 */
void uiDrawSplash();

/**
 * @brief Run one UI frame.
 *
 * Sends the heartbeat, samples touch and battery, updates the arm and throttle
 * state machines, repaints whatever changed, and flushes it to the panel. Call
 * at a steady rate; the firmware uses ~40 Hz.
 */
void uiTick();

/** @brief Current commanded throttle, 0..2000. @return Throttle value. */
uint16_t uiThrottle();

/** @brief Current arm state. @return true when armed. */
bool     uiArmed();

/** @brief The tester's controls. @see ui_screen.h */
enum class TesterCtl : uint8_t {
	Arm = 1,   /**< HOLD TO ARM, or DISARM. Acts while no USB host holds the card. */
	Hold,      /**< HOLD. */
	Cfg,       /**< CFG. */
	Throttle,  /**< The throttle gauge's track, as drawn; it takes a press 8 px beyond each side. Acts while armed. */
	Pad,       /**< The relative throttle pad over the readouts. Acts while armed. */
};

/** @brief The SETTINGS controls. @see ui_screen.h */
enum class CfgCtl : uint8_t {
	PolesMinus = 1, PolesPlus,  /**< MOTOR POLES - and +. */
	MaxMinus, MaxPlus,          /**< THROTTLE CEILING - and +. */
	Beep,                       /**< BEEP. */
	SdLog,                      /**< SD LOG. */
	Am32,                       /**< AM32. */
	Servo,                      /**< SERVO. */
	Kst,                        /**< KST, drawn disabled while it is not built. */
	Setup,                      /**< SETUP. */
};

/** @brief The SD LOG controls. @see ui_screen.h */
enum class SdLogCtl : uint8_t {
	Toggle = 1,  /**< START or STOP; EJECT while a host holds the card. */
	Retry,       /**< RETRY, absent while a host holds the card. */
	Usb,         /**< USB DRIVE, absent while a host holds the card. */
};
