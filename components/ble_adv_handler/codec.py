import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)
from .translator import (
    BleAdvTranslator,
)

bleadvhandler_ns = cg.esphome_ns.namespace('ble_adv_handler')
BleAdvEncoder = bleadvhandler_ns.class_('BleAdvEncoder')

CONF_BLE_ADV_TRANSLATOR_ID = "translator_id"

BASE_CODEC_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BleAdvEncoder),
    cv.GenerateID(CONF_BLE_ADV_TRANSLATOR_ID): cv.use_id(BleAdvTranslator),
    cv.Required("class"): cv.string,
    cv.Required("header"): cv.ensure_list(cv.hex_uint8_t),
    cv.Optional("max_forced_id", default=0xFFFFFFFF): cv.hex_uint32_t,
    cv.Optional("ble_param", default=[0x19, 0x03]): cv.ensure_list(cv.hex_uint8_t),
    cv.Optional("args", default=[]): cv.ensure_list(cv.valid),
    cv.Optional("debug_mode", default=False): cv.boolean,
})

def load_default_codecs(codecs, codecs_debug_mode):
    for codec in codecs:
        codec["args"] = ["user_defined", codec[CONF_ID].id] + codec["args"]
    for (encoding, data) in BLE_ADV_CODECS.items():
        for (variant, data_var) in data["variants"].items():
            tr_key = data_var.pop("translator")
            data_var["args"] = [encoding, variant] + data_var["args"]
            id = cv.declare_id(BleAdvEncoder)(f"{encoding}_{variant}")
            codecs.append({
                CONF_ID: id,
                CONF_BLE_ADV_TRANSLATOR_ID: cv.use_id(BleAdvTranslator)(tr_key),
                "debug_mode": id in codecs_debug_mode,
                **data_var,
            })
    return codecs

async def codec_to_code(config):
    class_gen = bleadvhandler_ns.class_(config["class"], BleAdvEncoder)
    var = cg.Pvariable(config[CONF_ID], class_gen.new(*config["args"]))
    cg.add(var.set_ble_param(*config["ble_param"]))
    cg.add(var.set_header(config["header"]))
    cg.add(var.set_translator(await cg.get_variable(config[CONF_BLE_ADV_TRANSLATOR_ID])))
    cg.add(var.set_debug_mode(config["debug_mode"]))
    return var

