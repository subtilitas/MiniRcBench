/**
 * @file board_desc.h
 * @brief The board as data: the pins and defaults the firmware reads at run time.
 *
 * MiniRcBench runs on one board, the Waveshare RP2350-Touch-LCD-2.8, and
 * @ref board_rp2350_touch_lcd_2_8.h is its pin map. This header carries the
 * part of that map the drivers, the UI and the settings rules read, as one
 * constant struct behind @ref g_board. The panel driver, the touch driver, the
 * battery readout and the pin rules all read their pins from it.
 *
 * The SD card wiring is not here: sd_hw_config.c takes it from the board
 * header directly, because the SD library reads its configuration from static
 * structs at link time.
 *
 * @section board_desc_scope What the image checks
 *
 * Nothing. The image does not identify the board it runs on. It assumes the
 * 2.8" and drives that board's pin map from the first instruction of setup().
 * Flashed onto any other RP2350 board, it drives the same pins, whatever they
 * are wired to there. @see main.cpp
 */

#pragma once

#include <stdint.h>

/**
 * @brief Everything this firmware needs to know about the board.
 *
 * Deliberately not a superset of the board header. Peripherals the firmware
 * does not drive — the IMU, the RTC, the audio codec — stay documented in the
 * header as pins not to reuse, and do not appear here: a descriptor field
 * nothing reads is a field that goes stale without anyone noticing.
 */
struct BoardDesc {
	/** @brief Short name, shown on the splash and on SETTINGS. */
	const char *label;

	/**
	 * @name Panel
	 * @{
	 */
	void    *lcdSpi;    /**< `spi_inst_t *`, opaque here to keep the SDK out. */
	uint8_t  lcdBl;     /**< Backlight, PWM. */
	uint8_t  lcdDc;     /**< Data / command select. */
	uint8_t  lcdCs;     /**< Chip select. */
	uint8_t  lcdSck;    /**< Clock. */
	uint8_t  lcdMosi;   /**< Data out. */
	uint8_t  lcdRst;    /**< Reset. */
	/** @} */

	/**
	 * @name Touch
	 * @{
	 */
	void    *i2c;       /**< `i2c_inst_t *`. */
	uint8_t  sda;       /**< I2C data. */
	uint8_t  scl;       /**< I2C clock. */
	uint8_t  tpInt;     /**< Touch interrupt, active low. */
	uint8_t  tpRst;     /**< Touch reset, a net of its own. */
	uint8_t  tpAddr;    /**< 7-bit I2C address. */
	/** @} */

	/**
	 * @name Battery
	 * @{
	 */
	uint8_t  batAdcPin;  /**< Divider midpoint. */
	uint8_t  batAdcChan; /**< ADC input index, which is not the GPIO number. */
	float    batDivider; /**< VBAT = Vadc * this. */
	/**
	 * @brief Power latch.
	 *
	 * The board switches itself off on battery when the power button is
	 * released, unless this pin is driven high first. @see setup() in main.cpp
	 */
	uint8_t  batEnPin;
	/** @} */

	/**
	 * @brief GPIOs free for the ESC or telemetry wire, as a bitmask.
	 *
	 * The thing SETTINGS steps through. Everything outside it drives an
	 * on-board peripheral. @see settingsPinFree()
	 */
	uint32_t freeGpioMask;

	/** @brief ESC pin a board with blank flash starts on. */
	uint8_t  defaultDshotPin;
	/** @brief KISS pin a board with blank flash starts on. */
	uint8_t  defaultKissPin;
	/** @brief Whether a board with blank flash expects a KISS wire. */
	bool     defaultKissEnable;
};

/**
 * @brief The board id the settings block records.
 *
 * On-flash format, shared with DshotDisplay, which writes the same value on
 * the same board. @see Settings::boardId
 */
#define BOARD_ID_LCD_2_8  28

/** @brief Descriptor for the RP2350-Touch-LCD-2.8. Built by board_desc.cpp. */
extern const struct BoardDesc BOARD_DESC_LCD_2_8;

/**
 * @brief The board this firmware drives: always @ref BOARD_DESC_LCD_2_8.
 *
 * Never null, and set before any code runs: it is a constant.
 */
extern const struct BoardDesc *const g_board;
