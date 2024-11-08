#include "web_server.h"
#include "logger.h"
#include "config.h"
#include "heating_control.h"

ESP8266WebServer server(80);

// HTML Template
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <title>Temperatur Controller</title>
    <style>
        :root {
            --primary-color: #2196F3;
            --secondary-color: #03A9F4;
            --success-color: #4CAF50;
            --warning-color: #FFC107;
            --error-color: #F44336;
            --background-color: #F5F5F5;
            --card-background: #FFFFFF;
            --text-color: #333333;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            line-height: 1.6;
            background-color: var(--background-color);
            color: var(--text-color);
            padding: 20px;
        }

        h2, h3, h4 {
            color: var(--primary-color);
            margin-bottom: 15px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        .config {
            background: var(--card-background);
            border-radius: 10px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin: 20px 0;
            padding: 20px;
            transition: transform 0.2s;
        }

        .config:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0,0,0,0.15);
        }

        .heizkreis {
            background: var(--background-color);
            border-radius: 8px;
            padding: 20px;
            margin: 10px 0;
        }

        .temp-display {
            font-size: 1.2em;
            margin: 15px 0;
        }

        .temp-value {
            font-size: 2em;
            font-weight: bold;
            color: var(--primary-color);
        }

        .pwm-display {
            color: var(--secondary-color);
            font-size: 1.1em;
        }

        input[type="number"], input[type="text"], input[type="password"] {
            width: 120px;
            padding: 8px 12px;
            border: 2px solid #ddd;
            border-radius: 4px;
            font-size: 16px;
            transition: border-color 0.3s;
        }

        input[type="number"]:focus, input[type="text"]:focus, input[type="password"]:focus {
            border-color: var(--primary-color);
            outline: none;
        }

        button, input[type="submit"] {
            background-color: var(--primary-color);
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            transition: background-color 0.3s;
        }

        button:hover, input[type="submit"]:hover {
            background-color: var(--secondary-color);
        }

        .pid-config {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
            margin: 15px 0;
        }

        .pid-config label {
            display: inline-block;
            margin: 5px 10px;
        }

        .log-container {
            text-align: left;
            background: #2b2b2b;
            color: #ffffff;
            padding: 15px;
            margin: 10px 0;
            height: 300px;
            overflow-y: auto;
            font-family: 'Consolas', monospace;
            font-size: 14px;
            border-radius: 8px;
        }

        .error { color: var(--error-color); }
        .warning { color: var(--warning-color); }
        .info { color: var(--success-color); }

        .mode-display {
            text-align: center;
            padding: 15px;
        }
        
        .mode-indicator {
            display: inline-block;
            padding: 10px 20px;
            border-radius: 5px;
            font-size: 1.2em;
            font-weight: bold;
            margin: 10px;
        }
        
        .mode-normal {
            background-color: var(--success-color);
            color: white;
        }
        
        .mode-heater {
            background-color: var(--error-color);
            color: white;
        }

        @media (max-width: 768px) {
            .container {
                padding: 10px;
            }
            
            input[type="number"], input[type="text"], input[type="password"] {
                width: 100%;
                margin: 5px 0;
            }
            
            .pid-config label {
                display: block;
                margin: 10px 0;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Temperatur Controller</h2>
        
        <div class="config">
            <div class="mode-display">
                <h3>Betriebsmodus</h3>
                <div class="mode-indicator %MODE_CLASS%">
                    %MODE_TEXT%
                </div>
            </div>
        </div>

        <div class="config">
            <h3>Heizkreis 1</h3>
            <div class="heizkreis">
                <div class="temp-display">
                    <span>Aktuelle Temperatur: </span>
                    <span class="temp-value">%TEMP1%°C</span>
                    <p>Zieltemperatur: %TARGET1%°C</p>
                    <p class="pwm-display">PWM: %PWM1%%</p>
                </div>
                <form action="/settemp" method="get">
                    <input type="number" name="temp1" step="0.5" value="%TARGET1%">
                    <input type="submit" value="Temperatur setzen">
                </form>
                <div class="pid-config">
                    <h4>PID Parameter</h4>
                    <form action="/setpid" method="get">
                        <label>Kp: <input type="number" name="kp1" step="0.1" value="%KP1%"></label>
                        <label>Ki: <input type="number" name="ki1" step="0.01" value="%KI1%"></label>
                        <label>Kd: <input type="number" name="kd1" step="0.1" value="%KD1%"></label>
                        <input type="submit" value="Parameter speichern">
                    </form>
                </div>
            </div>
        </div>

        <div class="config">
            <h3>Heizkreis 2</h3>
            <div class="heizkreis">
                <div class="temp-display">
                    <span>Aktuelle Temperatur: </span>
                    <span class="temp-value">%TEMP2%°C</span>
                    <p>Zieltemperatur: %TARGET2%°C</p>
                    <p class="pwm-display">PWM: %PWM2%%</p>
                </div>
                <form action="/settemp" method="get">
                    <input type="number" name="temp2" step="0.5" value="%TARGET2%">
                    <input type="submit" value="Temperatur setzen">
                </form>
                <div class="pid-config">
                    <h4>PID Parameter</h4>
                    <form action="/setpid" method="get">
                        <label>Kp: <input type="number" name="kp2" step="0.1" value="%KP2%"></label>
                        <label>Ki: <input type="number" name="ki2" step="0.01" value="%KI2%"></label>
                        <label>Kd: <input type="number" name="kd2" step="0.1" value="%KD2%"></label>
                        <input type="submit" value="Parameter speichern">
                    </form>
                </div>
            </div>
        </div>
        
        <div class="config">
            <h3>System Log</h3>
            <div class="log-container" id="logContainer">%SYSTEM_LOG%</div>
            <button onclick="clearLog()">Log löschen</button>
        </div>
        
        <div class="config">
            <h3>WLAN Konfiguration</h3>
            <form action="/setwifi" method="post">
                <p><label>SSID:<br><input type="text" name="ssid" value="%WIFI_SSID%" maxlength="32"></label></p>
                <p><label>Passwort:<br><input type="password" name="pass" value="%WIFI_PASS%" maxlength="64"></label></p>
                <input type="submit" value="WLAN Einstellungen speichern">
            </form>
            <p><small>Nach dem Speichern startet das Gerät neu!</small></p>
        </div>
    </div>

    <script>
        setInterval(function() {
            fetch('/getlog')
                .then(response => response.text())
                .then(log => {
                    document.getElementById('logContainer').innerHTML = log;
                    var container = document.getElementById('logContainer');
                    container.scrollTop = container.scrollHeight;
                });
        }, 5000);

        function clearLog() {
            fetch('/clearlog')
                .then(() => {
                    document.getElementById('logContainer').innerHTML = '';
                });
        }
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    String html = String(index_html);
    html.replace("%TEMP1%", String(currentTemp1, 1));
    html.replace("%TARGET1%", String(TARGET_TEMP1, 1));
    html.replace("%PWM1%", String((float)currentPWM1/MAX_PWM * 100, 0));
    html.replace("%KP1%", String(config.Kp1, 1));
    html.replace("%KI1%", String(config.Ki1, 3));
    html.replace("%KD1%", String(config.Kd1, 1));
    
    html.replace("%TEMP2%", String(currentTemp2, 1));
    html.replace("%TARGET2%", String(TARGET_TEMP2, 1));
    html.replace("%PWM2%", String((float)currentPWM2/MAX_PWM * 100, 0));
    html.replace("%KP2%", String(config.Kp2, 1));
    html.replace("%KI2%", String(config.Ki2, 3));
    html.replace("%KD2%", String(config.Kd2, 1));
    
    html.replace("%WIFI_SSID%", String(config.ssid));
    html.replace("%WIFI_PASS%", String(config.password));

    if (rtcData.magic == MAGIC_HEATER_ON) {
        html.replace("%MODE_CLASS%", "mode-heater");
        html.replace("%MODE_TEXT%", "DURCHHEIZEN (100%)");
    } else {
        html.replace("%MODE_CLASS%", "mode-normal");
        html.replace("%MODE_TEXT%", "NORMAL (Temperaturregelung)");
    }
    
    server.send(200, "text/html", html);
}

void handleSetTemp() {
    bool changed = false;
    
    if (server.hasArg("temp1")) {
        float newTemp1 = server.arg("temp1").toFloat();
        if (newTemp1 >= 5 && newTemp1 <= 30) {
            TARGET_TEMP1 = newTemp1;
            config.targetTemp1 = newTemp1;
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Heizkreis 1 Temperatur auf %.1f°C gesetzt", newTemp1);
            addLog(logMsg, 0);
        }
    }
    if (server.hasArg("temp2")) {
        float newTemp2 = server.arg("temp2").toFloat();
        if (newTemp2 >= 5 && newTemp2 <= 30) {
            TARGET_TEMP2 = newTemp2;
            config.targetTemp2 = newTemp2;
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Heizkreis 2 Temperatur auf %.1f°C gesetzt", newTemp2);
            addLog(logMsg, 0);
        }
    }
    
    if (changed) {
        saveConfig();
    }
    
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Updated");
}

void handleSetPID() {
    bool changed = false;
    
    // Heizkreis 1
    if (server.hasArg("kp1")) {
        float val = server.arg("kp1").toFloat();
        if (val >= 0 && val <= 100) {
            config.Kp1 = val;
            changed = true;
        }
    }
    if (server.hasArg("ki1")) {
        float val = server.arg("ki1").toFloat();
        if (val >= 0 && val <= 1) {
            config.Ki1 = val;
            changed = true;
        }
    }
    if (server.hasArg("kd1")) {
        float val = server.arg("kd1").toFloat();
        if (val >= 0 && val <= 100) {
            config.Kd1 = val;
            changed = true;
        }
    }
    
    // Heizkreis 2
    if (server.hasArg("kp2")) {
        float val = server.arg("kp2").toFloat();
        if (val >= 0 && val <= 100) {
            config.Kp2 = val;
            changed = true;
        }
    }
    if (server.hasArg("ki2")) {
        float val = server.arg("ki2").toFloat();
        if (val >= 0 && val <= 1) {
            config.Ki2 = val;
            changed = true;
        }
    }
    if (server.hasArg("kd2")) {
        float val = server.arg("kd2").toFloat();
        if (val >= 0 && val <= 100) {
            config.Kd2 = val;
            changed = true;
        }
    }
    
    if (changed) {
        saveConfig();
        addLog("PID Parameter aktualisiert", 0);
    }
    
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Updated");
}

void handleSetWifi() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String newSSID = server.arg("ssid");
        String newPass = server.arg("pass");
        
        strncpy(config.ssid, newSSID.c_str(), sizeof(config.ssid) - 1);
        strncpy(config.password, newPass.c_str(), sizeof(config.password) - 1);
        config.ssid[sizeof(config.ssid) - 1] = '\0';
        config.password[sizeof(config.password) - 1] = '\0';
        
        saveConfig();
        addLog("WLAN Einstellungen aktualisiert - Neustart erfolgt", 0);
        
        server.send(200, "text/html", 
            "<html><body>"
            "<h2>Einstellungen gespeichert</h2>"
            "<p>Das Gerät startet in 3 Sekunden neu...</p>"
            "<script>setTimeout(function(){ window.location.href='/' }, 3000);</script>"
            "</body></html>");
            
        delay(500);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Fehlende Parameter");
    }
}

void setupWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settemp", HTTP_GET, handleSetTemp);
    server.on("/setpid", HTTP_GET, handleSetPID);
    server.on("/setwifi", HTTP_POST, handleSetWifi);
    server.on("/getlog", HTTP_GET, handleGetLog);
    server.on("/clearlog", HTTP_GET, handleClearLog);
    
    // Captive Portal Routes
    server.on("/generate_204", HTTP_GET, handleRoot);
    server.on("/gen_204", HTTP_GET, handleRoot);
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
    
    server.begin();
    addLog("Webserver gestartet", 0);
}

void handleWebServer() {
    server.handleClient();
}

// Implementiere die restlichen Handler-Funktionen aus der ursprünglichen main.cpp