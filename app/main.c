/**
 *
 * Microvisor MQTT Communications Demo
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"
#include "log_helper.h"
#include "uart_logging.h"
#include "network_helper.h"
#include "work.h"
#include "application.h"


/*
 * GLOBALS
 */

// This is the CMSIS/FreeRTOS thread task that flashes the USER LED
osThreadId_t LEDTask;
const osThreadAttr_t led_task_attributes = {
    .name = "LEDTask",
    .stack_size = configMINIMAL_STACK_SIZE, // specified in words, size 4 for Microvisor
    .priority = (osPriority_t) osPriorityNormal
};

// This is the CMSIS/FreeRTOS thread task that manages the Microvisor network connection
osThreadId_t NetworkTask;
const osThreadAttr_t network_task_attributes = {
    .name = "NetworkTask",
    .stack_size = configMINIMAL_STACK_SIZE, // specified in words, size 4 for Microvisor
    .priority = (osPriority_t) osPriorityNormal
};

// This is the CMSIS/FreeRTOS thread task that manages the Config and MQTT operations
osThreadId_t WorkTask;
const osThreadAttr_t work_task_attributes = {
    .name = "WorkTask",
    .stack_size = configMINIMAL_STACK_SIZE + 4096, // specified in words, size 4 for Microvisor - we do a lot of work in this thread, allocate accordingly
    .priority = (osPriority_t) osPriorityNormal
};

// This is the CMSIS/FreeRTOS thread task that manages user application outside of communication
osThreadId_t ApplicationTask;
const osThreadAttr_t application_task_attributes = {
    .name = "ApplicationTask",
    .stack_size = configMINIMAL_STACK_SIZE, // specified in words, size 4 for Microvisor
    .priority = (osPriority_t) osPriorityNormal
};

// Buffer for Microvisor application logging
static uint8_t log_buffer[LOGGING_BUFFER_SIZE] __attribute__((aligned(512))) = {0} ;


/*
 * FORWARD DECLARATIONS
 */
static void system_clock_config(void);
static void gpio_init(void);
static void start_led_task(void *argument);
static void log_device_info(void);


/**
 *  @brief The application entry point.
 */
int main(void) {
    // Reset of all peripherals, Initializes the Flash interface and the sys tick.
    HAL_Init();

    // Configure the system clock
    system_clock_config();

    // Initialize peripherals
    gpio_init();

    // Initialize uart logging
    UART_init();

    // Initialize logging buffer, optional if logging not desired
    (void)mvServerLoggingInit(log_buffer, LOGGING_BUFFER_SIZE);

    // Get the Device ID and build number and log them
    log_device_info();

    // Init scheduler
    osKernelInitialize();

    // Create the FreeRTOS thread(s)
    LEDTask      = osThreadNew(start_led_task,      NULL, &led_task_attributes);
    assert(LEDTask != NULL);
    WorkTask     = osThreadNew(start_work_task,     NULL, &work_task_attributes);
    assert(WorkTask != NULL);
    NetworkTask  = osThreadNew(start_network_task,  NULL, &network_task_attributes);
    assert(NetworkTask != NULL);
    ApplicationTask     = osThreadNew(start_application_task,     NULL, &application_task_attributes);
    assert(ApplicationTask != NULL);

    // Start the scheduler
    osKernelStart();

    // We should never get here as control is now taken by the scheduler,
    // but just in case...
    while (1) {
        // NOP
    }
}


/**
 * @brief Initialize the MCU GPIO.
 *
 * Used to flash the Nucleo's USER LED, which is on GPIO Pin PA5.
 */
static void gpio_init(void) {
    // Enable GPIO port clock
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure GPIO pin output Level
    HAL_GPIO_WritePin(LED_GPIO_BANK, LED_GPIO_PIN, GPIO_PIN_RESET);

    // Configure GPIO pin : PA5 - Pin under test
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin   = LED_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LED_GPIO_BANK, &GPIO_InitStruct);
}


/**
 * @brief Function implementing the LED-flash task thread.
 *
 * @param  argument: Not used.
 */
static void start_led_task(void *argument) {
    uint32_t last_tick = 0;

    // The task's main loop
    while (1) {
        // Periodically update the display and flash the USER LED
        // Get the ms timer value
        uint32_t tick = HAL_GetTick();
        if (tick - last_tick > LED_TASK_PAUSE_MS) {
            last_tick = tick;

            // Toggle the USER LED's GPIO pin
            HAL_GPIO_TogglePin(LED_GPIO_BANK, LED_GPIO_PIN);
        }

        // End of cycle delay
        osDelay(10);
    }
}


/**
 * @brief Show basic device info.
 */
static void log_device_info(void) {
    uint8_t buffer[35] = { 0 };
    mvGetDeviceId(buffer, 34);
    server_log("Info:\nDevice: %s\n   App: %s %s\n Build: %i", buffer, APP_NAME, APP_VERSION, BUILD_NUM);
}


/**
 * @brief Get the MV clock value.
 *
 * Defined by HAL and Called from system_stm32u5xx_ns.c template via the HAL SystemCoreClockUpdate() method.
 *
 * @retval The clock value.
 */
uint32_t SECURE_SystemCoreClockUpdate() {
    uint32_t clock = 0;
    mvGetHClk(&clock);
    return clock;
}


/**
  * @brief System clock configuration.
  */
static void system_clock_config(void) {
    SystemCoreClockUpdate(); // Call HAL clock update method.
    HAL_InitTick(TICK_INT_PRIORITY);
}
