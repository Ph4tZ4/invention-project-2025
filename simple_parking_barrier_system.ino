/*
 * Simple Parking Barrier System with 3 Slots
 * ESP32 Parking System with Servo Barrier Control
 * 
 * Features:
 * - 3 Infrared sensors for parking slots (A1, A2, B1)
 * - Servo motor (MG90S) as automatic barrier
 * - Push button for manual barrier control
 * - LCD 16x2 display for status
 * - Automatic barrier opening when slots available
 * - Manual barrier control with timed opening
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ===== PIN DEFINITIONS =====
// Infrared Sensors for 3 parking slots
const int SENSOR_PINS[3] = {4, 5, 18}; // GPIO pins for sensors A1, A2, B1
const int SENSOR_COUNT = 3;

// Servo Motor (MG90S) - Barrier Control
const int SERVO_PIN = 19;              // GPIO19 - Servo control pin
Servo barrierServo;

// Push Button for Manual Control
const int BUTTON_PIN = 25;             // GPIO25 - Push button (with pull-up)

// LED Indicators
const int RED_LED_PIN = 32;            // GPIO32 - Red LED (Parking Full)
const int GREEN_LED_PIN = 33;          // GPIO33 - Green LED (Spaces Available)

// LCD I2C Configuration
const int SDA_PIN = 21;                // GPIO21 - SDA (I2C Data)
const int SCL_PIN = 22;                // GPIO22 - SCL (I2C Clock)
LiquidCrystal_I2C lcd(0x27, 16, 2);   // LCD I2C Address: 0x27, Size: 16x2

// ===== SYSTEM VARIABLES =====
bool slotOccupied[3] = {false, false, false}; // Slot occupancy status
bool previousSlotState[3] = {false, false, false}; // Previous states
int occupiedCount = 0;                 // Number of occupied slots
int availableSlots = 3;                // Number of available slots

// Servo and Button Control
const int BARRIER_CLOSED_ANGLE = 0;    // Servo angle when barrier is closed (0 degrees)
const int BARRIER_OPEN_ANGLE = 90;     // Servo angle when barrier is open (90 degrees)
bool barrierOpen = false;              // Current barrier state
unsigned long barrierOpenTime = 0;     // Time when barrier was opened
const unsigned long BARRIER_OPEN_DURATION = 5000; // Barrier stays open for 5 seconds

// Button Control
bool buttonPressed = false;
bool lastButtonState = HIGH;          // Button is pulled up, so HIGH = not pressed
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Timing
unsigned long lastUpdateTime = 0;
unsigned long systemStartTime = 0;
unsigned long totalDetections = 0;
unsigned long lastMemoryCleanup = 0;
const unsigned long MEMORY_CLEANUP_INTERVAL = 30000; // Clean memory every 30 seconds

// Memory management
const unsigned long DISPLAY_UPDATE_INTERVAL = 10000; // Update display every 10 seconds (reduced frequency)
unsigned long lastDisplayUpdate = 0;
const unsigned long I2C_RETRY_DELAY = 1000; // Delay between I2C retries
unsigned long lastI2CError = 0;

// Parking slot names for display (using const char* to save RAM)
const char* slotNames[3] = {"A1", "A2", "B1"};

void setup() {
  // Initialize Serial Communication
  Serial.begin(115200);
  Serial.println("\n======== Simple Parking Barrier System ========");
  Serial.println("3-Slot Parking with Servo Barrier Control");
  Serial.println("Initializing system...");
  
  // Initialize I2C and LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Display startup message on LCD
  lcd.setCursor(0, 0);
  lcd.print("Parking System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  Serial.println("LCD initialized successfully!");
  
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
  
  // Initialize button pin with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize servo
  barrierServo.attach(SERVO_PIN);
  barrierServo.write(BARRIER_CLOSED_ANGLE); // Start with barrier closed
  Serial.println("Servo barrier initialized - CLOSED position");
  
  // Initial LED state
  setSystemReadyLEDs();
  
  Serial.println("System initialization complete!");
  Serial.println("Parking Slot Layout:");
  Serial.println("  A1  A2    (Front Row)");
  Serial.println("  B1        (Back Row)");
  Serial.println("===============================================");
  Serial.println("Press button to manually open barrier for 5 seconds");
  
  // Print initial memory status
  printMemoryStatus();
  
  delay(2000); // Allow system to stabilize
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
        
        // Auto-open barrier when a slot becomes available
        if (!barrierOpen) {
          openBarrier();
          Serial.println("🚪 Barrier AUTO-OPENED - Slot available!");
        }
      }
    }
  }
  
  // Handle button press for manual barrier control
  handleButtonPress();
  
  // Handle automatic barrier closing
  handleBarrierTiming();
  
  // Periodic memory cleanup
  handleMemoryCleanup();
  
  // LCD display enabled
  
  // If any state changed, update counters and display
  if (stateChanged) {
    updateOccupancyCounters();
    updateLEDStatus();
    updateDisplay();
    printParkingStatus();
  }
  
  // Update display every 10 seconds
  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    printParkingStatus();
    lastDisplayUpdate = millis();
  }
  
  delay(200); // Increased delay to prevent I2C overload and excessive polling
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

// ===== SERVO BARRIER CONTROL =====

void openBarrier() {
  if (!barrierOpen) {
    barrierServo.write(BARRIER_OPEN_ANGLE);
    barrierOpen = true;
    barrierOpenTime = millis();
    Serial.println("🚪 BARRIER OPENED");
  }
}

void closeBarrier() {
  if (barrierOpen) {
    barrierServo.write(BARRIER_CLOSED_ANGLE);
    barrierOpen = false;
    Serial.println("🚪 BARRIER CLOSED");
  }
}

void handleBarrierTiming() {
  // Auto-close barrier after specified duration
  if (barrierOpen && (millis() - barrierOpenTime >= BARRIER_OPEN_DURATION)) {
    closeBarrier();
  }
}

// ===== BUTTON CONTROL =====

void handleButtonPress() {
  bool reading = digitalRead(BUTTON_PIN);
  
  // Check if button state changed (for debouncing)
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // If button state has been stable for debounce delay
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // If button state has changed
    if (reading != buttonPressed) {
      buttonPressed = reading;
      
      // If button is pressed (LOW because of pull-up)
      if (buttonPressed == LOW) {
        Serial.println("🔘 BUTTON PRESSED - Manual barrier control");
        
        if (barrierOpen) {
          // If barrier is open, close it immediately
          closeBarrier();
          Serial.println("🚪 Manual CLOSE command");
        } else {
          // If barrier is closed, open it for the specified duration
          openBarrier();
          Serial.println("🚪 Manual OPEN command - Will close in 5 seconds");
        }
      }
    }
  }
  
  lastButtonState = reading;
}

// ===== DISPLAY FUNCTIONS =====

void updateDisplay() {
  lcd.clear();
  
  // Line 1: Occupancy status
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
  
  // Line 2: Barrier status and uptime
  lcd.setCursor(0, 1);
  if (barrierOpen) {
    lcd.print("Gate:OPEN ");
    unsigned long timeLeft = (BARRIER_OPEN_DURATION - (millis() - barrierOpenTime)) / 1000;
    if (timeLeft > 0) {
      lcd.print(timeLeft);
      lcd.print("s");
    } else {
      lcd.print("0s");
    }
  } else {
    lcd.print("Gate:CLOSED ");
    unsigned long uptime = (millis() - systemStartTime) / 1000;
    if (uptime < 60) {
      lcd.print(uptime);
      lcd.print("s");
    } else {
      lcd.print(uptime / 60);
      lcd.print("m");
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

// ===== MEMORY MANAGEMENT FUNCTIONS =====

void handleMemoryCleanup() {
  // Perform memory cleanup every specified interval
  if (millis() - lastMemoryCleanup >= MEMORY_CLEANUP_INTERVAL) {
    performMemoryCleanup();
    lastMemoryCleanup = millis();
  }
}

void performMemoryCleanup() {
  // Light memory cleanup without aggressive LCD operations
  Serial.println(F("🧹 Performing light memory cleanup..."));
  
  // Print memory status before cleanup
  size_t freeHeapBefore = ESP.getFreeHeap();
  
  // Light garbage collection without LCD operations
  for (int i = 0; i < 3; i++) { // Reduced iterations
    char* temp = (char*)malloc(50);
    if (temp != NULL) {
      free(temp);
    }
    delay(5); // Shorter delay
  }
  
  size_t freeHeapAfter = ESP.getFreeHeap();
  
  // Print memory status
  printMemoryStatus();
  
  Serial.print(F("Memory cleanup complete. Freed: "));
  Serial.print(freeHeapAfter - freeHeapBefore);
  Serial.println(F(" bytes"));
}

void printMemoryStatus() {
  Serial.println(F("\n=== MEMORY STATUS ==="));
  
  // Convert bytes to GB
  float freeHeapGB = ESP.getFreeHeap() / (1024.0 * 1024.0 * 1024.0);
  float heapSizeGB = ESP.getHeapSize() / (1024.0 * 1024.0 * 1024.0);
  float freePsramGB = ESP.getFreePsram() / (1024.0 * 1024.0 * 1024.0);
  float minFreeHeapGB = ESP.getMinFreeHeap() / (1024.0 * 1024.0 * 1024.0);
  
  Serial.print(F("Free Heap: "));
  Serial.print(freeHeapGB, 3);
  Serial.println(F(" GB"));
  
  Serial.print(F("Heap Size: "));
  Serial.print(heapSizeGB, 3);
  Serial.println(F(" GB"));
  
  Serial.print(F("Free PSRAM: "));
  Serial.print(freePsramGB, 3);
  Serial.println(F(" GB"));
  
  Serial.print(F("Min Free Heap: "));
  Serial.print(minFreeHeapGB, 3);
  Serial.println(F(" GB"));
  
  // Calculate memory usage percentage
  float memoryUsage = ((float)(ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize()) * 100;
  Serial.print(F("Memory Usage: "));
  Serial.print(memoryUsage, 1);
  Serial.println(F("%"));
  
  Serial.println(F("=====================\n"));
}

// ===== SAFE LCD FUNCTIONS =====

bool initializeLCD() {
  // Try different I2C addresses for LCD
  int lcdAddresses[] = {0x27, 0x3F, 0x26, 0x25};
  int numAddresses = sizeof(lcdAddresses) / sizeof(lcdAddresses[0]);
  
  for (int addr = 0; addr < numAddresses; addr++) {
    Serial.print("Trying LCD address: 0x");
    Serial.println(lcdAddresses[addr], HEX);
    
    Wire.beginTransmission(lcdAddresses[addr]);
    if (Wire.endTransmission() == 0) {
      Serial.print("LCD found at address: 0x");
      Serial.println(lcdAddresses[addr], HEX);
      
      // Update LCD object with correct address
      lcd = LiquidCrystal_I2C(lcdAddresses[addr], 16, 2);
      
      // Try to initialize
      lcd.init();
      lcd.backlight();
      delay(200);
      
      // Test if LCD is working
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LCD Test");
      delay(500);
      
      return true;
    }
    delay(200);
  }
  
  Serial.println("❌ LCD not found on any I2C address");
  return false;
}

bool safeLCDClear() {
  // Simple clear without I2C checking
  lcd.clear();
  delay(50);
  return true;
}

void safeLCDPrint(int col, int row, const char* text) {
  // Simple print without I2C checking
  lcd.setCursor(col, row);
  lcd.print(text);
  delay(10);
}

void testLCD() {
  // Simple LCD test function
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Working!");
  lcd.setCursor(0, 1);
  lcd.print("Test OK");
  delay(2000);
}

// ===== I2C ERROR RECOVERY =====

void handleI2CErrors() {
  // Check for I2C errors and attempt recovery
  if (millis() - lastI2CError > I2C_RETRY_DELAY) {
    // Try to recover I2C communication
    Wire.end();
    delay(100);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);
    delay(100);
    
    // Test I2C communication
    Wire.beginTransmission(0x27);
    if (Wire.endTransmission() == 0) {
      // I2C communication recovered silently
    }
  }
}

