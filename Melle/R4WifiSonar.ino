// Arduino code that sends sonar data from multiple sensors through a multiplexer to a server.

#include <SoftwareSerial.h>

// Multiplexer select pins
#define S0 4
#define S1 5
#define S2 6
#define S3 7

#define MUX_SIG 10  // RX pin for sonar sensor data
#define MUX_SIG2 11  // TX pin for sonar sensor data

SoftwareSerial sonarSerial(MUX_SIG2, MUX_SIG);

void setup() {
    Serial.begin(57600);
    sonarSerial.begin(9600);

    // Set multiplexer control pins as output
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);

    Serial.println("Setup complete!");
}

// Function to select a channel on the multiplexer
void selectChannel(int channel) {
    Serial.print("Selecting channel: ");
    Serial.println(channel);

    // Set the multiplexer control pins based on the channel number
    digitalWrite(S0, (channel >> 0) & 1);  
    digitalWrite(S1, (channel >> 1) & 1);
    digitalWrite(S2, (channel >> 2) & 1);
    digitalWrite(S3, (channel >> 3) & 1);

    Serial.print("Pin states - S0: ");
    Serial.print((channel >> 0) & 1);
    Serial.print(", S1: ");
    Serial.print((channel >> 1) & 1);
    Serial.print(", S2: ");
    Serial.print((channel >> 2) & 1);
    Serial.print(", S3: ");
    Serial.println((channel >> 3) & 1);
}

// Function to read sonar sensor distance
float getDistance() {
    unsigned char data[4] = {0x00, 0x00, 0x00, 0x00};
    float distance = -1;

    if (sonarSerial.available() != 0) {
      Serial.println("Reading data from sonar...");
    } else {
      Serial.println("No serial data available");
    }

    // Read 4 bytes from sonar only if data is available
    while (sonarSerial.available() >= 4) {
      for (int i = 0; i < 4; i++) {
        data[i] = sonarSerial.read();
      }

      // Check if the first byte is 0xFF, indicating a valid frame
      if (data[0] == 0xFF) {
        break;  // Exit the loop if valid data is found
      } else {
        // If invalid frame, discard the data and try again
        clearSerialBuffer(sonarSerial);
        delay(500);
      }
    }

    sonarSerial.flush();

    Serial.print("Raw data: ");
    for (int i = 0; i < 4; i++) {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (data[0] == 0xff) {  // Check if the first byte is 0xFF
        int sum = (data[0] + data[1] + data[2]) & 0x00FF;
        Serial.print("Checksum sum: ");
        Serial.println(sum);

        if (sum == data[3]) {  // Checksum validation
            distance = (data[1] << 8) + data[2];
            Serial.print("Distance (raw): ");
            Serial.println(distance);

            if (distance > 280) { 
                distance = distance / 10;
                Serial.print("Distance: ");
                Serial.print(distance);
                Serial.println(" cm");
            } else {
                Serial.println("Below the lower limit (distance < 280). Setting to -1.");
                distance = -1;
            }
        } else {
            Serial.println("Checksum ERROR");
        }
    } else {
        Serial.println("No valid frame detected");
    }

    return distance;
}

// Function to clear the serial buffer
void clearSerialBuffer(SoftwareSerial &serialPort) {
    Serial.println("Clearing serial buffer...");
    while (serialPort.available()) {
        serialPort.read();  // Read and discard any data
    }
}

void loop() {
    Serial.println("Starting loop...");

    for (int i = 1; i < 4; i++) {  // Iterate through all 16 channels (0-15)
        Serial.print("Switching to sensor ");
        Serial.println(i);

        selectChannel(i);
        delay(500);

        float distance = getDistance();
        clearSerialBuffer(sonarSerial);

        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(": ");

        if (distance != -1) {
            Serial.print(distance);
            Serial.println(" cm");
        } else {
            Serial.println("No valid data");
        }
        delay(3000);
    }
    Serial.println("Loop completed. Restarting...");
}