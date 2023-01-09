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
        pushMessage(OnBrokerConnectFailed);
        return;
    }

    struct MvMqttAuthentication authentication = {
        .method = MV_MQTTAUTHENTICATIONMETHOD_NONE,
        .username_password = NULL,
    };

    struct MvSizedString device_certs[] = {
        {
            .data = (uint8_t *)cert,
            .length = cert_len
        },
    };

    struct MvTlsCertificateChain device_certificate = {
        .num_certs = 1,
        .certs = device_certs
    };

    struct MvSizedString key = {
        .data = (uint8_t *)private_key,
        .length = private_key_len
    };

    struct MvOwnTlsCertificateChain device_credentials = {
        .chain = &device_certificate,
        .key = &key
    };

    struct MvSizedString ca_certs[] = {
        {
            .data = (uint8_t *)root_ca,
            .length = root_ca_len
        },
    };

    struct MvTlsCertificateChain server_ca_certificate = {
        .num_certs = 1,
        .certs = ca_certs
    };

    struct MvTlsCredentials tls_credentials = {
        .cacert = &server_ca_certificate,
        .clientcert = &device_credentials,
    };

    struct MvMqttConnectRequest request = {
        .protocol_version = MV_MQTTPROTOCOLVERSION_V5,
        .host = {
            .data = broker_host,
            .length = broker_host_len
        },
        .port = broker_port,
        .clientid = {
            .data = client,
            .length = client_len 
        },
        .authentication = &authentication,
        .tls_credentials = &tls_credentials,
        .keepalive = 60,
        .clean_start = 0,
        .will = NULL,
    };

    status = mvMqttPerformConnectRequest(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttPerformConnectRequest returned 0x%02x\n", (int) status);
        pushMessage(OnBrokerConnectFailed);
        return;
    }
}

bool is_broker_connected() {
    return broker_connected;
}

void start_subscriptions() {
    char topic_str[128];
    sprintf(topic_str, "command/device/%.*s", client_len, client);

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

    status = mvMqttPerformSubscribeRequest(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttSubscribeRequest returned 0x%02x\n", (int) status);
        pushMessage(OnBrokerSubscriptionRequestFailed);
        return;
    }
}

void end_subscriptions() {
    char topic_str[128];
    sprintf(topic_str, "command/device/%.*s", client_len, client);

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

    status = mvMqttPerformUnsubscribeRequest(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttUnsubscribeRequest returned 0x%02x\n", (int) status);
        pushMessage(OnBrokerUnsubscriptionRequestFailed);
        return;
    }
}

void publish_sensor_data(float temperature) {
    char topic_str[128];
    sprintf(topic_str, "sensor/device/%.*s", client_len, client); // For AWS, requires policy to allow publish access to "arn:aws:iot:us-west-2:____:topic/sensor/device/<<DEVICE_SID>>"
    char payload_str[128];
    sprintf(payload_str, "{ \"temperature\": %.2f }", temperature);

    enum MvStatus status;

    const struct MvMqttPublishRequest request = {
        .correlation_id = correlation_id++,
        .topic = {
            .data = (uint8_t *)topic_str,
            .length = strlen(topic_str)
        },
        .payload = {
            .data = (uint8_t *)payload_str,
            .length = strlen(payload_str)
        },
        .desired_qos = 0,
        .retain = 0
    };

    server_log("starting publish request");
    status = mvMqttPerformPublishRequest(mqtt_channel, &request);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttPublishRequest returned 0x%02x\n", (int) status);
        pushMessage(OnBrokerPublishFailed);
        return;
    }
    server_log("completed publish request");
}

