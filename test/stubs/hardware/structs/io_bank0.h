// Host-test stub for hardware/structs/io_bank0.h.
//
// Only each GPIO's control register, which the S.BUS output job writes
// whole. A write is a call into the GPIO fake of fakes.cpp, which records it
// with FUNCSEL and the overrides as one value (fakeGpioCtrlLog() in fakes.h),
// as gpio_set_function() and gpio_set_inover() record theirs.
#pragma once
#include <stdint.h>

// hardware/regs/io_bank0.h, RP2350, SDK 2.1.1.
#define IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS 0x0000001fu
#define IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB  0u
#define IO_BANK0_GPIO0_CTRL_OUTOVER_BITS 0x00003000u
#define IO_BANK0_GPIO0_CTRL_OUTOVER_LSB  12u
#define IO_BANK0_GPIO0_CTRL_INOVER_BITS  0x00030000u
#define IO_BANK0_GPIO0_CTRL_INOVER_LSB   16u

/** @brief GPIOx_CTRL: a write is recorded by the fake, a read gives the last value. */
struct fake_gpio_ctrl {
	operator uint32_t() const;
	fake_gpio_ctrl &operator=(uint32_t v);
};
/** @brief One GPIO's status and control registers. */
typedef struct {
	uint32_t       status;
	fake_gpio_ctrl ctrl;
} io_bank0_status_ctrl_hw_t;
/** @brief IO_BANK0: GP0 to GP29. */
typedef struct {
	io_bank0_status_ctrl_hw_t io[30];
} io_bank0_hw_t;

extern io_bank0_hw_t g_fakeIoBank0;
#define io_bank0_hw (&g_fakeIoBank0)
