#include <unity.h>

#include "battery_toggle.h"

using HeatControl::BatteryToggleDetector;

void setUp() {}
void tearDown() {}

void test_requires_stable_samples_before_trigger() {
  BatteryToggleDetector detector(80, 300, 2);
  auto first = detector.update(70);
  TEST_ASSERT_FALSE(first.offNow);
  auto second = detector.update(60);
  TEST_ASSERT_TRUE(second.offNow);
  TEST_ASSERT_TRUE(second.offEdge);

  auto recover = detector.update(350);
  TEST_ASSERT_FALSE(recover.offNow);
  TEST_ASSERT_FALSE(recover.onNow);
  auto stableOn = detector.update(360);
  TEST_ASSERT_TRUE(stableOn.onNow);
  TEST_ASSERT_TRUE(stableOn.onEdge);
}

void test_mid_band_resets_counters() {
  BatteryToggleDetector detector(80, 300, 2);
  detector.update(70);
  auto mid = detector.update(150);
  TEST_ASSERT_FALSE(mid.offNow);
  TEST_ASSERT_FALSE(mid.onNow);
  auto final = detector.update(60);
  TEST_ASSERT_FALSE(final.offNow);
  final = detector.update(50);
  TEST_ASSERT_TRUE(final.offNow);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_requires_stable_samples_before_trigger);
  RUN_TEST(test_mid_band_resets_counters);
  return UNITY_END();
}
