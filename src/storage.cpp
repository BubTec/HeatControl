#include "storage.h"

#include <EEPROM.h>

#include "app_state.h"

namespace HeatControl {

namespace {

void writeFloatToEeprom(int addr, float value) {
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
  for (size_t i = 0; i < sizeof(float); ++i) {
    EEPROM.write(addr + static_cast<int>(i), bytes[i]);
  }
}

float readFloatFromEeprom(int addr) {
  float value = 0.0F;
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
  for (size_t i = 0; i < sizeof(float); ++i) {
    bytes[i] = EEPROM.read(addr + static_cast<int>(i));
  }
  return value;
}

}  // namespace

namespace {

uint8_t nextManualPowerPercent(uint8_t value) {
  const uint8_t current = clampManualPowerPercent(value);
  if (current == 25) return 50;
  if (current == 50) return 75;
  if (current == 75) return 100;
  return 25;
}

}  // namespace

uint8_t clampManualPowerPercent(uint8_t value) {
  if (value == 25 || value == 50 || value == 75 || value == 100) {
    return value;
  }
  return 25;
}

uint16_t clampManualToggleOffMs(uint16_t value) {
  // Limit the OFF/ON detection window to a sane, safe range (100ms..5000ms).
  if (value < 100U) return 100U;
  if (value > 5000U) return 5000U;
  return value;
}

void setNextBootMode(uint8_t mode) {
  EEPROM.write(EEPROM_BOOT_MODE_ADDR, mode);
  EEPROM.commit();
}

uint8_t getAndClearBootMode() {
  const uint8_t mode = EEPROM.read(EEPROM_BOOT_MODE_ADDR);
  EEPROM.write(EEPROM_BOOT_MODE_ADDR, 0);
  EEPROM.commit();
  return mode;
}

float clampTarget(float value) {
  if (value < 10.0F) return 10.0F;
  if (value > 45.0F) return 45.0F;
  return value;
}

void loadTemperatureTargets() {
  const float t1 = readFloatFromEeprom(EEPROM_TEMP1_ADDR);
  const float t2 = readFloatFromEeprom(EEPROM_TEMP2_ADDR);

  targetTemp1 = (isnan(t1) || t1 < 10.0F || t1 > 45.0F) ? DEFAULT_TARGET_TEMP : t1;
  targetTemp2 = (isnan(t2) || t2 < 10.0F || t2 > 45.0F) ? DEFAULT_TARGET_TEMP : t2;
}

void saveTemperatureTargets() {
  writeFloatToEeprom(EEPROM_TEMP1_ADDR, targetTemp1);
  writeFloatToEeprom(EEPROM_TEMP2_ADDR, targetTemp2);
  EEPROM.commit();
}

void loadSwapAssignment() {
  swapAssignment = EEPROM.read(EEPROM_SWAP_ADDR) == 1;
}

void saveSwapAssignment() {
  EEPROM.write(EEPROM_SWAP_ADDR, swapAssignment ? 1 : 0);
  EEPROM.commit();
}

void saveWiFiCredentials(const String &ssid, const String &password) {
  if (ssid.isEmpty()) {
    return;
  }

  EEPROM.write(EEPROM_INIT_ADDR, 0xAA);
  for (int i = 0; i < 32; ++i) {
    EEPROM.write(EEPROM_SSID_ADDR + i, 0);
    EEPROM.write(EEPROM_PASS_ADDR + i, 0);
  }

  for (size_t i = 0; i < ssid.length() && i < 31; ++i) {
    EEPROM.write(EEPROM_SSID_ADDR + static_cast<int>(i), ssid[i]);
  }
  for (size_t i = 0; i < password.length() && i < 31; ++i) {
    EEPROM.write(EEPROM_PASS_ADDR + static_cast<int>(i), password[i]);
  }

  EEPROM.commit();
  activeSsid = ssid;
  activePassword = password;
}

void loadWiFiCredentials() {
  if (EEPROM.read(EEPROM_INIT_ADDR) != 0xAA) {
    saveWiFiCredentials("HeatControl", "HeatControl");
    return;
  }

  char ssid[32] = {0};
  char pass[32] = {0};
  for (int i = 0; i < 31; ++i) {
    ssid[i] = static_cast<char>(EEPROM.read(EEPROM_SSID_ADDR + i));
    pass[i] = static_cast<char>(EEPROM.read(EEPROM_PASS_ADDR + i));
  }

  if (ssid[0] == '\0' || static_cast<uint8_t>(ssid[0]) == 0xFF) {
    saveWiFiCredentials("HeatControl", "HeatControl");
    return;
  }

  activeSsid = String(ssid);
  activePassword = String(pass);
}

void loadManualPowerPercents() {
  const uint8_t stored1 = EEPROM.read(EEPROM_MANUAL_POWER1_ADDR);
  const uint8_t stored2 = EEPROM.read(EEPROM_MANUAL_POWER2_ADDR);

  manualPowerPercent1 = clampManualPowerPercent(stored1);

  // Backward compatibility: older firmware stored only one value. If the second slot is unset, mirror channel 1.
  if (stored2 == 0xFF) {
    manualPowerPercent2 = manualPowerPercent1;
  } else {
    manualPowerPercent2 = clampManualPowerPercent(stored2);
  }
}

void saveManualPowerPercents() {
  EEPROM.write(EEPROM_MANUAL_POWER1_ADDR, clampManualPowerPercent(manualPowerPercent1));
  EEPROM.write(EEPROM_MANUAL_POWER2_ADDR, clampManualPowerPercent(manualPowerPercent2));
  EEPROM.commit();
}

void loadManualToggleOffMs() {
  uint16_t stored = 0U;
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&stored);
  for (size_t i = 0; i < sizeof(uint16_t); ++i) {
    bytes[i] = EEPROM.read(EEPROM_MANUAL_TOGGLE_MS_ADDR + static_cast<int>(i));
  }

