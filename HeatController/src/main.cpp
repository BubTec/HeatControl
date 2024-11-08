#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

// WLAN Einstellungen
const char* ssid = "TempController";
const char* password = "12345678";

// Webserver auf Port 80
ESP8266WebServer server(80);

// Pins
#define ONE_WIRE_BUS 2
#define MOSFET1_PIN 4
#define MOSFET2_PIN 5

// Temperatur-Einstellungen
float TARGET_TEMP = 22.0;    // Jetzt als Variable, nicht mehr als Define
#define TEMP_TOLERANCE 0.5
#define MAX_PWM 1023
#define MIN_PWM 0

// Regelparameter
#define Kp 100.0
#define Ki 0.1
#define CONTROL_INTERVAL 1000

// OneWire Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1, sensor2;

// Regelungsvariablen
unsigned long lastControlTime = 0;
float integral = 0;
int currentPWM = 0;

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

float avgTemp = 0;  // Global für Webzugriff

DNSServer dnsServer;
const byte DNS_PORT = 53;
const int CAPTIVE_PORTAL_TIMEOUT = 5000;  // 5 Sekunden Timeout

void handleRoot() {
  String html = String(index_html);
  html.replace("%TEMP%", String(avgTemp, 1));
  html.replace("%TARGET%", String(TARGET_TEMP, 1));
  html.replace("%PWM%", String((float)currentPWM/MAX_PWM * 100, 0));
  server.send(200, "text/html", html);
}

void handleSetTemp() {
  if (server.hasArg("temp")) {
    float newTemp = server.arg("temp").toFloat();
    if (newTemp >= 5 && newTemp <= 30) {  // Sicherheitsbereich
      TARGET_TEMP = newTemp;
    }
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Updated");
}

void handleNotFound() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(9600);
  
  // WiFi AP Mode Setup
  WiFi.softAP(ssid, password);
  
  // DNS Server starten für Captive Portal
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);
  
  Serial.print("AP IP Adresse: ");
  Serial.println(apIP);

  // Webserver Routen
  server.on("/", handleRoot);
  server.on("/settemp", handleSetTemp);
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

float calculateHeatingPower(float currentTemp) {
  float error = TARGET_TEMP - currentTemp;
  
  if (abs(error) < 2.0) {
    integral += error * (CONTROL_INTERVAL / 1000.0);
  }
  
  integral = constrain(integral, -10, 10);
  float output = (Kp * error) + (Ki * integral);
  output = constrain(output, MIN_PWM, MAX_PWM);
  
  return output;
}

void loop() {
  dnsServer.processNextRequest();
  
  server.handleClient();  // Webserver Anfragen bearbeiten
  
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
  }
}