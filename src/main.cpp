#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <esp_system.h>

#include "app_state.h"
#include "battery_toggle.h"
#include "control.h"
#include "logic_helpers.h"
#include "storage.h"
#include "web_server.h"

using namespace HeatControl;

namespace {

// NTC input model:
// 3.3V -> NTC (10k, B3950) -> ADC node -> 10k resistor -> GND
constexpr float NTC_VCC_MV = 3300.0F;
constexpr float NTC_SERIES_RESISTOR_OHM = 10000.0F;
constexpr float NTC_NOMINAL_RESISTANCE_OHM = 10000.0F;
constexpr float NTC_BETA = 3950.0F;
constexpr float NTC_NOMINAL_TEMP_C = 25.0F;
constexpr uint16_t BATTERY_ADC_OFF_THRESHOLD_MV = 80;
constexpr uint16_t BATTERY_ADC_ON_THRESHOLD_MV = 300;
constexpr uint8_t BATTERY_STABLE_SAMPLES = 2;

BatteryToggleDetector battery1Detector(BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV, BATTERY_STABLE_SAMPLES);
BatteryToggleDetector battery2Detector(BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV, BATTERY_STABLE_SAMPLES);

void appendSerialLogLine(const String &line) {
  char lineBuf[320];
  const int written = snprintf(lineBuf, sizeof(lineBuf), "%s\n", line.c_str());
  if (written <= 0) {
    return;
  }

  const size_t appendLen =
      static_cast<size_t>(written >= static_cast<int>(sizeof(lineBuf)) ? sizeof(lineBuf) - 1 : written);
  logic_helpers::appendLineToRollingBuffer(serialLogBuffer, sizeof(serialLogBuffer), serialLogLength, lineBuf,
                                           appendLen);
}

}  // namespace

namespace HeatControl {

void logLine(const String &line) {
  Serial.println(line);
  appendSerialLogLine(line);
}

void logf(const char *fmt, ...) {
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  Serial.println(buffer);
  appendSerialLogLine(String(buffer));
}

}  // namespace HeatControl

