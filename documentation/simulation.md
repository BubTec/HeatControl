# Simulation & Tooling

## PlatformIO CLI
1. Install PlatformIO locally (`pip install --user --break-system-packages platformio`).
2. Export the binary path (e.g. `export PATH=$PATH:~/.local/bin`).
3. Run the native unit tests: `pio test -e native`.

## Wokwi (ESP32-C3)
1. Install the CLI: `npm install -g @wokwi/cli`.
2. Create a `wokwi.toml` (sample below) to describe the ESP32-C3 board and serial monitor.
3. Launch the simulator: `wokwi-server --chip esp32c3 --baud 115200 --elf .pio/build/esp32-c3/firmware.elf`.

```toml
[wokwi]
version = 1
firmware = ".pio/build/esp32-c3/firmware.bin"
elf = ".pio/build/esp32-c3/firmware.elf"
[[wokwi.serial]]
console = "uart"
baud = 115200
```

Attach DS18B20 and SSR components in the Wokwi diagram to validate the `/status` responses and manual mode PWM from your desktop.
