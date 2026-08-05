#ifndef DISPLAY_H
#define DISPLAY_H

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

#endif
