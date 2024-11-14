#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

// HTTP Method definitions
#ifndef HTTP_GET
#define HTTP_GET     0x01
#define HTTP_POST    0x02
#define HTTP_DELETE  0x04
#define HTTP_PUT     0x08
#define HTTP_PATCH   0x10
#define HTTP_HEAD    0x20
#define HTTP_OPTIONS 0x40
#define HTTP_ANY     0x7F
#endif

// Pin definitions
#define ONE_WIRE_BUS 2
#define MOSFET_PIN_1 4
#define MOSFET_PIN_2 5
#define INPUT_PIN 14
#define SIGNAL_PIN 12

// Global variables
float targetTemp1 = 23.0;
float targetTemp2 = 23.0;
float currentTemp1 = 0.0;
float currentTemp2 = 0.0;
bool swapAssignment = false; // Variable for sensor assignment
bool powerMode = false;      // Stores the mode selected at startup

// Initialize objects
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);

// WiFi settings
String activeSSID = "HeatControl";     // Active SSID
String activePassword = "HeatControl"; // Active password

// EEPROM addresses
#define EEPROM_INIT_ADDR 0
#define EEPROM_TEMP1_ADDR 1
#define EEPROM_TEMP2_ADDR (EEPROM_TEMP1_ADDR + sizeof(float))
#define EEPROM_SSID_ADDR (EEPROM_TEMP2_ADDR + sizeof(float))
#define EEPROM_PASS_ADDR (EEPROM_SSID_ADDR + 32)
#define EEPROM_SWAP_ADDR (EEPROM_PASS_ADDR + 32)

// Forward declaration of functions
void saveWiFiCredentials(const String& ssid, const String& password);

// Function to load WiFi credentials
void loadWiFiCredentials() {
    EEPROM.begin(512);
    
    // Check if EEPROM is initialized
    byte initFlag = EEPROM.read(EEPROM_INIT_ADDR);
    if(initFlag != 0xAA) {
        Serial.println("First time setup - using defaults");
        EEPROM.write(EEPROM_INIT_ADDR, 0xAA);
        EEPROM.commit();
        saveWiFiCredentials("HeatControl", "HeatControl");
        return;
    }
    
    char storedSSID[32] = {0};
    char storedPassword[32] = {0};
    
    // Read SSID
    for (int i = 0; i < 31; i++) {
        storedSSID[i] = EEPROM.read(EEPROM_SSID_ADDR + i);
    }
    
    // Read password
    for (int i = 0; i < 31; i++) {
        storedPassword[i] = EEPROM.read(EEPROM_PASS_ADDR + i);
    }
    
    EEPROM.end();

    // Debug output
    Serial.println("\nLoaded WiFi credentials:");
    Serial.print("SSID: "); Serial.println(storedSSID);
    Serial.print("Password length: "); Serial.println(strlen(storedPassword));

    if(strlen(storedSSID) > 0 && storedSSID[0] != 0xFF) {
        activeSSID = String(storedSSID);
        activePassword = String(storedPassword);
    } else {
        Serial.println("Invalid stored SSID - using defaults");
        activeSSID = "HeatControl";
        activePassword = "HeatControl";
        saveWiFiCredentials(activeSSID, activePassword);
    }
}

// Function to save WiFi credentials
void saveWiFiCredentials(const String& ssid, const String& password) {
    if(ssid.length() == 0) return;  // Prevent empty SSID
    
    EEPROM.begin(512);
    
    // Clear the area
    for(int i = 0; i < 64; i++) {
        EEPROM.write(EEPROM_SSID_ADDR + i, 0);
    }
    
    // Write SSID
    for(size_t i = 0; i < ssid.length() && i < 31; i++) {
        EEPROM.write(EEPROM_SSID_ADDR + i, ssid[i]);
    }
    
    // Write password
    for(size_t i = 0; i < password.length() && i < 31; i++) {
        EEPROM.write(EEPROM_PASS_ADDR + i, password[i]);
    }
    
    if(EEPROM.commit()) {
        Serial.println("WiFi settings saved successfully");
        activeSSID = ssid;
        activePassword = password;
    } else {
        Serial.println("Error saving WiFi settings!");
    }
    
    EEPROM.end();
}

