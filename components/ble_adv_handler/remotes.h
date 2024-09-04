#pragma once

#include "ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

class RemoteEncoder: public BleAdvEncoder
{
public:
  RemoteEncoder(const std::string & encoding, const std::string & variant);

protected:
  struct data_map_t {
    uint8_t press_count;
    uint32_t identifier;
    uint8_t cmd;
    uint8_t tx_count;
    uint8_t checksum;
  }__attribute__((packed, aligned(1)));

  uint8_t checksum(uint8_t * buf, size_t len) const;

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const override;
};

} //namespace ble_adv_handler
} //namespace esphome
