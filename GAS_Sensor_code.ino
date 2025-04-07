#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// WiFi Credentials
const char *ssid = "10xTC-AP2";
const char *password = "10xTechClub#";

// Web Server on port 80
WebServer server(80);

// MQ2 Sensor Pin
const int mq2Pin = 32; // Use an analog pin

// Calibration values for MQ2
// These values may need adjustment based on your specific sensor
float R0 = 10.0; // Sensor resistance in clean air (calibration value)
bool isCalibrated = false;
unsigned long calibrationStartTime = 0;
const unsigned long calibrationDuration = 60000; // 60 seconds calibration

// HTML for the web page with CSS styling
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css">
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600&display=swap" rel="stylesheet">
  <style>
    body {
      font-family: 'Poppins', sans-serif;
      background-color: #121212;
      color: #FFFFFF;
      text-align: center;
      margin: 0;
      padding: 0;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      padding: 20px;
    }
    h1 {
      font-weight: 600;
      color: #BB86FC;
      margin-bottom: 30px;
    }
    #status {
      margin-bottom: 15px;
      font-size: 1.2em;
      color: #03DAC6;
    }
    #progressBar {
      width: 100%;
      height: 10px;
      background-color: #333333;
      border-radius: 5px;
      margin-bottom: 20px;
      overflow: hidden;
    }
    #bar {
      height: 100%;
      background-color: #03DAC6;
      width: 0%;
      transition: width 1s linear;
    }
    .reading-box {
      background-color: #1E1E1E;
      border-radius: 15px;
      padding: 20px;
      margin: 15px 0;
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
      transition: transform 0.3s ease;
    }
    .reading-box:hover {
      transform: translateY(-5px);
    }
    .reading-title {
      font-size: 1.5em;
      font-weight: 400;
      margin-bottom: 10px;
      color: #03DAC6;
    }
    .reading-value {
      font-size: 3em;
      font-weight: 600;
      margin: 10px 0;
    }
    .icon {
      font-size: 2em;
      margin-bottom: 10px;
      color: #CF6679;
    }
    .units {
      font-size: 0.8em;
      color: #BBBBBB;
    }
    .level-indicator {
      width: 100%;
      height: 10px;
      background-color: #333333;
      border-radius: 5px;
      margin-top: 15px;
      overflow: hidden;
    }
    .level-bar {
      height: 100%;
      border-radius: 5px;
      transition: width 0.5s ease, background-color 0.5s ease;
    }
    .safe {
      background-color: #4CAF50;
    }
    .warning {
      background-color: #FFC107;
    }
    .danger {
      background-color: #F44336;
    }
    .timestamp {
      margin-top: 30px;
      font-size: 0.9em;
      color: #AAAAAA;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Gas Sensor Monitor</h1>
    <div id="status">Calibrating Sensor... Please Wait</div>
    <div id="progressBar"><div id="bar"></div></div>
    <div class="reading-box">
      <i class="fas fa-wine-bottle icon"></i>
      <div class="reading-title">Alcohol Level</div>
      <div class="reading-value" id="alcohol">0</div>
      <div class="units">ppm</div>
      <div class="level-indicator">
        <div class="level-bar" id="alcohol-bar"></div>
      </div>
    </div>
    
    <div class="reading-box">
      <i class="fas fa-fire icon"></i>
      <div class="reading-title">LPG Level</div>
      <div class="reading-value" id="lpg">0</div>
      <div class="units">ppm</div>
      <div class="level-indicator">
        <div class="level-bar" id="lpg-bar"></div>
      </div>
    </div>
    
    <div class="timestamp" id="timestamp">Last updated: --</div>
  </div>
  
  <script>
    let calibrated = false;
    let countdown = 60;
    const statusText = document.getElementById('status');
    const bar = document.getElementById('bar');

    function checkCalibration() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          
          if (data.calibrated) {
            clearInterval(countdownInterval);
            bar.style.width = `100%`;
            statusText.innerText = "Calibration Complete. Monitoring Live Data!";
            calibrated = true;
          } else {
            // Calculate remaining time
            let progress = data.progress; // 0-100
            bar.style.width = `${progress}%`;
            countdown = Math.ceil(60 * (1 - progress/100));
            statusText.innerText = "Calibrating Sensor: " + countdown + "s remaining";
          }
        }
      };
      xhr.open("GET", "/calibration", true);
      xhr.send();
    }

    const countdownInterval = setInterval(checkCalibration, 1000);

    function updateValues() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          
          // Update alcohol
          document.getElementById("alcohol").textContent = data.alcohol.toFixed(2);
          var alcoholPercent = Math.min(data.alcohol / 1000 * 100, 100);
          var alcoholBar = document.getElementById("alcohol-bar");
          alcoholBar.style.width = alcoholPercent + "%";
          
          // Set alcohol bar color based on level
          if (data.alcohol < 400) {
            alcoholBar.className = "level-bar safe";
          } else if (data.alcohol < 800) {
            alcoholBar.className = "level-bar warning";
          } else {
            alcoholBar.className = "level-bar danger";
          }
          
          // Update LPG
          document.getElementById("lpg").textContent = data.lpg.toFixed(2);
          var lpgPercent = Math.min(data.lpg / 2000 * 100, 100);
          var lpgBar = document.getElementById("lpg-bar");
          lpgBar.style.width = lpgPercent + "%";
          
          // Set LPG bar color based on level
          if (data.lpg < 1000) {
            lpgBar.className = "level-bar safe";
          } else if (data.lpg < 1500) {
            lpgBar.className = "level-bar warning";
          } else {
            lpgBar.className = "level-bar danger";
          }
          
          // Update timestamp
          var now = new Date();
          document.getElementById("timestamp").textContent = "Last updated: " + now.toLocaleTimeString();
        }
      };
      xhr.open("GET", "/readings", true);
      xhr.send();
      
      // Update every 2 seconds
      setTimeout(updateValues, 2000);
    }
    
    // Start the update loop when page loads
    window.onload = updateValues;
  </script>
