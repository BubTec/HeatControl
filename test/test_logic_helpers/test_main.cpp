#include <string>

#include <unity.h>

#include "logic_helpers.h"

using namespace HeatControl::logic_helpers;

void setUp() {}
void tearDown() {}

void test_voltage_to_soc_bounds() {
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 100.0F, voltageToSocFloat(4.50F));
  TEST_ASSERT_TRUE(voltageToSocFloat(2.70F) < 0.0F);
}

void test_voltage_to_soc_interpolation() {
  const float soc = voltageToSocFloat(3.90F);
  TEST_ASSERT_FLOAT_WITHIN(0.5F, 73.0F, soc);
}

void test_clamp_soc_percent() {
  TEST_ASSERT_EQUAL_UINT8(0, clampSocPercent(-1.0F));
  TEST_ASSERT_EQUAL_UINT8(100, clampSocPercent(150.0F));
  TEST_ASSERT_EQUAL_UINT8(42, clampSocPercent(41.6F));
}

void test_update_battery_from_adc_defaults() {
  float packV = 0.0F;
  float cellV = 0.0F;
  uint8_t socPercent = 0;
  const float soc = updateBatteryFromAdc(2500U, 0, 4.0F, packV, cellV, socPercent);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 10.0F, packV);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 3.3333F, cellV);
  TEST_ASSERT_EQUAL_UINT8(14, socPercent);
  TEST_ASSERT_FLOAT_WITHIN(0.1F, 14.22F, soc);
}

void test_update_battery_from_adc_custom_cells() {
  float packV = 0.0F;
  float cellV = 0.0F;
  uint8_t socPercent = 0;
  const float soc = updateBatteryFromAdc(3000U, 4, 4.0F, packV, cellV, socPercent);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 12.0F, packV);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 3.0F, cellV);
  TEST_ASSERT_EQUAL_UINT8(2, socPercent);
  TEST_ASSERT_FLOAT_WITHIN(0.1F, 2.0F, soc);
}

void test_ntc_rejects_invalid_ranges() {
  float temp = 0.0F;
  TEST_ASSERT_FALSE(ntcMilliVoltsToTempC(0U, 3300.0F, 10000.0F, 10000.0F, 3950.0F, 25.0F, temp));
  TEST_ASSERT_FALSE(ntcMilliVoltsToTempC(3300U, 3300.0F, 10000.0F, 10000.0F, 3950.0F, 25.0F, temp));
}

void test_ntc_valid_values() {
  float temp = 0.0F;
  TEST_ASSERT_TRUE(ntcMilliVoltsToTempC(1650U, 3300.0F, 10000.0F, 10000.0F, 3950.0F, 25.0F, temp));
  TEST_ASSERT_FLOAT_WITHIN(0.05F, 25.0F, temp);

  TEST_ASSERT_TRUE(ntcMilliVoltsToTempC(400U, 3300.0F, 10000.0F, 10000.0F, 3950.0F, 25.0F, temp));
  TEST_ASSERT_FLOAT_WITHIN(0.5F, -13.78F, temp);
}

void test_log_buffer_within_bounds() {
  char buffer[32] = {0};
  size_t len = 0;
  appendLineToRollingBuffer(buffer, sizeof(buffer), len, "Hello", 5);
  appendLineToRollingBuffer(buffer, sizeof(buffer), len, "\n", 1);
  TEST_ASSERT_EQUAL_STRING("Hello\n", buffer);
}

void test_log_buffer_overflow_discards_oldest() {
  char buffer[16] = {0};
  size_t len = 0;
  appendLineToRollingBuffer(buffer, sizeof(buffer), len, "1234567890", 10);
  appendLineToRollingBuffer(buffer, sizeof(buffer), len, "ABCDEFGHIJ", 10);
  TEST_ASSERT_EQUAL_UINT32(15U, static_cast<uint32_t>(len));
  TEST_ASSERT_EQUAL_STRING("67890ABCDEFGHIJ", buffer);
}

void test_json_escape() {
  const std::string raw = "Line\"1\"\nLine\\2\rX";
  const std::string escaped = jsonEscape(raw);
  TEST_ASSERT_EQUAL_STRING("Line\\\"1\\\"\\nLine\\\\2\\rX", escaped.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_voltage_to_soc_bounds);
  RUN_TEST(test_voltage_to_soc_interpolation);
  RUN_TEST(test_clamp_soc_percent);
  RUN_TEST(test_update_battery_from_adc_defaults);
  RUN_TEST(test_update_battery_from_adc_custom_cells);
  RUN_TEST(test_ntc_rejects_invalid_ranges);
  RUN_TEST(test_ntc_valid_values);
  RUN_TEST(test_log_buffer_within_bounds);
  RUN_TEST(test_log_buffer_overflow_discards_oldest);
  RUN_TEST(test_json_escape);
  return UNITY_END();
}
