#pragma once

#include <Arduino.h>
#include <cstdint>

namespace HeatControl {

class LedPattern {
 public:
  explicit LedPattern(int pin);

  void begin();
  void update(unsigned long nowMs);

  void setBaseOn(bool on);
  void setTripLatched(bool latched);

  void triggerManualPowerStepFromPercent(uint8_t manualPowerPercent);

 private:
  void setOutput(bool on);

  void updateTrip(unsigned long nowMs);
  void updateStepBlink(unsigned long nowMs);
  void updateBase();

  int pin_;
  bool baseOn_ = false;
  bool tripLatched_ = false;

  bool outputOn_ = false;

  uint8_t stepBlinksRemaining_ = 0;
  bool stepPhaseOn_ = false;
  unsigned long stepPhaseUntilMs_ = 0;

  uint8_t sosPulseIndex_ = 0;
  bool sosPhaseOn_ = false;
  unsigned long sosPhaseUntilMs_ = 0;
};

}  // namespace HeatControl

