# Using this demo with Azure IoT

This demo can be used to work with Azure IoT.  Documented below you will find one path at configuring a Microvisor device for use with Azure IoT to receive an send messages over MQTT.

We will:

- Create a device in an Azure IoT Hub with a Symmetric key
- Store the Symmetric in the secure configuration storage area on Microvisor cloud

This demo does not currently show the following flows.  They should be fully supported via Azure IoT's MQTT support.

- DPS provisioning of the device into a specific IoT Hub instance
- X509 self-signed or CA signed certificate authentication (see also the Amazon AWS demonstration for certificate authentication examples)

# Azure IoT Hub configuration

- Log into your Azure account at https://portal.azure.com/ , creating one if needed
- Create or select an IoT Hub instance within Azure.  This can be a Free Tier IoT Hub.

## Device configuration

- Within the IoT Hub, select Device Management -> Devices
- Select 'Add Device'
- For Device ID, enter exactly your Microvisor device identifier SID (starts with UV...)
- Select Authentication Type 'Symmetric Key', leave Auto-generate keys checked
- Click 'Save'

## Obtaining the Azure IoT Hub connection string

- You may need to click 'Refresh' for the device to show up in the device list after creating it
- Click on the device you created
- Locate 'Primary connection string' on the page and click the Show/Hide Field Contents icon to view it or the Copy to Clipboard icon to copy it
- Your connection string should look similar to: 'HostName=myhub.azure-devices.net;DeviceId=UV00000000000000000000000000000000;SharedAccessKey=QWhveSwgd29ybGQhCg=='

## Storing the connection string in Microvisor cloud

To facilitate your Microvisor device connecting, you wil need to provide the connection string to the device.  We recommend provisioning this as a device-scoped secret within Microvisor cloud so it is securely available to your device.

You'll need your Microvisor device SID, Account SID, and account Auth Token - all of which are available at https://console.twilio.com/

- Next, we will add configuration and secret items to Microvisor for this device

        # First, we'll set an environment variable with the connection string and other required info to make the curl a bit cleaner in the next step:

        export CONNECTION_STRING="HostName=myhub.azure-devices.net;DeviceId=UV00000000000000000000000000000000;SharedAccessKey=QWhveSwgd29ybGQhCg=="
        export TWILIO_ACCOUNT_SID=AC00000000000000000000000000000000
        export TWILIO_AUTH_TOKEN=.......
        export MV_DEVICE_SID=UV00000000000000000000000000000000

        curl --fail -X POST \
                --data-urlencode "Key=azure-connection-string" \
                --data-urlencode "Value=${CONNECTION_STRING}" \
                --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Secrets \
                -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

# The demo

We use the connection string populated into the secrets store to generate shared access signature (SAS) tokens with an expiry.  We will be disconnected at the end of this expiration period and we'll generate a new SAS token and reconnect when this happens.

Azure IoT Hub has specific requirements for MQTT access, the demo in this repository uses the following topics:

Subscribes to: `devices/<device sid>/messages/devicebound/`

Publishes to: `devices/<device sid>/messages/events/`

Azure IoT MQTT requires MQTT 3.1.1, so be sure to configure the Microvisor managed MQTT client with protocol_version MV_MQTTPROTOCOLVERSION_V3_1_1 not MV_MQTTPROTOCOLVERSION_V5.

More details about Azure IoT's MQTT implementation can be found [in the Azure IoT documentation](https://learn.microsoft.com/en-us/azure/iot-hub/iot-hub-mqtt-support).
