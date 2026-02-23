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

void test_manual_power_clamp_accepts_all_valid_values() {
  TEST_ASSERT_EQUAL_UINT8(25, clampManualPowerPercent(25));
  TEST_ASSERT_EQUAL_UINT8(50, clampManualPowerPercent(50));
  TEST_ASSERT_EQUAL_UINT8(75, clampManualPowerPercent(75));
  TEST_ASSERT_EQUAL_UINT8(100, clampManualPowerPercent(100));
}

void test_manual_power_cycle() {
  TEST_ASSERT_EQUAL_UINT8(50, nextManualPowerPercent(25));
  TEST_ASSERT_EQUAL_UINT8(75, nextManualPowerPercent(50));
  TEST_ASSERT_EQUAL_UINT8(100, nextManualPowerPercent(75));
  TEST_ASSERT_EQUAL_UINT8(25, nextManualPowerPercent(100));
}

void test_manual_power_cycle_handles_invalid_input() {
  TEST_ASSERT_EQUAL_UINT8(50, nextManualPowerPercent(0));
}

void test_manual_toggle_window() {
  TEST_ASSERT_EQUAL_UINT16(100U, clampManualToggleOffMs(20U));
  TEST_ASSERT_EQUAL_UINT16(5000U, clampManualToggleOffMs(10000U));
  TEST_ASSERT_EQUAL_UINT16(1200U, clampManualToggleOffMs(1200U));
}

void test_ap_auto_off_minutes_clamp() {
  TEST_ASSERT_EQUAL_UINT16(0U, clampApAutoOffMinutes(0U));
  TEST_ASSERT_EQUAL_UINT16(10U, clampApAutoOffMinutes(10U));
  TEST_ASSERT_EQUAL_UINT16(240U, clampApAutoOffMinutes(240U));
  TEST_ASSERT_EQUAL_UINT16(240U, clampApAutoOffMinutes(999U));
}

void test_battery_cell_clamp() {
  TEST_ASSERT_EQUAL_UINT8(3, clampBatteryCellCount(0));
  TEST_ASSERT_EQUAL_UINT8(2, clampBatteryCellCount(2));
  TEST_ASSERT_EQUAL_UINT8(6, clampBatteryCellCount(6));
  TEST_ASSERT_EQUAL_UINT8(3, clampBatteryCellCount(10));
}

void test_battery_chemistry_clamp() {
  TEST_ASSERT_EQUAL_UINT8(BATTERY_CHEMISTRY_LI_ION, clampBatteryChemistry(0));
  TEST_ASSERT_EQUAL_UINT8(BATTERY_CHEMISTRY_LI_PO, clampBatteryChemistry(1));
  TEST_ASSERT_EQUAL_UINT8(BATTERY_CHEMISTRY_LI_FE_PO4, clampBatteryChemistry(2));
  TEST_ASSERT_EQUAL_UINT8(BATTERY_CHEMISTRY_NI_MH, clampBatteryChemistry(3));
  TEST_ASSERT_EQUAL_UINT8(BATTERY_CHEMISTRY_LEAD_GEL, clampBatteryChemistry(4));
  TEST_ASSERT_EQUAL_UINT8(BATTERY_CHEMISTRY_LI_ION, clampBatteryChemistry(9));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_clamp_target_limits);
  RUN_TEST(test_manual_power_clamp);
  RUN_TEST(test_manual_power_clamp_accepts_all_valid_values);
  RUN_TEST(test_manual_power_cycle);
  RUN_TEST(test_manual_power_cycle_handles_invalid_input);
  RUN_TEST(test_manual_toggle_window);
  RUN_TEST(test_ap_auto_off_minutes_clamp);
  RUN_TEST(test_battery_cell_clamp);
  RUN_TEST(test_battery_chemistry_clamp);
  return UNITY_END();
}
