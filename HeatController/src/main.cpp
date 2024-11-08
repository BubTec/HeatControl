#include "config.h"
#include "wifi_manager.h"
#include "heating_control.h"
#include "web_server.h"
#include "logger.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== HeatControl Start ===");
    
    // Einmalig ausführen und dann auskommentieren:
    // resetConfig();
    
    loadConfig();
    checkOperationMode();
    
    setupWiFi();
    setupHeating();
    setupWebServer();
}

void loop() {
    handleWiFi();
    handleHeating();
    handleWebServer();
}