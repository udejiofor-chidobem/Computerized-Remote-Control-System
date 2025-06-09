

void setup()
{
    Serial.begin(38400); // HC-05 default baud rate
    Serial1.begin(9600); // HC-05 default baud rate
    while (!Serial)
        ; // Wait for serial port to be ready (optional)

    Serial.println("Pico ready over Bluetooth");
}

void loop()
{
    if (Serial1.available())
    {
        byte opcode = Serial1.read();

        switch (opcode)
        {
        case 0x01:
            Serial.println("LED ON");
            break;
        case 0x02:
            Serial.println("LED OFF");
            break;
        default:
            Serial.print("Unknown opcode: ");
            Serial.println(opcode);
            break;
        }
    }
}
