#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
// #include <SD.h>
#include <SdFat.h> // SdFat by Bill Greiman

#define MESSAGE_SIZE 4

#define SD_MISO 6
#define SD_MOSI 7
#define SD_SCK 8
#define SD_CS 4

// Declare a software SPI interface
SoftSpiDriver<SD_MISO, SD_MOSI, SD_SCK> softSPI;
SdFat sd;

// Config with CS pin and software SPI
SdSpiConfig sdConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &softSPI);

// Sd2Card card;
// const int chipSelect = 4;

File currentLogFile;
// uint64_t wallTimeStart = 0;
bool loggingEnabled = false;
unsigned long lastLogTime = 0; // millis() timestamp
char logFilename[21] = "default.csv";

struct WallTime
{
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
};

WallTime currentWallTime;
unsigned long logStartMillis;
unsigned long now;

// #define BARO
#ifdef BARO
#include <TE_SM9000.h>

    // Example settings for SM4291-BCE-S-005-000
    SM9000_sensor T4291(5, 0, -26215, 26214);
#endif

#define CE_PIN 10
#define CSN_PIN 9
#define PWM_PIN 3

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// PAYLOAD FORMAT
// Byte 1
// {0x0[OP_CODE]}
// OP_CODES: 0x01|HRT, 0x02|PWM, 0x03|RAMP, 0x04|STP, 0x05|IMP, 0x06|SToP

#define HRT 0x01
#define PWMc 0x02
#define RMP 0x03
#define STP 0x04
#define IMP 0x05
#define STOP 0x06
#define GOOD 0x07
#define LOG 0x08
byte command_id = 0x0;

unsigned long lastHeartbeatTime = 0;        // Variable to store last heartbeat time
const unsigned long heartbeatTimeout = 15*1000; // Timeout period (5 seconds)
bool eStopTriggered = false;                 // Flag to track emergency stop status

int pwmValue = 0;                            // Current PWM value (0-255)

int pwmStartValue = 0;
unsigned long pwmStartTime = 0;              // Time when ramp started
unsigned long rampDuration = 0;              // Duration for ramping
int targetPWM = 0;                           // Target PWM value
bool isRamping = false;                      // Flag to track if ramping is active

int stepStartPWM = 0;                        // Starting PWM value for step command
int stepStepPWM = 0;                        // Step increment value (default 10)
int stepStopPWM = 0;                       // Stop PWM value for step command
int stepTime = 5;
bool stepInProgress = false;                 // Flag for step command progress

void setup()
{

#ifdef BARO
    Wire.begin();
    T4291.reset();
#endif

    Serial.begin(115200);
    if (!radio.begin())
    {
        Serial.println(F("radio hardware is not responding!!"));
        while (1)
        {
        } // hold in infinite loop
    }
    Serial.println("Transmitter Ready!");
    radio.setPALevel(RF24_PA_HIGH);
    radio.openReadingPipe(1, address);
    radio.setAutoAck(true);        // Enable Auto ACK
    radio.enableDynamicPayloads(); // Required for Auto ACK with dynamic data
    radio.setRetries(15, 10);
    radio.startListening();        // Set to receiver mode


    Serial.print("Initializing SD card...");
    if (!sd.begin(sdConfig))
    {
        Serial.println("SD initialization failed!");
        return;
    }
    Serial.println("SD initialization done.");

    pinMode(PWM_PIN, OUTPUT);
    analogWrite(PWM_PIN, 0);
}

void loop()
{
    // Check if the heartbeat timeout has been exceeded
    if (millis() - lastHeartbeatTime > heartbeatTimeout && !eStopTriggered)
    {
        Serial.println("Heartbeat timeout! Triggering emergency stop.");
        triggerEStop();
        eStopTriggered = true; // Set flag to prevent repeated e-stop
    }

    if (radio.available())
    {
        byte buffer[MESSAGE_SIZE]; // Max expected payload size
        radio.read(&buffer, sizeof(buffer));
        command_id = buffer[0];
        Serial.print("OP Code: ");
        Serial.println(command_id);

        Serial.print("Buffer: ");
        for (int i = 0; i < MESSAGE_SIZE; i++)
        {
            Serial.print(buffer[i], HEX); // Print in Hex
            Serial.print(" (");
            Serial.print(buffer[i], DEC); // Print in Decimal
            Serial.print(") ");
        }
        Serial.println();

        Serial.println("Received A Message");

        processCommand(buffer);
    }
    updatePWM();

    if (loggingEnabled && millis() - now >= 100)
    { // 10 Hz logging
#ifdef BARO
        T4291.readData();
#endif
        now = millis();
        unsigned long elapsed = now - logStartMillis;
        if (currentLogFile) {
            currentLogFile.print(elapsed);
            currentLogFile.print(",");
            currentLogFile.println(pwmValue); // Replace with your real value
#ifdef BARO
            Serial.print("HERE 2");
            currentLogFile.print(',');
            currentLogFile.print(T4291.getPressure());
            currentLogFile.print(',');
            currentLogFile.print(T4291.getTemp());
#endif
            currentLogFile.flush();  // Ensures data is saved every time
        }
    }
}

