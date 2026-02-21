#include "battery_toggle.h"

namespace HeatControl {

BatteryToggleDetector::BatteryToggleDetector(uint16_t offThresholdMv, uint16_t onThresholdMv, uint8_t stableSamples)
    : offThresholdMv_(offThresholdMv), onThresholdMv_(onThresholdMv), stableSamples_(stableSamples) {}

BatteryToggleDetector::SampleResult BatteryToggleDetector::update(uint16_t adcMilliVolts) {
  if (adcMilliVolts <= offThresholdMv_) {
    if (offStableCount_ < 255) {
      ++offStableCount_;
    }
    onStableCount_ = 0;
  } else if (adcMilliVolts >= onThresholdMv_) {
    if (onStableCount_ < 255) {
      ++onStableCount_;
    }
    offStableCount_ = 0;
  } else {
    offStableCount_ = 0;
    onStableCount_ = 0;
  }

  const bool offNow = offStableCount_ >= stableSamples_;
  const bool onNow = onStableCount_ >= stableSamples_;

  SampleResult result{offNow, onNow, offNow != lastOffNow_, onNow != lastOnNow_};
  lastOffNow_ = offNow;
  lastOnNow_ = onNow;
  return result;
}

}  // namespace HeatControl

