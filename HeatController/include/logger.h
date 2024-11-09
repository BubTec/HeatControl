#pragma once
#include <Arduino.h>

void addLog(const char* message, uint8_t type);
String getLog();
void handleGetLog();
void handleClearLog(); 