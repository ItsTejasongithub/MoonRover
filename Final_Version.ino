/***********************************************************************************************************************
 * ----------------------------------- Moon Rover Control Panel – ESP32 Web Interface -------------------------------- *
 *                                                                                                                     *
 *                                        𝘼𝙡𝙡 𝙧𝙞𝙜𝙝𝙩𝙨 𝙧𝙚𝙨𝙚𝙧𝙫𝙚𝙙 𝙗𝙮 – 10𝙭𝙏𝙚𝙘𝙝𝘾𝙡𝙪𝙗                                          *
 *                                                                                                                     *
 *  Developed by: 10𝙭𝙏𝙚𝙘𝙝𝘾𝙡𝙪𝙗                                                                                          *
 *                                                                                                                     *
 *  Description:                                                                                                       *
 *  A real-time Moon Rover control and monitoring system using ESP32 with a dark-themed web-based UI.                  *
 *                                                                                                                     *
 *  Features:                                                                                                          *
 *   - Manual / Auto mode toggle                                                                                       *
 *   - Joystick-based movement                                                                                         *
 *   - Servo scanner control                                                                                           *
 *   - Live sensor feedback (Ultrasonic, MQ2 Gas, Soil Moisture)                                                       *
 *   - Real-time dynamic gas visualization (C2H5OH, C3H8/C4H10)                                                        *
 *                                                                                                                     *
 *  Technologies Used:                                                                                                 *
 *   - ESP32 + WiFi                                                                                                    *
 *   - HTML, CSS, JavaScript                                                                                           *
 *   - WebServer Library                                                                                               *
 *   - MQ2 Gas Sensor, Ultrasonic Sensor, Servo Motor                                                                  *
 *                                                                                                                     *
 *  Required Library Dependencies:                                                                                     *      
 *   1. Arduino Core for ESP32                                                                                         *
 *      - Source: https://github.com/espressif/arduino-esp32                                                           *
 *      - Description: Core functionality for ESP32 with Arduino support.                                              *
 *                                                                                                                     *
 *   2. WiFi.h                                                                                                         *
 *      - Author: Espressif Systems (Arduino ESP32 Core)                                                               *
 *      - Description: Enables ESP32 to connect to Wi-Fi networks.                                                     *
 *                                                                                                                     *
 *   3. WebServer.h                                                                                                    *
 *      - Author: Espressif Systems (Arduino ESP32 Core)                                                               *
 *      - Description: Lightweight HTTP server to serve web content from ESP32.                                        *
 *                                                                                                                     *
 *   4. ESP32Servo.h                                                                                                   *
 *      - Author: John K. Bennett                                                                                      *
 *      - Source: https://github.com/jkb-git/ESP32Servo                                                                *
 *      - Description: Enables control of standard servos using PWM on ESP32.                                          *
 *                                                                                                                     *
 *  GitHub Repository:                                                                                                 *
 *      https://github.com/ItsTejasongithub/MoonRover.git                                                              *
 *                                                                                                                     *
 ***********************************************************************************************************************/

// ------------------Dependencies---------------
#include <Arduino.h>        // Include Arduino framework for analogRead
#include <WiFi.h>           // Includes the WiFi library to enable ESP32 to connect to a Wi-Fi network
#include <WebServer.h>      // Includes the WebServer library to create a web server on the ESP32
#include <ESP32Servo.h>     // Include ESP32Servo library for Servo functionality
#include <Adafruit_NeoPixel.h>

// -------------- PIN DEFINITIONS --------------
// Motor Driver Pins
#define MOTOR_IN1 26    // Right Motor Forward
#define MOTOR_IN2 14    // Right Motor Backward
#define MOTOR_IN3 27    // Left Motor Forward
#define MOTOR_IN4 12    // Left Motor Backward
#define MOTOR_ENA 25    // Right Motor Speed (PWM)
#define MOTOR_ENB 33    // Left Motor Speed (PWM)

// Ultrasonic Sensor Pins
#define TRIG_PIN 4      // Ultrasonic Trigger
#define ECHO_PIN 5      // Ultrasonic Echo

// Servo Pins
#define SCAN_SERVO_PIN 15   // Ultrasonic Scan Servo
#define MOISTURE_SERVO 23   // Moisture Sensor Servo (PWM)

// -------------- SERVO POSITIONS --------------
#define SERVO_LEFT 180      // Left position (degrees)
#define SERVO_CENTER 90     // Center position (degrees)
#define SERVO_RIGHT 0       // Right position (degrees)

// Sensor Pins
#define MOISTURE_SENSOR 34  // Moisture Sensor Pin (Analog)
#define MQ2_PIN 32          // MQ2 Gas Sensor Pin (Analog)

// -------------- SAFETY PARAMETERS --------------
#define SAFE_DISTANCE 40    // Safe distance in cm before obstacle alert
#define GAS_THRESHOLD 1000  // Gas level threshold for alert


// -------------- FUNCTION PROTOTYPES --------------
void stopMotors();
void handleRoot();
void handleControl();
void handleStatus();
void handleServo();
void handleMove();
void handleMoisture();
void measureDistance();
void autonomousMode();



//// -------------------------------------------------------------------------------------------------------------------------Start OF LED CONTROL

// GPIO pins for NeoPixel strips
#define PIN_FRONT 16
#define PIN_RIGHT 17
#define PIN_LEFT 18
#define PIN_BACK 21

// Number of LEDs per strip
#define NUM_LEDS 10

// Initialize NeoPixel strips
Adafruit_NeoPixel frontStrip(NUM_LEDS, PIN_FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightStrip(NUM_LEDS, PIN_RIGHT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel leftStrip(NUM_LEDS, PIN_LEFT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel backStrip(NUM_LEDS, PIN_BACK, NEO_GRB + NEO_KHZ800);

// Define colors - FIXED: using direct color definitions instead of strip.Color()
#define TEAL_COLOR ((uint32_t)0x00FFBF)  // R:0, G:255, B:191
#define WHITE_COLOR ((uint32_t)0xFFFFFF)  // White
#define RED_COLOR ((uint32_t)0xFF0000)    // Red
#define DIM_FACTOR 0.5  // 50% brightness for idle state


// Variable to keep track of current movement state
enum MovementState {
  STOP,
  FORWARD,
  RIGHT,
  LEFT,
  BACKWARD
};


MovementState currentState = STOP;
unsigned long lastUpdateTime = 0;
int animationStep = 0;
bool animationDirection = true;  // true = forward, false = backward

// Function to clear all LEDs
void clearAllLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, 0);
    rightStrip.setPixelColor(i, 0);
    leftStrip.setPixelColor(i, 0);
    backStrip.setPixelColor(i, 0);
  }
  frontStrip.show();
  rightStrip.show();
  leftStrip.show();
  backStrip.show();
}

