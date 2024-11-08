#pragma once

#include "config.h"

void addLog(const char* message, uint8_t type);
void handleGetLog();
void handleClearLog();
String getLog();
void clearLog(); 