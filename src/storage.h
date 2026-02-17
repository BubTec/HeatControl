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

uint8_t clampManualPowerPercent(uint8_t value);
void loadManualPowerPercents();
void saveManualPowerPercents();
void cycleManualPowerPercent1();
void cycleManualPowerPercent2();
void cycleManualPowerPercents();

uint16_t clampManualToggleOffMs(uint16_t value);
void loadManualToggleOffMs();
void saveManualToggleOffMs();

uint8_t clampBatteryCellCount(uint8_t value);
void loadBatteryCellCounts();
void saveBatteryCellCounts();

void writeRuntimeToEeprom(uint32_t minutes);
uint32_t readRuntimeFromEeprom();
void loadSavedRuntime();
void saveRuntimeMinute();

String formatRuntime(unsigned long seconds, bool showSeconds);

}  // namespace HeatControl
