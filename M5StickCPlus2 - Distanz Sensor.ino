#include <M5StickCPlus2.h>
#include <Ultrasonic.h>

// Pins für den HC-SR04
#define TRIG_PIN 25  // Verwendet G25 (GPIO25) als TRIG
#define ECHO_PIN 26  // Verwendet G26 (GPIO26) als ECHO

Ultrasonic ultrasonic(TRIG_PIN, ECHO_PIN);

void setup() {
  M5.begin();  // Initialisierung des M5StickC Plus2
  M5.Lcd.setRotation(1);  // Setzt die Ausrichtung des Displays
  M5.Lcd.setTextSize(4);  // Setzt die Textgröße
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.fillScreen(TFT_BLACK);  // Bildschirm leeren
}

void loop() {
  float distance = ultrasonic.distanceRead();  // Entfernung in cm messen
  
  M5.Lcd.fillScreen(TFT_BLACK);  // Bildschirm löschen, um Flimmern zu vermeiden
  
  // Zentrierte Anzeige der Distanz auf dem Bildschirm
  M5.Lcd.setCursor(10, M5.Lcd.height() / 2 - 10);  // Vertikal zentriert, horizontal leicht verschoben
  M5.Lcd.printf("%.2f cm", distance);
  
  delay(500);  // Wartezeit zur Stabilisierung der Messung
}
