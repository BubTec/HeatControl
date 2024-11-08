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

        .temp-control {
            margin: 20px 0;
        }

        .slider-container {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 10px;
        }

        input[type="range"] {
            flex: 1;
            height: 25px;
            -webkit-appearance: none;
            background: #d3d3d3;
            outline: none;
            border-radius: 12px;
            overflow: hidden;
            box-shadow: inset 0 0 5px rgba(0, 0, 0, 0.2);
        }

        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 25px;
            height: 25px;
            border-radius: 50%;
            background: var(--primary-color);
            cursor: pointer;
            border: 4px solid #fff;
            box-shadow: -407px 0 0 400px var(--primary-color);
        }

        input[type="range"]::-moz-range-thumb {
            width: 25px;
            height: 25px;
            border-radius: 50%;
            background: var(--primary-color);
            cursor: pointer;
            border: 4px solid #fff;
            box-shadow: -407px 0 0 400px var(--primary-color);
        }

        input[type="number"] {
            width: 80px !important;
            text-align: center;
        }

        .unit {
            color: var(--text-color);
            font-size: 1.1em;
            min-width: 30px;
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
                <form action="/settemp" method="get" class="temp-control">
                    <div class="slider-container">
                        <input type="range" name="temp1_slider" min="10" max="40" step="0.5" value="%TARGET1%" 
                            oninput="this.nextElementSibling.value = this.value">
                        <input type="number" name="temp1" min="10" max="40" step="0.5" value="%TARGET1%" 
                            oninput="this.previousElementSibling.value = this.value">
                        <span class="unit">°C</span>
                    </div>
                    <input type="submit" value="Temperatur setzen">
                </form>
                <div class="pid-config">
                    <h4>PID Parameter</h4>
                    <form action="/setpid" method="get">
                        <label>Kp: <input type="number" name="kp1" step="0.1" value="%KP1%" min="0" max="100" required
                            oninvalid="this.setCustomValidity('Kp muss zwischen 0 und 100 liegen')"
                            oninput="this.setCustomValidity('')"></label>
                        <label>Ki: <input type="number" name="ki1" step="0.01" value="%KI1%" min="0" max="1" required
                            oninvalid="this.setCustomValidity('Ki muss zwischen 0 und 1 liegen')"
                            oninput="this.setCustomValidity('')"></label>
                        <label>Kd: <input type="number" name="kd1" step="0.1" value="%KD1%" min="0" max="100" required
                            oninvalid="this.setCustomValidity('Kd muss zwischen 0 und 100 liegen')"
                            oninput="this.setCustomValidity('')"></label>
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
                <form action="/settemp" method="get" class="temp-control">
                    <div class="slider-container">
                        <input type="range" name="temp2_slider" min="10" max="40" step="0.5" value="%TARGET2%" 
                            oninput="this.nextElementSibling.value = this.value">
                        <input type="number" name="temp2" min="10" max="40" step="0.5" value="%TARGET2%" 
                            oninput="this.previousElementSibling.value = this.value">
                        <span class="unit">°C</span>
                    </div>
                    <input type="submit" value="Temperatur setzen">
                </form>
                <div class="pid-config">
                    <h4>PID Parameter</h4>
                    <form action="/setpid" method="get">
                        <label>Kp: <input type="number" name="kp2" step="0.1" value="%KP2%" min="0" max="100" required
                            oninvalid="this.setCustomValidity('Kp muss zwischen 0 und 100 liegen')"
                            oninput="this.setCustomValidity('')"></label>
                        <label>Ki: <input type="number" name="ki2" step="0.01" value="%KI2%" min="0" max="1" required
                            oninvalid="this.setCustomValidity('Ki muss zwischen 0 und 1 liegen')"
                            oninput="this.setCustomValidity('')"></label>
                        <label>Kd: <input type="number" name="kd2" step="0.1" value="%KD2%" min="0" max="100" required
                            oninvalid="this.setCustomValidity('Kd muss zwischen 0 und 100 liegen')"
                            oninput="this.setCustomValidity('')"></label>
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
                <p><label>SSID:<br><input type="text" name="ssid" value="%WIFI_SSID%" maxlength="32" minlength="1" required pattern="[A-Za-z0-9_-]{1,32}"
                    oninvalid="this.setCustomValidity('SSID darf nur Buchstaben, Zahlen, - und _ enthalten')"
                    oninput="this.setCustomValidity('')"></label></p>
                <p><label>Passwort:<br><input type="password" name="pass" value="%WIFI_PASS%" maxlength="64" minlength="8" required
                    oninvalid="this.setCustomValidity('Passwort muss mindestens 8 Zeichen lang sein')"
                    oninput="this.setCustomValidity('')"></label></p>
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

        // Validierungsfunktionen
        function validateTemp(value) {
            const temp = parseFloat(value);
            return !isNaN(temp) && temp >= 10 && temp <= 40;
        }

        function validatePID(kp, ki, kd) {
            const p = parseFloat(kp);
            const i = parseFloat(ki);
            const d = parseFloat(kd);
            return !isNaN(p) && !isNaN(i) && !isNaN(d) &&
                   p >= 0 && p <= 100 &&
                   i >= 0 && i <= 1 &&
                   d >= 0 && d <= 100;
        }

        // Event-Handler für Formulare
        document.querySelectorAll('form').forEach(form => {
            form.onsubmit = function(e) {
                const temp1 = this.querySelector('input[name="temp1"]');
                const temp2 = this.querySelector('input[name="temp2"]');
                const kp = this.querySelector('input[name="kp1"], input[name="kp2"]');
                const ki = this.querySelector('input[name="ki1"], input[name="ki2"]');
                const kd = this.querySelector('input[name="kd1"], input[name="kd2"]');
                
                if (temp1 && !validateTemp(temp1.value)) {
                    alert('Temperatur muss zwischen 10°C und 40°C liegen');
                    e.preventDefault();
                    return false;
                }
                
                if (temp2 && !validateTemp(temp2.value)) {
                    alert('Temperatur muss zwischen 10°C und 40°C liegen');
                    e.preventDefault();
                    return false;
                }
                
                if (kp && ki && kd && !validatePID(kp.value, ki.value, kd.value)) {
                    alert('Ungültige PID-Parameter');
                    e.preventDefault();
                    return false;
                }
                
                return true;
            };
        });

        // Funktion zum Formatieren der Slider-Werte
        document.querySelectorAll('input[type="range"]').forEach(slider => {
            slider.addEventListener('input', function() {
                // Aktualisiere den Zahlenwert
                let numberInput = this.nextElementSibling;
                numberInput.value = parseFloat(this.value).toFixed(1);
            });
        });

        document.querySelectorAll('input[type="number"]').forEach(input => {
            input.addEventListener('input', function() {
                // Aktualisiere den Slider
                let slider = this.previousElementSibling;
                slider.value = this.value;
            });
        });
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
    String message;
    
    if (server.hasArg("temp1")) {
        float newTemp1 = server.arg("temp1").toFloat();
        if (newTemp1 >= 10 && newTemp1 <= 40) {
            TARGET_TEMP1 = newTemp1;
            config.targetTemp1 = newTemp1;
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Heizkreis 1 Temperatur auf %.1f°C gesetzt", newTemp1);
            addLog(logMsg, 0);
        } else {
            message += "Temperatur 1 außerhalb des gültigen Bereichs (10-40°C). ";
            addLog("Ungültige Temperatur für Heizkreis 1", 1);
        }
    }
    
    if (server.hasArg("temp2")) {
        float newTemp2 = server.arg("temp2").toFloat();
        if (newTemp2 >= 10 && newTemp2 <= 40) {
            TARGET_TEMP2 = newTemp2;
            config.targetTemp2 = newTemp2;
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Heizkreis 2 Temperatur auf %.1f°C gesetzt", newTemp2);
            addLog(logMsg, 0);
        } else {
            message += "Temperatur 2 außerhalb des gültigen Bereichs (10-40°C). ";
            addLog("Ungültige Temperatur für Heizkreis 2", 1);
        }
    }
    
    if (changed) {
        saveConfig();
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Updated");
    } else {
        server.send(400, "text/plain", message);
    }
}

