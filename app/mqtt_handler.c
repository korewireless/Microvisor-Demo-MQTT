/**
 *
 * Microvisor MQTT Handler
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#include "mqtt_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"

#include "work.h"
#include "network_helper.h"
#include "log_helper.h"
#include "config_handler.h"

                        
static MvChannelHandle  mqtt_channel = 0;
static bool             broker_connected = false;
static uint32_t         correlation_id = 0;
static uint32_t         temp_num_items;

// Arrays to give to the work thread
static uint8_t in_topic[1024];
static uint8_t in_payload[1024];

const time_t SAS_TOKEN_LIFESPAN = 60*60*24*30;

/*
 * @brief Open channel for mqtt tasks
 */
void start_mqtt_connect() {
    MvNetworkHandle network_handle = get_network_handle();

    struct MvOpenChannelParams ch_params = {
        .version = 1,
        .v1 = {
            .notification_handle = work_notification_center_handle,
            .notification_tag = TAG_CHANNEL_MQTT,
            .network_handle = network_handle,
            .receive_buffer = (uint8_t*)work_receive_buffer,
            .receive_buffer_len = sizeof(work_receive_buffer),
            .send_buffer = (uint8_t*)work_send_buffer,
            .send_buffer_len = sizeof(work_send_buffer),
            .channel_type = MV_CHANNELTYPE_MQTT,
            STRING_ITEM(endpoint, "")
        }
    };

    enum MvStatus status;
    if ((status = mvOpenChannel(&ch_params, &mqtt_channel)) != MV_STATUS_OKAY) {
        // report error
        server_error("encountered error opening config channel: %x", status);
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }

    uint8_t azure_username[CREDENTIAL_SIZE] = {0};
    size_t azure_username_len = 0;
    uint8_t azure_password[CREDENTIAL_SIZE] = {0};
    size_t azure_password_len = 0;

    uint64_t now = 0;
    while (mvGetWallTime(&now) != MV_STATUS_OKAY) {} // we should have a valid time by now from the mv cloud, but make sure
    time_t expiry = (now/1000/1000) + SAS_TOKEN_LIFESPAN;

    if (!populate_azure_username(&azure_params, azure_username, &azure_username_len, CREDENTIAL_SIZE)) {
        server_error("mvMqttRequestConnect username generation failed");
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }

    if (!generate_azure_password(&azure_params, azure_password, &azure_password_len, CREDENTIAL_SIZE, expiry)) {
        server_error("mvMqttRequestConnect password generation failed");
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }

    struct MvSizedString username = {
        .data = (uint8_t *)azure_username,
        .length = azure_username_len
    };

    struct MvSizedString password = {
        .data = (uint8_t *)azure_password,
        .length = azure_password_len
    };

    struct MvMqttAuthentication authentication = {
        .method = MV_MQTTAUTHENTICATIONMETHOD_USERNAMEPASSWORD,
        .username_password = {
            .username = username,
            .password = password
        }
    };

    struct MvMqttConnectRequest request = {
        .protocol_version = MV_MQTTPROTOCOLVERSION_V3_1_1, // Azure is 3.1.1 only at this time
        .host = {
            .data = azure_params.hostname,
            .length = azure_params.hostname_len 
        },
        .port = 8883,
        .clientid = {
            .data = client,
            .length = client_len 
        },
        .authentication = authentication,
        .tls_credentials = NULL,
        .keepalive = 60,
        .clean_start = 0,
        .will = NULL,
    };

    status = mvMqttRequestConnect(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttRequestConnect returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }
}

bool is_broker_connected() {
    return broker_connected;
}

void start_subscriptions() {
    char topic_str[128];
    sprintf(topic_str, "devices/%.*s/messages/devicebound/", client_len, client);

    enum MvStatus status;

    const struct MvMqttSubscription subscriptions[] = {
        {
            .topic = {
                .data = (uint8_t *)topic_str,
                .length = strlen(topic_str)
            },
            .desired_qos = 0,
            .nl = 0,
            .rap = 0,
            .rh = 0,
        },
    };
    temp_num_items = sizeof(subscriptions)/sizeof(struct MvMqttSubscription);

    const struct MvMqttSubscribeRequest request = {
        .correlation_id = correlation_id++,
        .subscriptions = subscriptions,
        .num_subscriptions = temp_num_items,
    };

    status = mvMqttRequestSubscribe(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttRequestSubscribe returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerSubscriptionRequestFailed);
        return;
    }
}

void end_subscriptions() {
    char topic_str[128];
    sprintf(topic_str, "devices/%.*s/messages/devicebound/", client_len, client);

    enum MvStatus status;

    const struct MvSizedString topics[] = {
        {
            .data = (const uint8_t *)topic_str,
            .length = (uint16_t)strlen(topic_str),
        }
    };
    temp_num_items = sizeof(topics)/sizeof(struct MvSizedString);

    const struct MvMqttUnsubscribeRequest request = {
        .correlation_id = correlation_id++,
        .topics = topics,
        .num_topics = temp_num_items,
    };

    status = mvMqttRequestUnsubscribe(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttRequestUnsubscribe returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerUnsubscriptionRequestFailed);
        return;
    }
}

