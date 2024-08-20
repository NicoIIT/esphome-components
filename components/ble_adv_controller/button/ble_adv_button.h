#pragma once

#include "esphome/components/button/button.h"
#include "../ble_adv_controller.h"

namespace esphome {
namespace ble_adv_controller {

class BleAdvButton : public button::Button, public BleAdvEntity
{
 public:
  void dump_config() override;
  void press_action() override;
  void set_pair() { this->is_pair_ = true; }
  void set_unpair() { this->is_unpair_ = true; }
  void set_custom_cmd(uint8_t custom_cmd) { this->custom_cmd_ = custom_cmd; }
  void set_param(uint8_t param) { this->param_ = param; }
  void set_args(const std::vector<uint8_t> &args) { this->args_ = args; }

 protected:
  bool is_pair_{false};
  bool is_unpair_{false};
  uint8_t custom_cmd_ = 0;
  uint8_t param_ = 0;
  std::vector<uint8_t> args_;
};

} //namespace ble_adv_controller
} //namespace esphome
