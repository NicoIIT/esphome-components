import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import (
    CONF_ID,
    CONF_INDEX,
    CONF_VARIANT,
    CONF_TRIGGER_ID,
    PLATFORM_ESP32,
)
from esphome import automation
from esphome.cpp_helpers import setup_entity
from .const import (
    CONF_BLE_ADV_HANDLER_ID,
    CONF_BLE_ADV_ENCODING,
    CONF_BLE_ADV_FORCED_ID,
)
from .codec import (
    BASE_CODEC_SCHEMA,
    BLE_ADV_CODECS,
    BleAdvEncoder,
    load_default_codecs,
    codec_to_code,
)
from .translator import (
    BASE_TRANSLATOR_SCHEMA,
    define_all_translators,
    load_default_translators,
    translator_to_code,
)
from esphome.components.esp32_ble import (
    CONF_BLE_ID,
    ESP32BLE,
)

CONF_BLE_ADV_CODEC_ID = "codec_id"

AUTO_LOAD = ["esp32_ble", "select", "number"]
DEPENDENCIES = ["esp32"]
CONFLICTS_WITH = ["esp32_ble_tracker"]
MULTI_CONF = False

bleadvhandler_ns = cg.esphome_ns.namespace('ble_adv_handler')
BleAdvHandler = bleadvhandler_ns.class_('BleAdvHandler', cg.Component)

BleAdvDecodedTrigger = bleadvhandler_ns.class_('BleAdvDecodedTrigger')
BleAdvRawTrigger = bleadvhandler_ns.class_('BleAdvRawTrigger')
BleAdvDecodedConstRef = bleadvhandler_ns.class_('BleAdvDecoded_t').operator("ref").operator("const")
BleAdvRawConstRef = bleadvhandler_ns.class_('BleAdvParam').operator("ref").operator("const")

def forced_id_mig_msg(forced_id, max_forced_id):
    trunc_id = forced_id & max_forced_id
    return f"If migrating from previous force_id 0x{forced_id:X}, use 0x{trunc_id:X} to avoid the need to re pair"

def validate_ble_adv_device(config):
    # validate CONF_BLE_ADV_CODEC_ID
    if CONF_BLE_ADV_CODEC_ID in config:
        if CONF_BLE_ADV_ENCODING in config:
            raise cv.Invalid(f"'{CONF_BLE_ADV_CODEC_ID}' and '{CONF_BLE_ADV_ENCODING}' are exclusive")
        if CONF_VARIANT in config:
            raise cv.Invalid(f"'{CONF_BLE_ADV_CODEC_ID}' and '{CONF_VARIANT}' are exclusive")
        if not CONF_BLE_ADV_FORCED_ID in config:
            config[CONF_BLE_ADV_FORCED_ID] = 0
        return config

    # validate default encoders
    if not CONF_BLE_ADV_ENCODING in config:
        raise cv.Invalid(f"{CONF_BLE_ADV_ENCODING} is missing")       
    encoding = config[CONF_BLE_ADV_ENCODING]
    if not encoding in BLE_ADV_CODECS:
        raise cv.Invalid(f"'{encoding}' is not a valid encoding - should be one of {list(BLE_ADV_CODECS.keys())}")       
    enc_params = BLE_ADV_CODECS[encoding]
    pvs = enc_params["variants"]
    # variants
    if not CONF_VARIANT in config:
        config[CONF_VARIANT] = enc_params["default_variant"]
    else:
        variant = config[CONF_VARIANT]
        if variant in enc_params.get("legacy_variants", []):
            raise cv.Invalid("DEPRECATED '%s - %s', %s" % (encoding, variant, enc_params["legacy_variants"][variant]))
        if not variant in pvs:
            raise cv.Invalid("Invalid variant '%s' for encoding '%s' - should be one of %s" % (variant, encoding, list(pvs.keys())))
    # forced_id
    if not CONF_BLE_ADV_FORCED_ID in config:
        config[CONF_BLE_ADV_FORCED_ID] = enc_params["default_forced_id"]
    else:
        forced_id = config[CONF_BLE_ADV_FORCED_ID]
        max_forced_id = pvs[variant].get("max_forced_id", 0xFFFFFFFF)
        if forced_id > max_forced_id :
            raise cv.Invalid(f"Invalid 'forced_id' for {encoding} - {variant}: 0x{forced_id:X}. Maximum: 0x{max_forced_id:X}. {forced_id_mig_msg(forced_id, max_forced_id)}.")
    return config

DEVICE_BASE_CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BLE_ADV_HANDLER_ID): cv.use_id(BleAdvHandler),
        cv.Optional(CONF_BLE_ADV_CODEC_ID): cv.use_id(BleAdvEncoder),
        cv.Optional(CONF_BLE_ADV_ENCODING): cv.string,
        cv.Optional(CONF_VARIANT): cv.string,
        cv.Optional(CONF_BLE_ADV_FORCED_ID): cv.hex_uint32_t,
        cv.Optional(CONF_INDEX, default=0): cv.All(cv.positive_int, cv.Range(min=0, max=255)),
    }
)

