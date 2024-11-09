#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "globals.h"  // Für RTCData-Struktur

// Zieltemperaturen (nicht mehr const, da sie geändert werden können)
extern float TARGET_TEMP1;  // Zieltemperatur für Heizkreis 1
extern float TARGET_TEMP2;  // Zieltemperatur für Heizkreis 2

// Konstanten
const int MAX_PWM = 1023;          // Maximaler PWM-Wert
const uint16_t MAGIC_HEATER_ON = 0xAA55;  // Magic Number für "Heizung dauerhaft an"
const uint16_t MAGIC_NORMAL = 0x1234;     // Magic Number für normalen Betrieb
const uint16_t EEPROM_MAGIC = 0xABCD;     // Magic Number für EEPROM-Validierung
const int EEPROM_SIZE = 512;              // Größe des EEPROM-Speichers
const int MODE_PIN = D5;                  // Pin für Moduswechsel

// Konfigurationsstruktur
struct Config {
    uint16_t magic;
    char ssid[32];
    char password[64];
    float targetTemp1;
    float targetTemp2;
    float Kp1, Ki1, Kd1;  // PID-Parameter für Heizkreis 1
    float Kp2, Ki2, Kd2;  // PID-Parameter für Heizkreis 2
    uint16_t lastMode;
};

// Funktionsdeklarationen
void loadConfig();
void saveConfig();
void resetConfig();
void checkOperationMode();

extern Config config;