// Utility function to adjust brightness of a color
uint32_t adjustBrightness(uint32_t color, int brightness) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
  
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Function for when the car moves forward
void LEDmoveForward() {
  // Front is white
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, WHITE_COLOR);
  }
  
  // Back is red
  for (int i = 0; i < NUM_LEDS; i++) {
    backStrip.setPixelColor(i, RED_COLOR);
  }
  
  // Side strips flow backward (like air passing by)
  for (int i = 0; i < NUM_LEDS; i++) {
    // Calculate position with offset for animation
    int pos = (i + animationStep) % NUM_LEDS;
    
    // Create a gradient effect
    int brightness = 255 - (i * 25);  // Gradually dim trailing LEDs
    if (brightness < 50) brightness = 50;  // Minimum brightness
    
    uint32_t color = adjustBrightness(TEAL_COLOR, brightness);
    
    rightStrip.setPixelColor(pos, color);
    leftStrip.setPixelColor(pos, color);
  }
  
  // Update all strips
  frontStrip.show();
  backStrip.show();
  rightStrip.show();
  leftStrip.show();
}

// Function for when the car turns right
void LEDturnRight() {
  // Front is white
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, WHITE_COLOR);
  }
  
  // Back is red
  for (int i = 0; i < NUM_LEDS; i++) {
    backStrip.setPixelColor(i, RED_COLOR);
  }
  
  // Right strip flows backward faster (outside of turn)
  for (int i = 0; i < NUM_LEDS; i++) {
    int pos = (i + animationStep) % NUM_LEDS;
    int brightness = 255 - (i * 25);
    if (brightness < 50) brightness = 50;
    rightStrip.setPixelColor(pos, adjustBrightness(TEAL_COLOR, brightness));
  }
  
  // Left strip flows forward (inside of turn)
  for (int i = 0; i < NUM_LEDS; i++) {
    int pos = (NUM_LEDS - 1 - ((i + animationStep) % NUM_LEDS));
    int brightness = 255 - (i * 25);
    if (brightness < 50) brightness = 50;
    leftStrip.setPixelColor(pos, adjustBrightness(TEAL_COLOR, brightness));
  }
  
  // Update all strips
  frontStrip.show();
  backStrip.show();
  rightStrip.show();
  leftStrip.show();
}

// Function for when the car turns left
void LEDturnLeft() {
  // Front is white
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, WHITE_COLOR);
  }
  
  // Back is red
  for (int i = 0; i < NUM_LEDS; i++) {
    backStrip.setPixelColor(i, RED_COLOR);
  }
  
  // Left strip flows backward faster (outside of turn)
  for (int i = 0; i < NUM_LEDS; i++) {
    int pos = (i + animationStep) % NUM_LEDS;
    int brightness = 255 - (i * 25);
    if (brightness < 50) brightness = 50;
    leftStrip.setPixelColor(pos, adjustBrightness(TEAL_COLOR, brightness));
  }
  
  // Right strip flows forward (inside of turn)
  for (int i = 0; i < NUM_LEDS; i++) {
    int pos = (NUM_LEDS - 1 - ((i + animationStep) % NUM_LEDS));
    int brightness = 255 - (i * 25);
    if (brightness < 50) brightness = 50;
    rightStrip.setPixelColor(pos, adjustBrightness(TEAL_COLOR, brightness));
  }
  
  // Update all strips
  frontStrip.show();
  backStrip.show();
  rightStrip.show();
  leftStrip.show();
}

// Function for when the car moves backward
void LEDmoveBackward() {
  // Front is white but dimmed to indicate reverse
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, adjustBrightness(RED_COLOR, 180));
  }
  
  // Back is bright red for reverse
  for (int i = 0; i < NUM_LEDS; i++) {
    backStrip.setPixelColor(i, WHITE_COLOR);
  }
  
  // Side strips flow forward (opposite of forward motion)
  for (int i = 0; i < NUM_LEDS; i++) {
    // Calculate position with offset for animation
    int pos = (NUM_LEDS - 1 - ((i + animationStep) % NUM_LEDS));
    
    // Create a gradient effect
    int brightness = 255 - (i * 25);  // Gradually dim trailing LEDs
    if (brightness < 50) brightness = 50;  // Minimum brightness
    
    uint32_t color = adjustBrightness(TEAL_COLOR, brightness);
    
    rightStrip.setPixelColor(pos, color);
    leftStrip.setPixelColor(pos, color);
  }
  
  // Update all strips
  frontStrip.show();
  backStrip.show();
  rightStrip.show();
  leftStrip.show();
}

// Function for when the car is stopped
void LEDstopMoving() {
  // All LEDs fade to 50% brightness
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, adjustBrightness(WHITE_COLOR, 127));  // Front strip white at 50%
    backStrip.setPixelColor(i, adjustBrightness(RED_COLOR, 127));     // Back strip red at 50%
    rightStrip.setPixelColor(i, adjustBrightness(TEAL_COLOR, 127));   // Side strips teal at 50%
    leftStrip.setPixelColor(i, adjustBrightness(TEAL_COLOR, 127));    // Side strips teal at 50%
  }
  
  // Update all strips
  frontStrip.show();
  backStrip.show();
  rightStrip.show();
  leftStrip.show();
}

void LEDAlert() {
  // Flash all LEDs red to indicate alert
  for (int i = 0; i < NUM_LEDS; i++) {
    frontStrip.setPixelColor(i, RED_COLOR);
    rightStrip.setPixelColor(i, RED_COLOR);
    leftStrip.setPixelColor(i, RED_COLOR);
    backStrip.setPixelColor(i, RED_COLOR);
  }
  
  // Update all strips
  frontStrip.show();
  rightStrip.show();
  leftStrip.show();
  backStrip.show();
}
// Function to update LED animations based on movement state

// -------------------------------------------------------------------------------------------------------------------------END OF LED CONTROL





/* ------------------------------------------------------------------------------------------------*/
/*                            GAS SENSOR CODE                                                      */

// -------------- GAS SENSOR CONSTANTS --------------
// ADC settings
#define ADC_RESOLUTION 4095.0  // Maximum ADC value for ESP32 (12-bit)
#define VOLTAGE_REFERENCE 3.3  // Reference voltage for ADC

// Sensor calibration constants
#define RL_VALUE 10.0          // Load resistor value in kΩ
#define RO_CLEAN_AIR_FACTOR 9.83 // Rs/Ro ratio in clean air

