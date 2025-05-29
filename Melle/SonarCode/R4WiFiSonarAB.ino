#include <WiFiS3.h>
#include <SoftwareSerial.h>
#include <ArduinoHttpClient.h>

// WiFi-instellingen
const char* ssid = "ID";
const char* password = "WW";
const char* serverAddress = "192.168.0.100"; // <-- Pas aan
const int serverPort = 5000;

WiFiClient wifiClient;
HttpClient client(wifiClient, serverAddress, serverPort);

// Multiplexer pins
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10

SoftwareSerial sonarSerial(11, MUX_SIG);  // RX only

// Struct om alpha-beta toestand per sensor bij te houden
struct AlphaBetaFilter {
  float estimate = -1;
  float velocity = 0;
};

AlphaBetaFilter ab1, ab2, ab3, ab4;

const float alpha = 0.85;  // Positie aanpassing
const float beta = 0.05;   // Snelheid aanpassing

void setup() {
  Serial.begin(57600);
  sonarSerial.begin(9600);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  Serial.println("Setup complete!");
  connectToWiFi();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi!");
}

void selectChannel(int channel) {
  digitalWrite(S0, (channel >> 0) & 1);
  digitalWrite(S1, (channel >> 1) & 1);
  digitalWrite(S2, (channel >> 2) & 1);
  digitalWrite(S3, (channel >> 3) & 1);
}

void clearSerialBuffer(SoftwareSerial &serialPort) {
  while (serialPort.available()) {
    serialPort.read();
  }
}

float getDistance() {
  unsigned char data[4] = {};
  float distance = -1;

  clearSerialBuffer(sonarSerial);
  delay(10);

  while (sonarSerial.available() >= 4) {
    for (int i = 0; i < 4; i++) {
      data[i] = sonarSerial.read();
    }
    if (data[0] == 0xFF) break;
    clearSerialBuffer(sonarSerial);
    delay(500);
  }

  sonarSerial.flush();

  if (data[0] == 0xFF) {
    int sum = (data[0] + data[1] + data[2]) & 0x00FF;
    if (sum == data[3]) {
      distance = (data[1] << 8) + data[2];
      if (distance > 280) {
        return distance / 10.0;
      }
    }
  }
  return -1;
}

// Alpha-beta filter update
float alphaBetaUpdate(AlphaBetaFilter &filter, int channel) {
  selectChannel(channel);
  float measurement = getDistance();
  if (measurement < 0) return filter.estimate;

  if (filter.estimate < 0) filter.estimate = measurement;

  float prediction = filter.estimate + filter.velocity;
  float residual = measurement - prediction;

  filter.estimate = prediction + alpha * residual;
  filter.velocity = filter.velocity + beta * residual;

  return filter.estimate;
}

void sendData(float d1, float d2, float d3, float d4) {
  Serial.println("---- Sending Data to Server ----");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    return;
  }

  String message = "{\"sensor_1\": " + String(d1) + ", \"sensor_2\": " + String(d2) +
                   ", \"sensor_3\": " + String(d3) + ", \"sensor_4\": " + String(d4) + "}";

  Serial.print("Payload: ");
  Serial.println(message);

  client.beginRequest();
  client.post("/data");
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", message.length());
  client.beginBody();
  client.print(message);
  client.endRequest();

  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("HTTP Status: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  Serial.println("--------------------------------");
}

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

  delay(1000);
}
