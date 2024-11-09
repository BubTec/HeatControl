#pragma once

#include <Arduino.h>

// Global variables
extern float currentTemp1;
extern float currentTemp2;
extern int currentPWM1;
extern int currentPWM2;

// Common constants
const int MIN_PWM = 0;

// RTC Data structure for persistent data
struct RTCData {
    uint16_t magic;  // Magic number for operation mode
    // ... other RTC data if needed
};

extern RTCData rtcData;

void updateTemperatures(); // Declaration of the function