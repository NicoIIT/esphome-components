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
BleAdvTranslator = bleadvhandler_ns.class_('BleAdvTranslator_base')

def rewrite_float(val):
    return f"{val}f" if isinstance(val, float) else f"{val}"

class MultiplyParamAction:
    def __init__(self, factor):
        self._factor = factor

    def apply(self, statement, reverse = False):
        return f"({statement}) {'/' if reverse else '*'} {rewrite_float(self._factor)}" 

class InverseParamAction:
    def __init__(self, max_value):
        self._max = max_value

    def apply(self, statement, reverse = False):
        return f"({rewrite_float(self._max)} - ({statement}))" 

class ModuloParamAction:
    def __init__(self, factor):
        self._factor = factor

    def apply(self, statement, reverse = False):
        if reverse:
            return f"(uint8_t)({statement}) % {self._factor}" 
        return statement

class CmdParam:
    def __init__(self, conf_name, cpp_name, class_instance_ref):
        self._conf_name = conf_name
        self._cpp_name = cpp_name
        self._class_instance_ref = class_instance_ref
        self._min = None
        self._max = None
        self._copy_from = []
        self._actions = []

    def __repr__(self):
        pres = []
        if self.is_eq():
            pres.append(f"{self._conf_name}: {self._min}")
        else:
            if self._min is not None:
                pres.append(f"{self._conf_name} min: {self._min}")
            if self._max is not None:
                pres.append(f"{self._conf_name} max: {self._max}")
        return ", ".join(pres)
    
    def is_eq(self):
        return (self._min is not None) and (self._max is not None) and (self._min == self._max)

    def validate(self):
        if not self.is_eq() and (self._min is not None) and (self._max is not None) and (self._min > self._max):
            return f"{self._conf_name}_min / {self._conf_name}_max is invalid"
        return None

    def intersect_val_min_max(self, comp):
        if self.is_eq() and comp.is_eq():
            return self._min == comp._min

        if ((self._min is None) and (self._max is None)): return True # no limit for self: intersects with anything
        if ((comp._min is None) and (comp._max is None)): return True # no limit for comp: intersects with anything
        if ((self._min is None) and (comp._min is None)): return True # no min limit for both: intersects
        if ((self._max is None) and (comp._max is None)): return True # no max limit for both: intersects
        if (self._min is None): return (self._max >= comp._min)
        if (self._max is None): return (self._min <= comp._max)
        if (comp._min is None): return (comp._max >= self._min)
        if (comp._max is None): return (comp._min <= self._max)
        if ((self._min >= comp._min) and (self._min <= comp._max)): return True
        if ((self._max >= comp._min) and (self._max <= comp._max)): return True
        return False

    def get_cpp(self):
        cpp_statement = f"{self._class_instance_ref}.{self._cpp_name}"
        for action in self._actions:
            cpp_statement = action.apply(cpp_statement)
        return cpp_statement

    def get_cpp_cond(self):
        cond_list = []
        if self.is_eq():
            cond_list.append(f"({self._class_instance_ref}.{self._cpp_name} == {rewrite_float(self._min)})")
        else:
            if self._min is not None:
                cond_list.append(f"({self._class_instance_ref}.{self._cpp_name} >= {rewrite_float(self._min)})")
            if self._max is not None:
                cond_list.append(f"({self._class_instance_ref}.{self._cpp_name} <= {rewrite_float(self._max)})")
        return " && ".join(cond_list)

    def get_cpp_exec(self):
        if self.is_eq():
            return f"{self._class_instance_ref}.{self._cpp_name} = {rewrite_float(self._min)}; "
        elif self._copy_from:
            cpp_statement = ' + '.join([ from_param.get_cpp() for from_param in self._copy_from ])
            for action in self._actions[::-1]:
                cpp_statement = action.apply(cpp_statement, True)
            return f"{self._class_instance_ref}.{self._cpp_name} = {cpp_statement}; "
        return ""

