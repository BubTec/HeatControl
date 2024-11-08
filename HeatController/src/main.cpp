#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <AddrList.h>

#define DEBUG_SERIAL

// Pins
#define ONE_WIRE_BUS 2
#define MOSFET1_PIN 4
#define MOSFET2_PIN 5

// Temperatur-Einstellungen
#define TEMP_TOLERANCE 0.5
#define MAX_PWM 1023
#define MIN_PWM 0

// Regelparameter
#define CONTROL_INTERVAL 10000  // Erhöht von 1000ms auf 10000ms (10 Sekunden)
#define TEMP_HISTORY_SIZE 6     // 1 Minute Temperaturverlauf (6 x 10 Sekunden)

// WLAN Einstellungen
// const char* ssid = "TempController";
// const char* password = "12345678";

// DNS Server Einstellungen
const byte DNS_PORT = 53;

// Globale Variablen
float TARGET_TEMP1 = 22.0;
float TARGET_TEMP2 = 22.0;
float currentTemp1 = 0;
float currentTemp2 = 0;
int currentPWM1 = 0;
int currentPWM2 = 0;
float integral1 = 0;
float integral2 = 0;
unsigned long lastControlTime = 0;

// Temperaturverlauf
float tempHistory1[TEMP_HISTORY_SIZE] = {0};
float tempHistory2[TEMP_HISTORY_SIZE] = {0};
int historyIndex = 0;

// Objekte initialisieren
ESP8266WebServer server(80);
DNSServer dnsServer;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1, sensor2;

// Struktur für die Konfiguration
struct Config {
    char ssid[33];        // 32 Zeichen + Null-Terminator
    char password[65];    // 64 Zeichen + Null-Terminator
    float targetTemp1;    // Zieltemperatur Heizkreis 1
    float targetTemp2;    // Zieltemperatur Heizkreis 2
    float Kp1;
    float Ki1;
    float Kd1;
    float Kp2;
    float Ki2;
    float Kd2;
    byte magic;          // Validierung
};

// Globale Konfigurationsvariable
Config config;

// EEPROM Einstellungen
#define EEPROM_SIZE 512
#define EEPROM_MAGIC 0xAB  // Magic number zur Validierung

// Log-System Definitionen
#define MAX_LOG_ENTRIES 50
#define MAX_LOG_LENGTH 2048

// Log-Einträge Struktur
struct LogEntry {
    char message[64];
    uint8_t type;  // 0=info, 1=warning, 2=error
    unsigned long timestamp;
};

// Globale Log-Variablen
LogEntry logEntries[MAX_LOG_ENTRIES];
int logIndex = 0;
int logCount = 0;

// Forward Declarations
void saveConfig();
void loadConfig();
static void handleRoot();
static void handleSetTemp();
static float calculateHeatingPower1(float currentTemp);
static float calculateHeatingPower2(float currentTemp);
void updateHeating();
void handleSetPID();
void addLog(const char* message, uint8_t type);
void handleGetLog();
void handleClearLog();

// Neue Funktionen für die Konfiguration
void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, config);
    EEPROM.end();
    
    if (config.magic != EEPROM_MAGIC) {
        Serial.println("Keine valide Konfiguration gefunden, nutze Standardwerte");
        strncpy(config.ssid, "HeatControl", sizeof(config.ssid));
        strncpy(config.password, "12345678", sizeof(config.password));
        config.targetTemp1 = 22.0;
        config.targetTemp2 = 22.0;
        config.Kp1 = 20.0;
        config.Ki1 = 0.02;
        config.Kd1 = 10.0;
        config.Kp2 = 20.0;
        config.Ki2 = 0.02;
        config.Kd2 = 10.0;
        config.magic = EEPROM_MAGIC;
        saveConfig();
    }
    
    TARGET_TEMP1 = config.targetTemp1;
    TARGET_TEMP2 = config.targetTemp2;
}

void saveConfig() {
    config.magic = EEPROM_MAGIC;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("Konfiguration gespeichert");
}

// HTML Seite
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

        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 5px;
        }

        .status-active { background-color: var(--success-color); }
        .status-inactive { background-color: var(--error-color); }

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
            <h3>Heizkreis 1</h3>
            <div class="heizkreis">
                <div class="temp-display">
                    <div class="status-indicator status-active"></div>
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
                    <div class="status-indicator status-active"></div>
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
        // Automatisches Aktualisieren des Logs
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

