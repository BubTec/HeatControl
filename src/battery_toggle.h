#pragma once

#include <cstdint>

namespace HeatControl {

class BatteryToggleDetector {
 public:
  struct SampleResult {
    bool offNow;
    bool onNow;
    bool offEdge;
    bool onEdge;
  };

  BatteryToggleDetector(uint16_t offThresholdMv, uint16_t onThresholdMv, uint8_t stableSamples);

  SampleResult update(uint16_t adcMilliVolts);

 private:
  uint16_t offThresholdMv_;
  uint16_t onThresholdMv_;
  uint8_t stableSamples_;
  uint8_t offStableCount_ = 0;
  uint8_t onStableCount_ = 0;
  bool lastOffNow_ = false;
  bool lastOnNow_ = false;
};

}  // namespace HeatControl

