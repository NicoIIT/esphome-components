import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import (
    CONF_ID,
    CONF_INDEX,
    CONF_VARIANT,
    PLATFORM_ESP32,
)
from esphome.cpp_helpers import setup_entity
from .const import (
    CONF_BLE_ADV_HANDLER_ID,
    CONF_BLE_ADV_ENCODING,
    CONF_BLE_ADV_FORCED_ID,
)
from esphome.components.esp32_ble import (
    CONF_BLE_ID,
    ESP32BLE,
)

AUTO_LOAD = ["esp32_ble", "select", "number"]
DEPENDENCIES = ["esp32"]
CONFLICTS_WITH = ["esp32_ble_tracker"]
MULTI_CONF = False

bleadvhandler_ns = cg.esphome_ns.namespace('ble_adv_handler')
BleAdvEncoder = bleadvhandler_ns.class_('BleAdvEncoder')
BleAdvMultiEncoder = bleadvhandler_ns.class_('BleAdvMultiEncoder', BleAdvEncoder)
BleAdvHandler = bleadvhandler_ns.class_('BleAdvHandler', cg.Component)

FanLampEncoderV1 = bleadvhandler_ns.class_('FanLampEncoderV1')
FanLampEncoderV2 = bleadvhandler_ns.class_('FanLampEncoderV2')
ZhijiaEncoderV0 = bleadvhandler_ns.class_('ZhijiaEncoderV0')
ZhijiaEncoderV1 = bleadvhandler_ns.class_('ZhijiaEncoderV1')
ZhijiaEncoderV2 = bleadvhandler_ns.class_('ZhijiaEncoderV2')
RemoteEncoder = bleadvhandler_ns.class_('RemoteEncoder')

CT = bleadvhandler_ns.enum('CommandType')
BleAdvEncCmd = bleadvhandler_ns.class_('BleAdvEncCmd')
BleAdvGenCmd = bleadvhandler_ns.class_('BleAdvGenCmd')
CommandTranslator = bleadvhandler_ns.class_('CommandTranslator')

def multi_args(multi = 1):
    return { "raw_g2e": f"e.args[0] = (float){multi} * g.args[0]; e.args[1] = (float){multi} * g.args[1];",
             "raw_e2g": f"g.args[0] = ((float)e.args[0]) / (float){multi}; g.args[1] = ((float)e.args[1]) / (float){multi};" }

def multi_arg0(multi = 1):
    return { "raw_g2e": f"e.args[0] = (float){multi} * g.args[0];",
             "raw_e2g": f"g.args[0] = ((float)e.args[0]) / (float){multi};" }

def param1_arg0():
    return { "raw_g2e": f"e.param1 = (int)g.args[0] & 0xFF; e.args[0] = (int)g.args[0] >> 8;",
             "raw_e2g": f"g.args[0] = e.param1 + e.args[0] * 256;" }

def trunc_arg0():
    return { "raw_g2e": f"e.args[0] = std::min((int)g.args[0], 0xFF);",
             "raw_e2g": f"g.args[0] = e.args[0];" }

def zhijia_v0_multi_args():
    return {"raw_g2e": f"uint16_t arg16 = 1000*g.args[0]; e.args[1] = (arg16 & 0xFF00) >> 8; e.args[2] = arg16 & 0x00FF;",
            "raw_e2g": f"g.args[0] = (float)((((uint16_t)e.args[1]) << 8) | e.args[2]) / 1000.f;" }

def trans_cmd(gcmd, ecmd, gsupp = {}, esupp = {}):
    return {"g": { "cmd": gcmd } | gsupp, "e": { "cmd" : ecmd } | esupp}

