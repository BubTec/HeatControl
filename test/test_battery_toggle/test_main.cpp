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

void test_off_edge_emits_only_once_per_transition() {
  BatteryToggleDetector detector(80, 300, 2);
  detector.update(70);
  auto second = detector.update(60);
  TEST_ASSERT_TRUE(second.offEdge);
  auto third = detector.update(60);
  TEST_ASSERT_TRUE(third.offNow);
  TEST_ASSERT_FALSE(third.offEdge);
}

void test_mid_band_noise_never_sets_flags() {
  BatteryToggleDetector detector(80, 300, 2);
  auto first = detector.update(200);
  auto second = detector.update(220);
  TEST_ASSERT_FALSE(first.offNow);
  TEST_ASSERT_FALSE(first.onNow);
  TEST_ASSERT_FALSE(second.offNow);
  TEST_ASSERT_FALSE(second.onNow);
  TEST_ASSERT_FALSE(second.offEdge);
  TEST_ASSERT_FALSE(second.onEdge);
}

void test_switching_back_to_on_requires_two_samples() {
  BatteryToggleDetector detector(80, 300, 2);
  detector.update(60);
  detector.update(60);
  auto singleHigh = detector.update(320);
  TEST_ASSERT_FALSE(singleHigh.onNow);
  TEST_ASSERT_FALSE(singleHigh.onEdge);
  auto secondHigh = detector.update(330);
  TEST_ASSERT_TRUE(secondHigh.onNow);
  TEST_ASSERT_TRUE(secondHigh.onEdge);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_requires_stable_samples_before_trigger);
  RUN_TEST(test_mid_band_resets_counters);
  RUN_TEST(test_off_edge_emits_only_once_per_transition);
  RUN_TEST(test_mid_band_noise_never_sets_flags);
  RUN_TEST(test_switching_back_to_on_requires_two_samples);
  return UNITY_END();
}
