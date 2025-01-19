import logging
from functools import partialmethod
import esphome.codegen as cg
import esphome.config_validation as cv
import os
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
)

bleadvhandler_ns = cg.esphome_ns.namespace('ble_adv_handler')
# Crappy way to force the relevant prefix at C++ code generation time
ET = cg.esphome_ns.namespace('EntityType').enum('')
CT = cg.esphome_ns.namespace('CommandType').enum('')
FST = cg.esphome_ns.namespace('FanSubCmdType').enum('')
BleAdvTranslator = bleadvhandler_ns.class_('BleAdvTranslator')

class CmdParam:
    def __init__(self, conf_name, cpp_name, parent):
        self._conf_name = conf_name
        self._cpp_name = cpp_name
        self._eq = None
        self._range = None

    def __repr__(self):
        pres = ""
        if self._eq is not None:
            pres += f", {self._conf_name}: {self._eq}"
        elif self._range is not None:
            pres += f", {self._conf_name} range: [{self._range[0]}, {self._range[1]}]"
        return pres
    
    def validate(self):
        if self._eq is not None and self._range is not None:
            return f"{self._conf_name} and {self._conf_name}_range are exclusive"
        if self._range is not None:
            if self._range[0] is not None and self._range[1] is not None:
                if self._range[0] > self._range[1]:
                    return f"{self._conf_name}_range is invalid"
        return None

    def intersect_val_or_range(self, comp):
        if ((self._eq is None) and (self._range is None)) or ((comp._eq is None) and (comp._range is None)):
            return True
        a,b = self._range if self._eq is None else (self._eq, self._eq)
        c,d = comp._range if comp._eq is None else (comp._eq, comp._eq)

        # checking if [a,b] intersects [c,d], knowing a and b are not both None, neither c and d 
        if ((a is None) and (c is None)) or ((b is None) and (d is None)): return True
        if (a is None): return (b >= c) # b and c cannot be None
        if (b is None): return (a <= d) # a and d cannot be None            
        if (c is None): return (d >= a) # d and a cannot be None            
        if (d is None): return (c <= b) # c and b cannot be None
        return ((a >= c) and (a <= d)) or ((b >= c) and (b <= d)) # a,b,c,d not None

    def rewrite_float(self, val):
        return f"{val}f" if isinstance(val, float) else f"{val}"

    def get_cpp_cond(self, name):
        cond = ""
        if self._eq is not None:
            cond += f" && ({name}.{self._cpp_name} == {self.rewrite_float(self._eq)})"
        elif self._range is not None:
            if self._range[0] is not None:
                cond += f" && ({name}.{self._cpp_name} >= {self.rewrite_float(self._range[0])})"
            if self._range[1] is not None:
                cond += f" && ({name}.{self._cpp_name} <= {self.rewrite_float(self._range[1])})"
        return cond

    def get_cpp_exec(self, name):
        statement = ""
        if self._eq is not None:
            statement += f"{name}.{self._cpp_name} = {self.rewrite_float(self._eq)}; "
        return statement

class CmdBase:
    ## Represents the Conditions on a BleAdvGenCmd / BleAdvEncCmd param and args
    def __init__(self, attribs):
        self._attribs = {}
        for cmd_param in attribs:
            self._attribs[cmd_param._conf_name] = cmd_param
        
    def __repr__(self):
        pres = ""
        for cmd_param in self._attribs.values():
            pres += repr(cmd_param)
        return pres

    def set_eq(self, val, param):
        self._attribs[param]._eq = val
        return self

    def set_range(self, start, end, param):
        self._attribs[param]._range = (start, end)
        return self

    def validate(self):
        for cmd_param in self._attribs.values():
            if msg := cmd_param.validate():
                return msg
        return None
    
    def intersects(self, comp) -> bool:
        for name, cmd_param in self._attribs.items():
            if not cmd_param.intersect_val_or_range(comp._attribs[name]):
                return False
        return True
    
    def get_cpp_cond(self, name):
        cond = ""
        for cmd_param in self._attribs.values():
            cond += cmd_param.get_cpp_cond(name)
        return cond
    
    def get_cpp_exec(self, name):
        statement = ""
        for cmd_param in self._attribs.values():
            statement += cmd_param.get_cpp_exec(name)
        return statement
    
    def define_shortcuts(class_vars, params):
        for param in params:
            class_vars[f"{param}"] = partialmethod(CmdBase.set_eq, param=param)
            class_vars[f"{param}_range"] = partialmethod(CmdBase.set_range, param=param)


