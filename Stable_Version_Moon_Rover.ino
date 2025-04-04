#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

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
// If you experience servo movement issues, consider using a different pin (e.g., pin 13).

// Servo Positions
#define SERVO_LEFT 180     // Left position
#define SERVO_CENTER 90    // Center position
#define SERVO_RIGHT 0      // Right position

#define MOISTURE_MOTOR_UP_PIN 23
#define MOISTURE_MOTOR_DOWN_PIN 22

// Safety Parameters
#define SAFE_DISTANCE 20   // Safe distance in cm before alert
#define GAS_THRESHOLD 500  // Gas level threshold for alert

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
  pinMode(MOISTURE_MOTOR_UP_PIN, OUTPUT);
  pinMode(MOISTURE_MOTOR_DOWN_PIN, OUTPUT);
  
  // Initial state for moisture motor pins
  digitalWrite(MOISTURE_MOTOR_UP_PIN, LOW);
  digitalWrite(MOISTURE_MOTOR_DOWN_PIN, LOW);

  // Servo Setup: allocate ESP32 PWM timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Adjusted pulse width range from (500,2500) to (1000,2000)
  scanServo.attach(SCAN_SERVO_PIN, 1000, 2000);
  scanServo.write(SERVO_CENTER);
  
  // WiFi Access Point Setup
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  Serial.println("WiFi Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

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

  // Randomize seed for autonomous mode
  randomSeed(analogRead(0));
}

void loop() {
  server.handleClient();
  
  // Update gas sensor reading continuously
  // gasLevel = analogRead(GAS_PIN);
  
  // Measure distance
  measureDistance();
  
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

// Modified autonomousMode function for better navigation
void autonomousMode() {
  // If object is too close, back up first
  if (distance < SAFE_DISTANCE) {
    // Move backward
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, HIGH);
    digitalWrite(MOTOR_IN4, LOW);
    
    analogWrite(MOTOR_ENA, 150); // Slower speed for safety
    analogWrite(MOTOR_ENB, 150);
    
    // Back up for a bit longer
    delay(1200);
    
    // Stop before scanning
    stopMotors();
    delay(300);
  }

  // Scan environment using the ultrasonic scanner servo
  int leftDistance = 0;
  int rightDistance = 0;
  int centerDistance = 0;
  
  // Look center first
  scanServo.write(SERVO_CENTER);
  scanServoAngle = SERVO_CENTER;
  delay(800); // Longer delay for sensor stabilization
  measureDistance();
  centerDistance = distance;
  
  // Look right
  scanServo.write(SERVO_RIGHT);
  scanServoAngle = SERVO_RIGHT;
  delay(800); // Increased delay for better measurements
  measureDistance();
  rightDistance = distance;
  
  // Look left
  scanServo.write(SERVO_LEFT);
  scanServoAngle = SERVO_LEFT;
  delay(800); // Increased delay for better measurements
  measureDistance();
  leftDistance = distance;
  
  // Return to center after scanning
  scanServo.write(SERVO_CENTER);
  scanServoAngle = SERVO_CENTER;
  delay(500);
  
  // Decision making based on scan results
  if (centerDistance > 30) { // Clear path ahead
    // Move forward
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, HIGH);
    
    analogWrite(MOTOR_ENA, 160); // Slower speed for better control
    analogWrite(MOTOR_ENB, 160);
    
    delay(300); // Short movement before next scan
  } else {
    // Obstacle detected, turn based on scan results
    stopMotors();
    delay(200);
    
    if (leftDistance > rightDistance) {
      // Turn left
      digitalWrite(MOTOR_IN1, HIGH);
      digitalWrite(MOTOR_IN2, LOW);
      digitalWrite(MOTOR_IN3, LOW);
      digitalWrite(MOTOR_IN4, HIGH);
    } else {
      // Turn right
      digitalWrite(MOTOR_IN1, LOW);
      digitalWrite(MOTOR_IN2, HIGH);
      digitalWrite(MOTOR_IN3, HIGH);
      digitalWrite(MOTOR_IN4, LOW);
    }
    
    // Turn at reduced speed
    analogWrite(MOTOR_ENA, 130);
    analogWrite(MOTOR_ENB, 130);
    
    // Turn for a short duration
    delay(1000);
    
    // Stop after turning
    stopMotors();
    delay(200);
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
                        ",\"servoAngle\":" + String(scanServoAngle) + "}";
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
    
    Serial.print("Setting servo angle to: ");
    Serial.println(angle);
    
    server.send(200, "text/plain", "Servo angle set to " + String(angle));
  } else {
    server.send(400, "text/plain", "Missing angle parameter");
  }
}

// NEW: Handle moisture motor control
void handleMoisture() {
  if (server.hasArg("position")) {
    String position = server.arg("position");
    
    if (position == "up") {
      digitalWrite(MOISTURE_MOTOR_UP_PIN, HIGH);
      digitalWrite(MOISTURE_MOTOR_DOWN_PIN, LOW);
      server.send(200, "text/plain", "Moving moisture sensor up");
    } 
    else if (position == "down") {
      digitalWrite(MOISTURE_MOTOR_UP_PIN, LOW);
      digitalWrite(MOISTURE_MOTOR_DOWN_PIN, HIGH);
      server.send(200, "text/plain", "Moving moisture sensor down");
    }
    else if (position == "stop") {
      digitalWrite(MOISTURE_MOTOR_UP_PIN, LOW);
      digitalWrite(MOISTURE_MOTOR_DOWN_PIN, LOW);
      server.send(200, "text/plain", "Stopped moisture sensor movement");
    }
    else {
      server.send(400, "text/plain", "Invalid position parameter");
    }
  } else {
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
        input[type="range"] {
            width: 75%;
            accent-color: #00FF00;
            background: #333;
            height: 10px;
            border-radius: 5px;
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
        <div id="gasAlertBox" class="alert">GAS LEVEL ALERT!</div>
        
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
            <span class="label">Moisture Sensor:</span>
            <div style="display: flex; align-items: center;">
                <button id="upButton" class="button">Up</button>
                <button id="downButton" class="button">Down</button>
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
        </div>
    </div>
<script>
    const joystick = document.getElementById('joystick');
    const joystickDot = document.getElementById('joystickDot');
    const modeToggle = document.getElementById('modeToggle');
    const upButton = document.getElementById('upButton');
    const downButton = document.getElementById('downButton');
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

    // Servo controls - FIXED to use the correct endpoint
    leftButton.addEventListener('click', () => fetch('/servo?angle=180'));
    centerButton.addEventListener('click', () => fetch('/servo?angle=90'));
    rightButton.addEventListener('click', () => fetch('/servo?angle=0'));

    // Mode toggle - FIXED to update the server
    modeToggle.addEventListener('change', () => {
        isManualMode = modeToggle.checked;
        modeDisplay.textContent = isManualMode ? "MANUAL" : "AUTO";
        
        // Stop motors when changing modes
        fetch('/stop');
        
        // Inform server about mode change
        fetch(`/control?x=128&y=128&mode=${isManualMode}`);
    });

    // Moisture up/down buttons - FIXED to send actual commands
    upButton.addEventListener('click', () => fetch('/moisture?position=up'));
    downButton.addEventListener('click', () => fetch('/moisture?position=down'));

    // Status polling - FIXED to use the new status endpoint
    function updateSensorData() {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                distanceSpan.textContent = data.distance;
                moistureSpan.textContent = data.moisture;
                gasSpan.textContent = data.gas;
                
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
            })
            .catch(error => console.error('Error fetching sensor data:', error));
    }

    setInterval(updateSensorData, 1000);
</script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}
