#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_adv_handler/ble_adv_handler.h"
#include "esphome/components/ble_adv_controller/ble_adv_controller.h"
#include <vector>
#include <array>

namespace esphome {
namespace ble_adv_remote {

using CommandType = ble_adv_handler::CommandType;
using BleAdvGenCmd = ble_adv_handler::BleAdvGenCmd;
using BleAdvController = ble_adv_controller::BleAdvController;

using BleAdvBaseTrigger = Trigger<const BleAdvGenCmd &>;

using BleAdvCycle = std::vector< std::array< float, 2 > >;

/**
  BleAdvRemote:
    Listen to remotes, convert commands and publish command events
 */
class BleAdvRemote : public ble_adv_handler::BleAdvDevice
{
public:
  void setup() override;
  virtual void dump_config() override;
  virtual void publish(const BleAdvGenCmd & gen_cmd, bool apply_command) override;
  
  void register_trigger(BleAdvBaseTrigger * trigger) { this->triggers_.push_back(trigger); }
  void add_controlling(BleAdvController * controller) { this->controlling_controllers_.push_back(controller); }
  void add_publishing(BleAdvController * controller) { this->publishing_controllers_.push_back(controller); }
  void set_toggle(bool toggle) { this->toggle_ = toggle; }
  void set_level_count(uint8_t level_count) { this->level_count_ = level_count; }
  void set_cycle(const BleAdvCycle & cycle) { this->cycle_ = cycle; }

  void unpair();

protected:
  void publish_eff(const BleAdvGenCmd & gen_cmd, bool apply_command);
  std::vector< BleAdvBaseTrigger * > triggers_;
  std::vector< BleAdvController * > controlling_controllers_;
  std::vector< BleAdvController * > publishing_controllers_;
  bool toggle_{false};
  uint8_t level_count_{10};

  BleAdvCycle cycle_;
  size_t cycle_index_{0};
};

} //namespace ble_adv_remote
} //namespace esphome
