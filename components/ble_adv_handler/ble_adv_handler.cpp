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

std::string BleAdvParam::str() const {
  return esphome::format_hex_pretty(this->get_const_full_buf(), this->get_full_len());
}

void BleAdvParam::from_raw(const uint8_t * buf, size_t len) {
  // Copy the raw data as is, limiting to the max size of the buffer
  this->len_ = std::min(MAX_PACKET_LEN, len);
  std::copy(buf, buf + this->len_, this->buf_);

  // find the data / flag indexes in the buffer
  size_t cur_len = 0;
  while (cur_len < this->len_ - 2) {
    size_t sub_len = this->buf_[cur_len];
    if (sub_len + cur_len >= this->len_) break; // avoid trying to read outside of buffer (case of malformed messages)
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

bool BleAdvParam::is_data_equal(const BleAdvParam & comp) const{
  return (comp.has_data() && this->has_data() 
          && (this->get_data_len() == comp.get_data_len())
          && std::equal(this->get_const_data_buf(), this->get_const_data_buf() + this->get_data_len(), comp.get_const_data_buf()));
}

std::string BleAdvGenCmd::str() const {
  char ret_full[100]{0};
  size_t ind = 0;
  char * ret = ret_full;
  switch(this->ent_type) {
    case EntityType::NOTYPE:
      ind = std::sprintf(ret_full, "NOTYPE - ");
      break;
    case EntityType::CONTROLLER:
      ind = std::sprintf(ret_full, "CONTROLLER - ");
      break;
    case EntityType::LIGHT:
      ind = std::sprintf(ret_full, "LIGHT/%d - ", this->ent_index);
      break;
    case EntityType::FAN:
      ind = std::sprintf(ret_full, "FAN/%d - ", this->ent_index);
      break;
    case EntityType::ALL:
      ind = std::sprintf(ret_full, "ALL - ");
      break;
  }
  ret = ret_full + ind;

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
    case CommandType::TOGGLE:
      ind = std::sprintf(ret, "TOGGLE");
      break;
    case CommandType::ON:
      ind = std::sprintf(ret, "ON");
      break;
    case CommandType::OFF:
      ind = std::sprintf(ret, "OFF");
      break;
    case CommandType::TIMER:
      ind = std::sprintf(ret, "TIMER - %0.f minutes", this->args[0]);
      break;
    case CommandType::LIGHT_CWW_DIM:
      if (this->param == 0) {
        ind = std::sprintf(ret, "LIGHT_CWW_DIM - %.0f%%", this->args[0] * 100);
      } else if (this->param == 1) {
        ind = std::sprintf(ret, "LIGHT_CWW_DIM (+)");
      } else if (this->param == 2) {
        ind = std::sprintf(ret, "LIGHT_CWW_DIM (-)");
      } 
      break;
    case CommandType::LIGHT_CWW_WARM:
      if (this->param == 0) {
        ind = std::sprintf(ret, "LIGHT_CWW_WARM - %.0f%%", this->args[0] * 100);
      } else if (this->param == 1) {
        ind = std::sprintf(ret, "LIGHT_CWW_WARM (+)");
      } else if (this->param == 2) {
        ind = std::sprintf(ret, "LIGHT_CWW_WARM (-)");
      } 
      break;
    case CommandType::LIGHT_CWW_COLD_WARM:
      ind = std::sprintf(ret, "LIGHT_CWW_COLD_WARM/%d - cold: %.0f%%, warm: %.0f%%", this->param, this->args[0] * 100, this->args[1] * 100);
      break;
    case CommandType::LIGHT_CWW_WARM_DIM:
      ind = std::sprintf(ret, "LIGHT_CWW_WARM_DIM - warm: %.0f%%, brightness: %.0f%%", this->args[0] * 100, this->args[1] * 100);
      break;
    case CommandType::LIGHT_RGB_FULL:
      ind = std::sprintf(ret, "LIGHT_RGB_FULL - r: %.0f%%, g: %.0f%%, b: %.0f%%", this->args[0] * 100, this->args[1] * 100, this->args[2] * 100);
      break;
    case CommandType::LIGHT_RGB_DIM:
      ind = std::sprintf(ret, "LIGHT_RGB_DIM - %.0f%%", this->args[0] * 100);
      break;
    case CommandType::LIGHT_RGB_RGB:
      ind = std::sprintf(ret, "LIGHT_RGB_RGB - r: %.0f%%, g: %.0f%%, b: %.0f%%", this->args[0] * 100, this->args[1] * 100, this->args[2] * 100);
      break;
    case CommandType::FAN_FULL:
      ind = std::sprintf(ret, "FAN_FULL/0x%X - %0.f/%0.f/%0.f", this->param, this->args[0], this->args[1], this->args[2]);
      break;
    case CommandType::FAN_ONOFF_SPEED:
      ind = std::sprintf(ret, "FAN_ONOFF_SPEED - %0.f/%0.f", this->args[0], this->args[1]);
      break;
    case CommandType::FAN_DIR:
      ind = std::sprintf(ret, "FAN_DIR - %s", this->args[0] == 1 ? "Reverse" : "Forward");
      break;
    case CommandType::FAN_DIR_TOGGLE:
      ind = std::sprintf(ret, "FAN_DIR_TOGGLE");
      break;
    case CommandType::FAN_OSC:
      ind = std::sprintf(ret, "FAN_OSC - %s", this->args[0] == 1 ? "ON": "OFF");
      break;
    case CommandType::FAN_OSC_TOGGLE:
      ind = std::sprintf(ret, "FAN_OSC_TOGGLE");
      break;
    default:
      ind = std::sprintf(ret, "UNKNOWN - %d", this->cmd);
      break;
  }
  return ret_full;
}

std::string BleAdvEncCmd::str() const {
  char ret[100]{0};
  size_t ind = 0;
  ind = std::sprintf(ret, "cmd: 0x%02X - param1: 0x%02X - args: [%d, %d, %d]", this->cmd, this->param1, this->args[0], this->args[1], this->args[2]);
  return ret;
}

void BleAdvEncoder::translate_g2e(std::vector< BleAdvEncCmd > & enc_cmds, const BleAdvGenCmd & gen_cmd) const {
  BleAdvEncCmd enc_cmd;
  if (this->translator_->g2e_cmd(gen_cmd, enc_cmd)) {
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
  if (!this->check_eq(this->len_, len, "Data length")) return false;
  if (!this->check_eq_buf(this->header_.data(), cbuf, this->header_.size(), "Header")) return false;

  // copy the data to be decoded, not to alter it for other decoders
  uint8_t buf[MAX_PACKET_LEN]{0};
  std::copy(cbuf, cbuf + param.get_data_len(), buf);
  return this->decode(buf + this->header_.size(), enc_cmd, cont);
}

void BleAdvEncoder::encode(BleAdvParams & params, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const {
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

// 1100 1010 => 0101 0011
uint8_t BleAdvEncoder::reverse_byte(uint8_t x) const {
    x = ((x & 0x55) << 1) | ((x & 0xAA) >> 1);
    x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
    x = ((x & 0x0F) << 4) | ((x & 0xF0) >> 4);
    return x;
}

void BleAdvEncoder::reverse_all(uint8_t* buf, uint8_t len) const {
  for (size_t i = 0; i < len; ++i) {
    buf[i] = reverse_byte(buf[i]);
  }
}

uint8_t BleAdvEncoder::checksum(uint8_t * buf, size_t len) const {
  uint8_t ck = 0;
  for (size_t i = 0; i < len; ++i) {
    ck += buf[i];
  }
  return ck & 0xFF;
}

bool BleAdvEncoder::check_eq(uint32_t ref, uint32_t comp, const char * msg) const {
  if (ref != comp) {
    if (this->debug_mode_) {
      ESP_LOGD(this->id_.c_str(), "'%s' differs - expected: '0x%lX', received: '0x%X'", msg, ref, comp);
    }
    return false;
  }
  return true;
}

bool BleAdvEncoder::check_eq_buf(const uint8_t* ref_buf, const uint8_t* comp_buf, size_t len, const char * msg) const {
  if (!std::equal(ref_buf, ref_buf + len, comp_buf)) {
    if (this->debug_mode_) {
      std::string expected = esphome::format_hex_pretty(ref_buf, len);
      std::string received = esphome::format_hex_pretty(comp_buf, len);
      ESP_LOGD(this->id_.c_str(), "'%s' differs - expected: '%s', received: '%s'", msg, expected.c_str(), received.c_str());
    }
    return false;
  }
  return true;
}

void BleAdvEncoder::log_buffer(const uint8_t* buf, size_t len, const char * msg) const {
  if (!this->debug_mode_) return;
  std::string buffer = esphome::format_hex_pretty(buf, len);
  ESP_LOGD(this->id_.c_str(), "%s - %s", msg, buffer.c_str());
}

void BleAdvHandler::setup() {
#ifdef USE_API
  register_service(&BleAdvHandler::on_raw_decode, "raw_decode", {"raw"});
  register_service(&BleAdvHandler::on_raw_listen, "raw_listen", {"raw"});
#endif
  this->scan_result_lock_ = xSemaphoreCreateMutex();
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

uint16_t BleAdvHandler::add_to_advertiser(BleAdvParams & params) {
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
bool BleAdvHandler::handle_raw_param(BleAdvParam & param, bool publish) {
  if (this->log_raw_) {
    ESP_LOGD(TAG, "raw - %s", esphome::format_hex_pretty(param.get_full_buf(), param.get_full_len()).c_str());
  }
  if (!param.has_data()) {
    if (this->log_raw_) ESP_LOGD(TAG, "Malformed raw message - ignored.");
    return false;
  }
  for (auto & raw_trigger: this->raw_triggers_) {
    raw_trigger->trigger(param);
  }
  for(auto & encoder : this->encoders_) {
    ControllerParam_t cont;
    BleAdvDecoded_t decoded;
    if(encoder->decode(param, decoded.enc, cont)) {
      encoder->translate_e2g(decoded.gen, decoded.enc);
      if (this->log_command_) {
        ESP_LOGD(encoder->get_id().c_str(), "Decoded OK - tx: %d, gen: %s, enc: %s", 
                cont.tx_count_, decoded.gen.str().c_str(), encoder->to_str(decoded.enc).c_str());
      }
      for (auto & device: this->devices_) {
        if (publish && device->is_elligible(encoder->get_id(), cont)) {
          device->publish(decoded.gen, false);
        }
      }
      decoded.conf.encoding = encoder->get_encoding().c_str();
      decoded.conf.variant = encoder->get_variant().c_str();
      decoded.conf.forced_id = cont.id_;
      decoded.conf.index = cont.index_;
      if (this->log_config_) {
        ESP_LOGD(TAG, "Configuration Parameters:\n%s", decoded.conf.str().c_str());
      }
      for (auto & decoded_trigger: this->decoded_triggers_) {
        decoded_trigger->trigger(decoded);
      }
      
      if (this->check_reencoding_) {
        // Re encoding with the same parameters to check if it gives the same output
        BleAdvParams params;
        std::vector< BleAdvEncCmd > re_enc_cmds;
        encoder->translate_g2e(re_enc_cmds, decoded.gen);
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
      }
    } 
  }
  return false;
}

#ifdef USE_API
void BleAdvHandler::on_raw_decode(std::string raw) {
  BleAdvParam param;
  param.from_hex_string(raw);
  this->handle_raw_param(param, false);
}
void BleAdvHandler::on_raw_listen(std::string raw) {
  BleAdvParam param;
  param.from_hex_string(raw);
  this->handle_raw_param(param, true);
}
#endif

void BleAdvHandler::setup_max_tx_power() {
  // The standard interfaces for esp32 are limited to ESP_PWR_LVL_P9, whereas some other interfaces for ESP32-C2 / C3 / ..
  // are able to go up to ESP_PWR_LVL_P20.
  // This function will simply try to setup the max value by increasing by 1 each time, and checking if it gives an error
  if (this->max_tx_power_setup_done_ || !this->use_max_tx_power_ ) {
    return;
  }

  esp_power_level_t lev_init = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
  ESP_LOGD(TAG, "Advertising TX Power enum value (NOT dBm) before max setup: %d", lev_init);

  esp_err_t ret_code = ESP_OK;
  esp_power_level_t lev_code = ESP_PWR_LVL_P9;
  while( (ret_code == ESP_OK) && (lev_code < 0xFF)) {
    ret_code = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, lev_code);
    lev_code = (esp_power_level_t)( (uint8_t)(lev_code + 1));
  }

  esp_power_level_t lev_final = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
  ESP_LOGD(TAG, "Advertising TX Power enum value (NOT dBm) after max setup: %d", lev_final);

  this->max_tx_power_setup_done_ = true;
}

void BleAdvHandler::loop() {
  // prevent any action if ble stack not ready, and stop scan if started
  if (!this->get_parent()->is_active()) {
    if (this->scan_started_) {
      this->scan_started_ = false;
      esp_err_t err = esp_ble_gap_stop_scanning();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_stop_scanning failed: %d", err);
      }
    }
    return;
  }

  // Setup and Start scan if needed
  if (!this->scan_started_ && this->scan_activated_) {
    esp_err_t err = esp_ble_gap_set_scan_params(&this->scan_params_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %d", err);
    } else {
      err = esp_ble_gap_start_scanning(0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_scanning failed: %d", err);
      } else {
        this->scan_started_ = true;
      }
    }
  }

  // Cleanup expired packets
  this->processed_packets_.remove_if( [&](BleAdvParam & p){ return p.duration_ < millis(); } );

  // swap packet list to further process it outside of the lock
  std::list< BleAdvParam > new_packets;
  if (xSemaphoreTake(this->scan_result_lock_, 5L / portTICK_PERIOD_MS)) {
    std::swap(this->new_packets_, new_packets);
    xSemaphoreGive(this->scan_result_lock_);
  } else {
    ESP_LOGW(TAG, "loop - failed to take lock");
  }

  // handle new packets
  for (auto & param: new_packets) {
    auto idx = std::find_if(this->processed_packets_.begin(), this->processed_packets_.end(),
                              [&](BleAdvParam & p){ return (p == param) || p.is_data_equal(param); });
    if (idx == this->processed_packets_.end()) {
      this->handle_raw_param(param, true);
      this->processed_packets_.emplace_back(std::move(param));
    }
  }

  // Process advertizing
  if (this->adv_stop_time_ == 0) {
    // No packet is being advertised, process with clean-up IF already processed once and requested for removal
    this->packets_.remove_if([&](BleAdvProcess & p){ return p.processed_once_ && p.to_be_removed_; } );
    // if packets to be advertised, advertise the front one
    if (!this->packets_.empty()) {
      BleAdvParam & packet = this->packets_.front().param_;
      this->setup_max_tx_power();
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

void BleAdvHandler::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    BleAdvParam packet;
    packet.from_raw(param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
    packet.duration_ = millis() + 60 * 1000;
    if(xSemaphoreTake(this->scan_result_lock_, 5L / portTICK_PERIOD_MS)) {
      this->new_packets_.emplace_back(std::move(packet));
      xSemaphoreGive(this->scan_result_lock_);
    } else {
      ESP_LOGW(TAG, "evt - failed to take lock");
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

void BleAdvDevice::init(const std::string & encoding, const std::string & variant) {
  this->get_parent()->register_device(this);
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

bool BleAdvDevice::is_elligible(const std::string & enc_id, const ControllerParam_t & cont) {
  return (this->encoders_.size() == 1) && (this->encoders_.front()->get_id() == enc_id) 
              && (cont.id_ == this->params_.id_) && (cont.index_ == this->params_.index_);
}

} // namespace ble_adv_handler
} // namespace esphome
