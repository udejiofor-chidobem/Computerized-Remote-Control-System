#include <SPI.h>
#include <SD.h>
#include <Servo.h>

#include "pico/mutex.h"

#define MESSAGE_SIZE 4
#define LOG_PERIOD (1000 / 50) // 50 Hz
#define PWM_PIN 3
Servo ESC;

#define SD_MISO 12
#define SD_MOSI 11
#define SD_SCK 10
#define SD_CS 13

File currentLogFile;
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
#define SDA 0
#define SCL 1
#include <TE_SM9000.h>

    // Example settings for SM4291-BCE-S-005-000
    SM9000_sensor T4291(5, 0, -26215, 26214);
#endif

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
const unsigned long heartbeatTimeout = 2*1000; // Timeout period (2 seconds)
bool eStopTriggered = false;                 // Flag to track emergency stop status

int pwmValue = 0;                            // Current PWM value (0-100)
int pwmStartValue = 0;
unsigned long pwmStartTime = 0;              // Time when ramp started
unsigned long rampDuration = 0;              // Duration for ramping
int targetPWM = 0;                           // Target PWM value
bool isRamping = false;                      // Flag to track if ramping is active


float current_rpm;
int stepStartPWM = 0;                        // Starting PWM value for step command
int stepStepPWM = 0;                        // Step increment value (default 10)
int stepStopPWM = 0;                       // Stop PWM value for step command
int stepTime = 5;
int diff = stepStopPWM - stepStartPWM;
bool stepInProgress = false;                 // Flag for step command progress

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
 
#define RPM_PIN 2
float rpm = 0.0;
mutex_t rpm_mutex;

void setup1() {
  mutex_init(&rpm_mutex);
  pinMode(RPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RPM_PIN), rpmInterrupt, RISING);
}

void rpmInterrupt() {
  unsigned long now = micros();
  pulseInterval = now - lastPulseTime;
  lastPulseTime = now;

  float frequency = 1000000.0 / pulseInterval;
  float new_rpm = (frequency / 2.0) * 60.0;  // Assuming 2 pulses per revolution

  mutex_enter_blocking(&rpm_mutex);
  rpm = new_rpm;
  mutex_exit(&rpm_mutex);
}

void loop1() {
  while(1){delay(1);}
}

void setup()
{
    Serial.begin(115200);

    initBT();
    initSD();

    ESC.attach(PWM_PIN);
    ESC.writeMicroseconds(1000);
#ifdef BARO
    Wire.setSDA(SDA);
    Wire.setSCL(SCL);
    Wire.begin();
    T4291.reset();
#endif
}

