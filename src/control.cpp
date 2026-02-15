#include "control.h"

#include "app_state.h"

namespace HeatControl {

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
  return temperatureC == DEVICE_DISCONNECTED_C || temperatureC < -20.0F || temperatureC > 125.0F;
}

void controlHeater(int pin, bool forceOn, float currentTemp, float targetTemp) {
  if (forceOn || isSensorError(currentTemp) || currentTemp < targetTemp) {
    digitalWrite(pin, LOW);   // Inverted logic: LOW = ON
  } else {
    digitalWrite(pin, HIGH);  // Inverted logic: HIGH = OFF
  }
}

String heaterStateText(int pin) {
  return digitalRead(pin) == LOW ? "ON" : "OFF";
}

void updateSensorsAndHeaters() {
  sensors.requestTemperatures();
  currentTemp1 = sensors.getTempCByIndex(0);
  currentTemp2 = sensors.getTempCByIndex(1);

  const float controlTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
  const float controlTemp2 = swapAssignment ? currentTemp1 : currentTemp2;
  controlHeater(SSR_PIN_1, powerMode, controlTemp1, targetTemp1);
  controlHeater(SSR_PIN_2, powerMode, controlTemp2, targetTemp2);
}

}  // namespace HeatControl
