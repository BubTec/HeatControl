#include "logger.h"
#include "web_server.h"

#define MAX_LOG_ENTRIES 50

struct LogEntry {
    char message[64];
    uint8_t type;
    unsigned long timestamp;
};

static LogEntry logEntries[MAX_LOG_ENTRIES];
static int logIndex = 0;
static int logCount = 0;

extern ESP8266WebServer server;  // Deklariere server als extern

void addLog(const char* message, uint8_t type) {
    unsigned long now = millis();
    strncpy(logEntries[logIndex].message, message, 63);
    logEntries[logIndex].message[63] = '\0';
    logEntries[logIndex].type = type;
    logEntries[logIndex].timestamp = now;
    
    logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
    if (logCount < MAX_LOG_ENTRIES) logCount++;
}

void handleGetLog() {
    String log = "";
    int start = (logCount < MAX_LOG_ENTRIES) ? 0 : logIndex;
    
    for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % MAX_LOG_ENTRIES;
        String timeStr = String(logEntries[idx].timestamp / 1000);
        String typeClass = "";
        
        switch(logEntries[idx].type) {
            case 1: typeClass = "warning"; break;
            case 2: typeClass = "error"; break;
            default: typeClass = "info";
        }
        
        log += "<div class='" + typeClass + "'>[" + timeStr + "s] " + 
               String(logEntries[idx].message) + "</div>\n";
    }
    
    server.send(200, "text/html", log);
}

void handleClearLog() {
    logIndex = 0;
    logCount = 0;
    server.send(200, "text/plain", "OK");
} 