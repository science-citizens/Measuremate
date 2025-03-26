#define BLYNK_TEMPLATE_ID "TMPL4yewJxn_5"
#define BLYNK_TEMPLATE_NAME "Measuremate1"
#define BLYNK_AUTH_TOKEN "wJUpYCBhgcgkiV32aWt7QCL8E5Zxmo0l"

#include <SPI.h>
#include <WiFiS3.h>
#include <BlynkSimpleWifi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS1 2
#define ONE_WIRE_BUS2 3
#define TdsSensorPin1 4
#define TdsSensorPin2 5
#define VREF 5.0
#define SCOUNT 30

OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
DallasTemperature sensors1(&oneWire1);
DallasTemperature sensors2(&oneWire2);

char ssid[] = "mFi_07C655";
char pass[] = "1234567890";
char auth[] = "wJUpYCBhgcgkiV32aWt7QCL8E5Zxmo0l";

float temperature1 = 0;
float temperature2 = 0;
float tdsValue1 = 0;
float tdsValue2 = 0;

unsigned long lastSendTime = 0;
const long sendInterval = 10000; // 10 seconds

void setup() {
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  sensors1.begin();
  sensors2.begin();
}

void loop() {
  Blynk.run();
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval) {
    updateSensors();
    lastSendTime = currentTime;
  }
}

void updateSensors() {
  sensors1.requestTemperatures();
  temperature1 = sensors1.getTempCByIndex(0);
  sensors2.requestTemperatures();
  temperature2 = sensors2.getTempCByIndex(0);
  
  if (temperature1 != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature 1 is: ");
    Serial.print(temperature1);
    Serial.println("°C");
    Blynk.virtualWrite(V1, temperature1);
  } else {
    Serial.println("Error: Could not read temperature 1 data");
  }

  if (temperature2 != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature 2 is: ");
    Serial.print(temperature2);
    Serial.println("°C");
    Blynk.virtualWrite(V2, temperature2);
  } else {
    Serial.println("Error: Could not read temperature 2 data");
  }

  readTdsSensor(TdsSensorPin1, temperature1, tdsValue1, V3);
  readTdsSensor(TdsSensorPin2, temperature2, tdsValue2, V4);
}

void readTdsSensor(int pin, float temp, float &tdsValue, int vPin) {
  static int analogBuffer[SCOUNT];
  static int analogBufferIndex = 0;
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(pin);
    if (++analogBufferIndex == SCOUNT) analogBufferIndex = 0;
  }
  int analogBufferTemp[SCOUNT];
  for (int i = 0; i < SCOUNT; i++) analogBufferTemp[i] = analogBuffer[i];
  float averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 1024.0;
  float compensationCoefficient = 1.0 + 0.02 * (temp - 25.0);
  float compensationVoltage = averageVoltage / compensationCoefficient;
  tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
  Serial.print("TDS Value from pin "); Serial.print(pin); Serial.print(": "); Serial.print(tdsValue); Serial.println(" ppm");
  Blynk.virtualWrite(vPin, tdsValue);
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    return bTab[(iFilterLen - 1) / 2];
  else
    return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}