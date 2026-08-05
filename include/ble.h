#ifndef BLE_H
#define BLE_H

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

#endif
