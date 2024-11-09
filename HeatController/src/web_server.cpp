#include "web_server.h"
#include "logger.h"
#include "config.h"
#include "heating_control.h"
#include "data_logger.h"

ESP8266WebServer server(80);

// HTML Template for the main page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <title>Heat Controller</title>
    <style>
        :root {
            --primary-color: #ff3333;
            --background-color: #1a1a1a;
            --card-background: #2d2d2d;
            --text-color: #ffffff;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Segoe UI', Arial, sans-serif; line-height: 1.6; background-color: var(--background-color); color: var(--text-color); padding: 20px; }
        h2, h3 { color: var(--primary-color); margin-bottom: 15px; }
        .container { max-width: 1200px; margin: 0 auto; }
        .config { background: var(--card-background); border-radius: 10px; padding: 20px; margin: 20px 0; }
        button, input[type="submit"] { 
            background-color: var(--primary-color); 
            color: white; 
            border: none; 
            padding: 10px 20px; 
            border-radius: 5px; 
            cursor: pointer;
            font-size: 16px;
            margin: 10px 0;
        }
        button:hover, input[type="submit"]:hover {
            background-color: #ff4444;
        }
        .back-button { background-color: #ff3333; } /* Red for the Back button */
    </style>
</head>
<body>
    <div class="container">
        <h2>Heat Controller</h2>
        <div class="config">
            <h3>Operating Mode: %MODE_TEXT%</h3>
            <button onclick="window.location.href='/toggle_mode'">Toggle Mode</button>
        </div>

        <div class="config">
            <h3>Heating Circuit 1</h3>
            <div class="heating-circuit">
                <div class="temp-display">
                    <span>Current Temperature: </span>
                    <span class="temp-value">%TEMP1%°C</span>
                    <p>Target Temperature: %TARGET1%°C</p>
                    <p>Current Power: %PWM1%%</p>
                </div>
                <button onclick="window.location.href='/temperature_control?circuit=1'">Set Temperature</button>
            </div>
        </div>

        <div class="config">
            <h3>Heating Circuit 2</h3>
            <div class="heating-circuit">
                <div class="temp-display">
                    <span>Current Temperature: </span>
                    <span class="temp-value">%TEMP2%°C</span>
                    <p>Target Temperature: %TARGET2%°C</p>
                    <p>Current Power: %PWM2%%</p>
                </div>
                <button onclick="window.location.href='/temperature_control?circuit=2'">Set Temperature</button>
            </div>
        </div>

        <div class="config">
            <h3>WiFi Configuration</h3>
            <form action="/setwifi" method="post">
                <p><label>SSID:<br><input type="text" name="ssid" value="%WIFI_SSID%" required></label></p>
                <p><label>Password:<br><input type="password" name="pass" value="%WIFI_PASS%" required></label></p>
                <input type="submit" value="Save WiFi Settings">
            </form>
        </div>

        <div class="config">
            <h3>System Log</h3>
            <div class="log-container" id="logContainer">%SYSTEM_LOG%</div>
            <button onclick="clearLog()">Clear Log</button>
        </div>
    </div>

    <script>
        function clearLog() {
            fetch('/clearlog')
                .then(() => {
                    document.getElementById('logContainer').innerHTML = '';
                });
        }

        // Log update
        setInterval(function() {
            fetch('/getlog')
                .then(response => response.text())
                .then(log => {
                    document.getElementById('logContainer').innerHTML = log;
                });
        }, 5000);
    </script>
</body>
</html>
)rawliteral";

// HTML Template for the temperature control
const char temperature_control_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <title>Temperature Control</title>
    <style>
        :root {
            --primary-color: #ff3333;
            --background-color: #1a1a1a;
            --card-background: #2d2d2d;
            --text-color: #ffffff;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Segoe UI', Arial, sans-serif; line-height: 1.6; background-color: var(--background-color); color: var(--text-color); padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        .config { background: var(--card-background); border-radius: 10px; padding: 20px; margin: 20px 0; }
        .temp-display { font-size: 1.2em; margin: 15px 0; }
        .temp-value { font-size: 2em; font-weight: bold; color: var(--primary-color); }
        input[type="range"] { width: 100%; }
        button, input[type="submit"], .confirm-button { 
            background-color: var(--primary-color); 
            color: white; 
            border: none; 
            padding: 10px 20px; 
            border-radius: 5px; 
            cursor: pointer;
            font-size: 16px;
            margin: 10px 0;
        }

        button:hover, input[type="submit"]:hover, .confirm-button:hover {
            background-color: #ff4444;
        }

        .back-button {
            display: block;
            width: 200px;
            margin: 20px auto;
            text-align: center;
            background-color: #ff3333; /* Red for the Back button */
        }

        .pid-controls {
            margin-top: 20px;
            padding: 15px;
            border-top: 1px solid #444;
        }
        
        .response-slider {
            width: 100%;
            margin: 20px 0;
        }
        
        .slider-labels {
            display: flex;
            justify-content: space-between;
            margin: 10px 0;
            color: #888;
        }
        
        .advanced-toggle {
            color: #888;
            text-decoration: underline;
            cursor: pointer;
            margin: 10px 0;
            display: inline-block;
        }
        
        .advanced-settings {
            display: none;
            margin-top: 15px;
            padding-top: 15px;
            border-top: 1px dashed #444;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Temperature Control</h2>
        <div class="config">
            <h3>Heating Circuit %CIRCUIT%</h3>
            <div class="heating-circuit">
                <form action="/settemp" method="get" class="temp-control" id="tempForm" onsubmit="return prepareSubmit()">
                    <div class="temp-display">
                        <label for="temp">Set Target Temperature:</label>
                        <input type="range" id="temperature" name="temp" min="10" max="40" step="0.5" value="%TARGET%" oninput="updateTemperature(this.value)">
                        <span id="tempDisplay">%TARGET%°C</span>
                    </div>
                    <input type="hidden" name="circuit" value="%CIRCUIT%">
                    
                    <div class="pid-controls">
                        <h4>Control Response</h4>
                        <div class="slider-labels">
                            <span>Slow & Stable</span>
                            <span>Balanced</span>
                            <span>Fast & Aggressive</span>
                        </div>
                        <input type="range" id="response" name="response" min="0" max="100" value="%RESPONSE%" 
                               class="response-slider" oninput="updateResponse(this.value)">
                        
                        <a class="advanced-toggle" onclick="toggleAdvanced()">Show Advanced Settings</a>
                        
                        <div class="advanced-settings" id="advancedSettings">
                            <h4>PID Controller Settings</h4>
                            <label>
                                Kp (Proportional):
                                <input type="number" id="kp" name="kp" value="%KP%" step="0.1">
                            </label>
                            <label>
                                Ki (Integral):
                                <input type="number" id="ki" name="ki" value="%KI%" step="0.1">
                            </label>
                            <label>
                                Kd (Derivative):
                                <input type="number" id="kd" name="kd" value="%KD%" step="0.1">
                            </label>
                        </div>
                    </div>
                    
                    <button type="submit" class="confirm-button">Save Settings</button>
                </form>
            </div>
        </div>
        <button onclick="window.location.href='/'" class="back-button">Back to Main</button>
    </div>
    <script>
        function updateTemperature(value) {
            document.getElementById('tempDisplay').innerText = value + '°C';
        }
        
        function toggleAdvanced() {
            const advancedSettings = document.getElementById('advancedSettings');
            advancedSettings.style.display = advancedSettings.style.display === 'none' ? 'block' : 'none';
        }
        
        function updateResponse(value) {
            // Convert slider value (0-100) to PID values
            const v = value / 100; // normalize to 0-1
            
            // Exponential mapping for more intuitive control
            const kp = (1 + Math.exp(v * 2 - 1)) * 2; // Range ~2-10
            const ki = v * 0.5;                        // Range 0-0.5
            const kd = v * v * 2;                      // Range 0-2
            
            // Update PID inputs
            const kpInput = document.getElementById('kp');
            const kiInput = document.getElementById('ki');
            const kdInput = document.getElementById('kd');
            
            kpInput.value = kp.toFixed(1);
            kiInput.value = ki.toFixed(1);
            kdInput.value = kd.toFixed(1);
            
            // Trigger change event to ensure form picks up the new values
            kpInput.dispatchEvent(new Event('change'));
            kiInput.dispatchEvent(new Event('change'));
            kdInput.dispatchEvent(new Event('change'));
        }
        
        function prepareSubmit() {
            // Ensure PID values are included in the form submission
            const kp = document.getElementById('kp').value;
            const ki = document.getElementById('ki').value;
            const kd = document.getElementById('kd').value;
            
            // Log the values being sent (for debugging)
            console.log('Submitting PID values:', {kp, ki, kd});
            
            // Always return true to allow form submission
            return true;
        }
        
        // Initialize advanced values from response slider
        updateResponse(document.getElementById('response').value);
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    String html = String(index_html);
    html.replace("%TEMP1%", String(currentTemp1, 1));
    html.replace("%TARGET1%", String(TARGET_TEMP1, 1));
    html.replace("%PWM1%", String((float)currentPWM1 / MAX_PWM * 100, 0));
    html.replace("%TEMP2%", String(currentTemp2, 1));
    html.replace("%TARGET2%", String(TARGET_TEMP2, 1));
    html.replace("%PWM2%", String((float)currentPWM2 / MAX_PWM * 100, 0));
    html.replace("%WIFI_SSID%", String(config.ssid)); // WiFi SSID
    html.replace("%WIFI_PASS%", String(config.password)); // WiFi Passwort
    html.replace("%SYSTEM_LOG%", getLog()); // System Log

    if (rtcData.magic == MAGIC_HEATER_ON) {
        html.replace("%MODE_TEXT%", "FULL POWER (100%)");
    } else {
        html.replace("%MODE_TEXT%", "NORMAL (Temperature Control)");
    }
    
    server.send(200, "text/html", html);
}

void handleTemperatureControl() {
    int circuit = server.arg("circuit").toInt();
    String html = String(temperature_control_html);
    
    float targetTemp = (circuit == 1) ? TARGET_TEMP1 : TARGET_TEMP2;
    float kp = (circuit == 1) ? config.Kp1 : config.Kp2;
    float ki = (circuit == 1) ? config.Ki1 : config.Ki2;
    float kd = (circuit == 1) ? config.Kd1 : config.Kd2;
    
    // Calculate response value (0-100) from PID values
    float response = ((kp - 2) / 8) * 100; // Rough inverse of the kp calculation
    response = constrain(response, 0, 100);
    
    html.replace("%TARGET%", String(targetTemp, 1));
    html.replace("%CIRCUIT%", String(circuit));
    html.replace("%KP%", String(kp, 1));
    html.replace("%KI%", String(ki, 1));
    html.replace("%KD%", String(kd, 1));
    html.replace("%RESPONSE%", String(response, 0));

    server.send(200, "text/html", html);
}

void handleSetTemp() {
    char debugMsg[128];
    snprintf(debugMsg, sizeof(debugMsg), "Received args - temp: %s, kp: %s, ki: %s, kd: %s", 
        server.hasArg("temp") ? server.arg("temp").c_str() : "none",
        server.hasArg("kp") ? server.arg("kp").c_str() : "none",
        server.hasArg("ki") ? server.arg("ki").c_str() : "none",
        server.hasArg("kd") ? server.arg("kd").c_str() : "none");
    addLog(debugMsg, 0);
    
    bool changed = false;
    String message;
    int circuit = server.arg("circuit").toInt();

    // Check and update temperature
    if (server.hasArg("temp")) {
        float newTemp = server.arg("temp").toFloat();
        
        if (newTemp >= 10 && newTemp <= 40) {
            if (circuit == 1) {
                if (TARGET_TEMP1 != newTemp) {
                    TARGET_TEMP1 = newTemp;
                    config.targetTemp1 = newTemp;
                    changed = true;
                }
            } else {
                if (TARGET_TEMP2 != newTemp) {
                    TARGET_TEMP2 = newTemp;
                    config.targetTemp2 = newTemp;
                    changed = true;
                }
            }
            
            if (changed) {
                char logMsg[64];
                snprintf(logMsg, sizeof(logMsg), "Heating Circuit %d Temperature set to %.1f°C", circuit, newTemp);
                addLog(logMsg, 0);
            }
        } else {
            message += "Temperature outside the valid range (10-40°C). ";
            addLog("Invalid temperature for Heating Circuit", 1);
        }
    }

    // Check and update PID parameters
    if (circuit == 1) {
        if (server.hasArg("kp") && config.Kp1 != server.arg("kp").toFloat()) {
            config.Kp1 = server.arg("kp").toFloat();
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Circuit 1 Kp set to %.1f", config.Kp1);
            addLog(logMsg, 0);
        }
        if (server.hasArg("ki") && config.Ki1 != server.arg("ki").toFloat()) {
            config.Ki1 = server.arg("ki").toFloat();
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Circuit 1 Ki set to %.1f", config.Ki1);
            addLog(logMsg, 0);
        }
        if (server.hasArg("kd") && config.Kd1 != server.arg("kd").toFloat()) {
            config.Kd1 = server.arg("kd").toFloat();
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Circuit 1 Kd set to %.1f", config.Kd1);
            addLog(logMsg, 0);
        }
    } else {
        if (server.hasArg("kp") && config.Kp2 != server.arg("kp").toFloat()) {
            config.Kp2 = server.arg("kp").toFloat();
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Circuit 2 Kp set to %.1f", config.Kp2);
            addLog(logMsg, 0);
        }
        if (server.hasArg("ki") && config.Ki2 != server.arg("ki").toFloat()) {
            config.Ki2 = server.arg("ki").toFloat();
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Circuit 2 Ki set to %.1f", config.Ki2);
            addLog(logMsg, 0);
        }
        if (server.hasArg("kd") && config.Kd2 != server.arg("kd").toFloat()) {
            config.Kd2 = server.arg("kd").toFloat();
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Circuit 2 Kd set to %.1f", config.Kd2);
            addLog(logMsg, 0);
        }
    }

    if (changed) {
        saveConfig(); // Save all changes to EEPROM
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Updated");
    } else {
        // If nothing changed but we have a temperature, it's still a valid request
        if (server.hasArg("temp")) {
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "No changes needed");
        } else {
            server.send(400, "text/plain", message);
        }
    }
}

void handleToggleMode() {
    if (rtcData.magic == MAGIC_NORMAL) {
        rtcData.magic = MAGIC_HEATER_ON;
        config.lastMode = MAGIC_HEATER_ON;
        addLog("Switched to FULL POWER mode", 0);
    } else {
        rtcData.magic = MAGIC_NORMAL;
        config.lastMode = MAGIC_NORMAL;
        addLog("Switched to NORMAL mode", 0);
    }
    saveConfig(); // Speichern Sie die Konfiguration
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Mode toggled");
}

void setupWebServer() {
    // Load saved temperatures from config
    TARGET_TEMP1 = config.targetTemp1;
    TARGET_TEMP2 = config.targetTemp2;
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/temperature_control", HTTP_GET, handleTemperatureControl);
    server.on("/settemp", HTTP_GET, handleSetTemp);
    server.on("/toggle_mode", HTTP_GET, handleToggleMode);
    server.on("/getlog", HTTP_GET, handleGetLog);     // This function is implemented in logger.cpp
    server.on("/clearlog", HTTP_GET, handleClearLog); // This function is implemented in logger.cpp
    
    // Captive Portal Routes
    server.on("/generate_204", HTTP_GET, handleRoot);
    server.on("/gen_204", HTTP_GET, handleRoot);
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
    
    server.begin();
    addLog("Webserver started", 0);
}

void handleWebServer() {
    server.handleClient();
}

// Implement the remaining handler functions from the original main.cpp