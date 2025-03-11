#include "zhijia.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_handler {

std::string ZhijiaEncoder::to_str(const BleAdvEncCmd &enc_cmd) const {
  char ret[100];
  size_t ind =
      std::sprintf(ret, "0x%02X - args[%d,%d,%d]", enc_cmd.cmd, enc_cmd.args[0], enc_cmd.args[1], enc_cmd.args[2]);
  return ret;
}

uint16_t ZhijiaEncoder::crc16(uint8_t *buf, size_t len, uint16_t seed) const {
  return esphome::crc16(buf, len, seed, 0x8408, true, true);
}

// {0xAB, 0xCD, 0xEF} => 0xABCDEF
uint32_t ZhijiaEncoder::uuid_to_id(uint8_t *uuid, size_t len) const {
  uint32_t id = 0;
  for (size_t i = 0; i < len; ++i) {
    id |= uuid[len - i - 1] << (8 * i);
  }
  return id;
}

// 0xABCDEF => {0xAB, 0xCD, 0xEF}
void ZhijiaEncoder::id_to_uuid(uint8_t *uuid, uint32_t id, size_t len) const {
  for (size_t i = 0; i < len; ++i) {
    uuid[len - i - 1] = (id >> (8 * i)) & 0xFF;
  }
}

void ZhijiaEncoder::xor_all(uint8_t *buf, size_t len, uint16_t pivot) const {
  for (size_t i = 0; i < len; ++i) {
    buf[i] ^= pivot;
  }
}

ZhijiaEncoderV0::ZhijiaEncoderV0(const std::string &encoding, const std::string &variant, std::vector<uint8_t> &&mac)
    : ZhijiaEncoder(encoding, variant, mac) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderV0::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  this->whiten(buf, this->len_, 0x37);
  this->whiten(buf, this->len_, 0x7F);
  this->log_buffer(buf, this->len_, "Decoded");

  data_map_t *data = (data_map_t *) buf;
  uint16_t crc16 = this->crc16(buf, ADDR_LEN + TXDATA_LEN);
  if (!this->check_eq(crc16, data->crc16, "crc16"))
    return false;

  uint8_t addr[ADDR_LEN];
  this->reverse_all(buf, ADDR_LEN);
  std::reverse_copy(data->addr, data->addr + ADDR_LEN, addr);
  if (!this->check_eq_buf(this->mac_.data(), addr, ADDR_LEN, "Mac"))
    return false;

  cont.tx_count_ = data->txdata[0] ^ data->txdata[6];
  enc_cmd.args[0] = cont.tx_count_ ^ data->txdata[7];
  uint8_t pivot = data->txdata[1] ^ enc_cmd.args[0];
  uint8_t uuid[UUID_LEN];
  uuid[0] = pivot ^ data->txdata[0];
  uuid[1] = pivot ^ data->txdata[5];
  cont.id_ = this->uuid_to_id(uuid, UUID_LEN);
  cont.index_ = pivot ^ data->txdata[2];
  enc_cmd.cmd = pivot ^ data->txdata[4];
  enc_cmd.args[1] = pivot ^ data->txdata[3];
  enc_cmd.args[2] = uuid[0] ^ data->txdata[6];

  return true;
}

void ZhijiaEncoderV0::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  unsigned char uuid[UUID_LEN] = {0};
  this->id_to_uuid(uuid, cont.id_, UUID_LEN);

  data_map_t *data = (data_map_t *) buf;
  std::reverse_copy(this->mac_.begin(), this->mac_.end(), data->addr);
  this->reverse_all(data->addr, ADDR_LEN);

  uint8_t pivot = enc_cmd.args[2] ^ cont.tx_count_;
  data->txdata[0] = pivot ^ uuid[0];
  data->txdata[1] = pivot ^ enc_cmd.args[0];
  data->txdata[2] = pivot ^ cont.index_;
  data->txdata[3] = pivot ^ enc_cmd.args[1];
  data->txdata[4] = pivot ^ enc_cmd.cmd;
  data->txdata[5] = pivot ^ uuid[1];
  data->txdata[6] = enc_cmd.args[2] ^ uuid[0];
  data->txdata[7] = enc_cmd.args[0] ^ cont.tx_count_;

  data->crc16 = this->crc16(buf, ADDR_LEN + TXDATA_LEN);
  this->log_buffer(buf, this->len_, "Before encoding");
  this->whiten(buf, this->len_, 0x7F);
  this->whiten(buf, this->len_, 0x37);
}

