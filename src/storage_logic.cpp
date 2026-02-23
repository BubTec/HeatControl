#include "storage_logic.h"

namespace HeatControl {

float clampTarget(float value) {
  if (value < 10.0F) return 10.0F;
  if (value > 45.0F) return 45.0F;
  return value;
}

uint8_t clampManualPowerPercent(uint8_t value) {
  if (value == 25 || value == 50 || value == 75 || value == 100) {
    return value;
  }
  return 25;
}

uint16_t clampManualToggleOffMs(uint16_t value) {
  if (value < 100U) return 100U;
  if (value > 5000U) return 5000U;
  return value;
}

uint16_t clampApAutoOffMinutes(uint16_t value) {
  if (value > 240U) return 240U;
  return value;
}

uint8_t clampBatteryCellCount(uint8_t value) {
  if (value >= 2 && value <= 6) {
    return value;
  }
  return 3;
}

uint8_t clampBatteryChemistry(uint8_t value) {
  if (value == BATTERY_CHEMISTRY_LI_ION || value == BATTERY_CHEMISTRY_LI_PO || value == BATTERY_CHEMISTRY_LI_FE_PO4 ||
      value == BATTERY_CHEMISTRY_NI_MH || value == BATTERY_CHEMISTRY_LEAD_GEL) {
    return value;
  }
  return BATTERY_CHEMISTRY_LI_ION;
}

uint8_t nextManualPowerPercent(uint8_t value) {
  const uint8_t current = clampManualPowerPercent(value);
  if (current == 25) return 50;
  if (current == 50) return 75;
  if (current == 75) return 100;
  return 25;
}

}  // namespace HeatControl

