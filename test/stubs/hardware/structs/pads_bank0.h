// Host-test stub for hardware/structs/pads_bank0.h, with the two calls of
// hardware/address_mapped.h the S.BUS output job makes on a pad register.
//
// Each GPIO's pad register is an object of the GPIO fake of fakes.cpp, which
// records its value and the order of each write (fakePad() in fakes.h).
// gpio_pull_down() and its kin keep their own record in the fake and do not
// write it.
#pragma once
#include <stdint.h>

// hardware/regs/pads_bank0.h, RP2350, SDK 2.1.1.
#define PADS_BANK0_GPIO0_ISO_BITS 0x00000100u
#define PADS_BANK0_GPIO0_OD_BITS  0x00000080u
#define PADS_BANK0_GPIO0_IE_BITS  0x00000040u
#define PADS_BANK0_GPIO0_PUE_BITS 0x00000008u
#define PADS_BANK0_GPIO0_PDE_BITS 0x00000004u

/** @brief GPIOx's pad register. */
struct fake_pad_reg { uint32_t value; };
/** @brief PADS_BANK0: GP0 to GP29. */
typedef struct {
	uint32_t     voltage_select;
	fake_pad_reg io[30];
} pads_bank0_hw_t;

extern pads_bank0_hw_t g_fakePadsBank0;
#define pads_bank0_hw (&g_fakePadsBank0)

/** @brief hw_write_masked() on a pad register: the bits of @p mask from @p values. */
void hw_write_masked(fake_pad_reg *r, uint32_t values, uint32_t mask);
/** @brief hw_clear_bits() on a pad register. */
void hw_clear_bits(fake_pad_reg *r, uint32_t mask);
