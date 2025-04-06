#include <Arduino.h>  // Include Arduino framework for analogRead
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>  // Include ESP32Servo library for Servo functionality


// Pin Definitions
#define MOTOR_IN1 26    // Right Motor Forward
#define MOTOR_IN2 14    // Right Motor Backward
#define MOTOR_IN3 27    // Left Motor Forward
#define MOTOR_IN4 12    // Left Motor Backward
#define MOTOR_ENA 25    // Right Motor Speed
#define MOTOR_ENB 33    // Left Motor Speed

#define TRIG_PIN 4      // Ultrasonic Trigger
#define ECHO_PIN 5      // Ultrasonic Echo
#define SCAN_SERVO_PIN 15   // Ultrasonic Scan Servo 


#define MOISTURE_SERVO 23 // Moisture Servo Pin (PWM)
#define MOISTURE_SENSOR 34 // Moisture Sensor Pin (Analog)

#define MQ2_PIN 32  // Connect MQ2 A0 to GPIO34 (ADC-capable)

// Safety Parameters
#define SAFE_DISTANCE 40   // Safe distance in cm before alert
#define GAS_THRESHOLD 3000  // Gas level threshold for alert

// Servo Positions
#define SERVO_LEFT 180     // Left position
#define SERVO_CENTER 90    // Center position
#define SERVO_RIGHT 0      // Right position

// Function Prototypes
void stopMotors();
void handleRoot();
void handleControl();
void handleStatus();
void handleServo();
void handleMove();
void handleMoisture();
void measureDistance();
void autonomousMode();

// WiFi Configuration
const char* ssid = "MoonRoverAP";
const char* password = "rover12345";

// Static IP Configuration
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
Servo scanServo;  // Using ESP32Servo library
Servo moistureServoControl;  // Servo for moisture sensor control

// Global Variables
int scanServoAngle = 90;
int xPos = 128;
int yPos = 128;
int distance = 0;
int moisture = 0;
int gasLevel = 0;
bool manualMode = true;
bool objectAlert = false;
bool gasAlert = false;
int mappedMoistureValue = 0; // Mapped moisture value for servo control

// Motor control functions
void moveForward(int speed) {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, HIGH);

  analogWrite(MOTOR_ENA, speed);
  analogWrite(MOTOR_ENB, speed);
  }
  
  void moveBackward(int speed) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, HIGH);
    digitalWrite(MOTOR_IN4, LOW);

    analogWrite(MOTOR_ENA, speed);
    analogWrite(MOTOR_ENB, speed);
  }
  
  void turnLeft(int speed) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, HIGH);
    
    analogWrite(MOTOR_ENA, speed);
    analogWrite(MOTOR_ENB, speed);
  }
  
  void turnRight(int speed) {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    digitalWrite(MOTOR_IN3, HIGH);
    digitalWrite(MOTOR_IN4, LOW);
    
    analogWrite(MOTOR_ENA, speed);
    analogWrite(MOTOR_ENB, speed);
  }
  
  void stopMotors() {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, LOW);
    analogWrite(MOTOR_ENA, 0);
    analogWrite(MOTOR_ENB, 0);
  }
  

void setup() {
  Serial.begin(115200);

  // Motor Pin Setup
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  pinMode(MOTOR_ENB, OUTPUT);

  // Ultrasonic Sensor Pin Setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Moisture motor pins
  pinMode(MOISTURE_SERVO, OUTPUT);
  pinMode(MOISTURE_SENSOR, INPUT);
  
  // MQ2 Sensor Pin Setup
  pinMode(MQ2_PIN, INPUT);  // Set MQ2 pin as input
  

  // Servo Setup: allocate ESP32 PWM timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Adjusted pulse width range from (500,2500) to (1000,2000)
  scanServo.attach(SCAN_SERVO_PIN, 1000, 2000);
  scanServo.write(SERVO_CENTER);
  delay(500);  // Allow servo to reach the center position


  // Moisture Servo Setup
  moistureServoControl.attach(MOISTURE_SERVO, 1000, 2000);
  moistureServoControl.write(0);  // Start with the servo at 0 degrees
  delay(500);  // Allow servo to reach the initial position 
  
  // WiFi Access Point Setup
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  // Web Server Routes - FIXED to match the JavaScript requests
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/status", handleStatus);
  server.on("/servo", handleServo);
  server.on("/forward", []() { moveForward(200); server.send(200, "text/plain", "OK"); });
  server.on("/backward", []() { moveBackward(200); server.send(200, "text/plain", "OK"); });
  server.on("/left", []() { turnLeft(200); server.send(200, "text/plain", "OK"); });
  server.on("/right", []() { turnRight(200); server.send(200, "text/plain", "OK"); });
  server.on("/stop", []() { stopMotors(); server.send(200, "text/plain", "OK"); });
  server.on("/moisture", handleMoisture);
  
  server.begin();


}