// HTML Template - replace the existing template code with this:
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HeatControl</title>
  <style>
    html { 
      font-family: Arial; 
      text-align: center; 
      background-color: #1a1a1a; 
      color: white; 
    }
    body { 
      margin: 0; 
      padding: 20px; 
    }
    h2 { 
      font-size: 2.0rem; 
      background-color: #e74c3c;
      margin: 0;
      padding: 15px;
      margin-bottom: 20px;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
    }
    .box {
      background-color: #333;
      padding: 20px;
      border-radius: 8px;
      margin-bottom: 20px;
    }
    .status {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 4px;
      margin: 10px 0;
    }
    .status-on { background-color: #e74c3c; }
    .status-off { background-color: #666; }
    input[type="number"], input[type="text"], input[type="password"] {
      background-color: #444;
      border: 1px solid #666;
      color: white;
      padding: 8px;
      border-radius: 4px;
      width: 100px;
      margin: 5px;
    }
    input[type="submit"] {
      background-color: #e74c3c;
      color: white;
      padding: 10px 20px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      margin: 10px;
    }
    input[type="submit"]:hover {
      background-color: #c0392b;
    }

    /* New styles for temperature control */
    .temp-control {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 15px;
      margin: 15px 0;
    }
    .temp-button {
      width: 50px;
      height: 50px;
      font-size: 1.8rem;
      background-color: #e74c3c;
      color: white;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      padding: 0;
      line-height: 50px;
      touch-action: manipulation;
    }
    .temp-button:hover {
      background-color: #c0392b;
    }
    .temp-display {
      font-size: 1.4rem;
      min-width: 120px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .slider {
      -webkit-appearance: none;
      width: 120px;
      height: 8px;
      border-radius: 4px;
      background: #444;
      outline: none;
      margin-top: 10px;
    }
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: #e74c3c;
      cursor: pointer;
    }
    .slider::-moz-range-thumb {
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: #e74c3c;
      cursor: pointer;
    }
    .slider::-webkit-slider-thumb:hover {
      background: #c0392b;
    }
    .slider::-moz-range-thumb:hover {
      background: #c0392b;
    }
  </style>
</head>
<body>
  <h2>HeatControl</h2>
  <div class="container">
    <div class="box">
      Mode: %MODE%
    </div>
    
    <form action="/setTemp" method="POST">
      <div class="box">
        <h3>Heating 1</h3>
        <div id="currentTemp1Display">Current: %CURRENT1%°C</div>
        <div class="temp-control">
          <button type="button" class="temp-button" onclick="adjustTemp('temp1', -0.5)">-</button>
          <div class="temp-display">
            <span id="targetTemp1Display">%TEMP1%</span>°C
            <input type="range" class="slider" id="temp1Slider" 
                   min="10" max="45" step="0.5" value="%TEMP1%"
                   oninput="updateFromSlider('temp1')">
            <input type="number" id="temp1" name="temp1" value="%TEMP1%" 
                   step="0.5" min="10" max="45" style="display: none">
          </div>
          <button type="button" class="temp-button" onclick="adjustTemp('temp1', 0.5)">+</button>
        </div>
        <div id="status1" class="status %MOSFET1_STATUS_CLASS%">Status: %MOSFET1_STATUS%</div>
      </div>

      <div class="box">
        <h3>Heating 2</h3>
        <div id="currentTemp2Display">Current: %CURRENT2%°C</div>
        <div class="temp-control">
          <button type="button" class="temp-button" onclick="adjustTemp('temp2', -0.5)">-</button>
          <div class="temp-display">
            <span id="targetTemp2Display">%TEMP2%</span>°C
            <input type="range" class="slider" id="temp2Slider" 
                   min="10" max="45" step="0.5" value="%TEMP2%"
                   oninput="updateFromSlider('temp2')">
            <input type="number" id="temp2" name="temp2" value="%TEMP2%" 
                   step="0.5" min="10" max="45" style="display: none">
          </div>
          <button type="button" class="temp-button" onclick="adjustTemp('temp2', 0.5)">+</button>
        </div>
        <div id="status2" class="status %MOSFET2_STATUS_CLASS%">Status: %MOSFET2_STATUS%</div>
      </div>

      <input type="submit" value="Save Settings">
    </form>

    <form action="/swapSensors" method="POST" class="box">
      <label>
        <input type="checkbox" name="swap" %SWAP_CHECKED%>
        Swap Sensors
      </label>
      <br>
      <input type="submit" value="Update">
    </form>

    <form action="/setWiFi" method="POST" class="box">
      <h3>WiFi Settings</h3>
      <div>
        <label>SSID<br>
        <input type="text" name="ssid" value="%WIFI_SSID%"></label>
      </div>
      <div>
        <label>Password<br>
        <input type="password" name="password" value="%WIFI_PASSWORD%"></label>
      </div>
      <input type="submit" value="Save WiFi">
    </form>

    <form action="/restart" method="POST" class="box">
      <input type="submit" value="Restart ESP">
    </form>
  </div>

  <script>
    function updateStatus() {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                // Aktuelle Temperaturen aktualisieren
                document.getElementById('currentTemp1Display').textContent = 
                    'Current: ' + data.current1.toFixed(1) + '°C';
                document.getElementById('currentTemp2Display').textContent = 
                    'Current: ' + data.current2.toFixed(1) + '°C';
                
                // Status aktualisieren
                const status1 = document.getElementById('status1');
                const status2 = document.getElementById('status2');
                
                status1.textContent = 'Status: ' + (data.h1 ? 'ON' : 'OFF');
                status1.className = 'status ' + (data.h1 ? 'status-on' : 'status-off');
                
                status2.textContent = 'Status: ' + (data.h2 ? 'ON' : 'OFF');
                status2.className = 'status ' + (data.h2 ? 'status-on' : 'status-off');
            })
            .catch(error => console.error('Update failed:', error));
    }

    function adjustTemp(id, change) {
        const input = document.getElementById(id);
        const display = document.getElementById('target' + id.charAt(0).toUpperCase() + id.slice(1) + 'Display');
        const slider = document.getElementById(id + 'Slider');
        
        let newValue = parseFloat(input.value) + change;
        newValue = Math.min(Math.max(newValue, 10), 45);
        
        input.value = newValue.toFixed(1);
        display.textContent = newValue.toFixed(1);
        slider.value = newValue;
    }

    function updateFromSlider(id) {
        const slider = document.getElementById(id + 'Slider');
        const input = document.getElementById(id);
        const display = document.getElementById('target' + id.charAt(0).toUpperCase() + id.slice(1) + 'Display');
        
        input.value = slider.value;
        display.textContent = slider.value;
    }

    setInterval(updateStatus, 2000);
    updateStatus();
  </script>