void processCommand(byte* cmd)
{
    if (eStopTriggered == false) {
        if (command_id == STOP)
        {
            triggerEStop();
            eStopTriggered = true; // Set flag to prevent repeated e-stop
        }
        else if (command_id == HRT)
        {
            Serial.println("Heartbeat received!");
            lastHeartbeatTime = millis(); // Reset the heartbeat timeout
            eStopTriggered = false;       // Reset e-stop
            // Optional: Respond with a confirmation message
            report("ACK");
        }
        else if (command_id == GOOD)
        {
            lastHeartbeatTime = millis(); // Reset the heartbeat timeout
            eStopTriggered = false;       // Reset e-stop
            // Optional: Respond with a confirmation message
            report("ACK");
            if (stepInProgress)
            {
                stepNext(); // Move to the next step
            }
        }
        else if (command_id == LOG)
        {
            loggingEnabled = true;
            setWalltime(cmd[1], cmd[2], cmd[3]);
        }
        else if (!isRamping && !stepInProgress) {
            if (command_id == PWMc)
            {
                int pwmValue = (int) cmd[1];
                Serial.print("Setting PWM to: ");
                Serial.println(pwmValue);
                analogWrite(PWM_PIN, map(pwmValue, 0, 100, 0, 255));
            }
            else if (command_id == RMP)
            {
                // Parse ramp parameters from the message (e.g., RMP:0:255:10 for 0 to 255 over 10 seconds)
                int startPWM = (int) cmd[1];
                int stopPWM = (int) cmd[2];
                int duration = (int) cmd[3];
                rampPWM(startPWM, stopPWM, duration);                           // Start ramp
            }
            else if (command_id == STP)
            {
                // Parse ramp parameters from the message (e.g., RMP:0:255:10 for 0 to 255 over 10 seconds)
                int startPWM = (int) cmd[1];
                int stepPWM = (int) cmd[2];
                int stopPWM = (int) cmd[3];
                startStepCommand(startPWM, stepPWM, stopPWM);                  // Start ramp
            }
            else if (command_id == IMP)
            {
                // Parse impulse parameters (e.g., IMPULSE 100 5 for 100% PWM for 5 seconds)
                int pwmValue = (int)cmd[1];
                impulsePWM(pwmValue, 1); // Trigger impulse
            }
            Serial.println("Heartbeat received!");
            lastHeartbeatTime = millis(); // Reset the heartbeat timeout
            eStopTriggered = false;       // Reset e-stop
            // Optional: Respond with a confirmation message
            report("ACK");
        }
    }
    else if (command_id == GOOD)
    {
        lastHeartbeatTime = millis(); // Reset the heartbeat timeout
        eStopTriggered = false;       // Reset e-stop
        report("ESTOP OFF");
    } else {
        report("ESTOP");
    }
}

void triggerEStop()
{
    report("Emergency Stop Activated!");
    if (eStopTriggered == false)
    {
        int pwmValue = 0; // Current PWM value (0-255)

        int pwmStartValue = 0;
        unsigned long pwmStartTime = 0; // Time when ramp started
        unsigned long rampDuration = 0; // Duration for ramping
        int targetPWM = 0;              // Target PWM value
        bool isRamping = false;         // Flag to track if ramping is active

        int stepStartPWM = 0; // Starting PWM value for step command
        int stepStepPWM = 0;  // Step increment value (default 10)
        int stepStopPWM = 0;  // Stop PWM value for step command
        int stepTime = 5;
        bool stepInProgress = false; // Flag for step command progress
        rampPWM(pwmValue, 0, 3); // Ramp down to 0% PWM in 3 seconds
        stopLogging();
    }
}

// Start step command with initial values
void startStepCommand(int startPWM, int stepPWM, int stopPWM)
{
    if (!stepInProgress) {
        stepStartPWM = startPWM;
        stepStepPWM = stepPWM;
        stepStopPWM = stopPWM;
        targetPWM = startPWM + stepPWM; // Initialize with starting PWM value
        stepInProgress = true; // Mark step command as in progress
        report("STEP Command Started | " + String(startPWM) + ", " + String(stepPWM) + ", " + String(stopPWM));
        if (pwmValue != startPWM)
        {
            rampPWM(pwmValue, startPWM, stepTime); // Ramp over 5 seconds
        }
    }
}

