#pragma once

#include "ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

class ZhijiaEncoder: public BleAdvEncoder
{
public:
  ZhijiaEncoder(const std::string & encoding, const std::string & variant, std::vector< uint8_t > & mac): 
      BleAdvEncoder(encoding, variant), mac_(mac) {}
  
protected:
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const override;

  uint16_t crc16(uint8_t* buf, size_t len, uint16_t seed = 0) const;
  uint32_t uuid_to_id(uint8_t * uuid, size_t len) const;
  void id_to_uuid(uint8_t * uuid, uint32_t id, size_t len) const;
  
  std::vector< uint8_t > mac_;
};

class ZhijiaEncoderV0: public ZhijiaEncoder
{
public:
  ZhijiaEncoderV0(const std::string & encoding, const std::string & variant, std::vector< uint8_t > && mac);
  
protected:
  static constexpr size_t UUID_LEN = 2;
  static constexpr size_t ADDR_LEN = 3;
  static constexpr size_t TXDATA_LEN = 8;
  struct data_map_t {
    uint8_t addr[ADDR_LEN];
    uint8_t txdata[TXDATA_LEN];
    uint16_t crc16;
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
};

class ZhijiaEncoderV1: public ZhijiaEncoder
{
public:
  ZhijiaEncoderV1(const std::string & encoding, const std::string & variant, std::vector< uint8_t > && mac, uint8_t uid_start = 0);
  
protected:
  static constexpr size_t UID_LEN = 3;
  static constexpr size_t UUID_LEN = 3;
  static constexpr size_t ADDR_LEN = 4;
  static constexpr size_t TXDATA_LEN = 17;
  struct data_map_t {
    uint8_t addr[ADDR_LEN];
    uint8_t txdata[TXDATA_LEN];
    uint16_t crc16;
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;

  uint8_t uid_start_;
};

class ZhijiaEncoderV2: public ZhijiaEncoderV1
{
public:
  ZhijiaEncoderV2(const std::string & encoding, const std::string & variant, std::vector< uint8_t > && mac);
  
protected:
  static constexpr size_t UUID_LEN = 3;
  static constexpr size_t ADDR_LEN = 3;
  static constexpr size_t TXDATA_LEN = 16;
  static constexpr size_t SPARE_LEN = 7;
  struct data_map_t {
    uint8_t txdata[TXDATA_LEN];
    uint8_t pivot;
    uint8_t spare[SPARE_LEN];
  }__attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const override;
};


} //namespace ble_adv_handler
} //namespace esphome
