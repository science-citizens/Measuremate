#include <WiFiS3.h>                // WiFi-bibliotheek voor Arduino UNO R4 WiFi
#include <SoftwareSerial.h>       // Voor UART-communicatie met de sonar via software
#include <ArduinoHttpClient.h>    // Voor het versturen van HTTP POST-verzoeken

// === Netwerkconfiguratie ===
const char* ssid = "xxx";                     // WiFi SSID
const char* password = "xxx";                 // WiFi wachtwoord
const char* serverAddress = "xxx"; // Server IP-adres (pas aan indien nodig)
const int serverPort = 5000;                 // Serverpoort (bijvoorbeeld Flask op 5000)

WiFiClient wifiClient;
HttpClient client(wifiClient, serverAddress, serverPort);

// === Multiplexer pinconfiguratie ===
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10  // Pin waarop het sonar signaal binnenkomt via de multiplexer

SoftwareSerial sonarSerial(11, MUX_SIG);  // Alleen RX; sonar stuurt data naar de Arduino

// === Struct voor alpha-beta filtertoestand per sensor ===
struct AlphaBetaFilter {
  float estimate = -1;  // Huidige geschatte afstand
  float velocity = 0;   // Geschatte snelheidscomponent (verandering per cyclus)
};

// Filters voor vier sonarkanalen
AlphaBetaFilter ab1, ab2, ab3, ab4;

// Filterparameters (tunebaar)
const float alpha = 0.85;  // Gewicht voor positievergelijking (reactiever)
const float beta = 0.05;   // Gewicht voor snelheidscorrectie (trager)

// === Setupfase ===
void setup() {
  Serial.begin(57600);           // Debug via USB
  sonarSerial.begin(9600);       // Sonarcommunicatie via UART

  // Zet multiplexerselectpinnen als OUTPUT
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  Serial.println("Setup complete!");
  connectToWiFi();              // Verbind met WiFi
}

// === WiFi-verbinding maken ===
void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi!");
}

// === Selecteer één van de 16 kanalen op de multiplexer (0–15) ===
void selectChannel(int channel) {
  digitalWrite(S0, (channel >> 0) & 1);
  digitalWrite(S1, (channel >> 1) & 1);
  digitalWrite(S2, (channel >> 2) & 1);
  digitalWrite(S3, (channel >> 3) & 1);
}

// === Wis seriële buffer van sonar, voorkomt dat oude data gelezen wordt ===
void clearSerialBuffer(SoftwareSerial &serialPort) {
  while (serialPort.available()) {
    serialPort.read();
  }
}

// === Lees afstandsmeting van sonar via UART (4-byte protocol) ===
float getDistance() {
  unsigned char data[4] = {};
  float distance = -1;

  clearSerialBuffer(sonarSerial);
  delay(10);  // Korte wachttijd voor nieuwe data

  // Lees 4 bytes van sonar (protocol begint met 0xFF)
  while (sonarSerial.available() >= 4) {
    for (int i = 0; i < 4; i++) {
      data[i] = sonarSerial.read();
    }
    if (data[0] == 0xFF) break;  // Correcte startbyte gevonden
    clearSerialBuffer(sonarSerial);
    delay(500);  // Wacht en probeer opnieuw
  }

  sonarSerial.flush();

  // Valideer frame met checksum
  if (data[0] == 0xFF) {
    int sum = (data[0] + data[1] + data[2]) & 0x00FF;
    if (sum == data[3]) {
      distance = (data[1] << 8) + data[2];  // Combineer data bytes tot afstand
      if (distance > 280) {
        return distance / 10.0;  // Omzetten naar centimeters
      }
    }
  }

  return -1;  // Ongeldige meting
}

// === Update alpha-beta filter met nieuwe meting ===
float alphaBetaUpdate(AlphaBetaFilter &filter, int channel) {
  selectChannel(channel);
  float measurement = getDistance();
  if (measurement < 0) return filter.estimate;  // Gebruik vorige waarde als meting ongeldig is

  if (filter.estimate < 0) filter.estimate = measurement;  // Initialisatie eerste keer

  // Voorspel positie: vorige positie + snelheid
  float prediction = filter.estimate + filter.velocity;
  float residual = measurement - prediction;  // Verschil tussen verwachting en meting

  // Werk positie en snelheid bij met alpha en beta correctiefactoren
  filter.estimate = prediction + alpha * residual;
  filter.velocity = filter.velocity + beta * residual;

  return filter.estimate;
}

// === Verzend sensorwaarden als JSON naar de centrale server ===
void sendData(float d1, float d2, float d3, float d4) {
  Serial.println("---- Sending Data to Server ----");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    return;
  }

  // JSON-payload samenstellen
  String message = "{\"sensor_1\": " + String(d1) + ", \"sensor_2\": " + String(d2) +
                   ", \"sensor_3\": " + String(d3) + ", \"sensor_4\": " + String(d4) + "}";

  Serial.print("Payload: ");
  Serial.println(message);

  // HTTP POST uitvoeren
  client.beginRequest();
  client.post("/data");
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", message.length());
  client.beginBody();
  client.print(message);
  client.endRequest();

  // Verwerk respons van server
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("HTTP Status: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  Serial.println("--------------------------------");
}

// === Hoofdlus: meet en verzend elke seconde sensorwaarden ===
void loop() {
  Serial.println("Measuring distances...");

  float d1 = alphaBetaUpdate(ab1, 1);
  float d2 = alphaBetaUpdate(ab2, 3);
  float d3 = alphaBetaUpdate(ab3, 5);
  float d4 = alphaBetaUpdate(ab4, 7);

  Serial.print("Sensor 1: "); Serial.println(d1);
  Serial.print("Sensor 2: "); Serial.println(d2);
  Serial.print("Sensor 3: "); Serial.println(d3);
  Serial.print("Sensor 4: "); Serial.println(d4);

  sendData(d1, d2, d3, d4);

  delay(1000);  // Wacht 1 seconde tot volgende meetcyclus
}