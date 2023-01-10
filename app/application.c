#include "application.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"
#include "cmsis_os.h"

#include "work.h"
#include "log_helper.h"

/*
 *  FORWARD DECLARATIONS
 */
static void application_init();
static void application_poll();
static void application_process_message(const uint8_t* topic, size_t topic_len,
                                        const uint8_t* payload, size_t payload_len);
/*
 *  GENERIC DATA
 */
char application_message_payload[128];

static bool mqtt_connected = false;
static bool message_in_flight = false;
osMessageQueueId_t applicationMessageQueue;

/*
 *  APPLICATION_SPECIFIG DATA
 */
#ifdef APPLICATION_DUMMY
static size_t publish_counter = 0;
static float sensor_data = 0.0;
static bool application_running = true;
#endif
/**
 * @brief Push message into application queue.
 *
 * @param  type: ApplicationMessageType enumeration value
 */
void pushApplicationMessage(enum ApplicationMessageType type) {
    osStatus_t status;
    if ((status = osMessageQueuePut(applicationMessageQueue, &type, 0U, 0U)) != osOK) {
        server_log("failed to post application message: %ld", status);
    }
}

/**
 * @brief Function implementing the Application task thread.
 *
 * @param  argument: Not used.
 */
void start_application_task(void *argument) {
    server_log("starting application task...");
    applicationMessageQueue = osMessageQueueNew(16, sizeof(enum ApplicationMessageType), NULL);
    if (applicationMessageQueue == NULL) {
        server_log("failed to create queue");
        return;
    }

    application_init();

    enum ApplicationMessageType messageType;
    
    // The task's main loop
    while (1) {
        if (osMessageQueueGet(applicationMessageQueue, &messageType, NULL, 100U ) == osOK) {
            server_log("application event loop received a message: 0x%02x", messageType);
            switch (messageType) {
                case OnMqttConnected:
                    mqtt_connected = true;
                    break;
                case OnMqttDisconnected:
                    mqtt_connected = false;
                    break;
                case OnIncomingMqttMessage:
                    application_process_message(incoming_message_topic,
                                                incoming_message_topic_len,
                                          incoming_message_payload,
                                          incoming_message_payload_len);
                  pushWorkMessage(OnApplicationConsumedMessage);
                    break;
              case OnMqttMessageSent:
                  message_in_flight = false;
                    break;
            }
        }

        osDelay(100);
       application_poll();
    }
}

#ifdef APPLICATION_DUMMY
void application_init() {
    publish_counter = 0;
    sensor_data = 1.0;
    application_running = true;
}

void application_poll() {
    publish_counter++;

    if (application_running && mqtt_connected && !message_in_flight && (publish_counter >= 10)) {
        publish_counter = 0;
       message_in_flight = true;

        sprintf(application_message_payload, "{\"temperature\":%.2f}", sensor_data);
       pushWorkMessage(OnApplicationProducedMessage);

       sensor_data += 0.1;
       if (sensor_data > 50.0) {
            sensor_data = 1.0;
       }
    }
}

void application_process_message(const uint8_t* topic, size_t topic_len,
                                 const uint8_t* payload, size_t payload_len) {
    server_log("Got a message on topic '%.*s' with payload '%.*s",
               (int) topic_len, topic,
               (int) payload_len, payload);
    if (strncmp("stop", (char*) payload, payload_len) == 0) {
        application_running = false;
    }

    if (strncmp("restart", (char*) payload, payload_len) == 0) {
        application_running = true;
    }
}

#endif