</body>
</html>
)rawliteral";

// Template Processor
String processor(const String& var) {
    // Current temperatures for display
    float displayTemp1 = swapAssignment ? currentTemp2 : currentTemp1;
    float displayTemp2 = swapAssignment ? currentTemp1 : currentTemp2;
    
    // Target temperatures for input fields
    // Important: These must remain in the same order,
    // regardless of the swap status
    float displayTarget1 = targetTemp1;  // Always the first target temperature
    float displayTarget2 = targetTemp2;  // Always the second target temperature

    if(var == "TEMP1") return String(displayTarget1);
    if(var == "TEMP2") return String(displayTarget2);
    if(var == "CURRENT1") return String(displayTemp1);
    if(var == "CURRENT2") return String(displayTemp2);
    if(var == "SWAP_CHECKED") return swapAssignment ? "checked" : "";
    if(var == "MOSFET1_STATUS") return digitalRead(MOSFET_PIN_1) == HIGH ? "ON" : "OFF";
    if(var == "MOSFET2_STATUS") return digitalRead(MOSFET_PIN_2) == HIGH ? "ON" : "OFF";
    if(var == "MOSFET1_STATUS_CLASS") return digitalRead(MOSFET_PIN_1) == HIGH ? "status-on" : "status-off";
    if(var == "MOSFET2_STATUS_CLASS") return digitalRead(MOSFET_PIN_2) == HIGH ? "status-on" : "status-off";
    
    // WiFi settings
    if(var == "WIFI_SSID") {
        EEPROM.begin(512);
        char storedSSID[32] = {0};
        for (int i = 0; i < 32; i++) {
            storedSSID[i] = EEPROM.read(EEPROM_SSID_ADDR + i);
        }
        EEPROM.end();
        
        // Check if the read SSID is valid
        if (strlen(storedSSID) == 0 || storedSSID[0] == 255) {
            return String("HeatControl");
        }
        return String(storedSSID);
    }
    if(var == "WIFI_PASSWORD") {
        EEPROM.begin(512);
        char storedPassword[32] = {0};
        for (int i = 0; i < 32; i++) {
            storedPassword[i] = EEPROM.read(EEPROM_PASS_ADDR + i);
        }
        EEPROM.end();
        
        // Check if the read password is valid
        if (strlen(storedPassword) == 0 || storedPassword[0] == 255) {
            return String("HeatControl");
        }
        return String(storedPassword);
    }
    if(var == "MODE") {
        // Show the mode selected at startup, not the current pin status
        return powerMode ? "Power Mode" : "Normal Mode";
    }

    return String();
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request) {
        // Nur bestimmte Anfragen behandeln
        String host = request->host();
        if (host.indexOf("connectivitycheck.gstatic.com") >= 0 ||
            host.indexOf("apple.com") >= 0 ||
            host.indexOf("msftconnecttest.com") >= 0) {
            return true;
        }
        return false;  // Andere Anfragen normal verarbeiten lassen
    }

    void handleRequest(AsyncWebServerRequest *request) {
        // Schnelle Weiterleitung für Captive Portal Checks
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
        response->addHeader("Location", "http://4.3.2.1");
        request->send(response);
    }
};

