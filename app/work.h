/**
 *
 * Microvisor MQTT Communications Demo
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#ifndef WORK_H
#define WORK_H


#ifdef __cplusplus
extern "C" {
#endif


/*
 * INCLUDES
 */
#include <stdbool.h>

#include "cmsis_os.h"


/*
 * TYPES
 */
enum MessageType {
    ConnectNetwork = 0x1,
    OnNetworkConnected,
    OnNetworkDisconnected,

    PopulateConfig,
    OnConfigRequestReturn,
    OnConfigObtained,
    OnConfigFailed,

    PostData,
    PostSucceeded,
    PostFailed,

    PollData,
    PollSucceeded,
    PollFailed,
};
  
/*
 * PROTOTYPES
 */
void start_work_task(void *argument);
void pushMessage(enum MessageType type);


/*
 * GLOBALS
 */
extern osMessageQueueId_t workMessageQueue;


#ifdef __cplusplus
}
#endif


#endif /* WORK_H */
