#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(10, 9); // CE, CSN
const byte address[6] = "00001";

void setup()
{
    Serial.begin(115200);
    if (!radio.begin()){
        Serial.println(F("radio hardware is not responding!!"));
        while (1) {} // hold in infinite loop
    }
    Serial.println("Transmitter Ready!");
    radio.setPALevel(RF24_PA_LOW);
    radio.openReadingPipe(1, address);
    radio.setAutoAck(true);        // Enable Auto ACK
    radio.enableDynamicPayloads(); // Required for Auto ACK with dynamic data
    radio.startListening();        // Set to receiver mode
}

void loop()
{
    if (radio.available())
    {
        char text[32] = "";
        radio.read(&text, sizeof(text));

        Serial.print("Received: ");
        Serial.println(text);

        // Check if an ACK payload was received
        if (radio.isAckPayloadAvailable())
        {
            Serial.println("ACK Received!");
        }
    }
}
