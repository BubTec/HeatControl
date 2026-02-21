# ![HeatControl Logo](documentation/LOGO.png)

![Version](https://img.shields.io/github/v/tag/BubTec/HeatControl?label=Version&cache_seconds=0)
![Branch](https://img.shields.io/badge/dynamic/json?color=blue&label=Branch&query=$.default_branch&url=https://api.github.com/repos/BubTec/HeatControl)
![License](https://img.shields.io/github/license/BubTec/HeatControl)
![Last Commit](https://img.shields.io/github/last-commit/BubTec/HeatControl)
![Open Issues](https://img.shields.io/github/issues/BubTec/HeatControl)
![Pull Requests](https://img.shields.io/github/issues-pr/BubTec/HeatControl)
[![Lint Code Base](https://github.com/BubTec/HeatControl/actions/workflows/super-linter.yml/badge.svg)](https://github.com/BubTec/HeatControl/actions/workflows/super-linter.yml)

## Description
HeatControl is a dual-zone heating control system for drysuit diving, based on ESP32-C3. The controller regulates two heating channels using DS18B20 sensors and SSR/MOSFET outputs, monitors MOSFET temperature with two NTC sensors, and provides a web UI for setup and live monitoring.

## Features
- Dual-zone temperature control
- Web interface with live status
- Adjustable targets (10-45 C)
- Persistent settings in EEPROM
- Three modes:
  - Normal: temperature-based control (requires two valid sensors)
  - Power: full power output
  - Manual: fallback PWM mode when fewer than two sensors are available
- Swappable sensor assignment
- Captive portal AP mode
- OTA firmware upload from web UI (`/update`)
- MOSFET overtemperature protection with 2x NTC (trip at 80 C, re-enable at 75 C)

## Hardware
### Requirements
- ESP32-C3 board (DevKitM-1 compatible)
- 2x DS18B20 temperature sensors
- 2x SSR/MOSFET channels for heating
- 2x NTC thermistors (10k, B3950 recommended) for MOSFET monitoring
- 2x 10kOhm resistors (for NTC voltage divider)
- Step-down converter (15V to 5V)
- 1000uF capacitor
- 4.7kOhm pull-up for OneWire

### Pin Mapping (current firmware)
- `GPIO7` -> OneWire bus (`ONE_WIRE_BUS`)
- `GPIO4` -> Heater channel 1 (`SSR_PIN_1`)
- `GPIO5` -> Heater channel 2 (`SSR_PIN_2`)
- `GPIO10` -> Boot mode input (`INPUT_PIN`)
- `GPIO6` -> Startup signal output (`SIGNAL_PIN`)
- `GPIO0` -> ADC voltage input 1 (`ADC_PIN_1`)
- `GPIO1` -> ADC voltage input 2 (`ADC_PIN_2`)
- `GPIO3` -> MOSFET 1 NTC ADC input (`ADC_PIN_NTC_MOSFET_1`)
- `GPIO2` -> MOSFET 2 NTC ADC input (`ADC_PIN_NTC_MOSFET_2`)
- `GPIO20`/`GPIO21` -> UART0 reserved (left free by firmware)

### NTC Wiring (per MOSFET channel)
- Divider topology used by firmware: `3.3V -> NTC (10k, B3950) -> ADC node -> 10k resistor -> GND`
- Channel 1 ADC node -> `GPIO3` (`ADC_PIN_NTC_MOSFET_1`)
- Channel 2 ADC node -> `GPIO2` (`ADC_PIN_NTC_MOSFET_2`)
- Important: keep ADC pin voltage within `0..3.3V`

## Interface
### Pinout
![ESP32-C3 Pinout](documentation/PinOut.jpg)

**Important note about pin names (GPIO vs D-labels):**

- The firmware uses **GPIO numbers** (e.g. `GPIO4`) in `src/app_state.h`.
- Some ESP32-C3 boards also print **D-labels** (e.g. `D2`) on the silkscreen/pinout image. These are just aliases.
- When wiring, you can use either label, but always match the **GPIO number**.

**Connections (see `src/app_state.h`):**

| GPIO | Board label | Function              | Connect to                          |
|------|------------|------------------------|-------------------------------------|
| **7**  | **D5 / SS**     | OneWire bus            | DS18B20 data line (with 4.7kΩ pull-up to 3.3V) |
| **4**  | **D2 / A2**     | Heater channel 1       | SSR/MOSFET control (SSR_PIN_1)      |
| **5**  | **D3 / A3**     | Heater channel 2       | SSR/MOSFET control (SSR_PIN_2)      |
| **6**  | **D4 / SDA**    | Startup signal output  | Optional status LED or external signal (SIGNAL_PIN) |
| **10** | **D10 / MOSI**  | Boot mode input        | Optional: hold low/high for power vs normal mode (INPUT_PIN) |
| **0**  | **ADC1-0 / A0** | ADC voltage input 1    | Voltage sense input (use a divider; max 3.3V at the pin) |
| **1**  | **ADC1-1**      | ADC voltage input 2    | Voltage sense input (use a divider; max 3.3V at the pin) |
| **3**  | **D1 / A1**     | MOSFET 1 NTC ADC input | NTC divider on MOSFET 1 (`ADC_PIN_NTC_MOSFET_1`, max 3.3V at pin) |
| **2**  | **A2**          | MOSFET 2 NTC ADC input | NTC divider on MOSFET 2 (`ADC_PIN_NTC_MOSFET_2`, max 3.3V at pin) |
| **20/21** | **RX/TX**    | UART0                  | Reserved/free for serial adapter or debugging |

**Note:** `INPUT_PIN` here is a project-specific boot-mode input on **GPIO10 / D10** (not the ESP32-C3 BOOT button/strapping pin).
**Note:** ADC readings are reported in the UI/Serial as **millivolts** (`analogReadMilliVolts`).
**Note:** MOSFET overtemperature protection uses both NTC channels and blocks the affected heater above **80C** (re-enable below 75C). The trip is persisted as a latched diagnostics event (incl. trip temperature), shown as `HOT/TRIP` in the heater cards, and can be acknowledged via the Diagnostics reset button.

### Vibration / signal patterns (`SIGNAL_PIN` / GPIO6)

`SIGNAL_PIN` (GPIO6) can drive a small vibration motor or an LED to provide haptic/visual feedback.  
The firmware uses the following patterns (timings are approximate):

- **Boot: normal mode**
  - **Pattern**: 1× long pulse (HIGH ~300 ms, LOW ~200 ms)
  - **When**: Device starts in **normal** temperature-control mode.

- **Boot: power mode**
  - **Pattern**: 2× long pulses (each HIGH ~300 ms, LOW ~200 ms)
  - **When**: Device starts in **power** mode (full power).

- **Boot: manual mode**
  - **Pattern**:
    - 1× long intro pulse (HIGH ~500 ms, LOW ~220 ms)
    - Then **N** short pulses to show the manual power for heater channel 1:
      - 1× short pulse  → 25 %
      - 2× short pulses → 50 %
      - 3× short pulses → 75 %
      - 4× short pulses → 100 %
    - Short pulses are HIGH ~130 ms, LOW ~130 ms.
  - **When**: Fewer than two DS18B20 sensors are detected and the controller enters **manual** mode at boot.

- **Manual mode: change of manual power (heater 1 or 2)**
  - **Pattern**:
    - **N** short pulses (HIGH ~130 ms, LOW ~130 ms) encoding the **new** manual duty:
      - 1× short pulse  → 25 %
      - 2× short pulses → 50 %
      - 3× short pulses → 75 %
      - 4× short pulses → 100 %
  - **When**:
    - A short battery OFF/ON sequence (voltage drop < threshold, power returns within a configurable window, default ~500 ms) toggles the manual power step for the corresponding heater channel. Longer OFF periods do **not** change the step. The window can be adjusted in the web UI (Diagnostics → OFF/ON detection window).
    - The manual power for both channels is cycled once at boot when in manual mode and `INPUT_PIN` is held HIGH (first selection feedback).

- **Web API test pulse**
  - **Pattern**: 1× very short pulse (HIGH ~120 ms)
  - **When**: HTTP POST to `/signalTest` (useful to verify wiring of LED/vibration motor).

**Network:** AP IP `4.3.2.1` · Default SSID `HeatControl` · OTA at `/update`

### Web Interface
<img src="documentation/GUI.png" alt="Web Interface" width="300"/>

The web interface provides:
- Live temperature readings
- Target temperature controls
- Heater status indicators
- Sensor swap option
- MOSFET NTC diagnostics (mV + C) and `HOT/TRIP` warnings per heater card
- WiFi configuration
- Runtime reset
- MOSFET overtemp-history reset (acknowledge latched events)
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
   - If desktop captive portal does not open automatically, open `http://4.3.2.1` directly.
4. Configure targets and monitor status
5. Optional OTA update: open `/update` and upload `firmware.bin`

## Testing & Simulation
- Native logic/unit-tests: `export PATH=$PATH:~/.local/bin && pio test -e native`
- Simulation tips (PlatformIO + Wokwi) are documented in `documentation/simulation.md`

## License
This project is licensed under GPL-3.0. See `LICENSE`.
