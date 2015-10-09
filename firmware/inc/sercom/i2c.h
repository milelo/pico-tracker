/*
 * A wrapper around the samd20 i2c functions. Single master only
 * Copyright (C) 2015  Richard Meadows <richardeoin>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef I2C_H
#define I2C_H

#include "sercom/i2c_master.h"

/**
 * I2C Write.
 *
 * address is the full write address like 0xEE
 */
void i2c_master_write(uint8_t address, uint8_t* data, uint16_t data_length);

/**
 * I2C Read.
 *
 * address is the full write address like 0xEE
 */
void i2c_master_read(uint8_t address, uint8_t* data, uint16_t data_length);

/**
 * I2C bus master.
 */
void i2c_init(SercomI2cm*const sercom, uint32_t pad0_pinmux, uint32_t pad1_pinmux);

#endif /* I2C_H */
