/*
 * Simple Parking Barrier System with 3 Slots
 * ESP32 Parking System with Servo Barrier Control
 * 
 * Features:
 * - 3 Infrared sensors for parking slots (A1, A2, B1)
 * - Servo motor (MG90S) as automatic barrier
 * - Push button for manual barrier control
 * - OLED 128x64 display for status
 * - Automatic barrier opening when slots available
 * - Manual barrier control with timed opening
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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

// OLED I2C Configuration
const int SDA_PIN = 21;                // GPIO21 - SDA (I2C Data)
const int SCL_PIN = 22;                // GPIO22 - SCL (I2C Clock)
#define SCREEN_WIDTH 128               // OLED display width, in pixels
#define SCREEN_HEIGHT 64               // OLED display height, in pixels
#define OLED_RESET -1                  // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
  
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    Serial.println("OLED display not found - using Serial Monitor only");
  } else {
    Serial.println("OLED display initialized successfully");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Parking System");
    display.println("Initializing...");
    display.display();
  }
  
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
  
  // Update OLED display periodically
  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  // If any state changed, update counters and display
  if (stateChanged) {
    updateOccupancyCounters();
    updateLEDStatus();
    printParkingStatus();
  }
  
  // Print status every 10 seconds
  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
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
  // Update OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Line 1: Occupancy status
  display.setCursor(0, 0);
  if (occupiedCount == 0) {
    display.println("All slots free");
  } else if (occupiedCount == SENSOR_COUNT) {
    display.println("PARKING FULL!");
  } else {
    display.print(occupiedCount);
    display.print("/");
    display.print(SENSOR_COUNT);
    display.println(" slots used");
  }
  
  // Line 2: Barrier status and uptime
  display.setCursor(0, 10);
  if (barrierOpen) {
    display.print("Gate:OPEN ");
    unsigned long timeLeft = (BARRIER_OPEN_DURATION - (millis() - barrierOpenTime)) / 1000;
    if (timeLeft > 0) {
      display.print(timeLeft);
      display.println("s");
    } else {
      display.println("0s");
    }
  } else {
    display.print("Gate:CLOSED ");
    unsigned long uptime = (millis() - systemStartTime) / 1000;
    if (uptime < 60) {
      display.print(uptime);
      display.println("s");
    } else {
      display.print(uptime / 60);
      display.println("m");
    }
  }
  
  // Line 3: Individual slot status
  display.setCursor(0, 20);
  display.print("A1:");
  display.print(slotOccupied[0] ? "OCC" : "FREE");
  display.print(" A2:");
  display.print(slotOccupied[1] ? "OCC" : "FREE");
  
  display.setCursor(0, 30);
  display.print("B1:");
  display.print(slotOccupied[2] ? "OCC" : "FREE");
  
  // Line 4: Available slots count
  display.setCursor(0, 40);
  display.print("Available: ");
  display.print(availableSlots);
  display.print(" slots");
  
  // Line 5: System status
  display.setCursor(0, 50);
  if (occupiedCount == SENSOR_COUNT) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Inverted text
    display.println("PARKING FULL!");
  } else {
    display.setTextColor(SSD1306_WHITE);
    display.println("Spaces Available");
  }
  
  display.display();
  
  // Also print to Serial for debugging
  Serial.println("=== OLED DISPLAY UPDATE ===");
  Serial.print("Occupancy: ");
  Serial.print(occupiedCount);
  Serial.print("/");
  Serial.print(SENSOR_COUNT);
  Serial.print(" (");
  Serial.print((occupiedCount * 100) / SENSOR_COUNT);
  Serial.println("%)");
  Serial.print("Available: ");
  Serial.println(availableSlots);
  Serial.print("Barrier: ");
  Serial.println(barrierOpen ? "OPEN" : "CLOSED");
  Serial.println("=============================");
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

// ===== OLED DISPLAY FUNCTIONS =====

void initializeOLED() {
  // Initialize I2C for OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); // Fast I2C
  
  // Try to initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    return;
  }
  
  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Parking System");
  display.println("Initializing...");
  display.display();
  delay(1000);
}

void testOLED() {
  // Test OLED display function
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED Test");
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Display Working!");
  display.display();
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
    Wire.setClock(400000); // Fast I2C for OLED
    delay(100);
    
    // Test I2C communication with OLED
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
      // I2C communication recovered silently
      Serial.println("I2C communication recovered");
    }
  }
}

