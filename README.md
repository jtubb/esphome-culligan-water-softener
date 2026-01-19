# Culligan Water Softener ESPHome Component

**Native ESPHome component for Culligan CS Meter Soft water softeners**

This component enables direct BLE communication between your ESP32 and Culligan water softener, with full sensor support and device control.

## Features

- **Native BLE** - Direct ESP32 to water softener communication
- **Authentication** - CRC8-based auth for firmware < 6.0
- **20+ Sensors** - Real-time flow, usage, battery, settings, and statistics
- **Device Control** - Buttons for regeneration, time sync, counter reset
- **Adjustable Settings** - Number inputs for hardness, regen time, salt level
- **Binary Sensors** - Status flags (display, bypass, shutoff, regenerating)
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
| `days_until_regen` | days | Time to next regen |
| `total_gallons` | gal | Lifetime treated |
| `total_regenerations` | - | Lifetime regen count |
| `battery_level` | % | Device battery |
| `reserve_capacity` | % | Reserve setting |
| `resin_capacity` | grains | Resin capacity |
| `backwash_time` | min | Cycle time |
| `brine_draw_time` | min | Cycle time |
| `rapid_rinse_time` | min | Cycle time |
| `brine_refill_time` | min | Cycle time |

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
| Setting | Range | Description |
|---------|-------|-------------|
| `water_hardness` | 0-99 GPG | Water hardness |
| `regeneration_time_hour` | 1-12 | Regen hour |
| `reserve_capacity` | 0-49% | Reserve capacity |
| `salt_level` | 0-500 lbs | Salt level |

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
