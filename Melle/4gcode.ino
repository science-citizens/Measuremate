#define TINY_GSM_MODEM_SIM7600
#define BLYNK_TEMPLATE_ID "TMPL4ZqWO2Zmv"
#define BLYNK_TEMPLATE_NAME "Measuremate4G"
#define BLYNK_AUTH_TOKEN "QVW31s6a1bDYltJYkyzRgvr0AxnheeCs"
#define BLYNK_PRINT SerialUSB

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>

// Sensorpinnen
#define GL136_PIN A0
#define FLOW_PIN 5

// LTE-modem pinnen
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7

// APN instellingen
const char apn[]  = "globaldata.iot";
const char user[] = "";
const char pass[] = "";
const char auth[] = BLYNK_AUTH_TOKEN;

// Sensorvariabelen
uint16_t waterLevelVoltage;
float waterLevel;

// Flowmeter instellingen
volatile unsigned long pulseCount = 0;
float flowRate = 0.0;
const float calibrationFactor = 0.5; // Omdat pulsen per seconde gelijk zijn aan 0,5 * Q
unsigned long lastMeasurementTime = 0;

// Reconnect cooldown
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 30000; // 30 seconden

TinyGsm modem(Serial1);

void powerOnLTE();
void reconnectBlynk();
void readWaterLevelSensor();
void calculateFlowRate();
void onFlowPulse();

void setup() {
  SerialUSB.begin(115200);
  while (!SerialUSB) ;

  SerialUSB.println("Powering on LTE module...");
  powerOnLTE();

  Serial1.begin(115200);
  delay(3000);

  SerialUSB.println("Initializing modem...");
  if (!modem.restart()) {
    SerialUSB.println("Failed to restart modem.");
    while (true);
  }

  SerialUSB.println("Setting APN...");
  if (!modem.gprsConnect(apn, user, pass)) {
    SerialUSB.println("Failed to connect to GPRS network.");
    while (true);
  }

  SerialUSB.println("Connected to network. Starting Blynk...");
  Blynk.begin(auth, modem, apn, user, pass);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onFlowPulse, RISING);

  lastMeasurementTime = millis();
}

void loop() {
  Blynk.run();

  if (!Blynk.connected() && millis() - lastReconnectAttempt > reconnectInterval) {
    lastReconnectAttempt = millis();
    reconnectBlynk();
  }

  if (millis() - lastMeasurementTime >= 1000) {
    lastMeasurementTime = millis();
    readWaterLevelSensor();
    calculateFlowRate();
  }
}

void powerOnLTE() {
  pinMode(LTE_RESET_PIN, OUTPUT);
  digitalWrite(LTE_RESET_PIN, LOW);

  pinMode(LTE_PWRKEY_PIN, OUTPUT);
  digitalWrite(LTE_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(LTE_PWRKEY_PIN, HIGH);
  delay(2000);
  digitalWrite(LTE_PWRKEY_PIN, LOW);

  pinMode(LTE_FLIGHT_PIN, OUTPUT);
  digitalWrite(LTE_FLIGHT_PIN, LOW);
}

void reconnectBlynk() {
  SerialUSB.println("Reinitializing LTE modem...");
  powerOnLTE();
  delay(3000);

  if (!modem.restart()) {
    SerialUSB.println("Modem restart failed.");
    return;
  }

  if (!modem.gprsConnect(apn, user, pass)) {
    SerialUSB.println("GPRS reconnect failed.");
    return;
  }

  if (!Blynk.connect()) {
    SerialUSB.println("Blynk reconnect failed.");
  } else {
    SerialUSB.println("Reconnected to Blynk.");
    
  // Reattach the interrupt after reconnection
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onFlowPulse, RISING);
  }
}


void readWaterLevelSensor() {
  waterLevelVoltage = analogRead(GL136_PIN) * 5000.0 / 1024.0;
  waterLevel = map(waterLevelVoltage, 0, 5000, 0, 100);
  SerialUSB.print("Water Level: ");
  SerialUSB.print(waterLevel);
  SerialUSB.println("%");
  Blynk.virtualWrite(V0, waterLevel);
}

void calculateFlowRate() {
  noInterrupts();
  unsigned long pulses = pulseCount;
  pulseCount = 0;
  interrupts();

  float frequency = pulses;
  flowRate = frequency / calibrationFactor;

  float minFlowRate = flowRate * 0.97;
  float maxFlowRate = flowRate * 1.03;

  SerialUSB.print("Flow rate: ");
  SerialUSB.print(flowRate);
  SerialUSB.print(" L/min (Measured)");
  SerialUSB.print(" | Min: ");
  SerialUSB.print(minFlowRate);
  SerialUSB.print(" L/min | Max: ");
  SerialUSB.println(maxFlowRate);

  Blynk.virtualWrite(V1, flowRate);
}

void onFlowPulse() {
  pulseCount++;
}
