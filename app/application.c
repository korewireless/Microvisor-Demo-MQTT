#include "application.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"
#include "cmsis_os.h"

#include "work.h"
#include "log_helper.h"

#if defined(APPLICATION_TEMPERATURE)
#  include "i2c_helper.h"
#endif

#if defined(APPLICATION_SWITCH)
#  include "switch_helper.h"
#endif
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
 *  APPLICATION_SPECIFIC DATA
 */
#if defined(APPLICATION_DUMMY)
static uint64_t last_send_microsec = 0;
static float sensor_data = 0.0;
static bool application_running = true;
#elif defined(APPLICATION_TEMPERATURE)
static uint64_t last_send_microsec = 0;
static bool application_running = true;
static bool i2c_initialised = true;

#define TH02_ADDR 0x80

#define TH02_STATUS_ADDR 0
#define TH02_DATAH_ADDR 1
#define TH02_CONFIG_ADDR 3

#define TH02_CONFIG_START 0x01
#define TH02_CONFIG_TEMP 0x10

#define TH02_STATUS_RDY 0x01
#endif

/**
 * @brief Push message into application queue.
 *
 * @param  type: ApplicationMessageType enumeration value
 */
void pushApplicationMessage(enum ApplicationMessageType type) {
    osStatus_t status;
    if ((status = osMessageQueuePut(applicationMessageQueue, &type, 0U, 0U)) != osOK) {
        server_error("failed to post application message: %ld", status);
    }
}

/**
 * @brief Function implementing the Application task thread.
 *
 * @param  argument: Not used.
 */