// Improved helper functions
void writeFloat(int addr, float value) {
    EEPROM.begin(512);
    byte* p = (byte*)(void*)&value;
    for (unsigned int i = 0; i < sizeof(value); i++) {
        EEPROM.write(addr + i, *p++);
    }
    bool success = EEPROM.commit();
    EEPROM.end();
    
    Serial.print("Writing float value ");
    Serial.print(value);
    Serial.print(" to address ");
    Serial.print(addr);
    Serial.println(success ? " - Success" : " - Failed!");
}

float readFloat(int addr) {
    EEPROM.begin(512);
    float value;
    byte* p = (byte*)(void*)&value;
    for (unsigned int i = 0; i < sizeof(value); i++) {
        *p++ = EEPROM.read(addr + i);
    }
    EEPROM.end();
    return value;
}

// Improved temperature functions
void loadTemperatures() {
    Serial.println("\n=== Loading Temperatures ===");
    EEPROM.begin(512);
    
    // Load Temp1
    float temp1;
    byte* p1 = (byte*)(void*)&temp1;
    for (unsigned int i = 0; i < sizeof(float); i++) {
        *p1++ = EEPROM.read(EEPROM_TEMP1_ADDR + i);
    }
    
    // Load Temp2
    float temp2;
    byte* p2 = (byte*)(void*)&temp2;
    for (unsigned int i = 0; i < sizeof(float); i++) {
        *p2++ = EEPROM.read(EEPROM_TEMP2_ADDR + i);
    }
    
    EEPROM.end();

    // Validate and set the values
    if (isnan(temp1) || temp1 < 10.0 || temp1 > 45.0) {
        targetTemp1 = 23.0;
        Serial.println("Invalid Temp1, using default");
    } else {
        targetTemp1 = temp1;
    }
    
    if (isnan(temp2) || temp2 < 10.0 || temp2 > 45.0) {
        targetTemp2 = 23.0;
        Serial.println("Invalid Temp2, using default");
    } else {
        targetTemp2 = temp2;
    }

    Serial.printf("Loaded - T1: %.1f°C, T2: %.1f°C\n", targetTemp1, targetTemp2);
}

void saveTemperatures() {
    Serial.println("\n=== Saving Temperatures ===");
    EEPROM.begin(512);
    
    // Save Temp1
    byte* p1 = (byte*)(void*)&targetTemp1;
    for (unsigned int i = 0; i < sizeof(float); i++) {
        EEPROM.write(EEPROM_TEMP1_ADDR + i, *p1++);
    }
    
    // Save Temp2
    byte* p2 = (byte*)(void*)&targetTemp2;
    for (unsigned int i = 0; i < sizeof(float); i++) {
        EEPROM.write(EEPROM_TEMP2_ADDR + i, *p2++);
    }
    
    if(EEPROM.commit()) {
        Serial.printf("Saved - T1: %.1f°C, T2: %.1f°C\n", targetTemp1, targetTemp2);
    } else {
        Serial.println("Error saving temperatures!");
    }
    EEPROM.end();
}

