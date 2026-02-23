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
constexpr char DEFAULT_WIFI_SSID_FALLBACK[] = "HeatControl";
constexpr char DEFAULT_WIFI_PASSWORD_FALLBACK[] = "HeatControl";

BatteryToggleDetector battery1Detector(BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV, BATTERY_STABLE_SAMPLES);
BatteryToggleDetector battery2Detector(BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV, BATTERY_STABLE_SAMPLES);
const IPAddress AP_IP(4, 3, 2, 1);
const IPAddress AP_NETMASK(255, 255, 255, 0);
constexpr uint8_t AP_CHANNEL = 1;

const char *wifiModeToText(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_OFF:
      return "OFF";
    case WIFI_STA:
      return "STA";
    case WIFI_AP:
      return "AP";
    case WIFI_AP_STA:
      return "AP_STA";
    default:
      return "UNKNOWN";
  }
}

uint32_t apAutoOffTimeoutMs() {
  return static_cast<uint32_t>(apAutoOffMinutes) * 60000UL;
}

void stopApMode() {
  if (!apEnabled) {
    return;
  }
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apEnabled = false;
  logLine("AP mode disabled.");
}

void disableAllWifiRadios() {
  if (wifiRadiosDisabled) {
    return;
  }
  stopApMode();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  staConnected = false;
  wifiRadiosDisabled = true;
  logLine("WiFi radios disabled (AP timeout reached without STA connection).");
}

void handleWifiLifetime(unsigned long now) {
  if (wifiRadiosDisabled || apAutoOffMinutes == 0U) {
    return;
  }

  const uint32_t timeoutMs = apAutoOffTimeoutMs();
  if (timeoutMs == 0U || (now - wifiStartupMs) < timeoutMs) {
    return;
  }

  if (staConnected) {
    stopApMode();
    return;
  }

  disableAllWifiRadios();
}

void appendSerialLogLine(const char *line) {
  if (line == nullptr) {
    return;
  }
  char lineBuf[320];
  const int written = snprintf(lineBuf, sizeof(lineBuf), "%s\n", line);
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

const char *logLevelToText(LogLevel level) {
  switch (level) {
    case LogLevel::Error:
      return "ERROR";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Debug:
      return "DEBUG";
    default:
      return "INFO";
  }
}

LogLevel parseLogLevel(const String &value, bool *ok) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();
  if (normalized == "0" || normalized == "error") {
    if (ok != nullptr) {
      *ok = true;
    }
    return LogLevel::Error;
  }
  if (normalized == "1" || normalized == "info") {
    if (ok != nullptr) {
      *ok = true;
    }
    return LogLevel::Info;
  }
  if (normalized == "2" || normalized == "debug") {
    if (ok != nullptr) {
      *ok = true;
    }
    return LogLevel::Debug;
  }
  if (ok != nullptr) {
    *ok = false;
  }
  return currentLogLevel;
}

void setLogLevel(LogLevel level) {
  currentLogLevel = level;
}

bool shouldLog(LogLevel level) {
  return static_cast<uint8_t>(level) <= static_cast<uint8_t>(currentLogLevel);
}

void logLine(const char *line, LogLevel level) {
  if (!shouldLog(level) || line == nullptr) {
    return;
  }
  Serial.println(line);
  appendSerialLogLine(line);
}

void logLine(const String &line, LogLevel level) {
  if (!shouldLog(level)) {
    return;
  }
  Serial.println(line);
  appendSerialLogLine(line.c_str());
}

void logf(LogLevel level, const char *fmt, ...) {
  if (!shouldLog(level) || fmt == nullptr) {
    return;
  }
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  Serial.println(buffer);
  appendSerialLogLine(buffer);
}