class CmdBase:
    ## Represents the Conditions on a BleAdvGenCmd / BleAdvEncCmd
    def __init__(self, attribs):
        self._attribs = {}
        for cmd_param in attribs:
            self._attribs[cmd_param._conf_name] = cmd_param
        
    def __repr__(self):
        return ", ".join(list(filter(len, [ repr(cmd_param) for cmd_param in self._attribs.values() ])))

    def get_param(self, param_name):
        return self._attribs[param_name]

    def set_eq(self, param, val):
        self._attribs[param]._min = val
        self._attribs[param]._max = val
        return self

    def set_min(self, param, val):
        self._attribs[param]._min = val
        return self

    def set_max(self, param, val):
        self._attribs[param]._max = val
        return self

    def add_action(self, param, action_type, val):
        self._attribs[param]._actions.append(action_type(val))
        return self

    def validate(self):
        for cmd_param in self._attribs.values():
            if msg := cmd_param.validate():
                return msg
        return None
    
    def intersects(self, comp) -> bool:
        for name, cmd_param in self._attribs.items():
            if not cmd_param.intersect_val_min_max(comp._attribs[name]):
                return False
        return True
    
    def get_cpp_cond(self):
        return " && ".join(list(filter(len, [ cmd_param.get_cpp_cond() for cmd_param in self._attribs.values() ])))
    
    def get_cpp_exec(self):
        return "".join([ cmd_param.get_cpp_exec() for cmd_param in self._attribs.values()])
    
    def shortcuts_map(param, with_modifier):
        shortcuts = {}
        shortcuts[f"{param}"] = partialmethod(CmdBase.set_eq, param)
        if with_modifier:
            shortcuts[f"{param}_min"] = partialmethod(CmdBase.set_min, param)
            shortcuts[f"{param}_max"] = partialmethod(CmdBase.set_max, param)
            shortcuts[f"inv_{param}"] = partialmethod(CmdBase.add_action, param, InverseParamAction)
            shortcuts[f"multi_{param}"] = partialmethod(CmdBase.add_action, param, MultiplyParamAction)
            shortcuts[f"modulo_{param}"] = partialmethod(CmdBase.add_action, param, ModuloParamAction)
        return shortcuts

class Shortcut:
    def __init__(self, name, method, validator, required):
        self._name = name
        self._method = method
        self._validator = validator
        self._required = required

class Shortcuts(list):
    def create(self, class_vars):
        for shortcut in self:
            class_vars[shortcut._name] = shortcut._method

    def get_schema(self):
        return {**{cv.Required(f"{shortcut._name}"): shortcut._validator for shortcut in self if shortcut._required},
                **{cv.Optional(f"{shortcut._name}"): shortcut._validator for shortcut in self if not shortcut._required}}


class EncCmd(CmdBase):
    ## Represents a Condition on a BleAdvEncCmd
    ATTRIBS = [
        # Name, Cpp Name, validator, with modifiers, required
        ["cmd", "cmd", cv.uint8_t, False, True],
        ["param", "param1", cv.uint8_t, True, False],
        ["arg0", "args[0]", cv.uint8_t, True, False],
        ["arg1", "args[1]", cv.uint8_t, True, False],
        ["arg2", "args[2]", cv.uint8_t, True, False],
    ]

    def __init__(self, cmd: int):
        super().__init__([ CmdParam(x[0], x[1], "{ename}") for x in EncCmd.ATTRIBS ])
        self.cmd("0x%02X" % cmd)

    # shortcut functions arg0 / param / arg1_range / ...
    SHORTCUTS = Shortcuts([ Shortcut(k, v, attrib[2], attrib[4]) for attrib in ATTRIBS for k,v in CmdBase.shortcuts_map(attrib[0], attrib[3]).items() ])
    SHORTCUTS.create(vars())

def validate_gen_cmd(value):
    return str(getattr(CT, value))

def validate_gen_type(value):
    return str(getattr(ET, value))

