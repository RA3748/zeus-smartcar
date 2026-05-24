/*
 * PIN MAPPING:
 * ─────────────────────────────────────────────────────────────
 * Ultrasonik HC-SR05:
 *   - D0 (GPIO16) → ECHO
 *   - D6 (GPIO12) → TRIGGER
 * 
 * Infrared Line Follower (2 sensor):
 *   - D7 (GPIO13) → Sensor KIRI  (0=putih, 1=hitam)
 *   - D5 (GPIO14) → Sensor KANAN (0=putih, 1=hitam)
 * 
 * Motor Driver L298N:
 *   - D1 (GPIO5)  → IN1 (Motor Kiri Forward)
 *   - D2 (GPIO4)  → IN2 (Motor Kiri Backward)
 *   - D4 (GPIO2)  → IN3 (Motor Kanan Forward)
 *   - D8 (GPIO15) → IN4 (Motor Kanan Backward)
 *   - D3 (GPIO0)  → ENA + ENB (PWM Speed Control)
 * 
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

// ═══════════════════════════════════════════════════════════════
// KONFIGURASI UTAMA
// ═══════════════════════════════════════════════════════════════

// WiFi Settings
const char* ssid = "Zeus SmartCar V1";
const char* password = "zeussmartcar";
const int maxClients = 1;

// Pin Definitions
#define TRIG_PIN D6   // GPIO12
#define ECHO_PIN D0   // GPIO16
#define IR_LEFT  D7   // GPIO13
#define IR_RIGHT D5   // GPIO14
#define IN1 D1        // GPIO5  - Motor Kiri Forward
#define IN2 D2        // GPIO4  - Motor Kiri Backward
#define IN3 D4        // GPIO2  - Motor Kanan Forward
#define IN4 D8        // GPIO15 - Motor Kanan Backward
#define ENA D3        // GPIO0  - PWM Speed

// Speed Configuration (TUNING AREA - UDAH OPTIMAL!)
#define SPEED_MANUAL       200   // Manual control speed
#define SPEED_AUTO_FORWARD 90    // Jalan lurus di garis (OPTIMAL)
#define SPEED_AUTO_TURN    135   // Speed saat belok (OPTIMAL)

// Obstacle Detection (ULTRA RESPONSIVE!)
#define OBSTACLE_DISTANCE       20    // cm - jarak stop
#define OBSTACLE_CHECK_INTERVAL 30    // ms

// Debug Settings
#define DEBUG_MODE     true
#define DEBUG_INTERVAL 2000  // Print tiap 2 detik

// ═══════════════════════════════════════════════════════════════
// GLOBAL VARIABLES
// ═══════════════════════════════════════════════════════════════

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

enum Mode { MANUAL, AUTO_LINE_FOLLOWER };
Mode currentMode = MANUAL;

// Timing Variables
unsigned long lastUltrasonicCheck = 0;
unsigned long lastDebugPrint = 0;

// Obstacle Detection
bool obstacleDetected = false;

// ═══════════════════════════════════════════════════════════════
// FUNCTION PROTOTYPES
// ═══════════════════════════════════════════════════════════════

void setupPins();
void setupWiFi();
void setupWebServer();
void handleRoot();
void handleNotFound();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void motorStop();
void motorForward(int speed);
void motorBackward(int speed);
void motorLeft(int speed);
void motorRight(int speed);
float getDistanceUltraFast();
void handleManualControl(String command);
void handleLineFollower();
void debugPrintSensors();

// ═══════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n================================");
  Serial.println("  ESP8266");
  Serial.println("  ZEUS SMART CAR V1");
  Serial.println("================================");
  
  setupPins();
  setupWiFi();
  setupWebServer();
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("\n================================");
  Serial.println("  SYSTEM READY!");
  Serial.println("================================");
  Serial.println("Mode: MANUAL");
  Serial.println("IP: 192.168.4.1");
  Serial.println("================================\n");
}

// ═══════════════════════════════════════════════════════════════
// MAIN LOOP - ULTRA FAST RESPONSE
// ═══════════════════════════════════════════════════════════════

void loop() {
  // Handle web requests & websocket
  server.handleClient();
  webSocket.loop();
  
  // Debug print (optional)
  if (DEBUG_MODE && millis() - lastDebugPrint >= DEBUG_INTERVAL) {
    lastDebugPrint = millis();
    debugPrintSensors();
  }
  
  // Line follower mode - INSTANT IR READING EVERY LOOP!
  if (currentMode == AUTO_LINE_FOLLOWER) {
    handleLineFollower();
  }
}

// ═══════════════════════════════════════════════════════════════
// SETUP FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void setupPins() {
  Serial.println("[SETUP] Configuring pins...");
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  
  motorStop();
  
  Serial.println("  ✓ Pins configured");
  Serial.println("  ✓ PWM enabled on D3");
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
  );
  
  WiFi.softAP(ssid, password, 1, false, maxClients);
  
  Serial.println("[WIFI] Access Point Started");
  Serial.print("  SSID: ");
  Serial.println(ssid);
  Serial.print("  IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  
  // Mode switch endpoint
  server.on("/mode", HTTP_POST, []() {
    if (server.hasArg("mode")) {
      String mode = server.arg("mode");
      
      if (mode == "manual") {
        currentMode = MANUAL;
        motorStop();
        server.send(200, "text/plain", "Mode: MANUAL");
        Serial.println("[MODE] → MANUAL");
      } 
      else if (mode == "auto") {
        currentMode = AUTO_LINE_FOLLOWER;
        motorStop();
        obstacleDetected = false;
        server.send(200, "text/plain", "Mode: AUTO");
        Serial.println("[MODE] → AUTO LINE FOLLOWER");
      } 
      else {
        server.send(400, "text/plain", "Invalid mode");
      }
    } else {
      server.send(400, "text/plain", "Missing mode");
    }
  });
  
  server.begin();
  Serial.println("[WEB] Server started on port 80");
}

// ═══════════════════════════════════════════════════════════════
// WEB INTERFACE
// ═══════════════════════════════════════════════════════════════

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Car Control V3</title>
  
  <style>
    /* ============================================================
     * RESET & BASE STYLES
     * ============================================================ */
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    
    /* ============================================================
     * CONTAINER & LAYOUT
     * ============================================================ */
    .container {
      max-width: 800px;
      width: 100%;
    }
    
    h1 {
      text-align: center;
      color: #fff;
      margin-bottom: 30px;
      font-size: clamp(24px, 5vw, 36px);
      text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
    }
    
    /* ============================================================
     * MODE SELECTOR
     * ============================================================ */
    .mode-selector {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 30px;
    }
    
    .mode-card {
      background: #fff;
      border-radius: 20px;
      padding: 30px;
      cursor: pointer;
      transition: all 0.3s ease;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
      text-align: center;
    }
    
    .mode-card:hover {
      transform: translateY(-5px);
      box-shadow: 0 15px 40px rgba(0, 0, 0, 0.3);
    }
    
    .mode-card.active {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #fff;
    }
    
    .mode-icon {
      font-size: 60px;
      margin-bottom: 15px;
    }
    
    .mode-title {
      font-size: 24px;
      font-weight: 700;
      margin-bottom: 10px;
    }
    
    .mode-desc {
      font-size: 14px;
      opacity: 0.8;
    }
    
    /* ============================================================
     * CONTROL PANEL
     * ============================================================ */
    .control-panel {
      background: #fff;
      border-radius: 20px;
      padding: 30px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
      display: none; /* Hidden by default */
    }
    
    .control-panel.active {
      display: block; /* Show when active */
    }
    
    /* ============================================================
     * STATUS BAR (Connection status)
     * ============================================================ */
    .status-bar {
      background: #f0f0f0;
      padding: 15px;
      border-radius: 10px;
      margin-bottom: 20px;
      text-align: center;
      font-weight: 700;
    }
    
    .status-bar.connected {
      background: #4caf50;
      color: #fff;
    }
    
    .status-bar.disconnected {
      background: #f44336;
      color: #fff;
    }
    
    /* ============================================================
     * JOYSTICK CONTROLS (Manual mode buttons)
     * ============================================================ */
    .joystick-container {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      max-width: 300px;
      margin: 0 auto;
    }
    
    .btn {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #fff;
      border: none;
      padding: 20px;
      border-radius: 15px;
      font-size: 18px;
      cursor: pointer;
      transition: all 0.3s ease;
      box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
      touch-action: manipulation; /* Better touch response */
      -webkit-tap-highlight-color: transparent; /* Remove tap highlight */
    }
    
    .btn:active {
      transform: scale(0.95);
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
    }
    
    /* Button positions in grid */
    .btn-up { grid-column: 2; }
    .btn-left { grid-column: 1; grid-row: 2; }
    .btn-stop { 
      grid-column: 2; 
      grid-row: 2; 
      background: #f44336; /* Red color for stop */
    }
    .btn-right { grid-column: 3; grid-row: 2; }
    .btn-down { grid-column: 2; grid-row: 3; }
    
    /* ============================================================
     * AUTO MODE INFO (Line follower info panel)
     * ============================================================ */
    .auto-info {
      text-align: center;
      padding: 20px;
    }
    
    .auto-info h3 {
      color: #667eea;
      margin-bottom: 15px;
    }
    
    .auto-info p {
      line-height: 1.6;
      color: #666;
    }
    
    /* ============================================================
     * BACK BUTTON
     * ============================================================ */
    .back-btn {
      background: #9e9e9e;
      color: #fff;
      border: none;
      padding: 12px 24px;
      border-radius: 10px;
      font-size: 16px;
      cursor: pointer;
      margin-top: 20px;
      transition: all 0.3s ease;
    }
    
    .back-btn:hover {
      background: #757575;
      transform: translateY(-2px);
    }
    
    /* ============================================================
     * VERSION INFO
     * ============================================================ */
    .version {
      text-align: center;
      color: #fff;
      margin-top: 20px;
      font-size: 12px;
      opacity: 0.8;
    }
    
    /* ============================================================
     * RESPONSIVE DESIGN (Mobile optimization)
     * ============================================================ */
    @media (max-width: 600px) {
      .container { padding: 10px; }
      .mode-card { padding: 20px; }
      .mode-icon { font-size: 48px; }
      .btn { padding: 15px; font-size: 16px; }
    }
  </style>