void logf(const char *fmt, ...) {
  if (!shouldLog(LogLevel::Info) || fmt == nullptr) {
    return;
  }
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  Serial.println(buffer);
  appendSerialLogLine(buffer);
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

  digitalWrite(SSR_PIN_1, LOW);
  digitalWrite(SSR_PIN_2, LOW);
  digitalWrite(SIGNAL_PIN, HIGH);

  // ADC setup (ESP32-C3): 12-bit readings, extended input range.
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN_1, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_2, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_NTC_MOSFET_1, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_NTC_MOSFET_2, ADC_11db);
  
  // Initialize state tracking
  lastHeater1State = (digitalRead(SSR_PIN_1) == HIGH);
  lastHeater2State = (digitalRead(SSR_PIN_2) == HIGH);
  lastSignalPinState = (digitalRead(SIGNAL_PIN) == LOW);
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
  loadApCredentials();
  loadApAutoOffMinutes();
  loadLogLevel();
  loadBatteryCellCounts();
  loadBatteryChemistries();
  loadManualToggleOffMs();
  loadMosfetOvertempEvents();
  logf("Manual toggle window: %u ms (min=100, max=5000)", manualPowerToggleMaxOffMs);
  logf("AP auto-off timeout: %u min (0=disabled)", apAutoOffMinutes);
  loadSavedRuntime();

  if (activeApSsid.isEmpty()) {
    activeApSsid = "HeatControl";
    logLine("AP SSID was empty, fallback to default.");
  }
  if (activeApPassword.isEmpty()) {
    activeApPassword = "HeatControl";
    logLine("AP password was empty, fallback to default.");
  }

  startTimeMs = millis();
  lastRuntimeSaveMs = millis();

  fileSystemReady = LittleFS.begin(true);

  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
  apEnabled = WiFi.softAP(activeApSsid.c_str(), activeApPassword.c_str(), AP_CHANNEL, false, AP_MAX_CLIENTS);
  wifiRadiosDisabled = false;
  wifiStartupMs = millis();
  if (apEnabled) {
    logf("AP start ok | ssid=%s | ip=%s | channel=%u | max_clients=%u", activeApSsid.c_str(),
         WiFi.softAPIP().toString().c_str(), static_cast<unsigned int>(AP_CHANNEL), static_cast<unsigned int>(AP_MAX_CLIENTS));
  } else {
    logf("AP start failed | ssid=%s | wifi_mode=%s", activeApSsid.c_str(), wifiModeToText(WiFi.getMode()));
  }
  const bool hasDefaultStaCredentials =
      (activeSsid == DEFAULT_WIFI_SSID_FALLBACK && activePassword == DEFAULT_WIFI_PASSWORD_FALLBACK);
  if (!activeSsid.isEmpty() && !hasDefaultStaCredentials) {
    WiFi.begin(activeSsid.c_str(), activePassword.c_str());
    logf("STA connect attempt: ssid=%s", activeSsid.c_str());
  } else if (hasDefaultStaCredentials) {
    logLine("STA connect skipped: default credentials active. AP-only until STA settings are changed.");
  }
  WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_AP_START:
        logf("WiFi event: AP started | ssid=%s | ip=%s", activeApSsid.c_str(), WiFi.softAPIP().toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_AP_STOP:
        logLine("WiFi event: AP stopped.");
        break;
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", info.wifi_ap_staconnected.mac[0],
                 info.wifi_ap_staconnected.mac[1], info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                 info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
        logf("AP client connected: %s | aid=%d", mac, info.wifi_ap_staconnected.aid);
        break;
      }
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", info.wifi_ap_stadisconnected.mac[0],
                 info.wifi_ap_stadisconnected.mac[1], info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                 info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
        logf("AP client disconnected: %s | aid=%d", mac, info.wifi_ap_stadisconnected.aid);
        break;
      }
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        staConnected = true;
        logf("STA connected: ip=%s", WiFi.localIP().toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        staConnected = false;
        logf("STA disconnected: reason=%d", static_cast<int>(info.wifi_sta_disconnected.reason));
        if (info.wifi_sta_disconnected.reason == 201) {
          logLine("STA reason 201: network not found. STA retries in background, AP stays independent.");
        }
        break;
      default:
        break;
    }
  });

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  if (apEnabled) {
    dnsServer.start(53, "*", AP_IP);
  }

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
  logf("AP SSID: %s", activeApSsid.c_str());
  logf("Configured STA SSID: %s", activeSsid.c_str());
  logf("LittleFS: %s", fileSystemReady ? "ready" : "not ready");
  logLine("HTTP: /, /status, /runtime, /setTemp, /setLogLevel, /setApEnabled, /saveSettings, /swapSensors, /setWiFi, /restart, /resetRuntime, /update, /signalTest, /logs");
  logf("SSR1: %s | SSR2: %s", heaterStateText(SSR_PIN_1).c_str(), heaterStateText(SSR_PIN_2).c_str());
}

