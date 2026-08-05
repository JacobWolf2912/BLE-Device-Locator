#ifndef CONFIG_H
#define CONFIG_H

// Uncomment this line to clear all bonded devices on next startup
// #define CLEAR_BONDED_DEVICES

// I2C LCD setup (address 0x27, 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi and MQTT setup
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "fsiot/devicelocator/telemetry";
const char* mqtt_command_topic = "fsiot/devicelocator/command";
unsigned long lastMqttSend = 0;
const unsigned long MQTT_SEND_INTERVAL = 10000;

// BLE client setup
BLEClient* pClient = nullptr;
BLEAddress pairedPhoneAddress(TARGET_BLT_ADDRESS);
bool connected = false;
bool isPaired = false;
bool wasConnected = false;
uint8_t pairedBDAddr[6] = {0};
String pairedDeviceName = "";

// RSSI smoothing
const int RSSI_SAMPLES = 5;
int rssiBuffer[RSSI_SAMPLES] = {0};
int rssiIndex = 0;

// Display update timing and outlier rejection
unsigned long lastDisplayTime = 0;
const unsigned long DISPLAY_INTERVAL = 1000;
float lastValidDistance = 0;
const float OUTLIER_THRESHOLD = 0.5;

// RSSI threshold values for distance estimation
const int RSSI_AT_1M = -55;
const float PATH_LOSS = 2.0;

// Global variables
int32_t targetRSSI = 0;
bool targetFound = false;

#endif