// Gas detection ranges (ppm)
#define LPG_MIN 200            // Minimum LPG detection (ppm)
#define LPG_MAX 10000          // Maximum LPG detection (ppm)
#define ALCOHOL_MIN 100        // Minimum alcohol detection (ppm)
#define ALCOHOL_MAX 2000       // Maximum alcohol detection (ppm)

// -------------- GAS SENSOR GLOBAL VARIABLES --------------
float Ro = 10.0;               // Sensor resistance in clean air (initialized, will be calibrated)
float lpgRaw = 0, alcoholRaw = 0;           // Raw gas readings in ppm
float lpgFiltered = 0, alcoholFiltered = 0;  // Filtered/smoothed gas readings
int rawSensorValue = 0;                      // Raw ADC value
int lpgPercent = 0;                          // LPG percentage (0-100%)
int alcoholPercent = 0;                      // Alcohol percentage (0-100%)

// -------------- GAS SENSOR FUNCTION PROTOTYPES --------------
int mapToPercent(float value, float minVal, float maxVal);  // Maps sensor values to percentages
void warmupSensor();                                        // Performs sensor warm-up
void calibrateSensor();                                     // Calibrates sensor in clean air
int getSensorReading();                                     // Gets raw sensor reading
void updateGasConcentrations();                             // Calculates gas concentrations
void applySmoothing();                                      // Applies smoothing filter to readings
void displayData();                                         // Displays sensor data on Serial
float toVoltage(int rawVal);                                // Converts ADC value to voltage
float calculateRs(float voltage);                           // Calculates sensor resistance (Rs)
float getRatio(float rs);                                   // Calculates Rs/Ro ratio
float getLPGppm(float ratio);                               // Calculates LPG concentration in ppm
float getAlcoholppm(float ratio);                           // Calculates alcohol concentration in ppm

/**
 * Maps a sensor value to a percentage within a specified range
 * @param value The value to map
 * @param minVal Minimum value of the range
 * @param maxVal Maximum value of the range
 * @return Percentage (0-100)
 */
int mapToPercent(float value, float minVal, float maxVal) {
  float percent = (value - minVal) * 100.0 / (maxVal - minVal);
  return constrain((int)percent, 0, 100);
}

/**
 * Performs warm-up routine for the MQ2 sensor
 * (MQ2 sensors need warm-up time to stabilize)
 */
void warmupSensor() {
  Serial.println("Warming up MQ2...");
  // delay(20000);  // 20 seconds warm-up time
}

/**
 * Calibrates the sensor in clean air
 * Calculates the base resistance (Ro) used for gas concentration calculations
 */
void calibrateSensor() {
  Serial.println("Calibrating sensor...");
  float rs_sum = 0;
  int valid_readings = 0;
  
  for (int i = 0; i < 50; i++) {
    int val = getSensorReading();
    float voltage = toVoltage(val);
    // Skip unreliable readings
    if (voltage < 0.1) {
      continue;
    }
    
    float rs = calculateRs(voltage);
    rs_sum += rs;
    valid_readings++;
    delay(100);
  }
  
  // Check if we have valid readings
  if (valid_readings > 0) {
    float rs_air = rs_sum / valid_readings;
    Ro = rs_air / RO_CLEAN_AIR_FACTOR;
  } else {
    // Default value if all readings were invalid
    Ro = 10.0; // Use initial value
  }
  
  Serial.print("Calibrated Ro = ");
  Serial.println(Ro);
}

/**
 * Gets raw analog reading from the sensor
 * @return Raw ADC value (0-4095)
 */
int getSensorReading() {
  return analogRead(MQ2_PIN);
}

/**
 * Converts ADC value to voltage
 * @param rawVal Raw ADC value
 * @return Voltage (0-3.3V)
 */
float toVoltage(int rawVal) {
  return rawVal * (VOLTAGE_REFERENCE / ADC_RESOLUTION);
}

/**
 * Calculates the sensor resistance (Rs) from voltage
 * Uses voltage divider formula: Rs = RL * (Vc - Vout) / Vout
 * @param voltage Sensor output voltage
 * @return Sensor resistance in kΩ
 */
float calculateRs(float voltage) {
  // Prevent division by zero
  if (voltage < 0.1) return 999999.0; // Return a high value instead of infinity
  return ((VOLTAGE_REFERENCE - voltage) / voltage) * RL_VALUE;
}

/**
 * Calculates Rs/Ro ratio used for gas concentration formulas
 * @param rs Current sensor resistance
 * @return Rs/Ro ratio
 */
float getRatio(float rs) {
  return rs / Ro;
}

/**
 * Calculates LPG concentration using power regression formula
 * @param ratio Rs/Ro ratio
 * @return LPG concentration in ppm
 */
float getLPGppm(float ratio) {
  return 574.25 * pow(ratio, -2.222);  // Power regression formula for LPG
}

/**
 * Calculates alcohol concentration using power regression formula
 * @param ratio Rs/Ro ratio
 * @return Alcohol concentration in ppm
 */
float getAlcoholppm(float ratio) {
  return 177.8 * pow(ratio, -2.701);  // Power regression formula for alcohol
}

/**
 * Updates gas concentration values based on current sensor reading
 */
void updateGasConcentrations() {
  float voltage = toVoltage(rawSensorValue);
  float rs = calculateRs(voltage);
  float ratio = getRatio(rs);

  // Calculate gas concentrations using ratio
  lpgRaw = getLPGppm(ratio);
  alcoholRaw = getAlcoholppm(ratio);
}

/**
 * Applies exponential moving average filter to smooth readings
 * Uses formula: filteredValue = α * rawValue + (1-α) * filteredValue
 * where α = 0.1 (smoothing factor)
 */
void applySmoothing() {
  // Apply low-pass filter (90% previous value, 10% new value)
  lpgFiltered = 0.9 * lpgFiltered + 0.1 * lpgRaw;
  alcoholFiltered = 0.9 * alcoholFiltered + 0.1 * alcoholRaw;

  // Constrain values to defined ranges
  lpgFiltered = constrain(lpgFiltered, LPG_MIN, LPG_MAX);
  alcoholFiltered = constrain(alcoholFiltered, ALCOHOL_MIN, ALCOHOL_MAX);
}

/**
 * Displays gas sensor data on Serial monitor
 */
void displayData() {
  Serial.print("LPG [ppm]: ");
  Serial.print(lpgFiltered);
  Serial.print(" (");
  Serial.print(lpgPercent);
  Serial.print("%)");

  Serial.print("  | Alcohol [ppm]: ");
  Serial.print(alcoholFiltered);
  Serial.print(" (");
  Serial.print(alcoholPercent);
  Serial.print("%)");

  Serial.print("  | RAW ADC: ");
  Serial.println(rawSensorValue);
}

