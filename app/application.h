/**
 *
 * Microvisor MQTT Communications Demo
 * Version 1.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TYPES
 */
enum ApplicationMessageType {
    OnMqttConnected,
    OnMqttDisconnected,
    OnIncomingMqttMessage,
    OnMqttMessageSent
};
  
/*
 * PROTOTYPES
 */
void start_application_task(void *argument);
void pushApplicationMessage(enum ApplicationMessageType type);

/*
 * GLOBALS
 */
extern char application_message_payload[128];
#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_H */