// Improved sensor assignment functions
void loadAssignment() {
    EEPROM.begin(512);
    byte value = EEPROM.read(EEPROM_SWAP_ADDR);
    swapAssignment = (value == 1);
    EEPROM.end();
    Serial.printf("Loaded swap assignment: %s\n", swapAssignment ? "true" : "false");
}

void saveAssignment() {
    EEPROM.begin(512);
    Serial.printf("Saving swap assignment: %s\n", swapAssignment ? "true" : "false");
    EEPROM.write(EEPROM_SWAP_ADDR, swapAssignment ? 1 : 0);
    if(EEPROM.commit()) {
        Serial.println("Swap assignment saved successfully");
    } else {
        Serial.println("Error saving swap assignment!");
    }
    EEPROM.end();
}

void swapSensorMOSFET() {
    swapAssignment = !swapAssignment; // Swap the assignment
    saveAssignment(); // Save the new assignment
}

unsigned long previousMillis = 0; // Stores the last time the temperature was updated
const long interval = 1000; // Interval for updating (1 second)

DNSServer dnsServer;

void handleStartupSignal(bool isPowerMode) {
    if (isPowerMode) {
        // Power Mode Signal: 2x pulse
        digitalWrite(SIGNAL_PIN, LOW);   // Active
        delay(1000);
        digitalWrite(SIGNAL_PIN, HIGH);  // Inactive
        delay(1000);
        digitalWrite(SIGNAL_PIN, LOW);   // Active
        delay(1000);
        digitalWrite(SIGNAL_PIN, HIGH);  // Inactive
    } else {
        // Normal Mode Signal: 1x pulse
        digitalWrite(SIGNAL_PIN, LOW);   // Active
        delay(1000);
        digitalWrite(SIGNAL_PIN, HIGH);  // Inactive
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EEPROM and load temperatures
    EEPROM.begin(512);
    loadTemperatures();
    
    // Initialize sensors and pins
    sensors.begin();
    pinMode(MOSFET_PIN_1, OUTPUT);
    pinMode(MOSFET_PIN_2, OUTPUT);
    pinMode(INPUT_PIN, INPUT);
    pinMode(SIGNAL_PIN, OUTPUT);
    digitalWrite(SIGNAL_PIN, HIGH);  // Initial state is HIGH (inactive)
    
    // Read the power mode ONCE at startup
    powerMode = (digitalRead(INPUT_PIN) == HIGH);
    if (powerMode) {
        handleStartupSignal(true);
        // If power mode is active, turn both heaters on directly
        digitalWrite(MOSFET_PIN_1, HIGH);
        digitalWrite(MOSFET_PIN_2, HIGH);
        Serial.println("Power Mode activated - Both heaters will stay ON");
    } else {
        handleStartupSignal(false);
        Serial.println("Normal Mode activated - Temperature control active");
    }

    // Load the saved WiFi credentials
    loadWiFiCredentials();
    
    // Set up WiFi as an access point
    WiFi.mode(WIFI_AP);
    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.softAPConfig(IPAddress(4,3,2,1), IPAddress(4,3,2,1), IPAddress(255,255,255,0));
    WiFi.softAP(activeSSID.c_str(), activePassword.c_str());

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Using SSID: ");
    Serial.println(activeSSID);
    Serial.print("Using Password length: ");
    Serial.println(activePassword.length());

    // Configure DNS server
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", IPAddress(4,3,2,1));

    // Debug info for connected clients
    WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& evt) {
        Serial.println("=== Client Connected to AP ===");
        Serial.print("MAC: ");
        for(int i = 0; i < 6; i++) {
            Serial.printf("%02X", evt.mac[i]);
            if(i < 5) Serial.print(":");
        }
        Serial.println();
    });

    // Routen in dieser Reihenfolge registrieren
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, processor);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        char json[128];
        snprintf(json, sizeof(json), 
            "{\"current1\":%.1f,\"current2\":%.1f,\"h1\":%d,\"h2\":%d}",
            currentTemp1,
            currentTemp2,
            digitalRead(MOSFET_PIN_1),
            digitalRead(MOSFET_PIN_2)
        );
        request->send(200, "application/json", json);
    });

    server.on("/setTemp", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("Temperature update received");
        
        if(request->hasParam("temp1", true)) {
            String temp1Str = request->getParam("temp1", true)->value();
            targetTemp1 = temp1Str.toFloat();
            Serial.printf("New Temp1: %.1f\n", targetTemp1);
        }
        
        if(request->hasParam("temp2", true)) {
            String temp2Str = request->getParam("temp2", true)->value();
            targetTemp2 = temp2Str.toFloat();
            Serial.printf("New Temp2: %.1f\n", targetTemp2);
        }
        
        saveTemperatures();
        request->redirect("/");
    });

    server.on("/swapSensors", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("Swap sensors request received");
        swapAssignment = request->hasParam("swap", true);
        Serial.printf("Swap Assignment: %s\n", swapAssignment ? "true" : "false");
        saveAssignment();
        request->redirect("/");
    });

    server.on("/setWiFi", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("WiFi update received");
        
        if(request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String newSSID = request->getParam("ssid", true)->value();
            String newPassword = request->getParam("password", true)->value();
            
            Serial.printf("New SSID: %s, Password length: %d\n", 
                         newSSID.c_str(), newPassword.length());
            
            if(newSSID.length() > 0) {
                saveWiFiCredentials(newSSID, newPassword);
                
                // Send confirmation page
                String html = "<!DOCTYPE HTML><html><head>";
                html += "<meta charset='UTF-8'>";
                html += "<style>body{font-family:Arial;text-align:center;background:#1a1a1a;color:white;padding:20px;}</style>";
                html += "</head><body>";
                html += "<h2>WiFi Settings Saved</h2>";
                html += "<p>New SSID: " + newSSID + "</p>";
                html += "<p>Rebooting...</p>";
                html += "</body></html>";
                
                request->send(200, "text/html", html);
                
                // Delayed restart
                delay(1000);
                ESP.restart();
            }
        }
        request->redirect("/");
    });

    server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("Restart request received");
        
        // Send confirmation page
        String html = "<!DOCTYPE HTML><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<style>body{font-family:Arial;text-align:center;background:#1a1a1a;color:white;padding:20px;}</style>";
        html += "</head><body>";
        html += "<h2>Restarting ESP...</h2>";
        html += "</body></html>";
        
        request->send(200, "text/html", html);
        
        // Delayed restart
        delay(1000);
        ESP.restart();
    });

    // Captive Portal Handler zum Schluss
    server.addHandler(new CaptiveRequestHandler());

    // Server Konfiguration
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.begin();
    
    Serial.println("Webserver started");

    loadAssignment();
}