//                                         Gas Sensor Code END                                      //                  
/* ------------------------------------------------------------------------------------------------*/

// -------------- WiFi AND SERVER CONFIGURATION --------------
const char* ssid = "MoonRoverAP";      // Access Point SSID
const char* password = "rover12345";    // Access Point Password

// Static IP Configuration (Access Point Mode)
IPAddress local_IP(192, 168, 4, 1);    // IP address for AP
IPAddress gateway(192, 168, 4, 1);     // Gateway (same as IP)
IPAddress subnet(255, 255, 255, 0);    // Subnet mask

WebServer server(80);                  // Web server on port 80
Servo scanServo;                       // Servo for ultrasonic scanner
Servo moistureServoControl;            // Servo for moisture sensor positioning

// -------------- GLOBAL VARIABLES --------------
int scanServoAngle = 90;               // Current scan servo angle
int xPos = 128;                        // Joystick X position (0-255)
int yPos = 128;                        // Joystick Y position (0-255)
int distance = 0;                      // Measured distance in cm
int moisture = 0;                      // Moisture value (0-100%)
int gasLevel = 0;                      // Gas level
bool manualMode = true;                // Control mode flag (true=manual, false=auto)
bool objectAlert = false;              // Obstacle detection flag
bool gasAlert = false;                 // Gas alert flag
int mappedMoistureValue = 0;           // Mapped moisture value for servo control
String autoModeStatus = "Idle";        // Status message for autonomous mode

/**
 * Move the rover forward at specified speed
 * @param speed PWM value (0-255)
 */
void moveForward(int speed) {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, HIGH);

  analogWrite(MOTOR_ENA, speed);
  analogWrite(MOTOR_ENB, speed);
  LEDmoveForward();  // Update LED animation for forward movement
  currentState = FORWARD;  // Update current state
  lastUpdateTime = millis();  // Update last update time
  animationStep = (animationStep + 1) % NUM_LEDS;  // Update animation step
}
  
/**
 * Move the rover backward at specified speed
 * @param speed PWM value (0-255)
 */
void moveBackward(int speed) {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, HIGH);
  digitalWrite(MOTOR_IN4, LOW);

  analogWrite(MOTOR_ENA, speed);
  analogWrite(MOTOR_ENB, speed);
  LEDmoveBackward();  // Update LED animation for backward movement
  currentState = BACKWARD;  // Update current state
  lastUpdateTime = millis();  // Update last update time
  animationStep = (animationStep + 1) % NUM_LEDS;  // Update animation step

}
  
/**
 * Turn the rover left at specified speed
 * @param speed PWM value (0-255)
 */
void turnLeft(int speed) {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, HIGH);
  
  analogWrite(MOTOR_ENA, speed);
  analogWrite(MOTOR_ENB, speed);
  LEDturnLeft();  // Update LED animation for left turn
  currentState = LEFT;  // Update current state
  lastUpdateTime = millis();  // Update last update time
  animationStep = (animationStep + 1) % NUM_LEDS;  // Update animation step
}
  
/**
 * Turn the rover right at specified speed
 * @param speed PWM value (0-255)
 */
void turnRight(int speed) {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  digitalWrite(MOTOR_IN3, HIGH);
  digitalWrite(MOTOR_IN4, LOW);
  
  analogWrite(MOTOR_ENA, speed);
  analogWrite(MOTOR_ENB, speed);
  LEDturnRight();  // Update LED animation for right turn
  currentState = RIGHT;  // Update current state
  lastUpdateTime = millis();  // Update last update time
  animationStep = (animationStep + 1) % NUM_LEDS;  // Update animation step

}
  
/**
 * Stop all rover motors
 */
void stopMotors() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, LOW);

  analogWrite(MOTOR_ENA, 0);
  analogWrite(MOTOR_ENB, 0);
  LEDstopMoving();  // Update LED animation for stop
  currentState = STOP;  // Update current state
  lastUpdateTime = millis();  // Update last update time
  animationStep = (animationStep + 1) % NUM_LEDS;  // Update animation step

}

/**
 * Arduino setup function - runs once at startup
 */
void setup() {
  Serial.begin(115200);  // Initialize serial communication


    // Initialize NeoPixel strips
    frontStrip.begin();
    rightStrip.begin();
    leftStrip.begin();
    backStrip.begin();
    
    // Set initial brightness
    frontStrip.setBrightness(255);
    rightStrip.setBrightness(255);
    leftStrip.setBrightness(255);
    backStrip.setBrightness(255);
    
    // Clear all LEDs
    clearAllLEDs();

    
  // Configure ADC for better resolution (11dB attenuation)
  analogSetAttenuation(ADC_11db);
  pinMode(MQ2_PIN, INPUT);

  // Initialize gas sensor
  warmupSensor();
  calibrateSensor();

  // Configure motor control pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  pinMode(MOTOR_ENB, OUTPUT);

  // Configure ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Configure moisture sensor pins
  pinMode(MOISTURE_SERVO, OUTPUT);
  pinMode(MOISTURE_SENSOR, INPUT);
  
  // Configure gas sensor pin
  pinMode(MQ2_PIN, INPUT);

  // Initialize ESP32 PWM for servo control
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Configure scan servo with adjusted pulse width range
  scanServo.attach(SCAN_SERVO_PIN, 1000, 2000);
  scanServo.write(SERVO_CENTER);
  delay(500);  // Allow servo to reach center position

  // Configure moisture position servo
  moistureServoControl.attach(MOISTURE_SERVO, 1000, 2000);
  moistureServoControl.write(0);  // Initialize at 0 degrees
  delay(500);  // Allow servo to reach initial position 
  
  // Configure WiFi in Access Point mode
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  // Configure web server routes
  server.on("/", handleRoot);                // Main control page
  server.on("/control", handleControl);      // Joystick control
  server.on("/status", handleStatus);        // Sensor status
  server.on("/servo", handleServo);          // Scanner servo control
  
  // Direct movement command routes
  server.on("/forward", []() { moveForward(200); server.send(200, "text/plain", "OK"); });
  server.on("/backward", []() { moveBackward(200); server.send(200, "text/plain", "OK"); });
  server.on("/left", []() { turnLeft(200); server.send(200, "text/plain", "OK"); });
  server.on("/right", []() { turnRight(200); server.send(200, "text/plain", "OK"); });
  server.on("/stop", []() { stopMotors(); server.send(200, "text/plain", "OK"); });
  
  // Moisture sensor positioning
  server.on("/moisture", handleMoisture);
  
  // Start the web server
  server.begin();
}