void setup() {
  Serial.begin(115200);
  delay(500);

  EEPROM.begin(EEPROM_SIZE);

  pinMode(SSR_PIN_1, OUTPUT);
  pinMode(SSR_PIN_2, OUTPUT);
  pinMode(INPUT_PIN, INPUT_PULLDOWN);
  pinMode(SIGNAL_PIN, OUTPUT);

  digitalWrite(SSR_PIN_1, HIGH);
  digitalWrite(SSR_PIN_2, HIGH);
  digitalWrite(SIGNAL_PIN, LOW);

  // ADC setup (ESP32-C3): 12-bit readings, extended input range.
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN_1, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_2, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_NTC_MOSFET_1, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_NTC_MOSFET_2, ADC_11db);
  
  // Initialize state tracking
  lastHeater1State = (digitalRead(SSR_PIN_1) == LOW);
  lastHeater2State = (digitalRead(SSR_PIN_2) == LOW);
  lastSignalPinState = (digitalRead(SIGNAL_PIN) == HIGH);
  lastInputPinState = (digitalRead(INPUT_PIN) == HIGH);

  const uint8_t savedBootMode = getAndClearBootMode();
  if (savedBootMode == BOOT_MODE_NORMAL) {
    powerMode = false;
  } else if (savedBootMode == BOOT_MODE_POWER) {
    powerMode = true;
  } else {
    powerMode = (digitalRead(INPUT_PIN) == HIGH);
  }

  sensors.begin();
  const uint8_t sensorCount = static_cast<uint8_t>(sensors.getDeviceCount());

  // Normal (temperature-controlled) mode is only allowed when both sensors are present.
  // For any configuration with fewer than two sensors, fall back to manual PWM mode.
  manualMode = (sensorCount < 2);
  if (manualMode) {
    powerMode = false;
    loadManualPowerPercents();
    if (digitalRead(INPUT_PIN) == HIGH) {
      // Short OFF/ON gesture in manual mode: adjust only the heater that belonged
      // to the last active battery, if we have a clear mapping from EEPROM.
      const uint8_t lastMask = loadLastBatteryMask();
      if (lastMask == 0x01U) {
        cycleManualPowerPercent1();
        signalManualPowerChange(manualPowerPercent1);
      } else if (lastMask == 0x02U) {
        cycleManualPowerPercent2();
        signalManualPowerChange(manualPowerPercent2);
      } else {
        // Fallback: no clear last battery information -> advance both channels.
        cycleManualPowerPercents();
        signalManualPowerChange(manualPowerPercent1);
      }
    }
  }

  // Startup feedback uses channel 1 manual power as reference.
  startupSignal(powerMode, manualMode, manualPowerPercent1);
  loadTemperatureTargets();
  loadSwapAssignment();
  loadWiFiCredentials();
  loadBatteryCellCounts();
  loadManualToggleOffMs();
  loadMosfetOvertempEvents();
  logf("Manual toggle window: %u ms (min=100, max=5000)", manualPowerToggleMaxOffMs);
  loadSavedRuntime();

  startTimeMs = millis();
  lastRuntimeSaveMs = millis();

  fileSystemReady = LittleFS.begin(true);

  WiFi.mode(WIFI_AP);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(activeSsid.c_str(), activePassword.c_str(), 1, false, AP_MAX_CLIENTS);
  WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
    if (event != ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
      return;
    }
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", info.wifi_ap_staconnected.mac[0],
             info.wifi_ap_staconnected.mac[1], info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
             info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
    logf("AP client connected: %s | aid=%d", mac, info.wifi_ap_staconnected.aid);
  });

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", IPAddress(4, 3, 2, 1));

  setupWebServer();

  logLine("");
  logLine("=== ESP32-C3 modular setup test ===");
  logf("Reset reason: %d", static_cast<int>(esp_reset_reason()));
  const String modeText =
      manualMode ? ("MANUAL H1 " + String(manualPowerPercent1) + "% / H2 " + String(manualPowerPercent2) + "%")
                                     : (powerMode ? "POWER" : "NORMAL");
  logf("Detected mode: %s", modeText.c_str());
  logf("OneWire bus pin: GPIO%d", ONE_WIRE_BUS);
  logf("MOSFET NTC pins: H1=GPIO%d | H2=GPIO%d | overtemp_limit=%.1fC", ADC_PIN_NTC_MOSFET_1, ADC_PIN_NTC_MOSFET_2,
       MOSFET_OVERTEMP_LIMIT_C);
  logf("AP IP: %s", WiFi.softAPIP().toString().c_str());
  logf("AP SSID: %s", activeSsid.c_str());
  logf("LittleFS: %s", fileSystemReady ? "ready" : "not ready");
  logLine("HTTP: /, /status, /runtime, /setTemp, /saveSettings, /swapSensors, /setWiFi, /restart, /resetRuntime, /update, /signalTest, /logs");
  logf("SSR1: %s | SSR2: %s", heaterStateText(SSR_PIN_1).c_str(), heaterStateText(SSR_PIN_2).c_str());
}

