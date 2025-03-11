#include "ble_adv_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_adv_controller/ble_adv_controller.h"

namespace esphome {
namespace ble_adv_controller {

static const char *TAG = "ble_adv_fan";

void BleAdvFan::dump_config() {
  LOG_FAN("", "BleAdvFan", this);
  BleAdvEntity::dump_config_base(TAG);
}

void BleAdvFan::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
  }
}

void BleAdvFan::publish(const BleAdvGenCmd &gen_cmd) {
  fan::FanCall call = this->make_call();
  if (gen_cmd.cmd == CommandType::ON) {
    call.set_state(true).perform();
  } else if (gen_cmd.cmd == CommandType::OFF) {
    call.set_state(false).perform();
  } else if (gen_cmd.cmd == CommandType::TOGGLE) {
    call.set_state(!this->state).perform();
  } else if (gen_cmd.cmd == CommandType::FAN_ONOFF_SPEED) {
    if (gen_cmd.args[0] == 0) {
      call.set_state(false).perform();
    } else {
      float max_speed = (gen_cmd.args[1] == 0) ? REF_SPEED : gen_cmd.args[1];
      uint8_t rounded_speed =
          (uint8_t) (((float) (gen_cmd.args[0] * this->traits_.supported_speed_count()) / max_speed) + 0.5f);
      call.set_speed(rounded_speed);
      call.set_state(true).perform();
    }
  } else if (gen_cmd.cmd == CommandType::FAN_FULL) {
    if (gen_cmd.args[0] == 0) {
      call.set_state(false).perform();
    } else {
      call.set_speed(gen_cmd.args[0]);
      call.set_direction((gen_cmd.args[1] == 0) ? fan::FanDirection::FORWARD : fan::FanDirection::REVERSE);
      call.set_oscillating(gen_cmd.args[2] != 0);
      call.set_state(true).perform();
    }
  } else if (!this->state) {
    ESP_LOGD(TAG, "Change ignored as entity is OFF.");
    return;
  }

  if (gen_cmd.cmd == CommandType::FAN_DIR) {
    call.set_direction((gen_cmd.args[0] == 0) ? fan::FanDirection::FORWARD : fan::FanDirection::REVERSE).perform();
  } else if (gen_cmd.cmd == CommandType::FAN_DIR_TOGGLE) {
    call.set_direction((this->direction == fan::FanDirection::REVERSE) ? fan::FanDirection::FORWARD
                                                                       : fan::FanDirection::REVERSE)
        .perform();
  } else if (gen_cmd.cmd == CommandType::FAN_OSC) {
    call.set_oscillating(gen_cmd.args[0] != 0).perform();
  } else if (gen_cmd.cmd == CommandType::FAN_OSC_TOGGLE) {
    call.set_oscillating(!this->oscillating).perform();
  }
}

/**
On button ON / OFF pressed: only State ON or OFF received
On Speed change: State and Speed received
On ON with Speed: State and Speed received
On Direction Change: only direction received
*/
void BleAdvFan::control(const fan::FanCall &call) {
  bool direction_refresh = false;
  bool oscillation_refresh = false;
  uint8_t sub_cmds = 0;
  if (call.get_state().has_value()) {
    // State ON/OFF or SPEED changed
    sub_cmds |= ble_adv_handler::FanSubCmdType::STATE;
    if (!this->state && *call.get_state()) {
      // forcing direction / oscillation refresh on 'switch on' if requested
      direction_refresh |= this->forced_refresh_on_start_;
      oscillation_refresh |= this->forced_refresh_on_start_;
    }
    this->state = *call.get_state();
    if (call.get_speed().has_value()) {
      sub_cmds |= ble_adv_handler::FanSubCmdType::SPEED;
      this->speed = *call.get_speed();
    }
    // Switch ON always setting with SPEED or OFF
    ESP_LOGD(TAG, "BleAdvFan::control - Setting %s with speed %d", this->state ? "ON" : "OFF", this->speed);
    this->command(CommandType::FAN_ONOFF_SPEED, this->state ? this->speed : 0, this->traits_.supported_speed_count());
  }

  if (call.get_direction().has_value()) {
    // Change of direction
    sub_cmds |= ble_adv_handler::FanSubCmdType::DIR;
    this->direction = *call.get_direction();
    direction_refresh = true;
  }

  if (direction_refresh && this->traits_.supports_direction()) {
    bool isFwd = this->direction == fan::FanDirection::FORWARD;
    ESP_LOGD(TAG, "BleAdvFan::control - Setting direction %s", (isFwd ? "fwd" : "rev"));
    this->command(CommandType::FAN_DIR, !isFwd);
  }

  if (call.get_oscillating().has_value()) {
    // Switch Oscillation
    sub_cmds |= ble_adv_handler::FanSubCmdType::OSC;
    this->oscillating = *call.get_oscillating();
    oscillation_refresh = true;
  }

  if (oscillation_refresh && this->traits_.supports_oscillation()) {
    ESP_LOGD(TAG, "BleAdvFan::control - Setting Oscillation %s", (this->oscillating ? "ON" : "OFF"));
    this->command(CommandType::FAN_OSC, this->oscillating);
  }

  // Full command including everything: what is requested and full state
  // EXCLUSIVE with other commands
  BleAdvGenCmd gen_cmd(CommandType::FAN_FULL, EntityType::FAN);
  gen_cmd.param = sub_cmds;
  gen_cmd.args[0] = this->state ? this->speed : 0;
  gen_cmd.args[1] = this->direction == fan::FanDirection::REVERSE;
  gen_cmd.args[2] = this->oscillating;
  this->command(gen_cmd);

  this->publish_state();
}

}  // namespace ble_adv_controller
}  // namespace esphome
