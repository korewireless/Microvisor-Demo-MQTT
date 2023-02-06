/**
 *
 * Microvisor Log Helper
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */


#include "log_helper.h"
#include <string.h>
#include <stdio.h>
#include "uart_logging.h"

// Microvisor includes
#include "mv_syscalls.h"


/**
 * @brief Issue a debug message.
 *
 * @param format_string Message string with optional formatting
 * @param ...           Optional injectable values
 */
void server_log(char* format_string, ...) {
    if (LOG_DEBUG_MESSAGES) {
        va_list args;
        va_start(args, format_string);
        do_log(false, format_string, args);
        va_end(args);
    }
}


/**
 * @brief Issue an error message.
 *
 * @param format_string Message string with optional formatting
 * @param ...           Optional injectable values
 */
void server_error(char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    do_log(false, format_string, args);
    va_end(args);
}


/**
 * @brief Issue any log message.
 *
 * @param is_err        Is the message an error?
 * @param format_string Message string with optional formatting
 * @param args          va_list of args from previous call
 */
void do_log(bool is_err, char* format_string, va_list args) {
    char buffer[512] = {0}; // If you increase the buffer here, you may need to increase the stack size for the calling FreeRTOS task.

    // Write the message type to the message
    sprintf(buffer, is_err ? "[ERROR] " : "[DEBUG] ");

    // Write the formatted text to the message
    vsnprintf(&buffer[8], sizeof(buffer) - 9, format_string, args);

    // Output the message using the system call
    mvServerLog((const uint8_t*)buffer, (uint16_t)strlen(buffer));

    // Do we output via UART too?
    if (uart_available) {
        // Add NEWLINE to the message and output to UART
        buffer[sizeof(buffer) < strlen(buffer) ? sizeof(buffer)-1 : strlen(buffer)] = '\n';
        UART_output((uint8_t*)buffer, strlen(buffer));
    }
}
