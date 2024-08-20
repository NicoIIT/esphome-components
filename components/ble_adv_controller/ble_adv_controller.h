#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif
#include "esphome/components/ble_adv_handler/ble_adv_handler.h"
#include <vector>
#include <list>

namespace esphome {
namespace ble_adv_controller {

using CommandType = ble_adv_handler::CommandType;
using BleAdvGenCmd = ble_adv_handler::BleAdvGenCmd;
using BleAdvEncCmd = ble_adv_handler::BleAdvEncCmd;

/**
  BleAdvController:
    One physical device controlled == One Controller.
    Referenced by Entities as their parent to perform commands.
    Chooses which encoder(s) to be used to issue a command
    Interacts with the BleAdvHandler for Queue processing
 */
class BleAdvController : public Component, public ble_adv_handler::BleAdvDevice
#ifdef USE_API
  , public api::CustomAPIDevice
#endif
{
public:
  void setup() override;
  void loop() override;
  virtual void dump_config() override;
  
  void set_min_tx_duration(int tx_duration, int min, int max, int step);
  uint32_t get_min_tx_duration() { return (uint32_t)this->number_duration_.state; }
  void set_max_tx_duration(uint32_t tx_duration) { this->max_tx_duration_ = tx_duration; }
  void set_seq_duration(uint32_t seq_duration) { this->seq_duration_ = seq_duration; }
  void set_reversed(bool reversed) { this->reversed_ = reversed; }
  bool is_reversed() const { return this->reversed_; }
  void set_show_config(bool show_config) { this->show_config_ = show_config; }
  bool is_show_config() { return this->show_config_; }

#ifdef USE_API
  // Services
  void on_pair();
  void on_unpair();
  void on_cmd(float cmd, float param, float arg0, float arg1, float arg2);
  void on_raw_inject(std::string raw);
#endif

  bool enqueue(BleAdvGenCmd & cmd);

protected:
  void increase_counter();

  uint32_t max_tx_duration_ = 3000;
  uint32_t seq_duration_ = 150;

  bool reversed_;

  bool show_config_{false};
  ble_adv_handler::BleAdvNumber number_duration_;

  class QueueItem {
  public:
    QueueItem(CommandType cmd_type): cmd_type_(cmd_type) {}
    CommandType cmd_type_;
    std::vector< ble_adv_handler::BleAdvParam > params_;
  
    // Only move operators to avoid data copy
    QueueItem(QueueItem&&) = default;
    QueueItem& operator=(QueueItem&&) = default;
  };
  std::list< QueueItem > commands_;

  // Being advertised data properties
  uint32_t adv_start_time_ = 0;
  uint16_t adv_id_ = 0;
};

/**
  BleAdvEntity: 
    Base class for implementation of Entities, referencing the parent BleAdvController
 */
class BleAdvEntity: public Component, public Parented < BleAdvController >
{
  public:
    virtual void dump_config() override = 0;

  protected:
    void dump_config_base(const char * tag);
    void command(BleAdvGenCmd &gen_cmd);
    void command(CommandType cmd, float value1 = 0, float value2 = 0);
};

} //namespace ble_adv_controller
} //namespace esphome
