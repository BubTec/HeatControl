#include "web_server.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include <cmath>

#include "app_state.h"
#include "control.h"
#include "generated/embedded_files_registry.h"
#include "storage.h"

namespace HeatControl {

namespace {

bool otaUploadStarted = false;
bool otaUploadOk = false;
String otaUploadMessage;
size_t otaUploadBytes = 0;

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

void sendCaptiveRedirect(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
  response->addHeader("Location", "http://4.3.2.1/");
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  request->send(response);
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
    sendCaptiveRedirect(request);
  }
};

void scheduleRestart(uint32_t delayMs) {
  restartScheduled = true;
  restartAtMs = millis() + delayMs;
}

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
    const String ntc1TempJson = std::isnan(ntcMosfet1TempC) ? "null" : String(ntcMosfet1TempC, 2);
    const String ntc2TempJson = std::isnan(ntcMosfet2TempC) ? "null" : String(ntcMosfet2TempC, 2);
    const String trip1TempJson = std::isnan(mosfet1OvertempTripTempC) ? "null" : String(mosfet1OvertempTripTempC, 2);
    const String trip2TempJson = std::isnan(mosfet2OvertempTripTempC) ? "null" : String(mosfet2OvertempTripTempC, 2);
    String json = "{\"mode\":\"" + modeText + "\"" +
                  ",\"manualMode\":" + String(manualMode ? 1 : 0) +
                  ",\"manualPercent1\":" + String(manualPowerPercent1) +
                  ",\"manualPercent2\":" + String(manualPowerPercent2) +
                  ",\"manualH1Enabled\":" + String(manualHeater1Enabled ? 1 : 0) +
                  ",\"manualH2Enabled\":" + String(manualHeater2Enabled ? 1 : 0) +
                  ",\"bootPin\":\"" + bootPinText + "\"" +
                  ",\"adc1Mv\":" + String(adc1MilliVolts) +
                  ",\"adc2Mv\":" + String(adc2MilliVolts) +
                  ",\"ntcMosfet1Mv\":" + String(ntcMosfet1MilliVolts) +
                  ",\"ntcMosfet2Mv\":" + String(ntcMosfet2MilliVolts) +
                  ",\"ntcMosfet1C\":" + ntc1TempJson +
                  ",\"ntcMosfet2C\":" + ntc2TempJson +
                  ",\"mosfet1OvertempActive\":" + String(mosfet1OvertempActive ? 1 : 0) +
                  ",\"mosfet2OvertempActive\":" + String(mosfet2OvertempActive ? 1 : 0) +
                  ",\"mosfet1OvertempLatched\":" + String(mosfet1OvertempLatched ? 1 : 0) +
                  ",\"mosfet2OvertempLatched\":" + String(mosfet2OvertempLatched ? 1 : 0) +
                  ",\"mosfet1OvertempTripC\":" + trip1TempJson +
                  ",\"mosfet2OvertempTripC\":" + trip2TempJson +
                  ",\"mosfetOvertempLimitC\":" + String(MOSFET_OVERTEMP_LIMIT_C, 1) +
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

  server.on("/saveSettings", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }

    bool tempChanged = false;
    bool swapChanged = false;
    bool manualWindowChanged = false;
    bool batteryChanged = false;

    if (request->hasParam("temp1", true)) {
      targetTemp1 = clampTarget(request->getParam("temp1", true)->value().toFloat());
      tempChanged = true;
    }
    if (request->hasParam("temp2", true)) {
      targetTemp2 = clampTarget(request->getParam("temp2", true)->value().toFloat());
      tempChanged = true;
    }

    if (request->hasParam("swap", true)) {
      String swapRaw = request->getParam("swap", true)->value();
      swapRaw.toLowerCase();
      swapAssignment = (swapRaw == "1" || swapRaw == "true" || swapRaw == "on" || swapRaw == "yes");
      swapChanged = true;
    }

    if (request->hasParam("windowMs", true)) {
      const uint16_t value = static_cast<uint16_t>(request->getParam("windowMs", true)->value().toInt());
      manualPowerToggleMaxOffMs = clampManualToggleOffMs(value);
      manualWindowChanged = true;
    }

    if (request->hasParam("batt1Cells", true)) {
      battery1CellCount = clampBatteryCellCount(static_cast<uint8_t>(request->getParam("batt1Cells", true)->value().toInt()));
      batteryChanged = true;
    }
    if (request->hasParam("batt2Cells", true)) {
      battery2CellCount = clampBatteryCellCount(static_cast<uint8_t>(request->getParam("batt2Cells", true)->value().toInt()));
      batteryChanged = true;
    }

    if (tempChanged) {
      saveTemperatureTargets();
    }
    if (swapChanged) {
      saveSwapAssignment();
    }
    if (manualWindowChanged) {
      saveManualToggleOffMs();
    }
    if (batteryChanged) {
      saveBatteryCellCounts();
    }

    request->send(200, "text/plain", "OK");
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
    scheduleRestart(600);
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
    scheduleRestart(600);
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

