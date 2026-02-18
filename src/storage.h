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

// Persist last known battery presence mask (bit0 = battery1, bit1 = battery2).
uint8_t loadLastBatteryMask();
void saveLastBatteryMask(uint8_t mask);
void loadMosfetOvertempEvents();
void saveMosfetOvertempEvent(uint8_t channel, float tripTempC);
void clearMosfetOvertempEvents();

}  // namespace HeatControl
