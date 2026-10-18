/**
 * @file board_pins.h
 * @brief Pin map of the board this firmware is built for.
 *
 * MiniRcBench targets one board, the Waveshare RP2350-Touch-LCD-2.8, and this
 * file includes its header and does nothing else. The pin numbers live in
 * @ref board_rp2350_touch_lcd_2_8.h.
 *
 * @section board_contract What the board header defines
 *
 * There is no fallback: a missing macro is a compile error at the point of
 * use, which is the intended behaviour — silently defaulting a pin number is
 * how you drive an ESC signal out of a backlight transistor.
 *
 * | Macro | Meaning |
 * |---|---|
 * | `BOARD_LABEL` | Short board name, for the splash and SETTINGS |
 * | `BOARD_FREE_GPIO_MASK` | GPIOs the ESC and telemetry wires may use |
 * | `PIN_LCD_BL`, `PIN_LCD_DC`, `PIN_LCD_CS` | Panel control lines |
 * | `PIN_LCD_SCK`, `PIN_LCD_MOSI`, `PIN_LCD_RST` | Panel data and reset |
 * | `LCD_SPI_PORT` | Pico SDK SPI instance the panel hangs off |
 * | `BOARD_I2C` | Pico SDK I2C instance for the touch bus |
 * | `PIN_I2C_SDA`, `PIN_I2C_SCL` | Touch bus pins |
 * | `PIN_TP_INT`, `PIN_TP_RST` | Touch interrupt and reset |
 * | `CST328_I2C_ADDR` | Touch controller address |
 * | `PIN_BAT_ADC`, `BAT_ADC_CHAN`, `BAT_DIVIDER` | Battery sense |
 * | `PIN_BAT_EN` | Battery power latch |
 * | `PIN_SD_SCK`, `PIN_SD_CMD`, `PIN_SD_D0` .. `PIN_SD_D3` | microSD, 4-bit SDIO |
 *
 * Anything else the board header defines — IMU, RTC, audio — is documentation
 * of pins this firmware must not reuse, not an interface.
 *
 * @warning **RP2350-E9.** A pad configured as an input with the internal
 *          pull-down engaged can latch at roughly 2.1 V instead of being pulled
 *          to ground, and reads high from then on. Do not use internal
 *          pull-downs for level detection anywhere in this firmware — drive the
 *          pad and read it back instead. A diagnostic in tools/sdtest did use
 *          one, and duly reported six good SDIO lines as "held high".
 */

#pragma once

#include "board_rp2350_touch_lcd_2_8.h"
