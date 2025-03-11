#include "zhimei.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_handler {

std::string ZhimeiEncoder::to_str(const BleAdvEncCmd &enc_cmd) const {
  char ret[100];
  size_t ind =
      std::sprintf(ret, "0x%02X - args[%d,%d,%d]", enc_cmd.cmd, enc_cmd.args[0], enc_cmd.args[1], enc_cmd.args[2]);
  return ret;
}

ZhimeiEncoderV0::ZhimeiEncoderV0(const std::string &encoding, const std::string &variant)
    : ZhimeiEncoder(encoding, variant) {
  this->len_ = sizeof(data_map_t);
}

uint8_t ZhimeiEncoderV0::checksum(uint8_t *buf, size_t len) const {
  uint8_t cec = 0;
  for (size_t i = 0; i < this->header_.size(); ++i) {
    cec += this->header_[i];
  }
  for (size_t i = 0; i < len; ++i) {
    cec += buf[i];
  }
  return cec;
}

bool ZhimeiEncoderV0::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;
  this->log_buffer(buf, this->len_, "Decoded");
  if (!this->check_eq(this->checksum(buf, this->len_ - 1), data->checksum, "Checksum"))
    return false;
  enc_cmd.cmd = data->cmd;
  enc_cmd.args[0] = data->args[0];
  enc_cmd.args[1] = data->args[1];
  enc_cmd.args[2] = data->args[2];

  cont.tx_count_ = data->tx_count;
  cont.index_ = data->index;
  cont.id_ = data->id;

  return true;
}

void ZhimeiEncoderV0::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;
  data->cmd = enc_cmd.cmd;
  data->args[0] = enc_cmd.args[0];
  data->args[1] = enc_cmd.args[1];
  data->args[2] = enc_cmd.args[2];

  data->tx_count = cont.tx_count_;
  data->index = cont.index_;
  data->id = cont.id_ & 0xFFFF;
  data->checksum = this->checksum(buf, this->len_ - 1);
  this->log_buffer(buf, this->len_, "Before encoding");
}

constexpr uint8_t ZhimeiEncoderV1::MATRIX[];

ZhimeiEncoderV1::ZhimeiEncoderV1(const std::string &encoding, const std::string &variant)
    : ZhimeiEncoder(encoding, variant) {
  this->len_ = sizeof(data_map_t);
}

uint16_t ZhimeiEncoderV1::crc16(uint8_t *buf, size_t len) const { return esphome::crc16be(buf, len, 0); }

void ZhimeiEncoderV1::encrypt(uint8_t *buf, size_t len, uint8_t key) const {
  uint8_t pivot = MATRIX[((buf[1] >> 4) & 15) ^ (buf[1] & 15)];
  for (size_t i = 0; i < len; ++i) {
    buf[i] = (((buf[i] ^ pivot) + MATRIX[(key + i) & 0xF]) + 256) % 256;
  }
}

void ZhimeiEncoderV1::decrypt(uint8_t *buf, size_t len, uint8_t key) const {
  uint8_t pivot = (buf[0] - MATRIX[key & 0xF]) ^ 0xFF;
  for (size_t i = 0; i < len; ++i) {
    buf[i] = ((buf[i] - MATRIX[(key + i) & 0xF] + 256) % 256) ^ pivot;
  }
}

bool ZhimeiEncoderV1::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  uint8_t data_len = this->len_ - PAD_LEN;
  this->decrypt(buf, data_len, 6);

  uint16_t crc16 = this->crc16(buf, data_len - 3);

  data_map_t *data = (data_map_t *) buf;
  if (data->cmd != 0xB4) {
    this->decrypt(buf + 9, 5, 10);
    if (!this->check_eq(data->tx_count, data->tx2, "Dupe 2/10"))
      return false;
  }
  this->log_buffer(buf, this->len_, "Decoded");

  if (!this->check_eq(crc16, data->crc16, "crc16"))
    return false;
  if (!this->check_eq(0xFF, data->ff0, "0 not FF"))
    return false;
  if (!this->check_eq(0xFF, data->ff9, "9 not FF"))
    return false;

  for (size_t i = 0; i < PAD_LEN; ++i) {
    if (!this->check_eq(this->len_ - PAD_LEN + i, data->padding[i], "PADDING"))
      return false;
  }

  enc_cmd.cmd = data->cmd;
  std::copy(data->args, data->args + ARGS_LEN, enc_cmd.args);

  cont.tx_count_ = data->tx_count;
  cont.index_ = data->index;
  cont.id_ = data->id;
  cont.seed_ = data->seed;

  return true;
}

