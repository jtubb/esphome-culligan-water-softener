/**
 * Culligan Water Softener BLE Component Implementation
 *
 * Ported from Python protocol.py to C++ for ESP32 native execution
 * Protocol uses BIG-ENDIAN for all multi-byte values
 */

#include "culligan_water_softener.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32

#include <esp_random.h>
#include <ctime>

namespace esphome {
namespace culligan_water_softener {

static const char *TAG = "culligan_water_softener";

// Allowed CRC8 polynomials (4-5 bits set)
static const uint8_t ALLOWED_POLYNOMIALS[] = {
  0x1E, 0x1D, 0x2D, 0x2E, 0x35, 0x36, 0x39, 0x3A, 0x3C, 0x47,
  0x4B, 0x4D, 0x4E, 0x53, 0x55, 0x56, 0x59, 0x5A, 0x5C, 0x63,
  0x65, 0x66, 0x69, 0x6A, 0x6C, 0x71, 0x72, 0x74, 0x78, 0x87,
  0x8B, 0x8D, 0x8E, 0x93, 0x95, 0x96, 0x99, 0x9A, 0x9C, 0xA3,
  0xA5, 0xA6, 0xA9, 0xAA, 0xAC, 0xB1, 0xB2, 0xB4, 0xB8, 0xC3,
  0xC5, 0xC6, 0xC9, 0xCA, 0xCC, 0xD1, 0xD2, 0xD4, 0xD8, 0xE1,
  0xE2, 0xE4, 0xE8, 0xF0
};
static const size_t NUM_POLYNOMIALS = sizeof(ALLOWED_POLYNOMIALS) / sizeof(ALLOWED_POLYNOMIALS[0]);

void CulliganWaterSoftener::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Culligan Water Softener...");
}

void CulliganWaterSoftener::loop() {
  // Periodic data request
  uint32_t now = millis();
  if (this->authenticated_ && (now - this->last_poll_time_ >= this->poll_interval_ms_)) {
    this->last_poll_time_ = now;
    this->request_data();
  }
}

void CulliganWaterSoftener::dump_config() {
  ESP_LOGCONFIG(TAG, "Culligan Water Softener:");
  ESP_LOGCONFIG(TAG, "  Password: %d", this->password_);
  ESP_LOGCONFIG(TAG, "  Poll Interval: %d ms", this->poll_interval_ms_);
  LOG_SENSOR("  ", "Current Flow", this->current_flow_sensor_);
  LOG_SENSOR("  ", "Soft Water Remaining", this->soft_water_remaining_sensor_);
  LOG_SENSOR("  ", "Water Usage Today", this->water_usage_today_sensor_);
  LOG_SENSOR("  ", "Peak Flow Today", this->peak_flow_today_sensor_);
  LOG_SENSOR("  ", "Water Hardness", this->water_hardness_sensor_);
  LOG_SENSOR("  ", "Brine Level", this->brine_level_sensor_);
  LOG_SENSOR("  ", "Avg Daily Usage", this->avg_daily_usage_sensor_);
  LOG_SENSOR("  ", "Days Until Regen", this->days_until_regen_sensor_);
  LOG_SENSOR("  ", "Total Gallons", this->total_gallons_sensor_);
  LOG_SENSOR("  ", "Total Regens", this->total_regens_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Reserve Capacity", this->reserve_capacity_sensor_);
  LOG_SENSOR("  ", "Resin Capacity", this->resin_capacity_sensor_);
  LOG_BINARY_SENSOR("  ", "Display Off", this->display_off_sensor_);
  LOG_BINARY_SENSOR("  ", "Bypass Active", this->bypass_active_sensor_);
  LOG_BINARY_SENSOR("  ", "Shutoff Active", this->shutoff_active_sensor_);
  LOG_BINARY_SENSOR("  ", "Regen Active", this->regen_active_sensor_);
}

void CulliganWaterSoftener::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                                esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected to water softener");
        this->authenticated_ = false;
        this->handshake_received_ = false;
      }
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "Disconnected from water softener");
      this->buffer_.clear();
      this->handshake_received_ = false;
      this->authenticated_ = false;
      this->status_packet_count_ = 0;
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // Find TX characteristic (notifications) using UUID object
      auto service_uuid = esp32_ble_tracker::ESPBTUUID::from_raw("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
      auto tx_char_uuid = esp32_ble_tracker::ESPBTUUID::from_raw("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

      auto *chr = this->parent_->get_characteristic(service_uuid, tx_char_uuid);
      if (chr == nullptr) {
        ESP_LOGE(TAG, "TX characteristic not found");
        break;
      }
      this->tx_handle_ = chr->handle;

      // Subscribe to notifications
      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(),
                                                       this->parent_->get_remote_bda(), chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      } else {
        ESP_LOGI(TAG, "Subscribed to TX notifications");
      }

      // Find RX characteristic (write)
      auto rx_char_uuid = esp32_ble_tracker::ESPBTUUID::from_raw("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
      chr = this->parent_->get_characteristic(service_uuid, rx_char_uuid);
      if (chr != nullptr) {
        this->rx_handle_ = chr->handle;
        ESP_LOGI(TAG, "Found RX characteristic for write commands");
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGD(TAG, "Notification registration complete, sending handshake request");
      // Send handshake request: 't' x 20
      uint8_t handshake_req[20];
      memset(handshake_req, 0x74, 20);  // 't'
      this->write_command(handshake_req, 20);
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->tx_handle_) {
        ESP_LOGD(TAG, "Received notification: %d bytes", param->notify.value_len);
        this->handle_notification(param->notify.value, param->notify.value_len);
      }
      break;
    }

    default:
      break;
  }
}

