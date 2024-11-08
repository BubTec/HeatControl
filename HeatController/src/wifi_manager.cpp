#include "wifi_manager.h"
#include "logger.h"
#include "web_server.h"

DNSServer dnsServer;
const byte DNS_PORT = 53;

void setupWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.persistent(false);
    
    bool apResult = WiFi.softAPConfig(IPAddress(4,3,2,1), IPAddress(4,3,2,1), IPAddress(255,255,255,0));
    if (!apResult) {
        addLog("WiFi AP configuration failed", 2);
    }
    
    if (WiFi.softAP(config.ssid, config.password)) {
        char logMsg[64];
        snprintf(logMsg, sizeof(logMsg), "WiFi AP started - SSID: %s", config.ssid);
        addLog(logMsg, 0);
    } else {
        addLog("WiFi AP start failed", 2);
    }
    
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", IPAddress(4,3,2,1));
}

void handleWiFi() {
    dnsServer.processNextRequest();
} 