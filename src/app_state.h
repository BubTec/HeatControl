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
constexpr int EEPROM_MANUAL_POWER_ADDR = 304;
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
constexpr int ONE_WIRE_BUS = 3;
constexpr int AP_MAX_CLIENTS = 4;

constexpr float DEFAULT_TARGET_TEMP = 23.0F;

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

extern bool powerMode;
extern bool manualMode;
extern bool swapAssignment;
extern bool fileSystemReady;

extern float currentTemp1;
extern float currentTemp2;
extern float targetTemp1;
extern float targetTemp2;
extern uint8_t manualPowerPercent;

extern OneWire oneWire;
extern DallasTemperature sensors;
extern String activeSsid;
extern String activePassword;
extern String serialLogBuffer;
extern AsyncWebServer server;
extern DNSServer dnsServer;

}  // namespace HeatControl
