#include <map>

#include <unity.h>

#include "control_logic.h"

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

class MockGpio : public HeatControl::logic::IGpio {
 public:
  void writePin(int pin, int level) override { pinLevels[pin] = level; }

  int readPin(int pin) const override {
    auto it = pinLevels.find(pin);
    if (it == pinLevels.end()) {
      return HeatControl::logic::PIN_HIGH;
    }
    return it->second;
  }

  std::map<int, int> pinLevels;
};

class MockSensors : public HeatControl::logic::ITemperatureSensors {
 public:
  void requestTemperatures() override { requestCount++; }

  float getTempCByIndex(int index) override {
    if (index == 0) return temp0;
    if (index == 1) return temp1;
    return -127.0F;
  }

  int requestCount = 0;
  float temp0 = 20.0F;
  float temp1 = 20.0F;
};

void test_should_turn_on_when_force_on() {
  TEST_ASSERT_TRUE(HeatControl::logic::shouldHeaterBeOn(true, 100.0F, 10.0F));
}

void test_should_turn_on_for_sensor_error() {
  TEST_ASSERT_TRUE(HeatControl::logic::shouldHeaterBeOn(false, -127.0F, 23.0F));
  TEST_ASSERT_TRUE(HeatControl::logic::shouldHeaterBeOn(false, -30.0F, 23.0F));
  TEST_ASSERT_TRUE(HeatControl::logic::shouldHeaterBeOn(false, 130.0F, 23.0F));
}

void test_control_heater_sets_inverted_output() {
  MockGpio gpio;
  HeatControl::logic::controlHeater(gpio, 4, false, 20.0F, 23.0F);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(4));

  HeatControl::logic::controlHeater(gpio, 4, false, 24.0F, 23.0F);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(4));
}

void test_update_sensors_and_heaters_without_swap() {
  MockGpio gpio;
  MockSensors sensors;
  sensors.temp0 = 21.0F;
  sensors.temp1 = 26.0F;

  float current0 = 0.0F;
  float current1 = 0.0F;

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, false, 23.0F, 23.0F, current0, current1, 4, 5);

  TEST_ASSERT_EQUAL_INT(1, sensors.requestCount);
  TEST_ASSERT_EQUAL_FLOAT(21.0F, current0);
  TEST_ASSERT_EQUAL_FLOAT(26.0F, current1);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(4));
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(5));
}

void test_update_sensors_and_heaters_with_swap() {
  MockGpio gpio;
  MockSensors sensors;
  sensors.temp0 = 26.0F;
  sensors.temp1 = 21.0F;

  float current0 = 0.0F;
  float current1 = 0.0F;

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, true, 23.0F, 23.0F, current0, current1, 4, 5);

  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(4));
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(5));
}

void test_heater_state_text_from_level() {
  TEST_ASSERT_EQUAL_STRING("ON", HeatControl::logic::heaterStateTextFromLevel(HeatControl::logic::PIN_LOW));
  TEST_ASSERT_EQUAL_STRING("OFF", HeatControl::logic::heaterStateTextFromLevel(HeatControl::logic::PIN_HIGH));
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_should_turn_on_when_force_on);
  RUN_TEST(test_should_turn_on_for_sensor_error);
  RUN_TEST(test_control_heater_sets_inverted_output);
  RUN_TEST(test_update_sensors_and_heaters_without_swap);
  RUN_TEST(test_update_sensors_and_heaters_with_swap);
  RUN_TEST(test_heater_state_text_from_level);
  return UNITY_END();
}
