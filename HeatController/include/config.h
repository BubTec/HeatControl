#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <user_interface.h>  // For RTC Memory access

// Defines for operating modes
#define MAGIC_HEATER_ON 0xBE
#define MAGIC_NORMAL 0xEF
#define EEPROM_SIZE 512
#define EEPROM_MAGIC 0xAB
#define MODE_PIN D5  // Select a free pin

// RTC Data structure
struct RTCData {
    uint32_t magic;
    uint32_t bootCount;
};

extern RTCData rtcData;

// Config structure
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

// Function declarations
void loadConfig();
void saveConfig();
void checkOperationMode();
void resetConfig();