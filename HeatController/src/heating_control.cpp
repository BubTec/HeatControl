#include "heating_control.h"
#include "logger.h"

// Pins
#define ONE_WIRE_BUS 2
#define MOSFET1_PIN 4
#define MOSFET2_PIN 5

#define TEMP_HISTORY_SIZE 6
#define CONTROL_INTERVAL 10000

// Nur die lokalen Variablen behalten
float integral1 = 0;
float integral2 = 0;
float tempHistory1[TEMP_HISTORY_SIZE] = {0};
float tempHistory2[TEMP_HISTORY_SIZE] = {0};
int historyIndex = 0;
unsigned long lastControlTime = 0;

// OneWire Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1, sensor2;

void setupHeating() {
    pinMode(MOSFET1_PIN, OUTPUT);
    pinMode(MOSFET2_PIN, OUTPUT);
    
    analogWriteFreq(1000);
    analogWriteRange(1023);
    
    sensors.begin();
    
    if(sensors.getDeviceCount() < 2) {
        addLog("Nicht genügend Temperatursensoren gefunden!", 1);
    } else {
        sensors.getAddress(sensor1, 0);
        sensors.getAddress(sensor2, 1);
        sensors.setResolution(sensor1, 12);
        sensors.setResolution(sensor2, 12);
        addLog("Temperatursensoren initialisiert", 0);
    }
}

void handleHeating() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastControlTime >= CONTROL_INTERVAL) {
        lastControlTime = currentMillis;
        updateHeating();
    }
}

void updateHeating() {
    if(sensors.getDeviceCount() > 0) {
        sensors.requestTemperatures();
        
        // Heizkreis 1
        float temp1 = sensors.getTempC(sensor1);
        if(temp1 != DEVICE_DISCONNECTED_C) {
            currentTemp1 = temp1;
            if(rtcData.magic == MAGIC_HEATER_ON) {
                currentPWM1 = MAX_PWM;
            } else {
                currentPWM1 = (int)calculateHeatingPower1(currentTemp1);
            }
            analogWrite(MOSFET1_PIN, currentPWM1);
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "HK1: %.1f°C -> %d%% PWM", currentTemp1, (int)((float)currentPWM1/MAX_PWM * 100));
            addLog(logMsg, 0);
        } else {
            addLog("Heizkreis 1: Sensor Fehler!", 2);
            currentPWM1 = 0;
            analogWrite(MOSFET1_PIN, 0);
        }
        
        // Heizkreis 2
        if(sensors.getDeviceCount() > 1) {
            float temp2 = sensors.getTempC(sensor2);
            if(temp2 != DEVICE_DISCONNECTED_C) {
                currentTemp2 = temp2;
                if(rtcData.magic == MAGIC_HEATER_ON) {
                    currentPWM2 = MAX_PWM;
                } else {
                    currentPWM2 = (int)calculateHeatingPower2(currentTemp2);
                }
                analogWrite(MOSFET2_PIN, currentPWM2);
                char logMsg[64];
                snprintf(logMsg, sizeof(logMsg), "HK2: %.1f°C -> %d%% PWM", currentTemp2, (int)((float)currentPWM2/MAX_PWM * 100));
                addLog(logMsg, 0);
            } else {
                addLog("Heizkreis 2: Sensor Fehler!", 2);
                currentPWM2 = 0;
                analogWrite(MOSFET2_PIN, 0);
            }
        }
    } else {
        addLog("Keine Sensoren gefunden!", 2);
        currentPWM1 = 0;
        currentPWM2 = 0;
        analogWrite(MOSFET1_PIN, 0);
        analogWrite(MOSFET2_PIN, 0);
    }
    
    historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
}

float calculateHeatingPower1(float currentTemp) {
    float error = TARGET_TEMP1 - currentTemp;
    
    if (abs(error) < 5.0) {
        integral1 += error * (CONTROL_INTERVAL / 1000.0);
    }
    
    integral1 = constrain(integral1, -30, 30);
    
    float output = (config.Kp1 * error) + (config.Ki1 * integral1) - (config.Kd1 * 
        (currentTemp - tempHistory1[(historyIndex + TEMP_HISTORY_SIZE - 1) % TEMP_HISTORY_SIZE]) / 
        (CONTROL_INTERVAL / 1000.0));
    
    output = constrain(output, MIN_PWM, MAX_PWM);
    
    if (output >= MAX_PWM || output <= MIN_PWM) {
        integral1 *= 0.9;
    }
    
    return output;
}

float calculateHeatingPower2(float currentTemp) {
    float error = TARGET_TEMP2 - currentTemp;
    
    if (abs(error) < 5.0) {
        integral2 += error * (CONTROL_INTERVAL / 1000.0);
    }
    
    integral2 = constrain(integral2, -30, 30);
    
    float output = (config.Kp2 * error) + (config.Ki2 * integral2) - (config.Kd2 * 
        (currentTemp - tempHistory2[(historyIndex + TEMP_HISTORY_SIZE - 1) % TEMP_HISTORY_SIZE]) / 
        (CONTROL_INTERVAL / 1000.0));
    
    output = constrain(output, MIN_PWM, MAX_PWM);
    
    if (output >= MAX_PWM || output <= MIN_PWM) {
        integral2 *= 0.9;
    }
    
    return output;
}