/**
 * Arduino main loop function - runs repeatedly
 */
void loop() {
  // 1. FIRST: Collect ALL sensor data
  // Get all raw sensor readings
  rawSensorValue = getSensorReading();
  mappedMoistureValue = analogRead(MOISTURE_SENSOR);
  measureDistance();  // Get ultrasonic distance measurement
  
  // 2. SECOND: Process all sensor data
  // Process gas sensor data
  updateGasConcentrations();
  applySmoothing();
  
  // Calculate percentages and mappings
  lpgPercent = mapToPercent(lpgFiltered, LPG_MIN, LPG_MAX);
  alcoholPercent = mapToPercent(alcoholFiltered, ALCOHOL_MIN, ALCOHOL_MAX);
  moisture = map(mappedMoistureValue, 0, 4095, 100, 0);  // Higher value = lower moisture
  
  // 3. THIRD: Update alert flags based on processed data
  objectAlert = (distance < SAFE_DISTANCE);
  gasAlert = (rawSensorValue > GAS_THRESHOLD);
  
  // 4. FOURTH: Handle control logic
  // Run autonomous mode if selected
  if (!manualMode) {
    autonomousMode();
  }
  
  // 5. FIFTH: Update outputs based on current state
  switch(currentState) {
    case FORWARD:
      LEDmoveForward();
      break;
    case BACKWARD:
      LEDmoveBackward();
      break;
    case LEFT:
      LEDturnLeft();
      break;
    case RIGHT:
      LEDturnRight();
      break;
    case STOP:
    default:
      if (objectAlert) {
        LEDAlert();  // Flash LEDs red for obstacle alert
      } else if (gasAlert) {
        LEDAlert();  // Flash LEDs red for gas alert
      } else {
        LEDstopMoving();  // Clear LEDs if no alerts
      }
      break;
  }
  
  // 6. SIXTH: Handle timing and animations
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdateTime > 100) {  // Update every 100ms
    // Update animation step
    if (animationDirection) {
      animationStep = (animationStep + 1) % NUM_LEDS;
    } else {
      animationStep = (animationStep + NUM_LEDS - 1) % NUM_LEDS;
    }
    lastUpdateTime = currentMillis;
  }
  
  // 7. FINALLY: Handle communication
  // Process any pending web requests
  server.handleClient();
}



/**
 * Measures distance using ultrasonic sensor
 * Updates global 'distance' variable with measurement in cm
 */
void measureDistance() {
  // Send ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo duration (timeout after 30ms)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // Calculate distance (speed of sound = 0.034 cm/μs)
  distance = duration * 0.034 / 2;
  
    // Handle unreliable distance readings
      if (distance == 0 || distance > 300) 
      {
        distance = 50;  // Use default safe distance if reading is invalid
      }

  // Constrain distance to reasonable range
  distance = constrain(distance, 0, 400);
}

/**
 * Autonomous navigation mode
 * Uses obstacle avoidance to navigate safely
 */
void autonomousMode() 
{
  const int MOVE_SPEED = 120;  // Forward/backward speed
  const int TURN_SPEED = 100;  // Turning speed
  
  // Center scanner and measure distance
  scanServo.write(SERVO_CENTER);
  delay(300);
  measureDistance();


  
  // Obstacle avoidance logic
  if (distance < SAFE_DISTANCE) 
  {
    // Stop when obstacle detected
    stopMotors();
    LEDAlert();
    currentState = STOP;  // Update current state
    lastUpdateTime = millis();  // Update last update time
    autoModeStatus = "Obstacle detected - stopping";
    delay(300);
    
    // Back up from obstacle
    moveBackward(MOVE_SPEED);
    LEDmoveBackward();  // Update LED animation for backward movement
    currentState = BACKWARD;  // Update current state
    delay(1500);
    stopMotors();
    autoModeStatus = "Backing up to safe distance";
    delay(300);
    
    // Scan surroundings to find clear path
    int leftDistance = 0;
    int rightDistance = 0;
    
    // Scan left direction
    scanServo.write(SERVO_LEFT);
    delay(500);
    measureDistance();
    leftDistance = (distance == 0 || distance > 300) ? 50 : distance;
    
    // Scan right direction
    scanServo.write(SERVO_RIGHT);
    delay(500);
    measureDistance();
    rightDistance = (distance == 0 || distance > 300) ? 50 : distance;
    
    // Return scanner to center
    scanServo.write(SERVO_CENTER);
    delay(300);
    
    // Decision logic for navigation
    if (leftDistance < 30 && rightDistance < 30) {
      // Both directions blocked - back up more
      moveBackward(MOVE_SPEED);
      LEDmoveBackward();  // Update LED animation for backward movement
      currentState = BACKWARD;  // Update current state
      autoModeStatus = "Both sides blocked - backing up";
      delay(1500);
      stopMotors();
    }
    else if (leftDistance > rightDistance) {
      // More space on left - turn left
      turnLeft(TURN_SPEED);
      LEDturnLeft();  // Update LED animation for left turn
      currentState = LEFT;  // Update current state      
      autoModeStatus = "Turning left: " + String(leftDistance) + "cm";
      // Calculate turn duration based on distance (more distance = longer turn)
      int turnDuration = map(leftDistance, 30, 100, 500, 1000);
      delay(constrain(turnDuration, 500, 1000));
    }
    else {
      // More space on right - turn right
      turnRight(TURN_SPEED);
      LEDturnRight();  // Update LED animation for right turn
      currentState = RIGHT;  // Update current state
      autoModeStatus = "Turning right: " + String(rightDistance) + "cm";
      int turnDuration = map(rightDistance, 30, 100, 500, 1000);
      delay(constrain(turnDuration, 500, 1000));
    }
    
    stopMotors();  // Stop after completing turn
    LEDstopMoving();  // Stop LED animation
    currentState = STOP;  // Update current state
  }
  else {
    // No obstacle - continue forward
    moveForward(MOVE_SPEED);
    LEDmoveForward();  // Update LED animation for forward movement
    currentState = FORWARD;  // Update current state
    autoModeStatus = "Moving forward: " + String(distance) + "cm";
  }
}

/**
 * Controls motor speeds for differential steering
 * Allows precise control via joystick
 * @param leftSpeed Left motor speed (-255 to 255)
 * @param rightSpeed Right motor speed (-255 to 255)
 */