// Move to the next step in the sequence
void stepNext()
{
    if (!isRamping) {
        targetPWM = pwmValue + stepStepPWM; // Increment the PWM by step value
        if (targetPWM >= stepStopPWM)
        {
            targetPWM = stepStopPWM; // Ensure we do not exceed the stop value
            stepInProgress = false;
            report("Step Completed");
        }
        report("Next step PWM: ");
        Serial.println(targetPWM);
        rampPWM(pwmValue, targetPWM, stepTime); // Ramp over 5 seconds
    }
}

// Ramp PWM from current value to target value over a specified duration
void rampPWM(int startPWM, int stopPWM, int duration)
{
    
    targetPWM = stopPWM;
    rampDuration = duration * 1000; // Convert seconds to milliseconds
    pwmStartTime = millis();        // Store the start time of the ramp
    isRamping = true;
    report("RAMP Command Started | " + String(startPWM) + ", " + String(stopPWM) + ", " + String(duration));
    if (pwmValue != startPWM) {
        rampPWM(pwmValue, startPWM, stepTime); // Ramp over 5 seconds
    }
}

// Impulse behavior: Change PWM suddenly for a set duration
void impulsePWM(int impulsePWM, int duration)
{
    pwmValue = impulsePWM;
    analogWrite(PWM_PIN, map(pwmValue, 0, 100, 0, 255)); // Set PWM to impulse value
    report("Impulse started!");
    delay(duration * 1000);        // Wait for the duration of the impulse
    pwmValue = 0;                  // Set PWM back to 0 after impulse
    analogWrite(PWM_PIN, map(pwmValue, 0, 100, 0, 255)); // Stop PWM after impulse
    report("Impulse finished!");
}

// Update PWM based on ramping behavior
void updatePWM()
{
    if (isRamping)
    {
        unsigned long elapsed = millis() - pwmStartTime;
        if (elapsed < rampDuration)
        {
            // Calculate the current PWM value based on elapsed time
            float progress = (float)elapsed / rampDuration;
            pwmValue = pwmStartValue + (int)((targetPWM - pwmStartValue) * progress);
            analogWrite(PWM_PIN, map(pwmValue, 0, 100, 0, 255)); // Update PWM output
        }
        else
        {
            pwmValue = targetPWM;          // Ensure we reach the target PWM value at the end of the ramp
            analogWrite(PWM_PIN, map(pwmValue, 0, 100, 0, 255)); // Set PWM to the target value
            isRamping = false;             // Stop ramping
            report("Ramp Completed");
        }
    }
}

void report(String data) {
    Serial.println(data);
    radio.stopListening(); // Stop listening to transmit
    radio.write(data.c_str(), sizeof(data));
    radio.startListening(); // Resume listening
}

void setWalltime(uint8_t d, uint8_t h, uint8_t m)
{
    currentWallTime.day = d;
    currentWallTime.hour = h;
    currentWallTime.minute = m;

    Serial.print("Wall time set: ");
    Serial.print(d);
    Serial.print(" ");
    Serial.print(h);
    Serial.print(":");
    Serial.println(m);

    if (loggingEnabled && currentLogFile)
    {
        currentLogFile.close();
    }

    loggingEnabled = true;
    logStartMillis = millis();
    generateUniqueLogFilename(currentWallTime.month, currentWallTime.day, currentWallTime.hour, currentWallTime.minute);
    currentLogFile = sd.open(logFilename, FILE_WRITE);
    if (currentLogFile)
    {
        currentLogFile.println("time_ms,pwm_output,pressure,temperature");
        Serial.print("Started new log: ");
        Serial.println(logFilename);
    }
    else
    {
        Serial.println("Failed to open new log file.");
        loggingEnabled = false;
    }
}

void generateUniqueLogFilename(uint8_t month, uint8_t day, uint8_t hour, uint8_t minute)
{
    int counter = 0;
    bool exists = true;

    while (exists)
    {
        if (counter == 0)
        {
            sprintf(logFilename, "%02d_%02d_%02d.csv", day, hour, minute);
        }
        else
        {
            sprintf(logFilename, "%02d_%02d_%02d_%02d.csv", day, hour, minute, counter);
        }

        exists = sd.exists(logFilename);
        counter++;
    }

    Serial.print("Generated log filename: ");
    Serial.println(logFilename);
}

void stopLogging() {
    if (loggingEnabled) {
      if (currentLogFile) currentLogFile.close();
      loggingEnabled = false;
      Serial.println("Logging stopped.");
    }
}
