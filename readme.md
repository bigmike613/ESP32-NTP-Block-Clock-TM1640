# ESP32 NTP Block Clock (TM1640)

Wi-Fi connected digital clock with **web-based configuration** - no hardcoded credentials needed. Syncs time via NTP and displays on TM1640 4-digit LED modules (common in block/cube clocks).

## Features
- **Captive portal setup** - configure Wi-Fi, timezone, NTP server, and brightness on first boot
- **Web interface** - manage settings at `http://ntpclock.local` or device IP
- **8 US timezones** with automatic DST transitions
- **OTA updates** via Arduino IDE or PlatformIO
- **Persistent storage** - settings survive reboots
- 12-hour display with AM/PM indicators and blinking colon
- Visual alarm when NTP sync fails

## Hardware
- **ESP32** (any Arduino-compatible variant)
- **TM1640 4-digit LED display** (works with many block clock modules like [this 2.5" cube](https://www.amazon.com/dp/B07PFGG8C9))

### Wiring
```
TM1640 DIO → ESP32 GPIO 12
TM1640 CLK → ESP32 GPIO 13
TM1640 STB → ESP32 GPIO 7
GPIO 14 → Optional power control
VCC/GND → Appropriate power rails
```
NOTE: Some TM1640 modules require 5V. Check your module's specs.

## Software Requirements
Install via Arduino Library Manager:
- **NTP** by Stefan Staub
- **TM1640** LED driver library
- Built-in ESP32 libraries (WiFi, WebServer, DNSServer, Preferences, ESPmDNS, ArduinoOTA)

## Quick Start

### 1. Flash Firmware
Upload sketch to ESP32 via USB. No configuration needed.

### 2. Initial Setup
- Connect to Wi-Fi network: **NTP-Clock-Setup** (password: `clocksetup`)
- Captive portal opens automatically, or navigate to `http://192.168.4.1`
- Select your Wi-Fi network from the scanner
- Choose timezone, NTP server (default: `pool.ntp.org`), and brightness (0-7)
- Save and restart

### 3. Access Web Interface
After connecting, access at:
- `http://ntpclock.local` (if mDNS works)
- `http://[IP_ADDRESS]` (check your router)

## Web Interface Pages
- **Status (/)** - WiFi info, current time, NTP sync status
- **Settings (/settings)** - Change timezone, NTP server, brightness
- **Reset (/reset)** - Factory reset to AP mode

## Supported Timezones
Eastern, Central, Mountain, Pacific, Alaska, Hawaii (no DST), Arizona (no DST), UTC

DST transitions: 2nd Sunday in March (spring forward), 1st Sunday in November (fall back)

## Display Info
- **Format:** 12-hour with blinking colon (updates every 500ms)
- **AM/PM indicators:** Separate segment indicators
- **NTP sync failure:** Alarm indicator illuminates
- **Connection status:** Shows "CON" while connecting

## OTA Updates
- **Hostname:** `ntpclock`
- **Default password:** `admin`

**Arduino IDE:** Tools → Port → Network Ports → ntpclock  
**PlatformIO:** Add `upload_protocol = espota` and `upload_port = ntpclock.local` to platformio.ini

## Troubleshooting

**Clock shows "CON" continuously**  
Wi-Fi connection failed. Clock will restart in AP mode after 30 attempts. Reconnect to setup network.

**NTP sync fails (alarm indicator on)**  
Check internet connectivity, verify NTP server is reachable, ensure firewall allows UDP port 123.

**Display blank**  
Check power/wiring, verify brightness isn't 0, power cycle the ESP32.

**Can't access web interface**  
Use IP address instead of .local, check router for device IP, access via http:// not https://.

**OTA fails**  
Verify password, confirm same LAN, check firewall port 3232, try USB upload.

## Configuration Notes
- **Settings storage:** ESP32 NVS (non-volatile storage)
- **NTP sync:** Every 10 minutes, retry every 30 seconds on failure
- **Default passwords:** AP: `clocksetup`, OTA: `admin` - change both for production use
- **Wi-Fi password** stored in plaintext in NVS - no external data transmission except NTP queries


---
**Compatible with:** ESP32 Arduino Core 2.0.0+
