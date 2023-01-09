# Using this demo with AWS IoT

This demo can be used to work with AWS IoT.  Documented below you will find one path at configuring a Microvisor device for use with AWS IoT to receive and send messages over MQTT.

We will:

- Create a security Policy that can be applied to one or many Microvisor devices
- Create a Thing in AWS IoT for a Microvisor device
- Obtain PKI credentials for the Thing associated with the Microvisor device
- Provide the PKI credentials and connection information to the secure configuration storage area on Microvisor cloud

# AWS IoT Core configuration

- Log into your AWS account, creating one if needed
- Find the `AWS IoT` service in the Services menu

## Create an AWS IoT Policy

The following policy configures access for:

- allowing subscription and receiving on topics `command/device/all` and `command/device/UV...` where UV... is the device SID for this device
- allowing publishing to `sensor/device/UV...` where UV... is the device SID for this device
- Note: replace --ID-- with your account id (visible by clicking your account name in the upper right corner of the AWS IoT Core console and selecting the copy icon next to the Account ID field)
- Select 'Security' then 'Policies' in the AWS IoT Core menu
- Click on `Create policy`
- Name the policy something memorable
- Select JSON in the Policy Document section
- Paste in the below text, replacing --ID-- with your account id as mentioned above:

        {
          "Version": "2012-10-17",
          "Statement": [
            {
              "Effect": "Allow",
              "Action": "iot:Connect",
              "Resource": "arn:aws:iot:us-west-2:--ID--:client/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Publish",
              "Resource": "arn:aws:iot:us-west-2:--ID--:topic/sensor/device/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Subscribe",
              "Resource": [
                "arn:aws:iot:us-west-2:--ID--:topicfilter/command/device/${iot:Connection.Thing.ThingName}",
                "arn:aws:iot:us-west-2:--ID--:topicfilter/command/device/all"
              ]
            },
            {
              "Effect": "Allow",
              "Action": "iot:Receive",
              "Resource": [
                "arn:aws:iot:us-west-2:--ID--:topic/command/device/${iot:Connection.Thing.ThingName}",
                "arn:aws:iot:us-west-2:--ID--:topic/command/device/all"
              ]
            }
          ]
        }

## Create one or more AWS IoT Core Things

*Important:*  The Policy configured above as well as the demo code in this repository relies on the Thing name being *identical* to the device SID.  If you choose to change this, you will need to change it in all applicable places.

- Obtain the Microvisor device SID (starts with UV...) from the Twilio console and keep it handy
- Select `All devices` then `Things`
- Click `Create things`
- Select `Create single thing`
- For `Thing name` use the Microvisor device SID exactly as presented, no extra text
- For this demo, we won't need a Device Shadow but you can create one if you choose
- Click `Next`

- Keep `Auto-generate a new certificate (recommended)` checked and click `Next`

- Attach the policy created above and click `Create thing`

- A pop-up will appear with the newly generated certificate.  This is your only chance to download this information - if you do not you will need to create a new certificate for the Thing
- Download the `Device certificate` .crt file
- Download the `Key files` for the -public.pem.key and -private.pem.key
- Download the RSA 2048 bit Root CA certificate

## Importing the authentication credentials into Microvisor

- Obtain the MQTT broker hostname for your IoT Core instance by selecting `Settings` in the left hand menu in the AWS console.
- Make a copy of the `Device data endpoint` URL and keep it handy (example hostname: abcdefghi.iot.us-west-2.amazonaws.com)

- The scripts below rely on the following environment variables to make things easy for you:

        export MV_DEVICE_SID=UV...
        # the AWS Certificate ID is the 64 byte hexadecimal prefix to your downloaded certificate files
        export AWS_CERTIFICATE_ID=012345...
        
        # your Twilio account SID and auth token (visible at https://console.twilio.com/)
        export TWILIO_ACCOUNT_SID=AC...
        export TWILIO_AUTH_TOKEN=...
        
        # the `Device data endpoint` hostname obtained above:
        export BROKER_HOST=xxxx.amazonaws.com
        export BROKER_PORT=8883

- The certificate data from AWS IoT is provided in PEM format, but for space considerations Microvisor needs this information in DER format so we will convert it:

        openssl x509 -inform pem -in AmazonRootCA1.pem -outform der -out root-ca.der
        openssl x509 -inform pem -in ${AWS_CERTIFICATE_ID}-certificate.pem.crt -outform der -out ${MV_DEVICE_SID}-cert.der
        openssl rsa -inform pem -in ${AWS_CERTIFICATE_ID}-private.pem.key -outform der -out ${MV_DEVICE_SID}-private_key.der

- Next, we will add configuration and secret items to Microvisor for this device

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
        
        curl --fail -X POST \
                --data-urlencode "Key=root-CA" \
                --data-urlencode "Value=$(hexdump -v -e '1/1 "%02x"' root-ca.der)" \
                --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
                -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}
        
        curl --fail -X POST \
                --data-urlencode "Key=cert" \
                --data-urlencode "Value=$(hexdump -v -e '1/1 "%02x"' ${MV_DEVICE_SID}-cert.der)" \
                --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Configs \
                -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}
        
        curl --fail -X POST \
                --data-urlencode "Key=private_key" \
                --data-urlencode "Value=$(hexdump -v -e '1/1 "%02x"' ${MV_DEVICE_SID}-private_key.der)" \
                --silent https://microvisor.twilio.com/v1/Devices/${MV_DEVICE_SID}/Secrets \
                -u ${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}

# Run the demo

You should be able to run the demo now, it will obtain the device SID to use as a client id and as part of the publish topics from the device itself.  The broker host and port as well as PKI authentication credentials will come from the Microvisor config and secrets store.

It is important to note that subscribing or publishing to topics which do not match the AWS Policy will result in the AWS MQTT broker immediately disconnecting the client.  If you are seeing unexpected disconnections of the MQTT Broker, check your policy first.

