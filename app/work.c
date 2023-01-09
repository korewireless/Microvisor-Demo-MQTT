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
void process_payload(const uint8_t* topic, size_t topic_len,
                     const uint8_t* payload, size_t payload_len);


/*
 * STORAGE
 */
osMessageQueueId_t workMessageQueue;

static volatile struct MvNotification work_notification_buffer[WORK_NOTIFICATION_BUFFER_COUNT] __attribute__((aligned(8)));
static volatile uint32_t current_work_notification_index = 0;
MvNotificationHandle  work_notification_center_handle = 0;

uint8_t work_send_buffer[BUF_SEND_SIZE] __attribute__ ((aligned(512))); // shared by config and mqtt as only one is active at a time
uint8_t work_receive_buffer[BUF_RECEIVE_SIZE] __attribute__ ((aligned(512))); // shared by config and mqtt as only one is active at a time

bool connected = false;
bool running = true;
float sensor_data = 1.0;
int publish_counter = 0;

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
            server_log("event loop received a message: 0x%02x", messageType);
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
                    // TODO: store config in some nonvolatile storage
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
		    connected = true;
                    break;
                case OnBrokerSubscribeFailed:
                    server_log("subscription failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerUnsubscribeSucceeded:
                    server_log("unsubscription was successful");
                    break;
                case OnBrokerUnsubscribeFailed:
                    server_log("unsubscription failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerPublishSucceeded:
                    server_log("publish was successful");
                    break;
                case OnBrokerPublishFailed:
                    server_log("publish failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerMessageAcknowledgeFailed:
                    server_log("message acknowledgement failed");
                    mqtt_disconnect();
                    break;
                case OnBrokerConnectFailed:
                    server_log("cleaning up mqtt broker...");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerDisconnectFailed:
                    server_log("couldn't disconnect gracefully, closing the channel");
                    teardown_mqtt_connect();
                    break;
                case OnBrokerDisconnected:
		    connected = false;
                    server_log("mqtt channel closed");
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
		{
                    uint32_t correlation_id;
                    uint8_t *payload;
                    uint32_t payload_len;
                    uint8_t *topic;
                    uint32_t topic_len;
                    uint32_t _qos;
                    uint8_t _retain;
                    if (mqtt_get_received_message_data(&correlation_id,
                                                       &topic, &topic_len,
                                                       &payload, &payload_len,
                                                       &_qos, &_retain)) {
                        process_payload(topic, topic_len, payload, payload_len);
                    } else {
                        server_log("reading mqtt message failed");
                        mqtt_disconnect();
                    }
                    mqtt_acknowledge_message(correlation_id);
                    break;
		}
                case OnMQTTEventMessageLost:
                    if (!mqtt_handle_lost_message_data()) {
                        server_log("handling lost mqtt message failed");
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
                    server_log("Disconnected from MQTT broker gracefully");
                    teardown_mqtt_connect();
                    break;
                default:
                    server_error("received a message we haven't implemented yet: %d", messageType);
                    break;
            }
        }

        osDelay(100);
	publish_counter++;
        server_log("tick %d %d %d", running, connected, publish_counter);
	if (running && connected && (publish_counter >= 10)) {
	    publish_counter = 0;
            publish_sensor_data(sensor_data);

	    sensor_data += 0.1;
	    if (sensor_data > 50.0) {
		sensor_data = 1.0;
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

void process_payload(const uint8_t* topic, size_t topic_len,
                     const uint8_t* payload, size_t payload_len) {
    server_log("Got a message on topic '%.*s' with payload '%.*s",
               (int) topic_len, topic,
               (int) payload_len, payload);
    if (strncmp("stop", payload, payload_len) == 0) {
        running = false;
    }

    if (strncmp("restart", payload, payload_len) == 0) {
        running = true;
    }
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