FanLampCommonTranslators = [
    trans_cmd(CT.PAIR, 0x28),
    trans_cmd(CT.UNPAIR, 0x45),
    trans_cmd(CT.ALL_OFF, 0x6F),
    trans_cmd(CT.LIGHT_TOGGLE, 0x09),
    trans_cmd(CT.LIGHT_ON, 0x10),
    trans_cmd(CT.LIGHT_OFF, 0x11),
    trans_cmd(CT.LIGHT_SEC_ON, 0x12),
    trans_cmd(CT.LIGHT_SEC_OFF, 0x13),
    trans_cmd(CT.LIGHT_WCOLOR, 0x21, {"param":0}) | multi_args(255), # standard
    trans_cmd(CT.LIGHT_WCOLOR, 0x21, {"param":1}, {"param1": 0x40}) | multi_args(255), # standard, remote
    trans_cmd(CT.LIGHT_WCOLOR, 0x23, {"param":3, "args[0]":0, "args[1]":"0.1f"}), # night mode
    trans_cmd(CT.LIGHT_CCT, 0x21, {"param":1}, {"param1": 0x24}), # K+
    trans_cmd(CT.LIGHT_CCT, 0x21, {"param":2}, {"param1": 0x18}), # K-
    trans_cmd(CT.LIGHT_DIM, 0x21, {"param":1}, {"param1": 0x14}), # B+
    trans_cmd(CT.LIGHT_DIM, 0x21, {"param":2}, {"param1": 0x28}), # B-
    trans_cmd(CT.FAN_OSC_TOGGLE, 0x33),
]

ZhijiaV0Translator = [
    trans_cmd(CT.PAIR, 0xB4),
    trans_cmd(CT.UNPAIR, 0xB0),
    trans_cmd(CT.TIMER, 0xD4, {"args[0]":60}),
    trans_cmd(CT.TIMER, 0xD5, {"args[0]":120}),
    trans_cmd(CT.TIMER, 0xD6, {"args[0]":240}),
    trans_cmd(CT.TIMER, 0xD7, {"args[0]":480}),
    trans_cmd(CT.LIGHT_ON, 0xB3),
    trans_cmd(CT.LIGHT_OFF, 0xB2),
    trans_cmd(CT.LIGHT_DIM, 0xB5, {"param":0}) | zhijia_v0_multi_args(),
    trans_cmd(CT.LIGHT_CCT, 0xB7, {"param":0}) | zhijia_v0_multi_args(),
    trans_cmd(CT.LIGHT_SEC_ON, 0xA6, {}, {"args[0]":1}),
    trans_cmd(CT.LIGHT_SEC_OFF, 0xA6, {}, {"args[0]":2}),
    trans_cmd(CT.FAN_DIR, 0xD9, {"args[0]":0}),
    trans_cmd(CT.FAN_DIR, 0xDA, {"args[0]":1}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD8, {"args[0]":0, "args[1]": 6}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD2, {"args[0]":2, "args[1]": 6}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD1, {"args[0]":4, "args[1]": 6}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD0, {"args[0]":6, "args[1]": 6}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA1, {"param":0}) | multi_args(255), # not validated
    trans_cmd(CT.LIGHT_WCOLOR, 0xA1, {"param":3 , "args[0]":"0.1f", "args[1]":"0.1f"}, {"args[0]":25, "args[1]":25}), # night mode
    trans_cmd(CT.LIGHT_WCOLOR, 0xA2, {"param":1 , "args[0]":1, "args[1]":0}, {"args[0]":255, "args[1]":0}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA3, {"param":1 , "args[0]":0, "args[1]":1}, {"args[0]":0, "args[1]":255}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA4, {"param":1 , "args[0]":1, "args[1]":1}, {"args[0]":255, "args[1]":255}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA7, {"param":2 , "args[0]":1, "args[1]":0}, {"args[0]":1}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA7, {"param":2 , "args[0]":0, "args[1]":1}, {"args[0]":2}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA7, {"param":2 , "args[0]":1, "args[1]":1}, {"args[0]":3}),
]

