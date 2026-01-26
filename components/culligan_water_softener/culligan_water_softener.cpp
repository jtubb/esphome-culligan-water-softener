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
  if (this->auto_discover_) {
    ESP_LOGI(TAG, "Auto-discovery enabled, scanning for '%s'", this->device_name_.c_str());
  }
}

bool CulliganWaterSoftener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // Skip if auto-discovery is disabled or device already discovered
  if (!this->auto_discover_ || this->device_discovered_) {
    return false;
  }

  // Check if the device has the name we're looking for
  std::string name = device.get_name();
  if (name.empty() || name != this->device_name_) {
    return false;
  }

  // Found the device!
  this->device_discovered_ = true;
  this->discovered_address_ = device.address_uint64();

  // Format MAC address for logging and sensor
  char mac_str[18];
  const uint8_t *mac = device.address();
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  ESP_LOGI(TAG, "Discovered %s at %s (RSSI: %d dB)", name.c_str(), mac_str, device.get_rssi());

  // Publish MAC address to text sensor
  if (this->mac_address_sensor_ != nullptr) {
    this->mac_address_sensor_->publish_state(mac_str);
  }

  // Update the BLE client's address to connect to this device
  // Use explicit BLEClientNode::parent_ to disambiguate from ESPBTDeviceListener::parent_
  auto *ble_client = this->ble_client::BLEClientNode::parent_;
  ble_client->set_address(this->discovered_address_);

  // Trigger connection by re-enabling the client
  // This is needed because the client may have given up connecting to 00:00:00:00:00:00
  ble_client->set_enabled(false);
  ble_client->set_enabled(true);

  return true;  // We handled this device
}

void CulliganWaterSoftener::loop() {
  uint32_t now = millis();

  // Send keepalive to maintain connection (every 4 seconds)
  if (this->authenticated_ && (now - this->last_keepalive_time_ >= this->keepalive_interval_ms_)) {
    this->last_keepalive_time_ = now;
    this->send_keepalive();
  }

  // Non-blocking request state machine (20ms between commands)
  if (this->request_state_ != REQ_IDLE && this->request_state_ != REQ_DONE) {
    if (now - this->request_time_ >= 20) {
      this->request_time_ = now;
      uint8_t cmd[20];

      switch (this->request_state_) {
        case REQ_STATUS:
          memset(cmd, 0x75, 20);  // 'u'
          this->write_command(cmd, 20);
          this->request_state_ = REQ_SETTINGS;
          break;
        case REQ_SETTINGS:
          memset(cmd, 0x76, 20);  // 'v'
          this->write_command(cmd, 20);
          this->request_state_ = REQ_STATS;
          break;
        case REQ_STATS:
          memset(cmd, 0x77, 20);  // 'w'
          this->write_command(cmd, 20);
          this->request_state_ = REQ_DONE;
          ESP_LOGD(TAG, "Data requests complete (u, v, w)");
          break;
        default:
          break;
      }
    }
  }

  // Periodic data request (at poll_interval)
  if (this->authenticated_ && this->request_state_ == REQ_IDLE &&
      (now - this->last_poll_time_ >= this->poll_interval_ms_)) {
    this->last_poll_time_ = now;
    this->request_data();
  }

  // Reset request state after done
  if (this->request_state_ == REQ_DONE && (now - this->request_time_ >= 100)) {
    this->request_state_ = REQ_IDLE;
  }
}

