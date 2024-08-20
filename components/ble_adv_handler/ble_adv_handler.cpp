#include "ble_adv_handler.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32_BLE_CLIENT
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#endif

namespace esphome {
namespace ble_adv_handler {

static const char *TAG = "ble_adv_handler";

void BleAdvParam::from_raw(const uint8_t * buf, size_t len) {
  // Copy the raw data as is, limiting to the max size of the buffer
  this->len_ = std::min(MAX_PACKET_LEN, len);
  std::copy(buf, buf + this->len_, this->buf_);

  // find the data / flag indexes in the buffer
  size_t cur_len = 0;
  while (cur_len < this->len_ - 2) {
    size_t sub_len = this->buf_[cur_len];
    uint8_t type = this->buf_[cur_len + 1];
    if (type == ESP_BLE_AD_TYPE_FLAG) {
      this->ad_flag_index_ = cur_len;
    }
    if ((type == ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE) 
        || (type == ESP_BLE_AD_TYPE_16SRV_CMPL)
        || (type == ESP_BLE_AD_TYPE_SERVICE_DATA)){
      this->data_index_ = cur_len;
    }
    cur_len += (sub_len + 1);
  }  
}

void BleAdvParam::from_hex_string(std::string & raw) {
  // Clean-up input string
  raw = raw.substr(0, raw.find('('));
  raw.erase(std::remove_if(raw.begin(), raw.end(), [&](char & c) { return c == '.' || c == ' '; }), raw.end());
  if (raw.substr(0,2) == "0x") {
    raw = raw.substr(2);
  }

  // convert to integers
  uint8_t raw_int[MAX_PACKET_LEN]{0};
  uint8_t len = std::min(MAX_PACKET_LEN, raw.size()/2);
  for (uint8_t i=0; i < len; ++i) {
    raw_int[i] = stoi(raw.substr(2*i, 2), 0, 16);
  }
  this->from_raw(raw_int, len);
}

void BleAdvParam::init_with_ble_param(uint8_t ad_flag, uint8_t data_type) {
  if (ad_flag != 0x00) {
    this->ad_flag_index_ = 0;
    this->buf_[0] = 2;
    this->buf_[1] = ESP_BLE_AD_TYPE_FLAG;
    this->buf_[2] = ad_flag;
    this->data_index_ = 3;
    this->buf_[4] = data_type;
  } else {
    this->data_index_ = 0;
    this->buf_[1] = data_type;
  }
}

void BleAdvParam::set_data_len(size_t len) {
  this->buf_[this->data_index_] = len + 1;
  this->len_ = len + 2 + (this->has_ad_flag() ? 3 : 0);
}

std::string BleAdvGenCmd::str() {
  char ret[100]{0};
  size_t ind = 0;
  switch(this->cmd) {
    case CommandType::PAIR:
      ind = std::sprintf(ret, "PAIR");
      break;
    case CommandType::UNPAIR:
      ind = std::sprintf(ret, "UNPAIR");
      break;
    case CommandType::CUSTOM:
      ind = std::sprintf(ret, "CUSTOM");
      break;
    case CommandType::ALL_OFF:
      ind = std::sprintf(ret, "ALL_OFF");
      break;
    case CommandType::LIGHT_ON:
      ind = std::sprintf(ret, "LIGHT_ON");
      break;
    case CommandType::LIGHT_OFF:
      ind = std::sprintf(ret, "LIGHT_OFF");
      break;
    case CommandType::LIGHT_DIM:
      ind = std::sprintf(ret, "LIGHT_DIM - %.0f%%", this->args[0] * 100);
      break;
    case CommandType::LIGHT_CCT:
      ind = std::sprintf(ret, "LIGHT_CCT - %.0f%%", this->args[0] * 100);
      break;
    case CommandType::LIGHT_WCOLOR:
      ind = std::sprintf(ret, "LIGHT_WCOLOR/%d - cold: %.0f%%, warm: %.0f%%", this->param, this->args[0] * 100, this->args[1] * 100);
      break;
    case CommandType::LIGHT_SEC_ON:
      ind = std::sprintf(ret, "LIGHT_SEC_ON");
      break;
    case CommandType::LIGHT_SEC_OFF:
      ind = std::sprintf(ret, "LIGHT_SEC_OFF");
      break;
    case CommandType::FAN_ONOFF_SPEED:
      ind = std::sprintf(ret, "FAN_ONOFF_SPEED - %0.f/%0.f", this->args[0], this->args[1]);
      break;
    case CommandType::FAN_DIR:
      ind = std::sprintf(ret, "FAN_DIR - %s", this->args[0] == 1 ? "Reverse" : "Forward");
      break;
    case CommandType::FAN_OSC:
      ind = std::sprintf(ret, "FAN_OSC - %s", this->args[0] == 1 ? "ON": "OFF");
      break;
    default:
      ind = std::sprintf(ret, "UNKNOWN - %d", this->cmd);
      break;
  }
  return ret;
}

void BleAdvEncoder::translate_g2e(std::vector< BleAdvEncCmd > & enc_cmds, const BleAdvGenCmd & gen_cmd) const {
  BleAdvEncCmd enc_cmd;
  this->translator_->g2e_cmd(gen_cmd, enc_cmd);
  if (enc_cmd.cmd != 0) {
    enc_cmds.emplace_back(std::move(enc_cmd));
  }
}

void BleAdvEncoder::translate_e2g(BleAdvGenCmd & gen_cmd, const BleAdvEncCmd & enc_cmd) const {
  this->translator_->e2g_cmd(enc_cmd, gen_cmd);
}

bool BleAdvEncoder::decode(const BleAdvParam & param, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  // Check global len and header to discard most of encoders
  size_t len = param.get_data_len() - this->header_.size();
  const uint8_t * cbuf = param.get_const_data_buf();
  if (len != this->len_) return false;
  if (!std::equal(this->header_.begin(), this->header_.end(), cbuf)) return false;

  // copy the data to be decoded, not to alter it for other decoders
  uint8_t buf[MAX_PACKET_LEN]{0};
  std::copy(cbuf, cbuf + param.get_data_len(), buf);
  return this->decode(buf + this->header_.size(), enc_cmd, cont);
}

void BleAdvEncoder::encode(std::vector< BleAdvParam > & params, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
  params.emplace_back();
  BleAdvParam & param = params.back();
  param.init_with_ble_param(this->ad_flag_, this->adv_data_type_);
  std::copy(this->header_.begin(), this->header_.end(), param.get_data_buf());
  uint8_t * buf = param.get_data_buf() + this->header_.size();
  this->encode(buf, enc_cmd, cont);

  ESP_LOGD(this->id_.c_str(), "UUID: '0x%lX', index: %d, tx: %d, enc: %s", 
      cont.id_, cont.index_, cont.tx_count_, this->to_str(enc_cmd).c_str());

  param.set_data_len(this->len_ + this->header_.size());    
}

void BleAdvEncoder::whiten(uint8_t *buf, size_t len, uint8_t seed) const {
  uint8_t r = seed;
  for (size_t i=0; i < len; i++) {
    uint8_t b = 0;
    for (size_t j=0; j < 8; j++) {
      r <<= 1;
      if (r & 0x80) {
        r ^= 0x11;
        b |= 1 << j;
      }
      r &= 0x7F;
    }
    buf[i] ^= b;
  }
}

void BleAdvEncoder::reverse_all(uint8_t* buf, uint8_t len) const {
  for (size_t i = 0; i < len; ++i) {
    uint8_t & x = buf[i];
    x = ((x & 0x55) << 1) | ((x & 0xAA) >> 1);
    x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
    x = ((x & 0x0F) << 4) | ((x & 0xF0) >> 4);
  }
}

void BleAdvHandler::setup() {
#ifdef USE_API
  register_service(&BleAdvHandler::on_raw_decode, "raw_decode", {"raw"});
#endif
}

void BleAdvHandler::add_encoder(BleAdvEncoder * encoder) { 
  this->encoders_.push_back(encoder);
}

BleAdvEncoder * BleAdvHandler::get_encoder(const std::string & id) {
  for(auto & encoder : this->encoders_) {
    if (encoder->get_id() == id) {
      return encoder;
    }
  }
  ESP_LOGE(TAG, "No Encoder with id: %s", id.c_str());
  return nullptr;
}

std::vector<std::string> BleAdvHandler::get_ids(const std::string & encoding) {
  std::vector<std::string> ids;
  ids.push_back(BleAdvEncoder::ID(encoding, BleAdvEncoder::VARIANT_ALL));
  for(auto & encoder : this->encoders_) {
    if (encoder->get_encoding() == encoding) {
      ids.push_back(encoder->get_id());
    }
  }
  return ids;
}

uint16_t BleAdvHandler::add_to_advertiser(std::vector< BleAdvParam > & params) {
  uint32_t msg_id = ++this->id_count;
  for (auto & param : params) {
    this->packets_.emplace_back(BleAdvProcess(msg_id, std::move(param)));
    ESP_LOGD(TAG, "request start advertising - %ld: %s", msg_id, 
                esphome::format_hex_pretty(param.get_full_buf(), param.get_full_len()).c_str());
  }
  params.clear(); // As we moved the content, just to be sure no caller will re use it
  return this->id_count;
}

void BleAdvHandler::remove_from_advertiser(uint16_t msg_id) {
  ESP_LOGD(TAG, "request stop advertising - %d", msg_id);
  for (auto & param : this->packets_) {
    if (param.id_ == msg_id) {
      param.to_be_removed_ = true;
    }
  }
}

// try to identify the relevant encoder
bool BleAdvHandler::identify_param(const BleAdvParam & param, bool ignore_ble_param) {
  for(auto & encoder : this->encoders_) {
    if (!ignore_ble_param && !encoder->is_ble_param(param.get_ad_flag(), param.get_data_type())) {
      continue;
    }
    ControllerParam_t cont;
    BleAdvEncCmd enc_cmd;
    if(encoder->decode(param, enc_cmd, cont)) {
      BleAdvGenCmd gen_cmd;
      encoder->translate_e2g(gen_cmd, enc_cmd);
      ESP_LOGI(encoder->get_id().c_str(), "Decoded OK - tx: %d, gen: %s, enc: %s", 
                cont.tx_count_, gen_cmd.str().c_str(), encoder->to_str(enc_cmd).c_str());

      if (gen_cmd.cmd == CommandType::PAIR) {
        std::string config_str = "config: \nble_adv_controller:";
        config_str += "\n  - id: my_controller_id";
        config_str += "\n    encoding: %s";
        config_str += "\n    variant: %s";
        config_str += "\n    forced_id: 0x%X";
        if (cont.index_ != 0) {
          config_str += "\n    index: %d";
        }
        ESP_LOGI(TAG, config_str.c_str(), encoder->get_encoding().c_str(), encoder->get_variant().c_str(), cont.id_, cont.index_);
      }
      
      // Re encoding with the same parameters to check if it gives the same output
      std::vector< BleAdvParam > params;
      std::vector< BleAdvEncCmd > re_enc_cmds;
      encoder->translate_g2e(re_enc_cmds, gen_cmd);
      for (auto & re_enc_cmd: re_enc_cmds) {
        encoder->encode(params, re_enc_cmd, cont);
        BleAdvParam & fparam = params.back();
        ESP_LOGD(TAG, "enc - %s", esphome::format_hex_pretty(fparam.get_full_buf(), fparam.get_full_len()).c_str());
        if (std::equal(param.get_const_data_buf(), param.get_const_data_buf() + param.get_data_len(), fparam.get_data_buf())) {
          ESP_LOGI(TAG, "Decoded / Re-encoded with NO DIFF");
        } else {
          ESP_LOGE(TAG, "DIFF after Decode / Re-encode");
        }
      }
      if (re_enc_cmds.empty()){
        ESP_LOGD(TAG, "No corresponding command to encode.");
      }
      return true;
    } 
  }
  return false;
}

#ifdef USE_API
void BleAdvHandler::on_raw_decode(std::string raw) {
  BleAdvParam param;
  param.from_hex_string(raw);
  ESP_LOGD(TAG, "raw - %s", esphome::format_hex_pretty(param.get_full_buf(), param.get_full_len()).c_str());
  this->identify_param(param, true);
}
#endif

#ifdef USE_ESP32_BLE_CLIENT
/* Basic class inheriting esp32_ble_tracker::ESPBTDevice in order to access 
    protected attribute 'scan_result_' containing raw advertisement
*/
class HackESPBTDevice: public esp32_ble_tracker::ESPBTDevice {
public:
  void get_raw_packet(BleAdvParam & param) const {
    param.from_raw(this->scan_result_.ble_adv, this->scan_result_.adv_data_len);
  }
};

void BleAdvHandler::capture(const esp32_ble_tracker::ESPBTDevice & device, bool ignore_ble_param, uint16_t rem_time) {
  // Clean-up expired packets
  this->listen_packets_.remove_if( [&](BleAdvParam & p){ return p.duration_ < millis(); } );

  // Read raw advertised packets
  BleAdvParam param;
  const HackESPBTDevice * hack_device = reinterpret_cast< const HackESPBTDevice * >(&device);
  hack_device->get_raw_packet(param);
  if (!param.has_data()) return;

  // Check if not already received in the last 300s
  auto idx = std::find(this->listen_packets_.begin(), this->listen_packets_.end(), param);
  if (idx == this->listen_packets_.end()) {
    ESP_LOGD(TAG, "raw - %s", esphome::format_hex_pretty(param.get_full_buf(), param.get_full_len()).c_str());
    param.duration_ = millis() + (uint32_t)rem_time * 1000;
    this->identify_param(param, ignore_ble_param);
    this->listen_packets_.emplace_back(std::move(param));
  }
}
#endif

void BleAdvHandler::loop() {
  if (this->adv_stop_time_ == 0) {
    // No packet is being advertised, process with clean-up IF already processed once and requested for removal
    this->packets_.remove_if([&](BleAdvProcess & p){ return p.processed_once_ && p.to_be_removed_; } );
    // if packets to be advertised, advertise the front one
    if (!this->packets_.empty()) {
      BleAdvParam & packet = this->packets_.front().param_;
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_config_adv_data_raw(packet.get_full_buf(), packet.get_full_len()));
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_start_advertising(&(this->adv_params_)));
      this->adv_stop_time_ = millis() + this->packets_.front().param_.duration_;
      this->packets_.front().processed_once_ = true;
    }
  } else {
    // Packet is being advertised, check if time to switch to next one in case:
    // The advertise seq_duration expired AND
    // There is more than one packet to advertise OR the front packet was requested to be removed
    bool multi_packets = (this->packets_.size() > 1);
    bool front_to_be_removed = this->packets_.front().to_be_removed_;
    if ((millis() > this->adv_stop_time_) && (multi_packets || front_to_be_removed)) {
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_stop_advertising());
      this->adv_stop_time_ = 0;
      if (front_to_be_removed) {
        this->packets_.pop_front();
      } else if (multi_packets) {
        this->packets_.emplace_back(std::move(this->packets_.front()));
        this->packets_.pop_front();
      }
    }
  }
}

