#pragma once

#include "esphome/core/automation.h"
#include "ble_adv_fan.h"

namespace esphome {
namespace ble_adv_controller {

template<typename... Ts> class FanPublishStateAction : public Action<Ts...> {
 public:
  explicit FanPublishStateAction(BleAdvFan *fan) : fan_(fan) {}
  TEMPLATABLE_VALUE(bool, state)
  TEMPLATABLE_VALUE(bool, oscillating)
  TEMPLATABLE_VALUE(int, speed)
  TEMPLATABLE_VALUE(fan::FanDirection, direction)

  void play(Ts... x) override {
    if (this->state_.has_value()) {
      this->fan_->state = this->state_.value(x...);
    }
    if (this->oscillating_.has_value()) {
      this->fan_->oscillating = this->oscillating_.value(x...);
    }
    if (this->speed_.has_value()) {
      this->fan_->speed = this->speed_.value(x...);
    }
    if (this->direction_.has_value()) {
      this->fan_->direction = this->direction_.value(x...);
    }
    this->fan_->publish_state();
  }

 protected:
  BleAdvFan *fan_;
};

}  // namespace ble_adv_controller
}  // namespace esphome