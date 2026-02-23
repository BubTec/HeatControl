#include "logic_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "storage_logic.h"

namespace HeatControl {
namespace logic_helpers {

namespace {

struct VoltToSoc {
  float volt;
  uint8_t soc;
};

constexpr VoltToSoc VOLT_TO_SOC_LI_ION[] = {
    {4.18F, 100}, {4.10F, 96}, {3.99F, 82}, {3.85F, 68}, {3.77F, 58}, {3.58F, 34},
    {3.42F, 20},  {3.33F, 14}, {3.21F, 8},  {3.00F, 2},  {2.87F, 0},
};

constexpr VoltToSoc VOLT_TO_SOC_LI_PO[] = {
    {4.20F, 100}, {4.12F, 96}, {4.00F, 84}, {3.90F, 72}, {3.82F, 62}, {3.73F, 50},
    {3.63F, 34},  {3.52F, 20}, {3.42F, 10}, {3.25F, 2},  {3.10F, 0},
};

constexpr VoltToSoc VOLT_TO_SOC_LI_FE_PO4[] = {
    {3.60F, 100}, {3.45F, 96}, {3.38F, 86}, {3.34F, 72}, {3.30F, 58}, {3.26F, 42},
    {3.22F, 26},  {3.18F, 14}, {3.12F, 8},  {3.05F, 2},  {2.95F, 0},
};

constexpr VoltToSoc VOLT_TO_SOC_NI_MH[] = {
    {1.45F, 100}, {1.40F, 96}, {1.36F, 88}, {1.33F, 78}, {1.30F, 64}, {1.27F, 50},
    {1.24F, 36},  {1.21F, 22}, {1.18F, 12}, {1.12F, 4},  {1.05F, 0},
};

constexpr VoltToSoc VOLT_TO_SOC_LEAD_GEL[] = {
    {2.15F, 100}, {2.12F, 95}, {2.10F, 88}, {2.08F, 78}, {2.05F, 64}, {2.03F, 50},
    {2.00F, 36},  {1.98F, 24}, {1.95F, 12}, {1.90F, 4},  {1.85F, 0},
};

const VoltToSoc *selectSocCurve(uint8_t chemistry, size_t &tableSize) {
  if (chemistry == BATTERY_CHEMISTRY_LI_PO) {
    tableSize = sizeof(VOLT_TO_SOC_LI_PO) / sizeof(VOLT_TO_SOC_LI_PO[0]);
    return VOLT_TO_SOC_LI_PO;
  }
  if (chemistry == BATTERY_CHEMISTRY_LI_FE_PO4) {
    tableSize = sizeof(VOLT_TO_SOC_LI_FE_PO4) / sizeof(VOLT_TO_SOC_LI_FE_PO4[0]);
    return VOLT_TO_SOC_LI_FE_PO4;
  }
  if (chemistry == BATTERY_CHEMISTRY_NI_MH) {
    tableSize = sizeof(VOLT_TO_SOC_NI_MH) / sizeof(VOLT_TO_SOC_NI_MH[0]);
    return VOLT_TO_SOC_NI_MH;
  }
  if (chemistry == BATTERY_CHEMISTRY_LEAD_GEL) {
    tableSize = sizeof(VOLT_TO_SOC_LEAD_GEL) / sizeof(VOLT_TO_SOC_LEAD_GEL[0]);
    return VOLT_TO_SOC_LEAD_GEL;
  }
  tableSize = sizeof(VOLT_TO_SOC_LI_ION) / sizeof(VOLT_TO_SOC_LI_ION[0]);
  return VOLT_TO_SOC_LI_ION;
}

}  // namespace

float voltageToSocFloat(float cellVolt, uint8_t chemistry) {
  size_t tableSize = 0;
  const VoltToSoc *table = selectSocCurve(clampBatteryChemistry(chemistry), tableSize);
  const size_t last = tableSize - 1;

  if (cellVolt >= table[0].volt) {
    return static_cast<float>(table[0].soc);
  }

  if (cellVolt <= table[last].volt) {
    const float vHigh = table[last - 1].volt;
    const float vLow = table[last].volt;
    const float socHigh = static_cast<float>(table[last - 1].soc);
    const float socLow = static_cast<float>(table[last].soc);
    const float t = (cellVolt - vLow) / (vHigh - vLow);
    return socLow + t * (socHigh - socLow);
  }

  for (size_t i = 0; i < last; ++i) {
    const float vHigh = table[i].volt;
    const float vLow = table[i + 1].volt;
    if (cellVolt <= vHigh && cellVolt > vLow) {
      const float socHigh = static_cast<float>(table[i].soc);
      const float socLow = static_cast<float>(table[i + 1].soc);
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
                           uint8_t chemistry, float &socSmoothed, bool &smoothingInitialized, uint8_t &socPercent) {
  constexpr float SOC_EMA_ALPHA = 0.18F;
  const float adcV = static_cast<float>(adcMilliVolts) / 1000.0F;
  packV = adcV * dividerRatio;
  const uint8_t cells = cellCount == 0 ? 3 : cellCount;
  cellV = packV / static_cast<float>(cells);
  const float socRaw = voltageToSocFloat(cellV, chemistry);
  if (!smoothingInitialized) {
    socSmoothed = socRaw;
    smoothingInitialized = true;
  } else {
    socSmoothed += SOC_EMA_ALPHA * (socRaw - socSmoothed);
  }
  socPercent = clampSocPercent(socSmoothed);
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