void loop() {
  const unsigned long now = millis();

  if (restartScheduled && (static_cast<long>(now - restartAtMs) >= 0)) {
    restartScheduled = false;
    logLine("Restart scheduled: rebooting now.");
    delay(80);
    ESP.restart();
  }

  // High-frequency ADC check for manual OFF/ON detection (better than 1s resolution).
  static unsigned long lastManualToggleCheckMs = 0;
  if (manualMode && (now - lastManualToggleCheckMs) >= 50UL) {
    lastManualToggleCheckMs = now;

    // Read ADC inputs (millivolts) and update battery state.
    adc1MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_1));
    adc2MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_2));
    logic_helpers::updateBatteryFromAdc(adc1MilliVolts, battery1CellCount, BATTERY_DIVIDER_RATIO, battery1PackVoltage,
                                        battery1CellVoltage, battery1SocPercent);
    logic_helpers::updateBatteryFromAdc(adc2MilliVolts, battery2CellCount, BATTERY_DIVIDER_RATIO, battery2PackVoltage,
                                        battery2CellVoltage, battery2SocPercent);

    // Robust OFF/ON detection based on raw ADC with hysteresis and debounce.
    const auto batt1Sample = battery1Detector.update(adc1MilliVolts);
    const auto batt2Sample = battery2Detector.update(adc2MilliVolts);
    const bool batt1OffNow = batt1Sample.offNow;
    const bool batt1OnNow = batt1Sample.onNow;
    const bool batt2OffNow = batt2Sample.offNow;
    const bool batt2OnNow = batt2Sample.onNow;

    // Persist last known battery presence mask (bit0 = battery1, bit1 = battery2)
    // only when it changes, to avoid unnecessary EEPROM wear.
    uint8_t currentMask = 0;
    if (adc1MilliVolts >= BATTERY_ADC_ON_THRESHOLD_MV) {
      currentMask |= 0x01U;
    }
    if (adc2MilliVolts >= BATTERY_ADC_ON_THRESHOLD_MV) {
      currentMask |= 0x02U;
    }
    static uint8_t lastSavedMask = 0xFFU;
    if (currentMask != lastSavedMask) {
      lastSavedMask = currentMask;
      saveLastBatteryMask(currentMask);
    }

    // Edge logging for OFF/ON detection flags to debug threshold behavior.
    if (batt1Sample.offEdge) {
      logf("Batt1OffNow changed -> %d | adc1_mv=%u (off_thresh=%u, on_thresh=%u)", batt1OffNow ? 1 : 0,
           adc1MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }
    if (batt1Sample.onEdge) {
      logf("Batt1OnNow changed  -> %d | adc1_mv=%u (off_thresh=%u, on_thresh=%u)", batt1OnNow ? 1 : 0,
           adc1MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }
    if (batt2Sample.offEdge) {
      logf("Batt2OffNow changed -> %d | adc2_mv=%u (off_thresh=%u, on_thresh=%u)", batt2OffNow ? 1 : 0,
           adc2MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }
    if (batt2Sample.onEdge) {
      logf("Batt2OnNow changed  -> %d | adc2_mv=%u (off_thresh=%u, on_thresh=%u)", batt2OnNow ? 1 : 0,
           adc2MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }

    // Battery switch detection: treat an OFF period as a user trigger to cycle the manual PWM step
    // only if power returns within manualPowerToggleMaxOffMs.
    if (batt1OffNow && !battery1Off) {
      battery1Off = true;
      battery1OffSinceMs = now;
      logf("Battery 1 OFF edge detected | now_ms=%lu | adc1_mv=%u", now, adc1MilliVolts);
    } else if (!batt1OffNow && batt1OnNow && battery1Off) {
      const unsigned long offMs = now - battery1OffSinceMs;
      battery1Off = false;
      logf("Battery 1 ON edge detected  | off_ms=%lu (limit=%u) | adc1_mv=%u", offMs,
           static_cast<unsigned int>(manualPowerToggleMaxOffMs), adc1MilliVolts);
      if (offMs <= static_cast<unsigned long>(manualPowerToggleMaxOffMs)) {
        cycleManualPowerPercent1();
        logf("Battery 1 OFF/ON trigger (%lums) -> manual power 1 = %u%%", offMs, manualPowerPercent1);
        // Haptic feedback for manual heater 1 power change.
        signalManualPowerChange(manualPowerPercent1);
      } else {
        logf("Battery 1 OFF/ON ignored (off_ms too long for toggle window)");
      }
    }

    if (batt2OffNow && !battery2Off) {
      battery2Off = true;
      battery2OffSinceMs = now;
      logf("Battery 2 OFF edge detected | now_ms=%lu | adc2_mv=%u", now, adc2MilliVolts);
    } else if (!batt2OffNow && batt2OnNow && battery2Off) {
      const unsigned long offMs = now - battery2OffSinceMs;
      battery2Off = false;
      logf("Battery 2 ON edge detected  | off_ms=%lu (limit=%u) | adc2_mv=%u", offMs,
           static_cast<unsigned int>(manualPowerToggleMaxOffMs), adc2MilliVolts);
      if (offMs <= static_cast<unsigned long>(manualPowerToggleMaxOffMs)) {
        cycleManualPowerPercent2();
        logf("Battery 2 OFF/ON trigger (%lums) -> manual power 2 = %u%%", offMs, manualPowerPercent2);
        // Haptic feedback for manual heater 2 power change.
        signalManualPowerChange(manualPowerPercent2);
      } else {
        logf("Battery 2 OFF/ON ignored (off_ms too long for toggle window)");
      }
    }
  }

  if (now - lastSensorMs >= 1000) {
    lastSensorMs = now;
    updateSensorsAndHeaters();
    const bool prevOvertemp1 = mosfet1OvertempActive;
    const bool prevOvertemp2 = mosfet2OvertempActive;

    ntcMosfet1MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_NTC_MOSFET_1));
    ntcMosfet2MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_NTC_MOSFET_2));
    const bool ntc1Valid = logic_helpers::ntcMilliVoltsToTempC(ntcMosfet1MilliVolts, NTC_VCC_MV, NTC_SERIES_RESISTOR_OHM,
                                                              NTC_NOMINAL_RESISTANCE_OHM, NTC_BETA, NTC_NOMINAL_TEMP_C,
                                                              ntcMosfet1TempC);
    const bool ntc2Valid = logic_helpers::ntcMilliVoltsToTempC(ntcMosfet2MilliVolts, NTC_VCC_MV, NTC_SERIES_RESISTOR_OHM,
                                                              NTC_NOMINAL_RESISTANCE_OHM, NTC_BETA, NTC_NOMINAL_TEMP_C,
                                                              ntcMosfet2TempC);
    if (!ntc1Valid) {
      ntcMosfet1TempC = NAN;
    }
    if (!ntc2Valid) {
      ntcMosfet2TempC = NAN;
    }

    if (!mosfet1OvertempActive && ntc1Valid && ntcMosfet1TempC >= MOSFET_OVERTEMP_LIMIT_C) {
      mosfet1OvertempActive = true;
      saveMosfetOvertempEvent(1U, ntcMosfet1TempC);
      logf("MOSFET1 overtemp TRIP | temp=%.2fC | limit=%.1fC | heater forced OFF", ntcMosfet1TempC,
           MOSFET_OVERTEMP_LIMIT_C);
    } else if (mosfet1OvertempActive && ntc1Valid && ntcMosfet1TempC <= MOSFET_OVERTEMP_RESET_C) {
      mosfet1OvertempActive = false;
      logf("MOSFET1 cooled down | temp=%.2fC | resume<=%.1fC", ntcMosfet1TempC, MOSFET_OVERTEMP_RESET_C);
    }

    if (!mosfet2OvertempActive && ntc2Valid && ntcMosfet2TempC >= MOSFET_OVERTEMP_LIMIT_C) {
      mosfet2OvertempActive = true;
      saveMosfetOvertempEvent(2U, ntcMosfet2TempC);
      logf("MOSFET2 overtemp TRIP | temp=%.2fC | limit=%.1fC | heater forced OFF", ntcMosfet2TempC,
           MOSFET_OVERTEMP_LIMIT_C);
    } else if (mosfet2OvertempActive && ntc2Valid && ntcMosfet2TempC <= MOSFET_OVERTEMP_RESET_C) {
      mosfet2OvertempActive = false;
      logf("MOSFET2 cooled down | temp=%.2fC | resume<=%.1fC", ntcMosfet2TempC, MOSFET_OVERTEMP_RESET_C);
    }

    if (mosfet1OvertempActive) {
      digitalWrite(SSR_PIN_1, HIGH);
    }
    if (mosfet2OvertempActive) {
      digitalWrite(SSR_PIN_2, HIGH);
    }

    static unsigned long lastNtcLogMs = 0;
    const bool overtempChanged = (prevOvertemp1 != mosfet1OvertempActive) || (prevOvertemp2 != mosfet2OvertempActive);
    if (overtempChanged || (now - lastNtcLogMs) >= 5000UL) {
      lastNtcLogMs = now;
      const String ntc1Text = ntc1Valid ? (String(ntcMosfet1TempC, 2) + "C") : "n/a";
      const String ntc2Text = ntc2Valid ? (String(ntcMosfet2TempC, 2) + "C") : "n/a";
      logf("MOSFET NTC | h1=%s (%u mV) | h2=%s (%u mV) | ot1=%d | ot2=%d", ntc1Text.c_str(), ntcMosfet1MilliVolts,
           ntc2Text.c_str(), ntcMosfet2MilliVolts, mosfet1OvertempActive ? 1 : 0, mosfet2OvertempActive ? 1 : 0);
    }

    // In non-manual modes, update ADC/battery state at 1 Hz for diagnostics.
    if (!manualMode) {
      adc1MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_1));
      adc2MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_2));
      logic_helpers::updateBatteryFromAdc(adc1MilliVolts, battery1CellCount, BATTERY_DIVIDER_RATIO, battery1PackVoltage,
                                          battery1CellVoltage, battery1SocPercent);
      logic_helpers::updateBatteryFromAdc(adc2MilliVolts, battery2CellCount, BATTERY_DIVIDER_RATIO, battery2PackVoltage,
                                          battery2CellVoltage, battery2SocPercent);
    }
    
    // Check for heater state changes (motor/vibration)
    const bool currentHeater1State = (digitalRead(SSR_PIN_1) == LOW);
    const bool currentHeater2State = (digitalRead(SSR_PIN_2) == LOW);
    
    if (currentHeater1State != lastHeater1State) {
      Serial.printf("Motor/Vibration H1: %s -> %s\n", 
                    lastHeater1State ? "ON" : "OFF",
                    currentHeater1State ? "ON" : "OFF");
      lastHeater1State = currentHeater1State;
    }
    
    if (currentHeater2State != lastHeater2State) {
      Serial.printf("Motor/Vibration H2: %s -> %s\n", 
                    lastHeater2State ? "ON" : "OFF",
                    currentHeater2State ? "ON" : "OFF");
      lastHeater2State = currentHeater2State;
    }
    
    // Check for signal pin state changes
    const bool currentSignalPinState = (digitalRead(SIGNAL_PIN) == HIGH);
    if (currentSignalPinState != lastSignalPinState) {
      Serial.printf("Signal pin: %s -> %s\n",
                    lastSignalPinState ? "ON" : "OFF",
                    currentSignalPinState ? "ON" : "OFF");
      lastSignalPinState = currentSignalPinState;
    }
    
    // Check for boot pin (input pin) state changes
    const bool currentInputPinState = (digitalRead(INPUT_PIN) == HIGH);
    if (currentInputPinState != lastInputPinState) {
      Serial.printf("Boot pin: %s -> %s\n",
                    lastInputPinState ? "HIGH" : "LOW",
                    currentInputPinState ? "HIGH" : "LOW");
      lastInputPinState = currentInputPinState;
    }
  }

  if (now - lastPrintMs >= 2000) {
    lastPrintMs = now;
    const String modeLabel =
        manualMode ? ("MANUAL H1 " + String(manualPowerPercent1) + "% / H2 " + String(manualPowerPercent2) + "%")
                   : (powerMode ? "POWER" : "NORMAL");
    const bool heater1On = (digitalRead(SSR_PIN_1) == LOW);
    const bool heater2On = (digitalRead(SSR_PIN_2) == LOW);

    constexpr unsigned long aliveHeartbeatMs = 30000UL;
    constexpr float tempDeltaLogC = 0.2F;
    constexpr float voltageDeltaLogV = 0.05F;
    constexpr uint16_t adcDeltaLogMv = 40U;

    static bool aliveSnapshotValid = false;
    static unsigned long lastAliveLogMs = 0;
    static String lastModeLabel;
    static float lastCurrentTemp1 = 0.0F;
    static float lastCurrentTemp2 = 0.0F;
    static float lastTargetTemp1 = 0.0F;
    static float lastTargetTemp2 = 0.0F;
    static bool lastSwapAssignment = false;
    static bool lastHeater1On = false;
    static bool lastHeater2On = false;
    static uint16_t lastAdc1MilliVolts = 0;
    static uint16_t lastAdc2MilliVolts = 0;
    static uint8_t lastBattery1CellCount = 0;
    static uint8_t lastBattery2CellCount = 0;
    static float lastBattery1PackVoltage = 0.0F;
    static float lastBattery1CellVoltage = 0.0F;
    static uint8_t lastBattery1SocPercent = 0;
    static float lastBattery2PackVoltage = 0.0F;
    static float lastBattery2CellVoltage = 0.0F;
    static uint8_t lastBattery2SocPercent = 0;

    const auto floatChanged = [](float current, float previous, float threshold) {
      if (std::isnan(current) != std::isnan(previous)) {
        return true;
      }
      if (std::isnan(current) && std::isnan(previous)) {
        return false;
      }
      return std::fabs(current - previous) >= threshold;
    };
    const auto adcChanged = [](uint16_t current, uint16_t previous) {
      return (current > previous) ? ((current - previous) >= adcDeltaLogMv) : ((previous - current) >= adcDeltaLogMv);
    };

    const bool stateChanged = !aliveSnapshotValid || (modeLabel != lastModeLabel) ||
                              floatChanged(currentTemp1, lastCurrentTemp1, tempDeltaLogC) ||
                              floatChanged(currentTemp2, lastCurrentTemp2, tempDeltaLogC) ||
                              floatChanged(targetTemp1, lastTargetTemp1, 0.1F) ||
                              floatChanged(targetTemp2, lastTargetTemp2, 0.1F) || (swapAssignment != lastSwapAssignment) ||
                              (heater1On != lastHeater1On) || (heater2On != lastHeater2On) ||
                              adcChanged(adc1MilliVolts, lastAdc1MilliVolts) ||
                              adcChanged(adc2MilliVolts, lastAdc2MilliVolts) ||
                              (battery1CellCount != lastBattery1CellCount) ||
                              (battery2CellCount != lastBattery2CellCount) ||
                              floatChanged(battery1PackVoltage, lastBattery1PackVoltage, voltageDeltaLogV) ||
                              floatChanged(battery1CellVoltage, lastBattery1CellVoltage, voltageDeltaLogV) ||
                              (battery1SocPercent != lastBattery1SocPercent) ||
                              floatChanged(battery2PackVoltage, lastBattery2PackVoltage, voltageDeltaLogV) ||
                              floatChanged(battery2CellVoltage, lastBattery2CellVoltage, voltageDeltaLogV) ||
                              (battery2SocPercent != lastBattery2SocPercent);
    const bool heartbeatDue = !aliveSnapshotValid || ((now - lastAliveLogMs) >= aliveHeartbeatMs);

    if (stateChanged || heartbeatDue) {
      ++counter;
      logf("alive %u | uptime_ms=%lu | heap=%u | mode=%s | t1=%.2f | t2=%.2f | target1=%.1f | target2=%.1f | swap=%d | h1=%s | h2=%s | adc1_mv=%u | adc2_mv=%u | batt1_cells=%u | batt1_v=%.2f | batt1_cell_v=%.2f | batt1_soc=%u | batt2_cells=%u | batt2_v=%.2f | batt2_cell_v=%.2f | batt2_soc=%u",
           counter, now, ESP.getFreeHeap(), modeLabel.c_str(), currentTemp1, currentTemp2, targetTemp1, targetTemp2,
           swapAssignment ? 1 : 0, heater1On ? "ON" : "OFF", heater2On ? "ON" : "OFF", adc1MilliVolts, adc2MilliVolts,
           battery1CellCount, battery1PackVoltage, battery1CellVoltage, battery1SocPercent, battery2CellCount,
           battery2PackVoltage, battery2CellVoltage, battery2SocPercent);
      aliveSnapshotValid = true;
      lastAliveLogMs = now;
      lastModeLabel = modeLabel;
      lastCurrentTemp1 = currentTemp1;
      lastCurrentTemp2 = currentTemp2;
      lastTargetTemp1 = targetTemp1;
      lastTargetTemp2 = targetTemp2;
      lastSwapAssignment = swapAssignment;
      lastHeater1On = heater1On;
      lastHeater2On = heater2On;
      lastAdc1MilliVolts = adc1MilliVolts;
      lastAdc2MilliVolts = adc2MilliVolts;
      lastBattery1CellCount = battery1CellCount;
      lastBattery2CellCount = battery2CellCount;
      lastBattery1PackVoltage = battery1PackVoltage;
      lastBattery1CellVoltage = battery1CellVoltage;
      lastBattery1SocPercent = battery1SocPercent;
      lastBattery2PackVoltage = battery2PackVoltage;
      lastBattery2CellVoltage = battery2CellVoltage;
      lastBattery2SocPercent = battery2SocPercent;
    }
  }

  if (now - lastRuntimeSaveMs >= 60000UL) {
    saveRuntimeMinute();
    lastRuntimeSaveMs = now;
  }

  if (now - lastMemCheckMs >= 30000UL) {
    const uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 30000) {
      logf("Memory warning: free_heap=%u", freeHeap);
    }
    if (freeHeap < 12000) {
      logLine("Memory critical, restarting...");
      delay(100);
      ESP.restart();
    }
    lastMemCheckMs = now;
  }

  dnsServer.processNextRequest();
}
