#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>

namespace HeatControl {

constexpr int EEPROM_SIZE = 512;
constexpr int EEPROM_INIT_ADDR = 0;
constexpr int EEPROM_SSID_ADDR = 1;
constexpr int EEPROM_PASS_ADDR = 33;
constexpr int EEPROM_BOOT_MODE_ADDR = 300;
constexpr int EEPROM_MANUAL_POWER1_ADDR = 304;
constexpr int EEPROM_MANUAL_POWER2_ADDR = 305;
constexpr int EEPROM_BATTERY1_CELLS_ADDR = 308;
constexpr int EEPROM_BATTERY2_CELLS_ADDR = 309;
constexpr int EEPROM_MANUAL_TOGGLE_MS_ADDR = 310;
constexpr int EEPROM_LAST_BATTERY_MASK_ADDR = 312;
constexpr int EEPROM_TEMP1_ADDR = 64;
constexpr int EEPROM_TEMP2_ADDR = 68;
constexpr int EEPROM_SWAP_ADDR = 72;
constexpr int EEPROM_RUNTIME_ADDR = 200;

constexpr uint8_t BOOT_MODE_NORMAL = 0x01;
constexpr uint8_t BOOT_MODE_POWER = 0x02;

constexpr int SSR_PIN_1 = 4;
constexpr int SSR_PIN_2 = 5;
constexpr int INPUT_PIN = 10;
constexpr int SIGNAL_PIN = 6;
constexpr int ADC_PIN_1 = 0;
constexpr int ADC_PIN_2 = 1;
constexpr int ONE_WIRE_BUS = 3;
constexpr int AP_MAX_CLIENTS = 4;

constexpr float DEFAULT_TARGET_TEMP = 23.0F;
constexpr float BATTERY_DIVIDER_RATIO = 4.0F;  // Adjust to your resistor divider (V_batt = V_adc * ratio).

extern unsigned long lastPrintMs;
extern unsigned long lastSensorMs;
extern unsigned long lastRuntimeSaveMs;
extern unsigned long lastMemCheckMs;
extern unsigned long startTimeMs;

extern uint32_t counter;
extern uint32_t savedRuntimeMinutes;

extern bool lastHeater1State;
extern bool lastHeater2State;
extern bool lastSignalPinState;
extern bool lastInputPinState;

extern bool manualHeater1Enabled;
extern bool manualHeater2Enabled;
extern bool battery1Off;
extern bool battery2Off;
extern unsigned long battery1OffSinceMs;
extern unsigned long battery2OffSinceMs;

extern bool powerMode;
extern bool manualMode;
extern bool swapAssignment;
extern bool fileSystemReady;

extern float currentTemp1;
extern float currentTemp2;
extern float targetTemp1;
extern float targetTemp2;
extern uint8_t manualPowerPercent1;
extern uint8_t manualPowerPercent2;
extern uint16_t adc1MilliVolts;
extern uint16_t adc2MilliVolts;
extern uint8_t battery1CellCount;
extern float battery1PackVoltage;
extern float battery1CellVoltage;
extern uint8_t battery1SocPercent;
extern uint8_t battery2CellCount;
extern float battery2PackVoltage;
extern float battery2CellVoltage;
extern uint8_t battery2SocPercent;

extern uint16_t manualPowerToggleMaxOffMs;

extern OneWire oneWire;
extern DallasTemperature sensors;
extern String activeSsid;
extern String activePassword;
extern char serialLogBuffer[12001];
extern size_t serialLogLength;
extern AsyncWebServer server;
extern DNSServer dnsServer;

}  // namespace HeatControl
