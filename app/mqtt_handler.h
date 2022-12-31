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
void publish_sensor_data(float temperature);
void teardown_mqtt_connect();

void mqtt_handle_readable_event();
void mqtt_handle_connect_response_event();
void mqtt_handle_subscribe_response_event();
void mqtt_handle_unsubscribe_response_event();
void mqtt_handle_publish_response_event();


#ifdef __cplusplus
}
#endif


#endif /* MQTT_HANDLER_H */
