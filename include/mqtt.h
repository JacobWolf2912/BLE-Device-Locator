#ifndef MQTT_H
#define MQTT_H



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
