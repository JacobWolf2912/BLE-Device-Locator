#ifndef MQTT_H
#define MQTT_H

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

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT message on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  if (String(topic) == mqtt_command_topic) {
    if (message == "find") {
      flashLED();
    }
  }
}

// Reconnect to MQTT
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

// Send MQTT data
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

#endif
