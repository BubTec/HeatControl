# ![HeatControl Logo](documentation/LOGO.png)

![Version](https://img.shields.io/github/v/tag/BubTec/HeatControl?label=Version&cache_seconds=0)
![Branch](https://img.shields.io/badge/dynamic/json?color=blue&label=Branch&query=$.default_branch&url=https://api.github.com/repos/BubTec/HeatControl)
![License](https://img.shields.io/github/license/BubTec/HeatControl)
![Last Commit](https://img.shields.io/github/last-commit/BubTec/HeatControl)
![Open Issues](https://img.shields.io/github/issues/BubTec/HeatControl)
![Pull Requests](https://img.shields.io/github/issues-pr/BubTec/HeatControl)
[![Lint Code Base](https://github.com/BubTec/HeatControl/actions/workflows/super-linter.yml/badge.svg)](https://github.com/BubTec/HeatControl/actions/workflows/super-linter.yml)

## Description
HeatControl is a dual-zone heating control system for drysuit diving, based on ESP32-C3. The controller regulates two heating channels using DS18B20 sensors and SSR/MOSFET outputs, and provides a web UI for setup and live monitoring.

## Features
- Dual-zone temperature control
- Web interface with live status
- Adjustable targets (10-45 C)
- Persistent settings in EEPROM
- Two modes:
  - Normal: temperature-based control
  - Power: full power output
- Swappable sensor assignment
- Captive portal AP mode
- OTA firmware upload from web UI (`/update`)

## Hardware
### Requirements
- ESP32-C3 board (DevKitM-1 compatible)
- 2x DS18B20 temperature sensors
- 2x SSR/MOSFET channels for heating
- Step-down converter (15V to 5V)
- 1000uF capacitor
- 4.7kOhm pull-up for OneWire

### Pin Mapping (current firmware)
- `GPIO3` -> OneWire bus (`ONE_WIRE_BUS`)
- `GPIO4` -> Heater channel 1 (`SSR_PIN_1`)
- `GPIO5` -> Heater channel 2 (`SSR_PIN_2`)
- `GPIO10` -> Boot mode input (`INPUT_PIN`)
- `GPIO6` -> Startup signal output (`SIGNAL_PIN`)

## Interface
### Pinout
![ESP32-C3 Pinout](documentation/PinOut.jpg)

**Important note about pin names (GPIO vs D-labels):**

- The firmware uses **GPIO numbers** (e.g. `GPIO4`) in `src/app_state.h`.
- Some ESP32-C3 boards also print **D-labels** (e.g. `D2`) on the silkscreen/pinout image. These are just aliases.
- When wiring, you can use either label, but always match the **GPIO number**.

**GPIO ↔ D-label mapping (as shown in the pinout image):**

| GPIO | Board label |
|------|------------|
| **3**  | **D1** |
| **4**  | **D2** |
| **5**  | **D3** |
| **6**  | **D4** |
| **10** | **D10** |

**Connections (see `src/app_state.h`):**

| GPIO | Function              | Connect to                          |
|------|------------------------|-------------------------------------|
| **3**  | OneWire bus            | DS18B20 data line (with 4.7kΩ pull-up to 3.3V) |
| **4**  | Heater channel 1       | SSR/MOSFET control (SSR_PIN_1)      |
| **5**  | Heater channel 2       | SSR/MOSFET control (SSR_PIN_2)      |
| **6**  | Startup signal output  | Optional status LED or external signal (SIGNAL_PIN) |
| **10** | Boot mode input        | Optional: hold low/high for power vs normal mode (INPUT_PIN) |

**Note:** `INPUT_PIN` here is a project-specific boot-mode input on **GPIO10 / D10** (not the ESP32-C3 BOOT button/strapping pin).

**Network:** AP IP `4.3.2.1` · Default SSID `HeatControl` · OTA at `/update`

### Web Interface
<img src="documentation/GUI.png" alt="Web Interface" width="300"/>

The web interface provides:
- Live temperature readings
- Target temperature controls
- Heater status indicators
- Sensor swap option
- WiFi configuration
- Runtime reset
- OTA update upload page

## Installation
1. Clone this repository
2. Open it in PlatformIO
3. Build and upload to ESP32-C3
4. Connect to AP `HeatControl`
   - SSID: `HeatControl`
   - Password: `HeatControl`

## Operation
1. Power on the device
2. Connect to the AP
3. Open `http://4.3.2.1`
4. Configure targets and monitor status
5. Optional OTA update: open `/update` and upload `firmware.bin`

## License
This project is licensed under GPL-3.0. See `LICENSE`.