ZhijiaV1V2CommonTranslator = [
    trans_cmd(CT.PAIR, 0xA2),
    trans_cmd(CT.UNPAIR, 0xA3),
    trans_cmd(CT.TIMER, 0xD9) | multi_args(1.0/60.0),
    trans_cmd(CT.LIGHT_ON, 0xA5),
    trans_cmd(CT.LIGHT_OFF, 0xA6),
    trans_cmd(CT.LIGHT_DIM, 0xAD, {"param":0}) | multi_args(250),
    trans_cmd(CT.LIGHT_CCT, 0xAE, {"param":0}) | multi_args(250),
    trans_cmd(CT.LIGHT_SEC_ON, 0xAF),
    trans_cmd(CT.LIGHT_SEC_OFF, 0xB0),
    trans_cmd(CT.FAN_DIR, 0xDB, {"args[0]":0}),
    trans_cmd(CT.FAN_DIR, 0xDA, {"args[0]":1}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD7, {"args[0]":0, "args[1]": 6}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD6, {"args[0]":2, "args[1]": 6}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD5, {"args[0]":4, "args[1]": 6}),
    trans_cmd(CT.FAN_ONOFF_SPEED, 0xD4, {"args[0]":6, "args[1]": 6}),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA8, {"param":0}) | multi_args(250),
    trans_cmd(CT.LIGHT_WCOLOR, 0xA7, {"param":3 , "args[0]":"0.1f", "args[1]":"0.1f"}, {"args[0]":25, "args[1]":25}),
]

class TranslatorGenerator():
    translators = {
        str(FanLampEncoderV1): [
            *FanLampCommonTranslators,
            trans_cmd(CT.TIMER, 0x51) | trunc_arg0(),
            trans_cmd(CT.FAN_DIR, 0x15) | multi_arg0(),
            trans_cmd(CT.FAN_OSC, 0x16) | multi_arg0(),
            trans_cmd(CT.FAN_ONOFF_SPEED, 0x31, {"args[1]": 0}) | multi_args(0.5),
            trans_cmd(CT.FAN_ONOFF_SPEED, 0x32, {"args[1]": 6}) | multi_args(),
        ],
        str(FanLampEncoderV2): [ 
            *FanLampCommonTranslators,
            trans_cmd(CT.TIMER, 0x41) | param1_arg0(),
            trans_cmd(CT.FAN_DIR, 0x15, {"args[0]": 0}, {"param1": 0x00}),
            trans_cmd(CT.FAN_DIR, 0x15, {"args[0]": 1}, {"param1": 0x01}),
            trans_cmd(CT.FAN_OSC, 0x16, {"args[0]": 0}, {"param1": 0x00}),
            trans_cmd(CT.FAN_OSC, 0x16, {"args[0]": 1}, {"param1": 0x01}),
            trans_cmd(CT.FAN_ONOFF_SPEED, 0x31, {"args[1]": 0}, {"param1": 0x00}) | multi_arg0(0.5),
            trans_cmd(CT.FAN_ONOFF_SPEED, 0x31, {"args[1]": 6}, {"param1": 0x20}) | multi_arg0(),
        ],
        str(ZhijiaEncoderV0): ZhijiaV0Translator,
        str(ZhijiaEncoderV1): ZhijiaV1V2CommonTranslator,
        str(ZhijiaEncoderV2): ZhijiaV1V2CommonTranslator,
        str(RemoteEncoder): [
            trans_cmd(CT.LIGHT_ON, 0x08),
            trans_cmd(CT.LIGHT_OFF, 0x06),
            trans_cmd(CT.LIGHT_SEC_TOGGLE, 0x13),
            trans_cmd(CT.LIGHT_WCOLOR, 0x10, {"param":3, "args[0]":0, "args[1]":"0.1f"}), # night mode
            trans_cmd(CT.LIGHT_CCT, 0x0A, {"param":1}) | multi_args(), # K+ 
            trans_cmd(CT.LIGHT_CCT, 0x0B, {"param":2}) | multi_args(), # K-
            trans_cmd(CT.LIGHT_DIM, 0x02, {"param":1}) | multi_args(), # B+
            trans_cmd(CT.LIGHT_DIM, 0x03, {"param":2}) | multi_args(), # B-
            trans_cmd(CT.LIGHT_WCOLOR, 0x07, {"param":2}), # CCT / brightness Cycle
        ],
    }
    created = {}

    @classmethod
    def get_translator_instance(cls, name):
        trans_class_name = str(name).split(':')[-1].replace('Encoder', 'Translator')
        if not trans_class_name in cls.created:
            cls.define_class(trans_class_name, cls.translators[str(name)])
            trans_class_type = cg.esphome_ns.class_(trans_class_name)
            inst_name = f"translator_{trans_class_name.lower()}"
            cg.add(cg.RawExpression(f"{trans_class_type} * {inst_name} = new {trans_class_type}()"))
            cls.created[trans_class_name] = cg.RawExpression(inst_name)
        return cls.created[trans_class_name]

    @classmethod 
    def define_class(cls, name, translators):
        cl = f"class {name}: public {CommandTranslator}\n{{"
        cl += f"\npublic:"
        cl += f"\n  void g2e_cmd(const {BleAdvGenCmd} & g, {BleAdvEncCmd} & e) const override {{"
        for conds in translators:
            if_cond = " && ".join([f"(g.{attr} == {val})" for (attr, val) in conds["g"].items()])
            exec_cmd = "".join([f"e.{attr} = {val}; " for (attr, val) in conds["e"].items()])
            if "raw_g2e" in conds:
                exec_cmd += conds["raw_g2e"]
            cl += f"\n    if ({if_cond}) {{ {exec_cmd} }}"
        cl += f"\n  }}" # end of g2e
        cl += f"\n  void e2g_cmd(const {BleAdvEncCmd} & e, {BleAdvGenCmd} & g) const override {{"
        for conds in translators:
            if_cond = " && ".join([f"(e.{attr} == {val})" for (attr, val) in conds["e"].items()])
            exec_cmd = " ".join([f"g.{attr} = {val};" for (attr, val) in conds["g"].items()])
            if "raw_e2g" in conds:
                exec_cmd += conds["raw_e2g"]
            cl += f"\n    if ({if_cond}) {{ {exec_cmd} }}"
        cl += f"\n  }}" # end of e2g
        cl += f"\n}}"
        cg.add(cg.RawExpression(cl))

