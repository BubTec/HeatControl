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
bool manualHeater1Enabled = true;
bool manualHeater2Enabled = true;
bool battery1Off = false;
bool battery2Off = false;
unsigned long battery1OffSinceMs = 0;
unsigned long battery2OffSinceMs = 0;

bool powerMode = false;
bool manualMode = false;
bool swapAssignment = false;
bool fileSystemReady = false;

float currentTemp1 = DEVICE_DISCONNECTED_C;
float currentTemp2 = DEVICE_DISCONNECTED_C;
float targetTemp1 = DEFAULT_TARGET_TEMP;
float targetTemp2 = DEFAULT_TARGET_TEMP;
uint8_t manualPowerPercent1 = 25;
uint8_t manualPowerPercent2 = 25;
uint16_t adc1MilliVolts = 0;
uint16_t adc2MilliVolts = 0;
uint8_t battery1CellCount = 3;
float battery1PackVoltage = 0.0F;
float battery1CellVoltage = 0.0F;
uint8_t battery1SocPercent = 0;
uint8_t battery2CellCount = 3;
float battery2PackVoltage = 0.0F;
float battery2CellVoltage = 0.0F;
uint8_t battery2SocPercent = 0;

uint16_t manualPowerToggleMaxOffMs = 500;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
String activeSsid = "HeatControl";
String activePassword = "HeatControl";
char serialLogBuffer[12001] = {0};
size_t serialLogLength = 0;
AsyncWebServer server(80);
DNSServer dnsServer;

}  // namespace HeatControl
