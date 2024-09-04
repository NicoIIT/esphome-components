import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.automation import (
    maybe_simple_id,
)

from esphome.const import (
    CONF_ID,
    CONF_OUTPUT_ID,
    CONF_RESTORE_MODE,
    CONF_OSCILLATING,
    CONF_SPEED,
    CONF_DIRECTION,
    CONF_STATE,
)

from .. import (
    bleadvcontroller_ns,
    ENTITY_BASE_CONFIG_SCHEMA,
    entity_base_code_gen,
    BleAdvEntity,
    reg_controller_action,
)

from ..const import (
    CONF_BLE_ADV_SPEED_COUNT,
    CONF_BLE_ADV_DIRECTION_SUPPORTED,
    CONF_BLE_ADV_OSCILLATION_SUPPORTED,
    CONF_BLE_ADV_FORCED_REFRESH_ON_START,
)

BleAdvFan = bleadvcontroller_ns.class_('BleAdvFan', fan.Fan, BleAdvEntity)

CONFIG_SCHEMA = cv.All(
    fan.FAN_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BleAdvFan),
            cv.Optional(CONF_BLE_ADV_SPEED_COUNT, default=6): cv.one_of(0,3,6),
            cv.Optional(CONF_BLE_ADV_DIRECTION_SUPPORTED, default=True): cv.boolean,
            cv.Optional(CONF_BLE_ADV_OSCILLATION_SUPPORTED, default=False): cv.boolean,
            cv.Optional(CONF_BLE_ADV_FORCED_REFRESH_ON_START, default=False): cv.boolean,
            # override default value for restore mode, to always restore as it was if possible
            cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(fan.RESTORE_MODES, upper=True, space="_"),
        }
    ).extend(ENTITY_BASE_CONFIG_SCHEMA),
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await entity_base_code_gen(var, config)
    await fan.register_fan(var, config)
    cg.add(var.set_speed_count(config[CONF_BLE_ADV_SPEED_COUNT]))
    cg.add(var.set_direction_supported(config[CONF_BLE_ADV_DIRECTION_SUPPORTED]))
    cg.add(var.set_oscillation_supported(config[CONF_BLE_ADV_OSCILLATION_SUPPORTED]))
    cg.add(var.set_forced_refresh_on_start(config[CONF_BLE_ADV_FORCED_REFRESH_ON_START]))

###############
##  ACTIONS  ##
###############
FAN_PUBLISH_STATE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(fan.Fan),
        cv.Optional(CONF_STATE): cv.templatable(cv.boolean),
        cv.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
        cv.Optional(CONF_SPEED): cv.templatable(cv.int_range(1)),
        cv.Optional(CONF_DIRECTION): cv.templatable(cv.enum(fan.FAN_DIRECTION_ENUM, upper=True)),
    }
)

@reg_controller_action("fan.publish_state", 'FanPublishStateAction', FAN_PUBLISH_STATE_ACTION_SCHEMA)
async def fan_publish_state_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if (state := config.get(CONF_STATE)) is not None:
        template_ = await cg.templatable(state, args, bool)
        cg.add(var.set_state(template_))
    if (oscillating := config.get(CONF_OSCILLATING)) is not None:
        template_ = await cg.templatable(oscillating, args, bool)
        cg.add(var.set_oscillating(template_))
    if (speed := config.get(CONF_SPEED)) is not None:
        template_ = await cg.templatable(speed, args, int)
        cg.add(var.set_speed(template_))
    if (direction := config.get(CONF_DIRECTION)) is not None:
        template_ = await cg.templatable(direction, args, fan.FanDirection)
        cg.add(var.set_direction(template_))
    return var
