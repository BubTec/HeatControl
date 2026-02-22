#include "web_server.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include <cmath>
#include <string>

#include "app_state.h"
#include "control.h"
#include "generated/embedded_files_registry.h"
#include "logic_helpers.h"
#include "status_builder.h"
#include "storage.h"

namespace HeatControl {

namespace {

bool otaUploadStarted = false;
bool otaUploadOk = false;
String otaUploadMessage;
size_t otaUploadBytes = 0;

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
    const bool heater1On = (digitalRead(SSR_PIN_1) == LOW);
    const bool heater2On = (digitalRead(SSR_PIN_2) == LOW);
    const bool ntc1Valid = !std::isnan(ntcMosfet1TempC);
    const bool ntc2Valid = !std::isnan(ntcMosfet2TempC);
    const bool trip1Valid = !std::isnan(mosfet1OvertempTripTempC);
    const bool trip2Valid = !std::isnan(mosfet2OvertempTripTempC);
    const String totalRuntime = formatRuntime(savedRuntimeMinutes * 60UL, false);
    const String currentRuntime = formatRuntime(currentSessionSeconds, true);

    StatusMetrics metrics;
    metrics.modeText = modeText.c_str();
    metrics.manualMode = manualMode;
    metrics.manualPercent1 = manualPowerPercent1;
    metrics.manualPercent2 = manualPowerPercent2;
    metrics.manualHeater1Enabled = manualHeater1Enabled;
    metrics.manualHeater2Enabled = manualHeater2Enabled;
    metrics.bootPinText = bootPinText.c_str();
    metrics.adc1MilliVolts = adc1MilliVolts;
    metrics.adc2MilliVolts = adc2MilliVolts;
    metrics.ntcMosfet1MilliVolts = ntcMosfet1MilliVolts;
    metrics.ntcMosfet2MilliVolts = ntcMosfet2MilliVolts;
    metrics.ntcMosfet1Valid = ntc1Valid;
    metrics.ntcMosfet1TempC = ntcMosfet1TempC;
    metrics.ntcMosfet2Valid = ntc2Valid;
    metrics.ntcMosfet2TempC = ntcMosfet2TempC;
    metrics.mosfet1OvertempActive = mosfet1OvertempActive;
    metrics.mosfet2OvertempActive = mosfet2OvertempActive;
    metrics.mosfet1OvertempLatched = mosfet1OvertempLatched;
    metrics.mosfet2OvertempLatched = mosfet2OvertempLatched;
    metrics.mosfet1TripValid = trip1Valid;
    metrics.mosfet1TripTempC = mosfet1OvertempTripTempC;
    metrics.mosfet2TripValid = trip2Valid;
    metrics.mosfet2TripTempC = mosfet2OvertempTripTempC;
    metrics.mosfetOvertempLimitC = MOSFET_OVERTEMP_LIMIT_C;
    metrics.battery1CellCount = battery1CellCount;
    metrics.battery1PackVoltage = battery1PackVoltage;
    metrics.battery1CellVoltage = battery1CellVoltage;
    metrics.battery1SocPercent = battery1SocPercent;
    metrics.battery2CellCount = battery2CellCount;
    metrics.battery2PackVoltage = battery2PackVoltage;
    metrics.battery2CellVoltage = battery2CellVoltage;
    metrics.battery2SocPercent = battery2SocPercent;
    metrics.manualToggleMaxOffMs = manualPowerToggleMaxOffMs;
    metrics.displayTemp1 = displayTemp1;
    metrics.displayTemp2 = displayTemp2;
    metrics.targetTemp1 = targetTemp1;
    metrics.targetTemp2 = targetTemp2;
    metrics.swapAssignment = swapAssignment;
    metrics.ssid = activeSsid.c_str();
    metrics.heater1On = heater1On;
    metrics.heater2On = heater2On;
    metrics.totalRuntime = totalRuntime.c_str();
    metrics.currentRuntime = currentRuntime.c_str();

    const std::string json = buildStatusJson(metrics);
    request->send(200, "application/json", json.c_str());
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

