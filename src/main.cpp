#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_gap_ble_api.h>
#include "credentials.h"

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

// LED flash function - called when find device command is received
void flashLED() {
  Serial.println("LED FLASHING - Find device command received!");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

float calculateDistance(int32_t rssi) {
  if (rssi == 0) return 0;
  float distance = pow(10.0, ((RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS)));
  return distance;
}

int32_t getSmoothedRSSI(int32_t newRssi) {
  rssiBuffer[rssiIndex] = newRssi;
  rssiIndex = (rssiIndex + 1) % RSSI_SAMPLES;

  int32_t sum = 0;
  for (int i = 0; i < RSSI_SAMPLES; i++) {
    sum += rssiBuffer[i];
  }
  return sum / RSSI_SAMPLES;
}

float filterOutlier(float newDistance) {
  if (lastValidDistance == 0) {
    lastValidDistance = newDistance;
    return newDistance;
  }

  float difference = abs(newDistance - lastValidDistance);
  if (difference > OUTLIER_THRESHOLD) {
    Serial.print("Outlier rejected: ");
    Serial.print(newDistance, 1);
    Serial.print("m (diff: ");
    Serial.print(difference, 1);
    Serial.println("m)");
    return lastValidDistance;
  }

  lastValidDistance = newDistance;
  return newDistance;
}

void clearAllBonds() {
  Serial.println("Clearing all bonded devices...");
  int bondedDeviceCount = esp_ble_get_bond_device_num();
  Serial.print("Found ");
  Serial.print(bondedDeviceCount);
  Serial.println(" bonded device(s)");

  esp_ble_bond_dev_t *bondedDevices = new esp_ble_bond_dev_t[bondedDeviceCount];
  esp_ble_get_bond_device_list(&bondedDeviceCount, bondedDevices);

  for (int i = 0; i < bondedDeviceCount; i++) {
    esp_ble_remove_bond_device(bondedDevices[i].bd_addr);
    Serial.print("Removed bonded device ");
    Serial.println(i + 1);
  }

  delete[] bondedDevices;
  Serial.println("All bonded devices cleared!");
}

void setupDisplay() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.print("BLE Locator");
  delay(2000);
  lcd.clear();
}

void updateDisplay(int32_t rssi, float distance) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RSSI:");
  lcd.print(rssi);
  lcd.print(" dBm");

  lcd.setCursor(0, 1);
  lcd.print("Dist:");
  lcd.print(distance, 1);
  lcd.print("m");
}

void showConnected() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected!");
  delay(2000);
}

void showDisconnected() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Disconnected!");
  delay(2000);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed!");
  }
}

// MQTT callback for incoming messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT message on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  // Check if this is a find device command
  if (String(topic) == mqtt_command_topic) {
    if (message == "find") {
      flashLED();
    }
  }
}

void reconnectMQTT() {
  while (!client.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("MQTT...");
    if (client.connect("DeviceLocator-ESP32")) {
      Serial.println("OK");
      client.setCallback(mqttCallback);
      client.subscribe(mqtt_command_topic);
      Serial.print("Subscribed to: ");
      Serial.println(mqtt_command_topic);
      return;
    }
    delay(500);
  }
}

void sendMQTTData(const String& deviceName, int32_t rssi, float distance) {
  if (!client.connected()) {
    reconnectMQTT();
    if (!client.connected()) return;
  }

  String payload = "{\"DeviceName\":\"" + deviceName + "\",\"Rssi\":" + String(rssi) +
                   ",\"Distance\":" + String(distance, 1) + ",\"Status\":\"Connected\",\"Timestamp\":\"" +
                   String(millis()) + "\"}";

  client.publish(mqtt_topic, payload.c_str());
  Serial.print("MQTT: ");
  Serial.println(payload);
}

class MySecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() {
    Serial.println("PassKey requested");
    return 000000;
  }

  void onPassKeyNotify(uint32_t passKey) {
    Serial.print("PassKey: ");
    Serial.println(passKey);
  }

  bool onConfirmPIN(uint32_t passKey) {
    Serial.print("Confirming: ");
    Serial.println(passKey);
    return true;
  }

  bool onSecurityRequest() {
    Serial.println("Security request");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
    Serial.print("Pairing: ");
    Serial.println(cmpl.success ? "SUCCESS" : "FAILED");

    if (cmpl.success) {
      Serial.print("Address: ");
      for (int i = 0; i < 6; i++) {
        pairedBDAddr[i] = cmpl.bd_addr[i];
        if (cmpl.bd_addr[i] < 0x10) Serial.print("0");
        Serial.print(cmpl.bd_addr[i], HEX);
        if (i < 5) Serial.print(":");
      }
      Serial.println();

      pairedPhoneAddress = BLEAddress(pairedBDAddr);
      isPaired = true;
    }
  }
};

void setupBLE() {
  BLEDevice::init("DeviceLocator");
  BLEAddress myAddress = BLEDevice::getAddress();
  Serial.print("BLE Address: ");
  Serial.println(myAddress.toString().c_str());

  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("BLE ready for pairing");
}

bool connectToPhone() {
  if (!isPaired) return false;

  if (pClient == nullptr) {
    pClient = BLEDevice::createClient();
  }

  if (pClient->connect(pairedPhoneAddress, BLE_ADDR_TYPE_RANDOM)) {
    Serial.println("Phone connected!");
    pairedDeviceName = pClient->getPeerAddress().toString().c_str();
    connected = true;
    targetFound = true;
    return true;
  }
  return false;
}

int32_t readConnectionRSSI() {
  if (!pClient || !connected) return 0;
  return pClient->getRssi();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nBLE Device Locator + MQTT");

  // Initialize LED (starts OFF - will only flash when command received)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  #ifdef CLEAR_BONDED_DEVICES
  clearAllBonds();
  #endif

  setupDisplay();
  connectToWiFi();
  client.setServer(mqtt_server, mqtt_port);
  setupBLE();

  Serial.println("Ready");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    client.loop();
  }

  if (!isPaired) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting...");
    delay(1000);
  } else if (!connected) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");
    connectToPhone();
    delay(2000);
  } else {
    if (!wasConnected) {
      showConnected();
      wasConnected = true;
    }

    targetRSSI = readConnectionRSSI();

    if (targetRSSI != 0 && targetRSSI != 127) {
      int32_t smoothedRssi = getSmoothedRSSI(targetRSSI);
      float filteredDistance = filterOutlier(calculateDistance(smoothedRssi));

      if (millis() - lastDisplayTime >= DISPLAY_INTERVAL) {
        updateDisplay(smoothedRssi, filteredDistance);
        lastDisplayTime = millis();
      }

      if (millis() - lastMqttSend >= MQTT_SEND_INTERVAL) {
        sendMQTTData(DEVICE_FRIENDLY_NAME, smoothedRssi, filteredDistance);
        lastMqttSend = millis();
      }
    } else {
      connected = false;
      wasConnected = false;
      isPaired = false;
      lastValidDistance = 0;
      showDisconnected();
      if (pClient) pClient->disconnect();
    }

    delay(100);
  }
}
