#pragma once

#include "config.h"
#include "globals.h"
#include "logger.h"
#include <OneWire.h>
#include <DallasTemperature.h>

void setupHeating();
void handleHeating();
void updateHeating();
float calculateHeatingPower1(float currentTemp);
float calculateHeatingPower2(float currentTemp);
void setHeatingPower(int circuit, float power);
void controlHeating();
float readTemperatureSensor1();
float readTemperatureSensor2();