void loop() {
    // DNS Server nur alle 100ms prüfen
    static unsigned long lastDnsCheck = 0;
    if (millis() - lastDnsCheck >= 100) {
        dnsServer.processNextRequest();
        lastDnsCheck = millis();
    }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        // Update temperatures for display
        sensors.requestTemperatures();
        currentTemp1 = sensors.getTempCByIndex(0);
        currentTemp2 = sensors.getTempCByIndex(1);
        
        // Do nothing in Power Mode, since the heaters are already on
        if (!powerMode) {
            // Normal temperature control only if NOT in Power Mode
            if (swapAssignment) {
                if (currentTemp2 < targetTemp1) {
                    digitalWrite(MOSFET_PIN_1, HIGH);
                } else {
                    digitalWrite(MOSFET_PIN_1, LOW);
                }

                if (currentTemp1 < targetTemp2) {
                    digitalWrite(MOSFET_PIN_2, HIGH);
                } else {
                    digitalWrite(MOSFET_PIN_2, LOW);
                }
            } else {
                if (currentTemp1 < targetTemp1) {
                    digitalWrite(MOSFET_PIN_1, HIGH);
                } else {
                    digitalWrite(MOSFET_PIN_1, LOW);
                }

                if (currentTemp2 < targetTemp2) {
                    digitalWrite(MOSFET_PIN_2, HIGH);
                } else {
                    digitalWrite(MOSFET_PIN_2, LOW);
                }
            }
        }
    }
}