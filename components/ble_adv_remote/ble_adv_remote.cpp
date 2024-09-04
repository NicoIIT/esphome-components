#include "ble_adv_remote.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_remote {

static const char *TAG = "ble_adv_remote";

void BleAdvRemote::setup() {
#ifdef USE_API
  register_service(&BleAdvRemote::unpair, "unpair_" + this->get_object_id());
#endif
}

void BleAdvRemote::dump_config() {
  ESP_LOGCONFIG(TAG, "BleAdvRemote '%s'", this->get_object_id().c_str());
  ESP_LOGCONFIG(TAG, "  Hash ID '%X'", this->params_.id_);
  ESP_LOGCONFIG(TAG, "  Index '%d'", this->params_.index_);
}

void BleAdvRemote::unpair() {
  // The remote is not able to directly use its BleAdvHandler parent to send ADV message as it has no queue to do so
  // So the easiest way to send a command is to generate it and send it as raw to one of the controller it controls / publishes
  if (this->encoders_.empty()) {
    ESP_LOGW(TAG, "Unable to send UNPAIR as no encoder available.");
    return;
  }

  const auto & encoder = this->encoders_.front();
  std::vector< ble_adv_handler::BleAdvEncCmd > enc_cmds;
  encoder->translate_g2e(enc_cmds, CommandType::UNPAIR);
  if (enc_cmds.empty()) {
    ESP_LOGW(TAG, "Unable to send UNPAIR as feature not available for encoder.");
    return;
  }

  ble_adv_handler::BleAdvParams params;
  encoder->encode(params, enc_cmds.front(), this->params_);

  if (!this->controlling_controllers_.empty()) {
    this->controlling_controllers_.front()->enqueue(std::move(params));
  } else if (!this->publishing_controllers_.empty()) {
    this->publishing_controllers_.front()->enqueue(std::move(params));
  } else {
    ESP_LOGW(TAG, "Unable to send UNPAIR as no publishing / controlling controller referenced.");
  }
}

void BleAdvRemote::publish(const BleAdvGenCmd & gen_cmd, bool apply_command) {
  if (this->toggle_) {
    if (gen_cmd.cmd == CommandType::LIGHT_ON || gen_cmd.cmd == CommandType::LIGHT_OFF) {
      this->publish_eff(CommandType::LIGHT_TOGGLE, apply_command);
      return;
    } else if (gen_cmd.cmd == CommandType::LIGHT_SEC_ON || gen_cmd.cmd == CommandType::LIGHT_SEC_OFF) {
      this->publish_eff(CommandType::LIGHT_SEC_TOGGLE, apply_command);
      return;
    } else if (gen_cmd.cmd == CommandType::FAN_DIR) {
      this->publish_eff(CommandType::FAN_DIR_TOGGLE, apply_command);
      return;
    } else if (gen_cmd.cmd == CommandType::FAN_OSC) {
      this->publish_eff(CommandType::FAN_OSC_TOGGLE, apply_command);
      return;
    }
  }
  if ((gen_cmd.cmd == CommandType::LIGHT_WCOLOR) && (gen_cmd.param == 2) && !this->cycle_.empty()) {
    // Internal Cycle: change index and convert to standard Cold / Warm
    if (++this->cycle_index_ >= this->cycle_.size()) this->cycle_index_ = 0;
    auto & values =  this->cycle_[this->cycle_index_];
    BleAdvGenCmd new_cmd(CommandType::LIGHT_WCOLOR);
    new_cmd.args[0] = values[0];
    new_cmd.args[1] = values[1];
    this->publish_eff(new_cmd, apply_command);
    return;
  }
  if ((gen_cmd.cmd == CommandType::LIGHT_DIM || gen_cmd.cmd == CommandType::LIGHT_CCT) && (gen_cmd.param != 0)) {
    BleAdvGenCmd new_cmd = gen_cmd;
    new_cmd.args[0] = 1.0f / (float)this->level_count_;
    this->publish_eff(new_cmd, apply_command);
    return;
  }
  this->publish_eff(gen_cmd, apply_command);
}

void BleAdvRemote::publish_eff(const BleAdvGenCmd & gen_cmd, bool apply_command) {
  for (auto & trigger : this->triggers_) {
    trigger->trigger(gen_cmd);
  }

  for (auto & controller : this->controlling_controllers_) {
    controller->publish(gen_cmd, true);
  }

  for (auto & controller : this->publishing_controllers_) {
    controller->publish(gen_cmd, false);
  }
}

} // namespace ble_adv_remote
} // namespace esphome
