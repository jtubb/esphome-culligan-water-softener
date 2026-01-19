# Culligan CS Meter Soft - BLE Protocol Documentation

This document describes the Bluetooth Low Energy protocol used by the Culligan CS Meter Soft water softener. All information has been confirmed through APK decompilation and real device testing.

## Device Information

- **Device Name**: `CS_Meter_Soft`
- **MAC Address**: `E8:3B:AA:91:CD:68` (example)
- **Service**: Nordic UART Service (NUS)
  - Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
  - TX Characteristic (Device → App): `6e400003-b5a3-f393-e0a9-e50e24dcca9e` (Notifications)
  - RX Characteristic (App → Device): `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (Write)

## Communication Protocol

The device uses a packet-based protocol where each packet type is identified by a 2-byte ASCII header.

**IMPORTANT**: Multi-byte values use **BIG-ENDIAN** format (high byte at lower index).

### Packet Types

| Header | Hex | Type | Request | Description |
|--------|-----|------|---------|-------------|
| `rr` | 0x72 | Reset | `r` × 20 | Device reset |
| `tt` | 0x74 | DeviceList | `t` × 20 | Handshake/Authentication |
| `uu` | 0x75 | Dashboard | `u` × 20 | Real-time status data (6 packets) |
| `vv` | 0x76 | AdvancedSettings | `v` × 20 | Configuration settings (4 packets) |
| `ww` | 0x77 | StatusAndHistory | `w` × 20 | Statistics/history (4 packets) |
| `xx` | 0x78 | DealerInformation | `x` × 20 | Dealer info / Keepalive |

### Packet End Markers

Each packet has a specific end marker at byte 19:

| Packet | End Marker | Hex |
|--------|------------|-----|
| uu-0 | '9' | 0x39 |
| uu-1 | ':' | 0x3A |
| uu-2 | ';' | 0x3B |
| uu-3 | '<' | 0x3C |
| uu-4 | '=' | 0x3D |
| uu-5 | '>' | 0x3E |
| vv-0 | 'B' | 0x42 |
| vv-1 | 'C' | 0x43 |
| vv-2 | 'D' | 0x44 |
| vv-3 | 'E' | 0x45 |
| ww-0 | 'F' | 0x46 |
| ww-1 | 'G' | 0x47 |
| ww-2 | 'H' | 0x48 |
| ww-3 | 'I' | 0x49 |

## Authentication (Required for Firmware < 6.0)

The device requires authentication after each connection. The auth packet uses CRC8-chained password encoding.

### Handshake Response (tt packet)

**Example**: `74-74-00-01-00-04-38-80-04-03-00-36-00-00-71-47-78-00`

| Offset | Bytes | Type | Description | Example |
|--------|-------|------|-------------|---------|
| 0-1 | 2 | ASCII | Packet type: "tt" | 0x74 0x74 |
| 2 | 1 | uint8 | Packet number | 0x00 |
| 5 | 1 | uint8 | Firmware major version | 0x04 = 4 |
| 6 | 1 | uint8 | Firmware minor version (BCD) | 0x38 = 38 → "C4.38" |
| 7 | 1 | uint8 | Auth status | 0x80 = auth required |
| 11 | 1 | uint8 | Connection counter | Used in auth calculation |

### Auth Packet (20 bytes)

```
Byte 0-1: 0x74 0x74 ("tt")
Byte 2:   0x50 ('P')
Byte 3:   0x41 ('A')
Byte 4:   CRC8 polynomial (randomly selected, 4-5 bits set)
Byte 5:   seed (random 1-255)
Byte 6:   random2 XOR seed
Byte 7:   CRC8(counter_xor) XOR password_digit[3]
Byte 8:   password_digit[2] XOR CRC8(byte[7])
Byte 9:   password_digit[1] XOR CRC8(byte[8])
Byte 10:  password_digit[0] XOR CRC8(byte[9])
Byte 11-19: Random padding
```
Where `counter_xor = connection_counter XOR CRC8_legacy(byte[6])`

See `auth_algorithm.py` for complete implementation.

## Packet Structures

### Status Packet (uu) - Dashboard - 6 Packets

The status packet is sent as **6 separate notifications**, numbered 0-5 in byte offset 2.

#### Packet 0: Real-time Data (end marker 0x39 '9')

**Confirmed from APK decompilation** (`CsDashboardPacket.parsePacket1`):

| Offset | Bytes | Type | Description | Decoding |
|--------|-------|------|-------------|----------|
| 0-1 | 2 | ASCII | Packet type: "uu" | 0x75 0x75 |
| 2 | 1 | uint8 | Packet number | 0x00 |
| 3 | 1 | uint8 | Hour (1-12) | Direct value |
| 4 | 1 | uint8 | Minutes (0-59) | Direct value |
| 5 | 1 | uint8 | AM/PM | 0=AM, 1=PM |
| 6 | 1 | uint8 | Battery level | See battery encoding below |
| 7-8 | 2 | uint16_be | Current flow | (high×256+low) ÷ 100 = GPM |
| 9-10 | 2 | uint16_be | Soft water remaining | high×256+low = gallons |
| 11-12 | 2 | uint16_be | Water usage today | high×256+low = gallons |
| 13-14 | 2 | uint16_be | Peak flow today | (high×256+low) ÷ 100 = GPM |
| 15 | 1 | uint8 | Water hardness | Direct value in GPG |
| 16 | 1 | uint8 | Regen hour (1-12) | Direct value |
| 17 | 1 | uint8 | Regen AM/PM | 0=AM, 1=PM |
| 18 | 1 | uint8 | Flags | See bit flags below |
| 19 | 1 | uint8 | End marker | 0x39 ('9') |

**Battery Level Encoding** (`getBatteryCapacity()`):
| Raw Value | Battery % |
|-----------|-----------|
| 0 | 0% |
| 1 | 100% |
| 2 | 75% |
| 3 | 50% |
| 4 | 25% |
| 5+ | 0% |

**Flags Byte 18** (1-based bit numbering):
| Bit | Mask | Description |
|-----|------|-------------|
| 1 | 0x01 | Shutoff setting enabled |
| 2 | 0x02 | Bypass setting enabled |
| 3 | 0x04 | Shutoff state active |
| 4 | 0x08 | Bypass state active |
| 5 | 0x10 | Display off |

#### Packet 1: Brine Tank & Regen Status (end marker 0x3A ':')

**Confirmed from APK decompilation** (`CsDashboardPacket.parsePacket2` and `updateBrineTankData`):

| Offset | Bytes | Type | Description | Notes |
|--------|-------|------|-------------|-------|
| 0-1 | 2 | ASCII | Packet type: "uu" | 0x75 0x75 |
| 2 | 1 | uint8 | Packet number | 0x01 |
| 3 | 1 | uint8 | Filter backwash days | Days until filter backwash |
| 4 | 1 | uint8 | Air recharge days | Days until air recharge |
| 5 | 1 | uint8 | Position time | Current cycle position time |
| 6 | 1 | uint8 | Position option seconds | |
| 7 | 1 | uint8 | Regen cycle position | Current regen cycle step |
| 8 | 1 | uint8 | Regen active | 0=inactive, 1=active |
| 13 | 1 | uint8 | Brine tank regens remaining | 0xFF = not configured |
| 14 | 1 | uint8 | Low salt alert threshold | Regens count |
| 15 | 1 | uint8 | Tank type (diameter) | 16, 18, 24, or 30 inches |
| 16 | 1 | uint8 | Fill height | Inches (salt fill level) |
| 17 | 1 | uint8 | Brine refill time | Minutes (from cycle settings) |
| 19 | 1 | uint8 | End marker | 0x3A (':') |

**Brine Tank Salt Calculation** (firmware >= 4.22):

If byte[13] != 0xFF (brine tank is configured):

1. **Total capacity** = fillHeight × tankMultiplier

   | Tank Diameter | Multiplier | Example (24" height) |
   |---------------|------------|----------------------|
   | 16" | 8.1 | 194.4 lbs |
   | 18" | 10.4 | 249.6 lbs |
   | 24" | 18.6 | 446.4 lbs |
   | 30" | 29.55 | 709.2 lbs |

2. **Salt per regen** (residential) = refillTime × 1.5 lbs

3. **Salt remaining** = saltPerRegen × regensRemaining

**Example**: 16" tank, 24" fill height, 15 min refill, 2 regens remaining
- Total capacity: 24 × 8.1 = 194.4 lbs
- Salt per regen: 15 × 1.5 = 22.5 lbs
- Salt remaining: 22.5 × 2 = 45.0 lbs

#### Packets 2-5: Historical Data

These packets contain daily water usage history arrays.

### Settings Packet (vv) - Advanced Settings - 4 Packets

#### Packet 0: Configuration (end marker 0x42 'B')

| Offset | Bytes | Type | Description | Notes |
|--------|-------|------|-------------|-------|
| 0-1 | 2 | ASCII | Packet type: "vv" | 0x76 0x76 |
| 2 | 1 | uint8 | Packet number | 0x00 |
| 3 | 1 | uint8 | Days until regen | |
| 4 | 1 | uint8 | Regen day override | Days (0-29) |
| 5 | 1 | uint8 | Reserve capacity | Percentage (0-49) |
| 6-7 | 2 | uint16_be | Resin grain capacity | Value × 100 = grains |
| 8 | 1 | uint8 | Pre-fill enabled | 0=off, 1=on |
| 9 | 1 | uint8 | Pre-fill duration | Hours (1-4) |
| 10 | 1 | uint8 | Brine soak duration | Hours (1-4) |
| 16 | 1 | uint8 | Flags | Same as uu-0 byte 18 |
| 19 | 1 | uint8 | End marker | 0x42 ('B') |

#### Packet 1: Cycle Times (end marker 0x43 'C')

| Offset | Bytes | Type | Description | Notes |
|--------|-------|------|-------------|-------|
| 0-1 | 2 | ASCII | Packet type: "vv" | 0x76 0x76 |
| 2 | 1 | uint8 | Packet number | 0x01 |
| 3 | 1 | uint8 | Backwash time | Minutes (see encoding) |
| 4 | 1 | uint8 | Brine draw time | Minutes |
| 5 | 1 | uint8 | Rapid rinse time | Minutes |
| 6 | 1 | uint8 | Brine refill time | Minutes |
| 7-10 | 4 | uint8 | Positions 5-8 | Minutes |
| 19 | 1 | uint8 | End marker | 0x43 ('C') |

**Cycle Time Encoding**:
- If high bit (0x80) is SET: Position is **fixed/non-adjustable**
  - Actual time = (value & 0x7F)
  - Example: 0x8A = fixed at 10 minutes
- If high bit is CLEAR: Position is adjustable
  - Actual time = value directly
  - Example: 0x0A = adjustable, 10 minutes

### Statistics Packet (ww) - Status & History - 4 Packets

#### Packet 0: Totals & Counters (end marker 0x46 'F')

**Confirmed from APK decompilation** (`CsStatusAndHistoryPacket.updatePacket`):

| Offset | Bytes | Type | Description | Decoding |
|--------|-------|------|-------------|----------|
| 0-1 | 2 | ASCII | Packet type: "ww" | 0x77 0x77 |
| 2 | 1 | uint8 | Packet number | 0x00 |
| 3-4 | 2 | uint16_be | Current flow | (high×256+low) ÷ 100 = GPM |
| 5-7 | 3 | uint24_be | Total gallons treated | high×65536 + med×256 + low |
| 8-10 | 3 | uint24_be | Total gallons (resettable) | Same formula |
| 11-12 | 2 | uint16_be | Total regenerations | high×256 + low |
| 13-14 | 2 | uint16_be | Regens (resettable) | high×256 + low |
| 15 | 1 | uint8 | Regen active flag | 0 = not regenerating |
| 19 | 1 | uint8 | End marker | 0x46 ('F') |

#### Packets 1-3: Historical Statistics

Contains daily water usage and peak flow arrays for graphing.

### Keepalive Packet (xx)

**Example**: `78-78-00-00-00-00`

This packet serves as a heartbeat to maintain the connection. Send periodically to prevent timeout.

## Write Commands

All write commands are 20 bytes. The buffer is filled with the base packet type character, then specific bytes are modified.

### Dashboard Commands (base: 0x75 'u' × 20)

| Command | Byte 13 | Byte 14 | Byte 15 | Byte 16 | Byte 17 | Description |
|---------|---------|---------|---------|---------|---------|-------------|
| Regen Now | 'R' (82) | 'N' (78) | - | - | - | Start immediate regeneration |
| Regen Next | 'R' (82) | 'T' (84) | - | - | - | Regenerate at next scheduled time |
| Set Time | 'T' (84) | hour (1-12) | minute (0-59) | AM/PM (0/1) | second (0-59) | Set device clock |
| Set Hardness | 'H' (72) | hardness (0-99) | - | - | - | Set water hardness GPG |
| Set Regen Time | 't' (116) | hour (1-12) | AM/PM (0/1) | - | - | Set regeneration time |
| Set Salt Level | 'S' (83) | regens (0-100) | low_alert (0-100) | tank_type | fill_height (0-50) | Configure brine tank |
| Filter Backwash | 'F' (70) | days (0-29) | - | - | - | Set filter backwash frequency |

**Set Salt Level Notes**:
- `regens`: Calculate from salt_lbs ÷ salt_per_regen
- `low_alert`: Regens remaining to trigger low salt alert
- `tank_type`: 16, 18, 24, or 30 (diameter in inches)
- `fill_height`: Salt fill height in inches

### Advanced Settings Commands (base: 0x76 'v' × 20)

| Command | Byte 13 | Byte 14 | Byte 15 | Byte 16 | Description |
|---------|---------|---------|---------|---------|-------------|
| Set Regen Days | 'A' (65) | days (0-29) | - | - | Days between regens (0=demand only) |
| Set Reserve | 'B' (66) | percent (0-49) | - | - | Reserve capacity % |
| Set Resin Cap | 'C' (67) | high_byte | low_byte | - | Capacity ÷ 100 (0-399) |
| Set Rental Disable | 'D' (68) | enable (0/1) | - | - | Rental regen disable |
| Air Recharge | 'E' (69) | days (0-9) | - | - | Air recharge frequency |
| Pulse Chlorine | 'F' (70) | level (0-4) | - | - | Chlorine pulse level |
| Set Display | 'G' (71) | state (0/1) | - | - | Display on(0)/off(1) |
| Set Cycle Time | 'P' (80) | position | minutes (0-99) | - | Set cycle position time |
| Set Pre-fill | 'R' (82) | 'P' (80) | enable (0/1) | hours (1-4) | Pre-fill settings |

**Position Values** (for Set Cycle Time):
| Position | Value | Description |
|----------|-------|-------------|
| 1 | 49 ('1') | Backwash |
| 2 | 50 ('2') | Brine Draw / Slow Rinse |
| 3 | 51 ('3') | Rapid Rinse |
| 4 | 52 ('4') | Brine Refill |
| 5-8 | 53-56 | Additional positions |

**Note**: Positions marked as "fixed" (high bit set in response) cannot be changed.

### Statistics Commands (base: 0x77 'w' × 20)

| Command | Byte 13 | Description |
|---------|---------|-------------|
| Reset Gallons | 'A' (65) | Reset resettable gallons counter |
| Reset Regens | 'B' (66) | Reset resettable regen counter |

## Connection Sequence

1. **Scan** for device with name `CS_Meter_Soft`
2. **Connect** to device via BLE
3. **Enable notifications** on TX characteristic (`6e400003...`)
4. **Send handshake request** (`t` × 20 bytes)
5. **Receive handshake response** (tt) with firmware version and connection counter
6. **Send authentication** if firmware < 6.0 (see Authentication section)
7. **Request data**:
   - Send `u` × 20 → Receive 6 uu packets (status)
   - Send `v` × 20 → Receive 4 vv packets (settings)
   - Send `w` × 20 → Receive 4 ww packets (statistics)
8. **Parse notifications** based on packet type header and packet number

### Wake Strategy

For devices in deep sleep, a two-connection wake strategy may be needed:
1. First connection wakes the device (may timeout after ~7 seconds)
2. Second connection immediately receives data

However, testing shows a single connection with authentication works reliably when the device is recently active.

## Bit Numbering (CRITICAL)

The APK uses **1-based bit numbering** with `isBitSet(value, bit)` which checks:
```
(value & (1 << (bit - 1))) != 0
```

| APK Bit # | Mask | Standard Bit |
|-----------|------|--------------|
| Bit 1 | 0x01 | bit 0 |
| Bit 2 | 0x02 | bit 1 |
| Bit 3 | 0x04 | bit 2 |
| Bit 4 | 0x08 | bit 3 |
| Bit 5 | 0x10 | bit 4 |
| Bit 6 | 0x20 | bit 5 |
| Bit 7 | 0x40 | bit 6 |
| Bit 8 | 0x80 | bit 7 |

## Scaling Reference

| Measurement | Formula | Example |
|-------------|---------|---------|
| Flow rate (GPM) | raw ÷ 100 | 125 → 1.25 GPM |
| Resin capacity (grains) | raw × 100 | 320 → 32,000 grains |
| Salt capacity (lbs) | height × multiplier | 24" × 8.1 = 194.4 lbs |
| Salt per regen (lbs) | refill_min × 1.5 | 15 min → 22.5 lbs |
| All other values | Direct | Use as-is |

**Note**: Flow divisor is **100** (not 480 as incorrectly stated in some documentation).

## Implementation Tips

1. **Accumulate data across packets**: Don't clear parsed data between refreshes. Multiple packet types contribute to the full device state.

2. **Handle multi-packet sequences**: Status (uu), settings (vv), and statistics (ww) each arrive as multiple packets. Buffer until all packets received.

3. **Verify end markers**: Each packet has a specific end marker at byte 19. Use this to validate complete packet reception.

4. **Check packet numbers**: Byte 2 contains the packet number (0-5 for uu, 0-3 for vv/ww). Packets may arrive out of order.

5. **Timeout handling**: Device disconnects after ~8 seconds of inactivity. Send keepalive (xx) or data requests periodically.

6. **Fixed cycle times**: Check high bit (0x80) of cycle time values. If set, that position cannot be adjusted via commands.

7. **Brine tank setup check**: If uu-1 byte[13] = 0xFF, brine tank is not configured. The salt calculation fields will be invalid.

8. **Firmware version**: Check handshake response for firmware version. Features like brine tank monitoring require firmware >= 4.22.

9. **Password**: Default password is 1234. Required for authentication on firmware < 6.0.

10. **MTU**: Device supports 247 byte MTU, but all packets are 20 bytes.