void CulliganWaterSoftener::dump_config() {
  ESP_LOGCONFIG(TAG, "Culligan Water Softener:");
  ESP_LOGCONFIG(TAG, "  Password: %d", this->password_);
  ESP_LOGCONFIG(TAG, "  Poll Interval: %d ms", this->poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Auto-discover: %s", this->auto_discover_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Device Name: %s", this->device_name_.c_str());
  if (this->device_discovered_) {
    ESP_LOGCONFIG(TAG, "  Discovered Address: 0x%012llX", this->discovered_address_);
  }
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

        // Publish MAC address if not already published by auto-discovery
        if (this->mac_address_sensor_ != nullptr && !this->device_discovered_) {
          const uint8_t *mac = this->ble_client::BLEClientNode::parent_->get_remote_bda();
          char mac_str[18];
          snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
          this->mac_address_sensor_->publish_state(mac_str);
        }
      }
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "Disconnected from water softener");
      this->buffer_clear();
      this->handshake_received_ = false;
      this->authenticated_ = false;
      this->status_packet_count_ = 0;
      this->request_state_ = REQ_IDLE;
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // Find TX characteristic (notifications) using UUID object
      auto service_uuid = esp32_ble_tracker::ESPBTUUID::from_raw("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
      auto tx_char_uuid = esp32_ble_tracker::ESPBTUUID::from_raw("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

      auto *chr = this->ble_client::BLEClientNode::parent_->get_characteristic(service_uuid, tx_char_uuid);
      if (chr == nullptr) {
        ESP_LOGE(TAG, "TX characteristic not found");
        break;
      }
      this->tx_handle_ = chr->handle;

      // Subscribe to notifications
      auto status = esp_ble_gattc_register_for_notify(this->ble_client::BLEClientNode::parent_->get_gattc_if(),
                                                       this->ble_client::BLEClientNode::parent_->get_remote_bda(), chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      } else {
        ESP_LOGI(TAG, "Subscribed to TX notifications");
      }

      // Find RX characteristic (write)
      auto rx_char_uuid = esp32_ble_tracker::ESPBTUUID::from_raw("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
      chr = this->ble_client::BLEClientNode::parent_->get_characteristic(service_uuid, rx_char_uuid);
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

// Ring buffer append - optimized for BLE notification sizes
void CulliganWaterSoftener::buffer_append(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    this->buffer_[this->buffer_head_] = data[i];
    this->buffer_head_ = (this->buffer_head_ + 1) & (BUFFER_SIZE - 1);
    // Overflow check - if we catch up to tail, advance tail (drop old data)
    if (this->buffer_head_ == this->buffer_tail_) {
      this->buffer_tail_ = (this->buffer_tail_ + 1) & (BUFFER_SIZE - 1);
    }
  }
}

void CulliganWaterSoftener::handle_notification(const uint8_t *data, uint16_t length) {
  // Skip logging for keepalive packets (hot path optimization)
  // Only log at VERBOSE level if explicitly enabled
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  if (length < 2 || data[0] != 0x78 || data[1] != 0x78) {
    ESP_LOGV(TAG, "RX %d bytes: %02X %02X...", length, data[0], length > 1 ? data[1] : 0);
  }
#endif

  // Fast append to ring buffer
  this->buffer_append(data, length);

  // Try to parse complete packets from buffer
  this->process_buffer();
}

void CulliganWaterSoftener::process_buffer() {
  // Process ONE packet per call to avoid watchdog timeout
  // Note: Handshake packets are 18 bytes, data packets vary (19-20 bytes)
  size_t buf_len = this->buffer_size();
  if (buf_len < 18) {
    return;  // Not enough data yet
  }

  uint8_t type0 = this->buffer_peek(0);
  uint8_t type1 = this->buffer_peek(1);

  if (type0 == 0x74 && type1 == 0x74) {  // "tt" - Handshake
    this->parse_handshake();
  } else if (type0 == 0x75 && type1 == 0x75) {  // "uu" - Status
    this->parse_status_packet();
  } else if (type0 == 0x76 && type1 == 0x76) {  // "vv" - Settings
    this->parse_settings_packet();
  } else if (type0 == 0x77 && type1 == 0x77) {  // "ww" - Statistics
    this->parse_statistics_packet();
  } else if (type0 == 0x78 && type1 == 0x78) {  // "xx" - Keepalive
    // Silently consume keepalive packets - they're just connection maintenance
    // xx-0 is 6 bytes: 78 78 00 00 10 00
    // xx-1 through xx-6 are 4 bytes: 78 78 0X 00
    if (buf_len < 4) {
      return;  // Wait for complete packet
    }
    uint8_t packet_num = this->buffer_peek(2);
    size_t packet_len = (packet_num == 0) ? 6 : 4;
    if (buf_len < packet_len) {
      return;  // Wait for complete packet
    }
    this->buffer_consume(packet_len);
  } else {
    // Check if this is a daily usage continuation packet (no header)
    // These arrive after ww-1 and contain additional daily usage history
    if (this->daily_usage_packet_count_ > 0 && this->daily_usage_packet_count_ < 4) {
      // Expecting continuation packet - extract data from ring buffer
      if (this->daily_usage_packet_count_ == 1 && buf_len >= 20) {
        // Continuation 1: 20 bytes -> index 17-36
        uint8_t temp[20];
        for (size_t i = 0; i < 20; i++) temp[i] = this->buffer_peek(i);
        this->parse_daily_usage_data(temp, 20, 17);
        this->buffer_consume(20);
        this->daily_usage_packet_count_ = 2;
        return;
      } else if (this->daily_usage_packet_count_ == 2 && buf_len >= 20) {
        // Continuation 2: 20 bytes -> index 37-56
        uint8_t temp[20];
        for (size_t i = 0; i < 20; i++) temp[i] = this->buffer_peek(i);
        this->parse_daily_usage_data(temp, 20, 37);
        this->buffer_consume(20);
        this->daily_usage_packet_count_ = 3;
        return;
      } else if (this->daily_usage_packet_count_ == 3 && buf_len >= 6) {
        // Continuation 3: 5 bytes of data + end marker (0x38) -> index 57-61
        uint8_t temp[6];
        for (size_t i = 0; i < 6; i++) temp[i] = this->buffer_peek(i);
        this->parse_daily_usage_data(temp, 5, 57);
        this->buffer_consume(6);
        this->daily_usage_packet_count_ = 4;
        this->daily_usage_complete_ = true;
        // Now calculate average
        this->calculate_avg_daily_usage();
        return;
      }
      // Not enough data yet for continuation
      return;
    }

    // Unknown packet type - scan for next valid header
    // This handles headerless continuation packets (uu-3,4,5) more efficiently
    size_t scan_pos = 1;
    bool found = false;
    while (scan_pos < buf_len - 1) {
      uint8_t b0 = this->buffer_peek(scan_pos);
      uint8_t b1 = this->buffer_peek(scan_pos + 1);
      if ((b0 == 0x74 && b1 == 0x74) ||  // tt
          (b0 == 0x75 && b1 == 0x75) ||  // uu
          (b0 == 0x76 && b1 == 0x76) ||  // vv
          (b0 == 0x77 && b1 == 0x77) ||  // ww
          (b0 == 0x78 && b1 == 0x78)) {  // xx
        // Found a valid header, skip to it
        this->buffer_consume(scan_pos);
        found = true;
        break;
      }
      scan_pos++;
    }

    if (!found) {
      // No valid header found, clear buffer (likely headerless continuation data)
      this->buffer_clear();
    }
  }
}


void CulliganWaterSoftener::parse_handshake() {
  // Handshake packets are 18 bytes (not 20 like data packets)
  if (this->buffer_size() < 18) {
    return;
  }

  // tt packet structure (per PROTOCOL.md):
  // Offset 5: Firmware major version
  // Offset 6: Firmware minor version (BCD)
  // Offset 7: Auth status (0x80 = auth required)
  // Offset 11: Connection counter

  this->firmware_major_ = this->buffer_peek(5);
  this->firmware_minor_ = this->buffer_peek(6);
  uint8_t auth_flag = this->buffer_peek(7);
  this->connection_counter_ = this->buffer_peek(11);

  // Authentication is required for firmware < 6.0, regardless of the flag byte
  // The flag byte (0x80) may not always be set correctly by the device
  this->auth_required_ = (this->firmware_major_ < 6) || ((auth_flag & AUTH_REQUIRED_FLAG) != 0);

  char fw_version[16];
  snprintf(fw_version, sizeof(fw_version), "C%d.%d", this->firmware_major_, this->firmware_minor_);

  ESP_LOGI(TAG, "Handshake received, firmware: %s, auth flag: 0x%02X, counter: %d, already_auth: %s",
           fw_version, auth_flag, this->connection_counter_, this->authenticated_ ? "yes" : "no");

  if (this->firmware_version_sensor_ != nullptr) {
    this->firmware_version_sensor_->publish_state(fw_version);
  }

  this->handshake_received_ = true;

  // Remove the handshake packet from buffer (18 bytes)
  this->buffer_consume(18);

  // Only authenticate if we haven't already for this connection
  if (this->authenticated_) {
    ESP_LOGI(TAG, "Already authenticated, ignoring handshake");
    return;
  }

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
  if (this->buffer_size() < 20) {
    ESP_LOGD(TAG, "Status packet incomplete, waiting for more data");
    return;
  }

  // Packet number (which status packet this is - 0-5)
  uint8_t packet_num = this->buffer_peek(2);

  ESP_LOGD(TAG, "Status packet #%d (end marker: 0x%02X)", packet_num, this->buffer_peek(19));

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

    // Validate end marker - reject packet if corrupt
    uint8_t end_marker = this->buffer_peek(19);
    if (end_marker != END_MARKER_UU_0) {
      ESP_LOGW(TAG, "Invalid uu-0 end marker: 0x%02X (expected 0x%02X), rejecting packet",
               end_marker, END_MARKER_UU_0);
      this->buffer_consume(20);
      this->buffer_clear();  // Clear buffer to resync
      return;
    }

    uint8_t hour = this->buffer_peek(3);
    uint8_t minute = this->buffer_peek(4);
    uint8_t am_pm = this->buffer_peek(5);
    uint8_t battery_raw = this->buffer_peek(6);

    // Flow values use BIG-ENDIAN and ÷100 scaling
    uint16_t flow_raw = this->read_uint16_be(7);
    float current_flow_raw = flow_raw / 100.0f;

    uint16_t soft_water_raw = this->read_uint16_be(9);
    uint16_t usage_today_raw = this->read_uint16_be(11);

    uint16_t peak_flow_raw = this->read_uint16_be(13);
    float peak_flow_value = peak_flow_raw / 100.0f;

    uint8_t hardness = this->buffer_peek(15);
    uint8_t regen_hour = this->buffer_peek(16);
    uint8_t regen_am_pm = this->buffer_peek(17);
    uint8_t flags = this->buffer_peek(18);

    // Validate sensor values before publishing
    float current_flow = this->validate_current_flow(current_flow_raw);
    uint16_t soft_water = this->validate_soft_water_remaining(soft_water_raw);
    uint16_t usage_today = this->validate_water_usage_today(usage_today_raw);
    float peak_flow = this->validate_peak_flow(peak_flow_value);

    // Mark that we have valid readings (for jump detection)
    this->has_valid_readings_ = true;

    // Get battery percentage using lookup table
    float battery_pct = this->get_battery_percent(battery_raw);

    // Parse flags and update binary sensors
    this->parse_flags(flags);

    // Publish sensor values (using validated values)
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

    // Update number entities with current device values
    if (this->hardness_number_ != nullptr) {
      this->hardness_number_->publish_state(hardness);
    }

    if (this->regen_time_hour_number_ != nullptr) {
      this->regen_time_hour_number_->publish_state(regen_hour);
    }

    ESP_LOGI(TAG, "Parsed uu-0: Time=%d:%02d %s, Flow=%.2f GPM, Soft Water=%d gal, Usage=%d gal",
             hour, minute, am_pm ? "PM" : "AM", current_flow, soft_water, usage_today);

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

    // Validate end marker
    uint8_t end_marker = this->buffer_peek(19);
    if (end_marker != END_MARKER_UU_1) {
      ESP_LOGW(TAG, "Invalid uu-1 end marker: 0x%02X (expected 0x%02X), rejecting packet",
               end_marker, END_MARKER_UU_1);
      this->buffer_consume(20);
      this->buffer_clear();
      return;
    }

    uint8_t filter_backwash_days = this->buffer_peek(3);
    uint8_t air_recharge_days = this->buffer_peek(4);
    uint8_t regen_active = this->buffer_peek(8);
    uint8_t regens_remaining = this->buffer_peek(13);
    uint8_t low_salt_alert = this->buffer_peek(14);
    uint8_t tank_type = this->buffer_peek(15);
    uint8_t fill_height = this->buffer_peek(16);
    uint8_t refill_time = this->buffer_peek(17);

    // Publish filter/air recharge days
    if (this->filter_backwash_days_sensor_ != nullptr) {
      this->filter_backwash_days_sensor_->publish_state(filter_backwash_days);
    }
    if (this->air_recharge_days_sensor_ != nullptr) {
      this->air_recharge_days_sensor_->publish_state(air_recharge_days);
    }

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

    // Publish brine tank type and fill height sensors
    if (this->brine_tank_type_sensor_ != nullptr) {
      this->brine_tank_type_sensor_->publish_state(tank_type);
    }
    if (this->brine_fill_height_sensor_ != nullptr) {
      this->brine_fill_height_sensor_->publish_state(fill_height);
    }
    // Update brine tank number entities with current values
    if (this->brine_tank_type_number_ != nullptr) {
      this->brine_tank_type_number_->publish_state(tank_type);
    }
    if (this->brine_fill_height_number_ != nullptr) {
      this->brine_fill_height_number_->publish_state(fill_height);
    }

    // Publish low salt alert threshold
    if (this->low_salt_alert_sensor_ != nullptr) {
      this->low_salt_alert_sensor_->publish_state(low_salt_alert);
    }
    if (this->low_salt_alert_number_ != nullptr) {
      this->low_salt_alert_number_->publish_state(low_salt_alert);
    }

    // Calculate and publish brine level if configured
    if (this->brine_tank_configured_) {
      float salt_remaining = this->calculate_salt_remaining();
      float tank_multiplier = this->get_tank_multiplier(tank_type);
      float tank_capacity = fill_height * tank_multiplier;
      int salt_percent = (tank_capacity > 0) ? (int)((salt_remaining / tank_capacity) * 100) : 0;
      if (salt_percent > 100) salt_percent = 100;

      if (this->brine_level_sensor_ != nullptr) {
        this->brine_level_sensor_->publish_state(salt_remaining);
      }
      if (this->brine_tank_capacity_sensor_ != nullptr) {
        this->brine_tank_capacity_sensor_->publish_state(tank_capacity);
      }
      if (this->brine_salt_percent_sensor_ != nullptr) {
        this->brine_salt_percent_sensor_->publish_state(salt_percent);
      }
      // Update salt level number entity with current value
      if (this->salt_level_number_ != nullptr) {
        this->salt_level_number_->publish_state(salt_remaining);
      }
      ESP_LOGD(TAG, "Salt remaining: %.1f lbs (%.0f%%), capacity: %.1f lbs (tank=%d\", height=%d\", refill=%d min, regens=%d)",
               salt_remaining, (float)salt_percent, tank_capacity, tank_type, fill_height, refill_time, regens_remaining);
    } else {
      ESP_LOGD(TAG, "Brine tank not configured");
    }

    ESP_LOGI(TAG, "Parsed uu-1: Regen active=%d, Salt=%.1f lbs, Filter backwash=%d days, Air recharge=%d days",
             regen_active, this->brine_tank_configured_ ? this->calculate_salt_remaining() : 0.0f,
             filter_backwash_days, air_recharge_days);
  } else if (packet_num >= 2) {
    // uu-2 through uu-5: Historical data packets
    // We don't need this data, and uu-3,4,5 arrive WITHOUT headers (continuation packets)
    // Clear the entire buffer after uu-2 to avoid buffer corruption from headerless packets
    ESP_LOGD(TAG, "Status packet #%d (historical), clearing buffer to skip headerless continuations", packet_num);
    this->buffer_consume(20);
    this->buffer_clear();  // Clear remaining buffer to skip uu-3,4,5 headerless data
    this->status_packet_count_++;
    return;
  }

  // Remove the parsed packet from buffer (20 bytes)
  this->buffer_consume(20);
  this->status_packet_count_++;
}

void CulliganWaterSoftener::parse_settings_packet() {
  if (this->buffer_size() < 20) {
    ESP_LOGD(TAG, "Settings packet incomplete");
    return;
  }

  uint8_t packet_num = this->buffer_peek(2);

  ESP_LOGD(TAG, "Settings packet #%d", packet_num);

  if (packet_num == 0) {
    // vv-0: Configuration (per PROTOCOL.md and Python script)
    // Offset 3: Days until regen
    // Offset 4: Regen day override (days 0-29)
    // Offset 5: Reserve capacity %
    // Offset 6-7: Resin grain capacity (BE, ×1000)
    // Offset 8: Rental regen disabled (== 11 means disabled)
    // Offset 9: Rental unit setting (!= 0 means rental)
    // Offset 10: Air recharge frequency (days)
    // Offset 11: Regen active flag
    // Offset 12: Pre-fill enabled (!= 0)
    // Offset 13: Brine soak duration (hours, 1-4)
    // Offset 14: Pre-fill soak mode (& 0x08)
    // Offset 16: Flags
    // Offset 19: End marker 'B' (0x42)

    uint8_t days_until_regen = this->buffer_peek(3);
    uint8_t regen_day_override = this->buffer_peek(4);
    uint8_t reserve_capacity = this->buffer_peek(5);
    uint16_t resin_raw = this->read_uint16_be(6);
    uint32_t resin_capacity = resin_raw * 1000;  // Scale to grains (raw value is in thousands)
    uint8_t rental_regen_byte = this->buffer_peek(8);
    uint8_t rental_unit_byte = this->buffer_peek(9);
    uint8_t air_recharge_frequency = this->buffer_peek(10);
    uint8_t prefill_enabled_byte = this->buffer_peek(12);
    uint8_t soak_duration = this->buffer_peek(13);
    if (soak_duration < 1) soak_duration = 1;  // Min 1 hour
    uint8_t prefill_soak_byte = this->buffer_peek(14);
    uint8_t flags = this->buffer_peek(16);

    // Parse rental settings
    bool rental_regen_disabled = (rental_regen_byte == 11);
    bool rental_unit = (rental_unit_byte != 0);
    bool prefill_enabled = (prefill_enabled_byte != 0);
    bool prefill_soak_mode = (prefill_soak_byte & 0x08) != 0;

    if (this->days_until_regen_sensor_ != nullptr) {
      this->days_until_regen_sensor_->publish_state(days_until_regen);
    }

    if (this->regen_day_override_sensor_ != nullptr) {
      this->regen_day_override_sensor_->publish_state(regen_day_override);
    }
    // Update regen days number entity with current value
    if (this->regen_days_number_ != nullptr) {
      this->regen_days_number_->publish_state(regen_day_override);
    }

    if (this->reserve_capacity_sensor_ != nullptr) {
      this->reserve_capacity_sensor_->publish_state(reserve_capacity);
    }

    // Update reserve capacity number entity with current value
    if (this->reserve_capacity_number_ != nullptr) {
      this->reserve_capacity_number_->publish_state(reserve_capacity);
    }

    if (this->resin_capacity_sensor_ != nullptr) {
      this->resin_capacity_sensor_->publish_state(resin_capacity);
    }
    // Update resin capacity number entity (in thousands, e.g., 32 = 32,000 grains)
    if (this->resin_capacity_number_ != nullptr) {
      this->resin_capacity_number_->publish_state(resin_raw);
    }

    if (this->air_recharge_frequency_sensor_ != nullptr) {
      this->air_recharge_frequency_sensor_->publish_state(air_recharge_frequency);
    }

    // Publish prefill duration only if prefill is enabled
    if (this->prefill_duration_sensor_ != nullptr && prefill_enabled) {
      // Note: prefill_duration is at different offset in Python (byte 9 for duration when enabled)
      // But per protocol, byte 9 is rental_unit. The duration comes from elsewhere.
      // For now, soak_duration serves this purpose
      this->prefill_duration_sensor_->publish_state(soak_duration);
    }

    if (this->soak_duration_sensor_ != nullptr) {
      this->soak_duration_sensor_->publish_state(soak_duration);
    }
    // Update prefill duration number entity (0 = disabled, 1-4 = hours)
    if (this->prefill_duration_number_ != nullptr) {
      this->prefill_duration_number_->publish_state(prefill_enabled ? soak_duration : 0);
    }

    // Publish binary sensors for rental/prefill settings
    if (this->rental_regen_disabled_sensor_ != nullptr) {
      this->rental_regen_disabled_sensor_->publish_state(rental_regen_disabled);
    }

    if (this->rental_unit_sensor_ != nullptr) {
      this->rental_unit_sensor_->publish_state(rental_unit);
    }

    if (this->prefill_enabled_sensor_ != nullptr) {
      this->prefill_enabled_sensor_->publish_state(prefill_enabled);
    }

    if (this->prefill_soak_mode_sensor_ != nullptr) {
      this->prefill_soak_mode_sensor_->publish_state(prefill_soak_mode);
    }

    // Parse flags (same as uu-0 byte 18)
    this->parse_flags(flags);

    ESP_LOGI(TAG, "Parsed vv-0: Days until regen=%d, Regen override=%d, Reserve=%d%%, Resin=%lu grains",
             days_until_regen, regen_day_override, reserve_capacity, resin_capacity);

  } else if (packet_num == 1) {
    // vv-1: Cycle times (per PROTOCOL.md)
    // Offset 3: Backwash time (position 1)
    // Offset 4: Brine draw time (position 2)
    // Offset 5: Rapid rinse time (position 3)
    // Offset 6: Brine refill time (position 4)
    // Offset 7-10: Positions 5-8
    // Offset 19: End marker 'C' (0x43)
    //
    // Note: High bit (0x80) = fixed/non-adjustable
    // Actual time = value & 0x7F

    uint8_t backwash_raw = this->buffer_peek(3);
    uint8_t brine_draw_raw = this->buffer_peek(4);
    uint8_t rapid_rinse_raw = this->buffer_peek(5);
    uint8_t brine_refill_raw = this->buffer_peek(6);
    uint8_t pos5_raw = this->buffer_peek(7);
    uint8_t pos6_raw = this->buffer_peek(8);
    uint8_t pos7_raw = this->buffer_peek(9);
    uint8_t pos8_raw = this->buffer_peek(10);

    // Extract actual times (mask off fixed bit)
    uint8_t backwash_time = backwash_raw & 0x7F;
    uint8_t brine_draw_time = brine_draw_raw & 0x7F;
    uint8_t rapid_rinse_time = rapid_rinse_raw & 0x7F;
    uint8_t brine_refill_time = brine_refill_raw & 0x7F;
    uint8_t pos5_time = pos5_raw & 0x7F;
    uint8_t pos6_time = pos6_raw & 0x7F;
    uint8_t pos7_time = pos7_raw & 0x7F;
    uint8_t pos8_time = pos8_raw & 0x7F;

    if (this->backwash_time_sensor_ != nullptr) {
      this->backwash_time_sensor_->publish_state(backwash_time);
    }
    if (this->backwash_time_number_ != nullptr) {
      this->backwash_time_number_->publish_state(backwash_time);
    }

    if (this->brine_draw_time_sensor_ != nullptr) {
      this->brine_draw_time_sensor_->publish_state(brine_draw_time);
    }
    if (this->brine_draw_time_number_ != nullptr) {
      this->brine_draw_time_number_->publish_state(brine_draw_time);
    }

    if (this->rapid_rinse_time_sensor_ != nullptr) {
      this->rapid_rinse_time_sensor_->publish_state(rapid_rinse_time);
    }
    if (this->rapid_rinse_time_number_ != nullptr) {
      this->rapid_rinse_time_number_->publish_state(rapid_rinse_time);
    }

    if (this->brine_refill_time_sensor_ != nullptr) {
      this->brine_refill_time_sensor_->publish_state(brine_refill_time);
    }
    if (this->brine_refill_time_number_ != nullptr) {
      this->brine_refill_time_number_->publish_state(brine_refill_time);
    }

    // Publish cycle positions 5-8
    if (this->cycle_position_5_sensor_ != nullptr) {
      this->cycle_position_5_sensor_->publish_state(pos5_time);
    }

    if (this->cycle_position_6_sensor_ != nullptr) {
      this->cycle_position_6_sensor_->publish_state(pos6_time);
    }

    if (this->cycle_position_7_sensor_ != nullptr) {
      this->cycle_position_7_sensor_->publish_state(pos7_time);
    }

    if (this->cycle_position_8_sensor_ != nullptr) {
      this->cycle_position_8_sensor_->publish_state(pos8_time);
    }

    ESP_LOGD(TAG, "Settings 1: Backwash=%d min%s, Brine draw=%d min%s, Rapid rinse=%d min%s, Brine refill=%d min%s",
             backwash_time, (backwash_raw & 0x80) ? " (fixed)" : "",
             brine_draw_time, (brine_draw_raw & 0x80) ? " (fixed)" : "",
             rapid_rinse_time, (rapid_rinse_raw & 0x80) ? " (fixed)" : "",
             brine_refill_time, (brine_refill_raw & 0x80) ? " (fixed)" : "");
    ESP_LOGD(TAG, "Settings 1: Pos5=%d min%s, Pos6=%d min%s, Pos7=%d min%s, Pos8=%d min%s",
             pos5_time, (pos5_raw & 0x80) ? " (fixed)" : "",
             pos6_time, (pos6_raw & 0x80) ? " (fixed)" : "",
             pos7_time, (pos7_raw & 0x80) ? " (fixed)" : "",
             pos8_time, (pos8_raw & 0x80) ? " (fixed)" : "");
  }

  // Remove the parsed packet from buffer (20 bytes)
  this->buffer_consume(20);
}

void CulliganWaterSoftener::parse_statistics_packet() {
  // ww-0 is 19 bytes, ww-1 is 20 bytes
  if (this->buffer_size() < 19) {
    ESP_LOGD(TAG, "Statistics packet incomplete");
    return;
  }

  uint8_t packet_num = this->buffer_peek(2);

  ESP_LOGD(TAG, "Statistics packet #%d", packet_num);

  if (packet_num == 0) {
    // ww-0: Totals & counters (BIG-ENDIAN per PROTOCOL.md)
    // Offset 3-4: Current flow (BE, ÷100 = GPM)
    // Offset 5-7: Total gallons treated (BE, 24-bit)
    // Offset 8-10: Total gallons resettable (BE, 24-bit)
    // Offset 11-12: Total regenerations (BE)
    // Offset 13-14: Regens resettable (BE)
    // Offset 15: Regen active flag
    // Offset 18: End marker 'F' (0x46)

    // Validate end marker
    uint8_t end_marker = this->buffer_peek(18);
    if (end_marker != END_MARKER_WW_0) {
      ESP_LOGW(TAG, "Invalid ww-0 end marker: 0x%02X (expected 0x%02X), rejecting packet",
               end_marker, END_MARKER_WW_0);
      this->buffer_consume(19);
      this->buffer_clear();
      return;
    }

    // Current flow (validate)
    uint16_t flow_raw = this->read_uint16_be(3);
    float current_flow_raw = flow_raw / 100.0f;
    float current_flow = this->validate_current_flow(current_flow_raw);

    // Total gallons treated (24-bit big-endian, validate)
    uint32_t total_gallons_raw = this->read_uint24_be(5);
    uint32_t total_gallons = this->validate_total_gallons(total_gallons_raw);

    // Total gallons resettable (24-bit big-endian)
    uint32_t total_gallons_resettable = this->read_uint24_be(8);

    // Total regenerations
    uint16_t total_regens = this->read_uint16_be(11);

    // Total regenerations resettable
    uint16_t total_regens_resettable = this->read_uint16_be(13);

    if (this->current_flow_sensor_ != nullptr) {
      this->current_flow_sensor_->publish_state(current_flow);
    }

    if (this->total_gallons_sensor_ != nullptr) {
      this->total_gallons_sensor_->publish_state(total_gallons);
    }

    if (this->total_gallons_resettable_sensor_ != nullptr) {
      this->total_gallons_resettable_sensor_->publish_state(total_gallons_resettable);
    }

    if (this->total_regens_sensor_ != nullptr) {
      this->total_regens_sensor_->publish_state(total_regens);
    }

    if (this->total_regens_resettable_sensor_ != nullptr) {
      this->total_regens_resettable_sensor_->publish_state(total_regens_resettable);
    }

    ESP_LOGI(TAG, "Parsed ww-0: Flow=%.2f GPM, Total gallons=%lu (resettable=%lu), Total regens=%d (resettable=%d)",
             current_flow, total_gallons, total_gallons_resettable, total_regens, total_regens_resettable);

    // Remove ww-0 (19 bytes)
    this->buffer_consume(19);
  } else if (packet_num == 1) {
    // ww-1: Start of daily usage history data (20 bytes)
    // Bytes 3-19 contain first 17 daily usage values
    // Each byte × 10 = gallons for that day
    if (this->buffer_size() < 20) {
      ESP_LOGD(TAG, "ww-1 packet incomplete");
      return;
    }

    // Reset daily usage tracking
    this->daily_usage_complete_ = false;
    memset(this->daily_usage_data_, 0, sizeof(this->daily_usage_data_));

    // Extract bytes 3-19 (17 values) from ring buffer
    uint8_t temp[17];
    for (size_t i = 0; i < 17; i++) temp[i] = this->buffer_peek(3 + i);
    this->parse_daily_usage_data(temp, 17, 0);
    this->daily_usage_packet_count_ = 1;

    ESP_LOGD(TAG, "Parsed ww-1: Daily usage bytes 3-19 -> index 0-16, awaiting continuations");

    // Remove ww-1 (20 bytes)
    this->buffer_consume(20);
  } else {
    // ww-2, ww-3 are other data types (regen history, peak flow) - skip for now
    ESP_LOGD(TAG, "Skipping ww-%d packet", packet_num);
    // Assume 20 bytes for ww-2/3
    size_t packet_len = (this->buffer_size() >= 20) ? 20 : 19;
    this->buffer_consume(packet_len);
  }
}

void CulliganWaterSoftener::parse_daily_usage_data(const uint8_t *data, size_t len, size_t start_index) {
  // Each byte × 10 = gallons for that day
  for (size_t i = 0; i < len && (start_index + i) < 62; i++) {
    this->daily_usage_data_[start_index + i] = data[i] * 10.0f;
  }
}

void CulliganWaterSoftener::calculate_avg_daily_usage() {
  // Calculate average from last 31 non-zero values (second half of 62-day history)
  // Per APK: averages indices 31-61 (the more recent half)
  float sum = 0.0f;
  int count = 0;

  for (int i = 31; i < 62; i++) {
    // Validate individual daily values (each byte × 10, max = 255 × 10 = 2550)
    float daily_value = this->daily_usage_data_[i];
    if (daily_value > 0 && daily_value <= 2550) {
      sum += daily_value;
      count++;
    } else if (daily_value > 2550) {
      ESP_LOGW(TAG, "Ignoring errant daily usage at index %d: %.0f", i, daily_value);
    }
  }

  float avg_raw = 0.0f;
  if (count > 0) {
    avg_raw = sum / count;
  }

  // Validate the calculated average
  float avg = this->validate_avg_daily_usage(avg_raw);

  if (this->avg_daily_usage_sensor_ != nullptr) {
    this->avg_daily_usage_sensor_->publish_state(avg);
  }

  ESP_LOGI(TAG, "Calculated avg daily usage: %.0f gal (from %d valid days)", avg, count);
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
  uint32_t now = millis();
  this->last_poll_time_ = now;       // Reset poll timer so we don't immediately poll again
  this->last_keepalive_time_ = now;  // Reset keepalive timer
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
    this->ble_client::BLEClientNode::parent_->get_gattc_if(),
    this->ble_client::BLEClientNode::parent_->get_conn_id(),
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

void CulliganWaterSoftener::send_keepalive() {
  // Send keepalive packet to maintain BLE connection
  // The device disconnects after ~5 seconds of inactivity
  uint8_t keepalive[20];
  memset(keepalive, 0x78, 20);  // 'x' - keepalive packet
  this->write_command(keepalive, 20);
}

void CulliganWaterSoftener::request_data() {
  ESP_LOGD(TAG, "Starting data request sequence...");

  // Reset daily usage tracking for fresh data
  this->daily_usage_packet_count_ = 0;
  this->daily_usage_complete_ = false;

  // Start non-blocking request state machine
  // The actual requests are sent in loop() with 20ms spacing
  this->request_state_ = REQ_STATUS;
  this->request_time_ = millis() - 20;  // Trigger immediate first request
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
  // Get current time from ESP32
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Check if time is valid (year > 2020 means we have a real time source)
  if (timeinfo.tm_year < 120) {  // tm_year is years since 1900, so 120 = 2020
    ESP_LOGW(TAG, "Cannot sync time: ESP32 time not set. Add 'time:' component with 'platform: homeassistant' to your config.");
    return;
  }

  uint8_t hour_24 = timeinfo.tm_hour;
  uint8_t minute = timeinfo.tm_min;
  uint8_t second = timeinfo.tm_sec;
  uint8_t hour_12 = hour_24;
  uint8_t am_pm = 0;

  // Convert to 12-hour format
  if (hour_24 == 0) {
    hour_12 = 12;
    am_pm = 0;  // AM
  } else if (hour_24 < 12) {
    hour_12 = hour_24;
    am_pm = 0;  // AM
  } else if (hour_24 == 12) {
    hour_12 = 12;
    am_pm = 1;  // PM
  } else {
    hour_12 = hour_24 - 12;
    am_pm = 1;  // PM
  }

  ESP_LOGI(TAG, "Sending sync time: %02d:%02d:%02d (24h) -> %d:%02d:%02d %s",
           hour_24, minute, second, hour_12, minute, second, am_pm ? "PM" : "AM");

  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'T';     // 0x54
  cmd[14] = hour_12;
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
  // Use rounding (add 0.5) instead of truncation for better accuracy
  float salt_per_regen = this->brine_refill_time_ * 1.5f;
  uint8_t regens = 0;
  if (salt_per_regen > 0) {
    regens = (uint8_t)((lbs / salt_per_regen) + 0.5f);  // Round to nearest
    if (regens > 100) regens = 100;
  }

  ESP_LOGI(TAG, "Setting salt level to %.1f lbs (%d regens, %.1f lbs/regen)", lbs, regens, salt_per_regen);

  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' base
  cmd[13] = 'S';      // 0x53
  cmd[14] = regens;
  cmd[15] = 5;        // Low alert threshold (default)
  cmd[16] = this->brine_tank_type_;
  cmd[17] = this->brine_fill_height_;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_regen_days(uint8_t days) {
  ESP_LOGI(TAG, "Setting regen days to %d", days);
  uint8_t cmd[20];
  memset(cmd, 0x76, 20);  // 'v' = AdvancedSettings
  cmd[13] = 'A';  // 0x41
  cmd[14] = (days > 29) ? 29 : days;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_resin_capacity(uint16_t grains_thousands) {
  ESP_LOGI(TAG, "Setting resin capacity to %d thousand grains", grains_thousands);
  uint16_t value = (grains_thousands > 399) ? 399 : grains_thousands;
  uint8_t cmd[20];
  memset(cmd, 0x76, 20);  // 'v' = AdvancedSettings
  cmd[13] = 'C';  // 0x43
  cmd[14] = value / 256;
  cmd[15] = value % 256;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_prefill(bool enable, uint8_t duration_hours) {
  ESP_LOGI(TAG, "Setting prefill: %s, %d hours", enable ? "enabled" : "disabled", duration_hours);
  uint8_t cmd[20];
  memset(cmd, 0x76, 20);  // 'v' = AdvancedSettings
  cmd[13] = 'R';  // 0x52
  cmd[14] = 'P';  // 0x50
  cmd[15] = enable ? 1 : 0;
  cmd[16] = (duration_hours < 1) ? 1 : ((duration_hours > 4) ? 4 : duration_hours);
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_cycle_time(uint8_t position, uint8_t minutes) {
  ESP_LOGI(TAG, "Setting cycle position %c to %d minutes", (char)position, minutes);
  uint8_t cmd[20];
  memset(cmd, 0x76, 20);  // 'v' = AdvancedSettings
  cmd[13] = 'P';  // 0x50
  cmd[14] = position;  // Position value (49-56 for '1'-'8')
  cmd[15] = (minutes > 99) ? 99 : minutes;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_low_salt_alert(uint8_t threshold) {
  ESP_LOGI(TAG, "Setting low salt alert threshold to %d", threshold);
  // This uses the brine tank command with current values except for alert threshold
  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' = Dashboard
  cmd[13] = 'S';  // 0x53
  cmd[14] = this->brine_regens_remaining_;
  cmd[15] = (threshold > 100) ? 100 : threshold;
  cmd[16] = this->brine_tank_type_;
  cmd[17] = this->brine_fill_height_;
  this->write_command(cmd, 20);
}

void CulliganWaterSoftener::send_set_brine_tank_config(uint8_t tank_type, uint8_t fill_height) {
  ESP_LOGI(TAG, "Setting brine tank config: type=%d\", height=%d\"", tank_type, fill_height);
  // Validate tank type (16, 18, 24, or 30 inch diameter)
  if (tank_type != 16 && tank_type != 18 && tank_type != 24 && tank_type != 30) {
    ESP_LOGW(TAG, "Invalid tank type %d, must be 16, 18, 24, or 30", tank_type);
    return;
  }
  // Update local state
  this->brine_tank_type_ = tank_type;
  this->brine_fill_height_ = fill_height;
  // Send command to device
  uint8_t cmd[20];
  memset(cmd, 0x75, 20);  // 'u' = Dashboard
  cmd[13] = 'S';  // 0x53
  cmd[14] = this->brine_regens_remaining_;
  cmd[15] = 5;  // Low alert threshold (keep existing or default)
  cmd[16] = tank_type;
  cmd[17] = fill_height;
  this->write_command(cmd, 20);
}

// ============================================================================
// Helper Methods
// ============================================================================

// Note: read_uint16_be, read_uint24_be, read_uint32_be, read_uint16_le, read_uint32_le
// are now inline in the header file for better performance

float CulliganWaterSoftener::get_battery_percent(uint8_t raw) {
  // Battery calculation from APK's getBatteryCapacity formula
  // Raw value is ADC reading, convert to voltage: raw * 4 * 0.002 * 11
  float voltage = raw * 4.0f * 0.002f * 11.0f;

  float battery_pct;
  if (voltage >= 9.5f) {
    battery_pct = 100.0f;
  } else if (voltage >= 8.91f) {
    battery_pct = 100.0f - ((9.5f - voltage) * 8.78f);
  } else if (voltage >= 8.48f) {
    battery_pct = 94.78f - ((8.91f - voltage) * 30.26f);
  } else if (voltage >= 7.43f) {
    battery_pct = 81.84f - ((8.48f - voltage) * 60.47f);
  } else if (voltage >= 6.5f) {
    battery_pct = 18.68f - ((7.43f - voltage) * 20.02f);
  } else {
    battery_pct = 0.0f;
  }

  // Clamp to 0-100 range
  if (battery_pct < 0.0f) battery_pct = 0.0f;
  if (battery_pct > 100.0f) battery_pct = 100.0f;

  return battery_pct;
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

  // Calculate max tank capacity based on dimensions
  // Volume = π × r² × height, then multiply by salt density (~75 lbs/ft³)
  // Or use the pre-calculated multiplier: capacity = fill_height × tank_multiplier
  float tank_multiplier = this->get_tank_multiplier(this->brine_tank_type_);
  float max_capacity = this->brine_fill_height_ * tank_multiplier;

  // Salt per regen (residential) = refillTime × 1.5 lbs
  float salt_per_regen = this->brine_refill_time_ * 1.5f;

  // Salt remaining = saltPerRegen × regensRemaining
  float salt_remaining = salt_per_regen * this->brine_regens_remaining_;

  // Sanity check: salt level can't exceed tank capacity
  // If it does, we likely have corrupt data - return last valid value
  if (salt_remaining > max_capacity * 1.1f) {  // Allow 10% tolerance
    ESP_LOGW(TAG, "Ignoring corrupt salt level: %.1f lbs (max capacity: %.1f lbs, regens=%d)",
             salt_remaining, max_capacity, this->brine_regens_remaining_);
    return this->last_valid_salt_level_;
  }

  // Store last valid value for use when corrupt data is detected
  this->last_valid_salt_level_ = salt_remaining;

  return salt_remaining;
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

// ============================================================================
// Sensor Value Validation
// ============================================================================

// Maximum reasonable values for validation
static constexpr uint16_t MAX_WATER_USAGE_TODAY = 5000;      // 5000 gallons/day is extreme
static constexpr uint16_t MAX_SOFT_WATER_REMAINING = 15000; // 15000 gallon capacity is very large
static constexpr float MAX_CURRENT_FLOW = 30.0f;            // 30 GPM is high for residential
static constexpr float MAX_PEAK_FLOW = 30.0f;               // 30 GPM max
static constexpr uint32_t MAX_TOTAL_GALLONS = 50000000;     // 50 million lifetime gallons
static constexpr float MAX_AVG_DAILY_USAGE = 2000.0f;       // 2000 gallons/day average

// Maximum change thresholds (detect sudden jumps from corrupt data)
static constexpr uint16_t MAX_USAGE_JUMP = 500;             // 500 gallon jump is suspicious
static constexpr uint16_t MAX_SOFT_WATER_JUMP = 2000;       // Capacity shouldn't jump much
static constexpr float MAX_FLOW_JUMP = 15.0f;               // 15 GPM instant change is suspicious

uint16_t CulliganWaterSoftener::validate_water_usage_today(uint16_t raw_value) {
  // Check absolute maximum
  if (raw_value > MAX_WATER_USAGE_TODAY) {
    ESP_LOGW(TAG, "Rejecting errant water_usage_today: %d (max: %d), using last valid: %d",
             raw_value, MAX_WATER_USAGE_TODAY, this->last_valid_water_usage_today_);
    return this->last_valid_water_usage_today_;
  }

  // Check for suspicious jumps (only if we have previous valid data)
  if (this->has_valid_readings_ && raw_value > this->last_valid_water_usage_today_) {
    uint16_t jump = raw_value - this->last_valid_water_usage_today_;
    if (jump > MAX_USAGE_JUMP) {
      ESP_LOGW(TAG, "Rejecting suspicious water_usage_today jump: %d -> %d (delta: %d)",
               this->last_valid_water_usage_today_, raw_value, jump);
      return this->last_valid_water_usage_today_;
    }
  }

  this->last_valid_water_usage_today_ = raw_value;
  return raw_value;
}

uint16_t CulliganWaterSoftener::validate_soft_water_remaining(uint16_t raw_value) {
  if (raw_value > MAX_SOFT_WATER_REMAINING) {
    ESP_LOGW(TAG, "Rejecting errant soft_water_remaining: %d (max: %d), using last valid: %d",
             raw_value, MAX_SOFT_WATER_REMAINING, this->last_valid_soft_water_remaining_);
    return this->last_valid_soft_water_remaining_;
  }

  if (this->has_valid_readings_ && raw_value > this->last_valid_soft_water_remaining_) {
    uint16_t jump = raw_value - this->last_valid_soft_water_remaining_;
    if (jump > MAX_SOFT_WATER_JUMP) {
      ESP_LOGW(TAG, "Rejecting suspicious soft_water_remaining jump: %d -> %d (delta: %d)",
               this->last_valid_soft_water_remaining_, raw_value, jump);
      return this->last_valid_soft_water_remaining_;
    }
  }

  this->last_valid_soft_water_remaining_ = raw_value;
  return raw_value;
}

float CulliganWaterSoftener::validate_current_flow(float raw_value) {
  if (raw_value > MAX_CURRENT_FLOW || raw_value < 0.0f) {
    ESP_LOGW(TAG, "Rejecting errant current_flow: %.2f, using last valid: %.2f",
             raw_value, this->last_valid_current_flow_);
    return this->last_valid_current_flow_;
  }

  if (this->has_valid_readings_) {
    float jump = raw_value - this->last_valid_current_flow_;
    if (jump > MAX_FLOW_JUMP || jump < -MAX_FLOW_JUMP) {
      ESP_LOGW(TAG, "Rejecting suspicious current_flow jump: %.2f -> %.2f (delta: %.2f)",
               this->last_valid_current_flow_, raw_value, jump);
      return this->last_valid_current_flow_;
    }
  }

  this->last_valid_current_flow_ = raw_value;
  return raw_value;
}

float CulliganWaterSoftener::validate_peak_flow(float raw_value) {
  if (raw_value > MAX_PEAK_FLOW || raw_value < 0.0f) {
    ESP_LOGW(TAG, "Rejecting errant peak_flow: %.2f, using last valid: %.2f",
             raw_value, this->last_valid_peak_flow_);
    return this->last_valid_peak_flow_;
  }

  // Peak flow should only increase or reset to 0 (new day)
  if (this->has_valid_readings_ && raw_value > this->last_valid_peak_flow_) {
    float jump = raw_value - this->last_valid_peak_flow_;
    if (jump > MAX_FLOW_JUMP && this->last_valid_peak_flow_ > 0.0f) {
      ESP_LOGW(TAG, "Rejecting suspicious peak_flow jump: %.2f -> %.2f (delta: %.2f)",
               this->last_valid_peak_flow_, raw_value, jump);
      return this->last_valid_peak_flow_;
    }
  }

  this->last_valid_peak_flow_ = raw_value;
  return raw_value;
}

uint32_t CulliganWaterSoftener::validate_total_gallons(uint32_t raw_value) {
  if (raw_value > MAX_TOTAL_GALLONS) {
    ESP_LOGW(TAG, "Rejecting errant total_gallons: %lu (max: %lu), using last valid: %lu",
             (unsigned long)raw_value, (unsigned long)MAX_TOTAL_GALLONS,
             (unsigned long)this->last_valid_total_gallons_);
    return this->last_valid_total_gallons_;
  }

  // Total gallons should only increase (monotonic counter)
  if (this->has_valid_readings_ && raw_value < this->last_valid_total_gallons_) {
    uint32_t decrease = this->last_valid_total_gallons_ - raw_value;
    if (decrease > 1000) {  // Allow small decreases for legitimate resets
      ESP_LOGW(TAG, "Rejecting suspicious total_gallons decrease: %lu -> %lu",
               (unsigned long)this->last_valid_total_gallons_, (unsigned long)raw_value);
      return this->last_valid_total_gallons_;
    }
  }

  this->last_valid_total_gallons_ = raw_value;
  return raw_value;
}

float CulliganWaterSoftener::validate_avg_daily_usage(float raw_value) {
  if (raw_value > MAX_AVG_DAILY_USAGE || raw_value < 0.0f) {
    ESP_LOGW(TAG, "Rejecting errant avg_daily_usage: %.0f, using last valid: %.0f",
             raw_value, this->last_valid_avg_daily_usage_);
    return this->last_valid_avg_daily_usage_;
  }

  this->last_valid_avg_daily_usage_ = raw_value;
  return raw_value;
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

void RegenDaysNumber::control(float value) {
  this->parent_->send_set_regen_days((uint8_t)value);
  this->publish_state(value);
}

void ResinCapacityNumber::control(float value) {
  // Value is in thousands of grains (e.g., 32 = 32,000 grains)
  this->parent_->send_set_resin_capacity((uint16_t)value);
  this->publish_state(value);
}

void PrefillDurationNumber::control(float value) {
  // 0 = disabled, 1-4 = duration in hours
  bool enable = (value > 0);
  uint8_t hours = enable ? (uint8_t)value : 1;
  this->parent_->send_set_prefill(enable, hours);
  this->publish_state(value);
}

void BackwashTimeNumber::control(float value) {
  this->parent_->send_set_cycle_time(49, (uint8_t)value);  // Position '1'
  this->publish_state(value);
}

void BrineDrawTimeNumber::control(float value) {
  this->parent_->send_set_cycle_time(50, (uint8_t)value);  // Position '2'
  this->publish_state(value);
}

void RapidRinseTimeNumber::control(float value) {
  this->parent_->send_set_cycle_time(51, (uint8_t)value);  // Position '3'
  this->publish_state(value);
}

void BrineRefillTimeNumber::control(float value) {
  this->parent_->send_set_cycle_time(52, (uint8_t)value);  // Position '4'
  this->publish_state(value);
}

void LowSaltAlertNumber::control(float value) {
  this->parent_->send_set_low_salt_alert((uint8_t)value);
  this->publish_state(value);
}

void BrineTankTypeNumber::control(float value) {
  // Tank type must be 16, 18, 24, or 30
  uint8_t tank_type = (uint8_t)value;
  // Round to nearest valid value
  if (tank_type < 17) tank_type = 16;
  else if (tank_type < 21) tank_type = 18;
  else if (tank_type < 27) tank_type = 24;
  else tank_type = 30;
  this->parent_->send_set_brine_tank_config(tank_type, this->parent_->get_brine_fill_height());
  this->publish_state(tank_type);
}

void BrineFillHeightNumber::control(float value) {
  this->parent_->send_set_brine_tank_config(this->parent_->get_brine_tank_type(), (uint8_t)value);
  this->publish_state(value);
}

}  // namespace culligan_water_softener
}  // namespace esphome

#endif  // USE_ESP32
