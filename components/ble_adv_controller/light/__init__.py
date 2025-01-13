import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.cpp_helpers import setup_entity
from esphome.const import (
    CONF_CONSTANT_BRIGHTNESS,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
    CONF_MIN_BRIGHTNESS,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_RESTORE_MODE,
    CONF_TYPE,
    CONF_ID,
)

from .. import (
    bleadvcontroller_ns,
    ENTITY_BASE_CONFIG_SCHEMA,
    entity_base_code_gen,
    BleAdvEntity,
)

from ..const import (
    CONF_BLE_ADV_SECONDARY,
    CONF_BLE_ADV_SPLIT_DIM_CCT,
)

BleAdvLightBase = bleadvcontroller_ns.class_('BleAdvLightBase', light.LightOutput, BleAdvEntity)
BleAdvLightCww = bleadvcontroller_ns.class_('BleAdvLightCww', BleAdvLightBase)
BleAdvLightBinary = bleadvcontroller_ns.class_('BleAdvLightBinary', BleAdvLightBase)

LIGHT_BASE_CONFIG_SCHEMA = ENTITY_BASE_CONFIG_SCHEMA.extend({
    # override default value for restore mode, to always restore as it was if possible
    cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(light.RESTORE_MODES, upper=True, space="_"),
})

CONFIG_SCHEMA = cv.All(
    cv.Any(
        # BACKWARD COMPATIBILITY: Secondary light with no type, force type to 'onoff'
        light.RGB_LIGHT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BleAdvLightBinary),
                cv.Optional(CONF_TYPE, default='onoff'): cv.one_of('onoff'),
                cv.Required(CONF_BLE_ADV_SECONDARY): cv.one_of(True),
            }
        ).extend(LIGHT_BASE_CONFIG_SCHEMA),

        # Cold / Warm / WHite Light
        light.RGB_LIGHT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BleAdvLightCww),
                cv.Optional(CONF_TYPE, default='cww'): cv.one_of('cww'),
                cv.Optional(CONF_BLE_ADV_SECONDARY, default=False): cv.boolean,
                cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="167 mireds"): cv.color_temperature,
                cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="333 mireds"): cv.color_temperature,
                cv.Optional(CONF_CONSTANT_BRIGHTNESS, default=False): cv.boolean,
                cv.Optional(CONF_MIN_BRIGHTNESS, default="2%"): cv.percentage,
                cv.Optional(CONF_BLE_ADV_SPLIT_DIM_CCT, default=False): cv.boolean,
                # override default value of default_transition_length to 0s as mostly not supported by those lights
                cv.Optional(CONF_DEFAULT_TRANSITION_LENGTH, default="0s"): cv.positive_time_period_milliseconds,
            }
        ).extend(LIGHT_BASE_CONFIG_SCHEMA),

        # Binary Light
        light.RGB_LIGHT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BleAdvLightBinary),
                cv.Required(CONF_TYPE): cv.one_of('onoff'),
                cv.Optional(CONF_BLE_ADV_SECONDARY, default=False): cv.boolean,
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
    cg.add(var.set_secondary(config[CONF_BLE_ADV_SECONDARY]))
    if config[CONF_TYPE] == 'onoff':
        cg.add(var.set_traits())
    elif config[CONF_TYPE] == 'cww':
        cg.add(var.set_traits(config[CONF_COLD_WHITE_COLOR_TEMPERATURE], config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
        cg.add(var.set_constant_brightness(config[CONF_CONSTANT_BRIGHTNESS]))
        cg.add(var.set_split_dim_cct(config[CONF_BLE_ADV_SPLIT_DIM_CCT]))
        cg.add(var.set_min_brightness(config[CONF_MIN_BRIGHTNESS] * 100, 0, 100, 1))
    else:
        # TODO: add RGB
        pass
