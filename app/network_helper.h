/**
 *
 * Microvisor Network Helper
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#ifndef NETWORK_HELPER_H
#define NETWORK_HELPER_H


/*
 * These functions take ownership from Microvisor for the network state.  By default on
 * Microvisor startup, the network will always be avaialble until you tell it otherwise.
 *
 * These calls manage the network handle from Microvisor which allows you to open
 * additional communication channels (for example, HTTP or MQTT) and provide a
 * notification center which will alert you to changes in network status.
 */


/*
 * INCLUDES
 */
#include <stdbool.h>
#include "mv_syscalls.h"

  
#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void start_network_task(void *argument);
MvNetworkHandle get_network_handle();


/*
 * GLOBALS
 */
extern volatile bool want_network;


/*
 * DEFINES
 */
#define     NETWORK_TASK_PAUSE_MS       500


#ifdef __cplusplus
}
#endif


#endif /* NETWORK_HELPER_H */
