/**
 *
 * Microvisor MQTT Communications Demo
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _MAIN_H_
#define _MAIN_H_


/*
 * INCLUDES
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "cmsis_os.h"
#include "mv_syscalls.h"

// App includes


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CONSTANTS
 */
#define     LED_GPIO_BANK               GPIOA
#define     LED_GPIO_PIN                GPIO_PIN_5

#define     LED_TASK_PAUSE_MS           500
#define     NETWORK_TASK_PAUSE_MS       500

//#define     MQTT_RX_BUFFER_SIZE_B       50*1024
//#define     MQTT_TX_BUFFER_SIZE_B       5*1024
//#define     MQTT_NT_BUFFER_COUNT_R      8             // NOTE Count of records, not bytes
  
#ifdef __cplusplus
}
#endif


#endif      // _MAIN_H_
