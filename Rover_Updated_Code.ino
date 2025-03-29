#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// Pin Definitions
#define MOTOR_IN1 13    // Right Motor Forward
#define MOTOR_IN2 12    // Right Motor Backward
#define MOTOR_IN3 14    // Left Motor Forward
#define MOTOR_IN4 27    // Left Motor Backward
#define MOTOR_ENA 25    // Right Motor Speed
#define MOTOR_ENB 26    // Left Motor Speed

#define TRIG_PIN 17     // Ultrasonic Trigger
#define ECHO_PIN 16     // Ultrasonic Echo
#define SCAN_SERVO_PIN 4     // Ultrasonic Scan Servo
#define MOISTURE_SERVO_PIN 5 // Moisture Sensor Servo
#define MOISTURE_PIN 35 // Moisture Sensor
#define GAS_PIN 34      // Gas Sensor Pin

// Servo Positions
#define SERVO_LEFT 0     // Left position
#define SERVO_CENTER 90  // Center position
#define SERVO_RIGHT 180  // Right position

// Moisture Servo Positions
#define MOISTURE_UP 0    // Raised position
#define MOISTURE_DOWN 180 // Lowered position

// Safety Parameters
#define SAFE_DISTANCE 20 // Safe distance in cm before alert
#define GAS_THRESHOLD 500 // Gas level threshold for alert

// WiFi Configuration
const char* ssid = "MoonRoverAP";
const char* password = "rover12345";

// Static IP Configuration
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
Servo scanServo;
Servo moistureServo;

// Global Variables
int scanServoAngle = 90;
int moistureServoAngle = 0; // Start in up position
int xPos = 128;
int yPos = 128;
int distance = 0;
int moisture = 0;
int gasLevel = 0;
bool manualMode = true;
bool objectAlert = false;
bool gasAlert = false;

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

  // Sensor Pins Setup
  pinMode(MOISTURE_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);

  // Servo Setup
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  scanServo.attach(SCAN_SERVO_PIN, 500, 2500);
  scanServo.write(SERVO_CENTER);
  
  moistureServo.attach(MOISTURE_SERVO_PIN, 500, 2500);
  moistureServo.write(MOISTURE_UP); // Start with moisture sensor up

  // WiFi Access Point Setup
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  Serial.println("WiFi Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Web Server Routes
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.begin();

  // Randomize seed for autonomous mode
  randomSeed(analogRead(0));
}

void loop() {
  server.handleClient();
  
  // Update gas sensor reading continuously
  gasLevel = analogRead(GAS_PIN);
  
  // Only check moisture when the sensor is down
  if (moistureServoAngle > 90) {
    moisture = map(analogRead(MOISTURE_PIN), 0, 4095, 0, 100);
  }
  
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

void autonomousMode() {
  // If object is too close, back up first
  if (distance < SAFE_DISTANCE) {
    // Move backward
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, HIGH);
    
    analogWrite(MOTOR_ENA, 180); // Moderate speed
    analogWrite(MOTOR_ENB, 180);
    
    // Back up for a short duration
    delay(800);
    
    // Stop
    stopMotors();
  }

  // Scan environment using the ultrasonic scanner servo
  int leftDistance = 0;
  int rightDistance = 0;
  
  // Look right
  scanServo.write(SERVO_RIGHT);
  delay(500);
  measureDistance();
  rightDistance = distance;
  
  // Look left
  scanServo.write(SERVO_LEFT);
  delay(500);
  measureDistance();
  leftDistance = distance;
  
  // Return to center after scanning
  scanServo.write(SERVO_CENTER);
  delay(300);
  measureDistance();

  // Decision making based on scan results
  if (distance > 30) { // Clear path ahead
    // Move forward
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, HIGH);
    digitalWrite(MOTOR_IN4, LOW);
    
    analogWrite(MOTOR_ENA, 200); // Moderate speed
    analogWrite(MOTOR_ENB, 200);
  } else {
    // Obstacle detected, turn based on scan results
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
    analogWrite(MOTOR_ENA, 150);
    analogWrite(MOTOR_ENB, 150);
    
    // Turn for a short duration
    delay(800);
  }

  // Brief pause between movements
  delay(100);
}