void loop() {


  gasLevel = analogRead(MQ2_PIN);  // Read analog value (0–4095)
  
  Serial.print("MQ2 Gas Sensor Value: ");
  Serial.println(gasLevel);  

  delay(1000);  // Wait for 1 second




  server.handleClient();
  
  // Measure distance
  measureDistance();
  
  // Read moisture sensor value
  mappedMoistureValue = analogRead(MOISTURE_SENSOR);
  moisture  = map(mappedMoistureValue, 0, 4095, 100, 0); // Map to percentage (0-100%)
  
  // Check for alerts
  objectAlert = (distance < SAFE_DISTANCE);
  gasAlert = (gasLevel > GAS_THRESHOLD);
  
  // Check if in automatic mode
  if (!manualMode) {
    autonomousMode();
  }
}

void measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  distance = duration * 0.034 / 2;  // Calculate distance in cm
  
  // Constrain distance to reasonable range
  distance = constrain(distance, 0, 400);
}

String autoModeStatus = "Idle";

// Modified autonomousMode function for better navigation
void autonomousMode() 
{
  const int MOVE_SPEED = 120;
  const int TURN_SPEED = 100;
  
  // Center the servo and measure initial distance
  scanServo.write(SERVO_CENTER);
  delay(300);
  measureDistance();

  // Filter out unreliable distance readings
  if (distance == 0 || distance > 300) {
    delay(100);
    measureDistance();
    if (distance == 0 || distance > 300) {
      distance = 50; // Default safe distance
    }
  }
  
  // Obstacle avoidance logic
  if (distance < SAFE_DISTANCE) 
  {
    // Stop and back up sequence
    stopMotors();
    autoModeStatus = "Obstacle detected - stopping";
    delay(300);
    
    moveBackward(MOVE_SPEED);
    delay(1500);
    stopMotors();
    autoModeStatus = "Backing up to safe distance";
    delay(300);
    
    // Scan surroundings
    int leftDistance = 0;
    int rightDistance = 0;
    
    // Scan left
    scanServo.write(SERVO_LEFT);
    delay(500);
    measureDistance();
    leftDistance = (distance == 0 || distance > 300) ? 50 : distance;
    
    // Scan right
    scanServo.write(SERVO_RIGHT);
    delay(500);
    measureDistance();
    rightDistance = (distance == 0 || distance > 300) ? 50 : distance;
    
    // Return servo to center
    scanServo.write(SERVO_CENTER);
    delay(300);
    
    // Decision logic for navigation
    if (leftDistance < 30 && rightDistance < 30) {  // Fixed condition syntax
      // Both sides blocked, back up more
      moveBackward(MOVE_SPEED);
      autoModeStatus = "Both sides blocked - backing up";
      delay(1500);
      stopMotors();
    }
    else if (leftDistance > rightDistance) {
      // More space on left
      turnLeft(TURN_SPEED);
      autoModeStatus = "Turning left: " + String(leftDistance) + "cm";
      int turnDuration = map(leftDistance, 30, 100, 500, 1000);
      delay(constrain(turnDuration, 500, 1000));
    }
    else {
      // More space on right (or equal distance)
      turnRight(TURN_SPEED);
      autoModeStatus = "Turning right: " + String(rightDistance) + "cm";
      int turnDuration = map(rightDistance, 30, 100, 500, 1000);  // Fixed to use rightDistance
      delay(constrain(turnDuration, 500, 1000));
    }
    
    stopMotors();  // Ensure motors stop after turning
  }
  else {
    // No obstacle - move forward
    moveForward(MOVE_SPEED);
    autoModeStatus = "Moving forward: " + String(distance) + "cm";
  }
}


