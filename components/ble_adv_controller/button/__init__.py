import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from esphome.const import (
    CONF_OUTPUT_ID,
    ENTITY_CATEGORY_CONFIG,
    DEVICE_CLASS_IDENTIFY,
)

from .. import (
    bleadvcontroller_ns,
    ENTITY_BASE_CONFIG_SCHEMA,
    entity_base_code_gen,
    BleAdvEntity,
)

INVALID_CUSTOM = """
'custom' command format changed, please perform migration:
  - Migration for Zhijia / FanLamp V1: 
    From:
        cmd: custom
        args: [A,B,C,D,E]
    To:
        custom_cmd: A
        args: [B,C,D] (optional)
  - Migration for FanLamp V2/V3:
    From:
        cmd: custom
        args: [A,B,C,D,E]
    To:
        custom_cmd: A
        param: C
        args: [D,E] (optional)
"""

CONF_BLE_ADV_CMD = "cmd"
CONF_BLE_ADV_ARGS = "args"
CONF_BLE_ADV_PARAM = "param"
CONF_BLE_ADV_CUSTOM_CMD = "custom_cmd"

def validate_cmd(cmd):
    if cmd == "custom":
        raise cv.Invalid(INVALID_CUSTOM)
    return cmd

BleAdvButton = bleadvcontroller_ns.class_('BleAdvButton', button.Button, BleAdvEntity)

CONFIG_SCHEMA = cv.All(
    button.button_schema(
        BleAdvButton,
        device_class=DEVICE_CLASS_IDENTIFY,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(
        {
            cv.Optional(CONF_BLE_ADV_CMD, default=""): cv.All(cv.one_of("pair", "unpair", "custom", ""), validate_cmd),
            cv.Optional(CONF_BLE_ADV_CUSTOM_CMD, default=0): cv.uint8_t,
            cv.Optional(CONF_BLE_ADV_PARAM, default=0): cv.uint8_t,
            cv.Optional(CONF_BLE_ADV_ARGS, default=[0,0]): cv.ensure_list(cv.uint8_t),
        }
    ).extend(ENTITY_BASE_CONFIG_SCHEMA),

)

async def to_code(config):
    var = await button.new_button(config)
    await entity_base_code_gen(var, config)
    if config[CONF_BLE_ADV_CMD] == "pair":
        cg.add(var.set_pair())
    elif config[CONF_BLE_ADV_CMD] == "unpair":
        cg.add(var.set_unpair())
    else:
        cg.add(var.set_custom_cmd(config[CONF_BLE_ADV_CUSTOM_CMD]))
        cg.add(var.set_param(config[CONF_BLE_ADV_PARAM]))
        args = config[CONF_BLE_ADV_ARGS]
        args += [0] * len(args) # pad with 0
        cg.add(var.set_args(args))
