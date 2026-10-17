/**
 * @file sd_hw_config.c
 * @brief Board wiring for the FatFs SD driver.
 *
 * carlk3's no-OS-FatFS-SD library looks these up by name at link time --
 * there is no registration call -- so this file has to exist and has to be
 * named exactly this.
 *
 * The RP2350-Touch-LCD-2.8 brings out all six SDIO (Secure Digital
 * Input/Output) lines and cannot use hardware SPI (Serial Peripheral Interface)
 * for the card at all: its SD_SCK is GP19, which is SPI0 **TX** in the pin
 * mux, not SCK. So the one slot is configured for the PIO SDIO driver in
 * src/sdio/, four bits wide, from the pin macros in the board header.
 *
 * C rather than C++ because the library declares these with C linkage.
 */

#include "hw_config.h"

#include "board_pins.h"
#include "config.h"

/*
 * The rule the SDIO PIO programs encode and cannot check themselves: one
 * instruction moves DAT0..DAT3, so they are consecutive. CLK and CMD can be any
 * GPIO below 32, apart from those four. @see src/sdio/sd_pio.pio
 */
_Static_assert(PIN_SD_D1 == PIN_SD_D0 + 1, "SDIO D1 must be D0 + 1");
_Static_assert(PIN_SD_D2 == PIN_SD_D0 + 2, "SDIO D2 must be D0 + 2");
_Static_assert(PIN_SD_D3 == PIN_SD_D0 + 3, "SDIO D3 must be D0 + 3");
_Static_assert(PIN_SD_D3 < 32 && PIN_SD_SCK < 32 && PIN_SD_CMD < 32,
               "SDIO pins must be below GPIO32");
_Static_assert((PIN_SD_SCK < PIN_SD_D0 || PIN_SD_SCK > PIN_SD_D3) &&
               (PIN_SD_CMD < PIN_SD_D0 || PIN_SD_CMD > PIN_SD_D3) &&
               PIN_SD_SCK != PIN_SD_CMD,
               "SDIO CLK and CMD must be apart from DAT0..DAT3 and each other");

/**
 * @brief The SDIO-attached card interface.
 *
 * PIO: pio1, and nothing else uses it. The driver's two programs take 2 state
 * machines and all 32 instruction slots of the block, plus 2 DMA channels and
 * no interrupt. The DShot driver and the KISS receiver claim from whichever
 * block has room, and the SDK searches pio2 first, so they land on pio2 and
 * never compete with this driver. A block shared with this driver would make
 * whichever initialised second fail depending on timing. @see esc_task.cpp for
 * the budget of that block, and docs/design/sdio-driver.md for the driver.
 *
 * The driver ignores the DMA interrupt fields: it polls.
 */
static sd_sdio_if_t sdioIf = {
	.CLK_gpio  = PIN_SD_SCK,
	.CMD_gpio  = PIN_SD_CMD,
	.D0_gpio   = PIN_SD_D0,
	.D1_gpio   = PIN_SD_D1,
	.D2_gpio   = PIN_SD_D2,
	.D3_gpio   = PIN_SD_D3,
	.SDIO_PIO  = pio1,
	.baud_rate = SD_LOG_SDIO_HZ,
};

/**
 * @brief The one card slot.
 *
 * The board brings no card-detect line out, so `use_card_detect` is false:
 * the only way to know whether a card is fitted is to try to talk to it. That
 * is why sdLogBegin() treats a mount failure as "no card" rather than an
 * error -- on a bench, running without a card is the normal case.
 */
static sd_card_t sd_cards[] = {
	{
		.type            = SD_IF_SDIO,
		.sdio_if_p       = &sdioIf,
		.use_card_detect = false,
	}
};

/** @brief Number of card slots. Called by the library. @return Count. */
size_t sd_get_num() { return count_of(sd_cards); }

/**
 * @brief Look up a card slot.
 * @param num Index below sd_get_num().
 * @return    The slot, or NULL if out of range.
 */
sd_card_t *sd_get_by_num(size_t num) {
	return num < sd_get_num() ? &sd_cards[num] : NULL;
}