// For differential steering (like with joystick)
void setMotorSpeeds(int leftSpeed, int rightSpeed) {
  // Set motor directions based on speed values
  if (rightSpeed > 0) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
  } else {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
  }
  
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
}

// NEW: Handle the status endpoint
void handleStatus() {

  String jsonResponse = "{\"distance\":" + String(distance) + 
                        ",\"moisture\":" + String(moisture) + 
                        ",\"gas\":" + String(gasLevel) + 
                        ",\"objectAlert\":" + String(objectAlert ? "true" : "false") + 
                        ",\"gasAlert\":" + String(gasAlert ? "true" : "false") + 
                        ",\"servoAngle\":" + String(scanServoAngle) + 
                        ",\"autoStatus\":\"" + (manualMode ? "" : autoModeStatus) + "\"}";
  server.send(200, "application/json", jsonResponse);
}

// NEW: Handle the servo endpoint
void handleServo() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    // Constrain angle to valid values
    angle = constrain(angle, 0, 180);
    scanServo.write(angle);
    scanServoAngle = angle;
    
    server.send(200, "text/plain", "Servo angle set to " + String(angle));
  } else {
    server.send(400, "text/plain", "Missing angle parameter");
  }
}

// NEW: Handle moisture motor control
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

      // If in auto mode, skip manual control
      if (!manualMode) {
        server.send(200, "text/plain", "AUTO_MODE");
        return;
      }
      
      // Safety check
      if (objectAlert && yPos > 128) {
        stopMotors();
        server.send(200, "text/plain", "BLOCKED");
        return;
      }
      
      // Calculate motor speeds based on joystick
      int xSpeed = map(xPos, 0, 255, -255, 255);
      int ySpeed = map(yPos, 0, 255, -255, 255);
      int rightMotorSpeed = constrain(ySpeed - xSpeed, -255, 255);
      int leftMotorSpeed = constrain(ySpeed + xSpeed, -255, 255);
      
      setMotorSpeeds(leftMotorSpeed, rightMotorSpeed);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing joystick parameters");
    }
  }
}

void handleRoot() {
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
            justify-content: center; 
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
            max-width: 600px;
            width: 100%;
            box-shadow: 0 0 20px rgba(0, 255, 0, 0.3);
            position: relative;
            overflow: hidden;
        }
        .control-panel::before {
            content: "";
            position: absolute;
            top: -10px;
            left: -10px;
            right: -10px;
            bottom: -10px;
            z-index: -1;
            background: linear-gradient(45deg, rgba(0,255,0,0.1), rgba(0,0,0,0));
            border-radius: 20px;
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
        .button:hover {
            background-color: #00FF00;
            color: black;
        }
        @media (max-width: 500px) {
            #joystick {
                width: 150px;
                height: 150px;
            }
            .control-panel {
                padding: 15px;
            }
        }

        /* Completely revised vertical slider styles */
        .vertical-slider-container {
            width: 60px;
            height: 150px;
            position: relative;
            margin: 0 20px;
        }

        /* Make sure range input is not rotated by default */
        input[type="range"] {
            -webkit-appearance: none;
            background: transparent;
            cursor: pointer;
        }

        /* Vertical slider specific styles */
        .vertical-slider {
            -webkit-appearance: none;
            width: 150px; /* This becomes the height after rotation */
            height: 8px; /* This becomes the width after rotation */
            position: absolute;
            top: 75px;
            left: -45px; /* Center it */
            transform: rotate(270deg);
            background: #333;
            outline: none;
            z-index: 10;
        }

        /* Custom thumb styling */
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

        .vertical-slider::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #00FF00;
            cursor: pointer;
            border: none;
            box-shadow: 0 0 5px rgba(0, 255, 0, 0.7);
        }

        /* Slider track styling */
        .vertical-slider::-webkit-slider-runnable-track {
            width: 100%;
            height: 8px;
            cursor: pointer;
            background: #333;
            border-radius: 4px;
        }

        .vertical-slider::-moz-range-track {
            width: 100%;
            height: 8px;
            cursor: pointer;
            background: #333;
            border-radius: 4px;
        }

        /* Labels for min/max values */
        .slider-labels {
            position: absolute;
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            height: 100%;
            right: -15px;
            top: 0;
            font-size: 12px;
        }

        .value-display {
            font-size: 18px;
            text-align: center;
            margin-top: 10px;
            min-width: 90px;
        }

        /* Added CSS for alerts visibility */
        .visible {
            display: block !important;
        }
    </style>
