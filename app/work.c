/**
 *
 * Microvisor MQTT Communications Demo
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */


#include "work.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "log_helper.h"
#include "network_helper.h"
#include "config_handler.h"
#include "mqtt_handler.h"
#include "application.h"


/*
 * CONFIGURTATION
 */
#define WORK_NOTIFICATION_BUFFER_COUNT 8
// If you change the IRQ here, you must also rename the Handler function at the bottom of the page
// which the HAL calls.
#define WORK_NOTIFICATION_IRQ TIM8_BRK_IRQn

/*
 * FORWARD DECLARATIONS
 */
static void configure_work_notification_center();
static bool get_mqtt_message();

/*
 * STORAGE
 */
osMessageQueueId_t workMessageQueue;

static volatile struct MvNotification work_notification_buffer[WORK_NOTIFICATION_BUFFER_COUNT] __attribute__((aligned(8)));
static volatile uint32_t current_work_notification_index = 0;
MvNotificationHandle  work_notification_center_handle = 0;

uint8_t work_send_buffer[BUF_SEND_SIZE] __attribute__ ((aligned(512))); // shared by config and mqtt as only one is active at a time
uint8_t work_receive_buffer[BUF_RECEIVE_SIZE] __attribute__ ((aligned(512))); // shared by config and mqtt as only one is active at a time

bool application_processing_message = false;
static uint32_t correlation_id = 0;
static bool mqtt_message_pending = false;
static bool mqtt_connection_active = false;
static bool wait_for_config = false;

uint8_t *incoming_message_topic;
uint32_t incoming_message_topic_len;
uint8_t *incoming_message_payload;
uint32_t incoming_message_payload_len;

/**
 * @brief Push message into work queue.
 *
 * @param  type: WorkMessageType enumeration value
 */
void pushWorkMessage(enum WorkMessageType type) {
    osStatus_t status;
    if ((status = osMessageQueuePut(workMessageQueue, &type, 0U, 0U)) != osOK) { // osWaitForever might be better for some messages?
        server_error("failed to post message: %ld", status);
    }
}

/**
 * @brief Function implementing the Work task thread.
 *
 * @param  argument: Not used.
 */