ENC_CMD_ATTRIBS = [
    ["param", "param1", cv.uint8_t],
    ["arg0", "args[0]", cv.uint8_t],
    ["arg1", "args[1]", cv.uint8_t],
    ["arg2", "args[2]", cv.uint8_t],
]

class EncCmd(CmdBase):
    ## Represents a Condition on a BleAdvEncCmd
    def __init__(self, cmd: int):
        super().__init__([ CmdParam(x[0], x[1], self) for x in ENC_CMD_ATTRIBS ])
        self._cmd = ("0x%02X" % cmd)

    def __repr__(self):
        return f"cmd: {self._cmd}" + super().__repr__()

    def intersects(self, comp) -> bool:
        if self._cmd != comp._cmd:
            return False
        return super().intersects(comp)

    def get_cpp_cond(self):
        return f"(e.cmd == {self._cmd})" + super().get_cpp_cond("e")

    def get_cpp_exec(self):
        return f"e.cmd = {self._cmd}; " + super().get_cpp_exec("e")

    # shortcut functions arg0 / param / arg1_range / ...
    CmdBase.define_shortcuts(vars(), [x[0] for x in ENC_CMD_ATTRIBS])

GEN_CMD_ATTRIBS = [
    ["param", "param", cv.uint8_t],
    ["arg0", "args[0]", cv.float_range()],
    ["arg1", "args[1]", cv.float_range()],
    ["arg2", "args[2]", cv.float_range()],
]

class GenCmd(CmdBase):
    ## Represents a Condition on a BleAdvGenCmd
    def __init__(self, cmd: str, entity: str, index: int = 0):
        super().__init__([ CmdParam(x[0], x[1], self) for x in GEN_CMD_ATTRIBS ])
        self._cmd = str(cmd)
        self._ent = str(entity)
        self._ind = index

    def __repr__(self):
        return f"cmd: {self._cmd}, type: {self._ent}, index: {self._ind}" + super().__repr__()

    def intersects(self, comp) -> bool:
        if (self._cmd != comp._cmd) or (self._ent != comp._ent) or (self._ind != comp._ind):
            return False
        return super().intersects(comp)

    def get_cpp_cond(self):
        return f"(g.cmd == {self._cmd}) && (g.ent_type == {self._ent}) && (g.ent_index == {self._ind})" + super().get_cpp_cond("g")

    def get_cpp_exec(self):
        return f"g.cmd = {self._cmd}; g.ent_type = {self._ent}; g.ent_index = {self._ind}; " + super().get_cpp_exec("g")

    # shortcut functions arg0 / param / arg1_range / ...
    CmdBase.define_shortcuts(vars(), [x[0] for x in GEN_CMD_ATTRIBS])


class FanCmd(GenCmd):
    def __init__(self, cmd: str, index: int = 0):
        super().__init__(cmd, ET.FAN, index)

class LightCmd(GenCmd):
    def __init__(self, cmd: str, index: int = 0):
        super().__init__(cmd, ET.LIGHT, index)

class ContCmd(GenCmd):
    def __init__(self, cmd: str, index: int = 0):
        super().__init__(cmd, ET.CONTROLLER, index)

class AllCmd(GenCmd):
    def __init__(self, cmd: str):
        super().__init__(cmd, ET.ALL)