</body>
</html>
)rawliteral";

// Function to read MQ2 sensor and calculate gas concentrations
void readMQ2Sensor(float &alcohol, float &lpg) {
  // Read the analog value from MQ2 sensor
  int adcValue = analogRead(mq2Pin);
  
  // Convert ADC value to voltage (ESP32 has 3.3V reference and 12-bit ADC resolution)
  float voltage = adcValue * (3.3 / 4095.0);
  
  // Calculate sensor resistance
  float RS = ((3.3 * 10.0) / voltage) - 10.0; // 10K is the load resistor
  
  // Alcohol concentration calculation (approximate)
  // MQ2 alcohol sensitivity: RS/R0 = 6.5 * ppm^-0.6
  float RS_R0_ratio = RS / R0;
  alcohol = 9.47 * pow(RS_R0_ratio, -2.05);
  
  // LPG concentration calculation (approximate)
  // MQ2 LPG sensitivity: RS/R0 = 26.5 * ppm^-0.73
  lpg = 798.54 * pow(RS_R0_ratio, -2.95);
  
  // If values are negative or extremely high, they're likely errors
  if (alcohol < 0) alcohol = 0;
  if (lpg < 0) lpg = 0;
  
  // Cap to reasonable values if there's an error
  if (alcohol > 10000) alcohol = 10000;
  if (lpg > 10000) lpg = 10000;
}

// Function to calibrate the sensor
void calibrateSensor() {
  Serial.println("Calibrating MQ2 sensor...");
  float rs_air = 0;
  for (int i = 0; i < 10; i++) {
    rs_air += analogRead(mq2Pin);
    delay(100);
  }
  rs_air = rs_air / 10.0;
  float voltage = rs_air * (3.3 / 4095.0);
  float rs = ((3.3 * 10.0) / voltage) - 10.0;
  R0 = rs / 9.8; // 9.8 is RS/R0 ratio in clean air
  Serial.print("R0 = ");
  Serial.println(R0);
  isCalibrated = true;
}

void setup() {
  Serial.begin(115200);
  
  // Configure ADC
  analogReadResolution(12); // 12-bit resolution for better precision
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Set up OTA
  ArduinoOTA.setHostname("ESP32-MQ2");
  ArduinoOTA.setPassword("1234");
  ArduinoOTA.begin();
  
  // Set up web server routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });
  
  server.on("/readings", HTTP_GET, []() {
    float alcohol, lpg;
    readMQ2Sensor(alcohol, lpg);
    
    String json = "{\"alcohol\":" + String(alcohol) + ",\"lpg\":" + String(lpg) + "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/calibration", HTTP_GET, []() {
    unsigned long elapsedTime = millis() - calibrationStartTime;
    int progress = 0;
    
    if (isCalibrated) {
      progress = 100;
    } else if (elapsedTime >= calibrationDuration) {
      // Time to calibrate
      calibrateSensor();
      progress = 100;
    } else {
      progress = (elapsedTime * 100) / calibrationDuration;
    }
    
    String json = "{\"calibrated\":" + String(isCalibrated ? "true" : "false") + ",\"progress\":" + String(progress) + "}";
    server.send(200, "application/json", json);
  });
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  
  // Start calibration timer
  calibrationStartTime = millis();
  Serial.println("MQ2 warming up...");
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle web server clients
  server.handleClient();
  
  // Check if calibration should run
  if (!isCalibrated && (millis() - calibrationStartTime >= calibrationDuration)) {
    calibrateSensor();
  }
  
  // Print sensor values to Serial for debugging
  float alcohol, lpg;
  readMQ2Sensor(alcohol, lpg);
  
  Serial.print("Alcohol: ");
  Serial.print(alcohol);
  Serial.print(" ppm, LPG: ");
  Serial.print(lpg);
  Serial.println(" ppm");
  
  delay(1000);
}
