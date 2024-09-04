import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome import automation
from esphome.components.ble_adv_handler import (
    DEVICE_BASE_CONFIG_SCHEMA,
    setup_ble_adv_device,
    validate_ble_adv_device,
    bleadvhandler_ns,
)
from esphome.components.ble_adv_controller import (
    BleAdvController,
)
from esphome.automation import (
    maybe_simple_id,
    register_action,
    Action,
)

AUTO_LOAD = ["ble_adv_handler"]
MULTI_CONF = True

CONF_BLE_ADV_LEVEL_COUNT = "level_count"
CONF_BLE_ADV_CMD_AS_TOGGLE = "commands_as_toggle"
CONF_BLE_ADV_ON_COMMAND = "on_command"
CONF_BLE_ADV_REM_CONTROL = "controlling"
CONF_BLE_ADV_REM_PUBLISH = "publishing"
CONF_BLE_ADV_CYCLE = "cycle"

bleadvremote_ns = cg.esphome_ns.namespace('ble_adv_remote')
BleAdvRemote = bleadvremote_ns.class_('BleAdvRemote', cg.Component, cg.EntityBase)

BleAdvGenCmd = bleadvhandler_ns.class_('BleAdvGenCmd')
BleAdvGenCmdConstRef = BleAdvGenCmd.operator("ref").operator("const")
BleAdvCmdTrigger = bleadvremote_ns.class_("BleAdvCmdTrigger", automation.Trigger.template(BleAdvGenCmd))

CONFIG_SCHEMA = cv.All(
    DEVICE_BASE_CONFIG_SCHEMA.extend({
        cv.GenerateID(CONF_ID): cv.declare_id(BleAdvRemote),
        cv.Optional(CONF_BLE_ADV_CMD_AS_TOGGLE, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_LEVEL_COUNT, default=10): cv.uint8_t,
        cv.Optional(CONF_BLE_ADV_CYCLE, default=[]): cv.ensure_list(cv.All(cv.ensure_list(cv.percentage), cv.Length(min=2, max=2))),
        cv.Optional(CONF_BLE_ADV_REM_CONTROL): cv.ensure_list(cv.use_id(BleAdvController)),
        cv.Optional(CONF_BLE_ADV_REM_PUBLISH): cv.ensure_list(cv.use_id(BleAdvController)),
        cv.Optional(CONF_BLE_ADV_ON_COMMAND): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BleAdvCmdTrigger),
        }),
    }),
    validate_ble_adv_device,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_setup_priority(300)) # start after Bluetooth
    await setup_ble_adv_device(var, config)
    cg.add(var.set_toggle(config[CONF_BLE_ADV_CMD_AS_TOGGLE]))
    cg.add(var.set_level_count(config[CONF_BLE_ADV_LEVEL_COUNT]))
    cg.add(var.set_cycle(config[CONF_BLE_ADV_CYCLE]))
    for conf in config.get(CONF_BLE_ADV_ON_COMMAND, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(BleAdvGenCmdConstRef, "x")], conf)
    for cont in config.get(CONF_BLE_ADV_REM_CONTROL, []):
        controller = await cg.get_variable(cont)
        cg.add(var.add_controlling(controller))
    for cont in config.get(CONF_BLE_ADV_REM_PUBLISH, []):
        controller = await cg.get_variable(cont)
        cg.add(var.add_publishing(controller))

###############
##  ACTIONS  ##
###############
async def init_remote_action(config, action_id, template_arg):
    var = cg.new_Pvariable(action_id, template_arg)
    remote = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_remote(remote))
    return var

def reg_remote_action(name, class_name, schema):
    return register_action(f"ble_adv_remote.{name}", bleadvremote_ns.class_(class_name, Action), schema)

REMOTE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(BleAdvRemote),
    }
)

@reg_remote_action("unpair", "UnPairAction", REMOTE_ACTION_SCHEMA)
async def remote_simple_action_to_code(config, action_id, template_arg, args):
    return await init_remote_action(config, action_id, template_arg)