// Neue Handler-Funktion für WLAN-Einstellungen
void handleSetWifi() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String newSSID = server.arg("ssid");
        String newPass = server.arg("pass");
        
        // Konfiguration aktualisieren
        strncpy(config.ssid, newSSID.c_str(), sizeof(config.ssid) - 1);
        strncpy(config.password, newPass.c_str(), sizeof(config.password) - 1);
        config.ssid[sizeof(config.ssid) - 1] = '\0';
        config.password[sizeof(config.password) - 1] = '\0';
        
        saveConfig();
        
        server.send(200, "text/html", 
            "<html><body>"
            "<h2>Einstellungen gespeichert</h2>"
            "<p>Das Gerät startet in 3 Sekunden neu...</p>"
            "<script>setTimeout(function(){ window.location.href='/' }, 3000);</script>"
            "</body></html>");
            
        delay(500);  // Warte bis Antwort gesendet
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Fehlende Parameter");
    }
}

// Neue Handler-Funktion für PID-Einstellungen
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
    }
    
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Updated");
}

// Funktionsimplementierungen
static void handleRoot() {
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
    
    // Generiere Log-Einträge für die initiale Anzeige
    String logContent = "";
    int start = (logCount < MAX_LOG_ENTRIES) ? 0 : logIndex;
    
    for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % MAX_LOG_ENTRIES;
        String timeStr = String(logEntries[idx].timestamp / 1000);
        String typeClass = "";
        
        switch(logEntries[idx].type) {
            case 1: typeClass = "warning"; break;
            case 2: typeClass = "error"; break;
            default: typeClass = "info";
        }
        
        logContent += "<div class='" + typeClass + "'>[" + timeStr + "s] " + 
                     String(logEntries[idx].message) + "</div>\n";
    }
    
    html.replace("%SYSTEM_LOG%", logContent);
    
    server.send(200, "text/html", html);
}

static void handleSetTemp() {
    bool changed = false;
    
    if (server.hasArg("temp1")) {
        float newTemp1 = server.arg("temp1").toFloat();
        if (newTemp1 >= 5 && newTemp1 <= 30) {
            TARGET_TEMP1 = newTemp1;
            config.targetTemp1 = newTemp1;
            changed = true;
            Serial.printf("Heizkreis 1 Temperatur auf %.1f°C gesetzt\n", newTemp1);
        }
    }
    if (server.hasArg("temp2")) {
        float newTemp2 = server.arg("temp2").toFloat();
        if (newTemp2 >= 5 && newTemp2 <= 30) {
            TARGET_TEMP2 = newTemp2;
            config.targetTemp2 = newTemp2;
            changed = true;
            Serial.printf("Heizkreis 2 Temperatur auf %.1f°C gesetzt\n", newTemp2);
        }
    }
    
    if (changed) {
        saveConfig();
    }
    
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Updated");
}

static float calculateHeatingPower1(float currentTemp) {
    float error = TARGET_TEMP1 - currentTemp;
    
    // Speichere aktuelle Temperatur im Verlauf
    tempHistory1[historyIndex] = currentTemp;
    
    // Berechne Temperaturänderung (Derivative)
    float tempChange = 0;
    if (TEMP_HISTORY_SIZE > 1) {
        int prevIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
        tempChange = (currentTemp - tempHistory1[prevIndex]) / (CONTROL_INTERVAL / 1000.0);
    }
    
    // Integral nur anpassen wenn wir nicht zu weit vom Ziel entfernt sind
    if (abs(error) < 5.0) {  // Erhöht von 2.0 auf 5.0 Grad
        integral1 += error * (CONTROL_INTERVAL / 1000.0);
    }
    
    // Begrenzen Sie das Integral stärker für stabileres Verhalten
    integral1 = constrain(integral1, -30, 30);
    
    // PID Berechnung
    float output = (config.Kp1 * error) + (config.Ki1 * integral1) - (config.Kd1 * tempChange);
    
    // Sanftere Begrenzung des Ausgangs
    output = constrain(output, MIN_PWM, MAX_PWM);
    
    // Anti-Windup: Integral zurücksetzen wenn Ausgang in Sättigung
    if (output >= MAX_PWM || output <= MIN_PWM) {
        integral1 *= 0.9;  // Langsam reduzieren statt hartem Reset
    }
    
    return output;
}

static float calculateHeatingPower2(float currentTemp) {
    float error = TARGET_TEMP2 - currentTemp;
    
    tempHistory2[historyIndex] = currentTemp;
    
    float tempChange = 0;
    if (TEMP_HISTORY_SIZE > 1) {
        int prevIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
        tempChange = (currentTemp - tempHistory2[prevIndex]) / (CONTROL_INTERVAL / 1000.0);
    }
    
    if (abs(error) < 5.0) {
        integral2 += error * (CONTROL_INTERVAL / 1000.0);
    }
    
    integral2 = constrain(integral2, -30, 30);
    
    float output = (config.Kp2 * error) + (config.Ki2 * integral2) - (config.Kd2 * tempChange);
    output = constrain(output, MIN_PWM, MAX_PWM);
    
    if (output >= MAX_PWM || output <= MIN_PWM) {
        integral2 *= 0.9;
    }
    
    return output;
}

