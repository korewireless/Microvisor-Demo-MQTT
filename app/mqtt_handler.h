/**
 *
 * Microvisor MQTT Handler
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */


#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


/*
 * DEFINES
 */
#define TAG_CHANNEL_MQTT 101
#define CREDENTIAL_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PROTOTYPES
 */
void start_mqtt_connect();
bool is_broker_connected();
void start_subscriptions();
void end_subscriptions();
void publish_message(const char *payload);
void teardown_mqtt_connect();

void mqtt_handle_readable_event();
void mqtt_handle_connect_response_event();
void mqtt_handle_subscribe_response_event();
void mqtt_handle_unsubscribe_response_event();
void mqtt_handle_publish_response_event();
void mqtt_disconnect();
bool mqtt_get_received_message_data(uint32_t *correlation_id,
                                    uint8_t **topic, uint32_t *topic_len,
                                    uint8_t **payload, uint32_t *payload_len,
                                    uint32_t *qos, uint8_t *retain);
bool mqtt_handle_lost_message_data();
void mqtt_acknowledge_message(uint32_t correlation_id);

#ifdef __cplusplus
}
#endif


#endif /* MQTT_HANDLER_H */
