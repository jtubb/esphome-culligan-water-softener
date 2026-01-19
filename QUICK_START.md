# Quick Start Guide - Culligan Water Softener ESPHome Component

**Get your water softener connected in 10 minutes!**

## Prerequisites Checklist

- [ ] ESP32 device (any model with Bluetooth)
- [ ] ESPHome installed (`pip install esphome` or Home Assistant add-on)
- [ ] Water softener MAC address (use nRF Connect app to find it)
- [ ] WiFi credentials

## Step-by-Step Installation

### Step 1: Find Your Water Softener's MAC Address (2 minutes)

1. Install **nRF Connect** app on your phone
2. Open app and tap **Scan**
3. Look for **CS_Meter_Soft**
4. Write down the MAC address (format: `E8:3B:AA:91:CD:68`)

### Step 2: Prepare ESPHome Configuration (3 minutes)

If you already have an ESPHome Bluetooth proxy, you can add this component to it!

**Option A: Update Existing ESPHome Device**

```bash
# SSH into Home Assistant or access your ESPHome folder
cd /config/esphome

# Create components directory
mkdir -p components

# Copy the component files (adjust path as needed)
cp -r /path/to/esphome-component/components/culligan_water_softener components/
```

**Option B: New ESP32 Device**

```bash
# Create new ESPHome configuration
esphome wizard my-water-softener.yaml

# Follow prompts:
# - Device name: water-softener-monitor
# - Platform: ESP32
# - Board: esp32dev (or your specific board)
# - WiFi SSID: your_wifi_name
# - WiFi Password: your_wifi_password
```

### Step 3: Add Component Configuration (2 minutes)

Edit your ESPHome YAML file:

```yaml
# my-water-softener.yaml (or edit your existing device YAML)

esphome:
  name: water-softener-monitor
  platform: ESP32
  board: esp32dev

wifi:
  ssid: "YourWiFi"
  password: "YourPassword"

logger:
  level: DEBUG  # Helpful for first setup

api:
  encryption:
    key: "your-32-char-key-here"  # Auto-generated

ota:
  password: "your-ota-password"

# If you have existing bluetooth_proxy, keep it!
bluetooth_proxy:
  active: true

# Add this for your water softener
ble_client:
  - mac_address: "E8:3B:AA:91:CD:68"  # YOUR MAC HERE
    id: culligan_ble_client

external_components:
  - source:
      type: local
      path: components
    components: [ culligan_water_softener ]

culligan_water_softener:
  id: water_softener
  ble_client_id: culligan_ble_client

# Add the sensors you want
sensor:
  - platform: culligan_water_softener
    culligan_water_softener_id: water_softener
    current_flow:
      name: "Water Flow"
    soft_water_remaining:
      name: "Soft Water Remaining"
    battery_level:
      name: "Water Softener Battery"
    water_usage_today:
      name: "Water Usage Today"

text_sensor:
  - platform: culligan_water_softener
    culligan_water_softener_id: water_softener
    firmware_version:
      name: "Water Softener Firmware"
```

### Step 4: Compile and Upload (3 minutes)

**First Time (via USB):**

```bash
# Connect ESP32 via USB cable
esphome run my-water-softener.yaml

# Select serial port when prompted
# Wait for compilation and upload (~2-3 minutes)
```

**Future Updates (OTA):**

```bash
# After first upload, use wireless updates
esphome run my-water-softener.yaml --device my-water-softener.local
```

### Step 5: Verify It's Working (2 minutes)

```bash
# Watch the logs
esphome logs my-water-softener.yaml
```

**Expected output:**
```
[I][culligan_water_softener] Connected to water softener
[I][culligan_water_softener] Handshake received, firmware: C4.38
[D][culligan_water_softener] Status 0: Time=15:29, Battery=100%, Flow=0.07 GPM
[D][culligan_water_softener] Status 1: Hardness=25 GPG, Brine=180 lbs
```

### Step 6: Check Home Assistant (1 minute)

1. Go to **Settings → Devices & Services → ESPHome**
2. Find your device (e.g., "water-softener-monitor")
3. Click on it - you should see all sensors!
4. Check values - data should appear within 30 seconds

## Troubleshooting

### "Can't find device"

```bash
# Make sure ESP32 is on same network
ping water-softener-monitor.local

# If not found, use IP address instead
esphome logs my-water-softener.yaml --device 192.168.1.XXX
```

### "No sensors in Home Assistant"

1. Check ESPHome logs for errors
2. Restart Home Assistant
3. Wait 1-2 minutes for discovery
4. Check Developer Tools → States for `sensor.water_*`

### "Disconnected from water softener"

- Verify MAC address is correct (case-sensitive!)
- Move ESP32 closer to water softener (< 30 feet)
- Ensure mobile app is not connected
- Try power cycling the water softener

## Minimal Working Configuration

If you just want to test quickly, use this minimal config:

```yaml
esphome:
  name: water-softener-test
  platform: ESP32
  board: esp32dev

wifi:
  ssid: "YourWiFi"
  password: "YourPass"

logger:
api:
ota:

ble_client:
  - mac_address: "E8:3B:AA:91:CD:68"
    id: ws_client

external_components:
  - source: {type: local, path: components}
    components: [culligan_water_softener]

culligan_water_softener:
  id: ws
  ble_client_id: ws_client

sensor:
  - platform: culligan_water_softener
    culligan_water_softener_id: ws
    current_flow:
      name: "Flow"
    soft_water_remaining:
      name: "Soft Water"
```

## Next Steps

Once working:

1. **Add more sensors** - See README.md for full sensor list
2. **Create automations** - Low water alerts, usage tracking
3. **Customize names** - Match your Home Assistant naming scheme
4. **Add filters** - Throttle updates if desired
5. **Monitor performance** - Check ESPHome logs occasionally

## Support

If you run into issues:

1. Check logs: `esphome logs my-water-softener.yaml`
2. Validate config: `esphome config my-water-softener.yaml`
3. Check ESPHome docs: https://esphome.io
4. See full README.md for detailed troubleshooting

## Success Checklist

- [ ] ESP32 compiled and uploaded successfully
- [ ] Logs show "Connected to water softener"
- [ ] Logs show "Handshake received, firmware: C4.XX"
- [ ] Logs show status packets with sensor data
- [ ] Home Assistant shows the device
- [ ] Sensors display real values (not "unavailable")
- [ ] Sensors update automatically

**Congratulations! Your water softener is now integrated with Home Assistant! 🎉**
