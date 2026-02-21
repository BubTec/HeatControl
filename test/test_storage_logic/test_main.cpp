#include <unity.h>

#include "storage_logic.h"

using namespace HeatControl;

void setUp() {}
void tearDown() {}

void test_clamp_target_limits() {
  TEST_ASSERT_EQUAL_FLOAT(10.0F, clampTarget(5.0F));
  TEST_ASSERT_EQUAL_FLOAT(45.0F, clampTarget(100.0F));
  TEST_ASSERT_EQUAL_FLOAT(30.0F, clampTarget(30.0F));
}

void test_manual_power_clamp() {
  TEST_ASSERT_EQUAL_UINT8(25, clampManualPowerPercent(10));
  TEST_ASSERT_EQUAL_UINT8(75, clampManualPowerPercent(75));
}

void test_manual_power_cycle() {
  TEST_ASSERT_EQUAL_UINT8(50, nextManualPowerPercent(25));
  TEST_ASSERT_EQUAL_UINT8(75, nextManualPowerPercent(50));
  TEST_ASSERT_EQUAL_UINT8(100, nextManualPowerPercent(75));
  TEST_ASSERT_EQUAL_UINT8(25, nextManualPowerPercent(100));
}

void test_manual_toggle_window() {
  TEST_ASSERT_EQUAL_UINT16(100U, clampManualToggleOffMs(20U));
  TEST_ASSERT_EQUAL_UINT16(5000U, clampManualToggleOffMs(10000U));
  TEST_ASSERT_EQUAL_UINT16(1200U, clampManualToggleOffMs(1200U));
}

void test_battery_cell_clamp() {
  TEST_ASSERT_EQUAL_UINT8(3, clampBatteryCellCount(0));
  TEST_ASSERT_EQUAL_UINT8(6, clampBatteryCellCount(6));
  TEST_ASSERT_EQUAL_UINT8(3, clampBatteryCellCount(10));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_clamp_target_limits);
  RUN_TEST(test_manual_power_clamp);
  RUN_TEST(test_manual_power_cycle);
  RUN_TEST(test_manual_toggle_window);
  RUN_TEST(test_battery_cell_clamp);
  return UNITY_END();
}
