#include "zhijia.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_adv_handler {

std::string ZhijiaEncoder::to_str(const BleAdvEncCmd & enc_cmd) const {
  char ret[100];
  size_t ind = std::sprintf(ret, "0x%02X - args[%d,%d,%d]", enc_cmd.cmd, enc_cmd.args[0], enc_cmd.args[1], enc_cmd.args[2]);
  return ret;
}

uint16_t ZhijiaEncoder::crc16(uint8_t* buf, size_t len, uint16_t seed) const {
  return esphome::crc16(buf, len, seed, 0x8408, true, true);
}

// {0xAB, 0xCD, 0xEF} => 0xABCDEF
uint32_t ZhijiaEncoder::uuid_to_id(uint8_t * uuid, size_t len) const {
  uint32_t id = 0;
  for (size_t i = 0; i < len; ++i) {
    id |= uuid[len - i - 1] << (8*i);
  }
  return id;
}

// 0xABCDEF => {0xAB, 0xCD, 0xEF}
void ZhijiaEncoder::id_to_uuid(uint8_t * uuid, uint32_t id, size_t len) const {
  for (size_t i = 0; i < len; ++i) {
    uuid[len - i - 1] = (id >> (8*i)) & 0xFF;
  }
}

ZhijiaEncoderV0::ZhijiaEncoderV0(const std::string & encoding, const std::string & variant, std::vector< uint8_t > && mac): 
  ZhijiaEncoder(encoding, variant, mac) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderV0::decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  this->whiten(buf, this->len_, 0x37);
  this->whiten(buf, this->len_, 0x7F);

  data_map_t * data = (data_map_t *) buf;
  uint16_t crc16 = this->crc16(buf, ADDR_LEN + TXDATA_LEN);
  ENSURE_EQ(crc16, data->crc16, "Decoded KO (CRC)");

  uint8_t addr[ADDR_LEN];
  this->reverse_all(buf, ADDR_LEN);
  std::reverse_copy(data->addr, data->addr + ADDR_LEN, addr);
  if (!std::equal(addr, addr + ADDR_LEN, this->mac_.begin())) return false;

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

void ZhijiaEncoderV0::encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  unsigned char uuid[UUID_LEN] = {0};
  this->id_to_uuid(uuid, cont.id_, UUID_LEN);

  data_map_t * data = (data_map_t *) buf;
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
  this->whiten(buf, this->len_, 0x7F);
  this->whiten(buf, this->len_, 0x37);
}

ZhijiaEncoderV1::ZhijiaEncoderV1(const std::string & encoding, const std::string & variant, std::vector< uint8_t > && mac, uint8_t uid_start): 
  ZhijiaEncoder(encoding, variant, mac), uid_start_(uid_start) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderV1::decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  this->whiten(buf, this->len_, 0x37);

  data_map_t * data = (data_map_t *) buf;
  uint16_t crc16 = this->crc16(buf, ADDR_LEN + TXDATA_LEN);
  ENSURE_EQ(crc16, data->crc16, "Decoded KO (CRC)");

  uint8_t addr[ADDR_LEN];
  this->reverse_all(data->addr, ADDR_LEN);
  std::reverse_copy(data->addr, data->addr + ADDR_LEN, addr);
  if (!std::equal(addr, addr + ADDR_LEN, this->mac_.begin())) return false;

  ENSURE_EQ(data->txdata[7], data->txdata[14], "Decoded KO (Dupe 7/14)");
  ENSURE_EQ(data->txdata[8], data->txdata[11], "Decoded KO (Dupe 8/11)");
  ENSURE_EQ(data->txdata[11], data->txdata[16], "Decoded KO (Dupe 11/16)");

  uint8_t pivot = data->txdata[16];
  uint8_t uid[UID_LEN];
  uid[0] = data->txdata[7] ^ pivot;
  uid[1] = data->txdata[10] ^ pivot;
  uid[2] = data->txdata[4] ^ data->txdata[13];
  if(!std::equal(uid, uid + UID_LEN, this->mac_.begin() + this->uid_start_)) return false;

  enc_cmd.cmd = data->txdata[9] ^ pivot;
  enc_cmd.args[0] = data->txdata[0] ^ pivot;
  enc_cmd.args[1] = data->txdata[3] ^ pivot;
  enc_cmd.args[2] = data->txdata[5] ^ pivot;

  cont.tx_count_ = data->txdata[4] ^ pivot;
  cont.index_ = data->txdata[6] ^ pivot;
  uint8_t uuid[UUID_LEN];
  uuid[0] = data->txdata[2] ^ pivot;
  uuid[1] = data->txdata[2] ^ data->txdata[12];
  uuid[2] = data->txdata[9] ^ data->txdata[15];
  cont.id_ = this->uuid_to_id(uuid, UUID_LEN);

  uint8_t key = pivot ^ enc_cmd.args[0] ^ enc_cmd.args[1] ^ enc_cmd.args[2] ^ uid[0] ^ uid[1] ^ uid[2];
  key ^= uuid[0] ^ uuid[1] ^ uuid[2] ^ cont.tx_count_ ^ cont.index_ ^ enc_cmd.cmd;
  ENSURE_EQ(key, data->txdata[1], "Decoded KO (Key)");
  
  uint8_t re_pivot = uuid[1] ^ uuid[2] ^ uid[2];
  re_pivot ^= ((re_pivot & 1) - 1);
  ENSURE_EQ(pivot, re_pivot, "Decoded KO (Pivot)");

  return true;
}

