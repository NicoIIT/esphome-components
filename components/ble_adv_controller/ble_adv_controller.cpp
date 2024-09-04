#include "ble_adv_controller.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ble_adv_controller {

static const char *TAG = "ble_adv_controller";

void BleAdvController::set_min_tx_duration(int tx_duration, int min, int max, int step) {
  this->number_duration_.traits.set_min_value(min);
  this->number_duration_.traits.set_max_value(max);
  this->number_duration_.traits.set_step(step);
  this->number_duration_.state = tx_duration;
}

void BleAdvController::setup() {
#ifdef USE_API
  register_service(&BleAdvController::pair, "pair_" + this->get_object_id());
  register_service(&BleAdvController::unpair, "unpair_" + this->get_object_id());
  register_service(&BleAdvController::custom_cmd_float, "cmd_" + this->get_object_id(), {"cmd", "param", "arg0", "arg1", "arg2"});
  register_service(&BleAdvController::raw_inject, "inject_raw_" + this->get_object_id(), {"raw"});
#endif
  if (this->is_show_config()) {
    this->select_encoding_.init("Encoding", this->get_name());
    this->number_duration_.init("Duration", this->get_name());
  }
}

void BleAdvController::dump_config() {
  ESP_LOGCONFIG(TAG, "BleAdvController '%s'", this->get_object_id().c_str());
  ESP_LOGCONFIG(TAG, "  Hash ID '%lX'", this->params_.id_);
  ESP_LOGCONFIG(TAG, "  Index '%d'", this->params_.index_);
  ESP_LOGCONFIG(TAG, "  Transmission Min Duration: %ld ms", this->get_min_tx_duration());
  ESP_LOGCONFIG(TAG, "  Transmission Max Duration: %ld ms", this->max_tx_duration_);
  ESP_LOGCONFIG(TAG, "  Transmission Sequencing Duration: %ld ms", this->seq_duration_);
  ESP_LOGCONFIG(TAG, "  Configuration visible: %s", this->show_config_ ? "YES" : "NO");
}

void BleAdvController::pair() { 
  BleAdvGenCmd cmd(CommandType::PAIR);
  ESP_LOGD(TAG, "Controller cmd: %s.", cmd.str().c_str());
  this->enqueue(cmd);
}

void BleAdvController::unpair() {
  BleAdvGenCmd cmd(CommandType::UNPAIR);
  ESP_LOGD(TAG, "Controller cmd: %s.", cmd.str().c_str());
  this->enqueue(cmd);
}

void BleAdvController::custom_cmd(BleAdvEncCmd & enc_cmd) {
  // enqueue a new CUSTOM command and encode the buffer(s)
  ESP_LOGD(TAG, "Controller Custom Command.");
  this->commands_.emplace_back(CommandType::CUSTOM);
  this->increase_counter();
  for (auto encoder : this->encoders_) {
    encoder->encode(this->commands_.back().params_, enc_cmd, this->params_);
  }
}

void BleAdvController::custom_cmd_float(float cmd_type, float param, float arg0, float arg1, float arg2) {
  BleAdvEncCmd enc_cmd((uint8_t)cmd_type);
  enc_cmd.param1 = (uint8_t)param;
  enc_cmd.args[0] = (uint8_t)arg0;
  enc_cmd.args[1] = (uint8_t)arg1;
  enc_cmd.args[2] = (uint8_t)arg2;
  this->custom_cmd(enc_cmd);
}

void BleAdvController::raw_inject(std::string raw) {
  ESP_LOGD(TAG, "Controller Raw Injection.");
  this->commands_.emplace_back(CommandType::CUSTOM);
  this->commands_.back().params_.emplace_back();
  this->commands_.back().params_.back().from_hex_string(raw);
}

void BleAdvController::cancel_timer() {
  if (this->cancel_timeout(OFF_TIMER_NAME)) {
    ESP_LOGD(TAG, "Timer Cancelled.");
  }
}

void BleAdvController::enqueue(ble_adv_handler::BleAdvParams && params) {
  this->commands_.emplace_back(CommandType::CUSTOM);
  std::swap(this->commands_.back().params_, params);
}