void BleAdvSelect::control(const std::string &value) {
  this->publish_state(value);
  uint32_t hash_value = fnv1_hash(value);
  this->rtc_.save(&hash_value);
}

void BleAdvSelect::sub_init() { 
  App.register_select(this);
  this->rtc_ = global_preferences->make_preference< uint32_t >(this->get_object_id_hash());
  uint32_t restored;
  if (this->rtc_.load(&restored)) {
    for (auto & opt: this->traits.get_options()) {
      if(fnv1_hash(opt) == restored) {
        this->state = opt;
        return;
      }
    }
  }
}

void BleAdvNumber::control(float value) {
  this->publish_state(value);
  this->rtc_.save(&value);
}

void BleAdvNumber::sub_init() {
  App.register_number(this);
  this->rtc_ = global_preferences->make_preference< float >(this->get_object_id_hash());
  float restored;
  if (this->rtc_.load(&restored)) {
    this->state = restored;
  }
}

void BleAdvDevice::set_encoding_and_variant(const std::string & encoding, const std::string & variant) {
  this->select_encoding_.traits.set_options(this->get_parent()->get_ids(encoding));
  this->select_encoding_.state = BleAdvEncoder::ID(encoding, variant);
  this->encoders_.clear();
  this->encoders_.push_back(this->get_parent()->get_encoder(this->select_encoding_.state));
  this->select_encoding_.add_on_state_callback(std::bind(&BleAdvDevice::refresh_encoder, this, std::placeholders::_1, std::placeholders::_2));
}

void BleAdvDevice::refresh_encoder(std::string id, size_t index) {
  this->encoders_.clear();
  if (index == 0) {
    // "All" encoder selected, refresh from list, avoiding "All"
    for (auto & aid : this->select_encoding_.traits.get_options()) {
      if (aid != id) {
        this->encoders_.push_back(this->get_parent()->get_encoder(aid));
      }
    }
  } else {
    this->encoders_.push_back(this->get_parent()->get_encoder(id));
  }
}

} // namespace ble_adv_handler
} // namespace esphome
