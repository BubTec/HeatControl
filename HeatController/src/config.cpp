#include "config.h"
#include "logger.h"

RTCData rtcData;
Config config;

void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, config);
    EEPROM.end();
    
    bool needsReset = false;
    for (size_t i = 0; i < sizeof(config.ssid); i++) {
        if (config.ssid[i] != 0 && (config.ssid[i] < 32 || config.ssid[i] > 126)) {
            needsReset = true;
            break;
        }
    }
    for (size_t i = 0; i < sizeof(config.password); i++) {
        if (config.password[i] != 0 && (config.password[i] < 32 || config.password[i] > 126)) {
            needsReset = true;
            break;
        }
    }
    
    if (config.magic != EEPROM_MAGIC || needsReset) {
        Serial.println("Ungültige Konfiguration gefunden, setze zurück...");
        resetConfig();
    }
    
    rtcData.magic = config.lastMode;
    system_rtc_mem_write(64, &rtcData, sizeof(rtcData));
}

void saveConfig() {
    config.magic = EEPROM_MAGIC;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("Konfiguration gespeichert");
}

void checkOperationMode() {
    system_rtc_mem_read(64, &rtcData, sizeof(rtcData));
    
    if (rtcData.magic == MAGIC_HEATER_ON) {
        rtcData.magic = MAGIC_NORMAL;
        config.lastMode = MAGIC_NORMAL;
        addLog("Wechsel zu Normal-Modus", 0);
    } else if (rtcData.magic == MAGIC_NORMAL) {
        rtcData.magic = MAGIC_HEATER_ON;
        config.lastMode = MAGIC_HEATER_ON;
        addLog("Wechsel zu Heizung-An Modus (100%)", 0);
    } else {
        rtcData.magic = config.lastMode;
        addLog("Wiederherstellung des letzten Modus", 0);
    }
    
    rtcData.bootCount++;
    system_rtc_mem_write(64, &rtcData, sizeof(rtcData));
    saveConfig();
}

void resetConfig() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    EEPROM.end();
    
    strncpy(config.ssid, "HeatControl", sizeof(config.ssid));
    strncpy(config.password, "HeatControl", sizeof(config.password));
    config.targetTemp1 = 22.0;
    config.targetTemp2 = 22.0;
    config.Kp1 = 20.0;
    config.Ki1 = 0.02;
    config.Kd1 = 10.0;
    config.Kp2 = 20.0;
    config.Ki2 = 0.02;
    config.Kd2 = 10.0;
    config.lastMode = MAGIC_NORMAL;
    config.magic = EEPROM_MAGIC;
    
    saveConfig();
    
    addLog("Konfiguration zurückgesetzt", 0);
} 