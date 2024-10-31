#include <M5Core2.h>

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);

  // Display instructions
  M5.Lcd.setCursor(50, 30);
  M5.Lcd.print("Touch Button Test");

  M5.Lcd.setCursor(20, 80);
  M5.Lcd.print("Left Button: Pressed");
  M5.Lcd.setCursor(20, 130);
  M5.Lcd.print("Middle Button: Pressed");
  M5.Lcd.setCursor(20, 180);
  M5.Lcd.print("Right Button: Pressed");

  delay(1000);

  // Clear the "Pressed" text after setup
  M5.Lcd.fillRect(20, 80, 200, 40, BLACK);
  M5.Lcd.fillRect(20, 130, 200, 40, BLACK);
  M5.Lcd.fillRect(20, 180, 200, 40, BLACK);
}

void loop() {
  M5.update();

  // Left Button
  if (M5.Touch.ispressed() && M5.Touch.getPressPoint().x < 100) {
    M5.Lcd.setCursor(20, 80);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.print("Left Button: Pressed");
  } else {
    M5.Lcd.setCursor(20, 80);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.print("Left Button:       ");
  }

  // Middle Button
  if (M5.Touch.ispressed() && M5.Touch.getPressPoint().x >= 100 && M5.Touch.getPressPoint().x < 220) {
    M5.Lcd.setCursor(20, 130);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.print("Middle Button: Pressed");
  } else {
    M5.Lcd.setCursor(20, 130);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.print("Middle Button:       ");
  }

  // Right Button
  if (M5.Touch.ispressed() && M5.Touch.getPressPoint().x >= 220) {
    M5.Lcd.setCursor(20, 180);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.print("Right Button: Pressed");
  } else {
    M5.Lcd.setCursor(20, 180);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.print("Right Button:       ");
  }

  delay(100); // Debounce
}
