#pragma once

#include "ble_adv_handler.h"

namespace esphome {
namespace ble_adv_handler {

// Exception as this translator is really complex: hard code it
class BleAdvTranslator_agarce_base : public BleAdvTranslator_base {
 public:
  bool g2e_cmd(const BleAdvGenCmd &g, BleAdvEncCmd &e) const override {
    if ((g.cmd == CommandType::FAN_FULL) && (g.ent_type == EntityType::FAN) && (g.ent_index == 0)) {
      e.cmd = 0x80;
      e.args[0] = ((g.args[0] > 0) ? 0x80 : 0x00) | ((int) g.args[0]) | (g.args[1] ? 0x10 : 0x00);
      e.args[1] = g.args[2];
      e.args[2] = (g.param & FanSubCmdType::SPEED ? 0x01 : 0x00);
      e.args[2] |= (g.param & FanSubCmdType::DIR ? 0x02 : 0);
      e.args[2] |= (g.param & FanSubCmdType::STATE ? 0x08 : 0);
      e.args[2] |= (g.param & FanSubCmdType::OSC ? 0x10 : 0);
      return true;
    }
    return BleAdvTranslator_base::g2e_cmd(g, e);
  }
  bool e2g_cmd(const BleAdvEncCmd &e, BleAdvGenCmd &g) const override {
    if ((e.cmd == 0x80)) {
      g.cmd = CommandType::FAN_FULL;
      g.ent_type = EntityType::FAN;
      g.ent_index = 0;
      g.args[0] = (e.args[0] & 0x80) ? e.args[0] & 0x0F : 0;
      g.args[1] = (e.args[0] & 0x10) > 0;
      g.args[2] = e.args[1];
      g.param = (e.args[2] & 0x01 ? FanSubCmdType::SPEED : 0);
      g.param |= (e.args[2] & 0x02 ? FanSubCmdType::DIR : 0);
      g.param |= (e.args[2] & 0x08 ? FanSubCmdType::STATE : 0);
      g.param |= (e.args[2] & 0x10 ? FanSubCmdType::OSC : 0);
      return true;
    }
    return BleAdvTranslator_base::e2g_cmd(e, g);
  }
};

class AgarceEncoder : public BleAdvEncoder {
 public:
  AgarceEncoder(const std::string &encoding, const std::string &variant, uint8_t prefix);

 protected:
  static constexpr size_t ARGS_LEN = 3;
  struct data_map_t {
    uint8_t prefix;  // 0x03, 0x04, 0x83, 0x84
    uint16_t seed;
    uint8_t tx_count;
    uint8_t app_restart_count;
    uint16_t rem_seq;  // 0x1000 / 0x5000
    uint32_t id;
    uint8_t tx0;
    uint8_t args[ARGS_LEN];
    uint8_t tx4;
    uint8_t checksum;
    uint8_t checksum2;
  } __attribute__((packed, aligned(1)));

  virtual bool decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const override;
  virtual void encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const override;
  virtual std::string to_str(const BleAdvEncCmd &enc_cmd) const override;

  void crypt(uint8_t *buf, size_t len, uint16_t seed) const;
  uint8_t prefix_;
};

}  // namespace ble_adv_handler
}  // namespace esphome
