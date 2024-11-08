#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <user_interface.h>  // Für RTC Memory Zugriff

// Defines für Betriebsmodi
#define MAGIC_HEATER_ON 0xBE
#define MAGIC_NORMAL 0xEF
#define EEPROM_SIZE 512
#define EEPROM_MAGIC 0xAB
#define MODE_PIN D5  // Wähle einen freien Pin

// RTC Datenstruktur
struct RTCData {
    uint32_t magic;
    uint32_t bootCount;
};

extern RTCData rtcData;

// Config Struktur
struct Config {
    char ssid[33];
    char password[65];
    float targetTemp1;
    float targetTemp2;
    float Kp1;
    float Ki1;
    float Kd1;
    float Kp2;
    float Ki2;
    float Kd2;
    uint32_t lastMode;
    byte magic;
};

extern Config config;

// Funktionsdeklarationen
void loadConfig();
void saveConfig();
void checkOperationMode();
void resetConfig();