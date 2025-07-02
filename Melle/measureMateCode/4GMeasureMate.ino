// Gebruik van TinyGSM voor SIM7600-modem en Blynk voor data-overdracht via mobiel netwerk
#define TINY_GSM_MODEM_SIM7600
#define BLYNK_TEMPLATE_ID "TMPL4ZqWO2Zmv"
#define BLYNK_TEMPLATE_NAME "Measuremate4G"
#define BLYNK_AUTH_TOKEN "QVW31s6a1bDYltJYkyzRgvr0AxnheeCs"
#define BLYNK_PRINT SerialUSB  // Debug-uitvoer via USB

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>

// Sensorpinnen
#define GL136_PIN A0    // Analoge pin voor GL-136 waterniveausensor
#define FLOW_PIN 5      // Digitale input voor flowmeterpulsen

// LTE-modem besturingspinnen
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 8
#define LTE_FLIGHT_PIN 7

// Mobiele netwerkinstellingen
const char apn[]  = "globaldata.iot";
const char user[] = "";
const char pass[] = "";
const char auth[] = BLYNK_AUTH_TOKEN;

// Waterniveausensor variabelen
uint16_t waterLevelVoltage;
float waterLevel;

// Flowmeter variabelen
volatile unsigned long pulseCount = 0;      // Teller voor flowmeterpulsen (interrupt-driven)
float flowRate = 0.0;
const float calibrationFactor = 0.5;        // 0.5 Hz = 1 L/min → flow = freq / 0.5
unsigned long lastMeasurementTime = 0;      // Tijdstempel laatste meting

// Reconnect logica
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 30000; // 30 seconden wachttijd bij herverbinden

TinyGsm modem(Serial1);  // LTE-modemcommunicatie via hardwarematige Serial1

// Functiedeclaraties
void powerOnLTE();
void reconnectBlynk();
void readWaterLevelSensor();
void calculateFlowRate();
void onFlowPulse();  // Interrupt handler

void setup() {
  SerialUSB.begin(115200);
  while (!SerialUSB);  // Wacht op USB-verbinding voor debugging

  SerialUSB.println("Powering on LTE module...");
  powerOnLTE();  // Zet modem fysiek aan via control pins

  Serial1.begin(115200);  // Start communicatie met het modem
  delay(3000);            // Modem opstarttijd

  SerialUSB.println("Initializing modem...");
  if (!modem.restart()) {
    SerialUSB.println("Failed to restart modem.");
    while (true);  // Stop het programma als modem faalt
  }

  SerialUSB.println("Setting APN...");
  if (!modem.gprsConnect(apn, user, pass)) {
    SerialUSB.println("Failed to connect to GPRS network.");
    while (true);
  }

  SerialUSB.println("Connected to network. Starting Blynk...");
  Blynk.begin(auth, modem, apn, user, pass);  // Start Blynk verbinding

  // Setup voor de flowmeter: digitale input + interrupt
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onFlowPulse, RISING);

  lastMeasurementTime = millis();  // Starttijd voor eerste meting
}

void loop() {
  Blynk.run();  // Houdt Blynk actief

  // Herverbinden met Blynk indien nodig (elke 30 seconden poging)
  if (!Blynk.connected() && millis() - lastReconnectAttempt > reconnectInterval) {
    lastReconnectAttempt = millis();
    reconnectBlynk();
  }

  // Voer elke seconde een meting uit
  if (millis() - lastMeasurementTime >= 1000) {
    lastMeasurementTime = millis();
    readWaterLevelSensor();
    calculateFlowRate();
  }
}

// Fysiek aanzetten van de LTE-module via PWRKEY- en FLIGHT-pinnen
void powerOnLTE() {
  pinMode(LTE_RESET_PIN, OUTPUT);
  digitalWrite(LTE_RESET_PIN, LOW);

  pinMode(LTE_PWRKEY_PIN, OUTPUT);
  digitalWrite(LTE_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(LTE_PWRKEY_PIN, HIGH);
  delay(2000);  // Houd PWRKEY hoog voor minstens 2s volgens SIM7600 datasheet
  digitalWrite(LTE_PWRKEY_PIN, LOW);

  pinMode(LTE_FLIGHT_PIN, OUTPUT);
  digitalWrite(LTE_FLIGHT_PIN, LOW);  // Normale werkmodus
}

// Herinitialisatie van modem en Blynk bij verbindingsverlies
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
    // Interrupt opnieuw activeren na reconnect
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onFlowPulse, RISING);
  }
}

// Leest analoge spanning van GL-136 sensor en stuurt deze als percentage naar Blynk (V0)
void readWaterLevelSensor() {
  waterLevelVoltage = analogRead(GL136_PIN) * 5000.0 / 1024.0;  // Omzetting naar mV
  waterLevel = map(waterLevelVoltage, 0, 5000, 0, 100);         // Omzetten naar percentage
  SerialUSB.print("Water Level: ");
  SerialUSB.print(waterLevel);
  SerialUSB.println("%");
  Blynk.virtualWrite(V0, waterLevel);  // Stuur naar dashboard
}

// Berekent flowrate op basis van pulsen in de laatste seconde (pulsen per seconde / factor)
void calculateFlowRate() {
  noInterrupts();             // Voorkom race condition tijdens kopiëren
  unsigned long pulses = pulseCount;
  pulseCount = 0;
  interrupts();               // Heractiveer interrupt

  float frequency = pulses;  // Pulsen per seconde
  flowRate = frequency / calibrationFactor;  // Flow in L/min

  float minFlowRate = flowRate * 0.97;
  float maxFlowRate = flowRate * 1.03;

  SerialUSB.print("Flow rate: ");
  SerialUSB.print(flowRate);
  SerialUSB.print(" L/min (Measured)");
  SerialUSB.print(" | Min: ");
  SerialUSB.print(minFlowRate);
  SerialUSB.print(" L/min | Max: ");
  SerialUSB.println(maxFlowRate);

  Blynk.virtualWrite(V1, flowRate);  // Verstuur naar Blynk
}

// Interrupt handler: verhoog pulsenteller bij elke opgaande flank
void onFlowPulse() {
  pulseCount++;
}