BLE_ADV_CODECS = {
    "fanlamp_pro" :{
        "variants": {
            "v1": {
                "class": 'FanLampEncoderV1',
                "translator": 'default_translator_flv1',
                "args": [ 0x83, False ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x19, 0x03 ],
                "header": [0x77, 0xF8],
            },
            "v2": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
                "args": [ [0x10, 0x80, 0x00], 0x0400, False ],
                "ble_param": [ 0x19, 0x03 ],
                "header": [0xF0, 0x08],
            },
            "v3": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
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
                "class": 'FanLampEncoderV1',
                "translator": 'default_translator_flv1',
                "args": [ 0x81 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x19, 0x03 ],
                "header": [0x77, 0xF8],
            },
            # v2 is only used by LampSmart Pro - Soft Lighting
            "v2": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
                "args": [ [0x10, 0x80, 0x00], 0x0100, False ],
                "ble_param": [ 0x19, 0x03 ],
                "header": [0xF0, 0x08],
            },
            "v3": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
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
                "class": 'ZhijiaEncoderV0',
                "translator": 'default_translator_zjv0',
                "args": [ [0x19, 0x01, 0x10] ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v1": {
                "class": 'ZhijiaEncoderV1',
                "translator": 'default_translator_zjv1',
                "args": [ [0x19, 0x01, 0x10, 0xAA] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v2": {
                "class": 'ZhijiaEncoderV2',
                "translator": 'default_translator_zjv2',
                "args": [ [0x19, 0x01, 0x10] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0x22, 0x9D ],
            },
            "v2_fl": { # to resolve flickering issues
                "class": 'ZhijiaEncoderV2',
                "translator": 'default_translator_zjv2fl',
                "args": [ [0x19, 0x01, 0x10] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0x22, 0x9D ],
            },
            "vr1": {
                "class": 'ZhijiaEncoderRemote',
                "translator": 'default_translator_zjvr1',
                "args": [ [0x20, 0x03, 0x05] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF0, 0xFF ],
            },
        },
        "default_variant": "v2",
        "default_forced_id": 0xC630B8,
    },
    "zhiguang": {
        "variants": {
            "v0": {
                "class": 'ZhijiaEncoderV0',
                "translator": 'default_translator_zjv0',
                "args": [ [0xAA, 0x55, 0xCC] ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v1": {
                "class": 'ZhijiaEncoderV1',
                "translator": 'default_translator_zjv1',
                "args": [ [0x20, 0x20, 0x03, 0x05], 1 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
            "v2": {
                "class": 'ZhijiaEncoderV2',
                "translator": 'default_translator_zjv2',
                "args": [ [0x20, 0x03, 0x05] ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0x22, 0x9D ],
            },
        },
        "default_variant": "v2",
        "default_forced_id": 0xC630B8,
    },
    "zhimei": {
        "variants": {
            "v0": {
                "class": 'ZhimeiEncoderV0',
                "translator": 'default_translator_zmv0',
                "args": [  ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x19, 0x03 ],
                "header": [ 0x55 ],
            },
            "v1": {
                "class": 'ZhimeiEncoderV1',
                "translator": 'default_translator_zmv1',
                "args": [  ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0x03 ],
                "header": [ 0x48, 0x46, 0x4B, 0x4A ],
            },
            "v1b": {
                "class": 'ZhimeiEncoderV1',
                "translator": 'default_translator_zmv1',
                "args": [  ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0xFF ],
                "header": [ 0x58, 0x55, 0x18, 0x48, 0x46, 0x4B, 0x4A ],
            },
            "v2": {
                "class": 'ZhimeiEncoderV2',
                "translator": 'default_translator_zmv2',
                "args": [  ],
                "max_forced_id": 0xFFFF,
                "ble_param": [ 0x1A, 0x03 ],
                "header": [ 0xF9, 0x08, 0x49 ],
            },
        },
        "default_variant": "v1",
        "default_forced_id": 0,
    },
    "agarce": {
        "variants": {
            "v3": {
                "class": 'AgarceEncoder',
                "translator": 'default_translator_agv3',
                "args": [ 0x83 ],
                "ble_param": [ 0x19, 0xFF ],
                "header": [ 0xF9, 0x09 ],
            },
            "v4": {
                "class": 'AgarceEncoder',
                "translator": 'default_translator_agv3',
                "args": [ 0x84 ],
                "ble_param": [ 0x19, 0xFF ],
                "header": [ 0xF9, 0x09 ],
            },
        },
        "default_variant": "v4",
        "default_forced_id": 0,
    },
    "remote" : {
        "variants": {
            "v1": {
                "class": 'FanLampEncoderV1',
                "translator": 'default_translator_flv1',
                "args": [ 0x83, False, True, 0x00, 0x7293 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x00, 0xFF ],
                "header":[0x56, 0x55, 0x18, 0x87, 0x52],
                # 1E.FF.56.55.18.87.52.B6.5F.2B.5E.00.FC.31.51.50.50.9A.08.24.0A.EC.FC.A9.7B.8E.0D.4A.67.60.57
            },
            "v3": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
                "args": [ [0x10, 0x00, 0x56], 0x0400, True ],
                "ble_param": [ 0x02, 0x16 ],
                "header": [0xF0, 0x08],
            },
            "v31": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
                "args": [ [0x10, 0x00, 0x56], 0x0100, True ],
                "ble_param": [ 0x02, 0x16 ],
                "header": [0xF0, 0x08],
            },
            "v4": {
                "class": 'RemoteEncoder',
                "translator": 'default_translator_remote',
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
                "class": 'FanLampEncoderV1',
                "translator": 'default_translator_flv1',
                "args": [ 0x81, True, True, 0x55 ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x02, 0x16 ],
                "header":  [0xF9, 0x08],
                # 02.01.02.1B.03.F9.08.49.13.F0.69.25.4E.31.51.BA.32.08.0A.24.CB.3B.7C.71.DC.8B.B8.97.08.D0.4C (31)
            },
            "v1a": {
                "class": 'FanLampEncoderV1',
                "translator": 'default_translator_flv1',
                "args": [ 0x81, True, True ],
                "max_forced_id": 0xFFFFFF,
                "ble_param": [ 0x02, 0x03 ],
                "header": [0x77, 0xF8],
                # 02.01.02.1B.03.77.F8.B6.5F.2B.5E.00.FC.31.51.50.CB.92.08.24.CB.BB.FC.14.C6.9E.B0.E9.EA.73.A4 (31)
            },
            "v2": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
                "args": [ [0x10, 0x80, 0x00], 0x0100, False ],
                "ble_param": [ 0x19, 0x16 ],
                "header": [0xF0, 0x08],
                # 02.01.02.1B.16.F0.08.10.80.0B.9B.DA.CF.BE.B3.DD.56.3B.E9.1C.FC.27.A9.3A.A5.38.2D.3F.D4.6A.50 (31)
            },
            "v3": {
                "class": 'FanLampEncoderV2',
                "translator": 'default_translator_flv2',
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
