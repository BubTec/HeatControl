# HeatControl - Dual Channel Temperature Controller

## Description
HeatControl is an ESP8266-based controller for two independent heating circuits with the following features:
- Two independent temperature control circuits
- PID control for precise temperature regulation
- Web interface for configuration
- Two operating modes: Normal and FULL POWER (100%)
- Automatic captive portal for easy configuration
- Logging system for troubleshooting

## Hardware Components

### Required Parts
- 1x Wemos D1 Mini Pro (ESP8266)
- 2x DS18B20 temperature sensors (waterproof, with cable)
- 2x MOSFET IRLZ44N (or similar logic-level MOSFETs)
- 1x 4.7kΩ pull-up resistor (for OneWire bus)
- 1x 10kΩ pull-up resistor (for mode pin)
- 1x 1000µF electrolytic capacitor (min. 6.3V)
- 5V power supply (minimum 2A)
- Perfboard or PCB
- Terminal blocks for:
  - 2x heaters
  - 2x temperature sensors
  - 1x power supply
- Enclosure (waterproof if used in humid environment)
- Wires in various colors

### Detailed Schematic

#### Power Supply
```
   5V Power Supply
      ┌─────────────┐
      │   5V    GND │
      └─┬─────────┬─┘
        │         │
    ┌───┴─────────┴───┐
    │   1000µF/6.3V   │      ┌─────────────────┐
    └───┬─────────┬───┘      │    Wemos D1     │
        │         │          │                  │
        ├────────►│ GND      │ GND             │
        │         │          │                  │
        └────────►│ 5V       │ 5V              │
                  │          │                  │
                  └──────────┘
```

#### Temperature Sensors (OneWire Bus)
```
                     Wemos D1
                  ┌──────────┐
                  │          │
        ┌────────►│ D2      │
        │         │          │
        │   4.7kΩ │          │
        ├────┳────┘          │
        │    │               │
    ┌───┴────┴───┐          │
    │ DS18B20 #1 │          │
    │ VCC DATA GND│          │
    └──┬────┬──┬─┘          │
       │    │  │            │
    ┌──┴────┴──┴─┐          │
    │ DS18B20 #2 │          │
    │ VCC DATA GND│          │
    └──┬────────┬─┘         │
       │        │           │
       └────────┴───────────┘
```

#### MOSFET Control
```
                     Wemos D1
                  ┌──────────┐
                  │          │           IRLZ44N
        ┌────────►│ D4      │              ┌──────┐
        │         │          │      Gate ──►│G     │
        │         │          │              │      │ Drain
        │         │          │              │  D   ├───► Heizung 1 (-)
        │         │          │              │      │
        │         │          │              │  S   │
        │         │          │      Source  └──┬───┘
        │         │          │                 │
        │         │          │                 │
        │         │          │                 │
        └─────────┴──────────┴─────────────────┘
                    GND

        (Same setup for Heizung 2 with D5)
```

#### Mode Switching Circuit
```
                     Wemos D1
                  ┌──────────┐
     VCC (5V)     │          │
        ┌─────────┤ 5V       │
        │         │          │
        │   10kΩ  │          │
        ├──┳──────┤ D5       │
        │  │      │          │
   1000µF│  │     │          │
        │  │      │          │
        │  │      │          │
        └──┴──────┤ GND      │
                  └──────────┘
```

### Pin Assignment Wemos D1 Mini Pro
- D2 (GPIO4): OneWire bus for temperature sensors
- D4 (GPIO2): MOSFET 1 gate
- D5 (GPIO1): MOSFET 2 gate
- D5 (GPIO14): Mode pin for operation mode switching
- 5V: Power supply
- GND: Ground

### Complete Assembly

#### 1. Power Supply
- Connect 5V and GND from the power supply to Wemos D1
- 1000µF capacitor connected in parallel to the supply
- Adequate wiring (min. 0.75mm² for heaters)
- Power supply must be able to provide total current of heaters plus 500mA reserve

#### 2. Temperature Sensors
- Common OneWire bus at D2
- 4.7kΩ pull-up between VCC and DATA
- Sensors connected in parallel
- Shielded cables used for longer runs
- Waterproof design if needed
- Maximum cable length: 20m

#### 3. MOSFET Drivers
- Gate connections at D4 (Heizung 1) and D5 (Heizung 2)
- Source connected to GND
- Drain connected to respective heater (-)
- Heat sinks used for currents > 3A
- No freewheeling diodes needed (integrated in MOSFET)
- Good thermal connection is important

#### 4. Mode Switching Circuit
- 10kΩ pull-up between VCC and D5
- 1000µF between VCC and GND
- Placed as close as possible to the ESP
- Used for switching between Normal and FULL POWER modes

### Mounting Instructions

#### 1. General Mounting
- Components mounted on perfboard or PCB
- Adequate spacing between power components
- Wires cross-section sized according to current
- Cable strain relief provided for all wires
- Enclosure with adequate ventilation chosen

#### 2. Temperature Sensors
- Ensure good thermal contact
- Waterproof if needed
- Cables can be extended (max. 20m)
- If there are issues, shielded cables should be used

#### 3. MOSFETs
- Heat sinks used for currents > 3A
- Thermal paste used
- Adequate spacing between MOSFETs
- Good ventilation ensured

#### 4. Enclosure
- IP protection class chosen according to installation location
- Adequate ventilation for MOSFETs
- Cable passages sealed
- Cable strain relief provided for all cables

### Operating Modes

#### 1. Normal Mode
- PID-controlled temperature regulation
- Separate setpoints for both heating circuits
- Adjustable PID parameters
- Temperature range: 10-40°C

#### 2. FULL POWER Mode
- Both heating circuits at 100% power
- No temperature regulation
- Useful for rapid heating
- Automatic return to normal mode after extended power loss

#### Mode Switching
- Brief power cycle (< 2s): Switch between modes
- Longer power off (> 10s): Start in normal mode
- Status shown in web interface

### Safety Instructions

#### 1. Electrical Safety
- Use short-circuit protected power supply
- Implement overload protection
- Clean cable routing
- Ground enclosure if metallic

#### 2. Temperature Safety
- Regular sensor function checks
- Redundancy through two sensors
- Fail-safe on sensor failure
- Temperature monitoring implemented

#### 3. Fire Safety
- Observe fire protection regulations
- Ensure adequate ventilation
- Keep flammable materials away
- Regular maintenance

### Initial Setup

1. Build and check hardware
2. Flash ESP with code
3. Connect power supply
4. Connect to WiFi "HeatControl" (password: HeatControl)
5. Open 192.168.4.1 in browser
6. Set temperatures and PID parameters
7. Test functionality

### Maintenance

1. Regular Checks
   - Test temperature sensors
   - Check cable connections
   - Clean heat sinks
   - Check enclosure ventilation

2. Software Updates
   - Check current version
   - Backup settings
   - Perform update
   - Restore settings

3. Troubleshooting
   - Check ESP LED status
   - Serial Monitor (115200 baud) for debug output
   - Verify temperature values
   - Check PWM outputs with multimeter/oscilloscope
   - Analyze log files

### Technical Specifications

- Operating voltage: 5V DC
- Current consumption: max. 500mA (without heaters)
- Temperature range: 10-40°C
- Control accuracy: ±0.5°C
- WiFi: 2.4GHz
- Protection class: depends on enclosure
- Dimensions: depends on assembly