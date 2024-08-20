#include "fanlamp_pro.h"
#include "esphome/core/log.h"
#include <arpa/inet.h>

#define MBEDTLS_AES_ALT
#include <aes_alt.h>

namespace esphome {
namespace ble_adv_handler {

FanLampEncoder::FanLampEncoder(const std::string & encoding, const std::string & variant, const std::vector<uint8_t> & prefix):
         BleAdvEncoder(encoding, variant), prefix_(prefix) {
}

uint16_t FanLampEncoder::get_seed(uint16_t forced_seed) const {
  return (forced_seed == 0) ? (uint16_t) rand() % 0xFFF5 : forced_seed;
}

uint16_t FanLampEncoder::crc16(uint8_t* buf, size_t len, uint16_t seed) const {
  return esphome::crc16be(buf, len, seed);
}

FanLampEncoderV1::FanLampEncoderV1(const std::string & encoding, const std::string & variant, uint8_t pair_arg3,
                                    bool pair_arg_only_on_pair, bool xor1, uint8_t supp_prefix):
          FanLampEncoder(encoding, variant, {0xAA, 0x98, 0x43, 0xAF, 0x0B, 0x46, 0x46, 0x46}), pair_arg3_(pair_arg3), pair_arg_only_on_pair_(pair_arg_only_on_pair), 
              with_crc2_(supp_prefix == 0x00), xor1_(xor1) {
  if (supp_prefix != 0x00) this->prefix_.insert(this->prefix_.begin(), supp_prefix);
  this->len_ = this->prefix_.size() + sizeof(data_map_t) + (this->with_crc2_ ? 2 : 1);
}

std::string FanLampEncoderV1::to_str(const BleAdvEncCmd & enc_cmd) const {
  char ret[100];
  size_t ind = std::sprintf(ret, "0x%02X - param1 0x%02X - args[%d,%d,%d]", enc_cmd.cmd, enc_cmd.param1, 
                            enc_cmd.args[0], enc_cmd.args[1], enc_cmd.args[2]);
  return ret;
}

bool FanLampEncoderV1::decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  this->whiten(buf, this->len_, 0x6F);
  this->reverse_all(buf, this->len_);

  uint8_t data_start = this->prefix_.size();
  data_map_t * data = (data_map_t *) (buf + data_start);

  // distinguish in between different encoder variants
  if(!std::equal(this->prefix_.begin(), this->prefix_.end(), buf)) return false;
  if (data->cmd == 0x28 && this->pair_arg3_ != data->args[2]) return false;
  if (data->cmd != 0x28 && !this->pair_arg_only_on_pair_ && data->args[2] != this->pair_arg3_) return false;
  if (data->cmd != 0x28 && this->pair_arg_only_on_pair_ && data->args[2] != 0) return false;

  std::string decoded = esphome::format_hex_pretty(buf, this->len_);
  uint16_t seed = htons(data->seed);
  uint8_t seed8 = static_cast<uint8_t>(seed & 0xFF);
  ENSURE_EQ(data->r2, this->xor1_ ? seed8 ^ 1 : seed8, "Decoded KO (r2) - %s", decoded.c_str());

  uint16_t crc16 = htons(this->crc16((uint8_t*)(data), sizeof(data_map_t) - 2, ~seed));
  ENSURE_EQ(crc16, data->crc16, "Decoded KO (crc16) - %s", decoded.c_str());

  if (data->args[2] != 0) {
    ENSURE_EQ(data->args[2], this->pair_arg3_, "Decoded KO (arg3) - %s", decoded.c_str());
  }

  if (this->with_crc2_) {
    uint16_t crc16_mac = this->crc16(buf + 1, 5, 0xffff);
    uint16_t crc16_2 = htons(this->crc16(buf + data_start, sizeof(data_map_t), crc16_mac));
    uint16_t crc16_data_2 = *(uint16_t*) &buf[this->len_ - 2];
    ENSURE_EQ(crc16_data_2, crc16_2, "Decoded KO (crc16_2) - %s", decoded.c_str());
  }

  uint8_t rem_id = data->src ^ seed8;

  enc_cmd.cmd = data->cmd;
  enc_cmd.param1 = data->param1;
  std::copy(data->args, data->args + ARGS_LEN, enc_cmd.args);
  
  cont.tx_count_ = data->tx_count;
  cont.index_ = (data->group_index & 0x0F00) >> 8;
  cont.id_ = (uint32_t)data->group_index + 256*256*((uint32_t)rem_id);
  cont.seed_ = seed;
  return true;
}

void FanLampEncoderV1::encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  std::copy(this->prefix_.begin(), this->prefix_.end(), buf);
  data_map_t *data = (data_map_t*)(buf + this->prefix_.size());

  if (enc_cmd.cmd == 0x28) {
    enc_cmd.args[0] = cont.id_ & 0xFF;
    enc_cmd.args[1] = (cont.id_ >> 8) & 0xF0;
    enc_cmd.args[2] = this->pair_arg3_;
  } else {
    enc_cmd.args[2] = this->pair_arg_only_on_pair_ ? enc_cmd.args[2] : this->pair_arg3_;
  }
  data->cmd = enc_cmd.cmd;
  data->param1 = enc_cmd.param1;
  std::copy(enc_cmd.args, enc_cmd.args + ARGS_LEN, data->args);
  
