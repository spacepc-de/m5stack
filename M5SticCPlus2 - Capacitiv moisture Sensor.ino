# The program measures the soil moisture (via an analog input),
# the battery voltage and sends the data to a server via MQTT. 
# The device then switches to deep sleep. 
# A long press of the button when waking up starts a calibration mode
# in which the dry and wet values are saved and later used to convert the soil moisture percentage.
# After the measurement, WLAN is disconnected and the deep-sleep timer is activated.
#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <algorithm>

// WiFi-Zugangsdaten
const char* ssid        = "Garfield";
const char* password    = "Eichhornweg#2019";

// MQTT-Server-Daten
const char* mqtt_server    = "mqtt.eu.thingsboard.cloud";
const int   mqtt_port      = 1883;
const char* mqtt_client_id = "66mi3z6exbg14z6su4hq";

// Hardware-Pins
const int soilSensorPin = 36;
const int batteryPin    = 38;

// Konfiguration
const long SLEEP_INTERVAL_SEC  = 15L * 60L;  // 15 Minuten
const int  STAT_SAMPLES        = 10;         // Messwerte für Filterung
const int  CAL_HISTORY         = 24*7;       // 1 Woche Historiedaten
const int  WATERING_THRESHOLD  = 15;         // Feuchtigkeitsanstieg für Gießerkennung
const int  RESET_PRESS_TIME_MS = 3000;       // 3 Sekunden für Reset
const int  DYNAMIC_RANGE_DAYS  = 3;          // Anpassungszeitraum
const int  MIN_SENSOR_RANGE    = 500;        // Mindestbereich der Sensorwerte

// NVS-Speicher
Preferences prefs;

// Kalibrierungsdaten
int calDry = 3200;
int calWet = 700;
int learnedMin = 3200;
int learnedMax = 700;
int moistureHistory[CAL_HISTORY];
int historyIndex = 0;
unsigned long lastCalibrationUpdate = 0;

// WiFi & MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi();
void sendSensorData();
void resetCalibration();
void adaptiveCalibration(int currentValue);

void setup() {
  M5.begin();
  Serial.begin(115200);

  // Hardware-Initialisierung
  pinMode(soilSensorPin, INPUT);
  pinMode(batteryPin, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(soilSensorPin, ADC_11db);
  analogSetPinAttenuation(batteryPin, ADC_11db);

  // Kalibrierungswerte laden
  prefs.begin("soilCal", true);
  calDry = prefs.getInt("calDry", 3200);
  calWet = prefs.getInt("calWet", 700);
  learnedMin = prefs.getInt("learnedMin", calDry);
  learnedMax = prefs.getInt("learnedMax", calWet);
  lastCalibrationUpdate = prefs.getULong("lastCal", 0);
  prefs.end();

  // Reset-Knopfcheck
  bool resetPressed = false;
  unsigned long startTime = millis();
  while (millis() - startTime < RESET_PRESS_TIME_MS) {
    M5.update();
    if (!M5.BtnA.isPressed()) {
      resetPressed = false;
      break;
    }
    resetPressed = true;
    delay(50);
  }

  if (resetPressed) {
    Serial.println("Reset Kalibrierungswerte");
    resetCalibration();
  }

  // Messung durchführen
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  sendSensorData();

  // Vor Sleep-Cleanup
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  M5.Power.timerSleep(SLEEP_INTERVAL_SEC);
}

void loop() {}

void setup_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("Verbindung zu WiFi...");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nVerbunden! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFehlgeschlagen! Gehe schlafen...");
    M5.Power.timerSleep(SLEEP_INTERVAL_SEC);
  }
}

