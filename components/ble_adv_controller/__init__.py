import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import (
    CONF_DURATION,
    CONF_ID,
    CONF_REVERSED,
    CONF_TYPE,
 )
from esphome.cpp_helpers import setup_entity
from esphome.components.ble_adv_handler import (
    DEVICE_BASE_CONFIG_SCHEMA,
    setup_ble_adv_device,
    validate_ble_adv_device,
)
from .const import (
    CONF_BLE_ADV_CONTROLLER_ID,
    CONF_BLE_ADV_MAX_DURATION,
    CONF_BLE_ADV_SEQ_DURATION,
    CONF_BLE_ADV_SHOW_CONFIG,
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
        cv.Optional(CONF_BLE_ADV_MAX_DURATION, default=3000): cv.All(cv.positive_int, cv.Range(min=300, max=10000)),
        cv.Optional(CONF_BLE_ADV_SEQ_DURATION, default=100): cv.All(cv.positive_int, cv.Range(min=0, max=150)),
        cv.Optional(CONF_REVERSED, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_SHOW_CONFIG, default=True): cv.boolean,
    }),
    validate_ble_adv_device,
)

async def entity_base_code_gen(var, config):
    await cg.register_parented(var, config[CONF_BLE_ADV_CONTROLLER_ID])
    await cg.register_component(var, config)
    await setup_entity(var, config)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_setup_priority(300)) # start after Bluetooth
    await cg.register_component(var, config)
    await setup_ble_adv_device(var, config)
    cg.add(var.set_min_tx_duration(config[CONF_DURATION], 100, 500, 10))
    cg.add(var.set_max_tx_duration(config[CONF_BLE_ADV_MAX_DURATION]))
    cg.add(var.set_seq_duration(config[CONF_BLE_ADV_SEQ_DURATION]))
    cg.add(var.set_reversed(config[CONF_REVERSED]))
    cg.add(var.set_show_config(config[CONF_BLE_ADV_SHOW_CONFIG]))


