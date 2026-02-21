#include "logic_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace HeatControl {
namespace logic_helpers {

namespace {

struct VoltToSoc {
  float volt;
  uint8_t soc;
};

constexpr VoltToSoc VOLT_TO_SOC[] = {
    {4.18F, 100}, {4.10F, 96}, {3.99F, 82}, {3.85F, 68}, {3.77F, 58}, {3.58F, 34},
    {3.42F, 20},  {3.33F, 14}, {3.21F, 8},  {3.00F, 2},  {2.87F, 0},
};

}  // namespace

float voltageToSocFloat(float cellVolt) {
  const size_t last = (sizeof(VOLT_TO_SOC) / sizeof(VOLT_TO_SOC[0])) - 1;

  if (cellVolt >= VOLT_TO_SOC[0].volt) {
    return static_cast<float>(VOLT_TO_SOC[0].soc);
  }

  if (cellVolt <= VOLT_TO_SOC[last].volt) {
    const float vHigh = VOLT_TO_SOC[last - 1].volt;
    const float vLow = VOLT_TO_SOC[last].volt;
    const float socHigh = static_cast<float>(VOLT_TO_SOC[last - 1].soc);
    const float socLow = static_cast<float>(VOLT_TO_SOC[last].soc);
    const float t = (cellVolt - vLow) / (vHigh - vLow);
    return socLow + t * (socHigh - socLow);
  }

  for (size_t i = 0; i < last; ++i) {
    const float vHigh = VOLT_TO_SOC[i].volt;
    const float vLow = VOLT_TO_SOC[i + 1].volt;
    if (cellVolt <= vHigh && cellVolt > vLow) {
      const float socHigh = static_cast<float>(VOLT_TO_SOC[i].soc);
      const float socLow = static_cast<float>(VOLT_TO_SOC[i + 1].soc);
      const float t = (cellVolt - vLow) / (vHigh - vLow);
      return socLow + t * (socHigh - socLow);
    }
  }

  return 0.0F;
}

uint8_t clampSocPercent(float soc) {
  if (soc <= 0.0F) return 0;
  if (soc >= 100.0F) return 100;
  return static_cast<uint8_t>(soc + 0.5F);
}

float updateBatteryFromAdc(uint16_t adcMilliVolts, uint8_t cellCount, float dividerRatio, float &packV, float &cellV,
                           uint8_t &socPercent) {
  const float adcV = static_cast<float>(adcMilliVolts) / 1000.0F;
  packV = adcV * dividerRatio;
  const uint8_t cells = cellCount == 0 ? 3 : cellCount;
  cellV = packV / static_cast<float>(cells);
  const float socRaw = voltageToSocFloat(cellV);
  socPercent = clampSocPercent(socRaw);
  return socRaw;
}

bool ntcMilliVoltsToTempC(uint16_t adcMilliVolts, float vccMilliVolts, float seriesResistorOhm,
                          float nominalResistorOhm, float betaValue, float nominalTempC, float &tempC) {
  const float vNodeMv = static_cast<float>(adcMilliVolts);
  if (vNodeMv <= 0.0F || vNodeMv >= vccMilliVolts) {
    return false;
  }

  const float ntcOhm = seriesResistorOhm * ((vccMilliVolts / vNodeMv) - 1.0F);
  if (ntcOhm <= 0.0F) {
    return false;
  }

  const float nominalTempK = nominalTempC + 273.15F;
  const float invT = (1.0F / nominalTempK) + (1.0F / betaValue) * std::log(ntcOhm / nominalResistorOhm);
  if (invT <= 0.0F) {
    return false;
  }

  tempC = (1.0F / invT) - 273.15F;
  return true;
}

size_t appendLineToRollingBuffer(char *buffer, size_t bufferSize, size_t &currentLength, const char *line,
                                 size_t lineLength) {
  if (bufferSize == 0 || buffer == nullptr) {
    currentLength = 0;
    return 0;
  }

  if (line == nullptr || lineLength == 0) {
    return currentLength;
  }

  const size_t maxLen = bufferSize - 1;
  const size_t appendLen = std::min(lineLength + 1, bufferSize) - 1;

  if (appendLen >= maxLen) {
    const size_t copyOffset = lineLength - maxLen;
    std::memcpy(buffer, line + copyOffset, maxLen);
    buffer[maxLen] = '\0';
    currentLength = maxLen;
    return currentLength;
  }

  if (currentLength + appendLen > maxLen) {
    const size_t overflow = (currentLength + appendLen) - maxLen;
    std::memmove(buffer, buffer + overflow, currentLength - overflow);
    currentLength -= overflow;
  }

  std::memcpy(buffer + currentLength, line, appendLen);
  currentLength += appendLen;
  buffer[currentLength] = '\0';
  return currentLength;
}

std::string jsonEscape(const std::string &value) {
  std::string out;
  out.reserve(value.length() + 8);
  for (char c : value) {
    switch (c) {
      case '\\':
      case '"':
        out.push_back('\\');
        out.push_back(c);
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

}  // namespace logic_helpers
}  // namespace HeatControl