  uint16_t seed = this->get_seed(cont.seed_);
  uint8_t seed8 = static_cast<uint8_t>(seed & 0xFF);
  uint16_t cmd_id_trunc = static_cast<uint16_t>(cont.id_ & 0xF0FF);

  data->group_index = cmd_id_trunc + (((uint16_t)(cont.index_ & 0x0F)) << 8);
  data->tx_count = cont.tx_count_;
  data->src = this->xor1_ ? seed8 ^ 1 : seed8 ^ ((cont.id_ >> 16) & 0xFF);
  data->r2 = this->xor1_ ? seed8 ^ 1 : seed8;
  data->seed = htons(seed);
  data->crc16 = htons(this->crc16((uint8_t*)(data), sizeof(data_map_t) - 2, ~seed));
  
  if (this->with_crc2_) {
    uint16_t* crc16_2 = (uint16_t*) &buf[this->len_ - 2];
    uint16_t crc_mac = this->crc16(buf + 1, 5, 0xffff);
    *crc16_2 = htons(this->crc16((uint8_t*)(data), sizeof(data_map_t), crc_mac));
  } else {
    buf[this->len_ - 1] = 0xAA;
  }

  this->reverse_all(buf, this->len_);
  this->whiten(buf, this->len_, 0x6F);
}

FanLampEncoderV2::FanLampEncoderV2(const std::string & encoding, const std::string & variant, const std::vector<uint8_t> && prefix, uint16_t device_type, bool with_sign):
  FanLampEncoder(encoding, variant, prefix), device_type_(device_type), with_sign_(with_sign) {
  this->len_ = this->prefix_.size() + sizeof(data_map_t);
}

std::string FanLampEncoderV2::to_str(const BleAdvEncCmd & enc_cmd) const {
  char ret[100];
  size_t ind = std::sprintf(ret, "0x%02X - param1 0x%02X - args[%d,%d]", enc_cmd.cmd, enc_cmd.param1, enc_cmd.args[0], enc_cmd.args[1]);
  return ret;
}

uint16_t FanLampEncoderV2::sign(uint8_t* buf, uint8_t tx_count, uint16_t seed) const {
  uint8_t sigkey[16] = {0, 0, 0, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16};
  
  sigkey[0] = seed & 0xff;
  sigkey[1] = (seed >> 8) & 0xff;
  sigkey[2] = tx_count;
  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);
  mbedtls_aes_setkey_enc(&aes_ctx, sigkey, sizeof(sigkey)*8);
  uint8_t aes_in[16], aes_out[16];
  memcpy(aes_in, buf, 16);
  mbedtls_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, aes_in, aes_out);
  mbedtls_aes_free(&aes_ctx);
  uint16_t sign = ((uint16_t*) aes_out)[0]; 
  return sign == 0 ? 0xffff : sign;
}

void FanLampEncoderV2::whiten(uint8_t *buf, uint8_t size, uint8_t seed, uint8_t salt) const {
  static constexpr uint8_t XBOXES[128] = {
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
  };

  for (uint8_t i = 0; i < size; ++i) {
    buf[i] ^= XBOXES[((seed + i + 9) & 0x1f) + (salt & 0x3) * 0x20];
    buf[i] ^= seed;
  }
}

bool FanLampEncoderV2::decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  data_map_t * data = (data_map_t *) (buf + this->prefix_.size());
  uint16_t crc16 = this->crc16(buf , this->len_ - 2, ~(data->seed));

  this->whiten(buf + 2, this->len_ - 6, (uint8_t)(data->seed), 0);
  if (!std::equal(this->prefix_.begin(), this->prefix_.end(), buf)) return false;
  if (data->type != this->device_type_) return false;
  if (this->with_sign_ && data->sign == 0x0000) return false;
  if (!this->with_sign_ && data->sign != 0x0000) return false;

  std::string decoded = esphome::format_hex_pretty(buf, this->len_);
  ENSURE_EQ(crc16, data->crc16, "Decoded KO (crc16) - %s", decoded.c_str());

  if (this->with_sign_) {
    ENSURE_EQ(this->sign(buf + 1, data->tx_count, data->seed), data->sign, "Decoded KO (sign) - %s", decoded.c_str());
  }

  enc_cmd.cmd = data->cmd;
  enc_cmd.param1 = data->param1;
  std::copy(data->args, data->args + ARGS_LEN, enc_cmd.args);

  cont.tx_count_ = data->tx_count;
  cont.index_ = data->group_index;
  cont.id_ = data->identifier;
  cont.seed_ = data->seed;

  return true;
}

void FanLampEncoderV2::encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  std::copy(this->prefix_.begin(), this->prefix_.end(), buf);
  data_map_t * data = (data_map_t *) (buf + this->prefix_.size());

  data->cmd = enc_cmd.cmd;
  data->param1 = enc_cmd.param1;
  std::copy(enc_cmd.args, enc_cmd.args + ARGS_LEN, data->args);

  uint16_t seed = this->get_seed(cont.seed_);

  data->tx_count = cont.tx_count_;
  data->type = this->device_type_;
  data->identifier = cont.id_;
  data->group_index = cont.index_;
  data->seed = seed;

  if (this->with_sign_) {
    data->sign = this->sign(buf + 1, data->tx_count, seed);
  }

  this->whiten(buf + 2, this->len_ - 6, (uint8_t) seed);
  data->crc16 = this->crc16(buf, this->len_ - 2, ~seed);
}

} // namespace ble_adv_handler
} // namespace esphome