bool isIP(String str) {
    for (size_t i = 0; i < str.length(); i++) {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9')) {
            return false;
        }
    }
    return true;
}

String toStringIp(IPAddress ip) {
    String res = "";
    for (int i = 0; i < 3; i++) {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

void addLog(const char* message, uint8_t type) {
    unsigned long now = millis();
    strncpy(logEntries[logIndex].message, message, 63);
    logEntries[logIndex].message[63] = '\0';
    logEntries[logIndex].type = type;
    logEntries[logIndex].timestamp = now;
    
    logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
    if (logCount < MAX_LOG_ENTRIES) logCount++;
}

void handleGetLog() {
    String log = "";
    int start = (logCount < MAX_LOG_ENTRIES) ? 0 : logIndex;
    
    for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % MAX_LOG_ENTRIES;
        String timeStr = String(logEntries[idx].timestamp / 1000);
        String typeClass = "";
        
        switch(logEntries[idx].type) {
            case 1: typeClass = "warning"; break;
            case 2: typeClass = "error"; break;
            default: typeClass = "info";
        }
        
        log += "<div class='" + typeClass + "'>[" + timeStr + "s] " + 
               String(logEntries[idx].message) + "</div>\n";
    }
    
    server.send(200, "text/html", log);
}

void handleClearLog() {
    logIndex = 0;
    logCount = 0;
    server.send(200, "text/plain", "OK");
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== TempController Start ===");
    
    Serial.println("\nStarte WiFi Access Point...");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.persistent(false);
    
    // Lade Konfiguration vor WiFi Setup
    loadConfig();
    
    Serial.println("Konfiguriere AP...");
    Serial.printf("- SSID: %s\n", config.ssid);
    Serial.printf("- Passwort: %s\n", config.password);
    
    bool cfgResult = WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    Serial.printf("- AP Konfiguration: %s\n", cfgResult ? "OK" : "FEHLER");
    
    bool apResult = WiFi.softAP(config.ssid, config.password);
    Serial.printf("- AP Start: %s\n", apResult ? "OK" : "FEHLER");
    
    Serial.print("- AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    // DNS und Captive Portal Setup
    WiFi.mode(WIFI_AP);
    WiFi.persistent(false);
    
    // Lade Konfiguration vor WiFi Setup
    loadConfig();
    
    // Access Point konfigurieren
    WiFi.softAPConfig(IPAddress(4,3,2,1), IPAddress(4,3,2,1), IPAddress(255,255,255,0));
    WiFi.softAP(config.ssid, config.password);
    
    Serial.println("AP gestartet");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    // DNS Server für Captive Portal
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", IPAddress(4,3,2,1));
    
    // Webserver Routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settemp", HTTP_GET, handleSetTemp);
    server.on("/setwifi", HTTP_POST, handleSetWifi);
    server.on("/setpid", HTTP_GET, handleSetPID);
    server.on("/getlog", HTTP_GET, handleGetLog);
    server.on("/clearlog", HTTP_GET, handleClearLog);
    
    // Spezielle URLs für Captive Portal
    server.on("/generate_204", HTTP_GET, []() { handleRoot(); });  // Android
    server.on("/gen_204", HTTP_GET, []() { handleRoot(); });       // Android
    server.on("/fwlink", HTTP_GET, []() { handleRoot(); });        // Microsoft
    server.on("/hotspot-detect.html", HTTP_GET, []() { handleRoot(); }); // Apple
    server.on("/canonical.html", HTTP_GET, []() { handleRoot(); });
    server.on("/success.txt", HTTP_GET, []() { handleRoot(); });
    server.on("/ncsi.txt", HTTP_GET, []() { handleRoot(); });
    
    // Captive Portal Response
    server.onNotFound([]() {
        Serial.print("Captive Portal Request: ");
        Serial.println(server.uri());
        
        String content = String("")
            + "<HTML><HEAD><TITLE>Success</TITLE>"
            + "<meta http-equiv='Refresh' content='0; url=http://4.3.2.1/'>"
            + "</HEAD><BODY>"
            + "Redirecting... Click <a href='http://4.3.2.1/'>here</a> if you are not redirected."
            + "<script>window.location.href='http://4.3.2.1/';</script>"
            + "</BODY></HTML>";
            
        server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "-1");
        server.send(200, "text/html", content);
    });
    
    server.begin();
    Serial.println("HTTP Server gestartet");
    
    pinMode(MOSFET1_PIN, OUTPUT);
    pinMode(MOSFET2_PIN, OUTPUT);
    
    analogWriteFreq(1000);
    analogWriteRange(MAX_PWM);
    
    sensors.begin();
    
    addLog("System gestartet", 0);  // Info
    if(sensors.getDeviceCount() < 2) {
        addLog("Nicht genügend Temperatursensoren gefunden!", 1);  // Warning
    } else {
        addLog("Temperatursensoren erfolgreich initialisiert", 0);  // Info
    }
    
    if(sensors.getDeviceCount() > 0) {
        sensors.getAddress(sensor1, 0);
        sensors.setResolution(sensor1, 12);
        if(sensors.getDeviceCount() > 1) {
            sensors.getAddress(sensor2, 1);
            sensors.setResolution(sensor2, 12);
        }
    }
    
    loadConfig();
}

void loop() {
    static unsigned long lastClientCheck = 0;
    static uint8_t lastClientCount = 0;
    
    // DNS Anfragen verarbeiten
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - lastClientCheck > 5000) {
        lastClientCheck = millis();
        uint8_t currentClientCount = WiFi.softAPgetStationNum();
        
        Serial.println("\n--- Status Update ---");
        Serial.printf("Verbundene Clients: %d\n", currentClientCount);
        Serial.printf("Freier Heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Verbunden" : "Nicht verbunden");
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        
        if (currentClientCount != lastClientCount) {
            Serial.printf("Client-Anzahl geändert: %d -> %d\n", lastClientCount, currentClientCount);
            lastClientCount = currentClientCount;
        }
    }
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastControlTime >= CONTROL_INTERVAL) {
        lastControlTime = currentMillis;
        
        Serial.println("\n=== Temperatur Update ===");
        updateHeating();
        
        #ifdef DEBUG_SERIAL
            Serial.println("\nStatus:");
            Serial.printf("Heizkreis 1: Soll=%.1f°C Ist=%.1f°C PWM=%d%%\n", 
                TARGET_TEMP1, currentTemp1, (int)((float)currentPWM1/MAX_PWM * 100));
            Serial.printf("Heizkreis 2: Soll=%.1f°C Ist=%.1f°C PWM=%d%%\n", 
                TARGET_TEMP2, currentTemp2, (int)((float)currentPWM2/MAX_PWM * 100));
            Serial.printf("Sensoren gefunden: %d\n", sensors.getDeviceCount());
            Serial.printf("Freier Speicher: %d bytes\n", ESP.getFreeHeap());
        #endif
    }
    
    // Captive Portal Status
    static unsigned long lastPortalCheck = 0;
    if (millis() - lastPortalCheck > 1000) {  // Jede Sekunde
        lastPortalCheck = millis();
        if (WiFi.softAPgetStationNum() > 0) {
            Serial.println("Client verbunden - Captive Portal aktiv");
        }
    }
}