class GenCmd(CmdBase):
    ## Represents a Condition on a BleAdvGenCmd
    ATTRIBS = [
        # Name, Cpp Name, validator, with modifiers, required
        ["cmd", "cmd", validate_gen_cmd, False, True],
        ["type", "ent_type", validate_gen_type, False, True],
        ["index", "ent_index", cv.uint8_t, False, False],
        ["param", "param", cv.uint8_t, True, False],
        ["arg0", "args[0]", cv.float_range(), True, False],
        ["arg1", "args[1]", cv.float_range(), True, False],
        ["arg2", "args[2]", cv.float_range(), True, False],
    ]

    def __init__(self, cmd: str, entity: str, index: int = 0):
        super().__init__([ CmdParam(x[0], x[1], "{gname}") for x in GenCmd.ATTRIBS ])
        self.cmd(str(cmd))
        self.type(str(entity))
        self.index(index)

    # shortcut functions cmd / type / index / arg0 / param / arg1_range / ...
    SHORTCUTS = Shortcuts([ Shortcut(k, v, attrib[2], attrib[4]) for attrib in ATTRIBS for k,v in CmdBase.shortcuts_map(attrib[0], attrib[3]).items() ])
    SHORTCUTS.create(vars())


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
        self._no_reverse = False
        self._no_direct = False

    def __repr__(self):
        return f"Gen: {self._gen} <=> Enc: {self._enc}"

    def no_direct(self, val=True):
        # Flags the translator as not used in direct g2e
        self._no_direct = val
        return self

    def no_reverse(self, val=True):
        # Flags the translator as not used in reverse e2g 
        self._no_reverse = val
        return self

    def _field_copy(self, g_param, e_param, val=True):
        if val:
            self._gen.get_param(g_param)._copy_from.append(self._enc.get_param(e_param))
            self._enc.get_param(e_param)._copy_from.append(self._gen.get_param(g_param))
        return self
        
    def _field_multiply(self, g_param, e_param, multi):
        self._gen.get_param(g_param)._actions.append(MultiplyParamAction(float(multi)))
        return self._field_copy(g_param, e_param)

    def get_cpp_g2e(self, gname, ename):
        return f"if ({self._gen.get_cpp_cond().format(gname=gname, ename=ename)}) {{ {self._enc.get_cpp_exec().format(gname=gname, ename=ename)}return true; }}"
        
    def get_cpp_e2g(self, gname, ename):
        return f"if ({self._enc.get_cpp_cond().format(gname=gname, ename=ename)}) {{ {self._gen.get_cpp_exec().format(gname=gname, ename=ename)}return true; }}"

    # shortcut 'copy' and 'multi' functions for each combination of args and param
    # copy_arg0 / multi_arg0_to_arg2 / multi_param / copy_param_to_arg1 / ...
    SHORTCUTS = Shortcuts()
    for g_attr in GenCmd.ATTRIBS:
        for e_attr in EncCmd.ATTRIBS:
            sec_arg = f"_to_{e_attr[0]}" if (e_attr[0] != g_attr[0]) else ""
            SHORTCUTS.append(Shortcut(f"copy_{g_attr[0]}{sec_arg}", partialmethod(_field_copy, g_attr[0], e_attr[0]), cv.boolean, False))
            SHORTCUTS.append(Shortcut(f"multi_{g_attr[0]}{sec_arg}", partialmethod(_field_multiply, g_attr[0], e_attr[0]), cv.float_range(), False))
    SHORTCUTS.create(vars())
    SHORTCUTS.append(Shortcut(f"no_direct", no_direct, cv.boolean, False))
    SHORTCUTS.append(Shortcut(f"no_reverse", no_reverse, cv.boolean, False))


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
    
    def get_cmds_recursive(self, level = 0):
        if level > 10:
            raise cv.Invalid("Translator extend depth > 10, please check for reference loop.")
        return self._cmds if not self._extend else (self.Get(self._extend).get_cmds_recursive(level + 1) + self._cmds)

    def check_duplicate(self, cmd_ref, cmd_cmp):
        if cmd_ref.intersects(cmd_cmp):
            raise cv.Invalid(f"Translator ID '{self._id}': Intersecting Commands \n {cmd_ref}\n    and \n {cmd_cmp}\n")

    def check_exclusive(self, cmd_ref, cmd_cmp):
        for reason, pair1, pair2 in self.EXLUSIVE_CMD_PAIRS:
            if (cmd_ref.intersects(pair1) and cmd_cmp.intersects(pair2)) or (cmd_ref.intersects(pair2) and cmd_cmp.intersects(pair1)):
                raise cv.Invalid(f"Translator ID '{self._id}': Incompatible Commands ({reason}) \n {cmd_ref}\n    and \n {cmd_cmp}\n")
        return None

    def check_consistency(self):
        cmds = self.get_cmds_recursive()
        checked_cmds = []
        for cmd in cmds:
            err = cmd._gen.validate()
            if err is not None:
                raise cv.Invalid(f"Translator ID '{self._id}': Invalid gen Command({err}) \n {cmd._gen}\n")
            err = cmd._enc.validate()
            if err is not None:
                raise cv.Invalid(f"Translator ID '{self._id}': Invalid enc Command({err}) \n {cmd._enc}\n")
            for prev_cmd in checked_cmds:
                if not cmd._no_reverse and not prev_cmd._no_reverse:
                    self.check_duplicate(cmd._enc, prev_cmd._enc)
                if not cmd._no_direct and not prev_cmd._no_direct:
                    self.check_duplicate(cmd._gen, prev_cmd._gen)
                    self.check_exclusive(cmd._gen, prev_cmd._gen)
            checked_cmds.append(cmd)
            
    def get_class_name(self):
        return f"BleAdvTranslator_{self._id}"

    def get_cpp_class(self):
        gname = 'g'
        ename = 'e'
        inh_class = FullTranslator.Get(self._extend).get_class_name()
        cl = f"\nclass {self.get_class_name()}: public {inh_class}\n{{"
        cl += f"\npublic:"
        cl += f"\n  bool g2e_cmd(const BleAdvGenCmd & {gname}, BleAdvEncCmd & {ename}) const override {{"
        for conds in self._cmds:
            if not conds._no_direct:
                cl += f"\n    {conds.get_cpp_g2e(gname, ename)}"
        cl += f"\n    return {inh_class}::g2e_cmd({gname}, {ename});"
        cl += f"\n  }}" # end of g2e
        cl += f"\n  bool e2g_cmd(const BleAdvEncCmd & {ename}, BleAdvGenCmd & {gname}) const override {{"
        for conds in self._cmds:
            if not conds._no_reverse:
                cl += f"\n    {conds.get_cpp_e2g(gname, ename)}"
        cl += f"\n    return {inh_class}::e2g_cmd({ename}, {gname});"
        cl += f"\n  }}" # end of e2g
        cl += f"\n}};\n"
        return cl

    @classmethod
    def GenerateAllTranslators(cls):
        # sort the translators in the good order to have inheritance working in cpp
        sorted_translators = []
        map_translators = {}
        for trans in cls.REGISTERED_TRANSLATORS.values():
            map_translators.setdefault(trans._extend, []).append(trans)
        sorted_translators = map_translators.pop(None)
        while (map_translators):
            for trans in sorted_translators:
                if trans._id in map_translators:
                    sorted_translators += map_translators.pop(trans._id)
        
        # check consistency
        for trans in sorted_translators:
            trans.check_consistency()

        # write the translator classes in "generated_translators.h"
        with open(os.path.join(os.path.dirname(__file__), 'generated_translators.h'), 'w') as gen_file:
            gen_file.write('// Generated Translators - GENERATED FILE: DO NOT EDIT NOR COMMIT')
            gen_file.write('\n#include "ble_adv_handler.h"')
            gen_file.write('\nnamespace esphome {')
            gen_file.write('\nnamespace ble_adv_handler {\n')
            for trans in sorted_translators:
                if trans._extend is not None:
                    gen_file.write(trans.get_cpp_class())
            gen_file.write('\n} // namespace ble_adv_handler')
            gen_file.write('\n} // namespace esphome')
            gen_file.write('\n')

