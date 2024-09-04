import esphome.config_validation as cv

INVALID_CUSTOM = """
ble_adv_controller button is DEPRECATED, please perform migration to standard template button and ble_adv_controller 'custom_cmd' Action:
  - Migration for Zhijia / FanLamp V1: 
    From:
    - platform: ble_adv_controller
      ble_adv_controller_id: my_controller
      cmd: custom
      args: [A,B,C,D,E]
    To:
    - platform: template
      entity_category: config         # only if you want to have the button in the 'Configuration' part in HA
      ble_adv_controller.custom_cmd:
        id: my_controller
        cmd: A
        args: [B,C,D]                 # (optional)
  - Migration for FanLamp V2/V3:
    From:
    - platform: ble_adv_controller
      ble_adv_controller_id: my_controller
      cmd: custom
      args: [A,B,C,D,E]
    To:
    - platform: template
      entity_category: config         # only if you want to have the button in the 'Configuration' part in HA
      ble_adv_controller.custom_cmd:
        id: my_controller
        cmd: A
        param1: C                     # (optional)
        args: [D,E]                   # (optional)
"""

def INVALID_COMMAND(cmd):
  return f"""
ble_adv_controller button is DEPRECATED, please perform migration to standard template button and ble_adv_controller '{cmd}' Action:
From:

button:
  - platform: ble_adv_controller
    ble_adv_controller_id: my_controller
    cmd: {cmd}

To:

button:
  - platform: template
    entity_category: config         # only if you want to have the button in the 'Configuration' part in HA
    on_press:
      ble_adv_controller.{cmd}: my_controller
"""

def validate_cmd(cmd):
    if cmd == "custom" :
        raise cv.Invalid(INVALID_CUSTOM)
    elif cmd in ["pair", "unpair"]:
        raise cv.Invalid(INVALID_COMMAND(cmd))
    else:
        raise cv.Invalid("ble_adv_controller button is DEPRECATED, please use standard 'template' button and ble_adv_controller Actions.")
    return cmd

CONFIG_SCHEMA = { 
    cv.Optional("ble_adv_controller_id", default=""): cv.string,
    cv.Optional("name", default=""): cv.string,
    cv.Optional("cmd", default=""): validate_cmd,
    }
