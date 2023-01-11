/**
 *
 * Microvisor Config Handler 
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#include "config_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"

#include "work.h"
#include "network_helper.h"
#include "log_helper.h"

uint8_t  client[BUF_CLIENT_SIZE];
size_t   client_len;

uint8_t  broker_host[BUF_BROKER_HOST] = {0};
size_t   broker_host_len = 0;
uint16_t broker_port = 0;
uint8_t  root_ca[BUF_ROOT_CA] = {0};
size_t   root_ca_len = 0;
uint8_t  cert[BUF_CERT] = {0};
size_t   cert_len = 0;
uint8_t  private_key[BUF_PRIVATE_KEY] = {0};
size_t   private_key_len = 0;

static MvChannelHandle configuration_channel = 0;

/**
 * FORWARD DECLARATIONS
 */
static bool consume_binary_config_value(
        MvChannelHandle *configuration_channel,
        struct MvConfigResponseReadItemParams *item,
        uint8_t *destination,
        size_t destination_size,
        size_t *destination_len);

/**
 * CONFIGURATION OPERATIONS
 */

/*
 * @brief Open channel for configuration tasks
 */
void start_configuration_fetch() {
    mvGetDeviceId(client, BUF_CLIENT_SIZE);
    client_len = BUF_CLIENT_SIZE;

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
            STRING_ITEM(endpoint, "")
        }
    };

    enum MvStatus status;
    if ((status = mvOpenChannel(&ch_params, &configuration_channel)) != MV_STATUS_OKAY) {
        server_error("encountered error opening config channel: %x", status);
        pushWorkMessage(OnConfigFailed);
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
        .num_items = sizeof(items)/sizeof(struct MvConfigKeyToFetch),
        .keys_to_fetch = items,
    };

    server_log("sending config request...");
    if ((status = mvSendConfigFetchRequest(configuration_channel, &request)) != MV_STATUS_OKAY) {
        server_error("encountered an error requesting config: %x", status);
        pushWorkMessage(OnConfigFailed);
    }
}

void receive_configuration_items() {
    server_log("receiving configuration results");

    uint8_t read_buffer[BUF_READ_BUFFER] __attribute__ ((aligned(512))); // needs to support largest config item, likely string encoded hex for private key (~2400 bytes)
    uint32_t read_buffer_used = 0;

    struct MvConfigResponseData response;

    enum MvStatus status;
    if ((status = mvReadConfigFetchResponseData(configuration_channel, &response)) != MV_STATUS_OKAY) {
        server_error("encountered an error fetching configuration response");
        pushWorkMessage(OnConfigFailed);
        return;
    }

    if (response.result != MV_CONFIGFETCHRESULT_OK) {
        server_error("received different result from config fetch than expected: %d", response.result);
        pushWorkMessage(OnConfigFailed);
        return;
    }

    if (response.num_items != 5) {
        server_error("received different number of items than expected 5 != %d", response.num_items);
        pushWorkMessage(OnConfigFailed);
        return;
    }

    enum MvConfigKeyFetchResult result;
    struct MvConfigResponseReadItemParams item = {
        .item_index = 0,
        .result = &result,
        .buf = {
            .data = read_buffer,
            .size = sizeof(read_buffer) - 1,
            .length = &read_buffer_used
        }
    };
    bool consume_binary_success=false;


    item.item_index = 0;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d (MvConfigFetchResult)", item.item_index, status);
        pushWorkMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d (MvConfigKeyFetchResult)", item.item_index, result);
        pushWorkMessage(OnConfigFailed);
        return;
    }
    memcpy(broker_host, read_buffer, read_buffer_used);
    broker_host_len = read_buffer_used;
    server_log("broker_host: %.*s", broker_host_len, broker_host);


    item.item_index = 1;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item.item_index, status);
        pushWorkMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item.item_index, result);
        pushWorkMessage(OnConfigFailed);
        return;
    }
    read_buffer[read_buffer_used] = '\0';
    broker_port = strtol((const char *)read_buffer, NULL, 10);
    server_log("broker_port: %d (uint16_t)", broker_port);


    item.item_index = 2;
    consume_binary_success = consume_binary_config_value( &configuration_channel, &item, root_ca, BUF_ROOT_CA, &root_ca_len);
    if (!consume_binary_success) {
        pushWorkMessage(OnConfigFailed);
        return;
    }
    server_log("root_ca[%d] = 0x%02x", root_ca_len-1, root_ca[root_ca_len-1]);


    item.item_index = 3;
    consume_binary_success = consume_binary_config_value( &configuration_channel, &item, cert, BUF_CERT, &cert_len);
    if (!consume_binary_success) {
        pushWorkMessage(OnConfigFailed);
        return;
    }
    server_log("cert[%d] = 0x%02x", cert_len-1, cert[cert_len-1]);

    item.item_index = 4;
    consume_binary_success = consume_binary_config_value( &configuration_channel, &item, private_key, BUF_PRIVATE_KEY, &private_key_len);
    if (!consume_binary_success) {
        pushWorkMessage(OnConfigFailed);
        return;
    }
    server_log("private_key[%d] = 0x%02x", private_key_len-1, private_key[private_key_len-1]);


    pushWorkMessage(OnConfigObtained);
}

bool consume_binary_config_value(
        MvChannelHandle *configuration_channel,
        struct MvConfigResponseReadItemParams *item,
        uint8_t *destination,
        size_t destination_size,
        size_t *destination_len
        ) {
    enum MvStatus status;

    server_log("fetching item %d", item->item_index);
    if ((status = mvReadConfigResponseItem(*configuration_channel, item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d", item->item_index, status);
        return false;
    }
    if (*(item->result) != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d", item->item_index, *(item->result));
        return false;
    }
    if (*(item->buf.length) / 2 > destination_size) {
        server_error("target buffer (%d) insufficient for read data (%d)", destination_size, *(item->buf.length)/2);
        return false;
    }
    *destination_len = 0;
    for (int ndx=0; ndx < *(item->buf.length); ndx+=2) {
        uint8_t val = 0;

        for (int n=0; n<2; n++) {
            char byte = item->buf.data[ndx+n];
            if (byte >= '0' && byte <= '9') byte = byte - '0';
            else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
            else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
            val = (val << 4) | (byte & 0xf);
        }

        destination[*destination_len] = val;
        *destination_len = *destination_len + 1;
    }

    return true;
}

void finish_configuration_fetch() {
    server_log("closing configuration channel");
    mvCloseChannel(&configuration_channel);
}
