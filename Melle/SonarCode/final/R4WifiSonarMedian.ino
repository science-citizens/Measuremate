// == Librariën en instellingen ==
#include <SoftwareSerial.h>       // Maakt gebruik van softwarematige seriële poorten
#include <ArduinoHttpClient.h>    // HTTP-client voor communicatie met een server

// === WiFi & server instellingen ===
const char* ssid = "xxx";               // Naam van het WiFi-netwerk
const char* password = "xxx";           // Wachtwoord van het WiFi-netwerk
const char* serverAddress = "xxx";      // IP-adres van de server (niet meer nodig als je de server weghaalt)
const int serverPort = 5000;              // Poortnummer (wordt niet meer gebruikt)

// === Multiplexer & classificatie instellingen ===
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10  // Signaalpin verbonden met multiplex-uitgang

SoftwareSerial sonarSerial(11, MUX_SIG);

// Classificatie-drempels
const float GRADIENT_THRESHOLD = 5;
const float MOVEMENT_THRESHOLD = 4;
const float NEAR_THRESHOLD = 38;
const float EQUAL_MARGIN = 3;
const float SENSOR_POSITIONS[4] = {0, 17, 26, 43};

// Buffer voor vorige meetwaarden (voor bewegingsanalyse)
float previousReadings[4] = {0, 0, 0, 0};
bool hasPrevious = false;

// ================= Functies voor classificatie =================

String analyseGradient(float sensorValues[4]) {
  float gradients[3];
  for (int i = 0; i < 3; i++) {
    gradients[i] = sensorValues[i+1] - sensorValues[i];
  }
  // Recht voor
  bool small = true;
  for (int i = 0; i < 3; i++) {
    if (abs(gradients[i]) >= GRADIENT_THRESHOLD) small = false;
  }
  if (small) return "Object recht voor het systeem";
  // Zijkanten
  if (sensorValues[2] < NEAR_THRESHOLD && sensorValues[3] < NEAR_THRESHOLD)
    return "Object aan rechterkant voor het systeem";
  if (sensorValues[0] < NEAR_THRESHOLD && sensorValues[1] < NEAR_THRESHOLD)
    return "Object aan linkerkant voor het systeem";
  // Diagonaal
  if (gradients[1] < -GRADIENT_THRESHOLD && gradients[2] < -GRADIENT_THRESHOLD)
    return "Object komt schuin van rechts";
  if (gradients[0] > GRADIENT_THRESHOLD && gradients[1] > GRADIENT_THRESHOLD)
    return "Object komt schuin van links";
  if (gradients[0] < -GRADIENT_THRESHOLD && gradients[1] > GRADIENT_THRESHOLD)
    return "Object schuin rechtsvoor";
  if (gradients[0] > GRADIENT_THRESHOLD && gradients[1] < -GRADIENT_THRESHOLD)
    return "Object schuin linksvoor";
  return "Onregelmatig object of meerdere reflecties";
}

String analysePositions(float sensorValues[4]) {
  int nearIndices[4];
  int count = 0;
  for (int i = 0; i < 4; i++) {
    if (sensorValues[i] < NEAR_THRESHOLD) {
      nearIndices[count++] = i;
    }
  }
  float maxVal = sensorValues[0], minVal = sensorValues[0];
  for (int i = 1; i < 4; i++) {
    maxVal = max(maxVal, sensorValues[i]);
    minVal = min(minVal, sensorValues[i]);
  }
  float range = maxVal - minVal;
  // Breedte schatting
  auto geschat = [&](int idxCount) {
    float left = SENSOR_POSITIONS[nearIndices[0]];
    float right = SENSOR_POSITIONS[nearIndices[idxCount - 1]];
    return right - left;
  };
  if (range <= EQUAL_MARGIN) {
    bool allNear = true;
    for (int i = 0; i < 4; i++) if (sensorValues[i] >= NEAR_THRESHOLD) allNear = false;
    if (allNear) return "Alle sensoren volledige dekking (minimale breedte: 43 cm)";
    return "Breed object op afstand";
  }
  if (count == 0) return "Geen object zichtbaar";
  const char* posNames[4] = {"links", "midden-links", "midden-rechts", "rechts"};
  if (count == 1) {
    return String("Object dichtbij bij sensor ") + (nearIndices[0]+1) + " (" + posNames[nearIndices[0]] + ")";
  }
  float b = geschat(count);
  String besch;
  if (count == 2) besch = "Twee sensoren ";
  else if (count == 3) besch = "Drie sensoren ";
  else besch = "Alle sensoren ";
  besch += "- schatting breedte: " + String(b) + " cm";
  return besch;
}

