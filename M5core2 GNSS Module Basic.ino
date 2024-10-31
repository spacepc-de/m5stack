/*
 * GNSS Data Display for M5Core2 with TinyGPS++ Library
 * 
 * Description:
 * This script configures an M5Core2 device to display real-time GNSS data 
 * such as latitude, longitude, altitude, speed, and satellite count using a 
 * connected GNSS module. Data updates every second. The display is set with 
 * a black background and white text for clarity, and values are centered for 
 * easy reading. The code also adjusts the time to UTC+1 (for Central European 
 * Winter Time).
 * 
 * Author: spacepc.de
 */

#include <M5Core2.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

TinyGPSPlus gps;
HardwareSerial SerialGPS(2);

// Positionsdefinitionen für Anzeige
const int xCenter = 160; // Mittige Ausrichtung der Werte
const int yLatitude = 50;
const int yLongitude = 90;
const int yAltitude = 130;
const int ySpeed = 170;
const int ySatellites = 210;

void setup() {
  M5.begin();
  Serial.begin(115200);
  SerialGPS.begin(38400, SERIAL_8N1, 13, 14); // RX=13, TX=14

  // Hintergrund und statische Texte auf dem Display
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK); // Weißer Text auf schwarzem Hintergrund

  displayStaticText();
}

void displayStaticText() {
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print("GNSS Data");

  M5.Lcd.setCursor(10, yLatitude);
  M5.Lcd.print("Latitude");
  M5.Lcd.drawLine(10, yLatitude + 20, 310, yLatitude + 20, WHITE);

  M5.Lcd.setCursor(10, yLongitude);
  M5.Lcd.print("Longitude");
  M5.Lcd.drawLine(10, yLongitude + 20, 310, yLongitude + 20, WHITE);

  M5.Lcd.setCursor(10, yAltitude);
  M5.Lcd.print("Altitude");
  M5.Lcd.drawLine(10, yAltitude + 20, 310, yAltitude + 20, WHITE);

  M5.Lcd.setCursor(10, ySpeed);
  M5.Lcd.print("Speed");
  M5.Lcd.drawLine(10, ySpeed + 20, 310, ySpeed + 20, WHITE);

  M5.Lcd.setCursor(10, ySatellites);
  M5.Lcd.print("Satellites");
  M5.Lcd.drawLine(10, ySatellites + 20, 310, ySatellites + 20, WHITE);
}

void updateGPSData() {
  // Latitude
  M5.Lcd.setCursor(xCenter, yLatitude);
  M5.Lcd.fillRect(xCenter, yLatitude, 140, 20, BLACK); // Bereich löschen
  M5.Lcd.printf("%.6f", gps.location.lat());

  // Longitude
  M5.Lcd.setCursor(xCenter, yLongitude);
  M5.Lcd.fillRect(xCenter, yLongitude, 140, 20, BLACK);
  M5.Lcd.printf("%.6f", gps.location.lng());

  // Altitude
  M5.Lcd.setCursor(xCenter, yAltitude);
  M5.Lcd.fillRect(xCenter, yAltitude, 140, 20, BLACK);
  M5.Lcd.printf("%.2f m", gps.altitude.meters());

  // Speed
  M5.Lcd.setCursor(xCenter, ySpeed);
  M5.Lcd.fillRect(xCenter, ySpeed, 140, 20, BLACK);
  M5.Lcd.printf("%.2f km/h", gps.speed.kmph());

  // Satellites
  M5.Lcd.setCursor(xCenter, ySatellites);
  M5.Lcd.fillRect(xCenter, ySatellites, 140, 20, BLACK);
  M5.Lcd.printf("%d", gps.satellites.value());
}

void updateTime() {
  // Zeit anpassen und darstellen (UTC+1 für Winterzeit in Deutschland)
  int hour = gps.time.hour() + 1;
  if (hour >= 24) hour -= 24;

  M5.Lcd.setCursor(210, 10); // Position rechts oben
  M5.Lcd.fillRect(210, 10, 100, 20, BLACK); // Uhrzeitbereich leeren
  M5.Lcd.printf("%02d:%02d:%02d", hour, gps.time.minute(), gps.time.second());
}

void loop() {
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());

    if (gps.location.isUpdated()) {
      updateGPSData();
      updateTime();
      delay(1000); // Aktualisierungsintervall
    }
  }
}
