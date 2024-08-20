#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"

#include <esp_gap_ble_api.h>
#include <vector>
#include <list>

namespace esphome {

#ifdef USE_ESP32_BLE_CLIENT
namespace esp32_ble_tracker {
  class ESPBTDevice;
}
#endif

namespace ble_adv_handler {

enum CommandType {
  NOCMD = 0,
  PAIR = 1,
  UNPAIR = 2,
  CUSTOM = 3,
  ALL_OFF = 4,
  LIGHT_ON = 13,
  LIGHT_OFF = 14,
  LIGHT_DIM = 15,
  LIGHT_CCT = 16,
  LIGHT_WCOLOR = 17,
  LIGHT_SEC_ON = 18,
  LIGHT_SEC_OFF = 19,
  FAN_ONOFF_SPEED = 33,
  FAN_DIR = 34,
  FAN_OSC = 35,
};

/**
  Controller Parameters
 */
struct ControllerParam_t {
  uint32_t id_ = 0;
  uint8_t tx_count_ = 0;
  uint8_t index_ = 0;
  uint16_t seed_ = 0;
};

static constexpr size_t MAX_PACKET_LEN = 31;

class BleAdvParam
{
public:
  BleAdvParam() {};
  BleAdvParam(BleAdvParam&&) = default;
  BleAdvParam& operator=(BleAdvParam&&) = default;

  void from_raw(const uint8_t * buf, size_t len);
  void from_hex_string(std::string & raw);
  void init_with_ble_param(uint8_t ad_flag, uint8_t data_type);

  bool has_ad_flag() const { return this->ad_flag_index_ != MAX_PACKET_LEN; }
  uint8_t get_ad_flag() const { return this->buf_[this->ad_flag_index_ + 2]; }

  bool has_data() const { return this->data_index_ != MAX_PACKET_LEN; }
  void set_data_len(size_t len);
  uint8_t get_data_len() const { return this->buf_[this->data_index_] - 1; }
  uint8_t get_data_type() const { return this->buf_[this->data_index_ + 1]; }
  uint8_t * get_data_buf() {  return this->buf_ + this->data_index_ + 2; }
  const uint8_t * get_const_data_buf() const { return this->buf_ + this->data_index_ + 2; }

  uint8_t * get_full_buf() { return this->buf_; }
  uint8_t get_full_len() { return this->len_; }

  bool operator==(const BleAdvParam & comp) { return std::equal(comp.buf_, comp.buf_ + MAX_PACKET_LEN, this->buf_); }

  uint32_t duration_{100};

protected:
  uint8_t buf_[MAX_PACKET_LEN]{0};
  size_t len_{0};
  size_t ad_flag_index_{MAX_PACKET_LEN};
  size_t data_index_{MAX_PACKET_LEN};
};

class BleAdvProcess
{
public:
  BleAdvProcess(uint32_t id, BleAdvParam && param): param_(std::move(param)), id_(id) {}
  BleAdvParam param_;
  uint32_t id_{0};
  bool processed_once_{false};
  bool to_be_removed_{false};

  // Only move operators to avoid data copy
  BleAdvProcess(BleAdvProcess&&) = default;
  BleAdvProcess& operator=(BleAdvProcess&&) = default;
} ;

class BleAdvGenCmd
{
public:
  BleAdvGenCmd(CommandType cmd = CommandType::NOCMD): cmd(cmd) {}
  std::string str();

  CommandType cmd;
  uint8_t param = 0;
  float args[2]{0};
};

class BleAdvEncCmd
{
public:
  BleAdvEncCmd(uint8_t acmd = 0): cmd(acmd) {}
  uint8_t cmd;
  uint8_t param1 = 0;
  uint8_t args[3]{0};
};

class CommandTranslator 
{
public:
  virtual void g2e_cmd(const BleAdvGenCmd & gen_cmd, BleAdvEncCmd & enc_cmd) const = 0;
  virtual void e2g_cmd(const BleAdvEncCmd & enc_cmd, BleAdvGenCmd & gen_cmd) const = 0;
};

/**
  BleAdvEncoder: 
    Base class for encoders, for registration in the BleAdvHandler
    and usage by BleAdvController
 */
class BleAdvEncoder {
public:
  BleAdvEncoder(const std::string & encoding, const std::string & variant): 
      id_(ID(encoding, variant)), encoding_(encoding), variant_(variant) {}

  static constexpr const char * VARIANT_ALL = "All";
  static std::string ID(const std::string & encoding, const std::string & variant) { return (encoding + " - " + variant); }
  const std::string & get_id() const { return this->id_; }
  const std::string & get_encoding() const { return this->encoding_; }
  const std::string & get_variant() const { return this->variant_; }

  void set_ble_param(uint8_t ad_flag, uint8_t adv_data_type){ this->ad_flag_ = ad_flag; this->adv_data_type_ = adv_data_type; }
  bool is_ble_param(uint8_t ad_flag, uint8_t adv_data_type) const { return this->ad_flag_ == ad_flag && this->adv_data_type_ == adv_data_type; }
  void set_header(const std::vector< uint8_t > && header) { this->header_ = header; }
  void set_translator(CommandTranslator * trans) { this->translator_ = trans; }