BLE_ADV_ENCODERS = {
    "fanlamp_pro" :{
        "variants": {
            "v1": {
                "class": FanLampEncoderV1,
                "args": [ 0x83, False ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x19, 0x03 ],
                "header": [0x77, 0xF8],
            },
            "v2": {
                "class": FanLampEncoderV2,
                "args": [ [0x10, 0x80, 0x00], 0x0400, False ],
                "ble_param": [ 0x19, 0x03 ],
                "header": [0xF0, 0x08],
            },
            "v3": {
                "class": FanLampEncoderV2,
                "args": [ [0x20, 0x80, 0x00], 0x0400, True ],
                "ble_param": [ 0x19, 0x03 ],
                "header": [0xF0, 0x08],
            },
        },
        "legacy_variants": {
            "v1a": "please use 'other - v1a' for exact replacement, or 'fanlamp_pro' v1 / v2 / v3 if effectively using FanLamp Pro app",
            "v1b": "please use 'other - v1b' for exact replacement, or 'fanlamp_pro' v1 / v2 / v3 if effectively using FanLamp Pro app",
        },
        "default_variant": "v3",
        "default_forced_id": 0,
    },
    "lampsmart_pro": {
        "variants": {
            "v1": {
                "class": FanLampEncoderV1,
                "args": [ 0x81 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x19, 0x03 ],
                "header": [0x77, 0xF8],
            },
            # v2 is only used by LampSmart Pro - Soft Lighting
            "v2": {
                "class": FanLampEncoderV2,
                "args": [ [0x10, 0x80, 0x00], 0x0100, False ],
                "ble_param": [ 0x19, 0x03 ],
                "header": [0xF0, 0x08],
            },
            "v3": {
                "class": FanLampEncoderV2,
                "args": [ [0x30, 0x80, 0x00], 0x0100, True ],
                "ble_param": [ 0x19, 0x03 ],
                "header": [0xF0, 0x08],
            },
        },
        "legacy_variants": {
            "v1a": "please use 'other - v1a' for exact replacement, or 'lampsmart_pro' v1 / v3 if effectively using LampSmart Pro app",
            "v1b": "please use 'other - v1b' for exact replacement, or 'lampsmart_pro' v1 / v3 if effectively using LampSmart Pro app",
        },
        "default_variant": "v3",
        "default_forced_id": 0,
    },
    "zhijia": {
        "variants": {
            "v0": {
                "class": ZhijiaEncoderV0,
                "args": [ [0x19, 0x01, 0x10] ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v1": {
                "class": ZhijiaEncoderV1,
                "args": [ [0x19, 0x01, 0x10, 0xAA] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v2": {
                "class": ZhijiaEncoderV2,
                "args": [ [0x19, 0x01, 0x10] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0x22, 0x9D ],
            },
        },
        "default_variant": "v2",
        "default_forced_id": 0xC630B8,
    },
    "zhiguang": {
        "variants": {
            "v0": {
                "class": ZhijiaEncoderV0,
                "args": [ [0xAA, 0x55, 0xCC] ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v1": {
                "class": ZhijiaEncoderV1,
                "args": [ [0x20, 0x20, 0x03, 0x05], 1 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v2": {
                "class": ZhijiaEncoderV2,
                "args": [ [0x19, 0x01, 0x10] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0x22, 0x9D ],
            },
        },
        "default_variant": "v2",
        "default_forced_id": 0xC630B8,
    },
    "remote" : {
        "variants": {
            # Not working
            # "v1": {
            #     "class": FanLampEncoderV1, 
            #     "args": [ 0x83, False, True ],
            #     "max_forced_id": 0xFFFFFF,
            #     "ble_param": [ 0x00, 0xFF ],
            #     "header":[0x56, 0x55, 0x18, 0x87, 0x52], 
            # },
            "v3": {
                "class": FanLampEncoderV2,
                "args": [ [0x10, 0x00, 0x56], 0x0400, True ],
                "ble_param": [ 0x02, 0x16 ],
                "header": [0xF0, 0x08],
            },
            "v4": {
                "class": RemoteEncoder,
                "args": [ ],
                "ble_param": [ 0x1A, 0xFF ],
                "header": [0xF0, 0xFF],
                # 02.01.1A.0B.FF.F0.FF.00.55.8F.24.04.08.65.79
            }
        },
        "default_variant": "v3",
        "default_forced_id": 0,
    },
# legacy lampsmart_pro variants v1a / v1b / v2 / v3
# None of them are actually matching what FanLamp Pro / LampSmart Pro apps are generating
# Maybe generated by some remotes, kept here for backward compatibility, with some raw sample
    "other" : {
        "variants": {
            "v1b": {
                "class": FanLampEncoderV1,
                "args": [ 0x81, True, True, 0x55 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x02, 0x16 ],
                "header":  [0xF9, 0x08],
                # 02.01.02.1B.03.F9.08.49.13.F0.69.25.4E.31.51.BA.32.08.0A.24.CB.3B.7C.71.DC.8B.B8.97.08.D0.4C (31)
            },
            "v1a": {
                "class": FanLampEncoderV1, 
                "args": [ 0x81, True, True ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x02, 0x03 ],
                "header": [0x77, 0xF8],
                # 02.01.02.1B.03.77.F8.B6.5F.2B.5E.00.FC.31.51.50.CB.92.08.24.CB.BB.FC.14.C6.9E.B0.E9.EA.73.A4 (31)
            },
            "v2": {
                "class": FanLampEncoderV2,
                "args": [ [0x10, 0x80, 0x00], 0x0100, False ],
                "ble_param": [ 0x19, 0x16 ],
                "header": [0xF0, 0x08],
                # 02.01.02.1B.16.F0.08.10.80.0B.9B.DA.CF.BE.B3.DD.56.3B.E9.1C.FC.27.A9.3A.A5.38.2D.3F.D4.6A.50 (31)
            },
            "v3": {
                "class": FanLampEncoderV2,
                "args": [ [0x10, 0x80, 0x00], 0x0100, True ],
                "ble_param": [ 0x19, 0x16 ],
                "header": [0xF0, 0x08],
                # 02.01.02.1B.16.F0.08.10.80.33.BC.2E.B0.49.EA.58.76.C0.1D.99.5E.9C.D6.B8.0E.6E.14.2B.A5.30.A9 (31)
            },
        },
        "default_variant": "v1b",
        "default_forced_id": 0,
    },
}

def forced_id_mig_msg(forced_id, max_forced_id):
    trunc_id = forced_id & max_forced_id
    return f"If migrating from previous force_id 0x{forced_id:X}, use 0x{trunc_id:X} to avoid the need to re pair"

def validate_ble_adv_device(config):
    encoding = config[CONF_BLE_ADV_ENCODING]
    enc_params = BLE_ADV_ENCODERS[encoding]
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
        cv.Required(CONF_BLE_ADV_ENCODING): cv.one_of(*BLE_ADV_ENCODERS.keys()),
        cv.Optional(CONF_VARIANT): cv.alphanumeric,
        cv.Optional(CONF_BLE_ADV_FORCED_ID): cv.hex_uint32_t,
        cv.Optional(CONF_INDEX, default=0): cv.All(cv.positive_int, cv.Range(min=0, max=255)),
    }
)

async def setup_ble_adv_device(var, config):
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_BLE_ADV_HANDLER_ID])
    await setup_entity(var, config)
    cg.add(var.init(config[CONF_BLE_ADV_ENCODING], config[CONF_VARIANT]))
    cg.add(var.set_index(config[CONF_INDEX]))
    if config[CONF_BLE_ADV_FORCED_ID] > 0:
        cg.add(var.set_forced_id(config[CONF_BLE_ADV_FORCED_ID]))
    else:
        cg.add(var.set_forced_id(config[CONF_ID].id))

