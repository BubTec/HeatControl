#include "config.h"
#include "wifi_manager.h"
#include "heating_control.h"
#include "web_server.h"
#include "logger.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== HeatControl v1.0 Starting ===");
    
    // First load configuration
    loadConfig();
    checkOperationMode();
    
    // Then initialize components
    setupWiFi();
    setupHeating();
    setupWebServer();
    
    Serial.println("=== Startup complete ===");
}

void loop() {
    handleWiFi();
    handleHeating();
    handleWebServer();
}