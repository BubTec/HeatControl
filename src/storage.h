#pragma once

#include <Arduino.h>

namespace HeatControl {

void setNextBootMode(uint8_t mode);
uint8_t getAndClearBootMode();

float clampTarget(float value);

void loadTemperatureTargets();
void saveTemperatureTargets();

void loadSwapAssignment();
void saveSwapAssignment();

void saveWiFiCredentials(const String &ssid, const String &password);
void loadWiFiCredentials();

void writeRuntimeToEeprom(uint32_t minutes);
uint32_t readRuntimeFromEeprom();
void loadSavedRuntime();
void saveRuntimeMinute();

String formatRuntime(unsigned long seconds, bool showSeconds);

}  // namespace HeatControl