void loop() {
  const unsigned long now = millis();

  if (restartScheduled && (static_cast<long>(now - restartAtMs) >= 0)) {
    restartScheduled = false;
    if (pendingTempPersist) {
      saveTemperatureTargets();
      pendingTempPersist = false;
      logf(LogLevel::Info, "Pending temperature targets persisted before restart.");
    }
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
                                        battery1CellVoltage, battery1Chemistry, battery1SocSmoothed,
                                        battery1SocSmoothingInitialized, battery1SocPercent);
    logic_helpers::updateBatteryFromAdc(adc2MilliVolts, battery2CellCount, BATTERY_DIVIDER_RATIO, battery2PackVoltage,
                                        battery2CellVoltage, battery2Chemistry, battery2SocSmoothed,
                                        battery2SocSmoothingInitialized, battery2SocPercent);

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
      logf(LogLevel::Debug, "Batt1OffNow changed -> %d | adc1_mv=%u (off_thresh=%u, on_thresh=%u)", batt1OffNow ? 1 : 0,
           adc1MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }
    if (batt1Sample.onEdge) {
      logf(LogLevel::Debug, "Batt1OnNow changed  -> %d | adc1_mv=%u (off_thresh=%u, on_thresh=%u)", batt1OnNow ? 1 : 0,
           adc1MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }
    if (batt2Sample.offEdge) {
      logf(LogLevel::Debug, "Batt2OffNow changed -> %d | adc2_mv=%u (off_thresh=%u, on_thresh=%u)", batt2OffNow ? 1 : 0,
           adc2MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }
    if (batt2Sample.onEdge) {
      logf(LogLevel::Debug, "Batt2OnNow changed  -> %d | adc2_mv=%u (off_thresh=%u, on_thresh=%u)", batt2OnNow ? 1 : 0,
           adc2MilliVolts, BATTERY_ADC_OFF_THRESHOLD_MV, BATTERY_ADC_ON_THRESHOLD_MV);
    }

    // Battery switch detection: treat an OFF period as a user trigger to cycle the manual PWM step
    // only if power returns within manualPowerToggleMaxOffMs.
    if (batt1OffNow && !battery1Off) {
      battery1Off = true;
      battery1OffSinceMs = now;
      logf(LogLevel::Debug, "Battery 1 OFF edge detected | now_ms=%lu | adc1_mv=%u", now, adc1MilliVolts);
    } else if (!batt1OffNow && batt1OnNow && battery1Off) {
      const unsigned long offMs = now - battery1OffSinceMs;
      battery1Off = false;
      logf(LogLevel::Debug, "Battery 1 ON edge detected  | off_ms=%lu (limit=%u) | adc1_mv=%u", offMs,
           static_cast<unsigned int>(manualPowerToggleMaxOffMs), adc1MilliVolts);
      if (offMs <= static_cast<unsigned long>(manualPowerToggleMaxOffMs)) {
        cycleManualPowerPercent1();
        logf(LogLevel::Info, "Battery 1 OFF/ON trigger (%lums) -> manual power 1 = %u%%", offMs, manualPowerPercent1);
        // Haptic feedback for manual heater 1 power change.
        signalManualPowerChange(manualPowerPercent1);
      } else {
        logf(LogLevel::Debug, "Battery 1 OFF/ON ignored (off_ms too long for toggle window)");
      }
    }

    if (batt2OffNow && !battery2Off) {
      battery2Off = true;
      battery2OffSinceMs = now;
      logf(LogLevel::Debug, "Battery 2 OFF edge detected | now_ms=%lu | adc2_mv=%u", now, adc2MilliVolts);
    } else if (!batt2OffNow && batt2OnNow && battery2Off) {
      const unsigned long offMs = now - battery2OffSinceMs;
      battery2Off = false;
      logf(LogLevel::Debug, "Battery 2 ON edge detected  | off_ms=%lu (limit=%u) | adc2_mv=%u", offMs,
           static_cast<unsigned int>(manualPowerToggleMaxOffMs), adc2MilliVolts);
      if (offMs <= static_cast<unsigned long>(manualPowerToggleMaxOffMs)) {
        cycleManualPowerPercent2();
        logf(LogLevel::Info, "Battery 2 OFF/ON trigger (%lums) -> manual power 2 = %u%%", offMs, manualPowerPercent2);
        // Haptic feedback for manual heater 2 power change.
        signalManualPowerChange(manualPowerPercent2);
      } else {
        logf(LogLevel::Debug, "Battery 2 OFF/ON ignored (off_ms too long for toggle window)");
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
      digitalWrite(SSR_PIN_1, LOW);
    }
    if (mosfet2OvertempActive) {
      digitalWrite(SSR_PIN_2, LOW);
    }

    static unsigned long lastNtcLogMs = 0;
    const bool overtempChanged = (prevOvertemp1 != mosfet1OvertempActive) || (prevOvertemp2 != mosfet2OvertempActive);
    if (overtempChanged || (now - lastNtcLogMs) >= 5000UL) {
      lastNtcLogMs = now;
      char ntc1Text[16];
      char ntc2Text[16];
      if (ntc1Valid) {
        snprintf(ntc1Text, sizeof(ntc1Text), "%.2fC", ntcMosfet1TempC);
      } else {
        snprintf(ntc1Text, sizeof(ntc1Text), "n/a");
      }
      if (ntc2Valid) {
        snprintf(ntc2Text, sizeof(ntc2Text), "%.2fC", ntcMosfet2TempC);
      } else {
        snprintf(ntc2Text, sizeof(ntc2Text), "n/a");
      }
      logf(LogLevel::Debug, "MOSFET NTC | h1=%s (%u mV) | h2=%s (%u mV) | ot1=%d | ot2=%d", ntc1Text, ntcMosfet1MilliVolts,
           ntc2Text, ntcMosfet2MilliVolts, mosfet1OvertempActive ? 1 : 0, mosfet2OvertempActive ? 1 : 0);
    }

    // In non-manual modes, update ADC/battery state at 1 Hz for diagnostics.
    if (!manualMode) {
      adc1MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_1));
      adc2MilliVolts = static_cast<uint16_t>(analogReadMilliVolts(ADC_PIN_2));
      logic_helpers::updateBatteryFromAdc(adc1MilliVolts, battery1CellCount, BATTERY_DIVIDER_RATIO, battery1PackVoltage,
                                          battery1CellVoltage, battery1Chemistry, battery1SocSmoothed,
                                          battery1SocSmoothingInitialized, battery1SocPercent);
      logic_helpers::updateBatteryFromAdc(adc2MilliVolts, battery2CellCount, BATTERY_DIVIDER_RATIO, battery2PackVoltage,
                                          battery2CellVoltage, battery2Chemistry, battery2SocSmoothed,
                                          battery2SocSmoothingInitialized, battery2SocPercent);
    }
    
    // Check for heater state changes (motor/vibration)
    const bool currentHeater1State = (digitalRead(SSR_PIN_1) == HIGH);
    const bool currentHeater2State = (digitalRead(SSR_PIN_2) == HIGH);
    
    if (currentHeater1State != lastHeater1State) {
      logf(LogLevel::Debug, "Motor/Vibration H1: %s -> %s", lastHeater1State ? "ON" : "OFF", currentHeater1State ? "ON" : "OFF");
      lastHeater1State = currentHeater1State;
    }
    
    if (currentHeater2State != lastHeater2State) {
      logf(LogLevel::Debug, "Motor/Vibration H2: %s -> %s", lastHeater2State ? "ON" : "OFF", currentHeater2State ? "ON" : "OFF");
      lastHeater2State = currentHeater2State;
    }
    
    // Check for signal pin state changes
    const bool currentSignalPinState = (digitalRead(SIGNAL_PIN) == LOW);
    if (currentSignalPinState != lastSignalPinState) {
      logf(LogLevel::Debug, "Signal pin: %s -> %s", lastSignalPinState ? "ON" : "OFF", currentSignalPinState ? "ON" : "OFF");
      lastSignalPinState = currentSignalPinState;
    }
    
    // Check for boot pin (input pin) state changes
    const bool currentInputPinState = (digitalRead(INPUT_PIN) == HIGH);
    if (currentInputPinState != lastInputPinState) {
      logf(LogLevel::Debug, "Boot pin: %s -> %s", lastInputPinState ? "HIGH" : "LOW", currentInputPinState ? "HIGH" : "LOW");
      lastInputPinState = currentInputPinState;
    }
  }

  if (now - lastPrintMs >= 2000) {
    lastPrintMs = now;
    const String modeLabel =
        manualMode ? ("MANUAL H1 " + String(manualPowerPercent1) + "% / H2 " + String(manualPowerPercent2) + "%")
                   : (powerMode ? "POWER" : "NORMAL");
    const bool heater1On = (digitalRead(SSR_PIN_1) == HIGH);
    const bool heater2On = (digitalRead(SSR_PIN_2) == HIGH);

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
      logf(LogLevel::Debug, "alive %u | uptime_ms=%lu | heap=%u | mode=%s | t1=%.2f | t2=%.2f | target1=%.1f | target2=%.1f | swap=%d | h1=%s | h2=%s | adc1_mv=%u | adc2_mv=%u | batt1_cells=%u | batt1_v=%.2f | batt1_cell_v=%.2f | batt1_soc=%u | batt2_cells=%u | batt2_v=%.2f | batt2_cell_v=%.2f | batt2_soc=%u",
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

  if (pendingTempPersist && static_cast<long>(now - pendingTempPersistAtMs) >= 0) {
    saveTemperatureTargets();
    pendingTempPersist = false;
    logf(LogLevel::Debug, "Temperature targets persisted to EEPROM.");
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

  static unsigned long lastWifiDiagMs = 0;
  if ((now - lastWifiDiagMs) >= 20000UL) {
    const wifi_mode_t mode = WiFi.getMode();
    const bool modeHasAp = (mode == WIFI_AP || mode == WIFI_AP_STA);
    if (!wifiRadiosDisabled && !modeHasAp) {
      logf(LogLevel::Debug, "WIFI diag: AP missing in mode | mode=%s | ap_flag=%d | sta_status=%d | ap_timeout_min=%u",
           wifiModeToText(mode), apEnabled ? 1 : 0, static_cast<int>(WiFi.status()), static_cast<unsigned int>(apAutoOffMinutes));
    } else if (apEnabled) {
      logf(LogLevel::Debug, "WIFI diag: AP active | ssid=%s | ip=%s | stations=%u | mode=%s", activeApSsid.c_str(),
           WiFi.softAPIP().toString().c_str(), static_cast<unsigned int>(WiFi.softAPgetStationNum()), wifiModeToText(mode));
    }
    lastWifiDiagMs = now;
  }

  staConnected = (WiFi.status() == WL_CONNECTED);
  handleWifiLifetime(now);

  if (apEnabled) {
    dnsServer.processNextRequest();
  }
}
