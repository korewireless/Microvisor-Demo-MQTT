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


/*
 * INCLUDES
 */
#include <stdbool.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"
#include "cmsis_os.h"

#include "azure_helper.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * DEFINES
 */
#define BUF_SEND_SIZE 4*1024
#define BUF_RECEIVE_SIZE 7*1024

// CONFIG DATA

#define BUF_CLIENT_SIZE 34

/*
 * TYPES
 */
enum WorkMessageType {
    ConnectNetwork = 0x10,
    OnNetworkConnected,
    OnNetworkDisconnected,

    PopulateConfig = 0x30,
    OnConfigRequestReturn,
    OnConfigObtained,
    OnConfigFailed,

    // Managed MQTT operations and connection events
    ConnectMQTTBroker = 0x50,
    OnMqttChannelFailed,
    OnBrokerConnectFailed,
    OnBrokerConnected,
    OnBrokerSubscriptionRequestFailed,
    OnBrokerSubscribeFailed,
    OnBrokerSubscribeSucceeded,
    OnBrokerUnsubscriptionRequestFailed,
    OnBrokerUnsubscribeFailed,
    OnBrokerUnsubscribeSucceeded,
    OnBrokerPublishFailed,
    OnBrokerPublishSucceeded,
    OnBrokerPublishRateLimited,
    OnBrokerMessageAcknowledgeFailed,
    OnBrokerDisconnected,
    OnBrokerDisconnectFailed,
    OnBrokerDroppedConnection,

    // Managed MQTT readable events to handle
    OnMQTTReadable = 0x70,
    OnMQTTEventConnectResponse,
    OnMQTTEventMessageReceived,
    OnMQTTEventMessageLost,
    OnMQTTEventSubscribeResponse,
    OnMQTTEventUnsubscribeResponse,
    OnMQTTEventPublishResponse,
    OnMQTTEventDisconnectResponse,

    // Application events
    OnApplicationConsumedMessage = 0x90,
    OnApplicationProducedMessage,
};
  
/*
 * PROTOTYPES
 */
void start_work_task(void *argument);
void pushWorkMessage(enum WorkMessageType type);


/*
 * GLOBALS
 */
extern MvNotificationHandle work_notification_center_handle;
extern osMessageQueueId_t workMessageQueue;
extern uint8_t work_send_buffer[BUF_SEND_SIZE]; // shared by config and mqtt as only one is active at a time
extern uint8_t work_receive_buffer[BUF_RECEIVE_SIZE]; // shared by config and mqtt as only one is active at a time

extern uint8_t *incoming_message_topic;
extern uint32_t incoming_message_topic_len;
extern uint8_t *incoming_message_payload;
extern uint32_t incoming_message_payload_len;

// CONFIG DATA

extern uint8_t *incoming_message_topic;
extern uint32_t incoming_message_topic_len;
extern uint8_t *incoming_message_payload;
extern uint32_t incoming_message_payload_len;

extern uint8_t  client[BUF_CLIENT_SIZE];
extern size_t   client_len;

extern struct AzureConnectionStringParams azure_params;

#ifdef __cplusplus
}
#endif


#endif /* WORK_H */
