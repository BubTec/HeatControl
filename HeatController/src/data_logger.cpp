#include "data_logger.h"
#include "globals.h"
#include "logger.h"

unsigned long lastDataLog = 0;

void setupDataLogger() {
    if (!SPIFFS.begin()) {
        addLog("SPIFFS Initialisierung fehlgeschlagen!", 2);
        return;
    }
    
    // Prüfe ob Logdatei existiert, wenn nicht erstelle Header
    if (!SPIFFS.exists(LOG_FILE)) {
        File f = SPIFFS.open(LOG_FILE, "w");
        if (f) {
            f.println("timestamp,temp1,temp2,pwm1,pwm2");
            f.close();
            addLog("Neue Logdatei erstellt", 0);
        }
    }
}

void logData() {
    if (millis() - lastDataLog < LOG_INTERVAL) return;
    lastDataLog = millis();
    
    File f = SPIFFS.open(LOG_FILE, "a");
    if (f) {
        // Schreibe Zeitstempel (Sekunden seit Start), Temperaturen und PWM
        f.printf("%lu,%.1f,%.1f,%d,%d\n", 
            millis()/1000, 
            currentTemp1, 
            currentTemp2, 
            (int)((float)currentPWM1/MAX_PWM * 100),
            (int)((float)currentPWM2/MAX_PWM * 100)
        );
        f.close();
        
        // Prüfe Dateigröße und rotiere wenn nötig
        if (SPIFFS.exists(LOG_FILE)) {
            File f = SPIFFS.open(LOG_FILE, "r");
            if (f.size() > MAX_LOG_FILE_SIZE) {
                // Behalte nur die letzten 75% der Daten
                String newContent;
                f.seek(f.size() / 4);  // Überspringe erstes Viertel
                while (f.available()) {
                    newContent += (char)f.read();
                }
                f.close();
                
                f = SPIFFS.open(LOG_FILE, "w");
                f.println("timestamp,temp1,temp2,pwm1,pwm2");
                f.print(newContent);
                f.close();
                addLog("Logdatei rotiert", 0);
            }
        }
    }
}

String getLogData(uint32_t hours) {
    String data = "[";
    if (SPIFFS.exists(LOG_FILE)) {
        File f = SPIFFS.open(LOG_FILE, "r");
        
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
    if (SPIFFS.exists(LOG_FILE)) {
        SPIFFS.remove(LOG_FILE);
        File f = SPIFFS.open(LOG_FILE, "w");
        if (f) {
            f.println("timestamp,temp1,temp2,pwm1,pwm2");
            f.close();
            addLog("Logdatei zurückgesetzt", 0);
        }
    }
}