# BLE ADV ESPHome Components

Custom components for ESPHome using BLE Advertising.

## Requirements
Those components are [ESPHome external component](https://esphome.io/components/external_components.html). In order to use them you will need to have:
* A basic knowledge of [ESPHome](https://esphome.io/). A good entry point is [here](https://esphome.io/guides/getting_started_hassio.html).
* The [ESPHome integration](https://www.home-assistant.io/integrations/esphome/) in Home Assistant
* An [Espressif](https://www.espressif.com/) microcontroller supporting Bluetooth v4, such as any [ESP32](https://www.espressif.com/en/products/socs/esp32) based model. You can find some for a few dollars on any online marketplace searching for ESP32. ESP32-VROOM, ESP32-C3 and ESP32 Vemos Mini are known to work OK.

## Fans / Lamps controlled by BLE Advertising
Use this for various Chinese lamps that are controlled via BLE advertising packets.
Supported apps:

* LampSmart Pro
* Lamp Smart Pro - Soft Lighting / Smart Lighting
* FanLamp Pro
* ApplianceSmart
* Vmax smart
* Zhi Jia
* Zhi Guang
* ZhiMeiDengKong
* Smart Light (Only the control by device, not the Master Control)
* Other (Legacy), removed app from play store: 'FanLamp', 'ControlSwitch'

## Components
* [ble_adv_handler](components/ble_adv_handler/README.md), Main Technical component centralizing the interaction with the BLE ADV Stack:
  * listens to BLE ADV remotes / phone app and transmit the commands to ble_adv_remote and ble_adv_controller
  * sends BLE ADV messages generated by ble_adv_controller to the controlled devices
* [ble_adv_controller](components/ble_adv_controller/README.md), the functionnal component representing a Controlled Device and its related entities (Fan / Light / Button)
* [ble_adv_remote](components/ble_adv_remote/README.md), the functionnal component representing a Remote, allowing to perform actions and control/publish to ble_adv_controller

## Basic How To
1. As a preliminary step, be sure to be able to create a base ESPHome configuration from the ESPHome Dashboard, install it to your ESP32, have it available in Home Assistant and be able to access the logs (needed in case of issue). This is a big step if you are new to ESPHome but on top of [ESPHome doc](https://esphome.io/guides/getting_started_hassio.html) you will find tons of tutorial on the net for that.
2. Add to your up and running ESPHome configuration the reference to this repo using ([ESPHome external component](https://esphome.io/components/external_components.html))
3. Listen to the traffic generated by your Paired phone App and / or you Remote using the [ble_adv_handler](components/ble_adv_handler/README.md).
4. Add a device controller [ble_adv_controller](components/ble_adv_controller/README.md) with its configuration (encoding/variant/forced_id/index) as extracted from the Phone App traffic in Step (3). If several configs are available with several variants, use the variant with the higher value as a start.
5. Add one or several light or fan entities to the configuration with the `ble_adv_controller` platform
6. Find the relevant `variant` and `duration` corresponding to your device thanks to [Dynamic configuration](#dynamic-configuration)
7. Enjoy controlling your BLE light with Home Assistant created Entities:
* Light(s) entity allowing the control of Color Temperature and Brightness
* Fan entity allowing the control of Speed (3 or 6 levels), direction forward / reverse and Oscillation
8. Synchronize your Physical Remote with [ble_adv_remote](components/ble_adv_remote/README.md)

More details are available in the documentation of each component.

## For the Developers
The [wiki](../../wiki/Developer-Guide) is avialable for info on Architecture, Helpers to contribute more easily, ...

## Credits
Based on the initial work from:
* @MasterDevX, [lampify](https://github.com/MasterDevX/lampify)
* @flicker581, [lampsmart_pro_light](https://github.com/flicker581/esphome-lampsmart)
* @aronsky, [ble_adv_light](https://github.com/aronsky/esphome-components)
* @14roiron, [zhijia encoders](https://github.com/aronsky/esphome-components/issues/11), [investigations](https://github.com/aronsky/esphome-components/issues/18)
* All testers and bug reporters from the initial threads:
  * https://community.home-assistant.io/t/controlling-ble-ceiling-light-with-ha/520612/199
  * https://github.com/aronsky/esphome-components/pull/17
