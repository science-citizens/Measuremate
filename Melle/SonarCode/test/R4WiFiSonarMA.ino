#include <WiFiS3.h>                // Bibliotheek voor WiFi-functionaliteit op Arduino UNO R4 WiFi
#include <SoftwareSerial.h>       // Maakt extra seriële communicatie mogelijk via software
#include <ArduinoHttpClient.h>    // HTTP-client om data naar een server te sturen

// === WiFi en serverinstellingen ===
const char* ssid = "xxx";               // Naam van het WiFi-netwerk
const char* password = "xxx";       // Wachtwoord van het WiFi-netwerk
const char* serverAddress = "xxx"; // IP-adres van de server
const int serverPort = 5000;                  // Poort waarop de server luistert

WiFiClient wifiClient;
HttpClient client(wifiClient, serverAddress, serverPort);

// === Multiplexer pinconfiguratie (voor sonar selectie) ===
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10  // Signaalpin van de multiplexer (wordt RX op SoftwareSerial)

SoftwareSerial sonarSerial(11, MUX_SIG);  // Seriële communicatie met sonar (TX niet gebruikt)

// === Setup: initialisatie van communicatie en pinnen ===
void setup() {
    Serial.begin(57600);          // Debug-uitvoer via de seriële monitor
    sonarSerial.begin(9600);      // Seriële verbinding met sonar op 9600 baud
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);

    Serial.println("Setup complete!");
    connectToWiFi();              // Maak verbinding met het WiFi-netwerk
}

// === Verbindt met het WiFi-netwerk en wacht tot verbinding tot stand is gebracht ===
void connectToWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nConnected to WiFi!");
}

// === Selecteer het gewenste kanaal op de multiplexer (0-15) ===
void selectChannel(int channel) {
    digitalWrite(S0, (channel >> 0) & 1);
    digitalWrite(S1, (channel >> 1) & 1);
    digitalWrite(S2, (channel >> 2) & 1);
    digitalWrite(S3, (channel >> 3) & 1);
}

// === Leest een sonarafstand in cm via het sonarprotocol ===
float getDistance() {
    unsigned char data[4] = {};
    float distance = -1;

    clearSerialBuffer(sonarSerial);  // Leeg buffer voorafgaand aan lezing
    delay(100);                      // Wacht kort op nieuwe data

    Serial.println(sonarSerial.available());  // Print beschikbaar aantal bytes

    // Probeer tot een geldig sonarframe is ontvangen
    while (sonarSerial.available() >= 4) {
        for (int i = 0; i < 4; i++) {
            data[i] = sonarSerial.read();     // Lees 4 bytes van sonar
        }
        if (data[0] == 0xFF) break;           // Header gevonden, stop zoeken
        clearSerialBuffer(sonarSerial);       // Anders: reset buffer
        delay(100);
    }

    sonarSerial.flush();  // Zorg dat buffer volledig leeg is

    // Verwerk geldig sonar-frame
    if (data[0] == 0xFF) {
        int sum = (data[0] + data[1] + data[2]) & 0x00FF;
        if (sum == data[3]) {
            distance = (data[1] << 8) + data[2];  // Combineer high + low byte
            if (distance > 280) {
                return distance / 10.0;          // Omzetten naar cm
            }
        }
    }
    return -1;  // Geen geldige meting ontvangen
}

// === Past een moving average filter toe op meerdere metingen ===
float movingAverageFilter(int channel, int numSamples = 2) {
    float sum = 0;
    int validSamples = 0;

    for (int i = 0; i < numSamples; i++) {
        selectChannel(channel);              // Selecteer kanaal via multiplexer
        float reading = getDistance();       // Lees afstand
        Serial.println(reading);             // Toon ruwe waarde

        if (reading > 0) {                   // Alleen geldige metingen meenemen
            sum += reading;
            validSamples++;
        }
        delay(10);  // Kleine pauze tussen metingen
    }

    if (validSamples == 0) return -1;        // Geen geldige metingen beschikbaar
    return sum / validSamples;              // Gemiddelde berekenen
}

// === Leeg de seriële buffer van sonarcommunicatie ===
void clearSerialBuffer(SoftwareSerial &serialPort) {
    while (serialPort.available()) {
        serialPort.read();  // Verwijder oude of incomplete data
    }
}

// === Verstuurt JSON-data met afstandsmetingen naar een HTTP-server ===
void sendData(float distance1, float distance2, float distance3, float distance4) {
    Serial.println("---- Sending Data to Server ----");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot send data.");
        return;
    }

    // JSON-payload samenstellen
    String message = "{\"sensor_1\": " + String(distance1) +
                     ", \"sensor_2\": " + String(distance2) +
                     ", \"sensor_3\": " + String(distance3) +
                     ", \"sensor_4\": " + String(distance4) + "}";
    Serial.print("Payload: ");
    Serial.println(message);

    // Start HTTP POST-verzoek
    client.beginRequest();
    client.post("/data");

    client.sendHeader("Content-Type", "application/json");
    client.sendHeader("Content-Length", message.length());

    client.beginBody();
    client.print(message);
    client.endRequest();

    // Response van server uitlezen
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    Serial.print("HTTP Response Code: ");
    Serial.println(statusCode);

    // Foutanalyse op basis van statuscode
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

// === Hoofdlus: meet, print en verstuur sonarwaarden ===
void loop() {
    Serial.println("Measuring distances...");

    // Voer gefilterde metingen uit voor elk kanaal
    float distance1 = movingAverageFilter(7);  // Sensor op kanaal 7
    float distance2 = movingAverageFilter(5);  // Sensor op kanaal 5
    float distance3 = movingAverageFilter(3);  // Sensor op kanaal 3
    float distance4 = movingAverageFilter(1);  // Sensor op kanaal 1

    // Print resultaten naar seriële monitor
    Serial.print("Sensor 1: "); Serial.print(distance1); Serial.println(" cm");
    Serial.print("Sensor 2: "); Serial.print(distance2); Serial.println(" cm");
    Serial.print("Sensor 3: "); Serial.print(distance3); Serial.println(" cm");
    Serial.print("Sensor 4: "); Serial.print(distance4); Serial.println(" cm");

    // Verzend waarden naar server
    sendData(distance1, distance2, distance3, distance4);

    Serial.println("Waiting for next measurement...");
    delay(10);  // Pauze tussen metingen (kan verhoogd worden voor stabiliteit)
}