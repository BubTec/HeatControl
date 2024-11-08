#pragma once

#include "config.h"
#include "globals.h"
#include "data_logger.h"
#include <ESP8266WebServer.h>

extern ESP8266WebServer server;

void setupWebServer();
void handleWebServer();
void handleRoot();
void handleSetTemp();
void handleSetPID();
void handleSetWifi();
void handleGetTempLog();