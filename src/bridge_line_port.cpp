/**
 * @file bridge_line_port.cpp
 * @brief The USB bridge's line on two PIO state machines.
 *        @see bridge_line_port.h
 *
 * Device-only. The programs are assembled by `pico_generate_pio_header()`
 * (src/bridge.cmake, and CMakeLists.txt for pio_uart_rx.pio); test/stubs
 * carries stand-ins for both headers so `make syntax` typechecks this file.
 */

#include "bridge_line_port.h"
#include "bridge_wire.h"
#include "core1_job.h"

#include "bridge_line.pio.h"
#include "pio_uart_rx.pio.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>

static PIO     s_pio   = nullptr;  /**< The block both machines are in. */
static uint    s_txSm  = 0;        /**< The transmitter. */
static uint    s_rxSm  = 0;        /**< The receiver. */
static uint    s_txOff = 0;        /**< Where bridge_tx landed. */
static uint    s_rxOff = 0;        /**< Where pio_uart_rx landed. */
static uint8_t s_pin   = 0;        /**< The pad. */
static bool    s_open  = false;    /**< All of the above mean something. */

uint8_t bridgeLineOpen(uint8_t gpio) {
	bridgeLineClose();
	uint16_t divInt;
	uint8_t divFrac;
	if (!bridgeLineClock(clock_get_hz(clk_sys), &divInt, &divFrac)) return CORE1_JOB_CLOCK;

	// The SDK's search starts at pio2, which DShot and KISS have left while
	// the claim holds: both programs fit there (docs/design/pin-claim.md, "PIO
	// and DMA budget"). The receiver goes where the transmitter went, so the
	// two share one clock divider's phase and one pad.
	PIO pio;
	uint txSm, txOff;
	if (!pio_claim_free_sm_and_add_program_for_gpio_range(&bridge_tx_program, &pio, &txSm,
	                                                      &txOff, gpio, 1, true))
		return CORE1_JOB_PIO;
	const int rxSm = pio_claim_unused_sm(pio, false);
	if (rxSm < 0 || !pio_can_add_program(pio, &pio_uart_rx_program)) {
		if (rxSm >= 0) pio_sm_unclaim(pio, (uint)rxSm);
		pio_remove_program_and_unclaim_sm(&bridge_tx_program, pio, txSm, txOff);
		return CORE1_JOB_PIO;
	}
	const uint rxOff = pio_add_program(pio, &pio_uart_rx_program);

	// Transmitter: OUT and SET write the pad's level, the side-set its
	// direction (bridge_line.pio). OUT shifts right: least significant bit
	// first, then bit 8, the keep flag. No autopull: the program pulls.
	pio_sm_config tc = bridge_tx_program_get_default_config(txOff);
	sm_config_set_out_pins(&tc, gpio, 1);
	sm_config_set_set_pins(&tc, gpio, 1);
	sm_config_set_sideset_pins(&tc, gpio);
	sm_config_set_out_shift(&tc, true, false, 32);
	sm_config_set_fifo_join(&tc, PIO_FIFO_JOIN_TX);
	sm_config_set_clkdiv_int_frac8(&tc, divInt, divFrac);

	// Receiver: as pioUartRxBegin() sets it up, on the same pad. The byte
	// lands in bits 31 to 24.
	pio_sm_config rc = pio_uart_rx_program_get_default_config(rxOff);
	sm_config_set_in_pins(&rc, gpio);
	sm_config_set_jmp_pin(&rc, gpio);
	sm_config_set_in_shift(&rc, true, false, 32);
	sm_config_set_fifo_join(&rc, PIO_FIFO_JOIN_RX);
	sm_config_set_clkdiv_int_frac8(&rc, divInt, divFrac);

	pio_sm_init(pio, txSm, txOff, &tc);
	pio_sm_init(pio, (uint)rxSm, rxOff, &rc);

	// The pad: an input, its output latch high, its pull-up on, before the
	// function moves to PIO; so it idles high and nothing drives it until
	// the first start bit.
	const uint32_t mask = 1u << gpio;
	pio_sm_set_pindirs_with_mask(pio, txSm, 0, mask);
	pio_sm_set_pins_with_mask(pio, txSm, mask, mask);
	gpio_pull_up(gpio);
	pio_gpio_init(pio, gpio);

	pio->fdebug = 1u << (PIO_FDEBUG_RXSTALL_LSB + (uint)rxSm);
	pio_enable_sm_mask_in_sync(pio, (1u << txSm) | (1u << (uint)rxSm));

	s_pio = pio;
	s_txSm = txSm;
	s_rxSm = (uint)rxSm;
	s_txOff = txOff;
	s_rxOff = rxOff;
	s_pin = gpio;
	s_open = true;
	return CORE1_JOB_OK;
}

void bridgeLineClose() {
	if (!s_open) return;
	// The pad first: an input with its pull-up, as the claim leaves it in
	// Held, so nothing the machines do while they are taken down reaches the
	// wire.
	gpio_init(s_pin);
	gpio_set_dir(s_pin, GPIO_IN);
	gpio_pull_up(s_pin);
	pio_sm_set_enabled(s_pio, s_txSm, false);
	pio_sm_set_enabled(s_pio, s_rxSm, false);
	pio_sm_clear_fifos(s_pio, s_txSm);
	pio_sm_clear_fifos(s_pio, s_rxSm);
	pio_remove_program_and_unclaim_sm(&bridge_tx_program, s_pio, s_txSm, s_txOff);
	pio_remove_program_and_unclaim_sm(&pio_uart_rx_program, s_pio, s_rxSm, s_rxOff);
	// The receiver's framing flag, `irq 4 rel`: flag 4 + its number.
	pio_interrupt_clear(s_pio, 4u + (s_rxSm & 3u));
	s_pio = nullptr;
	s_open = false;
}

bool bridgeLineIsOpen() { return s_open; }

bool bridgeLinePut(uint16_t word) {
	if (pio_sm_is_tx_fifo_full(s_pio, s_txSm)) return false;
	pio_sm_put(s_pio, s_txSm, word);
	return true;
}

bool bridgeLineGet(uint8_t *out) {
	if (pio_sm_is_rx_fifo_empty(s_pio, s_rxSm)) return false;
	*out = (uint8_t)(pio_sm_get(s_pio, s_rxSm) >> 24);
	return true;
}

bool bridgeLineIdle() {
	// The FIFO first: only core1 fills it, so once it reads empty a
	// transmitter seen on `pull` has nothing left to send.
	return pio_sm_is_tx_fifo_empty(s_pio, s_txSm) &&
	       pio_sm_get_pc(s_pio, s_txSm) == s_txOff &&
	       pio_sm_get_pc(s_pio, s_rxSm) == s_rxOff;
}

bool bridgeLineTakeOverflow() {
	const uint32_t bit = 1u << (PIO_FDEBUG_RXSTALL_LSB + s_rxSm);
	if (!(s_pio->fdebug & bit)) return false;
	s_pio->fdebug = bit;  // write 1 to clear
	return true;
}