  // 0xFFFF or 0 means "not initialized yet" -> use default 500ms.
  if (stored == 0xFFFFU || stored == 0U) {
    manualPowerToggleMaxOffMs = 500U;
  } else {
    manualPowerToggleMaxOffMs = clampManualToggleOffMs(stored);
  }
}

void saveManualToggleOffMs() {
  const uint16_t value = clampManualToggleOffMs(manualPowerToggleMaxOffMs);
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
  for (size_t i = 0; i < sizeof(uint16_t); ++i) {
    EEPROM.write(EEPROM_MANUAL_TOGGLE_MS_ADDR + static_cast<int>(i), bytes[i]);
  }
  EEPROM.commit();
}

void cycleManualPowerPercent1() {
  manualPowerPercent1 = nextManualPowerPercent(manualPowerPercent1);
  saveManualPowerPercents();
}

void cycleManualPowerPercent2() {
  manualPowerPercent2 = nextManualPowerPercent(manualPowerPercent2);
  saveManualPowerPercents();
}

void cycleManualPowerPercents() {
  manualPowerPercent1 = nextManualPowerPercent(manualPowerPercent1);
  manualPowerPercent2 = nextManualPowerPercent(manualPowerPercent2);
  saveManualPowerPercents();
}

uint8_t clampBatteryCellCount(uint8_t value) {
  // Keep it simple and safe: 2S..6S, default 3S.
  if (value >= 2 && value <= 6) {
    return value;
  }
  return 3;
}

void loadBatteryCellCounts() {
  const uint8_t stored1 = EEPROM.read(EEPROM_BATTERY1_CELLS_ADDR);
  const uint8_t stored2 = EEPROM.read(EEPROM_BATTERY2_CELLS_ADDR);
  battery1CellCount = clampBatteryCellCount(stored1);
  battery2CellCount = clampBatteryCellCount(stored2);
}

void saveBatteryCellCounts() {
  EEPROM.write(EEPROM_BATTERY1_CELLS_ADDR, clampBatteryCellCount(battery1CellCount));
  EEPROM.write(EEPROM_BATTERY2_CELLS_ADDR, clampBatteryCellCount(battery2CellCount));
  EEPROM.commit();
}

void writeRuntimeToEeprom(uint32_t minutes) {
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&minutes);
  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    EEPROM.write(EEPROM_RUNTIME_ADDR + static_cast<int>(i), bytes[i]);
  }
  EEPROM.commit();
}

uint32_t readRuntimeFromEeprom() {
  uint32_t minutes = 0;
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&minutes);
  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    bytes[i] = EEPROM.read(EEPROM_RUNTIME_ADDR + static_cast<int>(i));
  }
  return minutes;
}

void loadSavedRuntime() {
  savedRuntimeMinutes = readRuntimeFromEeprom();
  if (savedRuntimeMinutes == 0xFFFFFFFF) {
    savedRuntimeMinutes = 0;
    writeRuntimeToEeprom(0);
  }
}

void saveRuntimeMinute() {
  ++savedRuntimeMinutes;
  writeRuntimeToEeprom(savedRuntimeMinutes);
}

String formatRuntime(unsigned long seconds, bool showSeconds) {
  const unsigned long days = seconds / 86400;
  seconds %= 86400;
  const unsigned long hours = seconds / 3600;
  seconds %= 3600;
  const unsigned long minutes = seconds / 60;
  seconds %= 60;

  String result;
  if (days > 0) result += String(days) + "d ";
  if (hours > 0) result += String(hours) + "h ";
  if (minutes > 0) result += String(minutes) + "m ";
  if (showSeconds) result += String(seconds) + "s";
  if (result.isEmpty()) result = showSeconds ? "0s" : "0m";
  return result;
}

}  // namespace HeatControl
