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
  register_service(&BleAdvController::all_on, "all_on_" + this->get_object_id());
  register_service(&BleAdvController::all_off, "all_off_" + this->get_object_id());
  register_service(&BleAdvController::set_timer, "set_timer_" + this->get_object_id(), {"duration"});
  register_service(&BleAdvController::custom_cmd_float, "cmd_" + this->get_object_id(), {"cmd", "param", "arg0", "arg1", "arg2"});
  register_service(&BleAdvController::raw_inject, "inject_raw_" + this->get_object_id(), {"raw"});
#endif
  this->select_encoding_.init("Encoding", this->get_name());
  this->number_duration_.init("Duration", this->get_name());
}

void BleAdvController::dump_config() {
  ESP_LOGCONFIG(TAG, "BleAdvController '%s'", this->get_object_id().c_str());
  ESP_LOGCONFIG(TAG, "  Hash ID '%lX'", this->params_.id_);
  ESP_LOGCONFIG(TAG, "  Index '%d'", this->params_.index_);
  ESP_LOGCONFIG(TAG, "  Transmission Min Duration: %ld ms", this->get_min_tx_duration());
  ESP_LOGCONFIG(TAG, "  Transmission Max Duration: %ld ms", this->max_tx_duration_);
  ESP_LOGCONFIG(TAG, "  Transmission Sequencing Duration: %ld ms", this->seq_duration_);
}

void BleAdvController::controller_command(const BleAdvGenCmd & gen_cmd) {
  ESP_LOGD(TAG, "Controller cmd: %s.", gen_cmd.str().c_str());
  this->enqueue(gen_cmd);
}

void BleAdvController::pair() { 
  this->controller_command(BleAdvGenCmd(CommandType::PAIR, EntityType::CONTROLLER));
}

void BleAdvController::unpair() {
  this->controller_command(BleAdvGenCmd(CommandType::UNPAIR, EntityType::CONTROLLER));
}

void BleAdvController::all_off() {
  this->publish_to_entities(BleAdvGenCmd(CommandType::OFF, EntityType::ALL));
}

void BleAdvController::all_on() {
  this->publish_to_entities(BleAdvGenCmd(CommandType::ON, EntityType::ALL));
}

void BleAdvController::set_timer(float duration) {  // duration is the number of minutes
  this->cancel_timer();
  if (duration == 0) return;
  BleAdvGenCmd gen_cmd(CommandType::TIMER, EntityType::CONTROLLER);
  gen_cmd.args[0] = duration;
  this->controller_command(gen_cmd);
  BleAdvGenCmd off_cmd(CommandType::OFF, EntityType::ALL);
  this->set_timeout(OFF_TIMER_NAME, duration * 60000, std::bind(&BleAdvController::publish, this, off_cmd, false));
}

void BleAdvController::custom_cmd(BleAdvEncCmd & enc_cmd) {
  // enqueue a new CUSTOM command and encode the buffer(s)
  ESP_LOGD(TAG, "Controller Custom Command.");
  this->commands_.emplace_back(CommandType::CUSTOM, EntityType::NOTYPE, 0);
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
  this->commands_.emplace_back(CommandType::CUSTOM, EntityType::NOTYPE, 0);
  this->commands_.back().params_.emplace_back();
  this->commands_.back().params_.back().from_hex_string(raw);
}

void BleAdvController::cancel_timer() {
  if (this->cancel_timeout(OFF_TIMER_NAME)) {
    ESP_LOGD(TAG, "Timer Cancelled.");
  }
}

void BleAdvController::enqueue(ble_adv_handler::BleAdvParams && params) {
  this->commands_.emplace_back(CommandType::CUSTOM, EntityType::NOTYPE, 0);
  std::swap(this->commands_.back().params_, params);
}

void BleAdvController::publish(const BleAdvGenCmd & gen_cmd, bool apply_command) {
  this->skip_commands_ = !apply_command;
  if ((gen_cmd.cmd == CommandType::TIMER) && (gen_cmd.ent_type == EntityType::CONTROLLER)) {
    this->set_timer(gen_cmd.args[0]);
  } else if (gen_cmd.ent_type != EntityType::CONTROLLER) {
    this->publish_to_entities(gen_cmd);
  }
  this->skip_commands_ = false;
}

void BleAdvController::publish_to_entities(const BleAdvGenCmd & gen_cmd) {
  for (auto & entity : this->entities_) {
    if(entity->matches(gen_cmd)) {
      entity->publish(gen_cmd);
    }
  }
}

void BleAdvController::increase_counter() {
  if (this->params_.app_restart_count_ == 0) {
    this->params_.app_restart_count_ = rand() & 0xFF;
  }
  // Reset tx count if near the limit
  if (this->params_.tx_count_ > 126) {
    this->params_.tx_count_ = 0;
    this->params_.app_restart_count_ += 1;
  }
  this->params_.tx_count_++;
}

bool BleAdvController::enqueue(const BleAdvGenCmd &gen_cmd) {
  // Cancel Timer on any command issued if configured
  if (this->is_cancel_timer_on_any_change() && (gen_cmd.ent_type != EntityType::CONTROLLER)) {
    this->cancel_timer();
  }

  // Check if it is needed to send the command to the controlled device
  if (this->skip_commands_) {
    ESP_LOGD(TAG, "Publishing mode - No Command sent to controlled Device.");
    return false;
  }

  // Remove any previous command of the same type in the queue
  uint8_t nb_rm = std::count_if(this->commands_.begin(), this->commands_.end(), [&](QueueItem& q) { return q.matches_cmd(gen_cmd); });
  if (nb_rm) {
    ESP_LOGD(TAG, "Removing %d previous pending commands", nb_rm);
    this->commands_.remove_if( [&](QueueItem& q){ return q.matches_cmd(gen_cmd); } );
  }
  
  // enqueue the new command and encode the buffer(s)
  this->commands_.emplace_back(gen_cmd.cmd, gen_cmd.ent_type, gen_cmd.ent_index);
  this->increase_counter();
  for (auto & encoder : this->encoders_) {
    std::vector< BleAdvEncCmd > enc_cmds;
    encoder->translate_g2e(enc_cmds, gen_cmd);
    for (auto & enc_cmd: enc_cmds) {
      // triggers
      for (auto & sent_trigger: this->sent_triggers_) {
        sent_trigger->trigger(gen_cmd, enc_cmd);
      }
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
  ESP_LOGCONFIG(TAG, "  Index: %d", this->index_);
}

bool BleAdvEntity::matches(const BleAdvGenCmd & gen_cmd) const {
  return (gen_cmd.ent_type == EntityType::ALL) || ((gen_cmd.ent_type == this->type_) && (gen_cmd.ent_index == this->index_));
}

void BleAdvEntity::command(BleAdvGenCmd &gen_cmd) {
  gen_cmd.ent_type = this->type_;
  gen_cmd.ent_index = this->index_;
  this->get_parent()->enqueue(gen_cmd);
}

void BleAdvEntity::command(CommandType cmd_type, float value1, float value2, float value3) {
  BleAdvGenCmd gen_cmd(cmd_type, this->type_);
  gen_cmd.ent_index = this->index_;
  gen_cmd.args[0] = value1;
  gen_cmd.args[1] = value2;
  gen_cmd.args[2] = value3;
  this->get_parent()->enqueue(gen_cmd);
}

} // namespace ble_adv_controller
} // namespace esphome
