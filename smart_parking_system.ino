/*
 * ESP32 Smart Parking Lot System (6 Sensors)
 * Professional Parking Management System with LCD Display
 * 
 * Features:
 * - 6 Infrared sensors for individual parking slots
 * - Real-time occupancy display (e.g., "3/6 slots used")
 * - Individual slot status monitoring
 * - LCD 16x2 display with occupancy info and system status
 * - LED indicators for overall system status
 * - Statistics tracking and uptime monitoring
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===== PIN DEFINITIONS =====
// Infrared Sensors for 6 parking slots
const int SENSOR_PINS[6] = {4, 5, 18, 19, 25, 26}; // GPIO pins for sensors 1-6
const int SENSOR_COUNT = 6;

// LED Indicators
const int RED_LED_PIN = 32;    // GPIO32 - Red LED (Parking Full/Alert)
const int GREEN_LED_PIN = 33;  // GPIO33 - Green LED (Spaces Available)

// LCD I2C Configuration
const int SDA_PIN = 21;        // GPIO21 - SDA (I2C Data)
const int SCL_PIN = 22;        // GPIO22 - SCL (I2C Clock)
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD I2C Address: 0x27, Size: 16x2

// ===== SYSTEM VARIABLES =====
bool slotOccupied[6] = {false, false, false, false, false, false}; // Slot occupancy status
bool previousSlotState[6] = {false, false, false, false, false, false}; // Previous states
int occupiedCount = 0;          // Number of occupied slots
int availableSlots = 6;         // Number of available slots
unsigned long totalDetections = 0; // Total detection events
unsigned long lastUpdateTime = 0;  // Last LCD update time
unsigned long systemStartTime = 0; // System start time for uptime calculation

// Parking slot names for display
const String slotNames[6] = {"A1", "A2", "B1", "B2", "C1", "C2"};

void setup() {
  // Initialize Serial Communication
  Serial.begin(115200);
  Serial.println("\n======== ESP32 Smart Parking Lot System ========");
  Serial.println("6-Sensor Professional Parking Management System");
  Serial.println("Initializing system...");
  
  // Initialize I2C and LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Display startup message on LCD
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  lcd.setCursor(0, 1);
  lcd.print("System Starting");
  
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
  
  // Initial LED state - spaces available (green)
  setSystemReadyLEDs();
  
  Serial.println("System initialization complete!");
  Serial.println("Parking Slot Layout:");
  Serial.println("  A1  A2    (Front Row)");
  Serial.println("  B1  B2    (Middle Row)");  
  Serial.println("  C1  C2    (Back Row)");
  Serial.println("===============================================");
  
  delay(2000); // Allow sensors to stabilize
  systemStartTime = millis();
  
  // Display initial status
  updateDisplay();
}

void loop() {
  bool stateChanged = false;
  
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
  
  // If any state changed, update counters and display
  if (stateChanged) {
    updateOccupancyCounters();
    updateLEDStatus();
    updateDisplay();
    printParkingStatus();
  }
  
  // Update display every 5 seconds (for uptime and other info)
  if (millis() - lastUpdateTime > 5000) {
    updateDisplay();
    lastUpdateTime = millis();
  }
  
  // Print detailed status every 30 seconds
  static unsigned long lastDetailedStatus = 0;
  if (millis() - lastDetailedStatus > 30000) {
    printDetailedStatus();
    lastDetailedStatus = millis();
  }
  
  delay(100); // Small delay to prevent excessive polling
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
  if (occupiedCount == 0) {
    // All slots available - Green only
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  } else if (occupiedCount == SENSOR_COUNT) {
    // Parking full - Red only
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    // Partial occupancy - Green only
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }
}

void setSystemReadyLEDs() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);  // Green = spaces available
}

void updateDisplay() {
  lcd.clear();
  
  // Line 1: Occupancy ratio (e.g., "3/6 slots used")
  lcd.setCursor(0, 0);
  if (occupiedCount == 0) {
    lcd.print("All slots free");
  } else if (occupiedCount == SENSOR_COUNT) {
    lcd.print("PARKING FULL!");
  } else {
    lcd.print(occupiedCount);
    lcd.print("/");
    lcd.print(SENSOR_COUNT);
    lcd.print(" slots used");
  }
  
  // Line 2: System uptime and additional info
  lcd.setCursor(0, 1);
  unsigned long uptime = (millis() - systemStartTime) / 1000;
  
  if (uptime < 60) {
    lcd.print("Up: ");
    lcd.print(uptime);
    lcd.print("s");
  } else if (uptime < 3600) {
    lcd.print("Up: ");
    lcd.print(uptime / 60);
    lcd.print("m");
  } else {
    lcd.print("Up: ");
    lcd.print(uptime / 3600);
    lcd.print("h");
  }
  
  // Add detection count if space allows
  if (lcd.getCursor() < 10) {
    lcd.print(" Det:");
    if (totalDetections < 100) {
      lcd.print(totalDetections);
    } else {
      lcd.print("99+");
    }
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
  
  Serial.println("Slot Details:");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.print("  ");
    Serial.print(slotNames[i]);
    Serial.print(": ");
    Serial.println(slotOccupied[i] ? "🚗 OCCUPIED" : "🟢 AVAILABLE");
  }
  Serial.println("=============================\n");
}

void printDetailedStatus() {
  Serial.println("\n======== DETAILED SYSTEM STATUS ========");
  
  // Parking overview
  Serial.print("🏢 Parking Lot Status: ");
  if (occupiedCount == 0) {
    Serial.println("EMPTY");
  } else if (occupiedCount == SENSOR_COUNT) {
    Serial.println("FULL");
  } else {
    Serial.println("PARTIALLY OCCUPIED");
  }
  
  Serial.print("📊 Occupancy Rate: ");
  Serial.print((occupiedCount * 100) / SENSOR_COUNT);
  Serial.println("%");
  
  Serial.print("🚗 Occupied Slots: ");
  Serial.print(occupiedCount);
  Serial.print(" / ");
  Serial.println(SENSOR_COUNT);
  
  Serial.print("🟢 Available Slots: ");
  Serial.println(availableSlots);
  
  // Individual slot status
  Serial.println("\n📍 Individual Slot Status:");
  Serial.println("   Front Row:  A1(" + String(slotOccupied[0] ? "🚗" : "🟢") + ")  A2(" + String(slotOccupied[1] ? "🚗" : "🟢") + ")");
  Serial.println("   Middle Row: B1(" + String(slotOccupied[2] ? "🚗" : "🟢") + ")  B2(" + String(slotOccupied[3] ? "🚗" : "🟢") + ")");
  Serial.println("   Back Row:   C1(" + String(slotOccupied[4] ? "🚗" : "🟢") + ")  C2(" + String(slotOccupied[5] ? "🚗" : "🟢") + ")");
  
  // System statistics
  Serial.print("\n📈 Total Detection Events: ");
  Serial.println(totalDetections);
  
  unsigned long uptime = (millis() - systemStartTime) / 1000;
  Serial.print("⏱️  System Uptime: ");
  if (uptime < 60) {
    Serial.print(uptime);
    Serial.println(" seconds");
  } else if (uptime < 3600) {
    Serial.print(uptime / 60);
    Serial.print(" minutes ");
    Serial.print(uptime % 60);
    Serial.println(" seconds");
  } else {
    Serial.print(uptime / 3600);
    Serial.print(" hours ");
    Serial.print((uptime % 3600) / 60);
    Serial.println(" minutes");
  }
  
  // LED status
  Serial.print("💡 LED Status: ");
  if (digitalRead(RED_LED_PIN)) {
    Serial.println("🔴 RED (Parking Full)");
  } else if (digitalRead(GREEN_LED_PIN)) {
    Serial.println("🟢 GREEN (Spaces Available)");
  }
  
  Serial.println("========================================\n");
}
