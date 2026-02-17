#include "control.h"

#include "app_state.h"
#include "control_logic.h"

namespace HeatControl {

namespace {

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
  if (includeIntroPulse) {
    // Manual mode intro pulse.
    digitalWrite(SIGNAL_PIN, HIGH);
    delay(500);
    digitalWrite(SIGNAL_PIN, LOW);
    delay(220);
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
    digitalWrite(SIGNAL_PIN, HIGH);
    delay(130);
    digitalWrite(SIGNAL_PIN, LOW);
    delay(130);
  }
}

}  // namespace
void startupSignal(bool isPowerMode, bool isManualMode, uint8_t manualPowerPercent) {
  if (isManualMode) {
    signalManualPowerPattern(manualPowerPercent, true);
    return;
  }

  const int pulseCount = isPowerMode ? 2 : 1;
  for (int i = 0; i < pulseCount; ++i) {
    digitalWrite(SIGNAL_PIN, HIGH);
    delay(300);
    digitalWrite(SIGNAL_PIN, LOW);
    delay(200);
  }
}

void signalManualPowerChange(uint8_t manualPowerPercent) {
  // Short feedback pattern without the long intro pulse.
  signalManualPowerPattern(manualPowerPercent, false);
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
