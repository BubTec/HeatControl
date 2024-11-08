#pragma once

#include <Arduino.h>

// Gemeinsame Konstanten
#define MAX_PWM 1023
#define MIN_PWM 0

// Temperatur-Variablen
extern float TARGET_TEMP1;
extern float TARGET_TEMP2;
extern float currentTemp1;
extern float currentTemp2;
extern int currentPWM1;
extern int currentPWM2; 