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

/*
 * The servo tester drives the two free pins from the RP2350's PWM slices. For
 * GP0 to GP31 the slice is (gpio >> 1) & 7 and the channel is gpio & 1, A for
 * an even GPIO and B for an odd one (PWM_GPIO_SLICE_NUM in Pico SDK 2.1.1).
 * Three rules follow, and a board whose free pins break one fails here.
 * @see docs/design/pin-claim.md
 *
 * - No free pin is on the backlight's slice: pwm_init() on that slice would
 *   retime the backlight. GP16 is slice 0, with GP0, GP1 and GP17.
 * - All free pins are on one slice. A slice has one wrap register, so two
 *   servos run at one frame rate.
 * - At most one free pin per channel of that slice. A channel is one compare
 *   register, and GP12 and GP28, say, share slice 6 channel A. With one even
 *   and one odd free pin, any two different free pins are the slice's two
 *   channels: two pulse widths.
 */
/** @brief PWM slice of a GPIO below 32. */
static constexpr unsigned pwmSliceOf(unsigned gpio) { return (gpio >> 1) & 7u; }

/** @brief True if no GPIO in @p mask is on PWM slice @p slice. */
static constexpr bool maskClearOfSlice(uint32_t mask, unsigned slice) {
	for (unsigned g = 0; g < 32; g++)
		if (((mask >> g) & 1u) && pwmSliceOf(g) == slice) return false;
	return true;
}

/** @brief True if every GPIO in @p mask is on PWM slice @p slice. */
static constexpr bool maskOnSlice(uint32_t mask, unsigned slice) {
	for (unsigned g = 0; g < 32; g++)
		if (((mask >> g) & 1u) && pwmSliceOf(g) != slice) return false;
	return true;
}

/** @brief How many GPIOs in @p mask are on PWM channel @p chan: 0 is A, 1 is B. */
static constexpr unsigned maskOnChannel(uint32_t mask, unsigned chan) {
	unsigned n = 0;
	for (unsigned g = 0; g < 32; g++)
		if (((mask >> g) & 1u) && (g & 1u) == chan) n++;
	return n;
}

static_assert(maskClearOfSlice(BOARD_FREE_GPIO_MASK, pwmSliceOf(PIN_LCD_BL)),
              "a free GPIO is on the backlight's PWM slice");
static_assert(maskOnSlice(BOARD_FREE_GPIO_MASK, pwmSliceOf(DSHOT_PIN_LCD_2_8)),
              "the free GPIOs are not on one PWM slice");
static_assert(maskOnChannel(BOARD_FREE_GPIO_MASK, 0) <= 1 &&
              maskOnChannel(BOARD_FREE_GPIO_MASK, 1) <= 1,
              "two free GPIOs share a PWM channel");
