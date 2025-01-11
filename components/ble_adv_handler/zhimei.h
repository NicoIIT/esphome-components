#pragma once

#include "ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

class ZhimeiEncoder: public BleAdvEncoder
{
public:
  ZhimeiEncoder(const std::string & encoding, const std::string & variant): 
      BleAdvEncoder(encoding, variant) {}
  
protected:
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const override;
};

class ZhimeiEncoderV0: public ZhimeiEncoder
{
public:
  ZhimeiEncoderV0(const std::string & encoding, const std::string & variant);
  
protected:
  static constexpr size_t ARGS_LEN = 3;
  struct data_map_t {
    uint8_t index;
    uint8_t tx_count;
    uint16_t id;
    uint8_t cmd;
    uint8_t args[ARGS_LEN];
    uint8_t checksum;
  }__attribute__((packed, aligned(1)));

  uint8_t checksum(uint8_t* buf, size_t len) const;
  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
};

class ZhimeiEncoderV1: public ZhimeiEncoder
{
public:
  ZhimeiEncoderV1(const std::string & encoding, const std::string & variant);
  
protected:
  static constexpr uint8_t MATRIX[16] = {29, 4, 17, 32, 152, 117, 40, 70, 11, 175, 67, 172, 214, 190, 137, 142};
  static constexpr size_t ARGS_LEN = 3;
  static constexpr size_t PAD_LEN = 6;
  struct data_map_t {
    uint8_t ff0;
    uint8_t seed;
    uint8_t tx_count;
    uint32_t id;
    uint8_t cmd;
    uint8_t index;
    uint8_t ff9;
    uint8_t tx2;
    uint8_t args[ARGS_LEN];
    uint16_t crc16;
    uint8_t padding[PAD_LEN];
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;

  void encrypt(uint8_t* buf, size_t len, uint8_t key) const;
  void decrypt(uint8_t* buf, size_t len, uint8_t key) const;

  uint16_t crc16(uint8_t* buf, size_t len) const;
};

class ZhimeiEncoderV2: public ZhimeiEncoder
{
public:
  ZhimeiEncoderV2(const std::string & encoding, const std::string & variant);
  
protected:
  static constexpr size_t PREFIX_LEN = 3;
  static constexpr uint8_t PREFIX[PREFIX_LEN] = {0x33, 0xAA, 0x55};

  static constexpr size_t TXDATA_LEN = 8;
  static constexpr size_t PAD_LEN = 10;
  struct data_map_t {
    uint8_t prefix[PREFIX_LEN];
    uint8_t txdata[TXDATA_LEN];
    uint16_t crc16;
    uint8_t padding[PAD_LEN];
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;

  uint16_t crc16(uint8_t* buf, size_t len) const;
};


} //namespace ble_adv_handler
} //namespace esphome
