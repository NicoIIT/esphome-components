#include "remotes.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_handler {

RemoteEncoder::RemoteEncoder(const std::string &encoding, const std::string &variant)
    : BleAdvEncoder(encoding, variant) {
  this->len_ = sizeof(data_map_t);
}

std::string RemoteEncoder::to_str(const BleAdvEncCmd &enc_cmd) const {
  char ret[100];
  size_t ind = std::sprintf(ret, "0x%02X", enc_cmd.cmd);
  if (enc_cmd.args[0] != 0) {
    ind += std::sprintf(ret + ind, " - %d t.u.", enc_cmd.args[0]);
  }
  if (enc_cmd.args[1] != 0) {
    ind += std::sprintf(ret + ind, " - %s", (enc_cmd.args[1] == 0x40) ? "HOLD" : "RELEASE");
  }
  return ret;
}

bool RemoteEncoder::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) (buf);
  this->log_buffer(buf, this->len_, "Decoded");
  if (!this->check_eq(this->checksum(buf, this->len_ - 1), data->checksum, "Checksum"))
    return false;
  cont.tx_count_ = data->tx_count;
  cont.id_ = data->identifier;
  enc_cmd.cmd = data->cmd & 0x3F;
  enc_cmd.args[0] = data->press_count;
  enc_cmd.args[1] = data->cmd & 0xC0;
  return true;
}

void RemoteEncoder::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) (buf);
  data->tx_count = cont.tx_count_;
  data->identifier = cont.id_;
  data->cmd = enc_cmd.cmd | enc_cmd.args[1];
  data->press_count = enc_cmd.args[0];
  data->checksum = this->checksum(buf, this->len_ - 1);
  this->log_buffer(buf, this->len_, "Before encoding");
}

}  // namespace ble_adv_handler
}  // namespace esphome
