#pragma once

#include <cstdint>
#include <string>

namespace HeatControl {

struct StatusMetrics {
  std::string modeText;
  bool manualMode = false;
  uint8_t manualPercent1 = 0;
  uint8_t manualPercent2 = 0;
  bool manualHeater1Enabled = false;
  bool manualHeater2Enabled = false;
  std::string bootPinText;
  uint16_t adc1MilliVolts = 0;
  uint16_t adc2MilliVolts = 0;
  uint16_t ntcMosfet1MilliVolts = 0;
  uint16_t ntcMosfet2MilliVolts = 0;
  bool ntcMosfet1Valid = false;
  float ntcMosfet1TempC = 0.0F;
  bool ntcMosfet2Valid = false;
  float ntcMosfet2TempC = 0.0F;
  bool mosfet1OvertempActive = false;
  bool mosfet2OvertempActive = false;
  bool mosfet1OvertempLatched = false;
  bool mosfet2OvertempLatched = false;
  bool mosfet1TripValid = false;
  float mosfet1TripTempC = 0.0F;
  bool mosfet2TripValid = false;
  float mosfet2TripTempC = 0.0F;
  float mosfetOvertempLimitC = 0.0F;
  uint8_t battery1CellCount = 0;
  float battery1PackVoltage = 0.0F;
  float battery1CellVoltage = 0.0F;
  uint8_t battery1SocPercent = 0;
  uint8_t battery2CellCount = 0;
  float battery2PackVoltage = 0.0F;
  float battery2CellVoltage = 0.0F;
  uint8_t battery2SocPercent = 0;
  uint16_t manualToggleMaxOffMs = 0;
  float displayTemp1 = 0.0F;
  float displayTemp2 = 0.0F;
  float targetTemp1 = 0.0F;
  float targetTemp2 = 0.0F;
  bool swapAssignment = false;
  std::string ssid;
  bool heater1On = false;
  bool heater2On = false;
  std::string totalRuntime;
  std::string currentRuntime;
};

std::string buildStatusJson(const StatusMetrics &metrics);

}  // namespace HeatControl

