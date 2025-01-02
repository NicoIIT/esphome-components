import esphome.config_validation as cv

def INVALID_CUSTOM(cont_id, name, args):
  args += [0] * (5 - len(args))
  return f"""
ble_adv_controller button is DEPRECATED, please perform migration to standard template button and ble_adv_controller 'custom_cmd' Action:
  - Migration for Zhijia / FanLamp V1: 
    From:
    - platform: ble_adv_controller
      ble_adv_controller_id: {cont_id}
      name: {name}
      cmd: custom
      args: {args}
    To:
    - platform: template
      entity_category: config         # only if you want to have the button in the 'Configuration' part in HA
      name: {name}
      on_press:
        ble_adv_controller.custom_cmd:
          id: {cont_id}
          cmd: {args[0]}
          args: {args[1:4]}                 # (optional if [0, 0, 0])
  - Migration for FanLamp V2/V3:
    From:
    - platform: ble_adv_controller
      ble_adv_controller_id: {cont_id}
      name: {name}
      cmd: custom
      args: {args}
    To:
    - platform: template
      entity_category: config         # only if you want to have the button in the 'Configuration' part in HA
      name: {name}
      on_press:
        ble_adv_controller.custom_cmd:
          id: {cont_id}
          cmd: {args[0]}
          param1: {args[2]}                # (optional if 0)
          args: {args[3:5]}             # (optional if [0, 0])


#
"""

def INVALID_COMMAND(cmd, cont_id, name):
  return f"""
ble_adv_controller button is DEPRECATED, please perform migration to standard template button and ble_adv_controller '{cmd}' Action:
From:

button:
  - platform: ble_adv_controller
    ble_adv_controller_id: {cont_id}
    name: {name}
    cmd: {cmd}

To:

button:
  - platform: template
    entity_category: config         # only if you want to have the button in the 'Configuration' part in HA
    name: {name}
    on_press:
      ble_adv_controller.{cmd}: {cont_id}

#
"""

def validate_config(config):
    cmd = config["cmd"]
    if cmd == "custom" :
        raise cv.Invalid(INVALID_CUSTOM(config["ble_adv_controller_id"], config["name"], config["args"]))
    elif cmd in ["pair", "unpair"]:
        raise cv.Invalid(INVALID_COMMAND(cmd, config["ble_adv_controller_id"], config["name"]))
    else:
        raise cv.Invalid("ble_adv_controller button is DEPRECATED, please use standard 'template' button and ble_adv_controller Actions.")
    return config

CONFIG_SCHEMA = cv.All({ 
    cv.Optional("ble_adv_controller_id", default=""): cv.string,
    cv.Optional("name", default="My Button Name"): cv.string,
    cv.Optional("cmd", default=""): cv.string,
    cv.Optional("args", default=[]): cv.ensure_list(cv.uint8_t),
    },
    validate_config,
    )
