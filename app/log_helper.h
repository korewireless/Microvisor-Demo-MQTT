/**
 *
 * Microvisor Log Helper
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#ifndef LOG_HELPER_H
#define LOG_HELPER_H


/*
 * These helper functions assume a logging buffer has been provided to Microvisor
 * elsewhere in your application.  An example of doing this follows:
 *
 * // Buffer for Microvisor application logging
 * const uint32_t log_buffer_size = 4096;
 * static uint8_t log_buffer[4096] __attribute__((aligned(512))) = {0} ;
 *
 * void init_loging(void) {
 *   (void)mvServerLoggingInit(log_buffer, log_buffer_size);
 * }
 *
 * Logging also depends on an active network connection to process and flush
 * the logging buffer.  This will vary based on your application.
 */


/*
 * INCLUDES
 */
#include <stdbool.h>
#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void server_log(char* format_string, ...);
void server_error(char* format_string, ...);
void do_log(bool is_err, char* format_string, va_list args);


#ifdef __cplusplus
}
#endif


#endif /* LOG_HELPER_H */
