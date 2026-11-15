// Host-test stub for hardware/uart.h: the calls and registers the firmware
// uses, on the UART0 fake of fakes.cpp (fakeUart*() in fakes.h).
//
// The device reads UARTFR and UARTDR through uart_get_hw(). Here those two
// registers are objects whose read is a call into the fake, so reading DR
// pops the receive FIFO as it does on the chip, and the firmware's source is
// the same on both. Only UART0 is modelled; uart1 names the same fake.
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct uart_inst uart_inst_t;
extern uart_inst_t *uart0;
extern uart_inst_t *uart1;
enum uart_parity_t { UART_PARITY_NONE = 0, UART_PARITY_EVEN, UART_PARITY_ODD };

// hardware/regs/uart.h, RP2350, SDK 2.1.1.
#define UART_UARTDR_OE_BITS   0x00000800u
#define UART_UARTDR_BE_BITS   0x00000400u
#define UART_UARTDR_PE_BITS   0x00000200u
#define UART_UARTDR_FE_BITS   0x00000100u
#define UART_UARTDR_DATA_BITS 0x000000ffu
#define UART_UARTFR_RXFE_BITS 0x00000010u

/** @brief UARTFR: reads RXFE from the fake's receive FIFO. */
struct fake_uart_fr { operator uint32_t() const; };
/** @brief UARTDR: a read pops the fake's receive FIFO. */
struct fake_uart_dr { operator uint32_t() const; };
/** @brief The two registers of uart_hw_t the firmware reads. */
typedef struct {
	fake_uart_dr dr;
	fake_uart_fr fr;
} uart_hw_t;
uart_hw_t *uart_get_hw(uart_inst_t *u);

unsigned uart_init(uart_inst_t *u, unsigned baud);
void uart_deinit(uart_inst_t *u);
bool uart_is_readable(uart_inst_t *u);
uint8_t uart_getc(uart_inst_t *u);
void uart_set_format(uart_inst_t *u, unsigned bits, unsigned stop, enum uart_parity_t p);
void uart_set_fifo_enabled(uart_inst_t *u, bool on);