void start_work_task(void *argument) {
    bool network_on = false;
    
    configure_work_notification_center();

    workMessageQueue = osMessageQueueNew(16, sizeof(enum WorkMessageType), NULL);
    if (workMessageQueue == NULL) {
        server_error("failed to create queue");
        return;
    }

    pushWorkMessage(ConnectNetwork);
    
    enum WorkMessageType messageType;
    
    // The task's main loop
    while (1) {
        osDelay(1);
        if (osMessageQueueGet(workMessageQueue, &messageType, NULL, osWaitForever) == osOK) {
            switch (messageType) {
                case ConnectNetwork:
                    want_network = true;
                    break;
                case OnNetworkConnected:
                    network_on = true;
                    pushWorkMessage(PopulateConfig);
                    break;
                case OnNetworkDisconnected:
                    network_on = false;
                    break;
                case PopulateConfig:
                    wait_for_config = true;
                    start_configuration_fetch();
                    break;
                case OnConfigRequestReturn:
                    wait_for_config = false;
                    receive_configuration_items();
                    break;
                case OnConfigObtained:
                    finish_configuration_fetch();
                    pushWorkMessage(ConnectMQTTBroker);
                    break;
                case OnConfigFailed:
                    server_error("we failed to obtain the needed configuration");
                    wait_for_config = false;
                    finish_configuration_fetch();
                    break;
                case ConnectMQTTBroker:
                    start_mqtt_connect();
                    break;
                case OnBrokerConnected:
                    mqtt_connection_active = true;
                    start_subscriptions();
                    break;
                case OnBrokerSubscribeSucceeded:
                    pushApplicationMessage(OnMqttConnected);
                    break;
                case OnBrokerSubscribeFailed:
                    server_error("subscription failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerUnsubscribeSucceeded:
                    break;
                case OnBrokerUnsubscribeFailed:
                    server_error("unsubscription failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerPublishSucceeded:
                    break;
                case OnBrokerPublishFailed:
                    server_error("publish failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerPublishRateLimited:
                    server_error("publish was rate limited");
                    break;
                case OnBrokerMessageAcknowledgeFailed:
                    server_error("message acknowledgement failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerConnectFailed:
                    server_error("broker connect failed - cleaning up mqtt broker...");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerDisconnectFailed:
                    mqtt_connection_active = false;
                    server_error("couldn't disconnect gracefully, closing the channel");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerDisconnected:
                    mqtt_connection_active = false;
                    pushApplicationMessage(OnMqttDisconnected);
                    if (network_on) {
                        server_log("reconnect to mqtt broker");
                        pushWorkMessage(ConnectMQTTBroker);
                    }
                    break;
                case OnBrokerDroppedConnection:
                    mqtt_connection_active = false;
                    teardown_mqtt_connect();
                    server_log("mqtt channel closed by server");
                    break;
                case OnMQTTReadable:
                    mqtt_handle_readable_event();
                    break;
                case OnMQTTEventConnectResponse:
                    mqtt_handle_connect_response_event();
                    break;
                case OnMQTTEventMessageReceived:
                    if (application_processing_message) {
                        mqtt_message_pending = true;
                    } else {
                        application_processing_message = false;
                        if(!get_mqtt_message()) {
                            server_error("reading mqtt message failed");
                            mqtt_disconnect();
                        } else {
                            pushApplicationMessage(OnIncomingMqttMessage);
                        }
                    }
                    break;
                case OnMQTTEventMessageLost:
                    if (!mqtt_handle_lost_message_data()) {
                        server_error("handling lost mqtt message failed");
                        mqtt_disconnect();
                    }
                    break;
                case OnMQTTEventSubscribeResponse:
                    mqtt_handle_subscribe_response_event();
                    break;
                case OnMQTTEventUnsubscribeResponse:
                    mqtt_handle_unsubscribe_response_event();
                    break;
                case OnMQTTEventPublishResponse:
                    mqtt_handle_publish_response_event();
                    break;
                case OnMQTTEventDisconnectResponse:
                    teardown_mqtt_connect();
                    break;
                case OnApplicationConsumedMessage:
                    if(application_processing_message) {
                        mqtt_acknowledge_message(correlation_id);

                        if (mqtt_message_pending) {
                            mqtt_message_pending = false;
                            if(!get_mqtt_message()) {
                                server_error("reading mqtt message failed");
                                mqtt_disconnect();
                            } else {
                                pushApplicationMessage(OnIncomingMqttMessage);
                            }
                        } else {
                            application_processing_message = false;
                     }
                    }
                  break;

                case OnApplicationProducedMessage:
                  publish_message(application_message_payload);
                  pushApplicationMessage(OnMqttMessageSent);
                  break;

                default:
                    server_error("received a message we haven't implemented yet: %d", messageType);
                    break;
            }
        }
    }
}

/**
 * @brief Configure notification center for all work helper initiated network operations.
 */
void configure_work_notification_center() {
    if (work_notification_center_handle != 0) {
        return;
    }

    memset((void *)work_notification_buffer, 0xff, sizeof(work_notification_buffer));

    static struct MvNotificationSetup notification_config = {
        .irq = WORK_NOTIFICATION_IRQ,
        .buffer = (struct MvNotification *)work_notification_buffer,
        .buffer_size = sizeof(work_notification_buffer)
    };

    // Configure notifications with Microvisor
    enum MvStatus status = mvSetupNotifications(&notification_config, &work_notification_center_handle);
    assert(status == MV_STATUS_OKAY);

    // Enable the notification IRQ via the HAL
    NVIC_ClearPendingIRQ(WORK_NOTIFICATION_IRQ);
    NVIC_EnableIRQ(WORK_NOTIFICATION_IRQ);
}

bool get_mqtt_message() {
    uint32_t _qos;
    uint8_t _retain;
    return mqtt_get_received_message_data(&correlation_id,
                                          &incoming_message_topic, &incoming_message_topic_len,
                                          &incoming_message_payload, &incoming_message_payload_len,
                                          &_qos, &_retain);
 
}

/**
 * @brief Handle network notification events
 */
void TIM8_BRK_IRQHandler(void) {
    volatile struct MvNotification notification = work_notification_buffer[current_work_notification_index];

    if (notification.tag == TAG_CHANNEL_CONFIG) {
        switch (notification.event_type) {
            case MV_EVENTTYPE_CHANNELDATAREADABLE:
                pushWorkMessage(OnConfigRequestReturn);
                break;
            case MV_EVENTTYPE_CHANNELNOTCONNECTED:
                if (wait_for_config) {
                    pushWorkMessage(OnConfigFailed);
                }
                break;
            default:
                break;
        }
    } else if (notification.tag == TAG_CHANNEL_MQTT) {
        switch (notification.event_type) {
            case MV_EVENTTYPE_CHANNELDATAREADABLE:
                pushWorkMessage(OnMQTTReadable);
                break;
            case MV_EVENTTYPE_CHANNELNOTCONNECTED:
                if (mqtt_connection_active) {
                    pushWorkMessage(OnBrokerDroppedConnection);
                }
                break;
            case MV_EVENTTYPE_CHANNELDATAWRITESPACE: // NOTE: this may be removed in a future kernel release
                break;
            default:
                break;
        }
    }

    // Point to the next record to be written
    current_work_notification_index = (current_work_notification_index + 1) % WORK_NOTIFICATION_BUFFER_COUNT;

    // Clear the current notifications event
    // See https://www.twilio.com/docs/iot/microvisor/microvisor-notifications#buffer-overruns
    notification.event_type = 0;
}
