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
class BleAdvEntity;

/**
  BleAdvController:
    One physical device controlled == One Controller.
    Referenced by Entities as their parent to perform commands.
    Chooses which encoder(s) to be used to issue a command
    Interacts with the BleAdvHandler for Queue processing
 */
class BleAdvController : public ble_adv_handler::BleAdvDevice
{
public:
  static constexpr const char * OFF_TIMER_NAME = "off_timer";

  void setup() override;
  void loop() override;
  virtual void dump_config() override;
  virtual void publish(const BleAdvGenCmd & gen_cmd, bool apply_command) override;
  
  void set_min_tx_duration(int tx_duration, int min, int max, int step);
  uint32_t get_min_tx_duration() { return (uint32_t)this->number_duration_.state; }
  void set_max_tx_duration(uint32_t tx_duration) { this->max_tx_duration_ = tx_duration; }
  void set_seq_duration(uint32_t seq_duration) { this->seq_duration_ = seq_duration; }
  void set_reversed(bool reversed) { this->reversed_ = reversed; }
  bool is_reversed() const { return this->reversed_; }
  void set_show_config(bool show_config) { this->show_config_ = show_config; }
  bool is_show_config() { return this->show_config_; }
  void set_cancel_timer_on_any_change(bool cancel_timer) { this->cancel_timer_on_any_change_ = cancel_timer; }
  bool is_cancel_timer_on_any_change() { return this->cancel_timer_on_any_change_; }
  void register_entity(BleAdvEntity * entity) { this->entities_.push_back(entity); }

  // Services / Actions
  void pair();
  void unpair();
  void custom_cmd_float(float cmd, float param, float arg0, float arg1, float arg2);
  void custom_cmd(BleAdvEncCmd & enc_cmd);
  void raw_inject(std::string raw);
  void cancel_timer();

  bool enqueue(const BleAdvGenCmd & cmd);
  void enqueue(ble_adv_handler::BleAdvParams && params);

protected:
  void publish_to_entities(const BleAdvGenCmd & gen_cmd, bool apply_command);
  void increase_counter();

  uint32_t max_tx_duration_ = 3000;
  uint32_t seq_duration_ = 150;

  bool reversed_;

  bool show_config_{false};
  bool cancel_timer_on_any_change_{false};
  ble_adv_handler::BleAdvNumber number_duration_;

  class QueueItem {
  public:
    QueueItem(CommandType cmd_type): cmd_type_(cmd_type) {}
    CommandType cmd_type_;
    ble_adv_handler::BleAdvParams params_;
  
    // Only move operators to avoid data copy
    QueueItem(QueueItem&&) = default;
    QueueItem& operator=(QueueItem&&) = default;
  };
  std::list< QueueItem > commands_;

  // Being advertised data properties
  uint32_t adv_start_time_ = 0;
  uint16_t adv_id_ = 0;

  // Publishing commands listened from remotes/phone
  std::vector< BleAdvEntity * > entities_;
};

/**
  BleAdvEntity: 
    Base class for implementation of Entities, referencing the parent BleAdvController
 */
class BleAdvEntity: public Component, public Parented < BleAdvController >
{
  public:
    virtual void dump_config() override = 0;
    virtual void publish(const BleAdvGenCmd & gen_cmd, bool apply_command);
    virtual void publish(const BleAdvGenCmd & gen_cmd) { }
    void init() { this->get_parent()->register_entity(this); }

  protected:
    void dump_config_base(const char * tag);
    void command(BleAdvGenCmd &gen_cmd);
    void command(CommandType cmd, float value1 = 0, float value2 = 0);

    // skip next commands
    bool skip_commands_{false};
};

} //namespace ble_adv_controller
} //namespace esphome
