#include "web_server.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>

#include "app_state.h"
#include "control.h"
#include "generated/embedded_files_registry.h"
#include "storage.h"

namespace HeatControl {

namespace {

bool otaUploadStarted = false;
bool otaUploadOk = false;
String otaUploadMessage;

String jsonEscape(const String &value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

bool sendEmbeddedFile(AsyncWebServerRequest *request, const String &path) {
  String normalized = path;
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }

  const EmbeddedFile *file = findEmbeddedFile(normalized.c_str());
  if (file == nullptr) {
    return false;
  }

  AsyncWebServerResponse *response = request->beginResponse(200, file->content_type, file->data, file->size);
  request->send(response);
  return true;
}

bool isFromLocalApSubnet(AsyncWebServerRequest *request) {
  if (request == nullptr || request->client() == nullptr) {
    return false;
  }
  const IPAddress ip = request->client()->remoteIP();
  return ip[0] == 4 && ip[1] == 3 && ip[2] == 2;
}

class CaptiveRequestHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) {
    const String host = request->host();
    if (host == WiFi.softAPIP().toString() || host == "4.3.2.1") {
      return false;
    }
    return host.length() > 0;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", "http://4.3.2.1");
    request->send(response);
  }
};

}  // namespace

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (sendEmbeddedFile(request, "/index.html")) {
      return;
    }

    if (fileSystemReady && LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
      return;
    }
    request->send(500, "text/plain", "Web UI not available (missing embedded /index.html).");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    const float displayTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
    const float displayTemp2 = swapAssignment ? currentTemp1 : currentTemp2;
    const uint32_t currentSessionSeconds = static_cast<uint32_t>((millis() - startTimeMs) / 1000UL);
    const String modeText = manualMode ? ("MANUAL H1 " + String(manualPowerPercent1) + "% / H2 " + String(manualPowerPercent2) + "%")
                                       : (powerMode ? "POWER" : "NORMAL");
    const String bootPinText = (digitalRead(INPUT_PIN) == HIGH) ? "HIGH" : "LOW";
    String json = "{\"mode\":\"" + modeText + "\"" +
                  ",\"manualMode\":" + String(manualMode ? 1 : 0) +
                  ",\"manualPercent1\":" + String(manualPowerPercent1) +
                  ",\"manualPercent2\":" + String(manualPowerPercent2) +
                  ",\"manualH1Enabled\":" + String(manualHeater1Enabled ? 1 : 0) +
                  ",\"manualH2Enabled\":" + String(manualHeater2Enabled ? 1 : 0) +
                  ",\"bootPin\":\"" + bootPinText + "\"" +
                  ",\"adc1Mv\":" + String(adc1MilliVolts) +
                  ",\"adc2Mv\":" + String(adc2MilliVolts) +
                  ",\"batt1Cells\":" + String(battery1CellCount) +
                  ",\"batt1V\":" + String(battery1PackVoltage, 2) +
                  ",\"batt1CellV\":" + String(battery1CellVoltage, 2) +
                  ",\"batt1Soc\":" + String(battery1SocPercent) +
                  ",\"batt2Cells\":" + String(battery2CellCount) +
                  ",\"batt2V\":" + String(battery2PackVoltage, 2) +
                  ",\"batt2CellV\":" + String(battery2CellVoltage, 2) +
                  ",\"batt2Soc\":" + String(battery2SocPercent) +
                  ",\"manualToggleMaxOffMs\":" + String(manualPowerToggleMaxOffMs) +
                  ",\"current1\":" + String(displayTemp1, 2) +
                  ",\"current2\":" + String(displayTemp2, 2) +
                  ",\"target1\":" + String(targetTemp1, 1) +
                  ",\"target2\":" + String(targetTemp2, 1) +
                  ",\"swap\":" + String(swapAssignment ? 1 : 0) +
                  ",\"ssid\":\"" + jsonEscape(activeSsid) + "\"" +
                  ",\"h1\":" + String(digitalRead(SSR_PIN_1) == LOW ? 1 : 0) +
                  ",\"h2\":" + String(digitalRead(SSR_PIN_2) == LOW ? 1 : 0) +
                  ",\"totalRuntime\":\"" + formatRuntime(savedRuntimeMinutes * 60UL, false) + "\"" +
                  ",\"currentRuntime\":\"" + formatRuntime(currentSessionSeconds, true) + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/setBattery1", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (request->hasParam("cells", true)) {
      battery1CellCount = clampBatteryCellCount(static_cast<uint8_t>(request->getParam("cells", true)->value().toInt()));
      saveBatteryCellCounts();
    }
    request->redirect("/");
  });

  server.on("/setBattery2", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (request->hasParam("cells", true)) {
      battery2CellCount = clampBatteryCellCount(static_cast<uint8_t>(request->getParam("cells", true)->value().toInt()));
      saveBatteryCellCounts();
    }
    request->redirect("/");
  });

  server.on("/setManualToggle", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (request->hasParam("windowMs", true)) {
      const uint16_t value =
          static_cast<uint16_t>(request->getParam("windowMs", true)->value().toInt());
      manualPowerToggleMaxOffMs = clampManualToggleOffMs(value);
      saveManualToggleOffMs();
    }
    request->redirect("/");
  });

  server.on("/runtime", HTTP_GET, [](AsyncWebServerRequest *request) {
    const uint32_t currentSessionSeconds = static_cast<uint32_t>((millis() - startTimeMs) / 1000UL);
    String json = "{\"total\":\"" + formatRuntime(savedRuntimeMinutes * 60UL, false) +
                  "\",\"current\":\"" + formatRuntime(currentSessionSeconds, true) + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/setTemp", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (request->hasParam("temp1", true)) {
      targetTemp1 = clampTarget(request->getParam("temp1", true)->value().toFloat());
    }
    if (request->hasParam("temp2", true)) {
      targetTemp2 = clampTarget(request->getParam("temp2", true)->value().toFloat());
    }
    saveTemperatureTargets();
    request->redirect("/");
  });

  server.on("/swapSensors", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    swapAssignment = request->hasParam("swap", true);
    saveSwapAssignment();
    request->redirect("/");
  });

  server.on("/setWiFi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (request->hasParam("ssid", true)) {
      const String newSsid = request->getParam("ssid", true)->value();
      const String newPassword = request->hasParam("password", true)
                                     ? request->getParam("password", true)->value()
                                     : String();
      saveWiFiCredentials(newSsid, newPassword);
    }
    request->send(200, "text/plain", "WiFi settings saved. Rebooting...");
    delay(150);
    ESP.restart();
  });

  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (request->hasParam("mode", true)) {
      const String mode = request->getParam("mode", true)->value();
      if (mode == "power") {
        setNextBootMode(BOOT_MODE_POWER);
      } else if (mode == "normal") {
        setNextBootMode(BOOT_MODE_NORMAL);
      }
    }
    request->send(200, "text/plain", "Rebooting...");
    delay(150);
    ESP.restart();
  });

  server.on("/signalTest", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    digitalWrite(SIGNAL_PIN, HIGH);
    delay(120);
    digitalWrite(SIGNAL_PIN, LOW);
    request->send(200, "text/plain", "OK");
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    request->send(200, "text/plain; charset=utf-8", String(serialLogBuffer));
  });

  server.on("/resetRuntime", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    savedRuntimeMinutes = 0;
    writeRuntimeToEeprom(0);
    startTimeMs = millis();
    request->redirect("/");
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    String page;
    page.reserve(1800);
    page += F(
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>HeatControl OTA</title><style>"
        "body{font-family:Arial,Helvetica,sans-serif;background:#1d1f24;color:#fff;margin:0;padding:16px}"
        ".box{max-width:600px;margin:0 auto;background:#2b2f36;padding:16px;border-radius:10px}"
        "input[type=file]{margin:10px 0;color:#fff}"
        "input[type=submit],a{background:#c0392b;color:#fff;border:0;padding:10px 14px;border-radius:6px;text-decoration:none;display:inline-block}"
        ".muted{color:#bdbdbd;font-size:.9rem}"
        "</style></head><body><div class='box'><h2>Firmware Update (OTA)</h2>"
        "<p class='muted'>Select <b>firmware.bin</b> from a HeatControl release and upload it.</p>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='firmware' accept='.bin' required><br>"
        "<input type='submit' value='Upload and Flash'>"
        "</form><br><a href='/'>Back</a></div></body></html>");
    request->send(200, "text/html", page);
  });

  server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!isFromLocalApSubnet(request)) {
          request->send(403, "text/plain", "Forbidden");
          return;
        }
        if (!otaUploadStarted) {
          request->send(400, "text/plain", "No upload received.");
          return;
        }

        if (!otaUploadOk) {
          const String msg = otaUploadMessage.isEmpty() ? String("Update failed.") : otaUploadMessage;
          otaUploadStarted = false;
          otaUploadOk = false;
          otaUploadMessage = "";
          request->send(500, "text/plain", msg);
          return;
        }

        otaUploadStarted = false;
        otaUploadOk = false;
        otaUploadMessage = "";
        request->send(200, "text/plain", "Update successful. Device will reboot.");
        delay(300);
        ESP.restart();
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!isFromLocalApSubnet(request)) {
          return;
        }
        if (index == 0) {
          otaUploadStarted = true;
          otaUploadOk = false;
          otaUploadMessage = "";

          if (!filename.endsWith(".bin")) {
            otaUploadMessage = "Only .bin firmware files are accepted.";
            return;
          }

          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            otaUploadMessage = "Update begin failed. Error code: " + String(Update.getError());
            return;
          }
        }

        if (!otaUploadMessage.isEmpty()) {
          return;
        }

        const size_t written = Update.write(data, len);
        if (written != len) {
          otaUploadMessage = "Write failed. Error code: " + String(Update.getError());
          return;
        }

        if (final) {
          if (!Update.end(true)) {
            otaUploadMessage = "Finalize failed. Error code: " + String(Update.getError());
            return;
          }
          otaUploadOk = true;
          otaUploadMessage = "OK";
        }
      });

  // Handle Windows Captive Portal Detection requests to prevent LittleFS errors
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
  server.on("/connecttest.txt.gz", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
  server.on("/connecttest.txt/index.htm", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
  server.on("/connecttest.txt/index.htm.gz", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (sendEmbeddedFile(request, request->url())) {
      return;
    }

    if (fileSystemReady && LittleFS.exists(request->url())) {
      request->send(LittleFS, request->url(), String(), false);
      return;
    }

    request->send(404, "text/plain", "Not found");
  });

  server.addHandler(new CaptiveRequestHandler());
  DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  if (fileSystemReady) {
    server.serveStatic("/", LittleFS, "/");
  }

  server.begin();
}

}  // namespace HeatControl
