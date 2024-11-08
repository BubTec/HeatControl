#include "data_logger.h"
#include "globals.h"
#include "logger.h"

unsigned long lastDataLog = 0;

void setupDataLogger() {
    if (!SPIFFS.begin()) {
        addLog("SPIFFS initialization failed!", 2);
        return;
    }
    
    // Check if log file exists, if not create header
    if (!SPIFFS.exists(LOG_FILE)) {
        File f = SPIFFS.open(LOG_FILE, "w");
        if (f) {
            f.println("timestamp,temp1,temp2,pwm1,pwm2");
            f.close();
            addLog("New log file created", 0);
        }
    }
}

void logData() {
    if (millis() - lastDataLog < LOG_INTERVAL) return;
    lastDataLog = millis();
    
    File f = SPIFFS.open(LOG_FILE, "a");
    if (f) {
        // Write timestamp (seconds since start), temperatures and PWM
        f.printf("%lu,%.1f,%.1f,%d,%d\n", 
            millis()/1000, 
            currentTemp1, 
            currentTemp2, 
            (int)((float)currentPWM1/MAX_PWM * 100),
            (int)((float)currentPWM2/MAX_PWM * 100)
        );
        f.close();
        
        // Check file size and rotate if necessary
        if (SPIFFS.exists(LOG_FILE)) {
            File f = SPIFFS.open(LOG_FILE, "r");
            if (f.size() > MAX_LOG_FILE_SIZE) {
                // Keep only the last 75% of data
                String newContent;
                f.seek(f.size() / 4);  // Skip first quarter
                while (f.available()) {
                    newContent += (char)f.read();
                }
                f.close();
                
                f = SPIFFS.open(LOG_FILE, "w");
                f.println("timestamp,temp1,temp2,pwm1,pwm2");
                f.print(newContent);
                f.close();
                addLog("Log file rotated", 0);
            }
        }
    }
}

String getLogData(uint32_t hours) {
    String data = "[";
    if (SPIFFS.exists(LOG_FILE)) {
        File f = SPIFFS.open(LOG_FILE, "r");
        
        // Skip header
        String line = f.readStringUntil('\n');
        
        uint32_t startTime = 0;
        bool firstLine = true;
        
        while (f.available()) {
            line = f.readStringUntil('\n');
            if (line.length() > 0) {
                // Parse CSV line
                int idx = line.indexOf(',');
                if (idx > 0) {
                    uint32_t timestamp = line.substring(0, idx).toInt();
                    if (firstLine) {
                        startTime = timestamp;
                        firstLine = false;
                    }
                    
                    // Only data from last X hours
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
    if (SPIFFS.exists(LOG_FILE)) {
        SPIFFS.remove(LOG_FILE);
        File f = SPIFFS.open(LOG_FILE, "w");
        if (f) {
            f.println("timestamp,temp1,temp2,pwm1,pwm2");
            f.close();
            addLog("Log file reset", 0);
        }
    }
}