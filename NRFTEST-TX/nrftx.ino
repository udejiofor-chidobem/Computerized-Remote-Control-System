#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(10, 9); // CE, CSN
const byte address[6] = "00001";

void setup()
{
    Serial.begin(115200);
    if (!radio.begin()) {
        Serial.println(F("radio hardware is not responding!!"));
        while (1) {} // hold in infinite loop
    }
    Serial.println("Transmitter Ready!");
    radio.setPALevel(RF24_PA_LOW); // Use LOW to prevent saturation at close range
    radio.openWritingPipe(address);
    radio.setAutoAck(true);        // Enable Auto ACK
    radio.enableDynamicPayloads(); // Required for Auto ACK with dynamic data
    radio.stopListening();         // Set to transmitter mode
}

void loop()
{
    const char text[] = "Hello Nano!";
    bool success = radio.write(&text, sizeof(text));

    if (success)
    {
        Serial.println("Message sent successfully!");
    }
    else
    {
        Serial.println("Message failed to send.");
    }

    delay(1000);
}
