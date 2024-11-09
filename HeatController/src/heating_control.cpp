#include "heating_control.h"
#include "globals.h"
#include "config.h"

// Pin-Definitionen
const int HEATER_1_PIN = D1;  // Pin für Heizkreis 1
const int HEATER_2_PIN = D2;  // Pin für Heizkreis 2

// Temperatur-Hysterese (°C)
const float TEMP_HYSTERESIS = 0.5;

// Funktionsdeklarationen
void heaterOn(int circuit);
void heaterOff(int circuit);

void setupHeating() {
    // Pins als Ausgänge konfigurieren
    pinMode(HEATER_1_PIN, OUTPUT);
    pinMode(HEATER_2_PIN, OUTPUT);
    
    // Heizungen initial ausschalten (LOW = AUS bei MOSFETs)
    digitalWrite(HEATER_1_PIN, LOW);
    digitalWrite(HEATER_2_PIN, LOW);
    
    addLog("Heating system initialized", 0);
}

// Grundlegende Funktionen zum Ein-/Ausschalten der Heizungen
void heaterOn(int circuit) {
    int pin = (circuit == 1) ? HEATER_1_PIN : HEATER_2_PIN;
    digitalWrite(pin, HIGH);  // HIGH = AN bei MOSFETs
    
    // PWM-Werte für Web-Interface aktualisieren
    if (circuit == 1) {
        currentPWM1 = MAX_PWM;
    } else {
        currentPWM2 = MAX_PWM;
    }
    
    char logMsg[64];
    snprintf(logMsg, sizeof(logMsg), "Heater %d turned ON", circuit);
    addLog(logMsg, 0);
}

void heaterOff(int circuit) {
    int pin = (circuit == 1) ? HEATER_1_PIN : HEATER_2_PIN;
    digitalWrite(pin, LOW);  // LOW = AUS bei MOSFETs
    
    // PWM-Werte für Web-Interface aktualisieren
    if (circuit == 1) {
        currentPWM1 = 0;
    } else {
        currentPWM2 = 0;
    }
    
    char logMsg[64];
    snprintf(logMsg, sizeof(logMsg), "Heater %d turned OFF", circuit);
    addLog(logMsg, 0);
}

// Hauptsteuerungsfunktion
void handleHeating() {
    // Debug-Ausgaben für Temperaturen und Zielwerte
    Serial.println("\n=== Heating Control Status ===");
    Serial.printf("Current Temp 1: %.2f°C (Target: %.2f°C)\n", currentTemp1, TARGET_TEMP1);
    Serial.printf("Current Temp 2: %.2f°C (Target: %.2f°C)\n", currentTemp2, TARGET_TEMP2);
    Serial.printf("Operation Mode: %s\n", (rtcData.magic == MAGIC_HEATER_ON) ? "FULL POWER" : "NORMAL");
    
    // Temperaturen aktualisieren
    updateTemperatures();
    
    // Betriebsmodus prüfen
    if (rtcData.magic == MAGIC_HEATER_ON) {
        Serial.println("FULL POWER Mode activated!");
        heaterOn(1);
        heaterOn(2);
        return;
    }
    
    // NORMAL Modus - temperaturabhängige Steuerung
    Serial.println("Normal temperature control mode:");
    
    // Heizkreis 1
    Serial.printf("Circuit 1: %.2f < %.2f - %.2f ?\n", 
                 currentTemp1, TARGET_TEMP1, TEMP_HYSTERESIS);
    if (currentTemp1 < (TARGET_TEMP1 - TEMP_HYSTERESIS)) {
        Serial.println("Circuit 1: Too cold -> turning ON");
        heaterOn(1);
    }
    else if (currentTemp1 > (TARGET_TEMP1 + TEMP_HYSTERESIS)) {
        Serial.println("Circuit 1: Too warm -> turning OFF");
        heaterOff(1);
    }
    
    // Heizkreis 2
    Serial.printf("Circuit 2: %.2f < %.2f - %.2f ?\n", 
                 currentTemp2, TARGET_TEMP2, TEMP_HYSTERESIS);
    if (currentTemp2 < (TARGET_TEMP2 - TEMP_HYSTERESIS)) {
        Serial.println("Circuit 2: Too cold -> turning ON");
        heaterOn(2);
    }
    else if (currentTemp2 > (TARGET_TEMP2 + TEMP_HYSTERESIS)) {
        Serial.println("Circuit 2: Too warm -> turning OFF");
        heaterOff(2);
    }
}

float readTemperatureSensor1() {
    float temp = 21.0;  // Beispielwert
    Serial.printf("Temperature Sensor 1 reading: %.2f°C\n", temp);
    return temp;
}

float readTemperatureSensor2() {
    float temp = 21.0;  // Beispielwert
    Serial.printf("Temperature Sensor 2 reading: %.2f°C\n", temp);
    return temp;
}

void updateTemperatures() {
    currentTemp1 = readTemperatureSensor1();
    currentTemp2 = readTemperatureSensor2();
}

void controlHeating() {
    handleHeating();  // Verwendet die existierende Implementation
}