void handleSetPID() {
    bool changed = false;
    String message;
    
    auto validatePIDValue = [](float val, float min, float max, const char* name) -> String {
        if (val < min || val > max || isnan(val)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "%s muss zwischen %.1f und %.1f liegen. ", name, min, max);
            return String(msg);
        }
        return "";
    };
    
    // Heizkreis 1
    if (server.hasArg("kp1") && server.hasArg("ki1") && server.hasArg("kd1")) {
        float kp = server.arg("kp1").toFloat();
        float ki = server.arg("ki1").toFloat();
        float kd = server.arg("kd1").toFloat();
        
        message += validatePIDValue(kp, 0, 100, "Kp1");
        message += validatePIDValue(ki, 0, 1, "Ki1");
        message += validatePIDValue(kd, 0, 100, "Kd1");
        
        if (message.length() == 0) {
            config.Kp1 = kp;
            config.Ki1 = ki;
            config.Kd1 = kd;
            changed = true;
            addLog("PID Parameter für Heizkreis 1 aktualisiert", 0);
        }
    }
    
    // Heizkreis 2
    if (server.hasArg("kp2") && server.hasArg("ki2") && server.hasArg("kd2")) {
        float kp = server.arg("kp2").toFloat();
        float ki = server.arg("ki2").toFloat();
        float kd = server.arg("kd2").toFloat();
        
        message += validatePIDValue(kp, 0, 100, "Kp2");
        message += validatePIDValue(ki, 0, 1, "Ki2");
        message += validatePIDValue(kd, 0, 100, "Kd2");
        
        if (message.length() == 0) {
            config.Kp2 = kp;
            config.Ki2 = ki;
            config.Kd2 = kd;
            changed = true;
            addLog("PID Parameter für Heizkreis 2 aktualisiert", 0);
        }
    }
    
    if (changed) {
        saveConfig();
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Updated");
    } else {
        if (message.length() == 0) {
            message = "Keine gültigen Parameter übermittelt";
        }
        server.send(400, "text/plain", message);
    }
}

void handleSetWifi() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String newSSID = server.arg("ssid");
        String newPass = server.arg("pass");
        
        // Validiere SSID
        if (newSSID.length() < 1 || newSSID.length() > 32) {
            server.send(400, "text/plain", "SSID muss zwischen 1 und 32 Zeichen lang sein");
            return;
        }
        
        // Validiere Passwort
        if (newPass.length() < 8 || newPass.length() > 64) {
            server.send(400, "text/plain", "Passwort muss zwischen 8 und 64 Zeichen lang sein");
            return;
        }
        
        // Prüfe auf erlaubte Zeichen
        for (char c : newSSID) {
            if (!isalnum(c) && c != '_' && c != '-') {
                server.send(400, "text/plain", "SSID darf nur Buchstaben, Zahlen, - und _ enthalten");
                return;
            }
        }
        
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