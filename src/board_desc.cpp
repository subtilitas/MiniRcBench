/**
 * @file board_desc.cpp
 * @brief The RP2350-Touch-LCD-2.8, as a value. @see board_desc.h
 */

#include "board_desc.h"
#include "board_pins.h"
#include "config.h"

#include <hardware/spi.h>
#include <hardware/i2c.h>

const BoardDesc BOARD_DESC_LCD_2_8 = {
	/* label      */ BOARD_LABEL,

	/* lcdSpi     */ (void *)LCD_SPI_PORT,
	/* lcdBl      */ PIN_LCD_BL,
	/* lcdDc      */ PIN_LCD_DC,
	/* lcdCs      */ PIN_LCD_CS,
	/* lcdSck     */ PIN_LCD_SCK,
	/* lcdMosi    */ PIN_LCD_MOSI,
	/* lcdRst     */ PIN_LCD_RST,

	/* i2c        */ (void *)BOARD_I2C,
	/* sda        */ PIN_I2C_SDA,
	/* scl        */ PIN_I2C_SCL,
	/* tpInt      */ PIN_TP_INT,
	/* tpRst      */ PIN_TP_RST,
	/* tpAddr     */ CST328_I2C_ADDR,

	/* batAdcPin  */ PIN_BAT_ADC,
	/* batAdcChan */ BAT_ADC_CHAN,
	/* batDivider */ BAT_DIVIDER,
	/* batEnPin   */ PIN_BAT_EN,

	/* freeGpio   */ BOARD_FREE_GPIO_MASK,
	/* dshotPin   */ DSHOT_PIN_LCD_2_8,
	/* kissPin    */ KISS_PIN_LCD_2_8,
	/* kissEnable */ KISS_ENABLE_LCD_2_8 != 0,
};

extern const BoardDesc *const g_board;
const BoardDesc *const g_board = &BOARD_DESC_LCD_2_8;

/*
 * The pin defaults are macros, so they are overridable from the build -- and an
 * override that cannot work must fail here rather than be repaired in silence
 * on the bench. settingsValidate() would quietly move an occupied ESC pin back
 * to the board default and switch a colliding KISS wire off, which is right for
 * a stored block nobody chose and wrong for a value someone typed on a command
 * line. @see cfg_pin_defaults
 *
 * The `& 31` keeps the shift in range when the pin is nonsense, so the message
 * that fires is this one rather than the compiler's own about the shift.
 */
static_assert(DSHOT_PIN_LCD_2_8 <= 29 &&
              ((BOARD_FREE_GPIO_MASK >> (DSHOT_PIN_LCD_2_8 & 31)) & 1u),
              "DSHOT_PIN_LCD_2_8 is not a free GPIO on the 2.8-inch board");
static_assert(KISS_PIN_LCD_2_8 <= 29 &&
              ((BOARD_FREE_GPIO_MASK >> (KISS_PIN_LCD_2_8 & 31)) & 1u),
              "KISS_PIN_LCD_2_8 is not a free GPIO on the 2.8-inch board");
static_assert(DSHOT_PIN_LCD_2_8 != KISS_PIN_LCD_2_8,
              "the ESC and telemetry wires cannot share a pin");
