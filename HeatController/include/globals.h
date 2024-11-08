#pragma once

#include <Arduino.h>

// Common constants
#define MAX_PWM 1023
#define MIN_PWM 0

// Temperature variables
extern float TARGET_TEMP1;
extern float TARGET_TEMP2;
extern float currentTemp1;
extern float currentTemp2;
extern int currentPWM1;
extern int currentPWM2;