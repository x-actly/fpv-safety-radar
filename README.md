# FPV Safety Radar 🛡️✈️

![Platform](https://img.shields.io/badge/Hardware-ESP32--2432S028R%20%28CYD%29-brightgreen)
![Data Feeds](https://img.shields.io/badge/Traffic-ADS--B%20%7C%20OGN%20FLARM-blue)
![UI Engine](https://img.shields.io/badge/Display-TFT__eSPI%20Double%20Buffered-orange)
![License](https://img.shields.io/badge/License-MIT-green)

An open-source, portable real-time air traffic radar for FPV drone pilots, general aviation enthusiasts, and spotters. Built specifically for the **ESP32-2432S028R** (popularly known as the **"Cheap Yellow Display" / CYD** 2.8" TFT resistive touch screen).

**FPV Safety Radar** tracks nearby commercial aircraft (ADS-B) and gliders/paramotors (Open Glider Network / FLARM) in real time. It uses your smartphone's web browser as a GPS & digital compass sensor source to dynamically orient the radar display to where you are standing and facing!

---

## 🌟 Key Features

* **Retro Tactical Radar Scope**:
  * Phosphor Green CRT radar aesthetic with dynamic 360° sweep line.
  * Smooth double-buffered sprite rendering (`TFT_eSPI`) eliminating screen flicker.
  * Auto-scaling concentric range rings (5 km, 10 km, 25 km, 50 km).
  * Target track vectors, altitude differentials (`+08` / `-05`), and callsign labels.
  * **Color-Coded Targets**:
    * 🟢 **Emerald Green**: ADS-B Commercial & General Aviation
    * 🔵 **Cyan**: OGN / FLARM Gliders, Helicopters & Paramotors
    * 🔴 **Flashing Red**: Collision Threat / Proximity Warning
* **Smartphone GPS & Compass Companion**:
  * Built-in HTTP web server hosts a companion streaming page (`http://<ESP32-IP>/gps`).
  * Connects to your phone's browser, streams live **GPS coordinates** and **magnetic compass heading** (`DeviceOrientation`) at 1 Hz.
  * Rotates the radar scope orientation according to the direction your phone/display is facing.
* **Pixel-Perfect Main Menu**:
  * Clean dark interface with uniform touch buttons (`START RADAR`, `WI-FI SETTINGS`, `ALERT LIMIT`).
  * Instant status readout for Wi-Fi connection, target count, and phone GPS lock.
* **Configurable Safety Alerts**:
  * Adjustable alert distance (e.g., 5 km to 50 km) and altitude threshold (up to 10,000 m).
  * Acoustic alert tones via CYD onboard speaker (`GPIO 26`).
  * RGB Status LED visual warnings (`GPIO 4/16/17`).
* **Interactive Touch Controls**:
  * **`[RNG]`**: Cycle radar scale (5 km ➔ 10 km ➔ 25 km ➔ 50 km).
  * **`[FLT]`**: Filter live traffic (`ALL` / `ADSB` / `OGN`).
  * **`[MENU]`**: Return to main menu at any time.
  * **Target Inspector**: Tap any target dot on screen to open detailed overlay (Callsign, Registration, Altitude, Speed, Distance, Bearing, Source).

---

## 📱 Hardware Requirements & Pinout

### ESP32-2432S028R (Cheap Yellow Display / CYD)

| Peripheral | Component / Signal | Pin / SPI Bus |
| :--- | :--- | :--- |
| **Display** | ILI9341 2.8" SPI (240x320) | CS: `15`, DC: `2`, MOSI: `13`, SCLK: `14`, BL: `21` |
| **Touchscreen** | XPT2046 SPI | CS: `33`, IRQ: `36`, MOSI: `32`, MISO: `39`, CLK: `25` |
| **RGB LED** | Visual Warning Indicator | RED: `GPIO 4`, GREEN: `GPIO 16`, BLUE: `GPIO 17` *(Active LOW)* |
| **Audio** | Buzzer / Speaker | `GPIO 26` |
| **Light Sensor** | LDR | `GPIO 34` |

---

## 🚀 Quick Start & Installation

### Option 1: PlatformIO (Recommended)

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE Extension**.
2. Clone or download this repository:
   ```bash
   git clone https://github.com/<YOUR_USERNAME>/fpv-safety-radar.git
   cd fpv-safety-radar
   ```
3. Open the project folder in VS Code / PlatformIO.
4. Connect your ESP32-2432S028R board via USB cable.
5. Click **PlatformIO: Build** and then **PlatformIO: Upload**.

### Option 2: Arduino IDE

1. Add ESP32 board support in Arduino IDE Preferences:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Install the following libraries via Arduino Library Manager:
   * **`TFT_eSPI`** by Bodmer
   * **`XPT2046_Touchscreen`** by Paul Stoffregen
   * **`ArduinoJson`** (v7.x)
   * **`WiFiManager`** by tzapu
3. Configure `User_Setup.h` in `TFT_eSPI` with the CYD pin numbers listed in the table above.
4. Open `src/main.cpp` (or rename to `.ino`), select board **ESP32 Dev Module**, and upload.

---

## 🌐 Wi-Fi & Smartphone GPS Companion Setup

1. **Initial Wi-Fi Setup**:
   * Power on your ESP32 CYD display.
   * If no Wi-Fi network is saved, the screen displays `AP: ESP32-Radar-Setup`.
   * On your mobile phone, connect to Wi-Fi SSID **`ESP32-Radar-Setup`** (Password: **`radar123`**).
   * A configuration window will open automatically. Choose your local Wi-Fi router or mobile hotspot, enter your password, and save.
2. **Smartphone Companion & Compass Stream**:
   * Once connected, note the IP address shown on the CYD main menu (e.g. `192.168.43.100`).
   * Open your mobile browser on your smartphone and go to:
     ```
     http://<ESP32-IP>/gps
     ```
   * Tap **"Start GPS & Compass Streaming"** and grant browser location & motion permissions.
   * Your phone will now stream precise GPS position and magnetic compass orientation to the ESP32 radar scope in real time!

---

## 📂 Project Structure

```
fpv-safety-radar/
├── include/
│   ├── adsb_client.h       # ADS-B REST client API parser
│   ├── aircraft.h          # Aircraft target data structures
│   ├── cyd_pinout.h        # ESP32-2432S028R hardware pins & setup
│   ├── ogn_client.h         # Open Glider Network APRS client
│   ├── radar_display.h     # UI engine, graphics & menu interface
│   └── wifi_manager.h      # Wi-Fi setup & mobile GPS web server
├── src/
│   ├── adsb_client.cpp
│   ├── main.cpp            # Main loop & task dispatcher
│   ├── ogn_client.cpp
│   ├── radar_display.cpp   # Double-buffered radar rendering engine
│   └── wifi_manager.cpp
├── pixel10_radar_companion.html # Standalone mobile web app source
├── platformio.ini          # PlatformIO build & dependency manifest
├── README.md               # Project documentation
└── LICENSE                 # MIT License
```

---

## ⚡ Performance Optimization & Memory Management

* **Zero-Lag Display Engine**: Dynamic radar redraws utilize RAM sprites to eliminate screen tearing and flickering on the ILI9341 SPI bus.
* **Memory-Optimized JSON Parsing**: Uses `ArduinoJson 7` stream filtering to discard unnecessary fields, reducing RAM usage from 60 KB down to ~4 KB per API query.
* **Flash Longevity**: High-frequency live GPS updates are processed purely in RAM without writing to NVS SPI Flash memory.

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## ⚠️ Disclaimer

*This software is intended solely as an auxiliary situational awareness tool for FPV pilots and spotters. It must NEVER be used as a primary collision avoidance system or certified navigation device.*
