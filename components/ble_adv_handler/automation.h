#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_adv_handler/ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

class BleAdvDecodedTrigger : public BleAdvBaseDecodedTrigger {
public:
  explicit BleAdvDecodedTrigger(BleAdvHandler *parent) { parent->register_decoded_trigger(this); }
};

class BleAdvRawTrigger : public BleAdvBaseRawTrigger {
public:
  explicit BleAdvRawTrigger(BleAdvHandler *parent) { parent->register_raw_trigger(this); }
};

}
}