ZhijiaEncoderV1::ZhijiaEncoderV1(const std::string &encoding, const std::string &variant, std::vector<uint8_t> &&mac,
                                 uint8_t uid_start)
    : ZhijiaEncoder(encoding, variant, mac), uid_start_(uid_start) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderV1::from_txdata(uint8_t *txdata, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  cont.tx_count_ = txdata[4];
  cont.index_ = txdata[6];
  enc_cmd.cmd = txdata[9];
  uint8_t addr[ADDR_LEN];
  addr[0] = txdata[7];
  addr[1] = txdata[10];
  addr[2] = txdata[13] ^ cont.tx_count_;
  enc_cmd.args[0] = txdata[0];
  enc_cmd.args[1] = txdata[3];
  enc_cmd.args[2] = txdata[5];
  uint8_t uuid[UUID_LEN];
  uuid[0] = txdata[2];
  uuid[1] = txdata[12] ^ uuid[0];
  uuid[2] = txdata[15] ^ enc_cmd.cmd;
  cont.id_ = this->uuid_to_id(uuid, UUID_LEN);

  return this->check_eq_buf(this->mac_.data() + this->uid_start_, addr, ADDR_LEN, "Mac");
}

void ZhijiaEncoderV1::to_txdata(uint8_t *txdata, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  unsigned char uuid[UUID_LEN] = {0};
  this->id_to_uuid(uuid, cont.id_, UUID_LEN);

  uint8_t addr[ADDR_LEN];
  std::copy(this->mac_.begin() + this->uid_start_, this->mac_.begin() + this->uid_start_ + ADDR_LEN, addr);

  uint8_t key = enc_cmd.cmd ^ enc_cmd.args[0] ^ enc_cmd.args[1] ^ enc_cmd.args[2];
  key ^= uuid[0] ^ uuid[1] ^ uuid[2] ^ cont.tx_count_ ^ cont.index_ ^ addr[0] ^ addr[1] ^ addr[2];

  txdata[0] = enc_cmd.args[0];
  txdata[1] = key;
  txdata[2] = uuid[0];
  txdata[3] = enc_cmd.args[1];
  txdata[4] = cont.tx_count_;
  txdata[5] = enc_cmd.args[2];
  txdata[6] = cont.index_;
  txdata[7] = addr[0];
  txdata[8] = 0x00;
  txdata[9] = enc_cmd.cmd;
  txdata[10] = addr[1];
  txdata[11] = 0x00;
  txdata[12] = uuid[1] ^ uuid[0];
  txdata[13] = addr[2] ^ cont.tx_count_;
  txdata[14] = 0x00;
  txdata[15] = uuid[2] ^ enc_cmd.cmd;
}

bool ZhijiaEncoderV1::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  this->whiten(buf, this->len_, 0x37);

  data_map_t *data = (data_map_t *) buf;
  uint16_t crc16 = this->crc16(buf, this->len_ - 2);
  if (!this->check_eq(crc16, data->crc16, "crc16"))
    return false;

  uint8_t mac[MAC_LEN];
  this->reverse_all(data->mac, MAC_LEN);
  std::reverse_copy(data->mac, data->mac + MAC_LEN, mac);
  if (!this->check_eq_buf(this->mac_.data(), mac, MAC_LEN, "Mac"))
    return false;

  this->xor_all(data->txdata, TXDATA_LEN, data->pivot);
  this->log_buffer(buf, this->len_, "Decoded");

  if (!this->from_txdata(data->txdata, enc_cmd, cont))
    return false;

  if (!this->check_eq(data->txdata[7], data->txdata[14], "Dupe 7/14"))
    return false;
  if (!this->check_eq(0x00, data->txdata[8], "8 as 0x00"))
    return false;
  if (!this->check_eq(0x00, data->txdata[11], "11 as 0x00"))
    return false;

  uint8_t re_pivot =
      data->txdata[2] ^ data->txdata[4] ^ data->txdata[9] ^ data->txdata[12] ^ data->txdata[13] ^ data->txdata[15];
  re_pivot ^= ((re_pivot & 1) - 1);
  if (!this->check_eq(re_pivot, data->pivot, "Pivot"))
    return false;

  return true;
}

void ZhijiaEncoderV1::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;
  std::reverse_copy(this->mac_.begin(), this->mac_.end(), data->mac);
  this->reverse_all(data->mac, MAC_LEN);

  this->to_txdata(data->txdata, enc_cmd, cont);
  data->txdata[14] = data->txdata[7];

  data->pivot =
      data->txdata[2] ^ data->txdata[4] ^ data->txdata[9] ^ data->txdata[12] ^ data->txdata[13] ^ data->txdata[15];
  data->pivot ^= (data->pivot & 1) - 1;

  this->log_buffer(buf, this->len_, "Before encoding");
  this->xor_all(data->txdata, TXDATA_LEN, data->pivot);
  data->crc16 = this->crc16(buf, this->len_ - 2);
  this->whiten(buf, this->len_, 0x37);
}