</head>
<body>
    <div class="control-panel">
        <h2>MOON ROVER CONTROL</h2>
        
        <div class="mode-section">
            <span>AUTO</span>
            <label class="toggle">
                <input type="checkbox" id="modeToggle" checked>
                <span class="slider"></span>
            </label>
            <span>MANUAL</span>
        </div>
        
        <div id="alertBox" class="alert">PROXIMITY ALERT!</div>
        <div id="gasAlertBox" class="alert">TOXIC GAS ALERT!</div>
        
        <div class="joystick-container">
            <div id="joystick">
                <div id="joystickDot"></div>
            </div>
            <div class="coordinates">
                <div>X: <span id="xValue">128</span></div>
                <div>Y: <span id="yValue">128</span></div>
            </div>
        </div>
        
        <div class="control-section">
            <span class="label">Moisture Sensor Position:</span>
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
        
        <!-- Scanner Control Buttons -->
        <div class="control-section">
            <span class="label">Scanner Direction:</span>
            <div style="display: flex; align-items: center;">
                <button id="leftButton" class="button">Left</button>
                <button id="centerButton" class="button">Center</button>
                <button id="rightButton" class="button">Right</button>
            </div>
        </div>

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
                <div class="label">GAS LEVEL</div>
                <div class="sensor-value"><span id="gas">0</span></div>
            </div>
            <div class="sensor-box">
                <div class="label">MODE</div>
                <div class="sensor-value"><span id="modeDisplay">MANUAL</span></div>
            </div>
            <!-- Add this to the sensor-section div -->
            <div class="sensor-box" style="width: 100%;">
                <div class="label">AUTO MODE STATUS</div>
                <div class="sensor-value"><span id="autoStatus">-</span></div>
            </div>
        </div>
    </div>