CONF_BLE_ADV_SCAN_ACTIVATED = "scan_activated"
CONF_BLE_ADV_CHECK_REENCODING = "check_reencoding"
CONF_BLE_ADV_LOG_RAW = "log_raw"
CONF_BLE_ADV_LOG_COMMAND = "log_command"
CONF_BLE_ADV_LOG_CONFIG = "log_config"

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
    }),
    cv.only_on([PLATFORM_ESP32]),
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_setup_priority(300)) # start after Bluetooth
    for encoding, params in BLE_ADV_ENCODERS.items():
        for variant, param_variant in params["variants"].items():
            if "class" in param_variant:
                enc_id = ID("encoder_%s_%s" % (encoding, variant), type=param_variant["class"])
                enc = cg.new_Pvariable(enc_id, encoding, variant, *param_variant["args"])
                cg.add(enc.set_ble_param(*param_variant["ble_param"]))
                cg.add(enc.set_header(param_variant["header"]))
                cg.add(enc.set_translator(TranslatorGenerator.get_translator_instance(param_variant["class"])))
                cg.add(var.add_encoder(enc))
    await cg.register_component(var, config)
    cg.add(var.set_scan_activated(config[CONF_BLE_ADV_SCAN_ACTIVATED]))
    cg.add(var.set_check_reencoding(config[CONF_BLE_ADV_CHECK_REENCODING]))
    cg.add(var.set_logging(config[CONF_BLE_ADV_LOG_RAW], config[CONF_BLE_ADV_LOG_COMMAND], config[CONF_BLE_ADV_LOG_CONFIG]))
    parent = await cg.get_variable(config[CONF_BLE_ID])
    cg.add(parent.register_gap_event_handler(var))
    cg.add(var.set_parent(parent))
