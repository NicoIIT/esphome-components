#include "ble_adv_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_controller {

static const char *TAG = "ble_adv_light";

float ensure_range(float f, float mini = 0.0f, float maxi = 1.0f) {
  return (f > maxi) ? maxi : ( (f < mini) ? mini : f );
}

void BleAdvLight::set_min_brightness(int min_brightness, int min, int max, int step) { 
  this->number_min_brightness_.traits.set_min_value(min);
  this->number_min_brightness_.traits.set_max_value(max);
  this->number_min_brightness_.traits.set_step(step);
  this->number_min_brightness_.state = min_brightness; 
}

void BleAdvLight::set_traits(float cold_white_temperature, float warm_white_temperature) {
  this->traits_.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
  this->traits_.set_min_mireds(cold_white_temperature);
  this->traits_.set_max_mireds(warm_white_temperature);
}

void BleAdvLight::setup() {
  if (this->get_parent()->is_show_config()) {
    this->number_min_brightness_.init("Min Brightness", this->get_name());
  }
}

void BleAdvLight::dump_config() {
  ESP_LOGCONFIG(TAG, "BleAdvLight");
  BleAdvEntity::dump_config_base(TAG);
  ESP_LOGCONFIG(TAG, "  Base Light '%s'", this->state_->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Cold White Temperature: %f mireds", this->traits_.get_min_mireds());
  ESP_LOGCONFIG(TAG, "  Warm White Temperature: %f mireds", this->traits_.get_max_mireds());
  ESP_LOGCONFIG(TAG, "  Constant Brightness: %s", this->constant_brightness_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Minimum Brightness: %.0f%%", this->get_min_brightness() * 100);
}

float BleAdvLight::get_ha_brightness(float device_brightness) {
  return ensure_range((ensure_range(device_brightness, this->get_min_brightness()) - this->get_min_brightness()) / (1.f - this->get_min_brightness()), 0.01f);
}

float BleAdvLight::get_device_brightness(float ha_brightness) {
  return ensure_range(this->get_min_brightness() + ha_brightness * (1.f - this->get_min_brightness()));
}

float BleAdvLight::get_ha_color_temperature(float device_color_temperature) {
  return ensure_range(device_color_temperature) * (this->traits_.get_max_mireds() - this->traits_.get_min_mireds()) + this->traits_.get_min_mireds();
}

float BleAdvLight::get_device_color_temperature(float ha_color_temperature) {
  return ensure_range((ha_color_temperature - this->traits_.get_min_mireds()) / (this->traits_.get_max_mireds() - this->traits_.get_min_mireds()));
}

void BleAdvLight::publish(const BleAdvGenCmd & gen_cmd) {
  if (!gen_cmd.is_light_cmd()) return;

  light::LightCall call = this->state_->make_call();
  call.set_color_mode(light::ColorMode::COLD_WARM_WHITE);

  if (gen_cmd.cmd == CommandType::LIGHT_ON) {
    call.set_state(1.0f).perform();
  } else if (gen_cmd.cmd == CommandType::LIGHT_OFF) {
    call.set_state(0.0f).perform();
  } else if (gen_cmd.cmd == CommandType::LIGHT_TOGGLE) {
    call.set_state(this->is_off_ ? 1.0f : 0.0f).perform();
  } else if (this->is_off_) {
    ESP_LOGD(TAG, "Change ignored as entity is OFF.");
    return;
  }
  
  if (gen_cmd.cmd == CommandType::LIGHT_CCT) {
    if (gen_cmd.param == 0) {
      call.set_color_temperature(this->get_ha_color_temperature(gen_cmd.args[0])).perform();
    } else if (gen_cmd.param == 1) { // Color Temp +
      call.set_color_temperature(this->get_ha_color_temperature(this->warm_color_ + gen_cmd.args[0])).perform();
    } else if (gen_cmd.param == 2) { // Color Temp -
      call.set_color_temperature(this->get_ha_color_temperature(this->warm_color_ - gen_cmd.args[0])).perform();
    }
  } else if (gen_cmd.cmd == CommandType::LIGHT_DIM) {
    if (gen_cmd.param == 0) {
      call.set_brightness(this->get_ha_brightness(gen_cmd.args[0])).perform();
    } else if (gen_cmd.param == 1) { // Brightness +
      call.set_brightness(this->get_ha_brightness(this->brightness_ + gen_cmd.args[0])).perform();
    } else if (gen_cmd.param == 2) { // Brightness -
      call.set_brightness(this->get_ha_brightness(this->brightness_ - gen_cmd.args[0])).perform();
    }
  } else if (gen_cmd.cmd == CommandType::LIGHT_WCOLOR) {
    // standard cold(args[0]) / warm(args[1]) update
    call.set_color_temperature(this->get_ha_color_temperature(gen_cmd.args[1] / (gen_cmd.args[0] + gen_cmd.args[1])));
    call.set_brightness(this->get_ha_brightness(std::max(gen_cmd.args[0], gen_cmd.args[1])));
    call.perform();
  }
}

void BleAdvLight::update_state(light::LightState *state) {
  // If target state is off, switch off
  if (state->current_values.get_state() == 0) {
    ESP_LOGD(TAG, "Switch OFF");
    this->command(CommandType::LIGHT_OFF);
    this->is_off_ = true;
    return;
  }

  // If current state is off, switch on
  if (this->is_off_) {
    ESP_LOGD(TAG, "Switch ON");
    this->command(CommandType::LIGHT_ON);
    this->is_off_ = false;
  }

  // Compute Corrected Brigtness / Warm Color Temperature (potentially reversed) as float: 0 -> 1
  float updated_brf = this->get_device_brightness(state->current_values.get_brightness());
  float updated_ctf = this->get_device_color_temperature(state->current_values.get_color_temperature());
  updated_ctf = this->get_parent()->is_reversed() ? 1.0 - updated_ctf : updated_ctf;

  // During transition(current / remote states are not the same), do not process change 
  //    if Brigtness / Color Temperature was not modified enough
  float br_diff = abs(this->brightness_ - updated_brf) * 100;
  float ct_diff = abs(this->warm_color_ - updated_ctf) * 100;
  bool is_last = (state->current_values == state->remote_values);
  if ((br_diff < 3 && ct_diff < 3 && !is_last) || (is_last && br_diff == 0 && ct_diff == 0)) {
    return;
  }
  
  this->brightness_ = updated_brf;
  this->warm_color_ = updated_ctf;

  if(!this->split_dim_cct_) {
    light::LightColorValues eff_values = state->current_values;
    eff_values.set_brightness(updated_brf);
    float cwf, wwf;
    if (this->get_parent()->is_reversed()) {
      eff_values.as_cwww(&wwf, &cwf, 0, this->constant_brightness_);
    } else {
      eff_values.as_cwww(&cwf, &wwf, 0, this->constant_brightness_);
    }
    ESP_LOGD(TAG, "Updating Cold: %.0f%%, Warm: %.0f%%", cwf*100, wwf*100);
    this->command(CommandType::LIGHT_WCOLOR, cwf, wwf);
  } else {
    if (ct_diff != 0) {
      ESP_LOGD(TAG, "Updating warm color temperature: %.0f%%", updated_ctf*100);
      this->command(CommandType::LIGHT_CCT, updated_ctf);
    }
    if (br_diff != 0) {
      ESP_LOGD(TAG, "Updating brightness: %.0f%%", updated_brf*100);
      this->command(CommandType::LIGHT_DIM, updated_brf);
    }
  }
}

/*********************
Secondary Light
**********************/

void BleAdvSecLight::dump_config() {
  ESP_LOGCONFIG(TAG, "BleAdvSecLight");
  BleAdvEntity::dump_config_base(TAG);
  ESP_LOGCONFIG(TAG, "  Base Light '%s'", this->state_->get_name().c_str());
}

void BleAdvSecLight::publish(const BleAdvGenCmd & gen_cmd) {
  if (!gen_cmd.is_sec_light_cmd()) return;

  light::LightCall call = this->state_->make_call();
  call.set_color_mode(light::ColorMode::ON_OFF);

  if (gen_cmd.cmd == CommandType::LIGHT_SEC_ON) {
    call.set_state(1.0f).perform();
  } else if (gen_cmd.cmd == CommandType::LIGHT_SEC_OFF) {
    call.set_state(0.0f).perform();
  } else if (gen_cmd.cmd == CommandType::LIGHT_SEC_TOGGLE) {
    bool binary;
    this->state_->current_values_as_binary(&binary);
    call.set_state(!binary).perform();
  }
}

void BleAdvSecLight::update_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);
  if (binary) {
    ESP_LOGD(TAG, "Secondary - Switch ON");
    this->command(CommandType::LIGHT_SEC_ON);
  } else {
    ESP_LOGD(TAG, "Secondary - Switch OFF");
    this->command(CommandType::LIGHT_SEC_OFF);
  }
}

} // namespace ble_adv_controller
} // namespace esphome

