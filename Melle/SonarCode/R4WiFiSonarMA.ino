#include <WiFiS3.h>
#include <SoftwareSerial.h>
#include <ArduinoHttpClient.h>

// WiFi & server instellingen
const char* ssid = "KidsBoven";
const char* password = "011167Jh1234!";
const char* serverAddress = "192.168.68.128";
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
    delay(100);
    
    Serial.println(sonarSerial.available());

    while (sonarSerial.available() >= 4) {
        for (int i = 0; i < 4; i++) {
            data[i] = sonarSerial.read();
        }
        if (data[0] == 0xFF) break;  
        clearSerialBuffer(sonarSerial);
        delay(100);
    }

    sonarSerial.flush();

    // Serial.println(data[0]);
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

// Functie om een moving average filter toe te passen
float movingAverageFilter(int channel, int numSamples = 2) {
    float sum = 0;
    int validSamples = 0;

    for (int i = 0; i < numSamples; i++) {
        selectChannel(channel);
        float reading = getDistance();
        Serial.println(reading);

        if (reading > 0) { // alleen geldige metingen meenemen
            sum += reading;
            validSamples++;
        }

        delay(10);
    }

    if (validSamples == 0) return -1; // geen geldige metingen
    return sum / validSamples;
}

// Functie om de seriële buffer te legen
void clearSerialBuffer(SoftwareSerial &serialPort) {
    while (serialPort.available()) {
        serialPort.read();
    }
}

// Functie om data naar de server te sturen
void sendData(float distance1, float distance2, float distance3, float distance4) {
    Serial.println("---- Sending Data to Server ----");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot send data.");
        return;
    }

    // Create JSON payload
    String message = "{\"sensor_1\": " + String(distance1) + ", \"sensor_2\": " + String(distance2) + ", \"sensor_3\": " + String(distance3) + ", \"sensor_4\": " + String(distance4) + "}";
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

    float distance1 = movingAverageFilter(7);
    float distance2 = movingAverageFilter(5);
    float distance3 = movingAverageFilter(3);
    float distance4 = movingAverageFilter(1);

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

    sendData(distance1, distance2, distance3, distance4);

    Serial.println("Waiting for next measurement...");
    delay(10);
}