void ZhijiaEncoderV1::encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  unsigned char uuid[UUID_LEN] = {0};
  this->id_to_uuid(uuid, cont.id_, UUID_LEN);

  data_map_t * data = (data_map_t *) buf;
  std::reverse_copy(this->mac_.begin(), this->mac_.end(), data->addr);
  this->reverse_all(data->addr, ADDR_LEN);

  uint8_t UID[UID_LEN];
  std::copy(this->mac_.begin() + this->uid_start_, this->mac_.begin() + this->uid_start_ + UID_LEN, UID);
  uint8_t pivot = uuid[1] ^ uuid[2] ^ UID[2];
  pivot ^= (pivot & 1) - 1;

  uint8_t key = enc_cmd.args[0] ^ enc_cmd.args[1] ^ enc_cmd.args[2];
  key ^=  uuid[0] ^ uuid[1] ^ uuid[2] ^ cont.tx_count_ ^ cont.index_ ^ enc_cmd.cmd ^ UID[0] ^ UID[1] ^ UID[2];

  data->txdata[0] = pivot ^ enc_cmd.args[0];
  data->txdata[1] = pivot ^ key;
  data->txdata[2] = pivot ^ uuid[0];
  data->txdata[3] = pivot ^ enc_cmd.args[1];
  data->txdata[4] = pivot ^ cont.tx_count_;
  data->txdata[5] = pivot ^ enc_cmd.args[2];
  data->txdata[6] = pivot ^ cont.index_;
  data->txdata[7] = pivot ^ UID[0];
  data->txdata[8] = pivot;
  data->txdata[9] = pivot ^ enc_cmd.cmd;
  data->txdata[10] = pivot ^ UID[1];
  data->txdata[11] = pivot;
  data->txdata[12] = uuid[1] ^ data->txdata[2];
  data->txdata[13] = UID[2] ^ data->txdata[4];
  data->txdata[14] = data->txdata[7];
  data->txdata[15] = uuid[2] ^ data->txdata[9];
  data->txdata[16] = pivot;

  data->crc16 = this->crc16(buf, ADDR_LEN + TXDATA_LEN);
  this->whiten(buf, this->len_, 0x37);
}

ZhijiaEncoderV2::ZhijiaEncoderV2(const std::string & encoding, const std::string & variant, std::vector< uint8_t > && mac): 
  ZhijiaEncoderV1(encoding, variant, std::move(mac)) {
  this->len_ = sizeof(data_map_t);
}