void CulliganWaterSoftener::handle_notification(const uint8_t *data, uint16_t length) {
  // Log raw notification data for debugging
  if (length <= 20) {
    char hex_str[61];
    for (uint16_t i = 0; i < length; i++) {
      snprintf(hex_str + i * 3, 4, "%02X ", data[i]);
    }
    ESP_LOGD(TAG, "Notification (%d bytes): %s", length, hex_str);
  }

  // Add data to buffer
  for (uint16_t i = 0; i < length; i++) {
    this->buffer_.push_back(data[i]);
  }

  ESP_LOGD(TAG, "Buffer size: %d bytes", this->buffer_.size());

  // Try to parse packet
  if (this->buffer_.size() >= 2) {
    this->parse_packet();
  }
}

void CulliganWaterSoftener::parse_packet() {
  // Check packet type (first 2 bytes)
  uint8_t type0 = this->buffer_[0];
  uint8_t type1 = this->buffer_[1];

  if (type0 == 0x74 && type1 == 0x74) {  // "tt" - Handshake
    this->parse_handshake();
  } else if (type0 == 0x75 && type1 == 0x75) {  // "uu" - Status
    this->parse_status_packet();
  } else if (type0 == 0x76 && type1 == 0x76) {  // "vv" - Settings
    this->parse_settings_packet();
  } else if (type0 == 0x77 && type1 == 0x77) {  // "ww" - Statistics
    this->parse_statistics_packet();
  } else if (type0 == 0x78 && type1 == 0x78) {  // "xx" - Keepalive
    ESP_LOGD(TAG, "Keepalive packet received");
    this->buffer_.clear();
  } else {
    ESP_LOGW(TAG, "Unknown packet type: 0x%02X 0x%02X", type0, type1);
    this->buffer_.clear();
  }
}

void CulliganWaterSoftener::parse_handshake() {
  if (this->buffer_.size() < 18) {
    ESP_LOGD(TAG, "Handshake packet incomplete, waiting for more data");
    return;
  }

  // tt packet structure (per PROTOCOL.md):
  // Offset 5: Firmware major version
  // Offset 6: Firmware minor version (BCD)
  // Offset 7: Auth status (0x80 = auth required)
  // Offset 11: Connection counter

  this->firmware_major_ = this->buffer_[5];
  this->firmware_minor_ = this->buffer_[6];
  uint8_t auth_flag = this->buffer_[7];
  this->connection_counter_ = this->buffer_[11];

  // Authentication is required for firmware < 6.0, regardless of the flag byte
  // The flag byte (0x80) may not always be set correctly by the device
  this->auth_required_ = (this->firmware_major_ < 6) || ((auth_flag & AUTH_REQUIRED_FLAG) != 0);

  char fw_version[16];
  snprintf(fw_version, sizeof(fw_version), "C%d.%d", this->firmware_major_, this->firmware_minor_);

  ESP_LOGI(TAG, "Handshake received, firmware: %s, auth flag: 0x%02X, counter: %d, will auth: %s",
           fw_version, auth_flag, this->connection_counter_, this->auth_required_ ? "yes" : "no");

  if (this->firmware_version_sensor_ != nullptr) {
    this->firmware_version_sensor_->publish_state(fw_version);
  }

  this->handshake_received_ = true;
  this->buffer_.clear();

  // Send authentication if required (firmware < 6.0)
  if (this->auth_required_) {
    ESP_LOGI(TAG, "Sending authentication with password %d...", this->password_);
    this->send_authentication();
  } else {
    // No auth needed, request data directly
    this->authenticated_ = true;
    this->request_data();
  }
}

