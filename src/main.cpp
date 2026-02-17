#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstdarg>
#include <cstdio>
#include <esp_system.h>

#include "app_state.h"
#include "control.h"
#include "storage.h"
#include "web_server.h"

using namespace HeatControl;

namespace {

constexpr size_t SERIAL_LOG_LIMIT = 12000;

void appendSerialLogLine(const String &line) {
  serialLogBuffer += line;
  serialLogBuffer += "\n";
  if (serialLogBuffer.length() > SERIAL_LOG_LIMIT) {
    serialLogBuffer.remove(0, serialLogBuffer.length() - SERIAL_LOG_LIMIT);
  }
}

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

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  EEPROM.begin(EEPROM_SIZE);

  pinMode(SSR_PIN_1, OUTPUT);
  pinMode(SSR_PIN_2, OUTPUT);
  pinMode(INPUT_PIN, INPUT);
  pinMode(SIGNAL_PIN, OUTPUT);

  digitalWrite(SSR_PIN_1, HIGH);
  digitalWrite(SSR_PIN_2, HIGH);
  digitalWrite(SIGNAL_PIN, LOW);
  
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
  manualMode = (sensors.getDeviceCount() == 0);
  if (manualMode) {
    powerMode = false;
    loadManualPowerPercent();
    if (digitalRead(INPUT_PIN) == HIGH) {
      cycleManualPowerPercent();
    }
  }

  startupSignal(powerMode, manualMode, manualPowerPercent);
  loadTemperatureTargets();
  loadSwapAssignment();
  loadWiFiCredentials();
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
  const String modeText = manualMode ? ("MANUAL " + String(manualPowerPercent) + "%")
                                     : (powerMode ? "POWER" : "NORMAL");
  logf("Detected mode: %s", modeText.c_str());
  logf("OneWire bus pin: GPIO%d", ONE_WIRE_BUS);
  logf("AP IP: %s", WiFi.softAPIP().toString().c_str());
  logf("AP SSID: %s", activeSsid.c_str());
  logf("LittleFS: %s", fileSystemReady ? "ready" : "not ready");
  logLine("HTTP: /, /status, /runtime, /setTemp, /swapSensors, /setWiFi, /restart, /resetRuntime, /update, /signalTest, /logs");
  logf("SSR1: %s | SSR2: %s", heaterStateText(SSR_PIN_1).c_str(), heaterStateText(SSR_PIN_2).c_str());
}

void loop() {
  const unsigned long now = millis();

  if (now - lastSensorMs >= 1000) {
    lastSensorMs = now;
    updateSensorsAndHeaters();
    
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
    ++counter;
    const String modeLabel =
        manualMode ? ("MANUAL " + String(manualPowerPercent) + "%") : (powerMode ? "POWER" : "NORMAL");
    logf("alive %u | uptime_ms=%lu | heap=%u | mode=%s | t1=%.2f | t2=%.2f | target1=%.1f | target2=%.1f | swap=%d | h1=%s | h2=%s",
         counter, now, ESP.getFreeHeap(), modeLabel.c_str(), currentTemp1, currentTemp2, targetTemp1, targetTemp2,
         swapAssignment ? 1 : 0, heaterStateText(SSR_PIN_1).c_str(), heaterStateText(SSR_PIN_2).c_str());
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
