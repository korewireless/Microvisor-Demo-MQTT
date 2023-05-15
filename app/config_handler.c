/**
 *
 * Microvisor Config Handler 
 * Version 1.1.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */

#include "config_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "work.h"
#include "network_helper.h"
#include "log_helper.h"

static MvChannelHandle configuration_channel = 0;

/**
 * CONFIGURATION OPERATIONS
 */

/*
 * @brief Open channel for configuration tasks
 */
void start_configuration_fetch(const struct ConfigHelperItem *items, uint8_t count) {
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

    struct MvConfigKeyToFetch config_items[count];
    memset(config_items, 0, sizeof(config_items));
    for (int ndx=0; ndx<count; ndx++) {
        memcpy(&config_items[ndx], &items[ndx].item, sizeof(struct MvConfigKeyToFetch));
    }

    struct MvConfigKeyFetchParams request = {
        .num_items = count,
        .keys_to_fetch = config_items,
    };

#if defined(CONFIG_DEBUGGING)
    server_log("requesting %d configuration items", count);
#endif

    if ((status = mvSendConfigFetchRequest(configuration_channel, &request)) != MV_STATUS_OKAY) {
        server_error("encountered an error requesting config: %x", status);
        pushWorkMessage(OnConfigFailed);
    }
}

void receive_configuration_items(struct ConfigHelperItem *items, uint8_t count) {
#if defined(CONFIG_DEBUGGING)
    server_log("receiving %d configuration results", count);
#endif

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

    if (response.num_items != count) {
        server_error("received different number of items than expected %d != %d", count, response.num_items);
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

    for (int ndx=0; ndx<count; ndx++) {
        item.item_index = ndx;
#if defined(CONFIG_DEBUGGING)
        server_log("fetching item %d", item.item_index);
#endif

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

        switch (items[ndx].config_type) {
            case CONFIG_ITEM_TYPE_UINT8:
                {
                    if (read_buffer_used > items[ndx].u8_item.buf_size) {
                        server_error("received config item # %d is too large to populate into buffer - %d > %d", ndx, read_buffer_used, items[ndx].u8_item.buf_size);
                        pushWorkMessage(OnConfigFailed);
                        return;
                    }
                    memcpy(items[ndx].u8_item.buf, read_buffer, read_buffer_used);
                    *items[ndx].u8_item.buf_len = read_buffer_used;
#if defined(CONFIG_DEBUGGING)
                    server_log("item[%d]: %.*s", ndx, *items[ndx].u8_item.buf_len, items[ndx].u8_item.buf);
#endif

                    break;
                }
            case CONFIG_ITEM_TYPE_B64:
                {
                    if (read_buffer_used / 2 > items[ndx].u8_item.buf_size) {
                        server_error("received config item # %d is too large to b64 decode into buffer - %d > %d", ndx, read_buffer_used, items[ndx].u8_item.buf_size);
                        pushWorkMessage(OnConfigFailed);
                        return;
                    }

                    *items[ndx].u8_item.buf_len = 0;

                    for (int read_ndx=0; read_ndx < read_buffer_used; read_ndx+=2) {
                        uint8_t val = 0;

                        for (int n=0; n<2; n++) {
                            char byte = read_buffer[read_ndx+n];
                            if (byte >= '0' && byte <= '9') byte = byte - '0';
                            else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
                            else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
                            val = (val << 4) | (byte & 0xf);
                        }

                        items[ndx].u8_item.buf[*(items[ndx].u8_item.buf_len)] = val;
                        *(items[ndx].u8_item.buf_len) = *(items[ndx].u8_item.buf_len) + 1;
                    }
#if defined(CONFIG_DEBUGGING)
                    server_log("item[%d][%d] = 0x%02x", ndx, (*items[ndx].u8_item.buf_len)-1, items[ndx].u8_item.buf[(*items[ndx].u8_item.buf_len)-1]);
#endif

                    break;
                }
            case CONFIG_ITEM_TYPE_ULONG:
                {
                    read_buffer[read_buffer_used] = '\0';
                    *(items[ndx].ulong_item.val) = strtoul((const char *)read_buffer, NULL, 10);
#if defined(CONFIG_DEBUGGING)
                    server_log("item[%d] = %d (uint16_t)", ndx, *(items[ndx].ulong_item.val));
#endif

                    break;
                }
            case CONFIG_ITEM_TYPE_LONG:
                {
                    read_buffer[read_buffer_used] = '\0';
                    *(items[ndx].long_item.val) = strtol((const char *)read_buffer, NULL, 10);
#if defined(CONFIG_DEBUGGING)
                    server_log("item[%d] = %d (int16_t)", ndx, *(items[ndx].long_item.val));
#endif

                    break;
                }
        };
    }

    pushWorkMessage(OnConfigObtained);
}

void finish_configuration_fetch() {
#if defined(CONFIG_DEBUGGING)
    server_log("closing configuration channel");
#endif
    mvCloseChannel(&configuration_channel);
}
