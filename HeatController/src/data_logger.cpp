#include "data_logger.h"
#include "globals.h"
#include <LittleFS.h>

unsigned long lastDataLog = 0;

void setupDataLogger() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }
    
    // Create log file if it doesn't exist
    if (!LittleFS.exists(LOG_FILE)) {
        File f = LittleFS.open(LOG_FILE, "w");
        if (f) {
            f.close();
        }
    }
}

void logData() {
    if (millis() - lastDataLog < LOG_INTERVAL) return;
    lastDataLog = millis();
    
    File f = LittleFS.open(LOG_FILE, "a");
    if (f) {
        // Get current timestamp
        time_t now = time(nullptr);
        
        // Format: timestamp,temp1,temp2,pwm1,pwm2
        f.printf("%lld,%.1f,%.1f,%d,%d\n", 
                 now, 
                 currentTemp1, 
                 currentTemp2,
                 currentPWM1,
                 currentPWM2);
        f.close();
        
        // Check file size and rotate if necessary
        if (LittleFS.exists(LOG_FILE)) {
            File f = LittleFS.open(LOG_FILE, "r");
            if (f) {
                if (f.size() > MAX_LOG_FILE_SIZE) {
                    // Read the second half of the file
                    f.seek(f.size() / 2);
                    String remainingData = f.readString();
                    f.close();
                    
                    // Write the second half back to the file
                    f = LittleFS.open(LOG_FILE, "w");
                    if (f) {
                        f.print(remainingData);
                        f.close();
                    }
                } else {
                    f.close();
                }
            }
        }
    }
}

String getLogData(uint32_t hours) {
    String data = "[";
    if (LittleFS.exists(LOG_FILE)) {
        File f = LittleFS.open(LOG_FILE, "r");
        
        // Überspringe Header
        String line = f.readStringUntil('\n');
        
        uint32_t startTime = 0;
        bool firstLine = true;
        
        while (f.available()) {
            line = f.readStringUntil('\n');
            if (line.length() > 0) {
                // Parse CSV Zeile
                int idx = line.indexOf(',');
                if (idx > 0) {
                    uint32_t timestamp = line.substring(0, idx).toInt();
                    if (firstLine) {
                        startTime = timestamp;
                        firstLine = false;
                    }
                    
                    // Nur Daten der letzten X Stunden
                    if ((timestamp - startTime) <= hours * 3600) {
                        if (data != "[") data += ",";
                        data += line;
                    }
                }
            }
        }
        f.close();
    }
    data += "]";
    return data;
}

void clearLogData() {
    if (LittleFS.exists(LOG_FILE)) {
        LittleFS.remove(LOG_FILE);
        File f = LittleFS.open(LOG_FILE, "w");
        if (f) {
            f.close();
        }
    }
}