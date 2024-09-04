import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import (
    CONF_DURATION,
    CONF_ID,
    CONF_REVERSED,
 )
from esphome.cpp_helpers import setup_entity
from esphome.components.ble_adv_handler import (
    DEVICE_BASE_CONFIG_SCHEMA,
    setup_ble_adv_device,
    validate_ble_adv_device,
)
from esphome.automation import (
    maybe_simple_id,
    register_action,
    Action,
)
from .const import (
    CONF_BLE_ADV_CONTROLLER_ID,
    CONF_BLE_ADV_MAX_DURATION,
    CONF_BLE_ADV_SEQ_DURATION,
    CONF_BLE_ADV_SHOW_CONFIG,
    CONF_BLE_ADV_CANCEL_TIMER,
)

AUTO_LOAD = ["ble_adv_handler"]
MULTI_CONF = True

bleadvcontroller_ns = cg.esphome_ns.namespace('ble_adv_controller')
BleAdvController = bleadvcontroller_ns.class_('BleAdvController', cg.Component, cg.EntityBase)
BleAdvEntity = bleadvcontroller_ns.class_('BleAdvEntity', cg.Component)

ENTITY_BASE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_BLE_ADV_CONTROLLER_ID): cv.use_id(BleAdvController),
    }
)

CONFIG_SCHEMA = cv.All(
    DEVICE_BASE_CONFIG_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(BleAdvController),
        cv.Optional(CONF_DURATION, default=200): cv.All(cv.positive_int, cv.Range(min=100, max=500)),
        cv.Optional(CONF_BLE_ADV_MAX_DURATION, default=1000): cv.All(cv.positive_int, cv.Range(min=300, max=10000)),
        cv.Optional(CONF_BLE_ADV_SEQ_DURATION, default=100): cv.All(cv.positive_int, cv.Range(min=0, max=150)),
        cv.Optional(CONF_REVERSED, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_SHOW_CONFIG, default=True): cv.boolean,
        cv.Optional(CONF_BLE_ADV_CANCEL_TIMER, default=True): cv.boolean,
    }),
    validate_ble_adv_device,
)

async def entity_base_code_gen(var, config):
    await cg.register_parented(var, config[CONF_BLE_ADV_CONTROLLER_ID])
    await cg.register_component(var, config)
    await setup_entity(var, config)
    cg.add(var.init())

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_setup_priority(300)) # start after Bluetooth
    await setup_ble_adv_device(var, config)
    cg.add(var.set_min_tx_duration(config[CONF_DURATION], 100, 500, 10))
    cg.add(var.set_max_tx_duration(config[CONF_BLE_ADV_MAX_DURATION]))
    cg.add(var.set_seq_duration(config[CONF_BLE_ADV_SEQ_DURATION]))
    cg.add(var.set_reversed(config[CONF_REVERSED]))
    cg.add(var.set_show_config(config[CONF_BLE_ADV_SHOW_CONFIG]))
    cg.add(var.set_cancel_timer_on_any_change(config[CONF_BLE_ADV_CANCEL_TIMER]))

###############
##  ACTIONS  ##
###############
async def init_controller_action(config, action_id, template_arg):
    var = cg.new_Pvariable(action_id, template_arg)
    controller = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_controller(controller))
    return var

def reg_controller_action(name, class_name, schema):
    return register_action(f"ble_adv_controller.{name}", bleadvcontroller_ns.class_(class_name, Action), schema)

CONTROLLER_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(BleAdvController),
    }
)

@reg_controller_action("cancel_timer", "CancelTimerAction", CONTROLLER_ACTION_SCHEMA)
@reg_controller_action("pair", "PairAction", CONTROLLER_ACTION_SCHEMA)
@reg_controller_action("unpair", "UnPairAction", CONTROLLER_ACTION_SCHEMA)
async def controller_simple_action_to_code(config, action_id, template_arg, args):
    return await init_controller_action(config, action_id, template_arg)

CONF_BLE_ADV_CMD = "cmd"
CONF_BLE_ADV_PARAM1 = "param1"
CONF_BLE_ADV_ARGS = "args"
CONTROLLER_ACTION_CMD_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(BleAdvController),
        cv.Required(CONF_BLE_ADV_CMD): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_BLE_ADV_PARAM1, default=0): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_BLE_ADV_ARGS, default=[0,0,0]): cv.All(cv.ensure_list(cv.uint8_t), cv.Length(min=1, max=3)),
    }
)

@reg_controller_action("custom_cmd", "CustomCmdAction", CONTROLLER_ACTION_CMD_SCHEMA)
async def custom_cmd_action_to_code(config, action_id, template_arg, args):
    var = await init_controller_action(config, action_id, template_arg)
    cg.add(var.set_cmd(await cg.templatable(config[CONF_BLE_ADV_CMD], args, cv.uint8_t)))
    cg.add(var.set_param1(await cg.templatable(config[CONF_BLE_ADV_PARAM1], args, cv.uint8_t)))
    cg.add(var.set_args(await cg.templatable(config[CONF_BLE_ADV_ARGS], args, cv.ensure_list(cv.uint8_t))))
    return var

CONF_BLE_ADV_RAW = "raw"
CONTROLLER_ACTION_RAW_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(BleAdvController),
        cv.Required(CONF_BLE_ADV_RAW): cv.templatable(cv.string),
    }
)

@reg_controller_action("raw_inject", "RawInjectAction", CONTROLLER_ACTION_RAW_SCHEMA)
async def raw_inject_action_to_code(config, action_id, template_arg, args):
    var = await init_controller_action(config, action_id, template_arg)
    cg.add(var.set_raw(await cg.templatable(config[CONF_BLE_ADV_RAW], args, cv.string)))
    return var
