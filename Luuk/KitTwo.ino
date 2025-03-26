#define BLYNK_TEMPLATE_ID "TMPL4BdiDc-dw"
#define BLYNK_TEMPLATE_NAME "Measuremate2"
#define BLYNK_AUTH_TOKEN "W9BNPKBObbFoSGXc6e9Uf63PczSSQhYA"

#define BLYNK_PRINT Serial
#include <SPI.h>
#include <WiFiS3.h>
#include <BlynkSimpleWifi.h>
#include "DFRobot_ECPRO.h"
#include "DFRobot_PH.h"

#define EC_PIN1 A0       // EC sensor pin 1
#define TE_PIN1 A1       // Temperature pin for EC measurements 1
#define EC_PIN2 A2       // EC sensor pin 2
#define TE_PIN2 A3       // Temperature pin for EC measurements 2
#define PH_PIN1 A4       // pH sensor pin 1
#define PH_PIN2 A5       // pH sensor pin 2

DFRobot_ECPRO ec1, ec2;
DFRobot_ECPRO_PT1000 ecpt1, ecpt2;
DFRobot_PH ph1, ph2;

uint16_t EC_Voltage, TE_Voltage;
float Conductivity, Temp, phValue1, phValue2;

// WiFi credentials.
char ssid[] = "mFi_07C655";
char pass[] = "1234567890";

// Blynk authentication token
char auth[] = "W9BNPKBObbFoSGXc6e9Uf63PczSSQhYA";

void setup() {
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  ph1.begin(); // Start the first pH sensor
  ph2.begin(); // Start the second pH sensor
}

void loop() {
  Blynk.run();
  readEcSensor1();
  readEcSensor2();
  readPhSensor1();
  readPhSensor2();
  delay(10000);  // Delay for 1 second before the next loop iteration
}

void readEcSensor1() {
  EC_Voltage = analogRead(EC_PIN1) * 5000.0 / 1024.0; // Read and convert EC voltage
  TE_Voltage = analogRead(TE_PIN1) * 5000.0 / 1024.0; // Read and convert temperature voltage
  Temp = ecpt1.convVoltagetoTemperature_C(TE_Voltage / 1000.0); // Convert voltage to temperature
  Conductivity = ec1.getEC_us_cm(EC_Voltage, Temp); // Calculate conductivity
  Serial.print("EC Sensor 1 Conductivity: ");
  Serial.println(Conductivity);
  Blynk.virtualWrite(V10, Conductivity); // Send conductivity data to Blynk
}

void readEcSensor2() {
  EC_Voltage = analogRead(EC_PIN2) * 5000.0 / 1024.0; // Read and convert EC voltage
  TE_Voltage = analogRead(TE_PIN2) * 5000.0 / 1024.0; // Read and convert temperature voltage
  Temp = ecpt2.convVoltagetoTemperature_C(TE_Voltage / 1000.0); // Convert voltage to temperature
  Conductivity = ec2.getEC_us_cm(EC_Voltage, Temp); // Calculate conductivity
  Serial.print("EC Sensor 2 Conductivity: ");
  Serial.println(Conductivity);
  Blynk.virtualWrite(V11, Conductivity); // Send conductivity data to Blynk
}

void readPhSensor1() {
  float voltage = analogRead(PH_PIN1) / 1024.0 * 5000; // Convert analog reading to voltage
  phValue1 = ph1.readPH(voltage, Temp); // Calculate pH value
  Serial.print("pH Sensor 1: ");
  Serial.println(phValue1);
  Blynk.virtualWrite(V12, phValue1); // Send pH value to Blynk, assuming V12 is the virtual pin
}

void readPhSensor2() {
  float voltage = analogRead(PH_PIN2) / 1024.0 * 5000; // Convert analog reading to voltage
  phValue2 = ph2.readPH(voltage, Temp); // Calculate pH value
  Serial.print("pH Sensor 2: ");
  Serial.println(phValue2);
  Blynk.virtualWrite(V13, phValue2); // Send pH value to Blynk, assuming V13 is the virtual pin
}
