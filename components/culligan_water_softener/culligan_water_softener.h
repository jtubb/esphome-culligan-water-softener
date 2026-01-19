/**
 * Culligan Water Softener BLE Component for ESPHome
 *
 * Handles BLE communication with Culligan CS Meter Soft water softeners
 * using Nordic UART Service (NUS) protocol.
 *
 * Protocol: BIG-ENDIAN for all multi-byte values
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/core/log.h"

#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace culligan_water_softener {

// Packet type identifiers
static const uint8_t PACKET_TYPE_HANDSHAKE[] = {0x74, 0x74};  // "tt"
static const uint8_t PACKET_TYPE_STATUS[] = {0x75, 0x75};     // "uu"
static const uint8_t PACKET_TYPE_SETTINGS[] = {0x76, 0x76};   // "vv"
static const uint8_t PACKET_TYPE_STATISTICS[] = {0x77, 0x77}; // "ww"
static const uint8_t PACKET_TYPE_KEEPALIVE[] = {0x78, 0x78};  // "xx"

// End markers for packet validation
static const uint8_t END_MARKER_UU_0 = 0x39;  // '9'
static const uint8_t END_MARKER_UU_1 = 0x3A;  // ':'
static const uint8_t END_MARKER_VV_0 = 0x42;  // 'B'
static const uint8_t END_MARKER_VV_1 = 0x43;  // 'C'
static const uint8_t END_MARKER_WW_0 = 0x46;  // 'F'

// Authentication constants
static const uint8_t AUTH_REQUIRED_FLAG = 0x80;
static const uint16_t DEFAULT_PASSWORD = 1234;

/**
 * CRC8 implementation for authentication
 * Matches the APK's CsCrc8 class
 */
class CsCrc8 {
 public:
  CsCrc8() : polynomial_(213), seed_(0) {}

  void set_options(uint8_t polynomial, uint8_t seed) {
    polynomial_ = polynomial;
    seed_ = seed;
  }

  /**
   * Compute CRC8 using the legacy algorithm.
   * Matches CsCrc8.computeLegacy() in the APK.
   */
  uint8_t compute_legacy(uint8_t value) {
    uint8_t b = value;
    uint8_t b2 = seed_;

    for (int i = 0; i < 8; i++) {
      bool z = (b2 & 0x80) != 0;
      b2 = (b2 << 1) & 0xFF;
      if ((b & 0x80) != 0) {
        b2 = (b2 | 1) & 0xFF;
      }
      b = (b << 1) & 0xFF;
      if (z) {
        b2 = (b2 ^ polynomial_) & 0xFF;
      }
    }

    seed_ = b2;
    return b2;
  }

  /**
   * Compute CRC8 using the standard algorithm.
   * Matches CsCrc8.compute(int) in the APK.
   */
  uint8_t compute(uint8_t value) {
    uint8_t b = (value ^ seed_) & 0xFF;

    for (int i = 0; i < 8; i++) {
      if ((b & 0x80) > 0) {
        b = ((b << 1) ^ polynomial_) & 0xFF;
      } else {
        b = (b << 1) & 0xFF;
      }
    }

    seed_ = b;
    return b;
  }

  uint8_t get_seed() const { return seed_; }

 protected:
  uint8_t polynomial_;
  uint8_t seed_;
};

// Forward declarations for button classes
class CulliganWaterSoftener;

class RegenNowButton : public button::Button, public Parented<CulliganWaterSoftener> {
 public:
  void press_action() override;
};

class RegenNextButton : public button::Button, public Parented<CulliganWaterSoftener> {
 public:
  void press_action() override;
};

class SyncTimeButton : public button::Button, public Parented<CulliganWaterSoftener> {
 public:
  void press_action() override;
};

class ResetGallonsButton : public button::Button, public Parented<CulliganWaterSoftener> {
 public:
  void press_action() override;
};

class ResetRegensButton : public button::Button, public Parented<CulliganWaterSoftener> {
 public:
  void press_action() override;
};

// Forward declaration for switch class
class DisplaySwitch : public switch_::Switch, public Parented<CulliganWaterSoftener> {
 public:
  void write_state(bool state) override;
};

// Forward declarations for number classes
class HardnessNumber : public number::Number, public Parented<CulliganWaterSoftener> {
 public:
  void control(float value) override;
};

class RegenTimeHourNumber : public number::Number, public Parented<CulliganWaterSoftener> {
 public:
  void control(float value) override;
};

class ReserveCapacityNumber : public number::Number, public Parented<CulliganWaterSoftener> {
 public:
  void control(float value) override;
};

class SaltLevelNumber : public number::Number, public Parented<CulliganWaterSoftener> {
 public:
  void control(float value) override;
};

/**
 * Main component class for Culligan Water Softener
 */