void CulliganWaterSoftener::parse_status_packet() {
  if (this->buffer_.size() < 20) {
    ESP_LOGD(TAG, "Status packet incomplete, waiting for more data");
    return;
  }

  // Packet number (which status packet this is - 0-5)
  uint8_t packet_num = this->buffer_[2];

  ESP_LOGD(TAG, "Status packet #%d (end marker: 0x%02X)", packet_num, this->buffer_[19]);

  if (packet_num == 0) {
    // uu-0: Real-time data (per PROTOCOL.md)
    // Offset 3: Hour (1-12)
    // Offset 4: Minute (0-59)
    // Offset 5: AM/PM (0=AM, 1=PM)
    // Offset 6: Battery level (lookup table)
    // Offset 7-8: Current flow (BE, ÷100 = GPM)
    // Offset 9-10: Soft water remaining (BE, gallons)
    // Offset 11-12: Water usage today (BE, gallons)
    // Offset 13-14: Peak flow today (BE, ÷100 = GPM)
    // Offset 15: Water hardness (GPG)
    // Offset 16: Regen hour (1-12)
    // Offset 17: Regen AM/PM (0=AM, 1=PM)
    // Offset 18: Flags
    // Offset 19: End marker '9' (0x39)

    uint8_t hour = this->buffer_[3];
    uint8_t minute = this->buffer_[4];
    uint8_t am_pm = this->buffer_[5];
    uint8_t battery_raw = this->buffer_[6];

    // Flow values use BIG-ENDIAN and ÷100 scaling
    uint16_t flow_raw = this->read_uint16_be(7);
    float current_flow = flow_raw / 100.0f;

    uint16_t soft_water = this->read_uint16_be(9);
    uint16_t usage_today = this->read_uint16_be(11);

    uint16_t peak_flow_raw = this->read_uint16_be(13);
    float peak_flow = peak_flow_raw / 100.0f;

    uint8_t hardness = this->buffer_[15];
    uint8_t regen_hour = this->buffer_[16];
    uint8_t regen_am_pm = this->buffer_[17];
    uint8_t flags = this->buffer_[18];

    // Get battery percentage using lookup table
    float battery_pct = this->get_battery_percent(battery_raw);

    // Parse flags and update binary sensors
    this->parse_flags(flags);

    // Publish sensor values
    if (this->device_time_sensor_ != nullptr) {
      this->device_time_sensor_->publish_state(this->format_time_12h(hour, minute, am_pm));
    }

    if (this->battery_level_sensor_ != nullptr) {
      this->battery_level_sensor_->publish_state(battery_pct);
    }

    if (this->current_flow_sensor_ != nullptr) {
      this->current_flow_sensor_->publish_state(current_flow);
    }

    if (this->soft_water_remaining_sensor_ != nullptr) {
      this->soft_water_remaining_sensor_->publish_state(soft_water);
    }

    if (this->water_usage_today_sensor_ != nullptr) {
      this->water_usage_today_sensor_->publish_state(usage_today);
    }

    if (this->peak_flow_today_sensor_ != nullptr) {
      this->peak_flow_today_sensor_->publish_state(peak_flow);
    }

    if (this->water_hardness_sensor_ != nullptr) {
      this->water_hardness_sensor_->publish_state(hardness);
    }

    if (this->regen_time_sensor_ != nullptr) {
      this->regen_time_sensor_->publish_state(this->format_time_12h(regen_hour, 0, regen_am_pm));
    }

    ESP_LOGD(TAG, "Status 0: Time=%s, Battery=%.0f%%, Flow=%.2f GPM, Soft Water=%d gal, Usage=%d gal, Peak=%.2f GPM, Hardness=%d GPG",
             this->format_time_12h(hour, minute, am_pm).c_str(), battery_pct, current_flow, soft_water, usage_today, peak_flow, hardness);

  } else if (packet_num == 1) {
    // uu-1: Brine tank & regen status (per PROTOCOL.md)
    // Offset 3: Filter backwash days
    // Offset 4: Air recharge days
    // Offset 5: Position time
    // Offset 6: Position option seconds
    // Offset 7: Regen cycle position
    // Offset 8: Regen active (0=inactive, 1=active)
    // Offset 13: Brine tank regens remaining (0xFF = not configured)
    // Offset 14: Low salt alert threshold
    // Offset 15: Tank type (16, 18, 24, or 30)
    // Offset 16: Fill height (inches)
    // Offset 17: Brine refill time (minutes)
    // Offset 19: End marker ':' (0x3A)

    uint8_t regen_active = this->buffer_[8];
    uint8_t regens_remaining = this->buffer_[13];
    uint8_t tank_type = this->buffer_[15];
    uint8_t fill_height = this->buffer_[16];
    uint8_t refill_time = this->buffer_[17];

    // Update regen active state
    this->regen_active_ = (regen_active != 0);
    if (this->regen_active_sensor_ != nullptr) {
      this->regen_active_sensor_->publish_state(this->regen_active_);
    }

    // Store brine tank configuration
    this->brine_regens_remaining_ = regens_remaining;
    this->brine_tank_type_ = tank_type;
    this->brine_fill_height_ = fill_height;
    this->brine_refill_time_ = refill_time;
    this->brine_tank_configured_ = (regens_remaining != 0xFF);

    // Calculate and publish brine level if configured
    if (this->brine_level_sensor_ != nullptr) {
      if (this->brine_tank_configured_) {
        float salt_remaining = this->calculate_salt_remaining();
        this->brine_level_sensor_->publish_state(salt_remaining);
        ESP_LOGD(TAG, "Salt remaining: %.1f lbs (tank=%d\", height=%d\", refill=%d min, regens=%d)",
                 salt_remaining, tank_type, fill_height, refill_time, regens_remaining);
      } else {
        ESP_LOGD(TAG, "Brine tank not configured");
      }
    }

    ESP_LOGD(TAG, "Status 1: Regen active=%d, Tank type=%d, Regens remaining=%d",
             regen_active, tank_type, regens_remaining);
  }

  this->buffer_.clear();
  this->status_packet_count_++;
}

