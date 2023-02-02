/**
 *
 * Microvisor I2C Helper
 * Version 1.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */

/* These helper functions provide access to I2C interface on pins PB6/PB9 with clock rate of 400KHz.
 * All access is blocking.
 */
#ifndef I2C_HELPER_H
#define I2C_HELPER_H

/*
 * INCLUDES
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * PROTOTYPES
 */
bool i2c_init();
bool i2c_write_reg(uint8_t dev_addr, uint16_t reg_addr, bool addr_16bit, uint8_t *write_data, size_t write_len);
bool i2c_read_reg(uint8_t dev_addr, uint16_t reg_addr, bool addr_16bit, uint8_t *read_data, size_t read_len);

#ifdef __cplusplus
}
#endif

#endif /* I2C_HELPER_H */