class CulliganWaterSoftener : public esphome::ble_client::BLEClientNode, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                          esp_ble_gattc_cb_param_t *param) override;

  // Configuration setters
  void set_password(uint16_t password) { password_ = password; }
  void set_poll_interval(uint32_t interval_ms) { poll_interval_ms_ = interval_ms; }

  // Sensor setters
  void set_current_flow_sensor(sensor::Sensor *sensor) { current_flow_sensor_ = sensor; }
  void set_soft_water_remaining_sensor(sensor::Sensor *sensor) { soft_water_remaining_sensor_ = sensor; }
  void set_water_usage_today_sensor(sensor::Sensor *sensor) { water_usage_today_sensor_ = sensor; }
  void set_peak_flow_today_sensor(sensor::Sensor *sensor) { peak_flow_today_sensor_ = sensor; }
  void set_water_hardness_sensor(sensor::Sensor *sensor) { water_hardness_sensor_ = sensor; }
  void set_brine_level_sensor(sensor::Sensor *sensor) { brine_level_sensor_ = sensor; }
  void set_avg_daily_usage_sensor(sensor::Sensor *sensor) { avg_daily_usage_sensor_ = sensor; }
  void set_days_until_regen_sensor(sensor::Sensor *sensor) { days_until_regen_sensor_ = sensor; }
  void set_total_gallons_sensor(sensor::Sensor *sensor) { total_gallons_sensor_ = sensor; }
  void set_total_regens_sensor(sensor::Sensor *sensor) { total_regens_sensor_ = sensor; }
  void set_battery_level_sensor(sensor::Sensor *sensor) { battery_level_sensor_ = sensor; }

  // New sensor setters for Phase 3
  void set_reserve_capacity_sensor(sensor::Sensor *sensor) { reserve_capacity_sensor_ = sensor; }
  void set_resin_capacity_sensor(sensor::Sensor *sensor) { resin_capacity_sensor_ = sensor; }
  void set_prefill_duration_sensor(sensor::Sensor *sensor) { prefill_duration_sensor_ = sensor; }
  void set_soak_duration_sensor(sensor::Sensor *sensor) { soak_duration_sensor_ = sensor; }
  void set_backwash_time_sensor(sensor::Sensor *sensor) { backwash_time_sensor_ = sensor; }
  void set_brine_draw_time_sensor(sensor::Sensor *sensor) { brine_draw_time_sensor_ = sensor; }
  void set_rapid_rinse_time_sensor(sensor::Sensor *sensor) { rapid_rinse_time_sensor_ = sensor; }
  void set_brine_refill_time_sensor(sensor::Sensor *sensor) { brine_refill_time_sensor_ = sensor; }

  // Text sensor setters
  void set_firmware_version_sensor(text_sensor::TextSensor *sensor) { firmware_version_sensor_ = sensor; }
  void set_device_time_sensor(text_sensor::TextSensor *sensor) { device_time_sensor_ = sensor; }
  void set_regen_time_sensor(text_sensor::TextSensor *sensor) { regen_time_sensor_ = sensor; }

  // Binary sensor setters
  void set_display_off_sensor(binary_sensor::BinarySensor *sensor) { display_off_sensor_ = sensor; }
  void set_bypass_active_sensor(binary_sensor::BinarySensor *sensor) { bypass_active_sensor_ = sensor; }
  void set_shutoff_active_sensor(binary_sensor::BinarySensor *sensor) { shutoff_active_sensor_ = sensor; }
  void set_regen_active_sensor(binary_sensor::BinarySensor *sensor) { regen_active_sensor_ = sensor; }

  // Button setters
  void set_regen_now_button(RegenNowButton *button) { regen_now_button_ = button; }
  void set_regen_next_button(RegenNextButton *button) { regen_next_button_ = button; }
  void set_sync_time_button(SyncTimeButton *button) { sync_time_button_ = button; }
  void set_reset_gallons_button(ResetGallonsButton *button) { reset_gallons_button_ = button; }
  void set_reset_regens_button(ResetRegensButton *button) { reset_regens_button_ = button; }

  // Switch setters
  void set_display_switch(DisplaySwitch *sw) { display_switch_ = sw; }

  // Number setters
  void set_hardness_number(HardnessNumber *num) { hardness_number_ = num; }
  void set_regen_time_hour_number(RegenTimeHourNumber *num) { regen_time_hour_number_ = num; }
  void set_reserve_capacity_number(ReserveCapacityNumber *num) { reserve_capacity_number_ = num; }
  void set_salt_level_number(SaltLevelNumber *num) { salt_level_number_ = num; }

  // Write command methods (for buttons/switches/numbers)
  void send_regen_now();
  void send_regen_next();
  void send_sync_time();
  void send_reset_gallons();
  void send_reset_regens();
  void send_set_display(bool on);
  void send_set_hardness(uint8_t hardness);
  void send_set_regen_time(uint8_t hour, bool is_pm);
  void send_set_reserve_capacity(uint8_t percent);
  void send_set_salt_level(float lbs);

  // Request data from device
  void request_data();

 protected:
  // BLE characteristic handles
  uint16_t tx_handle_{0};
  uint16_t rx_handle_{0};

  // Protocol parser state
  std::vector<uint8_t> buffer_;
  bool handshake_received_{false};
  bool authenticated_{false};
  uint8_t status_packet_count_{0};
  uint8_t connection_counter_{0};
  uint8_t firmware_major_{0};
  uint8_t firmware_minor_{0};
  bool auth_required_{false};

  // Brine tank configuration (from uu-1)
  uint8_t brine_tank_type_{16};
  uint8_t brine_fill_height_{0};
  uint8_t brine_refill_time_{0};
  uint8_t brine_regens_remaining_{0xFF};
  bool brine_tank_configured_{false};

  // Current flag states
  uint8_t current_flags_{0};
  bool regen_active_{false};

  // Configuration
  uint16_t password_{DEFAULT_PASSWORD};
  uint32_t poll_interval_ms_{60000};  // Default 60 seconds
  uint32_t last_poll_time_{0};

  // Sensor pointers
  sensor::Sensor *current_flow_sensor_{nullptr};
  sensor::Sensor *soft_water_remaining_sensor_{nullptr};
  sensor::Sensor *water_usage_today_sensor_{nullptr};
  sensor::Sensor *peak_flow_today_sensor_{nullptr};
  sensor::Sensor *water_hardness_sensor_{nullptr};
  sensor::Sensor *brine_level_sensor_{nullptr};
  sensor::Sensor *avg_daily_usage_sensor_{nullptr};
  sensor::Sensor *days_until_regen_sensor_{nullptr};
  sensor::Sensor *total_gallons_sensor_{nullptr};
  sensor::Sensor *total_regens_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};

  // New sensors for Phase 3
  sensor::Sensor *reserve_capacity_sensor_{nullptr};
  sensor::Sensor *resin_capacity_sensor_{nullptr};
  sensor::Sensor *prefill_duration_sensor_{nullptr};
  sensor::Sensor *soak_duration_sensor_{nullptr};
  sensor::Sensor *backwash_time_sensor_{nullptr};
  sensor::Sensor *brine_draw_time_sensor_{nullptr};
  sensor::Sensor *rapid_rinse_time_sensor_{nullptr};
  sensor::Sensor *brine_refill_time_sensor_{nullptr};

  // Text sensors
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
  text_sensor::TextSensor *device_time_sensor_{nullptr};
  text_sensor::TextSensor *regen_time_sensor_{nullptr};

  // Binary sensors
  binary_sensor::BinarySensor *display_off_sensor_{nullptr};
  binary_sensor::BinarySensor *bypass_active_sensor_{nullptr};
  binary_sensor::BinarySensor *shutoff_active_sensor_{nullptr};
  binary_sensor::BinarySensor *regen_active_sensor_{nullptr};

  // Buttons
  RegenNowButton *regen_now_button_{nullptr};
  RegenNextButton *regen_next_button_{nullptr};
  SyncTimeButton *sync_time_button_{nullptr};
  ResetGallonsButton *reset_gallons_button_{nullptr};
  ResetRegensButton *reset_regens_button_{nullptr};

  // Switches
  DisplaySwitch *display_switch_{nullptr};

  // Numbers
  HardnessNumber *hardness_number_{nullptr};
  RegenTimeHourNumber *regen_time_hour_number_{nullptr};
  ReserveCapacityNumber *reserve_capacity_number_{nullptr};
  SaltLevelNumber *salt_level_number_{nullptr};

  // Protocol parsing methods
  void handle_notification(const uint8_t *data, uint16_t length);
  void process_buffer();
  void parse_handshake();
  void parse_status_packet();
  void parse_settings_packet();
  void parse_statistics_packet();

  // Authentication methods
  void send_authentication();
  std::vector<uint8_t> build_auth_packet();
  uint8_t get_random_polynomial();

  // Write command helper
  void write_command(const uint8_t *data, size_t length);

  // Big-endian helper methods (protocol uses BIG-ENDIAN)
  uint16_t read_uint16_be(size_t offset);
  uint32_t read_uint24_be(size_t offset);
  uint32_t read_uint32_be(size_t offset);

  // Battery level lookup
  float get_battery_percent(uint8_t raw);

  // Brine tank salt calculation
  float calculate_salt_remaining();
  float get_tank_multiplier(uint8_t tank_type);

  // Time formatting
  std::string format_time_12h(uint8_t hour, uint8_t minute, uint8_t am_pm);
  std::string format_time_24h(uint8_t hour, uint8_t minute);

  // Flag parsing
  void parse_flags(uint8_t flags);
};

}  // namespace culligan_water_softener
}  // namespace esphome

#endif  // USE_ESP32
