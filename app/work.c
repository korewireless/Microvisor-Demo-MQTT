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

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"

#include "log_helper.h"
#include "network_helper.h"
#include "config_helper.h"


/*
 * CONFIGURTATION
 */
#define WORK_NOTIFICATION_BUFFER_COUNT 8
// If you change the IRQ here, you must also rename the Handler function at the bottom of the page
// which the HAL calls.
#define WORK_NOTIFICATION_IRQ TIM8_BRK_IRQn

#define TAG_CHANNEL_CONFIG 100

/*
 * FORWARD DECLARATIONS
 */
void start_configuration_fetch();
void receive_configuration_items();
void finish_configuration_fetch();


/*
 * STORAGE
 */
osMessageQueueId_t workMessageQueue;


void pushMessage(enum MessageType type) {
    osStatus_t status;
    if ((status = osMessageQueuePut(workMessageQueue, &type, 0U, 0U)) != osOK) { // osWaitForever might be better for some messages?
        server_log("failed to post message: %ld", status);
    } else {
        server_log("DEBUG: posted message %d", type);
    }
}

/**
 * @brief Function implementing the Work task thread.
 *
 * @param  argument: Not used.
 */
void start_work_task(void *argument) {
    server_log("starting work task...");

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
            server_log("received a message: %d", messageType);
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
                    server_log("we obtained the needed configuration, lets make use of it");
                    break;
                case OnConfigFailed:
                    finish_configuration_fetch();
                    server_error("we failed to obtain the needed configuration");
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
 * CONFIGURATION OPERATIONS
 */

/*
 * @brief Open channel for configuration tasks
 */
static volatile struct MvNotification work_notification_buffer[WORK_NOTIFICATION_BUFFER_COUNT] __attribute__((aligned(8)));
static volatile uint32_t              current_work_notification_index = 0;
static MvNotificationHandle           work_notification_center_handle = 0;

static uint8_t                        work_send_buffer[1024] __attribute__ ((aligned(512)));
static uint8_t                        work_receive_buffer[7*1024] __attribute__ ((aligned(512)));
static uint8_t                        read_buffer[3*1024] __attribute__ ((aligned(512)));

static uint8_t                        broker_host[128];
static size_t                         broker_host_len;
static uint16_t                       broker_port;
static uint8_t                        root_ca[1024];
static size_t                         root_ca_len;
static uint8_t                        cert[1024];
static size_t                         cert_len;
static uint8_t                        private_key[1536];
static size_t                         private_key_len;

static MvChannelHandle configuration_channel = 0;

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

#define STRING_ITEM(name, str) \
    . name = (uint8_t*)str, . name ## _len = strlen(str)
void start_configuration_fetch() {
    configure_work_notification_center();

    MvNetworkHandle network_handle = get_network_handle();
    struct MvOpenChannelParams ch_params = {
        .version = 1,
        .v1 = {
            .notification_handle = work_notification_center_handle,
            .notification_tag = TAG_CHANNEL_CONFIG,
            .network_handle = network_handle,
            .receive_buffer = (uint8_t*)work_receive_buffer,
            .receive_buffer_len = sizeof(work_receive_buffer),
            .send_buffer = (uint8_t*)work_send_buffer,
            .send_buffer_len = sizeof(work_send_buffer),
            .channel_type = MV_CHANNELTYPE_CONFIGFETCH,
            .endpoint = (void *)"",
            .endpoint_len = 0
        }
    };

    enum MvStatus status;
    if ((status = mvOpenChannel(&ch_params, &configuration_channel)) != MV_STATUS_OKAY) {
        // report error
        server_error("encountered error opening config channel: %x", status);
        return;
    }

    struct MvConfigKeyToFetch items[] = {
        {
            .scope = MV_CONFIGKEYFETCHSCOPE_DEVICE,
            .store = MV_CONFIGKEYFETCHSTORE_CONFIG,
            STRING_ITEM(key, "broker-host"),
        },
        {
            .scope = MV_CONFIGKEYFETCHSCOPE_DEVICE,
            .store = MV_CONFIGKEYFETCHSTORE_CONFIG,
            STRING_ITEM(key, "broker-port"),
        },
        {
            .scope = MV_CONFIGKEYFETCHSCOPE_DEVICE,
            .store = MV_CONFIGKEYFETCHSTORE_CONFIG,
            STRING_ITEM(key, "root-CA"),
        },
        {
            .scope = MV_CONFIGKEYFETCHSCOPE_DEVICE,
            .store = MV_CONFIGKEYFETCHSTORE_CONFIG,
            STRING_ITEM(key, "cert"),
        },
        {
            .scope = MV_CONFIGKEYFETCHSCOPE_DEVICE,
            .store = MV_CONFIGKEYFETCHSTORE_SECRET,
            STRING_ITEM(key, "private_key"),
        },
    };

    struct MvConfigKeyFetchParams request = {
        .num_items = 5,
        .keys_to_fetch = items,
    };

    server_log("sending config request...");
    if ((status = mvSendConfigFetchRequest(configuration_channel, &request)) != MV_STATUS_OKAY) {
        server_error("encountered an error requesting config: %x", status);
        return;
    }
}
#undef STRING_ITEM

void receive_configuration_items() {
    server_log("receiving configuration results");

    struct MvConfigResponseData response;

    enum MvStatus status;
    if ((status = mvReadConfigFetchResponseData(configuration_channel, &response)) != MV_STATUS_OKAY) {
        server_error("encountered an error fetching configuration response");
        pushMessage(OnConfigFailed);
        return;
    }

    if (response.result != MV_CONFIGFETCHRESULT_OK) {
        server_error("received different result from config fetch than expected: %d", response.result);
        pushMessage(OnConfigFailed);
        return;
    }

    if (response.num_items != 5) {
        server_error("received different number of items than expected 5 != %d", response.num_items);
        pushMessage(OnConfigFailed);
        return;
    }

    uint32_t readBufUsed = 0;
    enum MvConfigKeyFetchResult result;
    struct MvConfigResponseReadItemParams item = {
        .item_index = 0,
        .result = &result,
        .buf = read_buffer,
        .size = sizeof(read_buffer) - 1,
        .length_used = &readBufUsed,
    };


    item.item_index = 0;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item.item_index, status);
        pushMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item.item_index, result);
        pushMessage(OnConfigFailed);
        return;
    }
    memcpy(broker_host, read_buffer, readBufUsed);
    broker_host[readBufUsed] = '\0';
    broker_host_len = readBufUsed;
    server_log("broker_host: %s", broker_host);


    item.item_index = 1;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item.item_index, status);
        pushMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item.item_index, result);
        pushMessage(OnConfigFailed);
        return;
    }
    read_buffer[readBufUsed] = '\0';
    broker_port = strtol((const char *)read_buffer, NULL, 10);
    server_log("broker_port: %d (uint16_t)", broker_port);


    item.item_index = 2;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item.item_index, status);
        pushMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item.item_index, result);
        pushMessage(OnConfigFailed);
        return;
    }
    root_ca_len = 0;
    for (int ndx=0; ndx < readBufUsed; ndx+=2) {
        uint8_t val = 0;

        for (int n=0; n<2; n++) {
            char byte = read_buffer[ndx+n];
            if (byte >= '0' && byte <= '9') byte = byte - '0';
            else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
            else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
            val = (val << 4) | (byte & 0xf);
        }

        root_ca[root_ca_len++] = val;
    }
    server_log("root_ca[%d] = 0x%02x", root_ca_len-1, root_ca[root_ca_len-1]);


    item.item_index = 3;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item.item_index, status);
        pushMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item.item_index, result);
        pushMessage(OnConfigFailed);
        return;
    }
    cert_len = 0;
    for (int ndx=0; ndx < readBufUsed; ndx+=2) {
        uint8_t val = 0;

        for (int n=0; n<2; n++) {
            char byte = read_buffer[ndx+n];
            if (byte >= '0' && byte <= '9') byte = byte - '0';
            else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
            else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
            val = (val << 4) | (byte & 0xf);
        }

        cert[cert_len++] = val;
    }
    server_log("cert[%d] = 0x%02x", cert_len-1, cert[cert_len-1]);


    item.item_index = 4;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item.item_index, status);
        pushMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item.item_index, result);
        pushMessage(OnConfigFailed);
        return;
    }
    private_key_len = 0;
    for (int ndx=0; ndx < readBufUsed; ndx+=2) {
        uint8_t val = 0;

        for (int n=0; n<2; n++) {
            char byte = read_buffer[ndx+n];
            if (byte >= '0' && byte <= '9') byte = byte - '0';
            else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
            else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
            val = (val << 4) | (byte & 0xf);
        }

        private_key[private_key_len++] = val;
    }
    server_log("private_key[%d] = 0x%02x", private_key_len-1, private_key[private_key_len-1]);


    pushMessage(OnConfigObtained);
}

void finish_configuration_fetch() {
    server_log("closing configuration channel");
    mvCloseChannel(&configuration_channel);
}

/**
 * @brief Handle network notification events
 */
/**
 *  @brief Network notification ISR.
 */
void TIM8_BRK_IRQHandler(void) {
    volatile struct MvNotification notification = work_notification_buffer[current_work_notification_index];

    if (notification.tag == TAG_CHANNEL_CONFIG) {
        pushMessage(OnConfigRequestReturn);
    }

    // Point to the next record to be written
    current_work_notification_index = (current_work_notification_index + 1) % WORK_NOTIFICATION_BUFFER_COUNT;

    // Clear the current notifications event
    // See https://www.twilio.com/docs/iot/microvisor/microvisor-notifications#buffer-overruns
    notification.event_type = 0;
}