class Trans:
    ## Base Translator piece
    def __init__(self, gen: GenCmd, enc: EncCmd):
        self._gen: GenCmd = gen
        self._enc: EncCmd = enc
        self._raw_e2g = ""
        self._raw_g2e = ""

    def __repr__(self):
        return f"Gen: {self._gen} <=> Enc: {self._enc}"

    def field_copy(self, g_param, e_param):
        self._raw_g2e += f"e.{e_param} = g.{g_param}; "
        self._raw_e2g += f"g.{g_param} = e.{e_param}; "
        return self

    def field_multiply(self, multi, g_param, e_param):
        self._raw_g2e += f"e.{e_param} = (float){multi} * g.{g_param}; "
        self._raw_e2g += f"g.{g_param} = ((float)e.{e_param}) / (float){multi}; "
        return self
        
    def zhijia_v0_multi_args(self, reversed: bool = False):
        rev_state = "1.0f - " if reversed else ""
        self._raw_g2e += f"uint16_t arg16 = 1000*({rev_state}g.args[0]); e.args[1] = (arg16 & 0xFF00) >> 8; e.args[2] = arg16 & 0x00FF; "
        self._raw_e2g += f"g.args[0] = {rev_state}((float)((((uint16_t)e.args[1]) << 8) | e.args[2]) / 1000.f); "
        return self

    def custom_exec(self, raw_g2e, raw_e2g):
        self._raw_g2e += raw_g2e
        self._raw_e2g += raw_e2g
        return self

    def get_cpp_g2e(self):
        return f"if ({self._gen.get_cpp_cond()}) {{ {self._enc.get_cpp_exec()}{self._raw_g2e}}}"
        
    def get_cpp_e2g(self):
        return f"if ({self._enc.get_cpp_cond()}) {{ {self._gen.get_cpp_exec()}{self._raw_e2g}}}"

    # shortcut 'copy' and 'multi' functions for each combination of args and param
    # copy_arg0 / multi_arg0_to_arg2 / multi_param / copy_param_to_arg1 / ...
    for g_attr in GEN_CMD_ATTRIBS:
        for e_attr in ENC_CMD_ATTRIBS:
            sec_arg = f"_to_{e_attr[0]}" if (e_attr[0] != g_attr[0]) else ""
            vars()[f"copy_{g_attr[0]}{sec_arg}"] = partialmethod(field_copy, g_param=g_attr[1], e_param=e_attr[1])
            vars()[f"multi_{g_attr[0]}{sec_arg}"] = partialmethod(field_multiply, g_param=g_attr[1], e_param=e_attr[1])


class FullTranslator:
    REGISTERED_TRANSLATORS = {}
    EXLUSIVE_CMD_PAIRS = []

    @classmethod
    def Get(cls, id):
        return cls.REGISTERED_TRANSLATORS[id]

    @classmethod
    def Add_exclusive(cls, reason, cmd_ref: GenCmd, cmd_cmp: GenCmd):
        cls.EXLUSIVE_CMD_PAIRS.append((f"{reason} are mutually exclusive", cmd_ref, cmd_cmp))
    
    def __init__(self, id, extend, cmds):
        FullTranslator.REGISTERED_TRANSLATORS[id] = self
        self._id = id
        self._cmds = cmds
        self._extend = extend
    
    def get_cmds(self, level = 0):
        if level > 10:
            raise cv.Invalid("Translator extend depth > 10, please check for reference loop.")
        return self._cmds if not self._extend else (self.Get(self._extend).get_cmds(level + 1) + self._cmds)

    def check_duplicate(self, cmd_ref, cmd_cmp):
        if cmd_ref.intersects(cmd_cmp):
            raise cv.Invalid(f"Translator ID '{self._id}': Intersecting Commands \n {cmd_ref}\n    and \n {cmd_cmp}\n")

    def check_exclusive(self, cmd_ref, cmd_cmp):
        for reason, pair1, pair2 in self.EXLUSIVE_CMD_PAIRS:
            if (cmd_ref.intersects(pair1) and cmd_cmp.intersects(pair2)) or (cmd_ref.intersects(pair2) and cmd_cmp.intersects(pair1)):
                raise cv.Invalid(f"Translator ID '{self._id}': Incompatible Commands ({reason}) \n {cmd_ref}\n    and \n {cmd_cmp}\n")
        return None

    def check_consistency(self):
        cmds = self.get_cmds()
        checked_cmds = []
        for cmd in cmds:
            err = cmd._gen.validate()
            if err is not None:
                raise cv.Invalid(f"Translator ID '{self._id}': Invalid gen Command({err}) \n {cmd._gen}\n")
            err = cmd._enc.validate()
            if err is not None:
                raise cv.Invalid(f"Translator ID '{self._id}': Invalid enc Command({err}) \n {cmd._enc}\n")
            for prev_cmd in checked_cmds:
                self.check_duplicate(cmd._enc, prev_cmd._enc)
                self.check_duplicate(cmd._gen, prev_cmd._gen)
                self.check_exclusive(cmd._gen, prev_cmd._gen)
            checked_cmds.append(cmd)
            
    def get_class_name(self):
        return f"BleAdvTranslator_{self._id}"

    def get_cpp_class(self):
        cmds = self.get_cmds()
        cl = f"\nclass {self.get_class_name()}: public BleAdvTranslator\n{{"
        cl += f"\npublic:"
        cl += f"\n  void g2e_cmd(const BleAdvGenCmd & g, BleAdvEncCmd & e) const override {{"
        for conds in cmds:
            cl += f"\n    {conds.get_cpp_g2e()}"
        cl += f"\n  }}" # end of g2e
        cl += f"\n  void e2g_cmd(const BleAdvEncCmd & e, BleAdvGenCmd & g) const override {{"
        for conds in cmds:
            cl += f"\n    {conds.get_cpp_e2g()}"
        cl += f"\n  }}" # end of e2g
        cl += f"\n}};\n"
        return cl

