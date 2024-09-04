#pragma once

#include "esphome/components/fan/fan.h"
#include "../ble_adv_controller.h"

namespace esphome {
namespace ble_adv_controller {

class BleAdvFan : public fan::Fan, public BleAdvEntity
{
 public:
  void dump_config() override;
  fan::FanTraits get_traits() override { return this->traits_; }
  void setup() override;
  void control(const fan::FanCall &call) override;
  void publish(const BleAdvGenCmd & gen_cmd) override;

  void set_speed_count(uint8_t speed_count) { this->traits_.set_supported_speed_count(speed_count); this->traits_.set_speed(speed_count > 0);}
  void set_direction_supported(bool use_direction) { this->traits_.set_direction(use_direction); }
  void set_oscillation_supported(bool use_oscillation) { this->traits_.set_oscillation(use_oscillation); }
  void set_forced_refresh_on_start(bool forced_refresh_on_start) { this->forced_refresh_on_start_ = forced_refresh_on_start; }

protected:
  static constexpr const size_t REF_SPEED = 6;
  fan::FanTraits traits_;
  bool forced_refresh_on_start_{true};
};

} //namespace ble_adv_controller
} //namespace esphome