void loop()
{
    // Check if the heartbeat timeout has been exceeded
    if (millis() - lastHeartbeatTime > heartbeatTimeout && !eStopTriggered)
    {
        report("Heartbeat timeout! Triggering emergency stop.");
        triggerEStop();
        eStopTriggered = true; // Set flag to prevent repeated e-stop
    }

    if (Serial2.available() >= MESSAGE_SIZE)
    {
        byte buffer[MESSAGE_SIZE]; // Max expected payload size
        Serial2.readBytes(buffer, MESSAGE_SIZE);
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

    mutex_enter_blocking(&rpm_mutex);
    current_rpm = rpm;
    mutex_exit(&rpm_mutex);

    if (loggingEnabled && millis() - now >= LOG_PERIOD)
    { // 10 Hz logging
#ifdef BARO
        T4291.readData();
#endif
        now = millis();
        unsigned long elapsed = now - logStartMillis;
        if (currentLogFile) {
            currentLogFile.print(elapsed);
            currentLogFile.print(",");
            currentLogFile.print(pwmValue); // Replace with your real value
            currentLogFile.print(",");
            currentLogFile.println(current_rpm); // Replace with your real value
#ifdef BARO
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
        }
        else if (command_id == HRT)
        {
            Serial.println("Heartbeat received!");
            lastHeartbeatTime = millis(); // Reset the heartbeat timeout
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
            if (currentLogFile) {
                unsigned long elapsed = millis() - logStartMillis;
                currentLogFile.print(elapsed);
                currentLogFile.print(",");
                currentLogFile.print(pwmValue);
                currentLogFile.print(",");
                currentLogFile.print(current_rpm); // Replace with your real value
                currentLogFile.print(", ");
                for (int i = 0; i < MESSAGE_SIZE; i++)
                {
                    currentLogFile.print(cmd[i], DEC); // Print in Decimal
                    currentLogFile.print("|");
                }
                currentLogFile.println("");
                currentLogFile.flush(); // Ensures data is saved every time
            }
            if (command_id == PWMc)
            {
                pwmValue = (int) cmd[1];
                report("Setting PWM to: " + String(pwmValue));
                ESC.writeMicroseconds((int) (1000 + (1000 * ((float)pwmValue /100))));
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
        eStopTriggered = true; // Set flag to prevent repeated e-stop

        pwmStartValue = 0;
        pwmStartTime = 0; // Time when ramp started
        rampDuration = 0; // Duration for ramping
        targetPWM = 0;              // Target PWM value
        isRamping = false;         // Flag to track if ramping is active

        stepStartPWM = 0; // Starting PWM value for step command
        stepStepPWM = 0;  // Step increment value (default 10)
        stepStopPWM = 0;  // Stop PWM value for step command
        stepTime = 5;
        stepInProgress = false; // Flag for step command progress
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
        diff = stepStopPWM - stepStartPWM;
        targetPWM = startPWM + ((diff>0) - (diff<0))*stepPWM; // Initialize with starting PWM value
        stepInProgress = true; // Mark step command as in progress
        report("STEP Command Started | " + String(startPWM) + ", " + String(stepPWM) + ", " + String(stopPWM));
        if (pwmValue != startPWM)
        {
            report("PWM Mismatch, Try Again");
            rampPWM(pwmValue, startPWM, stepTime); // Ramp over 5 seconds
        }
    }
}

// Move to the next step in the sequence
void stepNext()
{
    if (!isRamping) {
        targetPWM = pwmValue + ((diff > 0) - (diff < 0)) * stepStepPWM; // Increment the PWM by step value
        if (((diff > 0) - (diff < 0)) * targetPWM >= stepStopPWM)
        {
            targetPWM = stepStopPWM; // Ensure we do not exceed the stop value
            stepInProgress = false;
            report("Step Completed");
        }
        report("Next step PWM: " + String(targetPWM));
        rampPWM(pwmValue, targetPWM, stepTime); // Ramp over 5 seconds
    }
}

// Ramp PWM from current value to target value over a specified duration
void rampPWM(int startPWM, int stopPWM, int duration)
{
    targetPWM = stopPWM;
    rampDuration = duration * 1000; // Convert seconds to milliseconds
    pwmStartTime = millis();        // Store the start time of the ramp
    pwmStartValue = startPWM;
    isRamping = true;
    report("RAMP Command Started | " + String(startPWM) + ", " + String(stopPWM) + ", " + String(duration));
    if (pwmValue != startPWM) {
        rampPWM(pwmValue, startPWM, stepTime);
    }
}

// Impulse behavior: Change PWM suddenly for a set duration
void impulsePWM(int impulsePWM, int duration)
{
    pwmValue = impulsePWM;
    ESC.writeMicroseconds((int) (1000 + (1000 * ((float)pwmValue /100)))); // Set PWM to impulse value
    report("Impulse started!");
    delay(duration * 1000);        // Wait for the duration of the impulse
    pwmValue = 0;                  // Set PWM back to 0 after impulse
    ESC.writeMicroseconds((int) (1000 + (1000 * ((float)pwmValue /100)))); // Stop PWM after impulse
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
            ESC.writeMicroseconds((int) (1000 + (1000 * ((float)pwmValue /100)))); // Update PWM output
        }
        else
        {
            pwmValue = targetPWM;          // Ensure we reach the target PWM value at the end of the ramp
            ESC.writeMicroseconds((int) (1000 + (1000 * ((float)pwmValue /100)))); // Set PWM to the target value
            isRamping = false;             // Stop ramping
            report("Ramp Completed");
        }
    }
}

void report(String data) {
    data = data +"\n";
    Serial.print(data);
    Serial2.write(data.c_str(), data.length());
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
    currentLogFile = SD.open(logFilename, FILE_WRITE);
    if (currentLogFile)
    {
        currentLogFile.println("time_ms,pwm_output,rpm,pressure,temperature,cmd");
        report("Started new log: ");
        report(logFilename);
    }
    else
    {
        report("Failed to open new log file.");
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

        exists = SD.exists(logFilename);
        counter++;
    }

    Serial.print("Generated log filename: ");
    Serial.println(logFilename);
}

void stopLogging() {
    if (loggingEnabled) {
      if (currentLogFile) currentLogFile.close();
      loggingEnabled = false;
      report("Logging stopped.");
    }
}

void initSD() {
    report("\nInitializing SD card...");

    // Ensure the SPI pinout the SD card is connected to is configured properly
    SPI1.setRX(SD_MISO);
    SPI1.setTX(SD_MOSI);
    SPI1.setSCK(SD_SCK);

    // see if the card is present and can be initialized:
    if (!SD.begin(SD_CS, SPI1))
    {
        report("Card failed, or not present");
        // don't do anything more:
        return;
    }
    report("Card initialized.");
}

void initBT() {
    // Begin serial communication with Arduino and HC-05
    Serial2.setTX(4); // note cannot swap pins
    Serial2.setRX(5);
    Serial2.begin(115200);
}