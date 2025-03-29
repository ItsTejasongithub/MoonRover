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
#define SERVO_PIN 4     // Servo Control
#define MOISTURE_PIN 35 // Moisture Sensor

// Servo Positions
#define SERVO_TOP 0     // Top position
#define SERVO_MIDDLE 90 // Middle position
#define SERVO_BOTTOM 180 // Bottom position

// WiFi Configuration
const char* ssid = "MoonRoverAP";
const char* password = "rover12345";

// Static IP Configuration
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
Servo scanServo;

// Global Variables
int servoAngle = 90;
int xPos = 128;
int yPos = 128;
int distance = 0;
int moisture = 0;
bool manualMode = false;

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

  // Moisture Sensor Pin Setup
  pinMode(MOISTURE_PIN, INPUT);

  // Servo Setup
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  scanServo.attach(SERVO_PIN, 500, 2500);
  scanServo.write(SERVO_MIDDLE);

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
  
  // Continuously update moisture reading
  moisture = map(analogRead(MOISTURE_PIN), 0, 4095, 0, 100);
  
  // Measure distance
  measureDistance();
  
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
  // Scan environment using servo
  int scanAngles[] = {SERVO_TOP, SERVO_MIDDLE, SERVO_BOTTOM};
  int bestAngle = SERVO_MIDDLE;
  int maxClearDistance = 0;

  // Scan environment at different angles
  for (int i = 0; i < 3; i++) {
    scanServo.write(scanAngles[i]);
    delay(500); // Wait for servo to reach position

    // Measure distance at current angle
    measureDistance();

    // Find the angle with the most clear path
    if (distance > maxClearDistance) {
      maxClearDistance = distance;
      bestAngle = scanAngles[i];
    }
  }

  // Return to middle after scanning
  scanServo.write(SERVO_MIDDLE);

  // Decision making based on scan results
  if (maxClearDistance > 30) { // 30 cm clear path
    // Move forward
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, HIGH);
    digitalWrite(MOTOR_IN4, LOW);
    
    analogWrite(MOTOR_ENA, 200); // Moderate speed
    analogWrite(MOTOR_ENB, 200);
  } else {
    // Obstacle detected, change direction
    if (bestAngle == SERVO_TOP) {
      // Turn left
      digitalWrite(MOTOR_IN1, HIGH);
      digitalWrite(MOTOR_IN2, LOW);
      digitalWrite(MOTOR_IN3, LOW);
      digitalWrite(MOTOR_IN4, HIGH);
    } else if (bestAngle == SERVO_BOTTOM) {
      // Turn right
      digitalWrite(MOTOR_IN1, LOW);
      digitalWrite(MOTOR_IN2, HIGH);
      digitalWrite(MOTOR_IN3, HIGH);
      digitalWrite(MOTOR_IN4, LOW);
    } else {
      // If middle is blocked, perform a random turn
      int randomTurn = random(2);
      if (randomTurn == 0) {
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
    }
    
    // Turn at reduced speed
    analogWrite(MOTOR_ENA, 150);
    analogWrite(MOTOR_ENB, 150);
    
    // Turn for a short duration
    delay(500);
  }

  // Brief pause between movements
  delay(200);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Moon Rover Control Panel</title>
    <style>
        body { 
            background-color: black; 
            color: #00FF00; 
            font-family: monospace; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
        }
        .control-panel {
            border: 2px solid #00FF00;
            padding: 20px;
            border-radius: 10px;
        }
        .toggle {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
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
        #joystick {
            width: 200px;
            height: 200px;
            border: 2px solid #00FF00;
            border-radius: 50%;
            position: relative;
            margin: 20px 0;
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
        }
        #servoSlider {
            width: 200px;
            accent-color: #00FF00;
        }
    </style>