def define_all_translators(translators):
    # write the translator classes in "generated_translators.h"
    with open(os.path.join(os.path.dirname(__file__), 'generated_translators.h'), 'w') as gen_file:
        gen_file.write('// Generated Translators - GENERATED FILE: DO NOT EDIT NOR COMMIT')
        gen_file.write('\n#include "ble_adv_handler.h"')
        gen_file.write('\nnamespace esphome {')
        gen_file.write('\nnamespace ble_adv_handler {\n')
        for config in translators:
            gen_file.write(FullTranslator.Get(config[CONF_ID].id).get_cpp_class())
        gen_file.write('\n} // namespace ble_adv_handler')
        gen_file.write('\n} // namespace esphome')
        gen_file.write('\n')

def validate_gen_cmd(value):
    return str(getattr(CT, value))

def validate_gen_type(value):
    return str(getattr(ET, value))

def validate_modifier(value):
    if not hasattr(Trans, value):
        raise cv.Invalid(f"Modifier function '{value}' invalid")
    return value

BASE_TRANSLATOR_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BleAdvTranslator),
    cv.Optional("extend"): cv.use_id(BleAdvTranslator),
    cv.Optional("cmds", default=[]): cv.ensure_list(cv.Schema({
        cv.Required("gen"): cv.Schema({
            cv.Required("cmd"): validate_gen_cmd,
            cv.Required("type"): validate_gen_type,
            cv.Optional("index", default=0): cv.uint8_t,
            **{cv.Optional(f"{x[0]}", default=None): cv.Any(None, x[2]) for x in GEN_CMD_ATTRIBS},
            **{cv.Optional(f"{x[0]}_range", default=None): cv.Any(None, cv.Schema({
                cv.Optional("min", default=None): cv.Any(None, x[2]),
                cv.Optional("max", default=None): cv.Any(None, x[2]),
            })) for x in GEN_CMD_ATTRIBS},
        }),
        cv.Required("enc"): cv.Schema({
            cv.Required("cmd"): cv.uint8_t,
            **{cv.Optional(f"{x[0]}", default=None): cv.Any(None, x[2]) for x in ENC_CMD_ATTRIBS},
            **{cv.Optional(f"{x[0]}_range", default=None): cv.Any(None, cv.Schema({
                cv.Optional("min", default=None): cv.Any(None, x[2]),
                cv.Optional("max", default=None): cv.Any(None, x[2]),
            })) for x in ENC_CMD_ATTRIBS},
        }),
        cv.Optional("modifiers", default=[]): cv.ensure_list(cv.Schema({
            cv.Required("function"): validate_modifier,
            cv.Optional("args", default=[]): cv.ensure_list(cv.string),
        })),
    })),
})

