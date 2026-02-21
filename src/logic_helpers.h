#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace HeatControl {
namespace logic_helpers {

float voltageToSocFloat(float cellVolt);
uint8_t clampSocPercent(float soc);
float updateBatteryFromAdc(uint16_t adcMilliVolts, uint8_t cellCount, float dividerRatio, float &packV, float &cellV,
                           uint8_t &socPercent);

bool ntcMilliVoltsToTempC(uint16_t adcMilliVolts, float vccMilliVolts, float seriesResistorOhm,
                          float nominalResistorOhm, float betaValue, float nominalTempC, float &tempC);

size_t appendLineToRollingBuffer(char *buffer, size_t bufferSize, size_t &currentLength, const char *line,
                                 size_t lineLength);

std::string jsonEscape(const std::string &value);

}  // namespace logic_helpers
}  // namespace HeatControl