void start_application_task(void *argument) {
    applicationMessageQueue = osMessageQueueNew(16, sizeof(enum ApplicationMessageType), NULL);
    if (applicationMessageQueue == NULL) {
        server_error("failed to create queue");
        return;
    }

    application_init();

    enum ApplicationMessageType messageType;
    
    // The task's main loop
    while (1) {
        if (osMessageQueueGet(applicationMessageQueue, &messageType, NULL, 100U ) == osOK) {
            // server_log("application event loop received a message: 0x%02x", messageType);
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

        application_poll();
    }
}

#if defined(APPLICATION_DUMMY)
void application_init() {
    last_send_microsec = 0;
    sensor_data = 1.0;
    application_running = true;
}

void application_poll() {
    uint64_t current_microsec = 0;
    mvGetMicroseconds(&current_microsec);

    if (application_running && mqtt_connected && !message_in_flight && ((current_microsec - last_send_microsec) > 60*1000*1000)) { // trigger approx every 60 seconds, depending on how chatty other messages are (relying on 100ms timeout for osMessageQueueGet above)
       last_send_microsec = current_microsec;
       message_in_flight = true;

       sprintf(application_message_payload, "{\"temperature_celsius\":%.2f}", sensor_data);
#if defined(APPLICATION_DEBUGGING)
       server_log("publishing: %s", application_message_payload);
#endif
       pushWorkMessage(OnApplicationProducedMessage);

       sensor_data += 0.1;
       if (sensor_data > 50.0) {
            sensor_data = 1.0;
       }
    }
}

void application_process_message(const uint8_t* topic, size_t topic_len,
                                 const uint8_t* payload, size_t payload_len) {
    // server_log("Got a message on topic '%.*s' with payload '%.*s",
    //            (int) topic_len, topic,
    //            (int) payload_len, payload);
    if (strncmp("stop", (char*) payload, payload_len) == 0) {
        application_running = false;
    }

    if (strncmp("restart", (char*) payload, payload_len) == 0) {
        application_running = true;
    }
}

#elif defined(APPLICATION_TEMPERATURE)
void application_init() {
    last_send_microsec = 0;
    application_running = true;

    i2c_initialised = i2c_init();

    if (!i2c_initialised) {
        server_error("Failed to initialise I2C, application will not be running");
    }
}

static bool get_temperature(float *temperature) {
    // Read temperature from TH02 sensor, see datasheet for the procedure
    uint8_t reg[2];

    reg[0] = TH02_CONFIG_START | TH02_CONFIG_TEMP;
    if (!i2c_write_reg(TH02_ADDR, TH02_CONFIG_ADDR, false, reg, 1)) {
        return false;
    }

    while (true) {
        if (!i2c_read_reg(TH02_ADDR, TH02_STATUS_ADDR, false, reg, 1)) {
            return false;
        }

        if (!(reg[0] & TH02_STATUS_RDY)) {
            break;
        }
    }

    if (!i2c_read_reg(TH02_ADDR, TH02_DATAH_ADDR, false, reg, 2)) {
        return false;
    }

    int temperature_readout = (reg[0] << 6) | (reg[1] >> 2);
    *temperature = (((float) temperature_readout) / 32.0) - 50.0;

    return true;
}

void application_poll() {
    uint64_t current_microsec = 0;
    mvGetMicroseconds(&current_microsec);

    if (application_running && mqtt_connected && !message_in_flight && ((current_microsec - last_send_microsec) > 60*1000*1000)) { // trigger approx every 60 seconds, depending on how chatty other messages are (relying on 100ms timeout for osMessageQueueGet above)
       last_send_microsec = current_microsec;
       message_in_flight = true;

       float temperature = 0.0;

       if (get_temperature(&temperature)) {
           // server_log("Temperature is %f", temperature);
           sprintf(application_message_payload, "{\"temperature_celsius\":%.2f}", temperature);
           pushWorkMessage(OnApplicationProducedMessage);
       } else {
           server_error("Failed to read temperature from sensor");
       }
    }
}

void application_process_message(const uint8_t* topic, size_t topic_len,
                                 const uint8_t* payload, size_t payload_len) {
    // server_log("Got a message on topic '%.*s' with payload '%.*s",
    //            (int) topic_len, topic,
    //            (int) payload_len, payload);
    if (strncmp("stop", (char*) payload, payload_len) == 0) {
        application_running = false;
    }

    if (strncmp("restart", (char*) payload, payload_len) == 0) {
        application_running = true;
    }
}

#elif defined(APPLICATION_SWITCH)
void application_init() {
    switch_init();
}

void application_poll() {
}

static inline bool is_whitespace(char c) {
    switch(c) {
        case ' ':
	case '\t':
	case '\r':
	case '\n':
	    return true;
	default:
	    return false;
    }
}

static inline size_t skip_whitespace(const uint8_t* str, size_t index, size_t len) {
    for(; index < len; index++) {
        if (!is_whitespace(str[index])) {
            break;
	}
    }
    return index;
}

void application_process_message(const uint8_t* topic, size_t topic_len,
                                 const uint8_t* payload, size_t payload_len) {
    // server_log("Got a message on topic '%.*s' with payload '%.*s",
    //            (int) topic_len, topic,
    //            (int) payload_len, payload);

    const char* action_str = strnstr((const char*)payload, "\"action\"", payload_len);
    if (action_str == NULL) {
       return;
    }

    size_t index = (size_t) (action_str - (const char*) payload);
    index += strlen("\"action\"");

    index = skip_whitespace(payload, index, payload_len);

    if (index == payload_len || payload[index] != ':') {
	return; // colon not found
    }

    index = skip_whitespace(payload, index+1, payload_len);

    size_t rest_len = payload_len - index;
    const char* close_command = "\"switch_close\"";
    const char* open_command = "\"switch_open\"";
    if (rest_len < strlen(open_command)) {
        return;
    }
    if (strncmp(open_command, (char*) &payload[index], strlen(open_command)) == 0) {
        switch_open();
    }

    if (rest_len < strlen(close_command)) {
        return;
    }

    if (strncmp(close_command, (char*) &payload[index], strlen(close_command)) == 0) {
        switch_close();
	return;
    }
}

#endif