<script>
    const joystick = document.getElementById('joystick');
    const joystickDot = document.getElementById('joystickDot');
    const modeToggle = document.getElementById('modeToggle');
    const xValue = document.getElementById('xValue');
    const yValue = document.getElementById('yValue');
    const distanceSpan = document.getElementById('distance');
    const moistureSpan = document.getElementById('moisture');
    const gasSpan = document.getElementById('gas');
    const modeDisplay = document.getElementById('modeDisplay');
    const leftButton = document.getElementById('leftButton');
    const centerButton = document.getElementById('centerButton');
    const rightButton = document.getElementById('rightButton');
    const alertBox = document.getElementById('alertBox');
    const gasAlertBox = document.getElementById('gasAlertBox');
    const autoStatusSpan = document.getElementById('autoStatus');
    const moistureSlider = document.getElementById('moistureSlider');
    const moistureValueDisplay = document.getElementById('moistureValue');

    // Set initial value display
    moistureValueDisplay.textContent = moistureSlider.value;

    // Moisture slider event
    moistureSlider.addEventListener('input', function() {
        const position = this.value;
        moistureValueDisplay.textContent = position;
        
        // Send the position value to the server
        fetch(`/moisture?position=${position}`)
            .then(response => {
                if (!response.ok) {
                    console.error('Failed to update moisture sensor position.');
                }
            })
            .catch(error => {
                console.error('Error:', error);
            });
    });

    let currentCommand = '';
    let isManualMode = true;

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

        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const distance = Math.sqrt(Math.pow(x - centerX, 2) + Math.pow(y - centerY, 2));
        const radius = rect.width / 2;

        if (distance > radius) {
            const angle = Math.atan2(y - centerY, x - centerX);
            x = centerX + radius * Math.cos(angle);
            y = centerY + radius * Math.sin(angle);
        }

        x = Math.max(0, Math.min(x, rect.width));
        y = Math.max(0, Math.min(y, rect.height));

        joystickDot.style.left = `${x}px`;
        joystickDot.style.top = `${y}px`;

        const mappedX = Math.round((x / rect.width) * 255);
        const mappedY = Math.round((y / rect.height) * 255);

        xValue.textContent = mappedX;
        yValue.textContent = mappedY;

        // Movement logic
        const xOffset = mappedX - 128;
        const yOffset = 128 - mappedY;

        let command = '';
        if (Math.abs(yOffset) > 40) {
            command = yOffset > 0 ? 'forward' : 'backward';
        } else if (Math.abs(xOffset) > 40) {
            command = xOffset > 0 ? 'right' : 'left';
        } else {
            command = 'stop';
        }

        if (command !== currentCommand) {
            console.log(`Joystick position: X=${mappedX}, Y=${mappedY}`);
            console.log(`Offsets: X=${xOffset}, Y=${yOffset}`);
            console.log(`Sending command: ${command}`);
            fetch(`/${command}`);
            currentCommand = command;
        }
    }

    function resetJoystick() {
        joystickDot.style.left = '50%';
        joystickDot.style.top = '50%';
        xValue.textContent = '128';
        yValue.textContent = '128';
        fetch('/stop');
        currentCommand = 'stop';
    }

    // Joystick event bindings
    joystick.addEventListener('mousedown', (e) => {
        if (!isManualMode) return;
        e.preventDefault();
        updateJoystick(e);
        joystick.addEventListener('mousemove', updateJoystick);
    });

    document.addEventListener('mouseup', () => {
        joystick.removeEventListener('mousemove', updateJoystick);
        resetJoystick();
    });

    joystick.addEventListener('touchstart', (e) => {
        if (!isManualMode) return;
        e.preventDefault();
        updateJoystick(e);
        joystick.addEventListener('touchmove', updateJoystick);
    });

    document.addEventListener('touchend', () => {
        joystick.removeEventListener('touchmove', updateJoystick);
        resetJoystick();
    });

    // Servo controls
    leftButton.addEventListener('click', () => fetch('/servo?angle=180'));
    centerButton.addEventListener('click', () => fetch('/servo?angle=90'));
    rightButton.addEventListener('click', () => fetch('/servo?angle=0'));

    // Mode toggle
    modeToggle.addEventListener('change', () => {
        isManualMode = modeToggle.checked;
        modeDisplay.textContent = isManualMode ? "MANUAL" : "AUTO";
        
        // Stop motors when changing modes
        fetch('/stop');
        
        // Inform server about mode change
        fetch(`/control?x=128&y=128&mode=${isManualMode}`);
    });

    // Status polling
    function updateSensorData() {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                distanceSpan.textContent = data.distance;
                moistureSpan.textContent = data.moisture;
                gasSpan.textContent = data.gas;
                console.log("Raw gas data:", data.gas);

                // Show/hide alerts
                if (data.objectAlert) {
                    alertBox.classList.add('visible');
                } else {
                    alertBox.classList.remove('visible');
                }
                
                if (data.gasAlert) {
                    gasAlertBox.classList.add('visible');
                } else {
                    gasAlertBox.classList.remove('visible');
                }

                // Add more debug info
                console.log("Mode toggle checked:", modeToggle.checked);
                console.log("Full data received:", data);

                if (!modeToggle.checked) {
                    console.log("In AUTO mode");
                    if (data.autoStatus) {
                        console.log("AUTO MODE: " + data.autoStatus);
                        autoStatusSpan.textContent = data.autoStatus;
                    } else {
                        console.log("No auto status data available");
                        autoStatusSpan.textContent = "No status";
                    }
                } else {
                    console.log("In MANUAL mode");
                    autoStatusSpan.textContent = "-";
                }
            })
            .catch(error => {
                console.error('Error fetching status:', error);
            });
    }

    // Initialize page and start polling
    updateSensorData();
    setInterval(updateSensorData, 1000);
    
    // Debug info
    console.log("Page loaded. Moisture slider value:", moistureSlider.value);
</script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}