  virtual void encode(std::vector< BleAdvParam > & params, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const;
  virtual bool decode(const BleAdvParam & packet, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const;
  virtual void translate_e2g(BleAdvGenCmd & gen_cmd, const BleAdvEncCmd & enc_cmd) const;
  virtual void translate_g2e(std::vector< BleAdvEncCmd > & enc_cmds, const BleAdvGenCmd & gen_cmd) const;
  virtual std::string to_str(const BleAdvEncCmd & enc_cmd) const = 0;

protected:
  virtual bool decode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const = 0;
  virtual void encode(uint8_t* buf, BleAdvEncCmd & enc_cmd, ControllerParam_t & cont) const = 0;

  // utils for encoding
  void reverse_all(uint8_t* buf, uint8_t len) const;
  void whiten(uint8_t *buf, size_t len, uint8_t seed) const;

  // encoder identifiers
  std::string id_;
  std::string encoding_;
  std::string variant_;

  // BLE parameters
  uint8_t ad_flag_{0x00};
  uint8_t adv_data_type_{ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE};

  // Common parameters
  std::vector< uint8_t > header_;
  size_t len_{0};

  // Translator
  CommandTranslator * translator_ = nullptr;
};

#define ENSURE_EQ(param1, param2, ...) if ((param1) != (param2)) { ESP_LOGD(this->id_.c_str(), __VA_ARGS__); return false; }

/**
  BleAdvHandler: Central class instanciated only ONCE
  It owns the list of registered encoders and their simplified access, to be used by Controllers.
  It owns the centralized Advertiser allowing to advertise multiple messages at the same time 
    with handling of prioritization and parallel send when possible
 */
class BleAdvHandler: public Component
#ifdef USE_API
  , public api::CustomAPIDevice
#endif
{
public:
  // component handling
  void setup() override;
  void loop() override;

  // Encoder registration and access
  void add_encoder(BleAdvEncoder * encoder);
  BleAdvEncoder * get_encoder(const std::string & id);
  std::vector<std::string> get_ids(const std::string & encoding);

  // Advertiser
  uint16_t add_to_advertiser(std::vector< BleAdvParam > & params);
  void remove_from_advertiser(uint16_t msg_id);

  // identify which encoder is relevant for the param, decode and log Action and Controller parameters
  bool identify_param(const BleAdvParam & param, bool ignore_ble_param);

  // Listener
#ifdef USE_ESP32_BLE_CLIENT
  void capture(const esp32_ble_tracker::ESPBTDevice & device, bool ignore_ble_param = true, uint16_t rem_time = 60);
#endif

#ifdef USE_API
  // HA service to decode
  void on_raw_decode(std::string raw);
#endif

protected:
  // ref to registered encoders
  std::vector< BleAdvEncoder * > encoders_;

  // packets being advertised
  std::list< BleAdvProcess > packets_;
  uint16_t id_count = 1;
  uint32_t adv_stop_time_ = 0;

  esp_ble_adv_params_t adv_params_ = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x20,
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = { 0x00 },
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };

  // Packets already captured once
  std::list< BleAdvParam > listen_packets_;
};


//  Base class to define a dynamic Configuration
template < class BaseEntity >
class BleAdvDynConfig: public BaseEntity
{
public:
  void init(const char * name, const StringRef & parent_name) {
    // Due to the use of sh... StringRef, we are forced to keep a ref on the built string...
    this->ref_name_ = std::string(parent_name) + " - " + std::string(name);
    this->set_object_id(this->ref_name_.c_str());
    this->set_name(this->ref_name_.c_str());
    this->set_entity_category(EntityCategory::ENTITY_CATEGORY_CONFIG);
    this->sub_init();
    this->publish_state(this->state);
  }

  // register to App and restore from config / saved data
  virtual void sub_init() = 0;

protected:
  std::string ref_name_;
  ESPPreferenceObject rtc_{nullptr};
};

/**
  BleAdvSelect: basic implementation of 'Select' to handle configuration choice from HA directly
 */
class BleAdvSelect: public BleAdvDynConfig < select::Select > {
protected:
  void control(const std::string &value) override;
  void sub_init() override;
};

/**
  BleAdvNumber: basic implementation of 'Number' to handle duration(s) choice from HA directly
 */
class BleAdvNumber: public BleAdvDynConfig < number::Number > {
protected:
  void control(float value) override;
  void sub_init() override;
};

/**
  Base Device class
 */
class BleAdvDevice: public EntityBase, public Parented < ble_adv_handler::BleAdvHandler >
{
public:
  void set_forced_id(uint32_t forced_id) { this->params_.id_ = forced_id; }
  void set_forced_id(const std::string & str_id) { this->params_.id_ = fnv1_hash(str_id); }
  void set_index(uint8_t index) { this->params_.index_ = index; }
  void set_encoding_and_variant(const std::string & encoding, const std::string & variant);
  void refresh_encoder(std::string id, size_t index);

protected:
  ControllerParam_t params_;
  BleAdvSelect select_encoding_;
  std::vector< BleAdvEncoder *> encoders_;
};

} //namespace ble_adv_handler
} //namespace esphome
