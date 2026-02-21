#pragma once

#include <cstdint>

namespace HeatControl {

float clampTarget(float value);
uint8_t clampManualPowerPercent(uint8_t value);
uint16_t clampManualToggleOffMs(uint16_t value);
uint8_t clampBatteryCellCount(uint8_t value);
uint8_t nextManualPowerPercent(uint8_t value);

}  // namespace HeatControl

