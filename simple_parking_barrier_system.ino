/*
 * Smart Parking System with Web Interface
 * ESP32 Parking System with WiFi and Web Dashboard
 * 
 * Features:
 * - 3 Infrared sensors for parking slots (A1, A2, B1)
 * - WiFi connectivity for web access
 * - Web interface for status monitoring
 * - Admin control panel
 * - Real-time parking status updates
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== PIN DEFINITIONS =====
// Infrared Sensors for 3 parking slots
const int SENSOR_PINS[3] = {4, 5, 18}; // GPIO pins for sensors A1, A2, B1
const int SENSOR_COUNT = 3;

// LED Indicators
const int RED_LED_PIN = 32;            // GPIO32 - Red LED (Parking Full)
const int GREEN_LED_PIN = 33;          // GPIO33 - Green LED (Spaces Available)

// ===== WIFI CONFIGURATION =====
const char* ssid = "KRITTIYA";     // WiFi SSID
const char* password = "00000000"; // WiFi password

// Web Server
WebServer server(80);

// ===== SYSTEM VARIABLES =====
bool slotOccupied[3] = {false, false, false}; // Slot occupancy status
bool previousSlotState[3] = {false, false, false}; // Previous states
int occupiedCount = 0;                 // Number of occupied slots
int availableSlots = 3;                // Number of available slots

// Timing
unsigned long lastUpdateTime = 0;
unsigned long systemStartTime = 0;
unsigned long totalDetections = 0;
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 1000; // Update web status every 1 second

// Parking slot names for display (using const char* to save RAM)
const char* slotNames[3] = {"A1", "A2", "B1"};

void setup() {
  // Initialize Serial Communication
  Serial.begin(115200);
  Serial.println("\n======== Smart Parking Web System ========");
  Serial.println("3-Slot Parking with Web Interface");
  Serial.println("Initializing system...");
  
  // Initialize sensor pins
  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(" (Slot ");
    Serial.print(slotNames[i]);
    Serial.print(") initialized on GPIO");
    Serial.println(SENSOR_PINS[i]);
  }
  
  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  
  // Initial LED state
  setSystemReadyLEDs();
  
  // Initialize WiFi
  initializeWiFi();
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("System initialization complete!");
  Serial.println("Parking Slot Layout:");
  Serial.println("  A1  A2    (Front Row)");
  Serial.println("  B1        (Back Row)");
  Serial.println("===============================================");
  Serial.print("Web interface available at: http://");
  Serial.println(WiFi.localIP());
  
  delay(1000); // Allow system to stabilize
  systemStartTime = millis();
}

void loop() {
  bool stateChanged = false;
  
  // Handle web server requests
  server.handleClient();
  
  // Read all sensors and check for changes
  for (int i = 0; i < SENSOR_COUNT; i++) {
    int sensorValue = digitalRead(SENSOR_PINS[i]);
    bool currentState = (sensorValue == LOW); // LOW = object detected (occupied)
    
    // Check if this slot's state changed
    if (currentState != previousSlotState[i]) {
      previousSlotState[i] = currentState;
      slotOccupied[i] = currentState;
      stateChanged = true;
      
      if (currentState) {
        Serial.print("🚗 Slot ");
        Serial.print(slotNames[i]);
        Serial.println(" OCCUPIED - Vehicle parked");
        totalDetections++;
      } else {
        Serial.print("🟢 Slot ");
        Serial.print(slotNames[i]);
        Serial.println(" AVAILABLE - Vehicle left");
      }
    }
  }
  
  // If any state changed, update counters and LEDs
  if (stateChanged) {
    updateOccupancyCounters();
    updateLEDStatus();
    printParkingStatus();
  }
  
  // Update status periodically
  if (millis() - lastStatusUpdate > STATUS_UPDATE_INTERVAL) {
    printParkingStatus();
    lastStatusUpdate = millis();
  }
  
  delay(100); // Reduced delay for better web responsiveness
}

// ===== CORE FUNCTIONS =====

void updateOccupancyCounters() {
  occupiedCount = 0;
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (slotOccupied[i]) {
      occupiedCount++;
    }
  }
  availableSlots = SENSOR_COUNT - occupiedCount;
}

void updateLEDStatus() {
  if (occupiedCount == SENSOR_COUNT) {
    // Parking full - Red only
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    // Spaces available - Green only
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }
}

void setSystemReadyLEDs() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);  // Green = spaces available
}

// ===== WIFI FUNCTIONS =====

void initializeWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi. Starting in offline mode.");
  }
}

// ===== WEB SERVER FUNCTIONS =====

void setupWebServer() {
  // Main dashboard page
  server.on("/", handleRoot);
  
  // Admin panel page
  server.on("/admin", handleAdmin);
  
  // API endpoint for parking status
  server.on("/api/status", handleStatusAPI);
  
  // API endpoint for admin actions
  server.on("/api/admin", HTTP_POST, handleAdminAPI);
  
  // Start server
  server.begin();
  Serial.println("Web server started on port 80");
}

// ===== WEB PAGE HANDLERS =====

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Smart Parking System</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; border-radius: 10px; padding: 30px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
  html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-bottom: 30px; }";
  html += ".status-card { padding: 20px; border-radius: 8px; text-align: center; color: white; font-weight: bold; }";
  html += ".available { background: #4CAF50; }";
  html += ".occupied { background: #f44336; }";
  html += ".parking-slots { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-bottom: 30px; }";
  html += ".slot { padding: 15px; border-radius: 8px; text-align: center; font-weight: bold; }";
  html += ".slot.free { background: #e8f5e8; color: #2e7d32; border: 2px solid #4caf50; }";
  html += ".slot.occupied { background: #ffebee; color: #c62828; border: 2px solid #f44336; }";
  html += ".admin-link { display: block; width: 200px; margin: 20px auto; padding: 12px; background: #2196F3; color: white; text-decoration: none; border-radius: 6px; text-align: center; }";
  html += ".admin-link:hover { background: #1976D2; }";
  html += ".info { background: #f5f5f5; padding: 15px; border-radius: 8px; margin-top: 20px; }";
  html += "</style>";
  html += "<script>";
  html += "function updateStatus() {";
  html += "  fetch('/api/status')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      document.getElementById('occupied').textContent = data.occupied + '/' + data.total;";
  html += "      document.getElementById('available').textContent = data.available;";
  html += "      for(let i = 0; i < 3; i++) {";
  html += "        const slot = document.getElementById('slot' + i);";
  html += "        if(data.slots[i]) {";
  html += "          slot.className = 'slot occupied';";
  html += "          slot.innerHTML = data.slotNames[i] + '<br>🚗 OCCUPIED';";
  html += "        } else {";
  html += "          slot.className = 'slot free';";
  html += "          slot.innerHTML = data.slotNames[i] + '<br>🟢 AVAILABLE';";
  html += "        }";
  html += "      }";
  html += "    });";
  html += "}";
  html += "setInterval(updateStatus, 1000);";
  html += "window.onload = updateStatus;";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🅿️ Smart Parking System</h1>";
  html += "<div class='status-grid'>";
  html += "<div class='status-card occupied'>";
  html += "<h3>Occupied</h3>";
  html += "<div id='occupied'>" + String(occupiedCount) + "/" + String(SENSOR_COUNT) + "</div>";
  html += "</div>";
  html += "<div class='status-card available'>";
  html += "<h3>Available</h3>";
  html += "<div id='available'>" + String(availableSlots) + "</div>";
  html += "</div>";
  html += "</div>";
  html += "<div class='parking-slots'>";
  for (int i = 0; i < SENSOR_COUNT; i++) {
    html += "<div id='slot" + String(i) + "' class='slot " + (slotOccupied[i] ? "occupied" : "free") + "'>";
    html += String(slotNames[i]) + "<br>";
    html += slotOccupied[i] ? "🚗 OCCUPIED" : "🟢 AVAILABLE";
    html += "</div>";
  }
  html += "</div>";
  html += "<a href='/admin' class='admin-link'>🔧 Admin Panel</a>";
  html += "<div class='info'>";
  html += "<strong>System Info:</strong><br>";
  html += "Uptime: " + String((millis() - systemStartTime) / 1000) + " seconds<br>";
  html += "Total Detections: " + String(totalDetections);
  html += "</div>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleAdmin() {
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Admin Panel - Smart Parking</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #ff7e5f 0%, #feb47b 100%); }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; padding: 30px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
  html += ".back-link { display: inline-block; margin-bottom: 20px; padding: 8px 16px; background: #6c757d; color: white; text-decoration: none; border-radius: 4px; }";
  html += ".back-link:hover { background: #5a6268; }";
  html += ".admin-section { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 20px; }";
  html += ".btn { padding: 10px 20px; margin: 5px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }";
  html += ".btn-danger { background: #dc3545; color: white; }";
  html += ".btn-success { background: #28a745; color: white; }";
  html += ".btn-info { background: #17a2b8; color: white; }";
  html += ".btn:hover { opacity: 0.8; }";
  html += ".status-table { width: 100%; border-collapse: collapse; margin-top: 15px; }";
  html += ".status-table th, .status-table td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += ".status-table th { background: #f1f3f4; }";
  html += "</style>";
  html += "<script>";
  html += "function resetSystem() {";
  html += "  if(confirm('Are you sure you want to reset the system?')) {";
  html += "    fetch('/api/admin', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({action: 'reset'}) })";
  html += "      .then(response => response.json())";
  html += "      .then(data => { alert(data.message); location.reload(); });";
  html += "  }";
  html += "}";
  html += "function updateStatus() {";
  html += "  fetch('/api/status')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      document.getElementById('uptime').textContent = Math.floor(data.uptime / 60) + ' minutes';";
  html += "      document.getElementById('detections').textContent = data.totalDetections;";
  html += "      document.getElementById('memory').textContent = data.freeMemory + ' bytes';";
  html += "    });";
  html += "}";
  html += "setInterval(updateStatus, 2000);";
  html += "window.onload = updateStatus;";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<a href='/' class='back-link'>← Back to Dashboard</a>";
  html += "<h1>🔧 Admin Control Panel</h1>";
  html += "<div class='admin-section'>";
  html += "<h3>System Control</h3>";
  html += "<button class='btn btn-danger' onclick='resetSystem()'>Reset System</button>";
  html += "<button class='btn btn-info' onclick='location.reload()'>Refresh Data</button>";
  html += "</div>";
  html += "<div class='admin-section'>";
  html += "<h3>System Statistics</h3>";
  html += "<table class='status-table'>";
  html += "<tr><th>Parameter</th><th>Value</th></tr>";
  html += "<tr><td>System Uptime</td><td id='uptime'>" + String((millis() - systemStartTime) / 60000) + " minutes</td></tr>";
  html += "<tr><td>Total Detections</td><td id='detections'>" + String(totalDetections) + "</td></tr>";
  html += "<tr><td>Free Memory</td><td id='memory'>" + String(ESP.getFreeHeap()) + " bytes</td></tr>";
  html += "<tr><td>WiFi Status</td><td>" + (WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</td></tr>";
  html += "<tr><td>IP Address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "</table>";
  html += "</div>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleStatusAPI() {
  DynamicJsonDocument doc(512);
  doc["occupied"] = occupiedCount;
  doc["available"] = availableSlots;
  doc["total"] = SENSOR_COUNT;
  doc["uptime"] = (millis() - systemStartTime) / 1000;
  doc["totalDetections"] = totalDetections;
  doc["freeMemory"] = ESP.getFreeHeap();
  
  JsonArray slots = doc.createNestedArray("slots");
  JsonArray names = doc.createNestedArray("slotNames");
  
  for (int i = 0; i < SENSOR_COUNT; i++) {
    slots.add(slotOccupied[i]);
    names.add(slotNames[i]);
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAdminAPI() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    
    String action = doc["action"];
    DynamicJsonDocument response(256);
    
    if (action == "reset") {
      // Reset system counters
      totalDetections = 0;
      systemStartTime = millis();
      
      // Reset slot states
      for (int i = 0; i < SENSOR_COUNT; i++) {
        slotOccupied[i] = false;
        previousSlotState[i] = false;
      }
      
      updateOccupancyCounters();
      updateLEDStatus();
      
      response["success"] = true;
      response["message"] = "System reset successfully";
      Serial.println("System reset via admin panel");
    } else {
      response["success"] = false;
      response["message"] = "Unknown action";
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  }
}

void printParkingStatus() {
  Serial.println("\n=== PARKING STATUS UPDATE ===");
  Serial.print("Occupancy: ");
  Serial.print(occupiedCount);
  Serial.print("/");
  Serial.print(SENSOR_COUNT);
  Serial.print(" (");
  Serial.print((occupiedCount * 100) / SENSOR_COUNT);
  Serial.println("%)");
  
  Serial.print("Available Slots: ");
  Serial.println(availableSlots);
  
  Serial.print("Barrier Status: ");
  Serial.println(barrierOpen ? "🚪 OPEN" : "🚪 CLOSED");
  
  Serial.println("Slot Details:");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.print("  ");
    Serial.print(slotNames[i]);
    Serial.print(": ");
    Serial.println(slotOccupied[i] ? "🚗 OCCUPIED" : "🟢 AVAILABLE");
  }
  Serial.println("=============================\n");
}