def cmd_from_enc_config(cmd_conf):
    cmd = EncCmd(cmd_conf['cmd'])
    for params in ENC_CMD_ATTRIBS:
        getattr(cmd, params[0])(cmd_conf[params[0]])
        range_param = f"{params[0]}_range"
        if (range_val := cmd_conf[range_param]) is not None:
            getattr(cmd, range_param)(range_val['min'], range_val['max'])
    return cmd

def cmd_from_gen_config(cmd_conf):
    cmd = GenCmd(cmd_conf['cmd'], cmd_conf['type'], cmd_conf['index'])
    for params in GEN_CMD_ATTRIBS:
        getattr(cmd, params[0])(cmd_conf[params[0]])
        range_param = f"{params[0]}_range"
        if (range_val := cmd_conf[range_param]) is not None:
            getattr(cmd, range_param)(range_val['min'], range_val['max'])
    return cmd

def load_default_translators(translators):
    # Complete the map of translators with user defined configs, for Class Generation
    for config in translators:
        extend_id = config["extend"].id if "extend" in config else None
        cmds = []
        for x in config["cmds"]:
            cmd_tr = Trans(cmd_from_gen_config(x['gen']), cmd_from_enc_config(x['enc']))
            for modif in x["modifiers"]:
                getattr(cmd_tr, modif["function"])(*modif["args"])
            cmds.append(cmd_tr)
        FullTranslator(config[CONF_ID].id, extend_id, cmds)

    # Generate Stub config with only ID for Default Translators, for use as reference in config
    for translator in BLE_ADV_DEFAULT_TRANSLATORS:
        translators.append({ 
            CONF_ID: cv.declare_id(BleAdvTranslator)(translator._id),
        })
    
    # Check consistency of commands
    for config in translators:
        FullTranslator.Get(config[CONF_ID].id).check_consistency()

    return translators

async def translator_to_code(config):
    class_gen = bleadvhandler_ns.class_(FullTranslator.Get(config[CONF_ID].id).get_class_name())
    return cg.Pvariable(config[CONF_ID], class_gen.new())