bool ZhijiaEncoderV2::decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  this->whiten(buf, this->len_, 0x6F);
  this->whiten(buf, this->len_ - 2, 0xD3);

  data_map_t * data = (data_map_t *) buf;
  for (size_t i = 0; i < TXDATA_LEN; ++i) {
    data->txdata[i] ^= data->pivot;
  }

  cont.tx_count_ = data->txdata[4];
  cont.index_ = data->txdata[6];
  enc_cmd.cmd = data->txdata[9];
  uint8_t addr[ADDR_LEN];
  addr[0] = data->txdata[7];
  addr[1] = data->txdata[10];
  addr[2] = data->txdata[13] ^ cont.tx_count_;
  enc_cmd.args[0] = data->txdata[0];
  enc_cmd.args[1] = data->txdata[3];
  enc_cmd.args[2] = data->txdata[5];
  uint8_t uuid[UUID_LEN]{0};
  uuid[0] = data->txdata[2];
  uuid[1] = data->txdata[12] ^ uuid[0];
  uuid[2] = data->txdata[15] ^ enc_cmd.cmd;
  cont.id_ = this->uuid_to_id(uuid, UUID_LEN);

  if (!std::equal(addr, addr + ADDR_LEN, this->mac_.begin())) return false;

  uint8_t key = addr[0] ^ addr[1] ^ addr[2] ^ cont.index_ ^ cont.tx_count_ ^ enc_cmd.args[0] ^ enc_cmd.args[1] ^ enc_cmd.args[2] ^ uuid[0] ^ uuid[1] ^ uuid[2];
  ENSURE_EQ(key, data->txdata[1], "Decoded KO (Key)");
  
  uint8_t re_pivot = uuid[0] ^ uuid[1] ^ uuid[2] ^ cont.tx_count_ ^ enc_cmd.args[1] ^ addr[0] ^ addr[2] ^ enc_cmd.cmd;
  re_pivot = ((re_pivot & 1) - 1) ^ re_pivot;
  ENSURE_EQ(data->pivot, re_pivot, "Decoded KO (Pivot)");

  ENSURE_EQ(data->txdata[8], uuid[0] ^ cont.tx_count_ ^ enc_cmd.args[1] ^ addr[0], "Decoded KO (txdata 8)");
  ENSURE_EQ(data->txdata[11], 0x00, "Decoded KO (txdata 11)");
  ENSURE_EQ(data->txdata[14], uuid[0] ^ cont.tx_count_ ^ enc_cmd.args[1] ^ enc_cmd.cmd, "Decoded KO (txdata 14)");

  return true;
}

void ZhijiaEncoderV2::encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  unsigned char uuid[UUID_LEN] = {0};
  this->id_to_uuid(uuid, cont.id_, UUID_LEN);

  data_map_t * data = (data_map_t *) buf;
  uint8_t key = this->mac_[0] ^ this->mac_[1] ^ this->mac_[2] ^ cont.index_ ^ cont.tx_count_;
  key ^= enc_cmd.args[0] ^ enc_cmd.args[1] ^ enc_cmd.args[2] ^ uuid[0] ^ uuid[1] ^ uuid[2];

  data->pivot = uuid[0] ^ uuid[1] ^ uuid[2] ^ cont.tx_count_ ^ enc_cmd.args[1] ^ this->mac_[0] ^ this->mac_[2] ^ enc_cmd.cmd;
  data->pivot = ((data->pivot & 1) - 1) ^ data->pivot;

  data->txdata[0] = enc_cmd.args[0];
  data->txdata[1] = key;
  data->txdata[2] = uuid[0];
  data->txdata[3] = enc_cmd.args[1];
  data->txdata[4] = cont.tx_count_;
  data->txdata[5] = enc_cmd.args[2];
  data->txdata[6] = cont.index_;
  data->txdata[7] = this->mac_[0];
  data->txdata[8] = uuid[0] ^ cont.tx_count_ ^ enc_cmd.args[1] ^ this->mac_[0];
  data->txdata[9] = enc_cmd.cmd;
  data->txdata[10] = this->mac_[1];
  data->txdata[11] = 0x00;
  data->txdata[12] = uuid[1] ^ uuid[0];
  data->txdata[13] = this->mac_[2] ^ cont.tx_count_;
  data->txdata[14] = uuid[0] ^ cont.tx_count_ ^ enc_cmd.args[1] ^ enc_cmd.cmd;
  data->txdata[15] = uuid[2] ^ enc_cmd.cmd;

  for (size_t i = 0; i < TXDATA_LEN; ++i) {
    data->txdata[i] ^= data->pivot;
  }
  
  this->whiten(buf, this->len_ - 2, 0xD3);
  this->whiten(buf, this->len_, 0x6F);
}

} // namespace ble_adv_handler
} // namespace esphome
