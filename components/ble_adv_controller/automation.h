#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_adv_controller/ble_adv_controller.h"
#include <vector>

namespace esphome {
namespace ble_adv_controller {

template<typename... Ts> class BaseControllerAction : public Action<Ts...> {
public:
  void set_controller(BleAdvController * controller) { this->controller_ = controller; }
  BleAdvController * get_controller() { return this->controller_; }
protected:
  BleAdvController * controller_;
};

template<typename... Ts> class CancelTimerAction : public BaseControllerAction<Ts...> {
public:
  void play(Ts... x) override { this->controller_->cancel_timer(); }
};

template<typename... Ts> class PairAction : public BaseControllerAction<Ts...> {
public:
  void play(Ts... x) override { this->controller_->pair(); }
};

template<typename... Ts> class UnPairAction : public BaseControllerAction<Ts...> {
public:
  void play(Ts... x) override { this->controller_->unpair(); }
};

template<typename... Ts> class CustomCmdAction : public BaseControllerAction<Ts...> {
public:
  TEMPLATABLE_VALUE(uint8_t, cmd)
  TEMPLATABLE_VALUE(uint8_t, param1)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, args)

  // needed in order to support initializer from python list
  void set_args(const std::vector<uint8_t> & args) { this->args_ = args; }
  
  void play(Ts... x) override { 
    BleAdvEncCmd enc_cmd(this->cmd_.value(x...));
    enc_cmd.param1 = this->param1_.value(x...);
    std::vector<uint8_t> args = this->args_.value(x...);
    std::copy(args.begin(), args.end(), enc_cmd.args);
    this->controller_->custom_cmd(enc_cmd);
  }
};

template<typename... Ts> class RawInjectAction : public BaseControllerAction<Ts...> {
public:
  TEMPLATABLE_VALUE(std::string, raw)
  void play(Ts... x) override { this->controller_->raw_inject(this->raw_.value(x...)); }
};

}
}