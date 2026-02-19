#include <map>
#include <vector>

#include <unity.h>

#include "control_logic.h"

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

class MockGpio : public HeatControl::logic::IGpio {
 public:
  void writePin(int pin, int level) override {
    pinLevels[pin] = level;
    writes.push_back({pin, level});
  }

  int readPin(int pin) const override {
    auto it = pinLevels.find(pin);
    if (it == pinLevels.end()) {
      return HeatControl::logic::PIN_HIGH;
    }
    return it->second;
  }

  std::map<int, int> pinLevels;
  struct PinWrite {
    int pin;
    int level;
  };
  std::vector<PinWrite> writes;
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

void test_is_sensor_error_boundaries() {
  TEST_ASSERT_TRUE(HeatControl::logic::isSensorError(-127.0F));
  TEST_ASSERT_TRUE(HeatControl::logic::isSensorError(-21.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::isSensorError(-20.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::isSensorError(0.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::isSensorError(125.0F));
  TEST_ASSERT_TRUE(HeatControl::logic::isSensorError(125.1F));
}

void test_should_turn_on_for_sensor_error() {
  TEST_ASSERT_FALSE(HeatControl::logic::shouldHeaterBeOn(false, -127.0F, 23.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldHeaterBeOn(false, -30.0F, 23.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldHeaterBeOn(false, 130.0F, 23.0F));
}

void test_should_turn_on_based_on_temperature_delta() {
  TEST_ASSERT_TRUE(HeatControl::logic::shouldHeaterBeOn(false, 22.9F, 23.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldHeaterBeOn(false, 23.0F, 23.0F));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldHeaterBeOn(false, 25.0F, 23.0F));
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

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, false, 25, 25, true, true, false, 23.0F, 23.0F,
                                              current0, current1, 4, 5, 0);

  TEST_ASSERT_EQUAL_INT(1, sensors.requestCount);
  TEST_ASSERT_EQUAL_FLOAT(21.0F, current0);
  TEST_ASSERT_EQUAL_FLOAT(26.0F, current1);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(4));
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(5));
}

void test_update_sensors_and_heaters_power_mode_forces_on() {
  MockGpio gpio;
  MockSensors sensors;
  sensors.temp0 = 35.0F;
  sensors.temp1 = 36.0F;

  float current0 = 0.0F;
  float current1 = 0.0F;

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, true, false, 0, 0, true, true, false, 20.0F, 20.0F,
                                              current0, current1, 6, 7, 0);

  TEST_ASSERT_EQUAL_INT(1, sensors.requestCount);
  TEST_ASSERT_EQUAL_FLOAT(35.0F, current0);
  TEST_ASSERT_EQUAL_FLOAT(36.0F, current1);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(6));
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(7));
  TEST_ASSERT_EQUAL_UINT32(2U, gpio.writes.size());
}

void test_update_sensors_and_heaters_with_swap() {
  MockGpio gpio;
  MockSensors sensors;
  sensors.temp0 = 26.0F;
  sensors.temp1 = 21.0F;

  float current0 = 0.0F;
  float current1 = 0.0F;

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, false, 25, 25, true, true, true, 23.0F, 23.0F,
                                              current0, current1, 4, 5, 0);

  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(4));
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(5));
}

void test_update_sensors_and_heaters_with_swap_and_distinct_targets() {
  MockGpio gpio;
  MockSensors sensors;
  sensors.temp0 = 26.0F;
  sensors.temp1 = 21.0F;

  float current0 = 0.0F;
  float current1 = 0.0F;

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, false, 25, 25, true, true, true, 25.0F, 22.0F,
                                              current0, current1, 8, 9, 0);

  TEST_ASSERT_EQUAL_FLOAT(26.0F, current0);
  TEST_ASSERT_EQUAL_FLOAT(21.0F, current1);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(8));
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(9));
}

void test_heater_state_text_from_level() {
  TEST_ASSERT_EQUAL_STRING("ON", HeatControl::logic::heaterStateTextFromLevel(HeatControl::logic::PIN_LOW));
  TEST_ASSERT_EQUAL_STRING("OFF", HeatControl::logic::heaterStateTextFromLevel(HeatControl::logic::PIN_HIGH));
}

void test_manual_pwm_decision() {
  TEST_ASSERT_FALSE(HeatControl::logic::shouldManualHeaterBeOn(0, 0));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldManualHeaterBeOn(24, 500));
  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(25, 0));
  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(25, 249));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldManualHeaterBeOn(25, 250));

  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(50, 499));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldManualHeaterBeOn(50, 500));
  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(50, 1499));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldManualHeaterBeOn(50, 1500));

  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(75, 749));
  TEST_ASSERT_FALSE(HeatControl::logic::shouldManualHeaterBeOn(75, 750));

  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(100, 999));
  TEST_ASSERT_TRUE(HeatControl::logic::shouldManualHeaterBeOn(130, 5432));
}

void test_update_sensors_and_heaters_manual_mode() {
  MockGpio gpio;
  MockSensors sensors;
  sensors.temp0 = 22.0F;
  sensors.temp1 = 24.0F;

  float current0 = 0.0F;
  float current1 = 0.0F;

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, true, 25, 75, true, false, false, 23.0F, 23.0F,
                                              current0, current1, 4, 5, 300);

  TEST_ASSERT_EQUAL_INT(1, sensors.requestCount);
  TEST_ASSERT_EQUAL_FLOAT(22.0F, current0);
  TEST_ASSERT_EQUAL_FLOAT(24.0F, current1);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(4));  // 25% at t=300ms -> OFF
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(5));

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, true, 25, 75, true, false, false, 23.0F, 23.0F,
                                              current0, current1, 4, 5, 100);
  TEST_ASSERT_EQUAL_INT(2, sensors.requestCount);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(4));    // 25% at t=100ms -> ON
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(5));   // Disabled channel stays OFF

  HeatControl::logic::updateSensorsAndHeaters(sensors, gpio, false, true, 80, 100, false, true, false, 23.0F, 23.0F,
                                              current0, current1, 4, 5, 50);
  TEST_ASSERT_EQUAL_INT(3, sensors.requestCount);
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_HIGH, gpio.readPin(4));   // Disabled heater stays OFF
  TEST_ASSERT_EQUAL_INT(HeatControl::logic::PIN_LOW, gpio.readPin(5));    // Enabled heater with 100% power stays ON
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_should_turn_on_when_force_on);
  RUN_TEST(test_is_sensor_error_boundaries);
  RUN_TEST(test_should_turn_on_for_sensor_error);
  RUN_TEST(test_should_turn_on_based_on_temperature_delta);
  RUN_TEST(test_control_heater_sets_inverted_output);
  RUN_TEST(test_update_sensors_and_heaters_without_swap);
  RUN_TEST(test_update_sensors_and_heaters_power_mode_forces_on);
  RUN_TEST(test_update_sensors_and_heaters_with_swap);
  RUN_TEST(test_update_sensors_and_heaters_with_swap_and_distinct_targets);
  RUN_TEST(test_heater_state_text_from_level);
  RUN_TEST(test_manual_pwm_decision);
  RUN_TEST(test_update_sensors_and_heaters_manual_mode);
  return UNITY_END();
}