void setMotorSpeeds(int leftSpeed, int rightSpeed) {
  // Set right motor direction
  if (rightSpeed > 0) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
  } else {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
  }
  
  // Set left motor direction
  if (leftSpeed > 0) {
    digitalWrite(MOTOR_IN3, HIGH);
    digitalWrite(MOTOR_IN4, LOW);
  } else {
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, HIGH);
  }
  
  // Set motor speeds using PWM
  analogWrite(MOTOR_ENA, abs(rightSpeed));
  analogWrite(MOTOR_ENB, abs(leftSpeed));

  // Movement direction logic
  if (leftSpeed > 0 && rightSpeed > 0) {
    // Going forward
    LEDmoveForward();
  } else if (leftSpeed < 0 && rightSpeed < 0) {
    // Going backward
    LEDmoveBackward();
  } else if (leftSpeed > rightSpeed) {
    // Turning left
    LEDturnLeft();
  } else if (rightSpeed > leftSpeed) {
    // Turning right
    LEDturnRight();
  } else if (leftSpeed == 0 && rightSpeed == 0) {
    // Stopped
    LEDstopMoving();
  }


}

/**
 * Handle status endpoint request
 * Returns JSON with all sensor data and status information
 */
void handleStatus() {
  // Build JSON response with current sensor values and status
  String jsonResponse = "{\"distance\":" + String(distance) + 
                        ",\"moisture\":" + String(moisture) + 
                        ",\"gas\":" + String(rawSensorValue) + 
                        ",\"gas1\":" + String(alcoholPercent) + 
                        ",\"gas2\":" + String(lpgPercent) + 
                        ",\"objectAlert\":" + String(objectAlert ? "true" : "false") + 
                        ",\"gasAlert\":" + String(gasAlert ? "true" : "false") + 
                        ",\"servoAngle\":" + String(scanServoAngle) + 
                        ",\"autoStatus\":\"" + (manualMode ? "" : autoModeStatus) + "\"}";
  
  server.send(200, "application/json", jsonResponse);
}

/**
 * Handle servo positioning endpoint
 * Allows remote control of scanner servo angle
 */
void handleServo() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    // Constrain angle to valid range
    angle = constrain(angle, 0, 180);
    scanServo.write(angle);
    scanServoAngle = angle;
    
    server.send(200, "text/plain", "Servo angle set to " + String(angle));
  } else {
    server.send(400, "text/plain", "Missing angle parameter");
  }
}

/**
 * Handle moisture sensor positioning
 * Controls servo that positions the moisture sensor
 */
void handleMoisture() {
  if (server.hasArg("position")) {
    String position = server.arg("position");
    int positionValue = position.toInt();
    
    // Map the slider value (0-100) to servo angle (0-180)
    int servoAngle = map(positionValue, 0, 100, 0, 180);
    
    // Set the servo to the mapped angle
    moistureServoControl.write(servoAngle);
    
    server.send(200, "text/plain", "Moving moisture sensor to position: " + position);
  } 
  else 
  {
    server.send(400, "text/plain", "Missing position parameter");
  }
}

/**
 * Handle control endpoint for joystick/manual control
 * Maps joystick position to motor speeds
 */
void handleControl() {
  if (server.method() == HTTP_GET) {
    // Handle joystick/manual control
    if (server.hasArg("x") && server.hasArg("y")) {
      xPos = server.arg("x").toInt();
      yPos = server.arg("y").toInt();
      
      // Check if mode is specified
      if (server.hasArg("mode")) {
        manualMode = (server.arg("mode") == "true");
      }

      // Skip manual control if in auto mode
      if (!manualMode) {
        server.send(200, "text/plain", "AUTO_MODE");
        return;
      }
      
      // Safety check - prevent forward movement if obstacle detected
      if (objectAlert && yPos > 128) {
        stopMotors();
        server.send(200, "text/plain", "BLOCKED");
        return;
      }
      
      // Calculate motor speeds based on joystick position
      int xSpeed = map(xPos, 0, 255, -255, 255);  // Left/right
      int ySpeed = map(yPos, 0, 255, -255, 255);  // Forward/backward
      
      // Differential steering calculation
      int rightMotorSpeed = constrain(ySpeed - xSpeed, -255, 255);
      int leftMotorSpeed = constrain(ySpeed + xSpeed, -255, 255);
      
      setMotorSpeeds(leftMotorSpeed, rightMotorSpeed);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing joystick parameters");
    }
  }
}

/**
 * Handle root endpoint - serves the HTML control interface
 */