void ZhimeiEncoderV1::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;
  data->cmd = enc_cmd.cmd;
  std::copy(enc_cmd.args, enc_cmd.args + ARGS_LEN, data->args);

  data->tx_count = cont.tx_count_;
  data->index = cont.index_;
  data->id = cont.id_ & 0xFFFF;
  data->seed = cont.seed_ & 0xFF;

  data->ff0 = 0xFF;
  data->ff9 = 0xFF;
  data->tx2 = data->tx_count;
  for (size_t i = 0; i < PAD_LEN; ++i) {
    data->padding[i] = this->len_ - PAD_LEN + i;
  }

  uint8_t data_len = this->len_ - PAD_LEN;
  if (enc_cmd.cmd != 0xB4) {
    this->encrypt(buf + 9, 5, 10);
  }
  data->crc16 = this->crc16(buf, data_len - 3);
  this->log_buffer(buf, this->len_, "Before encoding");
  this->encrypt(buf, data_len, 6);
}

constexpr uint8_t ZhimeiEncoderV2::PREFIX[];

ZhimeiEncoderV2::ZhimeiEncoderV2(const std::string &encoding, const std::string &variant)
    : ZhimeiEncoder(encoding, variant) {
  this->len_ = sizeof(data_map_t);
}

uint16_t ZhimeiEncoderV2::crc16(uint8_t *buf, size_t len) const {
  this->reverse_all(buf, len);
  uint16_t pre_cec = esphome::crc16be(buf, len, 0xFFFF);
  this->reverse_all(buf, len);
  return 0xFFFF ^ (((this->reverse_byte(pre_cec & 0xFF) << 8) & 0xFF00) | (this->reverse_byte(pre_cec >> 8) & 0xFF));
}

bool ZhimeiEncoderV2::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  this->whiten(buf, this->len_ - PAD_LEN, 0x48);
  this->log_buffer(buf, this->len_, "Decoded");

  if (!this->check_eq_buf(PREFIX, buf, PREFIX_LEN, "Prefix"))
    return false;

  data_map_t *data = (data_map_t *) buf;

  uint16_t crc16 = this->crc16(buf, this->len_ - PAD_LEN - 2);
  if (!this->check_eq(crc16, data->crc16, "crc16"))
    return false;

  for (size_t i = 0; i < PAD_LEN; ++i) {
    if (!this->check_eq(this->len_ - PAD_LEN + i + 3, data->padding[i], "PADDING"))
      return false;
  }

  uint8_t pivot = data->txdata[0] ^ data->txdata[1] ^ data->txdata[6] ^ data->txdata[7];

  enc_cmd.cmd = data->txdata[4] ^ pivot;
  enc_cmd.args[0] = data->txdata[1] ^ pivot;
  enc_cmd.args[1] = data->txdata[3] ^ pivot;
  enc_cmd.args[2] = data->txdata[6] ^ data->txdata[0] ^ pivot;

  cont.tx_count_ = data->txdata[7] ^ data->txdata[1] ^ pivot;
  cont.index_ = data->txdata[2] ^ pivot;
  cont.id_ = (data->txdata[5] ^ pivot) << 8 | (data->txdata[0] ^ pivot);

  return true;
}

void ZhimeiEncoderV2::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;

  uint16_t pivot = enc_cmd.args[2] ^ cont.tx_count_;
  data->txdata[0] = (cont.id_ & 0xFF) ^ pivot;
  data->txdata[1] = enc_cmd.args[0] ^ pivot;
  data->txdata[2] = cont.index_ ^ pivot;
  data->txdata[3] = enc_cmd.args[1] ^ pivot;
  data->txdata[4] = enc_cmd.cmd ^ pivot;
  data->txdata[5] = (cont.id_ >> 8) ^ pivot;
  data->txdata[6] = enc_cmd.args[2] ^ (cont.id_ & 0xFF);
  data->txdata[7] = enc_cmd.args[0] ^ cont.tx_count_;

  for (size_t i = 0; i < PAD_LEN; ++i) {
    data->padding[i] = this->len_ - PAD_LEN + i + 3;
  }

  std::copy(PREFIX, PREFIX + PREFIX_LEN, data->prefix);

  data->crc16 = this->crc16(buf, this->len_ - PAD_LEN - 2);
  this->log_buffer(buf, this->len_, "Before encoding");
  this->whiten(buf, this->len_ - PAD_LEN, 0x48);
}

}  // namespace ble_adv_handler
}  // namespace esphome