</head>
<body>
    <div class="control-panel">
        <h2>Moon Rover Control Panel</h2>
        Mode: <label class="toggle">
            <input type="checkbox" id="modeToggle">
            <span class="slider"></span>
        </label><br>
        <div id="joystick">
            <div id="joystickDot"></div>
        </div>
        X: <span id="xValue">128</span>
        Y: <span id="yValue">128</span><br>
        Servo: <input type="range" id="servoSlider" min="0" max="180" value="90"><br>
        Distance: <span id="distance">0</span> cm<br>
        Moisture: <span id="moisture">0</span> %
    </div>
    <script>
        const joystick = document.getElementById('joystick');
        const joystickDot = document.getElementById('joystickDot');
        const modeToggle = document.getElementById('modeToggle');
        const servoSlider = document.getElementById('servoSlider');
        const xValue = document.getElementById('xValue');
        const yValue = document.getElementById('yValue');
        const distanceSpan = document.getElementById('distance');
        const moistureSpan = document.getElementById('moisture');

        function updateJoystick(event) {
            const rect = joystick.getBoundingClientRect();
            let x = event.clientX - rect.left;
            let y = event.clientY - rect.top;

            x = Math.max(0, Math.min(x, rect.width));
            y = Math.max(0, Math.min(y, rect.height));

            joystickDot.style.left = `${x}px`;
            joystickDot.style.top = `${y}px`;

            xValue.textContent = Math.round((x / rect.width) * 255);
            yValue.textContent = Math.round((y / rect.height) * 255);

            fetch(`/control?x=${Math.round((x / rect.width) * 255)}&y=${Math.round((y / rect.height) * 255)}&mode=${modeToggle.checked}&servo=${servoSlider.value}`);
        }

        joystick.addEventListener('mousedown', (e) => {
            updateJoystick(e);
            joystick.addEventListener('mousemove', updateJoystick);
        });

        document.addEventListener('mouseup', () => {
            joystick.removeEventListener('mousemove', updateJoystick);
            joystickDot.style.left = '50%';
            joystickDot.style.top = '50%';
            xValue.textContent = '128';
            yValue.textContent = '128';
            fetch('/control?x=128&y=128&mode=false&servo=90');
        });

        modeToggle.addEventListener('change', () => {
            fetch(`/control?x=128&y=128&mode=${modeToggle.checked}&servo=90`);
        });

        servoSlider.addEventListener('input', () => {
            fetch(`/control?x=128&y=128&mode=${modeToggle.checked}&servo=${servoSlider.value}`);
        });

        function updateSensorData() {
            fetch('/control')
                .then(response => response.json())
                .then(data => {
                    distanceSpan.textContent = data.distance;
                    moistureSpan.textContent = data.moisture;
                });
        }

        setInterval(updateSensorData, 1000);
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
                             ",\"moisture\":" + String(moisture) + "}";
      server.send(200, "application/json", jsonResponse);
      return;
    }

    // Process control parameters
    if (server.hasArg("x") && server.hasArg("y") && 
        server.hasArg("mode") && server.hasArg("servo")) {
      
      xPos = server.arg("x").toInt();
      yPos = server.arg("y").toInt();
      manualMode = (server.arg("mode") == "true");
      servoAngle = server.arg("servo").toInt();

      // Servo Control
      scanServo.write(servoAngle);

      // Control Motors based on Joystick in Manual Mode
      if (manualMode) {
        int speedRight = map(xPos, 0, 255, -255, 255);
        int speedLeft = map(yPos, 0, 255, -255, 255);

        // Right Motor Control
        if (speedRight > 0) {
          digitalWrite(MOTOR_IN1, HIGH);
          digitalWrite(MOTOR_IN2, LOW);
        } else {
          digitalWrite(MOTOR_IN1, LOW);
          digitalWrite(MOTOR_IN2, HIGH);
        }

        // Left Motor Control
        if (speedLeft > 0) {
          digitalWrite(MOTOR_IN3, HIGH);
          digitalWrite(MOTOR_IN4, LOW);
        } else {
          digitalWrite(MOTOR_IN3, LOW);
          digitalWrite(MOTOR_IN4, HIGH);
        }

        analogWrite(MOTOR_ENA, abs(speedRight));
        analogWrite(MOTOR_ENB, abs(speedLeft));
      } else {
        // Stop motors in automatic mode
        digitalWrite(MOTOR_IN1, LOW);
        digitalWrite(MOTOR_IN2, LOW);
        digitalWrite(MOTOR_IN3, LOW);
        digitalWrite(MOTOR_IN4, LOW);
      }

      server.send(200, "text/plain", "OK");
    }
  }
}