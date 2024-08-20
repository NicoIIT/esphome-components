#include "ble_adv_button.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_adv_controller/ble_adv_controller.h"

namespace esphome {
namespace ble_adv_controller {

static const char *TAG = "ble_adv_button";

void BleAdvButton::dump_config() {
  LOG_BUTTON("", "BleAdvButton", this);
  BleAdvEntity::dump_config_base(TAG);
}

void BleAdvButton::press_action() {
  ESP_LOGD(TAG, "BleAdvButton::press_action called");
  if (this->is_pair_) {
    this->command(CommandType::PAIR);
  } else if (this->is_unpair_) {
    this->command(CommandType::UNPAIR);
  } else {
    this->get_parent()->on_cmd(this->custom_cmd_, this->param_, this->args_[0], this->args_[1], this->args_[2]);
  }
}

} // namespace ble_adv_controller
} // namespace esphome
