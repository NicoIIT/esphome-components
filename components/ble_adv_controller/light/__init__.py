import logging

import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_CONSTANT_BRIGHTNESS,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_ID,
    CONF_MIN_BRIGHTNESS,
    CONF_RESTORE_MODE,
    CONF_TYPE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

from .. import (
    ENTITY_BASE_CONFIG_SCHEMA,
    BleAdvEntity,
    bleadvcontroller_ns,
    entity_base_code_gen,
)
from ..const import (
    CONF_BLE_ADV_SECONDARY,
    CONF_BLE_ADV_SPLIT_DIM_CCT,
    CONF_BLE_ADV_SPLIT_DIM_RGB,
)

BleAdvLightBase = bleadvcontroller_ns.class_(
    "BleAdvLightBase", light.LightOutput, light.LightState, BleAdvEntity
)
BleAdvLightCww = bleadvcontroller_ns.class_("BleAdvLightCww", BleAdvLightBase)
BleAdvLightBinary = bleadvcontroller_ns.class_("BleAdvLightBinary", BleAdvLightBase)
BleAdvLightRGB = bleadvcontroller_ns.class_("BleAdvLightRGB", BleAdvLightBase)


def deprecated_split_option(value):
    if not value:
        logging.error(
            f"'{CONF_BLE_ADV_SPLIT_DIM_CCT}' / '{CONF_BLE_ADV_SPLIT_DIM_RGB}' - This option is DEPRECATED. A new variant zhijia / v2_bl is available to replace it in case of blinking light."
        )
    else:
        logging.warning(
            f"DEPRECATION: '{CONF_BLE_ADV_SPLIT_DIM_CCT}' / '{CONF_BLE_ADV_SPLIT_DIM_RGB}' is DEPRECATED, you can safely remove it."
        )
    return value


def deprecated_secondary_option(value):
    raise cv.Invalid(
        f"'{CONF_BLE_ADV_SECONDARY}' option is DEPRECATED. It must be replaced by the options:\n\n    type: 'onoff' # only if you have not specified it already\n    index: 1\n\nSee Change log."
    )


LIGHT_BASE_CONFIG_SCHEMA = ENTITY_BASE_CONFIG_SCHEMA.extend(
    {
        # override default value for restore mode, to always restore as it was if possible
        cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(
            light.RESTORE_MODES, upper=True, space="_"
        ),
        # override default value of default_transition_length to 0s as mostly not supported by those lights
        cv.Optional(
            CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_BLE_ADV_SPLIT_DIM_RGB): deprecated_split_option,
        cv.Optional(CONF_BLE_ADV_SPLIT_DIM_CCT): deprecated_split_option,
        cv.Optional(CONF_BLE_ADV_SECONDARY): deprecated_secondary_option,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Any(
        # Cold / Warm / White Light
        light.RGB_LIGHT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BleAdvLightCww),
                cv.Optional(CONF_TYPE, default="cww"): cv.one_of("cww"),
                cv.Optional(
                    CONF_COLD_WHITE_COLOR_TEMPERATURE, default="167 mireds"
                ): cv.color_temperature,
                cv.Optional(
                    CONF_WARM_WHITE_COLOR_TEMPERATURE, default="333 mireds"
                ): cv.color_temperature,
                cv.Optional(CONF_CONSTANT_BRIGHTNESS, default=False): cv.boolean,
                cv.Optional(CONF_MIN_BRIGHTNESS, default="2%"): cv.percentage,
            }
        ).extend(LIGHT_BASE_CONFIG_SCHEMA),
        # Binary Light
        light.RGB_LIGHT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BleAdvLightBinary),
                cv.Required(CONF_TYPE): cv.one_of("onoff"),
            }
        ).extend(LIGHT_BASE_CONFIG_SCHEMA),
        # RGB Light
        light.RGB_LIGHT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BleAdvLightRGB),
                cv.Required(CONF_TYPE): cv.one_of("rgb"),
            }
        ).extend(LIGHT_BASE_CONFIG_SCHEMA),
    ),
    cv.has_none_or_all_keys(
        [CONF_COLD_WHITE_COLOR_TEMPERATURE, CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ),
    light.validate_color_temperature_channels,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await entity_base_code_gen(var, config)
    cg.add(cg.App.register_light(var))
    await light.setup_light_core_(var, var, config)
    if config[CONF_TYPE] == "onoff":
        cg.add(var.set_traits())
    elif config[CONF_TYPE] == "cww":
        cg.add(
            var.set_traits(
                config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
                config[CONF_WARM_WHITE_COLOR_TEMPERATURE],
            )
        )
        cg.add(var.set_constant_brightness(config[CONF_CONSTANT_BRIGHTNESS]))
        cg.add(var.set_min_brightness(config[CONF_MIN_BRIGHTNESS] * 100, 0, 100, 1))
    elif config[CONF_TYPE] == "rgb":
        cg.add(var.set_traits())