void sendSensorData() {
  // Statistische Filterung
  int samples[STAT_SAMPLES];
  for(int i=0; i<STAT_SAMPLES; i++) {
    samples[i] = analogRead(soilSensorPin);
    delay(5);
  }
  std::sort(samples, samples+STAT_SAMPLES);
  int filteredValue = 0;
  for(int i=2; i<STAT_SAMPLES-2; i++) filteredValue += samples[i];
  filteredValue /= (STAT_SAMPLES-4);

  // Adaptive Kalibrierung
  adaptiveCalibration(filteredValue);

  // Hybrid-Kalibrierung
  int hybridDry = std::max(calDry, learnedMax);
  int hybridWet = std::min(calWet, learnedMin);
  
  // Sicherheitsfall für widersprüchliche Kalibrierung
  if(hybridDry <= hybridWet) {
    int tempDry = std::max(calDry, learnedMax);
    hybridDry = std::max(tempDry, 2500);  // Mindesttrockenwert
    int tempWet = std::min(calWet, learnedMin);
    hybridWet = std::min(tempWet, 2000);  // Maximalnasswert
  }

  // Feuchtigkeitsberechnung
  int moisturePercent = map(filteredValue, hybridDry, hybridWet, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // Gießerkennung
  prefs.begin("soilCal", false);
  int prevMoisture = prefs.getInt("prevMoisture", moisturePercent);
  bool wateringDetected = abs(moisturePercent - prevMoisture) > WATERING_THRESHOLD;
  prefs.putInt("prevMoisture", moisturePercent);
  prefs.end();

  // Batteriemessung
  int rawBattery = analogRead(batteryPin);
  float batteryVoltage = rawBattery * (3.3f / 4095.0f) * 2.0f;
  batteryVoltage = constrain(batteryVoltage, 3.0f, 4.2f);
  int batteryPercent = map(batteryVoltage * 100, 300, 420, 0, 100);

  // Low-Battery-Check
  if (batteryVoltage < 3.1) {
    Serial.println("Kritische Batteriespannung!");
    M5.Power.timerSleep(SLEEP_INTERVAL_SEC);
  }

  // MQTT-Payload
  String payload = String("{") +
    "\"soil_moisture\":" + String(moisturePercent) + "," +
    "\"soil_raw\":" + String(filteredValue) + "," +
    "\"battery_voltage\":" + String(batteryVoltage, 2) + "," +
    "\"battery_percent\":" + String(batteryPercent) + "," +
    "\"cal_dry\":" + String(hybridDry) + "," +
    "\"cal_wet\":" + String(hybridWet) + "," +
    "\"watering_event\":" + String(wateringDetected ? "true" : "false") +
  "}";

  // Daten senden
  if (client.connect(mqtt_client_id)) {
    client.publish("v1/devices/me/telemetry", payload.c_str());
    client.disconnect();
  }

  Serial.println("Daten gesendet: " + payload);
}

void resetCalibration() {
  M5.Lcd.wakeup();
  M5.Lcd.setBrightness(200);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Reset Kalibrierung...");

  // Werte zurücksetzen
  prefs.begin("soilCal", false);
  prefs.putInt("calDry", 3200);
  prefs.putInt("calWet", 700);
  prefs.putInt("learnedMin", 3200);
  prefs.putInt("learnedMax", 700);
  prefs.putULong("lastCal", 0);
  prefs.end();

  // Variablen aktualisieren
  calDry = 3200;
  calWet = 700;
  learnedMin = 3200;
  learnedMax = 700;
  lastCalibrationUpdate = 0;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.print("Reset erfolgt!\nStandardwerte:\nTrocken: 3200\nNass: 700");
  delay(5000);
  M5.Lcd.sleep();
}

void adaptiveCalibration(int currentValue) {
  // Zeitbasierte Anpassung
  unsigned long now = millis();
  if (now - lastCalibrationUpdate > DYNAMIC_RANGE_DAYS * 86400000L) {
    learnedMax = (learnedMax * 3 + currentValue) / 4;
    learnedMin = (learnedMin * 3 + currentValue) / 4;
    lastCalibrationUpdate = now;
    
    prefs.begin("soilCal", false);
    prefs.putInt("learnedMin", learnedMin);
    prefs.putInt("learnedMax", learnedMax);
    prefs.putULong("lastCal", lastCalibrationUpdate);
    prefs.end();
  }

  // Sofortige Anpassung bei Extremwerten
  if (currentValue > learnedMax) {
    learnedMax = currentValue;
    prefs.begin("soilCal", false);
    prefs.putInt("learnedMax", learnedMax);
    prefs.end();
  }
  
  if (currentValue < learnedMin) {
    learnedMin = currentValue;
    prefs.begin("soilCal", false);
    prefs.putInt("learnedMin", learnedMin);
    prefs.end();
  }

  // Bereichsvalidierung
  if (learnedMax - learnedMin < MIN_SENSOR_RANGE) {
    int midPoint = (learnedMax + learnedMin) / 2;
    learnedMax = midPoint + MIN_SENSOR_RANGE/2;
    learnedMin = midPoint - MIN_SENSOR_RANGE/2;
    
    prefs.begin("soilCal", false);
    prefs.putInt("learnedMin", learnedMin);
    prefs.putInt("learnedMax", learnedMax);
    prefs.end();
  }
}
