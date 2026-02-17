#include "app_state.h"

namespace HeatControl {

unsigned long lastPrintMs = 0;
unsigned long lastSensorMs = 0;
unsigned long lastRuntimeSaveMs = 0;
unsigned long lastMemCheckMs = 0;
unsigned long startTimeMs = 0;

uint32_t counter = 0;
uint32_t savedRuntimeMinutes = 0;

bool lastHeater1State = false;
bool lastHeater2State = false;
bool lastSignalPinState = false;
bool lastInputPinState = false;

bool powerMode = false;
bool manualMode = false;
bool swapAssignment = false;
bool fileSystemReady = false;

float currentTemp1 = DEVICE_DISCONNECTED_C;
float currentTemp2 = DEVICE_DISCONNECTED_C;
float targetTemp1 = DEFAULT_TARGET_TEMP;
float targetTemp2 = DEFAULT_TARGET_TEMP;
uint8_t manualPowerPercent = 25;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
String activeSsid = "HeatControl";
String activePassword = "HeatControl";
String serialLogBuffer;
AsyncWebServer server(80);
DNSServer dnsServer;

}  // namespace HeatControl
