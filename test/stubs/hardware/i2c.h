// Host-test stub for hardware/i2c.h. test/fake_i2c.cpp is the bus behind it:
// with no part added, every address answers with a NACK, which is what a
// board with no touch controller attached would do.
#pragma once
#include <stdint.h>
#include <stddef.h>
// The SDK's codes for a failed transfer, as pico/error.h numbers them.
#ifndef PICO_ERROR_GENERIC
#define PICO_ERROR_GENERIC (-1)
#endif
#ifndef PICO_ERROR_TIMEOUT
#define PICO_ERROR_TIMEOUT (-2)
#endif
typedef struct i2c_inst i2c_inst_t;
extern i2c_inst_t *i2c0;
extern i2c_inst_t *i2c1;
void i2c_init(i2c_inst_t *i2c, unsigned baud);
void i2c_deinit(i2c_inst_t *i2c);
int i2c_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src,
                         size_t len, bool nostop, unsigned timeout_us);
int i2c_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst,
                        size_t len, bool nostop, unsigned timeout_us);