void publish_message(const char* payload) {
    char topic_str[128];
    sprintf(topic_str, "devices/%.*s/messages/events/", client_len, client); // For Azure, topic names are strictly defined.  device bound messages can be sent to 'devices/<device_id>/messages/devicebound/' and device sourced messages can be sent to 'devices/<device_id>/messages/events/'

    enum MvStatus status;

    const struct MvMqttPublishRequest request = {
        .correlation_id = correlation_id++,
        .topic = {
            .data = (uint8_t *)topic_str,
            .length = strlen(topic_str)
        },
        .payload = {
            .data = (uint8_t *)payload,
            .length = strlen(payload)
        },
        .desired_qos = 0,
        .retain = 0
    };

    status = mvMqttRequestPublish(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttRequestPublish returned 0x%02x\n", (int) status);
        if (status == MV_STATUS_RATELIMITED) {
            pushWorkMessage(OnBrokerPublishRateLimited);
        } else {
            pushWorkMessage(OnBrokerPublishFailed);
        }
        return;
    }

    server_log("published to %s", topic_str);
}

void mqtt_handle_readable_event() {
    enum MvMqttReadableDataType readableDataType;
    if (mvMqttGetNextReadableDataType(mqtt_channel, &readableDataType) != MV_STATUS_OKAY) {
        pushWorkMessage(OnMqttChannelFailed);
        return;
    }

    switch (readableDataType) {
        case MV_MQTTREADABLEDATATYPE_CONNECTRESPONSE: //< Response to a connect request is available.
            pushWorkMessage(OnMQTTEventConnectResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_MESSAGERECEIVED: //< A message is ready for consumption.
            pushWorkMessage(OnMQTTEventMessageReceived);
            break;
        case MV_MQTTREADABLEDATATYPE_MESSAGELOST: //< A message was lost, details are available.
            pushWorkMessage(OnMQTTEventMessageLost);
            break;
        case MV_MQTTREADABLEDATATYPE_SUBSCRIBERESPONSE: //< Response to a subscribe request is available.
            pushWorkMessage(OnMQTTEventSubscribeResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_UNSUBSCRIBERESPONSE: //< Response to an unsubscribe request is available.
            pushWorkMessage(OnMQTTEventUnsubscribeResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_PUBLISHRESPONSE: //< Response to a publish request is available.
            pushWorkMessage(OnMQTTEventPublishResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_DISCONNECTRESPONSE: //< Response to a disconnect operation is available.
            pushWorkMessage(OnMQTTEventDisconnectResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_NONE: //< No unconsumed data is available at this time.
        default:
            break;
    }
}

void mqtt_handle_connect_response_event() {
    struct MvMqttConnectResponse response = {};

    enum MvStatus status = mvMqttReadConnectResponse(mqtt_channel, &response);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReadConnectResponse returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }

    if (response.request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        server_error("connect error: response.request_state = %d", response.request_state);
        // not the status we expect
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }

    if (response.reason_code != 0x00) {
        server_error("connect error: response.reason_code = 0x%02x", (int) response.reason_code);
        // not the status we expect
        pushWorkMessage(OnBrokerConnectFailed);
        return;
    }

    server_log("mqtt broker connection successful");
    broker_connected = true;
    pushWorkMessage(OnBrokerConnected);
}

void mqtt_handle_subscribe_response_event() {
    enum MvMqttRequestState request_state;
    uint32_t correlation_id;
    uint32_t reason_codes[temp_num_items];
    uint32_t reason_codes_len;
    struct MvMqttSubscribeResponse response = {
        .request_state = &request_state,
        .correlation_id = &correlation_id,
        .reason_codes = reason_codes,
        .reason_codes_size = temp_num_items,
        .reason_codes_len = &reason_codes_len
    };

    enum MvStatus status = mvMqttReadSubscribeResponse(mqtt_channel, &response);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReadSubscribeResponse returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerSubscribeFailed);
        return;
    }

    if (request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        // not the request_state we expect
        server_error("subscribe response.request_state = %d", request_state);
        pushWorkMessage(OnBrokerSubscribeFailed);
        return;
    }

    if (reason_codes_len != temp_num_items) {
        server_error("expected %d subscribe reason_codes but received %d", temp_num_items, reason_codes_len);
        pushWorkMessage(OnBrokerSubscribeFailed);
        return;
    }

    for (uint32_t ndx=0; ndx<temp_num_items; ndx++) {
        if (reason_codes[ndx] != 0x00) {
            // not the status we expect
            server_error("subscribe reason_codes[%d] = 0x%02x", ndx, (int) reason_codes[ndx]);
            pushWorkMessage(OnBrokerSubscribeFailed);
            return;
        }
    }

    pushWorkMessage(OnBrokerSubscribeSucceeded);
}

void mqtt_handle_unsubscribe_response_event() {
    enum MvMqttRequestState request_state;
    uint32_t correlation_id;
    uint32_t reason_codes[temp_num_items];
    uint32_t reason_codes_len;
    struct MvMqttUnsubscribeResponse response = {
        .request_state = &request_state,
        .correlation_id = &correlation_id,
        .reason_codes = reason_codes,
        .reason_codes_size = temp_num_items,
        .reason_codes_len = &reason_codes_len
    };

    enum MvStatus status = mvMqttReadUnsubscribeResponse(mqtt_channel, &response);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReadUnubscribeResponse returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerUnsubscribeFailed);
        return;
    }

    if (request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        // not the request_state we expect
        server_error("unsubscribe response.request_state = %d", request_state);
        pushWorkMessage(OnBrokerUnsubscribeFailed);
        return;
    }

    if (reason_codes_len != temp_num_items) {
        server_error("expected %d unsubscribe reason_codes but received %d", temp_num_items, reason_codes_len);
        pushWorkMessage(OnBrokerUnsubscribeFailed);
        return;
    }

    for (uint32_t ndx=0; ndx<temp_num_items; ndx++) {
        if (reason_codes[ndx] != 0x00) {
            // not the status we expect
            server_error("unsubscribe reason_codes[%d] = 0x%02x", ndx, (int) reason_codes[ndx]);
            pushWorkMessage(OnBrokerUnsubscribeFailed);
            return;
        }
    }

    pushWorkMessage(OnBrokerUnsubscribeSucceeded);
}

