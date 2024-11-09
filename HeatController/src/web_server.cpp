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
        button { background-color: var(--primary-color); color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; }
        .back-button { background-color: #ff3333; } /* Red for the Back button */
    </style>
</head>
<body>
    <div class="container">
        <h2>Heat Controller</h2>
        <div class="config">
            <h3>Operating Mode: %MODE_TEXT%</h3>
        </div>

        <div class="config">
            <h3>Heating Circuit 1</h3>
            <div class="heating-circuit">
                <div class="temp-display">
                    <span>Current Temperature: </span>
                    <span class="temp-value">%TEMP1%°C</span>
                    <p>Target Temperature: %TARGET1%°C</p>
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
    </style>
</head>
<body>
    <div class="container">
        <h2>Temperature Control</h2>
        <div class="config">
            <h3>Heating Circuit %CIRCUIT%</h3>
            <div class="heating-circuit">
                <form action="/settemp" method="get" class="temp-control" id="tempForm">
                    <div class="temp-display">
                        <label for="temp">Set Target Temperature:</label>
                        <input type="range" id="temperature" name="temp" min="10" max="40" step="0.5" value="%TARGET%" oninput="updateTemperature(this.value)">
                        <span id="tempDisplay">%TARGET%°C</span>
                    </div>
                    <input type="hidden" name="circuit" value="%CIRCUIT%">
                    <button type="submit" class="confirm-button">Set Temperature</button>
                </form>
            </div>
        </div>
        <button onclick="window.location.href='/'" class="back-button">Back to Main</button>
    </div>
    <script>
        function updateTemperature(value) {
            document.getElementById('tempDisplay').innerText = value + '°C';
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
    html.replace("%TEMP2%", String(currentTemp2, 1));
    html.replace("%TARGET2%", String(TARGET_TEMP2, 1));
    html.replace("%PWM2%", String((float)currentPWM2/MAX_PWM * 100, 0));
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
    html.replace("%TARGET%", String(targetTemp, 1));
    html.replace("%CIRCUIT%", String(circuit));

    server.send(200, "text/html", html);
}

void handleSetTemp() {
    bool changed = false;
    String message;

    if (server.hasArg("temp")) {
        float newTemp = server.arg("temp").toFloat();
        int circuit = server.arg("circuit").toInt();
        
        if (newTemp >= 10 && newTemp <= 40) {
            if (circuit == 1) {
                TARGET_TEMP1 = newTemp;
                config.targetTemp1 = newTemp;
            } else {
                TARGET_TEMP2 = newTemp;
                config.targetTemp2 = newTemp;
            }
            changed = true;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Heating Circuit %d Temperature set to %.1f°C", circuit, newTemp);
            addLog(logMsg, 0);
        } else {
            message += "Temperature outside the valid range (10-40°C). ";
            addLog("Invalid temperature for Heating Circuit", 1);
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

void setupWebServer() {
    // Load saved temperatures from config
    TARGET_TEMP1 = config.targetTemp1;
    TARGET_TEMP2 = config.targetTemp2;
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/temperature_control", HTTP_GET, handleTemperatureControl);
    server.on("/settemp", HTTP_GET, handleSetTemp);
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