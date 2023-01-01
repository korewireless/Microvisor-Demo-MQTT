# Using this demo with AWS IoT

## Sample AWS IoT Policy

The following policy configures access for:

- allowing subscription and receiving on topics `command/device/all` and `command/device/UV...` where UV... is the device SID for this device
- allowing publishing to `sensor/device/UV...` where UV... is the device SID for this device

        {
          "Version": "2012-10-17",
          "Statement": [
            {
              "Effect": "Allow",
              "Action": "iot:Connect",
              "Resource": "arn:aws:iot:us-west-2:<<ID>>:client/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Publish",
              "Resource": "arn:aws:iot:us-west-2:<<ID>>:topic/sensor/device/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Subscribe",
              "Resource": "arn:aws:iot:us-west-2:<<ID>>:topicfilter/command/device/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Receive",
              "Resource": "arn:aws:iot:us-west-2:<<ID>>:topic/command/device/${iot:Connection.Thing.ThingName}"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Subscribe",
              "Resource": "arn:aws:iot:us-west-2:<<ID>>:topicfilter/command/device/all"
            },
            {
              "Effect": "Allow",
              "Action": "iot:Receive",
              "Resource": "arn:aws:iot:us-west-2:<<ID>>:topic/command/device/all"
            }
          ]
        }