</head>

<body>
  <div class="container">
    <!-- ============================================================
         HEADER
         ============================================================ -->
    <h1>🚗 ZEUS SMARTCAR CONTROL</h1>
    
    <!-- ============================================================
         MODE SELECTOR (Manual / Auto)
         ============================================================ -->
    <div class="mode-selector">
      <div class="mode-card" id="manualCard" onclick="selectMode('manual')">
        <div class="mode-icon">🕹️</div>
        <div class="mode-title">MANUAL</div>
        <div class="mode-desc">Kontrol manual dengan tombol</div>
      </div>
      
      <div class="mode-card" id="autoCard" onclick="selectMode('auto')">
        <div class="mode-icon">🤖</div>
        <div class="mode-title">AUTO</div>
        <div class="mode-desc">Line follower otomatis</div>
      </div>
    </div>
    
    <!-- ============================================================
         MANUAL CONTROL PANEL
         ============================================================ -->
    <div class="control-panel" id="manualPanel">
      <div class="status-bar" id="wsStatus">Connecting...</div>
      
      <div class="joystick-container">
        <button class="btn btn-up" id="btnUp">▲</button>
        <button class="btn btn-left" id="btnLeft">◄</button>
        <button class="btn btn-stop" id="btnStop">■</button>
        <button class="btn btn-right" id="btnRight">►</button>
        <button class="btn btn-down" id="btnDown">▼</button>
      </div>
      
      <div style="text-align:center">
        <button class="back-btn" onclick="backToModeSelection()">◄ Kembali</button>
      </div>
    </div>
    
    <!-- ============================================================
         AUTO MODE PANEL
         ============================================================ -->
    <div class="control-panel" id="autoPanel">
      <div class="status-bar connected">Mode AUTO Aktif</div>
      
      <div class="auto-info">
        <h3>⚡ Line Follower V1</h3>
        <p style="margin-top:10px;font-weight:700;color:#f44336">
          ⚠️ Taruh Smartcar di tengah lakban hitam<br>
          LAKBAN HITAM = Line
        </p>
      </div>
      
      <div style="text-align:center">
        <button class="back-btn" onclick="backToModeSelection()">◄ Kembali</button>
      </div>
    </div>
    
    <!-- ============================================================
         VERSION INFO
         ============================================================ -->
    <div class="version">Version 1.0</div>
  </div>

  <script>
    /* ============================================================
     * JAVASCRIPT - WEBSOCKET & CONTROL LOGIC
     * ============================================================ */
    
    // Global Variables
    let ws; // WebSocket connection
    let currentMode = null; // Current mode (manual/auto)
    let isPressed = false; // Track button press state
    let modeSelected = false; // Track if mode is selected
    
    /* ============================================================
     * WEBSOCKET CONNECTION
     * ============================================================ */
    
    const connectWebSocket = () => {
      // Create WebSocket connection to ESP8266
      ws = new WebSocket("ws://" + window.location.hostname + ":81");
      
      // Connection opened
      ws.onopen = () => {
        console.log("WebSocket Connected");
        document.getElementById("wsStatus").textContent = "Connected";
        document.getElementById("wsStatus").className = "status-bar connected";
      };
      
      // Connection closed
      ws.onclose = () => {
        console.log("WebSocket Disconnected");
        document.getElementById("wsStatus").textContent = "Disconnected - Reconnecting...";
        document.getElementById("wsStatus").className = "status-bar disconnected";
        // Auto reconnect after 2 seconds
        setTimeout(connectWebSocket, 2000);
      };
      
      // Connection error
      ws.onerror = (error) => {
        console.error("WebSocket Error:", error);
      };
    }
    
    /* ============================================================
     * MODE SELECTION (Manual / Auto)
     * ============================================================ */
    
    const selectMode = (mode) => {
      currentMode = mode;
      modeSelected = true;
      
      // Hide mode selector
      document.querySelector(".mode-selector").style.display = "none";
      
      // Remove all active classes
      document.getElementById("manualCard").classList.remove("active");
      document.getElementById("autoCard").classList.remove("active");
      document.getElementById("manualPanel").classList.remove("active");
      document.getElementById("autoPanel").classList.remove("active");
      
      // Show selected mode
      if (mode === "manual") {
        document.getElementById("manualCard").classList.add("active");
        document.getElementById("manualPanel").classList.add("active");
        
        // Connect WebSocket for manual control
        if (!ws || ws.readyState !== WebSocket.OPEN) {
          connectWebSocket();
        }
      } else {
        document.getElementById("autoCard").classList.add("active");
        document.getElementById("autoPanel").classList.add("active");
      }
      
      // Send mode to ESP8266
      fetch("/mode", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "mode=" + mode
      });
    }
    
    /* ============================================================
     * SEND COMMAND TO ESP8266
     * ============================================================ */
    
    const sendCommand = (command) => {
      // Only send if in manual mode and WebSocket is connected
      if (currentMode === "manual" && ws && ws.readyState === WebSocket.OPEN && modeSelected) {
        ws.send(command);
      }
    }
    
    /* ============================================================
     * BACK TO MODE SELECTION
     * ============================================================ */
    
    const backToModeSelection = () => {
      // Stop car if in manual mode
      if (currentMode === "manual") {
        sendCommand("stop");
      }
      
      // Show mode selector
      document.querySelector(".mode-selector").style.display = "grid";
      
      // Hide panels
      document.getElementById("manualPanel").classList.remove("active");
      document.getElementById("autoPanel").classList.remove("active");
      document.getElementById("manualCard").classList.remove("active");
      document.getElementById("autoCard").classList.remove("active");
      
      // Reset state
      currentMode = null;
      modeSelected = false;
      
      // Close WebSocket
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
      
      // Reset to manual mode on ESP
      fetch("/mode", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "mode=manual"
      });
    }
    
    /* ============================================================
     * BUTTON SETUP (Touch & Mouse events)
     * ============================================================ */
    
    const setupButton = (buttonId, command) => {
      const btn = document.getElementById(buttonId);
      
      // Touch events (for mobile)
      btn.addEventListener("touchstart", (e) => {
        e.preventDefault(); // Prevent default touch behavior
        isPressed = true;
        sendCommand(command);
      });
      
      btn.addEventListener("touchend", (e) => {
        e.preventDefault();
        if (isPressed) {
          sendCommand("stop");
          isPressed = false;
        }
      });
      
      // Mouse events (for desktop)
      btn.addEventListener("mousedown", () => {
        isPressed = true;
        sendCommand(command);
      });
      
      btn.addEventListener("mouseup", () => {
        if (isPressed) {
          sendCommand("stop");
          isPressed = false;
        }
      });
      
      // Prevent drag
      btn.addEventListener("dragstart", (e) => e.preventDefault());
    }
    
    /* ============================================================
     * INITIALIZE BUTTONS
     * ============================================================ */
    setupButton("btnUp", "forward");
    setupButton("btnDown", "backward");
    setupButton("btnLeft", "left");
    setupButton("btnRight", "right");
    
    // Stop button (click only, no hold)
    document.getElementById("btnStop").addEventListener("click", () => {
      sendCommand("stop");
    });
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ═══════════════════════════════════════════════════════════════
// WEBSOCKET HANDLER
// ═══════════════════════════════════════════════════════════════

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      motorStop();
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WS] Client #%u connected from %d.%d.%d.%d\n", 
                      num, ip[0], ip[1], ip[2], ip[3]);
      }
      break;
      
    case WStype_TEXT:
      {
        String command = String((char*)payload);
        command.trim();
        if (currentMode == MANUAL) {
          handleManualControl(command);
        }
      }
      break;
  }
}

