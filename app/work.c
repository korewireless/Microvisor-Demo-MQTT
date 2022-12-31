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
void configure_work_notification_center();


/*
 * STORAGE
 */
osMessageQueueId_t workMessageQueue;

static volatile struct MvNotification work_notification_buffer[WORK_NOTIFICATION_BUFFER_COUNT] __attribute__((aligned(8)));
static volatile uint32_t current_work_notification_index = 0;
MvNotificationHandle  work_notification_center_handle = 0;

uint8_t work_send_buffer[BUF_SEND_SIZE] __attribute__ ((aligned(512))); // shared by config and mqtt as only one is active at a time
uint8_t work_receive_buffer[BUF_RECEIVE_SIZE] __attribute__ ((aligned(512))); // shared by config and mqtt as only one is active at a time


/**
 * @brief Push message into work queue.
 *
 * @param  type: MessageType enumeration value
 */
void pushMessage(enum MessageType type) {
    osStatus_t status;
    if ((status = osMessageQueuePut(workMessageQueue, &type, 0U, 0U)) != osOK) { // osWaitForever might be better for some messages?
        server_log("failed to post message: %ld", status);
    }
}

/**
 * @brief Function implementing the Work task thread.
 *
 * @param  argument: Not used.
 */
void start_work_task(void *argument) {
    server_log("starting work task...");
    
    configure_work_notification_center();

	workMessageQueue = osMessageQueueNew(16, sizeof(enum MessageType), NULL);
    if (workMessageQueue == NULL) {
        server_log("failed to create queue");
    }

    pushMessage(ConnectNetwork);
    
    enum MessageType messageType;
    
    // The task's main loop
    while (1) {
        osDelay(1);

		if (osMessageQueueGet(workMessageQueue, &messageType, NULL, 100U /*osWaitForever*/) == osOK) {
            server_log("received a message: 0x%02x", messageType);
            switch (messageType) {
                case ConnectNetwork:
                    server_log("connecting network...");
                    want_network = true;
                    break;
                case OnNetworkConnected:
                    server_log("we now have network");
                    pushMessage(PopulateConfig);
                    break;
                case OnNetworkDisconnected:
                    server_log("we lost network");
                    // TODO: any teardown here?
                    break;
                case PopulateConfig:
                    server_log("let's obtain the config from the server now...");
                    start_configuration_fetch();
                    break;
                case OnConfigRequestReturn:
                    server_log("we received a config response, process it");
                    receive_configuration_items();
                    break;
                case OnConfigObtained:
                    finish_configuration_fetch();
                    server_log("obtained configuration from server");
                    pushMessage(ConnectMQTTBroker);
                    break;
                case OnConfigFailed:
                    finish_configuration_fetch();
                    server_error("we failed to obtain the needed configuration");
                    break;
                case ConnectMQTTBroker:
                    server_log("connecting mqtt broker...");
                    start_mqtt_connect();
                    break;
                case OnBrokerConnected:
                    server_log("subscribing to topics...");
                    start_subscriptions();
                    break;
                case OnBrokerSubscribeSucceeded:
                    server_log("subscription was successful");
                    break;
                case OnBrokerSubscribeFailed:
                    server_log("subscription failed");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerUnsubscribeSucceeded:
                    server_log("unsubscription was successful");
                    break;
                case OnBrokerUnsubscribeFailed:
                    server_log("unsubscription failed");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerConnectFailed:
                    server_log("cleaning up mqtt broker...");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerDisconnected:
                    // FIXME: impl
                    break;

                case OnMQTTReadable:
                    server_log("processing mqtt connection readable event...");
                    mqtt_handle_readable_event();
                    break;
                case OnMQTTEventConnectResponse:
                    server_log("processing mqtt connection response event...");
                    mqtt_handle_connect_response_event();
                    break;
                case OnMQTTEventMessageReceived:
                    // FIXME: impl
                    break;
                case OnMQTTEventMessageLost:
                    // FIXME: impl
                    break;
                case OnMQTTEventSubscribeResponse:
                    mqtt_handle_subscribe_response_event();
                    break;
                case OnMQTTEventUnsubscribeResponse:
                    mqtt_handle_unsubscribe_response_event();
                    break;
                case OnMQTTEventPublishResponse:
                    // FIXME: impl
                    break;
                case OnMQTTEventDisconnectResponse:
                    // FIXME: impl
                    break;
                default:
                    server_error("received a message we haven't implemented yet: %d", messageType);
                    break;
            }
        }

        osDelay(100);
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

/**
 * @brief Handle network notification events
 */
void TIM8_BRK_IRQHandler(void) {
    volatile struct MvNotification notification = work_notification_buffer[current_work_notification_index];

    server_log("received channel tag %d notification event_type 0x%02x", notification.tag, notification.event_type);
    if (notification.tag == TAG_CHANNEL_CONFIG) {
        // FIXME: differentiate event_types
        pushMessage(OnConfigRequestReturn);
    } else if (notification.tag == TAG_CHANNEL_MQTT) {
        if (notification.event_type == MV_EVENTTYPE_CHANNELDATAREADABLE) {
            pushMessage(OnMQTTReadable);
        }
        // FIXME: handle other event_types
    }

    // Point to the next record to be written
    current_work_notification_index = (current_work_notification_index + 1) % WORK_NOTIFICATION_BUFFER_COUNT;

    // Clear the current notifications event
    // See https://www.twilio.com/docs/iot/microvisor/microvisor-notifications#buffer-overruns
    notification.event_type = 0;
}
