# Using this demo with any MQTT broker

The MQTT demo makes use of Microvisor's configs and secrets store.  Both stores can be read by Microvisor devices via the syscall APIs.  From the REST API, configs can be both written and read while secerts are write-only.

The MQTT expects values at certain keys, depending on your configuration:

|Key Name   |Scope |Store  |Description                                       |Required                       |
|-----------|------|-------|--------------------------------------------------|-------------------------------|
|broker-host|device|configs|Hostname of MQTT Broker                           |YES                            |
|broker-port|device|configs|Port for MQTT Broker                              |YES                            |
|client-id  |device|configs|MQTT Client ID if not the Microvisor Device SID   |                               |
|root-ca    |device|configs|Root CA chain of server if not covered by default |Sometimes                      |
|cert       |device|configs|Public certificate for device to authenticate with|YES, for cert auth             |
|private_key|device|secrets|Private key for device to authenticate with       |YES, for cert auth             |
|username   |device|configs|Username for device to authenticate with          |YES, for username/password auth|
|password   |device|secrets|Password for device to authenticate with          |YES, for username/password auth|

## TLS and PKI requirements for Microvisor Managed MQTT

### Certificate chain

Microvisor's Managed MQTT client requires TLS on all connections.  If your chosen MQTT broker's certificate chain is reflected in the [Mozilla CA Certificate Program's](https://wiki.mozilla.org/CA) certificate chain you don't need to supply a chain of your own.

If your MQTT broker provider supplies a certificate chain you need (or would prefer) to use, it should be presented to the managed MQTT client as a DER encoded chain.  For example, if your provider distributes a .der format chain, you can use that.  If they provide PEM encoded chain you can convert it with the openssl commandline as follows:

    openssl x509 -inform pem -in root-ca.pem -outform der -out root-ca.der

This chain can either be hardcoded into your application or stored in Microvisor's config and secrets store for greater flexibility.  See below for details on how to load the chain and/or pki keypair.

### Client certificate

If you will authenticate with your MQTT broker with a client certificate, the certificate must be DER binary encoded (just like the certificate chain above) and the private key must be placed in a PKCS#8 wrapped binary DER format.  Commands to accomplish both follow, though your `inform` type may differ depending on your MQTT broker provider:

    openssl x509 -inform pem -in client-certificate.pem -outform der -out client-certificate.der

    openssl pkcs8 -topk8 -in client-private-key.pem -inform pem -out client-private-key.der -outform der -nocrypt

The resulting certificate and private key should then be populated into the Microvisor config and secrets store as shown below.

## Configuring the demo

How the demo uses the managed MQTT client to connect to your broker is determined by preprocessor defines in work.h.

### Broker host and port

**IMPORTANT:** The Microvisor managed MQTT broker requires TLS communications.  It cannot connect to non-secure MQTT brokers (for example, ones that only support port 1883).  Typically this will mean you must connect using port 8883 or the broker's prescribed MQTTS port.

Required configuration keys in the Microvisor configs and secrets store: `broker-host`, `broker-port`

### Client ID

By default, the demo will use the Microvisor device sid as the MQTT client id.  If either your broker requires a specific client id or you wish to override this behavior, you can provide an alternate.

Required defines in `work.h`:

`#define CUSTOM_CLIENT_ID`

Required configuration keys in the Microvisor configs and secrets store: `client-id`

### Root TLS certificate store

If your broker uses a TLS certificate not inherently trusted by the [Mozilla CA Certificate Program](https://wiki.mozilla.org/CA), such as a self-signed certificate, you will need to provide a CA chain encoded in der format.

Required defines in `work.h`:

`#define CERTIFICATE_CA`

Required configuration keys in the Microvisor configs and secrets store: `root-ca`.  Please note that the certificate chain must be der binary encoded, a textual pem format is not supported.

### Certificate authentication

If your broker uses client certificate authentication, you can provide a public certificate and private key for Microvisor to authenticate using.

Required defines in `work.h`:

`#define CERTIFICATE_AUTH`

Required configuration keys in the Microvisor configs and secrets store: `cert`, `private_key`.

Please note that the client certificate must be der binary encoded, a textual pem format is not supported.

### Username/Password authentication

If your broker uses username/password authentication, you can provide the values to Microvisor to authenticate with.

Required defines in `work.h`:

`#define USERNAMEPASSWORD_AUTH`

Required configuration keys in the Microvisor configs and secrets store: `username`, `password`.

## Loading configuration and secrets using the Microvisor REST API

Depending on your specific usecase, some configuration (or even secrets) values may be suitable at either the Device or the Account level.  Items scoped at the Device level are only available to the specified device, while items scoped at the Account level are globally available to all Microvisor devices on an account.

The demo application by default uses Device scoped values in light of the demonstrative nature of the code.  You can modify this by changing both where you configure the following values and the configuration of the queries for this information in `work.c`.

You have a choice when it comes to how to populate the values into the config and secrets stores, you can use the REST API via the programming language or your choice, you can use a tool such as `curl`, or you can use the Twilio CLI to work with the Microvisor API's.  To provide a single example and to make easier the hex-encoding the binary certificate data required (shown below using the `hexdump` utility), we will show using `curl` at the command line.

### Configuring environment variables

To make the commands below as simple to execute as possible, we have set them up to rely on variables from the environment you can set as follows:

Required for all examples:

    export TWILIO_ACCOUNT_SID=AC...
    export TWILIO_AUTH_TOKEN=...
    export MV_DEVICE_SID=UV...

    export BROKER_HOST=mqtt.example.com
    export BROKER_PORT=8883

Optional if desired, or required for MQTT brokers which require a specific MQTT client identifier:

    export MQTT_CLIENT_ID=...

Required for MQTT brokers for which an alternate trusted certificate chain is required:

    export CA_CHAIN_FILE=root-ca.der

Required for MQTT brokers with client certificate authentication:

    export CERTIFICATE_FILE=client-certificate.der
    export PRIVATE_KEY_FILE=client-private-key.der

Required for MQTT brokers with username/password authentication:

    export BROKER_USERNAME=...
    export BROKER_PASSWORD=...

### Assigning the values into the Microvisor config and secrets stores

These commands rely on the variable names set in the section above.

Required for all examples:

    curl --fail -X POST \
        --data-urlencode "Key=broker-host" \
        --data-urlencode "Value=${BROKER_HOST}" \
        --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
        -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

    curl --fail -X POST \
            --data-urlencode "Key=broker-port" \
            --data-urlencode "Value=${BROKER_PORT}" \
            --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
            -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

Optional if desired, or required for MQTT brokers which require a specific MQTT client identifier:

    curl --fail -X POST \
            --data-urlencode "Key=client-id" \
            --data-urlencode "Value=${MQTT_CLIENT_ID}" \
            --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
            -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

Required for MQTT brokers for which an alternate trusted certificate chain is required:

    curl --fail -X POST \
            --data-urlencode "Key=root-CA" \
            --data-urlencode "Value=$(hexdump -v -e '1/1 "%02x"' ${CA_CHAIN_FILE})" \
            --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
            -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

Required for MQTT brokers with client certificate authentication:

    curl --fail -X POST \
            --data-urlencode "Key=cert" \
            --data-urlencode "Value=$(hexdump -v -e '1/1 "%02x"' ${CERTIFICATE_FILE})" \
            --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
            -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

    curl --fail -X POST \
            --data-urlencode "Key=private_key" \
            --data-urlencode "Value=$(hexdump -v -e '1/1 "%02x"' ${PRIVATE_KEY_FILE})" \
            --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Secrets \
            -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

Required for MQTT brokers with username/password authentication:

    curl --fail -X POST \
        --data-urlencode "Key=username" \
        --data-urlencode "Value=${BROKER_USERNAME}" \
        --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
        -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

    curl --fail -X POST \
        --data-urlencode "Key=password" \
        --data-urlencode "Value=${BROKER_PASSWORD}" \
        --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Secrets \
        -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

## MQTT topic selection

While many MQTT brokers allow you to define your own topics available to publish or subscribe to, some MQTT brokers such as those which are gateways into a specific IoT service such as Amazon AWS, Microsoft Azure, etc may have strict patterns they require adherence to.

Deviation from these specifications can lead to errors or your entire MQTT connection being terminated, especially seemingly on startup.  If you are experiencing unexpected disconnections from your chosen broker, we encourage you to try commenting out any code which subscribes to topics or publishes to topics to see if the connection itself is stable.  We also recommend you refer to your MQTT broker's documentation on supported topics for both publishing and subscribing.

Topics in the Microvisor MQTT demo are defined in the following places by default:

The default subscription topic is in a string in the `app/mqtt_handler.c` file, in the `start_subscriptions` and `end_subscriptions` functions.

The default topic used for publishing is in the `app/mqtt_handler.c` file, in this `publish_message` function.

## MQTT server version specification

Depending on your chosen MQTT broker, it may require you to choose either V3.1.1 or V5 client support.  If your MQTT broker does not suport V5 client connections, open the `app/mqtt_handler.c` file and locate the definition of the `MvMqttConnectRequest` struct.  In there, change `.protocol_version` from `MV_MQTTPROTOCOLVERSION_V5` to `MV_MQTTPROTOCOLVERSION_V3_1_1` as necessary.