  server.on("/resetOvertemp", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    clearMosfetOvertempEvents();
    request->send(200, "text/plain", "OK");
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    String page;
    page.reserve(6200);
    page += F(
        "<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>HeatControl OTA</title><style>"
        ":root{--bg-0:#0f1419;--bg-1:#151c24;--panel-0:#1c252f;--panel-1:#26323e;--line:#3b4a59;--text:#eaf2f7;--muted:#9db0bf;--warm:#ffb65c;--shadow:rgba(0,0,0,.44)}"
        "*{box-sizing:border-box}"
        "body{margin:0;min-height:100vh;font-family:Bahnschrift,'Segoe UI',sans-serif;color:var(--text);background:radial-gradient(120% 90% at 0% 0%,rgba(94,199,255,.16),transparent 58%),radial-gradient(120% 110% at 100% 100%,rgba(255,182,92,.12),transparent 62%),repeating-linear-gradient(90deg,rgba(255,255,255,.03) 0 1px,transparent 1px 22px),linear-gradient(180deg,var(--bg-0),var(--bg-1));padding:14px}"
        ".app{max-width:460px;margin:0 auto;display:grid;gap:12px}"
        ".card{border:1px solid var(--line);border-radius:16px;background:linear-gradient(180deg,var(--panel-0),var(--panel-1));box-shadow:inset 0 1px 0 rgba(255,255,255,.08),0 10px 24px var(--shadow);padding:14px}"
        ".logo-wrap{text-align:center;margin-bottom:8px}"
        ".hero-logo{width:min(84%,320px);max-width:320px;height:auto;object-fit:contain;filter:drop-shadow(0 8px 16px rgba(0,0,0,.38))}"
        ".title{margin:0 0 8px;text-transform:uppercase;letter-spacing:.06em;font-size:.74rem;color:#d1dbe3;font-weight:700}"
        ".hint{border:1px dashed #566777;border-radius:9px;background:#1a232b;color:#c2cdd6;padding:8px;font-size:.74rem;line-height:1.35;margin:0 0 10px}"
        ".file{display:block;width:100%;border:1px solid #566777;border-radius:10px;background:#1a232b;color:#e3eaf0;font-size:.82rem;padding:10px}"
        ".actions{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}"
        ".btn,.btn-link{border:1px solid #5d6f81;border-radius:10px;background:linear-gradient(180deg,#394450,#2b333d);color:var(--text);font-size:.8rem;padding:10px;min-height:40px;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;cursor:pointer}"
        ".btn-primary{border-color:rgba(255,182,92,.68);background:linear-gradient(180deg,rgba(255,182,92,.28),rgba(146,87,42,.56));color:#ffe7c7;font-weight:700}"
        ".note{margin-top:10px;color:var(--muted);font-size:.73rem;line-height:1.35}"
        "</style></head><body><main class='app'><section class='card'><div class='logo-wrap'><img class='hero-logo' src='/LOGO.png' alt='HeatControl Logo'></div>"
        "<div class='title'>Firmware Update (OTA)</div>"
        "<p class='hint'>Waehle eine passende <b>firmware.bin</b> aus einem HeatControl-Release und starte danach das Flashen.</p>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input class='file' type='file' name='firmware' accept='.bin' required>"
        "<div class='actions'><button class='btn btn-primary' type='submit'>Upload und Flashen</button><a class='btn-link' href='/'>Zurueck zur Konsole</a></div>"
        "</form>"
        "<div class='note'>Wichtig: Versorgung waehrend des Updates nicht unterbrechen. Das Geraet startet danach automatisch neu.</div>"
        "</section></main></body></html>");
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
        scheduleRestart(1200);
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!isFromLocalApSubnet(request)) {
          return;
        }
        if (index == 0) {
          otaUploadStarted = true;
          otaUploadOk = false;
          otaUploadMessage = "";
          otaUploadBytes = 0;

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
        otaUploadBytes += written;

        if (final) {
          constexpr size_t kMinFirmwareBytes = 100U * 1024U;
          if (otaUploadBytes < kMinFirmwareBytes) {
            Update.abort();
            otaUploadMessage = "Firmware image too small (" + String(otaUploadBytes) + " bytes). Aborting update.";
            return;
          }
          if (!Update.end(true)) {
            otaUploadMessage = "Finalize failed. Error code: " + String(Update.getError());
            return;
          }
          otaUploadOk = true;
          otaUploadMessage = "OK";
        }
      });

  // Known captive portal probes (Android / iOS / Windows): always redirect to portal root.
  server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/gen_204", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/library/test/success.html", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/success.txt", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/ncsi.txt", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/connecttest.txt.gz", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/connecttest.txt/index.htm", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/connecttest.txt/index.htm.gz", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/redirect", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });
  server.on("/fwlink", HTTP_ANY, [](AsyncWebServerRequest *request) { sendCaptiveRedirect(request); });

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET && isFromLocalApSubnet(request)) {
      sendCaptiveRedirect(request);
      return;
    }

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