### Mutual Exclusions as can be triggered simultaneously by the Entities in the software ###
# The translator must then define ONLY one of them for a same Entity
# Consider entity index can go up to 3, even if only 0 and 1 are effectivelly used for Light, and only 0 for Fan
for i in range(3):
    # Fan
    FullTranslator.Add_exclusive("Fan - Full / Speed", FanCmd(CT.FAN_ONOFF_SPEED, i).param(0), FanCmd(CT.FAN_FULL, i))
    FullTranslator.Add_exclusive("Fan - Full / Direction", FanCmd(CT.FAN_DIR, i).param(0), FanCmd(CT.FAN_FULL, i))
    FullTranslator.Add_exclusive("Fan - Full / Oscillation", FanCmd(CT.FAN_OSC, i).param(0), FanCmd(CT.FAN_FULL, i))

    # CWW Light
    FullTranslator.Add_exclusive("Light CWW - Brightness / Cold and Warm", LightCmd(CT.LIGHT_CWW_DIM, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_WARM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Color Temperature / Cold and Warm", LightCmd(CT.LIGHT_CWW_CCT, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_WARM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Brightness / Full BR and CT", LightCmd(CT.LIGHT_CWW_DIM, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_DIM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Color Temperature / Full BR and CT", LightCmd(CT.LIGHT_CWW_CCT, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_DIM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Full BR and CT / Cold and Warm", LightCmd(CT.LIGHT_CWW_COLD_DIM, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_WARM, i).param(0))

    # RGB Light
    FullTranslator.Add_exclusive("Light RGB - Brightness / Full RGB", LightCmd(CT.LIGHT_RGB_DIM, i).param(0), LightCmd(CT.LIGHT_RGB_FULL, i).param(0))
    FullTranslator.Add_exclusive("Light RGB - RGB / Full RGB", LightCmd(CT.LIGHT_RGB_RGB, i).param(0), LightCmd(CT.LIGHT_RGB_FULL, i).param(0))

########################################
###  DEFAULT TRANSLATORS DEFINITION  ###
########################################

BLE_ADV_DEFAULT_TRANSLATORS = [
    FullTranslator('default_translator_fanlamp_common', None, [
        Trans(ContCmd(CT.PAIR), EncCmd(0x28)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0x45)),
        Trans(AllCmd(CT.OFF), EncCmd(0x6F)),
        Trans(LightCmd(CT.TOGGLE), EncCmd(0x09)),
        Trans(LightCmd(CT.ON), EncCmd(0x10)),
        Trans(LightCmd(CT.OFF), EncCmd(0x11)),
        Trans(LightCmd(CT.ON, 1), EncCmd(0x12)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0x13)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(0), EncCmd(0x21).param(0x00)).multi_arg0(255).multi_arg1(255), # standard
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(1), EncCmd(0x21).param(0x40)).multi_arg0(255).multi_arg1(255), # standard, remote
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(3).arg0(0).arg1(0.1), EncCmd(0x23)), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(1), EncCmd(0x21).param(0x24)), # K+
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(2), EncCmd(0x21).param(0x18)), # K-
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(1), EncCmd(0x21).param(0x14)), # B+
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(2), EncCmd(0x21).param(0x28)), # B-
        Trans(FanCmd(CT.FAN_OSC_TOGGLE), EncCmd(0x33)),
    ]),

    FullTranslator('default_translator_flv1', 'default_translator_fanlamp_common', [
        Trans(ContCmd(CT.TIMER).arg0_range(None,0xFE), EncCmd(0x51).arg0_range(None,0xFE)).copy_arg0(),
        Trans(ContCmd(CT.TIMER).arg0_range(0xFF,None), EncCmd(0x51).arg0(0xFF)), # No'reverse' way possible...
        Trans(FanCmd(CT.FAN_DIR), EncCmd(0x15)).copy_arg0(),
        Trans(FanCmd(CT.FAN_OSC), EncCmd(0x16)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(0), EncCmd(0x31)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(6), EncCmd(0x32).arg1(6)).copy_arg0(),
    ]),

    FullTranslator('default_translator_flv2', 'default_translator_fanlamp_common', [ 
        Trans(ContCmd(CT.TIMER), EncCmd(0x41)).custom_exec(f"e.param1 = (int)g.args[0] & 0xFF; e.args[0] = (int)g.args[0] >> 8; ",
                                                           f"g.args[0] = e.param1 + e.args[0] * 256; "),
        Trans(LightCmd(CT.LIGHT_RGB_FULL), EncCmd(0x22)).multi_arg0_to_param(255).multi_arg1_to_arg0(255).multi_arg2_to_arg1(255),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0x15).param(0x00)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0x15).param(0x01)),
        Trans(FanCmd(CT.FAN_OSC).arg0(0), EncCmd(0x16).param(0x00)),
        Trans(FanCmd(CT.FAN_OSC).arg0(1), EncCmd(0x16).param(0x01)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(0), EncCmd(0x31).param(0x00)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(6), EncCmd(0x31).param(0x20)).copy_arg0(),
    ]),

    FullTranslator('default_translator_zjv0', None, [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xB0)),
        Trans(ContCmd(CT.TIMER).arg0(60), EncCmd(0xD4)),
        Trans(ContCmd(CT.TIMER).arg0(120), EncCmd(0xD5)),
        Trans(ContCmd(CT.TIMER).arg0(240), EncCmd(0xD6)),
        Trans(ContCmd(CT.TIMER).arg0(480), EncCmd(0xD7)),
        Trans(LightCmd(CT.ON), EncCmd(0xB3)),
        Trans(LightCmd(CT.OFF), EncCmd(0xB2)),
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(0), EncCmd(0xB5)).zhijia_v0_multi_args(),
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(0), EncCmd(0xB7)).zhijia_v0_multi_args(),
        Trans(LightCmd(CT.ON, 1), EncCmd(0xA6).arg0(1)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0xA6).arg0(2)),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0xD9)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0xDA)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(0).arg1(6), EncCmd(0xD8)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(2).arg1(6), EncCmd(0xD2)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(4).arg1(6), EncCmd(0xD1)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(6).arg1(6), EncCmd(0xD0)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(3).arg0(0.1).arg1(0.1), EncCmd(0xA1).arg0(25).arg1(25)), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(1).arg0(1).arg1(0), EncCmd(0xA2).arg0(255).arg1(0)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(1).arg0(0).arg1(1), EncCmd(0xA3).arg0(0).arg1(255)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(1).arg0(1).arg1(1), EncCmd(0xA4).arg0(255).arg1(255)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2).arg0(1).arg1(0), EncCmd(0xA7).arg0(1)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2).arg0(0).arg1(1), EncCmd(0xA7).arg0(2)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2).arg0(1).arg1(1), EncCmd(0xA7).arg0(3)),
    ]),

    FullTranslator('default_translator_zjv1v2_common', None, [
        Trans(ContCmd(CT.PAIR), EncCmd(0xA2)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xA3)),
        Trans(ContCmd(CT.TIMER), EncCmd(0xD9)).multi_arg0(1.0/60.0),
        Trans(LightCmd(CT.ON), EncCmd(0xA5)),
        Trans(LightCmd(CT.OFF), EncCmd(0xA6)),
        Trans(LightCmd(CT.ON, 1), EncCmd(0xAF)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0xB0)),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0xDB)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0xDA)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(0).arg1(6), EncCmd(0xD7)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(2).arg1(6), EncCmd(0xD6)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(4).arg1(6), EncCmd(0xD5)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(6).arg1(6), EncCmd(0xD4)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(3).arg0(0.1).arg1(0.1), EncCmd(0xA7).arg0(25).arg1(25)), # night mode
    ]),

    FullTranslator('default_translator_zjv1', 'default_translator_zjv1v2_common', [
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(0), EncCmd(0xAD)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(0), EncCmd(0xAE)).multi_arg0(250),
    ]),

    FullTranslator('default_translator_zjv2fl', 'default_translator_zjv1v2_common', [
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(3), EncCmd(0xAD)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(3), EncCmd(0xAE)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(0), EncCmd(0xA8)).multi_arg0(250).multi_arg1(250),
        Trans(LightCmd(CT.LIGHT_RGB_DIM), EncCmd(0xC8)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_RGB_RGB), EncCmd(0xCA)).multi_arg0(255).multi_arg1(255).multi_arg2(255),
    ]),

    FullTranslator('default_translator_zjv2', 'default_translator_zjv1v2_common', [
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(0), EncCmd(0xAD)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(0), EncCmd(0xAE)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_RGB_DIM), EncCmd(0xC8)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_RGB_RGB), EncCmd(0xCA)).multi_arg0(255).multi_arg1(255).multi_arg2(255),
    ]),

    FullTranslator('default_translator_zjvr1', None, [
        Trans(ContCmd(CT.PAIR), EncCmd(0xA2)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xA3)),
        Trans(LightCmd(CT.ON), EncCmd(0xA5)),
        Trans(LightCmd(CT.OFF), EncCmd(0xA6)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(0), EncCmd(0xA8)).multi_arg0(250).multi_arg1(250),
        #  Missing: AF / A7 / A9 / AC / AB / AA 
    ]),

    FullTranslator('default_translator_remote', None, [
        Trans(LightCmd(CT.ON), EncCmd(0x08)),
        Trans(LightCmd(CT.OFF), EncCmd(0x06)),
        Trans(LightCmd(CT.TOGGLE, 1), EncCmd(0x13)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(3).arg0(0).arg1(0.1), EncCmd(0x10)), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(1), EncCmd(0x0A)).copy_arg0(), # K+ 
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(2), EncCmd(0x0B)).copy_arg0(), # K-
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(1), EncCmd(0x02)).copy_arg0(), # B+
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(2), EncCmd(0x03)).copy_arg0(), # B-
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2), EncCmd(0x07)), # CCT / brightness Cycle
    ]),

    FullTranslator('default_translator_agv3', None, [
        Trans(ContCmd(CT.PAIR), EncCmd(0x00).arg0(1)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0x00).arg0(0)),
        Trans(AllCmd(CT.OFF), EncCmd(0x70).arg0_range(None,1)),
        Trans(AllCmd(CT.ON), EncCmd(0x70).arg0_range(2,None)),
        Trans(LightCmd(CT.ON), EncCmd(0x10).arg0(1)),
        Trans(LightCmd(CT.OFF), EncCmd(0x10).arg0(0)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_DIM), EncCmd(0x20)).multi_arg0(100).multi_arg1(100),
        # Holy CRAP !!!!
        Trans(FanCmd(CT.FAN_FULL), EncCmd(0x80))
            .custom_exec(f"e.args[0] = ((g.args[0] > 0) ? 0x80 : 0x00) | ((int) g.args[0]) | (g.args[1] ? 0x10 : 0x00); e.args[1] = g.args[2]; ",
                         f"g.args[0] = (e.args[0] & 0x80) ? e.args[0] & 0x0F : 0; g.args[1] = (e.args[0] & 0x10) > 0; g.args[2] = e.args[1]; ")
            .custom_exec(f"e.args[2] = ((g.param & {FST.SPEED} || (g.args[0] > 0)) ? 0x01:0 ) | (g.param & {FST.DIR} ? 0x02:0 | (g.param & {FST.STATE} ? 0x08:0) | (g.param & {FST.OSC} ? 0x10:0)); ",
                         f"g.param = ((e.args[2] & 0x01) ? {FST.SPEED}:0) | (e.args[2] & 0x02 ? {FST.DIR}:0) | (e.args[2] & 0x08 ? {FST.STATE}:0) | (e.args[2] & 0x10 ? {FST.OSC}:0); "),
    ]),

    FullTranslator('default_translator_zhimei_common', None, [
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xB0)),
        Trans(ContCmd(CT.TIMER), EncCmd(0xA5)).custom_exec(f"e.args[0] = (uint8_t)((int) g.args[0] / 60); e.args[1] = (int)g.args[0] % 60; ",
                                                           f"g.args[0] = e.args[0] * 60 + e.args[1]; "),
        Trans(LightCmd(CT.ON), EncCmd(0xB3)),
        Trans(LightCmd(CT.OFF), EncCmd(0xB2)),
        Trans(LightCmd(CT.ON, 1), EncCmd(0xA6).arg0(2)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0xA6).arg0(1)),
        Trans(LightCmd(CT.LIGHT_RGB_FULL, 1), EncCmd(0xCA)).multi_arg0(255).multi_arg1(255).multi_arg2(255),
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(0), EncCmd(0xB5)).zhijia_v0_multi_args(),
        Trans(LightCmd(CT.LIGHT_CWW_CCT).param(0), EncCmd(0xB7)).zhijia_v0_multi_args(True),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0xD9)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0xDA)),
        Trans(FanCmd(CT.FAN_OSC).arg0(0), EncCmd(0xDE).arg0(1)),
        Trans(FanCmd(CT.FAN_OSC).arg0(1), EncCmd(0xDE).arg0(2)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_range(1,None), EncCmd(0xD3)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(0), EncCmd(0xD1)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(3).arg0(0.1).arg1(0.1), EncCmd(0xA1).arg0(25).arg1(25)), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2).arg0(0).arg1(1), EncCmd(0xA7).arg0(1)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2).arg0(1).arg1(0), EncCmd(0xA7).arg0(2)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).param(2).arg0(1).arg1(1), EncCmd(0xA7).arg0(3)),
    ]),

    FullTranslator('default_translator_zmv0', 'default_translator_zhimei_common', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4).arg0(170).arg1(102).arg2(85)),
    ]),
    
    FullTranslator('default_translator_zmv1', 'default_translator_zhimei_common', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4).arg0(170).arg1(102).arg2(85)),
    ]),

    FullTranslator('default_translator_zmv2', 'default_translator_zhimei_common', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4)),
    ]),
]

