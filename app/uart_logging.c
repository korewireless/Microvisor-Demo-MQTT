/**
 *
 * Microvisor UART Log Helper
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */


#include "uart_logging.h"
#include <string.h>
#include <stdio.h>

// Microvisor includes
#include "stm32u5xx_hal.h"

#include "log_helper.h"


bool uart_available = false;
UART_HandleTypeDef uart;


/**
 * @brief Configure STM32U585 UART1.
 */
bool UART_init() {
    uart.Instance           = USART2;
    uart.Init.BaudRate      = 115200;              // Match your chosen speed
    uart.Init.WordLength    = UART_WORDLENGTH_8B;  // 8
    uart.Init.StopBits      = UART_STOPBITS_1;     // N
    uart.Init.Parity        = UART_PARITY_NONE;    // 1
    uart.Init.Mode          = UART_MODE_TX;        // TX only mode
    uart.Init.HwFlowCtl     = UART_HWCONTROL_NONE; // No CTS/RTS

    // Initialize the UART
    if (HAL_UART_Init(&uart) != HAL_OK) {
      // Log error
      server_log("Could not enable logging UART");
      return false;
    }

    uart_available = true;

    server_log("UART logging enabled");
    return true;
}

/**
 * @brief HAL-called function to configure UART.
 *
 * @param uart: A HAL UART_HandleTypeDef pointer to the UART instance.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *uart) {
    // This SDK-named function is called by HAL_UART_Init()

    // Configure U5 peripheral clock
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;

    // Initialize U5 peripheral clock
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        // Log error
        server_log("Could not enable logging UART clock");
        return;
    }

    // Enable the UART GPIO interface clock
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Configure the GPIO pins for UART
    // Pin PD5 - TX
    GPIO_InitTypeDef gpioConfig = { 0 };
    gpioConfig.Pin       = GPIO_PIN_5;              // TX pin
    gpioConfig.Mode      = GPIO_MODE_AF_PP;         // Pin's alt function with pull...
    gpioConfig.Pull      = GPIO_NOPULL;             // ...but don't apply a pull
    gpioConfig.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpioConfig.Alternate = GPIO_AF7_USART2;         // Select the alt function

    // Initialize the pins with the setup data
    HAL_GPIO_Init(GPIOD, &gpioConfig);


    // Enable the UART clock
    __HAL_RCC_USART2_CLK_ENABLE();
}

/**
 * @brief Output a UART-friendly log string, ie. one with
 *        RETURN+NEWLINE in place of NEWLINE.
 *
 * @param buffer: Source string.
 * @param length: String character count.
 */
void UART_output(uint8_t* buffer, uint16_t length) {
    if (!uart_available) {
        return;
    }

    const char nls[2] = "\r\n";
    for (uint32_t i = 0 ; i < length ; ++i) {
        uint8_t c = *buffer++;
        if (c == 0) return;
        if (c == '\n') {
            HAL_UART_Transmit(&uart, (uint8_t*)nls, 2, 100);
        } else {
            HAL_UART_Transmit(&uart, &c, 1, 100);
        }
    }
}