void BleAdvController::publish(const BleAdvGenCmd & gen_cmd, bool apply_command) {
  if (gen_cmd.cmd == CommandType::TIMER) {
    if (apply_command) {
      this->enqueue(gen_cmd);
    }
    this->cancel_timer();
    this->set_timeout(OFF_TIMER_NAME, gen_cmd.args[0] * 60000, std::bind(&BleAdvController::publish, this, CommandType::ALL_OFF, false));
    ESP_LOGD(TAG, "Timer setup - %.0fh.", gen_cmd.args[0] / 60.0);
  } else if (gen_cmd.cmd == CommandType::ALL_OFF) {
    this->publish_to_entities(BleAdvGenCmd(CommandType::LIGHT_OFF), apply_command);
    this->publish_to_entities(BleAdvGenCmd(CommandType::LIGHT_SEC_OFF), apply_command);
    this->publish_to_entities(BleAdvGenCmd(CommandType::FAN_ONOFF_SPEED), apply_command);
  } else if (!gen_cmd.is_controller_cmd()) {
    this->publish_to_entities(gen_cmd, apply_command);
  }
}

void BleAdvController::publish_to_entities(const BleAdvGenCmd & gen_cmd, bool apply_command) {
  for (auto & entity : this->entities_) {
    entity->publish(gen_cmd, apply_command);
  }
}

void BleAdvController::increase_counter() {
  // Reset tx count if near the limit
  if (this->params_.tx_count_ > 126) {
    this->params_.tx_count_ = 0;
  }
  this->params_.tx_count_++;
}

bool BleAdvController::enqueue(const BleAdvGenCmd &gen_cmd) {
  // Remove any previous command of the same type in the queue
  uint8_t nb_rm = std::count_if(this->commands_.begin(), this->commands_.end(), [&](QueueItem& q){ return q.cmd_type_ == gen_cmd.cmd; });
  if (nb_rm) {
    ESP_LOGD(TAG, "Removing %d previous pending commands", nb_rm);
    this->commands_.remove_if( [&](QueueItem& q){ return q.cmd_type_ == gen_cmd.cmd; } );
  }
  
  // enqueue the new command and encode the buffer(s)
  this->commands_.emplace_back(gen_cmd.cmd);
  this->increase_counter();
  for (auto & encoder : this->encoders_) {
    std::vector< BleAdvEncCmd > enc_cmds;
    encoder->translate_g2e(enc_cmds, gen_cmd);
    for (auto & enc_cmd: enc_cmds) {
      encoder->encode(this->commands_.back().params_, enc_cmd, this->params_);
    }
  }
  
  return !this->commands_.back().params_.empty();
}

void BleAdvController::loop() {
  uint32_t now = millis();
  if(this->adv_start_time_ == 0) {
    // no on going command advertised by this controller, check if any to advertise
    if(!this->commands_.empty()) {
      QueueItem & item = this->commands_.front();
      if (!item.params_.empty()) {
        // setup seq duration for each packet
        bool use_seq_duration = (this->seq_duration_ > 0) && (this->seq_duration_ < this->get_min_tx_duration());
        for (auto & param : item.params_) {
          param.duration_ = use_seq_duration ? this->seq_duration_: this->get_min_tx_duration();
        }
        this->adv_id_ = this->get_parent()->add_to_advertiser(item.params_);
        this->adv_start_time_ = now;
      }
      this->commands_.pop_front();
    }
  }
  else {
    // command is being advertised by this controller, check if stop and clean-up needed
    uint32_t duration = this->commands_.empty() ? this->max_tx_duration_ : this->number_duration_.state;
    if (now > this->adv_start_time_ + duration) {
      this->adv_start_time_ = 0;
      this->get_parent()->remove_from_advertiser(this->adv_id_);
    }
  }
}

void BleAdvEntity::dump_config_base(const char * tag) {
  ESP_LOGCONFIG(tag, "  Controller '%s'", this->get_parent()->get_name().c_str());
}

void BleAdvEntity::command(BleAdvGenCmd &gen_cmd) {
  if (!this->skip_commands_) {
    this->get_parent()->enqueue(gen_cmd);
  } else {
    ESP_LOGD(TAG, "Publishing mode - No Command sent to controlled Device.");
  }
  if (this->get_parent()->is_cancel_timer_on_any_change() && !gen_cmd.is_controller_cmd()) {
    this->get_parent()->cancel_timer();
  }
}

void BleAdvEntity::command(CommandType cmd_type, float value1, float value2) {
  BleAdvGenCmd gen_cmd(cmd_type);
  gen_cmd.args[0] = value1;
  gen_cmd.args[1] = value2;
  this->command(gen_cmd);
}

void BleAdvEntity::publish(const BleAdvGenCmd & gen_cmd, bool apply_command) {
  this->skip_commands_ = !apply_command;
  this->publish(gen_cmd);
  this->skip_commands_ = false;
}

} // namespace ble_adv_controller
} // namespace esphome
