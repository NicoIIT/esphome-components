#pragma once

#include "ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

class FanLampEncoder: public BleAdvEncoder
{
public:
  FanLampEncoder(const std::string & encoding, const std::string & variant, const std::vector<uint8_t> & prefix);

protected:

  uint16_t get_seed(uint16_t forced_seed = 0) const;
  uint16_t crc16(uint8_t* buf, size_t len, uint16_t seed) const;

  std::vector<uint8_t> prefix_;
};

class FanLampEncoderV1: public FanLampEncoder
{
public:
  FanLampEncoderV1(const std::string & encoding, const std::string & variant,
                    uint8_t pair_arg3, bool pair_arg_only_on_pair = true, bool xor1 = false, uint8_t supp_prefix = 0x00);

protected:
  static constexpr size_t ARGS_LEN = 3;
  struct data_map_t {
    uint8_t cmd;
    uint16_t group_index;
    uint8_t args[ARGS_LEN];
    uint8_t tx_count;
    uint8_t param1;
    uint8_t src;
    uint8_t r2;
    uint16_t seed;
    uint16_t crc16;
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const override;

  uint8_t pair_arg3_;
  bool pair_arg_only_on_pair_;
  bool with_crc2_;
  bool xor1_;
};

class FanLampEncoderV2: public FanLampEncoder
{
public:
  FanLampEncoderV2(const std::string & encoding, const std::string & variant, const std::vector<uint8_t> && prefix, uint16_t device_type, bool with_sign);

protected:
  static constexpr size_t ARGS_LEN = 2;
  struct data_map_t {
    uint8_t tx_count;
    uint16_t type;
    uint32_t identifier;
    uint8_t group_index;
    uint16_t cmd;
    uint8_t param0; // unused
    uint8_t param1;
    uint8_t args[ARGS_LEN];
    uint16_t sign;
    uint8_t spare; // unused
    uint16_t seed;
    uint16_t crc16;
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const override;

  uint16_t sign(uint8_t* buf, uint8_t tx_count, uint16_t seed) const;
  void whiten(uint8_t *buf, uint8_t size, uint8_t seed, uint8_t salt = 0) const;

  uint16_t device_type_;
  bool with_sign_;
};

} //namespace ble_adv_handler
} //namespace esphome
