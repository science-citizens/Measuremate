#define TINY_GSM_MODEM_SIM7600
#define BLYNK_TEMPLATE_ID "TMPL4kKYByLFC"
#define BLYNK_TEMPLATE_NAME "Measuremate3"
#define BLYNK_AUTH_TOKEN "hp2TInrGs5WulHwA02qOtYWJfxEWT4NP"
#define BLYNK_PRINT SerialUSB

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>

// Pin definitions for sensors
#define TURBIDITY_PIN A1  // Turbidity sensor pin
#define GL136_PIN A0      // GL-136 water level sensor pin
#define FLOW_PIN 5        // Flowmeter sensor pin (connect to digital pin 5)

// LTE pins
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7

// APN and Blynk credentials
const char apn[] = "globaldata.iot";
const char user[] = "";
const char pass[] = "";
const char auth[] = "hp2TInrGs5WulHwA02qOtYWJfxEWT4NP";

// Sensor variables
uint16_t turbidityVoltage;
float turbidityNTU;
uint16_t waterLevelVoltage;
float waterLevel;
float flowRate;
unsigned long pulseDuration;
const float calibrationFactor = 7.5; // Pulses per liter for flowmeter

TinyGsm modem(Serial1);

void powerOnLTE();
void readTurbiditySensor();
void readWaterLevelSensor();
void calculateFlowRate();

void setup() {
  SerialUSB.begin(115200);
  while (!SerialUSB) {
    ; // Wait for Serial Monitor to connect
  }

  SerialUSB.println("Powering on LTE module...");
  powerOnLTE();

  // Start modem communication
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

  // Setup sensor pins
  pinMode(FLOW_PIN, INPUT);
}

void loop() {
  Blynk.run(); // Keeps the Blynk connection alive

  // Read sensors and send data to Blynk
  readTurbiditySensor();
  readWaterLevelSensor();
  calculateFlowRate();

  delay(1000); // Wait 1 second before the next measurement
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
  digitalWrite(LTE_FLIGHT_PIN, LOW); // Normal Mode
}

void readTurbiditySensor() {
  turbidityVoltage = analogRead(TURBIDITY_PIN) * 5000.0 / 1024.0; // Voltage in mV
  turbidityNTU = map(turbidityVoltage, 0, 4500, 0, 3000); // Convert to NTU
  SerialUSB.print("Turbidity: ");
  SerialUSB.print(turbidityNTU);
  SerialUSB.println(" NTU");
  Blynk.virtualWrite(V7, turbidityNTU); // Send to Blynk
}

void readWaterLevelSensor() {
  waterLevelVoltage = analogRead(GL136_PIN) * 5000.0 / 1024.0; // Voltage in mV
  waterLevel = map(waterLevelVoltage, 0, 5000, 0, 100); // Convert to percentage
  SerialUSB.print("Water Level: ");
  SerialUSB.print(waterLevel);
  SerialUSB.println("%");
  Blynk.virtualWrite(V0, waterLevel); // Send to Blynk
}

void calculateFlowRate() {
  pulseDuration = pulseIn(FLOW_PIN, HIGH); // Time of a single pulse in microseconds

  if (pulseDuration > 0) {
    float pulseFrequency = 1000000.0 / pulseDuration; // Frequency in Hz
    flowRate = (pulseFrequency / calibrationFactor); // Flow rate in L/min
    SerialUSB.print("Flow rate: ");
    SerialUSB.print(flowRate);
    SerialUSB.println(" L/min");
    Blynk.virtualWrite(V4, flowRate); // Send to Blynk
  } else {
    SerialUSB.println("Flow rate: 0 L/min");
    Blynk.virtualWrite(V4, 0); // Send 0 to Blynk if no flow detected
  }
}
