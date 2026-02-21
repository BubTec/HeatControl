#include <string>

#include <unity.h>

#include "status_builder.h"

using namespace HeatControl;

void setUp() {}
void tearDown() {}

void test_status_json_basic_fields() {
  StatusMetrics metrics;
  metrics.modeText = "MANUAL";
  metrics.manualMode = true;
  metrics.manualPercent1 = 25;
  metrics.manualPercent2 = 50;
  metrics.manualHeater1Enabled = true;
  metrics.manualHeater2Enabled = false;
  metrics.bootPinText = "HIGH";
  metrics.adc1MilliVolts = 1234;
  metrics.adc2MilliVolts = 4321;
  metrics.ntcMosfet1MilliVolts = 2222;
  metrics.ntcMosfet2MilliVolts = 3333;
  metrics.ntcMosfet1Valid = true;
  metrics.ntcMosfet1TempC = 42.5F;
  metrics.ntcMosfet2Valid = false;
  metrics.mosfet1OvertempActive = true;
  metrics.mosfet2OvertempActive = false;
  metrics.mosfet1OvertempLatched = true;
  metrics.mosfet2OvertempLatched = false;
  metrics.mosfet1TripValid = true;
  metrics.mosfet1TripTempC = 81.23F;
  metrics.mosfet2TripValid = false;
  metrics.mosfetOvertempLimitC = 80.0F;
  metrics.battery1CellCount = 3;
  metrics.battery1PackVoltage = 11.5F;
  metrics.battery1CellVoltage = 3.8F;
  metrics.battery1SocPercent = 90;
  metrics.battery2CellCount = 4;
  metrics.battery2PackVoltage = 14.8F;
  metrics.battery2CellVoltage = 3.7F;
  metrics.battery2SocPercent = 75;
  metrics.manualToggleMaxOffMs = 1200;
  metrics.displayTemp1 = 23.45F;
  metrics.displayTemp2 = 21.1F;
  metrics.targetTemp1 = 24.0F;
  metrics.targetTemp2 = 22.5F;
  metrics.swapAssignment = true;
  metrics.ssid = "HeatControl";
  metrics.heater1On = true;
  metrics.heater2On = false;
  metrics.totalRuntime = "1h 2m";
  metrics.currentRuntime = "10m 2s";

  const std::string json = buildStatusJson(metrics);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"mode\":\"MANUAL\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"manualMode\":1"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"ntcMosfet2C\":null"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"mosfet2OvertempTripC\":null"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"h2\":0"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"totalRuntime\":\"1h 2m\""));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_json_basic_fields);
  return UNITY_END();
}
