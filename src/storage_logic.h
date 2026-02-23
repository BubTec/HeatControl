#pragma once

#include <cstdint>

namespace HeatControl {

constexpr uint8_t BATTERY_CHEMISTRY_LI_ION = 0U;
constexpr uint8_t BATTERY_CHEMISTRY_LI_PO = 1U;
constexpr uint8_t BATTERY_CHEMISTRY_LI_FE_PO4 = 2U;
constexpr uint8_t BATTERY_CHEMISTRY_NI_MH = 3U;
constexpr uint8_t BATTERY_CHEMISTRY_LEAD_GEL = 4U;

float clampTarget(float value);
uint8_t clampManualPowerPercent(uint8_t value);
uint16_t clampManualToggleOffMs(uint16_t value);
uint16_t clampApAutoOffMinutes(uint16_t value);
uint8_t clampBatteryCellCount(uint8_t value);
uint8_t clampBatteryChemistry(uint8_t value);
uint8_t nextManualPowerPercent(uint8_t value);

}  // namespace HeatControl

