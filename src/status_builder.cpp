#include "status_builder.h"

#include <cmath>
#include <cstdio>

#include "logic_helpers.h"

namespace HeatControl {
namespace {

std::string formatBool(bool value) {
  return value ? "1" : "0";
}

std::string formatFloat(float value, int decimals) {
  if (std::isnan(value)) {
    return "null";
  }
  char buffer[32];
  const float scale = std::pow(10.0F, static_cast<float>(decimals));
  const float rounded = std::round(value * scale) / scale;
  snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(rounded));
  return std::string(buffer);
}

std::string formatOptionalFloat(bool valid, float value, int decimals) {
  if (!valid) {
    return "null";
  }
  return formatFloat(value, decimals);
}

}  // namespace

std::string buildStatusJson(const StatusMetrics &m) {
  std::string json = "{";
  json += "\"mode\":\"" + logic_helpers::jsonEscape(m.modeText) + "\"";
  json += ",\"manualMode\":" + formatBool(m.manualMode);
  json += ",\"manualPercent1\":" + std::to_string(m.manualPercent1);
  json += ",\"manualPercent2\":" + std::to_string(m.manualPercent2);
  json += ",\"manualH1Enabled\":" + formatBool(m.manualHeater1Enabled);
  json += ",\"manualH2Enabled\":" + formatBool(m.manualHeater2Enabled);
  json += ",\"bootPin\":\"" + logic_helpers::jsonEscape(m.bootPinText) + "\"";
  json += ",\"adc1Mv\":" + std::to_string(m.adc1MilliVolts);
  json += ",\"adc2Mv\":" + std::to_string(m.adc2MilliVolts);
  json += ",\"ntcMosfet1Mv\":" + std::to_string(m.ntcMosfet1MilliVolts);
  json += ",\"ntcMosfet2Mv\":" + std::to_string(m.ntcMosfet2MilliVolts);
  json += ",\"ntcMosfet1C\":" + formatOptionalFloat(m.ntcMosfet1Valid, m.ntcMosfet1TempC, 2);
  json += ",\"ntcMosfet2C\":" + formatOptionalFloat(m.ntcMosfet2Valid, m.ntcMosfet2TempC, 2);
  json += ",\"mosfet1OvertempActive\":" + formatBool(m.mosfet1OvertempActive);
  json += ",\"mosfet2OvertempActive\":" + formatBool(m.mosfet2OvertempActive);
  json += ",\"mosfet1OvertempLatched\":" + formatBool(m.mosfet1OvertempLatched);
  json += ",\"mosfet2OvertempLatched\":" + formatBool(m.mosfet2OvertempLatched);
  json += ",\"mosfet1OvertempTripC\":" + formatOptionalFloat(m.mosfet1TripValid, m.mosfet1TripTempC, 2);
  json += ",\"mosfet2OvertempTripC\":" + formatOptionalFloat(m.mosfet2TripValid, m.mosfet2TripTempC, 2);
  json += ",\"mosfetOvertempLimitC\":" + formatFloat(m.mosfetOvertempLimitC, 1);
  json += ",\"batt1Cells\":" + std::to_string(m.battery1CellCount);
  json += ",\"batt1V\":" + formatFloat(m.battery1PackVoltage, 2);
  json += ",\"batt1CellV\":" + formatFloat(m.battery1CellVoltage, 2);
  json += ",\"batt1Soc\":" + std::to_string(m.battery1SocPercent);
  json += ",\"batt2Cells\":" + std::to_string(m.battery2CellCount);
  json += ",\"batt2V\":" + formatFloat(m.battery2PackVoltage, 2);
  json += ",\"batt2CellV\":" + formatFloat(m.battery2CellVoltage, 2);
  json += ",\"batt2Soc\":" + std::to_string(m.battery2SocPercent);
  json += ",\"manualToggleMaxOffMs\":" + std::to_string(m.manualToggleMaxOffMs);
  json += ",\"current1\":" + formatFloat(m.displayTemp1, 2);
  json += ",\"current2\":" + formatFloat(m.displayTemp2, 2);
  json += ",\"target1\":" + formatFloat(m.targetTemp1, 1);
  json += ",\"target2\":" + formatFloat(m.targetTemp2, 1);
  json += ",\"swap\":" + formatBool(m.swapAssignment);
  json += ",\"ssid\":\"" + logic_helpers::jsonEscape(m.ssid) + "\"";
  json += ",\"h1\":" + formatBool(m.heater1On);
  json += ",\"h2\":" + formatBool(m.heater2On);
  json += ",\"totalRuntime\":\"" + logic_helpers::jsonEscape(m.totalRuntime) + "\"";
  json += ",\"currentRuntime\":\"" + logic_helpers::jsonEscape(m.currentRuntime) + "\"";
  json += "}";
  return json;
}

}  // namespace HeatControl
