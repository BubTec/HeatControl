#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>

#include "app_state.h"
#include "control.h"
#include "storage.h"
#include "web_server.h"

using namespace HeatControl;

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

  const uint8_t savedBootMode = getAndClearBootMode();
  if (savedBootMode == BOOT_MODE_NORMAL) {
    powerMode = false;
  } else if (savedBootMode == BOOT_MODE_POWER) {
    powerMode = true;
  } else {
    powerMode = (digitalRead(INPUT_PIN) == HIGH);
  }

  startupSignal(powerMode);
  sensors.begin();
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
    Serial.print("AP client connected: ");
    for (int i = 0; i < 6; ++i) {
      Serial.printf("%02X", info.wifi_ap_staconnected.mac[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.printf(" | aid=%d\n", info.wifi_ap_staconnected.aid);
  });

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", IPAddress(4, 3, 2, 1));

  setupWebServer();

  Serial.println();
  Serial.println("=== ESP32-C3 modular setup test ===");
  Serial.printf("Reset reason: %d\n", static_cast<int>(esp_reset_reason()));
  Serial.printf("Detected mode: %s\n", powerMode ? "POWER" : "NORMAL");
  Serial.printf("OneWire bus pin: GPIO%d\n", ONE_WIRE_BUS);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP SSID: %s\n", activeSsid.c_str());
  Serial.printf("LittleFS: %s\n", fileSystemReady ? "ready" : "not ready");
  Serial.println("HTTP: /, /status, /runtime, /setTemp, /swapSensors, /setWiFi, /restart, /resetRuntime, /update");
  Serial.printf("SSR1: %s | SSR2: %s\n",
                heaterStateText(SSR_PIN_1).c_str(),
                heaterStateText(SSR_PIN_2).c_str());
}

void loop() {
  const unsigned long now = millis();

  if (now - lastSensorMs >= 1000) {
    lastSensorMs = now;
    updateSensorsAndHeaters();
  }

  if (now - lastPrintMs >= 2000) {
    lastPrintMs = now;
    ++counter;
    Serial.printf("alive %u | uptime_ms=%lu | heap=%u | mode=%s | t1=%.2f | t2=%.2f | target1=%.1f | target2=%.1f | swap=%d | h1=%s | h2=%s\n",
                  counter,
                  now,
                  ESP.getFreeHeap(),
                  powerMode ? "POWER" : "NORMAL",
                  currentTemp1,
                  currentTemp2,
                  targetTemp1,
                  targetTemp2,
                  swapAssignment ? 1 : 0,
                  heaterStateText(SSR_PIN_1).c_str(),
                  heaterStateText(SSR_PIN_2).c_str());
  }

  if (now - lastRuntimeSaveMs >= 60000UL) {
    saveRuntimeMinute();
    lastRuntimeSaveMs = now;
  }

  if (now - lastMemCheckMs >= 30000UL) {
    const uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 30000) {
      Serial.printf("Memory warning: free_heap=%u\n", freeHeap);
    }
    if (freeHeap < 12000) {
      Serial.println("Memory critical, restarting...");
      delay(100);
      ESP.restart();
    }
    lastMemCheckMs = now;
  }

  dnsServer.processNextRequest();
}
