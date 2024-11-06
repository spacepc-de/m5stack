#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("\nI2C-Scanner für M5StickC Plus2");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanne nach I2C-Geräten...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C-Gerät gefunden bei Adresse 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unbekannter Fehler bei Adresse 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("Keine I2C-Geräte gefunden\n");
  else
    Serial.println("Scan abgeschlossen\n");

  delay(5000); // 5 Sekunden warten, bevor erneut gescannt wird
}
