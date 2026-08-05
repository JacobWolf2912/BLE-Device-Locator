#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_gap_ble_api.h>
#include "credentials.h"
#include "config.h"
#include "display.h"
#include "wifi.h"
#include "mqtt.h"
#include "ble.h"

// Flash LED when find device command is received
void flashLED() {
  Serial.println("LED FLASHING - Find device command received!");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

// Calculate distance from RSSI
float calculateDistance(int32_t rssi) {
  if (rssi == 0) return 0;
  float distance = pow(10.0, ((RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS)));
  return distance;
}

// Smooth RSSI with circular buffer
int32_t getSmoothedRSSI(int32_t newRssi) {
  rssiBuffer[rssiIndex] = newRssi;
  rssiIndex = (rssiIndex + 1) % RSSI_SAMPLES;

  int32_t sum = 0;
  for (int i = 0; i < RSSI_SAMPLES; i++) {
    sum += rssiBuffer[i];
  }
  return sum / RSSI_SAMPLES;
}

// Reject distance spikes (outlier filtering)
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

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nBLE Device Locator + MQTT");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(50);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED initialized to OFF");

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
  if (digitalRead(LED_PIN) == HIGH) {
    digitalWrite(LED_PIN, LOW);
  }

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
