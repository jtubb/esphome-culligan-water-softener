# Culligan Water Softener ESPHome Component

**Native ESPHome component for Culligan CS Meter Soft water softeners**

This component enables direct BLE communication between your ESP32 and Culligan water softener, with full sensor support and device control.

## Features

- **Native BLE** - Direct ESP32 to water softener communication
- **Authentication** - CRC8-based auth for firmware < 6.0
- **30+ Sensors** - Real-time flow, usage, battery, cycle times, and statistics
- **12 Adjustable Settings** - Control hardness, regen schedule, cycle times, salt alerts
- **Device Control** - Buttons for regeneration, time sync, counter reset
- **Display Switch** - Toggle device display on/off
- **8 Binary Sensors** - Status flags (display, bypass, shutoff, regenerating, rental, prefill)
- **Home Assistant** - Automatic discovery and integration

## Sensors & Controls

### Sensors
| Sensor | Unit | Description |
|--------|------|-------------|
| `current_flow` | GPM | Real-time water flow |
| `soft_water_remaining` | gal | Capacity until regen |
| `water_usage_today` | gal | Daily consumption |
| `peak_flow_today` | GPM | Max flow today |
| `water_hardness` | GPG | Hardness setting |
| `brine_level` | lbs | Salt remaining |
| `brine_tank_capacity` | lbs | Max salt capacity |
| `brine_salt_percent` | % | Salt level percentage |
| `low_salt_alert` | % | Low salt alert threshold |
| `avg_daily_usage` | gal | Average daily usage |
| `days_until_regen` | days | Time to next regen |
| `regen_day_override` | days | Forced regen interval |
| `total_gallons` | gal | Lifetime treated |
| `total_regenerations` | - | Lifetime regen count |
| `total_gallons_resettable` | gal | Resettable gallon counter |
| `total_regenerations_resettable` | - | Resettable regen counter |
| `battery_level` | % | Device battery |
| `reserve_capacity` | % | Reserve setting |
| `resin_capacity` | grains | Resin capacity |
| `filter_backwash_days` | days | Days until filter backwash |
| `air_recharge_days` | days | Days until air recharge |
| `air_recharge_frequency` | days | Air recharge interval |
| `backwash_time` | min | Cycle position 1 time |
| `brine_draw_time` | min | Cycle position 2 time |
| `rapid_rinse_time` | min | Cycle position 3 time |
| `brine_refill_time` | min | Cycle position 4 time |
| `cycle_position_5` | min | Cycle position 5 time |
| `cycle_position_6` | min | Cycle position 6 time |
| `cycle_position_7` | min | Cycle position 7 time |
| `cycle_position_8` | min | Cycle position 8 time |

### Text Sensors
| Sensor | Description |
|--------|-------------|
| `firmware_version` | e.g., "C4.64" |
| `device_time` | Current device clock |
| `regeneration_time` | Scheduled regen time |

### Binary Sensors
| Sensor | Description |
|--------|-------------|
| `display_off` | Display is off |
| `bypass_active` | Bypass mode active |
| `shutoff_active` | Shutoff active |
| `regeneration_active` | Currently regenerating |
| `rental_regen_disabled` | Regeneration disabled (rental unit) |
| `rental_unit` | Device is a rental unit |
| `prefill_enabled` | Pre-fill feature enabled |
| `prefill_soak_mode` | Pre-fill soak mode active |

### Buttons (Commands)
| Button | Action |
|--------|--------|
| `regenerate_now` | Start immediate regen |
| `regenerate_next` | Schedule next regen |
| `sync_time` | Sync device clock |
| `reset_gallons` | Reset gallon counter |
| `reset_regenerations` | Reset regen counter |

### Switch
| Switch | Description |
|--------|-------------|
| `display` | Turn display on/off |

### Number (Adjustable Settings)

All settings are read from the device on connection and updated in real-time. Changes are sent to the device immediately when adjusted.

| Setting | Range | Description |
|---------|-------|-------------|
| `water_hardness` | 0-99 GPG | Input water hardness - determines how much softening is needed per gallon |
| `regeneration_time_hour` | 1-12 | Hour of day for scheduled regeneration (in 12-hour format) |
| `reserve_capacity` | 0-49% | Reserve capacity percentage - triggers early regen if usage exceeds prediction |
| `salt_level` | 0-500 lbs | Current salt level in brine tank - updates regens remaining calculation |
| `regen_days` | 0-29 days | Force regeneration every N days (0 = demand-based only) |
| `resin_capacity` | 0-399,000 grains | Total resin capacity in grains (e.g., 32000 = 32,000 grains) |
| `prefill_duration` | 0-4 hrs | Pre-fill soak duration in hours (0 = disabled) |
| `backwash_time` | 0-99 min | Cycle position 1: Backwash duration |
| `brine_draw_time` | 0-99 min | Cycle position 2: Brine draw duration |
| `rapid_rinse_time` | 0-99 min | Cycle position 3: Rapid rinse duration |
| `brine_refill_time` | 0-99 min | Cycle position 4: Brine tank refill duration |
| `low_salt_alert` | 0-100% | Salt level percentage threshold for low salt warning |