// ═══════════════════════════════════════════════════════════════
// MANUAL CONTROL MODE
// ═══════════════════════════════════════════════════════════════

void handleManualControl(String command) {
  command.toLowerCase();
  
  if (command == "forward") {
    motorForward(SPEED_MANUAL);
    if (DEBUG_MODE) Serial.println("[MANUAL] → FORWARD");
  }
  else if (command == "backward") {
    motorBackward(SPEED_MANUAL);
    if (DEBUG_MODE) Serial.println("[MANUAL] → BACKWARD");
  }
  else if (command == "left") {
    motorLeft(SPEED_MANUAL);
    if (DEBUG_MODE) Serial.println("[MANUAL] → LEFT");
  }
  else if (command == "right") {
    motorRight(SPEED_MANUAL);
    if (DEBUG_MODE) Serial.println("[MANUAL] → RIGHT");
  }
  else if (command == "stop") {
    motorStop();
    if (DEBUG_MODE) Serial.println("[MANUAL] → STOP");
  }
}

void handleLineFollower() {
  // ─────────────────────────────────────────────────────────────
  // ULTRA FAST OBSTACLE DETECTION
  // ─────────────────────────────────────────────────────────────
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastUltrasonicCheck >= OBSTACLE_CHECK_INTERVAL) {
    lastUltrasonicCheck = currentMillis;
    
    float distance = getDistanceUltraFast();
    
    // Update obstacle status
    if (distance > 0 && distance < OBSTACLE_DISTANCE) {
      if (!obstacleDetected) {
        obstacleDetected = true;
        motorStop();
        if (DEBUG_MODE) {
          Serial.printf("[AUTO] ⚠️  OBSTACLE! Distance: %.1f cm\n", distance);
        }
      }
    } else {
      if (obstacleDetected) {
        obstacleDetected = false;
        if (DEBUG_MODE) Serial.println("[AUTO] ✓ Obstacle cleared");
      }
    }
  }
  
  // ─────────────────────────────────────────────────────────────
  // STOP if obstacle detected
  // ─────────────────────────────────────────────────────────────
  if (obstacleDetected) {
    motorStop();
    return;
  }
  
  int leftSensor = digitalRead(IR_LEFT);
  int rightSensor = digitalRead(IR_RIGHT);
  
  if (leftSensor == 0 && rightSensor == 0) {
    // ✓ PUTIH-PUTIH = Mobil di tengah jalur -> MAJU LURUS
    motorForward(SPEED_AUTO_FORWARD);
  }
  else if (leftSensor == 1 && rightSensor == 0) {
    // KIRI HITAM (mobil keluar ke kiri) -> BELOK KIRI
    motorLeft(SPEED_AUTO_TURN);
  }
  else if (leftSensor == 0 && rightSensor == 1) {
    // KANAN HITAM (mobil keluar ke kanan) -> BELOK KANAN
    motorRight(SPEED_AUTO_TURN);
  }
  else if (leftSensor == 1 && rightSensor == 1) {
    // HITAM-HITAM = Finish line atau lost -> STOP
    motorStop();
  }
}