void stopMotors() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, LOW);
  analogWrite(MOTOR_ENA, 0);
  analogWrite(MOTOR_ENB, 0);
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
        const alertBox = document.getElementById('alertBox');
        const gasAlertBox = document.getElementById('gasAlertBox');

        function updateJoystick(event) {
            const rect = joystick.getBoundingClientRect();
            let x, y;
            
            // Handle both mouse and touch events
            if (event.type.startsWith('touch')) {
                x = event.touches[0].clientX - rect.left;
                y = event.touches[0].clientY - rect.top;
            } else {
                x = event.clientX - rect.left;
                y = event.clientY - rect.top;
            }

            // Calculate distance from center
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            const distance = Math.sqrt(Math.pow(x - centerX, 2) + Math.pow(y - centerY, 2));
            const radius = rect.width / 2;
            
            // If outside the circle, constrain to the edge
            if (distance > radius) {
                const angle = Math.atan2(y - centerY, x - centerX);
                x = centerX + radius * Math.cos(angle);
                y = centerY + radius * Math.sin(angle);
            }

            // Constrain to joystick boundaries
            x = Math.max(0, Math.min(x, rect.width));
            y = Math.max(0, Math.min(y, rect.height));

            joystickDot.style.left = `${x}px`;
            joystickDot.style.top = `${y}px`;

            // Map to 0-255 range
            const mappedX = Math.round((x / rect.width) * 255);
            const mappedY = Math.round((y / rect.height) * 255);
            
            xValue.textContent = mappedX;
            yValue.textContent = mappedY;

            fetch(`/control?x=${mappedX}&y=${mappedY}&mode=${modeToggle.checked}&moisture_pos=none`);
        }

        // Mouse events
        joystick.addEventListener('mousedown', (e) => {
            e.preventDefault();
            updateJoystick(e);
            joystick.addEventListener('mousemove', updateJoystick);
        });

        document.addEventListener('mouseup', () => {
            joystick.removeEventListener('mousemove', updateJoystick);
            resetJoystick();
        });
        
        // Touch events for mobile
        joystick.addEventListener('touchstart', (e) => {
            e.preventDefault();
            updateJoystick(e);
            joystick.addEventListener('touchmove', updateJoystick);
        });
        
        document.addEventListener('touchend', () => {
            joystick.removeEventListener('touchmove', updateJoystick);
            resetJoystick();
        });

        function resetJoystick() {
            joystickDot.style.left = '50%';
            joystickDot.style.top = '50%';
            xValue.textContent = '128';
            yValue.textContent = '128';
            fetch('/control?x=128&y=128&mode=' + modeToggle.checked + '&moisture_pos=none');
        }

        modeToggle.addEventListener('change', () => {
            modeDisplay.textContent = modeToggle.checked ? "MANUAL" : "AUTO";
            fetch(`/control?x=128&y=128&mode=${modeToggle.checked}&moisture_pos=none`);
        });

        upButton.addEventListener('click', () => {
            fetch(`/control?x=128&y=128&mode=${modeToggle.checked}&moisture_pos=up`);
        });

        downButton.addEventListener('click', () => {
            fetch(`/control?x=128&y=128&mode=${modeToggle.checked}&moisture_pos=down`);
        });

        function updateSensorData() {
            fetch('/control')
                .then(response => response.json())
                .then(data => {
                    distanceSpan.textContent = data.distance;
                    moistureSpan.textContent = data.moisture;
                    gasSpan.textContent = data.gasLevel;
                    
                    // Display alerts if needed
                    alertBox.style.display = data.objectAlert ? 'block' : 'none';
                    gasAlertBox.style.display = data.gasAlert ? 'block' : 'none';
                });
        }

        // Update sensor data every 500ms
        setInterval(updateSensorData, 500);
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleControl() {
  if (server.method() == HTTP_GET) {
    if (server.args() == 0) {
      // Send sensor data
      String jsonResponse = "{\"distance\":" + String(distance) + 
                           ",\"moisture\":" + String(moisture) + 
                           ",\"gasLevel\":" + String(gasLevel) + 
                           ",\"objectAlert\":" + String(objectAlert ? "true" : "false") + 
                           ",\"gasAlert\":" + String(gasAlert ? "true" : "false") + "}";
      server.send(200, "application/json", jsonResponse);
      return;
    }

    // Process control parameters
    if (server.hasArg("x") && server.hasArg("y") && 
        server.hasArg("mode")) {
      
      xPos = server.arg("x").toInt();
      yPos = server.arg("y").toInt();
      manualMode = (server.arg("mode") == "true");
      
      // Handle moisture sensor position control
      if (server.hasArg("moisture_pos")) {
        String moisturePos = server.arg("moisture_pos");
        
        if (moisturePos == "up") {
          moistureServo.write(MOISTURE_UP);
          moistureServoAngle = MOISTURE_UP;
        } else if (moisturePos == "down") {
          moistureServo.write(MOISTURE_DOWN);
          moistureServoAngle = MOISTURE_DOWN;
        }
      }

      // If there's an object too close in manual mode, stop motors unless backing up
      if (manualMode && distance < SAFE_DISTANCE && yPos < 128) {
        stopMotors();
        server.send(200, "text/plain", "BLOCKED");
        return;
      }

      // Control Motors based on Joystick in Manual Mode
      if (manualMode) {
        // Map joystick to motor speeds
        // Convert to -255 to 255 range with center at 0
        int xSpeed = map(xPos, 0, 255, -255, 255);
        int ySpeed = map(yPos, 0, 255, -255, 255);
        
        // Calculate motor speeds based on joystick position
        // Forward/backward movement is controlled by Y axis
        // Turning is controlled by X axis
        int rightMotorSpeed = ySpeed - xSpeed;
        int leftMotorSpeed = ySpeed + xSpeed;
        
        // Constrain motor speeds to valid range
        rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);
        leftMotorSpeed = constrain(leftMotorSpeed, -255, 255);
        
        // Set motor directions based on speed values
        if (rightMotorSpeed > 0) {
          digitalWrite(MOTOR_IN1, HIGH);
          digitalWrite(MOTOR_IN2, LOW);
        } else {
          digitalWrite(MOTOR_IN1, LOW);
          digitalWrite(MOTOR_IN2, HIGH);
        }
        
        if (leftMotorSpeed > 0) {
          digitalWrite(MOTOR_IN3, HIGH);
          digitalWrite(MOTOR_IN4, LOW);
        } else {
          digitalWrite(MOTOR_IN3, LOW);
          digitalWrite(MOTOR_IN4, HIGH);
        }
        
        // Set motor speeds using PWM
        analogWrite(MOTOR_ENA, abs(rightMotorSpeed));
        analogWrite(MOTOR_ENB, abs(leftMotorSpeed));
      }

      server.send(200, "text/plain", "OK");
    }
  }
}