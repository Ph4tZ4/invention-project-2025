/*
 * Simple LCD Test Code for ESP32
 * Basic LCD 16x2 I2C Display Test
 * 
 * Features:
 * - Simple LCD initialization and test
 * - Display basic messages
 * - Test LCD functionality
 * - Minimal code for testing purposes
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD I2C Configuration
const int SDA_PIN = 21;        // GPIO21 - SDA (I2C Data)
const int SCL_PIN = 22;        // GPIO22 - SCL (I2C Clock)
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD I2C Address: 0x27, Size: 16x2

void setup() {
  // Initialize Serial Communication
  Serial.begin(115200);
  Serial.println("LCD Test Starting...");
  
  // Initialize I2C and LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  Serial.println("LCD initialized successfully!");
  
  // Display test messages
  lcd.setCursor(0, 0);
  lcd.print("Hello World!");
  lcd.setCursor(0, 1);
  lcd.print("LCD Test OK");
  
  Serial.println("Test message displayed on LCD");
  delay(2000);
}

void loop() {
  // Test 1: Basic text display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test 1: Basic");
  lcd.setCursor(0, 1);
  lcd.print("Text Display");
  Serial.println("Test 1: Basic text display");
  delay(2000);
  
  // Test 2: Numbers
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test 2: Numbers");
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  lcd.print(millis() / 1000);
  Serial.println("Test 2: Numbers display");
  delay(2000);
  
  // Test 3: Scrolling text
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test 3: Scroll");
  for (int i = 0; i < 10; i++) {
    lcd.setCursor(0, 1);
    lcd.print("Position: ");
    lcd.print(i);
    delay(500);
  }
  Serial.println("Test 3: Scrolling text");
  delay(1000);
  
  // Test 4: System info
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test 4: System");
  lcd.setCursor(0, 1);
  lcd.print("Uptime: ");
  lcd.print(millis() / 1000);
  lcd.print("s");
  Serial.println("Test 4: System information");
  delay(2000);
  
  // Test 5: All characters
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test 5: Chars");
  lcd.setCursor(0, 1);
  lcd.print("ABCDEFGHIJKLMN");
  Serial.println("Test 5: Character display");
  delay(2000);
  
  Serial.println("All tests completed. Restarting...");
  delay(1000);
}
