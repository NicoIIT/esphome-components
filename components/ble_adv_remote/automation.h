#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_adv_remote/ble_adv_remote.h"

namespace esphome {
namespace ble_adv_remote {

class BleAdvCmdTrigger : public BleAdvBaseTrigger {
public:
  explicit BleAdvCmdTrigger(BleAdvRemote *parent) { parent->register_trigger(this); }
};

template<typename... Ts> class BaseRemoteAction : public Action<Ts...> {
public:
  void set_remote(BleAdvRemote * remote) { this->remote_ = remote; }
  BleAdvRemote * get_remote() { return this->remote_; }
protected:
  BleAdvRemote * remote_;
};

template<typename... Ts> class UnPairAction : public BaseRemoteAction<Ts...> {
public:
  void play(Ts... x) override { this->remote_->unpair(); }
};

}
}