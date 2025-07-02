#include <WiFiS3.h>                // WiFi-bibliotheek voor Arduino UNO R4 WiFi
#include <SoftwareSerial.h>       // Maakt gebruik van softwarematige seriële poorten
#include <ArduinoHttpClient.h>    // HTTP-client voor communicatie met een server

// === WiFi & server instellingen ===
const char* ssid = "xxx";               // Naam van het WiFi-netwerk
const char* password = "xxx";       // Wachtwoord van het WiFi-netwerk
const char* serverAddress = "xxx"; // IP-adres van de server
const int serverPort = 5000;                  // Poortnummer waarop de server luistert

WiFiClient wifiClient;
HttpClient client(wifiClient, serverAddress, serverPort);

// === Multiplexer: selectie van sonar-kanalen via digitale pinnen ===
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10  // Signaalpin verbonden met de uitgang van de multiplexer

SoftwareSerial sonarSerial(11, MUX_SIG);  // Sonar RX verbonden met MUX, TX ongebruikt

void setup() {
    Serial.begin(57600);               // Start de seriële monitor voor debugging
    sonarSerial.begin(9600);           // Start de seriële communicatie met de sonar sensor
    pinMode(S0, OUTPUT);               // Stel multiplexer-selectie pinnen in als output
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);

    Serial.println("Setup complete!");
    connectToWiFi();                   // Verbind met WiFi-netwerk
}

// === Verbind met WiFi-netwerk ===
void connectToWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nConnected to WiFi!");
}

// === Selecteer een kanaal op de multiplexer (0 t/m 15) ===
void selectChannel(int channel) {
    digitalWrite(S0, (channel >> 0) & 1);
    digitalWrite(S1, (channel >> 1) & 1);
    digitalWrite(S2, (channel >> 2) & 1);
    digitalWrite(S3, (channel >> 3) & 1);
}

// === Lees een afstandsmeting van de sonar ===
float getDistance() {
    unsigned char data[4] = {};   // Buffer voor 4 bytes: header, high byte, low byte, checksum
    float distance = -1;

    clearSerialBuffer(sonarSerial);  // Leeg buffer voorafgaand aan lezing
    delay(100);                      // Wacht even op nieuwe data

    Serial.println(sonarSerial.available());

    while (sonarSerial.available() >= 4) {
        for (int i = 0; i < 4; i++) {
            data[i] = sonarSerial.read();  // Lees 4 bytes
        }
        if (data[0] == 0xFF) break;        // Header gevonden
        clearSerialBuffer(sonarSerial);    // Probeer opnieuw indien ongeldige data
        delay(100);
    }

    sonarSerial.flush();  // Zorg dat buffer leeg is voor volgende meting

    // Validatie en conversie van data naar afstand in cm
    if (data[0] == 0xFF) {
        int sum = (data[0] + data[1] + data[2]) & 0x00FF;
        if (sum == data[3]) {
            distance = (data[1] << 8) + data[2];
            if (distance > 280) {
                return distance / 10.0;  // Converteer naar cm
            }
        }
    }
    return -1;  // Ongeldige meting
}

// === Voer een mediane filter uit voor robuustere metingen ===
float medianFilter(int channel, int numSamples = 3) {
    float values[5];

    for (int i = 0; i < numSamples; i++) {
        selectChannel(channel);           // Selecteer sensor via multiplexer
        values[i] = getDistance();        // Lees waarde
    }

    // Sorteer array om mediaan te bepalen
    for (int i = 0; i < numSamples - 1; i++) {
        for (int j = i + 1; j < numSamples; j++) {
            if (values[j] < values[i]) {
                float temp = values[i];
                values[i] = values[j];
                values[j] = temp;
            }
        }
    }

    return values[numSamples / 2];  // Return mediaanwaarde
}

// === Leeg de seriële buffer om oude of incomplete data te verwijderen ===
void clearSerialBuffer(SoftwareSerial &serialPort) {
    while (serialPort.available()) {
        serialPort.read();
    }
}

// === Stuur sensorwaarden via HTTP POST naar de server ===
void sendData(float distance1, float distance2, float distance3, float distance4) {
    Serial.println("---- Sending Data to Server ----");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot send data.");
        return;
    }

    // JSON-payload maken
    String message = "{\"sensor_1\": " + String(distance1) + ", \"sensor_2\": " + String(distance2) +
                     ", \"sensor_3\": " + String(distance3) + ", \"sensor_4\": " + String(distance4) + "}";
    Serial.print("Payload: ");
    Serial.println(message);

    // Start HTTP-verzoek
    client.beginRequest();
    client.post("/data");

    // Voeg headers toe
    client.sendHeader("Content-Type", "application/json");
    client.sendHeader("Content-Length", message.length());

    // Verzend body
    client.beginBody();
    client.print(message);
    client.endRequest();

    // Ontvang response van de server
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    // Print response en foutanalyse
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

// === Hoofdlus: meet afstanden, print ze en verstuur naar server ===
void loop() {
    Serial.println("Measuring distances...");

    float distance1 = medianFilter(7);  // Sensor 1 op kanaal 7
    float distance2 = medianFilter(3);  // Sensor 2 op kanaal 3
    float distance3 = medianFilter(5);  // Sensor 3 op kanaal 5
    float distance4 = medianFilter(1);  // Sensor 4 op kanaal 1

    Serial.print("Sensor 1: "); Serial.print(distance1); Serial.println(" cm");
    Serial.print("Sensor 2: "); Serial.print(distance2); Serial.println(" cm");
    Serial.print("Sensor 3: "); Serial.print(distance3); Serial.println(" cm");
    Serial.print("Sensor 4: "); Serial.print(distance4); Serial.println(" cm");

    sendData(distance1, distance2, distance3, distance4);  // Stuur metingen naar server

    Serial.println("Waiting for next measurement...");
    delay(10);  // Korte vertraging voor volgende cyclus
}