ZhijiaEncoderV2::ZhijiaEncoderV2(const std::string &encoding, const std::string &variant, std::vector<uint8_t> &&mac)
    : ZhijiaEncoderV1(encoding, variant, std::move(mac)) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderV2::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  this->whiten(buf, this->len_, 0x6F);
  this->whiten(buf, this->len_ - 2, 0xD3);

  data_map_t *data = (data_map_t *) buf;
  this->xor_all(data->txdata, TXDATA_LEN, data->pivot);
  this->log_buffer(buf, this->len_, "Decoded");

  if (!this->from_txdata(data->txdata, enc_cmd, cont))
    return false;

  // uint8_t key = addr[0] ^ addr[1] ^ addr[2] ^ cont.index_ ^ cont.tx_count_ ^ enc_cmd.args[0] ^ enc_cmd.args[1] ^
  // enc_cmd.args[2] ^ uuid[0] ^ uuid[1] ^ uuid[2]; ENSURE_EQ(key, data->txdata[1], "Decoded KO (Key)");

  uint8_t re_pivot = data->txdata[3] ^ data->txdata[7] ^ data->txdata[12] ^ data->txdata[13] ^ data->txdata[15];
  re_pivot = ((re_pivot & 1) - 1) ^ re_pivot;
  if (!this->check_eq(re_pivot, data->pivot, "Pivot"))
    return false;

  if (!this->check_eq(data->txdata[2] ^ data->txdata[3] ^ data->txdata[4] ^ data->txdata[7], data->txdata[8],
                      "txdata 8"))
    return false;
  if (!this->check_eq(0x00, data->txdata[11], "txdata 11"))
    return false;
  if (!this->check_eq(data->txdata[2] ^ data->txdata[3] ^ data->txdata[4] ^ data->txdata[9], data->txdata[14],
                      "txdata 14"))
    return false;

  return true;
}

void ZhijiaEncoderV2::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;
  this->to_txdata(data->txdata, enc_cmd, cont);
  data->txdata[1] ^= data->txdata[9];
  data->txdata[8] = data->txdata[2] ^ data->txdata[3] ^ data->txdata[4] ^ data->txdata[7];
  data->txdata[14] = data->txdata[2] ^ data->txdata[3] ^ data->txdata[4] ^ data->txdata[9];

  data->pivot = data->txdata[3] ^ data->txdata[7] ^ data->txdata[12] ^ data->txdata[13] ^ data->txdata[15];
  data->pivot = ((data->pivot & 1) - 1) ^ data->pivot;

  this->log_buffer(buf, this->len_, "Before encoding");
  this->xor_all(data->txdata, TXDATA_LEN, data->pivot);
  this->whiten(buf, this->len_ - 2, 0xD3);
  this->whiten(buf, this->len_, 0x6F);
}

ZhijiaEncoderRemote::ZhijiaEncoderRemote(const std::string &encoding, const std::string &variant,
                                         std::vector<uint8_t> &&mac)
    : ZhijiaEncoderV1(encoding, variant, std::move(mac)) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderRemote::decode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;

  // workaround for pivot: at pos 5 is arg2 which is always 0, so effective pivot has this value
  uint8_t eff_pivot = data->txdata[5];
  this->xor_all(data->txdata, TXDATA_LEN, eff_pivot);
  this->log_buffer(buf, this->len_, "Decoded");

  std::string decoded = esphome::format_hex_pretty(buf, this->len_);
  ESP_LOGD(this->id_.c_str(), "Decoded  - %s", decoded.c_str());

  if (!this->from_txdata(data->txdata, enc_cmd, cont))
    return false;

  if (!this->check_eq(0x01, data->txdata[8], "txdata 8"))
    return false;
  if (!this->check_eq(0x02, data->txdata[11], "txdata 11"))
    return false;
  if (!this->check_eq(data->txdata[2], data->txdata[14], "txdata 14"))
    return false;

  // Attempt to have more info so that we could deduce more effisciently the encoding
  if ((data->pivot ^ 0x06) != eff_pivot) {
    ESP_LOGE(this->id_.c_str(), "Pivot different than expected, please open an issue to component owner.");
  }

  return true;
}

void ZhijiaEncoderRemote::encode(uint8_t *buf, BleAdvEncCmd &enc_cmd, ControllerParam_t &cont) const {
  data_map_t *data = (data_map_t *) buf;

  this->to_txdata(data->txdata, enc_cmd, cont);
  data->txdata[1] ^= 0x04;
  data->txdata[8] = 0x01;
  data->txdata[11] = 0x02;
  data->txdata[14] = data->txdata[2];

  // not sure at all of this...
  data->pivot = 0xC9;

  this->log_buffer(buf, this->len_, "Before encoding");
  this->xor_all(data->txdata, TXDATA_LEN, data->pivot ^ 0x06);
}

}  // namespace ble_adv_handler
}  // namespace esphome
