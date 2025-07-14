// --- Bibliotheken ---
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GravityTDS.h>       // SEN0244 TDS library
#include "DFRobot_EC.h"       // DFR0300 EC library
#include "DFRobot_PH.h"       // SEN0161 pH library
#include <WiFi.h>
#include <BlynkSimpleEsp32.h> // of: BlynkSimpleEsp8266.h voor ESP8266

// --- Blynk credentials ---
#define BLYNK_TEMPLATE_ID   "TMPL4kKYByLFC"
#define BLYNK_TEMPLATE_NAME "Measuremate3"
#define BLYNK_AUTH_TOKEN    "c4Pi4KWISLQs_quo-X9QRyAgoqauWid-"

// --- WiFi credentials (vul zelf in) ---
const char* ssid     = "JOUW_SSID";
const char* password = "JOUW_WACHTWOORD";

// --- Multiplexer pinnen ---
#define S0 4
#define S1 5
#define S2 6
#define S3 7

void selectChannel(int ch) {
  digitalWrite(S0, (ch >> 0) & 1);
  digitalWrite(S1, (ch >> 1) & 1);
  digitalWrite(S2, (ch >> 2) & 1);
  digitalWrite(S3, (ch >> 3) & 1);
}

// --- DS18B20 (kanaal 0) ---
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- TDS (kanaal 1) ---
#define TDS_PIN A0
GravityTDS gravityTds;

// --- EC (kanaal 2) ---
#define EC_PIN A1
DFRobot_EC ec;

// --- pH (kanaal 3) ---
#define PH_PIN A2
DFRobot_PH ph;

// timers
unsigned long lastMillis = 0;
const unsigned long interval = 2000; // meet elke 2s

void setup() {
  Serial.begin(115200);

  // Multiplexer outputen
  pinMode(S0, OUTPUT); pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT); pinMode(S3, OUTPUT);
  selectChannel(0); // start zonder voeding

  // DS18B20
  sensors.begin();

  // TDS
  gravityTds.setPin(TDS_PIN);
  gravityTds.setAref(5.0);
  gravityTds.setAdcRange(1024);
  gravityTds.begin();

  // EC
  ec.begin();

  // pH
  ph.begin();

  // Blynk + WiFi
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
}

void loop() {
  Blynk.run();

  if (millis() - lastMillis < interval) return;
  lastMillis = millis();

  float temperature = NAN, tds= NAN, ecValue = NAN, phValue = NAN;

  // ——— Temperatuur ———
  selectChannel(0);
  delay(100);                      // stabilisatietijd
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  Serial.printf("Temp: %.2f°C\n", temperature);
  Blynk.virtualWrite(V5, temperature);

  // ——— TDS ———
  selectChannel(1);
  delay(100);
  gravityTds.setTemperature(temperature);
  gravityTds.update();
  tds = gravityTds.getTdsValue();
  Serial.printf("TDS: %.0f ppm\n", tds);
  Blynk.virtualWrite(V6, tds);

  // ——— Conductivity ———
  selectChannel(2);
  delay(100);
  {
    float voltage = analogRead(EC_PIN) / 1024.0 * 5000;
    ecValue = ec.readEC(voltage, temperature);
  }
  Serial.printf("EC: %.2f ms/cm\n", ecValue);
  Blynk.virtualWrite(V10, ecValue);

  // ——— pH ———
  selectChannel(3);
  delay(100);
  {
    float voltage = analogRead(PH_PIN) / 1024.0 * 5000;
    phValue = ph.readPH(voltage, temperature);
  }
  Serial.printf("pH: %.2f\n", phValue);
  Blynk.virtualWrite(V9, phValue);

  // Schakel voeding uit
  selectChannel(0);
}