async def setup_ble_adv_device(var, config):
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_BLE_ADV_HANDLER_ID])
    await setup_entity(var, config)
    if CONF_BLE_ADV_CODEC_ID in config:
        codec = await cg.get_variable(config[CONF_BLE_ADV_CODEC_ID])
        cg.add(var.init(codec.get_encoding(), codec.get_variant()))
    else:
        cg.add(var.init(config[CONF_BLE_ADV_ENCODING], config[CONF_VARIANT]))
    cg.add(var.set_index(config[CONF_INDEX]))
    if config[CONF_BLE_ADV_FORCED_ID] > 0:
        cg.add(var.set_forced_id(config[CONF_BLE_ADV_FORCED_ID]))
    else:
        cg.add(var.set_forced_id(config[CONF_ID].id))

def load_defaults(config):
    load_default_translators(config[CONF_BLE_ADV_TRANSLATORS])
    load_default_codecs(config[CONF_BLE_ADV_CODECS], config[CONF_BLE_ADV_CODECS_DEBUG_MODE])
    return config

CONF_BLE_ADV_SCAN_ACTIVATED = "scan_activated"
CONF_BLE_ADV_CHECK_REENCODING = "check_reencoding"
CONF_BLE_ADV_LOG_RAW = "log_raw"
CONF_BLE_ADV_LOG_COMMAND = "log_command"
CONF_BLE_ADV_LOG_CONFIG = "log_config"
CONF_BLE_ADV_USE_MAX_TX_POWER = "use_max_tx_power"
CONF_BLE_ADV_ON_DECODED = "on_decoded"
CONF_BLE_ADV_ON_RAW = "on_raw"
CONF_BLE_ADV_TRANSLATORS = "translators"
CONF_BLE_ADV_CODECS = "codecs"
CONF_BLE_ADV_CODECS_DEBUG_MODE = "codecs_debug_mode"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleAdvHandler),
        cv.GenerateID(CONF_BLE_ID): cv.use_id(ESP32BLE),
        cv.Optional(CONF_BLE_ADV_SCAN_ACTIVATED, default=True): cv.boolean,
        cv.Optional(CONF_BLE_ADV_CHECK_REENCODING, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_LOG_RAW, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_LOG_COMMAND, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_LOG_CONFIG, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_USE_MAX_TX_POWER, default=False): cv.boolean,
        cv.Optional(CONF_BLE_ADV_ON_DECODED): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BleAdvDecodedTrigger),
        }),
        cv.Optional(CONF_BLE_ADV_ON_RAW): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BleAdvRawTrigger),
        }),
        cv.Optional(CONF_BLE_ADV_TRANSLATORS, default=[]): cv.ensure_list(BASE_TRANSLATOR_SCHEMA),
        cv.Optional(CONF_BLE_ADV_CODECS, default=[]): cv.ensure_list(BASE_CODEC_SCHEMA),
        cv.Optional(CONF_BLE_ADV_CODECS_DEBUG_MODE, default=[]): cv.ensure_list(cv.use_id(BleAdvEncoder)),
    }),
    cv.only_on([PLATFORM_ESP32]),
    load_defaults,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_setup_priority(300)) # start after Bluetooth
    define_all_translators(config.get(CONF_BLE_ADV_TRANSLATORS, []))
    for conf_tr in config.get(CONF_BLE_ADV_TRANSLATORS, []):
        trans = await translator_to_code(conf_tr)
    for conf_en in config.get(CONF_BLE_ADV_CODECS, []):
        enc = await codec_to_code(conf_en)
        cg.add(var.add_encoder(enc))
    await cg.register_component(var, config)
    cg.add(var.set_scan_activated(config[CONF_BLE_ADV_SCAN_ACTIVATED]))
    cg.add(var.set_check_reencoding(config[CONF_BLE_ADV_CHECK_REENCODING]))
    cg.add(var.set_logging(config[CONF_BLE_ADV_LOG_RAW], config[CONF_BLE_ADV_LOG_COMMAND], config[CONF_BLE_ADV_LOG_CONFIG]))
    cg.add(var.set_use_max_tx_power(config[CONF_BLE_ADV_USE_MAX_TX_POWER]))
    for conf in config.get(CONF_BLE_ADV_ON_DECODED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(BleAdvDecodedConstRef, "x")], conf)
    for conf in config.get(CONF_BLE_ADV_ON_RAW, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(BleAdvRawConstRef, "x")], conf)
    parent = await cg.get_variable(config[CONF_BLE_ID])
    cg.add(parent.register_gap_event_handler(var))
    cg.add(var.set_parent(parent))
