#include <WiFiS3.h>
#include <SoftwareSerial.h>
#include <ArduinoHttpClient.h>

// WiFi & server instellingen
const char* ssid = "Paricutin";
const char* password = "3311495676";
const char* serverAddress = "192.168.168.191";
const int serverPort = 5000;

WiFiClient wifiClient;
HttpClient client(wifiClient, serverAddress, serverPort);

// Multiplexer select pins
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10  // RX pin for sonar sensor data

SoftwareSerial sonarSerial(11, MUX_SIG);  // RX only

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

// Functie om Wi-Fi te verbinden
void connectToWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nConnected to WiFi!");
}

// Functie om een kanaal op de multiplexer te selecteren
void selectChannel(int channel) {
    digitalWrite(S0, (channel >> 0) & 1);  
    digitalWrite(S1, (channel >> 1) & 1);
    digitalWrite(S2, (channel >> 2) & 1);
    digitalWrite(S3, (channel >> 3) & 1);
}

// Functie om sonarafstand te lezen
float getDistance() {
    unsigned char data[4] = {};
    float distance = -1;

    clearSerialBuffer(sonarSerial);
    delay(150);

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
// Functie om een mediane filter toe te passen
// Deze functie neemt een aantal metingen en retourneert de mediaan
float medianFilter(int channel, int numSamples = 5) {
    float values[5];

    for (int i = 0; i < numSamples; i++) {
        selectChannel(channel);
        values[i] = getDistance();
        delay(10);
    }

    // Sorteer de array
    for (int i = 0; i < numSamples - 1; i++) {
        for (int j = i + 1; j < numSamples; j++) {
            if (values[j] < values[i]) {
                float temp = values[i];
                values[i] = values[j];
                values[j] = temp;
            }
        }
    }

    // Return het middelste element
    return values[numSamples / 2];
}

// Functie om de seriële buffer te legen
void clearSerialBuffer(SoftwareSerial &serialPort) {
    while (serialPort.available()) {
        serialPort.read();
    }
}

// Functie om data naar de server te sturen
void sendData(float distance1, float distance2) {
    Serial.println("---- Sending Data to Server ----");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot send data.");
        return;
    }

    // Create JSON payload
    String message = "{\"sensor1\": " + String(distance1) + ", \"sensor2\": " + String(distance2) + "}";
    Serial.print("Payload: ");
    Serial.println(message);

    // Start HTTP request
    Serial.println("Starting HTTP request...");
    client.beginRequest();
    client.post("/data");

    // Send headers
    client.sendHeader("Content-Type", "application/json");
    client.sendHeader("Content-Length", message.length());

    // Send body
    client.beginBody();
    client.print(message);
    client.endRequest();

    // Get response status
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    // Print debug info
    Serial.print("HTTP Response Code: ");
    Serial.println(statusCode);

    if (statusCode == -3) {
        Serial.println("Error -3: Connection failed. Possible causes:");
        Serial.println("- Server might be unreachable.");
        Serial.println("- Firewall or network issues.");
        Serial.println("- Wrong server IP or port.");
    } else if (statusCode < 0) {
        Serial.println("HTTP request failed. Possible issues:");
        Serial.println("- WiFi disconnected?");
        Serial.println("- Server not running?");
        Serial.println("- Wrong API endpoint?");
    } else {
        Serial.println("Server response received successfully!");
    }

    Serial.print("Server Response: ");
    Serial.println(response);
    Serial.println("--------------------------------");
}


void loop() {
    Serial.println("Measuring distances...");

    float distance1 = medianFilter(1);
    float distance2 = medianFilter(2);
    float distance3 = medianFilter(3);
    float distance4 = medianFilter(4);

    Serial.print("Sensor 1: ");
    Serial.print(distance1);
    Serial.println(" cm");

    Serial.print("Sensor 2: ");
    Serial.print(distance2);
    Serial.println(" cm");

    Serial.print("Sensor 3: ");
    Serial.print(distance3);
    Serial.println(" cm");

    Serial.print("Sensor 4: ");
    Serial.print(distance4);
    Serial.println(" cm");

    sendData(distance1, distance2);

    Serial.println("Waiting for next measurement...");
    delay(5000);
}