BASE_TRANSLATOR_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BleAdvTranslator),
    cv.Optional("extend"): cv.use_id(BleAdvTranslator),
    cv.Optional("cmds", default=[]): cv.ensure_list(cv.Schema({
        cv.Required("gen"): cv.Schema(GenCmd.SHORTCUTS.get_schema()),
        cv.Required("enc"): cv.Schema(EncCmd.SHORTCUTS.get_schema()),
        cv.Optional("trans"): cv.Schema(Trans.SHORTCUTS.get_schema()),
    })),
})

def cmd_config(cmd, cmd_conf):
    for func, val in cmd_conf.items():
        getattr(cmd, func)(val)
    return cmd

def load_default_translators(translators):
    # Complete the map of translators with user defined configs, for Class Generation
    for config in translators:
        extend_id = config["extend"].id if "extend" in config else None
        cmds = []
        for x in config["cmds"]:
            cmd_tr = Trans(cmd_config(GenCmd(CT.NOCMD, ET.NOTYPE), x['gen']), cmd_config(EncCmd(0xFF), x['enc']))
            for func, val in x.get("trans", {}).items():
                getattr(cmd_tr, func)(val)
            cmds.append(cmd_tr)
        FullTranslator(config[CONF_ID].id, extend_id, cmds)

    # Generate Stub config with only ID for Default Translators, for use as reference in config
    for translator in BLE_ADV_DEFAULT_TRANSLATORS:
        translators.append({ 
            CONF_ID: cv.declare_id(BleAdvTranslator)(translator._id),
        })
    
    # Check consistency and generate the cpp translators
    FullTranslator.GenerateAllTranslators()

    return translators

