void setup()
{
    // Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
    Serial.begin(9600);

    // Begin serial communication with Arduino and HC-05
    Serial2.setTX(4); // note cannot swap pins
    Serial2.setRX(5);
    Serial2.begin(38400);

    Serial.println("Initializing...");
    Serial.println("The device started, now you can pair it with bluetooth!");
}

void loop()
{
    if (Serial.available())
    {
        Serial2.write(Serial.read()); // Forward what Serial received to Software Serial Port
    }
    if (Serial2.available())
    {
        Serial.write(Serial2.read()); // Forward what Software Serial received to Serial Port
    }
    delay(20);
}