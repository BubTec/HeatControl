#pragma once

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
void controlHeater(IGpio &gpio, int pin, bool forceOn, float currentTemp, float targetTemp);
const char *heaterStateTextFromLevel(int level);
void updateSensorsAndHeaters(ITemperatureSensors &sensors, IGpio &gpio, bool powerMode, bool swapAssignment,
                             float targetTemp1, float targetTemp2, float &currentTemp1, float &currentTemp2,
                             int heaterPin1, int heaterPin2);

}  // namespace logic
}  // namespace HeatControl
