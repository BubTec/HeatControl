#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>

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
#define Kp 100.0
#define Ki 0.1
#define CONTROL_INTERVAL 1000

// WLAN Einstellungen
const char* ssid = "TempController";
const char* password = "12345678";

// DNS Server Einstellungen
const byte DNS_PORT = 53;

// Globale Variablen
float TARGET_TEMP = 22.0;
float avgTemp = 0;
unsigned long lastControlTime = 0;
float integral = 0;
int currentPWM = 0;

// Objekte initialisieren
ESP8266WebServer server(80);
DNSServer dnsServer;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1, sensor2;

// HTML Seite
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    body { font-family: Arial; text-align: center; margin: 20px; }
    .button { padding: 10px 20px; font-size: 16px; }
  </style>
</head>
<body>
  <h2>Temperatur Controller</h2>
  <p>Aktuelle Temperatur: %TEMP%°C</p>
  <p>Zieltemperatur: %TARGET%°C</p>
  <p>PWM: %PWM%%</p>
  <form action="/settemp" method="get">
    <input type="number" name="temp" step="0.5" value="%TARGET%">
    <input type="submit" value="Temperatur setzen">
  </form>
  <p><small>Heizungssteuerung</small></p>
</body>
</html>
)rawliteral";

// Funktionsdeklarationen
static void handleRoot();
static void handleSetTemp();
static void handleNotFound();
static float calculateHeatingPower(float currentTemp);

// Funktionsimplementierungen
static void handleRoot() {
    #ifdef DEBUG_SERIAL
        Serial.println("Hauptseite aufgerufen");
        Serial.print("Client IP: ");
        Serial.println(server.client().remoteIP());
    #endif
    
    String html = String(index_html);
    html.replace("%TEMP%", String(avgTemp, 1));
    html.replace("%TARGET%", String(TARGET_TEMP, 1));
    html.replace("%PWM%", String((float)currentPWM/MAX_PWM * 100, 0));
    server.send(200, "text/html", html);
}

static void handleSetTemp() {
    if (server.hasArg("temp")) {
        float newTemp = server.arg("temp").toFloat();
        if (newTemp >= 5 && newTemp <= 30) {
            TARGET_TEMP = newTemp;
        }
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Updated");
}

static void handleNotFound() {
    #ifdef DEBUG_SERIAL
        Serial.print("Nicht gefundene URL: ");
        Serial.println(server.uri());
        Serial.print("Host Header: ");
        Serial.println(server.hostHeader());
        Serial.print("Client IP: ");
        Serial.println(server.client().remoteIP());
    #endif
    
    String redirectUrl = String("http://") + WiFi.softAPIP().toString();
    server.sendHeader("Location", redirectUrl, true);
    server.send(302, "text/plain", "");
}

static float calculateHeatingPower(float currentTemp) {
    float error = TARGET_TEMP - currentTemp;
    
    if (abs(error) < 2.0) {
        integral += error * (CONTROL_INTERVAL / 1000.0);
    }
    
    integral = constrain(integral, -10, 10);
    float output = (Kp * error) + (Ki * integral);
    output = constrain(output, MIN_PWM, MAX_PWM);
    
    return output;
}

void setup() {
    Serial.begin(9600);
    
    Serial.println("\nStarte WiFi Access Point...");
    
    WiFi.mode(WIFI_AP);
    WiFi.persistent(false);
    
    Serial.println("Konfiguriere AP...");
    bool cfgResult = WiFi.softAPConfig(IPAddress(8,8,8,8), IPAddress(8,8,8,8), IPAddress(255,255,255,0));
    Serial.println(cfgResult ? "AP Konfiguration OK" : "AP Konfiguration fehlgeschlagen");
    
    bool apResult = WiFi.softAP(ssid, password);
    Serial.println(apResult ? "AP Start OK" : "AP Start fehlgeschlagen");
    
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", IPAddress(8,8,8,8));
    Serial.println("DNS Server gestartet");
    
    server.on("/", handleRoot);
    server.on("/settemp", handleSetTemp);
    server.on("/generate_204", handleRoot);
    server.on("/fwlink", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin();
    
    pinMode(MOSFET1_PIN, OUTPUT);
    pinMode(MOSFET2_PIN, OUTPUT);
    
    analogWriteFreq(1000);
    analogWriteRange(MAX_PWM);
    
    sensors.begin();
    
    if(sensors.getDeviceCount() < 2) {
        Serial.println("FEHLER: Nicht genügend Temperatursensoren gefunden!");
        while(1) { delay(1000); }
    }
    
    sensors.getAddress(sensor1, 0);
    sensors.getAddress(sensor2, 1);
    sensors.setResolution(sensor1, 12);
    sensors.setResolution(sensor2, 12);
}

void loop() {
    static unsigned long lastClientCheck = 0;
    static uint8_t lastClientCount = 0;
    
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - lastClientCheck > 5000) {
        lastClientCheck = millis();
        uint8_t currentClientCount = WiFi.softAPgetStationNum();
        
        if (currentClientCount != lastClientCount) {
            Serial.print("Verbundene Clients: ");
            Serial.println(currentClientCount);
            lastClientCount = currentClientCount;
        }
        
        #ifdef DEBUG_SERIAL
            Serial.print("Freier Heap: ");
            Serial.println(ESP.getFreeHeap());
        #endif
    }
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastControlTime >= CONTROL_INTERVAL) {
        lastControlTime = currentMillis;
        
        sensors.requestTemperatures();
        float temp1 = sensors.getTempC(sensor1);
        float temp2 = sensors.getTempC(sensor2);
        
        avgTemp = (temp1 + temp2) / 2.0;
        currentPWM = (int)calculateHeatingPower(avgTemp);
        
        analogWrite(MOSFET1_PIN, currentPWM);
        analogWrite(MOSFET2_PIN, currentPWM);
        
        #ifdef DEBUG_SERIAL
            Serial.println("\n--- Temperatur Update ---");
            Serial.print("Ziel: ");
            Serial.print(TARGET_TEMP);
            Serial.print("°C, Aktuell: ");
            Serial.print(avgTemp);
            Serial.print("°C (S1: ");
            Serial.print(temp1);
            Serial.print("°C, S2: ");
            Serial.print(temp2);
            Serial.print("°C), PWM: ");
            Serial.print((float)currentPWM/MAX_PWM * 100);
            Serial.println("%");
        #endif
    }
}