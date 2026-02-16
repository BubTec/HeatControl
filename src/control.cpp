#include "control.h"

#include "app_state.h"
#include "control_logic.h"

namespace HeatControl {

namespace {

class ArduinoGpio : public logic::IGpio {
 public:
  void writePin(int pin, int level) override {
    digitalWrite(pin, level == PIN_LOW ? LOW : HIGH);
  }

  int readPin(int pin) const override {
    return digitalRead(pin) == LOW ? PIN_LOW : PIN_HIGH;
  }
};

class DallasSensorsAdapter : public logic::ITemperatureSensors {
 public:
  void requestTemperatures() override { sensors.requestTemperatures(); }

  float getTempCByIndex(int index) override { return sensors.getTempCByIndex(index); }
};

}  // namespace

void startupSignal(bool isPowerMode) {
  const int pulseCount = isPowerMode ? 2 : 1;
  for (int i = 0; i < pulseCount; ++i) {
    digitalWrite(SIGNAL_PIN, HIGH);
    delay(300);
    digitalWrite(SIGNAL_PIN, LOW);
    delay(200);
  }
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
  logic::updateSensorsAndHeaters(tempSensors, gpio, powerMode, swapAssignment, targetTemp1, targetTemp2, currentTemp1,
                                 currentTemp2, SSR_PIN_1, SSR_PIN_2);
}

}  // namespace HeatControl
