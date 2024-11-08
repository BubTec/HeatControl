#pragma once

#include <Arduino.h>
#include <FS.h>

#define LOG_INTERVAL 60000  // 1 minute between logs
#define MAX_LOG_FILE_SIZE 1000000  // 1MB max file size
#define LOG_FILE "/templog.csv"

void setupDataLogger();
void logData();
String getLogData(uint32_t hours);
void clearLogData();