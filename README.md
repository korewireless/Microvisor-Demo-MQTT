# Microvisor MQTT Demo 1.0.1

This repo provides a basic demonstration of a user application capable of working with Microvisor’s MQTT communications system calls. It has no hardware dependencies beyond the Twilio Microvisor Nucleo Development Board.

It is based on the [FreeRTOS](https://freertos.org/) real-time operating system and which will run on the “non-secure” side of Microvisor. FreeRTOS is included as a submodule.

The [ARM CMSIS-RTOS API](https://github.com/ARM-software/CMSIS_5) is used as an intermediary between the application and FreeRTOS to make it easier to swap out the RTOS layer for another.

The application code files can be found in the [app/](app/) directory. The [ST_Code/](ST_Code/) directory contains required components that are not part of Twilio Microvisor STM32U5 HAL, which this sample accesses as a submodule. The `FreeRTOSConfig.h` and `stm32u5xx_hal_conf.h` configuration files are located in the [config/](config/) directory.

## Release Notes

Version 1.0.1 expands authentication options within the code and documentation on configuring the demo.

Version 1.0.0 is the initial MQTT demo.

## Actions

The code creates and runs four threads:

- A thread periodically toggles GPIO A5, which is the user LED on the [Microvisor Nucleo Development Board](https://www.twilio.com/docs/iot/microvisor/microvisor-nucleo-development-board).  This acts as a heartbeat to let you know the demo is working.
- A thread manages the network state of your application, requesting control of the network from Microvisor.
- A work thread which consumes events and dispatches them in support of the configuration loading and managed MQTT broker operations.
- An application thread which consumes data from an attached sensor (or demo source) and sends it to the work thread for publishing.

## Cloning the Repo

This repo makes uses of git submodules, some of which are nested within other submodules. To clone the repo, run:

```bash
git clone https://github.com/twilio/twilio-microvisor-mqtt-demo.git
```

and then:

```bash
cd twilio-microvisor-mqtt-demo
git submodule update --init --recursive
```

## Repo Updates

When the repo is updated, and you pull the changes, you should also always update dependency submodules. To do so, run:

```bash
git submodule update --remote --recursive
```

We recommend following this by deleting your `build` directory.

## Requirements

You will need a Twilio account. [Sign up now if you don’t have one](https://www.twilio.com/try-twilio).

You will also need a Twilio Microvisor [Nucleo Development Board](https://www.twilio.com/docs/iot/microvisor/microvisor-nucleo-development-board). These are currently only available to Beta Program participants: [Join the Beta](https://interactive.twilio.com/iot-microvisor-private-beta-sign-up?utm_source=github&utm_medium=github&utm_campaign=IOT&utm_content=MQTT_GitHub_Demo).

### MQTT Broker

MQTT is quite flexible both in authentication mechanisms offered as well as how brokers can choose to allow publishing and subscribing to topics.

Unless using a self-hosted broker, such as [Eclipse Mosquitto](https://mosquitto.org/) you will need to adhere to your chosen broker's security and usage guidelines which may specify not just authentication mechanism but also allowable publish and subscribe topics, restrictions on client identifier, etc.

While we cannot document every broker that is compatible with Microvisor we have taken two approaches to assist you:

- We support a wide variety of configuration options per the MQTT specification for compatibility with most v3.1.1 and v5 brokers
- We have documented a number of paths for use with this codebase

For help connecting to a generic MQTT broker or one not explicitly listed below, please visit [general MQTT options](README-General.md).

For a guide on connecting to AWS IoT's MQTT broker, please visit our [AWS IoT guide](README-AWS.md).

For a repository extended to support Azure's use of SAS token authentication, please visit out [Azure MQTT Demo](https://github.com/twilio/twilio-microvisor-azure-demo/).

You may benefit from enabling additional debugging in the demo application as you are working with it, to obtain the most verbose logging you can un-comment the debugging directives in CMakeLists.txt before building.

## Software Setup

This project is written in C. At this time, we only support Ubuntu 20.0.4. Users of other operating systems should build the code under a virtual machine running Ubuntu, or with Docker.

**Note** Users of unsupported platforms may attempt to install the Microvisor toolchain using [this guidance](https://www.twilio.com/docs/iot/microvisor/install-microvisor-app-development-tools-on-unsupported-platforms).

### With Docker

Build the image:

```shell
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -t mv-mqtt-demo-image .
```

Run the build:

```
docker run -it --rm -v $(pwd)/:/home/mvisor/project/ \
  --env-file env.list \
  --name mv-mqtt-demo mv-mqtt-demo-image
```

**Note** You will need to have exported certain environment variables, as [detailed below](#environment-variables).

Under Docker, the demo is compiled, uploaded and deployed to your development board. It also initiates logging — hit <b>ctrl</b>-<b>c</b> to break out to the command prompt.

Diagnosing crashes:

```
docker run -it --rm -v $(pwd)/:/home/mvisor/project/ \
  --env-file env.list \
  --name mv-mqtt-demo --entrypoint /bin/bash mv-mqtt-demo-image
```

To inspect useful info, to start with PC and LR:
```
gdb-multiarch project/build/app/mv-mqtt-demo.elf
info symbol <...>
```

### Without Docker

#### Libraries and Tools

Under Ubuntu, run the following:

```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi \
  git curl build-essential cmake libsecret-1-dev jq openssl
```

#### Twilio CLI

Install the Twilio CLI. This is required to view streamed logs and for remote debugging. You need version 4.0.1 or above.

**Note** If you have already installed the Twilio CLI using *npm*, we recommend removing it and then reinstalling as outlined below. Remove the old version with `npm remove -g twilio-cli`.

```bash
wget -qO- https://twilio-cli-prod.s3.amazonaws.com/twilio_pub.asc | sudo apt-key add -
sudo touch /etc/apt/sources.list.d/twilio.list
echo 'deb https://twilio-cli-prod.s3.amazonaws.com/apt/ /' | sudo tee /etc/apt/sources.list.d/twilio.list
sudo apt update
sudo apt install -y twilio
```

Close your terminal window or tab, and open a new one. Now run:

```bash
twilio plugins:install "@twilio/plugin-microvisor@0.3.7"
```

Note: This project currently requires the plugin-microvisor version 0.3.7.  It will not work with newer versions of the plugin.

### Environment Variables

Running the Twilio CLI and the project's [deploy script](./deploy.sh) — for uploading the built code to the Twilio cloud and subsequent deployment to your Microvisor Nucleo Board — uses the following Twilio credentials stored as environment variables. They should be added to your shell profile:

```bash
export TWILIO_ACCOUNT_SID=ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
export TWILIO_AUTH_TOKEN=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
export MV_DEVICE_SID=UVxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

You can get the first two from your Twilio Console [account dashboard](https://console.twilio.com/).

Enter the following command to get your target device’s SID and, if set, its unqiue name:

```bash
twilio api:microvisor:v1:devices:list
```

## Build and Deploy the Application

Run:

```bash
./deploy.sh --log
```

This will compile, bundle and upload the code, and stage it for deployment to your device. If you encounter errors, please check your stored Twilio credentials.

The `--log` flag initiates log-streaming.

## Choosing which application to build

Multiple applications are supported in the demo. Right now the two applications implemented are

    - `dummy` - send dummy temperature data once a second
    - `temperature` - send real temperature read from TH02 sensor

You can choose what application to build by providing `--application` argument to `deploy.sh`

```bash
./deploy.sh --application temperature --log
```

By default `dummy` application is built.

## View Log Output

You can start log streaming separately — for example, in a second terminal window — with this command:

```bash
./deploy.sh --logonly
```

For more information, run

```bash
./deploy.sh --help
```

## Remote Debugging

This release supports remote debugging, and builds are enabled for remote debugging automatically. Change the value of the line

```
set(ENABLE_REMOTE_DEBUGGING 1)
```

in the root `CMakeLists.txt` file to `0` to disable this.

Enabling remote debugging in the build does not initiate a GDB session — you will have to do this manually. Follow the instructions in the [Microvisor documentation](https://www.twilio.com/docs/iot/microvisor/microvisor-remote-debugging) **Private Beta participants only**

This repo contains a `.gdbinit` file which sets the remote target to localhost on port 8001 to match the Twilio CLI Microvisor plugin remote debugging defaults.

### Remote Debugging Encryption

Remote debugging sessions are encrypted. To generate keys, add the `--gen-keys` switch to the deploy script call, or generate your own keys — see the documentation linked above for details.

Use the `--public-key-path` and `--private-key-path` options to either specify existing keys, or to specify where you would like script-generated keys to be stored. By default, keys will be stored in the `build` directory so they will not be inadvertently push to a public git repo:

```
./deploy.sh --private-key /path/to/private/key.pem --public-key /path/to/public/key.pem
```

You will need to pass the path to the private key to the Twilio CLI Microvisor plugin to decrypt debugging data. The deploy script will output this path for you.

## Copyright and Licensing

The sample code and Microvisor SDK is © 2022, Twilio, Inc. It is licensed under the terms of the [Apache 2.0 License](./LICENSE).

The SDK makes used of code © 2021, STMicroelectronics and affiliates. This code is licensed under terms described in [this file](https://github.com/twilio/twilio-microvisor-hal-stm32u5/blob/main/LICENSE-STM32CubeU5.md).

The SDK makes use [ARM CMSIS](https://github.com/ARM-software/CMSIS_5) © 2004, ARM. It is licensed under the terms of the [Apache 2.0 License](./LICENSE).

[FreeRTOS](https://freertos.org/) is © 2021, Amazon Web Services, Inc. It is licensed under the terms of the [Apache 2.0 License](./LICENSE).