void CulliganWaterSoftener::parse_settings_packet() {
  if (this->buffer_.size() < 20) {
    ESP_LOGD(TAG, "Settings packet incomplete");
    return;
  }

  uint8_t packet_num = this->buffer_[2];

  ESP_LOGD(TAG, "Settings packet #%d", packet_num);

  if (packet_num == 0) {
    // vv-0: Configuration (per PROTOCOL.md)
    // Offset 3: Days until regen
    // Offset 4: Regen day override
    // Offset 5: Reserve capacity %
    // Offset 6-7: Resin grain capacity (BE, ×100)
    // Offset 8: Pre-fill enabled
    // Offset 9: Pre-fill duration (hours)
    // Offset 10: Brine soak duration (hours)
    // Offset 16: Flags
    // Offset 19: End marker 'B' (0x42)

    uint8_t days_until_regen = this->buffer_[3];
    uint8_t reserve_capacity = this->buffer_[5];
    uint16_t resin_raw = this->read_uint16_be(6);
    uint32_t resin_capacity = resin_raw * 100;  // Scale to grains
    uint8_t prefill_enabled = this->buffer_[8];
    uint8_t prefill_duration = this->buffer_[9];
    uint8_t soak_duration = this->buffer_[10];
    uint8_t flags = this->buffer_[16];

    if (this->days_until_regen_sensor_ != nullptr) {
      this->days_until_regen_sensor_->publish_state(days_until_regen);
    }

    if (this->reserve_capacity_sensor_ != nullptr) {
      this->reserve_capacity_sensor_->publish_state(reserve_capacity);
    }

    if (this->resin_capacity_sensor_ != nullptr) {
      this->resin_capacity_sensor_->publish_state(resin_capacity);
    }

    if (this->prefill_duration_sensor_ != nullptr && prefill_enabled) {
      this->prefill_duration_sensor_->publish_state(prefill_duration);
    }

    if (this->soak_duration_sensor_ != nullptr) {
      this->soak_duration_sensor_->publish_state(soak_duration);
    }

    // Parse flags (same as uu-0 byte 18)
    this->parse_flags(flags);

    ESP_LOGD(TAG, "Settings 0: Days until regen=%d, Reserve=%d%%, Resin=%lu grains",
             days_until_regen, reserve_capacity, resin_capacity);

  } else if (packet_num == 1) {
    // vv-1: Cycle times (per PROTOCOL.md)
    // Offset 3: Backwash time
    // Offset 4: Brine draw time
    // Offset 5: Rapid rinse time
    // Offset 6: Brine refill time
    // Offset 19: End marker 'C' (0x43)
    //
    // Note: High bit (0x80) = fixed/non-adjustable
    // Actual time = value & 0x7F

    uint8_t backwash_raw = this->buffer_[3];
    uint8_t brine_draw_raw = this->buffer_[4];
    uint8_t rapid_rinse_raw = this->buffer_[5];
    uint8_t brine_refill_raw = this->buffer_[6];

    // Extract actual times (mask off fixed bit)
    uint8_t backwash_time = backwash_raw & 0x7F;
    uint8_t brine_draw_time = brine_draw_raw & 0x7F;
    uint8_t rapid_rinse_time = rapid_rinse_raw & 0x7F;
    uint8_t brine_refill_time = brine_refill_raw & 0x7F;

    if (this->backwash_time_sensor_ != nullptr) {
      this->backwash_time_sensor_->publish_state(backwash_time);
    }

    if (this->brine_draw_time_sensor_ != nullptr) {
      this->brine_draw_time_sensor_->publish_state(brine_draw_time);
    }

    if (this->rapid_rinse_time_sensor_ != nullptr) {
      this->rapid_rinse_time_sensor_->publish_state(rapid_rinse_time);
    }

    if (this->brine_refill_time_sensor_ != nullptr) {
      this->brine_refill_time_sensor_->publish_state(brine_refill_time);
    }

    ESP_LOGD(TAG, "Settings 1: Backwash=%d min%s, Brine draw=%d min%s, Rapid rinse=%d min%s, Brine refill=%d min%s",
             backwash_time, (backwash_raw & 0x80) ? " (fixed)" : "",
             brine_draw_time, (brine_draw_raw & 0x80) ? " (fixed)" : "",
             rapid_rinse_time, (rapid_rinse_raw & 0x80) ? " (fixed)" : "",
             brine_refill_time, (brine_refill_raw & 0x80) ? " (fixed)" : "");
  }

  this->buffer_.clear();
}

