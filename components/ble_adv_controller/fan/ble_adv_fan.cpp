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

/**
On button ON / OFF pressed: only State ON or OFF received
On Speed change: State and Speed received
On ON with Speed: State and Speed received
On Direction Change: only direction received
*/
void BleAdvFan::control(const fan::FanCall &call) {
  bool direction_refresh = false;
  bool oscillation_refresh = false;
  if (call.get_state().has_value()) {
    // State ON/OFF or SPEED changed
    if (!this->state && *call.get_state()) {
      // forcing direction / oscillation refresh on 'switch on' if requested
      direction_refresh |= this->forced_refresh_on_start_;
      oscillation_refresh |= this->forced_refresh_on_start_;
    }
    this->state = *call.get_state();
    if (call.get_speed().has_value()) {
      this->speed = *call.get_speed();
    }
    // Switch ON always setting with SPEED or OFF
    ESP_LOGD(TAG, "BleAdvFan::control - Setting %s with speed %d", this->state ? "ON":"OFF", this->speed);
    uint8_t eff_speed = (REF_SPEED * this->speed) / this->traits_.supported_speed_count();
    this->command(CommandType::FAN_ONOFF_SPEED, this->state ? eff_speed : 0, REF_SPEED);
  }

  if (call.get_direction().has_value()) {
    // Change of direction
    this->direction = *call.get_direction();
    direction_refresh = true;
  }

  if (direction_refresh && this->traits_.supports_direction()) {
    bool isFwd = this->direction == fan::FanDirection::FORWARD;
    ESP_LOGD(TAG, "BleAdvFan::control - Setting direction %s", (isFwd ? "fwd":"rev"));
    this->command(CommandType::FAN_DIR, !isFwd);
  }

  if (call.get_oscillating().has_value()) {
    // Switch Oscillation
    this->oscillating = *call.get_oscillating();
    oscillation_refresh = true;
  }

  if (oscillation_refresh && this->traits_.supports_oscillation()) {
    ESP_LOGD(TAG, "BleAdvFan::control - Setting Oscillation %s", (this->oscillating ? "ON":"OFF"));
    this->command(CommandType::FAN_OSC, this->oscillating);
  }

  this->publish_state();
}

} // namespace ble_adv_controller
} // namespace esphome
