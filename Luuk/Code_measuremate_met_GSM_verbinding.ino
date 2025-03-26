#define BLYNK_TEMPLATE_ID "TMPL4kKYByLFC"
#define BLYNK_TEMPLATE_NAME "Measuremate3"
#define BLYNK_AUTH_TOKEN "hp2TInrGs5WulHwA02qOtYWJfxEWT4NP"

#define BLYNK_PRINT Serial
#include <SoftwareSerial.h>
#include <BlynkSimpleStream.h>

// Pin definitie voor de sensoren
#define TURBIDITY_PIN A1  // Turbidity sensor pin
#define GL136_PIN A0      // GL-136 water level sensor pin
#define FLOW_PIN 5        // Flowmeter sensor pin (connect to digital pin 5)

// Variabelen voor turbidity sensor
uint16_t turbidityVoltage;
float turbidityNTU;

// Variabelen voor GL-136 waterniveau sensor
uint16_t waterLevelVoltage;
float waterLevel;

// Variabelen voor flowmeter sensor
float flowRate;
unsigned long pulseDuration;  // Tijd van een enkele puls

// Kalibratiefactor voor flowmeter (afhankelijk van de flowmeter specificatie)
const float calibrationFactor = 7.5;  // Aantal pulsen per liter

// SIM7600G-H configuratie
SoftwareSerial sim7600Serial(7, 8); // RX, TX
#define SIM7600_BAUDRATE 115200

void setup() {
  Serial.begin(9600);
  sim7600Serial.begin(SIM7600_BAUDRATE);

  Serial.println("Initializing SIM7600G-H...");

  // Configureer SIM7600G-H
  sendCommand("AT");
  sendCommand("AT+CGATT=1");  // Activeer mobiele netwerkverbinding
  sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  sendCommand("AT+SAPBR=3,1,\"APN\",\"your_apn_here\"");  // Vervang 'your_apn_here' door je APN
  sendCommand("AT+SAPBR=1,1");  // Start de verbinding

  // Verbind Blynk via de SIM7600-seriële poort
  Blynk.begin(sim7600Serial, BLYNK_AUTH_TOKEN);

  // Setup voor flowmeter pin
  pinMode(FLOW_PIN, INPUT);
}

void loop() {
  Blynk.run();
  
  // Sensorwaarden uitlezen
  readTurbiditySensor();
  readWaterLevelSensor();
  calculateFlowRate();

  delay(1000);  // Wacht 1 seconde voor de volgende meting
}

void readTurbiditySensor() {
  turbidityVoltage = analogRead(TURBIDITY_PIN) * 5000.0 / 1024.0;  // Voltage uitlezen en converteren naar millivolt
  turbidityNTU = calculateNTU(turbidityVoltage);  // Converteren naar NTU
  Serial.print("Turbidity: ");
  Serial.print(turbidityNTU);
  Serial.println(" NTU");
  Blynk.virtualWrite(V7, turbidityNTU);  // Stuur turbidity data naar Blynk (virtuele pin V7)
}

// Functie om voltage naar NTU te converteren
float calculateNTU(uint16_t voltage) {
  return map(voltage, 0, 4500, 0, 3000);  // Lineaire conversie van voltage naar NTU
}

void readWaterLevelSensor() {
  waterLevelVoltage = analogRead(GL136_PIN) * 5000.0 / 1024.0;  // Voltage uitlezen en converteren
  waterLevel = map(waterLevelVoltage, 0, 5000, 0, 100);  // Converteren naar percentage (0-100%)
  Serial.print("Water Level: ");
  Serial.print(waterLevel);
  Serial.println("%");
  Blynk.virtualWrite(V0, waterLevel);  // Stuur waterniveau data naar Blynk (virtuele pin V0)
}

void calculateFlowRate() {
  pulseDuration = pulseIn(FLOW_PIN, HIGH);  // Meet de tijd van een enkele HIGH-puls in microseconden

  // Als er een puls gemeten is, bereken de flow rate
  if (pulseDuration > 0) {
    // Bereken de tijd tussen pulsen in seconden (microseconden naar seconden)
    float pulseFrequency = 1000000.0 / pulseDuration;  // Frequentie in Hz (pulsen per seconde)

    // Bereken de flow rate in liters per minuut
    flowRate = (pulseFrequency / calibrationFactor);  // Flow rate in L/min
    Serial.print("Flow rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");
    
    Blynk.virtualWrite(V4, flowRate);  // Stuur flow rate data naar Blynk (virtuele pin V1)
  } else {
    // Geen flow gedetecteerd
    Serial.print("Flow rate: ");
    Serial.print(0);
    Serial.println(" L/min");
    Blynk.virtualWrite(V4, 0);  // Geen flow rate naar Blynk sturen
  }
}

// Helperfunctie om AT-commando's naar de SIM7600G-H te sturen
void sendCommand(const char* command) {
  sim7600Serial.println(command);
  delay(500);
  while (sim7600Serial.available()) {
    Serial.write(sim7600Serial.read());
  }
}
