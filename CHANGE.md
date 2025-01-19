# MAJOR CHANGE LOG

## 25 January 2025

### New features
* Support for `Smart Light` App (beta), `encoding: agarce`
* Creation of the Developer's Guide in the [wiki](../../wiki/Developer-Guide)
* Creation of the Change Log (this page)
* Possibility to define codecs and translators in configuration directly, and more easilly debug them, details in [wiki](../../wiki/Supporting-a-new-Physical-Remote-or-application#user-defined-codecs-and-translators). Mainly for Developers (Python and C++ minimum skills required)
* Various fixes for RGB Lights
* Refactoring of Zhi Jia encoders (TECH)
* Refactoring translators C++ generation (TECH)

### Interface changes and Deprecations
* The option `show_config` is DEPRECATED as it simply does not work properly. If you do not want to see those configs in HA, you can hide them in HA UI directly
* The option `secondary` is DEPRECATED as it was preventing a standardization of the framework. It is replaced by the entity `index` to be setup to 1 for secondary lights (0 by default for main lights). The `type` option also needs to be specified for your secondary light (`onoff`, `cww` or `rgb`)
* The options `separate_dim_cct` and `separate_dim_rgb` are DEPRECATED. They were needed systematically for some Zhi Jia / Zhi Mei devices whereas their default value was not the good one for those devices... At the end, those options were ONLY needed to try to solve some flickering issues for some Zhi Jia v2 lights. The feature has now been integrated directly at variant level and is then no more needed: a dedicated new variant `zhijia/v2_fl` has been created to handle the flickering issue.

## 14 January 2025

### New features
* Support for [RGB light type](components/ble_adv_controller/README.md#configuration-for-rgb-light) (beta), using `type: rgb` at light level
* Support for `ZhiMeiDengKong` App (beta), `encoding: zhimei`
* Possibility to increase the bluetooth range with option [use_max_tx_power](README.md#variables)
* More `Actions` and `Automation Triggers` available

## September 2024 (Dev branch) / 9 Januray 2025 (Main branch)

### New features
* Updated list of officially fully [supported Phone Apps](README.md#fans--lamps-controlled-by-ble-advertising), with corrected variants to send exactly the same messages than the Phone Apps
* [Listening to Phone App traffic](components/ble_adv_handler/README.md) and extract parameters to shadow it with a ble_adv_controller without the need to pair the ESP32
* [Listening to physical remotes](components/ble_adv_remote/README.md) and update HA state when remote is used, or do whatever action you want
* BLE ADV raw message decoding and [injection](components/ble_adv_controller#actions), for the ones that have devices that cannot be controlled by the Phone Apps given.

### Interface changes and Deprecations
* The `forced_id` is now verified in order to see if its length is supported by the codec used, this is done to prevent entering values that would look different but that would be truncated by the codec choosen and then ending in the same value at the end... The Error message will give you the relevant way to convert it as to avoid the need for re pair.
* The encoding / variant names have been reviewed, an Error will explain you the proper encoding / variant to select for your case as defined [here](components/ble_adv_controller/README.md)
* The native button is DEPRECATED as not providing any additional features compared to [standard template button](https://esphome.io/components/button/template.html), see how to [replace it](components/ble_adv_controller/README.md#configuration-for-button) or follow the Error message.
* The native scanner it includes to listen to config and remote commands makes it incompatible with the [ESP32 BLE Tracker](https://esphome.io/components/esp32_ble_tracker) or the components that would include it, such as [Bluetooth Proxy](https://esphome.io/components/bluetooth_proxy.html).
* 3 components are now used, with dedicated doc: `ble_adv_handler`, `ble_adv_remote`, `ble_adv_controller`

## August 2024

This one is not detailed, it was the first big change which I hope is now widely adopted.
Creation of ble_adv_controller component (instead of ble_adv_light), Support for Fan, etc ...
