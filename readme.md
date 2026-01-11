# ESP32 NTP Block Clock (TM1640)

This project turns an **ESP32** into a Wi‑Fi connected digital clock that syncs time from an **NTP server** and displays it on a **TM1640-based 4‑digit LED display** (common in “block/cube” clocks). It also supports **Arduino OTA** updates.

## What it does

- Connects to Wi‑Fi using credentials in the sketch
- Retrieves time from `pool.ntp.org` using `NTP`
- Displays **12‑hour** time with a **blinking colon**
- Shows **AM/PM** status using the display’s extra indicator segments
- **If the NTP server is not reachable, the alarm light/indicator will illuminate**
- Supports **Over‑The‑Air (OTA)** firmware updates

## Hardware

- **ESP32** (Arduino framework)
- **TM1640 4‑digit LED display module**
- Can be used with many “block clock” style displays, including this 2.5" cube clock:
  - https://www.amazon.com/dp/B07PFGG8C9

> Note: Many retail block clocks include their own controller board. To use this project, you typically need access to the TM1640 display signals (CLK/DIO and optional STB) going to the LED board.

## Default Pinout

Configured in the sketch as:

```cpp
// Pins connected to display
const int PIN_DIO   = 18;
const int PIN_CLOCK = 19;
// usually not needed for display to work
const int PIN_STB   = 7;
// optional for display power reset at boot
const int PIN_power = 4;
```

### Typical wiring

- **TM1640 DIO → ESP32 GPIO 18**
- **TM1640 CLK → ESP32 GPIO 19**
- **TM1640 STB → ESP32 GPIO 7** (often optional depending on module)
- **VCC/GND** accordingly
- **GPIO 4** is toggled at boot as an optional power-enable control (useful if you have a transistor/MOSFET controlling display power)

⚠️ Some TM1640 boards are powered at **5V**. Verify your module’s voltage requirements and consider level shifting if needed.

## Software / Libraries

The sketch uses these Arduino libraries/headers:

- `WiFi.h`, `WiFiUdp.h`
- `ArduinoOTA.h`
- `NTP.h`
- `TM1640.h`

Install them via Arduino Library Manager or your build system.

## Configuration

Edit these values in the `.ino/.c` file:

### Wi‑Fi

```cpp
const char* ssid     = "<WI-FI SSID HERE>";
const char* password = "<WI-FI Password here>";
```

### Timezone (Including DST)

```cpp
 // timezone settings
  ntp.ruleDST("EDT", Second, Sun, Mar, 2, -240);
  ntp.ruleSTD("EST", First, Sun, Nov, 2, -300);
```

### NTP refresh interval

```cpp
const long updateinterval_Msec = 600000; // 10 minutes
```

### Brightness

```cpp
const int brightness = 3; // 0–7
```

## NTP failure indicator (“alarm light”)

The main loop calls `ntpUpdateReturnSuccess()` regularly. When it fails (e.g., no internet or the NTP server is unreachable), the firmware sets the display’s indicator segments to turn on the **alarm** indicator so you have a visible warning that time sync is failing.

## OTA Updates

OTA is enabled with:

```cpp
ArduinoOTA.setPassword("admin");
ArduinoOTA.begin();
```

- Default password: `admin` (change this for real use)
- Ensure your network allows OTA traffic and your ESP32 is on the same LAN as your uploader

## Build & Upload

### Arduino IDE

1. Install ESP32 board support (Arduino-ESP32)
2. Install required libraries
3. Select your ESP32 board and serial port
4. Upload via USB for the first flash
5. After it joins Wi‑Fi, upload via the **Network Port** (OTA)

### PlatformIO (optional)

- Use the Arduino framework for ESP32
- Add libraries in `platformio.ini`
- Configure OTA with `upload_protocol = espota` if desired

## License

Add a license file (e.g., MIT) if you plan to publish this publicly.
