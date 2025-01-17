#pragma once

#include "ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

class AgarceEncoder: public BleAdvEncoder
{
public:
  AgarceEncoder(const std::string & encoding, const std::string & variant, uint8_t prefix);

protected:
  static constexpr size_t ARGS_LEN = 3;
  struct data_map_t {
    uint8_t prefix; // 0x03, 0x04, 0x83, 0x84
    uint16_t seed;
    uint8_t tx_count;
    uint8_t app_restart_count;
    uint16_t rem_seq; // 0x1000 / 0x5000
    uint32_t id;
    uint8_t tx0;
    uint8_t args[ARGS_LEN];
    uint8_t tx4;
    uint8_t checksum;
    uint8_t checksum2;
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const override;

  void crypt(uint8_t* buf, size_t len, uint16_t seed) const;
  uint8_t prefix_;
};

} //namespace ble_adv_handler
} //namespace esphome
