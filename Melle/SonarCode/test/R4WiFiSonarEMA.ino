#include <WiFiS3.h>                // Bibliotheek voor WiFi-functionaliteit (UNO R4 WiFi)
#include <SoftwareSerial.h>       // Voor het creëren van een tweede seriële poort (UART)
#include <ArduinoHttpClient.h>    // Voor HTTP-verkeer over WiFi (client POST/GET)


// === Netwerkinstellingen ===
const char* ssid = "xxx";                  // WiFi SSID
const char* password = "xxx";              // WiFi wachtwoord
const char* serverAddress = "xxx"; // Server IP-adres voor data-overdracht
const int serverPort = 5000;              // Serverpoort (bijv. Flask server op poort 5000)

WiFiClient wifiClient;
HttpClient client(wifiClient, serverAddress, serverPort);


// === Multiplexer pinnen (CD74HC4067) ===
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define MUX_SIG 10  // Signaalpin van multiplexer (verbonden met sonar TX)

// Instantie van softwarematige seriële poort voor sonarcommunicatie
SoftwareSerial sonarSerial(11, MUX_SIG);  // RX only (TX niet nodig)

// === EMA-buffers voor vier sensoren ===
float ema1 = -1;
float ema2 = -1;
float ema3 = -1;
float ema4 = -1;


// === Setupfase: start seriële communicatie, initialiseer pinnen en verbind met WiFi ===
void setup() {
    Serial.begin(57600);              // Debug communicatie via USB
    sonarSerial.begin(9600);          // Sonar werkt op 9600 baud

    // Multiplexer-selectpinnen als output
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);

    Serial.println("Setup complete!");
    connectToWiFi();                  // Verbind met WiFi
}


// === Maak verbinding met WiFi-netwerk ===
void connectToWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }

    Serial.println("\nConnected to WiFi!");
}


// === Selecteer actief kanaal op multiplexer (0–15) ===
void selectChannel(int channel) {
    digitalWrite(S0, (channel >> 0) & 1);
    digitalWrite(S1, (channel >> 1) & 1);
    digitalWrite(S2, (channel >> 2) & 1);
    digitalWrite(S3, (channel >> 3) & 1);
}


// === Leeg de seriële buffer voordat nieuwe data gelezen wordt ===
void clearSerialBuffer(SoftwareSerial &serialPort) {
    while (serialPort.available()) {
        serialPort.read();
    }
}


// === Lees ruwe afstandsmeting van sonar via UART-protocol ===
float getDistance() {
    unsigned char data[4] = {};
    float distance = -1;

    clearSerialBuffer(sonarSerial);  // Oude data verwijderen
    delay(10);                       // Korte pauze voor nieuwe data

    // Wacht tot een geldig 4-byte sonar frame beschikbaar is
    while (sonarSerial.available() >= 4) {
        for (int i = 0; i < 4; i++) {
            data[i] = sonarSerial.read();
        }

        // Check of het frame begint met 0xFF (header)
        if (data[0] == 0xFF) break;

        clearSerialBuffer(sonarSerial);
        delay(500);  // Extra wachttijd bij foutieve start
    }

    sonarSerial.flush();  // Zorg dat buffer leeg is voor volgende cyclus

    // Controleer of data geldig is via checksom
    if (data[0] == 0xFF) {
        int sum = (data[0] + data[1] + data[2]) & 0x00FF;
        if (sum == data[3]) {
            distance = (data[1] << 8) + data[2];  // Combineer bytes tot afstand
            if (distance > 280) {
                return distance / 10.0;  // Conversie naar centimeter
            }
        }
    }

    return -1;  // Ongeldige meting
}


// === Pas een Exponential Moving Average (EMA) filter toe ===
// Voorkomt ruis en uitschieters in metingen
float emaFilter(int channel, float &previousEma, float alpha = 0.5, int attempts = 3) {
    float reading = -1;

    // Meerdere pogingen voor geldige meting
    for (int i = 0; i < attempts; i++) {
        selectChannel(channel);
        reading = getDistance();
        if (reading > 0) break;  // Stop als geldige meting gevonden
        delay(10);
    }

    // Als geen geldige meting: houd vorige waarde
    if (reading < 0) return previousEma;

    // Eerste keer initialiseren met ruwe waarde
    if (previousEma < 0) previousEma = reading;

    // EMA-berekening: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    previousEma = alpha * reading + (1 - alpha) * previousEma;
    return previousEma;
}


// === Stuur JSON-geformatteerde sensorwaarden via HTTP POST naar de server ===
void sendData(float d1, float d2, float d3, float d4) {
    Serial.println("---- Sending Data to Server ----");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected.");
        return;
    }

    // JSON-string met sensorwaarden
    String message = "{\"sensor_1\": " + String(d1) +
                     ", \"sensor_2\": " + String(d2) +
                     ", \"sensor_3\": " + String(d3) +
                     ", \"sensor_4\": " + String(d4) + "}";
    Serial.print("Payload: ");
    Serial.println(message);

    // HTTP POST-verzoek opbouwen
    client.beginRequest();
    client.post("/data");
    client.sendHeader("Content-Type", "application/json");
    client.sendHeader("Content-Length", message.length());
    client.beginBody();
    client.print(message);
    client.endRequest();

    // Lees en toon serverrespons
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    Serial.print("HTTP Status: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
    Serial.println("--------------------------------");
}


// === Hoofdlus: meet met EMA-filter en stuur data door ===
void loop() {
    Serial.println("Measuring distances...");

    // Meet gefilterde afstanden per sensor (multiplexerkanalen)
    float d1 = emaFilter(1, ema1);
    float d2 = emaFilter(3, ema2);
    float d3 = emaFilter(5, ema3);
    float d4 = emaFilter(7, ema4);

    // Toon de meetwaarden op de seriële monitor
    Serial.print("Sensor 1: "); Serial.println(d1);
    Serial.print("Sensor 2: "); Serial.println(d2);
    Serial.print("Sensor 3: "); Serial.println(d3);
    Serial.print("Sensor 4: "); Serial.println(d4);

    // Verstuur gemeten waarden naar de server
    sendData(d1, d2, d3, d4);

    // Wacht 1 seconde tot volgende meetcyclus
    delay(1000);
}