void updateHeating() {
    if(sensors.getDeviceCount() > 0) {
        sensors.requestTemperatures();
        
        // Heizkreis 1
        float temp1 = sensors.getTempC(sensor1);
        if(temp1 != DEVICE_DISCONNECTED_C) {
            currentTemp1 = temp1;
            currentPWM1 = (int)calculateHeatingPower1(currentTemp1);
            analogWrite(MOSFET1_PIN, currentPWM1);
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "HK1: %.1f°C -> %d%% PWM", currentTemp1, (int)((float)currentPWM1/MAX_PWM * 100));
            addLog(logMsg, 0);
        } else {
            addLog("Heizkreis 1: Sensor Fehler!", 2);  // Error
            currentPWM1 = 0;
            analogWrite(MOSFET1_PIN, 0);
        }
        
        // Heizkreis 2
        if(sensors.getDeviceCount() > 1) {
            float temp2 = sensors.getTempC(sensor2);
            if(temp2 != DEVICE_DISCONNECTED_C) {
                currentTemp2 = temp2;
                currentPWM2 = (int)calculateHeatingPower2(currentTemp2);
                analogWrite(MOSFET2_PIN, currentPWM2);
                char logMsg[64];
                snprintf(logMsg, sizeof(logMsg), "HK2: %.1f°C -> %d%% PWM", currentTemp2, (int)((float)currentPWM2/MAX_PWM * 100));
                addLog(logMsg, 0);
            } else {
                addLog("Heizkreis 2: Sensor Fehler!", 2);  // Error
                currentPWM2 = 0;
                analogWrite(MOSFET2_PIN, 0);
            }
        }
    } else {
        addLog("Keine Sensoren gefunden!", 2);  // Error
        currentPWM1 = 0;
        currentPWM2 = 0;
        analogWrite(MOSFET1_PIN, 0);
        analogWrite(MOSFET2_PIN, 0);
    }
    
    historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
}