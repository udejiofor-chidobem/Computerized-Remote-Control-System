#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN 10
#define CSN_PIN 9

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

#define MESSAGE_SIZE 4

void setup()
{
    Serial.begin(115200);
    if (!radio.begin())
    {
        Serial.println(F("radio hardware is not responding!!"));
        while (1)
        {
        } // hold in infinite loop
    }
    Serial.println("Transmitter Ready!");
    radio.setPALevel(RF24_PA_HIGH); // Use LOW to prevent saturation at close range
    radio.openWritingPipe(address);
    radio.setAutoAck(true);        // Enable Auto ACK
    radio.enableDynamicPayloads(); // Required for Auto ACK with dynamic data
    radio.setRetries(15, 10);
    radio.stopListening(); // Set to transmitter mode
}

void loop()
{
    if (Serial.available() >= MESSAGE_SIZE)
    {
        byte buffer[4]; // Ensure buffer is large enough to hold 4 bytes

        for (int i = 0; i < MESSAGE_SIZE; i++)
        {
            buffer[i] = Serial.read();  // Read 1 byte at a time
        }

        // Print the received bytes for debugging
        Serial.print("Received Bytes: ");
        for (int i = 0; i < MESSAGE_SIZE; i++)
        {
            Serial.print(buffer[i], HEX);   // Print in HEX for clarity
            Serial.print(" ");
        }
        Serial.println();

        radio.stopListening();
        bool success = radio.write(buffer, MESSAGE_SIZE); // Send to receiver
        radio.startListening();

        if (success)
        {
            Serial.println("Message sent successfully!");
        }
        else
        {
            Serial.println("Message failed to send.");
        }

        while (Serial.available() > 0)
        {
            Serial.read(); // Read and discard a byte
        }
    }

    if (radio.available())
    {
        char receivedMessage[32] = "";
        radio.read(&receivedMessage, sizeof(receivedMessage));
        Serial.print("Received: ");
        Serial.println(receivedMessage);
    }
}