#### Setting Details

**Water Hardness**: Set this to match your water test results. Higher hardness means more frequent regeneration.

**Regeneration Time**: The device regenerates at this hour. Choose a time when water usage is low (typically 2-3 AM).

**Reserve Capacity**: Safety margin for unexpected high usage. At 25%, if you use 75% of capacity, it will regenerate early.

**Salt Level**: Manually update after adding salt. The device calculates remaining regenerations based on this value.

**Regen Days Override**: Forces regeneration every N days regardless of water usage. Useful for vacation homes or low-usage periods. Set to 0 for pure demand-based regeneration.

**Resin Capacity**: Total softening capacity of your resin bed. Only change if you've replaced the resin or know your exact capacity.

**Prefill Duration**: Pre-fills the brine tank before regeneration. Can improve efficiency but uses more water. Set to 0 to disable.

**Cycle Times (Backwash, Brine Draw, Rapid Rinse, Brine Refill)**: Duration of each regeneration phase. These are typically set at installation and rarely need adjustment. Note: Some cycle times may be fixed by the device and not adjustable.

**Low Salt Alert**: Threshold percentage for triggering low salt warnings. At 25%, you'll be alerted when salt drops to 25% of tank capacity.

## Installation

### From GitHub (Recommended)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/jtubb/esphome-culligan-water-softener
      ref: main
    components: [ culligan_water_softener ]
```

### Local Development

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ culligan_water_softener ]
```

## Configuration

### Minimal Example

```yaml
esphome:
  name: water-softener
  platform: ESP32
  board: esp32dev

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
logger:

ble_client:
  - mac_address: "E8:3B:AA:91:CD:68"  # Your device MAC
    id: culligan_ble_client

external_components:
  - source:
      type: git
      url: https://github.com/jtubb/esphome-culligan-water-softener
      ref: main
    components: [ culligan_water_softener ]

culligan_water_softener:
  id: water_softener
  ble_client_id: culligan_ble_client
  password: 1234  # Default, change if needed
  poll_interval: 60s

sensor:
  - platform: culligan_water_softener
    culligan_water_softener_id: water_softener
    current_flow:
      name: "Water Softener Flow"
    soft_water_remaining:
      name: "Soft Water Remaining"
    battery_level:
      name: "Water Softener Battery"

text_sensor:
  - platform: culligan_water_softener
    culligan_water_softener_id: water_softener
    firmware_version:
      name: "Firmware Version"
```

### Full Example

See [culligan-water-softener-example.yaml](culligan-water-softener-example.yaml) for a complete configuration with all sensors, buttons, switches, and numbers.

## Finding Your MAC Address

1. Install **nRF Connect** on your phone
2. Scan for devices - look for **"CS_Meter_Soft"**
3. Note the MAC address (e.g., `E8:3B:AA:91:CD:68`)

## Configuration Options

```yaml
culligan_water_softener:
  id: water_softener
  ble_client_id: culligan_ble_client
  password: 1234          # Device password (default: 1234)
  poll_interval: 60s      # Data refresh interval (default: 60s)
```

## Troubleshooting

### No Data After Connection

The device uses a "two-connection wake" strategy:
1. First connection wakes the device (~5-7 seconds, may disconnect)
2. Second connection receives data

This is normal - the ESP32 will automatically reconnect.

### Authentication Issues

For firmware < 6.0, authentication is required. Ensure your password is correct (default: 1234).

Check logs for:
```
[I][culligan_water_softener]: Handshake received, firmware: C4.64, auth flag: 0x00, counter: 114, will auth: yes
[I][culligan_water_softener]: Auth packet: 74 74 50 41 ...
```

### Connection Drops

- Ensure only one device connects at a time (close mobile app)
- Check ESP32 is within range (< 30 feet)
- Power cycle the water softener if needed

## Protocol

Uses Nordic UART Service (NUS):
- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- TX (notifications): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`
- RX (write): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`

Packet types: `tt` (handshake), `uu` (status), `vv` (settings), `ww` (statistics), `xx` (keepalive)

## License

Apache 2.0

## Credits

- Protocol reverse-engineered from Culligan Legacy View APK
- ESPHome Framework: https://esphome.io
- Home Assistant: https://home-assistant.io