void mqtt_handle_publish_response_event() {
    struct MvMqttPublishResponse response = { 0 };

    enum MvStatus status = mvMqttReadPublishResponse(mqtt_channel, &response);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReadPublishResponse returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerPublishFailed);
        return;
    }

    if (response.request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        // not the request_state we expect
        server_error("publish response.request_state = %d", response.request_state);
        pushWorkMessage(OnBrokerPublishFailed);
        return;
    }

    if (response.reason_code != 0x00) {
        // not the status we expect
        server_error("publish reason_code = 0x%02x", (int) response.reason_code);
        pushWorkMessage(OnBrokerPublishFailed);
        return;
    }

    pushWorkMessage(OnBrokerPublishSucceeded);
}

bool mqtt_get_received_message_data(uint32_t *correlation_id,
                                    uint8_t **topic, uint32_t *topic_len,
                                    uint8_t **payload, uint32_t *payload_len,
                                    uint32_t *qos, uint8_t *retain) {
    struct MvMqttMessage message = {
        .correlation_id = correlation_id,
        .topic = {
            .data = in_topic,
            .size = sizeof(in_topic),
            .length = topic_len,
        },
        .payload = {
            .data = in_payload,
            .size = sizeof(in_payload),
            .length = payload_len,
        },
        .qos = qos,
        .retain = retain
    };

    enum MvStatus status = mvMqttReceiveMessage(mqtt_channel, &message);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReceiveMessage returned 0x%02x\n", (int) status);
        return false;
    }

    *topic = in_topic;
    *payload = in_payload;

    return true;
}

bool mqtt_handle_lost_message_data() {
    enum MvMqttLostMessageReason reason;
    uint32_t topic_len;
    uint32_t message_len;
    struct MvMqttLostMessageInfo lostMessage = {
        .reason = &reason,
        .topic = {
            .data = in_topic,
            .size = sizeof(in_topic),
            .length = &topic_len,
        },
        .message_len = &message_len
    };

    enum MvStatus status = mvMqttReceiveLostMessageInfo(mqtt_channel, &lostMessage);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReceiveLostMessageInfo returned 0x%02x\n", (int) status);
        return false;
    }

    server_error("Message with topic %.*s was dropped. MQTT buffer should be at least %d bytes long to receive it.\n", (int) topic_len, in_topic, (int) message_len);
    return true;
}

void mqtt_acknowledge_message(uint32_t correlation_id) {
    enum MvStatus status = mvMqttAcknowledgeMessage(mqtt_channel, correlation_id);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttAcknowledgeMessage returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerMessageAcknowledgeFailed);
        return;
    }
}

void teardown_mqtt_connect() {
    struct MvMqttDisconnectResponse response = { 0 };

    enum MvStatus status = mvMqttReadDisconnectResponse(mqtt_channel, &response);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReadDisconnectResponse returned 0x%02x", (int) status);
        server_log("lost mqtt connection (disconnect_code not available), closing mqtt channel", response.disconnect_code);
    } else {
        server_log("lost mqtt connection (disconnect_code %04x), closing mqtt channel", response.disconnect_code); // Will contain the disconnect_code if using MQTT v5.  0x8E here will indicate another client with the same identity has taken over the session
    }

    broker_connected = false;
    mvCloseChannel(&mqtt_channel);

    pushWorkMessage(OnBrokerDisconnected);
}

void mqtt_disconnect() {
    enum MvStatus status = mvMqttRequestDisconnect(mqtt_channel);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttRequestDisconnect returned 0x%02x\n", (int) status);
        pushWorkMessage(OnBrokerDisconnectFailed);
        return;
    }
}