  server.on("/cycleManualPower", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isFromLocalApSubnet(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    if (!manualMode) {
      request->send(409, "text/plain", "Manual mode required");
      return;
    }
    if (!request->hasParam("channel", true)) {
      request->send(400, "text/plain", "Missing channel");
      return;
    }

    const int channel = request->getParam("channel", true)->value().toInt();
    if (channel == 1) {
      cycleManualPowerPercent1();
      signalManualPowerChange(manualPowerPercent1);
      request->send(200, "text/plain", "OK");
      return;
    }
    if (channel == 2) {
      cycleManualPowerPercent2();
      signalManualPowerChange(manualPowerPercent2);
      request->send(200, "text/plain", "OK");
      return;
    }

    request->send(400, "text/plain", "Invalid channel");
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
    // Web UI restart always boots in normal mode.
    setNextBootMode(BOOT_MODE_NORMAL);
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

  server.on("/version.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!sendEmbeddedFile(request, "/version.txt")) {
      request->send(404, "text/plain", "Not found");
    }
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
        ".file-picker{position:relative;display:block;width:100%;border:1px solid #566777;border-radius:12px;background:#1a232b;color:#e3eaf0;padding:14px;text-align:center;cursor:pointer}"
        ".file-picker strong{display:block;font-size:.86rem;margin-bottom:4px}"
        ".file-picker span{display:block;font-size:.72rem;color:var(--muted)}"
        ".file-picker input{position:absolute;inset:0;border:0;opacity:0;width:100%;height:100%;cursor:pointer}"
        ".file-name{margin-top:6px;text-align:center;font-size:.72rem;color:var(--muted)}"
        ".actions{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;margin-top:10px}"
        ".btn,.btn-link{border:1px solid #5d6f81;border-radius:10px;background:linear-gradient(180deg,#394450,#2b333d);color:var(--text);font-size:.8rem;padding:10px;min-height:40px;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;cursor:pointer}"
        ".btn-primary{border-color:rgba(255,182,92,.68);background:linear-gradient(180deg,rgba(255,182,92,.28),rgba(146,87,42,.56));color:#ffe7c7;font-weight:700}"
        ".note{margin-top:10px;color:var(--muted);font-size:.73rem;line-height:1.35}"
        "</style></head><body><main class='app'><section class='card'><div class='logo-wrap'><img class='hero-logo' src='/LOGO.png' alt='HeatControl Logo'></div>"
        "<div class='title'>Firmware Update (OTA)</div>"
        "<p class='hint'>Waehle eine passende <b>firmware.bin</b> aus einem HeatControl-Release und starte danach das Flashen.</p>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<label class='file-picker'>"
        "<strong>Datei auswaehlen</strong>"
        "<span>Firmware (.bin) aus dem aktuellen Release</span>"
        "<input type='file' id='firmwareFileInput' name='firmware' accept='.bin' required>"
        "</label>"
        "<p class='file-name' id='firmwareFileName'>Keine Datei ausgewaehlt.</p>"
        "<div class='actions'><button class='btn btn-primary' type='submit'>Upload und Flashen</button><a class='btn-link' href='/'>Zurueck zur Konsole</a>"
        "<a class='btn-link' href='https://github.com/BubTec/HeatControl/releases/latest' target='_blank' rel='noopener'>Neueste Version (GitHub)</a></div>"
        "</form>"
        "<div class='note'>Wichtig: Versorgung waehrend des Updates nicht unterbrechen. Das Geraet startet danach automatisch neu.</div>"
        "</section></main><script>document.addEventListener('DOMContentLoaded',function(){var input=document.getElementById('firmwareFileInput');var nameEl=document.getElementById('firmwareFileName');if(!input||!nameEl){return;}var updateName=function(){if(input.files&&input.files.length){nameEl.textContent=input.files[0].name;}else{nameEl.textContent='Keine Datei ausgewaehlt.';}};input.addEventListener('change',updateName);});</script></body></html>");
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
