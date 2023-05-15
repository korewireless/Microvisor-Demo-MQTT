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
- Note: replace --REGION-- and --ID-- with your region and account id (visible by clicking your account name in the upper right corner of the AWS IoT Core console and selecting the copy icon next to the Account ID field)
- Select 'Security' then 'Policies' in the AWS IoT Core menu
- Click on `Create policy`
- Name the policy something memorable
- Select JSON in the Policy Document section
- Paste in the below text, replacing --REGION-- and --ID-- with your region and account id as mentioned above:

        {
          "Version": "2012-10-17",
          "Statement": [
            {
              "Effect": "Allow",
              "Action": "iot:Connect",
              "Resource": "arn:aws:iot:--REGION--:--ID--:client/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Publish",
              "Resource": "arn:aws:iot:--REGION--:--ID--:topic/sensor/device/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Subscribe",
              "Resource": [
                "arn:aws:iot:--REGION--:--ID--:topicfilter/command/device/${iot:Connection.Thing.ThingName}",
                "arn:aws:iot:--REGION--:--ID--:topicfilter/command/device/all"
              ]
            },
            {
              "Effect": "Allow",
              "Action": "iot:Receive",
              "Resource": [
                "arn:aws:iot:--REGION--:--ID--:topic/command/device/${iot:Connection.Thing.ThingName}",
                "arn:aws:iot:--REGION--:--ID--:topic/command/device/all"
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

Microvisor offers a key/value store for data you wish to share with devices.  These values fall into two types of data and offer two scopes:

Available stores:

- Configs store - less sensitive configuration values which support both setting and reading via the Microvisor REST API, and reading by Microvisor devices
- Secrets store - sensitive configuration values which support only setting via the Microvisor REST API, and reading by Microvisor devices

Available scopes:

- Account - values with this scope are available to read to every Microvisor device on the owning Twilio account
- Device - values with this scope are available to read only for the specified Microvisor device SID

For this MQTT demo, we made the following choices:

- MQTT endpoint configuration keys `broker-host` and `broker-port` are placed in the Configs store with Device scope.  This could have easily been Account scope if we wanted to share these values with all devices.
- The AWS IoT CA chain is stored in `root-CA` in the Configs store with Device scope, this also could have been Account scope if our fleet of devices all use AWS IoT.
- The AWS certificate for our device is stored in `cert` in the Configs store with Device scope.  It is appropriate to scope this type of item to a single device.  We chose the Configs store for this item as it is not sensitive on its own and may provide value in being able to query a device's public certificate later.
- The AWS private key for the device certificate is stored in `private_key` in the Secrets store.  This means only the Microvisor device can read this value back out, it is not available via the REST API.

The REST API endpoints support only non-binary data so the certificate data must be made into a URL encoded text string.  Further, the managed MQTT client in Microvisor requires DER encoded binary certificate and key data.  To work around both of these, we convert the certificates and key from AWS IoT to DER encoded binary form then send this as a hex encoded string to the cloud.  The demo code then converts the hex encoded string to binary.  We chose this format for its simplicity and lack of additional device-side dependencies, not its space efficiency.  You are welcome to use a more efficient method provided it results in DER encoded certificates and private key in your project's Microvisor device code.

- Obtain the MQTT broker hostname for your IoT Core instance by selecting `Settings` in the left hand menu in the AWS console.
- Make a copy of the `Device data endpoint` URL and keep it handy (example hostname: abcdefghi.iot.us-west-2.amazonaws.com)

- The shell snippets below rely on the following environment variables to make things easy for you:

        export MV_DEVICE_SID=UV...
        # the AWS Certificate ID is the 64 byte hexadecimal prefix to your downloaded certificate files
        export AWS_CERTIFICATE_ID=012345...
        
        # your Twilio account SID and auth token (visible at https://console.twilio.com/)
        export TWILIO_ACCOUNT_SID=AC...
        export TWILIO_AUTH_TOKEN=...
        
        # the `Device data endpoint` hostname obtained above:
        export BROKER_HOST=xxxx.amazonaws.com
        export BROKER_PORT=8883

- The certificate data from AWS IoT is provided in PEM format, but for space considerations Microvisor needs the PKCS#8 format private key and certificates as DER format binary values, so we will convert them:

        openssl x509 -inform pem -in AmazonRootCA1.pem -outform der -out root-ca.der
        openssl x509 -inform pem -in ${AWS_CERTIFICATE_ID}-certificate.pem.crt -outform der -out ${MV_DEVICE_SID}-cert.der
        openssl pkcs8 -topk8 -in ${AWS_CERTIFICATE_ID}-private.pem.key -inform pem -out ${MV_DEVICE_SID}-private_key.der -outform der -nocrypt

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

# Viewing AWS IoT logs with CloudWatch

A primer on setting up CloudWatch and AWS IoT logging can be found on AWS: https://docs.aws.amazon.com/iot/latest/developerguide/configure-logging.html

An abbreviated version of that resource follows.

## Configuring logging role

NOTE: If you do not need fine grained control over the logging role setup, you can skip to the next step and use the 'Create role' button in the Log settings for the IoT core instance below.

Logging requires a role, either an existing one or a new one should do.  We'll create a dedicated role:

- Open https://console.aws.amazon.com/iam/home#/roles
- Select `Create Role`
- Set the Trusted Entity Type to `AWS Service`
- Set the Use Case to `IoT`, using the search box if needed
- Click `Next`
- We'll keep the default permissions on this screen - no changes necessary
- Click `Next`
- Give the role a memorable name and apply a description as desired, the rest can be left as-is
- Create the role

## Configure IoT Core instance to use the logging role

We'll now configure our IoT Core instance to make use of this new logging role.

- Open https://us-west-2.console.aws.amazon.com/iot/home - be sure the region is set to the same region as you created your Microvisor device in above
- Select `Settings` in the menu on the left
- Find the Logs section and click `Manage Logs`
- Select the logging role you just created from the select box
- Select the logging level you desire from the drop-down
- Click `Update`

## Checking the logs

Generate some traffic against IoT Core using your device then we'll view the available logs.

- Open https://us-west-2.console.aws.amazon.com/cloudwatch/home (ensure the correct region is reflected in the top menu bar)
- Select Logs then Log Groups in the menu on the left
- Click on the AWSIoTLogsV2 group
- To simplify viewing of the log entries, click the `Search all log streams` button
- Here you can search for specific events or metadata.  The quickest option here is sasrch for the Microvisor device SID (starts with UV...)