String analyseMovement(float current[4]) {
  if (!hasPrevious) {
    // Nog geen vorige waarden
    for (int i = 0; i < 4; i++) previousReadings[i] = current[i];
    hasPrevious = true;
    return "Onvoldoende gegevens";
  }
  String trends = "";
  bool allCloser = true, allFarther = true;
  int pattern[4];
  for (int i = 0; i < 4; i++) {
    float diff = current[i] - previousReadings[i];
    if (diff > MOVEMENT_THRESHOLD) {
      pattern[i] = 1; allFarther = false;
      trends += "verderaf,";
    } else if (diff < -MOVEMENT_THRESHOLD) {
      pattern[i] = -1; allCloser = false;
      trends += "dichterbij,";
    } else {
      pattern[i] = 0;
      allCloser = false;
      allFarther = false;
      trends += "gelijk,";
    }
    // update for next loop
    previousReadings[i] = current[i];
  }
  trends.remove(trends.length()-1); // laatste komma weg
  if (allCloser) return "Object komt dichterbij";
  if (allFarther) return "Object beweegt weg";
  // Specifieke patronen:
  if (pattern[0]==-1 && pattern[1]==-1 && pattern[2]==1 && pattern[3]==1) return "Object beweegt van rechts naar links";
  if (pattern[0]==1 && pattern[1]==1 && pattern[2]==-1 && pattern[3]==-1) return "Object beweegt van links naar rechts";
  return "Beweging per sensor: " + trends;
}

// === Functie om serial buffer schoon te maken ===
void clearSerialBuffer(SoftwareSerial &serialPort) {
  while (serialPort.available()) serialPort.read();
}

// === Sonar en verstuurfunctie blijven onveranderd ===
float getDistance() {
  unsigned char data[4] = {};
  float distance = -1;
  clearSerialBuffer(sonarSerial);
  delay(100);
  while (sonarSerial.available() >= 4) {
    for (int i = 0; i < 4; i++) data[i] = sonarSerial.read();
    if (data[0] == 0xFF) break;
    clearSerialBuffer(sonarSerial);
    delay(100);
  }
  sonarSerial.flush();
  if (data[0] == 0xFF) {
    int sum = (data[0] + data[1] + data[2]) & 0x00FF;
    if (sum == data[3]) {
      int raw = (data[1] << 8) + data[2];
      if (raw > 280) distance = raw / 10.0;
    }
  }
  return distance;
}

float medianFilter(int channel, int numSamples = 3) {
  float values[5];
  for (int i = 0; i < numSamples; i++) {
    selectChannel(channel);
    values[i] = getDistance();
  }
  // Sorteer om mediaan te vinden
  for (int i = 0; i < numSamples - 1; i++) {
    for (int j = i+1; j < numSamples; j++) {
      if (values[j] < values[i]) {
        float t = values[i]; values[i] = values[j]; values[j] = t;
      }
    }
  }
  return values[numSamples/2];
}

void selectChannel(int channel) {
  digitalWrite(S0, (channel >> 0) & 1);
  digitalWrite(S1, (channel >> 1) & 1);
  digitalWrite(S2, (channel >> 2) & 1);
  digitalWrite(S3, (channel >> 3) & 1);
}

void sendData(float d1, float d2, float d3, float d4) {
  // (optioneel blijven printen of weglaten)
}

// === Setup ===
void setup() {
  Serial.begin(57600);
  sonarSerial.begin(9600);
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  Serial.println("Setup complete!");
}

// === Hoofdlus ===
void loop() {
  // Meet afstanden
  float d1 = medianFilter(7);
  float d2 = medianFilter(3);
  float d3 = medianFilter(5);
  float d4 = medianFilter(1);

  // Print rauwe waarden
  Serial.print("Sensor: "); Serial.print(d1); Serial.print(", ");
  Serial.print(d2); Serial.print(", "); Serial.print(d3); Serial.print(", "); Serial.println(d4);

  // Classificatie
  float readings[4] = {d1, d2, d3, d4};
  String posInfo = analysePositions(readings);
  String gradInfo = analyseGradient(readings);
  String moveInfo = analyseMovement(readings);
  Serial.print("Classificatie: ");
  Serial.print(posInfo); Serial.print(" | ");
  Serial.print(gradInfo); Serial.print(" | ");
  Serial.println(moveInfo);

  // (Optioneel) Versturen naar server
  sendData(d1, d2, d3, d4);

  delay(1000); // Wacht 1 seconde voor volgend cyclus
}