// ═══════════════════════════════════════════════════════════════
// SENSOR FUNCTIONS
// ═══════════════════════════════════════════════════════════════

float getDistanceUltraFast() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 15000);
  
  if (duration == 0) {
    return -1; // No echo
  }
  
  float distance = duration * 0.034 / 2;
  
  // Filter invalid values
  if (distance < 2 || distance > 400) {
    return -1;
  }
  
  return distance;
}

// ═══════════════════════════════════════════════════════════════
// MOTOR CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
}

void motorForward(int speed) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, speed);
}

void motorBackward(int speed) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, speed);
}

void motorLeft(int speed) {
  // Pivot turn kiri: Motor kiri forward, Motor kanan backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, speed);
}

void motorRight(int speed) {
  // Pivot turn kanan: Motor kiri backward, Motor kanan forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, speed);
}

// ═══════════════════════════════════════════════════════════════
// DEBUG FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void debugPrintSensors() {
  int irLeft = digitalRead(IR_LEFT);
  int irRight = digitalRead(IR_RIGHT);
  float distance = getDistanceUltraFast();
  
  Serial.println("┌─────────────────────────────────────┐");
  Serial.print("│ Mode: ");
  Serial.print(currentMode == MANUAL ? "MANUAL " : "AUTO   ");
  Serial.print(" | Speed: ");
  Serial.print(currentMode == MANUAL ? SPEED_MANUAL : SPEED_AUTO_FORWARD);
  Serial.println("    │");
  
  Serial.print("│ IR-L: ");
  Serial.print(irLeft);
  Serial.print(irLeft == 1 ? " (HITAM)" : " (PUTIH)");
  Serial.print(" | IR-R: ");
  Serial.print(irRight);
  Serial.print(irRight == 1 ? " (HITAM)" : " (PUTIH)");
  Serial.println(" │");
  
  Serial.print("│ Distance: ");
  if (distance > 0) {
    Serial.print(distance, 1);
    Serial.print(" cm");
    if (distance < OBSTACLE_DISTANCE) {
      Serial.print(" [OBSTACLE!]");
    } else {
      Serial.print("          ");
    }
  } else {
    Serial.print("-- cm             ");
  }
  Serial.println(" │");
  
  Serial.print("│ Status: ");
  if (obstacleDetected) {
    Serial.print("⚠️  STOPPED (obstacle)");
  } else if (currentMode == AUTO_LINE_FOLLOWER) {
    if (irLeft == 0 && irRight == 0) {
      Serial.print("✓ On track (white) ");
    } else if (irLeft == 1 && irRight == 0) {
      Serial.print("← Left edge (turn) ");
    } else if (irLeft == 0 && irRight == 1) {
      Serial.print("→ Right edge (turn)");
    } else {
      Serial.print("⛔ Both black (stop)");
    }
  } else {
    Serial.print("Manual control    ");
  }
  Serial.println("    │");
  
  Serial.println("└─────────────────────────────────────┘");
}