void mqtt_handle_readable_event() {
    enum MvMqttReadableDataType readableDataType;
    if (mvMqttGetNextReadableDataType(mqtt_channel, &readableDataType) != MV_STATUS_OKAY) {
        pushMessage(OnMqttChannelFailed);
        return;
    }

    server_log("mqtt readable %02x", readableDataType);
    switch (readableDataType) {
        case MV_MQTTREADABLEDATATYPE_CONNECTRESPONSE: //< Response to a connect request is available.
            pushMessage(OnMQTTEventConnectResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_MESSAGERECEIVED: //< A message is ready for consumption.
            pushMessage(OnMQTTEventMessageReceived);
            break;
        case MV_MQTTREADABLEDATATYPE_MESSAGELOST: //< A message was lost, details are available.
            pushMessage(OnMQTTEventMessageLost);
            break;
        case MV_MQTTREADABLEDATATYPE_SUBSCRIBERESPONSE: //< Response to a subscribe request is available.
            pushMessage(OnMQTTEventSubscribeResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_UNSUBSCRIBERESPONSE: //< Response to an unsubscribe request is available.
            pushMessage(OnMQTTEventUnsubscribeResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_PUBLISHRESPONSE: //< Response to a publish request is available.
            pushMessage(OnMQTTEventPublishResponse);
            break;
        case MV_MQTTREADABLEDATATYPE_DISCONNECTRESPONSE: //< Response to a disconnect operation is available.
            pushMessage(OnMQTTEventDisconnectResponse);
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
        pushMessage(OnBrokerConnectFailed);
        return;
    }

    server_log("connect response.status = %d", response.status);
    if (response.status != MV_MQTTCONNECTSTATUS_CONNACKRECEIVED) {
        // not the status we expect
        pushMessage(OnBrokerConnectFailed);
        return;
    }

    server_log("connect response.reason_code = 0x%02x", (int) response.reason_code);
    if (response.reason_code != 0x00) {
        // not the status we expect
        pushMessage(OnBrokerConnectFailed);
        return;
    }

    server_log("mqtt broker connection successful");
    broker_connected = true;
    pushMessage(OnBrokerConnected);
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
        pushMessage(OnBrokerSubscribeFailed);
        return;
    }

    server_log("subscribe response.request_state = %d", request_state);
    if (request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        // not the request_state we expect
        pushMessage(OnBrokerSubscribeFailed);
        return;
    }

    if (reason_codes_len != temp_num_items) {
        server_log("expected %d subscribe reason_codes but received %d", temp_num_items, reason_codes_len);
        pushMessage(OnBrokerSubscribeFailed);
        return;
    }

    for (uint32_t ndx=0; ndx<temp_num_items; ndx++) {
        server_log("subscribe reason_codes[%d] = 0x%02x", ndx, (int) reason_codes[ndx]);
        if (reason_codes[ndx] != 0x00) {
            // not the status we expect
            pushMessage(OnBrokerSubscribeFailed);
            return;
        }
    }

    server_log("mqtt subscribe successful");
    pushMessage(OnBrokerSubscribeSucceeded);
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
        pushMessage(OnBrokerUnsubscribeFailed);
        return;
    }

    server_log("unsubscribe response.request_state = %d", request_state);
    if (request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        // not the request_state we expect
        pushMessage(OnBrokerUnsubscribeFailed);
        return;
    }

    if (reason_codes_len != temp_num_items) {
        server_log("expected %d unsubscribe reason_codes but received %d", temp_num_items, reason_codes_len);
        pushMessage(OnBrokerUnsubscribeFailed);
        return;
    }

    for (uint32_t ndx=0; ndx<temp_num_items; ndx++) {
        server_log("unsubscribe reason_codes[%d] = 0x%02x", ndx, (int) reason_codes[ndx]);
        if (reason_codes[ndx] != 0x00) {
            // not the status we expect
            pushMessage(OnBrokerUnsubscribeFailed);
            return;
        }
    }

    server_log("mqtt unsubscribe successful");
    pushMessage(OnBrokerUnsubscribeSucceeded);
}

void mqtt_handle_publish_response_event() {
    server_log("received publish request response");
    enum MvMqttRequestState request_state;
    uint32_t correlation_id;
    uint32_t reason_code;
    struct MvMqttPublishResponse response = {
        .request_state = &request_state,
        .correlation_id = &correlation_id,
        .reason_code = &reason_code
    };

    enum MvStatus status = mvMqttReadPublishResponse(mqtt_channel, &response);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttReadPublishResponse returned 0x%02x\n", (int) status);
        pushMessage(OnBrokerPublishFailed);
        return;
    }

    server_log("publish response.request_state = %d", request_state);
    if (request_state != MV_MQTTREQUESTSTATE_REQUESTCOMPLETED) {
        // not the request_state we expect
        pushMessage(OnBrokerPublishFailed);
        return;
    }

    server_log("publish reason_code = 0x%02x", (int) reason_code);
    if (reason_code != 0x00) {
        // not the status we expect
        pushMessage(OnBrokerPublishFailed);
        return;
    }

    server_log("mqtt publish successful");
    pushMessage(OnBrokerPublishSucceeded);
}

bool mqtt_get_received_message_data(uint32_t *correlation_id,
                                    uint8_t **topic, uint32_t *topic_len,
                                    uint8_t **payload, uint32_t *payload_len,
                                    uint32_t *qos, uint8_t *retain) {
    server_log("received mqtt message");
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
    server_log("received mqtt dropped message notification");
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
        pushMessage(OnBrokerMessageAcknowledgeFailed);
        return;
    }
}

void teardown_mqtt_connect() {
    server_log("closing mqtt channel");
    broker_connected = false;
    mvCloseChannel(&mqtt_channel);

    pushMessage(OnBrokerDisconnected);
}

void mqtt_disconnect() {
    enum MvStatus status = mvMqttDisconnect(mqtt_channel);
    if (status != MV_STATUS_OKAY) {
        server_error("mvMqttDisconnect returned 0x%02x\n", (int) status);
        pushMessage(OnBrokerDisconnectFailed);
        return;
    }
}
