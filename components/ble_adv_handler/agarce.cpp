#include "agarce.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_handler {

// ref: https://github.com/NicoIIT/esphome-components/issues/17
AgarceEncoder::AgarceEncoder(const std::string &encoding, const std::string &variant, uint8_t prefix)
    : BleAdvEncoder(encoding, variant), prefix_(prefix) {
  this->len_ = sizeof(data_map_t);
}

std::string AgarceEncoder::to_str(const BleAdvEncCmd &enc_cmd) const {
  char ret[100];
  size_t ind =
      std::sprintf(ret, "0x%02X - args[%d,%d,%d]", enc_cmd.cmd, enc_cmd.args[0], enc_cmd.args[1], enc_cmd.args[2]);
  return ret;
}

void AgarceEncoder::crypt(uint8_t *buf, size_t len, uint16_t seed) const {
  static constexpr uint8_t MATRIX[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x5A, 0xA5, 0xA5, 0x5A};
  uint8_t pivot0 = seed & 0xFF;
  uint8_t pivot1 = seed >> 8;
  for (size_t i = 0; i < len; ++i) {
    buf[i] = buf[i] ^ MATRIX[i % 8] ^ (((i + 1) / 2 % 2 == 0) ? pivot0 : pivot1);
  }
}

bool AgarceEncoder::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) (buf);

  if (!this->check_eq(this->checksum(buf, this->len_ - 1), data->checksum2, "Checksum 2"))
    return false;
  this->crypt(buf + 3, this->len_ - 4, data->seed);
  this->log_buffer(buf, this->len_, "Decoded");

  if (!this->check_eq(this->checksum(buf + 3, this->len_ - 5), data->checksum, "Checksum"))
    return false;

  enc_cmd.cmd = data->tx0 & 0xF0;
  // Exclude Group Commands, ref https://github.com/NicoIIT/esphome-components/issues/17#issuecomment-2597871821
  if ((enc_cmd.cmd == 0x00) && (data->args[1] == 0x00))
    return false;
  // Exclude wrong prefix
  if ((enc_cmd.cmd != 0x00) && data->prefix != this->prefix_)
    return false;
  if ((enc_cmd.cmd == 0x00) && data->prefix != (this->prefix_ & 0x0F))
    return false;

  std::copy(data->args, data->args + ARGS_LEN, enc_cmd.args);
  cont.index_ = (data->tx4 & 0x0F) << 4;
  if (enc_cmd.cmd == 0x00) {
    cont.index_ |= data->args[2];
  } else {
    cont.index_ |= data->tx0 & 0x0F;
  }

  cont.tx_count_ = data->tx_count;
  cont.app_restart_count_ = data->app_restart_count;
  cont.id_ = data->id;
  cont.seed_ = data->seed;

  return true;
}

void AgarceEncoder::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) (buf);
  data->tx0 = enc_cmd.cmd;
  data->tx4 = (cont.index_ >> 4) & 0x0F;
  std::copy(enc_cmd.args, enc_cmd.args + ARGS_LEN, data->args);
  if (enc_cmd.cmd == 0x00) {
    data->args[1] = (this->prefix_ >> 4) & 0x0F;
    data->args[2] = cont.index_ & 0x0F;
    data->tx4 |= 0xC0;
    data->prefix = this->prefix_ & 0x0F;
  } else {
    data->tx0 |= cont.index_ & 0x0F;
    data->prefix = this->prefix_;
  }

  data->tx_count = cont.tx_count_;
  data->id = cont.id_;
  data->rem_seq = 0x1000;  // ??? 0x5000 in some captured msgs
  data->app_restart_count = cont.app_restart_count_;
  data->seed = (cont.seed_ == 0) ? (uint16_t) rand() % 0xFFFF : cont.seed_;
  data->checksum = this->checksum(buf + 3, this->len_ - 5);

  this->log_buffer(buf, this->len_, "Before encoding");
  this->crypt(buf + 3, this->len_ - 4, data->seed);
  data->checksum2 = this->checksum(buf, this->len_ - 1);
}

}  // namespace ble_adv_handler
}  // namespace esphome
