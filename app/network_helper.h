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


#ifdef __cplusplus
extern "C" {
#endif


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

  
/*
 * PROTOTYPES
 */
void spin_network();
bool have_network();
MvNetworkHandle get_network_handle();


/*
 * GLOBALS
 */
extern volatile bool want_network;

#ifdef __cplusplus
}
#endif


#endif /* NETWORK_HELPER_H */
