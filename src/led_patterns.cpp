#include "led_patterns.h"

#include "app_state.h"

namespace HeatControl {

namespace {

constexpr unsigned long STEP_ON_MS = 130;
constexpr unsigned long STEP_OFF_MS = 130;
constexpr unsigned long STEP_DONE_PAUSE_MS = 600;

constexpr unsigned long SOS_SHORT_ON_MS = 150;
constexpr unsigned long SOS_LONG_ON_MS = 450;
constexpr unsigned long SOS_OFF_MS = 150;
constexpr unsigned long SOS_DONE_PAUSE_MS = 800;

constexpr bool sosIsLongPulse(uint8_t index) {
  return index >= 3 && index <= 5;
}

}  // namespace

LedPattern::LedPattern(int pin) : pin_(pin) {}

void LedPattern::begin() {
  pinMode(pin_, OUTPUT);
  setOutput(false);
}

void LedPattern::setBaseOn(bool on) {
  baseOn_ = on;
}

void LedPattern::setTripLatched(bool latched) {
  if (tripLatched_ == latched) {
    return;
  }
  tripLatched_ = latched;

  sosPulseIndex_ = 0;
  sosPhaseOn_ = false;
  sosPhaseUntilMs_ = 0;
}

void LedPattern::triggerManualPowerStepFromPercent(uint8_t manualPowerPercent) {
  uint8_t pulses = 1;
  if (manualPowerPercent >= 100) {
    pulses = 4;
  } else if (manualPowerPercent >= 75) {
    pulses = 3;
  } else if (manualPowerPercent >= 50) {
    pulses = 2;
  }

  stepBlinksRemaining_ = pulses;
  stepPhaseOn_ = false;
  stepPhaseUntilMs_ = 0;
}

void LedPattern::update(unsigned long nowMs) {
  if (tripLatched_) {
    updateTrip(nowMs);
    return;
  }
  if (stepBlinksRemaining_ > 0) {
    updateStepBlink(nowMs);
    return;
  }

  updateBase();
}

void LedPattern::setOutput(bool on) {
  if (outputOn_ == on) {
    return;
  }
  outputOn_ = on;
  digitalWrite(pin_, on ? HIGH : LOW);
}

void LedPattern::updateBase() {
  setOutput(baseOn_);
}

void LedPattern::updateStepBlink(unsigned long nowMs) {
  if (stepPhaseUntilMs_ != 0 && nowMs < stepPhaseUntilMs_) {
    return;
  }

  if (!stepPhaseOn_) {
    stepPhaseOn_ = true;
    setOutput(true);
    stepPhaseUntilMs_ = nowMs + scaleSignalMs(STEP_ON_MS, signalTimingPreset);
    return;
  }

  stepPhaseOn_ = false;
  setOutput(false);
  stepPhaseUntilMs_ = nowMs + scaleSignalMs(STEP_OFF_MS, signalTimingPreset);

  if (stepBlinksRemaining_ > 0) {
    --stepBlinksRemaining_;
  }

  if (stepBlinksRemaining_ == 0) {
    stepPhaseUntilMs_ = nowMs + scaleSignalMs(STEP_DONE_PAUSE_MS, signalTimingPreset);
  }
}

void LedPattern::updateTrip(unsigned long nowMs) {
  if (sosPhaseUntilMs_ != 0 && nowMs < sosPhaseUntilMs_) {
    return;
  }

  if (sosPulseIndex_ >= 9) {
    sosPulseIndex_ = 0;
    sosPhaseOn_ = false;
    setOutput(false);
    sosPhaseUntilMs_ = nowMs + SOS_DONE_PAUSE_MS;
    return;
  }

  if (!sosPhaseOn_) {
    sosPhaseOn_ = true;
    setOutput(true);
    const unsigned long onMs = sosIsLongPulse(sosPulseIndex_) ? SOS_LONG_ON_MS : SOS_SHORT_ON_MS;
    sosPhaseUntilMs_ = nowMs + onMs;
    return;
  }

  sosPhaseOn_ = false;
  setOutput(false);
  sosPhaseUntilMs_ = nowMs + SOS_OFF_MS;
  ++sosPulseIndex_;
}

}  // namespace HeatControl
