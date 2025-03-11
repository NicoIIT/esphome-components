#pragma once

#include "esphome/components/light/light_output.h"
#include "../ble_adv_controller.h"

namespace esphome {
namespace ble_adv_controller {

class BleAdvLightBase : public light::LightOutput, public light::LightState, public BleAdvEntity {
 public:
  BleAdvLightBase() : light::LightState(this), BleAdvEntity(EntityType::LIGHT) {}
  void setup() override { light::LightState::setup(); };
  void dump_config() override;
  void write_state(light::LightState *state) override {};
  light::LightTraits get_traits() override { return this->traits_; }

  virtual void publish(const BleAdvGenCmd &gen_cmd) override final;
  void update_state(light::LightState *state) override final;

 protected:
  virtual void control() {}
  virtual void publish_impl(const BleAdvGenCmd &gen_cmd) {}

  light::LightTraits traits_;
  bool is_off_{true};
};

class BleAdvLightCww : public BleAdvLightBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_traits(float cold_white_temperature, float warm_white_temperature);
  void set_constant_brightness(bool constant_brightness) { this->constant_brightness_ = constant_brightness; }
  void set_min_brightness(int min_brightness, int min, int max, int step);

  float get_min_brightness() { return ((float) this->number_min_brightness_.state) / 100.0f; }

  float get_ha_brightness(float device_brightness);
  float get_device_brightness(float ha_brightness);
  float get_ha_color_temperature(float device_color_temperature);
  float get_device_color_temperature(float ha_color_temperature);

 protected:
  void control() override;
  void publish_impl(const BleAdvGenCmd &gen_cmd) override;

  bool constant_brightness_;
  ble_adv_handler::BleAdvNumber number_min_brightness_;

  float brightness_{0};
  float warm_color_{0};
};

class BleAdvLightBinary : public BleAdvLightBase {
 public:
  void set_traits() { this->traits_.set_supported_color_modes({light::ColorMode::ON_OFF}); };
  void dump_config() override;
};

class BleAdvLightRGB : public BleAdvLightBase {
 public:
  void set_traits() { this->traits_.set_supported_color_modes({light::ColorMode::RGB}); };
  void dump_config() override;

 protected:
  void control() override;
  void publish_impl(const BleAdvGenCmd &gen_cmd) override;

  float brightness_{0};
  float red_{0};
  float green_{0};
  float blue_{0};
};

}  // namespace ble_adv_controller
}  // namespace esphome
