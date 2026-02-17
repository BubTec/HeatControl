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

String generateIndexHtml() {
  const float displayTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
  const float displayTemp2 = swapAssignment ? currentTemp1 : currentTemp2;
  const uint32_t currentSessionSeconds = static_cast<uint32_t>((millis() - startTimeMs) / 1000UL);

  String html;
  html.reserve(6500);
  html += F(
      "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' "
      "content='width=device-width, initial-scale=1'><title>HeatControl</title><style>"
      "body{font-family:Arial,Helvetica,sans-serif;margin:0;background:#1d1f24;color:#fff}"
      "h1{margin:0;padding:16px;background:#c0392b;font-size:1.5rem;text-align:center}"
      ".wrap{max-width:700px;margin:0 auto;padding:14px}.box{background:#2b2f36;padding:14px;"
      "border-radius:10px;margin-bottom:12px}.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}"
      "label{display:block;margin-bottom:6px;font-size:.95rem}.muted{color:#bbb;font-size:.9rem}"
      "input[type=number],input[type=text],input[type=password]{background:#3a3f47;border:1px solid #666;"
      "color:#fff;padding:8px;border-radius:6px;width:110px}"
      "button,input[type=submit]{background:#c0392b;border:0;color:#fff;padding:9px 14px;border-radius:6px;cursor:pointer}"
      ".status{display:inline-block;padding:4px 10px;border-radius:6px;font-weight:bold}"
      ".on{background:#c0392b}.off{background:#555}</style></head><body><h1>HeatControl</h1><div class='wrap'>");

  html += "<div class='box'><div><b>Mode:</b> " + String(powerMode ? "POWER" : "NORMAL") +
          "</div><div class='muted'>AP: " + activeSsid + " (" + WiFi.softAPIP().toString() + ")</div></div>";

  html += "<form class='box' action='/setTemp' method='POST'><h3>Temperature Targets</h3>"
          "<div class='row'><label>Target 1<br><input type='number' step='0.5' min='10' max='45' name='temp1' value='" +
          String(targetTemp1, 1) + "'></label>"
          "<label>Target 2<br><input type='number' step='0.5' min='10' max='45' name='temp2' value='" +
          String(targetTemp2, 1) + "'></label></div><input type='submit' value='Save targets'></form>";

  html += "<div class='box'><h3>Current Values</h3>"
          "<div id='line1'>Sensor 1: " + String(displayTemp1, 2) + " C</div>"
          "<div id='line2'>Sensor 2: " + String(displayTemp2, 2) + " C</div>"
          "<div class='row' style='margin-top:8px'><span>Heater 1:</span><span id='h1' class='status " +
          String(heaterStateText(SSR_PIN_1) == "ON" ? "on" : "off") + "'>" + heaterStateText(SSR_PIN_1) +
          "</span><span>Heater 2:</span><span id='h2' class='status " +
          String(heaterStateText(SSR_PIN_2) == "ON" ? "on" : "off") + "'>" + heaterStateText(SSR_PIN_2) + "</span></div>"
          "</div>";

  html += "<form class='box' action='/swapSensors' method='POST'><h3>Sensor Assignment</h3><label>"
          "<input type='checkbox' name='swap' " +
          String(swapAssignment ? "checked" : "") + "> Swap sensor mapping</label><br><br>"
          "<input type='submit' value='Apply swap'></form>";

  html += "<form class='box' action='/setWiFi' method='POST'><h3>WiFi Settings</h3>"
          "<div class='row'><label>SSID<br><input type='text' name='ssid' value='" + activeSsid + "'></label>"
          "<label>Password<br><input type='password' name='password' value='" + activePassword + "'></label></div>"
          "<input type='submit' value='Save WiFi & restart'></form>";

  html += "<div class='box'><h3>Runtime</h3><div id='runtimeTotal'>Total: " +
          formatRuntime(savedRuntimeMinutes * 60UL, false) + "</div><div id='runtimeCurrent'>Current: " +
          formatRuntime(currentSessionSeconds, true) + "</div><form action='/resetRuntime' method='POST' style='margin-top:10px'>"
          "<input type='submit' value='Reset runtime'></form></div>";

  html += F("<div class='box'><h3>Firmware Update (OTA)</h3>"
            "<div class='muted'>Upload firmware.bin from the latest release.</div>"
            "<form action='/update' method='GET' style='margin-top:10px'>"
            "<input type='submit' value='Open OTA Update'></form></div>");

  html += F("<div class='box'><h3>Restart</h3><div class='row'>"
            "<form action='/restart' method='POST'><input type='hidden' name='mode' value='normal'><input type='submit' value='Restart normal'></form>"
            "<form action='/restart' method='POST'><input type='hidden' name='mode' value='power'><input type='submit' value='Restart power'></form>"
            "</div></div>");

  html += F(
      "<script>"
      "function upd(){fetch('/status').then(r=>r.json()).then(d=>{"
      "document.getElementById('line1').textContent='Sensor 1: '+d.current1.toFixed(2)+' C';"
      "document.getElementById('line2').textContent='Sensor 2: '+d.current2.toFixed(2)+' C';"
      "const h1=document.getElementById('h1');const h2=document.getElementById('h2');"
      "h1.textContent=d.h1?'ON':'OFF';h1.className='status '+(d.h1?'on':'off');"
      "h2.textContent=d.h2?'ON':'OFF';h2.className='status '+(d.h2?'on':'off');"
      "document.getElementById('runtimeTotal').textContent='Total: '+d.totalRuntime;"
      "document.getElementById('runtimeCurrent').textContent='Current: '+d.currentRuntime;"
      "}).catch(()=>{});}setInterval(upd,2000);upd();"
      "</script></div></body></html>");

  return html;
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
    request->send(200, "text/html", generateIndexHtml());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    const float displayTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
    const float displayTemp2 = swapAssignment ? currentTemp1 : currentTemp2;
    const uint32_t currentSessionSeconds = static_cast<uint32_t>((millis() - startTimeMs) / 1000UL);
    const String modeText = manualMode ? ("MANUAL " + String(manualPowerPercent) + "%")
                                       : (powerMode ? "POWER" : "NORMAL");
    String json = "{\"mode\":\"" + modeText + "\"" +
                  ",\"current1\":" + String(displayTemp1, 2) +
                  ",\"current2\":" + String(displayTemp2, 2) +
                  ",\"target1\":" + String(targetTemp1, 1) +
                  ",\"target2\":" + String(targetTemp2, 1) +
                  ",\"swap\":" + String(swapAssignment ? 1 : 0) +
                  ",\"ssid\":\"" + jsonEscape(activeSsid) + "\"" +
                  ",\"password\":\"" + jsonEscape(activePassword) + "\"" +
                  ",\"h1\":" + String(digitalRead(SSR_PIN_1) == LOW ? 1 : 0) +
                  ",\"h2\":" + String(digitalRead(SSR_PIN_2) == LOW ? 1 : 0) +
                  ",\"totalRuntime\":\"" + formatRuntime(savedRuntimeMinutes * 60UL, false) + "\"" +
                  ",\"currentRuntime\":\"" + formatRuntime(currentSessionSeconds, true) + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/runtime", HTTP_GET, [](AsyncWebServerRequest *request) {
    const uint32_t currentSessionSeconds = static_cast<uint32_t>((millis() - startTimeMs) / 1000UL);
    String json = "{\"total\":\"" + formatRuntime(savedRuntimeMinutes * 60UL, false) +
                  "\",\"current\":\"" + formatRuntime(currentSessionSeconds, true) + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/setTemp", HTTP_POST, [](AsyncWebServerRequest *request) {
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
    swapAssignment = request->hasParam("swap", true);
    saveSwapAssignment();
    request->redirect("/");
  });

  server.on("/setWiFi", HTTP_POST, [](AsyncWebServerRequest *request) {
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

  server.on("/resetRuntime", HTTP_POST, [](AsyncWebServerRequest *request) {
    savedRuntimeMinutes = 0;
    writeRuntimeToEeprom(0);
    startTimeMs = millis();
    request->redirect("/");
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        (void)request;
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
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  if (fileSystemReady) {
    server.serveStatic("/", LittleFS, "/");
  }

  server.begin();
}

}  // namespace HeatControl
