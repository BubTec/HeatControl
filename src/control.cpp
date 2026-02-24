#include "control.h"

#include "app_state.h"
#include "control_logic.h"

namespace HeatControl {

namespace {

void setSignalAndLeds(bool active) {
  digitalWrite(SIGNAL_PIN, active ? LOW : HIGH);
  digitalWrite(BATTERY_LED_PIN_1, active ? HIGH : LOW);
  digitalWrite(BATTERY_LED_PIN_2, active ? HIGH : LOW);
}

void delayScaled(unsigned long baseMs) {
  delay(scaleSignalMs(baseMs, signalTimingPreset));
}

class ArduinoGpio : public logic::IGpio {
 public:
  void writePin(int pin, int level) override {
    digitalWrite(pin, level == logic::PIN_LOW ? LOW : HIGH);
  }

  int readPin(int pin) const override {
    return digitalRead(pin) == LOW ? logic::PIN_LOW : logic::PIN_HIGH;
  }
};

class DallasSensorsAdapter : public logic::ITemperatureSensors {
 public:
  void requestTemperatures() override { sensors.requestTemperatures(); }

  float getTempCByIndex(int index) override { return sensors.getTempCByIndex(index); }
};

void signalManualPowerPattern(uint8_t manualPowerPercent, bool includeIntroPulse) {
  const bool prevLed1 = digitalRead(BATTERY_LED_PIN_1) == HIGH;
  const bool prevLed2 = digitalRead(BATTERY_LED_PIN_2) == HIGH;
  if (includeIntroPulse) {
    // Manual mode intro pulse.
    setSignalAndLeds(true);
    delayScaled(500);
    setSignalAndLeds(false);
    delayScaled(220);
  }

  // Duty step feedback: 1/2/3/4 short pulses for 25/50/75/100%.
  int stepPulses = 1;
  if (manualPowerPercent >= 100) {
    stepPulses = 4;
  } else if (manualPowerPercent >= 75) {
    stepPulses = 3;
  } else if (manualPowerPercent >= 50) {
    stepPulses = 2;
  }
  for (int i = 0; i < stepPulses; ++i) {
    setSignalAndLeds(true);
    delayScaled(130);
    setSignalAndLeds(false);
    delayScaled(130);
  }

  digitalWrite(BATTERY_LED_PIN_1, prevLed1 ? HIGH : LOW);
  digitalWrite(BATTERY_LED_PIN_2, prevLed2 ? HIGH : LOW);
}

}  // namespace
void startupSignal(bool isPowerMode, bool isManualMode, uint8_t manualPowerPercent) {
  if (isManualMode) {
    signalManualPowerPattern(manualPowerPercent, true);
    return;
  }

  const int pulseCount = isPowerMode ? 2 : 1;
  const bool prevLed1 = digitalRead(BATTERY_LED_PIN_1) == HIGH;
  const bool prevLed2 = digitalRead(BATTERY_LED_PIN_2) == HIGH;
  for (int i = 0; i < pulseCount; ++i) {
    setSignalAndLeds(true);
    delayScaled(300);
    setSignalAndLeds(false);
    delayScaled(200);
  }

  digitalWrite(BATTERY_LED_PIN_1, prevLed1 ? HIGH : LOW);
  digitalWrite(BATTERY_LED_PIN_2, prevLed2 ? HIGH : LOW);
}

void signalManualPowerChange(uint8_t manualPowerPercent) {
  // Short feedback pattern without the long intro pulse.
  signalManualPowerPattern(manualPowerPercent, false);
}

void signalTestPulse() {
  const bool prevLed1 = digitalRead(BATTERY_LED_PIN_1) == HIGH;
  const bool prevLed2 = digitalRead(BATTERY_LED_PIN_2) == HIGH;
  setSignalAndLeds(true);
  delayScaled(120);
  setSignalAndLeds(false);
  digitalWrite(BATTERY_LED_PIN_1, prevLed1 ? HIGH : LOW);
  digitalWrite(BATTERY_LED_PIN_2, prevLed2 ? HIGH : LOW);
}

bool isSensorError(float temperatureC) {
  return logic::isSensorError(static_cast<float>(temperatureC));
}

void controlHeater(int pin, bool forceOn, float currentTemp, float targetTemp) {
  ArduinoGpio gpio;
  logic::controlHeater(gpio, pin, forceOn, currentTemp, targetTemp);
}

String heaterStateText(int pin) {
  ArduinoGpio gpio;
  return logic::heaterStateTextFromLevel(gpio.readPin(pin));
}

void updateSensorsAndHeaters() {
  ArduinoGpio gpio;
  DallasSensorsAdapter tempSensors;
  logic::updateSensorsAndHeaters(tempSensors, gpio, powerMode, manualMode, manualPowerPercent1, manualPowerPercent2,
                                 manualHeater1Enabled, manualHeater2Enabled, swapAssignment, targetTemp1, targetTemp2,
                                 currentTemp1, currentTemp2, SSR_PIN_1, SSR_PIN_2, millis());
}

}  // namespace HeatControl
