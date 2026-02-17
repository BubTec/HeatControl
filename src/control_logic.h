#pragma once

#include <cstdint>

namespace HeatControl {
namespace logic {

constexpr int PIN_LOW = 0;
constexpr int PIN_HIGH = 1;

class IGpio {
 public:
  virtual ~IGpio() = default;
  virtual void writePin(int pin, int level) = 0;
  virtual int readPin(int pin) const = 0;
};

class ITemperatureSensors {
 public:
  virtual ~ITemperatureSensors() = default;
  virtual void requestTemperatures() = 0;
  virtual float getTempCByIndex(int index) = 0;
};

bool isSensorError(float temperatureC);
bool shouldHeaterBeOn(bool forceOn, float currentTemp, float targetTemp);
bool shouldManualHeaterBeOn(uint8_t manualPowerPercent, unsigned long nowMs);
void controlHeater(IGpio &gpio, int pin, bool forceOn, float currentTemp, float targetTemp);
const char *heaterStateTextFromLevel(int level);
void updateSensorsAndHeaters(ITemperatureSensors &sensors, IGpio &gpio, bool powerMode, bool manualMode,
                             uint8_t manualPowerPercent1, uint8_t manualPowerPercent2, bool manualHeater1Enabled,
                             bool manualHeater2Enabled, bool swapAssignment, float targetTemp1, float targetTemp2,
                             float &currentTemp1, float &currentTemp2, int heaterPin1, int heaterPin2, unsigned long nowMs);

}  // namespace logic
}  // namespace HeatControl
