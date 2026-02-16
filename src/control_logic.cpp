#include "control_logic.h"

namespace HeatControl {
namespace logic {

bool isSensorError(float temperatureC) {
  return temperatureC == -127.0F || temperatureC < -20.0F || temperatureC > 125.0F;
}

bool shouldHeaterBeOn(bool forceOn, float currentTemp, float targetTemp) {
  return forceOn || isSensorError(currentTemp) || currentTemp < targetTemp;
}

void controlHeater(IGpio &gpio, int pin, bool forceOn, float currentTemp, float targetTemp) {
  // Inverted logic: LOW = ON, HIGH = OFF.
  gpio.writePin(pin, shouldHeaterBeOn(forceOn, currentTemp, targetTemp) ? PIN_LOW : PIN_HIGH);
}

const char *heaterStateTextFromLevel(int level) {
  return level == PIN_LOW ? "ON" : "OFF";
}

void updateSensorsAndHeaters(ITemperatureSensors &sensors, IGpio &gpio, bool powerMode, bool swapAssignment,
                             float targetTemp1, float targetTemp2, float &currentTemp1, float &currentTemp2,
                             int heaterPin1, int heaterPin2) {
  sensors.requestTemperatures();
  currentTemp1 = sensors.getTempCByIndex(0);
  currentTemp2 = sensors.getTempCByIndex(1);

  const float controlTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
  const float controlTemp2 = swapAssignment ? currentTemp1 : currentTemp2;

  controlHeater(gpio, heaterPin1, powerMode, controlTemp1, targetTemp1);
  controlHeater(gpio, heaterPin2, powerMode, controlTemp2, targetTemp2);
}

}  // namespace logic
}  // namespace HeatControl