void CulliganWaterSoftener::parse_statistics_packet() {
  if (this->buffer_.size() < 20) {
    ESP_LOGD(TAG, "Statistics packet incomplete");
    return;
  }

  uint8_t packet_num = this->buffer_[2];

  ESP_LOGD(TAG, "Statistics packet #%d", packet_num);

  if (packet_num == 0) {
    // ww-0: Totals & counters (per PROTOCOL.md)
    // Offset 3-4: Current flow (BE, ÷100 = GPM)
    // Offset 5-7: Total gallons treated (BE, 24-bit)
    // Offset 8-10: Total gallons resettable (BE, 24-bit)
    // Offset 11-12: Total regenerations (BE)
    // Offset 13-14: Regens resettable (BE)
    // Offset 15: Regen active flag
    // Offset 19: End marker 'F' (0x46)

    // Current flow (duplicate of uu-0, but useful during stats-only updates)
    uint16_t flow_raw = this->read_uint16_be(3);
    float current_flow = flow_raw / 100.0f;

    // Total gallons treated (24-bit big-endian)
    uint32_t total_gallons = this->read_uint24_be(5);

    // Total regenerations
    uint16_t total_regens = this->read_uint16_be(11);

    if (this->current_flow_sensor_ != nullptr) {
      this->current_flow_sensor_->publish_state(current_flow);
    }

    if (this->total_gallons_sensor_ != nullptr) {
      this->total_gallons_sensor_->publish_state(total_gallons);
    }

    if (this->total_regens_sensor_ != nullptr) {
      this->total_regens_sensor_->publish_state(total_regens);
    }

    ESP_LOGD(TAG, "Statistics: Flow=%.2f GPM, Total gallons=%lu, Total regens=%d",
             current_flow, total_gallons, total_regens);
  }

  this->buffer_.clear();
}

// ============================================================================
// Authentication Methods
// ============================================================================

void CulliganWaterSoftener::send_authentication() {
  std::vector<uint8_t> auth_packet = this->build_auth_packet();

  // Log the auth packet for debugging
  char hex_str[61];  // 20 bytes * 3 chars each + null
  for (int i = 0; i < 20; i++) {
    snprintf(hex_str + i * 3, 4, "%02X ", auth_packet[i]);
  }
  ESP_LOGI(TAG, "Auth packet: %s", hex_str);

  this->write_command(auth_packet.data(), auth_packet.size());

  // After sending auth, wait briefly then request data
  // The device needs time to process authentication
  delay(200);
  this->authenticated_ = true;
  this->last_poll_time_ = millis();  // Reset poll timer so we don't immediately poll again
  this->request_data();
}

