#include "config.h"
#include "logger.h"

RTCData rtcData;
Config config;

void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, config);
    EEPROM.end();
    
    // Check only Magic Number
    if (config.magic != EEPROM_MAGIC) {
        Serial.println("First initialization - setting defaults");
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
    }
    
    // Debug output of loaded configuration
    Serial.println("Loaded configuration:");
    Serial.printf("SSID: %s\n", config.ssid);
    Serial.printf("Temp1: %.1f°C\n", config.targetTemp1);
    Serial.printf("Temp2: %.1f°C\n", config.targetTemp2);
    
    rtcData.magic = config.lastMode;
    system_rtc_mem_write(64, &rtcData, sizeof(rtcData));
}

void saveConfig() {
    config.magic = EEPROM_MAGIC;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("Configuration saved");
}

void checkOperationMode() {
    // Configure mode pin as input with pull-up
    pinMode(MODE_PIN, INPUT_PULLUP);
    delay(50);  // Short delay for stable reading
    
    // Read mode pin
    bool pinState = digitalRead(MODE_PIN);
    Serial.printf("Mode Pin Status: %d\n", pinState);
    
    // If pin is LOW (capacitor discharged), start in normal mode
    if (pinState == LOW) {
        rtcData.magic = MAGIC_NORMAL;
        config.lastMode = MAGIC_NORMAL;
        addLog("Normal mode activated (capacitor discharged)", 0);
        
        // Set pin HIGH for next restart
        pinMode(MODE_PIN, OUTPUT);
        digitalWrite(MODE_PIN, HIGH);
    }
    // If pin is HIGH (capacitor charged), switch mode
    else {
        // If we were in normal mode, switch to full power
        if (config.lastMode == MAGIC_NORMAL) {
            rtcData.magic = MAGIC_HEATER_ON;
            config.lastMode = MAGIC_HEATER_ON;
            addLog("FULL POWER mode activated (capacitor charged)", 0);
            
            // Set pin LOW for next restart
            pinMode(MODE_PIN, OUTPUT);
            digitalWrite(MODE_PIN, LOW);
        }
        // If we were in full power mode, switch to normal
        else {
            rtcData.magic = MAGIC_NORMAL;
            config.lastMode = MAGIC_NORMAL;
            addLog("Normal mode activated (capacitor charged)", 0);
            
            // Set pin LOW for next restart
            pinMode(MODE_PIN, OUTPUT);
            digitalWrite(MODE_PIN, LOW);
        }
    }
    
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
    
    addLog("Configuration reset to defaults", 0);
} 