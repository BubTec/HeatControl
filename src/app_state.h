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
constexpr int EEPROM_MOSFET1_OVERTEMP_FLAG_ADDR = 320;
constexpr int EEPROM_MOSFET2_OVERTEMP_FLAG_ADDR = 321;
constexpr int EEPROM_MOSFET1_OVERTEMP_TEMP_ADDR = 324;
constexpr int EEPROM_MOSFET2_OVERTEMP_TEMP_ADDR = 328;
constexpr int EEPROM_TEMP1_ADDR = 64;
constexpr int EEPROM_TEMP2_ADDR = 68;
constexpr int EEPROM_SWAP_ADDR = 72;
constexpr int EEPROM_RUNTIME_ADDR = 200;

constexpr uint8_t BOOT_MODE_NORMAL = 0x01;
constexpr uint8_t BOOT_MODE_POWER = 0x02;

// GPIO2/GPIO8/GPIO9 are ESP32-C3 strapping pins.
// Never place fixed voltage-divider signals (NTC/battery sensing) on those pins.
constexpr int SSR_PIN_1 = 2;
constexpr int SSR_PIN_2 = 5;
constexpr int INPUT_PIN = 10;
constexpr int SIGNAL_PIN = 6;
constexpr int ADC_PIN_1 = 0;
constexpr int ADC_PIN_2 = 1;
constexpr int ADC_PIN_NTC_MOSFET_1 = 3;
constexpr int ADC_PIN_NTC_MOSFET_2 = 4;
constexpr int ONE_WIRE_BUS = 7;
constexpr int AP_MAX_CLIENTS = 4;

// Keep UART0 free on final PCB for external flashing/debug adapter.
constexpr int UART0_RX_RESERVED_PIN = 20;
constexpr int UART0_TX_RESERVED_PIN = 21;
// Keep native USB pins free for potential future USB use.
constexpr int USB_DM_RESERVED_PIN = 18;
constexpr int USB_DP_RESERVED_PIN = 19;

constexpr bool isForbiddenDividerStrappingPin(const int pin) {
  return pin == 2 || pin == 8 || pin == 9;
}

constexpr bool isReservedFuturePin(const int pin) {
  return pin == UART0_RX_RESERVED_PIN || pin == UART0_TX_RESERVED_PIN || pin == USB_DM_RESERVED_PIN ||
         pin == USB_DP_RESERVED_PIN;
}

static_assert(!isForbiddenDividerStrappingPin(ADC_PIN_1),
              "ADC_PIN_1 must not use GPIO2/GPIO8/GPIO9 (ESP32-C3 strapping pins).");
static_assert(!isForbiddenDividerStrappingPin(ADC_PIN_2),
              "ADC_PIN_2 must not use GPIO2/GPIO8/GPIO9 (ESP32-C3 strapping pins).");
static_assert(!isForbiddenDividerStrappingPin(ADC_PIN_NTC_MOSFET_1),
              "ADC_PIN_NTC_MOSFET_1 must not use GPIO2/GPIO8/GPIO9 (ESP32-C3 strapping pins).");
static_assert(!isForbiddenDividerStrappingPin(ADC_PIN_NTC_MOSFET_2),
              "ADC_PIN_NTC_MOSFET_2 must not use GPIO2/GPIO8/GPIO9 (ESP32-C3 strapping pins).");

static_assert(!isReservedFuturePin(SSR_PIN_1) && !isReservedFuturePin(SSR_PIN_2) && !isReservedFuturePin(INPUT_PIN) &&
                  !isReservedFuturePin(SIGNAL_PIN) && !isReservedFuturePin(ADC_PIN_1) &&
                  !isReservedFuturePin(ADC_PIN_2) && !isReservedFuturePin(ADC_PIN_NTC_MOSFET_1) &&
                  !isReservedFuturePin(ADC_PIN_NTC_MOSFET_2) && !isReservedFuturePin(ONE_WIRE_BUS),
              "GPIO18/GPIO19 (USB) and GPIO20/GPIO21 (UART0) are reserved and must stay free.");

constexpr float DEFAULT_TARGET_TEMP = 23.0F;
constexpr float BATTERY_DIVIDER_RATIO = 4.0F;  // Adjust to your resistor divider (V_batt = V_adc * ratio).
constexpr float MOSFET_OVERTEMP_LIMIT_C = 80.0F;
constexpr float MOSFET_OVERTEMP_RESET_C = 75.0F;  // Hysteresis for re-enable after cooldown.

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
extern uint16_t ntcMosfet1MilliVolts;
extern uint16_t ntcMosfet2MilliVolts;
extern float ntcMosfet1TempC;
extern float ntcMosfet2TempC;
extern bool mosfet1OvertempActive;
extern bool mosfet2OvertempActive;
extern bool mosfet1OvertempLatched;
extern bool mosfet2OvertempLatched;
extern float mosfet1OvertempTripTempC;
extern float mosfet2OvertempTripTempC;

extern uint16_t manualPowerToggleMaxOffMs;

extern OneWire oneWire;
extern DallasTemperature sensors;
extern String activeSsid;
extern String activePassword;
extern char serialLogBuffer[12001];
extern size_t serialLogLength;
extern AsyncWebServer server;
extern DNSServer dnsServer;

extern bool restartScheduled;
extern unsigned long restartAtMs;

void logLine(const String &line);
void logf(const char *fmt, ...);

}  // namespace HeatControl