std::vector<uint8_t> CulliganWaterSoftener::build_auth_packet() {
  // Build 20-byte authentication packet per PROTOCOL.md
  std::vector<uint8_t> buffer(20, 0x74);  // Fill with 't'

  // Get password bytes: [units, tens, hundreds, thousands]
  uint16_t pw = this->password_;
  uint8_t pwd_bytes[4];
  pwd_bytes[3] = pw / 1000;          // thousands
  pwd_bytes[2] = (pw / 100) % 10;    // hundreds
  pwd_bytes[1] = (pw / 10) % 10;     // tens
  pwd_bytes[0] = pw % 10;            // units

  // Select random polynomial
  uint8_t polynomial = this->get_random_polynomial();

  // Generate random values
  uint8_t seed = (esp_random() % 254) + 1;      // 1-255
  uint8_t random2 = (esp_random() % 254) + 1;   // 1-255

  // Initialize CRC8
  CsCrc8 crc;
  crc.set_options(polynomial, seed);

  // Compute authentication values
  uint8_t xored_random = random2 ^ seed;
  uint8_t crc_result = crc.compute_legacy(xored_random);
  uint8_t counter_xor = this->connection_counter_ ^ crc_result;

  // Fill buffer
  buffer[2] = 0x50;  // 'P'
  buffer[3] = 0x41;  // 'A'
  buffer[4] = polynomial;
  buffer[5] = seed;
  buffer[6] = xored_random;

  // Encode password digits with chained CRC
  buffer[7] = crc.compute_legacy(counter_xor) ^ pwd_bytes[3];
  buffer[8] = pwd_bytes[2] ^ crc.compute_legacy(buffer[7]);
  buffer[9] = pwd_bytes[1] ^ crc.compute_legacy(buffer[8]);
  buffer[10] = pwd_bytes[0] ^ crc.compute_legacy(buffer[9]);

  // Fill remaining bytes with random values
  for (int i = 11; i < 20; i++) {
    buffer[i] = (esp_random() % 254) + 1;
  }

  ESP_LOGD(TAG, "Built auth packet with polynomial=0x%02X, seed=0x%02X", polynomial, seed);

  return buffer;
}

uint8_t CulliganWaterSoftener::get_random_polynomial() {
  return ALLOWED_POLYNOMIALS[esp_random() % NUM_POLYNOMIALS];
}

// ============================================================================
// Write Commands
// ============================================================================

void CulliganWaterSoftener::write_command(const uint8_t *data, size_t length) {
  if (this->rx_handle_ == 0) {
    ESP_LOGW(TAG, "RX handle not available, cannot write command");
    return;
  }

  esp_err_t status = esp_ble_gattc_write_char(
    this->parent_->get_gattc_if(),
    this->parent_->get_conn_id(),
    this->rx_handle_,
    length,
    const_cast<uint8_t *>(data),
    ESP_GATT_WRITE_TYPE_RSP,
    ESP_GATT_AUTH_REQ_NONE
  );

  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Write command failed, status=%d", status);
  } else {
    ESP_LOGD(TAG, "Write command sent, %d bytes", length);
  }
}

void CulliganWaterSoftener::request_data() {
  ESP_LOGI(TAG, "Requesting data from device...");

  // Request status (uu)
  uint8_t status_req[20];
  memset(status_req, 0x75, 20);  // 'u'
  this->write_command(status_req, 20);

  delay(20);

  // Request settings (vv)
  uint8_t settings_req[20];
  memset(settings_req, 0x76, 20);  // 'v'
  this->write_command(settings_req, 20);

  delay(20);

  // Request statistics (ww)
  uint8_t stats_req[20];
  memset(stats_req, 0x77, 20);  // 'w'
  this->write_command(stats_req, 20);

  ESP_LOGI(TAG, "Data requests sent (u, v, w)");
}