async def translator_to_code(config):
    class_gen = bleadvhandler_ns.class_(FullTranslator.Get(config[CONF_ID].id).get_class_name())
    return cg.Pvariable(config[CONF_ID], class_gen.new())

### Mutual Exclusions as can be triggered simultaneously by the Entities in the software ###
# The translator must then define ONLY one of them for a same Entity
# Consider entity index can go up to 3, even if only 0 and 1 are effectivelly used for Light, and only 0 for Fan
for i in range(3):
    # Fan
    FullTranslator.Add_exclusive("Fan - Full / On Off Speed", FanCmd(CT.FAN_FULL, i).param(0), FanCmd(CT.FAN_ONOFF_SPEED, i))
    FullTranslator.Add_exclusive("Fan - Full / Direction", FanCmd(CT.FAN_FULL, i).param(0), FanCmd(CT.FAN_DIR, i))
    FullTranslator.Add_exclusive("Fan - Full / Oscillation", FanCmd(CT.FAN_FULL, i).param(0), FanCmd(CT.FAN_OSC, i))

    # CWW Light
    FullTranslator.Add_exclusive("Light CWW - Brightness / Cold and Warm", LightCmd(CT.LIGHT_CWW_DIM, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_WARM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Color Temperature / Cold and Warm", LightCmd(CT.LIGHT_CWW_WARM, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_WARM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Brightness / Full BR and CT", LightCmd(CT.LIGHT_CWW_DIM, i).param(0), LightCmd(CT.LIGHT_CWW_WARM_DIM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Color Temperature / Full BR and CT", LightCmd(CT.LIGHT_CWW_WARM, i).param(0), LightCmd(CT.LIGHT_CWW_WARM_DIM, i).param(0))
    FullTranslator.Add_exclusive("Light CWW - Full BR and CT / Cold and Warm", LightCmd(CT.LIGHT_CWW_WARM_DIM, i).param(0), LightCmd(CT.LIGHT_CWW_COLD_WARM, i).param(0))

    # RGB Light
    FullTranslator.Add_exclusive("Light RGB - Brightness / Full RGB", LightCmd(CT.LIGHT_RGB_DIM, i).param(0), LightCmd(CT.LIGHT_RGB_FULL, i).param(0))
    FullTranslator.Add_exclusive("Light RGB - RGB / Full RGB", LightCmd(CT.LIGHT_RGB_RGB, i).param(0), LightCmd(CT.LIGHT_RGB_FULL, i).param(0))

########################################
###  DEFAULT TRANSLATORS DEFINITION  ###
########################################
BLE_ADV_BASE_TRANSLATORS = ['base', 'agarce_base'] # translators defined in software

BLE_ADV_DEFAULT_TRANSLATORS = [
    *[ FullTranslator(x, None, []) for x in BLE_ADV_BASE_TRANSLATORS ],
    FullTranslator('default_translator_fanlamp_common', 'base', [
        Trans(ContCmd(CT.PAIR), EncCmd(0x28)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0x45)),
        Trans(AllCmd(CT.OFF), EncCmd(0x6F)),
        Trans(LightCmd(CT.TOGGLE), EncCmd(0x09)),
        Trans(LightCmd(CT.ON), EncCmd(0x10)),
        Trans(LightCmd(CT.OFF), EncCmd(0x11)),
        Trans(LightCmd(CT.ON, 1), EncCmd(0x12)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0x13)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM), EncCmd(0x21).param(0x00)).multi_arg0(255).multi_arg1(255),
        # Physical Remote and app phone shortcut buttons, only reverse
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM), EncCmd(0x21).param(0x40)).multi_arg0(255).multi_arg1(255).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0.1).arg1(0.1), EncCmd(0x23)).no_direct(), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_WARM).param(1), EncCmd(0x21).param(0x24)).no_direct(), # K+
        Trans(LightCmd(CT.LIGHT_CWW_WARM).param(2), EncCmd(0x21).param(0x18)).no_direct(), # K-
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(1), EncCmd(0x21).param(0x14)).no_direct(), # B+
        Trans(LightCmd(CT.LIGHT_CWW_DIM).param(2), EncCmd(0x21).param(0x28)).no_direct(), # B-
        Trans(FanCmd(CT.FAN_OSC_TOGGLE), EncCmd(0x33)).no_direct(),
    ]),

    FullTranslator('default_translator_flv1', 'default_translator_fanlamp_common', [
        Trans(ContCmd(CT.TIMER).arg0_max(0xFF), EncCmd(0x51)).copy_arg0(),
        Trans(ContCmd(CT.TIMER).arg0_min(0x100), EncCmd(0x51).arg0(0xFF)).no_reverse(),
        Trans(FanCmd(CT.FAN_DIR), EncCmd(0x15)).copy_arg0(),
        Trans(FanCmd(CT.FAN_OSC), EncCmd(0x16)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(3), EncCmd(0x31)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(6), EncCmd(0x32).arg1(6)).copy_arg0(),
    ]),

    FullTranslator('default_translator_flv2', 'default_translator_fanlamp_common', [ 
        Trans(ContCmd(CT.TIMER), EncCmd(0x41).multi_arg0(256).modulo_param(256)).copy_arg0_to_param().copy_arg0(),
        Trans(LightCmd(CT.LIGHT_RGB_FULL), EncCmd(0x22)).multi_arg0_to_param(255).multi_arg1_to_arg0(255).multi_arg2_to_arg1(255),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0x15).param(0x00)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0x15).param(0x01)),
        Trans(FanCmd(CT.FAN_OSC).arg0(0), EncCmd(0x16).param(0x00)),
        Trans(FanCmd(CT.FAN_OSC).arg0(1), EncCmd(0x16).param(0x01)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(3), EncCmd(0x31).param(0x00)).copy_arg0(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg1(6), EncCmd(0x31).param(0x20)).copy_arg0(),
    ]),

    FullTranslator('default_translator_zjv0', 'base', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xB0)),
        Trans(ContCmd(CT.TIMER).arg0(60), EncCmd(0xD4)),
        Trans(ContCmd(CT.TIMER).arg0(120), EncCmd(0xD5)),
        Trans(ContCmd(CT.TIMER).arg0(240), EncCmd(0xD6)),
        Trans(ContCmd(CT.TIMER).arg0(480), EncCmd(0xD7)),
        Trans(LightCmd(CT.ON), EncCmd(0xB3)),
        Trans(LightCmd(CT.OFF), EncCmd(0xB2)),
        Trans(LightCmd(CT.LIGHT_CWW_DIM).multi_arg0(1000.0), EncCmd(0xB5).multi_arg1(256.0).modulo_arg2(256)).copy_arg0_to_arg1().copy_arg0_to_arg2(),
        Trans(LightCmd(CT.LIGHT_CWW_WARM).multi_arg0(1000.0), EncCmd(0xB7).multi_arg1(256.0).modulo_arg2(256)).copy_arg0_to_arg1().copy_arg0_to_arg2(),
        Trans(LightCmd(CT.ON, 1), EncCmd(0xA6).arg0(1)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0xA6).arg0(2)),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0xD9)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0xDA)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(0), EncCmd(0xD8)),
        # Fan speed_count 3, direct and reverse
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(1).arg1(3), EncCmd(0xD2)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(2).arg1(3), EncCmd(0xD1)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(3).arg1(3), EncCmd(0xD0)),
        # Fan speed_count 6, direct only
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(1).arg0_max(2).arg1(6), EncCmd(0xD2)).no_reverse(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(3).arg0_max(4).arg1(6), EncCmd(0xD1)).no_reverse(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(5).arg0_max(6).arg1(6), EncCmd(0xD0)).no_reverse(),
        # Physical remote and phone app shortcut buttons, reverse only
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0.1).arg1(0.1), EncCmd(0xA1).arg0(25).arg1(25)).no_direct(), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(0), EncCmd(0xA2).arg0(255).arg1(0)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0).arg1(1), EncCmd(0xA3).arg0(0).arg1(255)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(1), EncCmd(0xA4).arg0(255).arg1(255)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(0), EncCmd(0xA7).arg0(1)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0).arg1(1), EncCmd(0xA7).arg0(2)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(1), EncCmd(0xA7).arg0(3)).no_direct(),
    ]),

    FullTranslator('default_translator_zjv1v2_common', 'base', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xA2)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xA3)),
        Trans(ContCmd(CT.TIMER), EncCmd(0xD9)).multi_arg0(1.0/60.0),
        Trans(LightCmd(CT.ON), EncCmd(0xA5)),
        Trans(LightCmd(CT.OFF), EncCmd(0xA6)),
        Trans(LightCmd(CT.ON, 1), EncCmd(0xAF)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0xB0)),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0xDB)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0xDA)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(0), EncCmd(0xD7)),
        # Fan speed_count 3 configured
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(1).arg1(3), EncCmd(0xD6)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(2).arg1(3), EncCmd(0xD5)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(3).arg1(3), EncCmd(0xD4)),
        # Fan speed_count 6 configured, used for send only
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(1).arg0_max(2).arg1(6), EncCmd(0xD6)).no_reverse(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(3).arg0_max(4).arg1(6), EncCmd(0xD5)).no_reverse(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(5).arg0_max(6).arg1(6), EncCmd(0xD4)).no_reverse(),
    ]),

    FullTranslator('default_translator_zjv1', 'default_translator_zjv1v2_common', [
        Trans(LightCmd(CT.LIGHT_CWW_DIM), EncCmd(0xAD)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_CWW_WARM), EncCmd(0xAE)).multi_arg0(250),
    ]),

    FullTranslator('default_translator_zjv2fl', 'default_translator_zjv1v2_common', [
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM), EncCmd(0xA8)).multi_arg0(250).multi_arg1(250),
        Trans(LightCmd(CT.LIGHT_RGB_DIM), EncCmd(0xC8)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_RGB_RGB), EncCmd(0xCA)).multi_arg0(255).multi_arg1(255).multi_arg2(255),
        # req from app, only reverse, replaced by CT.LIGHT_CWW_COLD_WARM on direct to get ride of flickering
        Trans(LightCmd(CT.LIGHT_CWW_DIM), EncCmd(0xAD)).multi_arg0(250).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_WARM), EncCmd(0xAE)).multi_arg0(250).no_direct(),
    ]),

    FullTranslator('default_translator_zjv2', 'default_translator_zjv1v2_common', [
        Trans(LightCmd(CT.LIGHT_CWW_DIM), EncCmd(0xAD)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_CWW_WARM), EncCmd(0xAE)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_RGB_DIM), EncCmd(0xC8)).multi_arg0(250),
        Trans(LightCmd(CT.LIGHT_RGB_RGB), EncCmd(0xCA)).multi_arg0(255).multi_arg1(255).multi_arg2(255),
    ]),

    FullTranslator('default_translator_zjvr1', 'base', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xA2)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xA3)),
        Trans(LightCmd(CT.ON), EncCmd(0xA5)),
        Trans(LightCmd(CT.OFF), EncCmd(0xA6)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM), EncCmd(0xA8)).multi_arg0(250).multi_arg1(250),
        #  Missing: AF / A7 / A9 / AC / AB / AA 
    ]),

    FullTranslator('default_translator_remote', 'base', [
        Trans(LightCmd(CT.ON), EncCmd(0x08)),
        Trans(LightCmd(CT.OFF), EncCmd(0x06)),
        Trans(LightCmd(CT.TOGGLE, 1), EncCmd(0x13)),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0).arg1(0.1), EncCmd(0x10)).no_direct(), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_WARM), EncCmd(0x0A)).copy_arg0().no_direct(), # K+ 
        Trans(LightCmd(CT.LIGHT_CWW_WARM), EncCmd(0x0B)).copy_arg0().no_direct(), # K-
        Trans(LightCmd(CT.LIGHT_CWW_DIM), EncCmd(0x02)).copy_arg0().no_direct(), # B+
        Trans(LightCmd(CT.LIGHT_CWW_DIM), EncCmd(0x03)).copy_arg0().no_direct(), # B-
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM), EncCmd(0x07)).no_direct(), # CCT / brightness Cycle
    ]),

    FullTranslator('default_translator_agv3', 'agarce_base', [
        Trans(ContCmd(CT.PAIR), EncCmd(0x00).arg0(1)),
        Trans(ContCmd(CT.UNPAIR), EncCmd(0x00).arg0(0)),
        Trans(AllCmd(CT.OFF), EncCmd(0x70).arg0_max(1)),
        Trans(AllCmd(CT.ON), EncCmd(0x70).arg0_min(2)),
        Trans(LightCmd(CT.ON), EncCmd(0x10).arg0(1)),
        Trans(LightCmd(CT.OFF), EncCmd(0x10).arg0(0)),
        Trans(LightCmd(CT.LIGHT_CWW_WARM_DIM).inv_arg0(1.0), EncCmd(0x20)).multi_arg0(100).multi_arg1(100),
    ]),

    FullTranslator('default_translator_zhimei_common_light', 'base', [
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xB0)),
        Trans(ContCmd(CT.TIMER), EncCmd(0xA5).multi_arg0(60.0).modulo_arg1(60)).copy_arg0().copy_arg0_to_arg1(),
        Trans(LightCmd(CT.ON), EncCmd(0xB3)),
        Trans(LightCmd(CT.OFF), EncCmd(0xB2)),
        Trans(LightCmd(CT.ON, 1), EncCmd(0xA6).arg0(2)),
        Trans(LightCmd(CT.OFF, 1), EncCmd(0xA6).arg0(1)),
        Trans(LightCmd(CT.LIGHT_RGB_FULL, 1), EncCmd(0xCA)).multi_arg0(255).multi_arg1(255).multi_arg2(255),
        Trans(LightCmd(CT.LIGHT_CWW_DIM).multi_arg0(1000.0), EncCmd(0xB5).multi_arg1(256.0).modulo_arg2(256)).copy_arg0_to_arg1().copy_arg0_to_arg2(),
        Trans(LightCmd(CT.LIGHT_CWW_WARM).inv_arg0(1.0).multi_arg0(1000.0), EncCmd(0xB7).multi_arg1(256.0).modulo_arg2(256)).copy_arg0_to_arg1().copy_arg0_to_arg2(),
        # Shortcut phone app buttons, only reverse
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0.1).arg1(0.1), EncCmd(0xA1).arg0(25).arg1(25)).no_direct(), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0).arg1(1), EncCmd(0xA7).arg0(1)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(0), EncCmd(0xA7).arg0(2)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(1), EncCmd(0xA7).arg0(3)).no_direct(),
    ]),

    FullTranslator('default_translator_zmv1', 'default_translator_zhimei_common_light', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4).arg0(170).arg1(102).arg2(85)),
    ]),

    FullTranslator('default_translator_zmv2', 'default_translator_zhimei_common_light', [
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4)),
    ]),

    FullTranslator('default_translator_zhimei_fan', 'base', [
        Trans(ContCmd(CT.UNPAIR), EncCmd(0xB0)),
        Trans(ContCmd(CT.PAIR), EncCmd(0xB4).arg0(170).arg1(102).arg2(85)),
        Trans(ContCmd(CT.TIMER), EncCmd(0xD4)).multi_arg0(1.0 / 60.0),
        Trans(AllCmd(CT.ON), EncCmd(0xB3)),
        Trans(AllCmd(CT.OFF), EncCmd(0xB2)),
        Trans(LightCmd(CT.ON), EncCmd(0xA6).arg0(2)),
        Trans(LightCmd(CT.OFF), EncCmd(0xA6).arg0(1)),
        Trans(LightCmd(CT.LIGHT_CWW_DIM).multi_arg0(1000.0), EncCmd(0xB5).multi_arg1(256.0).modulo_arg2(256)).copy_arg0_to_arg1().copy_arg0_to_arg2(),
        Trans(LightCmd(CT.LIGHT_CWW_WARM).inv_arg0(1.0).multi_arg0(1000.0), EncCmd(0xB7).multi_arg1(256.0).modulo_arg2(256)).copy_arg0_to_arg1().copy_arg0_to_arg2(),
        Trans(FanCmd(CT.FAN_DIR).arg0(0), EncCmd(0xD9)),
        Trans(FanCmd(CT.FAN_DIR).arg0(1), EncCmd(0xDA)),
        Trans(FanCmd(CT.FAN_OSC).arg0(0), EncCmd(0xDE).arg0(1)),
        Trans(FanCmd(CT.FAN_OSC).arg0(1), EncCmd(0xDE).arg0(2)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0(0), EncCmd(0xD1)),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(1).arg1(3), EncCmd(0xD3)).multi_arg0(2).no_reverse(),
        Trans(FanCmd(CT.FAN_ONOFF_SPEED).arg0_min(1).arg1(6), EncCmd(0xD3)).copy_arg0(),
        # Shortcut phone app buttons, only reverse
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0.1).arg1(0.1), EncCmd(0xA1).arg0(25).arg1(25)).no_direct(), # night mode
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(0).arg1(1), EncCmd(0xA7).arg0(1)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(0), EncCmd(0xA7).arg0(2)).no_direct(),
        Trans(LightCmd(CT.LIGHT_CWW_COLD_WARM).arg0(1).arg1(1), EncCmd(0xA7).arg0(3)).no_direct(),
    ]),

]

