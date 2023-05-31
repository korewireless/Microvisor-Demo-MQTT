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
#include <assert.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"
#include "cmsis_os.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * DEFINES
 */
#define BUF_SEND_SIZE 4*1024
#define BUF_RECEIVE_SIZE 7*1024

// CONFIG DATA

#define CERTIFICATE_CA
#define CERTIFICATE_AUTH
//#define USERNAMEPASSWORD_AUTH

//#define CUSTOM_CLIENT_ID

#define BUF_CLIENT_SIZE 34

#define BUF_BROKER_HOST 128

#if defined(CERTIFICATE_CA)
#define BUF_ROOT_CA 1536
#endif // CERTIFICATE_CA

#if defined(CERTIFICATE_AUTH)
#define BUF_CERT 1024
#define BUF_PRIVATE_KEY 1536
#endif // CERTIFICATE_AUTH

#if defined(USERNAMEPASSWORD_AUTH)
#define BUF_USERNAME 128
#define BUF_PASSWORD 128
#endif // USERNAMEPASSWORD_AUTH


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

extern uint8_t  broker_host[BUF_BROKER_HOST];
extern size_t   broker_host_len;
extern uint16_t broker_port;

#if defined(CERTIFICATE_CA)
extern uint8_t  root_ca[BUF_ROOT_CA];
extern size_t   root_ca_len;
#endif // CERTIFICATE_CA

#if defined(CERTIFICATE_AUTH)
extern uint8_t  cert[BUF_CERT];
extern size_t   cert_len;
extern uint8_t  private_key[BUF_PRIVATE_KEY];
extern size_t   private_key_len;
#endif // CERTIFICATE_AUTH

#if defined(USERNAMEPASSWORD_AUTH)
extern uint8_t  username[BUF_USERNAME];
extern size_t   username_len;
extern uint8_t  password[BUF_PASSWORD];
extern size_t   password_len;
#endif // USERNAMEPASSWORD_AUTH


#ifdef __cplusplus
}
#endif


#endif /* WORK_H */