void CulliganWaterSoftener::send_regen_now() {
  ESP_LOGI(TAG, "Sending regen now command");
  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'R';  // 0x52
  cmd[14] = 'N';  // 0x4E
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_regen_next() {
  ESP_LOGI(TAG, "Sending regen next command");
  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'R';  // 0x52
  cmd[14] = 'T';  // 0x54
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_sync_time() {
  ESP_LOGI(TAG, "Sending sync time command");

  // Get current time from ESP32
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  uint8_t hour = timeinfo.tm_hour;
  uint8_t minute = timeinfo.tm_min;
  uint8_t second = timeinfo.tm_sec;
  uint8_t am_pm = 0;

  // Convert to 12-hour format
  if (hour == 0) {
    hour = 12;
    am_pm = 0;  // AM
  } else if (hour < 12) {
    am_pm = 0;  // AM
  } else if (hour == 12) {
    am_pm = 1;  // PM
  } else {
    hour -= 12;
    am_pm = 1;  // PM
  }

  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'T';     // 0x54
  cmd[14] = hour;
  cmd[15] = minute;
  cmd[16] = am_pm;
  cmd[17] = second;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_reset_gallons() {
  ESP_LOGI(TAG, "Sending reset gallons command");
  uint8_t cmd[20];
  memset(cmd, 0x77, 20);  // 'w' base
  cmd[13] = 'A';  // 0x41
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_reset_regens() {
  ESP_LOGI(TAG, "Sending reset regens command");
  uint8_t cmd[20];
  memset(cmd, 0x77, 20);  // 'w' base
  cmd[13] = 'B';  // 0x42
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_display(bool on) {
  ESP_LOGI(TAG, "Setting display %s", on ? "ON" : "OFF");
  uint8_t cmd[20];
  memset(cmd, 0x76, 20);  // 'v' base
  cmd[13] = 'G';      // 0x47
  cmd[14] = on ? 0 : 1;  // 0=on, 1=off (inverted)
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_hardness(uint8_t hardness) {
  ESP_LOGI(TAG, "Setting hardness to %d GPG", hardness);
  if (hardness > 99) hardness = 99;
  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'H';      // 0x48
  cmd[14] = hardness;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_regen_time(uint8_t hour, bool is_pm) {
  ESP_LOGI(TAG, "Setting regen time to %d %s", hour, is_pm ? "PM" : "AM");
  if (hour < 1) hour = 1;
  if (hour > 12) hour = 12;
  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 't';      // 0x74
  cmd[14] = hour;
  cmd[15] = is_pm ? 1 : 0;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_reserve_capacity(uint8_t percent) {
  ESP_LOGI(TAG, "Setting reserve capacity to %d%%", percent);
  if (percent > 49) percent = 49;
  uint8_t cmd[20];
  memset(cmd, 0x76, 20);  // 'v' base
  cmd[13] = 'B';      // 0x42
  cmd[14] = percent;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_salt_level(float lbs) {
  // Calculate regens remaining from pounds
  float salt_per_regen = this->brine_refill_time_ * 1.5f;
  uint8_t regens = 0;
  if (salt_per_regen > 0) {
    regens = (uint8_t)(lbs / salt_per_regen);
    if (regens > 100) regens = 100;
  }

  ESP_LOGI(TAG, "Setting salt level to %.1f lbs (%d regens)", lbs, regens);

  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'S';      // 0x53
  cmd[14] = regens;
  cmd[15] = 5;        // Low alert threshold (default)
  cmd[16] = this->brine_tank_type_;
  cmd[17] = this->brine_fill_height_;
  this->write_command(cmd, 20);
}

// ============================================================================
// Helper Methods
// ============================================================================

uint16_t CulliganWaterSoftener::read_uint16_be(size_t offset) {
  if (offset + 1 >= this->buffer_.size()) {
    return 0;
  }
  return ((uint16_t)this->buffer_[offset] << 8) | (uint16_t)this->buffer_[offset + 1];
}

uint32_t CulliganWaterSoftener::read_uint24_be(size_t offset) {
  if (offset + 2 >= this->buffer_.size()) {
    return 0;
  }
  return ((uint32_t)this->buffer_[offset] << 16) |
         ((uint32_t)this->buffer_[offset + 1] << 8) |
         (uint32_t)this->buffer_[offset + 2];
}

uint32_t CulliganWaterSoftener::read_uint32_be(size_t offset) {
  if (offset + 3 >= this->buffer_.size()) {
    return 0;
  }
  return ((uint32_t)this->buffer_[offset] << 24) |
         ((uint32_t)this->buffer_[offset + 1] << 16) |
         ((uint32_t)this->buffer_[offset + 2] << 8) |
         (uint32_t)this->buffer_[offset + 3];
}

float CulliganWaterSoftener::get_battery_percent(uint8_t raw) {
  // Battery lookup table per PROTOCOL.md
  switch (raw) {
    case 0: return 0.0f;
    case 1: return 100.0f;
    case 2: return 75.0f;
    case 3: return 50.0f;
    case 4: return 25.0f;
    default: return 0.0f;
  }
}

float CulliganWaterSoftener::get_tank_multiplier(uint8_t tank_type) {
  // Tank multipliers per PROTOCOL.md
  switch (tank_type) {
    case 16: return 8.1f;
    case 18: return 10.4f;
    case 24: return 18.6f;
    case 30: return 29.55f;
    default: return 8.1f;
  }
}

float CulliganWaterSoftener::calculate_salt_remaining() {
  if (!this->brine_tank_configured_ || this->brine_regens_remaining_ == 0xFF) {
    return 0.0f;
  }

  // Salt per regen (residential) = refillTime × 1.5 lbs
  float salt_per_regen = this->brine_refill_time_ * 1.5f;

  // Salt remaining = saltPerRegen × regensRemaining
  return salt_per_regen * this->brine_regens_remaining_;
}

std::string CulliganWaterSoftener::format_time_12h(uint8_t hour, uint8_t minute, uint8_t am_pm) {
  char time_str[12];
  snprintf(time_str, sizeof(time_str), "%d:%02d %s", hour, minute, am_pm ? "PM" : "AM");
  return std::string(time_str);
}

std::string CulliganWaterSoftener::format_time_24h(uint8_t hour, uint8_t minute) {
  char time_str[6];
  snprintf(time_str, sizeof(time_str), "%02d:%02d", hour, minute);
  return std::string(time_str);
}

void CulliganWaterSoftener::parse_flags(uint8_t flags) {
  // Flag bits per PROTOCOL.md:
  // Bit 1 (0x01): Shutoff setting enabled
  // Bit 2 (0x02): Bypass setting enabled
  // Bit 3 (0x04): Shutoff state active
  // Bit 4 (0x08): Bypass state active
  // Bit 5 (0x10): Display off

  this->current_flags_ = flags;

  bool shutoff_active = (flags & 0x04) != 0;
  bool bypass_active = (flags & 0x08) != 0;
  bool display_off = (flags & 0x10) != 0;

  if (this->shutoff_active_sensor_ != nullptr) {
    this->shutoff_active_sensor_->publish_state(shutoff_active);
  }

  if (this->bypass_active_sensor_ != nullptr) {
    this->bypass_active_sensor_->publish_state(bypass_active);
  }

  if (this->display_off_sensor_ != nullptr) {
    this->display_off_sensor_->publish_state(display_off);
  }

  // Update display switch state if available
  if (this->display_switch_ != nullptr) {
    this->display_switch_->publish_state(!display_off);  // Invert: switch ON = display ON
  }

  ESP_LOGD(TAG, "Flags: shutoff=%d, bypass=%d, display_off=%d", shutoff_active, bypass_active, display_off);
}

// ============================================================================
// Button Implementations
// ============================================================================

void RegenNowButton::press_action() {
  this->parent_->send_regen_now();
}

void RegenNextButton::press_action() {
  this->parent_->send_regen_next();
}

void SyncTimeButton::press_action() {
  this->parent_->send_sync_time();
}

void ResetGallonsButton::press_action() {
  this->parent_->send_reset_gallons();
}

void ResetRegensButton::press_action() {
  this->parent_->send_reset_regens();
}

// ============================================================================
// Switch Implementation
// ============================================================================

void DisplaySwitch::write_state(bool state) {
  this->parent_->send_set_display(state);
  this->publish_state(state);
}

// ============================================================================
// Number Implementations
// ============================================================================

void HardnessNumber::control(float value) {
  this->parent_->send_set_hardness((uint8_t)value);
  this->publish_state(value);
}

void RegenTimeHourNumber::control(float value) {
  // Assume AM for now; could be extended with separate AM/PM control
  this->parent_->send_set_regen_time((uint8_t)value, false);
  this->publish_state(value);
}

void ReserveCapacityNumber::control(float value) {
  this->parent_->send_set_reserve_capacity((uint8_t)value);
  this->publish_state(value);
}

void SaltLevelNumber::control(float value) {
  this->parent_->send_set_salt_level(value);
  this->publish_state(value);
}

}  // namespace culligan_water_softener
}  // namespace esphome

#endif  // USE_ESP32
