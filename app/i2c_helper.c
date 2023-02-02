/**
 *
 * Microvisor I2C Helper
 * Version 1.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */

#include "i2c_helper.h"
#include "log_helper.h"

#include "stm32u5xx_hal.h"

#define I2C_TIMEOUT 1000

static bool initialised = false;
I2C_HandleTypeDef i2c;

/**
 * @brief Configure STM32U585 I2C1.
 */
bool i2c_init() {
    uint32_t pclk1;

    if (mvGetPClk1(&pclk1) != MV_STATUS_OKAY) {
        return false;
    }

    const unsigned presc = (pclk1/16000000) - 1;

    const unsigned timing = (presc << I2C_TIMINGR_PRESC_Pos) |
                              (7 << I2C_TIMINGR_SCLDEL_Pos) |
                              (5 << I2C_TIMINGR_SDADEL_Pos) |
                              (7 << I2C_TIMINGR_SCLH_Pos) |
                              (19 << I2C_TIMINGR_SCLL_Pos);

    i2c.Instance             = I2C1;
    i2c.Init.Timing          = timing;
    i2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    // Initialize the peripheral
    if (HAL_I2C_Init(&i2c) != HAL_OK) {
      server_error("Could not enable I2C");
      return false;
    }

    initialised = true;
    return true;
}

/**
 * @brief HAL-called function to configure I2C clock and pind.
 *
 * @param i2c: A HAL I2C_HandleTypeDef pointer to the I2C instance.
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *i2c) {
    // This SDK-named function is called by HAL_I2C_Init()

    // Configure U5 peripheral clock
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;

    // Initialize U5 peripheral clock
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        // Log error
        server_error("Could not enable I2C clock");
        return;
    }

    // Enable the I2C GPIO interface clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure the GPIO pins for I2C
    // Pins PB6 and PB9 - SCL and SDA
    GPIO_InitTypeDef gpioConfig = { 0 };
    gpioConfig.Pin       = GPIO_PIN_6 | GPIO_PIN_9;
    gpioConfig.Mode      = GPIO_MODE_AF_OD;         // Open drain mode
    gpioConfig.Pull      = GPIO_NOPULL;             // External pull-up
    gpioConfig.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpioConfig.Alternate = GPIO_AF4_I2C1;         // Select the alt function

    // Initialize the pins with the setup data
    HAL_GPIO_Init(GPIOB, &gpioConfig);


    // Enable the I2C clock
    __HAL_RCC_I2C1_CLK_ENABLE();
}

bool i2c_write_reg(uint8_t dev_addr, uint16_t reg_addr, bool addr_16bit, uint8_t *write_data, size_t write_len) {
    if (HAL_I2C_IsDeviceReady(&i2c, dev_addr, 3, I2C_TIMEOUT) != HAL_OK) {
        return false;
    }

    if (HAL_I2C_Mem_Write(&i2c, dev_addr, reg_addr, (addr_16bit) ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT, write_data, write_len, I2C_TIMEOUT) != HAL_OK) {
        return false;
    }

    return true;
}

bool i2c_read_reg(uint8_t dev_addr, uint16_t reg_addr, bool addr_16bit, uint8_t *read_data, size_t read_len) {
    if (HAL_I2C_IsDeviceReady(&i2c, dev_addr, 3, I2C_TIMEOUT) != HAL_OK) {
        return false;
    }

    if (HAL_I2C_Mem_Read(&i2c, dev_addr, reg_addr, (addr_16bit) ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT, read_data, read_len, I2C_TIMEOUT) != HAL_OK) {
       return false;
    }

    return true;
}

