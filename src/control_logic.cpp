#include "control_logic.h"

namespace HeatControl {
namespace logic {

bool isSensorError(float temperatureC) {
  return temperatureC == -127.0F || temperatureC < -20.0F || temperatureC > 125.0F;
}

bool shouldHeaterBeOn(bool forceOn, float currentTemp, float targetTemp) {
  // Fail-safe in temperature-controlled mode: sensor errors must not force heater ON.
  return forceOn || (!isSensorError(currentTemp) && currentTemp < targetTemp);
}

bool shouldManualHeaterBeOn(uint8_t manualPowerPercent, unsigned long nowMs) {
  if (manualPowerPercent >= 100) {
    return true;
  }
  if (manualPowerPercent < 25) {
    return false;
  }
  const unsigned long cycleMs = 1000UL;
  const unsigned long onTimeMs = (cycleMs * static_cast<unsigned long>(manualPowerPercent)) / 100UL;
  return (nowMs % cycleMs) < onTimeMs;
}

void controlHeater(IGpio &gpio, int pin, bool forceOn, float currentTemp, float targetTemp) {
  // Active-high logic: HIGH = ON, LOW = OFF.
  gpio.writePin(pin, shouldHeaterBeOn(forceOn, currentTemp, targetTemp) ? PIN_HIGH : PIN_LOW);
}

const char *heaterStateTextFromLevel(int level) {
  return level == PIN_HIGH ? "ON" : "OFF";
}

void updateSensorsAndHeaters(ITemperatureSensors &sensors, IGpio &gpio, bool powerMode, bool manualMode,
                             uint8_t manualPowerPercent1, uint8_t manualPowerPercent2, bool manualHeater1Enabled,
                             bool manualHeater2Enabled, bool swapAssignment, float targetTemp1, float targetTemp2,
                             float &currentTemp1, float &currentTemp2, int heaterPin1, int heaterPin2, unsigned long nowMs) {
  sensors.requestTemperatures();
  currentTemp1 = sensors.getTempCByIndex(0);
  currentTemp2 = sensors.getTempCByIndex(1);

  if (manualMode) {
    const bool manualOn1 = shouldManualHeaterBeOn(manualPowerPercent1, nowMs);
    const bool manualOn2 = shouldManualHeaterBeOn(manualPowerPercent2, nowMs);
    gpio.writePin(heaterPin1, (manualHeater1Enabled && manualOn1) ? PIN_HIGH : PIN_LOW);
    gpio.writePin(heaterPin2, (manualHeater2Enabled && manualOn2) ? PIN_HIGH : PIN_LOW);
    return;
  }

  const float controlTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
  const float controlTemp2 = swapAssignment ? currentTemp1 : currentTemp2;

  controlHeater(gpio, heaterPin1, powerMode, controlTemp1, targetTemp1);
  controlHeater(gpio, heaterPin2, powerMode, controlTemp2, targetTemp2);
}

}  // namespace logic
}  // namespace HeatControl
