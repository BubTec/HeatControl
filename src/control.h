#pragma once

#include <Arduino.h>

namespace HeatControl {

void startupSignal(bool isPowerMode, bool isManualMode, uint8_t manualPowerPercent);
bool isSensorError(float temperatureC);
void controlHeater(int pin, bool forceOn, float currentTemp, float targetTemp);
String heaterStateText(int pin);

void updateSensorsAndHeaters();

}  // namespace HeatControl