void handleRoot() {
  // HTML content for rover control interface
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Moon Rover Control Panel</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { 
            background-color: #121212; 
            color: #00FF00; 
            font-family: 'Courier New', monospace; 
            display: flex; 
            justify-content: flex-start; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
            padding: 10px;
            box-sizing: border-box;
        }
        
            .control-panel {
            border: 2px solid #00FF00;
            padding: 20px;
            border-radius: 15px;
            max-width: 800px;
            width: 100%;
            box-shadow: 0 0 20px rgba(0, 255, 0, 0.3);
            position: relative;
            overflow: hidden;
        }

        h2 {
            text-align: center;
            margin-top: 0;
            text-shadow: 0 0 10px rgba(0, 255, 0, 0.7);
        }
        .mode-section {
            display: flex;
            justify-content: center;
            align-items: center;
            margin-bottom: 15px;
        }
        .toggle {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
            margin-left: 10px;
        }
        .toggle input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #333;
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: #00FF00;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: #00FF00;
        }
        input:checked + .slider:before {
            transform: translateX(26px);
            background-color: black;
        }
        .joystick-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin: 15px 0;
        }
        #joystick {
            width: 200px;
            height: 200px;
            border: 2px solid #00FF00;
            border-radius: 50%;
            position: relative;
            margin: 10px 0;
            box-shadow: inset 0 0 15px rgba(0, 255, 0, 0.5);
        }
        #joystickDot {
            width: 20px;
            height: 20px;
            background-color: #00FF00;
            border-radius: 50%;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            box-shadow: 0 0 10px rgba(0, 255, 0, 0.9);
        }
        .coordinates {
            display: flex;
            justify-content: space-around;
            width: 100%;
        }
        .control-section {
            margin: 20px 0;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 1rem;
        }
        .sensor-section {
            display: flex;
            justify-content: space-between;
            margin-top: 15px;
            flex-wrap: wrap;
        }
        .sensor-box {
            padding: 10px;
            border: 1px solid #00FF00;
            border-radius: 8px;
            text-align: center;
            width: 45%;
            margin-bottom: 10px;
            background-color: rgba(0, 255, 0, 0.05);
        }
        .sensor-value {
            font-size: 20px;
            font-weight: bold;
        }
        .alert {
            color: #FF0000;
            font-weight: bold;
            animation: blink 1s infinite;
            text-align: center;
            margin: 10px 0;
            display: none;
        }
        @keyframes blink {
            0% { opacity: 1; }
            50% { opacity: 0.3; }
            100% { opacity: 1; }
        }
        .label {
            font-weight: bold;
        }
    /* General button styling */
    .button {
        background-color: #333;
        color: #00FF00;
        border: 1px solid #00FF00;
        border-radius: 5px;
        padding: 5px 10px;
        cursor: pointer;
        transition: all 0.3s;
        margin-left: 10px;
    }

    /* Button hover effect */
    .button:hover {
        background-color: #00FF00;
        color: black;
    }

    /* Responsive design for small screens */
    @media (max-width: 500px) {
        #joystick {
            width: 150px;
            height: 150px;
        }
        .control-panel {
            padding: 15px;
        }
    }

    /* Container for vertical slider */
    .vertical-slider-container {
        width: 60px;
        height: 150px;
        position: relative;
        margin: 10px;
    }

    /* Reset appearance of range inputs */
    input[type="range"] {
        -webkit-appearance: none;
        appearance: none;
        background: transparent;
        cursor: pointer;
    }

    /* Rotated range input for vertical slider */
    .vertical-slider {
        -webkit-appearance: none;
        width: 150px; /* Height after rotation */
        height: 8px; /* Width after rotation */
        position: absolute;
        top: 75px;
        left: -100px;
        transform: rotate(270deg);
        background: #333;
        outline: none;
        z-index: 10;
    }

    /* Custom thumb design for WebKit browsers */
    .vertical-slider::-webkit-slider-thumb {
        -webkit-appearance: none;
        width: 20px;
        height: 10px;
        border-radius: 50%;
        background: #00FF00;
        cursor: pointer;
        border: none;
        box-shadow: 0 0 5px rgba(0, 255, 0, 0.7);
    }

    /* Custom thumb design for Firefox */
    .vertical-slider::-moz-range-thumb {
        width: 20px;
        height: 20px;
        border-radius: 50%;
        background: #00FF00;
        cursor: pointer;
        border: none;
        box-shadow: 0 0 5px rgba(0, 255, 0, 0.7);
    }

    /* Slider track style for WebKit */
    .vertical-slider::-webkit-slider-runnable-track {
        width: 100%;
        height: 8px;
        cursor: pointer;
        background: #333;
        border-radius: 4px;
    }

    /* Slider track style for Firefox */
    .vertical-slider::-moz-range-track {
        width: 100%;
        height: 8px;
        cursor: pointer;
        background: #333;
        border-radius: 4px;
    }

    /* Label container for slider (upper/lower text) */
    .slider-labels {
        position: absolute;
        display: flex;
        flex-direction: column;
        justify-content: space-between;
        height: 100%;
        top: 0;
        font-size: 20px;
    }

    /* Text display for moisture slider position */
    .value-display {
        font-size: 18px;
        text-align: center;
        margin-top: 10px;
        min-width: 90px;
    }

    /* Utility class to force visibility */
    .visible {
        display: block !important;
    }

    /* Centering container elements */
    .container {
        display: flex;
        justify-content: center;
        align-items: center;
        flex-direction: column;
    }

    /* Gas title styling */
    .title_gas {
        font-size: 1rem;
        display: flex;
        justify-content: left;
        align-items: left;
        flex-direction: column;
        margin-top: 20px;
    }

    /* Layout box for gas sensors */
    .gas_box {
        display: flex;
        justify-content: space-around;
        align-items: baseline;
    }

    /* Left gas label styling */
    .gas_value1 {
        margin-right: 10px;
    }

    /* Right gas label styling with bold text */
    .gas_value2 {
        font-weight: bold;
    }

    /* Container for gas level bar */
    .gas-bar-container {
        margin-top: 5px;
        height: 15px;
        width: 100%;
        background: rgba(0, 255, 0, 0.05);
        border: 1px solid #00FF00;
        border-radius: 10px;
        overflow: hidden;
        box-shadow: inset 0 0 8px rgba(0, 255, 0, 0.3);
    }

    /* Dynamic gas bar fill */
    .gas-bar {
        height: 100%;
        width: 0%;
        background: linear-gradient(90deg, #00FF00, #55FF55, #00FF00);
        box-shadow: 0 0 8px #00FF00;
        transition: width 0.3s ease-in-out;
        border-radius: 10px;
    }
</style>
</head>
<body>
    <!-- Main Control Panel Container -->
    <div class="control-panel">
        <h2>MOON ROVER CONTROL</h2>

        <!-- Mode Toggle Switch: AUTO / MANUAL -->
        <div class="mode-section">
            <span>AUTO</span>
            <label class="toggle">
                <input type="checkbox" id="modeToggle" checked>
                <span class="slider"></span>
            </label>
            <span>MANUAL</span>
        </div>

        <!-- Alerts -->
        <div id="alertBox" class="alert">PROXIMITY ALERT!</div>
        <div id="gasAlertBox" class="alert">TOXIC GAS ALERT!</div>

        <!-- Joystick UI for Manual Navigation -->
        <div class="joystick-container">
            <div id="joystick">
                <div id="joystickDot"></div>
            </div>
            <div class="coordinates">
                <div>X: <span id="xValue">128</span></div>
                <div>Y: <span id="yValue">128</span></div>
            </div>
        </div>

        <!-- Moisture Sensor Servo Angle Slider -->
        <div class="control-section">
            <span>MOISTURE SENSOR POSITION:</span>
            <div class="vertical-slider-container">
                <input type="range" min="0" max="180" value="90" id="moistureSlider" class="vertical-slider">
                <div class="slider-labels">
                    <span> UPPER</span>
                    <span> LOWER</span>
                </div>
            </div>
            <div class="value-display">
                Position: <span id="moistureValue">90</span>°
            </div>
        </div>

        <!-- Servo Scanner Direction Buttons -->
        <div class="control-section">
            <span>SCANNER DIRECTION:</span>
            <div style="display: flex; align-items: center;">
                <button id="leftButton" class="button">Left</button>
                <button id="centerButton" class="button">Center</button>
                <button id="rightButton" class="button">Right</button>
            </div>
        </div>

        <!-- Main Sensor Status Display -->
        <div class="sensor-section">
            <div class="sensor-box">
                <div class="label">DISTANCE</div>
                <div class="sensor-value"><span id="distance">0</span> cm</div>
            </div>
            <div class="sensor-box">
                <div class="label">MOISTURE</div>
                <div class="sensor-value"><span id="moisture">0</span> %</div>
            </div>
            <div class="sensor-box">
                <div class="label">Raw Gas Component</div>
                <div class="sensor-value"><span id="gas">0</span></div>
            </div>
            <div class="sensor-box">
                <div class="label">MODE</div>
                <div class="sensor-value"><span id="modeDisplay">MANUAL</span></div>
            </div>
        </div>

        <!-- Detailed Gas Sensor Readings with Progress Bars -->
        <div class="title_gas">DETAILED GAS SENSOR DATA:</div>
        <div class="gas_box">
            <div class="sensor-box">
                <div class="gas_value1">C2H5OH</div>
                <div class="sensor-value"><span id="gas1">0</span> PPM in %</div>
                <div class="gas-bar-container"><div class="gas-bar" id="gasBar1"></div></div>
            </div>
            <div class="sensor-box">
                <div class="gas_value2">C3H8 / C4H10</div>
                <div class="sensor-value"><span id="gas2">0</span> PPM in %</div>
                <div class="gas-bar-container"><div class="gas-bar" id="gasBar2"></div></div>
            </div>
        </div>
    </div>

<script>
    // Initialize DOM Elements
    const joystick = document.getElementById('joystick');
    const joystickDot = document.getElementById('joystickDot');
    const modeToggle = document.getElementById('modeToggle');
    const xValue = document.getElementById('xValue');
    const yValue = document.getElementById('yValue');
    const distanceSpan = document.getElementById('distance');
    const moistureSpan = document.getElementById('moisture');
    const gasSpan = document.getElementById('gas');
    const gas1Span = document.getElementById('gas1');
    const gas2Span = document.getElementById('gas2');
    const gasBar1 = document.getElementById('gasBar1');
    const gasBar2 = document.getElementById('gasBar2');
    const modeDisplay = document.getElementById('modeDisplay');
    const leftButton = document.getElementById('leftButton');
    const centerButton = document.getElementById('centerButton');
    const rightButton = document.getElementById('rightButton');
    const alertBox = document.getElementById('alertBox');
    const gasAlertBox = document.getElementById('gasAlertBox');
    const moistureSlider = document.getElementById('moistureSlider');
    const moistureValueDisplay = document.getElementById('moistureValue');

    // Update initial moisture value
    moistureValueDisplay.textContent = moistureSlider.value;

    // Send updated moisture angle to ESP32
    moistureSlider.addEventListener('input', function() {
        const position = this.value;
        moistureValueDisplay.textContent = position;
        fetch(`/moisture?position=${position}`)
            .catch(error => console.error('Error:', error));
    });

    let currentCommand = '';
    let isManualMode = true;

    // Update joystick position and send movement commands
    function updateJoystick(event) {
        if (!isManualMode) return;

        const rect = joystick.getBoundingClientRect();
        let x, y;

        if (event.type.startsWith('touch')) {
            x = event.touches[0].clientX - rect.left;
            y = event.touches[0].clientY - rect.top;
        } else {
            x = event.clientX - rect.left;
            y = event.clientY - rect.top;
        }

        // Calculate joystick boundary and stick to circle
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const distance = Math.sqrt((x - centerX) ** 2 + (y - centerY) ** 2);
        const radius = rect.width / 2;

        if (distance > radius) {
            const angle = Math.atan2(y - centerY, x - centerX);
            x = centerX + radius * Math.cos(angle);
            y = centerY + radius * Math.sin(angle);
        }

        // Clamp values
        x = Math.max(0, Math.min(x, rect.width));
        y = Math.max(0, Math.min(y, rect.height));

        // Update UI
        joystickDot.style.left = `${x}px`;
        joystickDot.style.top = `${y}px`;

        // Normalize to 0-255 range
        const mappedX = Math.round((x / rect.width) * 255);
        const mappedY = Math.round((y / rect.height) * 255);
        xValue.textContent = mappedX;
        yValue.textContent = mappedY;

        // Movement threshold logic
        const xOffset = mappedX - 128;
        const yOffset = 128 - mappedY;
        let command = '';

        if (Math.abs(yOffset) > 40) command = yOffset > 0 ? 'forward' : 'backward';
        else if (Math.abs(xOffset) > 40) command = xOffset > 0 ? 'right' : 'left';
        else command = 'stop';

        // Send command if changed
        if (command !== currentCommand) {
            fetch(`/${command}`);
            currentCommand = command;
        }
    }

    // Reset joystick to center and stop movement
    function resetJoystick() {
        joystickDot.style.left = '50%';
        joystickDot.style.top = '50%';
        xValue.textContent = '128';
        yValue.textContent = '128';
        fetch('/stop');
        currentCommand = 'stop';
    }

    // Joystick Event Listeners
    joystick.addEventListener('mousedown', (e) => {
        if (!isManualMode) return;
        updateJoystick(e);
        joystick.addEventListener('mousemove', updateJoystick);
    });
    document.addEventListener('mouseup', () => {
        joystick.removeEventListener('mousemove', updateJoystick);
        resetJoystick();
    });
    joystick.addEventListener('touchstart', (e) => {
        if (!isManualMode) return;
        updateJoystick(e);
        joystick.addEventListener('touchmove', updateJoystick);
    });
    document.addEventListener('touchend', () => {
        joystick.removeEventListener('touchmove', updateJoystick);
        resetJoystick();
    });

    // Servo Direction Buttons
    leftButton.addEventListener('click', () => fetch('/servo?angle=180'));
    centerButton.addEventListener('click', () => fetch('/servo?angle=90'));
    rightButton.addEventListener('click', () => fetch('/servo?angle=0'));

    // Toggle between Manual and Auto Mode
    modeToggle.addEventListener('change', () => {
        isManualMode = modeToggle.checked;
        modeDisplay.textContent = isManualMode ? "MANUAL" : "AUTO";
        fetch('/stop'); // stop motors during mode switch
        fetch(`/control?x=128&y=128&mode=${isManualMode}`); // notify server
    });

    // Fetch live sensor data from ESP32 server every second
    function updateSensorData() {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                // Update values
                distanceSpan.textContent = data.distance;
                moistureSpan.textContent = data.moisture;
                gasSpan.textContent = data.gas;
                gas1Span.textContent = data.gas1;
                gas2Span.textContent = data.gas2;
                gasBar1.style.width = data.gas1 + '%';
                gasBar2.style.width = data.gas2 + '%';

                // Show/Hide Alerts
                if (data.objectAlert) alertBox.classList.add('visible');
                else alertBox.classList.remove('visible');

                if (data.gasAlert) gasAlertBox.classList.add('visible');
                else gasAlertBox.classList.remove('visible');
            })
            .catch(error => console.error('Error fetching status:', error));
    }

    // Initial sensor update and set polling interval
    updateSensorData();
    setInterval(updateSensorData, 1000);
</script>
</body>
</html>
)rawliteral";
  // Send HTML response to client
  server.send(200, "text/html", html);
}
        
