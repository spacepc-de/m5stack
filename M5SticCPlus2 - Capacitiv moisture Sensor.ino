# The program measures the soil moisture (via an analog input),
# the battery voltage and sends the data to a server via MQTT. 
# The device then switches to deep sleep. 
# A long press of the button when waking up starts a calibration mode
# in which the dry and wet values are saved and later used to convert the soil moisture percentage.
# After the measurement, WLAN is disconnected and the deep-sleep timer is activated.

Translated with DeepL.com (free version)

#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_sleep.h>
#include <Preferences.h>

// WiFi-Zugangsdaten
const char* ssid        = "Garfield";
const char* password    = "Eichhornweg#2019";

// MQTT-Server-Daten
const char* mqtt_server    = "mqtt.eu.thingsboard.cloud";
const int   mqtt_port      = 1883;
const char* mqtt_client_id = "66mi3z6exbg14z6su4hq";

// Bodenfeuchtigkeitssensor & Batterie
const int soilSensorPin = 36; 
const int batteryPin    = 38;

// Deep-Sleep-Zeit: 15 Minuten
const long SLEEP_INTERVAL_US = 15L * 60L * 1000000L;

// NVS-Speicher für Kalibrierung
Preferences prefs;

// Standard-Kalibrierungswerte (falls noch nicht gespeichert)
int calDry = 3200;  // Trocken
int calWet = 700;   // Nass

// WiFi + MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Annahme: Der Button A liegt an GPIO 37 und geht beim Drücken auf LOW
// -> Bitte anpassen, wenn dein Setup anders ist!
#define BUTTON_GPIO 37

void setup_wifi();
void sendSensorData();
void calibrateSensor();

void setup() {
  M5.begin();
  Serial.begin(115200);

  pinMode(soilSensorPin, INPUT);
  pinMode(batteryPin,    INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(soilSensorPin, ADC_11db);
  analogSetPinAttenuation(batteryPin,    ADC_11db);

  // Button-Pin auf INPUT_PULLUP
  pinMode(BUTTON_GPIO, INPUT_PULLUP);

  // Kalibrierungswerte aus NVS laden
  prefs.begin("soilCal", false);
  calDry = prefs.getInt("calDry", 3200);
  calWet = prefs.getInt("calWet", 700);
  prefs.end();

  // Aufwachgrund prüfen
  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
  if (wakeReason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Aufgewacht durch Button (EXT0). Teste langen Druck...");
    bool calibrate = false;
    unsigned long pressStart = 0;
    unsigned long checkStart = millis();
    while (millis() - checkStart < 3000) {
      M5.update();
      if (M5.BtnA.isPressed()) {
        if (pressStart == 0) {
          pressStart = millis();
        }
        if (millis() - pressStart > 2000) {
          calibrate = true;
          break;
        }
      } else {
        pressStart = 0;  
      }
      delay(50);
    }

    if (calibrate) {
      Serial.println("Starte Kalibrierungsmodus.");
      calibrateSensor();
    } else {
      Serial.println("Kein Kalibrierungsmodus, normaler Betrieb.");
    }
  } else {
    Serial.println("Aufwachgrund: Timer oder Kaltstart.");
  }

  // WLAN
  setup_wifi();
  // MQTT
  client.setServer(mqtt_server, mqtt_port);

  // Messung und Senden
  sendSensorData();

  // WLAN aus
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Deep-Sleep vorbereiten
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_GPIO, 0);

  Serial.println("Gehe in Deep-Sleep...");
  esp_deep_sleep_start();
}

void loop() {
  // Leer
}

/**
 * WLAN verbinden
 */
void setup_wifi() {
  WiFi.begin(ssid, password);
  Serial.println("Verbindungsaufbau WiFi...");

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbunden!");
  } else {
    Serial.println("\nWiFi NICHT verbunden, gehe direkt schlafen...");
    esp_deep_sleep_start();
  }
}

/**
 * Sensorwerte lesen und versenden
 */
void sendSensorData() {
  // Bodenfeuchte
  int sensorValue = analogRead(soilSensorPin);
  int moisturePercent = map(sensorValue, calDry, calWet, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // Batterie-Spannung
  int rawBattery = analogRead(batteryPin);
  float batteryVoltage = rawBattery * (3.3f / 4095.0f) * 2.0f;

  // Batterie-Prozent (3,0 V = 0 %, 4,2 V = 100 %)
  float battPercentF = (batteryVoltage - 3.0f) / (3.96f - 3.0f) * 100.0f;
  if (battPercentF < 0)   battPercentF = 0;
  if (battPercentF > 100) battPercentF = 100;
  int batteryPercent = (int)battPercentF;

  Serial.print("Feuchtigkeit: ");
  Serial.print(moisturePercent);
  Serial.print("% | Batterie: ");
  Serial.print(batteryVoltage);
  Serial.print(" V (");
  Serial.print(batteryPercent);
  Serial.println("%)");

  // Wenn Spannung < 3.1V -> Sofort wieder schlafen, um Tiefentladung zu vermeiden
  if (batteryVoltage < 3.1) {
    Serial.println("Batterie unter 3.1V! -> Direkt in Deep-Sleep...");
    esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_GPIO, 0);
    esp_deep_sleep_start();
  }

  // JSON für MQTT
  String payload = "{\"soil_moisture\":" + String(moisturePercent) +
                   ",\"battery_voltage\":" + String(batteryVoltage, 2) +
                   ",\"battery_percent\":" + String(batteryPercent) + "}";

  // Senden
  if (client.connect(mqtt_client_id)) {
    client.publish("v1/devices/me/telemetry", payload.c_str());
    client.disconnect();
  }
}

/**
 * Kalibrierungsmodus:
 * 1) Display einschalten
 * 2) Trocken messen (kurzer Button-Druck)
 * 3) Nass messen (kurzer Button-Druck)
 * 4) Speichern in NVS
 * 5) Anzeige "Kalibriert"
 */
void calibrateSensor() {
  M5.Lcd.wakeup();
  M5.Lcd.setBrightness(200);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Kalibrierung\n\nSensor TROCKEN.\nDruecke kurz,\nwenn bereit.");

  // Warte auf kurzen Druck
  while (true) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      break;
    }
    delay(50);
  }
  int dryVal = analogRead(soilSensorPin);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Sensor NASS.\nDruecke kurz,\nwenn bereit.");

  while (true) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      break;
    }
    delay(50);
  }
  int wetVal = analogRead(soilSensorPin);

  // Speichern
  prefs.begin("soilCal", false);
  prefs.putInt("calDry", dryVal);
  prefs.putInt("calWet", wetVal);
  prefs.end();

  calDry = dryVal;
  calWet = wetVal;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Kalibriert!\n5 Sekunden warten...");
  delay(5000);

  M5.Lcd.sleep();
}
