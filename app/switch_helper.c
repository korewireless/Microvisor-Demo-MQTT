/**
 *
 * Microvisor switch Helper
 * Version 1.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */

#include "switch_helper.h"
#include "log_helper.h"

#include "stm32u5xx_hal.h"

/**
 * @brief Configure GPIO switch.
 */
void switch_init() {
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpioConfig = { 0 };
    gpioConfig.Pin       = GPIO_PIN_7;
    gpioConfig.Mode      = GPIO_MODE_OUTPUT_PP;     // Push-pull
    gpioConfig.Pull      = GPIO_NOPULL;             // No pull-up
    gpioConfig.Speed     = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &gpioConfig);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);

    return;
}

/**
 * @brief Close the switch
 */
void switch_close() {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
}

/**
 * @brief Open the switch
 */
void switch_open() {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
}
